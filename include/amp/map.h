#ifndef _AMP_MAP_H
#define _AMP_MAP_H 1

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

#include <amp/type.h>
#include <amp/list.h>

typedef struct amp_entry_st amp_entry_t;

struct amp_entry_st {
  amp_object_t *key;
  amp_object_t *value;
  amp_entry_t *next;
  amp_entry_t *prev;
};

typedef struct {
  AMP_HEAD;
  amp_list_t *buckets;
  int size;
  amp_entry_t *head;
  amp_entry_t *tail;
  amp_region_t *region;
} amp_map_t;

AMP_ENCODABLE_DECL(MAP, map)

amp_map_t *amp_map(amp_region_t *mem, int buckets);
bool amp_map_has(amp_map_t *m, amp_object_t *k);
amp_object_t *amp_map_get(amp_map_t *m, amp_object_t *k);
void amp_map_set(amp_map_t *m, amp_object_t *k, amp_object_t *v);
amp_object_t *amp_map_pop(amp_map_t *m, amp_object_t *k);
int amp_map_size(amp_map_t *m);
amp_entry_t *amp_map_head(amp_map_t *m);
amp_entry_t *amp_map_tail(amp_map_t *m);
amp_entry_t *amp_entry_next(amp_entry_t *e);
amp_entry_t *amp_entry_prev(amp_entry_t *e);
amp_object_t *amp_entry_key(amp_entry_t *e);
amp_object_t *amp_entry_value(amp_entry_t *e);

#endif /* map.h */
