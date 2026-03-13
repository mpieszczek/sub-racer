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

# libmpv configuration
set(LIBMPV_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/external/libmpv/include)
set(LIBMPV_LIBRARY ${CMAKE_SOURCE_DIR}/external/libmpv/libmpv.dll.a)

# raygui - use fetched version
set(RAYGUI_INCLUDE_DIR ${CMAKE_BINARY_DIR}/_deps/raygui-src/src)

target_include_directories(${PROJECT_NAME} PRIVATE ${LIBMPV_INCLUDE_DIR} ${RAYGUI_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} ${LIBMPV_LIBRARY})

# Copy libmpv DLL and LICENSE to output directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/external/libmpv/libmpv-2.dll
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/libmpv-2.dll
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/external/libmpv/LICENSE.LGPL
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/LICENSE.lgpl
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
