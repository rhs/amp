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
print
print '#include <stdarg.h>'
print '#include <stdlib.h>'
print '#include <stdio.h>'
print
print '#include "protocol.h"'
print '#include <amp/scalars.h>'
print '#include <amp/string.h>'
print '#include <amp/symbol.h>'

PROMOTIONS = {
  "bool": "int",
  "uint8_t": "int",
  "uint16_t": "int"
  }

LIST = "list"

for type in TYPES:
  fields = list(type.query["field"])
  source = type["@source"]

  print
  print "amp_box_t *amp_proto_%s_kw(amp_region_t *mem, int first, ...)" % tname(type)
  print "{"
  if source == LIST:
    print "  amp_list_t *l = amp_list(mem, %s);" % len(fields)
    print "  amp_list_fill(l, NULL, %s);" % len(fields)
  else:
    print "  amp_map_t *m = amp_map(mem, %s);" % len(fields)
  print "  va_list ap;"
  print "  va_start(ap, first);"
  print
  print "  int arg = first;"
  print "  bool done = false;"
  print "  while (!done) {"
  print "    switch (arg) {"

  idx = 0
  for f in fields:
    print "    case %s:" % field_kw(f)
    ftype = declaration(f)[0]
    if declaration(f)[1].startswith("*"):
      ftype += " *"
    expr = fconstruct(f, "va_arg(ap, %s)" % PROMOTIONS.get(ftype, ftype))
    if source == LIST:
      print "      amp_list_set(l, %s, %s);" % (idx, expr)
    else:
      print '      amp_map_set(m, amp_symbol(mem, "%s"), %s);' % (f["@name"], expr)
    print "      break;"
    idx += 1

  print "    case KW_END:"
  print "      done = true;"
  print "      continue;"
  print "    default:"
  print '      fprintf(stderr, "unrecognized arg: %u\\n", arg);'
  print "      exit(-1);"
  print "      done = true;"
  print "      continue;"
  print "    }"
  print
  print "    arg = va_arg(ap, int);"
  print "  }"
  print
  print "  va_end(ap);"
  print
  if source == LIST:
    print '  return amp_box(mem, amp_symbol(mem, "%s"), l);' % type["descriptor/@name"]
  else:
    print '  return amp_box(mem, amp_symbol(mem, "%s"), m);' % type["descriptor/@name"]
  print "}"
