#include "platform/app.hpp"

#include <stdexcept>
#include <string>

void require(bool condition) {
    if (!condition) throw std::runtime_error("app test failed");
}

int main() {
    auto app = platform::make_app();
    require(app.modules().size() == 6);
    require(app.handle({"GET", "/api/health", {}, {}}).body == "{\"status\":\"ok\"}");
    const auto catalog = app.handle({"GET", "/api/modules", {}, {}});
    require(catalog.status == 200);
    require(catalog.body.find("student-profile") != std::string::npos);
    require(catalog.body.find("\"routes\":[]") != std::string::npos);
    require(app.handle({"GET", "/api/modules/student-profile/example", {}, {}}).status == 404);

    app.add_module_route("student-profile", "POST", "/example",
                         [](const platform::Request& request) {
                             return platform::Response{201, "text/plain", request.body};
                         });
    auto response = app.handle({"POST", "/api/modules/student-profile/example", {}, "sample"});
    require(response.status == 201 && response.body == "sample");
    require(app.handle({"GET", "/api/modules/student-profile/example", {}, {}}).status == 404);
    require(app.handle({"GET", "/api/modules", {}, {}}).body.find(
                "\"path\":\"/api/modules/student-profile/example\"") != std::string::npos);

    app.add_module_route("student-profile", "GET", "/:student_id",
                         [](const platform::Request& request) {
                             return platform::Response{200, "text/plain", request.path_params.at("student_id") +
                                                                        "?" + request.query};
                         });
    response = app.handle({"GET", "/api/modules/student-profile/123", {}, {}, "term=1"});
    require(response.body == "123?term=1");
    require(app.handle({"GET", "/api/modules/student-profile/123/more", {}, {}}).status == 404);

    bool rejected = false;
    try {
        app.add_module_route("student-profile", "POST", "/example",
                             [](const platform::Request&) { return platform::Response{}; });
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected);
}
