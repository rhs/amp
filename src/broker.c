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

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <amp/driver.h>
#include <amp/engine.h>


void handle(amp_connection_t* connection, void* context)
{
  amp_endpoint_t endpoint = amp_connection_next_endpoint(connection);
  while (endpoint.type != NULL_ENDPOINT) {
    switch (endpoint.type) {
    case CONNECTION_ENDPOINT:
      if (amp_connection_opened_by_peer(endpoint.value.as_connection) && !amp_connection_opened(endpoint.value.as_connection)) {
        amp_connection_open(endpoint.value.as_connection);
        printf("connection opened\n");
      } else if (amp_connection_closed_by_peer(endpoint.value.as_connection) && amp_connection_opened(endpoint.value.as_connection)) {
        amp_connection_close(endpoint.value.as_connection);
        printf("connection closed\n");
      }
      break;
    case SESSION_ENDPOINT:
      if (amp_session_begun_by_peer(endpoint.value.as_session) && !amp_session_active(endpoint.value.as_session)) {
        amp_session_begin(endpoint.value.as_session);
        printf("session begun\n");
      } else if (amp_session_ended_by_peer(endpoint.value.as_session) && amp_session_active(endpoint.value.as_session)) {
        amp_session_end(endpoint.value.as_session);
        printf("session ended\n");
      }
      break;
    case LINK_ENDPOINT:
      if (amp_link_attached_by_peer(endpoint.value.as_link) && !amp_link_attached(endpoint.value.as_link)) {
        amp_link_attach(endpoint.value.as_link);
        printf("link attached\n");
      } else if (amp_link_detached_by_peer(endpoint.value.as_link) && amp_link_attached(endpoint.value.as_link)) {
        amp_link_detach(endpoint.value.as_link);
        printf("link detached\n");
      }
      break;
    case NULL_ENDPOINT://will never be true given while above but is required for compiler
      break;
    }
    endpoint = amp_connection_next_endpoint(connection);
  }
  amp_link_t* link = amp_connection_next_link(connection);
  while (link) {
    if (!amp_link_sender(link)) {
      const char* bytes = 0;
      size_t size = 0;
      if (amp_link_get(link, &bytes, &size) > 0) {
        printf("Got message: %.*s\n", (int)size, bytes);
      }
    }
    link = amp_connection_next_link(connection);
  }
}

int main(int argc, char **argv)
{
  amp_driver_t *drv = amp_driver();
  amp_selectable_t *sel = amp_acceptor("0.0.0.0", "5672", &handle, NULL);
  if (!sel) {
    perror("driver");
    exit(-1);
  }
  amp_driver_add(drv, sel);
  amp_driver_run(drv);
  return 0;
}
