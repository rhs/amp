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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <amp/allocation.h>

amp_region_t *amp_region(size_t size)
{
  amp_region_t *mem = malloc(sizeof(amp_region_t) + size);
  mem->start = mem + 1;
  mem->size = size;
  mem->allocated = 0;
  return mem;
}

void amp_region_clear(amp_region_t *mem)
{
  mem->allocated = 0;
}

void amp_region_free(amp_region_t *mem)
{
  free(mem);
}

void *amp_allocate(amp_region_t *mem, void *ptr, size_t size)
{
  if (mem == AMP_HEAP)
    return realloc(ptr, size);

  size += 15;
  size &= ~((size_t)0xF);

  if (mem->size - mem->allocated < size) {
    errno = ENOMEM;
    return NULL;
  } else {
    void *result = ((char *) mem->start) + mem->allocated;
    mem->allocated += size;
    if (ptr)
    {
      memmove(result, ptr, size);
    }
    return result;
  }
}
