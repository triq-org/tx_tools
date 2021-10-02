########################################################################
# Project setup
########################################################################
cmake_minimum_required(VERSION 2.8.0)
project(tx_tools C)

#select the release build type by default to get optimization flags
if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE "Release")
   message(STATUS "Build type not specified: defaulting to release.")
endif(NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE} CACHE STRING "")

#local include directories first
include_directories(${PROJECT_SOURCE_DIR}/src/utils)

#include local cmake modules
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

########################################################################
# Find SoapySDR
########################################################################
find_package(SoapySDR "0.6" NO_MODULE)
if(SoapySDR_FOUND)
    message(STATUS "SoapySDR_INCLUDE_DIRS: ${SoapySDR_INCLUDE_DIRS}")
    message(STATUS "SoapySDR_LIBRARIES: ${SoapySDR_LIBRARIES}")
    message(STATUS "SoapySDR_DEFINITIONS: ${SoapySDR_DEFINITIONS}")
    include_directories(${SoapySDR_INCLUDE_DIRS})
    add_definitions(-DHAS_SOAPY)
    list(APPEND TX_TOOLS_LIBS ${SoapySDR_LIBRARIES})
    list(APPEND COMMON_SOURCES src/sdr/sdr_soapy.c)
else()
    message(STATUS "SoapySDR development files not found...")
endif()

########################################################################
# Find LimeSuite
########################################################################
find_package(LimeSuite "19.04" NO_MODULE)
if(LimeSuite_FOUND)
    message(STATUS "LimeSuite_INCLUDE_DIRS: ${LimeSuite_INCLUDE_DIRS}")
    message(STATUS "LimeSuite_LIBRARIES: ${LimeSuite_LIBRARIES}")
    message(STATUS "LimeSuite_DEFINITIONS: ${LimeSuite_DEFINITIONS}")
    include_directories(${LimeSuite_INCLUDE_DIRS})
    add_definitions(-DHAS_LIME)
    list(APPEND TX_TOOLS_LIBS ${LimeSuite_LIBRARIES})
    list(APPEND COMMON_SOURCES src/sdr/sdr_lime.c)
else()
    message(STATUS "LimeSuite development files not found...")
endif()

########################################################################
# Find LibIIO
########################################################################
find_package(LibIIO 0.9)
if(LibIIO_FOUND)
    message(STATUS "LibIIO_INCLUDE_DIRS: ${LibIIO_INCLUDE_DIRS}")
    message(STATUS "LibIIO_LIBRARIES: ${LibIIO_LIBRARIES}")
    message(STATUS "LibIIO_DEFINITIONS: ${LibIIO_DEFINITIONS}")
    include_directories(${LibIIO_INCLUDE_DIRS})
    add_definitions(-DHAS_IIO)
    list(APPEND TX_TOOLS_LIBS ${LibIIO_LIBRARIES})
    list(APPEND COMMON_SOURCES src/sdr/sdr_pluto.c)
else()
    message(STATUS "LibIIO development files not found...")
endif()

########################################################################
# Find LibAD9361
########################################################################
find_package(LibAD9361)
if(LibAD9361_FOUND)
    message(STATUS "LibAD9361_INCLUDE_DIRS: ${LibAD9361_INCLUDE_DIRS}")
    message(STATUS "LibAD9361_LIBRARIES: ${LibAD9361_LIBRARIES}")
    message(STATUS "LibAD9361_DEFINITIONS: ${LibAD9361_DEFINITIONS}")
    include_directories(${LibAD9361_INCLUDE_DIRS})
    add_definitions(-DHAS_AD9361_IIO)
    list(APPEND TX_TOOLS_LIBS ${LibAD9361_LIBRARIES})
else()
    message(STATUS "LibAD9361 development files not found...")
endif()

if(NOT SoapySDR_FOUND AND NOT LimeSuite_FOUND AND NOT LibIIO_FOUND)
    message(FATAL_ERROR "At least one of SoapySDR, LimeSuite, or LibIIO (Pluto) needed...")
endif()

########################################################################
# Dependencies
########################################################################

# Link with libm when available
find_library(MATH_LIBRARY m)
message(STATUS "MATH_LIBRARY: ${MATH_LIBRARY}")
if(MATH_LIBRARY)
    list(APPEND TX_TOOLS_LIBS -lm)
endif()

# Link with librt (for clock_*) when available
find_library(RT_LIBRARY rt)
message(STATUS "RT_LIBRARY: ${RT_LIBRARY}")
if(RT_LIBRARY)
    list(APPEND TX_TOOLS_LIBS -Wl,--no-as-needed -lrt)
endif()

# Link with pthreads
set(THREADS_USE_PTHREADS_WIN32 true)
find_package(Threads)
if(NOT Threads_FOUND)
    message(FATAL_ERROR "pthreads development files not found...")
endif()
include_directories(${THREADS_PTHREADS_INCLUDE_DIR})
list(APPEND TX_TOOLS_LIBS ${CMAKE_THREAD_LIBS_INIT})
message(STATUS "THREADS_PTHREADS_INCLUDE_DIR: ${THREADS_PTHREADS_INCLUDE_DIR}")
message(STATUS "CMAKE_THREAD_LIBS_INIT: ${CMAKE_THREAD_LIBS_INIT}")

#windows getopt compatibility
if(WIN32)
    include_directories(${PROJECT_SOURCE_DIR}/src/getopt)
    list(APPEND COMMON_SOURCES src/getopt/getopt.c)
endif()

########################################################################
# Helper library
########################################################################
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
list(APPEND COMMON_SOURCES src/sdr/sdr_backend.c src/tx_lib.c)
list(APPEND COMMON_SOURCES src/read_text.c src/tone_text.c src/code_text.c src/pulse_text.c src/transform.c src/iq_render.c src/sample.c)
list(APPEND COMMON_SOURCES src/utils/optparse.c)
add_library(common STATIC ${COMMON_SOURCES})
list(INSERT TX_TOOLS_LIBS 0 common)

########################################################################
# Compiler specific setup
########################################################################
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_C_COMPILER_ID}" MATCHES "Clang") AND NOT WIN32)
    ADD_DEFINITIONS(-Wall)
    ADD_DEFINITIONS(-Wextra)
    ADD_DEFINITIONS(-Wsign-compare)
    ADD_DEFINITIONS(-Winline)
    ADD_DEFINITIONS(-Wno-unused)
    ADD_DEFINITIONS(-Wno-unused-parameter)
    ADD_DEFINITIONS(-Wno-missing-field-initializers)
    ADD_DEFINITIONS(-pedantic)
    ADD_DEFINITIONS(-Wdouble-promotion)
    ADD_DEFINITIONS(-Wformat-nonliteral)
    ADD_DEFINITIONS(-Wformat-security)
    ADD_DEFINITIONS(-Wno-padded)
    # strdup, sigaction need -D_XOPEN_SOURCE=700 or -D_POSIX_C_SOURCE=200809L
    # strsep, strcasecmp need
    ADD_DEFINITIONS(-D_DEFAULT_SOURCE)
    # older glibc might need -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809C

    # CMake Release default for GCC/Clang is "-O3 -DNDEBUG"
    # set(CMAKE_C_FLAGS_RELEASE -O2)
    # CMake Debug default for GCC/Clang is "-g -DNDEBUG"
    # set(CMAKE_C_FLAGS_DEBUG -g3 -O0)

    # make use of ASAN
    set(CMAKE_C_FLAGS_DEBUG "-ggdb -fsanitize=undefined -fsanitize=address -fno-omit-frame-pointer")
endif()
if("${CMAKE_C_COMPILER_ID}" MATCHES "Clang")
    # make sure we don't accidentally copy more than an int
    ADD_DEFINITIONS(-Wlarge-by-value-copy=8)
    ADD_DEFINITIONS(-Weverything)
    ADD_DEFINITIONS(-Wno-documentation-unknown-command)
    ADD_DEFINITIONS(-Wno-documentation-deprecated-sync)
    ADD_DEFINITIONS(-Wno-documentation)
    ADD_DEFINITIONS(-Wno-missing-noreturn)
    ADD_DEFINITIONS(-Wno-bad-function-cast)
endif()

# skip warnings from iio.h and ad9361.h
if("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_C_COMPILER_ID}" MATCHES "Clang")
	SET_SOURCE_FILES_PROPERTIES(src/sdr/sdr_pluto.c PROPERTIES COMPILE_FLAGS "-w -Wno-reserved-id-macro")
endif()

########################################################################
# Build executables
########################################################################
add_executable(tx_sdr src/tx_sdr.c)
target_link_libraries(tx_sdr ${TX_TOOLS_LIBS})

add_executable(pulse_gen src/pulse_gen.c src/read_text.c src/tone_text.c src/pulse_text.c src/transform.c src/utils/optparse.c src/iq_render.c src/sample.c)
if(UNIX)
target_link_libraries(pulse_gen m)
endif()

add_executable(code_gen src/code_gen.c src/read_text.c src/tone_text.c src/code_text.c src/transform.c src/utils/optparse.c src/iq_render.c src/sample.c)
if(UNIX)
target_link_libraries(code_gen m)
endif()

add_executable(code_dump src/code_dump.c src/read_text.c src/tone_text.c src/code_text.c src/transform.c src/sample.c)

add_executable(example_gen src/example_gen.c src/read_text.c src/tone_text.c src/code_text.c src/transform.c src/sample.c)

add_executable(fast_osc_tests src/fast_osc_tests.c)
if(UNIX)
target_link_libraries(fast_osc_tests m)
endif()

add_executable(encode_ascii src/transform.c)
target_compile_definitions(encode_ascii PRIVATE -DPROG_ASCII)

add_executable(encode_hex src/transform.c)
target_compile_definitions(encode_hex PRIVATE -DPROG_HEX)

add_executable(encode_dmc src/transform.c)
target_compile_definitions(encode_dmc PRIVATE -DPROG_DMC)

add_executable(encode_mc src/transform.c)
target_compile_definitions(encode_mc PRIVATE -DPROG_MC)

add_executable(encode_imc src/transform.c)
target_compile_definitions(encode_imc PRIVATE -DPROG_IMC)

add_executable(byte-stat src/byte-stat.c)

########################################################################
# Install executables
########################################################################
install(TARGETS tx_sdr DESTINATION bin)

########################################################################
# Build tests with analyzer
########################################################################
option(BUILD_TESTING_ANALYZER "Build the testing tree with static
analyzer (requires Clang)" OFF)

########################################################################
# Build tests
########################################################################
include(CTest) # note: this adds a BUILD_TESTING which defaults to ON

########################################################################
# Add subdirectories
########################################################################
if(BUILD_TESTING)
    add_subdirectory(tests)
endif(BUILD_TESTING)
