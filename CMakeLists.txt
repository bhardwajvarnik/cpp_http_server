cmake_minimum_required(VERSION 3.16)
project(cpp_http_server CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O2")
add_compile_options(-Wall -Wextra)

find_package(Threads REQUIRED)
include_directories(include)

set(CORE_SOURCES
    src/thread_pool.cpp
    src/http_request.cpp
    src/http_response.cpp
    src/router.cpp
    src/tcp_server.cpp
    src/load_balancer.cpp
)

add_executable(server src/main.cpp ${CORE_SOURCES})
target_link_libraries(server PRIVATE Threads::Threads)

enable_testing()

add_executable(test_http_parser tests/test_http_parser.cpp
    src/http_request.cpp src/http_response.cpp)
target_link_libraries(test_http_parser PRIVATE Threads::Threads)
add_test(NAME HttpParserTests COMMAND test_http_parser)

add_executable(test_thread_pool tests/test_thread_pool.cpp src/thread_pool.cpp)
target_link_libraries(test_thread_pool PRIVATE Threads::Threads)
add_test(NAME ThreadPoolTests COMMAND test_thread_pool)
