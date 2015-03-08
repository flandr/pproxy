/*
 * Copyright (c) 2015 Nathan Rosenblum <flander@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef PPROXY_INTERNAL_H_
#define PPROXY_INTERNAL_H_

#include <inttypes.h>
#include <stdio.h>

#include <event2/buffer.h>
#include <event2/event.h>
#include <http_parser.h>

#if !defined(NDEBUG)
#define log_debug(...) fprintf(stderr, __VA_ARGS__)
#else
#define log_debug(...) while (0) fprintf(stderr, __VA_ARGS__)
#endif

/* states that the server run loop can be in */
enum proxy_server_state { PROXY_INIT, PROXY_RUNNING, PROXY_TERMINATED };

struct pproxy {
    int16_t port;
    struct event_base *base;
    struct evdns_base *dns_base;
    struct evconnlistener *listener;
    int run_state;
};

struct conn_handle;

enum pproxy_connection_state {
    /* initial state, receiving data */
    CONN_RECV,
    /* connecting state */
    CONN_CONNECTING,
    /* receiving data and forwarding */
    CONN_RECV_FORWARD,
    /* completely received message, just forwarding */
    CONN_FORWARD,
    /* completely received response */
    CONN_COMPLETE,
    /* direct (pass through) mode, parsing remaining HTTP request */
    CONN_DIRECT_PARSING,
    /* direct (pass through) mode */
    CONN_DIRECT,
};

/* source side of the proxy connection */
struct pproxy_source_state {
    struct bufferevent *bev;
    struct evbuffer *buffer;
    size_t peek_offset;
    struct http_parser parser;
    struct http_parser_settings parser_settings;
};

/* target side of the proxy connection */
struct pproxy_target_state {
    struct bufferevent *bev;
    struct http_parser parser;
    struct http_parser_settings parser_settings;
};

/* proxy connection */
struct pproxy_connection {
    struct pproxy *handle;
    enum pproxy_connection_state state;
    struct pproxy_source_state source_state;
    struct pproxy_target_state target_state;
};

int pproxy_connection_init(struct pproxy *handle, int fd,
    struct pproxy_connection **conn);
void pproxy_connection_free(struct pproxy_connection *conn);

#endif /* PPROXY_INTERNAL_H_ */
