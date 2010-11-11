#ifndef _AMP_DECODER_H
#define _AMP_DECODER_H 1

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

#include <iconv.h>
#include <amp/codec.h>
#include <amp/type.h>

typedef enum {
  ST_NOOP,
  ST_LIST,
  ST_MAP,
  ST_DESC
} amp_stop_t;

typedef struct {
  amp_stop_t op;
  amp_object_t *value;
  amp_object_t *key;
  int limit;
  int count;
} amp_stack_t;

typedef struct {
  amp_region_t *region;
  amp_object_t *values[4*1024];
  size_t size;
  amp_stack_t stack[4*1024];
  size_t depth;
  iconv_t utf8;
  iconv_t utf16;
} amp_decoder_t;

extern amp_data_callbacks_t *decoder;

amp_decoder_t *amp_decoder(amp_region_t *mem);
void amp_decoder_init(amp_decoder_t *dec, amp_region_t *region);

#endif /* decoder.h */
