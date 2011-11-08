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
#include <amp/util.h>
#include <stdarg.h>
#include <stdbool.h>
#include <ctype.h>
#include <wchar.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <iconv.h>
#include <arpa/inet.h>
#include "codec/encodings.h"

int amp_compare_string(amp_string_t a, amp_string_t b)
{
  if (a.size == b.size)
    return wmemcmp(a.wcs, b.wcs, a.size);
  else
    return b.size - a.size;
}

int amp_compare_binary(amp_binary_t a, amp_binary_t b)
{
  if (a.size == b.size)
    return memcmp(a.bytes, b.bytes, a.size);
  else
    return b.size - a.size;
}

int amp_compare_value(amp_value_t a, amp_value_t b)
{
  if (a.type == b.type) {
    switch (a.type)
    {
    case EMPTY:
      return 0;
    case BOOLEAN:
      return b.u.as_boolean && a.u.as_boolean;
    case UBYTE:
      return b.u.as_ubyte - a.u.as_ubyte;
    case USHORT:
      return b.u.as_ushort - a.u.as_ushort;
    case UINT:
      return b.u.as_uint - a.u.as_uint;
    case ULONG:
      return b.u.as_ulong - a.u.as_ulong;
    case BYTE:
      return b.u.as_byte - a.u.as_byte;
    case SHORT:
      return b.u.as_short - a.u.as_short;
    case INT:
      return b.u.as_int - a.u.as_int;
    case LONG:
      return b.u.as_long - a.u.as_long;
    case FLOAT:
      return b.u.as_float - a.u.as_float;
    case DOUBLE:
      return b.u.as_double - a.u.as_double;
    case CHAR:
      return b.u.as_char - a.u.as_char;
    case STRING:
      return amp_compare_string(a.u.as_string, b.u.as_string);
    case BINARY:
      return amp_compare_binary(a.u.as_binary, b.u.as_binary);
    case REF:
      return (char *)b.u.as_ref - (char *)a.u.as_ref;
    default:
      amp_fatal("uncomparable: %s, %s", amp_aformat(a), amp_aformat(b));
      return -1;
    }
  } else {
    return b.type - a.type;
  }
}

uintptr_t amp_hash_string(amp_string_t s)
{
  wchar_t *c;
  uintptr_t hash = 1;
  for (c = s.wcs; *c; c++)
  {
    hash = 31*hash + *c;
  }
  return hash;
}

uintptr_t amp_hash_binary(amp_binary_t b)
{
  uintptr_t hash = 0;
  for (int i = 0; i < b.size; i++)
  {
    hash = 31*hash + b.bytes[i];
  }
  return hash;
}

uintptr_t amp_hash_value(amp_value_t v)
{
  switch (v.type)
  {
  case EMPTY:
    return 0;
  case UBYTE:
    return v.u.as_ubyte;
  case USHORT:
    return v.u.as_ushort;
  case UINT:
    return v.u.as_uint;
  case ULONG:
    return v.u.as_ulong;
  case BYTE:
    return v.u.as_byte;
  case SHORT:
    return v.u.as_short;
  case INT:
    return v.u.as_int;
  case LONG:
    return v.u.as_long;
  case FLOAT:
    return v.u.as_float;
  case DOUBLE:
    return v.u.as_double;
  case CHAR:
    return v.u.as_char;
  case STRING:
    return amp_hash_string(v.u.as_string);
  case BINARY:
    return amp_hash_binary(v.u.as_binary);
  default:
    return 0;
  }
}

int scan_size(const char *str)
{
  int i, level = 0, size = 0;
  for (i = 0; str[i]; i++)
  {
    switch (str[i])
    {
    case '@':
      i++;
      break;
    case '(':
    case '[':
    case '{':
      level++;
      break;
    case ')':
    case ']':
    case '}':
      if (level == 0)
        return size;
      level--;
      // fall through intentionally here
    default:
      if (level == 0) size++;
      break;
    }
  }

  return -1;
}

int amp_scan(amp_value_t *value, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int n = amp_vscan(value, fmt, ap);
  va_end(ap);
  return n;
}

static enum TYPE code_to_type(char c)
{
  switch (c)
  {
  case 'n': return EMPTY;
  case 'b': return BYTE;
  case 'B': return UBYTE;
  case 'h': return SHORT;
  case 'H': return USHORT;
  case 'i': return INT;
  case 'I': return UINT;
  case 'l': return LONG;
  case 'L': return ULONG;
  case 'f': return FLOAT;
  case 'd': return DOUBLE;
  case 'C': return CHAR;
  case 'S': return STRING;
  case 'z': return BINARY;
  case 't': return LIST;
  case 'm': return MAP;
  default:
    amp_fatal("unrecognized code: %c", c);
    return -1;
  }
}

static char type_to_code(enum TYPE type)
{
  switch (type)
  {
  case EMPTY: return 'n';
  case BYTE: return 'b';
  case UBYTE: return 'B';
  case SHORT: return 'h';
  case USHORT: return 'H';
  case INT: return 'i';
  case UINT: return 'I';
  case LONG: return 'l';
  case ULONG: return 'L';
  case FLOAT: return 'f';
  case DOUBLE: return 'd';
  case CHAR: return 'C';
  case STRING: return 'S';
  case BINARY: return 'z';
  case LIST: return 't';
  case MAP: return 'm';
  default: return -1;
  }
}

static uint8_t type_to_amqp_code(enum TYPE type)
{
  switch (type)
  {
  case EMPTY: return AMPE_NULL;
  case BOOLEAN: return AMPE_BOOLEAN;
  case BYTE: return AMPE_BYTE;
  case UBYTE: return AMPE_UBYTE;
  case SHORT: return AMPE_SHORT;
  case USHORT: return AMPE_USHORT;
  case INT: return AMPE_INT;
  case UINT: return AMPE_UINT;
  case CHAR: return AMPE_UTF32;
  case LONG: return AMPE_LONG;
  case ULONG: return AMPE_ULONG;
  case FLOAT: return AMPE_FLOAT;
  case DOUBLE: return AMPE_DOUBLE;
  case STRING: return AMPE_STR32_UTF8;
  case BINARY: return AMPE_VBIN32;
  case LIST: return AMPE_LIST32;
  case MAP: return AMPE_MAP32;
  case ARRAY: return AMPE_ARRAY32;
  default:
    amp_fatal("no amqp code for type: %i", type);
    return -1;
  }
}

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

typedef struct stack_frame_st {
  amp_value_t *value;
  int count;
} stack_frame_t;

int amp_vscan(amp_value_t *value, const char *fmt, va_list ap)
{
  stack_frame_t stack[strlen(fmt)];
  int level = 0, count = 0;

  for ( ; *fmt; fmt++)
  {
    if (level == 0)
      count++;

    switch (*fmt)
    {
    case 'n':
      value->type = EMPTY;
      break;
    case 'b':
      value->type = BYTE;
      value->u.as_byte = va_arg(ap, int);
      break;
    case 'B':
      value->type = UBYTE;
      value->u.as_ubyte = va_arg(ap, unsigned int);
      break;
    case 'h':
      value->type = SHORT;
      value->u.as_short = va_arg(ap, int);
      break;
    case 'H':
      value->type = USHORT;
      value->u.as_ushort = va_arg(ap, unsigned int);
      break;
    case 'i':
      value->type = INT;
      value->u.as_int = va_arg(ap, int32_t);
      break;
    case 'I':
      value->type = UINT;
      value->u.as_uint = va_arg(ap, uint32_t);
      break;
    case 'l':
      value->type = LONG;
      value->u.as_long = va_arg(ap, int64_t);
      break;
    case 'L':
      value->type = ULONG;
      value->u.as_ulong = va_arg(ap, uint64_t);
      break;
    case 'f':
      value->type = FLOAT;
      value->u.as_float = va_arg(ap, double);
      break;
    case 'd':
      value->type = DOUBLE;
      value->u.as_double = va_arg(ap, double);
      break;
    case 'C':
      value->type = CHAR;
      value->u.as_char = va_arg(ap, wchar_t);
      break;
    case 'S':
      value->type = STRING;
      wchar_t *wcs = va_arg(ap, wchar_t *);
      value->u.as_string.size = wcslen(wcs);
      value->u.as_string.wcs = wcs;
      break;
    case 'z':
      value->type = BINARY;
      value->u.as_binary.size = va_arg(ap, size_t);
      value->u.as_binary.bytes = va_arg(ap, char *);
      break;
    case '[':
      stack[level] = (stack_frame_t) {value, scan_size(fmt+1)};
      value->type = LIST;
      value->u.as_list = amp_vlist(stack[level].count);
      value->u.as_list->size = stack[level].count;
      value = value->u.as_list->values;
      level++;
      continue;
    case ']':
      value = stack[--level].value;
      break;
    case '{':
      stack[level] = (stack_frame_t) {value, scan_size(fmt+1)};
      value->type = MAP;
      value->u.as_map = amp_vmap(stack[level].count/2);
      value->u.as_map->size = stack[level].count/2;
      value = value->u.as_map->pairs;
      level++;
      continue;
    case '}':
      value = stack[--level].value;
      break;
    case '(':
      // XXX: need to figure out how to detect missing descriptor value here
      // XXX: also, decrementing count may not work when nested
      value--;
      count--;
      amp_tag_t *tag = amp_tag(*value, EMPTY_VALUE);
      value->type = TAG;
      value->u.as_tag = tag;
      stack[level++] = (stack_frame_t) {value, 0};
      value = &value->u.as_tag->value;
      continue;
    case ')':
      value = stack[--level].value;
      break;
    case '@':
      value->type = ARRAY;
      enum TYPE etype = code_to_type(*(++fmt));
      stack[level] = (stack_frame_t) {value, scan_size(++fmt+1)};
      value->u.as_array = amp_array(etype, stack[level].count);
      value->u.as_array->size = stack[level].count;
      value = value->u.as_array->values;
      level++;
      continue;
    default:
      fprintf(stderr, "unrecognized type: %c\n", *fmt);
      return -1;
    }

    value++;
  }

  return count;
}

amp_value_t amp_value(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  amp_value_t value = amp_vvalue(fmt, ap);
  va_end(ap);
  return value;
}

amp_value_t amp_vvalue(const char *fmt, va_list ap)
{
  amp_value_t value;
  amp_vscan(&value, fmt, ap);
  return value;
}

amp_list_t *amp_to_list(amp_value_t v)
{
  return v.u.as_list;
}

amp_map_t *amp_to_map(amp_value_t v)
{
  return v.u.as_map;
}

amp_tag_t *amp_to_tag(amp_value_t v)
{
  return v.u.as_tag;
}

void *amp_to_ref(amp_value_t v)
{
  return v.u.as_ref;
}

amp_value_t amp_from_list(amp_list_t *l)
{
  return (amp_value_t) {.type = LIST, .u.as_list = l};
}

amp_value_t amp_from_map(amp_map_t *m)
{
  return (amp_value_t) {.type = MAP, .u.as_map = m};
}

amp_value_t amp_from_tag(amp_tag_t *t)
{
  return (amp_value_t) {.type = TAG, .u.as_tag = t};
}

amp_value_t amp_from_ref(void *r)
{
  return (amp_value_t) {.type = REF, .u.as_ref = r};
}

int amp_format_binary(char **pos, char *limit, amp_binary_t binary)
{
  for (int i = 0; i < binary.size; i++)
  {
    uint8_t b = binary.bytes[i];
    if (isprint(b)) {
      if (*pos < limit) {
        **pos = b;
        *pos += 1;
      } else {
        return -1;
      }
    } else {
      if (limit - *pos > 4)
      {
        sprintf(*pos, "\\x%.2x", b);
        *pos += 4;
      } else {
        return -1;
      }
    }
  }

  return 0;
}

int amp_fmt(char **pos, char *limit, const char *fmt, ...)
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

int amp_format_value(char **pos, char *limit, amp_value_t *values, size_t n)
{
  int e;
  for (int i = 0; i < n; i++)
  {
    if (i > 0 && (e = amp_fmt(pos, limit, ", "))) return e;

    amp_value_t v = values[i];
    switch (v.type)
    {
    case EMPTY:
      if ((e = amp_fmt(pos, limit, "NULL"))) return e;
      break;
    case BOOLEAN:
      if (v.u.as_boolean) {
        if ((e = amp_fmt(pos, limit, "true"))) return e;
      } else {
        if ((e = amp_fmt(pos, limit, "false"))) return e;
      }
      break;
    case BYTE:
      if ((e = amp_fmt(pos, limit, "%hhi", v.u.as_byte))) return e;
      break;
    case UBYTE:
      if ((e = amp_fmt(pos, limit, "%hhu", v.u.as_ubyte))) return e;
      break;
    case SHORT:
      if ((e = amp_fmt(pos, limit, "%hi", v.u.as_short))) return e;
      break;
    case USHORT:
      if ((e = amp_fmt(pos, limit, "%hu", v.u.as_ushort))) return e;
      break;
    case INT:
      if ((e = amp_fmt(pos, limit, "%i", v.u.as_int))) return e;
      break;
    case UINT:
      if ((e = amp_fmt(pos, limit, "%u", v.u.as_uint))) return e;
      break;
    case LONG:
      if ((e = amp_fmt(pos, limit, "%lli", v.u.as_long))) return e;
      break;
    case ULONG:
      if ((e = amp_fmt(pos, limit, "%llu", v.u.as_ulong))) return e;
      break;
    case FLOAT:
      if ((e = amp_fmt(pos, limit, "%f", v.u.as_float))) return e;
      break;
    case DOUBLE:
      if ((e = amp_fmt(pos, limit, "%f", v.u.as_double))) return e;
      break;
    case CHAR:
      if ((e = amp_fmt(pos, limit, "%lc", v.u.as_char))) return e;
      break;
    case STRING:
      if ((e = amp_fmt(pos, limit, "%ls", v.u.as_string.wcs))) return e;
      break;
    case BINARY:
      if ((e = amp_format_binary(pos, limit, v.u.as_binary))) return e;
      break;
    case ARRAY:
      if ((e = amp_format_array(pos, limit, v.u.as_array))) return e;
      break;
    case LIST:
      if ((e = amp_format_list(pos, limit, v.u.as_list))) return e;
      break;
    case MAP:
      if ((e = amp_format_map(pos, limit, v.u.as_map))) return e;
      break;
    case TAG:
      if ((e = amp_format_tag(pos, limit, v.u.as_tag))) return e;
      break;
    case REF:
      if ((e = amp_fmt(pos, limit, "%p", v.u.as_ref))) return e;
      break;
    }
  }

  return 0;
}

char *amp_aformat(amp_value_t v)
{
  int size = 16;
  while (true)
  {
    char *buf = malloc(size);
    char *pos = buf;
    if (!buf) return NULL;
    int n = amp_format_value(&pos, buf + size, &v, 1);
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

size_t amp_vencode_sizeof(amp_value_t v)
{
  switch (v.type)
  {
  case EMPTY:
    return 1;
  case BOOLEAN:
  case BYTE:
  case UBYTE:
    return 2;
  case SHORT:
  case USHORT:
    return 3;
  case INT:
  case UINT:
  case CHAR:
  case FLOAT:
    return 5;
  case LONG:
  case ULONG:
  case DOUBLE:
    return 9;
  case STRING:
    return 5 + 4*v.u.as_string.size;
  case BINARY:
    return 5 + v.u.as_binary.size;
  case ARRAY:
    return amp_vencode_sizeof_array(v.u.as_array);
  case LIST:
    return amp_vencode_sizeof_list(v.u.as_list);
  case MAP:
    return amp_vencode_sizeof_map(v.u.as_map);
  case TAG:
    return amp_vencode_sizeof_tag(v.u.as_tag);
  default:
    amp_fatal("unencodable type: %s", amp_aformat(v));
    return 0;
  }
}

size_t amp_vencode(amp_value_t v, char *out)
{
  char *old = out;
  size_t size = amp_vencode_sizeof(v);
  iconv_t cd;
  char *inbuf;
  size_t insize;
  char *outbuf;
  size_t utfsize;

  switch (v.type)
  {
  case EMPTY:
    amp_write_null(&out, out + size);
    return 1;
  case BOOLEAN:
    amp_write_boolean(&out, out + size, v.u.as_boolean);
    return 2;
  case BYTE:
    amp_write_byte(&out, out + size, v.u.as_byte);
    return 2;
  case UBYTE:
    amp_write_ubyte(&out, out + size, v.u.as_ubyte);
    return 2;
  case SHORT:
    amp_write_short(&out, out + size, v.u.as_short);
    return 3;
  case USHORT:
    amp_write_ushort(&out, out + size, v.u.as_ushort);
    return 3;
  case INT:
    amp_write_int(&out, out + size, v.u.as_int);
    return 5;
  case UINT:
    amp_write_uint(&out, out + size, v.u.as_uint);
    return 5;
  case CHAR:
    amp_write_char(&out, out + size, v.u.as_char);
    return 5;
  case FLOAT:
    amp_write_float(&out, out + size, v.u.as_float);
    return 5;
  case LONG:
    amp_write_long(&out, out + size, v.u.as_long);
    return 9;
  case ULONG:
    amp_write_ulong(&out, out + size, v.u.as_ulong);
    return 9;
  case DOUBLE:
    amp_write_double(&out, out + size, v.u.as_double);
    return 9;
  case STRING:
    cd = iconv_open("UTF-8", "WCHAR_T");
    insize = 4*v.u.as_string.size;
    inbuf = (char *)v.u.as_string.wcs;
    outbuf = out + 5;
    utfsize = iconv(cd, &inbuf, &insize, &outbuf, &size);
    iconv_close(cd);
    amp_write_utf8(&out, out + size, outbuf - out - 5, out + 5);
    return out - old;
  case BINARY:
    amp_write_binary(&out, out + size, v.u.as_binary.size, v.u.as_binary.bytes);
    return 5 + v.u.as_binary.size;
  case ARRAY:
    return amp_vencode_array(v.u.as_array, out);
  case LIST:
    return amp_vencode_list(v.u.as_list, out);
  case MAP:
    return amp_vencode_map(v.u.as_map, out);
  case TAG:
    return amp_vencode_tag(v.u.as_tag, out);
  default:
    amp_fatal("unencodable type: %s", amp_aformat(v));
    return 0;
  }
}

/* arrays */

amp_array_t *amp_array(enum TYPE type, int capacity)
{
  amp_array_t *l = malloc(sizeof(amp_array_t) + capacity*sizeof(amp_value_t));
  if (l) {
    l->type = type;
    l->capacity = capacity;
    l->size = 0;
  }
  return l;
}

int amp_format_array(char **pos, char *limit, amp_array_t *array)
{
  int e;
  if ((e = amp_fmt(pos, limit, "@%c[", type_to_code(array->type)))) return e;
  if ((e = amp_format_value(pos, limit, array->values, array->size))) return e;
  if ((e = amp_fmt(pos, limit, "]"))) return e;
  return 0;
}

size_t amp_vencode_sizeof_array(amp_array_t *array)
{
  size_t result = 9;
  for (int i = 0; i < array->size; i++)
  {
    // XXX: this is wrong, need to compensate for code
    result += amp_vencode_sizeof(array->values[i]);
  }
  return result;
}

size_t amp_vencode_array(amp_array_t *array, char *out)
{
  // code
  out[0] = (uint8_t) AMPE_ARRAY32;
  // size will be backfilled
  // count
  *((uint32_t *) (out + 5)) = htonl(array->size);
  // element code
  out[9] = (uint8_t) type_to_amqp_code(array->type);

  char *vout = out + 10;
  for (int i = 0; i < array->size; i++)
  {
    char *codeptr = vout - 1;
    char codeval = *codeptr;
    vout += amp_vencode(array->values[i], vout-1) - 1;
    *codeptr = codeval;
  }

  // backfill size
  *((uint32_t *) (out + 1)) = htonl(vout - out - 5);
  return vout - out;
}

/* lists */

amp_list_t *amp_vlist(int capacity)
{
  amp_list_t *l = malloc(sizeof(amp_list_t) + capacity*sizeof(amp_value_t));
  if (l) {
    l->capacity = capacity;
    l->size = 0;
  }
  return l;
}

int amp_vlist_size(amp_list_t *l)
{
  return l->size;
}

amp_value_t amp_vlist_get(amp_list_t *l, int index)
{
  return l->values[index];
}

amp_value_t amp_vlist_set(amp_list_t *l, int index, amp_value_t v)
{
  amp_value_t r = l->values[index];
  l->values[index] = v;
  return r;
}

amp_value_t amp_vlist_pop(amp_list_t *l, int index)
{
  int i, n = l->size;
  amp_value_t v = l->values[index];
  for (i = index; i < n - 1; i++)
    l->values[i] = l->values[i+1];
  l->size--;
  return v;
}

int amp_vlist_add(amp_list_t *l, amp_value_t v)
{
  if (l->capacity <= l->size) {
    fprintf(stderr, "wah!\n");
    return -1;
  }

  l->values[l->size++] = v;
  return 0;
}

int amp_vlist_extend(amp_list_t *l, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int n = amp_vscan(l->values + l->size, fmt, ap);
  va_end(ap);
  if (n > 0) l->size += n;
  return n;
}

int amp_vlist_index(amp_list_t *l, amp_value_t v)
{
  int i, n = l->size;
  for (i = 0; i < n; i++)
    if (amp_compare_value(v, l->values[i]) == 0)
      return i;
  return -1;
}

bool amp_vlist_remove(amp_list_t *l, amp_value_t v)
{
  int i = amp_vlist_index(l, v);
  if (i >= 0) {
    amp_vlist_pop(l, i);
    return true;
  } else {
    return false;
  }
}

int amp_vlist_fill(amp_list_t *l, amp_value_t v, int n)
{
  int i, e;

  for (i = 0; i < n; i++)
    if ((e = amp_vlist_add(l, v))) return e;

  return 0;
}

void amp_vlist_clear(amp_list_t *l)
{
  l->size = 0;
}

static int min(int a, int b)
{
  if (a < b)
    return a;
  else
    return b;
}

int amp_format_list(char **pos, char *limit, amp_list_t *list)
{
  int e;
  if ((e = amp_fmt(pos, limit, "["))) return e;
  if ((e = amp_format_value(pos, limit, list->values, list->size))) return e;
  if ((e = amp_fmt(pos, limit, "]"))) return e;
  return 0;
}

uintptr_t amp_hash_list(amp_list_t *list)
{
  int i, n = list->size;
  uintptr_t hash = 1;

  for (i = 0; i < n; i++)
  {
    hash = 31*hash + amp_hash_value(amp_vlist_get(list, i));
  }

  return hash;
}

int amp_compare_list(amp_list_t *a, amp_list_t *b)
{
  int i, n = min(a->size, b->size);
  int c;

  for (i = 0; i < n; i++)
  {
    c = amp_compare_value(amp_vlist_get(a, i), amp_vlist_get(b, i));
    if (!c)
      return c;
  }

  return 0;
}

size_t amp_vencode_sizeof_list(amp_list_t *l)
{
  size_t result = 9;
  for (int i = 0; i < l->size; i++)
  {
    result += amp_vencode_sizeof(l->values[i]);
  }
  return result;
}

size_t amp_vencode_list(amp_list_t *l, char *out)
{
  char *old = out;
  char *start;
  // XXX
  amp_write_start(&out, out + 1024, &start);
  for (int i = 0; i < l->size; i++)
  {
    out += amp_vencode(l->values[i], out);
  }
  amp_write_list(&out, out + 1024, start, l->size);
  return out - old;
}

/* maps */

amp_map_t *amp_vmap(int capacity)
{
  amp_map_t *map = malloc(sizeof(amp_map_t) + 2*capacity*sizeof(amp_value_t));
  map->capacity = capacity;
  map->size = 0;
  return map;
}

amp_value_t amp_vmap_key(amp_map_t *map, int index)
{
  return map->pairs[2*index];
}

amp_value_t amp_vmap_value(amp_map_t *map, int index)
{
  return map->pairs[2*index+1];
}

int amp_format_map(char **pos, char *limit, amp_map_t *map)
{
  bool first = true;
  int i, e;
  if ((e = amp_fmt(pos, limit, "{"))) return e;
  for (i = 0; i < map->size; i++)
  {
    amp_value_t key = amp_vmap_key(map, i);
    amp_value_t value = amp_vmap_value(map, i);
    if (first) first = false;
    else if ((e = amp_fmt(pos, limit, ", "))) return e;
    if ((e = amp_format_value(pos, limit, &key, 1))) return e;
    if ((e = amp_fmt(pos, limit, ": "))) return e;
    if ((e = amp_format_value(pos, limit, &value, 1))) return e;
  }
  if ((e = amp_fmt(pos, limit, "}"))) return e;
  return 0;
}

uintptr_t amp_hash_map(amp_map_t *map)
{
  uintptr_t hash = 0;
  int i;
  for (i = 0; i < map->size; i++)
  {
    hash += (amp_hash_value(amp_vmap_key(map, i)) ^
             amp_hash_value(amp_vmap_value(map, i)));
  }
  return hash;
}

static bool has_entry(amp_map_t *m, amp_value_t key, amp_value_t value)
{
  int i;
  for (i = 0; i < m->size; i++)
  {
    if (!amp_compare_value(amp_vmap_key(m, i), key) &&
        !amp_compare_value(amp_vmap_value(m, i), value))
      return true;
  }

  return false;
}

int amp_compare_map(amp_map_t *a, amp_map_t *b)
{
  int i;

  if (a->size != b->size)
    return b->size - a->size;

  for (i = 0; i < a->size; i++)
    if (!has_entry(b, amp_vmap_key(a, i), amp_vmap_value(a, i)))
      return -1;

  for (i = 0; i < b->size; i++)
    if (!has_entry(a, amp_vmap_key(b, i), amp_vmap_value(b, i)))
      return -1;

  return 0;
}

bool amp_vmap_has(amp_map_t *map, amp_value_t key)
{
  for (int i = 0; i < map->size; i++)
  {
    if (!amp_compare_value(key, amp_vmap_key(map, i)))
      return true;
  }

  return false;
}

amp_value_t amp_vmap_get(amp_map_t *map, amp_value_t key)
{
  for (int i = 0; i < map->size; i++)
  {
    if (!amp_compare_value(key, amp_vmap_key(map, i)))
      return amp_vmap_value(map, i);
  }

  return EMPTY_VALUE;
}

int amp_vmap_set(amp_map_t *map, amp_value_t key, amp_value_t value)
{
  for (int i = 0; i < map->size; i++)
  {
    if (!amp_compare_value(key, amp_vmap_key(map, i)))
    {
      map->pairs[2*i + 1] = value;
      return 0;
    }
  }

  if (map->size < map->capacity)
  {
    map->pairs[2*map->size] = key;
    map->pairs[2*map->size+1] = value;
    map->size++;
    return 0;
  }
  else
  {
    return -1;
  }
}

amp_value_t amp_vmap_pop(amp_map_t *map, amp_value_t key)
{
  for (int i = 0; i < map->size; i++)
  {
    if (!amp_compare_value(key, amp_vmap_key(map, i)))
    {
      amp_value_t result = amp_vmap_value(map, i);
      memmove(&map->pairs[2*i], &map->pairs[2*(i+1)],
              (map->size - i - 1)*2*sizeof(amp_value_t));
      map->size--;
      return result;
    }
  }

  return EMPTY_VALUE;
}

int amp_vmap_size(amp_map_t *map)
{
  return map->size;
}

int amp_vmap_capacity(amp_map_t *map)
{
  return map->capacity;
}

size_t amp_vencode_sizeof_map(amp_map_t *m)
{
  size_t result = 0;
  for (int i = 0; i < 2*m->size; i++)
  {
    result += amp_vencode_sizeof(m->pairs[i]);
  }
  return result;
}

size_t amp_vencode_map(amp_map_t *m, char *out)
{
  char *old = out;
  char *start;
  int count = 2*m->size;
  // XXX
  amp_write_start(&out, out + 1024, &start);
  for (int i = 0; i < count; i++)
  {
    out += amp_vencode(m->pairs[i], out);
  }
  amp_write_map(&out, out + 1024, start, m->size);
  return out - old;
}

/* tags */

amp_tag_t *amp_tag(amp_value_t descriptor, amp_value_t value)
{
  amp_tag_t *t = malloc(sizeof(amp_tag_t));
  t->descriptor = descriptor;
  t->value = value;
  return t;
}

amp_value_t amp_tag_descriptor(amp_tag_t *t)
{
  return t->descriptor;
}

amp_value_t amp_tag_value(amp_tag_t *t)
{
  return t->value;
}

int amp_format_tag(char **pos, char *limit, amp_tag_t *tag)
{
  int e;

  if ((e = amp_format_value(pos, limit, &tag->descriptor, 1))) return e;
  if ((e = amp_fmt(pos, limit, "("))) return e;
  if ((e = amp_format_value(pos, limit, &tag->value, 1))) return e;
  if ((e = amp_fmt(pos, limit, ")"))) return e;

  return 0;
}

size_t amp_vencode_sizeof_tag(amp_tag_t *t)
{
  return 1 + amp_vencode_sizeof(t->descriptor) + amp_vencode_sizeof(t->value);
}

size_t amp_vencode_tag(amp_tag_t *t, char *out)
{
  size_t size = 1;
  amp_write_descriptor(&out, out + 1);
  size += amp_vencode(t->descriptor, out);
  size += amp_vencode(t->value, out + size - 1);
  return size;
}

/* decode */

struct amp_vdecode_context_frame_st {
  size_t count;
  size_t limit;
  amp_value_t *values;
};

struct amp_vdecode_context_st {
  size_t depth;
  struct amp_vdecode_context_frame_st frames[1024];
};

#define CTX_CAST(ctx) ((struct amp_vdecode_context_st *) (ctx))

static amp_value_t *push_frame(void *ptr, amp_value_t *values, size_t limit)
{
  struct amp_vdecode_context_st *ctx = CTX_CAST(ptr);
  struct amp_vdecode_context_frame_st *old = &ctx->frames[ctx->depth - 1];
  struct amp_vdecode_context_frame_st *frm = &ctx->frames[ctx->depth++];
  frm->count = 0;
  frm->limit = limit;
  frm->values = values;
  return &old->values[old->count];
}

static void pop_frame(void *ptr)
{
  struct amp_vdecode_context_st *ctx = CTX_CAST(ptr);
  ctx->depth--;
}

static amp_value_t *next_value(void *ptr)
{
  struct amp_vdecode_context_st *ctx = CTX_CAST(ptr);
  struct amp_vdecode_context_frame_st *frm = &ctx->frames[ctx->depth-1];
  amp_value_t *result = &frm->values[frm->count];
  frm->count++;
  if (frm->count == frm->limit) {
    pop_frame(ptr);
  }
  return result;
}

void amp_vdecode_null(void *ctx) {
  amp_value_t *value = next_value(ctx);
  value->type = EMPTY;
}
void amp_vdecode_bool(void *ctx, bool v) {
  //amp_value_t *value = VALUE(ctx);
  //  value->type = BOOLEAN;
  //  value->u.as_bool = v;
}
void amp_vdecode_ubyte(void *ctx, uint8_t v) {
  amp_value_t *value = next_value(ctx);
  value->type = UBYTE;
  value->u.as_ubyte = v;
}
void amp_vdecode_byte(void *ctx, int8_t v) {
  amp_value_t *value = next_value(ctx);
  value->type = BYTE;
  value->u.as_byte = v;
}
void amp_vdecode_ushort(void *ctx, uint16_t v) {
  amp_value_t *value = next_value(ctx);
  value->type = USHORT;
  value->u.as_ushort = v;
}
void amp_vdecode_short(void *ctx, int16_t v) {
  amp_value_t *value = next_value(ctx);
  value->type = SHORT;
  value->u.as_short = v;
}
void amp_vdecode_uint(void *ctx, uint32_t v) {
  amp_value_t *value = next_value(ctx);
  value->type = UINT;
  value->u.as_uint = v;
}
void amp_vdecode_int(void *ctx, int32_t v) {
  amp_value_t *value = next_value(ctx);
  value->type = INT;
  value->u.as_int = v;
}
void amp_vdecode_float(void *ctx, float f) {
  amp_value_t *value = next_value(ctx);
  value->type = FLOAT;
  value->u.as_float = f;
}
void amp_vdecode_ulong(void *ctx, uint64_t v) {
  amp_value_t *value = next_value(ctx);
  value->type = ULONG;
  value->u.as_ulong = v;
}
void amp_vdecode_long(void *ctx, int64_t v) {
  amp_value_t *value = next_value(ctx);
  value->type = LONG;
  value->u.as_long = v;
}
void amp_vdecode_double(void *ctx, double v) {
  amp_value_t *value = next_value(ctx);
  value->type = DOUBLE;
  value->u.as_double = v;
}
void amp_vdecode_binary(void *ctx, size_t size, char *bytes) {
  amp_value_t *value = next_value(ctx);
  value->type = BINARY;
  value->u.as_binary = (amp_binary_t) {.size = size, .bytes = bytes};
}
void amp_vdecode_utf8(void *ctx, size_t size, char *bytes) {
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
    perror("amp_vdecode_utf8");
  }
  *out = L'\0';
  iconv_close(cd);
  value->u.as_string = (amp_string_t) {.size = wcslen(buf), .wcs = buf};
}
void amp_vdecode_utf16(void *ctx, size_t size, char *bytes) {
  // XXX: this encoding no longer exists
}
void amp_vdecode_symbol(void *ctx, size_t size, char *bytes) {
  //  amp_value_t *value = next_value(ctx);
  //  value->type = SYMBOL;
  //  value->u.as_symbol = {.size = size, .bytes = bytes};
}

void amp_vdecode_start_array(void *ctx, size_t count, uint8_t code) {
  amp_array_t *array = amp_array(amqp_code_to_type(code), count);
  array->size = count;
  amp_value_t *value = push_frame(ctx, array->values, count);
  value->type = ARRAY;
  value->u.as_array = array;
}
void amp_vdecode_stop_array(void *ctx, size_t count, uint8_t code) {}

void amp_vdecode_list(void *ctx, size_t count) {}

void amp_vdecode_start_list(void *ctx, size_t count) {
  amp_list_t *list = amp_vlist(count);
  list->size = count;
  amp_value_t *value = push_frame(ctx, list->values, count);
  value->type = LIST;
  value->u.as_list = list;
}

void amp_vdecode_stop_list(void *ctx, size_t count) {}

void amp_vdecode_map(void *ctx, size_t count) {}

void amp_vdecode_start_map(void *ctx, size_t count) {
  amp_map_t *map = amp_vmap(count/2);
  map->size = count/2;
  amp_value_t *value = push_frame(ctx, map->pairs, count);
  value->type = MAP;
  value->u.as_map = map;
}

void amp_vdecode_stop_map(void *ctx, size_t count) {}

void amp_vdecode_descriptor(void *ctx) {}

void amp_vdecode_start_descriptor(void *ctx) {
  amp_tag_t *tag = amp_tag(EMPTY_VALUE, EMPTY_VALUE);
  amp_value_t *value = push_frame(ctx, &tag->value, 1);
  value->type = TAG;
  value->u.as_tag = tag;
  push_frame(ctx, &tag->descriptor, 1);
}

void amp_vdecode_stop_descriptor(void *ctx) {}

amp_data_callbacks_t *amp_vdecoder = &AMP_DATA_CALLBACKS(amp_vdecode);

/*
static bool amp_vdecode_stop(void *ptr)
{
  struct amp_vdecode_context_st *ctx = CTX_CAST(ptr);
  return ctx->depth == 0;
}
*/

ssize_t amp_vdecode(amp_value_t *v, char *bytes, size_t n)
{
  struct amp_vdecode_context_st ctx = {.depth = 0};
  push_frame(&ctx, v, 1);
  ssize_t read = amp_read_datum(bytes, n, amp_vdecoder, &ctx);
  return read;
}
