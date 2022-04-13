function(set_source_files_base)
    set(oneValueArgs RESULT)
    cmake_parse_arguments(BASE_SSS "" "${oneValueArgs}" "" ${ARGN})
    if(NOT DEFINED BASE_SSS_RESULT)
        message(FATAL_ERROR "set_source_files_base needs to be called with RESULT")
    endif()

    set(
        ${BASE_SSS_RESULT}
        Fleece/API_Impl/Fleece.cc
        Fleece/API_Impl/FLSlice.cc
        Fleece/Core/Array.cc
        Fleece/Core/DeepIterator.cc
        Fleece/Core/Dict.cc
        Fleece/Core/Doc.cc
        Fleece/Core/Encoder.cc
        Fleece/Core/JSONConverter.cc
        Fleece/Core/JSONDelta.cc
        Fleece/Core/Path.cc
        Fleece/Core/Pointer.cc
        Fleece/Core/SharedKeys.cc
        Fleece/Core/Value+Dump.cc
        Fleece/Core/Value.cc
        Fleece/Integration/MContext.cc
        Fleece/Mutable/HeapArray.cc
        Fleece/Mutable/HeapDict.cc
        Fleece/Mutable/HeapValue.cc
        Fleece/Mutable/ValueSlot.cc
        Fleece/Support/Backtrace.cc
        Fleece/Support/Base64.cc
        Fleece/Support/betterassert.cc
        Fleece/Support/Bitmap.cc
        Fleece/Support/ConcurrentArena.cc
        Fleece/Support/ConcurrentMap.cc
        Fleece/Support/FileUtils.cc
        Fleece/Support/FleeceException.cc
        Fleece/Support/InstanceCounted.cc
        Fleece/Support/NumConversion.cc
        Fleece/Support/JSON5.cc
        Fleece/Support/JSONEncoder.cc
        Fleece/Support/LibC++Debug.cc
        Fleece/Support/ParseDate.cc
        Fleece/Support/RefCounted.cc
        Fleece/Support/slice_stream.cc
        Fleece/Support/sliceIO.cc
        Fleece/Support/StringTable.cc
        Fleece/Support/varint.cc
        Fleece/Support/Writer.cc
        Fleece/Tree/HashTree.cc
        Fleece/Tree/MutableHashTree.cc
        Fleece/Tree/NodeRef.cc
        vendor/jsonsl/jsonsl.c
        vendor/libb64/cdecode.c
        vendor/libb64/cencode.c
        vendor/SwiftDtoa/SwiftDtoa.cc
        PARENT_SCOPE
    )
endfunction()

function(set_test_source_files_base)
    set(oneValueArgs RESULT)
    cmake_parse_arguments(BASE_SSS "" "${oneValueArgs}" "" ${ARGN})
    if(NOT DEFINED BASE_SSS_RESULT)
        message(FATAL_ERROR "set_source_files_base needs to be called with RESULT")
    endif()

    set(
        ${BASE_SSS_RESULT}
        Tests/API_ValueTests.cc
        Tests/DeltaTests.cc
        Tests/EncoderTests.cc
        Tests/FleeceTests.cc
        Tests/FleeceTestsMain.cc
        Tests/HashTreeTests.cc
        Tests/JSON5Tests.cc
        Tests/MutableTests.cc
        Tests/PerfTests.cc
        Tests/SharedKeysTests.cc
        Tests/SupportTests.cc
        Tests/ValueTests.cc
        Tests/C_Test.c
        Experimental/KeyTree.cc
        PARENT_SCOPE
    )
endfunction()

function(get_all_targets outvar)
    set(targets)
    get_all_targets_recursive(targets ${CMAKE_CURRENT_SOURCE_DIR})
    set(${outvar} ${targets} PARENT_SCOPE)
endfunction()

macro(get_all_targets_recursive targets dir)
    get_directory_property(subdirectories DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)
    foreach(subdir ${subdirectories})
        get_all_targets_recursive(${targets} ${subdir})
    endforeach()

    get_property(current_targets DIRECTORY ${dir} PROPERTY BUILDSYSTEM_TARGETS)
    list(APPEND ${targets} ${current_targets})
endmacro()

function(setup_build_base)
    if(CMAKE_COMPILER_IS_GNUCC)
        # Suppress an annoying note about GCC 7 ABI changes, and linker errors about the Fleece C API

        get_all_targets(all_targets)
		list(FILTER all_targets EXCLUDE REGEX "lib|apidoc")
        foreach(target ${all_targets})
            target_compile_options(
                ${target} PRIVATE
                $<$<COMPILE_LANGUAGE:CXX>:"-Wno-psabi;-Wno-odr">
            )
        endforeach()
     endif()
endfunction()
