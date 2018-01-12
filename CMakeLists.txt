cmake_minimum_required(VERSION 2.8.8)

project(greyhound)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules" ${CMAKE_MODULE_PATH})

set(GREYHOUND_VERSION_MAJOR 1 CACHE STRING "Greyhound major version" FORCE)
set(GREYHOUND_VERSION_MINOR 1 CACHE STRING "Greyhound minor version" FORCE)
set(GREYHOUND_VERSION_PATCH 0 CACHE STRING "Greyhound patch version" FORCE)
set(GREYHOUND_VERSION_STRING
    "${GREYHOUND_VERSION_MAJOR}.${GREYHOUND_VERSION_MINOR}.${GREYHOUND_VERSION_PATCH}"
    CACHE STRING "Entwine version string" FORCE)

set(GREYHOUND_API_VERSION "1")
set(GREYHOUND_BUILD_VERSION ${GREYHOUND_VERSION_STRING})

if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
    MESSAGE("Defaulting to release mode")
    set(CMAKE_BUILD_TYPE "Release")
endif()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR
    "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_definitions(${CMAKE_CXX_FLAGS} "-std=c++11")
    add_definitions(${CMAKE_CXX_FLAGS} "-Wno-deprecated-declarations")
    add_definitions(${CMAKE_CXX_FLAGS} "-Wall")
    add_definitions(${CMAKE_CXX_FLAGS} "-pedantic")
    add_definitions(${CMAKE_CXX_FLAGS} "-fexceptions")
    add_definitions(${CMAKE_CXX_FLAGS} "-fPIC")
endif()

if (MSVC)
    # prevents clashes between macros min\max and std::min\std::max
    add_definitions(${CMAKE_CXX_FLAGS} "/DNOMINMAX")
endif(MSVC)

set(PDAL_FIND_VERSION 1.2)
find_package(PDAL ${PDAL_FIND_VERSION} REQUIRED CONFIG NO_POLICY_SCOPE)

find_package(Threads REQUIRED)
find_package(LazPerf REQUIRED)
if (NOT WIN32)
find_package(Backtrace REQUIRED)
endif()
find_package(Curl)
find_package(JsonCpp)
find_package(OpenSSL)
find_package(Simple-Web-Server REQUIRED)

if (CURL_FOUND)
    message("Found curl")
    set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
    find_package(Threads REQUIRED)
    include_directories(${CURL_INCLUDE_DIRS})
    set(GREYHOUND_CURL TRUE)
    add_definitions("-DGREYHOUND_CURL")
    add_definitions("-DARBITER_CURL")
else()
    message("Curl NOT found")
endif()

if (OPENSSL_FOUND)
    message("Found OpenSSL ${OPENSSL_VERSION}")
    include_directories(${OPENSSL_INCLUDE_DIR})
    set(ARBITER_OPENSSL TRUE)
    set(GREYHOUND_OPENSSL TRUE)
    add_definitions("-DARBITER_OPENSSL")
    add_definitions("-DGREYHOUND_OPENSSL")
else()
    # For me this is /usr/local/opt/openssl
    message("OpenSSL NOT found - `export OPENSSL_ROOT_DIR=___`")
    message("Google storage IO will not be available")
endif()

get_target_property(PDALCPP_INCLUDE_DIRS pdalcpp INTERFACE_INCLUDE_DIRECTORIES)
if (PDALCPP_INCLUDE_DIRS)
    message("Including from PDAL: ${PDALCPP_INCLUDE_DIRS}")
    include_directories(${PDALCPP_INCLUDE_DIRS})
else()
    include_directories(/usr/include/laszip)
    include_directories(/usr/local/include/laszip)
endif()

mark_as_advanced(CLEAR PDAL_INCLUDE_DIRS)
mark_as_advanced(CLEAR LazPerf_INCLUDE_DIR)
mark_as_advanced(CLEAR PDAL_LIBRARIES)
include_directories(${PDAL_INCLUDE_DIRS})
include_directories(${LAZPERF_INCLUDE_DIR})
include_directories(${SIMPLE_WEB_SERVER_INCLUDE_DIR})
include_directories(${JSONCPP_INCLUDE_DIRS})

if (CMAKE_MAJOR_VERSION GREATER 2)
    cmake_policy(SET CMP0022 OLD) # interface link libraries
    cmake_policy(SET CMP0042 NEW) # osx rpath
endif()

include_directories(BEFORE "${CMAKE_CURRENT_BINARY_DIR}")
include_directories(BEFORE "${CMAKE_CURRENT_SOURCE_DIR}")

add_subdirectory(greyhound)

