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
#include <string.h>
#include <stdlib.h>
#include "value-internal.h"

amp_map_t *amp_map(int capacity)
{
  amp_map_t *map = malloc(sizeof(amp_map_t) + 2*capacity*sizeof(amp_value_t));
  map->capacity = capacity;
  map->size = 0;
  return map;
}

void amp_free_map(amp_map_t *m)
{
  free(m);
}

amp_value_t amp_map_key(amp_map_t *map, int index)
{
  return map->pairs[2*index];
}

amp_value_t amp_map_value(amp_map_t *map, int index)
{
  return map->pairs[2*index+1];
}

void amp_visit_map(amp_map_t *m, void (*visitor)(amp_value_t))
{
  for (int i = 0; i < m->size; i++)
  {
    amp_visit(amp_map_key(m, i), visitor);
    amp_visit(amp_map_value(m, i), visitor);
  }
}

size_t amp_format_sizeof_map(amp_map_t *map)
{
  size_t result = 2;
  for (int i = 0; i < map->size; i++)
  {
    amp_value_t key = amp_map_key(map, i);
    amp_value_t value = amp_map_value(map, i);
    result += amp_format_sizeof(key) + 2 + amp_format_sizeof(value) + 2;
  }
  return result;
}

int amp_format_map(char **pos, char *limit, amp_map_t *map)
{
  bool first = true;
  int i, e;
  if ((e = amp_fmt(pos, limit, "{"))) return e;
  for (i = 0; i < map->size; i++)
  {
    amp_value_t key = amp_map_key(map, i);
    amp_value_t value = amp_map_value(map, i);
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
    hash += (amp_hash_value(amp_map_key(map, i)) ^
             amp_hash_value(amp_map_value(map, i)));
  }
  return hash;
}

static bool has_entry(amp_map_t *m, amp_value_t key, amp_value_t value)
{
  int i;
  for (i = 0; i < m->size; i++)
  {
    if (!amp_compare_value(amp_map_key(m, i), key) &&
        !amp_compare_value(amp_map_value(m, i), value))
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
    if (!has_entry(b, amp_map_key(a, i), amp_map_value(a, i)))
      return -1;

  for (i = 0; i < b->size; i++)
    if (!has_entry(a, amp_map_key(b, i), amp_map_value(b, i)))
      return -1;

  return 0;
}

bool amp_map_has(amp_map_t *map, amp_value_t key)
{
  for (int i = 0; i < map->size; i++)
  {
    if (!amp_compare_value(key, amp_map_key(map, i)))
      return true;
  }

  return false;
}

amp_value_t amp_map_get(amp_map_t *map, amp_value_t key)
{
  for (int i = 0; i < map->size; i++)
  {
    if (!amp_compare_value(key, amp_map_key(map, i)))
      return amp_map_value(map, i);
  }

  return EMPTY_VALUE;
}

int amp_map_set(amp_map_t *map, amp_value_t key, amp_value_t value)
{
  for (int i = 0; i < map->size; i++)
  {
    if (!amp_compare_value(key, amp_map_key(map, i)))
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

amp_value_t amp_map_pop(amp_map_t *map, amp_value_t key)
{
  for (int i = 0; i < map->size; i++)
  {
    if (!amp_compare_value(key, amp_map_key(map, i)))
    {
      amp_value_t result = amp_map_value(map, i);
      memmove(&map->pairs[2*i], &map->pairs[2*(i+1)],
              (map->size - i - 1)*2*sizeof(amp_value_t));
      map->size--;
      return result;
    }
  }

  return EMPTY_VALUE;
}

int amp_map_size(amp_map_t *map)
{
  return map->size;
}

int amp_map_capacity(amp_map_t *map)
{
  return map->capacity;
}

size_t amp_encode_sizeof_map(amp_map_t *m)
{
  size_t result = 0;
  for (int i = 0; i < 2*m->size; i++)
  {
    result += amp_encode_sizeof(m->pairs[i]);
  }
  return result;
}

size_t amp_encode_map(amp_map_t *m, char *out)
{
  char *old = out;
  char *start;
  int count = 2*m->size;
  // XXX
  amp_write_start(&out, out + 1024, &start);
  for (int i = 0; i < count; i++)
  {
    out += amp_encode(m->pairs[i], out);
  }
  amp_write_map(&out, out + 1024, start, m->size);
  return out - old;
}
