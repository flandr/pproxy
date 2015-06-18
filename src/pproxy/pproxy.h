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

#ifndef PPROXY_PPROXY_H_
#define PPROXY_PPROXY_H_

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/** pproxy library handle. */
struct pproxy;

/**
 * Allocates and initializes a pproxy instance.
 *
 * The output handle must be released with @see pproxy_free.
 *
 * Passing 0 for the port will cause pproxy to bind to a random port. Use
 * @see pproxy_get_port to look up the bound port in this case.
 *
 * @param handle the allocated handle
 * @param bind_address a bind address, in dotted-quad notation
 * @param port the port to bind to
 * @return 0 on success, -1 on error
 */
int pproxy_init(struct pproxy **handle, const char *bind_address, int16_t port);

typedef void (*pproxy_general_cb)(struct pproxy *handle);

struct pproxy_callbacks {
    pproxy_general_cb on_connect;
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

/**
 * Releases the pproxy instance.
 *
 * @param handle a handle previously allocated with @see pproxy_init.
 */
void pproxy_free(struct pproxy *handle);

/**
 * Starts the proxy server running, bound to the requested host and port.
 *
 * This method will not return until the proxy server exits, typically by
 * invoking @see proxy_stop in another thread.
 *
 * @param handle the pproxy handle
 * @return -1 on error
 */
int pproxy_start(struct pproxy *handle);

/** Immediately stop the pproxy server, unbinding network resources. */
void pproxy_stop(struct pproxy *handle);

/**
 * Gets the port for a running pproxy server.
 *
 * The output value is undefined if @see pproxy is not running.
 *
 * @param handle the pproxy handle
 * @param port the port
 * @return 0 on success, -1 on error
 */
int pproxy_get_port(struct pproxy *handle, int16_t *port);

/** @return non-zero if the pproxy server is running. */
int pproxy_running(struct pproxy *handle);

#ifdef __cplusplus
}
#endif

#endif /* PPROXY_PPROXY_H_ */
