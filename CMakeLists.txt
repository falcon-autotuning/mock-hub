cmake_minimum_required(VERSION 3.20)
project(
  mock-hub
  VERSION 1.0.0
  DESCRIPTION "Mock instrument hub service and FFI bindings for Falcon DSL tests"
  HOMEPAGE_URL "https://github.com/falcon-autotuning/mock-hub"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Ensure RPATH includes library directories of linked targets
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_BUILD_RPATH_USE_ORIGIN TRUE)
if(UNIX AND NOT APPLE)
  add_link_options("-Wl,--disable-new-dtags")
endif()

# Find dependencies via vcpkg
find_package(nlohmann_json   CONFIG REQUIRED)
find_package(spdlog          CONFIG REQUIRED)
find_package(cnats           CONFIG REQUIRED)
find_package(falcon-typing   CONFIG REQUIRED)
find_package(falcon-core     CONFIG REQUIRED)
find_package(falcon-comms    CONFIG REQUIRED)

# Build the FFI wrapper shared library
add_library(mock-hub-wrapper SHARED
  mock-hub-wrapper.cpp
)

set_target_properties(mock-hub-wrapper PROPERTIES
  PREFIX ""
  OUTPUT_NAME "mock-hub-wrapper"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
)

target_include_directories(mock-hub-wrapper
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mock-hub-wrapper
  PRIVATE
    falcon::falcon-comms
    falcon-core::falcon-core
    falcon::falcon-typing
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    cnats::nats
)

target_compile_options(mock-hub-wrapper PRIVATE -Wall -Wextra -O3 -fPIC)
