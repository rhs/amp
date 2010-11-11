#ifndef _AMP_BINARY_H
#define _AMP_BINARY_H 1

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
  char *bytes;
  size_t capacity;
  size_t size;
} amp_binary_t;

AMP_ENCODABLE_DECL(BINARY, binary)

amp_binary_t *amp_binary(amp_region_t *mem, size_t capacity);

char *amp_binary_bytes(amp_binary_t *b);
size_t amp_binary_size(amp_binary_t *b);
size_t amp_binary_capacity(amp_binary_t *b);
void amp_binary_add(amp_binary_t *b, char byte);
void amp_binary_extend(amp_binary_t *b, char *bytes, size_t n);

#endif /* binary.h */
