cmake_minimum_required(VERSION 3.10)
project(LinuxNetworkingLab)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(network_server
    main.cpp
    server.cpp
    connection.cpp
    socket_utils.cpp
)

target_include_directories(network_server
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Linux-specific requirements
target_compile_definitions(network_server
    PRIVATE
        _GNU_SOURCE
)

target_link_libraries(network_server
    PRIVATE
        pthread
)
