#ifndef _AMP_ALLOCATION_H
#define _AMP_ALLOCATION_H 1

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

#include <stddef.h>

typedef struct {
  void *start;
  size_t size;
  size_t allocated;
} amp_region_t;

amp_region_t *amp_region(size_t size);
void amp_region_clear(amp_region_t *mem);
void amp_region_free(amp_region_t *mem);

#define AMP_HEAP (NULL)

void *amp_allocate(amp_region_t *mem, void *ptr, size_t size);

#endif /* allocation.h */
