cmake_minimum_required (VERSION 3.9)
cmake_policy(VERSION 3.9)
project (Fleece)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS
    $<$<CONFIG:Debug>:DEBUG>
)

if(MSVC)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/platform_win.cmake")
elseif(APPLE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/platform_apple.cmake")
elseif(ANDROID OR "${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/platform_linux.cmake")
else()
    message(FATAL_ERROR "Unsupported platform ${CMAKE_SYSTEM_NAME}!")
endif()

set_source_files(RESULT FLEECE_SRC)
add_library(FleeceObjects OBJECT ${FLEECE_SRC})
add_library(Fleece        SHARED  $<TARGET_OBJECTS:FleeceObjects>)
add_library(FleeceStatic  STATIC  $<TARGET_OBJECTS:FleeceObjects>)
target_compile_definitions(FleeceObjects PRIVATE FLEECE_EXPORTS)

# "FleeceBase" static lib for clients that just need support stuff like slice, varint, RefCounted...
set_base_platform_files(RESULT FLEECE_BASE_PLATFORM_SRC)
set(FLEECE_BASE_SRC Fleece/API_Impl/FLSlice.cc
                    Fleece/Support/Backtrace.cc
                    Fleece/Support/Base64.cc
                    Fleece/Support/FleeceException.cc
                    Fleece/Support/InstanceCounted.cc
                    Fleece/Support/NumConversion.cc
                    Fleece/Support/ParseDate.cc
                    Fleece/Support/RefCounted.cc
                    Fleece/Support/Writer.cc
                    Fleece/Support/betterassert.cc
                    Fleece/Support/slice_stream.cc
                    Fleece/Support/varint.cc
                    vendor/libb64/cdecode.c
                    vendor/libb64/cencode.c
                    vendor/SwiftDtoa/SwiftDtoa.cc
                    ${FLEECE_BASE_PLATFORM_SRC})

add_library(FleeceBase    STATIC  ${FLEECE_BASE_SRC})

# Command-Line Tool
add_executable(fleeceTool Tool/fleece_tool.cc)
target_link_libraries(fleeceTool FleeceObjects)

# Fleece Tests
set_test_source_files(RESULT FLEECE_TEST_SRC)
add_executable(FleeceTests EXCLUDE_FROM_ALL ${FLEECE_TEST_SRC})
setup_test_build()
target_include_directories(FleeceTests PRIVATE Tests vendor/catch)
target_link_libraries(FleeceTests FleeceObjects)
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    target_link_libraries(FleeceTests  "pthread")
endif()

file(COPY Tests/1000people.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Tests)
file(COPY Tests/1person.fleece DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Tests)
file(COPY Tests/1person.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Tests)

foreach(platform FleeceObjects Fleece FleeceStatic FleeceBase fleeceTool FleeceTests)
    target_include_directories(
        ${platform} PRIVATE
        API
        Fleece/API_Impl
        Fleece/Core
        Fleece/Integration
        Fleece/Mutable
        Fleece/Support
        Fleece/Tree
        Experimental
        vendor/date/include
        vendor/jsonsl
        vendor/libb64
        vendor/SwiftDtoa
        vendor/wyhash
    )

    target_compile_definitions(
        ${platform} PRIVATE
        HAS_UNCAUGHT_EXCEPTIONS # date.h use std::uncaught_exceptions instead of std::uncaught_exception
        __STDC_FORMAT_MACROS # To use PRIx64 and friends for formatting variable size types in printf
        _LIBCPP_REMOVE_TRANSITIVE_INCLUDES # Stop libc++ headers from including extra headers
    )
endforeach()

setup_build()
