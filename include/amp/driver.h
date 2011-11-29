#ifndef _AMP_DRIVER_H
#define _AMP_DRIVER_H 1

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
#include <stdlib.h>

typedef struct amp_driver_t amp_driver_t;
typedef struct amp_selectable_st amp_selectable_t;

#define AMP_SEL_RD (0x0001)
#define AMP_SEL_WR (0x0002)

amp_selectable_t *amp_acceptor(char *host, char *port,
                               void (*cb)(amp_connection_t*, void*), void* context);
amp_selectable_t *amp_connector(char *host, char *port, amp_connection_t *conn,
                                void (*cb)(amp_connection_t*, void*), void* context);

amp_selectable_t *amp_selectable_create(void);
void amp_selectable_set_context(amp_selectable_t *s, void*);
void* amp_selectable_get_context(amp_selectable_t *s);
void amp_selectable_set_readable(amp_selectable_t *s, void (*readable)(amp_selectable_t *));
void amp_selectable_set_writable(amp_selectable_t *s, void (*writable)(amp_selectable_t *));
void amp_selectable_set_tick(amp_selectable_t *s, time_t (*tick)(amp_selectable_t *, time_t));
void amp_selectable_set_status(amp_selectable_t *s, int flags);
int amp_selectable_connect(amp_selectable_t *s, const char *host, const char *port);
int amp_selectable_recv(amp_selectable_t *s, void* buffer, size_t size);
int amp_selectable_send(amp_selectable_t *s, void* buffer, size_t size);
void amp_selectable_close(amp_selectable_t *sel);

amp_driver_t *amp_driver(void);
void amp_driver_add(amp_driver_t *d, amp_selectable_t *s);
void amp_driver_remove(amp_driver_t *d, amp_selectable_t *s);
void amp_driver_run(amp_driver_t *d);
void amp_driver_stop(amp_driver_t *d);

#endif /* driver.h */
