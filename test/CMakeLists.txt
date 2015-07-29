# Copyright (C) 2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required (VERSION 2.8.1)

set(PRJ_NAME SomeIPTests)

#set(CMAKE_VERBOSE_MAKEFILE on)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -Wall -O0 -std=c++0x -D_GLIBCXX_USE_NANOSLEEP -DLINUX")

message(STATUS "Compiler options: ${CMAKE_CXX_FLAGS}")

if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE "Debug" CACHE STRING
       "Choose the type of build, options are: Debug Release." FORCE)
endif(NOT CMAKE_BUILD_TYPE)
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

find_package(CommonAPI 3.1.2 REQUIRED CONFIG NO_SYSTEM_ENVIRONMENT_PATH NO_CMAKE_SYSTEM_PATH)
find_package(CommonAPI-SomeIP 3.1.2 REQUIRED)
find_package(vsomeip 1.2.0 REQUIRED)

##############################################################################
# google test

# check for set environment variable
if(DEFINED ENV{GTEST_ROOT})
    message("GTEST_ROOT is set. GTEST_ROOT = $ENV{GTEST_ROOT}")
else()
    message("GTEST_ROOT is not defined. For building the tests environment variable
             GTEST_ROOT have to be defined. Tests can not be built.")
    # early exit
    return()  # test can not be build -> make check is not available
endif()

# build google test as static library (always) -> therefore deactivate BUILD_SHARED_LIBS in case it is active
set(BUILD_SHARED_LIBS_AUTOMATIC_OFF 0)
if ("${BUILD_SHARED_LIBS}" STREQUAL "ON")
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_SHARED_LIBS_AUTOMATIC_OFF 1)
endif()
add_subdirectory($ENV{GTEST_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/gtest EXCLUDE_FROM_ALL)
if ("${BUILD_SHARED_LIBS_AUTOMATIC_OFF}" STREQUAL "1")
    set(BUILD_SHARED_LIBS ON)
    set(BUILD_SHARED_LIBS_AUTOMATIC_OFF 0)
endif()


include_directories(
    src-gen/core
    src-gen/someip
    ${COMMONAPI_INCLUDE_DIRS}
    ${COMMONAPI_SOMEIP_INCLUDE_DIRS}
    ${VSOMEIP_INCLUDE_DIRS}
    ${gtest_SOURCE_DIR}/include
)

##############################################################################
# GlueCode
##############################################################################
file(GLOB SomeIPManagedTest_GEN_SRC_SOMEIP src-gen/someip/v1_0/managed/*.cpp)

add_library (${PRJ_NAME}-someip SHARED
    ${SomeIPManagedTest_GEN_SRC_SOMEIP})
target_link_libraries(${PRJ_NAME}-someip CommonAPI-SomeIP)

set(TEST_LINK_LIBRARIES gtest -Wl,--as-needed CommonAPI)

##############################################################################
# SomeIPManagedTest
##############################################################################

file(GLOB SomeIPManagedTest_SRC src/managed/*.cpp)
file(GLOB SomeIPManagedTest_GEN_SRC_CORE src-gen/core/v1_0/managed/*.cpp)
add_executable(SomeIPManagedTest
    ${SomeIPManagedTest_SRC}
    ${SomeIPManagedTest_GEN_SRC_CORE}
    )
target_link_libraries(SomeIPManagedTest ${TEST_LINK_LIBRARIES})

##############################################################################
# Add for every test a dependency to gtest
##############################################################################

add_dependencies(SomeIPManagedTest gtest)

##############################################################################
# Add tests to the target build_tests
##############################################################################
enable_testing()

set(CMAKE_CTEST_COMMAND ctest -V)
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})
add_dependencies(check SomeIPManagedTest)

##############################################################################
# Add tests
##############################################################################
set(SOMEIP_COMMONAPI_CONFIG "COMMONAPI_CONFIG=../commonapi4someip.ini")
set(SOMEIP_VSOMEIP_JSON_FILE "VSOMEIP_CONFIGURATION_FILE=../vsomeip.json")
set(SOMEIP_LD_LIBRARY_PATH "LD_LIBRARY_PATH=.")
set(SOMEIP_TEST_ENVIRONMENT
    ${SOMEIP_COMMONAPI_CONFIG}
    ${SOMEIP_VSOMEIP_JSON_FILE}
    ${SOMEIP_LD_LIBRARY_PATH}
)

add_test(NAME SomeIPManagedTest COMMAND SomeIPManagedTest)
set_property(TEST SomeIPManagedTest APPEND PROPERTY ENVIRONMENT ${SOMEIP_TEST_ENVIRONMENT})
