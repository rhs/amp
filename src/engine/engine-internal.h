#ifndef _AMP_ENGINE_INTERNAL_H
#define _AMP_ENGINE_INTERNAL_H 1

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
#include "../util.h"

#define DESCRIPTION (1024)

struct amp_error_t {
  const char *condition;
  char description[DESCRIPTION];
  amp_map_t *info;
};

struct amp_endpoint_t {
  amp_endpoint_type_t type;
  amp_endpoint_state_t local_state, remote_state;
  amp_error_t local_error, remote_error;
  amp_endpoint_t *endpoint_next;
  amp_endpoint_t *endpoint_prev;
  amp_endpoint_t *transport_next;
  amp_endpoint_t *transport_prev;
  bool modified;
};

typedef int32_t amp_sequence_t;

typedef struct {
  amp_delivery_t *delivery;
  amp_sequence_t id;
  bool sent;
} amp_delivery_state_t;

typedef struct {
  amp_sequence_t next;
  size_t capacity;
  size_t head;
  size_t size;
  amp_delivery_state_t *deliveries;
} amp_delivery_buffer_t;

typedef struct {
  amp_link_t *link;
  // XXX: stop using negative numbers
  uint32_t local_handle;
  uint32_t remote_handle;
  amp_sequence_t delivery_count;
  // XXX: this is only used for receiver
  amp_sequence_t link_credit;
} amp_link_state_t;

typedef struct {
  amp_session_t *session;
  // XXX: stop using negative numbers
  uint16_t local_channel;
  uint16_t remote_channel;
  amp_delivery_buffer_t incoming;
  amp_delivery_buffer_t outgoing;
  amp_link_state_t *links;
  size_t link_capacity;
  amp_link_state_t **handles;
  size_t handle_capacity;
} amp_session_state_t;

#define SCRATCH (1024)

struct amp_transport_t {
  amp_endpoint_t endpoint;
  amp_connection_t *connection;
  amp_map_t *dispatch;
  amp_list_t *args;
  const char* payload_bytes;
  size_t payload_size;
  char *output;
  size_t available;
  size_t capacity;
  bool open_sent;
  bool close_sent;
  amp_session_state_t *sessions;
  size_t session_capacity;
  amp_session_state_t **channels;
  size_t channel_capacity;
  char scratch[SCRATCH];
};

struct amp_connection_t {
  amp_endpoint_t endpoint;
  amp_endpoint_t *endpoint_head;
  amp_endpoint_t *endpoint_tail;
  amp_endpoint_t *transport_head;
  amp_endpoint_t *transport_tail;
  amp_session_t **sessions;
  size_t session_capacity;
  size_t session_count;
  amp_transport_t *transport;
  amp_delivery_t *work_head;
  amp_delivery_t *work_tail;
  amp_delivery_t *tpwork_head;
  amp_delivery_t *tpwork_tail;
};

struct amp_session_t {
  amp_endpoint_t endpoint;
  amp_connection_t *connection;
  amp_link_t **links;
  size_t link_capacity;
  size_t link_count;
  size_t id;
};

struct amp_link_t {
  amp_endpoint_t endpoint;
  wchar_t *name;
  amp_session_t *session;
  wchar_t *local_source;
  wchar_t *local_target;
  wchar_t *remote_source;
  wchar_t *remote_target;
  amp_delivery_t *head;
  amp_delivery_t *tail;
  amp_delivery_t *current;
  amp_delivery_t *settled_head;
  amp_delivery_t *settled_tail;
  amp_sequence_t credit;
  size_t id;
};

struct amp_sender_t {
  amp_link_t link;
};

struct amp_receiver_t {
  amp_link_t link;
  amp_sequence_t credits;
};

struct amp_delivery_t {
  amp_link_t *link;
  amp_binary_t tag;
  int local_state;
  int remote_state;
  bool local_settled;
  bool remote_settled;
  bool dirty;
  amp_delivery_t *link_next;
  amp_delivery_t *link_prev;
  amp_delivery_t *work_next;
  amp_delivery_t *work_prev;
  bool work;
  amp_delivery_t *tpwork_next;
  amp_delivery_t *tpwork_prev;
  bool tpwork;
  void *context;
};

void amp_destroy_connection(amp_connection_t *connection);
void amp_destroy_transport(amp_transport_t *transport);
void amp_destroy_session(amp_session_t *session);
void amp_destroy_sender(amp_sender_t *sender);
void amp_destroy_receiver(amp_receiver_t *receiver);

void amp_link_dump(amp_link_t *link);

#define AMP_ENSURE(ARRAY, CAPACITY, COUNT)                      \
  while ((CAPACITY) < (COUNT)) {                                \
    (CAPACITY) = (CAPACITY) ? 2 * (CAPACITY) : 16;              \
    (ARRAY) = realloc((ARRAY), (CAPACITY) * sizeof (*(ARRAY))); \
  }                                                             \

#define AMP_ENSUREZ(ARRAY, CAPACITY, COUNT)                \
  {                                                        \
    size_t _old_capacity = (CAPACITY);                     \
    AMP_ENSURE((ARRAY), (CAPACITY), (COUNT));              \
    memset((ARRAY) + _old_capacity, 0,                     \
           sizeof(*(ARRAY))*((CAPACITY) - _old_capacity)); \
  }

void amp_dump(amp_connection_t *conn);

#endif /* engine-internal.h */
