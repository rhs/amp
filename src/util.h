#ifndef _AMP_UTIL_H
#define _AMP_UTIL_H 1

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define DIE_IFR(EXPR, STRERR)                                           \
  do {                                                                  \
    int __code__ = (EXPR);                                              \
    if (__code__) {                                                     \
      fprintf(stderr, "%s:%d: %s: %s (%d)\n", __FILE__, __LINE__,       \
              #EXPR, STRERR(__code__), __code__);                       \
      exit(-1);                                                         \
    }                                                                   \
  } while (0)

#define DIE_IFE(EXPR)                                                   \
  do {                                                                  \
    if ((EXPR) == -1) {                                                 \
      int __code__ = errno;                                             \
      fprintf(stderr, "%s:%d: %s: %s (%d)\n", __FILE__, __LINE__,       \
              #EXPR, strerror(__code__), __code__);                     \
      exit(-1);                                                         \
    }                                                                   \
  } while (0)

#endif /* util.h */
