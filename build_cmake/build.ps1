param(
    [Parameter(Mandatory=$true)][string]$Platform
)

if($Platform -eq "x86" -or $Platform -eq "Win32") {
    pushd build_cmake/x86
    cmake -G "Visual Studio 15 2017" ..\..
    if($LASTEXITCODE -ne 0) {
        throw "x86 generation failed"
    }

    cmake --build . --config RelWithDebInfo --target fleeceTool
    if($LASTEXITCODE -ne 0) {
        throw "x86 build failed"
    }

    popd

    pushd build_cmake/x86_store
    cmake -G "Visual Studio 15 2017" -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION="10.0" -DCMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION="10.0.16299.0" ..\..
    if($LASTEXITCODE -ne 0) {
        throw "x86 UWP generation failed"
    }

    cmake --build . --config RelWithDebInfo --target Fleece
    if($LASTEXITCODE -ne 0) {
        throw "x86 UWP build failed"
    }

    popd
} elseif($Platform -eq "x64" -or $Platform -eq "Win64") {
    pushd build_cmake/x64
    cmake -G "Visual Studio 15 2017 Win64" ..\..
    if($LASTEXITCODE -ne 0) {
        throw "x64 generation failed"
    }

    cmake --build . --config RelWithDebInfo --target fleeceTool
    if($LASTEXITCODE -ne 0) {
        throw "x64 build failed"
    }

    popd

    pushd build_cmake/x64_store
    if($LASTEXITCODE -ne 0) {
        throw "x64 UWP generation failed"
    }

    cmake -G "Visual Studio 15 2017 Win64" -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION="10.0" -DCMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION="10.0.16299.0" ..\..
    if($LASTEXITCODE -ne 0) {
        throw "x64 UWP build failed"
    }

    cmake --build . --config RelWithDebInfo --target Fleece
    popd
} else {
    pushd build_cmake/arm
    cmake -G "Visual Studio 15 2017 ARM" -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION="10.0" -DCMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION="10.0.16299.0" ..\..
    if($LASTEXITCODE -ne 0) {
        throw "ARM generation failed"
    }

    cmake --build . --config RelWithDebInfo --target Fleece
    if($LASTEXITCODE -ne 0) {
        throw "ARM build failed"
    }

    popd
}
