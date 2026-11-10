cmake_minimum_required(VERSION 3.20)
project(student_growth_platform LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)
if(TARGET SQLite3::SQLite3)
    set(PLATFORM_SQLITE_TARGET SQLite3::SQLite3)
else()
    set(PLATFORM_SQLITE_TARGET SQLite::SQLite3)
endif()
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONC REQUIRED IMPORTED_TARGET json-c)

add_library(platform_core
    src/app.cpp
    src/modules/data_ingestion/module.cpp
    src/modules/data_governance/module.cpp
    src/modules/student_profile/module.cpp
    src/modules/academic_alerts/module.cpp
    src/modules/career_recommendation/module.cpp
    src/modules/data_services/module.cpp
    src/modules/data_services/store.cpp
    src/modules/data_services/security.cpp
    src/modules/data_services/services.cpp
    src/modules/data_services/gateway.cpp
)
target_include_directories(platform_core PUBLIC include)
target_link_libraries(platform_core PUBLIC OpenSSL::Crypto ${PLATFORM_SQLITE_TARGET} PkgConfig::JSONC)
target_compile_options(platform_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(platform_server src/main.cpp src/http_server.cpp)
target_link_libraries(platform_server PRIVATE platform_core)
target_compile_definitions(platform_server PRIVATE PLATFORM_WEB_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/web")
target_compile_options(platform_server PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()
add_executable(platform_tests tests/app_test.cpp)
target_link_libraries(platform_tests PRIVATE platform_core)
add_test(NAME platform_tests COMMAND platform_tests)
add_executable(security_tests tests/security_test.cpp)
target_link_libraries(security_tests PRIVATE platform_core)
add_test(NAME security_tests COMMAND security_tests)
add_executable(service_tests tests/service_test.cpp)
target_link_libraries(service_tests PRIVATE platform_core)
add_test(NAME service_tests COMMAND service_tests)
add_executable(gateway_tests tests/gateway_test.cpp)
target_link_libraries(gateway_tests PRIVATE platform_core)
add_test(NAME gateway_tests COMMAND gateway_tests)
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME http_smoke
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/http_smoke.py
                $<TARGET_FILE:platform_server>)
    add_test(NAME data_services_http
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/data_services_http.py
                $<TARGET_FILE:platform_server>)
endif()
