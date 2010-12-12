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
#include <string.h>
#include "../protocol.h"

struct amp_error_t {
  AMP_HEAD;
  int code;
  // ...
};

struct amp_engine_t {
  AMP_HEAD;
  amp_error_t error;
  amp_connection_t *connection;
  amp_region_t *region;
  amp_decoder_t *decoder;
  amp_map_t *dispatch;

  amp_encoder_t *encoder;
  char *output;
  size_t available;
  size_t capacity;

  bool open_sent;
  bool open_rcvd;
  bool close_sent;
  bool close_rcvd;
};

AMP_TYPE_DECL(ENGINE, engine)

amp_type_t *ENGINE = &AMP_TYPE(engine);

#define __DISPATCH(MAP, NAME)                                           \
  amp_map_set(MAP, amp_symbol(AMP_HEAP, NAME ## _NAME), amp_byte(AMP_HEAP, NAME)); \
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

  o->encoder = amp_encoder(AMP_HEAP);
  // XXX
  o->capacity = 4*1024;
  o->output = amp_allocate(AMP_HEAP, NULL, o->capacity);
  o->available = 0;

  o->open_sent = false;
  o->open_rcvd = false;
  o->close_sent = false;
  o->close_rcvd = false;

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

int amp_engine_input(amp_engine_t *engine, char *src, size_t available)
{
  amp_frame_t frame;
  size_t n = amp_read_frame(&frame, src, available);
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
  }

  return n;
}

int amp_engine_output(amp_engine_t *engine, char *dst, size_t n)
{
  memmove(dst, engine->output, engine->available < n ? engine->available : n);
  // XXX: need to check engine for errors
  return 0;
}

#define BUF_SIZE (1024*1024)
#include <stdio.h>
void amp_engine_post_frame(amp_engine_t *e, uint16_t channel, amp_box_t *body)
{
  amp_frame_t frame = {0};
  char bytes[BUF_SIZE];
  char *pos = bytes;
  amp_encoder_init(e->encoder);
  amp_encode(body, e->encoder, &pos, pos + BUF_SIZE);
  frame.payload = bytes;
  frame.size = pos - bytes;
  size_t n;
  while (!(n = amp_write_frame(e->output, e->capacity - e->available, frame))) {
    e->capacity *= 2;
    e->output = amp_allocate(AMP_HEAP, e->output, e->capacity);
  }
  e->available += n;
}

#define DOUBLE_CLOSE (1)

int amp_engine_close(amp_engine_t *e, char *condition, wchar_t *description)
{
  if (e->close_sent)
  {
    e->error.code = DOUBLE_CLOSE;
    return -1;
  }

  amp_box_t *error = amp_proto_error(e->region, CONDITION, condition,
                                     DESCRIPTION, description);
  amp_engine_post_frame(e, 0, amp_proto_close(e->region, ERROR, error));
  e->close_sent = true;
  return 0;
}

#include <stdio.h>

void amp_engine_do_open(amp_engine_t *e, amp_list_t *args)
{
  printf("OPEN: %s\n", amp_ainspect(args));
  if (e->open_rcvd) {
    amp_engine_close(e, "double-open", L"double open");
  } else {
    e->open_rcvd = true;
  }
}

void amp_engine_do_begin(amp_engine_t *e, uint16_t channel, amp_list_t *args)
{
  printf("BEGIN: %s\n", amp_ainspect(args));
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

void amp_engine_do_detach(amp_engine_t *e, uint16_t channel, amp_list_t *args)
{
  printf("DETACH: %s\n", amp_ainspect(args));
}

void amp_engine_do_end(amp_engine_t *e, uint16_t channel, amp_list_t *args)
{
  printf("END: %s\n", amp_ainspect(args));
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
