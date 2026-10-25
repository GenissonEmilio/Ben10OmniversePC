#!/usr/bin/env bash
# Bounded diagnostic run, separate from any existing saves or other ports.
set -euo pipefail
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
data=${BEN10_DATA_DIR:-${XDG_DATA_HOME:-$HOME/.local/share}/Ben10OmniversePC}
seconds=${1:-30}
quota=${BEN10_PROBE_CPU_QUOTA:-200%}
audio=${BEN10_PROBE_AUDIO_DRIVER:-dummy}
window=${BEN10_PROBE_WINDOW:-1}
binary=${BEN10_PROBE_BINARY:-$root/build/native/Ben10OmniverseRecomp}
[[ "$seconds" =~ ^[0-9]+$ ]] && (( seconds > 0 && seconds <= 600 )) || exit 2
[[ "$quota" == 200% || "$quota" == 300% || "$quota" == 400% || "$quota" == 500% || "$quota" == 600% ]] || exit 2
case "$audio" in dummy|pulseaudio|pipewire|alsa) ;; *) exit 2 ;; esac
[[ "$window" == 0 || "$window" == 1 ]] || exit 2
[[ -f "$binary" && -x "$binary" ]] || exit 2
ulimit -c 0
exec systemd-run --user --scope --unit="ben10-native-probe-$$" \
    --property="CPUQuota=$quota" --property=MemoryMax=4G --property=MemorySwapMax=0 \
    timeout --signal=TERM --kill-after=3s "${seconds}s" env \
    LD_LIBRARY_PATH="$root/artifacts/native-libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    __NV_PRIME_RENDER_OFFLOAD=1 __VK_LAYER_NV_optimus=NVIDIA_only \
    SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER="$audio" \
    PS3_TITLE='Ben 10 Omniverse — Native Linux' \
    PS3_VFS_ROOT="$data/game" \
    PS3_HDD0_ROOT="$data/probe-user/hdd0" PS3_HDD1_ROOT="$data/probe-user/cache" \
    PS3_GAMEDATA_ROOT="$data/probe-user" \
    PS3RECOMP_VK_WINDOW="$window" PS3RECOMP_VK_GUEST_PROGRAMS=1 RSX_LIVE_DRAW=0 \
    stdbuf -oL -eL "$binary" \
    "$data/game/PS3_GAME/USRDIR/EBOOT.elf"
