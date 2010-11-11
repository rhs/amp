#ifndef _AMP_BOX_H
#define _AMP_BOX_H 1

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
  amp_object_t *tag;
  amp_object_t *value;
} amp_box_t;

AMP_ENCODABLE_DECL(BOX, box)

amp_box_t *amp_box(amp_region_t *mem, amp_object_t *tag, amp_object_t *value);
amp_object_t *amp_box_tag(amp_box_t *box);
amp_object_t *amp_box_value(amp_box_t *box);
void amp_box_set_tag(amp_box_t *box, amp_object_t *tag);
void amp_box_set_value(amp_box_t *box, amp_object_t *value);

#endif /* map.h */
