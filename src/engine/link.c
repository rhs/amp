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
#include <amp/util.h>
#include <amp/value.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

struct amp_message_t {
  const char *bytes;
  size_t size;
  const char *delivery_tag;
};

#define MAX_CAPACITY (1024)

struct amp_link_t {
  const wchar_t *name;
  amp_session_t *session;
  int handle;
  const wchar_t *source;
  const wchar_t *target;
  bool sender;
  bool attached;
  bool attach_sent;
  bool attach_rcvd;
  bool detach_sent;
  bool detach_rcvd;
  size_t head;
  size_t tail;
  size_t sent;
  size_t capacity;
  struct amp_message_t messages[MAX_CAPACITY];
  sequence_t delivery_count;
  int requested_credit;
  int outstanding_credit;
  amp_list_t* unsettled_transfers;
};

#define INITIAL_DELIVERY_COUNT (0)

wchar_t *amp_wcs_dup(const wchar_t *s)
{
  size_t size = wcslen(s);
  wchar_t* copy = malloc((sizeof(wchar_t*) * size) + 1);
  wcsncpy(copy, s, size);
  copy[size] = L'\0';
  return copy;
}

amp_link_t *amp_link_create(bool sender, const wchar_t *name)
{
  amp_link_t *o = malloc(sizeof(amp_link_t));
  o->name = amp_wcs_dup(name);
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
  o->head = 0;
  o->tail = 0;
  o->sent = 0;
  o->capacity = MAX_CAPACITY; //TODO: what is appropriate initial value here? 0? needs tied to credit
  o->delivery_count = INITIAL_DELIVERY_COUNT;
  o->requested_credit = 0;
  o->outstanding_credit = 0;
  return o;
}

int amp_link_destroy(amp_link_t* l)
{
  return 0;//TODO
}

void amp_link_set_source(amp_link_t *link, const wchar_t *source)
{
  link->source = source;
}

void amp_link_set_target(amp_link_t *link, const wchar_t *target)
{
  link->target = target;
}

const wchar_t *amp_link_get_source(amp_link_t *link)
{
  return link->source;
}

const wchar_t *amp_link_get_target(amp_link_t *link)
{
  return link->target;
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

int amp_link_detach(amp_link_t *link)
{
  link->attached = false;
  return 0;
}

int amp_link_used(amp_link_t *link)
{
  if (link->tail >= link->head) {
    return link->tail - link->head;
  } else {
    return MAX_CAPACITY - (link->head - link->tail);
  }
}

int amp_link_available(amp_link_t *link)
{
    return link->capacity - amp_link_used(link);
}

int amp_link_send(amp_link_t *link, const char *delivery_tag, const char *bytes,
                  size_t n)
{
  if (!(amp_session_record(link->session, link, delivery_tag, bytes, n)))
    amp_fatal("send capacity exceeded?\n");
  return 0;
}

int amp_link_flow(amp_link_t *link, int n)
{
  link->requested_credit = n;
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

void amp_link_received(amp_link_t *link, const char *data, size_t size, const char* delivery_tag, sequence_t transfer_id)
{
  link->outstanding_credit -= 1;
  link->delivery_count++;
  //TODO: whats the ideal allocation strategy here? Are we supposed to
  //be using some 'region' etc? who frees these and when?
  char* copy = malloc(size);
  if (copy) {
    memmove(copy, data, size);
    link->messages[link->tail].bytes = copy;
    link->messages[link->tail].size = size;
    ++(link->tail);
  }
  amp_transfer_t *xfr = amp_session_record(link->session, link, delivery_tag,
                                           copy, size);
  if (!xfr) amp_fatal("recv capacity exceeded?\n");
  if (xfr->id != transfer_id)
    amp_fatal("sequencing error: %i, %i\n", xfr->id, transfer_id);
}

void amp_receiver_tick(amp_link_t *link, amp_engine_t *eng)
{
  if (link->requested_credit > link->outstanding_credit) {
    int shortfall = link->requested_credit - link->outstanding_credit;
    amp_session_flow(link->session, eng, link, shortfall);
    link->outstanding_credit = link->requested_credit;
    link->requested_credit = 0;
  }
}

void amp_link_tick(amp_link_t *link, amp_engine_t *eng)
{
  if (link->attached) {
    if (!link->attach_sent) {
      amp_engine_attach(eng, amp_session_channel(link->session),
                        !link->sender, link->name, link->handle,
                        link->delivery_count, link->source,
                        link->target);
      link->attach_sent = true;
      if (!link->sender) {
        amp_session_flow(link->session, eng, link, link->requested_credit);
        link->outstanding_credit = link->requested_credit;
        link->requested_credit = 0;
      }
    }

    if (amp_link_sender(link)) {
      amp_sender_tick(link, eng);
    } else {
      amp_receiver_tick(link, eng);
    }
  } else {
    if (link->attach_sent && !link->detach_sent) {
      if (amp_link_sender(link)) {
        amp_sender_tick(link, eng);
      }
      amp_engine_detach(eng, amp_session_channel(link->session),
                        link->handle,
                        NULL, NULL);
      link->detach_sent = true;
    }
  }
}

int amp_link_get(amp_link_t *link, const char **bytes, size_t *n)
{
  if (link->attached && !amp_link_sender(link)) {
    if (link->head != link->tail) {
      *bytes = link->messages[link->head].bytes;
      *n = link->messages[link->head++].size;
      return 1;
      //TODO: allocation strategy and responsibilities around free
    } else {
      return 0;
    }
  } else {
    return -1;
  }
}

int amp_link_pending(amp_link_t *link)
{
  //TODO: what does pending actually mean?
  return amp_link_used(link);
}

bool amp_link_attached(amp_link_t *link)
{
  return link->attached;
}

bool amp_link_attached_by_peer(amp_link_t *link)
{
  return link->attach_rcvd;
}

bool amp_link_detached_by_peer(amp_link_t *link)
{
  return link->detach_rcvd;
}

void amp_link_attach_received(amp_link_t *link)
{
  link->attach_rcvd = true;
  link->detach_rcvd = false;//reset
}

void amp_link_detach_received(amp_link_t *link)
{
  link->detach_rcvd = true;
  link->attach_rcvd = false;//reset
}

bool amp_link_has_pending_work(amp_link_t* link)
{
  return (link->attach_rcvd && !link->attached) || (link->detach_rcvd && link->attached);
}

const amp_transfer_t* amp_link_unsettled(amp_link_t* link, const char* delivery_tag)
{
  return amp_session_unsettled(link->session, link, delivery_tag);
}

bool amp_link_update_unsettled(amp_link_t* link, const char* delivery_tag, outcome_t outcome, bool settled)
{
  return amp_session_update_unsettled(link->session, link, delivery_tag, outcome, settled);
}
int amp_link_handle(amp_link_t* link)
{
  return link->handle;
}
amp_session_t* amp_link_session(amp_link_t *link)
{
  return link->session;
}
sequence_t amp_link_delivery_count(amp_link_t *link)
{
  return link->delivery_count;
}
