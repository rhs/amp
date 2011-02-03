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

#define _POSIX_C_SOURCE 1

#include <poll.h>
#include <stdio.h>
#include <time.h>
#include <amp/type.h>
#include <amp/list.h>
#include "util.h"

/* Decls */

typedef struct {
  AMP_HEAD;
  amp_list_t *selectables;
  amp_region_t *region;
} amp_driver_t;

AMP_TYPE_DECL(DRIVER, driver)

#define AMP_SEL_RD (0x0001)
#define AMP_SEL_WR (0x0002)

typedef struct amp_selectable_st amp_selectable_t;

struct amp_selectable_st {
  AMP_HEAD;
  amp_driver_t *driver;
  int fd;
  int status;
  time_t wakeup;
  void (*readable)(amp_selectable_t *s);
  void (*writable)(amp_selectable_t *s);
  void (*tick)(amp_selectable_t *s, time_t now);
  amp_region_t *region;
  void *context;
};

AMP_TYPE_DECL(SELECTABLE, selectable)

/* Impls */

amp_type_t *DRIVER = &AMP_TYPE(driver);

amp_driver_t *amp_driver(amp_region_t *mem)
{
  amp_driver_t *o = amp_allocate(mem, NULL, sizeof(amp_region_t));
  o->type = DRIVER;
  o->selectables = amp_list(mem, 16);
  o->region = mem;
  return o;
}

int amp_driver_inspect(amp_object_t *o, char **pos, char *limit)
{
  amp_driver_t *d = o;
  int n;
  if ((n = amp_format(pos, limit, "driver<%p>(selectables=", o))) return n;
  if ((n = amp_inspect(d->selectables, pos, limit))) return n;
  if ((n = amp_format(pos, limit, ")"))) return n;
  return 0;
}

uintptr_t amp_driver_hash(amp_object_t *o)
{
  return (uintptr_t) o;
}

int amp_driver_compare(amp_object_t *a, amp_object_t *b)
{
  return b != a;
}

void amp_driver_add(amp_driver_t *d, amp_selectable_t *s)
{
  amp_list_add(d->selectables, s);
  s->driver = d;
}

void amp_driver_remove(amp_driver_t *d, amp_selectable_t *s)
{
  amp_list_remove(d->selectables, s);
  s->driver = NULL;
}

void amp_driver_run(amp_driver_t *d)
{
  int i, nfds = 0;
  struct pollfd *fds = NULL;

  while (true)
  {
    int n = amp_list_size(d->selectables);
    if (n == 0) break;
    if (n > nfds) {
      fds = amp_allocate(d->region, fds, n*sizeof(struct pollfd));
      nfds = n;
    }

    for (i = 0; i < n; i++)
    {
      amp_selectable_t *s = amp_list_get(d->selectables, i);
      fds[i].fd = s->fd;
      fds[i].events = (s->status & AMP_SEL_RD ? POLLIN : 0) |
        (s->status & AMP_SEL_WR ? POLLOUT : 0);
      fds[i].revents = 0;
    }

    DIE_IFE(poll(fds, n, -1));

    for (i = 0; i < n; i++)
    {
      amp_selectable_t *s = amp_list_get(d->selectables, i);
      if (fds[i].revents & POLLIN)
        s->readable(s);
      if (fds[i].revents & POLLOUT)
        s->writable(s);
    }
  }

  amp_allocate(d->region, fds, 0);
}

amp_type_t *SELECTABLE = &AMP_TYPE(selectable);

amp_selectable_t *amp_selectable(amp_region_t *mem)
{
  amp_selectable_t *s = amp_allocate(mem, NULL, sizeof(amp_selectable_t));
  if (!s) return NULL;
  s->type = SELECTABLE;
  s->status = 0;
  s->wakeup = 0;
  s->region = mem;
  s->context = NULL;
  return s;
}

int amp_selectable_inspect(amp_object_t *o, char **pos, char *limit)
{
  return amp_format(pos, limit, "selectable<%p>", o);
}

uintptr_t amp_selectable_hash(amp_object_t *o)
{
  return (uintptr_t) o;
}

int amp_selectable_compare(amp_object_t *a, amp_object_t *b)
{
  return b != a;
}

#include <amp/engine.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#define IO_BUF_SIZE (4*1024)

struct amp_engine_ctx {
  amp_engine_t *engine;
  int in_size;
  int out_size;
  char input[IO_BUF_SIZE];
  char output[IO_BUF_SIZE];
};

void amp_engine_readable(amp_selectable_t *sel)
{
  struct amp_engine_ctx *ctx = sel->context;
  amp_engine_t *engine = ctx->engine;
  int n = recv(sel->fd, ctx->input + ctx->in_size, IO_BUF_SIZE - ctx->in_size, 0);

  if (n == 0) {
    printf("disconnected");
    sel->status = 0;
    if (close(sel->fd) == -1)
      perror("close");
    amp_driver_remove(sel->driver, sel);
  } else {
    ctx->in_size += n;
  }

  amp_engine_input(engine, ctx->input, ctx->in_size);
}

void amp_engine_writable(amp_selectable_t *sel)
{
  struct amp_engine_ctx *ctx = sel->context;
  amp_engine_t *engine = ctx->engine;
  ssize_t n = amp_engine_output(engine, ctx->output + ctx->out_size, IO_BUF_SIZE - ctx->out_size);
  if (n < 0) {
    printf("internal error");
    sel->status = 0;
    if (close(sel->fd) == -1)
      perror("close");
    amp_driver_remove(sel->driver, sel);
  } else {
    ctx->out_size += n;
    n = send(sel->fd, ctx->output, ctx->out_size, 0);
    if (n < 0) {
      // XXX
      perror("writable");
    } else {
      ctx->out_size -= n;
      memmove(ctx->output, ctx->output + n, ctx->out_size);
    }
    if (!ctx->out_size) {
      sel->status &= ~AMP_SEL_WR;
    }
  }
}

amp_selectable_t *amp_selectable_engine(amp_region_t *mem, int sock,
                                        amp_connection_t *conn)
{
  amp_selectable_t *sel = amp_selectable(mem);
  sel->fd = sock;
  sel->readable = &amp_engine_readable;
  sel->writable = &amp_engine_writable;
  sel->status = AMP_SEL_RD | AMP_SEL_WR;
  struct amp_engine_ctx *ctx = amp_allocate(mem, NULL, sizeof(struct amp_engine_ctx));
  ctx->engine = amp_engine_create(conn);
  ctx->in_size = 0;
  ctx->out_size = 0;
  sel->context = ctx;
  return sel;
}

void readable(amp_selectable_t *s)
{
  char buf[1024];
  int i, n = recv(s->fd, buf, 1024, 0);

  if (n == 0)
  {
    printf("disconnected");
    s->status = 0;
    if (close(s->fd) == -1)
      perror("close");
    amp_driver_remove(s->driver, s);
  }

  for (i = 0; i < n; i++)
  {
    if (isprint(buf[i]))
      printf("%c", buf[i]);
    else
      printf("\\x%.2x", buf[i]);
  }
  printf("\n");
}

void writable(amp_selectable_t *s)
{
  ssize_t n = send(s->fd, s->context, strlen(s->context), 0);
  if (n == -1)
    perror("writable");
  else
    s->context = ((char *) s->context) + n;
  if (!*((char *) s->context)) {
    s->status &= ~AMP_SEL_WR;
    if (shutdown(s->fd, SHUT_WR) == -1)
      perror("shutdown");
  }
}

amp_selectable_t *amp_connector(amp_region_t *mem, char *host, char *port,
                                amp_connection_t *conn)
{
  struct addrinfo *addr;
  int code = getaddrinfo(host, port, NULL, &addr);
  if (code) {
    fprintf(stderr, gai_strerror(code));
    return NULL;
  }

  int sock = socket(AF_INET, SOCK_STREAM, getprotobyname("tcp")->p_proto);
  if (sock == -1)
    return NULL;

  if (connect(sock, addr->ai_addr, addr->ai_addrlen) == -1)
    return NULL;

  amp_selectable_t *s = amp_selectable_engine(mem, sock, conn);
  return s;
}

void do_accept(amp_selectable_t *s)
{
  struct sockaddr_in addr = {0};
  socklen_t addrlen;
  int sock = accept(s->fd, (struct sockaddr *) &addr, &addrlen);
  if (sock == -1) {
    perror("accept");
  } else {
    char host[1024], serv[64];
    int code;
    if ((code = getnameinfo((struct sockaddr *) &addr, addrlen, host, 1024, serv, 64, 0)))
      printf("getnameinfo: %s\n", gai_strerror(code));
    else
      printf("accepted from %s:%s\n", host, serv);
    amp_connection_t *conn = amp_connection_create();
    amp_selectable_t *a = amp_selectable_engine(s->region, sock, conn);
    a->status = AMP_SEL_RD | AMP_SEL_WR;
    amp_driver_add(s->driver, a);
  }
}

amp_selectable_t *amp_acceptor(amp_region_t *mem, char *host, char *port)
{
  struct addrinfo *addr;
  int code = getaddrinfo(host, port, NULL, &addr);
  if (code) {
    fprintf(stderr, gai_strerror(code));
    return NULL;
  }

  int sock = socket(AF_INET, SOCK_STREAM, getprotobyname("tcp")->p_proto);
  if (sock == -1)
    return NULL;

  int optval = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1)
    return NULL;

  if (bind(sock, addr->ai_addr, addr->ai_addrlen) == -1)
    return NULL;

  if (listen(sock, 50) == -1)
    return NULL;

  amp_selectable_t *s = amp_selectable(mem);
  s->fd = sock;
  s->readable = &do_accept;
  s->writable = NULL;
  s->status = AMP_SEL_RD;
  return s;
}

int main(int argc, char **argv)
{
  amp_driver_t *drv = amp_driver(AMP_HEAP);
  amp_selectable_t *sel;
  //sel = amp_selectable(AMP_HEAP);
  if (argc > 1)
    sel = amp_acceptor(AMP_HEAP, "0.0.0.0", "5672");
  else {
    amp_connection_t *conn = amp_connection_create();
    sel = amp_connector(AMP_HEAP, "0.0.0.0", "5672", conn);
  }
  if (!sel) {
    perror("driver");
    exit(-1);
  }
  amp_driver_add(drv, sel);
  printf("%s\n", amp_ainspect(drv));
  amp_driver_run(drv);
  return 0;
}
