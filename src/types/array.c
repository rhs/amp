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

#include <amp/util.h>
#include <amp/value.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include "../codec/encodings.h"

static char type_to_code(enum TYPE type)
{
  switch (type)
  {
  case EMPTY: return 'n';
  case BYTE: return 'b';
  case UBYTE: return 'B';
  case SHORT: return 'h';
  case USHORT: return 'H';
  case INT: return 'i';
  case UINT: return 'I';
  case LONG: return 'l';
  case ULONG: return 'L';
  case FLOAT: return 'f';
  case DOUBLE: return 'd';
  case CHAR: return 'C';
  case STRING: return 'S';
  case BINARY: return 'z';
  case LIST: return 't';
  case MAP: return 'm';
  default: return -1;
  }
}

static uint8_t type_to_amqp_code(enum TYPE type)
{
  switch (type)
  {
  case EMPTY: return AMPE_NULL;
  case BOOLEAN: return AMPE_BOOLEAN;
  case BYTE: return AMPE_BYTE;
  case UBYTE: return AMPE_UBYTE;
  case SHORT: return AMPE_SHORT;
  case USHORT: return AMPE_USHORT;
  case INT: return AMPE_INT;
  case UINT: return AMPE_UINT;
  case CHAR: return AMPE_UTF32;
  case LONG: return AMPE_LONG;
  case ULONG: return AMPE_ULONG;
  case FLOAT: return AMPE_FLOAT;
  case DOUBLE: return AMPE_DOUBLE;
  case STRING: return AMPE_STR32_UTF8;
  case BINARY: return AMPE_VBIN32;
  case LIST: return AMPE_LIST32;
  case MAP: return AMPE_MAP32;
  case ARRAY: return AMPE_ARRAY32;
  default:
    amp_fatal("no amqp code for type: %i", type);
    return -1;
  }
}

amp_array_t *amp_array(enum TYPE type, int capacity)
{
  amp_array_t *l = malloc(sizeof(amp_array_t) + capacity*sizeof(amp_value_t));
  if (l) {
    l->type = type;
    l->capacity = capacity;
    l->size = 0;
  }
  return l;
}

void amp_free_array(amp_array_t *a)
{
  free(a);
}

void amp_visit_array(amp_array_t *a, void (*visitor)(amp_value_t))
{
  for (int i = 0; i < a->size; i++)
  {
    amp_visit(a->values[i], visitor);
  }
}

int amp_format_array(char **pos, char *limit, amp_array_t *array)
{
  int e;
  if ((e = amp_fmt(pos, limit, "@%c[", type_to_code(array->type)))) return e;
  if ((e = amp_format_value(pos, limit, array->values, array->size))) return e;
  if ((e = amp_fmt(pos, limit, "]"))) return e;
  return 0;
}

size_t amp_encode_sizeof_array(amp_array_t *array)
{
  size_t result = 9;
  for (int i = 0; i < array->size; i++)
  {
    // XXX: this is wrong, need to compensate for code
    result += amp_encode_sizeof(array->values[i]);
  }
  return result;
}

size_t amp_encode_array(amp_array_t *array, char *out)
{
  // code
  out[0] = (uint8_t) AMPE_ARRAY32;
  // size will be backfilled
  // count
  *((uint32_t *) (out + 5)) = htonl(array->size);
  // element code
  out[9] = (uint8_t) type_to_amqp_code(array->type);

  char *vout = out + 10;
  for (int i = 0; i < array->size; i++)
  {
    char *codeptr = vout - 1;
    char codeval = *codeptr;
    vout += amp_encode(array->values[i], vout-1) - 1;
    *codeptr = codeval;
  }

  // backfill size
  *((uint32_t *) (out + 1)) = htonl(vout - out - 5);
  return vout - out;
}
