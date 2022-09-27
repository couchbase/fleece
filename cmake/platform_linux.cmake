include("${CMAKE_CURRENT_LIST_DIR}/platform_base.cmake")

function(set_source_files)
    set(oneValueArgs RESULT)
    cmake_parse_arguments(LINUX_SSS "" "${oneValueArgs}" "" ${ARGN})
    if(NOT DEFINED LINUX_SSS_RESULT)
        message(FATAL_ERROR "set_source_files_base needs to be called with RESULT")
    endif()

    set_source_files_base(RESULT BASE_SRC_FILES)
    set(${LINUX_SSS_RESULT} ${BASE_SRC_FILES} PARENT_SCOPE)
endfunction()

function(set_base_platform_files)
    # No-op
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
