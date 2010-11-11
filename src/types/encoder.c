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

#include <amp/encoder.h>

amp_encoder_t* amp_encoder(amp_region_t *mem)
{
  amp_encoder_t *enc = amp_allocate(mem, NULL, sizeof(amp_encoder_t));
  enc->utf8 = iconv_open("UTF-8", "WCHAR_T");
  enc->utf16 = iconv_open("UTF-16BE", "WCHAR_T");
  return enc;
}

void amp_encoder_init(amp_encoder_t *enc)
{
  iconv(enc->utf8, NULL, NULL, NULL, NULL);
  iconv(enc->utf16, NULL, NULL, NULL, NULL);
}
