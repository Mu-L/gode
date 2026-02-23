param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Config = "Debug"
)

$SourceDir = "D:/Godot/gode"
$BuildDir  = "D:/Godot/gode/build"
$CMakeExe  = "D:/cmake/bin/cmake.exe"
$Generator = "MinGW Makefiles"
$CC        = "D:/msys64/ucrt64/bin/gcc.exe"
$CXX       = "D:/msys64/ucrt64/bin/g++.exe"

if (!(Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

& $CMakeExe `
    -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE `
    -DCMAKE_C_COMPILER:FILEPATH=$CC `
    -DCMAKE_CXX_COMPILER:FILEPATH=$CXX `
    --no-warn-unused-cli `
    -S $SourceDir `
    -B $BuildDir `
    -G $Generator

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& $CMakeExe `
    --build $BuildDir `
    --config $Config `
    --

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

# 手动复制 libnode.dll 到 example/addons/gode/bin
# 即使 CMake 中有 POST_BUILD，这里作为双重保险
$LibNodeSrc = "$SourceDir/node/out/Release/libnode.dll"
$BinDir = "$SourceDir/example/addons/gode/bin"

if (Test-Path $LibNodeSrc) {
    if (!(Test-Path $BinDir)) {
        New-Item -ItemType Directory -Path $BinDir | Out-Null
    }
    Copy-Item -Path $LibNodeSrc -Destination $BinDir -Force
    Write-Host "已复制 libnode.dll 到 $BinDir"
} else {
    Write-Warning "未找到 libnode.dll: $LibNodeSrc"
}

exit $LASTEXITCODE
