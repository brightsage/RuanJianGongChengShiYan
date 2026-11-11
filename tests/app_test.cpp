#include "platform/app.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>

namespace {

void require(bool condition, const std::string& message = "app test failed") {
    if (!condition) throw std::runtime_error(message);
}

std::string make_temp_db_path() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return (std::filesystem::temp_directory_path() /
            ("student_platform_app_test_" + std::to_string(stamp) + ".db"))
        .string();
}

void set_database_path(const std::string& path) {
#ifdef _WIN32
    _putenv_s("PLATFORM_DB_PATH", path.c_str());
#else
    setenv("PLATFORM_DB_PATH", path.c_str(), 1);
#endif
}

void remove_database_files(const std::string& path) {
    std::error_code error;
    std::filesystem::remove(path, error);
    std::filesystem::remove(path + "-wal", error);
    std::filesystem::remove(path + "-shm", error);
}

platform::Response handle(platform::App& app, const std::string& method,
                          const std::string& path, const std::string& body = {},
                          const std::string& query = {}) {
    platform::Request request;
    request.method = method;
    request.path = path;
    request.body = body;
    request.query = query;
    return app.handle(request);
}

}  // namespace

int main() {
    const std::string database_path = make_temp_db_path();
    set_database_path(database_path);
    try {
        auto app = platform::make_app();
        require(app.modules().size() == 6);
        require(handle(app, "GET", "/api/health").body == "{\"status\":\"ok\"}");
        const auto catalog = handle(app, "GET", "/api/modules");
        require(catalog.status == 200);
        require(catalog.body.find("student-profile") != std::string::npos);
        require(catalog.body.find("\"routes\":[]") != std::string::npos);
        require(catalog.body.find("/api/modules/data-ingestion/sources") != std::string::npos);
        require(catalog.body.find("/api/modules/student-profile/students") != std::string::npos);
        require(handle(app, "GET", "/api/modules/student-profile/example").status == 404);

        app.add_module_route("student-profile", "POST", "/example",
                             [](const platform::Request& request) {
                                 return platform::Response{201, "text/plain", request.body};
                             });
        auto response = handle(app, "POST", "/api/modules/student-profile/example", "sample");
        require(response.status == 201 && response.body == "sample");
        require(handle(app, "GET", "/api/modules/student-profile/example").status == 404);
        require(handle(app, "GET", "/api/modules").body.find(
                    "\"path\":\"/api/modules/student-profile/example\"") !=
                std::string::npos);

        app.add_module_route("student-profile", "GET", "/:student_id",
                             [](const platform::Request& request) {
                                 return platform::Response{
                                     200, "text/plain",
                                     request.path_params.at("student_id") + "?" + request.query};
                             });
        response = handle(app, "GET", "/api/modules/student-profile/123", {}, "term=1");
        require(response.body == "123?term=1");
        require(handle(app, "GET", "/api/modules/student-profile/123/more").status == 404);

        bool rejected = false;
        try {
            app.add_module_route(
                "student-profile", "POST", "/example",
                [](const platform::Request&) { return platform::Response{}; });
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        require(rejected);
    } catch (const std::exception&) {
        remove_database_files(database_path);
        throw;
    }
    remove_database_files(database_path);
}
