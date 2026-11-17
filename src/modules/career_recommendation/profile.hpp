#pragma once

#include "platform/db.hpp"

namespace platform::career {

struct Profile {
    std::string student_no;
    std::string term;
    std::string version;
    std::map<std::string, double> scores;
};

Profile load_profile(Database& db, const std::string& student_no, const std::string& term);
Profile demo_profile(const std::string& student_no, const std::string& term);

}  // namespace platform::career
