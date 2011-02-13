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
#include <amp/driver.h>

int main(int argc, char **argv)
{
  amp_driver_t *drv = amp_driver(AMP_HEAP);
  amp_selectable_t *sel;
  //sel = amp_selectable(AMP_HEAP);
  if (argc > 1)
    sel = amp_acceptor(AMP_HEAP, "0.0.0.0", "5672");
  else {
    amp_connection_t *conn = amp_connection_create();
    amp_session_t *ssn = amp_session_create();
    amp_link_t *lnk = amp_link_create(true);
    amp_link_set_target(lnk, L"queue");
    amp_connection_add(conn, ssn);
    amp_session_add(ssn, lnk);
    amp_connection_open(conn);
    amp_session_begin(ssn);
    amp_link_attach(lnk);
    sel = amp_connector(AMP_HEAP, "0.0.0.0", "5672", conn);
  }
  if (!sel) {
    perror("driver");
    exit(-1);
  }
  amp_driver_add(drv, sel);
  amp_driver_run(drv);
  return 0;
}
