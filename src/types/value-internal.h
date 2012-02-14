#ifndef _AMP_VALUE_INTERNAL_H
#define _AMP_VALUE_INTERNAL_H 1

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

#include <amp/value.h>

struct amp_array_st {
  enum TYPE type;
  size_t size;
  size_t capacity;
  amp_value_t values[];
};

struct amp_list_st {
  size_t size;
  size_t capacity;
  amp_value_t values[];
};

struct amp_map_st {
  size_t size;
  size_t capacity;
  amp_value_t pairs[];
};

#endif /* value-internal.h */
