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
#include <amp/driver.h>
#include <amp/value.h>
#include "util.h"

/* Decls */

struct amp_driver_t {
  amp_list_t *selectables;
};

struct amp_selectable_st {
  amp_driver_t *driver;
  int fd;
  int status;
  time_t wakeup;
  void (*readable)(amp_selectable_t *s);
  void (*writable)(amp_selectable_t *s);
  time_t (*tick)(amp_selectable_t *s, time_t now);
  void *context;
};

/* Impls */

amp_driver_t *amp_driver()
{
  amp_driver_t *o = malloc(sizeof(amp_driver_t));
  o->selectables = amp_list(16);
  return o;
}

void amp_driver_add(amp_driver_t *d, amp_selectable_t *s)
{
  amp_list_add(d->selectables, amp_from_ref(s));
  s->driver = d;
}

void amp_driver_remove(amp_driver_t *d, amp_selectable_t *s)
{
  amp_list_remove(d->selectables, amp_from_ref(s));
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
      fds = realloc(fds, n*sizeof(struct pollfd));
      nfds = n;
    }

    for (i = 0; i < n; i++)
    {
      amp_selectable_t *s = amp_to_ref(amp_list_get(d->selectables, i));
      fds[i].fd = s->fd;
      fds[i].events = (s->status & AMP_SEL_RD ? POLLIN : 0) |
        (s->status & AMP_SEL_WR ? POLLOUT : 0);
      fds[i].revents = 0;
      if (s->tick) {
        // XXX
        s->tick(s, 0);
      }
    }

    DIE_IFE(poll(fds, n, -1));

    for (i = 0; i < n; i++)
    {
      amp_selectable_t *s = amp_to_ref(amp_list_get(d->selectables, i));
      if (fds[i].revents & POLLIN)
        s->readable(s);
      if (fds[i].revents & POLLOUT)
        s->writable(s);
    }
  }

  free(fds);
}

amp_selectable_t *amp_selectable()
{
  amp_selectable_t *s = malloc(sizeof(amp_selectable_t));
  if (!s) return NULL;
  s->status = 0;
  s->wakeup = 0;
  s->context = NULL;
  return s;
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

void amp_selectable_engine_close(amp_selectable_t *sel)
{
  sel->status = 0;
  if (close(sel->fd) == -1)
    perror("close");
  amp_driver_remove(sel->driver, sel);
  free(sel->context);
}

struct amp_engine_ctx *amp_selectable_engine_read(amp_selectable_t *sel)
{
  struct amp_engine_ctx *ctx = sel->context;
  int n = recv(sel->fd, ctx->input + ctx->in_size, IO_BUF_SIZE - ctx->in_size, 0);

  if (n == 0) {
    printf("disconnected\n");
    amp_selectable_engine_close(sel);
    return NULL;
  } else {
    ctx->in_size += n;
  }

  return ctx;
}

void amp_selectable_engine_consume(struct amp_engine_ctx *ctx, int n)
{
  ctx->in_size -= n;
  memmove(ctx->input, ctx->input + n, ctx->in_size);
}

void amp_engine_readable_input(struct amp_engine_ctx *ctx)
{
  amp_engine_t *engine = ctx->engine;
  int n = amp_engine_input(engine, ctx->input, ctx->in_size);
  amp_selectable_engine_consume(ctx, n);
}

void amp_engine_readable(amp_selectable_t *sel)
{
  struct amp_engine_ctx *ctx = amp_selectable_engine_read(sel);
  if (ctx) amp_engine_readable_input(ctx);
}

void amp_engine_readable_hdr(amp_selectable_t *sel)
{
  struct amp_engine_ctx *ctx = amp_selectable_engine_read(sel);

  if (!ctx)
    return;

  if (ctx->in_size >= 8) {
    if (memcmp(ctx->input, "AMQP\x00\x01\x00\x00", 8)) {
      printf("header missmatch");
      amp_selectable_engine_close(sel);
    } else {
      amp_selectable_engine_consume(ctx, 8);
      sel->readable = &amp_engine_readable;
      amp_engine_readable_input(ctx);
    }
  }
}

void amp_engine_writable(amp_selectable_t *sel)
{
  struct amp_engine_ctx *ctx = sel->context;
  amp_engine_t *engine = ctx->engine;
  ssize_t n = amp_engine_output(engine, ctx->output + ctx->out_size, IO_BUF_SIZE - ctx->out_size);
  if (n < 0) {
    printf("internal error");
    amp_selectable_engine_close(sel);
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

time_t amp_selectable_engine_tick(amp_selectable_t *sel, time_t now)
{
  struct amp_engine_ctx *ctx = sel->context;
  amp_engine_t *engine = ctx->engine;
  return amp_engine_tick(engine, now);
}

amp_selectable_t *amp_selectable_engine(int sock, amp_connection_t *conn)
{
  amp_selectable_t *sel = amp_selectable();
  sel->fd = sock;
  sel->readable = &amp_engine_readable_hdr;
  sel->writable = &amp_engine_writable;
  sel->tick = &amp_selectable_engine_tick;
  sel->status = AMP_SEL_RD | AMP_SEL_WR;
  struct amp_engine_ctx *ctx = malloc(sizeof(struct amp_engine_ctx));
  ctx->engine = amp_engine_create(conn);
  ctx->in_size = 0;
  memmove(ctx->output, "AMQP\x00\x01\x00\x00", 8);
  ctx->out_size = 8;
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

amp_selectable_t *amp_connector(char *host, char *port, amp_connection_t *conn)
{
  struct addrinfo *addr;
  int code = getaddrinfo(host, port, NULL, &addr);
  if (code) {
    fprintf(stderr, "%s", gai_strerror(code));
    return NULL;
  }

  int sock = socket(AF_INET, SOCK_STREAM, getprotobyname("tcp")->p_proto);
  if (sock == -1)
    return NULL;

  if (connect(sock, addr->ai_addr, addr->ai_addrlen) == -1)
    return NULL;

  amp_selectable_t *s = amp_selectable_engine(sock, conn);

  printf("Connected to %s:%s\n", host, port);
  return s;
}

void do_accept(amp_selectable_t *s)
{
  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  socklen_t addrlen;
  int sock = accept(s->fd, (struct sockaddr *) &addr, &addrlen);
  if (sock == -1) {
    perror("accept");
  } else {
    char host[1024], serv[64];
    int code;
    if ((code = getnameinfo((struct sockaddr *) &addr, addrlen, host, 1024, serv, 64, 0))) {
      printf("getnameinfo: %s\n", gai_strerror(code));
      if (close(sock) == -1)
        perror("close");
    } else {
      printf("accepted from %s:%s\n", host, serv);
      amp_connection_t *conn = amp_connection_create();
      amp_selectable_t *a = amp_selectable_engine(sock, conn);
      a->status = AMP_SEL_RD | AMP_SEL_WR;
      amp_driver_add(s->driver, a);
    }
  }
}

amp_selectable_t *amp_acceptor(char *host, char *port)
{
  struct addrinfo *addr;
  int code = getaddrinfo(host, port, NULL, &addr);
  if (code) {
    fprintf(stderr, "%s", gai_strerror(code));
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

  amp_selectable_t *s = amp_selectable();
  s->fd = sock;
  s->readable = &do_accept;
  s->writable = NULL;
  s->status = AMP_SEL_RD;

  printf("Listening on %s:%s\n", host, port);
  return s;
}
