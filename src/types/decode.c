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

#include <amp/codec.h>
#include <amp/value.h>
#include <iconv.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include "../codec/encodings.h"

static enum TYPE amqp_code_to_type(uint8_t code)
{
  switch (code)
  {
  case AMPE_NULL: return EMPTY;
    //  case AMPE_TRUE:
    //  case AMPE_FALSE:
    //  case AMPE_BOOLEAN: return BOOLEAN;
  case AMPE_UBYTE: return UBYTE;
  case AMPE_BYTE: return BYTE;
  case AMPE_USHORT: return USHORT;
  case AMPE_SHORT: return SHORT;
  case AMPE_UINT:
  case AMPE_UINT0: return UINT;
  case AMPE_INT: return INT;
  case AMPE_FLOAT: return FLOAT;
  case AMPE_ULONG0:
  case AMPE_ULONG: return ULONG;
  case AMPE_LONG: return LONG;
  case AMPE_DOUBLE: return DOUBLE;
  case AMPE_VBIN8:
  case AMPE_VBIN32: return BINARY;
  case AMPE_STR8_UTF8:
  case AMPE_STR32_UTF8: return STRING;
    //  case AMPE_SYM8:
    //  case AMPE_SYM32: return SYMBOL;
  case AMPE_LIST0:
  case AMPE_LIST8:
  case AMPE_LIST32: return LIST;
  case AMPE_ARRAY8:
  case AMPE_ARRAY32: return ARRAY;
  case AMPE_MAP8:
  case AMPE_MAP32: return MAP;
  }
  return -1;
}

struct amp_decode_context_frame_st {
  size_t count;
  size_t limit;
  amp_value_t *values;
};

struct amp_decode_context_st {
  size_t depth;
  struct amp_decode_context_frame_st frames[1024];
};

#define CTX_CAST(ctx) ((struct amp_decode_context_st *) (ctx))

static amp_value_t *push_frame(void *ptr, amp_value_t *values, size_t limit)
{
  struct amp_decode_context_st *ctx = CTX_CAST(ptr);
  struct amp_decode_context_frame_st *old = &ctx->frames[ctx->depth - 1];
  struct amp_decode_context_frame_st *frm = &ctx->frames[ctx->depth++];
  frm->count = 0;
  frm->limit = limit;
  frm->values = values;
  return &old->values[old->count];
}

static void pop_frame(void *ptr)
{
  struct amp_decode_context_st *ctx = CTX_CAST(ptr);
  ctx->depth--;
}

static amp_value_t *next_value(void *ptr)
{
  struct amp_decode_context_st *ctx = CTX_CAST(ptr);
  struct amp_decode_context_frame_st *frm = &ctx->frames[ctx->depth-1];
  amp_value_t *result = &frm->values[frm->count];
  frm->count++;
  if (frm->count == frm->limit) {
    pop_frame(ptr);
  }
  return result;
}

void amp_decode_null(void *ctx) {
  amp_value_t *value = next_value(ctx);
  value->type = EMPTY;
}
void amp_decode_bool(void *ctx, bool v) {
  //amp_value_t *value = VALUE(ctx);
  //  value->type = BOOLEAN;
  //  value->u.as_bool = v;
}
void amp_decode_ubyte(void *ctx, uint8_t v) {
  amp_value_t *value = next_value(ctx);
  value->type = UBYTE;
  value->u.as_ubyte = v;
}
void amp_decode_byte(void *ctx, int8_t v) {
  amp_value_t *value = next_value(ctx);
  value->type = BYTE;
  value->u.as_byte = v;
}
void amp_decode_ushort(void *ctx, uint16_t v) {
  amp_value_t *value = next_value(ctx);
  value->type = USHORT;
  value->u.as_ushort = v;
}
void amp_decode_short(void *ctx, int16_t v) {
  amp_value_t *value = next_value(ctx);
  value->type = SHORT;
  value->u.as_short = v;
}
void amp_decode_uint(void *ctx, uint32_t v) {
  amp_value_t *value = next_value(ctx);
  value->type = UINT;
  value->u.as_uint = v;
}
void amp_decode_int(void *ctx, int32_t v) {
  amp_value_t *value = next_value(ctx);
  value->type = INT;
  value->u.as_int = v;
}
void amp_decode_float(void *ctx, float f) {
  amp_value_t *value = next_value(ctx);
  value->type = FLOAT;
  value->u.as_float = f;
}
void amp_decode_ulong(void *ctx, uint64_t v) {
  amp_value_t *value = next_value(ctx);
  value->type = ULONG;
  value->u.as_ulong = v;
}
void amp_decode_long(void *ctx, int64_t v) {
  amp_value_t *value = next_value(ctx);
  value->type = LONG;
  value->u.as_long = v;
}
void amp_decode_double(void *ctx, double v) {
  amp_value_t *value = next_value(ctx);
  value->type = DOUBLE;
  value->u.as_double = v;
}
void amp_decode_binary(void *ctx, size_t size, char *bytes) {
  amp_value_t *value = next_value(ctx);
  value->type = BINARY;
  value->u.as_binary = (amp_binary_t) {.size = size, .bytes = bytes};
}
void amp_decode_utf8(void *ctx, size_t size, char *bytes) {
  amp_value_t *value = next_value(ctx);
  value->type = STRING;
  // XXX: this is a leak
  size_t remaining = (size+1)*sizeof(wchar_t);
  wchar_t *buf = malloc(remaining);
  iconv_t cd = iconv_open("WCHAR_T", "UTF-8");
  wchar_t *out = buf;
  size_t n = iconv(cd, &bytes, &size, (char **)&out, &remaining);
  if (n == -1)
  {
    perror("amp_decode_utf8");
  }
  *out = L'\0';
  iconv_close(cd);
  value->u.as_string = (amp_string_t) {.size = wcslen(buf), .wcs = buf};
}
void amp_decode_symbol(void *ctx, size_t size, char *bytes) {
  //  amp_value_t *value = next_value(ctx);
  //  value->type = SYMBOL;
  //  value->u.as_symbol = {.size = size, .bytes = bytes};
}

void amp_decode_start_array(void *ctx, size_t count, uint8_t code) {
  amp_array_t *array = amp_array(amqp_code_to_type(code), count);
  array->size = count;
  amp_value_t *value = push_frame(ctx, array->values, count);
  value->type = ARRAY;
  value->u.as_array = array;
}
void amp_decode_stop_array(void *ctx, size_t count, uint8_t code) {}

void amp_decode_start_list(void *ctx, size_t count) {
  amp_list_t *list = amp_list(count);
  list->size = count;
  amp_value_t *value = push_frame(ctx, list->values, count);
  value->type = LIST;
  value->u.as_list = list;
}

void amp_decode_stop_list(void *ctx, size_t count) {}

void amp_decode_start_map(void *ctx, size_t count) {
  amp_map_t *map = amp_map(count/2);
  map->size = count/2;
  amp_value_t *value = push_frame(ctx, map->pairs, count);
  value->type = MAP;
  value->u.as_map = map;
}

void amp_decode_stop_map(void *ctx, size_t count) {}

void amp_decode_start_descriptor(void *ctx) {
  amp_tag_t *tag = amp_tag(EMPTY_VALUE, EMPTY_VALUE);
  amp_value_t *value = push_frame(ctx, &tag->value, 1);
  value->type = TAG;
  value->u.as_tag = tag;
  push_frame(ctx, &tag->descriptor, 1);
}

void amp_decode_stop_descriptor(void *ctx) {}

amp_data_callbacks_t *amp_decoder = &AMP_DATA_CALLBACKS(amp_decode);

ssize_t amp_decode(amp_value_t *v, char *bytes, size_t n)
{
  struct amp_decode_context_st ctx = {.depth = 0};
  push_frame(&ctx, v, 1);
  ssize_t read = amp_read_datum(bytes, n, amp_decoder, &ctx);
  return read;
}
