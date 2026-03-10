include("${CMAKE_CURRENT_LIST_DIR}/platform_base.cmake")
include(CheckCXXSourceCompiles)

# libbacktrace is bundled inside GCC's private library directory.
# Ask GCC where it keeps the library and header so that builds with
# other compilers (e.g. Clang) can find it.
find_program(_gcc NAMES gcc)
if(_gcc)
    execute_process(
        COMMAND ${_gcc} -print-file-name=libbacktrace.a
        OUTPUT_VARIABLE _gcc_bt_lib OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(_gcc_bt_lib AND NOT _gcc_bt_lib STREQUAL "libbacktrace.a")
        get_filename_component(_gcc_lib_dir "${_gcc_bt_lib}" DIRECTORY)
        find_path(BACKTRACE_INCLUDE_DIR backtrace.h HINTS "${_gcc_lib_dir}/include")
        if(BACKTRACE_INCLUDE_DIR)
            set(BACKTRACE_LIBRARY "${_gcc_bt_lib}")
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
    endif()
    unset(_gcc_bt_lib)
    unset(_gcc_lib_dir)
endif()
unset(_gcc)

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
        Fleece/Support/Backtrace+signals-posix.cc
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
