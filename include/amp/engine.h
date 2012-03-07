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
#include <stddef.h>
#include <sys/types.h>
#include <amp/value.h>

typedef struct amp_error_t amp_error_t;
typedef struct amp_endpoint_t amp_endpoint_t;
typedef struct amp_transport_t amp_transport_t;
typedef struct amp_connection_t amp_connection_t;
typedef struct amp_session_t amp_session_t;
typedef struct amp_link_t amp_link_t;
typedef struct amp_sender_t amp_sender_t;
typedef struct amp_receiver_t amp_receiver_t;
typedef struct amp_delivery_t amp_delivery_t;

typedef enum amp_endpoint_state_t {UNINIT=1, ACTIVE=2, CLOSED=4} amp_endpoint_state_t;
typedef enum amp_endpoint_type_t {CONNECTION=1, TRANSPORT=2, SESSION=3, SENDER=4, RECEIVER=5} amp_endpoint_type_t;
typedef enum amp_disposition_t {RECEIVED=1, ACCEPTED=2, REJECTED=3, RELEASED=4, MODIFIED=5} amp_disposition_t;

/* Currently the way inheritence is done it is safe to "upcast" from
   amp_{transport,connection,session,link,sender,or receiver}_t to
   amp_endpoint_t and to "downcast" based on the endpoint type. I'm
   not sure if this should be part of the ABI or not. */

// endpoint
amp_endpoint_type_t amp_endpoint_type(amp_endpoint_t *endpoint);
amp_endpoint_state_t amp_local_state(amp_endpoint_t *endpoint);
amp_endpoint_state_t amp_remote_state(amp_endpoint_t *endpoint);
amp_error_t *amp_local_error(amp_endpoint_t *endpoint);
amp_error_t *amp_remote_error(amp_endpoint_t *endpoint);
void amp_destroy(amp_endpoint_t *endpoint);
void amp_open(amp_endpoint_t *endpoint);
void amp_close(amp_endpoint_t *endpoint);

// connection
amp_connection_t *amp_connection();
amp_delivery_t *amp_work_head(amp_connection_t *connection);
amp_delivery_t *amp_work_next(amp_delivery_t *delivery);

amp_session_t *amp_session(amp_connection_t *connection);
amp_transport_t *amp_transport(amp_connection_t *connection);

void amp_endpoint_mask(amp_connection_t *connection, amp_endpoint_state_t local, amp_endpoint_state_t remote);
amp_endpoint_t *amp_endpoint_head(amp_connection_t *connection,
                                  amp_endpoint_state_t local,
                                  amp_endpoint_state_t remote);
amp_endpoint_t *amp_endpoint_next(amp_endpoint_t *endpoint,
                                  amp_endpoint_state_t local,
                                  amp_endpoint_state_t remote);

// transport
#define EOS (-1)
ssize_t amp_input(amp_transport_t *transport, char *bytes, size_t available);
ssize_t amp_output(amp_transport_t *transport, char *bytes, size_t size);
time_t amp_tick(amp_transport_t *engine, time_t now);

// session
amp_sender_t *amp_sender(amp_session_t *session, const wchar_t *name);
amp_receiver_t *amp_receiver(amp_session_t *session, const wchar_t *name);

// link
amp_session_t *amp_get_session(amp_link_t *link);
void amp_set_source(amp_link_t *link, const wchar_t *source);
void amp_set_target(amp_link_t *link, const wchar_t *target);
wchar_t *amp_remote_source(amp_link_t *link);
wchar_t *amp_remote_target(amp_link_t *link);
amp_delivery_t *amp_delivery(amp_link_t *link, amp_binary_t *tag);
amp_delivery_t *amp_current(amp_link_t *link);
bool amp_advance(amp_link_t *link);

amp_delivery_t *amp_unsettled_head(amp_link_t *link);
amp_delivery_t *amp_unsettled_next(amp_delivery_t *delivery);

// sender
void amp_offer(amp_sender_t *sender, int credits);
ssize_t amp_send(amp_sender_t *sender, const char *bytes, size_t n);
void amp_abort(amp_sender_t *sender);

// receiver
#define EOM (-1)
void amp_flow(amp_receiver_t *receiver, int credits);
ssize_t amp_recv(amp_receiver_t *receiver, char *bytes, size_t n);

// delivery
amp_binary_t *amp_delivery_tag(amp_delivery_t *delivery);
amp_link_t *amp_link(amp_delivery_t *delivery);
// how do we do delivery state?
int amp_local_disp(amp_delivery_t *delivery);
int amp_remote_disp(amp_delivery_t *delivery);
bool amp_writable(amp_delivery_t *delivery);
bool amp_readable(amp_delivery_t *delivery);
bool amp_dirty(amp_delivery_t *delivery);
void amp_clean(amp_delivery_t *delivery);
void amp_disposition(amp_delivery_t *delivery, amp_disposition_t disposition);
//int amp_format(amp_delivery_t *delivery);
void amp_settle(amp_delivery_t *delivery);
void amp_delivery_dump(amp_delivery_t *delivery);

#endif /* engine.h */
