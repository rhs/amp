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
#include <amp/list.h>

struct amp_session_t {
  AMP_HEAD;
  amp_connection_t *connection;
  int channel;
  int remote_channel;
  amp_list_t *links;
  bool begun;
  bool begin_sent;
  bool begin_rcvd;
  bool end_sent;
  bool end_rcvd;
  sequence_t next_incoming_id;
  uint32_t incoming_window;
  sequence_t next_outgoing_id;
  uint32_t outgoing_window;
};

AMP_TYPE_DECL(SESSION, session)

amp_type_t *SESSION = &AMP_TYPE(session);

amp_session_t *amp_session_create()
{
  amp_session_t *o = amp_allocate(AMP_HEAP, NULL, sizeof(amp_session_t));
  o->type = SESSION;
  o->connection = NULL;
  o->channel = -1;
  o->remote_channel = -1;
  o->links = amp_list(AMP_HEAP, 16);
  o->begun = false;
  o->begin_sent = false;
  o->begin_rcvd = false;
  o->end_sent = false;
  o->end_rcvd = false;
  o->incoming_window = 65536;
  o->next_outgoing_id = 0;
  o->outgoing_window = 65536;
  return o;
}

AMP_DEFAULT_INSPECT(session)
AMP_DEFAULT_HASH(session)
AMP_DEFAULT_COMPARE(session)

int amp_session_begin(amp_session_t *session)
{
  session->begun = true;
  return 0;
}

int amp_session_channel(amp_session_t *session)
{
  return session->channel;
}

int amp_session_links(amp_session_t *session)
{
  return amp_list_size(session->links);
}

amp_link_t *amp_session_get_link(amp_session_t *session, int index)
{
  return amp_list_get(session->links, index);
}

void amp_session_bind(amp_session_t *ssn, amp_connection_t *conn, int channel)
{
  ssn->connection = conn;
  ssn->channel = channel;
}

void amp_session_add(amp_session_t *ssn, amp_link_t *link)
{
  int handle = amp_list_size(ssn->links);
  amp_list_add(ssn->links, link);
  amp_link_bind(link, ssn, handle);
}

void amp_session_tick(amp_session_t *ssn, amp_engine_t *eng)
{
  if (ssn->begun) {
    if (!ssn->begin_sent) {
      amp_engine_begin(eng, ssn->channel, ssn->remote_channel,
                       ssn->next_outgoing_id, ssn->incoming_window,
                       ssn->outgoing_window);
      ssn->begin_sent = true;
    }

    int links = amp_session_links(ssn);
    for (int i = 0; i < links; i++) {
      amp_link_t *lnk = amp_session_get_link(ssn, i);
      amp_link_tick(lnk, eng);
    }
  } else {
    if (ssn->begin_sent && !ssn->end_sent) {
      amp_engine_end(eng, ssn->channel, NULL, NULL);
      ssn->end_sent = true;
    }
  }
}
