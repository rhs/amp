#ifndef _AMP_VALUE_H
#define _AMP_VALUE_H 1

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

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

enum TYPE {
  EMPTY,
  UBYTE,
  USHORT,
  UINT,
  ULONG,
  BYTE,
  SHORT,
  INT,
  LONG,
  FLOAT,
  DOUBLE,
  CHAR,
  STRING,
  BINARY,
  ARRAY,
  LIST,
  MAP,
  TAG
};

typedef struct amp_value_st amp_value_t;
typedef struct amp_string_st amp_string_t;
typedef struct amp_binary_st amp_binary_t;
typedef struct amp_array_st amp_array_t;
typedef struct amp_list_st amp_list_t;
typedef struct amp_map_st amp_map_t;
typedef struct amp_tag_st amp_tag_t;

struct amp_string_st {
  size_t size;
  wchar_t *wcs;
};

struct amp_binary_st {
  size_t size;
  char *bytes;
};

struct  amp_value_st {
  enum TYPE type;
  union {
    uint8_t as_ubyte;
    uint16_t as_ushort;
    uint32_t as_uint;
    uint64_t as_ulong;
    int8_t as_byte;
    int16_t as_short;
    int32_t as_int;
    int64_t as_long;
    float as_float;
    double as_double;
    wchar_t as_char;
    amp_string_t as_string;
    amp_binary_t as_binary;
    amp_array_t *as_array;
    amp_list_t *as_list;
    amp_map_t *as_map;
    amp_tag_t *as_tag;
  } u;
};

struct amp_array_st {
  enum TYPE type;
  size_t size;
  size_t capacity;
  amp_value_t values[];
};

struct amp_list_st {
  size_t size;
  size_t capacity;
  amp_value_t values[];
};

struct amp_map_st {
  size_t size;
  size_t capacity;
  amp_value_t pairs[];
};

struct amp_tag_st {
  amp_value_t descriptor;
  amp_value_t value;
};

#define EMPTY_VALUE ((amp_value_t) {.type = EMPTY})

int amp_scan(amp_value_t *value, const char *fmt, ...);
int amp_vscan(amp_value_t *value, const char *fmt, va_list ap);
amp_value_t amp_value(const char *fmt, ...);
amp_value_t amp_vvalue(const char *fmt, va_list ap);

amp_list_t *amp_to_list(amp_value_t v);
amp_map_t *amp_to_map(amp_value_t v);
amp_tag_t *amp_to_tag(amp_value_t v);

amp_value_t amp_from_list(amp_list_t *l);
amp_value_t amp_from_map(amp_map_t *m);
amp_value_t amp_from_tag(amp_tag_t *t);

int amp_compare_string(amp_string_t a, amp_string_t b);
int amp_compare_binary(amp_binary_t a, amp_binary_t b);
int amp_compare_list(amp_list_t *a, amp_list_t *b);
int amp_compare_map(amp_map_t *a, amp_map_t *b);
int amp_compare_tag(amp_tag_t *a, amp_tag_t *b);
int amp_compare_value(amp_value_t a, amp_value_t b);

uintptr_t amp_hash_string(amp_string_t s);
uintptr_t amp_hash_binary(amp_binary_t b);
uintptr_t amp_hash_list(amp_list_t *l);
uintptr_t amp_hash_map(amp_map_t *m);
uintptr_t amp_hash_tag(amp_tag_t *t);
uintptr_t amp_hash_value(amp_value_t v);

int amp_format_binary(char **pos, char *limit, amp_binary_t binary);
int amp_format_array(char **pos, char *limit, amp_array_t *array);
int amp_format_list(char **pos, char *limit, amp_list_t *list);
int amp_format_map(char **pos, char *limit, amp_map_t *map);
int amp_format_tag(char **pos, char *limit, amp_tag_t *tag);
int amp_format_value(char **pos, char *limit, amp_value_t *values, size_t n);

/* arrays */

amp_array_t *amp_array(enum TYPE type, int capacity);
amp_value_t amp_array_get(amp_array_t *a, int index);

/* lists */

amp_list_t *amp_vlist(int capacity);
amp_value_t amp_vlist_get(amp_list_t *l, int index);
int amp_vlist_extend(amp_list_t *l, const char *fmt, ...);

/* maps */

amp_map_t *amp_vmap(int capacity);
amp_value_t amp_vmap_pop(amp_map_t *map, amp_value_t key);

/* tags */

amp_tag_t *amp_tag(amp_value_t descriptor, amp_value_t value);
amp_value_t amp_tag_descriptor(amp_tag_t *t);
amp_value_t amp_tag_value(amp_tag_t *t);

#endif /* value.h */