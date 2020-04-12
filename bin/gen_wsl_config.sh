#!/usr/bin/env bash

BASE=$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")

cd "${BASE}" || exit ${?}

curl -LSso arch/x86/configs/wsl2_defconfig https://github.com/microsoft/WSL2-Linux-Kernel/raw/linux-msft-wsl-4.19.y/Microsoft/config-wsl

./scripts/config --file arch/x86/configs/wsl2_defconfig \
                 --disable MODULES \
                 --disable FTRACE \
                 --enable LTO_CLANG \
                 --set-val LOCALVERSION \"-microsoft-cbl\" \
                 --undefine FRAME_WARN

./build.sh -u
