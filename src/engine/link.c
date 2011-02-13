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

struct amp_link_t {
  AMP_HEAD;
  wchar_t *name;
  amp_session_t *session;
  int handle;
  amp_object_t *source;
  amp_object_t *target;
  bool sender;
  bool attached;
  bool attach_sent;
  bool attach_rcvd;
  bool detach_sent;
  bool detach_rcvd;
};

AMP_TYPE_DECL(LINK, link)

amp_type_t *LINK = &AMP_TYPE(link);

amp_link_t *amp_link_create(bool sender)
{
  amp_link_t *o = amp_allocate(AMP_HEAP, NULL, sizeof(amp_link_t));
  o->type = LINK;
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
  return o;
}

AMP_DEFAULT_INSPECT(link)
AMP_DEFAULT_HASH(link)
AMP_DEFAULT_COMPARE(link)

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

void amp_link_bind(amp_link_t *link, amp_session_t *ssn, int handle)
{
  link->session = ssn;
  link->handle = handle;
}

void amp_sender_tick(amp_link_t *snd, amp_engine_t *eng)
{
  
}

void amp_receiver_tick(amp_link_t *rcv, amp_engine_t *eng)
{
  
}

void amp_link_tick(amp_link_t *link, amp_engine_t *eng)
{
  if (link->attached) {
    if (!link->attach_sent) {
      amp_engine_attach(eng, amp_session_channel(link->session),
                        !link->sender, link->name, link->handle, link->source,
                        link->target);
      link->attach_sent = true;
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
                        link->source, link->target,
                        NULL, NULL);
      link->detach_sent = true;
    }
  }
}
