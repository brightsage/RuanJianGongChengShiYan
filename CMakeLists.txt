cmake_minimum_required(VERSION 3.21)
project(student_growth_platform LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
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

# MySQL 8.4 客户端库。Windows 使用官方 ZIP 内的 libmysql.lib；
# 其他平台优先使用系统的 mysqlclient/mariadb。
set(PLATFORM_MYSQL_ROOT "$ENV{MYSQL_ROOT}" CACHE PATH "MySQL 8.4 installation root")
if(WIN32)
    if(NOT PLATFORM_MYSQL_ROOT)
        set(_mysql_default "$ENV{LOCALAPPDATA}/Programs/mysql-8.4.13/mysql-8.4.13-winx64")
        if(EXISTS "${_mysql_default}/include/mysql.h")
            set(PLATFORM_MYSQL_ROOT "${_mysql_default}" CACHE PATH "MySQL 8.4 installation root" FORCE)
        endif()
    endif()
    if(NOT EXISTS "${PLATFORM_MYSQL_ROOT}/include/mysql.h")
        message(FATAL_ERROR
            "MySQL headers not found. Set -DPLATFORM_MYSQL_ROOT=<MySQL ZIP root> or MYSQL_ROOT.")
    endif()
    add_library(platform_mysql INTERFACE)
    target_include_directories(platform_mysql INTERFACE "${PLATFORM_MYSQL_ROOT}/include")
    target_link_libraries(platform_mysql INTERFACE "${PLATFORM_MYSQL_ROOT}/lib/libmysql.lib")
else()
    # Debian/Ubuntu 把 mysql.h 装在 include/mysql（或 include/mariadb）下，
    # find_path 不会递归子目录，必须显式给 PATH_SUFFIXES。
    find_path(MYSQL_INCLUDE_DIR NAMES mysql.h PATH_SUFFIXES mysql mariadb mysql/mariadb)
    find_library(MYSQL_LIBRARY NAMES mysqlclient mariadb libmysqlclient)
    if(NOT MYSQL_INCLUDE_DIR OR NOT MYSQL_LIBRARY)
        message(FATAL_ERROR
            "MySQL client headers/library not found (install libmysqlclient-dev or mariadb-devel).")
    endif()
    add_library(platform_mysql INTERFACE)
    target_include_directories(platform_mysql INTERFACE "${MYSQL_INCLUDE_DIR}")
    target_link_libraries(platform_mysql INTERFACE "${MYSQL_LIBRARY}")
endif()

add_library(platform_core
    src/app.cpp
    src/platform/json.cpp
    src/platform/db.cpp
    src/platform/api.cpp
    src/platform/crypto.cpp
    src/modules/data_ingestion/module.cpp
    src/modules/data_ingestion/service.cpp
    src/modules/data_ingestion/sqlite_repository.cpp
    src/modules/data_governance/module.cpp
    src/modules/data_governance/repository.cpp
    src/modules/data_governance/service.cpp
    src/modules/data_governance/types.cpp
    src/modules/data_governance/quality_rules.cpp
    src/modules/data_governance/student_master.cpp
    src/modules/data_governance/metrics.cpp
    src/modules/data_governance/quality_report.cpp
    src/modules/student_profile/module.cpp
    src/modules/student_profile/service.cpp
    src/modules/student_profile/sqlite_repository.cpp
    src/modules/academic_alerts/module.cpp
    src/modules/academic_alerts/alerts_api.cpp
    src/modules/academic_alerts/repository.cpp
    src/modules/career_recommendation/module.cpp
    src/modules/career_recommendation/catalog.cpp
    src/modules/career_recommendation/recommendations.cpp
    src/modules/career_recommendation/profile.cpp
    src/modules/career_recommendation/outcomes.cpp
    src/modules/data_services/module.cpp
    src/modules/data_services/store.cpp
    src/modules/data_services/security.cpp
    src/modules/data_services/services.cpp
    src/modules/data_services/gateway.cpp
)
target_include_directories(platform_core PUBLIC include ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(platform_core PUBLIC platform_mysql)

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

add_executable(data_governance_tests tests/test_data_governance.cpp)
target_link_libraries(data_governance_tests PRIVATE platform_core)
add_test(NAME data_governance_tests COMMAND data_governance_tests)

add_executable(platform_storage_tests tests/storage_test.cpp)
target_link_libraries(platform_storage_tests PRIVATE platform_core)
add_test(NAME platform_storage_tests COMMAND platform_storage_tests)

foreach(suite IN ITEMS security service gateway)
    add_executable(${suite}_tests tests/${suite}_test.cpp)
    target_link_libraries(${suite}_tests PRIVATE platform_core)
    add_test(NAME ${suite}_tests COMMAND ${suite}_tests)
endforeach()

add_executable(career_profile_tests tests/career_profile_test.cpp)
target_link_libraries(career_profile_tests PRIVATE platform_core)
add_test(NAME career_profile_tests COMMAND career_profile_tests)

add_executable(academic_alerts_tests tests/academic_alerts_test.cpp)
target_link_libraries(academic_alerts_tests PRIVATE platform_core)
add_test(NAME academic_alerts_tests COMMAND academic_alerts_tests)

add_executable(platform_module_tests tests/module_api_test.cpp)
target_link_libraries(platform_module_tests PRIVATE platform_core)
add_test(NAME platform_module_tests COMMAND platform_module_tests)

# MinGW 下静态链接运行库，生成的 exe 不依赖 w64devkit 的 DLL。
if(MINGW)
    foreach(target platform_server platform_tests data_governance_tests academic_alerts_tests platform_storage_tests platform_module_tests security_tests service_tests gateway_tests career_profile_tests)
        target_link_options(${target} PRIVATE -static)
    endforeach()
endif()

set(PLATFORM_EXECUTABLES
    platform_server platform_tests data_governance_tests academic_alerts_tests
    platform_storage_tests platform_module_tests security_tests service_tests
    gateway_tests career_profile_tests)

if(WIN32)
    function(copy_mysql_runtime target)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PLATFORM_MYSQL_ROOT}/lib/libmysql.dll"
                "${PLATFORM_MYSQL_ROOT}/bin/libssl-3-x64.dll"
                "${PLATFORM_MYSQL_ROOT}/bin/libcrypto-3-x64.dll"
                "$<TARGET_FILE_DIR:${target}>")
    endfunction()
    foreach(target IN LISTS PLATFORM_EXECUTABLES)
        copy_mysql_runtime(${target})
    endforeach()
endif()

if(WIN32)
    set(PLATFORM_MYSQL_CLIENT "${PLATFORM_MYSQL_ROOT}/bin/mysql.exe")
else()
    set(PLATFORM_MYSQL_CLIENT "mysql")
endif()
set(PLATFORM_TEST_ENVIRONMENT "PLATFORM_TEST_DB=1;MYSQL_CLIENT=${PLATFORM_MYSQL_CLIENT}")
set_tests_properties(
    platform_tests data_governance_tests platform_storage_tests
    security_tests service_tests gateway_tests career_profile_tests
    academic_alerts_tests platform_module_tests
    PROPERTIES ENVIRONMENT "${PLATFORM_TEST_ENVIRONMENT}")

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME http_smoke
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/http_smoke.py
                $<TARGET_FILE:platform_server>)
    add_test(NAME data_services_http
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/data_services_http.py
                $<TARGET_FILE:platform_server>)
    set_tests_properties(http_smoke data_services_http
        PROPERTIES ENVIRONMENT "${PLATFORM_TEST_ENVIRONMENT}")
endif()
