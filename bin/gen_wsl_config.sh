#!/usr/bin/env bash

BASE=$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"/.. && pwd)
cd "${BASE}" || exit ${?}

curl -LSso arch/x86/configs/wsl2_defconfig https://github.com/microsoft/WSL2-Linux-Kernel/raw/linux-msft-wsl-4.19.y/Microsoft/config-wsl

# Initial tuning
#   * FTRACE: Limit attack surface and avoids a warning at boot.
#   * MODULES: Limit attack surface and we don't support them anyways.
#   * LTO_CLANG: Optimization.
#   * CFI_CLANG: Hardening.
#   * LOCALVERSION_AUTO: Helpful when running development builds.
#   * LOCALVERSION: Replace 'standard' with 'cbl' since this is a Clang built kernel.
#   * FRAME_WARN: The 64-bit default is 2048. Clang uses more stack space so this avoids build-time warnings.
./scripts/config \
    --file arch/x86/configs/wsl2_defconfig \
    -d FTRACE \
    -d MODULES \
    -e LTO_CLANG \
    -e CFI_CLANG \
    -e LOCALVERSION_AUTO \
    --set-str LOCALVERSION "-microsoft-cbl" \
    -u FRAME_WARN

./bin/build.sh -u
