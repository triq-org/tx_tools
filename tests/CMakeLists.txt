########################################################################
# Define clang static analyzer checks
########################################################################
if(BUILD_TESTING_ANALYZER)
file(GLOB ANALYZER_CHECK_FILES ../src/*.h ../src/*.c ../src/utils/optparse.h ../src/utils/optparse.c)
add_test(clang-analyzer
    ${CMAKE_CURRENT_SOURCE_DIR}/exitcode-for-output.sh
    clang
    -I${CMAKE_CURRENT_SOURCE_DIR}/../src/utils
    -I${SoapySDR_INCLUDE_DIRS}
    --analyze
    -Xanalyzer -analyzer-output=text
    -Xanalyzer -analyzer-disable-checker=deadcode.DeadStores
    ${ANALYZER_CHECK_FILES})
endif()
