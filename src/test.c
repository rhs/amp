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
#include <stdarg.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <amp/framing.h>
#include <amp/codec.h>
#include <amp/decoder.h>
#include <amp/string.h>

#include <amp/engine.h>
#include "protocol.h"
#include "util.h"


double seconds(const struct timeval t)
{
  double s = t.tv_sec;
  s += ((double)t.tv_usec)/1000000;
  return s;
}

int codec_main(int argc, char **argv)
{
  char bytes[10*1024];
  char *pos;
  char *limit = bytes + 10*1024;
  struct timeval start, stop, encode, decode;
  int n = 0, i, count = 1000000;

  gettimeofday(&start, NULL);
  for (i = 0; i < count; i++)
  {
    pos = bytes;
    amp_write_descriptor(&pos, limit);
    amp_write_boolean(&pos, limit, true);
    amp_write_boolean(&pos, limit, false);
    amp_write_ushort(&pos, limit, 65534);
    amp_write_short(&pos, limit, -30000);
    amp_write_uint(&pos, limit, 1 << 31);
    amp_write_int(&pos, limit, -1 << 31);
    amp_write_float(&pos, limit, 3.14159265359);
    amp_write_ulong(&pos, limit, (uint64_t)1 << 32);
    amp_write_long(&pos, limit, (int64_t)-1 << 32);
    amp_write_double(&pos, limit, 3.14159265359);
    amp_write_binary(&pos, limit, 4, "asdf");
    amp_write_utf8(&pos, limit, 4, "fdsa");
    amp_write_symbol(&pos, limit, 7, "testing");

    char *outer;
    amp_write_start(&pos, limit, &outer);
    amp_write_symbol(&pos, limit, 5, "test2");
    //amp_descriptor(&enc);
    amp_write_double(&pos, limit, 3.14159265359);
    amp_write_utf8(&pos, limit, 4, "qwer");
    //amp_descriptor(&enc);
    char *inner;
    amp_write_start(&pos, limit, &inner);
    amp_write_symbol(&pos, limit, 6, "nested");
    amp_write_list(&pos, limit, inner, 1);
    amp_write_symbol(&pos, limit, 4, "jkl;");
    amp_write_list(&pos, limit, outer, 5);

    amp_write_start(&pos, limit, &outer);
    amp_write_symbol(&pos, limit, 3, "key");
    amp_write_utf8(&pos, limit, 5, "value");
    amp_write_map(&pos, limit, outer, 1);
  }
  gettimeofday(&stop, NULL);
  timersub(&stop, &start, &encode);

  size_t size = pos - bytes;
  amp_region_t *region = amp_region(10*1024*1024);
  amp_decoder_t *dec = amp_decoder(AMP_HEAP);
  gettimeofday(&start, NULL);

  for (i = 0; i < count; i++)
  {
    amp_region_clear(region);
    amp_decoder_init(dec, region);
    n = amp_read_data(bytes, size, decoder, dec);
  }
  gettimeofday(&stop, NULL);
  timersub(&stop, &start, &decode);

  amp_read_data(bytes, size, printer, NULL);
  printf("------\n");
  for (i = 0; i < dec->size; i++)
  {
    printf("%s\n", amp_ainspect(dec->values[i]));
  }

  printf("------\n");

  printf("encode: %f\n", seconds(encode));
  printf("decode: %f\n", seconds(decode));

  return 0;
}

#define CAPACITY (10*1024)

int piper(int argc, char **argv)
{
  int err, n;
  char bytes[CAPACITY];
  size_t available = 0;
  amp_frame_t frame;

  while (1) {
    available += fread(bytes + available, sizeof(char), CAPACITY - available, stdin);

    while ((n = amp_read_frame(&frame, bytes, available)))
    {
      int e = amp_read_data(frame.payload, frame.size, printer, NULL);
      if (e) {
        printf("error: %d\n", e);
      }
      available -= n;
      memmove(bytes, bytes + n, available);
    }

    if ((err = ferror(stdin))) {
      printf("%s\n", strerror(err));
      return 1;
    }

    if (feof(stdin)) {
      break;
    }
  }

  return 0;
}


struct handler_st {
  amp_decoder_t *dec;
  amp_region_t *region;
  int (*handle)(struct handler_st *h, char *bytes, size_t available);
};


#define BUF_SIZE (64*1024)

int frame_reader(struct handler_st *h, char *bytes, size_t available)
{
  amp_frame_t frame;
  size_t n = amp_read_frame(&frame, bytes, available);

  if (n) {
    amp_decoder_init(h->dec, h->region);
    int e = amp_read_data(frame.payload, frame.size, decoder, h->dec);
    if (e) {
      printf("error: %d\n", e);
    } else {
      for (int i = 0; i < h->dec->size; i++) {
        printf("%s\n", amp_ainspect(h->dec->values[i]));
      }
    }
  }

  return n;
}

int proto_hdr(struct handler_st *h, char *bytes, size_t available)
{
  if (available >= AMQP_HEADER_SIZE) {
    int i;
    printf("HEADER %.*s", 4, bytes);
    for (i = 4; i < 8; i++)
    {
      printf(" %d", bytes[i]);
    }
    printf("\n");
    h->handle = frame_reader;
    return AMQP_HEADER_SIZE;
  } else {
    return 0;
  }
}

static void send_frame(char **output, char *limit, amp_encoder_t *enc, amp_object_t *payload)
{
  amp_frame_t frame = {0};
  char bytes[BUF_SIZE];
  char *pos = bytes;
  amp_encode(payload, enc, &pos, pos + BUF_SIZE);
  frame.payload = bytes;
  frame.size = pos - bytes;
  size_t n = amp_write_frame(*output, limit - *output, frame);
  *output += n;
}

int netpipe(int argc, char **argv)
{
  int sock;
  ssize_t n;
  size_t m;
  struct addrinfo *addr;
  char input[BUF_SIZE];
  char output[BUF_SIZE];
  char *pos = output;
  char *limit = output + BUF_SIZE;

  amp_encoder_t *enc = amp_encoder(AMP_HEAP);
  amp_encoder_init(enc);
  amp_decoder_t *dec = amp_decoder(AMP_HEAP);
  amp_region_t *region = amp_region(1024*1024);
  struct handler_st h = {dec, region, proto_hdr};

  memmove(pos, "AMQP\x00\x01\x00\x00", 8);
  pos += 8;

  bool sender = false;
  bool receiver = true;
  bool role = sender;

  send_frame(&pos, limit, enc, amp_proto_open(region,
                                              HOSTNAME, L"asdf",
                                              CONTAINER_ID, L"fdsa",
                                              MAX_FRAME_SIZE, 4*1024,
                                              CHANNEL_MAX, 255));
  send_frame(&pos, limit, enc, amp_proto_begin(region));
  amp_box_t *address = amp_proto_source(region, ADDRESS, amp_string(region, L"queue"));
  amp_box_t *flow_state;
  if (role == receiver)
    flow_state = amp_proto_flow_state(region, LINK_CREDIT, 10);
  else
    flow_state = amp_proto_flow_state(region, TRANSFER_COUNT, 0, LINK_CREDIT, 0);
  send_frame(&pos, limit, enc,
             amp_proto_attach(region,
                              NAME, L"link",
                              HANDLE, 0,
                              ROLE, role,
                              FLOW_STATE, flow_state,
                              LOCAL, amp_proto_linkage(region, SOURCE, address)));
  if (role == sender) {
    amp_list_t *msg = amp_list(region, 4);
    amp_list_add(msg, amp_proto_fragment(region,
                                         FIRST, false,
                                         LAST, true,
                                         FORMAT_CODE, 0,
                                         FRAGMENT_OFFSET, (uint64_t) 0));
    send_frame(&pos, limit, enc,
               amp_proto_transfer(region,
                                  HANDLE, 0,
                                  TRANSFER_ID, 0,
                                  SETTLED, true,
                                  FLOW_STATE, amp_proto_flow_state(region,
                                                                   TRANSFER_COUNT, 1,
                                                                   LINK_CREDIT, 0),
                                  FRAGMENTS, msg));
  }
  send_frame(&pos, limit, enc, amp_proto_detach(region, HANDLE, 0));
  send_frame(&pos, limit, enc, amp_proto_end(region));
  send_frame(&pos, limit, enc, amp_proto_close(region));

  DIE_IFR(getaddrinfo(argv[1], argv[2], NULL, &addr), gai_strerror);
  DIE_IFE(sock = socket(AF_INET, SOCK_STREAM, getprotobyname("tcp")->p_proto));
  DIE_IFE(connect(sock, addr->ai_addr, addr->ai_addrlen));

  DIE_IFE(n = send(sock, output, pos - output, 0));
  DIE_IFE(shutdown(sock, SHUT_WR));

  if (n != pos - output) {
    fprintf(stderr, "send failed");
    exit(-1);
  }

  size_t available = 0;
  while (1) {
    DIE_IFE(n = recv(sock, input + available, BUF_SIZE - available, 0));
    available += n;

    while ((m = h.handle(&h, input, available))) {
      available -= m;
      memmove(input, input + m, available);
    }

    if (n == 0)
    {
      break;
    }
  }

  DIE_IFE(close(sock));

  return 0;
}

int main(int argc, char **argv)
{
  return netpipe(argc, argv);
  //return codec_main(argc, argv);
  char buf[4*1024];
  amp_box_t *b = amp_proto_open(AMP_HEAP,
                                HOSTNAME, L"asdf",
                                CONTAINER_ID, L"fdsa",
                                MAX_FRAME_SIZE, 64*1024,
                                CHANNEL_MAX, 255,
                                HEARTBEAT_INTERVAL, 60*60);
  //printf("%s\n", amp_ainspect(b));
  amp_encoder_t *enc = amp_encoder(AMP_HEAP);
  amp_encoder_init(enc);
  char *pos = buf;
  int n = amp_encode(b, enc, &pos, pos + 4*1024);
  if (n) {
    printf("%d\n", n);
  } else {
    size_t size = pos - buf;
    for (int i = 0; i < size; i++)
      putchar(buf[i]);
  }

  return n; //return netpipe(argc, argv);
}
