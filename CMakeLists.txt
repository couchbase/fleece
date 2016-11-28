cmake_minimum_required (VERSION 2.6)
project (Fleece)

set(COMPILE_FLAGS   "${COMPILE_FLAGS}   -Wall -Werror")
if(NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

# Debug/Release builds
if(NOT MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG}   -O0    -DDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Ofast -DNDEBUG")
else()
    set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} /Od /DDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE   "${CMAKE_CXX_FLAGS_RELEASE} /O2 /DNDEBUG")
endif()

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

if(!MSVC)
	set_source_files_properties(Fleece/Fleece_C_impl.cc  PROPERTIES
                            	COMPILE_FLAGS  -Wno-return-type-c-linkage)
endif()

add_library(Fleece        SHARED  ${FLEECE_SRC})
add_library(FleeceStatic  STATIC  ${FLEECE_SRC})

if (APPLE)
    target_link_libraries(Fleece  
                          "-framework CoreFoundation" 
                          "-framework Foundation")
endif()
