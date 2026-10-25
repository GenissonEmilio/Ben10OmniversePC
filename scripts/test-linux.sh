#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
python3 "$root/tools/apply_runtime_patches.py"
python3 -m unittest discover -s "$root/tests" -v
docker run --rm --cpus=2 --memory=4g --memory-swap=4g --network=none \
    --user "$(id -u):$(id -g)" --cap-drop=ALL --security-opt=no-new-privileges \
    -e HOME=/tmp -e CCACHE_DIR=/workspace/build/ccache \
    -v "$root:/workspace:Z" -w /workspace ben10-omniverse-tools:local bash -c '
        set -euo pipefail
        cmake -S . -B build/validation -G Ninja -DCMAKE_BUILD_TYPE=Release \
            -DBEN10_BUILD_GAME=OFF -DPS3RECOMP_BUILD_TESTS=ON \
            -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
        cmake --build build/validation --parallel 2
        ctest --test-dir build/validation --output-on-failure --timeout 60 -j 1
        python3 third_party/ps3recomp/tools/test_gcm_flip_stream.py
        python3 third_party/ps3recomp/tools/test_spurs_scheduler.py
    '
