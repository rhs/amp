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
  int ctrl[2];//pipe for updating selectable status
  bool stopping;
};

struct amp_selectable_st {
  amp_driver_t *driver;
  int fd;
  int status;
  time_t wakeup;
  void (*readable)(amp_selectable_t *s);
  void (*writable)(amp_selectable_t *s);
  time_t (*tick)(amp_selectable_t *s, time_t now);
  void (*destroy)(amp_selectable_t *s);
  void *context;
};

/* Impls */

amp_driver_t *amp_driver()
{
  amp_driver_t *o = malloc(sizeof(amp_driver_t));
  if (o) {
    o->selectables = amp_list(16);
    o->ctrl[0] = 0;
    o->ctrl[1] = 0;
    o ->stopping = false;
  }
  return o;
}

void amp_driver_destroy(amp_driver_t *d)
{
  for (int i = 0; i < amp_list_size(d->selectables); i++)
  {
    amp_selectable_t *s = amp_to_ref(amp_list_get(d->selectables, i));
    amp_driver_remove(d, s);
  }
  amp_free_list(d->selectables);
  free(d);
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

  if (pipe(d->ctrl)) {
      perror("Can't create control pipe");
  }
  while (!d->stopping)
  {
    int n = amp_list_size(d->selectables);
    if (n == 0) break;
    if (n > nfds) {
      fds = realloc(fds, (n+1)*sizeof(struct pollfd));
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
    fds[n].fd = d->ctrl[0];
    fds[n].events = POLLIN;
    fds[n].revents = 0;

    DIE_IFE(poll(fds, n+1, -1));

    for (i = 0; i < n; i++)
    {
      amp_selectable_t *s = amp_to_ref(amp_list_get(d->selectables, i));
      if (fds[i].revents & POLLIN)
        s->readable(s);
      if (fds[i].revents & POLLOUT)
        s->writable(s);
    }

    if (fds[n].revents & POLLIN) {
      //clear the pipe
      char buffer[512];
      while (read(d->ctrl[0], buffer, 512) == 512);
    }
  }

  close(d->ctrl[0]);
  close(d->ctrl[1]);
  free(fds);
}

void amp_driver_stop(amp_driver_t *d)
{
  d->stopping = true;
  write(d->ctrl[1], "x", 1);
}

amp_selectable_t *amp_selectable()
{
  amp_selectable_t *s = malloc(sizeof(amp_selectable_t));
  if (!s) return NULL;
  s->status = 0;
  s->wakeup = 0;
  s->readable = NULL;
  s->writable = NULL;
  s->tick = NULL;
  s->destroy = NULL;
  s->context = NULL;
  return s;
}

void amp_selectable_destroy(amp_selectable_t *s)
{
  if (s->destroy) s->destroy(s);
  free(s);
}

void amp_selectable_set_context(amp_selectable_t *s, void* context)
{
    s->context = context;
}

void* amp_selectable_get_context(amp_selectable_t *s)
{
    return s->context;
}

void amp_selectable_set_readable(amp_selectable_t *s, void (*readable)(amp_selectable_t *s))
{
    s->readable = readable;
}

void amp_selectable_set_writable(amp_selectable_t *s, void (*writable)(amp_selectable_t *s))
{
    s->writable = writable;
}

void amp_selectable_set_tick(amp_selectable_t *s, time_t (*tick)(amp_selectable_t *s, time_t now))
{
    s->tick = tick;
}

#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#define IO_BUF_SIZE (4*1024)

struct amp_engine_ctx {
  amp_connection_t *connection;
  amp_transport_t *transport;
  int in_size;
  int out_size;
  char input[IO_BUF_SIZE];
  char output[IO_BUF_SIZE];
  void (*callback)(amp_connection_t*, void*);
  void *context;
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
  amp_transport_t *transport = ctx->transport;
  int n = amp_input(transport, ctx->input, ctx->in_size);
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
  amp_transport_t *transport = ctx->transport;
  ssize_t n = amp_output(transport, ctx->output + ctx->out_size, IO_BUF_SIZE - ctx->out_size);
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
    if (ctx->out_size)
      sel->status |= AMP_SEL_WR;
    else
      sel->status &= ~AMP_SEL_WR;
  }
}

time_t amp_selectable_engine_tick(amp_selectable_t *sel, time_t now)
{
  struct amp_engine_ctx *ctx = sel->context;
  time_t result = amp_tick(ctx->transport, now);
  if (ctx->callback) ctx->callback(ctx->connection, ctx->context);
  amp_engine_writable(sel);
  return result;
}

void amp_engine_destroy(amp_selectable_t *s)
{
  if (s->context) free(s->context);
}

amp_selectable_t *amp_selectable_engine(int sock, amp_connection_t *conn,
                                        void (*cb)(amp_connection_t*, void*), void* ctx)
{
  amp_selectable_t *sel = amp_selectable();
  sel->fd = sock;
  sel->readable = &amp_engine_readable_hdr;
  sel->writable = &amp_engine_writable;
  sel->destroy = &amp_engine_destroy;
  sel->tick = &amp_selectable_engine_tick;
  sel->status = AMP_SEL_RD | AMP_SEL_WR;
  struct amp_engine_ctx *sctx = malloc(sizeof(struct amp_engine_ctx));
  sctx->connection = conn;
  sctx->transport = amp_transport(conn);
  sctx->in_size = 0;
  memmove(sctx->output, "AMQP\x00\x01\x00\x00", 8);
  sctx->out_size = 8;
  sctx->callback = cb;
  sctx->context = ctx;
  sel->context = sctx;
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

amp_selectable_t *amp_connector(char *host, char *port, amp_connection_t *conn,
                                void (*cb)(amp_connection_t*, void*), void* ctx)
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

  amp_selectable_t *s = amp_selectable_engine(sock, conn, cb, ctx);

  printf("Connected to %s:%s\n", host, port);
  return s;
}

int amp_selectable_connect(amp_selectable_t *s, const char *host, const char *port)
{
  struct addrinfo *addr;
  int code = getaddrinfo(host, port, NULL, &addr);
  if (code) {
    fprintf(stderr, "%s", gai_strerror(code));
    return code;
  }

  int sock = socket(AF_INET, SOCK_STREAM, getprotobyname("tcp")->p_proto);
  if (sock == -1)
    return sock;

  code = connect(sock, addr->ai_addr, addr->ai_addrlen);
  if (code == -1)
    return code;

  s->fd = sock;
  s->status = AMP_SEL_RD | AMP_SEL_WR;
  printf("Connected to %s:%s\n", host, port);
  return 0;//OK
}

int amp_selectable_recv(amp_selectable_t *s, void* buffer, size_t size)
{
  return recv(s->fd, buffer, size, 0);
}

int amp_selectable_send(amp_selectable_t *s, void* buffer, size_t size)
{
  return send(s->fd, buffer, size, 0);
}

void amp_selectable_close(amp_selectable_t *sel)
{
  sel->status = 0;
  if (close(sel->fd) == -1)
    perror("close");
  amp_driver_remove(sel->driver, sel);
  free(sel->context);
}

void amp_selectable_set_status(amp_selectable_t *s, int flags)
{
    s->status = flags;
    write(s->driver->ctrl[1], "x", 1);
}

void do_accept(amp_selectable_t *s)
{
  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  socklen_t addrlen = sizeof(addr);
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
      amp_connection_t *conn = amp_connection();
      struct amp_engine_ctx *ctx = s->context;
      amp_selectable_t *a = amp_selectable_engine(sock, conn, ctx->callback, ctx->context);
      a->status = AMP_SEL_RD | AMP_SEL_WR;
      amp_driver_add(s->driver, a);
    }
  }
}

amp_selectable_t *amp_acceptor(char *host, char *port, void (*cb)(amp_connection_t*, void*), void* context)
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
  struct amp_engine_ctx *ctx = malloc(sizeof(struct amp_engine_ctx));
  ctx->callback = cb;
  ctx->context = context;
  s->context = ctx;

  printf("Listening on %s:%s\n", host, port);
  return s;
}
