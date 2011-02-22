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

int main(int argc, char **argv)
{
  amp_driver_t *drv = amp_driver(AMP_HEAP);
  amp_selectable_t *sel;
  if (argc > 1) {
    amp_connection_t *conn = amp_connection_create();
    amp_session_t *ssn = amp_session_create();
    bool send = !strcmp(argv[1], "send");
    amp_link_t *lnk = amp_link_create(send);
    if (send) {
      amp_link_set_target(lnk, L"queue");
    } else {
      amp_link_set_source(lnk, L"queue");
    }
    amp_connection_add(conn, ssn);
    amp_session_add(ssn, lnk);
    amp_connection_open(conn);
    amp_session_begin(ssn);
    amp_link_attach(lnk);
    if (send) {
      amp_link_send(lnk, "testing", 7);
      amp_link_send(lnk, "one", 3);
      amp_link_send(lnk, "two", 3);
      amp_link_send(lnk, "three", 5);
    } else {
      amp_link_flow(lnk, 10);
    }
    sel = amp_connector(AMP_HEAP, "0.0.0.0", "5672", conn);
  } else {
    sel = amp_acceptor(AMP_HEAP, "0.0.0.0", "5672");
  }
  if (!sel) {
    perror("driver");
    exit(-1);
  }
  amp_driver_add(drv, sel);
  amp_driver_run(drv);
  return 0;
}
