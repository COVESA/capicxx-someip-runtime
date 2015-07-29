# Copyright (C) 2013-2015 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

cmake_minimum_required (VERSION 2.8)

project (CommonAPI-SomeIP)

set (CMAKE_VERBOSE_MAKEFILE off)

set (LIBCOMMONAPI_SOMEIP_MAJOR_VERSION 3)
set (LIBCOMMONAPI_SOMEIP_MINOR_VERSION 1)
set (LIBCOMMONAPI_SOMEIP_PATCH_VERSION 3)

message(STATUS "Project name: ${PROJECT_NAME}")

set (COMPONENT_VERSION ${LIBCOMMONAPI_SOMEIP_MAJOR_VERSION}.${LIBCOMMONAPI_SOMEIP_MINOR_VERSION}.${LIBCOMMONAPI_SOMEIP_PATCH_VERSION})
set (COMMONAPI_API_HEADER_VERSION ${LIBCOMMONAPI_SOMEIP_MAJOR_VERSION}.${LIBCOMMONAPI_SOMEIP_MINOR_VERSION})

SET(PACKAGE_VERSION "${COMPONENT_VERSION}") # used in *.cmake.in

if (MSVC) 
# Boost
find_package( Boost 1.54 COMPONENTS system thread log REQUIRED )
include_directories( ${Boost_INCLUDE_DIR} )
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_CRT_SECURE_NO_WARNINGS")
link_directories(${Boost_LIBRARY_DIR})
else()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -D_GLIBCXX_USE_NANOSLEEP")
endif()

SET(MAX_LOG_LEVEL "DEBUG" CACHE STRING "maximum log level")
message(STATUS "MAX_LOG_LEVEL is set to value: ${MAX_LOG_LEVEL}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCOMMONAPI_INTERNAL_COMPILATION")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCOMMONAPI_SOMEIP_VERSION_MAJOR=${LIBCOMMONAPI_SOMEIP_MAJOR_VERSION}") 
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCOMMONAPI_SOMEIP_VERSION_MINOR=${LIBCOMMONAPI_SOMEIP_MINOR_VERSION}") 
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCOMMONAPI_LOGLEVEL=COMMONAPI_LOGLEVEL_${MAX_LOG_LEVEL}")

# Package config module not found message macro
macro (pkg_config_module_not_found_message PKG_CONFIG_MODULE)
    message (FATAL_ERROR "pkg-config could not find the required module ${PKG_CONFIG_MODULE}!"
      " Please adjust your PKG_CONFIG_PATH environment variable accordingly.")
endmacro ()

# BEGIN TEMPORARY WORKAROUND #
# OS
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set (OS "LINUX")
    set (NO_DEPRECATED_REGISTER "")
endif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")

if (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    set (OS "FREEBSD")
    set (NO_DEPRECATED_REGISTER "-Wno-deprecated-register")
endif (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")

if (NOT MSVC)
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D${OS} ${NO_DEPRECATED_REGISTER} -DBOOST_LOG_DYN_LINK -pthread -g -std=c++0x -O0")
endif()
# END TEMPORARY WORKAROUND #

###################################################################################################
# see http://www.cmake.org/Wiki/CMake/Tutorials/How_to_create_a_ProjectConfig.cmake_file
###################################################################################################

# Offer the user the choice of overriding the installation directories
set (INSTALL_LIB_DIR lib CACHE PATH "Installation directory for libraries")
set (INSTALL_INCLUDE_DIR include/CommonAPI-${COMMONAPI_API_HEADER_VERSION} CACHE PATH "Installation directory for header files")

if (WIN32 AND NOT CYGWIN)
    set (DEF_INSTALL_CMAKE_DIR cmake)
else ()
    set (DEF_INSTALL_CMAKE_DIR lib/cmake/CommonAPI-SomeIP-${COMPONENT_VERSION})
endif ()

set (INSTALL_CMAKE_DIR ${DEF_INSTALL_CMAKE_DIR} CACHE PATH "Installation directory for CMake files")

# Make relative paths absolute (needed later on)
foreach (p LIB BIN INCLUDE CMAKE)
    set (var INSTALL_${p}_DIR)
    if (NOT IS_ABSOLUTE "${${var}}")
        set (ABSOLUTE_${var} "${CMAKE_INSTALL_PREFIX}/${${var}}")
    endif ()
endforeach ()

###################################################################################################

if ("${USE_INSTALLED_COMMONAPI}" STREQUAL "ON")
    FIND_PACKAGE(CommonAPI 3.1.3 REQUIRED CONFIG NO_CMAKE_PACKAGE_REGISTRY)
else()
    FIND_PACKAGE(CommonAPI 3.1.3 REQUIRED CONFIG NO_SYSTEM_ENVIRONMENT_PATH NO_CMAKE_SYSTEM_PATH)
endif()

message(STATUS "CommonAPI_CONSIDERED_CONFIGS: ${CommonAPI_CONSIDERED_CONFIGS}")
message(STATUS "COMMONAPI_INCLUDE_DIRS: ${COMMONAPI_INCLUDE_DIRS}")
message(STATUS "CommonAPI Version: ${CommonAPI_VERSION}")

# Boost
find_package( Boost 1.54 COMPONENTS system thread log REQUIRED )
include_directories( ${Boost_INCLUDE_DIR} )

find_package (vsomeip 1.3.0 REQUIRED)
message(STATUS "vsomeip version: ${vsomeip_VERSION}")

include_directories (
	${Boost_INCLUDE_DIR}
    ${COMMONAPI_INCLUDE_DIRS}
    ${VSOMEIP_INCLUDE_DIRS}
    include
)

file (GLOB CommonAPI-SomeIP_SRC "src/CommonAPI/SomeIP/*.cpp")

if (MSVC)
set (RPCRT Rpcrt4)
else()
set (RPCRT "")
endif()
 
# CommonAPI
add_library (CommonAPI-SomeIP SHARED ${CommonAPI-SomeIP_SRC})
set_target_properties (CommonAPI-SomeIP PROPERTIES VERSION ${COMPONENT_VERSION} SOVERSION ${LIBCOMMONAPI_SOMEIP_MAJOR_VERSION})
target_link_libraries (CommonAPI-SomeIP CommonAPI vsomeip ${RPCRT})

###################################################################################################

file (GLOB_RECURSE CommonAPI-SomeIP_INCLUDE_INSTALL_FILES "include/*.hpp")

set_target_properties (CommonAPI-SomeIP PROPERTIES PUBLIC_HEADER "${CommonAPI-SomeIP_INCLUDE_INSTALL_FILES}")

install (
    TARGETS CommonAPI-SomeIP
    # IMPORTANT: Add the CommonAPI-SomeIP library to the "export-set"
    EXPORT CommonAPI-SomeIPTargets
    LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
    RUNTIME DESTINATION "${PROJECT_BINARY_DIR}/Bin"
    PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}/CommonAPI/SomeIP"
)

##############################################################################
# exporting, configuring and installing of cmake files

# Add all targets to the build-tree export set
export (TARGETS CommonAPI-SomeIP
        FILE "${PROJECT_BINARY_DIR}/CommonAPI-SomeIPTargets.cmake")

# Export the package for use from the build-tree
# (this registers the build-tree with a global CMake-registry)
export (PACKAGE CommonAPI-SomeIP)

# Create the CommonAPI-SomeIPConfig.cmake and CommonAPI-SomeIPConfigVersion files
file (RELATIVE_PATH REL_INCLUDE_DIR "${ABSOLUTE_INSTALL_CMAKE_DIR}" "${ABSOLUTE_INSTALL_INCLUDE_DIR}")

# ... for the build tree
set (CONF_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/include")
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/cmake/CommonAPI-SomeIPConfig.cmake.in
    "${PROJECT_BINARY_DIR}/CommonAPI-SomeIPConfig.cmake" @ONLY)

# ... for the install tree
set (CONF_INCLUDE_DIRS "\${COMMONAPI_SOMEIP_CMAKE_DIR}/${REL_INCLUDE_DIR}")
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/cmake/CommonAPI-SomeIPConfig.cmake.in
    "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CommonAPI-SomeIPConfig.cmake" @ONLY)

# ... for both
configure_file (${CMAKE_CURRENT_SOURCE_DIR}/cmake/CommonAPI-SomeIPConfigVersion.cmake.in
    "${PROJECT_BINARY_DIR}/CommonAPI-SomeIPConfigVersion.cmake" @ONLY)

# Install the CommonAPI-SomeIPConfig.cmake and CommonAPI-SomeIPConfigVersion.cmake
install (
    FILES
    "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CommonAPI-SomeIPConfig.cmake"
    "${PROJECT_BINARY_DIR}/CommonAPI-SomeIPConfigVersion.cmake"
    DESTINATION "${INSTALL_CMAKE_DIR}"
)

# Install the export set for use with the install-tree
install (
    EXPORT CommonAPI-SomeIPTargets
    DESTINATION "${INSTALL_CMAKE_DIR}"
)

##############################################################################
# maintainer-clean
add_custom_target(maintainer-clean COMMAND rm -rf *)
