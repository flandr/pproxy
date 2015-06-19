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

#include "pproxy/callbacks.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "pproxy-internal.h"

int pproxy_set_callbacks(struct pproxy *handle,
        const struct pproxy_callbacks *callbacks) {
    if (!handle) {
        return -1;
    }

    if (!callbacks) {
        memset(&handle->callbacks, 0, sizeof(handle->callbacks));
        return 0;
    }

    handle->callbacks = *callbacks;

    return 0;
}

struct pproxy* pproxy_conn_get_proxy(struct pproxy_connection_handle *handle) {
    if (!handle) {
        return NULL;
    }
    return pproxy_cb_handle_connection(handle)->handle;
}

void pproxy_conn_insert_pause(struct pproxy_connection_handle *handle,
        const struct timeval *tv) {
    assert(handle);
    assert(tv);

    handle->delay = *tv;
}

int pproxy_connection_handle_init(struct pproxy_connection_handle *handle) {
    memset(handle, 0, sizeof(*handle));
    return 0;
}

void pproxy_connection_handle_free(struct pproxy_connection_handle *handle) {
    if (handle->timer) {
        event_del(handle->timer);
        event_free(handle->timer);
        handle->timer = NULL;
    }
}

int pproxy_connection_handle_has_delay(
        struct pproxy_connection_handle *handle) {
    if (!handle) {
        return 0;
    }
    return handle->delay.tv_sec != 0 || handle->delay.tv_usec != 0;
}
