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
#include <amp/framing.h>
#include <amp/value.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "../protocol.h"

struct amp_engine_t {
  amp_error_t error;
  amp_connection_t *connection;
  amp_map_t *dispatch;

  amp_list_t *args;
  const char* payload_bytes;
  size_t payload_size;
  char *output;
  size_t available;
  size_t capacity;
};

  /*  amp_vmap_set(MAP, amp_symbol(AMP_HEAP, NAME ## _SYM), amp_ulong(AMP_HEAP, NAME)); \ */
#define __DISPATCH(MAP, NAME)                                           \
  amp_vmap_set(MAP, amp_ulong(NAME ## _CODE), amp_ulong(NAME))

amp_engine_t *amp_engine_create(amp_connection_t *connection)
{
  amp_engine_t *o = malloc(sizeof(amp_engine_t));
  if (o) {
    o->connection = connection;

    amp_map_t *m = amp_vmap(32);
    o->dispatch = m;

    __DISPATCH(m, OPEN);
    __DISPATCH(m, BEGIN);
    __DISPATCH(m, ATTACH);
    __DISPATCH(m, TRANSFER);
    __DISPATCH(m, FLOW);
    __DISPATCH(m, DISPOSITION);
    __DISPATCH(m, DETACH);
    __DISPATCH(m, END);
    __DISPATCH(m, CLOSE);

    o->args = amp_vlist(16);
    // XXX
    o->capacity = 4*1024;
    o->output = malloc(o->capacity);
    o->available = 0;
  }

  return o;
}

void amp_engine_dispatch(amp_engine_t *e, uint16_t channel, amp_tag_t *performative, const char* payload_bytes, size_t payload_size);

ssize_t amp_engine_input(amp_engine_t *engine, char *src, size_t available)
{
  size_t read = 0;
  while (true) {
    amp_frame_t frame;
    size_t n = amp_read_frame(&frame, src + read, available);
    if (n) {
      amp_value_t performative;
      ssize_t e = amp_vdecode(&performative, frame.payload, frame.size);
      if (e < 0) {
        fprintf(stderr, "Error decoding frame: %i\n", (int)e);
        fprintf(stderr, "%s\n", amp_aformat(amp_value("z", frame.size, frame.payload)));
        return e;
      }

      amp_tag_t *perf = amp_to_tag(performative);
      amp_engine_dispatch(engine, frame.channel, perf, frame.payload + e, frame.size - e);

      available -= n;
      read += n;
    } else {
      break;
    }
  }

  return read;
}

ssize_t amp_engine_output(amp_engine_t *engine, char *dst, size_t n)
{
  int m = engine->available < n ? engine->available : n;
  memmove(dst, engine->output, m);
  memmove(engine->output, engine->output + m, engine->available - m);
  engine->available -= m;
  // XXX: need to check engine for errors
  return m;
}

void amp_engine_init_frame(amp_engine_t *eng)
{
  amp_vlist_clear(eng->args);
  eng->payload_size = 0;
  eng->payload_bytes = 0;
}

void amp_engine_field(amp_engine_t *eng, int index, amp_value_t arg)
{
  int n = amp_vlist_size(eng->args);
  if (index >= n)
    amp_vlist_fill(eng->args, EMPTY_VALUE, index - n + 1);
  amp_vlist_set(eng->args, index, arg);
}

void amp_engine_append_payload(amp_engine_t *eng, const char* data, size_t size)
{
    eng->payload_bytes = data;
    eng->payload_size = size;
}

#define BUF_SIZE (1024*1024)

void amp_engine_post_frame(amp_engine_t *eng, uint16_t ch, uint32_t performative)
{
  amp_tag_t tag;
  amp_frame_t frame = {0};
  char bytes[BUF_SIZE];
  tag.descriptor = amp_ulong(performative);
  tag.value = amp_from_list(eng->args);
  fprintf(stderr, "POST: %s\n", amp_aformat(amp_from_tag(&tag)));
  // XXX: sizeof
  size_t size = amp_vencode(amp_from_tag(&tag), bytes);
  if (eng->payload_size) {
    memmove(bytes + size, eng->payload_bytes, eng->payload_size);
    size += eng->payload_size;
  }
  frame.channel = ch;
  frame.payload = bytes;
  frame.size = size;
  size_t n;
  while (!(n = amp_write_frame(eng->output + eng->available,
                               eng->capacity - eng->available, frame))) {
    eng->capacity *= 2;
    eng->output = realloc(eng->output, eng->capacity);
  }
  eng->available += n;
}

int amp_engine_open(amp_engine_t *eng, wchar_t *container_id, wchar_t *hostname)
{
  amp_engine_init_frame(eng);
  if (container_id)
    amp_engine_field(eng, OPEN_CONTAINER_ID, amp_value("S", container_id));
  if (hostname)
    amp_engine_field(eng, OPEN_HOSTNAME, amp_value("S", hostname));
  amp_engine_post_frame(eng, 0, OPEN_CODE);
  return 0;
}

void amp_engine_do_open(amp_engine_t *e, amp_list_t *args)
{
  printf("OPEN: %s\n", amp_aformat(amp_from_list(args)));
}

int amp_engine_begin(amp_engine_t *eng, int channel, int remote_channel,
                     sequence_t next_outgoing_id, uint32_t incoming_window,
                     uint32_t outgoing_window)
{
  amp_engine_init_frame(eng);
  if (remote_channel != -1)
    amp_engine_field(eng, BEGIN_REMOTE_CHANNEL, amp_value("H", remote_channel));
  amp_engine_field(eng, BEGIN_NEXT_OUTGOING_ID, amp_value("I", next_outgoing_id));
  amp_engine_field(eng, BEGIN_INCOMING_WINDOW, amp_value("I", incoming_window));
  amp_engine_field(eng, BEGIN_OUTGOING_WINDOW, amp_value("I", outgoing_window));
  amp_engine_post_frame(eng, channel, BEGIN_CODE);
  return 0;
}

void amp_engine_do_begin(amp_engine_t *e, uint16_t channel, amp_list_t *args)
{
  printf("BEGIN: %s\n", amp_aformat(amp_from_list(args)));
}

int amp_engine_attach(amp_engine_t *eng, uint16_t channel, bool role,
                      wchar_t *name, int handle, sequence_t initial_transfer_count,
                      wchar_t *source, wchar_t *target)
{
  amp_engine_init_frame(eng);
  amp_engine_field(eng, ATTACH_ROLE, amp_boolean(role));
  amp_engine_field(eng, ATTACH_NAME, amp_value("S", name));
  amp_engine_field(eng, ATTACH_HANDLE, amp_value("I", handle));
  amp_engine_field(eng, ATTACH_INITIAL_DELIVERY_COUNT, amp_value("I", initial_transfer_count));
  if (source)
    amp_engine_field(eng, ATTACH_SOURCE, amp_value("B([S])", SOURCE_CODE, source));
  if (target)
    amp_engine_field(eng, ATTACH_TARGET, amp_value("B([S])", TARGET_CODE, target));
  amp_engine_post_frame(eng, channel, ATTACH_CODE);
  return 0;
}

void amp_engine_do_attach(amp_engine_t *e, uint16_t channel, amp_list_t *args)
{
  printf("ATTACH: %s\n", amp_aformat(amp_from_list(args)));
}

int amp_engine_transfer(amp_engine_t *eng, uint16_t channel, int handle,
                        char *dtag, sequence_t id, char *bytes, size_t n)
{
  amp_engine_init_frame(eng);
  amp_engine_field(eng, TRANSFER_HANDLE, amp_value("I", handle));
  amp_engine_field(eng, TRANSFER_DELIVERY_ID, amp_value("I", id));
  amp_engine_field(eng, TRANSFER_DELIVERY_TAG, amp_value("z", strlen(dtag), dtag));
  amp_engine_field(eng, TRANSFER_MESSAGE_FORMAT, amp_value("I", 0));
  amp_engine_append_payload(eng, bytes, n);
  amp_engine_post_frame(eng, channel, TRANSFER_CODE);
  return 0;
}

void amp_engine_do_transfer(amp_engine_t *e, uint16_t channel, amp_list_t *args, const char* payload_bytes, size_t payload_size)
{
  printf("TRANSFER: %s payload: %.*s\n", amp_aformat(amp_from_list(args)), (int) payload_size, payload_bytes);
}

int amp_engine_flow(amp_engine_t *eng, uint16_t channel, sequence_t in_next,
                    int in_win, sequence_t out_next, int out_win, int handle,
                    sequence_t transfer_count, int credit)
{
  amp_engine_init_frame(eng);
  amp_engine_field(eng, FLOW_NEXT_INCOMING_ID, amp_value("I", in_next));
  amp_engine_field(eng, FLOW_INCOMING_WINDOW, amp_value("I", in_win));
  amp_engine_field(eng, FLOW_NEXT_OUTGOING_ID, amp_value("I", out_next));
  amp_engine_field(eng, FLOW_OUTGOING_WINDOW, amp_value("I", out_win));
  amp_engine_field(eng, FLOW_HANDLE, amp_value("I", handle));
  amp_engine_field(eng, FLOW_DELIVERY_COUNT, amp_value("I", transfer_count));
  amp_engine_field(eng, FLOW_LINK_CREDIT, amp_value("I", credit));
  amp_engine_post_frame(eng, channel, FLOW_CODE);
  return 0;
}

void amp_engine_do_flow(amp_engine_t *e, uint16_t channel, amp_list_t *args)
{
  printf("FLOW: %s\n", amp_aformat(amp_from_list(args)));
}

void amp_engine_do_disposition(amp_engine_t *e, uint16_t channel, amp_list_t *args)
{
  printf("DISP: %s\n", amp_aformat(amp_from_list(args)));
}

int amp_engine_detach(amp_engine_t *eng, int channel, int handle, char *condition,
                      wchar_t *description)
{
  amp_engine_init_frame(eng);
  amp_engine_field(eng, DETACH_HANDLE, amp_value("I", handle));
  if (condition)
    // XXX: symbol
    amp_engine_field(eng, DETACH_ERROR, amp_value("B([zS])", ERROR_CODE, condition, description));
  amp_engine_post_frame(eng, channel, DETACH_CODE);
  return 0;
}

void amp_engine_do_detach(amp_engine_t *e, uint16_t channel, amp_list_t *args)
{
  printf("DETACH: %s\n", amp_aformat(amp_from_list(args)));
}

int amp_engine_end(amp_engine_t *eng, int channel, char *condition,
                   wchar_t *description)
{
  amp_engine_init_frame(eng);
  if (condition)
    // XXX: symbol
    amp_engine_field(eng, DETACH_ERROR, amp_value("B([zS])", ERROR_CODE, condition, description));
  amp_engine_post_frame(eng, channel, END_CODE);
  return 0;
}

void amp_engine_do_end(amp_engine_t *e, uint16_t channel, amp_list_t *args)
{
  printf("END: %s\n", amp_aformat(amp_from_list(args)));
}

int amp_engine_close(amp_engine_t *eng, char *condition, wchar_t *description)
{
  amp_engine_init_frame(eng);
  if (condition)
    // XXX: symbol
    amp_engine_field(eng, CLOSE_ERROR, amp_value("B([zS])", ERROR_CODE, condition, description));
  amp_engine_post_frame(eng, 0, CLOSE_CODE);
  return 0;
}


void amp_engine_do_close(amp_engine_t *e, amp_list_t *args)
{
  printf("CLOSE: %s\n", amp_aformat(amp_from_list(args)));
}

void amp_engine_dispatch(amp_engine_t *e, uint16_t channel, amp_tag_t *performative, const char* payload_bytes, size_t payload_size)
{
  amp_value_t desc = amp_tag_descriptor(performative);
  amp_list_t *args = amp_to_list(amp_tag_value(performative));
  amp_value_t cval = amp_vmap_get(e->dispatch, desc);
  uint8_t code = amp_to_uint8(cval);
  switch (code)
  {
  case OPEN:
    amp_engine_do_open(e, args);
    break;
  case BEGIN:
    amp_engine_do_begin(e, channel, args);
    break;
  case ATTACH:
    amp_engine_do_attach(e, channel, args);
    break;
  case TRANSFER:
    amp_engine_do_transfer(e, channel, args, payload_bytes, payload_size);
    break;
  case FLOW:
    amp_engine_do_flow(e, channel, args);
    break;
  case DISPOSITION:
    amp_engine_do_disposition(e, channel, args);
    break;
  case DETACH:
    amp_engine_do_detach(e, channel, args);
    break;
  case END:
    amp_engine_do_end(e, channel, args);
    break;
  case CLOSE:
    amp_engine_do_close(e, args);
    break;
  }
}

time_t amp_engine_tick(amp_engine_t *eng, time_t now)
{
  return amp_connection_tick(eng->connection, eng, now);
}
