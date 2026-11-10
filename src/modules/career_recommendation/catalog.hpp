#pragma once

#include "platform/db.hpp"
#include "platform/json.hpp"

namespace platform::career {

class CatalogError : public std::runtime_error {
public:
    CatalogError(int status, const std::string& message)
        : std::runtime_error(message), status(status) {}
    int status;
};

Json validate_job(const Json& input);
Json validate_strategy(const Json& input);
std::string validate_status(const Json& input);

class Catalog {
public:
    explicit Catalog(const std::string& database_path);

    Json list_jobs();
    Json create_job(const Json& details);
    Json update_job(std::int64_t id, const Json& details);
    Json set_status(std::int64_t id, const std::string& status);
    Json get_strategy();
    Json save_strategy(const Json& details);

private:
    Database db_;
};

}  // namespace platform::career
