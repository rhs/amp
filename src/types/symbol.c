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

#include <string.h>
#include <amp/symbol.h>
#include <amp/codec.h>

amp_type_t *SYMBOL = &AMP_ENCODABLE(symbol);

amp_symbol_t *amp_symbol(amp_region_t *mem, const char *name)
{
  return amp_symboln(mem, name, strlen(name));
}

amp_symbol_t *amp_symboln(amp_region_t *mem, const char *name, size_t size)
{
  int hash = 1;
  amp_symbol_t *o = amp_allocate(mem, NULL, sizeof(amp_symbol_t) + size + 1);
  if (!o) return NULL;
  o->type = SYMBOL;
  o->size = size;
  o->name = (char *)(o + 1);
  strncpy(o->name, name, size);
  o->name[size] = '\0';
  for (int i = 0; i < size; i++)
  {
    hash = 31*hash + o->name[i];
  }
  o->hash = hash;
  return o;
}

int amp_symbol_inspect(amp_object_t *o, char **pos, char *limit)
{
  amp_symbol_t *s = o;
  return amp_format(pos, limit, "%s", s->name);
}

intptr_t amp_symbol_hash(amp_object_t *o)
{
  amp_symbol_t *s = o;
  return s->hash;
}

int amp_symbol_compare(amp_object_t *oa, amp_object_t *ob)
{
  amp_symbol_t *a = oa, *b = ob;
  return strcmp(a->name, b->name);
}

size_t amp_symbol_encode_space(amp_object_t *o)
{
  amp_symbol_t *s = o;
  return 5 + s->size;
}

int amp_symbol_encode(amp_object_t *o, amp_encoder_t *e, char **pos, char *limit)
{
  amp_symbol_t *s = o;
  return amp_write_symbol(pos, limit, s->size, s->name);
}
