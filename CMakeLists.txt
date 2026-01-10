cmake_minimum_required(VERSION 3.16)

project(network_lab
    VERSION 0.1
    DESCRIPTION "Linux Network Performance & Failure Lab"
    LANGUAGES CXX
)

# ---- Global rules ----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Strict compilation flags
add_compile_options(
    -Wall
    -Wextra
    -Werror
    -Wpedantic
)

# Build type default
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Subprojects
add_subdirectory(server)
add_subdirectory(client)
