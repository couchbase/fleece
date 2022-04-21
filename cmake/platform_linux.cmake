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

    target_link_libraries(
        FleeceBase INTERFACE
        dl
    )

    target_link_libraries(
        FleeceObjects INTERFACE
        dl
    )

    target_compile_definitions(
        FleeceObjects PRIVATE
        __STDC_WANT_LIB_EXT1__=1 # For memset_s
    )

    foreach(platform FleeceObjects FleeceBase)
        set_target_properties(
            ${platform} PROPERTIES COMPILE_FLAGS
            "-Wformat=2"
        )
    endforeach()
endfunction()

function(setup_test_build)
    # No-op
endfunction()
