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

#include <amp/value.h>
#include <stdarg.h>
#include <ctype.h>
#include <wchar.h>
#include <string.h>
#include <stdio.h>

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
    default:
      // XXX
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

void amp_scan(amp_value_t *value, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);

  for ( ; *fmt; fmt++)
  {
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
    }

    value++;
  }

  va_end(ap);
}

int amp_format_binary(char **pos, char *limit, amp_binary_t binary)
{
  for (int i = 0; i < binary.size; i++)
  {
    char c = binary.bytes[i];
    if (isprint(c)) {
      if (*pos < limit) {
        **pos = c;
        *pos += 1;
      } else {
        return -1;
      }
    } else {
      if (limit - *pos > 4)
      {
        sprintf(*pos, "\\x%.2x", c);
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
    if (i > 0)
      if ((e = amp_fmt(pos, limit, ", ")))
        return e;

    amp_value_t v = values[i];
    switch (v.type)
    {
    case EMPTY:
      if ((e = amp_fmt(pos, limit, "NULL")))
        return e;
      break;
    case BYTE:
      if ((e = amp_fmt(pos, limit, "%hhi", v.u.as_byte)))
        return e;
      break;
    case UBYTE:
      if ((e = amp_fmt(pos, limit, "%hhu", v.u.as_ubyte)))
        return e;
      break;
    case SHORT:
      if ((e = amp_fmt(pos, limit, "%hi", v.u.as_short)))
        return e;
      break;
    case USHORT:
      if ((e = amp_fmt(pos, limit, "%hu", v.u.as_ushort)))
        return e;
      break;
    case INT:
      if ((e = amp_fmt(pos, limit, "%i", v.u.as_int)))
        return e;
      break;
    case UINT:
      if ((e = amp_fmt(pos, limit, "%u", v.u.as_uint)))
        return e;
      break;
    case LONG:
      if ((e = amp_fmt(pos, limit, "%lli", v.u.as_long)))
        return e;
      break;
    case ULONG:
      if ((e = amp_fmt(pos, limit, "%llu", v.u.as_ulong)))
        return e;
      break;
    case FLOAT:
      if ((e = amp_fmt(pos, limit, "%f", v.u.as_float)))
        return e;
      break;
    case DOUBLE:
      if ((e = amp_fmt(pos, limit, "%f", v.u.as_double)))
        return e;
      break;
    case CHAR:
      if ((e = amp_fmt(pos, limit, "%lc", v.u.as_char)))
        return e;
      break;
    case STRING:
      if ((e = amp_fmt(pos, limit, "%ls", v.u.as_string.wcs)))
        return e;
      break;
    case BINARY:
      if ((e = amp_format_binary(pos, limit, v.u.as_binary)))
        return e;
      break;
    }
  }

  return 0;
}
