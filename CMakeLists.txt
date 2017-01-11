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
    aux_source_directory(ObjC/  OBJC_SRC)
    set(FLEECE_SRC ${FLEECE_SRC} ${OBJC_SRC})
    set_source_files_properties(ObjC/Fleece+CoreFoundation.mm  PROPERTIES
                                COMPILE_FLAGS  -Wno-return-type-c-linkage)
endif()

include_directories("Fleece" 
                    "vendor/libb64" 
                    "vendor/jsonsl" )

if(!MSVC)
	set_source_files_properties(Fleece/Fleece_C_impl.cc  PROPERTIES
                            	COMPILE_FLAGS  -Wno-return-type-c-linkage)
else()
    include_directories("MSVC")
    set(FLEECE_SRC ${FLEECE_SRC} MSVC/memmem.cc)
endif()

add_library(Fleece        SHARED  ${FLEECE_SRC})
add_library(FleeceStatic  STATIC  ${FLEECE_SRC})

if (APPLE)
    target_link_libraries(Fleece  
                          "-framework CoreFoundation" 
                          "-framework Foundation")
endif()
