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

struct context_t {
  int count;
};

void server_callback(amp_connection_t *conn, void *context)
{
  struct context_t *ctx = context;

  amp_endpoint_t *endpoint = amp_endpoint_head(conn, UNINIT, ACTIVE);
  while (endpoint)
  {
    switch (amp_endpoint_type(endpoint))
    {
    case CONNECTION:
    case SESSION:
      if (amp_remote_state(endpoint) != UNINIT)
        amp_open(endpoint);
      break;
    case SENDER:
    case RECEIVER:
      {
        amp_link_t *link = (amp_link_t *) endpoint;
        if (amp_remote_state(endpoint) != UNINIT) {
          printf("%ls, %ls\n", amp_remote_source(link), amp_remote_target(link));
          amp_set_source(link, amp_remote_source(link));
          amp_set_target(link, amp_remote_target(link));
          amp_open(endpoint);
          if (amp_endpoint_type(endpoint) == RECEIVER) {
            amp_flow((amp_receiver_t *) endpoint, 100);
          } else {
            amp_delivery(link, strtag("blah"));
          }
        }
      }
      break;
    case TRANSPORT:
      break;
    }

    endpoint = amp_endpoint_next(endpoint, UNINIT, ACTIVE);
  }

  amp_delivery_t *delivery = amp_work_head(conn);
  while (delivery)
  {
    amp_binary_t tag = amp_delivery_tag(delivery);
    amp_link_t *link = amp_link(delivery);
    if (amp_endpoint_type((amp_endpoint_t *)link) == RECEIVER) {
      printf("received delivery: %s\n", amp_aformat(amp_from_binary(tag)));
      amp_receiver_t *receiver = (amp_receiver_t *) link;
      amp_recv(receiver, NULL, 0); amp_advance(link);
      amp_disposition(delivery, ACCEPTED);
    } else {
      amp_sender_t *sender = (amp_sender_t *) link;
      while (true) {
        amp_delivery_t *current = amp_current(link);
        amp_send(sender, NULL, 0);
        if (amp_advance(link)) {
          printf("sent delivery: %s\n", amp_aformat(amp_from_binary(amp_delivery_tag(current))));
          char tagbuf[16];
          sprintf(tagbuf, "%i", ctx->count++);
          amp_delivery(link, strtag(tagbuf));
        } else {
          break;
        }
      }
    }
    delivery = amp_work_next(delivery);
  }

  endpoint = amp_endpoint_head(conn, ACTIVE, CLOSED);
  while (endpoint)
  {
    switch (amp_endpoint_type(endpoint))
    {
    case CONNECTION:
    case SESSION:
    case SENDER:
    case RECEIVER:
      if (amp_remote_state(endpoint) == CLOSED) {
        amp_close(endpoint);
      }
      break;
    case TRANSPORT:
      break;
    }

    endpoint = amp_endpoint_next(endpoint, ACTIVE, CLOSED);
  }
}

void client_callback(amp_connection_t *connection, void *context)
{
  amp_delivery_t *delivery = amp_work_head(connection);
  while (delivery)
  {
    amp_link_t *link = amp_link(delivery);
    if (amp_endpoint_type((amp_endpoint_t *) link) == SENDER) {
      amp_sender_t *snd = (amp_sender_t *) link;
      while (true) {
        amp_delivery_t *current = amp_current(link);
        if (!current) {
          amp_close((amp_endpoint_t *)link);
          amp_close((amp_endpoint_t *)amp_get_session(link));
          amp_close((amp_endpoint_t *)connection);
        } else {
          amp_send(snd, NULL, 0);
        }
        if (amp_advance(link)) {
          printf("sent delivery: %s\n", amp_aformat(amp_from_binary(amp_delivery_tag(current))));
        } else {
          break;
        }
      }
    } else {
      amp_delivery_t *current = amp_current(link);
      printf("received delivery: %s\n", amp_aformat(amp_from_binary(amp_delivery_tag(current))));
      amp_receiver_t *receiver = (amp_receiver_t *) link;
      amp_recv(receiver, NULL, 0); amp_advance(link);
      amp_disposition(delivery, ACCEPTED);
    }
    delivery = amp_work_next(delivery);
  }

  amp_endpoint_t *endpoint = amp_endpoint_head(connection, CLOSED, CLOSED);
  while (endpoint)
  {
    switch (amp_endpoint_type(endpoint)) {
    case CONNECTION:
      amp_driver_stop(context);
      break;
    case SESSION:
    case SENDER:
    case RECEIVER:
    case TRANSPORT:
      break;
    }
    endpoint = amp_endpoint_next(endpoint, ACTIVE, CLOSED);
  }
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
    amp_connection_t *conn = amp_connection();
    amp_session_t *ssn = amp_session(conn);
    bool send = !strcmp(argv[1], "send");
    amp_link_t *lnk;
    if (send) {
      lnk = (amp_link_t *) amp_sender(ssn, L"link");
      amp_set_target(lnk, L"queue");
    } else {
      lnk = (amp_link_t *) amp_receiver(ssn, L"link");
      amp_set_source(lnk, L"queue");
    }
    amp_open((amp_endpoint_t *)conn); amp_open((amp_endpoint_t *)ssn); amp_open((amp_endpoint_t *)lnk);
    if (send) {
      amp_sender_t *snd = (amp_sender_t *) lnk;
      amp_delivery(lnk, strtag("a")); amp_send(snd, "testing", 7); amp_advance(lnk);
      amp_delivery(lnk, strtag("b")); amp_send(snd, "one", 3); amp_advance(lnk);
      amp_delivery(lnk, strtag("c")); amp_send(snd, "two", 3); amp_advance(lnk);
      amp_delivery(lnk, strtag("d")); amp_send(snd, "three", 5); amp_advance(lnk);
    } else {
      amp_receiver_t *rcv = (amp_receiver_t *) lnk;
      amp_flow(rcv, 10);
    }
    sel = amp_connector("0.0.0.0", "5672", conn, client_callback, drv);
  } else {
    struct context_t ctx = {0};
    sel = amp_acceptor("0.0.0.0", "5672", server_callback, &ctx);
  }
  if (!sel) {
    perror("driver");
    exit(-1);
  }
  amp_driver_add(drv, sel);
  amp_driver_run(drv);
  return 0;
}
