cmake_minimum_required(VERSION 3.20)
project(Beout_OSSecurityAppliance VERSION 1.0.0 LANGUAGES CXX C)
enable_testing()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build configurations
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler warnings and security flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -D_FORTIFY_SOURCE=2 -fstack-protector-strong")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -fsanitize=address,undefined,leak")

# Dependencies
find_package(OpenSSL)
find_package(SQLite3)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.12.1
)
FetchContent_MakeAvailable(googletest)


if(NOT SQLite3_FOUND)
  FetchContent_Declare(
    sqlite3_src
    URL https://sqlite.org/2023/sqlite-amalgamation-3430100.zip
  )
  FetchContent_Populate(sqlite3_src)
  add_library(sqlite3_lib STATIC ${sqlite3_src_SOURCE_DIR}/sqlite3.c)
  target_include_directories(sqlite3_lib PUBLIC ${sqlite3_src_SOURCE_DIR})
  add_library(SQLite::SQLite3 ALIAS sqlite3_lib)
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/common)
include_directories(${CMAKE_SOURCE_DIR}/third_party/nlohmann)
include_directories(${CMAKE_SOURCE_DIR}/third_party/httplib)

# Subdirectories
add_subdirectory(common)
add_subdirectory(crypto)
add_subdirectory(logging)
add_subdirectory(database)
add_subdirectory(config-engine)
add_subdirectory(provisioning)
add_subdirectory(appliance-cli)
add_subdirectory(activation)
add_subdirectory(licensing)
add_subdirectory(api)
add_subdirectory(tests)

# Static analysis and formatting
find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_TIDY_EXE)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE};-checks=*,-fuchsia-*,-google-*,-zircon-*,-abseil-*,-modernize-use-trailing-return-type")
endif()
