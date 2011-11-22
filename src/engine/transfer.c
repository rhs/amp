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
#include <stdio.h>

void amp_transfer_buffer_init(amp_transfer_buffer_t* o, sequence_t next, size_t capacity)
{
  o->transfers = malloc(sizeof(amp_transfer_t) * capacity);
  o->next = next;
  o->capacity = capacity;
  o->head = 0;
  o->size = 0;
}

size_t amp_transfer_buffer_size(amp_transfer_buffer_t* o)
{
  return o->size;
}

size_t amp_transfer_buffer_available(amp_transfer_buffer_t *o)
{
  return o->capacity - o->size;
}

bool amp_transfer_buffer_empty(amp_transfer_buffer_t* o)
{
  return o->size == 0;
}

amp_transfer_t* amp_transfer_buffer_get(amp_transfer_buffer_t* o, size_t index)
{
  if (index < o->size) return o->transfers + ((o->head + index) % o->capacity);
  else return NULL;
}

amp_transfer_t* amp_transfer_buffer_head(amp_transfer_buffer_t* o)
{
  if (o->size) return o->transfers + o->head;
  else return NULL;
}

amp_transfer_t* amp_transfer_buffer_tail(amp_transfer_buffer_t* o)
{
  if (o->size) return amp_transfer_buffer_get(o, o->size - 1);
  else return NULL;
}

static void amp_transfer_init(amp_transfer_t* o, const char* delivery_tag, sequence_t id, amp_link_t* link)
{
  o->delivery_tag = delivery_tag;
  o->id = id;
  o->link = link;
  o->bytes = 0;
  o->size = 0;
  o->local_outcome = IN_DOUBT;
  o->remote_outcome = IN_DOUBT;
  o->local_settled = false;
  o->remote_settled = false;
  o->local_bytes_transferred = 0;
  o->remote_bytes_transferred = 0;
  o->dirty = false;
  o->unread = false;
  o->sent = false;
}

amp_transfer_t *amp_transfer_buffer_push(amp_transfer_buffer_t* o,
                                         const char* delivery_tag, amp_link_t* link)
{
  if (!amp_transfer_buffer_available(o))
    return NULL;
  o->size++;
  amp_transfer_t *xfr = amp_transfer_buffer_tail(o);
  amp_transfer_init(xfr, delivery_tag, o->next++, link);
  return xfr;
}

bool amp_transfer_buffer_pop(amp_transfer_buffer_t* o)
{
  if (o->size) {
    o->head = (o->head + 1) % o->capacity;
    o->size--;
    return true;
  } else {
    return false;
  }
}

const char *amp_transfer_delivery_tag(const amp_transfer_t* transfer)
{
  return transfer->delivery_tag;
}
bool amp_transfer_is_settled_locally(const amp_transfer_t* transfer)
{
  return transfer->local_settled;
}
bool amp_transfer_is_settled_remotely(const amp_transfer_t* transfer)
{
  return transfer->remote_settled;
}
outcome_t amp_transfer_local_outcome(const amp_transfer_t* transfer)
{
  return transfer->local_outcome;
}
outcome_t amp_transfer_remote_outcome(const amp_transfer_t* transfer)
{
  return transfer->remote_outcome;
}
void amp_transfer_clear_unread(amp_transfer_t* transfer)
{
  transfer->unread = false;
}
