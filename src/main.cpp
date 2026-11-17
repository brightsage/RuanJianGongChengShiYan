#include "http_server.hpp"
#include "platform/app.hpp"

#include <charconv>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    std::uint16_t port = 8080;
    if (argc > 2 || (argc == 2 && [&] {
            const std::string arg = argv[1];
            unsigned value = 0;
            const auto result = std::from_chars(arg.data(), arg.data() + arg.size(), value);
            if (result.ec != std::errc{} || result.ptr != arg.data() + arg.size() ||
                value == 0 || value > 65535) return true;
            port = static_cast<std::uint16_t>(value);
            return false;
        }())) {
        std::cerr << "Usage: platform_server [port 1-65535]\n";
        return 2;
    }
    try {
        const auto app = platform::make_app();
        platform::serve(app, port);
    } catch (const std::exception& error) {
        std::cerr << "Server error: " << error.what() << '\n';
        return 1;
    }
}
