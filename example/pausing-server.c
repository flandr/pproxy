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

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "pproxy/callbacks.h"
#include "pproxy/pproxy.h"

static void connectCallback(struct pproxy_connection_handle *handle) {
    struct timeval tv;
    struct tm *tm;
    char buf[64];

    gettimeofday(&tv, NULL);
    tm = localtime(&tv.tv_sec);

    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %Z", tm);
    fprintf(stderr, "%s: pausing post-CONNECT for 30 seconds...\n", buf);

    struct timeval pause = { 30, 0 };
    pproxy_conn_insert_pause(handle, &pause);
}

int main(int argc, char **argv) {
    struct pproxy *handle = 0;
    if (pproxy_init(&handle, "127.0.0.1", 31337)) {
        fprintf(stderr, "Failed to initialize pproxy\n");
        exit(1);
    }

    short port = 0;
    if (pproxy_get_port(handle, &port)) {
        fprintf(stderr, "Failed to retrieve bound port\n");
        exit(1);
    }

    printf("pproxy is listening on 127.0.0.1:%hu\n", port);

    struct pproxy_callbacks callbacks =  { NULL, connectCallback };

    pproxy_set_callbacks(handle, &callbacks);
    printf("\n---> each CONNECT will pause for 30 seconds <---\n");

    /* Well, now it's listening... */
    int rc = pproxy_start(handle);
    printf("pproxy exited with %d\n", rc);

    return rc;
}
