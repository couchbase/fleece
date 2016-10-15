cmake_minimum_required (VERSION 2.6)
project (Fleece)

set(COMPILE_FLAGS   "${COMPILE_FLAGS}   -Wall -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

# Debug/Release builds
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG}   -O0    -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Ofast -DNDEBUG")

aux_source_directory(Fleece  FLEECE_SRC)
set(FLEECE_SRC ${FLEECE_SRC} vendor/jsonsl/jsonsl.c
                             vendor/libb64/cdecode.c
                             vendor/libb64/cencode.c)

if (APPLE)
    aux_source_directory(ObjC/  OBJC_SRC)
    set(FLEECE_SRC ${FLEECE_SRC} ${OBJC_SRC})
    set_source_files_properties(ObjC/Fleece+CoreFoundation.mm  PROPERTIES
                                COMPILE_FLAGS  -Wno-return-type-c-linkage)
endif()

include_directories("Fleece" 
                    "vendor/libb64" 
                    "vendor/jsonsl" )

set_source_files_properties(Fleece/Fleece_C_impl.cc  PROPERTIES
                            COMPILE_FLAGS  -Wno-return-type-c-linkage)

add_library(Fleece        SHARED  ${FLEECE_SRC})
add_library(FleeceStatic  STATIC  ${FLEECE_SRC})

if (APPLE)
    target_link_libraries(Fleece  
                          "-framework CoreFoundation" 
                          "-framework Foundation")
endif()
