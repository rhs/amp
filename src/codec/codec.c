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

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <amp/codec.h>
#include "encodings.h"

typedef union {
  uint32_t i;
  uint32_t a[2];
  uint64_t l;
  float f;
  double d;
} conv_t;

static int amp_write_code(char **pos, char *limit, uint8_t code) {
  char *dst = *pos;
  if (limit - dst < 1) {
    return -1;
  } else {
    dst[0] = code;
    *pos += 1;
    return 0;
  }
}
int amp_write_descriptor(char **pos, char *limit) {
  return amp_write_code(pos, limit, AMPE_DESCRIPTOR);
}
int amp_write_null(char **pos, char *limit) {
  return amp_write_code(pos, limit, AMPE_NULL);
}
int amp_write_boolean(char **pos, char *limit, bool v) {
  if (v) return amp_write_code(pos, limit, AMPE_TRUE);
  else return amp_write_code(pos, limit, AMPE_FALSE);
}

static int amp_write_fixed8(char **pos, char *limit, uint8_t v, uint8_t code) {
  char *dst = *pos;
  if (limit - dst < 2) {
    return -1;
  } else {
    dst[0] = code;
    dst[1] = v;
    *pos += 2;
    return 0;
  }
}

int amp_write_ubyte(char **pos, char *limit, uint8_t v) {
  return amp_write_fixed8(pos, limit, v, AMPE_UBYTE);
}
int amp_write_byte(char **pos, char *limit, int8_t v) {
  return amp_write_fixed8(pos, limit, v, AMPE_BYTE);
}

static int amp_write_fixed16(char **pos, char *limit, uint16_t v,
                             uint8_t code) {
  char *dst = *pos;
  if (limit - dst < 3) {
    return -1;
  } else {
    dst[0] = code;
    *((uint16_t *) (dst + 1)) = htons(v);
    *pos += 3;
    return 0;
  }
}
int amp_write_ushort(char **pos, char *limit, uint16_t v) {
  return amp_write_fixed16(pos, limit, v, AMPE_USHORT);
}
int amp_write_short(char **pos, char *limit, int16_t v) {
  return amp_write_fixed16(pos, limit, v, AMPE_SHORT);
}

static int amp_write_fixed32(char **pos, char *limit, uint32_t v, uint8_t code) {
  char *dst = *pos;
  if (limit - dst < 5) {
    return -1;
  } else {
    dst[0] = code;
    *((uint32_t *) (dst + 1)) = htonl(v);
    *pos += 5;
    return 0;
  }
}
int amp_write_uint(char **pos, char *limit, uint32_t v) {
  return amp_write_fixed32(pos, limit, v, AMPE_UINT);
}
int amp_write_int(char **pos, char *limit, int32_t v) {
  return amp_write_fixed32(pos, limit, v, AMPE_INT);
}
int amp_write_char(char **pos, char *limit, wchar_t v) {
  return amp_write_fixed32(pos, limit, v, AMPE_UTF32);
}
int amp_write_float(char **pos, char *limit, float v) {
  conv_t c;
  c.f = v;
  return amp_write_fixed32(pos, limit, c.i, AMPE_FLOAT);
}

static int amp_write_fixed64(char **pos, char *limit, uint64_t v, uint8_t code) {
  char *dst = *pos;
  if (limit - dst < 9) {
    return -1;
  } else {
    dst[0] = code;
    uint32_t hi = v >> 32;
    uint32_t lo = v;
    *((uint32_t *) (dst + 1)) = htonl(hi);
    *((uint32_t *) (dst + 5)) = htonl(lo);
    *pos += 9;
    return 0;
  }
}
int amp_write_ulong(char **pos, char *limit, uint64_t v) {
  return amp_write_fixed64(pos, limit, v, AMPE_ULONG);
}
int amp_write_long(char **pos, char *limit, int64_t v) {
  return amp_write_fixed64(pos, limit, v, AMPE_LONG);
}
int amp_write_double(char **pos, char *limit, double v) {
  conv_t c;
  c.d = v;
  return amp_write_fixed64(pos, limit, c.l, AMPE_DOUBLE);
}

static int amp_write_variable(char **pos, char *limit, size_t size, char *src,
                              uint8_t code8, uint8_t code32) {
  int n;

  if (size < 256) {
    if ((n = amp_write_fixed8(pos, limit, size, code8)))
      return n;
  } else {
    if ((n = amp_write_fixed32(pos, limit, size, code32)))
      return n;
  }

  if (limit - *pos < size) return -1;

  memcpy(*pos, src, size);
  *pos += size;
  return 0;
}
int amp_write_binary(char **pos, char *limit, size_t size, char *src) {
  return amp_write_variable(pos, limit, size, src, AMPE_VBIN8, AMPE_VBIN32);
}
int amp_write_utf8(char **pos, char *limit, size_t size, char *utf8) {
  return amp_write_variable(pos, limit, size, utf8, AMPE_STR8_UTF8, AMPE_STR32_UTF8);
}
int amp_write_utf16(char **pos, char *limit, size_t size, char *utf16) {
  return amp_write_variable(pos, limit, size, utf16, AMPE_STR8_UTF16, AMPE_STR32_UTF16);
}
int amp_write_symbol(char **pos, char *limit, size_t size, char *symbol) {
  return amp_write_variable(pos, limit, size, symbol, AMPE_SYM8, AMPE_SYM32);
}

int amp_write_start(char **pos, char *limit, char **start) {
  char *dst = *pos;
  if (limit - dst < 9) {
    return -1;
  } else {
    *start = dst;
    *pos += 9;
    return 0;
  }
}

static int amp_write_end(char **pos, char *limit, char *start, size_t count, uint8_t code) {
  int n;
  if ((n = amp_write_fixed32(&start, limit, *pos - start, code)))
    return n;
  *((uint32_t *) start) = htonl(count);
  return 0;
}

int amp_write_list(char **pos, char *limit, char *start, size_t count) {
  return amp_write_end(pos, limit, start, count, AMPE_LIST32);
}

int amp_write_map(char **pos, char *limit, char *start, size_t count) {
  return amp_write_end(pos, limit, start, 2*count, AMPE_MAP32);
}

int amp_read_data(char *bytes, size_t n, amp_data_callbacks_t *cb, void *ctx)
{
  size_t size;
  size_t count;
  conv_t conv;

  for (int offset = 0; offset < n; )
  {
    uint8_t code = bytes[offset];
    offset += 1;

    switch(code)
    {
    case AMPE_DESCRIPTOR:
      cb->on_descriptor(ctx);
      break;
    case AMPE_NULL:
      cb->on_null(ctx);
      break;
    case AMPE_TRUE:
      cb->on_bool(ctx, true);
      break;
    case AMPE_FALSE:
      cb->on_bool(ctx, false);
      break;
    case AMPE_UBYTE:
      cb->on_ubyte(ctx, *((uint8_t *) (bytes + offset)));
      offset += 1;
      break;
    case AMPE_BYTE:
      cb->on_byte(ctx, *((int8_t *) (bytes + offset)));
      offset += 1;
      break;
    case AMPE_USHORT:
      cb->on_ushort(ctx, ntohs(*((uint16_t *) (bytes + offset))));
      offset += 2;
      break;
    case AMPE_SHORT:
      cb->on_short(ctx, (int16_t) ntohs(*((int16_t *) (bytes + offset))));
      offset += 2;
      break;
    case AMPE_UINT:
      cb->on_uint(ctx, ntohl(*((uint32_t *) (bytes + offset))));
      offset += 4;
      break;
    case AMPE_INT:
      cb->on_int(ctx, ntohl(*((uint32_t *) (bytes + offset))));
      offset += 4;
      break;
    case AMPE_FLOAT:
      // XXX: this assumes the platform uses IEEE floats
      conv.i = ntohl(*((uint32_t *) (bytes + offset)));
      cb->on_float(ctx, conv.f);
      offset += 4;
      break;
    case AMPE_ULONG:
    case AMPE_LONG:
    case AMPE_DOUBLE:
      {
        uint32_t hi = ntohl(*((uint32_t *) (bytes + offset)));
        offset += 4;
        uint32_t lo = ntohl(*((uint32_t *) (bytes + offset)));
        offset += 4;
        conv.l = (((uint64_t) hi) << 32) | lo;
      }

      switch (code)
      {
      case AMPE_ULONG:
        cb->on_ulong(ctx, conv.l);
        break;
      case AMPE_LONG:
        cb->on_long(ctx, (int64_t) conv.l);
        break;
      case AMPE_DOUBLE:
        // XXX: this assumes the platform uses IEEE floats
        cb->on_double(ctx, conv.d);
        break;
      default:
        return -1;
      }

      break;
    case AMPE_VBIN8:
    case AMPE_STR8_UTF8:
    case AMPE_STR8_UTF16:
    case AMPE_SYM8:
    case AMPE_VBIN32:
    case AMPE_STR32_UTF8:
    case AMPE_STR32_UTF16:
    case AMPE_SYM32:
      switch (code & 0xF0)
      {
      case 0xA0:
        size = *(uint8_t *) (bytes + offset);
        offset += 1;
        break;
      case 0xB0:
        size = ntohl(*(uint32_t *) (bytes + offset));
        offset += 4;
        break;
      default:
        return -2;
      }

      {
        char *start = (char *) (bytes + offset);
        switch (code & 0x0F)
        {
        case 0x0:
          cb->on_binary(ctx, size, start);
          break;
        case 0x1:
          cb->on_utf8(ctx, size, start);
          break;
        case 0x2:
          cb->on_utf16(ctx, size, start);
          break;
        case 0x3:
          cb->on_symbol(ctx, size, start);
          break;
        default:
          return -3;
        }
      }

      offset += size;
      break;
    case AMPE_LIST8:
    case AMPE_LIST32:
    case AMPE_MAP8:
    case AMPE_MAP32:
      switch (code & 0xF0)
      {
      case 0xC0:
        size = *(uint8_t *) (bytes + offset);
        offset += 1;
        count = *(uint8_t *) (bytes + offset);
        offset += 1;
        break;
      case 0xD0:
        size = ntohl(*(uint32_t *) (bytes + offset));
        offset += 4;
        count = ntohl(*(uint32_t *) (bytes + offset));
        offset += 4;
        break;
      default:
        return -4;
      }

      switch (code & 0x0F)
      {
      case 0x0:
        cb->on_list(ctx, count);
        break;
      case 0x1:
        cb->on_map(ctx, count);
        break;
      default:
        return -5;
      }
      break;
    default:
      return -6;
    }
  }

  return 0;
}

void noop_null(void *ctx) {}
void noop_bool(void *ctx, bool v) {}
void noop_ubyte(void *ctx, uint8_t v) {}
void noop_byte(void *ctx, int8_t v) {}
void noop_ushort(void *ctx, uint16_t v) {}
void noop_short(void *ctx, int16_t v) {}
void noop_uint(void *ctx, uint32_t v) {}
void noop_int(void *ctx, int32_t v) {}
void noop_float(void *ctx, float f) {}
void noop_ulong(void *ctx, uint64_t v) {}
void noop_long(void *ctx, int64_t v) {}
void noop_double(void *ctx, double v) {}
void noop_binary(void *ctx, size_t size, char *bytes) {}
void noop_utf8(void *ctx, size_t size, char *bytes) {}
void noop_utf16(void *ctx, size_t size, char *bytes) {}
void noop_symbol(void *ctx, size_t size, char *bytes) {}
void noop_list(void *ctx, size_t count) {}
void noop_map(void *ctx, size_t count) {}
void noop_descriptor(void *ctx) {}

amp_data_callbacks_t *noop = &AMP_DATA_CALLBACKS(noop);

void print_null(void *ctx) { printf("null\n"); }
void print_bool(void *ctx, bool v) { if (v) printf("true\n"); else printf("false\n"); }
void print_ubyte(void *ctx, uint8_t v) { printf("%hhu\n", v); }
void print_byte(void *ctx, int8_t v) { printf("%hhi\n", v); }
void print_ushort(void *ctx, uint16_t v) { printf("%hu\n", v); }
void print_short(void *ctx, int16_t v) { printf("%hi\n", v); }
void print_uint(void *ctx, uint32_t v) { printf("%u\n", v); }
void print_int(void *ctx, int32_t v) { printf("%i\n", v); }
void print_float(void *ctx, float v) { printf("%f\n", v); }
void print_ulong(void *ctx, uint64_t v) { printf("%llu\n", v); }
void print_long(void *ctx, int64_t v) { printf("%lli\n", v); }
void print_double(void *ctx, double v) { printf("%f\n", v); }

void print_bytes(char *label, size_t size, char *bytes) {
  printf("%s(%.*s)\n", label, size, bytes);
}

void print_binary(void *ctx, size_t size, char *bytes) { print_bytes("bin", size, bytes); }
void print_utf8(void *ctx, size_t size, char *bytes) { print_bytes("utf8", size, bytes); }
void print_utf16(void *ctx, size_t size, char *bytes) { print_bytes("utf16", size, bytes); }
void print_symbol(void *ctx, size_t size, char *bytes) { print_bytes("sym", size, bytes); }
void print_list(void *ctx, size_t count) { printf("begin list %d\n", count); }
void print_map(void *ctx, size_t count) { printf("begin map %d\n", count); }
void print_descriptor(void *ctx) { printf("descriptor "); }

amp_data_callbacks_t *printer = &AMP_DATA_CALLBACKS(print);
