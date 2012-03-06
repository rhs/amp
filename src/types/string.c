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

amp_string_t *amp_string(wchar_t *wcs)
{
  size_t size = wcslen(wcs);
  amp_string_t *str = malloc(sizeof(amp_string_t) + (size+1)*sizeof(wchar_t));
  str->size = size;
  wcscpy(str->wcs, wcs);
  return str;
}

void amp_free_string(amp_string_t *str)
{
  free(str);
}

size_t amp_string_size(amp_string_t *str)
{
  return str->size;
}

wchar_t *amp_string_wcs(amp_string_t *str)
{
  return str->wcs;
}

uintptr_t amp_hash_string(amp_string_t *s)
{
  wchar_t *c;
  uintptr_t hash = 1;
  for (c = s->wcs; *c; c++)
  {
    hash = 31*hash + *c;
  }
  return hash;
}

int amp_compare_string(amp_string_t *a, amp_string_t *b)
{
  if (a->size == b->size)
    return wmemcmp(a->wcs, b->wcs, a->size);
  else
    return b->size - a->size;
}
