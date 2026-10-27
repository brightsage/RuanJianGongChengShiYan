#include "http_server.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace platform {
namespace {
constexpr std::size_t max_header_bytes = 16 * 1024;
constexpr std::size_t max_body_bytes = 1024 * 1024;

#ifdef _WIN32
using socket_handle = SOCKET;
constexpr socket_handle invalid_socket = INVALID_SOCKET;
#else
using socket_handle = int;
constexpr socket_handle invalid_socket = -1;
#endif

#ifdef MSG_NOSIGNAL
constexpr int send_flags = MSG_NOSIGNAL;
#else
constexpr int send_flags = 0;
#endif

std::string status_text(int status) {
    switch (status) {
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        default: return "Response";
    }
}

bool interrupted() {
#ifdef _WIN32
    return WSAGetLastError() == WSAEINTR;
#else
    return errno == EINTR;
#endif
}

void close_socket(socket_handle fd) {
#ifdef _WIN32
    ::closesocket(fd);
#else
    ::close(fd);
#endif
}

#ifdef _WIN32
class WinsockGuard {
public:
    WinsockGuard() {
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
            throw std::runtime_error("WSAStartup failed");
    }
    ~WinsockGuard() { WSACleanup(); }
    WinsockGuard(const WinsockGuard&) = delete;
    WinsockGuard& operator=(const WinsockGuard&) = delete;
};
#endif

void send_all(socket_handle fd, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        const int count = ::send(fd, data.data() + sent,
                                 static_cast<int>(data.size() - sent), send_flags);
        if (count < 0 && interrupted()) continue;
        if (count <= 0) return;
        sent += static_cast<std::size_t>(count);
    }
}

void send_response(socket_handle fd, const Response& response) {
    std::string head = "HTTP/1.1 " + std::to_string(response.status) + " " +
                       status_text(response.status) + "\r\n";
    head += "Content-Type: " + response.content_type + "\r\n";
    head += "Content-Length: " + std::to_string(response.body.size()) + "\r\n";
    for (const auto& [name, value] : response.headers) {
        if (name.find_first_of("\r\n:") == std::string::npos &&
            value.find_first_of("\r\n") == std::string::npos)
            head += name + ": " + value + "\r\n";
    }
    head += "Connection: close\r\nX-Content-Type-Options: nosniff\r\n\r\n";
    send_all(fd, head + response.body);
}

Response error_response(int status, const std::string& message) {
    return {status, "text/plain; charset=utf-8", message};
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

Response static_file(const std::string& path) {
    const char* filename = nullptr;
    const char* type = nullptr;
    if (path == "/") {
        filename = "index.html";
        type = "text/html; charset=utf-8";
    } else if (path == "/app.js") {
        filename = "app.js";
        type = "text/javascript; charset=utf-8";
    } else if (path == "/style.css") {
        filename = "style.css";
        type = "text/css; charset=utf-8";
    } else {
        return error_response(404, "asset not found");
    }
    const std::string file_path = std::string(PLATFORM_WEB_ROOT) + "/" + filename;
    std::ifstream file(file_path, std::ios::binary);
    if (!file) return error_response(500, "web asset unavailable");
    return {200, type, std::string(std::istreambuf_iterator<char>(file), {})};
}

Response receive_and_handle(socket_handle fd, const App& app) {
    std::string raw;
    char buffer[4096];
    std::size_t split = std::string::npos;
    while ((split = raw.find("\r\n\r\n")) == std::string::npos) {
        if (raw.size() > max_header_bytes) return error_response(413, "headers too large");
        const auto count = ::recv(fd, buffer, static_cast<int>(sizeof(buffer)), 0);
        if (count <= 0) return error_response(400, "incomplete request");
        raw.append(buffer, static_cast<std::size_t>(count));
    }
    if (split > max_header_bytes) return error_response(413, "headers too large");
    const auto first_end = raw.find("\r\n");
    if (first_end == std::string::npos || first_end >= split)
        return error_response(400, "invalid request line");
    const std::string first = raw.substr(0, first_end);
    const auto first_space = first.find(' ');
    const auto second_space = first.find(' ', first_space == std::string::npos ? 0 : first_space + 1);
    if (first_space == std::string::npos || second_space == std::string::npos ||
        first.substr(second_space + 1) != "HTTP/1.1")
        return error_response(400, "invalid request line");
    Request request;
    request.method = first.substr(0, first_space);
    const auto target = first.substr(first_space + 1, second_space - first_space - 1);
    if (target.empty() || target[0] != '/') return error_response(400, "invalid request target");
    request.path = target.substr(0, target.find('?'));
    if (const auto question = target.find('?'); question != std::string::npos)
        request.query = target.substr(question + 1);
    std::size_t cursor = first_end + 2;
    while (cursor < split) {
        const auto end = raw.find("\r\n", cursor);
        if (end == std::string::npos || end > split)
            return error_response(400, "invalid headers");
        const auto colon = raw.find(':', cursor);
        if (colon == std::string::npos || colon >= end)
            return error_response(400, "invalid header");
        std::string name = lowercase(raw.substr(cursor, colon - cursor));
        std::size_t value_start = colon + 1;
        while (value_start < end && raw[value_start] == ' ') ++value_start;
        if (request.headers.count(name)) return error_response(400, "duplicate header");
        request.headers.emplace(std::move(name), raw.substr(value_start, end - value_start));
        cursor = end + 2;
    }
    if (request.headers.count("transfer-encoding"))
        return error_response(501, "transfer encoding unsupported");
    std::size_t length = 0;
    if (const auto it = request.headers.find("content-length"); it != request.headers.end()) {
        if (it->second.empty() || it->second.find_first_not_of("0123456789") != std::string::npos)
            return error_response(400, "invalid content length");
        try {
            length = std::stoull(it->second);
        } catch (const std::exception&) {
            return error_response(400, "invalid content length");
        }
    }
    if (length > max_body_bytes) return error_response(413, "body too large");
    request.body = raw.substr(split + 4);
    if (request.body.size() > length) request.body.resize(length);
    while (request.body.size() < length) {
        const auto count = ::recv(
            fd, buffer,
            static_cast<int>(std::min<std::size_t>(sizeof(buffer), length - request.body.size())),
            0);
        if (count <= 0) return error_response(400, "incomplete body");
        request.body.append(buffer, static_cast<std::size_t>(count));
    }
    if (request.path == "/" || request.path == "/app.js" || request.path == "/style.css") {
        if (request.method != "GET") return error_response(405, "method not allowed");
        return static_file(request.path);
    }
    return app.handle(request);
}
}  // namespace

void serve(const App& app, std::uint16_t port) {
#ifdef _WIN32
    WinsockGuard winsock;
#endif
    const socket_handle listener = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listener == invalid_socket) throw std::runtime_error("socket failed");
    int reuse = 1;
    ::setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char*>(&reuse), sizeof(reuse));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    if (::bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 ||
        ::listen(listener, 16) < 0) {
        close_socket(listener);
        throw std::runtime_error("bind or listen failed");
    }
    std::cout << "Open http://127.0.0.1:" << port << "/\n" << std::flush;
    while (true) {
        const socket_handle client = ::accept(listener, nullptr, nullptr);
        if (client == invalid_socket) {
            if (interrupted()) continue;
            close_socket(listener);
            throw std::runtime_error("accept failed");
        }
#ifdef _WIN32
        DWORD timeout = 5000;
        ::setsockopt(client, SOL_SOCKET, SO_RCVTIMEO,
                     reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
        timeval timeout{5, 0};
        ::setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif
        try {
            send_response(client, receive_and_handle(client, app));
        } catch (const std::exception& error) {
            send_response(client, error_response(500, error.what()));
        }
        close_socket(client);
    }
}
}  // namespace platform
