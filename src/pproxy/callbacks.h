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

#ifndef PPROXY_CALLBACKS_H_
#define PPROXY_CALLBACKS_H_

#include <sys/time.h>

#ifdef __cplusplus
extern "C" {
#endif

struct pproxy;
struct pproxy_connection_handle;

typedef void (*pproxy_general_cb)(struct pproxy_connection_handle *conn);

struct pproxy_callbacks {
    /* Fired on initial connection from a proxy client. */
    pproxy_general_cb on_connect;
    /* Fired when transitining to direct forwarding after CONNECT. */
    pproxy_general_cb on_direct_connect;
    /* Fired when transitioning to waiting for a server response. */
    pproxy_general_cb on_request_complete;
};

/**
 * Set or clear callbacks.
 *
 * @param handle the pproxy handle
 * @param callbacks the callbacks, or NULL to clear all callbacks
 * @return 0 on success, -1 on error
 */
int pproxy_set_callbacks(struct pproxy *handle,
    const struct pproxy_callbacks *callbacks);

/** @return the pproxy handle for this connection, or NULL. */
struct pproxy* pproxy_conn_get_pproxy(struct pproxy_connection_handle *handle);

/**
 * Insert a pause on the connection.
 *
 * The next action on the connection will be delayed by the provided
 * value. This method can be used to induce pauses on a specific connection
 * without preventing other connections from being serviced.
 */
void pproxy_conn_insert_pause(struct pproxy_connection_handle *handle,
    const struct timeval *tv);

#ifdef __cplusplus
}
#endif

#endif /* PPROXY_CALLBACKS_H_ */
