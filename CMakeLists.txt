cmake_minimum_required (VERSION 3.9)
cmake_policy(VERSION 3.9)
project (Fleece)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS
    $<$<CONFIG:Debug>:DEBUG>
)

option(FLEECE_WARNINGS_HARDCORE "Enables tons of warnings and makes them errors (Clang only)" OFF)
option(FLEECE_SANITIZE "Enables address and undefined-behavior sanitizers (Clang only)" OFF)

if(MSVC)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/platform_win.cmake")
elseif(APPLE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/platform_apple.cmake")
elseif(ANDROID OR "${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/platform_linux.cmake")
else()
    message(FATAL_ERROR "Unsupported platform ${CMAKE_SYSTEM_NAME}!")
endif()

set(FLEECE_CXX_WARNINGS "")
if(FLEECE_WARNINGS_HARDCORE)
    if (CMAKE_CXX_COMPILER_ID MATCHES Clang)
        set(FLEECE_CXX_WARNINGS
            -Werror
            -Weverything            # "WARN ALL THE THINGS!!!"
            -Wformat=2
            # Disabled C++ warnings:
            -Wno-nullable-to-nonnull-conversion # TODO: "implicit conversion from nullable pointer to non-nullable pointer type"
            -Wno-sign-compare # TODO "comparison of integers of different signs"
            -Wno-undefined-func-template # TODO: Can't figure out how to fix MValue.hh
            -Wno-implicit-int-float-conversion #TODO: diff_match_patch.hh
            -Wno-alloca
            -Wno-atomic-implicit-seq-cst # "implicit use of sequentially-consistent atomic may incur stronger memory barriers than necessary"
            -Wno-c++98-compat
            -Wno-c++98-compat-pedantic
            -Wno-c99-extensions
            -Wno-cast-align # "cast from X* to Y* increases required alignment"
            -Wno-cast-qual  # "cast drops const qualifier"
            -Wno-covered-switch-default # "default label in switch which covers all enumeration values"
            -Wno-ctad-maybe-unsupported # "X may not intend to support class template argument deduction"
            -Wno-custom-atomic-properties # "[Obj-C] atomic by default property 'X' has a user defined getter"
            -Wno-date-time # "expansion of date or time macro is not reproducible"
            -Wno-direct-ivar-access # "[Obj-C] instance variable '_x' is being directly accessed"
            -Wno-double-promotion # "implicit conversion increases floating-point precision: 'float' to 'double'"
            -Wno-exit-time-destructors # "declaration requires an exit-time destructor"
            -Wno-float-equal
            -Wno-format-pedantic # "format specifies type 'void *' but the argument has type 'C4Document *'"
            -Wno-global-constructors
            -Wno-gnu-anonymous-struct # "anonymous structs are a GNU extension"
            -Wno-gnu-zero-variadic-macro-arguments # "token pasting of ',' and __VA_ARGS__ is a GNU extension"
            -Wno-missing-field-initializers # "missing field 'x' initializer"
            -Wno-nested-anon-types # "anonymous types declared in an anonymous union are an extension"
            -Wno-nullability-extension
            -Wno-objc-messaging-id
            -Wno-old-style-cast
            -Wno-padded # "padding size of X with N bytes to alignment boundary"
            -Wno-sign-conversion
            -Wno-suggest-destructor-override # "'~Foo' overrides a destructor but is not marked 'override'"
            -Wno-super-class-method-mismatch # Obj-C "method parameter type does not match super class method parameter type"
            -Wno-switch-enum
            -Wno-undef      # `#if X` where X isn't defined
            -Wno-unused-macros
            -Wno-unused-parameter # Unused fn parameter
            -Wno-weak-vtables # "Class has no out-of-line virtual method definitions; its vtable will be emitted in every translation unit"
            -Wno-reserved-identifier
            -Wno-unsafe-buffer-usage
            -Wno-zero-as-null-pointer-constant # Using 0 instead of nullptr. Some of our dependencies do this in headers.
        )
    endif()
endif()


if (LITECORE_SANITIZE AND NOT CODE_COVERAGE_ENABLED AND (CMAKE_CXX_COMPILER_ID MATCHES Clang))
    set(FLEECE_COMPILE_OPTIONS
            -fstack-protector
            -fsanitize=address
            -fsanitize-address-use-after-return=always
            -fsanitize-address-use-after-scope
            -fsanitize=undefined
            -fsanitize=nullability
            -fno-sanitize-recover=all   # Always exit after UBSan warning
            # Note: _FORTIFY_SOURCE is incompatible with ASan; defining it will cause build errors
    )
    set(FLEECE_LINK_OPTIONS
        -fsanitize=address
        -fsanitize=undefined
    )
elseif(NOT WIN32)
    set(FLEECE_COMPILE_OPTIONS
        -fstack-protector
        -D_FORTIFY_SOURCE=2
    )
    set(FLEECE_LINK_OPTIONS "")
endif()

set_source_files(RESULT FLEECE_SRC)
add_library(FleeceObjects OBJECT ${FLEECE_SRC})
add_library(Fleece        SHARED  $<TARGET_OBJECTS:FleeceObjects>)
add_library(FleeceStatic  STATIC  $<TARGET_OBJECTS:FleeceObjects>)
target_compile_definitions(FleeceObjects PRIVATE FLEECE_EXPORTS)
target_link_options(Fleece PRIVATE ${FLEECE_LINK_OPTIONS})

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
target_compile_definitions(FleeceBase PRIVATE FLEECE_EXPORTS)

# Command-Line Tool
add_executable(fleeceTool Tool/fleece_tool.cc)
target_link_libraries(fleeceTool FleeceObjects)
target_link_options(fleeceTool PRIVATE ${FLEECE_LINK_OPTIONS})

# Fleece Tests
set_test_source_files(RESULT FLEECE_TEST_SRC)
add_executable(FleeceTests EXCLUDE_FROM_ALL ${FLEECE_TEST_SRC})
setup_test_build()
target_include_directories(FleeceTests PRIVATE Tests vendor/catch)
target_link_libraries(FleeceTests FleeceObjects)
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    target_link_libraries(FleeceTests  "pthread")
endif()
target_link_options(FleeceTests PRIVATE ${FLEECE_LINK_OPTIONS})

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
        vendor/date/include
        vendor/jsonsl
        vendor/libb64
        vendor/SwiftDtoa
        vendor/wyhash
    )

    target_compile_options(
        ${platform} PRIVATE
        ${FLEECE_COMPILE_OPTIONS}
        $<$<COMPILE_LANGUAGE:CXX>:${FLEECE_CXX_WARNINGS}>
    )

    target_compile_definitions(
        ${platform} PRIVATE
        HAS_UNCAUGHT_EXCEPTIONS # date.h use std::uncaught_exceptions instead of std::uncaught_exception
        __STDC_FORMAT_MACROS # To use PRIx64 and friends for formatting variable size types in printf
        _LIBCPP_REMOVE_TRANSITIVE_INCLUDES # Stop libc++ headers from including extra headers
    )
endforeach()

setup_build()
