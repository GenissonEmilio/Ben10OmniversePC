#!/usr/bin/env bash
# Compile local translated game sources; never package these sources or the executable.
set -euo pipefail
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
data=${BEN10_DATA_DIR:-${XDG_DATA_HOME:-$HOME/.local/share}/Ben10OmniversePC}
simd=OFF
if [[ -r /proc/cpuinfo ]] && rg -q '^flags.*\bavx2\b' /proc/cpuinfo; then simd=ON; fi
vertex_default=OFF
if [[ "$simd" == ON ]] && rg -q '^flags.*\bfma\b' /proc/cpuinfo; then vertex_default=ON; fi
vertex_fma=${BEN10_VERTEX_FMA:-$vertex_default}
[[ "$vertex_fma" == ON || "$vertex_fma" == OFF ]] || { echo 'BEN10_VERTEX_FMA deve ser ON ou OFF.' >&2; exit 1; }
vertex_cache=${BEN10_VERTEX_CACHE:-OFF}
[[ "$vertex_cache" == ON || "$vertex_cache" == OFF ]] || exit 2
[[ -f "$data/native/ppu/ppu_recomp.h" ]] || { echo 'Tradução PPU ainda não gerada.' >&2; exit 1; }
python3 "$root/tools/apply_runtime_patches.py"
mkdir -p "$root/build/ccache" "$root/artifacts"
docker run --rm --cpus=2 --memory=4g --memory-swap=4g --network=none \
    --user "$(id -u):$(id -g)" --cap-drop=ALL --security-opt=no-new-privileges \
    -e HOME=/tmp -e CCACHE_DIR=/workspace/build/ccache \
    -e BEN10_SPU_AVX2="$simd" \
    -e BEN10_VERTEX_FMA="$vertex_fma" \
    -e BEN10_VERTEX_CACHE="$vertex_cache" \
    -v "$root:/workspace:z" -v "$data:/data:z" --workdir /workspace \
    ben10-omniverse-tools:local bash -c '
        set -euo pipefail
        cmake -S . -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Release \
            -DBEN10_BUILD_GAME=ON -DPS3RECOMP_BUILD_TESTS=OFF -DRECOMP_DIR=/data/native/ppu -DSPU_DIR=/data/native/spu \
            -DSPU_JOB_DIR=/data/native/jobs/lifted \
            -DSPU_OVERLAY_DIR=/data/native/overlays/lifted \
            -DSPU_FIRMWARE_DIR=/data/native/firmware-spu/lifted \
            -DFIRMWARE_PPU_DIR=/data/native/firmware-ppu \
            -DSPU_CODEC_DIR=/data/native/firmware-codecs/lifted \
            -DBEN10_SPU_AVX2="$BEN10_SPU_AVX2" \
            -DBEN10_VERTEX_FMA="$BEN10_VERTEX_FMA" \
            -DBEN10_VERTEX_CACHE="$BEN10_VERTEX_CACHE" \
            -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
        cmake --build build/native --target Ben10OmniverseRecomp --parallel 2
    '
python3 "$root/tools/prepare_host_libraries.py"
