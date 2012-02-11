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
#include "../codec/encodings.h"

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
      value->u.as_list = amp_list(stack[level].count);
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
      value->u.as_map = amp_map(stack[level].count/2);
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

amp_value_t amp_from_binary(amp_binary_t b)
{
  return (amp_value_t) {.type = BINARY, .u.as_binary = b};
}

amp_binary_t amp_binary_dup(amp_binary_t binary)
{
  amp_binary_t dup;
  dup.bytes = malloc(binary.size);
  memcpy(dup.bytes, binary.bytes, binary.size);
  dup.size = binary.size;
  return dup;
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

size_t amp_encode_sizeof(amp_value_t v)
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
    return amp_encode_sizeof_array(v.u.as_array);
  case LIST:
    return amp_encode_sizeof_list(v.u.as_list);
  case MAP:
    return amp_encode_sizeof_map(v.u.as_map);
  case TAG:
    return amp_encode_sizeof_tag(v.u.as_tag);
  default:
    amp_fatal("unencodable type: %s", amp_aformat(v));
    return 0;
  }
}

size_t amp_encode(amp_value_t v, char *out)
{
  char *old = out;
  size_t size = amp_encode_sizeof(v);
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
    return amp_encode_array(v.u.as_array, out);
  case LIST:
    return amp_encode_list(v.u.as_list, out);
  case MAP:
    return amp_encode_map(v.u.as_map, out);
  case TAG:
    return amp_encode_tag(v.u.as_tag, out);
  default:
    amp_fatal("unencodable type: %s", amp_aformat(v));
    return 0;
  }
}

void amp_free_value(amp_value_t v)
{
  switch (v.type)
  {
  case EMPTY:
  case BOOLEAN:
  case BYTE:
  case UBYTE:
  case SHORT:
  case USHORT:
  case INT:
  case UINT:
  case CHAR:
  case FLOAT:
  case LONG:
  case ULONG:
  case DOUBLE:
  case STRING:
  case BINARY:
  case REF:
    break;
  case ARRAY:
    amp_free_array(v.u.as_array);
    break;
  case LIST:
    amp_free_list(v.u.as_list);
    break;
  case MAP:
    amp_free_map(v.u.as_map);
    break;
  case TAG:
    amp_free_tag(v.u.as_tag);
    break;
  }
}

void amp_visit(amp_value_t v, void (*visitor)(amp_value_t))
{
  switch (v.type)
  {
  case EMPTY:
  case BOOLEAN:
  case BYTE:
  case UBYTE:
  case SHORT:
  case USHORT:
  case INT:
  case UINT:
  case CHAR:
  case FLOAT:
  case LONG:
  case ULONG:
  case DOUBLE:
  case STRING:
  case BINARY:
  case REF:
    break;
  case ARRAY:
    amp_visit_array(v.u.as_array, visitor);
    break;
  case LIST:
    amp_visit_list(v.u.as_list, visitor);
    break;
  case MAP:
    amp_visit_map(v.u.as_map, visitor);
    break;
  case TAG:
    amp_visit_tag(v.u.as_tag, visitor);
    break;
  }

  visitor(v);
}

/* tags */

amp_tag_t *amp_tag(amp_value_t descriptor, amp_value_t value)
{
  amp_tag_t *t = malloc(sizeof(amp_tag_t));
  t->descriptor = descriptor;
  t->value = value;
  return t;
}

void amp_free_tag(amp_tag_t *t)
{
  free(t);
}

void amp_visit_tag(amp_tag_t *t, void (*visitor)(amp_value_t))
{
  amp_visit(t->descriptor, visitor);
  amp_visit(t->value, visitor);
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

size_t amp_encode_sizeof_tag(amp_tag_t *t)
{
  return 1 + amp_encode_sizeof(t->descriptor) + amp_encode_sizeof(t->value);
}

size_t amp_encode_tag(amp_tag_t *t, char *out)
{
  size_t size = 1;
  amp_write_descriptor(&out, out + 1);
  size += amp_encode(t->descriptor, out);
  size += amp_encode(t->value, out + size - 1);
  return size;
}
