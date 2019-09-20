cmake_minimum_required (VERSION 3.9)
cmake_policy(VERSION 3.9)
project (Fleece)

set(COMPILE_FLAGS   "${COMPILE_FLAGS}   -Wall -Werror -Wno-invalid-offsetof")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
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

if(CMAKE_COMPILER_IS_GNUCC)
   # Suppress a note about GCC 7 ABI changes
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-psabi")
endif()

add_subdirectory(vendor/double-conversion)
set_source_files(RESULT FLEECE_SRC)
add_library(Fleece        SHARED  ${FLEECE_SRC})
target_link_libraries(Fleece double-conversion)
add_library(FleeceStatic  STATIC  ${FLEECE_SRC})
target_link_libraries(FleeceStatic double-conversion)

# "FleeceBase" static lib for clients that just need support stuff like slice, varint, RefCounted...
set_base_platform_files(RESULT FLEECE_BASE_PLATFORM_SRC)
set(FLEECE_BASE_SRC Fleece/Support/Backtrace.cc
                    Fleece/Support/FleeceException.cc
                    Fleece/Support/InstanceCounted.cc
                    Fleece/Support/ParseDate.cc
                    Fleece/Support/RefCounted.cc
                    Fleece/Support/Writer.cc
                    Fleece/Support/betterassert.cc
                    Fleece/Support/slice.cc
                    Fleece/Support/varint.cc
                    vendor/libb64/cdecode.c
                    vendor/libb64/cencode.c
                    ${FLEECE_BASE_PLATFORM_SRC})

add_library(FleeceBase    STATIC  ${FLEECE_BASE_SRC})

setup_build()

# Command-Line Tool
add_executable(fleeceTool Tool/fleece_tool.cc)
target_link_libraries(fleeceTool FleeceStatic)

# Fleece Tests
set_test_source_files(RESULT FLEECE_TEST_SRC)
add_executable(FleeceTests EXCLUDE_FROM_ALL ${FLEECE_TEST_SRC})
setup_test_build()
target_include_directories(FleeceTests PRIVATE Tests vendor/catch)
target_link_libraries(FleeceTests FleeceStatic)
file(COPY Tests/1000people.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Tests)
file(COPY Tests/1person.fleece DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Tests)
file(COPY Tests/1person.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Tests)

foreach(platform Fleece FleeceStatic FleeceBase fleeceTool FleeceTests)
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
        vendor/libb64
        vendor/jsonsl
    )
endforeach()
