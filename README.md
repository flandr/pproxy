Pico Proxy
==========

[![Build Status](https://travis-ci.org/flandr/pproxy.svg?branch=master)](https://travis-ci.org/flandr/pproxy)

Pico Proxy is a tiny-featured HTTP proxy server library. It is intended for use
in environments where a programmatically controllable proxy server is needed
(e.g. to test HTTP proxy client code). Don't expect too much out of it.

Refer to the [example server](example/server.c) for sample usage.

Building
--------

    mkdir build
    cd build
    cmake ..
    make

Future work
-----------

 - Connection upgrade support
 - Connection reuse / persistent connections

License
-------

Copyright (c) 2015 Nate Rosenblum <flander@gmail.com>

Licensed under the MIT License.
