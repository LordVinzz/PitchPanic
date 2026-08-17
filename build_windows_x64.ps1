$ErrorActionPreference = "Stop"

$ProjectDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$SdkDir = if ($env:VST3_SDK_ROOT) { $env:VST3_SDK_ROOT } else { Join-Path $ProjectDir "vst3sdk" }
$SdkCommit = "58f8da7936800732561402d7936584ca4505de07"

if (-not (Test-Path (Join-Path $SdkDir "CMakeLists.txt"))) {
    git clone --recursive https://github.com/steinbergmedia/vst3sdk.git $SdkDir
    git -C $SdkDir checkout $SdkCommit
    git -C $SdkDir submodule update --init --recursive
}

$BuildDir = Join-Path $ProjectDir "build-windows"
cmake -S $ProjectDir -B $BuildDir `
    -A x64 `
    -DVST3_SDK_ROOT="$SdkDir" `
    -DBUILD_TESTING=ON
cmake --build $BuildDir --config Release --target PitchPanic pitchpanic_dsp_smoke pitchpanic_ui_smoke
ctest --test-dir $BuildDir --build-config Release --output-on-failure

Write-Host "Pitch Panic Windows build and tests completed."
Get-ChildItem $BuildDir -Directory -Filter "PitchPanic.vst3" -Recurse | Select-Object -ExpandProperty FullName
