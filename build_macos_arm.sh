#!/usr/bin/env bash

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ -n "${VST3_SDK_ROOT:-}" ]]; then
    SDK_DIR="${VST3_SDK_ROOT}"
elif [[ -f "${PROJECT_DIR}/../vst3sdk/CMakeLists.txt" ]]; then
    SDK_DIR="${PROJECT_DIR}/../vst3sdk"
else
    SDK_DIR="${PROJECT_DIR}/vst3sdk"
fi

if [[ ! -f "${SDK_DIR}/CMakeLists.txt" ]]; then
    git clone --recursive https://github.com/steinbergmedia/vst3sdk.git "${SDK_DIR}"
fi

if [[ ! -f "${SDK_DIR}/public.sdk/CMakeLists.txt" ]]; then
    git -C "${SDK_DIR}" submodule update --init --recursive
fi

cmake -S "${PROJECT_DIR}" -B "${PROJECT_DIR}/build" \
    -DVST3_SDK_ROOT="${SDK_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DBUILD_TESTING=ON

cmake --build "${PROJECT_DIR}/build" --config Release --target \
    PitchPanicBundle pitchpanic_dsp_smoke pitchpanic_ui_smoke
ctest --test-dir "${PROJECT_DIR}/build" --build-config Release --output-on-failure

echo
echo "Pitch Panic build, validation, and smoke tests completed."
find "${PROJECT_DIR}/build" -type d -name "PitchPanic.vst3" -print
