#include "platform/app.hpp"
#include "platform/modules.hpp"

#include <stdexcept>
#include <utility>

namespace platform {
namespace {
std::string json_escape(const std::string& value) {
    std::string out;
    for (unsigned char c : value) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    const char* hex = "0123456789abcdef";
                    out += "\\u00";
                    out += hex[c >> 4];
                    out += hex[c & 15];
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

Response json_response(int status, std::string body) {
    return {status, "application/json; charset=utf-8", std::move(body)};
}

bool match_path(const std::string& pattern, const std::string& actual,
                std::map<std::string, std::string>& params) {
    std::size_t p = 0;
    std::size_t a = 0;
    while (p < pattern.size() && a < actual.size()) {
        const auto p_end = pattern.find('/', p + 1);
        const auto a_end = actual.find('/', a + 1);
        const auto p_part = pattern.substr(p, p_end == std::string::npos ? p_end : p_end - p);
        const auto a_part = actual.substr(a, a_end == std::string::npos ? a_end : a_end - a);
        if (p_part.size() > 1 && p_part[1] == ':') {
            if (a_part.size() <= 1) return false;
            params[p_part.substr(2)] = a_part.substr(1);
        } else if (p_part != a_part) {
            return false;
        }
        p = p_end == std::string::npos ? pattern.size() : p_end;
        a = a_end == std::string::npos ? actual.size() : a_end;
    }
    return p == pattern.size() && a == actual.size();
}
}  // namespace

void App::add_module(std::string id, std::string name) {
    if (id.empty() || id.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-") != std::string::npos ||
        name.empty()) {
        throw std::invalid_argument("invalid module metadata");
    }
    for (const auto& module : modules_) {
        if (module.id == id) throw std::invalid_argument("duplicate module id");
    }
    modules_.push_back({std::move(id), std::move(name), {}});
}

void App::add_module_route(const std::string& module_id, std::string method,
                           std::string suffix, Handler handler) {
    if (method.empty() || suffix.empty() || suffix[0] != '/' ||
        suffix.find('?') != std::string::npos || suffix.find(' ') != std::string::npos || !handler) {
        throw std::invalid_argument("invalid module route");
    }
    for (auto& module : modules_) {
        if (module.id != module_id) continue;
        std::string path = "/api/modules/" + module_id + suffix;
        for (const auto& route : routes_) {
            if (route.method == method && route.path == path)
                throw std::invalid_argument("duplicate route");
        }
        module.routes.push_back({method, path});
        routes_.push_back({std::move(method), std::move(path), std::move(handler)});
        return;
    }
    throw std::invalid_argument("unknown module id");
}

Response App::handle(const Request& request) const {
    if (request.method == "GET" && request.path == "/api/health")
        return json_response(200, "{\"status\":\"ok\"}");
    if (request.method == "GET" && request.path == "/api/modules") {
        std::string body = "[";
        for (const auto& module : modules_) {
            if (body.size() > 1) body += ',';
            body += "{\"id\":\"" + json_escape(module.id) + "\",\"name\":\"" +
                    json_escape(module.name) + "\",\"routes\":[";
            bool first = true;
            for (const auto& route : module.routes) {
                if (!first) body += ',';
                first = false;
                body += "{\"method\":\"" + json_escape(route.method) +
                        "\",\"path\":\"" + json_escape(route.path) + "\"}";
            }
            body += "]}";
        }
        body += ']';
        return json_response(200, std::move(body));
    }
    for (const auto& route : routes_) {
        if (route.method != request.method) continue;
        Request matched = request;
        if (match_path(route.path, request.path, matched.path_params))
            return route.handler(matched);
    }
    return json_response(404, "{\"error\":\"route not found\"}");
}

App make_app() {
    App app;
    register_data_ingestion(app);
    register_data_governance(app);
    register_student_profile(app);
    register_academic_alerts(app);
    register_career_recommendation(app);
    register_data_services(app);
    return app;
}
}  // namespace platform
