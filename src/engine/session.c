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
};

amp_session_t *amp_session_create()
{
  amp_session_t *o = malloc(sizeof(amp_session_t));
  o->connection = NULL;
  o->channel = -1;
  o->remote_channel = -1;
  o->links = amp_vlist(16);
  o->begun = false;
  o->begin_sent = false;
  o->begin_rcvd = false;
  o->end_sent = false;
  o->end_rcvd = false;
  o->incoming_window = 65536;
  o->next_outgoing_id = 0;
  o->outgoing_window = 65536;
  return o;
}

int amp_session_begin(amp_session_t *session)
{
  session->begun = true;
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
  return amp_vlist_size(session->links);
}

amp_link_t *amp_session_get_link(amp_session_t *session, int index)
{
  return amp_to_ref(amp_vlist_get(session->links, index));
}

void amp_session_bind(amp_session_t *ssn, amp_connection_t *conn, int channel)
{
  ssn->connection = conn;
  ssn->channel = channel;
}

void amp_session_add(amp_session_t *ssn, amp_link_t *link)
{
  int handle = amp_vlist_size(ssn->links);
  amp_vlist_add(ssn->links, amp_from_ref(link));
  amp_link_bind(link, ssn, handle);
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

    int links = amp_session_links(ssn);
    for (int i = 0; i < links; i++) {
      amp_link_t *lnk = amp_session_get_link(ssn, i);
      amp_link_tick(lnk, eng);
    }
  } else {
    if (ssn->begin_sent && !ssn->end_sent) {
      amp_engine_end(eng, ssn->channel, NULL, NULL);
      ssn->end_sent = true;
    }
  }
}
