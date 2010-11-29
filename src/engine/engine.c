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

struct amp_error_t {
  AMP_HEAD;
  int code;
  // ...
};

struct amp_engine_t {
  AMP_HEAD;
  amp_connection_t *connection;
  // ...
};

AMP_TYPE_DECL(ENGINE, engine)

amp_type_t *ENGINE = &AMP_TYPE(engine);

amp_engine_t *amp_engine_create(amp_connection_t *connection)
{
  amp_engine_t *o = amp_allocate(AMP_HEAP, NULL, sizeof(amp_engine_t));
  o->type = ENGINE;
  o->connection = connection;
  return o;
}

int amp_engine_inspect(amp_object_t *o, char **pos, char *limit)
{
  amp_engine_t *e = o;
  int n;
  if ((n = amp_format(pos, limit, "engine<%p>(connection=", o))) return n;
  if ((n = amp_inspect(e->connection, pos, limit))) return n;
  if ((n = amp_format(pos, limit, ")"))) return n;
  return 0;
}

AMP_DEFAULT_HASH(engine)
AMP_DEFAULT_COMPARE(engine)
