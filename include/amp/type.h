#ifndef _AMP_TYPE_H
#define _AMP_TYPE_H 1

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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <wchar.h>
#include <amp/allocation.h>
#include <amp/encoder.h>

typedef void amp_object_t;
typedef struct amp_head_st amp_head_t;
typedef struct amp_type_st amp_type_t;

#define AMP_HEAD amp_type_t *type

struct amp_head_st {
  AMP_HEAD;
};

struct amp_type_st {
  const char *name;

  /* Basic Protocol */
  int (*inspect)(amp_object_t *o, char **pos, char *limit);
  intptr_t (*hash)(amp_object_t *o);
  int (*compare)(amp_object_t *a, amp_object_t *b);
  bool encodable;
  size_t (*encode_space)(amp_object_t *o);
  int (*encode)(amp_object_t *o, amp_encoder_t *e, char **p, char *l);
  bool scalar;

  /* Scalar Protocol */
  bool (*to_bool)(amp_object_t *o);
  uint8_t (*to_uint8)(amp_object_t *o);
  uint16_t (*to_uint16)(amp_object_t *o);
  uint32_t (*to_uint32)(amp_object_t *o);
  uint64_t (*to_uint64)(amp_object_t *o);
  int8_t (*to_int8)(amp_object_t *o);
  int16_t (*to_int16)(amp_object_t *o);
  int32_t (*to_int32)(amp_object_t *o);
  int64_t (*to_int64)(amp_object_t *o);
  float (*to_float)(amp_object_t *o);
  double (*to_double)(amp_object_t *o);
  char (*to_char)(amp_object_t *o);
  wchar_t (*to_wchar)(amp_object_t *o);
};

#define amp_head(OBJECT) ((amp_head_t *) (OBJECT))
#define amp_type(OBJECT) (amp_head(OBJECT)->type)

#define AMP_TYPE(NAME) ((amp_type_t) {            \
  /* Basic Protocol */                            \
  .name = #NAME,                                  \
  .inspect = &amp_ ## NAME ## _inspect,           \
  .hash = &amp_ ## NAME ## _hash,                 \
  .compare = &amp_ ## NAME ## _compare,           \
  .encodable = false,                             \
  .encode_space = &amp_default_encode_space,      \
  .encode = &amp_default_encode,                  \
  .scalar = false,                                \
                                                  \
  /* Scalar Protocol */                           \
  .to_bool = &amp_default_to_bool,                \
  .to_uint8 = &amp_default_to_uint8,              \
  .to_uint16 = &amp_default_to_uint16,            \
  .to_uint32 = &amp_default_to_uint32,            \
  .to_uint64 = &amp_default_to_uint64,            \
  .to_int8 = &amp_default_to_int8,                \
  .to_int16 = &amp_default_to_int16,              \
  .to_int32 = &amp_default_to_int32,              \
  .to_int64 = &amp_default_to_int64,              \
  .to_float = &amp_default_to_float,              \
  .to_double = &amp_default_to_double,            \
  .to_char = &amp_default_to_char,                \
  .to_wchar = &amp_default_to_wchar               \
})

#define AMP_ENCODABLE(NAME) ((amp_type_t) {       \
  /* Basic Protocol */                            \
  .name = #NAME,                                  \
  .inspect = &amp_ ## NAME ## _inspect,           \
  .hash = &amp_ ## NAME ## _hash,                 \
  .compare = &amp_ ## NAME ## _compare,           \
  .encodable = true,                              \
  .encode_space = &amp_ ## NAME ## _encode_space, \
  .encode = &amp_ ## NAME ## _encode,             \
  .scalar = false,                                \
                                                  \
  /* Scalar Protocol */                           \
  .to_bool = &amp_default_to_bool,                \
  .to_uint8 = &amp_default_to_uint8,              \
  .to_uint16 = &amp_default_to_uint16,            \
  .to_uint32 = &amp_default_to_uint32,            \
  .to_uint64 = &amp_default_to_uint64,            \
  .to_int8 = &amp_default_to_int8,                \
  .to_int16 = &amp_default_to_int16,              \
  .to_int32 = &amp_default_to_int32,              \
  .to_int64 = &amp_default_to_int64,              \
  .to_float = &amp_default_to_float,              \
  .to_double = &amp_default_to_double,            \
  .to_char = &amp_default_to_char,                \
  .to_wchar = &amp_default_to_wchar               \
})

#define AMP_SCALAR(NAME) ((amp_type_t) {        \
  /* Basic Protocol */                               \
  .name = #NAME,                                     \
  .inspect = &amp_ ## NAME ## _inspect,              \
  .hash = &amp_ ## NAME ## _hash,                    \
  .compare = &amp_ ## NAME ## _compare,              \
  .encodable = true,                                 \
  .encode_space = &amp_ ## NAME ## _encode_space,    \
  .encode = &amp_ ## NAME ## _encode,                \
  .scalar = true,                                    \
                                                     \
  /* Scalar Protocol */                              \
  .to_bool = &amp_ ## NAME ## _to_bool,              \
  .to_uint8 = &amp_ ## NAME ## _to_uint8,            \
  .to_uint16 = &amp_ ## NAME ## _to_uint16,          \
  .to_uint32 = &amp_ ## NAME ## _to_uint32,          \
  .to_uint64 = &amp_ ## NAME ## _to_uint64,          \
  .to_int8 = &amp_ ## NAME ## _to_int8,              \
  .to_int16 = &amp_ ## NAME ## _to_int16,            \
  .to_int32 = &amp_ ## NAME ## _to_int32,            \
  .to_int64 = &amp_ ## NAME ## _to_int64,            \
  .to_float = &amp_ ## NAME ## _to_float,            \
  .to_double = &amp_ ## NAME ## _to_double,          \
  .to_char = &amp_ ## NAME ## _to_char,              \
  .to_wchar = &amp_ ## NAME ## _to_wchar             \
})

#define AMP_TYPE_DECL(TYPE, STEM)                                         \
  extern amp_type_t *TYPE;                                                \
  int amp_ ## STEM ## _inspect(amp_object_t *o, char **pos, char *limit); \
  intptr_t amp_ ## STEM ## _hash(amp_object_t *o);                             \
  int amp_ ## STEM ## _compare(amp_object_t *a, amp_object_t *b);

#define AMP_ENCODABLE_DECL(TYPE, STEM)                                  \
  AMP_TYPE_DECL(TYPE, STEM)                                             \
  size_t amp_ ## STEM ## _encode_space(amp_object_t *o);                \
  int amp_ ## STEM ## _encode(amp_object_t *o, amp_encoder_t *e,        \
                              char **p, char *l);

#define AMP_SCALAR_DECL(TYPE, STEM)                       \
  AMP_ENCODABLE_DECL(TYPE, STEM)                          \
  bool amp_ ## STEM ## _to_bool(amp_object_t *o);         \
  uint8_t amp_ ## STEM ## _to_uint8(amp_object_t *o);     \
  uint16_t amp_ ## STEM ## _to_uint16(amp_object_t *o);   \
  uint32_t amp_ ## STEM ## _to_uint32(amp_object_t *o);   \
  uint64_t amp_ ## STEM ## _to_uint64(amp_object_t *o);   \
  int8_t amp_ ## STEM ## _to_int8(amp_object_t *o);       \
  int16_t amp_ ## STEM ## _to_int16(amp_object_t *o);     \
  int32_t amp_ ## STEM ## _to_int32(amp_object_t *o);     \
  int64_t amp_ ## STEM ## _to_int64(amp_object_t *o);     \
  float amp_ ## STEM ## _to_float(amp_object_t *o);       \
  double amp_ ## STEM ## _to_double(amp_object_t *o);     \
  char amp_ ## STEM ## _to_char(amp_object_t *o);         \
  wchar_t amp_ ## STEM ## _to_wchar(amp_object_t *o);

/* Basic Protocol */
int amp_format(char **pos, char *limit, const char *fmt, ...);
int amp_inspect(amp_object_t *o, char **pos, char *limit);
char *amp_ainspect(amp_object_t *o);
intptr_t amp_hash(amp_object_t *o);
int amp_compare(amp_object_t *a, amp_object_t *b);
bool amp_equal(amp_object_t *a, amp_object_t *b);
bool amp_isa(amp_object_t *o, amp_type_t *t);
size_t amp_encode_space(amp_object_t *o);
int amp_encode(amp_object_t *o, amp_encoder_t *e, char **p, char *l);
bool amp_scalar(amp_object_t *o);

/* Default Prototypes */

AMP_SCALAR_DECL(DEFAULT, default)

void amp_unimplemented(amp_object_t *o, const char *msg);

/* Convenience */

#define AMP_DEFAULT_INSPECT(STEM)                                        \
  int amp_ ## STEM ## _inspect(amp_object_t *o, char **pos, char *limit) \
  {                                                                      \
    int n;                                                               \
    if ((n = amp_format(pos, limit, #STEM))) return n;                   \
    if ((n = amp_format(pos, limit, "<%p>", o))) return n;               \
    return 0;                                                            \
  }                                                                      \

#define AMP_DEFAULT_HASH(STEM)                        \
  intptr_t amp_ ## STEM ## _hash(amp_object_t *o)     \
  {                                                   \
    return (intptr_t) o;                              \
  }

#define AMP_DEFAULT_COMPARE(STEM)                                 \
  int amp_ ## STEM ## _compare(amp_object_t *a, amp_object_t *b)  \
  {                                                               \
    return b != a;                                                \
  }

#endif /* type.h */
