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

#include <ctype.h>
#include <string.h>

#include <amp/binary.h>
#include <amp/codec.h>

amp_type_t *BINARY = &AMP_ENCODABLE(binary);

amp_binary_t *amp_binary(amp_region_t *mem, size_t capacity)
{
  amp_binary_t *o = amp_allocate(mem, NULL, sizeof(amp_binary_t));
  o->type = BINARY;
  o->bytes = amp_allocate(mem, NULL, capacity);
  o->capacity = capacity;
  o->size = 0;
  return o;
}

int amp_binary_inspect(amp_object_t *o, char **pos, char *limit)
{
  amp_binary_t *b = o;
  int n;
  if ((n = amp_format(pos, limit, "\"")))
    return n;
  for (int i = 0; i < b->size; i++) {
    char c = b->bytes[i];
    if (c == '"') {
      if ((limit - *pos) > 1) {
        *((*pos)++) = '\\';
        *((*pos)++) = '\"';
      } else {
        return -1;
      }
    } else if (isprint(c)) {
      if ((limit - *pos) > 0)
        *((*pos)++) = c;
      else
        return -1;
    } else {
      if ((n = amp_format(pos, limit, "\\x%.2hhx", c)))
        return n;
    }
  }
  if ((n = amp_format(pos, limit, "\"")))
    return n;
  else
    return 0;
}

intptr_t amp_binary_hash(amp_object_t *o)
{
  amp_binary_t *b = o;
  int hash = 0;
  for (int i = 0; i < b->size; i++)
  {
    hash = 31*hash + b->bytes[i];
  }
  return hash;
}

int amp_binary_compare(amp_object_t *oa, amp_object_t *ob)
{
  amp_binary_t *a = oa, *b = ob;
  int result = memcmp(a->bytes, b->bytes, b->size > a->size ? a->size : b->size);
  if (result == 0 && b->size != a->size)
    return b->size - a->size;
  else
    return result;
}

size_t amp_binary_encode_space(amp_object_t *o)
{
  amp_binary_t *b = o;
  return 5 + b->size;
}

int amp_binary_encode(amp_object_t *o, amp_encoder_t *e, char **pos, char *limit)
{
  amp_binary_t *b = o;
  return amp_write_binary(pos, limit, b->size, b->bytes);
}

char *amp_binary_bytes(amp_binary_t *b)
{
  return b->bytes;
}

size_t amp_binary_size(amp_binary_t *b)
{
  return b->size;
}

size_t amp_binary_capacity(amp_binary_t *b)
{
  return b->capacity;
}

void amp_binary_add(amp_binary_t *b, char byte)
{
  b->bytes[b->size++] = byte;
}

void amp_binary_extend(amp_binary_t *b, char *bytes, size_t n)
{
  for (int i = 0; i < n; i++)
  {
    b->bytes[b->size++] = bytes[i];
  }
}
