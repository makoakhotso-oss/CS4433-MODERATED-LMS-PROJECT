cmake_minimum_required(VERSION 3.16)
project(LMS VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LMS_BUILD_TESTS "Build the automated tests (GoogleTest)" ON)
option(LMS_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------- dependencies
find_package(SQLite3 REQUIRED)

find_package(unofficial-sodium CONFIG QUIET)   # vcpkg (Windows)
if(TARGET unofficial-sodium::sodium)
  set(LMS_SODIUM_TARGET unofficial-sodium::sodium)
else()
  find_path(SODIUM_INCLUDE_DIR sodium.h)
  find_library(SODIUM_LIBRARY NAMES sodium libsodium)
  if(NOT SODIUM_INCLUDE_DIR OR NOT SODIUM_LIBRARY)
    message(FATAL_ERROR
      "libsodium was not found. Install it first, e.g.\n"
      "  Debian/Ubuntu: sudo apt install libsodium-dev\n"
      "  Fedora:        sudo dnf install libsodium-devel\n"
      "  macOS:         brew install libsodium\n"
      "  Windows:       vcpkg install libsodium sqlite3")
  endif()
  add_library(lms_sodium UNKNOWN IMPORTED)
  set_target_properties(lms_sodium PROPERTIES
    IMPORTED_LOCATION "${SODIUM_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${SODIUM_INCLUDE_DIR}")
  set(LMS_SODIUM_TARGET lms_sodium)
endif()

# ------------------------------------------------- embed sql/schema.sql in code
# sql/schema.sql is the single source of truth for the database design.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/sql/schema.sql" LMS_SCHEMA_SQL)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/sql/schema.sql")
configure_file(cmake/SchemaSql.cpp.in "${CMAKE_BINARY_DIR}/generated/SchemaSql.cpp" @ONLY)

# ---------------------------------------------------------------- core library
file(GLOB_RECURSE LMS_CORE_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/Application.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/common/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/domain/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/security/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/data/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/services/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/*.cpp")

add_library(lms_core STATIC ${LMS_CORE_SOURCES} "${CMAKE_BINARY_DIR}/generated/SchemaSql.cpp")
target_include_directories(lms_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(lms_core PUBLIC SQLite::SQLite3 ${LMS_SODIUM_TARGET})

function(lms_enable_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
    if(LMS_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow
      -Wnon-virtual-dtor -Woverloaded-virtual -Wcast-align -Wconversion -Wno-sign-conversion)
    if(LMS_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()
lms_enable_warnings(lms_core)

# ------------------------------------------------------------------ executable
add_executable(lms src/main.cpp)
target_link_libraries(lms PRIVATE lms_core)
lms_enable_warnings(lms)

# ----------------------------------------------------------------------- tests
if(LMS_BUILD_TESTS)
  enable_testing()
  find_package(GTest QUIET)
  if(NOT GTest_FOUND)
    include(FetchContent)
    FetchContent_Declare(googletest
      URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
  endif()
  file(GLOB LMS_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
  add_executable(lms_tests ${LMS_TEST_SOURCES})
  target_link_libraries(lms_tests PRIVATE lms_core GTest::gtest GTest::gtest_main)
  include(GoogleTest)
  gtest_discover_tests(lms_tests DISCOVERY_TIMEOUT 60)
endif()
