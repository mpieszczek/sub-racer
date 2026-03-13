cmake_minimum_required(VERSION 3.24)

# Fetch Unity
include(FetchContent)
FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG v2.6.0
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
)
FetchContent_MakeAvailable(unity)

# Get Unity source directory
set(UNITY_SRC_DIR ${unity_SOURCE_DIR}/src)

# Test source files
set(TEST_SOURCES
    test_subtitle.c
    ${CMAKE_SOURCE_DIR}/src/time_utils.c
    ${CMAKE_SOURCE_DIR}/src/subtitle.c
    ${UNITY_SRC_DIR}/unity.c
)

# Create test executable
add_executable(sub-racer-tests ${TEST_SOURCES})

# Include directories
target_include_directories(sub-racer-tests PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
    ${UNITY_SRC_DIR}
)

# Add to ctest
enable_testing()
add_test(NAME sub-racer-tests COMMAND sub-racer-tests)
