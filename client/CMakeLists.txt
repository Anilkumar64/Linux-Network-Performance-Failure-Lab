add_executable(network_client
    main.cpp
    client.cpp
)

target_include_directories(network_client
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(network_client
    PRIVATE
        _GNU_SOURCE
)
