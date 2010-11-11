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

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <amp/type.h>
#include <amp/codec.h>

/* Basic Protocol */

int amp_format(char **pos, char *limit, const char *fmt, ...)
{
  va_list ap;
  int result;
  char *dst = *pos;
  int n = limit - dst;
  va_start(ap, fmt);
  result = vsnprintf(dst, n, fmt, ap);
  va_end(ap);
  if (result >= n) {
    *pos += n;
    return -1;
  } else if (result >= 0) {
    *pos += result;
    return 0;
  } else {
    // XXX: should convert error codes
    return result;
  }
}

int amp_inspect(amp_object_t *o, char **pos, char *limit)
{
  if (o)
    return amp_type(o)->inspect(o, pos, limit);
  else
    return amp_format(pos, limit, "NULL");
}

char *amp_ainspect(amp_object_t *o)
{
  int size = 16;
  while (true)
  {
    char *buf = malloc(size);
    char *pos = buf;
    if (!buf) return NULL;
    int n = amp_inspect(o, &pos, buf + size);
    if (!n) {
      pos[0] = '\0';
      return buf;
    } else if (n == -1) {
      size *= 2;
      free(buf);
    } else {
      // XXX
      free(buf);
      return NULL;
    }
  }
}

intptr_t amp_hash(amp_object_t *o)
{
  if (o)
    return amp_type(o)->hash(o);
  else
    return 0;
}

int amp_compare(amp_object_t *a, amp_object_t *b)
{
  if (a == NULL || b == NULL)
    return a == b ? 0 : -1;
  else if (amp_type(a) == amp_type(b))
    return  amp_type(a)->compare(a, b);
  return -1;
}

bool amp_equal(amp_object_t *a, amp_object_t *b)
{
  return amp_compare(a, b) == 0;
}

bool amp_isa(amp_object_t *o, amp_type_t *t)
{
  return amp_type(o) == t;
}

size_t amp_encode_space(amp_object_t *o)
{
  if (o)
    return amp_type(o)->encode_space(o);
  else
    return 1;
}

int amp_encode(amp_object_t *o, amp_encoder_t *enc, char **pos, char *limit)
{
  if (o)
    return amp_type(o)->encode(o, enc, pos, limit);
  else
    return amp_write_null(pos, limit);
}

bool amp_scalar(amp_object_t *o)
{
  return amp_type(o)->scalar;
}

/* Default Implementation */

#define DEFAULT_DEF(RETURN, NAME) \
  RETURN amp_default_ ## NAME(amp_object_t *o) {                        \
    amp_unimplemented(o, #NAME);                                        \
    exit(-1);                                                           \
  }

DEFAULT_DEF(bool, to_bool)
DEFAULT_DEF(uint8_t, to_uint8)
DEFAULT_DEF(uint16_t, to_uint16)
DEFAULT_DEF(uint32_t, to_uint32)
DEFAULT_DEF(uint64_t, to_uint64)
DEFAULT_DEF(int8_t, to_int8)
DEFAULT_DEF(int16_t, to_int16)
DEFAULT_DEF(int32_t, to_int32)
DEFAULT_DEF(int64_t, to_int64)
DEFAULT_DEF(float, to_float)
DEFAULT_DEF(double, to_double)
DEFAULT_DEF(char, to_char)
DEFAULT_DEF(wchar_t, to_wchar)

DEFAULT_DEF(size_t, encode_space)

int amp_default_encode(amp_object_t *o, amp_encoder_t *e, char **pos, char *limit)
{
  amp_unimplemented(o, "encode");
  exit(-1);
}

void amp_unimplemented(amp_object_t *o, const char *msg)
{
  fprintf(stderr, "%s: %s does not support %s\n", amp_ainspect(o),
          amp_type(o)->name, msg);
  exit(-1);
}
