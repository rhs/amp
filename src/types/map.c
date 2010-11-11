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

#include <amp/map.h>
#include <amp/codec.h>

amp_type_t *MAP = &AMP_ENCODABLE(map);

amp_map_t *amp_map(amp_region_t *mem, int buckets)
{
  amp_map_t *o = amp_allocate(mem, NULL, sizeof(amp_map_t));
  o->type = MAP;
  o->size = 0;
  o->buckets = amp_list(mem, buckets);
  amp_list_fill(o->buckets, NULL, buckets);
  o->head = NULL;
  o->tail = NULL;
  o->region = mem;
  return o;
}

int amp_map_inspect(amp_object_t *o, char **pos, char *limit)
{
  amp_map_t *m = o;
  bool first = true;
  int n;
  if ((n = amp_format(pos, limit, "{"))) return n;
  for (amp_entry_t *e = amp_map_head(m); e; e = amp_entry_next(e))
  {
    amp_object_t *key = amp_entry_key(e);
    amp_object_t *value = amp_entry_value(e);
    if (first) first = false;
    else if ((n = amp_format(pos, limit, ", "))) return n;
    if ((n = amp_inspect(key, pos, limit))) return n;
    if ((n = amp_format(pos, limit, ": "))) return n;
    if ((n = amp_inspect(value, pos, limit))) return n;
  }
  if ((n = amp_format(pos, limit, "}"))) return n;
  return 0;
}

int amp_map_hash(amp_object_t *o)
{
  amp_map_t *m = o;
  int hash = 0;
  for (amp_entry_t *e = amp_map_head(m); e; e = amp_entry_next(e))
  {
    hash += amp_hash(amp_entry_key(e)) ^ amp_hash(amp_entry_value(e));
  }
  return hash;
}

static amp_list_t *get_bucket(amp_map_t *m, amp_object_t *k, bool create)
{
  int hash = amp_hash(k);
  int bidx = hash % amp_list_size(m->buckets);

  amp_list_t *bucket = amp_list_get(m->buckets, bidx);
  if (!bucket && create) {
    bucket = amp_list(m->region, 4);
    amp_list_set(m->buckets, bidx, bucket);
  }

  return bucket;
}

static int get_index(amp_list_t *bucket, amp_object_t *k)
{
  int n = amp_list_size(bucket);
  for (int i = 0; i < n; i++) {
    amp_entry_t *e = amp_list_get(bucket, i);
    if (amp_equal(k, e->key))
      return i;
  }

  return -1;
}

static amp_entry_t *get_entry(amp_map_t *m, amp_object_t *k, bool create)
{
  amp_list_t *bucket = get_bucket(m, k, create);

  if (bucket) {
    int index = get_index(bucket, k);
    if (index >= 0) {
      return amp_list_get(bucket, index);
    } else if (create) {
      amp_entry_t *e = amp_allocate(m->region, NULL, sizeof(amp_entry_t));
      e->prev = m->tail;
      e->next = NULL;
      if (m->tail) m->tail->next = e;
      m->tail = e;
      if (!m->head) m->head = e;
      amp_list_add(bucket, e);
      m->size++;
      return e;
    }
  }

  return NULL;
}

static amp_object_t *pop_entry(amp_map_t *m, amp_object_t *k)
{
  amp_list_t *bucket = get_bucket(m, k, false);
  int index = get_index(bucket, k);
  if (index >= 0) {
    amp_entry_t *e = amp_list_pop(bucket, index);
    amp_entry_t *n = e->next;
    amp_entry_t *p = e->prev;
    if (p) p->next = n;
    if (n) n->prev = p;
    if (e == m->head) m->head = p;
    if (e == m->tail) m->tail = n;
    amp_object_t *old = e->value;
    amp_allocate(m->region, e, 0);
    m->size--;
    return old;
  } else {
    return NULL;
  }
}

static bool has_entry(amp_map_t *m, amp_entry_t *e)
{
  amp_entry_t *f = get_entry(m, amp_entry_key(e), false);
  return f && amp_equal(amp_entry_value(e), amp_entry_value(f));
}

int amp_map_compare(amp_object_t *oa, amp_object_t *ob)
{
  amp_map_t *a = oa, *b = ob;

  for (amp_entry_t *e = amp_map_head(a); e; e = amp_entry_next(e))
    if (!has_entry(b, e))
      return -1;

  for (amp_entry_t *e = amp_map_head(b); e; e = amp_entry_next(e))
    if (!has_entry(a, e))
      return -1;

  return 0;
}

static size_t entry_encode_space(amp_entry_t *e)
{
  return amp_encode_space(e->key) + amp_encode_space(e->value);
}

size_t amp_map_encode_space(amp_object_t *o)
{
  amp_map_t *m = o;
  size_t space = 9;
  for (amp_entry_t *e = amp_map_head(m); e; e = amp_entry_next(e))
    space += entry_encode_space(e);
  return space;
}

int amp_map_encode(amp_object_t *o, amp_encoder_t *enc, char **pos, char *limit)
{
  amp_map_t *m = o;
  char *start;
  int n;
  if ((n = amp_write_start(pos, limit, &start)))
    return n;
  for (amp_entry_t *e = amp_map_head(m); e; e = amp_entry_next(e)) {
    if ((n = amp_encode(e->key, enc, pos, limit)))
      return n;
    if ((n = amp_encode(e->value, enc, pos, limit)))
      return n;
  }
  if ((n = amp_write_map(pos, limit, start, m->size)))
    return n;
  return 0;
}

bool amp_map_has(amp_map_t *m, amp_object_t *k)
{
  if (get_entry(m, k, false))
    return true;
  else
    return false;
}

amp_object_t *amp_map_get(amp_map_t *m, amp_object_t *k)
{
  amp_entry_t *entry = get_entry(m, k, false);
  if (entry) return entry->value;
  else return NULL;
}

void amp_map_set(amp_map_t *m, amp_object_t *k, amp_object_t *v)
{
  amp_entry_t *entry = get_entry(m, k, true);
  entry->key = k;
  entry->value = v;
}

amp_object_t *amp_map_pop(amp_map_t *m, amp_object_t *k)
{
  return pop_entry(m, k);
}

int amp_map_size(amp_map_t *m)
{
  return m->size;
}

amp_entry_t *amp_map_head(amp_map_t *m)
{
  return m->head;
}

amp_entry_t *amp_map_tail(amp_map_t *m)
{
  return m->tail;
}

amp_entry_t *amp_entry_next(amp_entry_t *e)
{
  return e->next;
}

amp_entry_t *amp_entry_prev(amp_entry_t *e)
{
  return e->prev;
}

amp_object_t *amp_entry_key(amp_entry_t *e)
{
  return e->key;
}

amp_object_t *amp_entry_value(amp_entry_t *e)
{
  return e->value;
}
