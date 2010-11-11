#ifndef _AMP_LIST_H
#define _AMP_LIST_H 1

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

typedef struct {
  AMP_HEAD;
  amp_object_t **elements;
  size_t capacity;
  size_t size;
  amp_region_t *region;
} amp_list_t;

AMP_ENCODABLE_DECL(LIST, list)

amp_list_t *amp_list(amp_region_t *mem, int capacity);
amp_object_t *amp_list_get(amp_list_t *l, int index);
amp_object_t *amp_list_set(amp_list_t *l, int index, amp_object_t *o);
amp_object_t *amp_list_pop(amp_list_t *l, int index);
void amp_list_add(amp_list_t *l, amp_object_t *o);
int amp_list_index(amp_list_t *l, amp_object_t *o);
bool amp_list_remove(amp_list_t *l, amp_object_t *o);
void amp_list_fill(amp_list_t *l, amp_object_t *o, int n);
int amp_list_size(amp_list_t *l);

#endif /* list.h */
