cmake_minimum_required(VERSION 3.24...3.30)
project(sub-racer)

include(FetchContent)

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Dependencies
set(RAYLIB_VERSION 5.5)
set(RAYGUI_VERSION 4.0)

FetchContent_Declare(
    raylib
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    URL https://github.com/raysan5/raylib/archive/refs/tags/${RAYLIB_VERSION}.tar.gz
    FIND_PACKAGE_ARGS
)

FetchContent_Declare(
    raygui
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    URL https://github.com/raysan5/raygui/archive/refs/tags/${RAYGUI_VERSION}.tar.gz
    FIND_PACKAGE_ARGS
)

FetchContent_MakeAvailable(raylib raygui)

# Our Project
add_executable(${PROJECT_NAME})
add_subdirectory(src)

# libmpv configuration - try system libmpv first, fall back to local
set(USE_SYSTEM_LIBMPV OFF)

# Try to find system libmpv using find_path and find_library
# This works on Linux without needing cmake config files
find_path(LIBMPV_INCLUDE_DIR NAMES client.h PATHS /usr/include /usr/local/include NO_DEFAULT_PATH)
find_library(LIBMPV_LIBRARY NAMES mpv PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib NO_DEFAULT_PATH)

if(LIBMPV_INCLUDE_DIR AND LIBMPV_LIBRARY)
    message(STATUS "Using system libmpv")
    message(STATUS "  Include: ${LIBMPV_INCLUDE_DIR}")
    message(STATUS "  Library: ${LIBMPV_LIBRARY}")
    set(USE_SYSTEM_LIBMPV ON)
else()
    # Fallback to local libmpv (for Windows development)
    set(LOCAL_LIBMPV_INCLUDE "${CMAKE_SOURCE_DIR}/external/libmpv/include")
    set(LOCAL_LIBMPV_LIB "${CMAKE_SOURCE_DIR}/external/libmpv/libmpv.dll.a")
    if(EXISTS "${LOCAL_LIBMPV_INCLUDE}" AND EXISTS "${LOCAL_LIBMPV_LIB}")
        message(STATUS "Using local libmpv from external/")
        set(LIBMPV_INCLUDE_DIR "${LOCAL_LIBMPV_INCLUDE}")
        set(LIBMPV_LIBRARY "${LOCAL_LIBMPV_LIB}")
    else()
        message(FATAL_ERROR "libmpv not found! Install libmpv-dev (Linux), mpv (macOS), or place libmpv in external/libmpv/")
    endif()
endif()

# raygui - use fetched version
set(RAYGUI_INCLUDE_DIR ${CMAKE_BINARY_DIR}/_deps/raygui-src/src)

target_include_directories(${PROJECT_NAME} PRIVATE ${LIBMPV_INCLUDE_DIR} ${RAYGUI_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} ${LIBMPV_LIBRARY})

# Copy libmpv DLL and LICENSE to output directory (only for local build)
if(NOT USE_SYSTEM_LIBMPV)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_SOURCE_DIR}/external/libmpv/libmpv-2.dll
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/libmpv-2.dll
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_SOURCE_DIR}/external/libmpv/LICENSE.LGPL
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/LICENSE.lgpl
    )
endif()

# Always copy LICENSE
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/LICENSE
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/LICENSE
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PROJECT_NAME})

set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:${PROJECT_NAME}>)

if ("${PLATFORM}" STREQUAL "Web")
    add_custom_command(
        TARGET ${PROJECT_NAME} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/src/resources $<TARGET_FILE_DIR:${PROJECT_NAME}>/../resources
    )
    #DEPENDS ${PROJECT_NAME}
else()
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/src/resources $<TARGET_FILE_DIR:${PROJECT_NAME}>/resources
    )
    #DEPENDS ${PROJECT_NAME}
endif()

#set(raylib_VERBOSE 1)
target_link_libraries(${PROJECT_NAME} raylib)

# Web Configurations
if ("${PLATFORM}" STREQUAL "Web")
    # Tell Emscripten to build an example.html file.
    set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".html")
    target_link_options(${PROJECT_NAME} PUBLIC -sUSE_GLFW=3 PUBLIC --preload-file resources)
endif()

# Checks if OSX and links appropriate frameworks (Only required on MacOS)
if (APPLE)
    target_link_libraries(${PROJECT_NAME} "-framework IOKit")
    target_link_libraries(${PROJECT_NAME} "-framework Cocoa")
    target_link_libraries(${PROJECT_NAME} "-framework OpenGL")
endif()

# Unit tests
add_subdirectory(test)
