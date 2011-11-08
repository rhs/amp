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

struct amp_message_t {
  char *bytes;
  size_t size;
};

struct amp_link_t {
  wchar_t *name;
  amp_session_t *session;
  int handle;
  wchar_t *source;
  wchar_t *target;
  bool sender;
  bool attached;
  bool attach_sent;
  bool attach_rcvd;
  bool detach_sent;
  bool detach_rcvd;
  size_t nmessages;
  struct amp_message_t messages[1024];
  sequence_t transfer_count;
  int credit;
};

#define INITIAL_TRANSFER_COUNT (0)

amp_link_t *amp_link_create(bool sender)
{
  amp_link_t *o = malloc(sizeof(amp_link_t));
  o->name = L"test-name";
  o->session = NULL;
  o->handle = -1;
  o->source = NULL;
  o->target = NULL;
  o->sender = sender;
  o->attached = false;
  o->attach_sent = false;
  o->attach_rcvd = false;
  o->detach_sent = false;
  o->detach_rcvd = false;
  o->nmessages = 0;
  o->transfer_count = INITIAL_TRANSFER_COUNT;
  o->credit = 0;
  return o;
}

void amp_link_set_source(amp_link_t *link, wchar_t *source)
{
  link->source = source;
}

void amp_link_set_target(amp_link_t *link, wchar_t *target)
{
  link->target = target;
}

bool amp_link_sender(amp_link_t *link)
{
  return link->sender;
}

int amp_link_attach(amp_link_t *link)
{
  link->attached = true;
  return 0;
}

int amp_link_send(amp_link_t *link, char *bytes, size_t n)
{
  link->messages[link->nmessages++] = (struct amp_message_t) {bytes, n};
  return 0;
}

int amp_link_flow(amp_link_t *link, int n)
{
  link->credit += n;
  return 0;
}

void amp_link_bind(amp_link_t *link, amp_session_t *ssn, int handle)
{
  link->session = ssn;
  link->handle = handle;
}

void amp_sender_tick(amp_link_t *snd, amp_engine_t *eng)
{
  for (int i = 0; i < snd->nmessages; i++) {
    sequence_t id = amp_session_next(snd->session);
    amp_engine_transfer(eng, amp_session_channel(snd->session), snd->handle,
                        "test", id, snd->messages[i].bytes,
                        snd->messages[i].size);
  }
}

void amp_receiver_tick(amp_link_t *rcv, amp_engine_t *eng)
{
  
}

void amp_link_tick(amp_link_t *link, amp_engine_t *eng)
{
  if (link->attached) {
    if (!link->attach_sent) {
      amp_engine_attach(eng, amp_session_channel(link->session),
                        !link->sender, link->name, link->handle,
                        link->transfer_count, link->source,
                        link->target);
      link->attach_sent = true;
      if (!link->sender) {
        amp_engine_flow(eng, amp_session_channel(link->session),
                        amp_session_in_next(link->session),
                        amp_session_in_win(link->session),
                        amp_session_out_next(link->session),
                        amp_session_out_win(link->session),
                        link->handle,
                        link->transfer_count,
                        link->credit);
      }
    }

    if (amp_link_sender(link)) {
      amp_sender_tick(link, eng);
    } else {
      amp_receiver_tick(link, eng);
    }
  } else {
    if (link->attach_sent && !link->detach_sent) {
      amp_engine_detach(eng, amp_session_channel(link->session),
                        link->handle,
                        NULL, NULL);
      link->detach_sent = true;
    }
  }
}
