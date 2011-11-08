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
#include <stdio.h>
#include <stdlib.h>

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
