cmake_minimum_required (VERSION 2.6)
cmake_policy(VERSION 3.1)
project (Fleece)

set(COMPILE_FLAGS   "${COMPILE_FLAGS}   -Wall -Werror")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Debug/Release builds
if(CMAKE_BUILD_TYPE STREQUAL Debug)
    add_definitions(-DDEBUG)
endif()

aux_source_directory(Fleece  FLEECE_SRC)
set(FLEECE_SRC ${FLEECE_SRC} vendor/jsonsl/jsonsl.c
                             vendor/libb64/cdecode.c
                             vendor/libb64/cencode.c)

if (APPLE AND NOT ANDROID)
    include_directories(ObjC/)
    aux_source_directory(ObjC/  OBJC_SRC)
    set(FLEECE_SRC ${FLEECE_SRC} ${OBJC_SRC})
    set_source_files_properties(ObjC/Fleece+CoreFoundation.mm  PROPERTIES
                                COMPILE_FLAGS  -Wno-return-type-c-linkage)
elseif(WIN32)
    include_directories("MSVC")
    set(FLEECE_SRC ${FLEECE_SRC} MSVC/memmem.cc)
elseif(ANDROID)
    include_directories(Fleece/Mutable)
    aux_source_directory(Fleece/Mutable  MUTABLE_SRC)
    set(FLEECE_SRC ${FLEECE_SRC} ${MUTABLE_SRC})
endif()

include_directories("Fleece"
                    "vendor/libb64"
                    "vendor/jsonsl" )

if(!MSVC)
	set_source_files_properties(Fleece/Fleece_C_impl.cc  PROPERTIES)
endif()

add_library(Fleece        SHARED  ${FLEECE_SRC})
add_library(FleeceStatic  STATIC  ${FLEECE_SRC})

add_executable(fleece Tool/fleece_tool.cc ${FLEECE_SRC})

# Fleece Tests
aux_source_directory(Tests FLEECE_TEST_SRC)
if(NOT APPLE)
    LIST(REMOVE_ITEM FLEECE_TEST_SRC Tests/ObjCTests.mm)
endif()
include_directories(Tests vendor/catch)
add_executable(FleeceTests EXCLUDE_FROM_ALL ${FLEECE_TEST_SRC})
target_link_libraries(FleeceTests FleeceStatic)
target_compile_definitions(FleeceTests PRIVATE FLEECE_TEST_PROGRAM)
file(COPY Tests/1000people.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Tests)
file(COPY Tests/1person.fleece DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Tests)
file(COPY Tests/1person.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Tests)

if (APPLE)
    target_link_libraries(Fleece
                           "-framework CoreFoundation"
                           "-framework Foundation")
    target_link_libraries(FleeceTests
                            "-framework CoreFoundation"
                            "-framework Foundation")
endif()
