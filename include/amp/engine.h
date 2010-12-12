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

#include <sys/time.h>

typedef struct amp_error_t amp_error_t;
typedef struct amp_connection_t amp_connection_t;
typedef struct amp_session_t amp_session_t;
typedef struct amp_link_t amp_link_t;
typedef struct amp_engine_t amp_engine_t;

/* Bottom Half */

amp_engine_t *amp_engine_create(amp_connection_t *connection);
void *amp_engine_destroy(amp_engine_t *engine);
amp_error_t *amp_engine_error(amp_engine_t *engine);

// supplies engine with n bytes of input, engine returns number of
// bytes consumed, or -1 if there was an error
int amp_engine_input(amp_engine_t *engine, char *src, size_t n);

// supplies engine with n bytes of space for output, engine returns
// number of bytes produced, or -1 if there was an error
int amp_engine_output(amp_engine_t *engine, char *dst, size_t n);

time_t amp_engine_tick(amp_engine_t *engine, time_t now);

/* Middle Half */

// ...

/* Top Half */

/* Connections */
amp_connection_t *amp_connection_create();
int amp_connection_destroy(amp_connection_t *conn);
amp_error_t *amp_connection_error(amp_connection_t *connection);

int amp_connection_open(amp_connection_t *conn, ...);
int amp_connection_close(amp_connection_t *conn, ...);

/* Sessions */
amp_session_t *amp_session_create();
int amp_session_destroy(amp_session_t *session);
amp_error_t *amp_session_error(amp_session_t *session);

int amp_session_begin(amp_session_t *session);
int amp_session_end(amp_session_t *session);

/* Links */
amp_link_t *amp_link_create();
int amp_link_destroy(amp_link_t *link);
amp_error_t *amp_link_error(amp_link_t *link);

int amp_link_open(amp_link_t *link, ...);
int amp_link_attach(amp_link_t *link, amp_session_t *session, ...);
int amp_link_capacity(amp_link_t *link);
int amp_link_disposition(amp_link_t *link, ...);
int amp_link_settle(amp_link_t *link, ...);
int amp_link_detach(amp_link_t *link, ...);
int amp_link_close(amp_link_t *link, ...);

// sender
int amp_link_send(amp_link_t *link, ...);
int amp_link_drained(amp_link_t *link);

// receiver
int amp_link_flow(amp_link_t *link, int credit);
int amp_link_drain(amp_link_t *link);
int amp_link_pending(amp_link_t *link);
int amp_link_get(amp_link_t *link, ...);

#endif /* engine.h */
