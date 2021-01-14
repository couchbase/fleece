include("${CMAKE_CURRENT_LIST_DIR}/platform_base.cmake")

function(set_source_files)
    set(oneValueArgs RESULT)
    cmake_parse_arguments(WIN_SSS "" "${oneValueArgs}" "" ${ARGN})
    if(NOT DEFINED WIN_SSS_RESULT)
        message(FATAL_ERROR "set_source_files_base needs to be called with RESULT")
    endif()

    set_source_files_base(RESULT BASE_SRC_FILES)
    set(
        ${WIN_SSS_RESULT}
        ${BASE_SRC_FILES}
        MSVC/vasprintf-msvc.c
        MSVC/asprintf.c
        PARENT_SCOPE
    )
endfunction()

function(set_base_platform_files)
    # No-op
endfunction()

function(set_test_source_files)
    set(oneValueArgs RESULT)
    cmake_parse_arguments(WIN_SSS "" "${oneValueArgs}" "" ${ARGN})
    if(NOT DEFINED WIN_SSS_RESULT)
        message(FATAL_ERROR "set_source_files_base needs to be called with RESULT")
    endif()

    set_test_source_files_base(RESULT BASE_SRC_FILES)
    set(${WIN_SSS_RESULT} ${BASE_SRC_FILES} PARENT_SCOPE)
endfunction()

function(setup_build)
    foreach(target FleeceStatic Fleece FleeceBase)
        target_include_directories(
            ${target} PRIVATE
            MSVC
        )

        target_compile_definitions(
            ${target} PRIVATE
            -D_CRT_SECURE_NO_WARNINGS
            -DNOMINMAX
        )
    endforeach()
endfunction()

function(setup_test_build)
    target_compile_definitions(
        FleeceTests PRIVATE
        -D_USE_MATH_DEFINES     # Define math constants like PI
        -DNOMINMAX              # Get rid of pesky Windows macros for min and max
    )
    target_compile_options(
        FleeceTests PRIVATE
        "/utf-8"                # Some source files have UTF-8 literals
    )
endfunction()
