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
#define _GNU_SOURCE
#include <wchar.h>

#include <stdio.h>

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

amp_error_t amp_local_error(amp_endpoint_t *endpoint)
{
  return endpoint->local_error;
}

amp_error_t amp_remote_error(amp_endpoint_t *endpoint)
{
  return endpoint->remote_error;
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
  if (delivery->tag.bytes) {
    free(delivery->tag.bytes);
    delivery->tag.bytes = NULL;
    delivery->tag.size = 0;
  }
}

void amp_free_deliveries(amp_delivery_t *delivery)
{
  while (delivery)
  {
    amp_delivery_t *next = delivery->link_next;
    amp_clear_tag(delivery);
    free(delivery);
    delivery = next;
  }
}

void amp_dump_deliveries(amp_delivery_t *delivery)
{
  if (delivery) {
    while (delivery)
    {
      printf("%p(%.*s)", (void *) delivery, (int) delivery->tag.size, delivery->tag.bytes);
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
  endpoint->local_error = (amp_error_t) {0};
  endpoint->remote_error = (amp_error_t) {0};
  endpoint->endpoint_next = NULL;
  endpoint->endpoint_prev = NULL;
  endpoint->transport_next = NULL;
  endpoint->transport_prev = NULL;
  endpoint->modified = false;

  LL_ADD_PFX(conn->endpoint_head, conn->endpoint_tail, endpoint, endpoint_);
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
  return delivery->work_next;
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
  if (delivery == current) {
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

void amp_add_tpwork(amp_connection_t *connection, amp_delivery_t *delivery)
{
  if (!delivery->tpwork)
  {
    LL_ADD_PFX(connection->tpwork_head, connection->tpwork_tail, delivery, tpwork_);
    delivery->tpwork = true;
  }
}

void amp_clear_tpwork(amp_connection_t *connection, amp_delivery_t *delivery)
{
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

void amp_open(amp_connection_t *conn)
{
  // TODO: do we care about the current state?
  conn->endpoint.local_state = ACTIVE;
  amp_modified(conn, &conn->endpoint);
}

void amp_close(amp_connection_t *conn)
{
  // TODO: do we care about the current state?
  conn->endpoint.local_state = CLOSED;
  amp_modified(conn, &conn->endpoint);
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

void amp_begin(amp_session_t *ssn)
{
  // TODO: do we care about the current state?
  ssn->endpoint.local_state = ACTIVE;
  amp_modified(ssn->connection, &ssn->endpoint);
}

void amp_end(amp_session_t *ssn)
{
  // TODO: do we care about the current state?
  ssn->endpoint.local_state = CLOSED;
  amp_modified(ssn->connection, &ssn->endpoint);
}

  /*  amp_map_set(MAP, amp_symbol(AMP_HEAP, NAME ## _SYM), amp_ulong(AMP_HEAP, NAME)); \ */
#define __DISPATCH(MAP, NAME)                                           \
  amp_map_set(MAP, amp_ulong(NAME ## _CODE), amp_ulong(NAME ## _IDX))

void amp_transport_init(amp_transport_t *transport)
{
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
                                                    .remote_channel=-1,
                                                    .incoming_window=1024};
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

wchar_t *wcsdup(wchar_t *src)
{
  if (src) {
    wchar_t *dest = malloc((wcslen(src)+1)*sizeof(wchar_t));
    return wcscpy(dest, src);
  } else {
    return src;
  }
}

void amp_link_init(amp_link_t *link, int type, amp_session_t *session, wchar_t *name)
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
}

void amp_set_source(amp_link_t *link, wchar_t *source)
{
  link->local_source = source;
}

void amp_set_target(amp_link_t *link, wchar_t *target)
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

amp_sender_t *amp_sender(amp_session_t *session, wchar_t *name)
{
  amp_sender_t *snd = malloc(sizeof(amp_sender_t));
  amp_link_init(&snd->link, SENDER, session, name);
  return snd;
}

amp_receiver_t *amp_receiver(amp_session_t *session, wchar_t *name)
{
  amp_receiver_t *rcv = malloc(sizeof(amp_receiver_t));
  amp_link_init(&rcv->link, RECEIVER, session, name);
  rcv->credits = 0;
  return rcv;
}

void amp_attach(amp_link_t *link)
{
  // TODO: do we care about the current state?
  link->endpoint.local_state = ACTIVE;
  amp_modified(link->session->connection, &link->endpoint);
}

void amp_detach(amp_link_t *link)
{
  // TODO: do we care about the current state?
  link->endpoint.local_state = CLOSED;
  amp_modified(link->session->connection, &link->endpoint);
}

amp_delivery_t *amp_delivery(amp_link_t *link, amp_binary_t tag)
{
  amp_delivery_t *delivery = link->settled_head;
  LL_POP_PFX(link->settled_head, link->settled_tail, link_);
  if (!delivery) delivery = malloc(sizeof(amp_delivery_t));
  delivery->link = link;
  delivery->tag = amp_binary_dup(tag);
  delivery->state = 0;
  LL_ADD_PFX(link->head, link->tail, delivery, link_);
  delivery->work_next = NULL;
  delivery->work_prev = NULL;
  delivery->work = false;
  delivery->tpwork_next = NULL;
  delivery->tpwork_prev = NULL;
  delivery->tpwork = false;
  delivery->context = NULL;

  if (!link->current)
    link->current = delivery;

  amp_work_update(link->session->connection, delivery);

  return delivery;
}

amp_binary_t amp_delivery_tag(amp_delivery_t *delivery)
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
    amp_add_tpwork(link->session->connection, link->current);
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

void amp_settle(amp_delivery_t *delivery)
{
  amp_link_t *link = delivery->link;
  LL_REMOVE_PFX(link->head, link->tail, delivery, link_);
  // TODO: what if we settle the current delivery?
  LL_ADD_PFX(link->settled_head, link->settled_tail, delivery, link_);
  amp_clear_tag(delivery);
}

void amp_do_open(amp_transport_t *transport, amp_list_t *args)
{
  printf("OPEN!!!! %s\n", amp_aformat(amp_from_list(args)));
  amp_connection_t *conn = transport->connection;
  // TODO: store the state
  conn->endpoint.remote_state = ACTIVE;
}

void amp_do_begin(amp_transport_t *transport, uint16_t ch, amp_list_t *args)
{
  printf("BEGIN!!!! %s\n", amp_aformat(amp_from_list(args)));
  amp_value_t remote_channel = amp_list_get(args, BEGIN_REMOTE_CHANNEL);
  amp_session_state_t *state;
  if (remote_channel.type == USHORT) {
    // XXX: what if session is NULL?
    state = &transport->sessions[amp_to_uint16(remote_channel)];
  } else {
    amp_session_t* ssn = amp_session(transport->connection);
    state = amp_session_state(transport, ssn);
  }
  amp_map_channel(transport, ch, state);
  state->session->endpoint.remote_state = ACTIVE;
}

amp_link_state_t *amp_find_link(amp_session_state_t *ssn_state, amp_string_t name)
{
  for (int i = 0; i < ssn_state->session->link_count; i++)
  {
    amp_link_t *link = ssn_state->session->links[i];
    if (!wcsncmp(name.wcs, link->name, name.size))
    {
      return amp_link_state(ssn_state, link);
    }
  }
  return NULL;
}

void amp_do_attach(amp_transport_t *transport, uint16_t ch, amp_list_t *args)
{
  printf("ATTACH!!!! %s\n", amp_aformat(amp_from_list(args)));
  uint32_t handle = amp_to_uint32(amp_list_get(args, ATTACH_HANDLE));
  bool is_sender = amp_to_bool(amp_list_get(args, ATTACH_ROLE));
  amp_string_t name = amp_to_string(amp_list_get(args, ATTACH_NAME));
  amp_session_state_t *ssn_state = amp_channel_state(transport, ch);
  amp_link_state_t *link_state = amp_find_link(ssn_state, name);
  if (!link_state) {
    amp_link_t *link;
    if (is_sender) {
      link = (amp_link_t *) amp_sender(ssn_state->session, name.wcs);
    } else {
      link = (amp_link_t *) amp_receiver(ssn_state->session, name.wcs);
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
    link_state->link->remote_source = amp_to_string(amp_list_get(amp_to_list(remote_source), SOURCE_ADDRESS)).wcs;
  if (remote_target.type == LIST)
    link_state->link->remote_target = amp_to_string(amp_list_get(amp_to_list(remote_target), TARGET_ADDRESS)).wcs;

  if (!is_sender) {
    link_state->delivery_count = amp_to_int32(amp_list_get(args, ATTACH_INITIAL_DELIVERY_COUNT));
  }
}

amp_delivery_state_t *amp_delivery_state(amp_delivery_t *delivery)
{
  amp_delivery_state_t *state = delivery->context;
  if (!state) {
    // XXX: how do we free this?
    state = malloc(sizeof(amp_delivery_state_t));
    delivery->context = state;
    state->delivery = delivery;
  }
  return state;
}

void amp_do_transfer(amp_transport_t *transport, uint16_t channel, amp_list_t *args, const char* payload_bytes, size_t payload_size)
{
  // XXX: multi transfer
  printf("TRANSFER: %s payload[%u]: %.*s\n", amp_aformat(amp_from_list(args)),
         (unsigned int) payload_size, (int) payload_size, payload_bytes);
  amp_session_state_t *ssn_state = amp_channel_state(transport, channel);
  uint32_t handle = amp_to_uint32(amp_list_get(args, TRANSFER_HANDLE));
  amp_link_state_t *link_state = amp_handle_state(ssn_state, handle);
  amp_link_t *link = link_state->link;
  amp_binary_t tag = amp_to_binary(amp_list_get(args, TRANSFER_DELIVERY_TAG));
  amp_delivery_t *delivery = amp_delivery(link, tag);
  amp_delivery_state_t *state = amp_delivery_state(delivery);
  state->id = amp_to_int32(amp_list_get(args, TRANSFER_DELIVERY_ID));
}

void amp_do_flow(amp_transport_t *transport, uint16_t channel, amp_list_t *args)
{
  printf("FLOW!!!! %s\n", amp_aformat(amp_from_list(args)));
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

void amp_do_detach(amp_transport_t *transport, uint16_t channel, amp_list_t *args)
{
  printf("DETACH!!!! %s\n", amp_aformat(amp_from_list(args)));

  uint32_t handle = amp_to_uint32(amp_list_get(args, DETACH_HANDLE));
  bool closed = amp_to_bool(amp_list_get(args, DETACH_CLOSED));

  amp_session_state_t *ssn_state = amp_channel_state(transport, channel);
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
  printf("END!!!! %s\n", amp_aformat(amp_from_list(args)));

  amp_session_state_t *ssn_state = amp_channel_state(transport, channel);
  amp_session_t *session = ssn_state->session;

  ssn_state->remote_channel = -1;
  session->endpoint.remote_state = CLOSED;
}

void amp_do_close(amp_transport_t *transport, amp_list_t *args)
{
  printf("CLOSE!!!! %s\n", amp_aformat(amp_from_list(args)));

  transport->connection->endpoint.remote_state = CLOSED;
}

void amp_dispatch(amp_transport_t *transport, uint16_t channel,
                  amp_tag_t *performative, const char* payload_bytes,
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
    //    amp_engine_do_disposition(e, channel, args);
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
  size_t read = 0;
  while (true) {
    amp_frame_t frame;
    size_t n = amp_read_frame(&frame, bytes + read, available);
    if (n) {
      amp_value_t performative;
      ssize_t e = amp_decode(&performative, frame.payload, frame.size);
      if (e < 0) {
        fprintf(stderr, "Error decoding frame: %zi\n", e);
        fprintf(stderr, "%s\n", amp_aformat(amp_value("z", frame.size, frame.payload)));
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
  transport->payload_size = 0;
  transport->payload_bytes = 0;
}

void amp_field(amp_transport_t *transport, int index, amp_value_t arg)
{
  int n = amp_list_size(transport->args);
  if (index >= n)
    amp_list_fill(transport->args, EMPTY_VALUE, index - n + 1);
  amp_list_set(transport->args, index, arg);
}

void amp_append_payload(amp_transport_t *transport, const char* data, size_t size)
{
  transport->payload_bytes = data;
  transport->payload_size = size;
}

#define BUF_SIZE (1024*1024)

void amp_post_frame(amp_transport_t *transport, uint16_t ch, uint32_t performative)
{
  amp_tag_t tag;
  amp_frame_t frame = {0};
  char bytes[BUF_SIZE];
  tag.descriptor = amp_ulong(performative);
  tag.value = amp_from_list(transport->args);
  fprintf(stderr, "POST: %s\n", amp_aformat(amp_from_tag(&tag)));
  // XXX: sizeof
  size_t size = amp_encode(amp_from_tag(&tag), bytes);
  if (transport->payload_size) {
    memmove(bytes + size, transport->payload_bytes, transport->payload_size);
    size += transport->payload_size;
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
      if (state->remote_channel != -1)
        amp_field(transport, BEGIN_REMOTE_CHANNEL, amp_value("H", state->remote_channel));
      amp_field(transport, BEGIN_NEXT_OUTGOING_ID, amp_value("I", state->next_outgoing_id));
      amp_field(transport, BEGIN_INCOMING_WINDOW, amp_value("I", state->incoming_window));
      amp_field(transport, BEGIN_OUTGOING_WINDOW, amp_value("I", state->outgoing_window));
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
      amp_field(transport, FLOW_INCOMING_WINDOW, amp_value("I", ssn_state->incoming_window));
      amp_field(transport, FLOW_NEXT_OUTGOING_ID, amp_value("I", ssn_state->next_outgoing_id));
      amp_field(transport, FLOW_OUTGOING_WINDOW, amp_value("I", ssn_state->outgoing_window));
      amp_field(transport, FLOW_HANDLE, amp_value("I", state->local_handle));
      //amp_field(transport, FLOW_DELIVERY_COUNT, amp_value("I", delivery_count));
      amp_field(transport, FLOW_LINK_CREDIT, amp_value("I", state->link_credit));
      amp_post_frame(transport, ssn_state->local_channel, FLOW_CODE);
    }
  }
}

void amp_process_disp_receiver(amp_transport_t *transport, amp_endpoint_t *endpoint)
{
  if (endpoint->type == CONNECTION && endpoint->local_state == ACTIVE)
  {
    amp_connection_t *conn = (amp_connection_t *) endpoint;
    amp_delivery_t *delivery = conn->tpwork_head;
    while (delivery)
    {
      amp_link_t *link = delivery->link;
      if (link->endpoint.type == RECEIVER) {
        amp_delivery_state_t *state = amp_delivery_state(delivery);
        amp_session_state_t *ssn_state = amp_session_state(transport, link->session);
        amp_init_frame(transport);
        amp_field(transport, DISPOSITION_ROLE, amp_boolean(link->endpoint.type == RECEIVER));
        amp_field(transport, DISPOSITION_FIRST, amp_uint(state->id));
        amp_field(transport, DISPOSITION_LAST, amp_uint(state->id));
        // XXX
        amp_field(transport, DISPOSITION_SETTLED, amp_boolean(false));
        uint8_t code;
        switch(delivery->state) {
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
          amp_field(transport, DISPOSITION_STATE, amp_value("B([])", code));
        //amp_field(transport, DISPOSITION_BATCHABLE, amp_boolean(batchable));
        amp_post_frame(transport, ssn_state->local_channel, DISPOSITION_CODE);
      }
      delivery = delivery->tpwork_next;
    }
  }
}

void amp_process_msg_data(amp_transport_t *transport, amp_endpoint_t *endpoint)
{
  if (endpoint->type == CONNECTION && endpoint->local_state == ACTIVE)
  {
    amp_connection_t *conn = (amp_connection_t *) endpoint;
    amp_delivery_t *delivery = conn->tpwork_head;
    while (delivery)
    {
      amp_link_t *link = delivery->link;
      if (link->endpoint.type == SENDER) {
        amp_session_state_t *ssn_state = amp_session_state(transport, link->session);
        amp_link_state_t *link_state = amp_link_state(ssn_state, link);
        amp_init_frame(transport);
        amp_field(transport, TRANSFER_HANDLE, amp_value("I", link_state->local_handle));
        amp_field(transport, TRANSFER_DELIVERY_ID, amp_value("I", ssn_state->next_outgoing_id++));
        amp_field(transport, TRANSFER_DELIVERY_TAG, amp_from_binary(delivery->tag));
        amp_field(transport, TRANSFER_MESSAGE_FORMAT, amp_value("I", 0));
        //amp_append_payload(transport, bytes, n);
        amp_post_frame(transport, ssn_state->local_channel, TRANSFER_CODE);
      }
      delivery = delivery->tpwork_next;
    }
  }
}

void amp_process_disp_sender(amp_transport_t *transport, amp_endpoint_t *endpoint)
{
  // TODO: implement
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
    if (endpoint->local_state == CLOSED && state->local_handle != -1) {
      amp_init_frame(transport);
      amp_field(transport, DETACH_HANDLE, amp_value("I", state->local_handle));
      /* XXX: error
    if (condition)
      // XXX: symbol
      amp_engine_field(eng, DETACH_ERROR, amp_value("B([zS])", ERROR_CODE, condition, description)); */
      amp_post_frame(transport, ssn_state->local_channel, DETACH_CODE);
      state->local_handle = -1;
    }
  }
}

void amp_process_ssn_teardown(amp_transport_t *transport, amp_endpoint_t *endpoint)
{
  if (endpoint->type == SESSION)
  {
    amp_session_t *session = (amp_session_t *) endpoint;
    amp_session_state_t *state = amp_session_state(transport, session);
    if (endpoint->local_state == CLOSED && state->local_channel != -1)
    {
      amp_init_frame(transport);
      /*if (condition)
      // XXX: symbol
      amp_engine_field(eng, DETACH_ERROR, amp_value("B([zS])", ERROR_CODE, condition, description));*/
      amp_post_frame(transport, state->local_channel, END_CODE);
      state->local_channel = -1;
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
}

ssize_t amp_output(amp_transport_t *transport, char *bytes, size_t size)
{
  amp_process(transport);

  int n = transport->available < size ? transport->available : size;
  memmove(bytes, transport->output, n);
  memmove(transport->output, transport->output + n, transport->available - n);
  transport->available -= n;
  // XXX: need to check endpoint for errors
  return n;
}

ssize_t amp_send(amp_sender_t *sender, char *bytes, size_t n)
{
  // TODO: implement
  return 0;
}

ssize_t amp_recv(amp_receiver_t *receiver, char *bytes, size_t n)
{
  amp_link_t *link = &receiver->link;
  amp_delivery_t *delivery = link->current;
  if (delivery->link_next) {
    // do nothing
  }
  return EOM;
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

void amp_disposition(amp_delivery_t *delivery, amp_disposition_t disposition)
{
  delivery->state = disposition;
  amp_connection_t *connection = delivery->link->session->connection;
  amp_add_tpwork(connection, delivery);
  amp_modified(connection, &connection->endpoint);
}
