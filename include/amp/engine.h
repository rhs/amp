#ifndef _AMP_ENGINE_H
#define _AMP_ENGINE_H 1

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

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>

typedef struct amp_error_t amp_error_t;
typedef struct amp_connection_t amp_connection_t;
typedef struct amp_session_t amp_session_t;
typedef struct amp_link_t amp_link_t;
typedef struct amp_transfer_t amp_transfer_t;
typedef struct amp_engine_t amp_engine_t;

// internal
struct amp_error_t {
  int code;
  // ...
};

typedef int32_t sequence_t;

enum OUTCOME {IN_DOUBT, ACCEPTED, RELEASED, REJECTED, MODIFIED};
typedef enum OUTCOME outcome_t;

/* Bottom Half */

amp_engine_t *amp_engine_create(amp_connection_t *connection);
void *amp_engine_destroy(amp_engine_t *engine);
amp_error_t *amp_engine_error(amp_engine_t *engine);

// supplies engine with n bytes of input, engine returns number of
// bytes consumed, or -1 if there was an error
ssize_t amp_engine_input(amp_engine_t *engine, char *src, size_t n);

// supplies engine with n bytes of space for output, engine returns
// number of bytes produced, or -1 if there was an error
ssize_t amp_engine_output(amp_engine_t *engine, char *dst, size_t n);

time_t amp_engine_tick(amp_engine_t *engine, time_t now);

// internal
int amp_engine_open(amp_engine_t *eng, wchar_t *container_id, wchar_t *hostname);
int amp_engine_begin(amp_engine_t *eng, int channel, int remote_channel,
                     sequence_t next_outgoing_id, uint32_t incoming_window,
                     uint32_t outgoing_window);
int amp_engine_attach(amp_engine_t *eng, uint16_t channel, bool role,
                      const wchar_t *name, int handle, sequence_t initial_delivery_count,
                      const wchar_t *source, const wchar_t *target);
int amp_engine_transfer(amp_engine_t *eng, uint16_t channel, int handle,
                        const char *dtag, sequence_t id, const char *bytes,
                        size_t n);
int amp_engine_flow(amp_engine_t *eng, uint16_t channel, sequence_t in_next,
                    int in_win, sequence_t out_next, int out_win, int handle,
                    sequence_t delivery_count, int credit);
int amp_engine_disposition(amp_engine_t* eng, uint16_t channel, bool role,
                           bool batchable, sequence_t first, sequence_t last, 
                           bool settled, uint64_t bytes_transferred, outcome_t outcome);
int amp_engine_detach(amp_engine_t *eng, int channel, int handle,
                      char *condition, wchar_t *description);
int amp_engine_end(amp_engine_t *eng, int channel, char *condition,
                   wchar_t *description);
int amp_engine_close(amp_engine_t *eng, char *condition, wchar_t *description);
amp_connection_t *amp_engine_connection(amp_engine_t *eng);

/* Middle Half */

// ...

/* Top Half */

/* Connections */
amp_connection_t *amp_connection_create();
int amp_connection_destroy(amp_connection_t *conn);
amp_error_t *amp_connection_error(amp_connection_t *connection);

wchar_t *amp_connection_container(amp_connection_t *c);
wchar_t *amp_connection_hostname(amp_connection_t *c);
int amp_connection_open(amp_connection_t *conn);
bool amp_connection_opened(amp_connection_t *conn);
int amp_connection_close(amp_connection_t *conn);
bool amp_connection_opened_by_peer(amp_connection_t *conn);
bool amp_connection_closed_by_peer(amp_connection_t *conn);

void amp_connection_add(amp_connection_t *conn, amp_session_t *ssn);
int amp_connection_sessions(amp_connection_t *conn);
amp_session_t *amp_connection_get_session(amp_connection_t *conn, int index);

enum ENDPOINT_TYPE {CONNECTION_ENDPOINT, SESSION_ENDPOINT, LINK_ENDPOINT, NULL_ENDPOINT};
typedef struct {
  enum ENDPOINT_TYPE type;
  union {
    amp_connection_t* as_connection;
    amp_session_t* as_session;
    amp_link_t* as_link;
  } value;
} amp_endpoint_t;

amp_link_t *amp_connection_next_link(amp_connection_t *conn);
amp_endpoint_t amp_connection_next_endpoint(amp_connection_t *conn);

// internal
time_t amp_connection_tick(amp_connection_t *conn, amp_engine_t *eng, time_t now);
void amp_connection_open_received(amp_connection_t *conn);
void amp_connection_close_received(amp_connection_t *conn);
void amp_connection_add_pending_link(amp_connection_t *conn, amp_link_t* link);

/* Sessions */
amp_session_t *amp_session_create();
int amp_session_destroy(amp_session_t *session);
amp_error_t *amp_session_error(amp_session_t *session);

int amp_session_begin(amp_session_t *session);
int amp_session_end(amp_session_t *session);
bool amp_session_active(amp_session_t *session);
bool amp_session_begun_by_peer(amp_session_t *session);
bool amp_session_ended_by_peer(amp_session_t *session);

void amp_session_add(amp_session_t *ssn, amp_link_t *link);
int amp_session_links(amp_session_t *session);
amp_link_t *amp_session_get_link(amp_session_t *session, int index);

// internal
int amp_session_channel(amp_session_t *session);
void amp_session_bind(amp_session_t *ssn, amp_connection_t *conn, int channel);
void amp_session_tick(amp_session_t *ssn, amp_engine_t *eng);
int amp_session_flow(amp_session_t *ssn, amp_engine_t *eng, amp_link_t *link, int credit);
void amp_session_begin_received(amp_session_t* session, sequence_t initial_transfer_count);
void amp_session_end_received(amp_session_t* session);
amp_endpoint_t amp_session_next_endpoint(amp_session_t* session);
void amp_session_set_remote_channel(amp_session_t* session, int remote_channel);

amp_transfer_t *amp_session_record(amp_session_t* session, amp_link_t* link,
                                   const char* delivery_tag,
                                   const char* bytes, size_t size);
const amp_transfer_t* amp_session_unsettled(amp_session_t* session, amp_link_t* link, const char* delivery_tag);
bool amp_session_update_unsettled(amp_session_t* session, amp_link_t* link, const char* delivery_tag, outcome_t outcome, bool settled);
void amp_session_disposition_received(amp_session_t* session, bool role, sequence_t first, sequence_t last, outcome_t outcome, bool settled);
const char *amp_transfer_delivery_tag(const amp_transfer_t* transfer);
bool amp_transfer_is_settled_locally(const amp_transfer_t* transfer);
bool amp_transfer_is_settled_remotely(const amp_transfer_t* transfer);
outcome_t amp_transfer_local_outcome(const amp_transfer_t* transfer);
outcome_t amp_transfer_remote_outcome(const amp_transfer_t* transfer);

/* Links */
amp_link_t *amp_link_create(bool sender, const wchar_t* name);
int amp_link_destroy(amp_link_t *link);
amp_error_t *amp_link_error(amp_link_t *link);
bool amp_link_sender(amp_link_t *link);
void amp_link_set_source(amp_link_t *link, const wchar_t *source);
void amp_link_set_target(amp_link_t *link, const wchar_t *target);
const wchar_t *amp_link_get_source(amp_link_t *link);
const wchar_t *amp_link_get_target(amp_link_t *link);

amp_session_t* amp_link_session(amp_link_t *link);
int amp_link_open(amp_link_t *link, ...);
int amp_link_attach(amp_link_t *link);
bool amp_link_attached(amp_link_t *link);
int amp_link_capacity(amp_link_t *link);
int amp_link_disposition(amp_link_t *link, ...);
int amp_link_settle(amp_link_t *link, ...);
int amp_link_detach(amp_link_t *link);
int amp_link_close(amp_link_t *link, ...);
bool amp_link_attached_by_peer(amp_link_t *link);
bool amp_link_detached_by_peer(amp_link_t *link);
const amp_transfer_t* amp_link_unsettled(amp_link_t* link, const char* delivery_tag);
bool amp_link_update_unsettled(amp_link_t* link, const char* delivery_tag, outcome_t outcome, bool settled);

// sender
int amp_link_send(amp_link_t *link, const char *delivery_tag, const char *bytes, size_t n);
int amp_link_drained(amp_link_t *link);

// receiver
int amp_link_flow(amp_link_t *link, int credit);
int amp_link_drain(amp_link_t *link);
int amp_link_pending(amp_link_t *link);
int amp_link_get(amp_link_t *link, const char **bytes, size_t *n);


// internal
void amp_link_bind(amp_link_t *link, amp_session_t *ssn, int handle);
void amp_link_tick(amp_link_t *link, amp_engine_t *eng);
void amp_link_received(amp_link_t *link, const char *bytes, size_t n,
                       const char* delivery_tag, sequence_t transfer_id);
void amp_link_attach_received(amp_link_t *link);
void amp_link_detach_received(amp_link_t *link);
bool amp_link_has_pending_work(amp_link_t* link);
int amp_link_handle(amp_link_t* link);
sequence_t amp_link_delivery_count(amp_link_t *link);

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

struct amp_transfer_buffer_t {
  amp_transfer_t* transfers;
  sequence_t next;
  size_t capacity;
  size_t head;
  size_t size;
};
typedef struct amp_transfer_buffer_t amp_transfer_buffer_t;

void amp_transfer_buffer_init(amp_transfer_buffer_t* o, sequence_t next, size_t size);
size_t amp_transfer_buffer_size(amp_transfer_buffer_t* o);
size_t amp_transfer_buffer_available(amp_transfer_buffer_t *o);
amp_transfer_t *amp_transfer_buffer_get(amp_transfer_buffer_t* o, size_t index);
bool amp_transfer_buffer_empty(amp_transfer_buffer_t* o);
amp_transfer_t *amp_transfer_buffer_head(amp_transfer_buffer_t* o);
bool amp_transfer_buffer_pop(amp_transfer_buffer_t* o);
amp_transfer_t *amp_transfer_buffer_push(amp_transfer_buffer_t* o,
                                    const char* delivery_tag, amp_link_t* link);
amp_transfer_t* amp_transfer_buffer_tail(amp_transfer_buffer_t* o);

#endif /* engine.h */
