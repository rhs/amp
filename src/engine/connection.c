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

#include <amp/allocation.h>
#include <amp/engine.h>
#include <amp/type.h>

struct amp_connection_t {
  AMP_HEAD;
  char *host;
  // ...
};

AMP_TYPE_DECL(CONNECTION, connection)

amp_type_t *CONNECTION = &AMP_TYPE(connection);

amp_connection_t *amp_connection_create() {
  amp_connection_t *o = amp_allocate(AMP_HEAP, NULL, sizeof(amp_connection_t));
  o->type = CONNECTION;
  return o;
}

AMP_DEFAULT_INSPECT(connection)
AMP_DEFAULT_HASH(connection)
AMP_DEFAULT_COMPARE(connection)
