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

struct amp_link_t {
  AMP_HEAD;
  amp_connection_t *connection;
  amp_session_t *session;
  // ...
};

AMP_TYPE_DECL(LINK, link)

amp_type_t *LINK = &AMP_TYPE(link);

amp_link_t *amp_link_create()
{
  amp_link_t *o = amp_allocate(AMP_HEAP, NULL, sizeof(amp_link_t));
  o->type = LINK;
  return o;
}

AMP_DEFAULT_INSPECT(link)
AMP_DEFAULT_HASH(link)
AMP_DEFAULT_COMPARE(link)
