#include "http_server.hpp"
#include "platform/app.hpp"
#include "platform/db.hpp"

#include <charconv>
#include <cstdint>
#include <exception>
#include <cstdlib>
#include <filesystem>
#ifndef _WIN32
#include <sys/stat.h>
#endif
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
#ifndef _WIN32
        ::umask(0077);
#endif
        platform::AppOptions options;
        const auto* demo = std::getenv("PLATFORM_CAREER_DEMO");
        const bool demo_mode = demo && std::string(demo) == "1";
        options.database_path = demo_mode ? ":memory:" : platform::default_database_path();
        if (options.database_path != ":memory:") {
            const auto parent = std::filesystem::path(options.database_path).parent_path();
            if (!parent.empty()) std::filesystem::create_directories(parent);
        }
        if (!demo_mode) {
            if (const auto* password = std::getenv("PLATFORM_ADMIN_PASSWORD")) options.bootstrap_password = password;
            options.require_bootstrap = true;
        }
        const auto app = platform::make_app(options);
        platform::serve(app, port);
    } catch (const std::exception& error) {
        std::cerr << "Server error: " << error.what() << '\n';
        return 1;
    }
}
