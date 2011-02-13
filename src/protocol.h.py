#!/usr/bin/python
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

from protocol import *

print "/* generated */"
print "#ifndef _AMP_PROTOCOL_H"
print "#define _AMP_PROTOCOL_H 1"
print
print '#include <amp/binary.h>'
print '#include <amp/list.h>'
print '#include <amp/map.h>'
print '#include <amp/box.h>'
print

for type in TYPES:
  fidx = 0
  for f in type.query["field"]:
    print "#define %s_%s (%s)" % (field_kw(type), field_kw(f), fidx)
    fidx += 1

kws = []

for type in TYPES:
  name = tname(type)
  print "#define amp_proto_%s(...) amp_proto_%s_kw(__VA_ARGS__, KW_END)" % (name, name)
  print "amp_box_t *amp_proto_%s_kw(amp_region_t *mem, int first, ...);" % name
  print

  for f in type.query["field"]:
    kw = field_kw(f)
    if kw not in kws:
      kws.append(kw)

idx = 0
for kw in kws:
  print "#define %s %s" % (kw, idx)
  idx += 1

print "#define KW_END %s" % idx
print

idx = 0

for type in TYPES:
  provides = type["@provides"]
  if provides and "frame" in provides:
    desc = type["descriptor"]
    print "#define %s_SYM (\"%s\")" % (type["@name"].upper(), desc["@name"])
    hi, lo = [int(x, 0) for x in desc["@code"].split(":")]
    code = (hi << 32) + lo
    print "#define %s_CODE (%s)" % (type["@name"].upper(), code)
    print "#define %s (%s)" % (type["@name"].upper(), idx)
    idx += 1

print
print "#endif /* protocol.h */"
