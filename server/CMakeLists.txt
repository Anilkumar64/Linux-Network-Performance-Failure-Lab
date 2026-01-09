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
