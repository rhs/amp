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
#include "../protocol.h"

struct amp_connection_t {
  amp_error_t error;
  bool open;
  wchar_t *container;
  wchar_t *hostname;
  amp_list_t *sessions;
  bool open_sent;
  bool open_rcvd;
  bool close_sent;
  bool close_rcvd;
  amp_list_t *pending_links;
  bool work_pending;
};

amp_connection_t *amp_connection_create() {
  amp_connection_t *o = malloc(sizeof(amp_connection_t));
  o->open = false;
  o->container = NULL;
  o->hostname = NULL;
  // XXX: max sessions
  o->sessions = amp_list(16);
  o->open_sent = false;
  o->open_rcvd = false;
  o->close_sent = false;
  o->close_rcvd = false;
  o->pending_links = amp_list(1024);//TODO: what is appropriate capacity here? does this need to be dynamic?
  o->work_pending = false;
  return o;
}

int amp_connection_destroy(amp_connection_t* c)
{
    return 0;//TODO
}

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
  if (amp_list_size(conn->sessions) > index) {
    return amp_to_ref(amp_list_get(conn->sessions, index));
  } else {
    return 0;
  }
}

void amp_connection_add(amp_connection_t *conn, amp_session_t *ssn) {
  int num = amp_list_size(conn->sessions);
  amp_list_add(conn->sessions, amp_from_ref(ssn));
  amp_session_bind(ssn, conn, num);
}

time_t amp_connection_tick(amp_connection_t *conn, amp_engine_t *eng, time_t now)
{
  if (conn->open) {
    if (!conn->open_sent) {
      amp_engine_open(eng, amp_connection_container(conn),
                      amp_connection_hostname(conn));
      conn->open_sent = true;
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
      int sessions = amp_connection_sessions(conn);
      for (int i = 0; i < sessions; i++)
      {
        amp_session_t *ssn = amp_connection_get_session(conn, i);
        amp_session_tick(ssn, eng);
      }
      amp_engine_close(eng, NULL, NULL);
      conn->close_sent = true;
    }
  }

  return 0;
}

void amp_connection_open_received(amp_connection_t *conn)
{
  conn->open_rcvd = true;
  conn->close_rcvd = false;//reset
}

void amp_connection_close_received(amp_connection_t *conn)
{
  conn->close_rcvd = true;
  conn->open_rcvd = false;//reset
}

bool amp_connection_opened_by_peer(amp_connection_t *conn)
{
  return conn->open_rcvd;
}

bool amp_connection_closed_by_peer(amp_connection_t *conn)
{
  return conn->close_rcvd;
}

amp_link_t *amp_connection_next_link(amp_connection_t *conn)
{
  //TODO: change this to work off the incoming transfers in session?
  if (amp_list_size(conn->pending_links)) {
    return amp_to_ref(amp_list_pop(conn->pending_links, 0));
  } else {
    return 0;
  }
}

amp_endpoint_t amp_connection_next_endpoint(amp_connection_t *conn)
{
  amp_endpoint_t result;
  result.type = NULL_ENDPOINT;
  if ((conn->open_rcvd && !conn->open) || (conn->close_rcvd && conn->open)) {
     result.type = CONNECTION_ENDPOINT;
     result.value.as_connection = conn;
  } else {
    int size = amp_list_size(conn->sessions);
    for (int i = 0; i < size && result.type == NULL_ENDPOINT; ++i) {
      amp_session_t* session = amp_to_ref(amp_list_get(conn->sessions, i));
      result = amp_session_next_endpoint(session);
    }
  }
  return result;
}

void amp_connection_add_pending_link(amp_connection_t *conn, amp_link_t* link)
{
  amp_list_add(conn->pending_links, amp_from_ref(link));
}
