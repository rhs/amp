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

#include <amp/box.h>
#include <amp/codec.h>

amp_type_t *BOX = &AMP_ENCODABLE(box);

amp_box_t *amp_box(amp_region_t *mem, amp_object_t *tag, amp_object_t *value)
{
  amp_box_t *o = amp_allocate(mem, NULL, sizeof(amp_box_t));
  o->type = BOX;
  o->tag = tag;
  o->value = value;
  return o;
}

int amp_box_inspect(amp_object_t *o, char **pos, char *limit)
{
  amp_box_t *b = o;
  amp_object_t *tag = amp_box_tag(b);
  amp_object_t *value = amp_box_value(b);
  int n;
  if ((n = amp_inspect(tag, pos, limit))) return n;
  if ((n = amp_format(pos, limit, "("))) return n;
  if ((n = amp_inspect(value, pos, limit))) return n;
  if ((n = amp_format(pos, limit, ")"))) return n;
  return 0;
}

uintptr_t amp_box_hash(amp_object_t *o)
{
  amp_box_t *b = o;
  return amp_hash(amp_box_tag(b)) + amp_hash(amp_box_value(b));
}

int amp_box_compare(amp_object_t *oa, amp_object_t *ob)
{
  amp_box_t *a = oa, *b = ob;

  if (!amp_equal(amp_box_tag(a), amp_box_tag(b)))
    return -1;

  return amp_compare(amp_box_value(a), amp_box_value(b));
}

size_t amp_box_encode_space(amp_object_t *o)
{
  amp_box_t *b = o;
  return 1 + amp_encode_space(b->tag) + amp_encode_space(b->value);
}

int amp_box_encode(amp_object_t *o, amp_encoder_t *e, char **pos, char *limit)
{
  amp_box_t *b = o;
  int n;
  if ((n = amp_write_descriptor(pos, limit)))
    return n;
  if ((n = amp_encode(b->tag, e, pos, limit)))
    return n;
  if ((n = amp_encode(b->value, e, pos, limit)))
    return n;
  return 0;
}

amp_object_t *amp_box_tag(amp_box_t *box)
{
  return box->tag;
}

amp_object_t *amp_box_value(amp_box_t *box)
{
  return box->value;
}

void amp_box_set_tag(amp_box_t *box, amp_object_t *tag)
{
  box->tag = tag;
}

void amp_box_set_value(amp_box_t *box, amp_object_t *value)
{
  box->value = value;
}
