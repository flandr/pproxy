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

#include "util.h"

#include <event2/bufferevent.h>
#include <event2/thread.h>

namespace test {

void handle_request(evhttp_request *request, void *ctx) {
    EchoServer *server = reinterpret_cast<EchoServer*>(ctx);
    server->handleRequest(request);
}

void EchoServer::handleRequest(evhttp_request *request) {
    /*
    if (maxWriteSize_ > 0) {
        // Add a filtering bufferevent to clamp writes
        struct evhttp_connection* conn = evhttp_request_get_connection(request);
        bufferevent_set_max_single_write(bev, maxWriteSize_);
    }
    */
    evbuffer *output = evbuffer_new();

    switch (evhttp_request_get_command(request)) {
    case EVHTTP_REQ_GET:
        evbuffer_add_printf(output, "GET");
        break;
    case EVHTTP_REQ_PUT: {
        evbuffer_add_printf(output, "PUT");
        struct evbuffer *input = evhttp_request_get_input_buffer(request);
        if (evbuffer_get_length(input) > 0) {
            evbuffer_add_printf(output, " ");
            evbuffer_add_buffer(output, input);
        }
        break;
    }
    default:
        evbuffer_add_printf(output, "Unsupported method");
    }
    evhttp_send_reply(request, HTTP_OK, "OK", output);
    evbuffer_free(output);
}

void persist_callback(int, int16_t, void*) {
    // don't care
}

void EchoServer::start() {
    loop_ = std::thread([this]() -> void {
            auto event = event_new(base_, -1, EV_PERSIST, persist_callback,
                nullptr);
            timeval tv = {3600, 0};
            event_add(event, &tv);
            event_base_dispatch(base_);
            event_free(event);
        });
    struct evhttp_bound_socket *bound =
        evhttp_bind_socket_with_handle(server_, "127.0.0.1", 0);
    if (!bound) {
        throw std::runtime_error("Failed to bind");
    }

    struct sockaddr_in saddr;
    socklen_t len = sizeof(saddr);
    getsockname(evhttp_bound_socket_get_fd(bound),
        (struct sockaddr*) &saddr, &len);
    port_ = ntohs(saddr.sin_port);
}

void EchoServer::stop() {
    event_base_loopexit(base_, 0);
    if (loop_.joinable()) {
        loop_.join();
    }
}

int16_t EchoServer::port() {
    return port_;
}

EchoServer::EchoServer() : EchoServer(0) { }

EchoServer::EchoServer(int maxWriteSize) : base_(nullptr), server_(nullptr),
        port_(0), maxWriteSize_(maxWriteSize) {
#ifdef _WIN32
    evthread_use_windows_threads();
#else
    evthread_use_pthreads();
#endif
    base_ = event_base_new();
    server_ = evhttp_new(base_);
    evhttp_set_gencb(server_, handle_request, this);
}

EchoServer::~EchoServer() {
    stop();
    evhttp_free(server_);
    event_base_free(base_);
}

HttpClient::~HttpClient() {
}

HttpClient::HttpClient(std::string const& host, int16_t port)
        : host_(host), port_(port), proxyPort_(port) {
}

HttpClient::HttpClient(std::string const& host, int16_t port, int16_t proxyPort)
        : host_(host), port_(port), proxyPort_(proxyPort) {
}

void request_done(struct evhttp_request *request, void *ctx) {
    auto promise = reinterpret_cast<std::promise<
        std::pair<int, std::string>>*>(ctx);
    if (!request) {
        promise->set_exception(std::make_exception_ptr(
            std::runtime_error("Timeout on request")));
        return;
    }

    auto buffer = evhttp_request_get_input_buffer(request);
    size_t size = evbuffer_get_length(buffer);
    std::string body(reinterpret_cast<char*>(evbuffer_pullup(buffer, size)),
        size);
    promise->set_value(std::make_pair(
        evhttp_request_get_response_code(request), body));
}

std::string HttpClient::formatAbsoluteUri(std::string const& path) {
    return "http://" + host_ + ":" + std::to_string(port_) + path;
}

void HttpClient::execute(struct evhttp_request *request, std::string const& path,
        evhttp_cmd_type type) {
    struct event_base *base = event_base_new();
    if (!base) {
        throw std::runtime_error("Error creating base");
    }

    struct evhttp_connection *conn = evhttp_connection_base_new(base, nullptr,
        host_.c_str(), proxyPort_);
    if (!conn) {
        throw std::runtime_error("Error connecting");
    }

    if (-1 == evhttp_make_request(conn, request, type,
            formatAbsoluteUri(path).c_str())) {
        throw std::runtime_error("Request failed");
    }

    event_base_dispatch(base);

    // Leaks on throw, but this is for unit testing and I don't care
    evhttp_connection_free(conn);
    event_base_free(base);
}

std::pair<int, std::string> HttpClient::get(std::string const& path) {
    std::promise<decltype(this->get(path))> promise;
    auto result = promise.get_future();
    struct evhttp_request *req = evhttp_request_new(request_done, &promise);
    execute(req, path, EVHTTP_REQ_GET);
    return result.get();
}

std::pair<int, std::string> HttpClient::put(std::string const& path,
        std::string const& content) {
    std::promise<decltype(this->get(path))> promise;
    auto result = promise.get_future();
    struct evhttp_request *req = evhttp_request_new(request_done, &promise);
    struct evbuffer *output = evhttp_request_get_output_buffer(req);
    struct evbuffer *buf = evbuffer_new();
    evbuffer_add(buf, content.c_str(), content.size());
    evbuffer_add_buffer(output, buf);
    evbuffer_free(buf);
    execute(req, path, EVHTTP_REQ_PUT);
    return result.get();
}

} // test namespace
