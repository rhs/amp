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
import mllib, os, sys

doc = mllib.xml_parse(os.path.join(os.path.dirname(__file__), "transport.xml"))
mdoc = mllib.xml_parse(os.path.join(os.path.dirname(__file__), "messaging.xml"))

def eq(attr, value):
  return lambda nd: nd[attr] == value

TYPES = doc.query["amqp/section/type", eq("@class", "composite")] + \
    mdoc.query["amqp/section/type", eq("@class", "composite")]
RESTRICTIONS = {}
COMPOSITES = {}

for type in doc.query["amqp/section/type"] + mdoc.query["amqp/section/type"]:
  source = type["@source"]
  if source:
    RESTRICTIONS[type["@name"]] = source
  if type["@class"] == "composite":
    COMPOSITES[type["@name"]] = type

def resolve(name):
  if name in RESTRICTIONS:
    return resolve(RESTRICTIONS[name])
  else:
    return name

TYPEMAP = {
  "boolean": ("bool", "", ""),
  "binary": ("amp_binary_t", "*", ""),
  "string": ("wchar_t", "*", ""),
  "symbol": ("char", "*", ""),
  "ubyte": ("uint8_t", "", ""),
  "ushort": ("uint16_t", "", ""),
  "uint": ("uint32_t", "", ""),
  "ulong": ("uint64_t", "", ""),
  "timestamp": ("uint64_t", "", ""),
  "list": ("amp_list_t", "*", ""),
  "map": ("amp_map_t", "*", ""),
  "box": ("amp_box_t", "*", ""),
  "*": ("amp_object_t", "*", "")
  }

CONSTRUCTORS = {
  "boolean": "boolean",
  "string": "string",
  "symbol": "symbol",
  "ubyte": "ubyte",
  "ushort": "ushort",
  "uint": "uint",
  "ulong": "ulong",
  "timestamp": "ulong"
  }

NULLABLE = set(["string", "symbol"])

def fname(field):
  return field["@name"].replace("-", "_")

def tname(t):
  return t["@name"].replace("-", "_")

def multi(f):
  return f["@multiple"] == "true"

def ftype(field):
  if multi(field):
    return "list"
  elif field["@type"] in COMPOSITES:
    return "box"
  else:
    return resolve(field["@type"]).replace("-", "_")

def fconstruct(field, expr):
  type = ftype(field)
  if type in CONSTRUCTORS:
    result = "amp_%s(mem, %s)" % (CONSTRUCTORS[type], expr)
    if type in NULLABLE:
      result = "%s ? %s : NULL" % (expr, result)
  else:
    result = expr
  if multi(field):
    result = "amp_box(mem, amp_boolean(mem, true), %s)" % result
  return result

def declaration(field):
  name = fname(field)
  type = ftype(field)
  t, pre, post = TYPEMAP.get(type, (type, "", ""))
  return t, "%s%s%s" % (pre, name, post)

def field_kw(field):
  return fname(field).upper()
