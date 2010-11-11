#ifndef _SCALARS_H
#define _SCALARS_H 1

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

#define SCALAR_DECL(NAME, TYPE, CTYPE)                                  \
  typedef struct {                                                      \
    AMP_HEAD;                                                           \
    CTYPE value;                                                        \
  } amp_ ## NAME ## _t;                                                 \
                                                                        \
  AMP_SCALAR_DECL(TYPE, NAME)                                           \
                                                                        \
  amp_ ## NAME ## _t *amp_ ## NAME(amp_region_t *mem, CTYPE n);

SCALAR_DECL(boolean, BOOLEAN, bool)
SCALAR_DECL(ubyte, UBYTE, uint8_t)
SCALAR_DECL(ushort, USHORT, uint16_t)
SCALAR_DECL(uint, UINT, uint32_t)
SCALAR_DECL(ulong, ULONG, uint64_t)
SCALAR_DECL(byte, BYTE, int8_t)
SCALAR_DECL(short, SHORT, int16_t)
SCALAR_DECL(int, INT, int32_t)
SCALAR_DECL(long, LONG, int64_t)
SCALAR_DECL(float, FLOAT, float)
SCALAR_DECL(double, DOUBLE, double)
SCALAR_DECL(char, CHAR, wchar_t)

#endif /* scalars.h */
