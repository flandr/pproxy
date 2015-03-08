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

#if !defined(_WIN32)
#include <arpa/inet.h>
#endif

#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <event2/bufferevent.h>
#include <event2/dns.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/thread.h>

#include "pproxy/pproxy.h"
#include "pproxy-internal.h"

static int get_state(struct pproxy *handle) {
    // TODO: cross platform / use modern C11 atomic intrinsics
    __sync_synchronize();
    return handle->run_state;
}

static int terminated(struct pproxy *handle) {
    return get_state(handle) == PROXY_TERMINATED;
}

static void listener_cb(struct evconnlistener* listener, int fd,
        struct sockaddr *saddr, int saddr_len, void *ctx) {
    struct pproxy *handle = (struct pproxy*) ctx;

    struct pproxy_connection *conn = NULL;
    if (-1 == pproxy_connection_init(handle, fd, &conn)) {
        // TODO: error reporting, obv.
        close(fd);
        return;
    }
}

int pproxy_init(struct pproxy **handle, const char *bind_address,
        int16_t port) {
    /* le sigh */
#ifdef _WIN32
    evthread_use_windows_threads();
#else
    evthread_use_pthreads();
#endif

    if (!handle) {
        return -1;
    }

    if (!bind_address) {
        return -1;
    }

    struct pproxy *ret = (struct pproxy*) malloc(sizeof(struct pproxy));
    if (!ret) {
        return -1;
    }
    memset(ret, 0, sizeof(*ret));

    ret->run_state = PROXY_INIT;

    int fd = -1;
    for (;;) {
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd == -1) {
            break;
        }

        int rc = evutil_make_socket_nonblocking(fd);
        if (rc == -1) {
            break;
        }

        rc = evutil_make_listen_socket_reuseable(fd);
        if (rc == -1) {
            break;
        }

        struct sockaddr_in saddr;
        saddr.sin_family = AF_INET;
        rc = inet_pton(AF_INET, bind_address, &saddr.sin_addr);
        if (rc != 1) {
            break;
        }

        saddr.sin_port = htons(port);
        rc = bind(fd, (struct sockaddr*) &saddr, sizeof(saddr));
        if (rc == -1) {
            break;
        }

        /* extract bound port */
        socklen_t len = sizeof(saddr);
        rc = getsockname(fd, (struct sockaddr*) &saddr, &len);
        if (rc == -1) {
            break;
        }
        ret->port = ntohs(saddr.sin_port);

        /* construct an event base */
        ret->base = event_base_new();
        if (!ret->base) {
            break;
        }

        /* construct a DNS lookup base */
        ret->dns_base = evdns_base_new(ret->base, /*initialize=*/ 1);
        if (!ret->dns_base) {
            break;
        }

        /* set up a connection listener */
        ret->listener = evconnlistener_new(ret->base,
            listener_cb, ret, LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, -1,
            fd);
        if (!ret->listener) {
            break;
        }
        fd = -1; /* listener owns the socket now */

        *handle = ret;
        return 0;
    }

    /* cleanup on error */

    pproxy_free(ret);
    if (fd != -1) {
        close(fd);
    }
    return -1;
}

void pproxy_free(struct pproxy *handle) {
    if (!handle) {
        return;
    }

    if (handle->listener) {
        evconnlistener_free(handle->listener);
    }

    if (handle->dns_base) {
        evdns_base_free(handle->dns_base, /*fail requests=*/ 1);
    }

    if (handle->base) {
        event_base_free(handle->base);
    }

    free(handle);
}

int pproxy_get_port(struct pproxy *handle, int16_t *port) {
    if (!handle) {
        return -1;
    }

    if (!port) {
        return -1;
    }

    if (terminated(handle)) {
        return -1;
    }

    *port = handle->port;
    return 0;
}

int pproxy_running(struct pproxy *handle) {
    if (!handle) {
        return 0;
    }
    return get_state(handle) == PROXY_RUNNING;
}

int pproxy_start(struct pproxy *handle) {
    if (!handle) {
        return -1;
    }

    if (pproxy_running(handle)) {
        return -1;
    }

    handle->run_state = PROXY_RUNNING;
    __sync_synchronize();

    /* Run the event loop until interrupted. We loop to guard against
       premature termination of some event dispatch backends. For example, the
       Windows select-based backend may terminate if network interfaces become
       available (select can exit with WSAENETDOWN). */
    do {
        event_base_dispatch(handle->base);
    } while (!terminated(handle));

    return 0;
}

static void terminate(struct pproxy *handle) {
    // TODO: cross platform / use modern C11 atomic intrinsics
    handle->run_state = PROXY_TERMINATED;
    __sync_synchronize();
}

void pproxy_stop(struct pproxy *handle) {
    if (!handle) {
        return;
    }
    terminate(handle);
    event_base_loopexit(handle->base, 0);
}
