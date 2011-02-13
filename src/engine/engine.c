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
#include <amp/framing.h>
#include <amp/type.h>
#include <amp/map.h>
#include <amp/symbol.h>
#include <amp/scalars.h>
#include <amp/decoder.h>
#include <amp/string.h>
#include <string.h>
#include "../protocol.h"

struct amp_engine_t {
  AMP_HEAD;
  amp_error_t error;
  amp_connection_t *connection;
  amp_region_t *region;
  amp_decoder_t *decoder;
  amp_map_t *dispatch;

  amp_list_t *args;
  amp_box_t *body;
  amp_encoder_t *encoder;
  char *output;
  size_t available;
  size_t capacity;
};

AMP_TYPE_DECL(ENGINE, engine)

amp_type_t *ENGINE = &AMP_TYPE(engine);

#define __DISPATCH(MAP, NAME)                                           \
  amp_map_set(MAP, amp_symbol(AMP_HEAP, NAME ## _SYM), amp_byte(AMP_HEAP, NAME)); \
  amp_map_set(MAP, amp_long(AMP_HEAP, NAME ## _CODE), amp_byte(AMP_HEAP, NAME))

amp_engine_t *amp_engine_create(amp_connection_t *connection)
{
  amp_engine_t *o = amp_allocate(AMP_HEAP, NULL, sizeof(amp_engine_t));
  o->type = ENGINE;
  o->connection = connection;
  // XXX
  o->region = amp_region(64*1024);
  o->decoder = amp_decoder(AMP_HEAP);

  amp_map_t *m = amp_map(AMP_HEAP, 32);
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


  o->args = amp_list(AMP_HEAP, 16);
  o->body = amp_box(AMP_HEAP, NULL, o->args);
  o->encoder = amp_encoder(AMP_HEAP);
  // XXX
  o->capacity = 4*1024;
  o->output = amp_allocate(AMP_HEAP, NULL, o->capacity);
  o->available = 0;

  return o;
}

int amp_engine_inspect(amp_object_t *o, char **pos, char *limit)
{
  amp_engine_t *e = o;
  int n;
  if ((n = amp_format(pos, limit, "engine<%p>(connection=", o))) return n;
  if ((n = amp_inspect(e->connection, pos, limit))) return n;
  if ((n = amp_format(pos, limit, ")"))) return n;
  return 0;
}

AMP_DEFAULT_HASH(engine)
AMP_DEFAULT_COMPARE(engine)

void amp_engine_dispatch(amp_engine_t *e, uint16_t channel, amp_box_t *body);

ssize_t amp_engine_input(amp_engine_t *engine, char *src, size_t available)
{
  size_t read = 0;
  while (true) {
    amp_frame_t frame;
    size_t n = amp_read_frame(&frame, src + read, available);
    if (n) {
      amp_decoder_init(engine->decoder, engine->region);
      int e = amp_read_data(frame.payload, frame.size, decoder, engine->decoder);
      if (e < 0) {
        return e;
      }

      if (engine->decoder->size != 1) {
        // XXX
      }

      amp_box_t *body = engine->decoder->values[0];
      amp_engine_dispatch(engine, frame.channel, body);
      amp_region_clear(engine->region);

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

void amp_engine_init_frame(amp_engine_t *eng, uint32_t frame)
{
  amp_list_clear(eng->args);
  amp_box_set_tag(eng->body, amp_long(eng->region, frame));
}

void amp_engine_field(amp_engine_t *eng, int index, amp_object_t *arg)
{
  int n = amp_list_size(eng->args);
  if (index >= n)
    amp_list_fill(eng->args, NULL, index - n + 1);
  amp_list_set(eng->args, index, arg);
}

#define BUF_SIZE (1024*1024)

void amp_engine_post_frame(amp_engine_t *eng, uint16_t ch)
{
  amp_frame_t frame = {0};
  char bytes[BUF_SIZE];
  char *pos = bytes;
  amp_encoder_init(eng->encoder);
  amp_encode(eng->body, eng->encoder, &pos, pos + BUF_SIZE);
  frame.channel = ch;
  frame.payload = bytes;
  frame.size = pos - bytes;
  size_t n;
  while (!(n = amp_write_frame(eng->output + eng->available,
                               eng->capacity - eng->available, frame))) {
    eng->capacity *= 2;
    eng->output = amp_allocate(AMP_HEAP, eng->output, eng->capacity);
  }
  eng->available += n;
}

#include <stdio.h>

int amp_engine_open(amp_engine_t *eng, wchar_t *container_id, wchar_t *hostname)
{
  amp_engine_init_frame(eng, OPEN_CODE);
  if (container_id)
    amp_engine_field(eng, OPEN_CONTAINER_ID, amp_string(eng->region, container_id));
  if (hostname)
    amp_engine_field(eng, OPEN_HOSTNAME, amp_string(eng->region, hostname));
  amp_engine_post_frame(eng, 0);
  return 0;
}

void amp_engine_do_open(amp_engine_t *e, amp_list_t *args)
{
  printf("OPEN: %s\n", amp_ainspect(args));
}

int amp_engine_begin(amp_engine_t *eng, int channel, int remote_channel,
                     sequence_t next_outgoing_id, uint32_t incoming_window,
                     uint32_t outgoing_window)
{
  amp_engine_init_frame(eng, BEGIN_CODE);
  if (remote_channel != -1)
    amp_engine_field(eng, BEGIN_REMOTE_CHANNEL, amp_ushort(eng->region, remote_channel));
  amp_engine_field(eng, BEGIN_NEXT_OUTGOING_ID, amp_uint(eng->region, next_outgoing_id));
  amp_engine_field(eng, BEGIN_INCOMING_WINDOW, amp_uint(eng->region, incoming_window));
  amp_engine_field(eng, BEGIN_OUTGOING_WINDOW, amp_uint(eng->region, outgoing_window));
  amp_engine_post_frame(eng, channel);
  return 0;
}

void amp_engine_do_begin(amp_engine_t *e, uint16_t channel, amp_list_t *args)
{
  printf("BEGIN: %s\n", amp_ainspect(args));
}

int amp_engine_attach(amp_engine_t *eng, uint16_t channel, bool role,
                      wchar_t *name, int handle, wchar_t *source,
                      wchar_t *target)
{
  amp_engine_init_frame(eng, ATTACH_CODE);
  amp_engine_field(eng, ATTACH_ROLE, amp_boolean(eng->region, role));
  amp_engine_field(eng, ATTACH_NAME, amp_string(eng->region, name));
  amp_engine_field(eng, ATTACH_HANDLE, amp_uint(eng->region, handle));
  if (source)
    amp_engine_field(eng, ATTACH_SOURCE,
                     amp_proto_source(eng->region,
                                      ADDRESS, amp_string(eng->region, source)));
  if (target)
    amp_engine_field(eng, ATTACH_TARGET,
                     amp_proto_target(eng->region,
                                      ADDRESS, amp_string(eng->region, target)));
  amp_engine_post_frame(eng, channel);
  return 0;
}

void amp_engine_do_attach(amp_engine_t *e, uint16_t channel, amp_list_t *args)
{
  printf("ATTACH: %s\n", amp_ainspect(args));
}

void amp_engine_do_transfer(amp_engine_t *e, uint16_t channel, amp_list_t *args)
{
  printf("TRANSFER: %s\n", amp_ainspect(args));
}

void amp_engine_do_flow(amp_engine_t *e, uint16_t channel, amp_list_t *args)
{
  printf("FLOW: %s\n", amp_ainspect(args));
}

void amp_engine_do_disposition(amp_engine_t *e, uint16_t channel, amp_list_t *args)
{
  printf("DISP: %s\n", amp_ainspect(args));
}

int amp_engine_detach(amp_engine_t *eng, int channel, int handle, wchar_t *source,
                      wchar_t *target, char *condition, wchar_t *description)
{
  amp_engine_init_frame(eng, DETACH_CODE);
  amp_engine_field(eng, DETACH_HANDLE, amp_uint(eng->region, handle));
  if (source)
    amp_engine_field(eng, DETACH_SOURCE,
                     amp_proto_source(eng->region,
                                      ADDRESS, amp_string(eng->region, source)));
  if (target)
    amp_engine_field(eng, DETACH_TARGET,
                     amp_proto_target(eng->region,
                                      ADDRESS, amp_string(eng->region, target)));
  amp_box_t *error;
  if (condition) {
    error = amp_proto_error(eng->region, CONDITION, condition,
                            DESCRIPTION, description);
  } else {
    error = NULL;
  }
  if (error)
    amp_engine_field(eng, DETACH_ERROR, error);
  amp_engine_post_frame(eng, channel);
  return 0;
}

void amp_engine_do_detach(amp_engine_t *e, uint16_t channel, amp_list_t *args)
{
  printf("DETACH: %s\n", amp_ainspect(args));
}

int amp_engine_end(amp_engine_t *eng, int channel, char *condition,
                   wchar_t *description)
{
  amp_box_t *error;
  if (condition) {
    error = amp_proto_error(eng->region, CONDITION, condition,
                            DESCRIPTION, description);
  } else {
    error = NULL;
  }
  amp_engine_init_frame(eng, END_CODE);
  if (error)
    amp_engine_field(eng, DETACH_ERROR, error);
  amp_engine_post_frame(eng, channel);
  return 0;
}

void amp_engine_do_end(amp_engine_t *e, uint16_t channel, amp_list_t *args)
{
  printf("END: %s\n", amp_ainspect(args));
}

int amp_engine_close(amp_engine_t *eng, char *condition, wchar_t *description)
{
  amp_box_t *error;
  if (condition) {
    error = amp_proto_error(eng->region, CONDITION, condition,
                                       DESCRIPTION, description);
  } else{
    error = NULL;
  }
  amp_engine_init_frame(eng, CLOSE_CODE);
  if (error)
    amp_engine_field(eng, CLOSE_ERROR, error);
  amp_engine_post_frame(eng, 0);
  return 0;
}


void amp_engine_do_close(amp_engine_t *e, amp_list_t *args)
{
  printf("CLOSE: %s\n", amp_ainspect(args));
}

void amp_engine_dispatch(amp_engine_t *e, uint16_t channel, amp_box_t *body)
{
  amp_object_t *desc = amp_box_tag(body);
  amp_object_t *args = amp_box_value(body);
  uint8_t code = amp_to_uint8(amp_map_get(e->dispatch, desc));
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
    amp_engine_do_transfer(e, channel, args);
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
