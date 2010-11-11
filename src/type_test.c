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

#include <stdio.h>
#include <wchar.h>
#include <inttypes.h>
#include <amp/type.h>
#include <amp/map.h>
#include <amp/scalars.h>
#include <amp/symbol.h>
#include <amp/string.h>
#include <amp/binary.h>

int main(int argc, char **argv) {
  amp_region_t *mem = amp_region(64*1024);

  amp_byte_t *v1 = amp_byte(mem, 123);
  amp_boolean_t *v2 = amp_boolean(mem, true);
  amp_list_t *l = amp_list(mem, 0);
  amp_list_add(l, v1);
  amp_list_add(l, v2);
  amp_list_add(l, v1);
  amp_list_add(l, v2);
  amp_list_add(l, v1);
  amp_list_add(l, v2);
  amp_list_add(l, v1);
  amp_list_add(l, v2);
  amp_list_add(l, v1);
  amp_list_add(l, v2);
  amp_list_add(l, v1);
  amp_list_add(l, v2);
  amp_list_add(l, v1);
  amp_list_add(l, v2);
  amp_list_add(l, v1);
  amp_list_add(l, v2);
  amp_list_add(l, amp_symbol(mem, "symbol!"));
  printf("%"PRIdPTR"\n", amp_hash(v1));
  printf("%s %s\n", amp_ainspect(v1), amp_ainspect(v2));
  printf("%s\n", amp_ainspect(l));
  amp_list_set(l, 3, amp_int(mem, -4321));
  amp_list_set(l, 4, amp_float(mem, 3.14159));
  printf("%d %s\n", amp_list_size(l), amp_ainspect(l));
  printf("%d\n", amp_compare(amp_symbol(mem, "asdf"), amp_symbol(mem, "asdf")));
  printf("pi hash: %"PRIdPTR"\n", amp_hash(amp_float(mem, 3.14159265359)));
  printf("%s\n", amp_ainspect(amp_char(mem, 'a')));
  printf("%d %d\n", amp_isa(amp_list_get(l, 0), BYTE), amp_isa(amp_list_get(l, 1), BYTE));

  amp_object_t *o = l;
  printf("size: %d\n", amp_list_size(o));

  o = amp_list_get(l, 7);
  if (amp_scalar(o))
  {
    printf("scalar: %s %d\n", amp_ainspect(o), amp_type(o)->to_int32(o));
  }

  amp_map_t *m = amp_map(mem, 16);
  amp_symbol_t *k = amp_symbol(mem, "key");
  amp_map_set(m, k, amp_symbol(mem, "value"));
  printf("v: %s\n", amp_ainspect(amp_map_get(m, k)));
  printf("v: %s\n", amp_ainspect(amp_map_get(m, amp_symbol(mem, "key"))));
  printf("v: %s\n", amp_ainspect(amp_map_get(m, amp_symbol(mem, "asdf"))));
  printf("%s\n", amp_ainspect(m));
  amp_map_set(m, amp_int(mem, 1234), amp_int(mem, 4321));
  printf("%s\n", amp_ainspect(m));
  printf("%"PRIdPTR" %"PRIdPTR"\n", amp_hash(amp_string(mem, L"asdf")), amp_hash(amp_symbol(mem, "asdf")));
  amp_map_set(m, amp_string(mem, L"asdf"), amp_float(mem, 3.14159));
  printf("%s\n", amp_ainspect(m));
  amp_map_set(m, amp_symbol(mem, "asdf"), amp_int(mem, 3));
  printf("%s\n", amp_ainspect(m));
  printf("%s %s\n",
         amp_ainspect(amp_map_get(m, amp_symbol(mem, "asdf"))),
         amp_ainspect(amp_map_get(m, amp_string(mem, L"asdf"))));

  wchar_t wcs[1024];
  int i;
  for (i = 0; i < 15; i++)
  {
    swprintf(wcs, 1024, L"value-%d", i);
    amp_map_set(m, amp_int(mem, i), amp_string(mem, wcs));
  }

  amp_map_pop(m, amp_int(mem, 246));
  amp_map_set(m, amp_int(mem, 246), amp_string(mem, L"value-246"));

  printf("%s\n%d\n", amp_ainspect(m), amp_map_size(m));

  amp_map_t *ma = amp_map(mem, 16);
  amp_map_t *mb = amp_map(mem, 16);
  amp_map_set(ma, amp_symbol(mem, "key3"), amp_string(mem, L"value3"));
  amp_map_set(ma, amp_symbol(mem, "key2"), amp_string(mem, L"value2"));
  amp_map_set(ma, amp_symbol(mem, "key1"), amp_string(mem, L"value1"));
  amp_map_set(mb, amp_symbol(mem, "key1"), amp_string(mem, L"value1"));
  amp_map_set(mb, amp_symbol(mem, "key2"), amp_string(mem, L"value2"));
  amp_map_set(mb, amp_symbol(mem, "key3"), amp_string(mem, L"value3"));

  printf("%"PRIdPTR" %"PRIdPTR" %d\n", amp_hash(ma), amp_hash(mb), amp_equal(ma, mb));

  printf("%zd %zd\n", mem->allocated, amp_encode_space(ma));

  amp_binary_t *bin = amp_binary(mem, 256);
  for (int i = 0; i < 256; i++)
    amp_binary_add(bin, i);

  printf("%s\n", amp_ainspect(bin));

  amp_region_free(mem);
  return 0;
}
