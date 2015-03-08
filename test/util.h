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

#ifndef TEST_UTIL_H_
#define TEST_UTIL_H_

#include <future>
#include <string>
#include <thread>
#include <utility>

#include <evhttp.h>

namespace test {

template<typename R, typename Task>
std::future<R> runAsync(Task &&task) {
    std::packaged_task<R()> packaged(std::move(task));
    auto future = packaged.get_future();
    std::thread(std::move(packaged)).detach(); // Off you go, child!
    return std::move(future);
}

// Returns "GET" for GET, "PUT <body>" for PUT
class EchoServer {
public:
    EchoServer();
    explicit EchoServer(int maxWriteSize);
    ~EchoServer();
    void start();
    void stop();
    int16_t port();

    void handleRequest(evhttp_request *);
private:
    struct event_base *base_;
    struct evhttp *server_;
    int16_t port_;
    int maxWriteSize_;
    std::thread loop_;
};

class HttpClient {
public:
    HttpClient(std::string const& host, int16_t port);
    HttpClient(std::string const& host, int16_t port, int16_t proxyPort);
    ~HttpClient();
    std::pair<int, std::string> get(std::string const& path);
    std::pair<int, std::string> put(std::string const& path,
        std::string const& content);
private:
    std::string formatAbsoluteUri(std::string const& path);
    void execute(struct evhttp_request *request, std::string const& path,
        evhttp_cmd_type type);

    std::string host_;
    uint16_t port_;
    uint16_t proxyPort_;
};

} // test namespace

#endif // TEST_UTIL_H_

