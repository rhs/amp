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

#include <amp/allocation.h>
#include <amp/engine.h>
#include <amp/type.h>
#include <amp/scalars.h>
#include <amp/list.h>
#include "../protocol.h"

struct amp_connection_t {
  AMP_HEAD;
  amp_error_t error;
  bool open;
  wchar_t *container;
  wchar_t *hostname;
  amp_list_t *sessions;
  bool open_sent;
  bool open_rcvd;
  bool close_sent;
  bool close_rcvd;
};

AMP_TYPE_DECL(CONNECTION, connection)

amp_type_t *CONNECTION = &AMP_TYPE(connection);

amp_connection_t *amp_connection_create() {
  amp_connection_t *o = amp_allocate(AMP_HEAP, NULL, sizeof(amp_connection_t));
  o->type = CONNECTION;
  o->open = false;
  o->container = NULL;
  o->hostname = NULL;
  o->sessions = amp_list(AMP_HEAP, 16);
  o->open_sent = false;
  o->open_rcvd = false;
  o->close_sent = false;
  o->close_rcvd = false;
  return o;
}

AMP_DEFAULT_INSPECT(connection)
AMP_DEFAULT_HASH(connection)
AMP_DEFAULT_COMPARE(connection)

int amp_connection_open(amp_connection_t *c) {
  c->open = true;
  return 0;
}

bool amp_connection_opened(amp_connection_t *c) {
  return c->open;
}

int amp_connection_close(amp_connection_t *c) {
  c->open = false;
  return 0;
}

wchar_t *amp_connection_container(amp_connection_t *c) {
  return c->container;
}

wchar_t *amp_connection_hostname(amp_connection_t *c) {
  return c->hostname;
}

int amp_connection_sessions(amp_connection_t *conn) {
  return amp_list_size(conn->sessions);
}

amp_session_t *amp_connection_get_session(amp_connection_t *conn, int index) {
  return amp_list_get(conn->sessions, index);
}

void amp_connection_add(amp_connection_t *conn, amp_session_t *ssn) {
  int num = amp_list_size(conn->sessions);
  amp_list_add(conn->sessions, ssn);
  amp_session_bind(ssn, conn, num);
}

time_t amp_connection_tick(amp_connection_t *conn, amp_engine_t *eng, time_t now)
{
  if (conn->open) {
    if (!conn->open_sent) {
      amp_engine_open(eng, amp_connection_container(conn),
                      amp_connection_hostname(conn));
    }

    int sessions = amp_connection_sessions(conn);
    for (int i = 0; i < sessions; i++)
    {
      amp_session_t *ssn = amp_connection_get_session(conn, i);
      amp_session_tick(ssn, eng);
    }
  } else {
    if (conn->open_sent && !conn->close_sent)
    {
      amp_engine_close(eng, NULL, NULL);
    }
  }

  return 0;
}
