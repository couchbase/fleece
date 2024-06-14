include("${CMAKE_CURRENT_LIST_DIR}/platform_base.cmake")

set(EMSCRIPTEN_COMPILE_FLAGS
    "-pthread"
    "-fwasm-exceptions"
    "-flto"
)
set(EMSCRIPTEN_LINK_FLAGS
    "-pthread"
    "-fwasm-exceptions"
    "-flto"
    "-lembind"
    "SHELL:-s ALLOW_MEMORY_GROWTH=1"
    "SHELL:-s EXIT_RUNTIME=1"
    "SHELL:-s WASM_BIGINT=1"
)

function(set_source_files)
    set(oneValueArgs RESULT)
    cmake_parse_arguments(EMSCRIPTEN_SSS "" "${oneValueArgs}" "" ${ARGN})
    if(NOT DEFINED EMSCRIPTEN_SSS_RESULT)
        message(FATAL_ERROR "set_source_files_base needs to be called with RESULT")
    endif()

    set_source_files_base(RESULT BASE_SRC_FILES)
    set(${EMSCRIPTEN_SSS_RESULT} ${BASE_SRC_FILES} PARENT_SCOPE)
endfunction()

function(set_base_platform_files)
    # No-op
endfunction()

function(set_test_source_files)
    set(oneValueArgs RESULT)
    cmake_parse_arguments(EMSCRIPTEN_SSS "" "${oneValueArgs}" "" ${ARGN})
    if(NOT DEFINED EMSCRIPTEN_SSS_RESULT)
        message(FATAL_ERROR "set_source_files_base needs to be called with RESULT")
    endif()

    set_test_source_files_base(RESULT BASE_SRC_FILES)
    set(${EMSCRIPTEN_SSS_RESULT} ${BASE_SRC_FILES} PARENT_SCOPE)
endfunction()

function(setup_build)
    foreach(platform Fleece FleeceStatic FleeceBase FleeceObjects fleeceTool)
        target_compile_options(
            ${platform}
            PRIVATE
            "-Wformat=2"
            ${EMSCRIPTEN_COMPILE_FLAGS}
        )
    endforeach()

    target_link_options(
        fleeceTool
        PRIVATE
        ${EMSCRIPTEN_LINK_FLAGS}
    )
endfunction()

function(setup_test_build)
    target_compile_options(
        FleeceTests
        PRIVATE
        ${EMSCRIPTEN_COMPILE_FLAGS}
    )
    target_link_options(
        FleeceTests
        PRIVATE
        ${EMSCRIPTEN_LINK_FLAGS}
        "-lnodefs.js"
        "-lnoderawfs.js"
        "SHELL:-s PTHREAD_POOL_SIZE=8"
    )
endfunction()
