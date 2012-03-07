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

#include "engine-internal.h"
#include <stdlib.h>
#include <string.h>
#include <amp/framing.h>
#include <amp/value.h>
#include "../protocol.h"
#include <wchar.h>

#include <stdarg.h>
#include <stdio.h>

// delivery buffers

void amp_delivery_buffer_init(amp_delivery_buffer_t *db, amp_sequence_t next, size_t capacity)
{
  // XXX: error handling
  db->deliveries = malloc(sizeof(amp_delivery_state_t) * capacity);
  db->next = next;
  db->capacity = capacity;
  db->head = 0;
  db->size = 0;
}

void amp_delivery_buffer_destroy(amp_delivery_buffer_t *db)
{
  free(db->deliveries);
}

size_t amp_delivery_buffer_size(amp_delivery_buffer_t *db)
{
  return db->size;
}

size_t amp_delivery_buffer_available(amp_delivery_buffer_t *db)
{
  return db->capacity - db->size;
}

bool amp_delivery_buffer_empty(amp_delivery_buffer_t *db)
{
  return db->size == 0;
}

amp_delivery_state_t *amp_delivery_buffer_get(amp_delivery_buffer_t *db, size_t index)
{
  if (index < db->size) return db->deliveries + ((db->head + index) % db->capacity);
  else return NULL;
}

amp_delivery_state_t *amp_delivery_buffer_head(amp_delivery_buffer_t *db)
{
  if (db->size) return db->deliveries + db->head;
  else return NULL;
}

amp_delivery_state_t *amp_delivery_buffer_tail(amp_delivery_buffer_t *db)
{
  if (db->size) return amp_delivery_buffer_get(db, db->size - 1);
  else return NULL;
}

amp_sequence_t amp_delivery_buffer_lwm(amp_delivery_buffer_t *db)
{
  if (db->size) return amp_delivery_buffer_head(db)->id;
  else return db->next;
}

static void amp_delivery_state_init(amp_delivery_state_t *ds, amp_delivery_t *delivery, amp_sequence_t id)
{
  ds->delivery = delivery;
  ds->id = id;
  ds->sent = false;
}

amp_delivery_state_t *amp_delivery_buffer_push(amp_delivery_buffer_t *db, amp_delivery_t *delivery)
{
  if (!amp_delivery_buffer_available(db))
    return NULL;
  db->size++;
  amp_delivery_state_t *ds = amp_delivery_buffer_tail(db);
  amp_delivery_state_init(ds, delivery, db->next++);
  return ds;
}

bool amp_delivery_buffer_pop(amp_delivery_buffer_t *db)
{
  if (db->size) {
    db->head = (db->head + 1) % db->capacity;
    db->size--;
    return true;
  } else {
    return false;
  }
}

void amp_delivery_buffer_gc(amp_delivery_buffer_t *db)
{
  while (db->size && !amp_delivery_buffer_head(db)->delivery) {
    amp_delivery_buffer_pop(db);
  }
}

// endpoints

amp_endpoint_type_t amp_endpoint_type(amp_endpoint_t *endpoint)
{
  return endpoint->type;
}

amp_endpoint_state_t amp_local_state(amp_endpoint_t *endpoint)
{
  return endpoint->local_state;
}

amp_endpoint_state_t amp_remote_state(amp_endpoint_t *endpoint)
{
  return endpoint->remote_state;
}

amp_error_t *amp_local_error(amp_endpoint_t *endpoint)
{
  if (endpoint->local_error.condition)
    return &endpoint->local_error;
  else
    return NULL;
}

amp_error_t *amp_remote_error(amp_endpoint_t *endpoint)
{
  if (endpoint->remote_error.condition)
    return &endpoint->remote_error;
  else
    return NULL;
}

void amp_destroy(amp_endpoint_t *endpoint)
{
  switch (endpoint->type)
  {
  case CONNECTION:
    amp_destroy_connection((amp_connection_t *)endpoint);
    break;
  case TRANSPORT:
    amp_destroy_transport((amp_transport_t *)endpoint);
    break;
  case SESSION:
    amp_destroy_session((amp_session_t *)endpoint);
    break;
  case SENDER:
    amp_destroy_sender((amp_sender_t *)endpoint);
    break;
  case RECEIVER:
    amp_destroy_receiver((amp_receiver_t *)endpoint);
    break;
  }
}

void amp_destroy_connection(amp_connection_t *connection)
{
  amp_destroy_transport(connection->transport);
  while (connection->session_count)
    amp_destroy_session(connection->sessions[connection->session_count - 1]);
  free(connection->sessions);
  free(connection);
}

void amp_destroy_transport(amp_transport_t *transport)
{
  amp_free_map(transport->dispatch);
  amp_free_list(transport->args);
  for (int i = 0; i < transport->session_capacity; i++) {
    amp_delivery_buffer_destroy(&transport->sessions[i].incoming);
    amp_delivery_buffer_destroy(&transport->sessions[i].outgoing);
    free(transport->sessions[i].links);
    free(transport->sessions[i].handles);
  }
  free(transport->sessions);
  free(transport->channels);
  free(transport->output);
  free(transport);
}

void amp_add_session(amp_connection_t *conn, amp_session_t *ssn)
{
  AMP_ENSURE(conn->sessions, conn->session_capacity, conn->session_count + 1);
  conn->sessions[conn->session_count++] = ssn;
  ssn->connection = conn;
  ssn->id = conn->session_count;
}

void amp_remove_session(amp_connection_t *conn, amp_session_t *ssn)
{
  for (int i = 0; i < conn->session_count; i++)
  {
    if (conn->sessions[i] == ssn)
    {
      memmove(&conn->sessions[i], &conn->sessions[i+1], conn->session_count - i - 1);
      conn->session_count--;
      break;
    }
  }
  ssn->connection = NULL;
}

void amp_destroy_session(amp_session_t *session)
{
  while (session->link_count)
    amp_destroy(&session->links[session->link_count - 1]->endpoint);
  amp_remove_session(session->connection, session);
  free(session->links);
  free(session);
}

void amp_add_link(amp_session_t *ssn, amp_link_t *link)
{
  AMP_ENSURE(ssn->links, ssn->link_capacity, ssn->link_count + 1);
  ssn->links[ssn->link_count++] = link;
  link->session = ssn;
  link->id = ssn->link_count;
}

void amp_remove_link(amp_session_t *ssn, amp_link_t *link)
{
  for (int i = 0; i < ssn->link_count; i++)
  {
    if (ssn->links[i] == link)
    {
      memmove(&ssn->links[i], &ssn->links[i+1], ssn->link_count - i - 1);
      ssn->link_count--;
      break;
    }
  }
  link->session = NULL;
}

void amp_clear_tag(amp_delivery_t *delivery)
{
  if (delivery->tag) {
    amp_free_binary(delivery->tag);
    delivery->tag = NULL;
  }
}

void amp_free_deliveries(amp_delivery_t *delivery)
{
  while (delivery)
  {
    amp_delivery_t *next = delivery->link_next;
    amp_clear_tag(delivery);
    if (delivery->capacity) free(delivery->bytes);
    free(delivery);
    delivery = next;
  }
}

void amp_dump_deliveries(amp_delivery_t *delivery)
{
  if (delivery) {
    while (delivery)
    {
      printf("%p(%.*s)", (void *) delivery, (int) amp_binary_size(delivery->tag),
             amp_binary_bytes(delivery->tag));
      if (delivery->link_next) printf(" -> ");
      delivery = delivery->link_next;
    }
  } else {
    printf("NULL");
  }
}

void amp_link_dump(amp_link_t *link)
{
  amp_dump_deliveries(link->settled_head);
  printf("\n");
  amp_dump_deliveries(link->head);
  printf("\n");
}

void amp_link_uninit(amp_link_t *link)
{
  if (link->remote_source) free(link->remote_source);
  if (link->remote_target) free(link->remote_target);
  amp_remove_link(link->session, link);
  amp_free_deliveries(link->settled_head);
  amp_free_deliveries(link->head);
  free(link->name);
}

void amp_destroy_sender(amp_sender_t *sender)
{
  amp_link_uninit(&sender->link);
  free(sender);
}
void amp_destroy_receiver(amp_receiver_t *receiver)
{
  amp_link_uninit(&receiver->link);
  free(receiver);
}

void amp_endpoint_init(amp_endpoint_t *endpoint, int type, amp_connection_t *conn)
{
  endpoint->type = type;
  endpoint->local_state = UNINIT;
  endpoint->remote_state = UNINIT;
  endpoint->local_error = (amp_error_t) {.condition = NULL};
  endpoint->remote_error = (amp_error_t) {.condition = NULL};
  endpoint->endpoint_next = NULL;
  endpoint->endpoint_prev = NULL;
  endpoint->transport_next = NULL;
  endpoint->transport_prev = NULL;
  endpoint->modified = false;

  LL_ADD_PFX(conn->endpoint_head, conn->endpoint_tail, endpoint, endpoint_);
}

amp_connection_t *amp_get_connection(amp_endpoint_t *endpoint)
{
  switch (endpoint->type) {
  case CONNECTION:
    return (amp_connection_t *) endpoint;
  case SESSION:
    return ((amp_session_t *) endpoint)->connection;
  case SENDER:
  case RECEIVER:
    return ((amp_link_t *) endpoint)->session->connection;
  case TRANSPORT:
    return ((amp_transport_t *) endpoint)->connection;
  }

  return NULL;
}

void amp_modified(amp_connection_t *connection, amp_endpoint_t *endpoint);

void amp_open(amp_endpoint_t *endpoint)
{
  // TODO: do we care about the current state?
  endpoint->local_state = ACTIVE;
  amp_modified(amp_get_connection(endpoint), endpoint);
}

void amp_close(amp_endpoint_t *endpoint)
{
  // TODO: do we care about the current state?
  endpoint->local_state = CLOSED;
  amp_modified(amp_get_connection(endpoint), endpoint);
}

amp_connection_t *amp_connection()
{
  amp_connection_t *conn = malloc(sizeof(amp_connection_t));
  conn->endpoint_head = NULL;
  conn->endpoint_tail = NULL;
  amp_endpoint_init(&conn->endpoint, CONNECTION, conn);
  conn->transport_head = NULL;
  conn->transport_tail = NULL;
  conn->sessions = NULL;
  conn->session_capacity = 0;
  conn->session_count = 0;
  conn->transport = NULL;
  conn->work_head = NULL;
  conn->work_tail = NULL;
  conn->tpwork_head = NULL;
  conn->tpwork_tail = NULL;

  return conn;
}

amp_delivery_t *amp_work_head(amp_connection_t *connection)
{
  return connection->work_head;
}

amp_delivery_t *amp_work_next(amp_delivery_t *delivery)
{
  if (delivery->work)
    return delivery->work_next;
  else
    return amp_work_head(delivery->link->session->connection);
}

void amp_add_work(amp_connection_t *connection, amp_delivery_t *delivery)
{
  if (!delivery->work)
  {
    LL_ADD_PFX(connection->work_head, connection->work_tail, delivery, work_);
    delivery->work = true;
  }
}

void amp_clear_work(amp_connection_t *connection, amp_delivery_t *delivery)
{
  if (delivery->work)
  {
    LL_REMOVE_PFX(connection->work_head, connection->work_tail, delivery, work_);
    delivery->work = false;
  }
}

void amp_work_update(amp_connection_t *connection, amp_delivery_t *delivery)
{
  amp_link_t *link = amp_link(delivery);
  amp_delivery_t *current = amp_current(link);
  if (delivery->dirty) {
    amp_add_work(connection, delivery);
  } else if (delivery == current) {
    if (link->endpoint.type == SENDER) {
      if (link->credit > 0) {
        amp_add_work(connection, delivery);
      } else {
        amp_clear_work(connection, delivery);
      }
    } else {
      amp_add_work(connection, delivery);
    }
  } else {
    amp_clear_work(connection, delivery);
  }
}

void amp_add_tpwork(amp_delivery_t *delivery)
{
  amp_connection_t *connection = delivery->link->session->connection;
  if (!delivery->tpwork)
  {
    LL_ADD_PFX(connection->tpwork_head, connection->tpwork_tail, delivery, tpwork_);
    delivery->tpwork = true;
  }
  amp_modified(connection, &connection->endpoint);
}

void amp_clear_tpwork(amp_delivery_t *delivery)
{
  amp_connection_t *connection = delivery->link->session->connection;
  if (delivery->tpwork)
  {
    LL_REMOVE_PFX(connection->tpwork_head, connection->tpwork_tail, delivery, tpwork_);
    delivery->tpwork = false;
  }
}

void amp_dump(amp_connection_t *conn)
{
  amp_endpoint_t *endpoint = conn->transport_head;
  while (endpoint)
  {
    printf("%p", (void *) endpoint);
    endpoint = endpoint->transport_next;
    if (endpoint)
      printf(" -> ");
  }
  printf("\n");
}

void amp_modified(amp_connection_t *connection, amp_endpoint_t *endpoint)
{
  if (!endpoint->modified) {
    LL_ADD_PFX(connection->transport_head, connection->transport_tail, endpoint, transport_);
    endpoint->modified = true;
  }
}

void amp_clear_modified(amp_connection_t *connection, amp_endpoint_t *endpoint)
{
  if (endpoint->modified) {
    LL_REMOVE_PFX(connection->transport_head, connection->transport_tail, endpoint, transport_);
    endpoint->transport_next = NULL;
    endpoint->transport_prev = NULL;
    endpoint->modified = false;
  }
}

bool amp_matches(amp_endpoint_t *endpoint, amp_endpoint_state_t local,
                 amp_endpoint_state_t remote)
{
  return (endpoint->local_state & local) && (endpoint->remote_state & remote);
}

amp_endpoint_t *amp_find(amp_endpoint_t *endpoint, amp_endpoint_state_t local,
                         amp_endpoint_state_t remote)
{
  while (endpoint)
  {
    if (amp_matches(endpoint, local, remote))
      return endpoint;
    endpoint = endpoint->endpoint_next;
  }
  return NULL;
}

amp_endpoint_t *amp_endpoint_head(amp_connection_t *conn,
                                  amp_endpoint_state_t local,
                                  amp_endpoint_state_t remote)
{
  return amp_find(conn->endpoint_head, local, remote);
}

amp_endpoint_t *amp_endpoint_next(amp_endpoint_t *endpoint,
                                  amp_endpoint_state_t local,
                                  amp_endpoint_state_t remote)
{
  return amp_find(endpoint->endpoint_next, local, remote);
}

amp_session_t *amp_session(amp_connection_t *conn)
{

  amp_session_t *ssn = malloc(sizeof(amp_session_t));
  amp_endpoint_init(&ssn->endpoint, SESSION, conn);
  amp_add_session(conn, ssn);
  ssn->links = NULL;
  ssn->link_capacity = 0;
  ssn->link_count = 0;

  return ssn;
}

  /*  amp_map_set(MAP, amp_symbol(AMP_HEAP, NAME ## _SYM), amp_ulong(AMP_HEAP, NAME)); \ */
#define __DISPATCH(MAP, NAME)                                           \
  amp_map_set(MAP, amp_ulong(NAME ## _CODE), amp_ulong(NAME ## _IDX))

void amp_transport_init(amp_transport_t *transport)
{
  amp_endpoint_init(&transport->endpoint, TRANSPORT, transport->connection);

  amp_map_t *m = amp_map(32);
  transport->dispatch = m;

  __DISPATCH(m, OPEN);
  __DISPATCH(m, BEGIN);
  __DISPATCH(m, ATTACH);
  __DISPATCH(m, TRANSFER);
  __DISPATCH(m, FLOW);
  __DISPATCH(m, DISPOSITION);
  __DISPATCH(m, DETACH);
  __DISPATCH(m, END);
  __DISPATCH(m, CLOSE);

  transport->args = amp_list(16);
  // XXX
  transport->capacity = 4*1024;
  transport->output = malloc(transport->capacity);
  transport->available = 0;

  transport->open_sent = false;
  transport->close_sent = false;

  transport->sessions = NULL;
  transport->session_capacity = 0;

  transport->channels = NULL;
  transport->channel_capacity = 0;
}

amp_session_state_t *amp_session_state(amp_transport_t *transport, amp_session_t *ssn)
{
  int old_capacity = transport->session_capacity;
  AMP_ENSURE(transport->sessions, transport->session_capacity, ssn->id + 1);
  for (int i = old_capacity; i < transport->session_capacity; i++)
  {
    transport->sessions[i] = (amp_session_state_t) {.session=NULL,
                                                    .local_channel=-1,
                                                    .remote_channel=-1};
    amp_delivery_buffer_init(&transport->sessions[i].incoming, 0, 1024);
    amp_delivery_buffer_init(&transport->sessions[i].outgoing, 0, 1024);
  }
  amp_session_state_t *state = &transport->sessions[ssn->id];
  state->session = ssn;
  return state;
}

amp_session_state_t *amp_channel_state(amp_transport_t *transport, uint16_t channel)
{
  AMP_ENSUREZ(transport->channels, transport->channel_capacity, channel + 1);
  return transport->channels[channel];
}

void amp_map_channel(amp_transport_t *transport, uint16_t channel, amp_session_state_t *state)
{
  AMP_ENSUREZ(transport->channels, transport->channel_capacity, channel + 1);
  state->remote_channel = channel;
  transport->channels[channel] = state;
}

amp_transport_t *amp_transport(amp_connection_t *conn)
{
  if (conn->transport) {
    return NULL;
  } else {
    conn->transport = malloc(sizeof(amp_transport_t));
    conn->transport->connection = conn;
    amp_transport_init(conn->transport);
    return conn->transport;
  }
}

wchar_t *wcsdup(const wchar_t *src)
{
  if (src) {
    wchar_t *dest = malloc((wcslen(src)+1)*sizeof(wchar_t));
    return wcscpy(dest, src);
  } else {
    return 0;
  }
}

void amp_link_init(amp_link_t *link, int type, amp_session_t *session, const wchar_t *name)
{
  amp_endpoint_init(&link->endpoint, type, session->connection);
  amp_add_link(session, link);
  link->name = wcsdup(name);
  link->local_source = NULL;
  link->local_target = NULL;
  link->remote_source = NULL;
  link->remote_target = NULL;
  link->settled_head = link->settled_tail = NULL;
  link->head = link->tail = link->current = NULL;
  link->credit = 0;
}

void amp_set_source(amp_link_t *link, const wchar_t *source)
{
  link->local_source = source;
}

void amp_set_target(amp_link_t *link, const wchar_t *target)
{
  link->local_target = target;
}

wchar_t *amp_remote_source(amp_link_t *link)
{
  return link->remote_source;
}

wchar_t *amp_remote_target(amp_link_t *link)
{
  return link->remote_target;
}

amp_link_state_t *amp_link_state(amp_session_state_t *ssn_state, amp_link_t *link)
{
  int old_capacity = ssn_state->link_capacity;
  AMP_ENSURE(ssn_state->links, ssn_state->link_capacity, link->id + 1);
  for (int i = old_capacity; i < ssn_state->link_capacity; i++)
  {
    ssn_state->links[i] = (amp_link_state_t) {.link=NULL, .local_handle = -1,
                                              .remote_handle=-1};
  }
  amp_link_state_t *state = &ssn_state->links[link->id];
  state->link = link;
  return state;
}

void amp_map_handle(amp_session_state_t *ssn_state, uint32_t handle, amp_link_state_t *state)
{
  AMP_ENSUREZ(ssn_state->handles, ssn_state->handle_capacity, handle + 1);
  state->remote_handle = handle;
  ssn_state->handles[handle] = state;
}

amp_link_state_t *amp_handle_state(amp_session_state_t *ssn_state, uint32_t handle)
{
  AMP_ENSUREZ(ssn_state->handles, ssn_state->handle_capacity, handle + 1);
  return ssn_state->handles[handle];
}

amp_sender_t *amp_sender(amp_session_t *session, const wchar_t *name)
{
  amp_sender_t *snd = malloc(sizeof(amp_sender_t));
  amp_link_init(&snd->link, SENDER, session, name);
  return snd;
}

amp_receiver_t *amp_receiver(amp_session_t *session, const wchar_t *name)
{
  amp_receiver_t *rcv = malloc(sizeof(amp_receiver_t));
  amp_link_init(&rcv->link, RECEIVER, session, name);
  rcv->credits = 0;
  return rcv;
}

amp_session_t *amp_get_session(amp_link_t *link)
{
  return link->session;
}

amp_delivery_t *amp_delivery(amp_link_t *link, amp_binary_t *tag)
{
  amp_delivery_t *delivery = link->settled_head;
  LL_POP_PFX(link->settled_head, link->settled_tail, link_);
  if (!delivery) delivery = malloc(sizeof(amp_delivery_t));
  delivery->link = link;
  delivery->tag = amp_binary_dup(tag);
  delivery->local_state = 0;
  delivery->remote_state = 0;
  delivery->local_settled = false;
  delivery->remote_settled = false;
  delivery->dirty = false;
  LL_ADD_PFX(link->head, link->tail, delivery, link_);
  delivery->work_next = NULL;
  delivery->work_prev = NULL;
  delivery->work = false;
  delivery->tpwork_next = NULL;
  delivery->tpwork_prev = NULL;
  delivery->tpwork = false;
  delivery->bytes = NULL;
  delivery->size = 0;
  delivery->capacity = 0;
  delivery->context = NULL;

  if (!link->current)
    link->current = delivery;

  amp_work_update(link->session->connection, delivery);

  return delivery;
}

bool amp_is_current(amp_delivery_t *delivery)
{
  amp_link_t *link = delivery->link;
  return amp_current(link) == delivery;
}

void amp_delivery_dump(amp_delivery_t *d)
{
  char tag[1024];
  amp_format(tag, 1024, amp_from_binary(d->tag));
  printf("{tag=%s, local_state=%u, remote_state=%u, local_settled=%u, "
         "remote_settled=%u, dirty=%u, current=%u, writable=%u, readable=%u, "
         "work=%u}",
         tag, d->local_state, d->remote_state, d->local_settled,
         d->remote_settled, d->dirty, amp_is_current(d), amp_writable(d),
         amp_readable(d), d->work);
}

amp_binary_t *amp_delivery_tag(amp_delivery_t *delivery)
{
  return delivery->tag;
}

amp_delivery_t *amp_current(amp_link_t *link)
{
  return link->current;
}

void amp_advance_sender(amp_sender_t *sender)
{
  amp_link_t *link = &sender->link;
  if (link->credit > 0) {
    link->credit--;
    amp_add_tpwork(link->current);
    link->current = link->current->link_next;
  }
}

void amp_advance_receiver(amp_receiver_t *receiver)
{
  amp_link_t *link = &receiver->link;
  link->current = link->current->link_next;
}

bool amp_advance(amp_link_t *link)
{
  if (link->current) {
    amp_delivery_t *prev = link->current;
    if (link->endpoint.type == SENDER) {
      amp_advance_sender((amp_sender_t *)link);
    } else {
      amp_advance_receiver((amp_receiver_t *)link);
    }
    amp_delivery_t *next = link->current;
    amp_work_update(link->session->connection, prev);
    if (next) amp_work_update(link->session->connection, next);
    return prev != next;
  } else {
    return false;
  }
}

void amp_real_settle(amp_delivery_t *delivery)
{
  amp_link_t *link = delivery->link;
  LL_REMOVE_PFX(link->head, link->tail, delivery, link_);
  // TODO: what if we settle the current delivery?
  LL_ADD_PFX(link->settled_head, link->settled_tail, delivery, link_);
  amp_clear_tag(delivery);
  delivery->size = 0;
}

void amp_full_settle(amp_delivery_buffer_t *db, amp_delivery_t *delivery)
{
  amp_delivery_state_t *state = delivery->context;
  delivery->context = NULL;
  state->delivery = NULL;
  amp_real_settle(delivery);
  amp_delivery_buffer_gc(db);
}

void amp_settle(amp_delivery_t *delivery)
{
  delivery->local_settled = true;
  amp_add_tpwork(delivery);
}

static void amp_trace(amp_transport_t *transport, uint16_t ch, char *op, amp_list_t *args)
{
  amp_format(transport->scratch, SCRATCH, amp_from_list(args));
  fprintf(stderr, "[%u] %s %s\n", ch, op, transport->scratch);
}

void amp_do_error(amp_transport_t *transport, const char *condition, const char *fmt, ...)
{
  va_list ap;
  transport->endpoint.local_error.condition = condition;
  va_start(ap, fmt);
  // XXX: result
  vsnprintf(transport->endpoint.local_error.description, DESCRIPTION, fmt, ap);
  va_end(ap);
  transport->endpoint.local_state = CLOSED;
  fprintf(stderr, "ERROR %s %s\n", condition, transport->endpoint.local_error.description);
  // XXX: need to write close frame if appropriate
}

void amp_do_open(amp_transport_t *transport, amp_list_t *args)
{
  amp_trace(transport, 0, "<- OPEN", args);
  amp_connection_t *conn = transport->connection;
  // TODO: store the state
  conn->endpoint.remote_state = ACTIVE;
}

void amp_do_begin(amp_transport_t *transport, uint16_t ch, amp_list_t *args)
{
  amp_trace(transport, ch, "<- BEGIN", args);
  amp_value_t remote_channel = amp_list_get(args, BEGIN_REMOTE_CHANNEL);
  amp_session_state_t *state;
  if (remote_channel.type == USHORT) {
    // XXX: what if session is NULL?
    state = &transport->sessions[amp_to_uint16(remote_channel)];
  } else {
    amp_session_t *ssn = amp_session(transport->connection);
    state = amp_session_state(transport, ssn);
  }
  amp_map_channel(transport, ch, state);
  state->session->endpoint.remote_state = ACTIVE;
}

amp_link_state_t *amp_find_link(amp_session_state_t *ssn_state, amp_string_t *name)
{
  for (int i = 0; i < ssn_state->session->link_count; i++)
  {
    amp_link_t *link = ssn_state->session->links[i];
    if (!wcsncmp(amp_string_wcs(name), link->name, amp_string_size(name)))
    {
      return amp_link_state(ssn_state, link);
    }
  }
  return NULL;
}

void amp_do_attach(amp_transport_t *transport, uint16_t ch, amp_list_t *args)
{
  amp_trace(transport, ch, "<- ATTACH", args);
  uint32_t handle = amp_to_uint32(amp_list_get(args, ATTACH_HANDLE));
  bool is_sender = amp_to_bool(amp_list_get(args, ATTACH_ROLE));
  amp_string_t *name = amp_to_string(amp_list_get(args, ATTACH_NAME));
  amp_session_state_t *ssn_state = amp_channel_state(transport, ch);
  amp_link_state_t *link_state = amp_find_link(ssn_state, name);
  if (!link_state) {
    amp_link_t *link;
    if (is_sender) {
      link = (amp_link_t *) amp_sender(ssn_state->session, amp_string_wcs(name));
    } else {
      link = (amp_link_t *) amp_receiver(ssn_state->session, amp_string_wcs(name));
    }
    link_state = amp_link_state(ssn_state, link);
  }

  amp_map_handle(ssn_state, handle, link_state);
  link_state->link->endpoint.remote_state = ACTIVE;
  amp_value_t remote_source = amp_list_get(args, ATTACH_SOURCE);
  if (remote_source.type == TAG)
    remote_source = amp_tag_value(amp_to_tag(remote_source));
  amp_value_t remote_target = amp_list_get(args, ATTACH_TARGET);
  if (remote_target.type == TAG)
    remote_target = amp_tag_value(amp_to_tag(remote_target));
  // XXX: dup src/tgt
  if (remote_source.type == LIST)
    link_state->link->remote_source = wcsdup(amp_string_wcs(amp_to_string(amp_list_get(amp_to_list(remote_source), SOURCE_ADDRESS))));
  if (remote_target.type == LIST)
    link_state->link->remote_target = wcsdup(amp_string_wcs(amp_to_string(amp_list_get(amp_to_list(remote_target), TARGET_ADDRESS))));

  if (!is_sender) {
    link_state->delivery_count = amp_to_int32(amp_list_get(args, ATTACH_INITIAL_DELIVERY_COUNT));
  }
}

void amp_do_transfer(amp_transport_t *transport, uint16_t channel, amp_list_t *args, const char *payload_bytes, size_t payload_size)
{
  // XXX: multi transfer

  amp_trace(transport, channel, "<- TRANSFER", args);
  fprintf(stderr, "  PAYLOAD[%u]: %.*s\n", (unsigned int) payload_size, (int) payload_size, payload_bytes);
  amp_session_state_t *ssn_state = amp_channel_state(transport, channel);
  uint32_t handle = amp_to_uint32(amp_list_get(args, TRANSFER_HANDLE));
  amp_link_state_t *link_state = amp_handle_state(ssn_state, handle);
  amp_link_t *link = link_state->link;
  amp_binary_t *tag = amp_to_binary(amp_list_get(args, TRANSFER_DELIVERY_TAG));
  amp_delivery_t *delivery = amp_delivery(link, tag);
  amp_delivery_state_t *state = amp_delivery_buffer_push(&ssn_state->incoming, delivery);
  delivery->context = state;
  // XXX: need to check that state is not null (i.e. we haven't hit the limit)
  amp_sequence_t id = amp_to_int32(amp_list_get(args, TRANSFER_DELIVERY_ID));
  if (id != state->id) {
    // XXX: signal error somehow
  }

  AMP_ENSURE(delivery->bytes, delivery->capacity, payload_size);
  memmove(delivery->bytes, payload_bytes, payload_size);
  delivery->size = payload_size;
}

void amp_do_flow(amp_transport_t *transport, uint16_t channel, amp_list_t *args)
{
  amp_trace(transport, channel, "<- FLOW", args);
  amp_session_state_t *ssn_state = amp_channel_state(transport, channel);

  amp_value_t vhandle = amp_list_get(args, FLOW_HANDLE);
  if (vhandle.type != EMPTY) {
    uint32_t handle = amp_to_uint32(vhandle);
    amp_link_state_t *link_state = amp_handle_state(ssn_state, handle);
    amp_link_t *link = link_state->link;
    if (link->endpoint.type == SENDER) {
      amp_value_t delivery_count = amp_list_get(args, FLOW_DELIVERY_COUNT);
      amp_sequence_t receiver_count;
      if (delivery_count.type == EMPTY) {
        // our initial delivery count
        receiver_count = 0;
      } else {
        receiver_count = amp_to_int32(delivery_count);
      }
      amp_sequence_t link_credit = amp_to_uint32(amp_list_get(args, FLOW_LINK_CREDIT));
      link->credit = receiver_count + link_credit - link_state->delivery_count;
      amp_delivery_t *delivery = amp_current(link);
      if (delivery) amp_work_update(transport->connection, delivery);
    }
  }
}

void amp_do_disposition(amp_transport_t *transport, uint16_t channel, amp_list_t *args)
{
  amp_trace(transport, channel, "<- DISPOSITION", args);
  amp_session_state_t *ssn_state = amp_channel_state(transport, channel);
  bool role = amp_to_bool(amp_list_get(args, DISPOSITION_ROLE));
  amp_sequence_t first = amp_to_int32(amp_list_get(args, DISPOSITION_FIRST));
  amp_sequence_t last = amp_to_int32(amp_list_get(args, DISPOSITION_LAST));
  //bool settled = amp_to_bool(amp_list_get(args, DISPOSITION_SETTLED));
  amp_tag_t *dstate = amp_to_tag(amp_list_get(args, DISPOSITION_STATE));
  uint64_t code = amp_to_uint32(amp_tag_descriptor(dstate));
  amp_disposition_t disp;
  switch (code)
  {
  case ACCEPTED_CODE:
    disp = ACCEPTED;
    break;
  case REJECTED_CODE:
    disp = REJECTED;
    break;
  default:
    // XXX
    fprintf(stderr, "default %lu\n", code);
    disp = 0;
    break;
  }

  amp_delivery_buffer_t *deliveries;
  if (role) {
    deliveries = &ssn_state->outgoing;
  } else {
    deliveries = &ssn_state->incoming;
  }

  amp_sequence_t lwm = amp_delivery_buffer_lwm(deliveries);

  for (amp_sequence_t id = first; id <= last; id++) {
    amp_delivery_state_t *state = amp_delivery_buffer_get(deliveries, id - lwm);
    amp_delivery_t *delivery = state->delivery;
    delivery->remote_state = disp;
    delivery->dirty = true;
    amp_work_update(transport->connection, delivery);
  }
}

void amp_do_detach(amp_transport_t *transport, uint16_t channel, amp_list_t *args)
{
  amp_trace(transport, channel, "<- DETACH", args);

  uint32_t handle = amp_to_uint32(amp_list_get(args, DETACH_HANDLE));
  bool closed = amp_to_bool(amp_list_get(args, DETACH_CLOSED));

  amp_session_state_t *ssn_state = amp_channel_state(transport, channel);
  if (!ssn_state) {
    amp_do_error(transport, "amqp:invalid-field", "no such channel: %u", channel);
    return;
  }
  amp_link_state_t *link_state = amp_handle_state(ssn_state, handle);
  amp_link_t *link = link_state->link;

  link_state->remote_handle = -1;

  if (closed)
  {
    link->endpoint.remote_state = CLOSED;
  } else {
    // TODO: implement
  }
}

void amp_do_end(amp_transport_t *transport, uint16_t channel, amp_list_t *args)
{
  amp_trace(transport, channel, "<- END", args);

  amp_session_state_t *ssn_state = amp_channel_state(transport, channel);
  amp_session_t *session = ssn_state->session;

  ssn_state->remote_channel = -1;
  session->endpoint.remote_state = CLOSED;
}

void amp_do_close(amp_transport_t *transport, amp_list_t *args)
{
  amp_trace(transport, 0, "<- CLOSE", args);

  transport->connection->endpoint.remote_state = CLOSED;
  transport->endpoint.remote_state = CLOSED;
}

void amp_dispatch(amp_transport_t *transport, uint16_t channel,
                  amp_tag_t *performative, const char *payload_bytes,
                  size_t payload_size)
{
  amp_value_t desc = amp_tag_descriptor(performative);
  amp_list_t *args = amp_to_list(amp_tag_value(performative));
  amp_value_t cval = amp_map_get(transport->dispatch, desc);
  uint8_t code = amp_to_uint8(cval);
  switch (code)
  {
  case OPEN_IDX:
    amp_do_open(transport, args);
    break;
  case BEGIN_IDX:
    amp_do_begin(transport, channel, args);
    break;
  case ATTACH_IDX:
    amp_do_attach(transport, channel, args);
    break;
  case TRANSFER_IDX:
    amp_do_transfer(transport, channel, args, payload_bytes, payload_size);
    break;
  case FLOW_IDX:
    amp_do_flow(transport, channel, args);
    break;
  case DISPOSITION_IDX:
    amp_do_disposition(transport, channel, args);
    break;
  case DETACH_IDX:
    amp_do_detach(transport, channel, args);
    break;
  case END_IDX:
    amp_do_end(transport, channel, args);
    break;
  case CLOSE_IDX:
    amp_do_close(transport, args);
    break;
  }
}

ssize_t amp_input(amp_transport_t *transport, char *bytes, size_t available)
{
  if (transport->endpoint.local_state == CLOSED) {
    return EOS;
  }

  if (transport->endpoint.remote_state == CLOSED) {
    amp_do_error(transport, "amqp:connection:framing-error", "data after close");
    return EOS;
  }

  size_t read = 0;
  while (true) {
    amp_frame_t frame;
    size_t n = amp_read_frame(&frame, bytes + read, available);
    if (n) {
      amp_value_t performative;
      ssize_t e = amp_decode(&performative, frame.payload, frame.size);
      if (e < 0) {
        fprintf(stderr, "Error decoding frame: %zi\n", e);
        amp_format(transport->scratch, SCRATCH, amp_value("z", frame.size, frame.payload));
        fprintf(stderr, "%s\n", transport->scratch);
        return e;
      }

      amp_tag_t *perf = amp_to_tag(performative);
      amp_dispatch(transport, frame.channel, perf, frame.payload + e, frame.size - e);
      amp_visit(performative, amp_free_value);

      available -= n;
      read += n;
    } else {
      break;
    }
  }

  return read;
}

void amp_init_frame(amp_transport_t *transport)
{
  amp_list_clear(transport->args);
  transport->payload_bytes = NULL;
  transport->payload_size = 0;
}

void amp_field(amp_transport_t *transport, int index, amp_value_t arg)
{
  int n = amp_list_size(transport->args);
  if (index >= n)
    amp_list_fill(transport->args, EMPTY_VALUE, index - n + 1);
  amp_list_set(transport->args, index, arg);
}

void amp_append_payload(amp_transport_t *transport, const char *data, size_t size)
{
  transport->payload_bytes = data;
  transport->payload_size = size;
}

#define BUF_SIZE (1024*1024)

char *amp_p2op(uint32_t performative)
{
  switch (performative)
  {
  case OPEN_CODE:
    return "OPEN";
  case BEGIN_CODE:
    return "BEGIN";
  case ATTACH_CODE:
    return "ATTACH";
  case TRANSFER_CODE:
    return "TRANSFER";
  case FLOW_CODE:
    return "FLOW";
  case DISPOSITION_CODE:
    return "DISPOSITION";
  case DETACH_CODE:
    return "DETACH";
  case END_CODE:
    return "END";
  case CLOSE_CODE:
    return "CLOSE";
  default:
    return "<UNKNOWN>";
  }
}

void amp_post_frame(amp_transport_t *transport, uint16_t ch, uint32_t performative)
{
  amp_tag_t tag;
  amp_frame_t frame = {0};
  char bytes[BUF_SIZE];
  tag.descriptor = amp_ulong(performative);
  tag.value = amp_from_list(transport->args);
  fprintf(stderr, "-> "); amp_trace(transport, ch, amp_p2op(performative), transport->args);
  // XXX: sizeof
  size_t size = amp_encode(amp_from_tag(&tag), bytes);
  for (int i = 0; i < amp_list_size(transport->args); i++)
    amp_visit(amp_list_get(transport->args, i), amp_free_value);
  if (transport->payload_size) {
    fprintf(stderr, "  PAYLOAD[%u]: %.*s\n", (unsigned int) transport->payload_size,
            (int) transport->payload_size, transport->payload_bytes);
    memmove(bytes + size, transport->payload_bytes, transport->payload_size);
    size += transport->payload_size;
    transport->payload_bytes = NULL;
    transport->payload_size = 0;
  }
  frame.channel = ch;
  frame.payload = bytes;
  frame.size = size;
  size_t n;
  while (!(n = amp_write_frame(transport->output + transport->available,
                               transport->capacity - transport->available, frame))) {
    transport->capacity *= 2;
    transport->output = realloc(transport->output, transport->capacity);
  }
  transport->available += n;
}

void amp_process_conn_setup(amp_transport_t *transport, amp_endpoint_t *endpoint)
{
  if (endpoint->type == CONNECTION)
  {
    if (endpoint->local_state != UNINIT && !transport->open_sent)
    {
      amp_init_frame(transport);
      /*if (container_id)
        amp_field(eng, OPEN_CONTAINER_ID, amp_value("S", container_id));*/
      /*if (hostname)
        amp_field(eng, OPEN_HOSTNAME, amp_value("S", hostname));*/
      amp_post_frame(transport, 0, OPEN_CODE);
      transport->open_sent = true;
    }
  }
}

void amp_process_ssn_setup(amp_transport_t *transport, amp_endpoint_t *endpoint)
{
  if (endpoint->type == SESSION)
  {
    amp_session_t *ssn = (amp_session_t *) endpoint;
    amp_session_state_t *state = amp_session_state(transport, ssn);
    if (endpoint->local_state != UNINIT && state->local_channel == (uint16_t) -1)
    {
      amp_init_frame(transport);
      if ((int16_t) state->remote_channel >= 0)
        amp_field(transport, BEGIN_REMOTE_CHANNEL, amp_value("H", state->remote_channel));
      amp_field(transport, BEGIN_NEXT_OUTGOING_ID, amp_value("I", state->outgoing.next));
      amp_field(transport, BEGIN_INCOMING_WINDOW, amp_value("I", state->incoming.capacity));
      amp_field(transport, BEGIN_OUTGOING_WINDOW, amp_value("I", state->outgoing.capacity));
      // XXX: we use the session id as the outgoing channel, we depend
      // on this for looking up via remote channel
      uint16_t channel = ssn->id;
      amp_post_frame(transport, channel, BEGIN_CODE);
      state->local_channel = channel;
    }
  }
}

void amp_process_link_setup(amp_transport_t *transport, amp_endpoint_t *endpoint)
{
  if (endpoint->type == SENDER || endpoint->type == RECEIVER)
  {
    amp_link_t *link = (amp_link_t *) endpoint;
    amp_session_state_t *ssn_state = amp_session_state(transport, link->session);
    amp_link_state_t *state = amp_link_state(ssn_state, link);
    if (endpoint->local_state != UNINIT && state->local_handle == (uint32_t) -1)
    {
      amp_init_frame(transport);
      amp_field(transport, ATTACH_ROLE, amp_boolean(endpoint->type == RECEIVER));
      amp_field(transport, ATTACH_NAME, amp_value("S", link->name));
      // XXX
      state->local_handle = link->id;
      amp_field(transport, ATTACH_HANDLE, amp_value("I", state->local_handle));
      // XXX
      amp_field(transport, ATTACH_INITIAL_DELIVERY_COUNT, amp_value("I", 0));
      if (link->local_source)
        amp_field(transport, ATTACH_SOURCE, amp_value("B([S])", SOURCE_CODE,
                                                      link->local_source));
      if (link->local_target)
        amp_field(transport, ATTACH_TARGET, amp_value("B([S])", TARGET_CODE,
                                                      link->local_target));
      amp_post_frame(transport, ssn_state->local_channel, ATTACH_CODE);
    }
  }
}

void amp_process_flow_receiver(amp_transport_t *transport, amp_endpoint_t *endpoint)
{
  if (endpoint->type == RECEIVER && endpoint->local_state == ACTIVE)
  {
    amp_receiver_t *rcv = (amp_receiver_t *) endpoint;
    if (rcv->credits) {
      amp_session_state_t *ssn_state = amp_session_state(transport, rcv->link.session);
      amp_link_state_t *state = amp_link_state(ssn_state, &rcv->link);
      state->link_credit += rcv->credits;
      rcv->credits = 0;

      amp_init_frame(transport);
      //amp_field(transport, FLOW_NEXT_INCOMING_ID, amp_value("I", ssn_state->next_incoming_id));
      amp_field(transport, FLOW_INCOMING_WINDOW, amp_value("I", ssn_state->incoming.capacity));
      amp_field(transport, FLOW_NEXT_OUTGOING_ID, amp_value("I", ssn_state->outgoing.next));
      amp_field(transport, FLOW_OUTGOING_WINDOW, amp_value("I", ssn_state->outgoing.capacity));
      amp_field(transport, FLOW_HANDLE, amp_value("I", state->local_handle));
      //amp_field(transport, FLOW_DELIVERY_COUNT, amp_value("I", delivery_count));
      amp_field(transport, FLOW_LINK_CREDIT, amp_value("I", state->link_credit));
      amp_post_frame(transport, ssn_state->local_channel, FLOW_CODE);
    }
  }
}

void amp_post_disp(amp_transport_t *transport, amp_delivery_t *delivery)
{
  amp_link_t *link = delivery->link;
  amp_session_state_t *ssn_state = amp_session_state(transport, link->session);
  // XXX: check for null state
  amp_delivery_state_t *state = delivery->context;
  amp_init_frame(transport);
  amp_field(transport, DISPOSITION_ROLE, amp_boolean(link->endpoint.type == RECEIVER));
  amp_field(transport, DISPOSITION_FIRST, amp_uint(state->id));
  amp_field(transport, DISPOSITION_LAST, amp_uint(state->id));
  // XXX
  amp_field(transport, DISPOSITION_SETTLED, amp_boolean(delivery->local_settled));
  uint64_t code;
  switch(delivery->local_state) {
  case ACCEPTED:
    code = ACCEPTED_CODE;
    break;
  case RELEASED:
    code = RELEASED_CODE;
    break;
    //TODO: rejected and modified (both take extra data which may need to be passed through somehow) e.g. change from enum to discriminated union?
  default:
    code = 0;
  }
  if (code)
    amp_field(transport, DISPOSITION_STATE, amp_value("L([])", code));
  //amp_field(transport, DISPOSITION_BATCHABLE, amp_boolean(batchable));
  amp_post_frame(transport, ssn_state->local_channel, DISPOSITION_CODE);
}

void amp_process_disp_receiver(amp_transport_t *transport, amp_endpoint_t *endpoint)
{
  if (endpoint->type == CONNECTION && !transport->close_sent)
  {
    amp_connection_t *conn = (amp_connection_t *) endpoint;
    amp_delivery_t *delivery = conn->tpwork_head;
    while (delivery)
    {
      amp_link_t *link = delivery->link;
      if (link->endpoint.type == RECEIVER) {
        // XXX: need to prevent duplicate disposition sending
        amp_session_state_t *ssn_state = amp_session_state(transport, link->session);
        if ((int16_t) ssn_state->local_channel >= 0) {
          amp_post_disp(transport, delivery);
        }

        if (delivery->local_settled) {
          amp_full_settle(&ssn_state->incoming, delivery);
        }
      }
      delivery = delivery->tpwork_next;
    }
  }
}

void amp_process_msg_data(amp_transport_t *transport, amp_endpoint_t *endpoint)
{
  if (endpoint->type == CONNECTION && !transport->close_sent)
  {
    amp_connection_t *conn = (amp_connection_t *) endpoint;
    amp_delivery_t *delivery = conn->tpwork_head;
    while (delivery)
    {
      amp_link_t *link = delivery->link;
      if (link->endpoint.type == SENDER) {
        amp_session_state_t *ssn_state = amp_session_state(transport, link->session);
        amp_link_state_t *link_state = amp_link_state(ssn_state, link);
        amp_delivery_state_t *state = delivery->context;
        if (!state) {
          state = amp_delivery_buffer_push(&ssn_state->outgoing, delivery);
          delivery->context = state;
        }
        if (!state->sent && (int16_t) ssn_state->local_channel >= 0 && (int32_t) link_state->local_handle >= 0) {
          amp_init_frame(transport);
          amp_field(transport, TRANSFER_HANDLE, amp_value("I", link_state->local_handle));
          amp_field(transport, TRANSFER_DELIVERY_ID, amp_value("I", state->id));
          amp_field(transport, TRANSFER_DELIVERY_TAG, amp_from_binary(amp_binary_dup(delivery->tag)));
          amp_field(transport, TRANSFER_MESSAGE_FORMAT, amp_value("I", 0));
          if (delivery->bytes) {
            amp_append_payload(transport, delivery->bytes, delivery->size);
            delivery->size = 0;
          }
          amp_post_frame(transport, ssn_state->local_channel, TRANSFER_CODE);
          state->sent = true;
        }
      }
      delivery = delivery->tpwork_next;
    }
  }
}

void amp_process_disp_sender(amp_transport_t *transport, amp_endpoint_t *endpoint)
{
  if (endpoint->type == CONNECTION && !transport->close_sent)
  {
    amp_connection_t *conn = (amp_connection_t *) endpoint;
    amp_delivery_t *delivery = conn->tpwork_head;
    while (delivery)
    {
      amp_link_t *link = delivery->link;
      if (link->endpoint.type == SENDER) {
        // XXX: need to prevent duplicate disposition sending
        amp_session_state_t *ssn_state = amp_session_state(transport, link->session);
        /*if ((int16_t) ssn_state->local_channel >= 0) {
          amp_post_disp(transport, delivery);
          }*/

        if (delivery->local_settled) {
          amp_full_settle(&ssn_state->outgoing, delivery);
        }
      }
      delivery = delivery->tpwork_next;
    }
  }
}

void amp_process_flow_sender(amp_transport_t *transport, amp_endpoint_t *endpoint)
{
  // TODO: implement
}

void amp_process_link_teardown(amp_transport_t *transport, amp_endpoint_t *endpoint)
{
  if (endpoint->type == SENDER || endpoint->type == RECEIVER)
  {
    amp_link_t *link = (amp_link_t *) endpoint;
    amp_session_t *session = link->session;
    amp_session_state_t *ssn_state = amp_session_state(transport, session);
    amp_link_state_t *state = amp_link_state(ssn_state, link);
    if (endpoint->local_state == CLOSED && (int32_t) state->local_handle >= 0) {
      amp_init_frame(transport);
      amp_field(transport, DETACH_HANDLE, amp_value("I", state->local_handle));
      amp_field(transport, DETACH_CLOSED, amp_boolean(true));
      /* XXX: error
    if (condition)
      // XXX: symbol
      amp_engine_field(eng, DETACH_ERROR, amp_value("B([zS])", ERROR_CODE, condition, description)); */
      amp_post_frame(transport, ssn_state->local_channel, DETACH_CODE);
      state->local_handle = -2;
    }
  }
}

void amp_process_ssn_teardown(amp_transport_t *transport, amp_endpoint_t *endpoint)
{
  if (endpoint->type == SESSION)
  {
    amp_session_t *session = (amp_session_t *) endpoint;
    amp_session_state_t *state = amp_session_state(transport, session);
    if (endpoint->local_state == CLOSED && (int16_t) state->local_channel >= 0)
    {
      amp_init_frame(transport);
      /*if (condition)
      // XXX: symbol
      amp_engine_field(eng, DETACH_ERROR, amp_value("B([zS])", ERROR_CODE, condition, description));*/
      amp_post_frame(transport, state->local_channel, END_CODE);
      state->local_channel = -2;
    }
  }
}

void amp_process_conn_teardown(amp_transport_t *transport, amp_endpoint_t *endpoint)
{
  if (endpoint->type == CONNECTION)
  {
    if (endpoint->local_state == CLOSED && !transport->close_sent) {
      amp_init_frame(transport);
      /*if (condition)
      // XXX: symbol
      amp_field(eng, CLOSE_ERROR, amp_value("B([zS])", ERROR_CODE, condition, description));*/
      amp_post_frame(transport, 0, CLOSE_CODE);
      transport->close_sent = true;
    }
  }
}

void amp_clear_phase(amp_transport_t *transport, amp_endpoint_t *endpoint)
{
  amp_clear_modified(transport->connection, endpoint);
}

void amp_phase(amp_transport_t *transport, void (*phase)(amp_transport_t *, amp_endpoint_t *))
{
  amp_connection_t *conn = transport->connection;
  amp_endpoint_t *endpoint = conn->transport_head;
  while (endpoint)
  {
    phase(transport, endpoint);
    endpoint = endpoint->transport_next;
  }
}

void amp_process(amp_transport_t *transport)
{
  amp_phase(transport, amp_process_conn_setup);
  amp_phase(transport, amp_process_ssn_setup);
  amp_phase(transport, amp_process_link_setup);
  amp_phase(transport, amp_process_flow_receiver);
  amp_phase(transport, amp_process_disp_receiver);
  amp_phase(transport, amp_process_msg_data);
  amp_phase(transport, amp_process_disp_sender);
  amp_phase(transport, amp_process_flow_sender);
  amp_phase(transport, amp_process_link_teardown);
  amp_phase(transport, amp_process_ssn_teardown);
  amp_phase(transport, amp_process_conn_teardown);
  amp_phase(transport, amp_clear_phase);

  amp_delivery_t *delivery = transport->connection->tpwork_head;
  while (delivery) {
    amp_clear_tpwork(delivery);
    delivery = delivery->tpwork_next;
  }
}

ssize_t amp_output(amp_transport_t *transport, char *bytes, size_t size)
{
  amp_process(transport);

  if (!transport->available && transport->endpoint.local_state == CLOSED) {
    return EOS;
  }

  int n = transport->available < size ? transport->available : size;
  memmove(bytes, transport->output, n);
  memmove(transport->output, transport->output + n, transport->available - n);
  transport->available -= n;
  // XXX: need to check endpoint for errors
  return n;
}

ssize_t amp_send(amp_sender_t *sender, const char *bytes, size_t n)
{
  amp_delivery_t *current = amp_current(&sender->link);
  if (!current) return -1;
  if (current->bytes) return 0;
  AMP_ENSURE(current->bytes, current->capacity, current->size + n);
  memmove(current->bytes + current->size, bytes, n);
  current->size = +n;
  amp_add_tpwork(current);
  return n;
}

ssize_t amp_recv(amp_receiver_t *receiver, char *bytes, size_t n)
{
  amp_link_t *link = &receiver->link;
  amp_delivery_t *delivery = link->current;
  if (delivery) {
    if (delivery->size) {
      size_t size = n > delivery->size ? delivery->size : n;
      memmove(bytes, delivery->bytes, size);
      memmove(bytes, bytes + size, delivery->size - size);
      delivery->size -= size;
      return size;
    } else {
      return EOM;
    }
  } else {
    // XXX: ?
    return EOM;
  }
}

void amp_flow(amp_receiver_t *receiver, int credits)
{
  receiver->credits += credits;
  amp_modified(receiver->link.session->connection, &receiver->link.endpoint);
}

time_t amp_tick(amp_transport_t *engine, time_t now)
{
  return 0;
}

amp_link_t *amp_link(amp_delivery_t *delivery)
{
  return delivery->link;
}

int amp_local_disp(amp_delivery_t *delivery)
{
  return delivery->local_state;
}

int amp_remote_disp(amp_delivery_t *delivery)
{
  return delivery->remote_state;
}

bool amp_dirty(amp_delivery_t *delivery)
{
  return delivery->dirty;
}

void amp_clean(amp_delivery_t *delivery)
{
  delivery->dirty = false;
  amp_work_update(delivery->link->session->connection, delivery);
}

void amp_disposition(amp_delivery_t *delivery, amp_disposition_t disposition)
{
  delivery->local_state = disposition;
  amp_add_tpwork(delivery);
}

bool amp_writable(amp_delivery_t *delivery)
{
  amp_link_t *link = delivery->link;
  return link->endpoint.type == SENDER && amp_is_current(delivery) && link->credit > 0;
}

bool amp_readable(amp_delivery_t *delivery)
{
  amp_link_t *link = delivery->link;
  return link->endpoint.type == RECEIVER && amp_is_current(delivery);
}
