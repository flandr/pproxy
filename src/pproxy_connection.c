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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <event2/buffer.h>
#include <event2/bufferevent.h>

#include "pproxy-internal.h"

static void connect_event_cb(struct bufferevent *bev, int16_t what, void *ctx);
static void target_event_cb(struct bufferevent *bev, int16_t what, void *ctx);
static void source_event_cb(struct bufferevent *bev, int16_t what, void *ctx);

static void target_read_cb(struct bufferevent *be, void *ctx);
static void source_read_cb(struct bufferevent *be, void *ptr);

static void direct_source_event_cb(struct bufferevent *, int16_t, void *);
static void direct_source_read_cb(struct bufferevent *, void *);
static void direct_target_event_cb(struct bufferevent *, int16_t, void *);
static void direct_target_read_cb(struct bufferevent *bev, void *ctx);

static int url_cb(struct http_parser *parser, const char *data, size_t len);
static int source_message_complete(struct http_parser *parser);
static int target_message_complete(struct http_parser *parser);

static void drive_request(struct pproxy_connection *conn);

static void free_source_state(struct pproxy_source_state *source) {
    if (source->buffer) {
        evbuffer_free(source->buffer);
        source->buffer = 0;
    }
    if (source->bev) {
        bufferevent_free(source->bev);
        source->bev = 0;
    }
}

static void free_target_state(struct pproxy_target_state *target) {
    if (target->bev) {
        bufferevent_free(target->bev);
        target->bev = 0;
    }
}

static const struct http_parser_settings source_parser_settings = {
    0, /* on_message_begin */
    url_cb,
    0, /* on_status_complete */
    0, /* on_header_field */
    0, /* on_header_value */
    0, /* receive_headers_complete */
    0, /* receive_body */
    source_message_complete
};

static void reset_source_state(struct pproxy_source_state *source) {
    http_parser_init(&source->parser, HTTP_REQUEST);
    source->parser_settings = source_parser_settings;
}

static int init_source_state(struct pproxy_source_state *source,
        struct pproxy_connection *conn, int fd) {
    memset(source, 0, sizeof(*source));

    /* Ensure nonblocking */
    if (evutil_make_socket_nonblocking(fd)) {
        return -1;
    }

    reset_source_state(source);
    source->parser.data = conn;

    for (;;) {
        source->bev = bufferevent_socket_new(conn->handle->base, fd,
            BEV_OPT_CLOSE_ON_FREE);
        if (!source->bev) {
            break;
        }

        source->buffer = evbuffer_new();
        if (!source->buffer) {
            break;
        }

        return 0;
    }

    /* Error path cleanup */
    if (source->bev) {
        bufferevent_free(source->bev);
        source->bev = 0;
    }

    if (source->buffer) {
        evbuffer_free(source->buffer);
    }

    return -1;
}

static const struct http_parser_settings target_parser_settings = {
    0, /* on_message_begin */
    0, /* on_url */
    0, /* on_status_complete */
    0, /* on_header_field */
    0, /* on_header_value */
    0, /* receive_headers_complete */
    0, /* receive_body */
    target_message_complete
};

static int init_target_state(struct pproxy_target_state *target,
        struct pproxy_connection *conn, struct bufferevent *bev) {
    memset(target, 0, sizeof(*target));

    http_parser_init(&target->parser, HTTP_RESPONSE);
    target->parser.data = conn;
    target->parser_settings = target_parser_settings;
    target->bev = bev;

    return 0;
}

void pproxy_connection_free(struct pproxy_connection *conn) {
    if (!conn) {
        return;
    }

    free_source_state(&conn->source_state);
    free_target_state(&conn->target_state);

    free(conn);
}

/*
 * Initializes or resets the connection structures to begin handling
 * a new request.
 *
 *  - request_state.buffer allocated or reset
 *  - request_state.peek_offset = 0
 *  - request_state.parser reset
 *  - request_state.settings = receive_settings
 *
 * The bufferevent callbacks are set to the receive-handling callbacks.
 */
static int set_connection_state_recv(struct pproxy_connection *conn) {
    reset_source_state(&conn->source_state);

    conn->state = CONN_RECV;

    bufferevent_setcb(conn->source_state.bev, source_read_cb, /*write_cb=*/ 0,
        source_event_cb, conn);

    /* enable read and write callbacks on the source bufferevent */
    bufferevent_enable(conn->source_state.bev, EV_READ | EV_WRITE);

    return 0;
}

static int set_connection_state_connecting(struct pproxy_connection *conn,
        const char *host, uint16_t port) {
    assert(conn->state == CONN_RECV);
    conn->state = CONN_CONNECTING;

    /* disable callbacks on the source bufferevent */
    bufferevent_disable(conn->source_state.bev, EV_READ);

    struct bufferevent *bev = bufferevent_socket_new(conn->handle->base, -1,
        BEV_OPT_CLOSE_ON_FREE);
    if (!bev) {
        return -1;
    }
    bufferevent_setcb(bev, /*read_cb=*/ 0, /*write_cb=*/ 0, connect_event_cb,
        conn);
    bufferevent_enable(bev, EV_READ | EV_WRITE);

    /* Start connecting */
    return bufferevent_socket_connect_hostname(bev, conn->handle->dns_base,
        AF_UNSPEC, host, port);
}

/*
 * Transition to receive & forward mode.
 *
 * - response_state.bev connected
 * - response_state
 *
 * No state changes.
 */
static int set_connection_state_recv_forward(struct pproxy_connection *conn,
        struct bufferevent *bev) {
    assert(conn->state == CONN_CONNECTING);

    if (conn->target_state.bev && conn->target_state.bev != bev) {
        bufferevent_free(conn->target_state.bev);
    }

    init_target_state(&conn->target_state, conn, bev);
    conn->state = CONN_RECV_FORWARD;

    http_parser_pause(&conn->source_state.parser, 0);

    /* turn on the read callback on the target bufferevent */
    bufferevent_setcb(bev, target_read_cb, /*write_cb=*/ 0, target_event_cb,
        conn);

    /* enable read and write callbacks on the source bufferevent */
    bufferevent_enable(conn->source_state.bev, EV_READ | EV_WRITE);

    return 0;
}

/*
 * Transition to forward-only mode.
 *
 * - settings = forward_settings
 */
static int set_connection_state_forward(struct pproxy_connection *conn) {
    assert(conn->state == CONN_RECV_FORWARD);
    conn->state = CONN_FORWARD;

    return 0;
}

static int set_connection_state_complete(struct pproxy_connection *conn) {
    assert(conn->state == CONN_FORWARD);
    conn->state = CONN_COMPLETE;

    bufferevent_disable(conn->target_state.bev, EV_READ | EV_WRITE);

    return 0;
}

static int set_connection_state_direct_parsing(struct pproxy_connection *conn,
        struct bufferevent *bev) {
    assert(conn->state == CONN_CONNECTING);
    assert(conn->target_state.bev == 0);

    conn->state = CONN_DIRECT_PARSING;
    conn->target_state.bev = bev;

    http_parser_pause(&conn->source_state.parser, 0);

    return 0;
}

static int set_connection_state_direct(struct pproxy_connection *conn) {
    assert(conn->state == CONN_DIRECT_PARSING);
    assert(conn->target_state.bev != 0);

    conn->state = CONN_DIRECT;

    bufferevent_setcb(conn->source_state.bev, direct_source_read_cb,
        /*write_cb=*/ 0, direct_source_event_cb, conn);
    bufferevent_enable(conn->source_state.bev, EV_READ | EV_WRITE);
    bufferevent_setcb(conn->target_state.bev, direct_target_read_cb,
        /*write_cb=*/ 0, direct_target_event_cb, conn);
    bufferevent_enable(conn->target_state.bev, EV_READ | EV_WRITE);

    return 0;
}

/* Connects to the target host and initializes transfer structures. */
static int set_connection_target(struct pproxy_connection *conn,
        const char *host, size_t host_len, uint16_t port) {
    /* Oof, stomping this memory temporarily. We know that this is safe to
     * do because the format of the request line requires that there be
     * additional data following the host portion of the url. */
    char *tmphost = (char *) host;
    char save = tmphost[host_len];
    tmphost[host_len] = '\0';
    int rc = set_connection_state_connecting(conn, tmphost, port);
    tmphost[host_len] = save;

    return rc;
}

static int target_message_complete(struct http_parser *parser) {
    struct pproxy_connection *conn = (struct pproxy_connection*) parser->data;

    /* TODO: handle keep-alive? */

    if (conn->state == CONN_FORWARD) {
        /* Source is done */
        set_connection_state_complete(conn);
        http_parser_pause(parser, 1);
    } else {
        /* Otherwise this is a PUT w/ Expect: 100-continue, probably */
        assert(conn->state == CONN_RECV_FORWARD);
    }

    return 0;
}

static int source_message_complete(struct http_parser *parser) {
    struct pproxy_connection *conn = (struct pproxy_connection*) parser->data;

    switch (conn->state) {
    case CONN_RECV_FORWARD:
        set_connection_state_forward(conn);
        break;
    case CONN_DIRECT_PARSING:
        set_connection_state_direct(conn);
        break;
    default:
        assert(0 && "Unexpected state in message_complete_cb");
    }

    /* We're at the end of the message. Pause the parser so that we
     * get control back in the driver loop. TODO: just return HPE_PAUSED? */
    http_parser_pause(parser, 1);
    return 0;
}

static int url_cb(struct http_parser *parser, const char *data, size_t len) {
    struct pproxy_connection *conn = (struct pproxy_connection*) parser->data;

    /* TODO: the http_parser implementation only invokes on_url once, but
     * this does not appear to be required from the callback documentation
     * example. Figure out whether we can depend on this. */
    struct http_parser_url url;
    int rc = http_parser_parse_url(data, len, parser->method == HTTP_CONNECT,
        &url);
    if (rc != 0) {
        log_debug("While parsing url %.*s: %s\n", (int) len, data,
            http_errno_description((enum http_errno) parser->http_errno));
        return rc;
    }

    if (!(url.field_set & (1 << UF_HOST))) {
        log_debug("No host in url %.*s\n", (int) len, data);
        return -1;
    }

    uint16_t port = 80;
    if (url.field_set & (1 << UF_PORT)) {
        /* http_parser checks that this is numeric */
        port = atoi(&data[url.field_data[UF_PORT].off]);
    }

    log_debug("%s %.*s:%hu\n",
        http_method_str((enum http_method) parser->method),
        url.field_data[UF_HOST].len, &data[url.field_data[UF_HOST].off], port);

    /* Set the connection target and maybe start connecting to it */
    rc = set_connection_target(conn, &data[url.field_data[UF_HOST].off],
        url.field_data[UF_HOST].len, port);
    if (rc != 0) {
        log_debug("Failed to start connection\n");
        return rc;
    }

    /* pause parser execution */
    http_parser_pause(parser, 1);

    return 0;
}

static void connect_event_cb(struct bufferevent *bev, int16_t what, void *ctx) {
    struct pproxy_connection *conn = (struct pproxy_connection*) ctx;

    if (what & BEV_EVENT_CONNECTED) {
        switch (conn->source_state.parser.method) {
        case HTTP_CONNECT:
            set_connection_state_direct_parsing(conn, bev);
            break;
        default:
            set_connection_state_recv_forward(conn, bev);
        }

        /* Run an iteration of the driver for anything buffered */
        drive_request(conn);
    } else if (what & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        log_debug("While connecting to remote host: ");
        if (what & BEV_EVENT_ERROR) {
            int dnserr = bufferevent_socket_get_dns_error(bev);
            if (dnserr) {
                log_debug("DNS error %s\n", evutil_gai_strerror(dnserr));
            } else {
                log_debug("unknown error\n");
            }
        } else {
            log_debug("connection closed\n");
        }
        /* An error during connect means we have to free the bufferevent */
        assert(conn->target_state.bev != 0);
        bufferevent_free(bev);
        pproxy_connection_free(conn);
    }
}

static int move_buffers(struct bufferevent *src, struct bufferevent *dst) {
    return bufferevent_write_buffer(dst, bufferevent_get_input(src));
}

static void target_event_cb(struct bufferevent *bev, int16_t what, void *ctx) {
    (void) bev;

    if (what & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        struct pproxy_connection *conn = (struct pproxy_connection *) ctx;
        pproxy_connection_free(conn);
    } else {
        /* no other events are expected */
        assert(0 && "unexpected event on target channel");
    }
}

static void source_last_write_cb(struct bufferevent *bev, void *ctx) {
    (void) bev;

    struct pproxy_connection *conn = (struct pproxy_connection*) ctx;
    pproxy_connection_free(conn);
}

static int is_http_error(struct http_parser *parser) {
    switch (parser->http_errno) {
    case HPE_OK:
    case HPE_PAUSED:
        return 0;
    default:
        log_debug("HTTP parsing error %s: %s\n",
            http_errno_name((enum http_errno) parser->http_errno),
            http_errno_description((enum http_errno) parser->http_errno));
        return 1;
    }
}

static void target_read_cb(struct bufferevent *be, void *ctx) {
    struct pproxy_connection *conn = (struct pproxy_connection*) ctx;
    struct evbuffer *buffer = bufferevent_get_input(be);

    struct evbuffer_iovec extents[1];
    struct evbuffer_ptr peek;
    evbuffer_ptr_set(buffer, &peek, 0, EVBUFFER_PTR_SET);

    int eavail = evbuffer_peek(buffer, -1, &peek, extents, 1);
    if (!eavail) {
        assert(0 && "event read callback with empty buffer");
        return;
    }

    do {
        size_t parsed = http_parser_execute(&conn->target_state.parser,
            &conn->target_state.parser_settings, (char *) extents[0].iov_base,
            extents[0].iov_len);

        if (is_http_error(&conn->source_state.parser)) {
            pproxy_connection_free(conn);
            return;
        }

        if (parsed < extents[0].iov_len) {
            if (conn->state != CONN_COMPLETE) {
                pproxy_connection_free(conn);
                return;
            } else {
                /* XXX This is an exception condition in the current
                 * implementation; should not see extraneous data from the
                 * server. */
            }
        }

        /* Advance to determine if this was a single extent. XXX Is this cheaper
         * than checking the length of the evbuffer beforehand? */
        struct evbuffer_iovec cur = extents[0];
        int rc = evbuffer_ptr_set(buffer, &peek, parsed, EVBUFFER_PTR_ADD);
        if (!rc) {
            eavail = evbuffer_peek(buffer, -1, &peek, extents, 1);
        } else {
            eavail = 0;
        }
        if (!eavail && parsed == cur.iov_len) {
            /* Common case: only a single extent and totally consumed */
            rc = bufferevent_write_buffer(conn->source_state.bev, buffer);
        } else {
            rc = bufferevent_write(conn->source_state.bev, cur.iov_base,
                parsed);
            evbuffer_drain(buffer, parsed);
        }

        if (rc) {
            log_debug("Error forwarding to proxy client\n");
            break;
        }

        if (conn->state == CONN_COMPLETE) {
            break;
        }
    } while (eavail);

    if (conn->state == CONN_COMPLETE) {
        /* Register for state transitions post-write */
        bufferevent_setcb(conn->source_state.bev, 0, source_last_write_cb,
            source_event_cb, conn);
    }
}

static void direct_source_event_cb(struct bufferevent *bev, int16_t what,
        void *ctx) {
    (void) bev;

    if (what & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        struct pproxy_connection *conn = (struct pproxy_connection *) ctx;
        pproxy_connection_free(conn);
    } else {
        /* no other events are expected */
        assert(0 && "unexpected event on direct source channel");
    }
}

static void direct_source_read_cb(struct bufferevent *bev, void *ctx) {
    (void) bev;

    struct pproxy_connection *conn = (struct pproxy_connection*) ctx;
    if (move_buffers(conn->source_state.bev, conn->target_state.bev)) {
        log_debug("Error forwarding to direct proxy client\n");
        pproxy_connection_free(conn);
    }
}

static void direct_target_event_cb(struct bufferevent *bev, int16_t what,
        void *ctx) {
    (void) bev;

    if (what & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        struct pproxy_connection *conn = (struct pproxy_connection *) ctx;
        pproxy_connection_free(conn);
    } else {
        /* no other events are expected */
        assert(0 && "unexpected event on direct target channel");
    }
}

static void direct_target_read_cb(struct bufferevent *bev, void *ctx) {
    (void) bev;

    struct pproxy_connection *conn = (struct pproxy_connection*) ctx;
    if (move_buffers(conn->target_state.bev, conn->source_state.bev)) {
        pproxy_connection_free(conn);
    }
}

static void send_direct_ok_response(struct pproxy_connection *conn) {
    static char kOk[] = "HTTP/1.1 200 Connection established\r\n\r\n";
    bufferevent_write(conn->source_state.bev, kOk, sizeof(kOk) - 1);
}

static void source_event_cb(struct bufferevent *bev, int16_t what, void *ctx) {
    (void) bev;

    if (what & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        struct pproxy_connection *conn = (struct pproxy_connection *) ctx;
        pproxy_connection_free(conn);
    } else {
        /* no other events are expected */
        assert(0 && "unexpected event on source channel");
    }
}

static int is_direct_state(enum pproxy_connection_state state) {
    return state == CONN_DIRECT_PARSING || state == CONN_DIRECT;
}

static size_t xplat_min(size_t x, size_t y) {
    return x > y ? y : x;
}

static size_t write_atmost(struct evbuffer *buffer, size_t len,
        struct bufferevent *bev) {
    size_t remain = len;
    while (remain > 0) {
        struct evbuffer_iovec extents[2];
        size_t cnt = evbuffer_peek(buffer, -1, 0, extents, 2);
        if (!cnt) {
            break;
        } else if (cnt == 1 && extents[0].iov_len <= remain) {
            /* Write without copying in the common case of a single extent */
            bufferevent_write_buffer(bev, buffer);
            remain -= extents[0].iov_len;
            break;
        }

        size_t orig = remain;
        int i = 0;
        for (; i < 2 && remain > 0; ++i) {
            size_t w = xplat_min(extents[i].iov_len, remain);
            bufferevent_write(bev, extents[0].iov_base, w);
            remain -= w;
        }
        evbuffer_drain(buffer, orig - remain);
    }
    return len - remain;
}

/* Drives the request. Invoked by the source read callback and after
 * connection, to push through data buffered during connection. */
static void drive_request(struct pproxy_connection *conn) {
    struct evbuffer *buffer = conn->source_state.buffer;

    if (conn->source_state.peek_offset > 0) {
        if (!is_direct_state(conn->state)) {
            size_t written = write_atmost(buffer,
                conn->source_state.peek_offset, conn->target_state.bev);
            assert(written == conn->source_state.peek_offset);
        } else {
            /* Just skip */
            evbuffer_drain(buffer, conn->source_state.peek_offset);
        }
        conn->source_state.peek_offset = 0;
    }

    struct evbuffer_ptr peek;
    evbuffer_ptr_set(buffer, &peek, conn->source_state.peek_offset,
        EVBUFFER_PTR_SET);

    /* We process the buffer contents one extent at a time */
    int loop = 1;
    int skip = 0;
    do {
        struct evbuffer_iovec extents[1];
        int eavail = evbuffer_peek(buffer, -1, &peek, extents, 1);
        if (!eavail) {
            break;
        }

        size_t parsed;
        if (conn->state != CONN_DIRECT) {
            parsed = http_parser_execute(&conn->source_state.parser,
                &conn->source_state.parser_settings,
                (char *) extents[0].iov_base, extents[0].iov_len);

            if (is_http_error(&conn->source_state.parser)) {
                pproxy_connection_free(conn);
                return;
            }

            /* If we just transitioned to direct on message complete, we need
             * to skip over the residual from the HTTP request */
            if (conn->state == CONN_DIRECT) {
                evbuffer_drain(buffer, parsed + skip);
                skip = 0;
                if (evbuffer_ptr_set(buffer, &peek, 0, EVBUFFER_PTR_SET)) {
                    break;
                }
                continue;
            }
        } else {
            parsed = extents[0].iov_len;
        }

        int write_data = 0;

        switch (conn->state) {
        case CONN_RECV:
            /* No url yet, so we continue to buffer. If we broke out of the
             * parser w/o finishing the current extent, this must have been
             * a parsing error */
            break;
        case CONN_CONNECTING:
            /* In progress connecting; we should not continue. The post-
             * connection callback will drain the rest of the buffer. */
            loop = 0;
            break;
        case CONN_RECV_FORWARD:
            write_data = 1;
            break;
        case CONN_FORWARD:
            /* We've hit message end, and need to forward anything left
             * in the buffer. The callback has been disabled. */
            write_data = 1;
            loop = 0;
            break;
        case CONN_DIRECT_PARSING:
            /* Still parsing a direct connection setup */
            break;
        case CONN_DIRECT:
            /* Parsing is over; another read handler has been installed,
             * but we need to drain the rest of the buffer. */
            write_data = 1;
            break;
        case CONN_COMPLETE:
            assert(0 && "Invalid state in request driver");
        }

        if (write_data) {
            // TODO: avoid this copying write when the buffer is a single extent
            bufferevent_write(conn->target_state.bev,
                extents[0].iov_base, parsed);
            /* Drain the buffer */
            evbuffer_drain(buffer, skip + parsed);
            int rc = evbuffer_ptr_set(buffer, &peek, 0, EVBUFFER_PTR_SET);
            if (rc) {
                /* buffer is empty */
                loop = 0;
            }
            skip = 0;
        } else {
            /* Advance locally, but don't advance in peek_offset. */
            int rc = evbuffer_ptr_set(buffer, &peek, parsed, EVBUFFER_PTR_ADD);
            if (rc) {
                /* buffer is empty */
                loop = 0;
            }
            /* Recall how much to skip on next invocation of drive_request */
            skip += parsed;
        }
    } while (loop);

    /* Advance the peek offset as far as we've processed */
    conn->source_state.peek_offset = skip;

    /* If this was a connect, return a 200 response */
    if (conn->state == CONN_DIRECT) {
        send_direct_ok_response(conn);
    }
}

static void source_read_cb(struct bufferevent *be, void *ptr) {
    struct pproxy_connection *conn = (struct pproxy_connection*) ptr;
    struct evbuffer *buffer = conn->source_state.buffer;

    if (bufferevent_read_buffer(be, buffer)) {
        log_debug("Error reading from proxy client\n");
        return;
    }

    drive_request(conn);
}

int pproxy_connection_init(struct pproxy *handle, int fd,
        struct pproxy_connection **conn) {
    if (!conn) {
        return -1;
    }

    struct pproxy_connection *ret = (struct pproxy_connection*) malloc(
        sizeof(struct pproxy_connection));
    if (!ret) {
        return -1;
    }
    memset(ret, 0, sizeof(*ret));

    ret->handle = handle;

    for (;;) {
        if (init_source_state(&ret->source_state, ret, fd)) {
            break;
        }

        set_connection_state_recv(ret);
        *conn = ret;
        return 0;
    }

    /* cleanup */

    free(ret);
    return -1;
}
