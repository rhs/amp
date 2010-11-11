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

#include <amp/scalars.h>
#include <amp/codec.h>

#define SCALAR_CONV(TYPE, NAME, METH)                                   \
  TYPE amp_ ## NAME ## _ ## METH(amp_object_t *o)                       \
  {                                                                     \
    amp_ ## NAME ## _t *n = o;                                          \
    return n->value;                                                    \
  }

#define SCALAR_CONVERSIONS(NAME)                                        \
  SCALAR_CONV(bool, NAME, to_bool)                                      \
  SCALAR_CONV(uint8_t, NAME, to_uint8)                                  \
  SCALAR_CONV(uint16_t, NAME, to_uint16)                                \
  SCALAR_CONV(uint32_t, NAME, to_uint32)                                \
  SCALAR_CONV(uint64_t, NAME, to_uint64)                                \
  SCALAR_CONV(int8_t, NAME, to_int8)                                    \
  SCALAR_CONV(int16_t, NAME, to_int16)                                  \
  SCALAR_CONV(int32_t, NAME, to_int32)                                  \
  SCALAR_CONV(int64_t, NAME, to_int64)                                  \
  SCALAR_CONV(float, NAME, to_float)                                    \
  SCALAR_CONV(double, NAME, to_double)                                  \
  SCALAR_CONV(char, NAME, to_char)                                      \
  SCALAR_CONV(wchar_t, NAME, to_wchar)

/* Boolean */

amp_type_t *BOOLEAN = &AMP_SCALAR(boolean);

amp_boolean_t *amp_boolean(amp_region_t *mem, bool b)
{
  amp_boolean_t *o = amp_allocate(mem, NULL, sizeof(amp_boolean_t));
  if (!o) return NULL;
  o->type = BOOLEAN;
  o->value = b;
  return o;
}

int amp_boolean_inspect(amp_object_t *o, char **pos, char *limit)
{
  amp_boolean_t *b = o;
  return amp_format(pos, limit, b->value ? "true" : "false");
}

intptr_t amp_boolean_hash(amp_object_t *o)
{
  amp_boolean_t *b = o;
  return b->value ? 1 : 0;
}

int amp_boolean_compare(amp_object_t *oa, amp_object_t *ob)
{
  amp_boolean_t *a = oa, *b = ob;
  return (a->value ? 1 : 0) - (b->value ? 1 : 0);
}

size_t amp_boolean_encode_space(amp_object_t *o)
{
  return 1;
}

int amp_boolean_encode(amp_object_t *o, amp_encoder_t *e, char **pos, char *limit)
{
  amp_boolean_t *b = o;
  return amp_write_boolean(pos, limit, b->value);
}

SCALAR_CONVERSIONS(boolean)

/* Numbers */

#define NUMBER_DEF(NAME, TYPE, CTYPE, FORMAT, FTYPE, SIZE)                \
                                                                          \
  /* NAME */                                                              \
                                                                          \
  amp_type_t *TYPE = &AMP_SCALAR(NAME);                                   \
                                                                          \
  amp_ ## NAME ## _t *amp_ ## NAME(amp_region_t *mem, CTYPE n)            \
  {                                                                       \
    amp_ ## NAME ## _t *o = amp_allocate(mem, NULL,                       \
                                         sizeof(amp_ ## NAME ## _t));     \
    if (!o) return NULL;                                                  \
    o->type = TYPE;                                                       \
    o->value = n;                                                         \
    return o;                                                             \
  }                                                                       \
                                                                          \
  intptr_t amp_ ## NAME ## _hash(amp_object_t *o)                              \
  {                                                                       \
    amp_ ## NAME ## _t *n = o;                                            \
    return n->value;                                                      \
  }                                                                       \
                                                                          \
  int amp_ ## NAME ## _compare(amp_object_t *oa, amp_object_t *ob)        \
  {                                                                       \
    amp_ ## NAME ## _t *a = oa, *b = ob;                                  \
    return b->value - a->value;                                           \
  }                                                                       \
                                                                          \
  int amp_ ## NAME ## _inspect(amp_object_t *o, char **pos, char *limit)  \
  {                                                                       \
    amp_ ## NAME ## _t *n = o;                                            \
    return amp_format(pos, limit, FORMAT, (FTYPE) n->value);              \
  }                                                                       \
                                                                          \
  size_t amp_ ## NAME ## _encode_space(amp_object_t *o)                   \
  {                                                                       \
    return SIZE;                                                          \
  }                                                                       \
                                                                          \
  int amp_ ## NAME ## _encode(amp_object_t *o, amp_encoder_t *e,          \
                              char **pos, char *limit)                    \
  {                                                                       \
    amp_ ## NAME ## _t *s = o;                                            \
    return amp_write_ ## NAME(pos, limit, s->value);                      \
  }                                                                       \
                                                                          \
  SCALAR_CONVERSIONS(NAME)

NUMBER_DEF(ubyte, UBYTE, uint8_t, "%hhu", uint8_t, 2)
NUMBER_DEF(ushort, USHORT, uint16_t, "%hu", uint16_t, 3)
NUMBER_DEF(uint, UINT, uint32_t, "%u", uint32_t, 5)
NUMBER_DEF(ulong, ULONG, uint64_t, "%llu", uint64_t, 9)
NUMBER_DEF(byte, BYTE, int8_t, "%hhi", int8_t, 2)
NUMBER_DEF(short, SHORT, int16_t, "%hi", int16_t, 3)
NUMBER_DEF(int, INT, int32_t, "%i", int32_t, 5)
NUMBER_DEF(long, LONG, int64_t, "%lli", int64_t, 9)
NUMBER_DEF(float, FLOAT, float, "%f", float, 5)
NUMBER_DEF(double, DOUBLE, double, "%f", double, 9)
NUMBER_DEF(char, CHAR, wchar_t, "%lc", wint_t, 5)
