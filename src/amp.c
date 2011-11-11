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

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <amp/driver.h>
#include <amp/value.h>

void print(amp_value_t value)
{
  char buf[1024];
  char *pos = buf;
  amp_format_value(&pos, buf + 1024, &value, 1);
  *pos = '\0';
  printf("%s\n", buf);
}

int value(int argc, char **argv)
{
  amp_value_t v[1024];
  int count = amp_scan(v, "niIlLISz[iii{SSSi}]i([iiiiz])@i[iii]{SiSiSi}", -3, 3, -123456789101112, 123456719101112, 3,
                       L"this is a string", (size_t) 16, "this is binary\x00\x01",
                       1, 2, 3, L"key", L"value", L"one", 1,
                       1, 2, 3, 4, 5, 7, "binary",
                       1, 2, 3,
                       L"one", 1, L"two", 2, L"three", 3);

  amp_list_t *list = amp_to_list(v[8]);
  amp_map_t *map = amp_to_map(amp_list_get(list, 3));
  print(amp_list_get(list, 3));
  printf("POP: ");
  print(amp_map_pop(map, amp_value("S", L"key")));

  printf("scanned %i values\n", count);
  for (int i = 0; i < count; i++) {
    printf("value %.2i [%zi]: ", i, amp_encode_sizeof(v[i])); print(v[i]);
  }

  amp_list_t *l = amp_list(1024);
  amp_list_extend(l, "SIi[iii]", L"One", 2, -3, 4, 5, 6);
  printf("list [%zi]: ", amp_encode_sizeof_list(l)); print(amp_from_list(l));

  for (int i = 0; i < count; i++)
  {
    char buf[amp_encode_sizeof(v[i])];
    size_t size = amp_encode(v[i], buf);
    amp_value_t value;
    size_t read = amp_decode(&value, buf, size);
    printf("read=%zi: ", read); print(value);
  }

  return 0;
}

int main(int argc, char **argv)
{
  if (argc > 1 && !strcmp(argv[1], "value"))
  {
    return value(argc, argv);
  }

  amp_driver_t *drv = amp_driver();
  amp_selectable_t *sel;
  if (argc > 1) {
    amp_connection_t *conn = amp_connection_create();
    amp_session_t *ssn = amp_session_create();
    bool send = !strcmp(argv[1], "send");
    amp_link_t *lnk = amp_link_create(send, L"test-link");
    if (send) {
      amp_link_set_target(lnk, L"queue");
    } else {
      amp_link_set_source(lnk, L"queue");
    }
    amp_connection_add(conn, ssn);
    amp_session_add(ssn, lnk);
    amp_connection_open(conn);
    amp_session_begin(ssn);
    amp_link_attach(lnk);
    if (send) {
      amp_link_send(lnk, "a", "testing", 7);
      amp_link_send(lnk, "b", "one", 3);
      amp_link_send(lnk, "c", "two", 3);
      amp_link_send(lnk, "d", "three", 5);
    } else {
      amp_link_flow(lnk, 10);
    }
    sel = amp_connector("0.0.0.0", "5672", conn, 0, 0);
  } else {
    sel = amp_acceptor("0.0.0.0", "5672", 0, 0);
  }
  if (!sel) {
    perror("driver");
    exit(-1);
  }
  amp_driver_add(drv, sel);
  amp_driver_run(drv);
  return 0;
}
