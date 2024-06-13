# Fleece makefile ... just to kick off CMake builds

native:   OUT = build_cmake/native
native:
	mkdir -p $(OUT)
	cmake -S . -B $(OUT) -DCMAKE_BUILD_TYPE=RelWithDebInfo
	cmake --build $(OUT) -j

test:   OUT = build_cmake/test
test:
	mkdir -p $(OUT)
	cmake -S . -B $(OUT) -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(OUT) -j --target FleeceTests
	$(OUT)/FleeceTests -r list

tests: test


# WASM build requires Emscripten <https://emscripten.org>
wasm:	OUT = build_cmake/wasm
wasm:
	mkdir -p $(OUT)
	emcmake cmake -S . -B $(OUT) -DCMAKE_BUILD_TYPE=RelWithDebInfo
	cd $(OUT) && emmake make -j

test_wasm:   OUT = build_cmake/test_wasm
test_wasm:
	mkdir -p $(OUT)
	emcmake cmake -S . -B $(OUT) -DCMAKE_BUILD_TYPE=Debug
	cd $(OUT) && emmake make -j FleeceTests
	node $(OUT)/FleeceTests.js -r list

wasm_test: test_wasm
wasm_tests: test_wasm


docs:
	doxygen Documentation/Doxyfile
	doxygen Documentation/Doxyfile_C++


clean:
	rm -rf build_cmake/{native,test,wasm,test_wasm}
