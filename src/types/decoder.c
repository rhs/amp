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

#include <amp/decoder.h>
#include <amp/scalars.h>
#include <amp/binary.h>
#include <amp/string.h>
#include <amp/symbol.h>
#include <amp/list.h>
#include <amp/map.h>
#include <amp/box.h>

static void dec_push(amp_decoder_t *dec, amp_stop_t op, amp_object_t *value, int limit)
{
  amp_stack_t *stack = &dec->stack[dec->depth++];
  stack->op = op;
  stack->value = value;
  stack->limit = limit;
  stack->count = 0;
}

amp_decoder_t* amp_decoder(amp_region_t *mem)
{
  amp_decoder_t *dec = amp_allocate(mem, NULL, sizeof(amp_decoder_t));
  dec->utf8 = iconv_open("WCHAR_T", "UTF-8");
  dec->utf16 = iconv_open("WCHAR_T", "UTF-16BE");
  return dec;
}

void amp_decoder_init(amp_decoder_t *dec, amp_region_t *region)
{
  iconv(dec->utf8, NULL, NULL, NULL, NULL);
  iconv(dec->utf16, NULL, NULL, NULL, NULL);
  dec->size = 0;
  dec->depth = 0;
  dec_push(dec, ST_NOOP, NULL, -1);
  dec->region = region;
}

static void value(amp_decoder_t *dec, amp_object_t *o)
{
  amp_stack_t *stack = &dec->stack[dec->depth-1];
  switch (stack->op)
  {
  case ST_NOOP:
    dec->values[dec->size++] = o;
    break;
  case ST_LIST:
    amp_list_add(stack->value, o);
    break;
  case ST_MAP:
    if ((stack->count % 2) == 0)
      stack->key = o;
    else
      amp_map_set(stack->value, stack->key, o);
    break;
  case ST_DESC:
    if (stack->count == 0)
      amp_box_set_tag(stack->value, o);
    else
      amp_box_set_value(stack->value, o);
    break;
  }

  if (++(stack->count) == stack->limit) {
    dec->depth--;
  }
}

void dec_null(void *ctx) {
  amp_decoder_t *dec = ctx;
  value(dec, NULL);
}

void dec_bool(void *ctx, bool b) {
  amp_decoder_t *dec = ctx;
  value(dec, amp_boolean(dec->region, b));
}

void dec_ubyte(void *ctx, uint8_t u) {
  amp_decoder_t *dec = ctx;
  value(dec, amp_ubyte(dec->region, u));
}

void dec_byte(void *ctx, int8_t i) {
  amp_decoder_t *dec = ctx;
  value(dec, amp_ubyte(dec->region, i));
}

void dec_ushort(void *ctx, uint16_t u) {
  amp_decoder_t *dec = ctx;
  value(dec, amp_ushort(dec->region, u));
}

void dec_short(void *ctx, int16_t i) {
  amp_decoder_t *dec = ctx;
  value(dec, amp_short(dec->region, i));
}

void dec_uint(void *ctx, uint32_t u) {
  amp_decoder_t *dec = ctx;
  value(dec, amp_uint(dec->region, u));
}

void dec_int(void *ctx, int32_t i) {
  amp_decoder_t *dec = ctx;
  value(dec, amp_int(dec->region, i));
}

void dec_float(void *ctx, float f) {
  amp_decoder_t *dec = ctx;
  value(dec, amp_float(dec->region, f));
}

void dec_ulong(void *ctx, uint64_t u) {
  amp_decoder_t *dec = ctx;
  value(dec, amp_ulong(dec->region, u));
}

void dec_long(void *ctx, int64_t i) {
  amp_decoder_t *dec = ctx;
  value(dec, amp_long(dec->region, i));
}

void dec_double(void *ctx, double d) {
  amp_decoder_t *dec = ctx;
  value(dec, amp_double(dec->region, d));
}

void dec_binary(void *ctx, size_t size, char *bytes) {
  amp_decoder_t *dec = ctx;
  amp_binary_t *bin = amp_binary(dec->region, size);
  amp_binary_extend(bin, bytes, size);
  value(dec, bin);
}

// XXX
#include <stdio.h>

void dec_utf8(void *ctx, size_t size, char *bytes) {
  amp_decoder_t *dec = ctx;
  // XXX
  wchar_t buf[1024];
  wchar_t *out = buf;
  size_t remaining = 1024;
  size_t n = iconv(dec->utf8, &bytes, &size, (char **)&out, &remaining);
  if (n == -1)
  {
    perror("dec_utf8");
  }
  value(dec, amp_stringn(dec->region, buf, out - buf));
}

void dec_utf16(void *ctx, size_t size, char *bytes) {
  amp_decoder_t *dec = ctx;
  // XXX
  value(dec, amp_string(dec->region, L"fake utf16"));
}

void dec_symbol(void *ctx, size_t size, char *bytes) {
  amp_decoder_t *dec = ctx;
  value(dec, amp_symboln(dec->region, bytes, size));
}

void dec_list(void *ctx, size_t count) {
  amp_decoder_t *dec = ctx;
  amp_object_t *o = amp_list(dec->region, count);
  value(dec, o);
  dec_push(dec, ST_LIST, o, count);
}

void dec_map(void *ctx, size_t count) {
  amp_decoder_t *dec = ctx;
  amp_object_t *o = amp_map(dec->region, count);
  value(dec, o);
  dec_push(dec, ST_MAP, o, count);
}

void dec_descriptor(void *ctx) {
  amp_decoder_t *dec = ctx;
  amp_box_t *box = amp_box(dec->region, NULL, NULL);
  value(dec, box);
  dec_push(dec, ST_DESC, box, 2);
}

amp_data_callbacks_t *decoder = &AMP_DATA_CALLBACKS(dec);
