cmake_minimum_required(VERSION 3.20)
project(student_growth_platform LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(platform_core
    src/app.cpp
    src/modules/data_ingestion/module.cpp
    src/modules/data_governance/module.cpp
    src/modules/student_profile/module.cpp
    src/modules/academic_alerts/module.cpp
    src/modules/career_recommendation/module.cpp
    src/modules/data_services/module.cpp
)
target_include_directories(platform_core PUBLIC include)
target_compile_options(platform_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(platform_server src/main.cpp src/http_server.cpp)
target_link_libraries(platform_server PRIVATE platform_core)
if(WIN32)
    target_link_libraries(platform_server PRIVATE ws2_32)
endif()
target_compile_definitions(platform_server PRIVATE PLATFORM_WEB_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/web")
target_compile_options(platform_server PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()
add_executable(platform_tests tests/app_test.cpp)
target_link_libraries(platform_tests PRIVATE platform_core)
add_test(NAME platform_tests COMMAND platform_tests)
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME http_smoke
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/http_smoke.py
                $<TARGET_FILE:platform_server>)
endif()
