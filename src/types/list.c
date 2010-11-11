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

#include <amp/list.h>
#include <amp/codec.h>

amp_type_t *LIST = &AMP_ENCODABLE(list);

amp_list_t *amp_list(amp_region_t *mem, int capacity)
{
  amp_list_t *o = amp_allocate(mem, NULL, sizeof(amp_list_t));
  if (!o) return NULL;
  o->type = LIST;
  o->elements = capacity ? amp_allocate(mem, NULL, capacity*sizeof(amp_object_t *)) : NULL;
  o->capacity = capacity;
  o->size = 0;
  o->region = mem;
  return o;
}

static int min(int a, int b)
{
  if (a < b)
    return a;
  else
    return b;
}

int amp_list_inspect(amp_object_t *o, char **pos, char *limit)
{
  amp_list_t *l = o;
  int i, s = l->size;
  int n;
  if ((n = amp_format(pos, limit, "["))) return n;
  for (i = 0; i < s; i++)
  {
    if (i) if ((n = amp_format(pos, limit, ", "))) return n;
    if ((n = amp_inspect(amp_list_get(l, i), pos, limit))) return n;
  }
  if ((n = amp_format(pos, limit, "]"))) return n;
  return 0;
}

intptr_t amp_list_hash(amp_object_t *o)
{
  amp_list_t *l = o;
  int i, hash = 1, n = l->size;

  for (i = 0; i < n; i++)
  {
    hash = 31*hash + amp_hash(amp_list_get(l, i));
  }

  return hash;
}

int amp_list_compare(amp_object_t *oa, amp_object_t *ob)
{
  amp_list_t *a = oa, *b = ob;
  int i, n = min(a->size, b->size);
  int c;

  for (i = 0; i < n; i++)
  {
    c = amp_compare(amp_list_get(a, i), amp_list_get(b, i));
    if (!c)
      return c;
  }

  return 0;
}

size_t amp_list_encode_space(amp_object_t *o)
{
  amp_list_t *l = o;
  size_t space = 9;
  for (int i = 0; i < l->size; i++)
    space += amp_encode_space(l->elements[i]);
  return space;
}

int amp_list_encode(amp_object_t *o, amp_encoder_t *e, char **pos, char *limit)
{
  amp_list_t *l = o;
  char *start;
  int n;
  if ((n = amp_write_start(pos, limit, &start))) return n;
  for (int i = 0; i < l->size; i++) {
    if ((n = amp_encode(l->elements[i], e, pos, limit)))
      return n;
  }
  if ((n = amp_write_list(pos, limit, start, l->size)))
    return n;
  return 0;
}

int amp_list_size(amp_list_t *l)
{
  return l->size;
}

amp_object_t *amp_list_get(amp_list_t *l, int index)
{
  return l->elements[index];
}

amp_object_t *amp_list_set(amp_list_t *l, int index, amp_object_t *o)
{
  amp_object_t *r = l->elements[index];
  l->elements[index] = o;
  return r;
}

amp_object_t *amp_list_pop(amp_list_t *l, int index)
{
  int i, n = l->size;
  amp_object_t *o = l->elements[index];
  for (i = index; i < n - 1; i++)
    l->elements[i] = l->elements[i+1];
  l->size--;
  return o;
}

void amp_list_add(amp_list_t *l, amp_object_t *e)
{
  if (l->capacity <= l->size) {
    l->capacity = l->capacity ? 2 * l->capacity : 16;
    // XXX: need to deal with errors
    l->elements = amp_allocate(l->region, l->elements,
                               l->capacity * sizeof(amp_object_t *));
  }

  l->elements[l->size++] = e;
}

int amp_list_index(amp_list_t *l, amp_object_t *o)
{
  int i, n = l->size;
  for (i = 0; i < n; i++)
    if (amp_equal(o, l->elements[i]))
      return i;
  return -1;
}

bool amp_list_remove(amp_list_t *l, amp_object_t *o)
{
  int i = amp_list_index(l, o);
  if (i >= 0) {
    amp_list_pop(l, i);
    return true;
  } else {
    return false;
  }
}

void amp_list_fill(amp_list_t *l, amp_object_t *o, int n)
{
  int i;
  for (i = 0; i < n; i++)
    amp_list_add(l, o);
}
