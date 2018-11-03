########################################################################
# Project setup
########################################################################
cmake_minimum_required(VERSION 2.8.0)
project(tx_tools C)

#local include directories first
include_directories(${PROJECT_SOURCE_DIR}/src/convenience)

#include local cmake modules
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

########################################################################
# Dependencies
########################################################################
find_package(SoapySDR "0.6" NO_MODULE)
if (NOT SoapySDR_FOUND)
    message(FATAL_ERROR "Soapy SDR development files not found...")
endif ()
include_directories(${SoapySDR_INCLUDE_DIRS})
list(APPEND TX_TOOLS_LIBS ${SoapySDR_LIBRARIES})

#link with libm when available
find_library(
    MATH_LIBRARIES NAMES m
    PATHS /usr/lib /usr/lib64
)
if (MATH_LIBRARIES)
    list(APPEND TX_TOOLS_LIBS ${MATH_LIBRARIES})
endif ()

#link with pthreads
set(THREADS_USE_PTHREADS_WIN32 true)
find_package(Threads)
if (NOT THREADS_FOUND)
    message(FATAL_ERROR "pthreads development files not found...")
endif ()
include_directories(${THREADS_PTHREADS_INCLUDE_DIR})
list(APPEND TX_TOOLS_LIBS ${CMAKE_THREAD_LIBS_INIT})
message(STATUS "THREADS_PTHREADS_INCLUDE_DIR: ${THREADS_PTHREADS_INCLUDE_DIR}")
message(STATUS "CMAKE_THREAD_LIBS_INIT: ${CMAKE_THREAD_LIBS_INIT}")

#windows getopt compatibility
if (WIN32)
    include_directories(${PROJECT_SOURCE_DIR}/src/getopt)
    list(APPEND COMMON_SOURCES src/getopt/getopt.c)
endif ()

########################################################################
# Helper library
########################################################################
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
list(APPEND COMMON_SOURCES src/convenience/convenience.c src/argparse.c)
add_library(common STATIC ${COMMON_SOURCES})
list(APPEND TX_TOOLS_LIBS common)

########################################################################
# Compiler specific setup
########################################################################
if (("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_C_COMPILER_ID}" STREQUAL "Clang") AND NOT WIN32)
    ADD_DEFINITIONS(-Wall)
    ADD_DEFINITIONS(-Wextra)
    ADD_DEFINITIONS(-Wsign-compare)
    ADD_DEFINITIONS(-Wno-unused)
    ADD_DEFINITIONS(-Wno-unused-parameter)
    ADD_DEFINITIONS(-Wno-missing-field-initializers)
    ADD_DEFINITIONS(-std=c99)
    ADD_DEFINITIONS(-pedantic)
    ADD_DEFINITIONS(-Wdouble-promotion)
    ADD_DEFINITIONS(-Wformat-nonliteral)
    ADD_DEFINITIONS(-Wformat-security)
    # strdup, sigaction need -D_XOPEN_SOURCE=700 or -D_POSIX_C_SOURCE=200809L
    # strsep, strcasecmp need
    ADD_DEFINITIONS(-D_DEFAULT_SOURCE)
    # older glibc might need -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809C

    # CMake Release default for GCC/Clang is "-O3 -DNDEBUG"
    # set(CMAKE_C_FLAGS_RELEASE -O2)
    # CMake Debug default for GCC/Clang is "-g -DNDEBUG"
    # set(CMAKE_C_FLAGS_DEBUG -g3 -O0)
endif()
if ("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
    # make sure we don't accidentally copy more than an int
    ADD_DEFINITIONS(-Wlarge-by-value-copy=8)
    ADD_DEFINITIONS(-Weverything)
    ADD_DEFINITIONS(-Wno-documentation-unknown-command)
    ADD_DEFINITIONS(-Wno-documentation-deprecated-sync)
    ADD_DEFINITIONS(-Wno-documentation)
    ADD_DEFINITIONS(-Wno-missing-noreturn)
    ADD_DEFINITIONS(-Wno-bad-function-cast)
endif()

########################################################################
# Build executables
########################################################################
add_executable(tx_sdr src/tx_sdr.c)
target_link_libraries(tx_sdr ${TX_TOOLS_LIBS})

add_executable(fsk_gen src/fsk_gen.c src/argparse.c)
if(UNIX)
target_link_libraries(fsk_gen m)
endif()

########################################################################
# Install executables
########################################################################
install(TARGETS tx_sdr DESTINATION bin)
