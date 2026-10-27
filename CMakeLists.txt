cmake_minimum_required(VERSION 3.21)
project(student_growth_platform LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /utf-8)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/permissive->)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# SQLite 以源码形式内置，避免额外安装数据库服务或依赖包。
add_library(platform_sqlite STATIC third_party/sqlite/sqlite3.c)
target_include_directories(platform_sqlite PUBLIC third_party/sqlite)
if(MSVC)
    target_compile_options(platform_sqlite PRIVATE /w)
else()
    target_compile_options(platform_sqlite PRIVATE -w)
endif()

add_library(platform_core
    src/app.cpp
    src/platform/json.cpp
    src/platform/db.cpp
    src/modules/data_ingestion/module.cpp
    src/modules/data_ingestion/sqlite_repository.cpp
    src/modules/data_governance/module.cpp
    src/modules/student_profile/module.cpp
    src/modules/student_profile/sqlite_repository.cpp
    src/modules/academic_alerts/module.cpp
    src/modules/career_recommendation/module.cpp
    src/modules/data_services/module.cpp
)
target_include_directories(platform_core PUBLIC include ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(platform_core PUBLIC platform_sqlite)

add_executable(platform_server src/main.cpp src/http_server.cpp)
target_link_libraries(platform_server PRIVATE platform_core)
target_compile_definitions(platform_server PRIVATE PLATFORM_WEB_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/web")
if(WIN32)
    target_link_libraries(platform_server PRIVATE ws2_32)
endif()

enable_testing()
add_executable(platform_tests tests/app_test.cpp)
target_link_libraries(platform_tests PRIVATE platform_core)
add_test(NAME platform_tests COMMAND platform_tests)

add_executable(platform_storage_tests tests/storage_test.cpp)
target_link_libraries(platform_storage_tests PRIVATE platform_core)
add_test(NAME platform_storage_tests COMMAND platform_storage_tests)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME http_smoke
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/http_smoke.py
                $<TARGET_FILE:platform_server>)
endif()
