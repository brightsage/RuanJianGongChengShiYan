#pragma once

#include <functional>
#include <map>
#include <memory>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace platform {

namespace security { class Security; }
struct AppOptions {
    std::string database_path = ":memory:";
    std::string bootstrap_username = "admin";
    std::string bootstrap_password;
    bool require_bootstrap = false;
    std::function<std::int64_t()> clock;
};

struct Request {
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    std::string body;
    std::string query;
    std::map<std::string, std::string> path_params;
};

struct Response {
    Response(int status = 200, std::string content_type = "application/json; charset=utf-8",
             std::string body = {}, std::map<std::string, std::string> headers = {})
        : status(status), content_type(std::move(content_type)), body(std::move(body)),
          headers(std::move(headers)) {}

    int status;
    std::string content_type;
    std::string body;
    std::map<std::string, std::string> headers;
};

using Handler = std::function<Response(const Request&)>;

struct RouteInfo {
    std::string method;
    std::string path;
};

struct ModuleInfo {
    std::string id;
    std::string name;
    std::vector<RouteInfo> routes;
};

class App {
public:
    void add_module(std::string id, std::string name);
    void add_module_route(const std::string& module_id, std::string method,
                          std::string suffix, Handler handler);
    Response handle(const Request& request) const;
    const std::vector<ModuleInfo>& modules() const { return modules_; }
    std::shared_ptr<security::Security> security() const { return security_; }
    void set_security(std::shared_ptr<security::Security> value) { security_ = std::move(value); }

private:
    struct Route {
        std::string method;
        std::string path;
        Handler handler;
    };
    std::vector<ModuleInfo> modules_;
    std::vector<Route> routes_;
    std::shared_ptr<security::Security> security_;
};

App make_app(const AppOptions& options = {});

}  // namespace platform
