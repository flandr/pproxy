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

#include <chrono>
#include <thread>

#include <gtest/gtest.h>

extern "C" {
#include "pproxy/pproxy.h"
}

#include "util.h"

#define ASSERT_SUCCESS(rc) ASSERT_EQ(0, rc)

namespace test {

class PproxyTest : public ::testing::Test {
public:
    PproxyTest() : proxy_host("127.0.0.1"), handle(nullptr) { }

    virtual void SetUp() {
        ASSERT_SUCCESS(pproxy_init(&handle, proxy_host, 0));
        ASSERT_SUCCESS(pproxy_get_port(handle, &proxy_port));
    }

    ~PproxyTest() {
        pproxy_free(handle);
    }
protected:
    struct pproxy *handle;
    const char *proxy_host;
    int16_t proxy_port;
};

// Wrapper
class PproxyServer {
public:
    explicit PproxyServer(struct pproxy *handle) : handle(handle) { }

    void start() {
        server = std::thread([this]() -> void {
                pproxy_start(this->handle);
            });
        bool running = false;
        for (int i = 0; i < 100; ++i) {
            running = pproxy_running(handle);
            if (running) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!running) {
            throw std::runtime_error("Failed to start server");
        }
    }

    int16_t port() {
        int16_t ret = 0;
        if (pproxy_get_port(handle, &ret)) {
            throw std::runtime_error("Failed to query port");
        }
        return ret;
    }

    ~PproxyServer() {
        pproxy_stop(handle);
        if (server.joinable()) {
            server.join();
        }
    }
private:
    struct pproxy *handle;
    std::thread server;
};

TEST_F(PproxyTest, ProxyIsNotRunningAfterInit) {
    ASSERT_FALSE(pproxy_running(handle));
}

TEST_F(PproxyTest, PproxyIsRunningAfterStart) {
    auto result = runAsync<int>([this]() -> int {
            return pproxy_start(handle);
        });

    // Spin for a while waiting for it to come up. A callback would be nice.
    bool running = false;
    for (int i = 0; i < 100; ++i) {
        running = pproxy_running(handle);
        if (running) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_TRUE(running);

    pproxy_stop(handle);

    auto done = result.wait_for(std::chrono::seconds(1));
    ASSERT_EQ(std::future_status::ready, done);
}

TEST_F(PproxyTest, TestGet) {
    EchoServer echo;
    echo.start();

    PproxyServer proxy(handle);
    proxy.start();

    HttpClient echoClient("127.0.0.1", echo.port());
    auto eret = echoClient.get("");
    ASSERT_EQ(200, eret.first);

    HttpClient proxyClient("127.0.0.1", echo.port(), proxy.port());
    auto pret = proxyClient.get("");
    ASSERT_EQ(eret, pret);
}

TEST_F(PproxyTest, TestPut) {
    EchoServer echo;
    echo.start();

    PproxyServer proxy(handle);
    proxy.start();

    HttpClient echoClient("127.0.0.1", echo.port());
    auto eret = echoClient.put("", "zomg");
    ASSERT_EQ(200, eret.first);

    HttpClient proxyClient("127.0.0.1", echo.port(), proxy.port());
    auto pret = proxyClient.put("", "zomg");
    ASSERT_EQ(eret, pret);
}

} // test namespace
