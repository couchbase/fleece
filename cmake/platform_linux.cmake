include("${CMAKE_CURRENT_LIST_DIR}/platform_base.cmake")
include(CheckCXXSourceCompiles)

# Both GCC and Clang support -print-file-name to locate bundled libraries.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    execute_process(
        COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libbacktrace.a
        OUTPUT_VARIABLE _bt_lib OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(_bt_lib AND NOT _bt_lib STREQUAL "libbacktrace.a")
        get_filename_component(_bt_lib_dir "${_bt_lib}" DIRECTORY)
        find_path(BACKTRACE_INCLUDE_DIR backtrace.h HINTS "${_bt_lib_dir}/include")
        if(BACKTRACE_INCLUDE_DIR)
            set(BACKTRACE_LIBRARY "${_bt_lib}")
            set(CMAKE_REQUIRED_INCLUDES "${BACKTRACE_INCLUDE_DIR}")
            set(CMAKE_REQUIRED_LIBRARIES "${BACKTRACE_LIBRARY}")
            check_cxx_source_compiles("
                #include <backtrace.h>
                int main() {
                    backtrace_state* s = backtrace_create_state(nullptr, 0, nullptr, nullptr);
                    return s ? 0 : 1;
                }
            " HAVE_LIBBACKTRACE)
            unset(CMAKE_REQUIRED_INCLUDES)
            unset(CMAKE_REQUIRED_LIBRARIES)
        endif()
        unset(_bt_lib_dir)
    endif()
    unset(_bt_lib)
endif()

function(set_source_files)
    set(oneValueArgs RESULT)
    cmake_parse_arguments(LINUX_SSS "" "${oneValueArgs}" "" ${ARGN})
    if(NOT DEFINED LINUX_SSS_RESULT)
        message(FATAL_ERROR "set_source_files_base needs to be called with RESULT")
    endif()

    set_source_files_base(RESULT BASE_SRC_FILES)
    set(
        ${LINUX_SSS_RESULT} 
        ${BASE_SRC_FILES} 
        Fleece/Support/Backtrace+signals-posix.cc
        Fleece/Support/Backtrace+capture-posix.cc
        Fleece/Support/Backtrace+capture-linux.cc
        PARENT_SCOPE
    )
endfunction()

function(set_base_platform_files)
    set(oneValueArgs RESULT)
    cmake_parse_arguments(LINUX_SSS "" "${oneValueArgs}" "" ${ARGN})
    if(NOT DEFINED LINUX_SSS_RESULT)
        message(FATAL_ERROR "set_source_files_base needs to be called with RESULT")
    endif()

    set(
        ${LINUX_SSS_RESULT}
        Fleece/Support/Backtrace+capture-posix.cc
        Fleece/Support/Backtrace+capture-linux.cc
        PARENT_SCOPE
    )
endfunction()

function(set_test_source_files)
    set(oneValueArgs RESULT)
    cmake_parse_arguments(LINUX_SSS "" "${oneValueArgs}" "" ${ARGN})
    if(NOT DEFINED LINUX_SSS_RESULT)
        message(FATAL_ERROR "set_source_files_base needs to be called with RESULT")
    endif()

    set_test_source_files_base(RESULT BASE_SRC_FILES)
    set(${LINUX_SSS_RESULT} ${BASE_SRC_FILES} PARENT_SCOPE)
endfunction()

function(setup_build)
    setup_build_base()

    foreach(platform FleeceBase FleeceObjects FleeceStatic Fleece)
        target_link_libraries(
            ${platform} INTERFACE
            dl
        ) 
    endforeach()

    if(HAVE_LIBBACKTRACE)
        foreach(platform FleeceBase FleeceObjects)
            target_compile_definitions(${platform} PRIVATE HAVE_LIBBACKTRACE)
            target_include_directories(${platform} PRIVATE "${BACKTRACE_INCLUDE_DIR}")
            target_link_libraries(${platform} PRIVATE "${BACKTRACE_LIBRARY}")
        endforeach()
    endif()

    foreach(platform FleeceObjects FleeceBase)
        target_compile_options(
            ${platform} PRIVATE
            "-Wformat=2"
            "-fstack-protector"
            "-D_FORTIFY_SOURCE=2"
            "$<$<COMPILE_LANGUAGE:CXX>:-Wno-psabi;-Wno-odr>"
        )
    endforeach()
endfunction()

function(setup_test_build)
    # No-op
endfunction()
