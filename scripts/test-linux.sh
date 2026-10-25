#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
python3 "$root/tools/apply_runtime_patches.py"
python3 -m unittest discover -s "$root/tests" -v
docker run --rm --cpus=2 --memory=4g --memory-swap=4g --network=none \
    --user "$(id -u):$(id -g)" --cap-drop=ALL --security-opt=no-new-privileges \
    -e HOME=/tmp -e CCACHE_DIR=/workspace/build/ccache \
    -v "$root:/workspace:z" -w /workspace ben10-omniverse-tools:local bash -c '
        set -euo pipefail
        cmake -S . -B build/validation -G Ninja -DCMAKE_BUILD_TYPE=Release \
            -DBEN10_BUILD_GAME=OFF -DPS3RECOMP_BUILD_TESTS=ON \
            -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
        cmake --build build/validation --parallel 2
        ctest --test-dir build/validation --output-on-failure --timeout 60 -j 1
        python3 third_party/ps3recomp/tools/test_gcm_flip_stream.py
        python3 third_party/ps3recomp/tools/test_spurs_scheduler.py
        python3 third_party/ps3recomp/tools/test_ppu_jumptables.py
        python3 third_party/ps3recomp/tools/test_rsx_immediate_vertices.py
        if python3 -c '\''from pathlib import Path; raise SystemExit("avx2" not in Path("/proc/cpuinfo").read_text())'\''; then
            "$CC" -O3 -mavx2 -ffp-contract=off -Ithird_party/ps3recomp/include \
                third_party/ps3recomp/runtime/spu/tests/test_spu_float_neon.c -lm \
                -o build/validation/test_spu_float_avx2
            build/validation/test_spu_float_avx2
            "$CC" -O3 -mavx2 -Ithird_party/ps3recomp/include \
                third_party/ps3recomp/runtime/spu/tests/test_spu_shufb.c -lm \
                -o build/validation/test_spu_shufb_avx2
            build/validation/test_spu_shufb_avx2
            "$CC" -O3 -mssse3 -UNDEBUG -Ithird_party/ps3recomp/include \
                -Ithird_party/ps3recomp/runtime/spu tests/test_spu_rotate.c -lm \
                -o build/validation/test_ben10_spu_rotate
            build/validation/test_ben10_spu_rotate
        fi
    '
