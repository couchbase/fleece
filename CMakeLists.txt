cmake_minimum_required (VERSION 2.6)
cmake_policy(VERSION 3.1)
project (Fleece)

set(COMPILE_FLAGS   "${COMPILE_FLAGS}   -Wall -Werror -Wno-invalid-offsetof")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Debug/Release builds
if(CMAKE_BUILD_TYPE STREQUAL Debug)
    add_definitions(-DDEBUG)
endif()

aux_source_directory(Fleece/Core/        FLEECE_CORE_SRC)
aux_source_directory(Fleece/Integration/ FLEECE_INTEGRATION_SRC)
aux_source_directory(Fleece/Mutable/     FLEECE_MUTABLE_SRC)
aux_source_directory(Fleece/Support/     FLEECE_SUPPORT_SRC)
aux_source_directory(Fleece/Tree/        FLEECE_TREE_SRC)
set(FLEECE_SRC ${FLEECE_CORE_SRC} ${FLEECE_INTEGRATION_SRC} ${FLEECE_MUTABLE_SRC}
               ${FLEECE_SUPPORT_SRC} ${FLEECE_TREE_SRC}
               vendor/jsonsl/jsonsl.c
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
    set(FLEECE_SRC ${FLEECE_SRC} MSVC/memmem.cc MSVC/vasprintf-msvc.c)
endif()

include_directories(Fleece
                    Fleece/Core
                    Fleece/Integration
                    Fleece/Mutable
                    Fleece/Support
                    Fleece/Tree
                    Experimental
                    "vendor/libb64"
                    "vendor/jsonsl" )

if(!MSVC)
	set_source_files_properties(Fleece/Core/Fleece_C_impl.cc  PROPERTIES)
endif()

add_library(Fleece        SHARED  ${FLEECE_SRC})
add_library(FleeceStatic  STATIC  ${FLEECE_SRC})

# Command-Line Tool
add_executable(fleeceTool Tool/fleece_tool.cc)
target_link_libraries(fleeceTool FleeceStatic)

# Fleece Tests
aux_source_directory(Tests FLEECE_TEST_SRC)
if(NOT APPLE)
    LIST(REMOVE_ITEM FLEECE_TEST_SRC Tests/ObjCTests.mm)
endif()
include_directories(Tests vendor/catch)
add_executable(FleeceTests EXCLUDE_FROM_ALL ${FLEECE_TEST_SRC})
target_link_libraries(FleeceTests FleeceStatic)
file(COPY Tests/1000people.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Tests)
file(COPY Tests/1person.fleece DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Tests)
file(COPY Tests/1person.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/Tests)

if (APPLE)
    set_target_properties(Fleece PROPERTIES LINK_FLAGS
                          "-exported_symbols_list ${PROJECT_SOURCE_DIR}/Fleece/Support/Fleece.exp")
    target_link_libraries(Fleece
                           "-framework CoreFoundation"
                           "-framework Foundation")
    target_link_libraries(FleeceTests
                            "-framework CoreFoundation"
                            "-framework Foundation")
endif()
