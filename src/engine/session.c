/*
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 */

#include <amp/engine.h>
#include <amp/value.h>
#include <stdio.h>

struct amp_transfer_t {
  const char* delivery_tag;
  sequence_t id;
  amp_link_t* link;
  const char *bytes;
  size_t size;
  enum OUTCOME local_outcome;
  bool local_settled;
  enum OUTCOME remote_outcome;
  bool remote_settled;
  int local_bytes_transferred;
  int remote_bytes_transferred;
  bool dirty;//true when there are local changes to propagate to peer
  bool unread;//true when there are remote changes yet to be read
  bool sent;
};

void amp_transfer_init(amp_transfer_t* o, const char* delivery_tag, sequence_t id, amp_link_t* link)
{
  o->delivery_tag = delivery_tag;
  o->id = id;
  o->link = link;
  o->bytes = 0;
  o->size = 0;
  o->local_outcome = IN_DOUBT;
  o->remote_outcome = IN_DOUBT;
  o->local_settled = false;
  o->remote_settled = false;
  o->local_bytes_transferred = 0;
  o->remote_bytes_transferred = 0;
  o->dirty = false;
  o->unread = false;
  o->sent = false;
}

struct amp_transfer_buffer_t {
    amp_transfer_t* transfers;
    size_t max_size;
    size_t head;
    size_t tail;
};
typedef struct amp_transfer_buffer_t amp_transfer_buffer_t;

void amp_transfer_buffer_init(amp_transfer_buffer_t* o, size_t size)
{
  o->transfers = malloc(sizeof(amp_transfer_t) * size);
  o->max_size = size;
  o->head = 0;
  o->tail = 0;
}

size_t amp_transfer_buffer_size(amp_transfer_buffer_t* o)
{
  return o->head <= o->tail ? o->tail - o->head : o->max_size - (o->head - o->tail);
}

bool amp_transfer_buffer_empty(amp_transfer_buffer_t* o)
{
    return o->head == o->tail;
}

amp_transfer_t* amp_transfer_buffer_get(amp_transfer_buffer_t* o, size_t index)
{
    return o->transfers + ((o->head + index) % o->max_size);
}

amp_transfer_t* amp_transfer_buffer_head(amp_transfer_buffer_t* o)
{
  return o->transfers + o->head;
}

amp_transfer_t* amp_transfer_buffer_tail(amp_transfer_buffer_t* o)
{
  return o->transfers + (o->tail > 0 ? o->tail - 1 : o->max_size - 1);
}

void amp_transfer_buffer_push(amp_transfer_buffer_t* o)
{
  if (++(o->tail) >= o->max_size) o->tail -= o->max_size;
}

void amp_transfer_buffer_pop(amp_transfer_buffer_t* o)
{
  if (++(o->head) >= o->max_size) o->head -= o->max_size;
}

struct amp_session_t {
  amp_connection_t *connection;
  int channel;
  int remote_channel;
  amp_list_t *links;
  bool begun;
  bool begin_sent;
  bool begin_rcvd;
  bool end_sent;
  bool end_rcvd;
  sequence_t next_incoming_id;
  uint32_t incoming_window;
  sequence_t next_outgoing_id;
  uint32_t outgoing_window;
  amp_transfer_buffer_t unsettled_in;
  amp_transfer_buffer_t unsettled_out;
};

amp_session_t *amp_session_create()
{
  amp_session_t *o = malloc(sizeof(amp_session_t));
  o->connection = NULL;
  o->channel = -1;
  o->remote_channel = -1;
  o->links = amp_list(16);
  o->begun = false;
  o->begin_sent = false;
  o->begin_rcvd = false;
  o->end_sent = false;
  o->end_rcvd = false;
  o->incoming_window = 65536;
  o->next_outgoing_id = 0;
  o->outgoing_window = 65536;
  amp_transfer_buffer_init(&o->unsettled_in, 1024);
  amp_transfer_buffer_init(&o->unsettled_out, 1024);
  return o;
}

int amp_session_destroy(amp_session_t* s)
{
    return 0;//TODO
}

int amp_session_begin(amp_session_t *session)
{
  session->begun = true;
  return 0;
}

int amp_session_end(amp_session_t *session)
{
  session->begun = false;
  return 0;
}

int amp_session_channel(amp_session_t *session)
{
  return session->channel;
}

sequence_t amp_session_next(amp_session_t *session)
{
  return session->next_outgoing_id++;
}

sequence_t amp_session_in_next(amp_session_t *session)
{
  return session->next_incoming_id;
}

int amp_session_in_win(amp_session_t *session)
{
  return session->incoming_window;
}

sequence_t amp_session_out_next(amp_session_t *session)
{
  return session->next_outgoing_id;
}

int amp_session_out_win(amp_session_t *session)
{
  return session->outgoing_window;
}

int amp_session_links(amp_session_t *session)
{
  return amp_list_size(session->links);
}

amp_link_t *amp_session_get_link(amp_session_t *session, int index)
{
  if (amp_list_size(session->links) > index)
    return amp_to_ref(amp_list_get(session->links, index));
  else
    return 0;
}

void amp_session_bind(amp_session_t *ssn, amp_connection_t *conn, int channel)
{
  ssn->connection = conn;
  ssn->channel = channel;
}

void amp_session_add(amp_session_t *ssn, amp_link_t *link)
{
  int handle = amp_list_size(ssn->links);
  amp_list_add(ssn->links, amp_from_ref(link));
  amp_link_bind(link, ssn, handle);
}

void amp_session_do_transfer(amp_session_t* session, amp_engine_t* eng)
{
  //TODO: avoid recomputing size?
  int size = amp_transfer_buffer_size(&session->unsettled_out);
  for (int i = 0; i < size; i++) {
  //  for (int i = size-1; i >= 0; --i) {
    amp_transfer_t* transfer = amp_transfer_buffer_get(&session->unsettled_out, i);
    if (!transfer->sent) {
      amp_engine_transfer(eng, amp_session_channel(session), amp_link_handle(transfer->link),
                          transfer->delivery_tag, transfer->id, transfer->bytes,
                          transfer->size);
      transfer->sent = true;
    } else {
        break;
    }
  }
}

void amp_session_process_unsettled(amp_session_t* session, amp_engine_t* eng, amp_transfer_buffer_t* buffer)
{
   while (!amp_transfer_buffer_empty(buffer)) {
     amp_transfer_t* transfer = amp_transfer_buffer_head(buffer);
     if (transfer->remote_settled && !transfer->unread) {
       amp_transfer_buffer_pop(buffer);
     } else {
         break;
     }
   }

   int size = amp_transfer_buffer_size(buffer);
   for (int i = 0; i < size; ++i) {
     amp_transfer_t* transfer = amp_transfer_buffer_get(buffer, i);
     if (transfer->dirty) {
       amp_engine_disposition(eng, session->channel, !amp_link_sender(transfer->link),
                              false/*TODO: batchable?*/,
                              transfer->id, transfer->id, transfer->local_settled,/*TODO: merge dispositions where possible*/
                              transfer->local_bytes_transferred, transfer->local_outcome);
       transfer->dirty = false;
     }
   }
}

void amp_session_set_remote_channel(amp_session_t* session, int remote_channel)
{
    session->remote_channel = remote_channel;
}

void amp_session_tick(amp_session_t *ssn, amp_engine_t *eng)
{
  if (ssn->begun) {
    if (!ssn->begin_sent) {
      amp_engine_begin(eng, ssn->channel, ssn->remote_channel,
                       ssn->next_outgoing_id, ssn->incoming_window,
                       ssn->outgoing_window);
      ssn->begin_sent = true;
    }
  }
  int links = amp_session_links(ssn);
  for (int i = 0; i < links; i++) {
    amp_link_t *lnk = amp_session_get_link(ssn, i);
    amp_link_tick(lnk, eng);
  }
  amp_session_do_transfer(ssn, eng);
  amp_session_process_unsettled(ssn, eng, &ssn->unsettled_in);
  amp_session_process_unsettled(ssn, eng, &ssn->unsettled_out);

  if (!ssn->begun) {
    if (ssn->begin_sent && !ssn->end_sent) {
      amp_engine_end(eng, ssn->channel, NULL, NULL);
      ssn->end_sent = true;
    }
  }
}

bool amp_session_active(amp_session_t *session)
{
  return session->begun;
}

bool amp_session_begun_by_peer(amp_session_t *session)
{
  return session->begin_rcvd;
}

bool amp_session_ended_by_peer(amp_session_t *session)
{
  return session->end_rcvd;
}

void amp_session_begin_received(amp_session_t* session)
{
  session->begin_rcvd = true;
  //need to set 'remote channel'
}

void amp_session_end_received(amp_session_t* session)
{
  session->end_rcvd = true;
}

amp_endpoint_t amp_session_next_endpoint(amp_session_t* session)
{
  amp_endpoint_t result;
  result.type = NULL_ENDPOINT;
  if ((session->begin_rcvd && !session->begun) || (session->end_rcvd && session->begun)) {
    result.type = SESSION_ENDPOINT;
    result.value.as_session = session;
  } else {
    int size = amp_list_size(session->links);
    for (int i = 0; i < size && result.type == NULL_ENDPOINT; ++i) {
      amp_link_t* link = amp_to_ref(amp_list_get(session->links, i));
      if (amp_link_has_pending_work(link)) {
        result.type = LINK_ENDPOINT;
        result.value.as_link = link;
      }
    }
  }
  return result;
}

const amp_transfer_t* amp_session_unsettled(amp_session_t* session, amp_link_t* link, const char* delivery_tag)
{
  amp_transfer_buffer_t* buffer = amp_link_sender(link) ? &session->unsettled_out : &session->unsettled_in;
  int size = amp_transfer_buffer_size(buffer);
  for (int i = 0; i < size; ++i) {
    amp_transfer_t* transfer = amp_transfer_buffer_get(buffer, i);
    if (transfer->delivery_tag == delivery_tag && transfer->link == link) return transfer;
  }
  return 0;
}

bool amp_session_update_unsettled(amp_session_t* session, amp_link_t* link, const char* delivery_tag, outcome_t outcome, bool settled)
{
  amp_transfer_buffer_t* buffer = &session->unsettled_in;
  int size = amp_transfer_buffer_size(buffer);
  for (int i = 0; i < size; ++i) {
    amp_transfer_t* transfer = amp_transfer_buffer_get(buffer, i);
    if (!delivery_tag || (transfer->delivery_tag == delivery_tag && transfer->link == link)) {
      transfer->dirty = (transfer->local_outcome != outcome) || (transfer->local_settled != settled);
      transfer->local_outcome = outcome;
      transfer->local_settled = settled;
      if (delivery_tag) return true;
    }
  }
  return delivery_tag == 0;
}

void amp_session_disposition_received(amp_session_t* session, bool role, sequence_t first, sequence_t last, outcome_t outcome, bool settled)
{
  amp_transfer_buffer_t* buffer = role ? &session->unsettled_out : &session->unsettled_in;
  int size = amp_transfer_buffer_size(buffer);
  //TODO: could improve algorithm here to utilise sequence
  for (int i = 0; i < size; ++i) {
    amp_transfer_t* transfer = amp_transfer_buffer_get(buffer, i);
    if (transfer->id < first) continue;
    else if (transfer->id > last) break;
    //else transfer is in range, i.e. first <= transfer->id <= last
    transfer->remote_outcome = outcome;
    transfer->remote_settled = settled;
    transfer->unread = true;
  }
}

void amp_session_record(amp_session_t* session, amp_link_t* link, const char* delivery_tag, sequence_t transfer_id,
                        const char* bytes, size_t size)
{
  amp_transfer_buffer_t* buffer = amp_link_sender(link) ? &session->unsettled_out : &session->unsettled_in;
  //TODO: check for overflow
  amp_transfer_buffer_push(buffer);
  amp_transfer_t* transfer = amp_transfer_buffer_tail(buffer);
  amp_transfer_init(transfer, delivery_tag, transfer_id, link);
  transfer->bytes = bytes;
  transfer->size = size;
}

const char *amp_transfer_delivery_tag(const amp_transfer_t* transfer)
{
  return transfer->delivery_tag;
}
bool amp_transfer_is_settled_locally(const amp_transfer_t* transfer)
{
  return transfer->local_settled;
}
bool amp_transfer_is_settled_remotely(const amp_transfer_t* transfer)
{
  return transfer->remote_settled;
}
outcome_t amp_transfer_local_outcome(const amp_transfer_t* transfer)
{
  return transfer->local_outcome;
}
outcome_t amp_transfer_remote_outcome(const amp_transfer_t* transfer)
{
  return transfer->remote_outcome;
}
void amp_transfer_clear_unread(amp_transfer_t* transfer)
{
  transfer->unread = false;
}
