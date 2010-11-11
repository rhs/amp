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

#include <amp/string.h>
#include <amp/codec.h>

amp_type_t *STRING = &AMP_ENCODABLE(string);

amp_string_t *amp_string(amp_region_t *mem, const wchar_t *wcs)
{
  return amp_stringn(mem, wcs, wcslen(wcs));
}

amp_string_t *amp_stringn(amp_region_t *mem, const wchar_t *wcs, size_t size)
{
  amp_string_t *o = amp_allocate(mem, NULL, sizeof(amp_string_t) +
                                 (size + 1)*sizeof(wchar_t));
  if (!o) return NULL;
  o->type = STRING;
  o->size = size;
  o->wcs = (wchar_t *)(o + 1);
  wcsncpy(o->wcs, wcs, size);
  o->wcs[size] = L'\0';
  return o;
}

int amp_string_inspect(amp_object_t *o, char **pos, char *limit)
{
  amp_string_t *s = o;
  return amp_format(pos, limit, "%ls", s->wcs);
}

intptr_t amp_string_hash(amp_object_t *o)
{
  amp_string_t *s = o;
  wchar_t *c;
  int hash = 1;
  for (c = s->wcs; *c; c++)
  {
    hash = 31*hash + *c;
  }
  return hash;
}

int amp_string_compare(amp_object_t *oa, amp_object_t *ob)
{
  amp_string_t *a = oa, *b = ob;
  return wcscmp(a->wcs, b->wcs);
}

size_t amp_string_encode_space(amp_object_t *o)
{
  amp_string_t *s = o;
  return 5 + s->size*4;
}

#include <stdio.h>
#include <stdlib.h>

int amp_string_encode(amp_object_t *o, amp_encoder_t *e, char **pos, char *limit)
{
  amp_string_t *s = o;
  // XXX: need to convert properly
  char buf[1024];
  char *inbuf = (char *) s->wcs;
  size_t inleft = 4 * s->size;
  char *outbuf = buf;
  size_t outleft = 1024;
  size_t n = iconv(e->utf8, &inbuf, &inleft, &outbuf, &outleft);
  if (n == -1) {
    perror("amp_string_encode");
    exit(-1);
  }
  size_t size = outbuf - buf;
  return amp_write_utf8(pos, limit, size, buf);
}
