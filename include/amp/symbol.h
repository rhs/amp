#ifndef _SYMBOL_H
#define _SYMBOL_H 1

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
  size_t size;
  int hash;
  char *name;
} amp_symbol_t;

AMP_ENCODABLE_DECL(SYMBOL, symbol)

amp_symbol_t *amp_symbol(amp_region_t *mem, const char *name);
amp_symbol_t *amp_symboln(amp_region_t *mem, const char *name, size_t size);

#endif /* type.h */
