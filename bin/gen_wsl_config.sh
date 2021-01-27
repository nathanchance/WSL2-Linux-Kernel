#!/usr/bin/env bash

BIN_DIR=$(dirname "$(readlink -f "${0}")")
cd "${BIN_DIR%/*}" || exit ${?}

CONFIG=arch/x86/configs/wsl2_defconfig

curl -LSso "${CONFIG}" https://github.com/microsoft/WSL2-Linux-Kernel/raw/linux-msft-wsl-5.10.y/Microsoft/config-wsl

# Initial tuning
#   * FTRACE: Limit attack surface and avoids a warning at boot.
#   * MODULES: Limit attack surface and we don't support them anyways.
#   * LTO_CLANG: Optimization.
#   * CFI_CLANG: Hardening.
#   * LOCALVERSION_AUTO: Helpful when running development builds.
#   * LOCALVERSION: Replace 'standard' with 'cbl' since this is a Clang built kernel.
#   * FRAME_WARN: The 64-bit default is 2048. Clang uses more stack space so this avoids build-time warnings.
./scripts/config \
    --file "${CONFIG}" \
    -d FTRACE \
    -d MODULES \
    -d LTO_NONE \
    -e LTO_CLANG \
    -e LTO_CLANG_THIN \
    -e CFI_CLANG \
    -e LOCALVERSION_AUTO \
    --set-str LOCALVERSION "-microsoft-cbl" \
    -u FRAME_WARN

./bin/build.sh -u
