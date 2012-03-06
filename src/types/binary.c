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
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "value-internal.h"

amp_binary_t *amp_binary(char *bytes, size_t size)
{
  amp_binary_t *bin = malloc(sizeof(amp_binary_t) + size);
  bin->size = size;
  memmove(bin->bytes, bytes, size);
  return bin;
}

void amp_free_binary(amp_binary_t *b)
{
  free(b);
}

size_t amp_binary_size(amp_binary_t *b)
{
  return b->size;
}

char *amp_binary_bytes(amp_binary_t *b)
{
  return b->bytes;
}

uintptr_t amp_hash_binary(amp_binary_t *b)
{
  uintptr_t hash = 0;
  for (int i = 0; i < b->size; i++)
  {
    hash = 31*hash + b->bytes[i];
  }
  return hash;
}

int amp_compare_binary(amp_binary_t *a, amp_binary_t *b)
{
  if (a->size == b->size)
    return memcmp(a->bytes, b->bytes, a->size);
  else
    return b->size - a->size;
}

amp_binary_t *amp_binary_dup(amp_binary_t *b)
{
  return amp_binary(b->bytes, b->size);
}

int amp_format_binary(char **pos, char *limit, amp_binary_t *binary)
{
  if (!binary) return amp_fmt(pos, limit, "(null)");

  for (int i = 0; i < binary->size; i++)
  {
    uint8_t b = binary->bytes[i];
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
