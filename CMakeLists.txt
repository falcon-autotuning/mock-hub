cmake_minimum_required(VERSION 3.20)
project(
  falcon-mock-hub
  VERSION 1.0.0
  DESCRIPTION "Mock instrument hub service and FFI bindings for Falcon"
  HOMEPAGE_URL "https://github.com/falcon-autotuning/mock-hub"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_TESTS "Build tests" ON)

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

# Core C++ shared library: falcon-mock-hub
add_library(falcon-mock-hub SHARED
  src/mock_hub.cpp
)

target_include_directories(falcon-mock-hub
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(falcon-mock-hub
  PUBLIC
    falcon::falcon-comms
    falcon-core::falcon-core
    falcon::falcon-typing
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    cnats::nats
)

target_compile_definitions(falcon-mock-hub PRIVATE falcon_mock_hub_EXPORTS)
target_compile_options(falcon-mock-hub PRIVATE -Wall -Wextra -O3 -fPIC)

# FFI wrapper shared library: mock-hub-wrapper
add_library(mock-hub-wrapper SHARED
  src/mock_hub.cpp
  mock-hub-wrapper.cpp
)

set_target_properties(mock-hub-wrapper PROPERTIES
  PREFIX ""
  OUTPUT_NAME "mock-hub-wrapper"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
)

target_include_directories(mock-hub-wrapper
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
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

# Tests
if(BUILD_TESTS)
  enable_testing()
  find_package(GTest CONFIG REQUIRED)

  add_executable(falcon_mock_hub_unit_tests
    tests/unit/test_mock_hub.cpp
  )

  target_include_directories(falcon_mock_hub_unit_tests
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
  )

  target_link_libraries(falcon_mock_hub_unit_tests
    PRIVATE
      falcon-mock-hub
      GTest::gtest
      GTest::gtest_main
  )

  add_test(NAME falcon_mock_hub_unit_tests COMMAND falcon_mock_hub_unit_tests)
endif()

# Install rules
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS falcon-mock-hub
  EXPORT falcon-mock-hub-targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/falcon-mock-hub
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING
    PATTERN "*.hpp"
    PATTERN "*.h"
)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/falcon-mock-hub-config-version.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion
)

install(EXPORT falcon-mock-hub-targets
  FILE falcon-mock-hub-targets.cmake
  NAMESPACE falcon::
  DESTINATION ${CMAKE_INSTALL_DATADIR}/falcon-mock-hub
)
