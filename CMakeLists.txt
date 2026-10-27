cmake_minimum_required(VERSION 3.10)
project(data_governance_module)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

add_library(data_governance
    src/module.cpp
    src/types.cpp
    src/quality_rules.cpp
    src/student_master.cpp
    src/metrics.cpp
    src/quality_report.cpp
)
