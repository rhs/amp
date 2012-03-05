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

amp_driver_t *amp_driver(void);
void amp_driver_run(amp_driver_t *d);
void amp_driver_stop(amp_driver_t *d);
void amp_driver_destroy(amp_driver_t *d);

amp_selectable_t *amp_acceptor(amp_driver_t *driver, char *host, char *port,
                               void (*cb)(amp_connection_t*, void*),
                               void* context);
amp_selectable_t *amp_connector(amp_driver_t *driver, char *host, char *port,
                                void (*cb)(amp_connection_t*, void*),
                                void* context);

void amp_selectable_destroy(amp_selectable_t *sel);


#endif /* driver.h */
