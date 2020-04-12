#!/usr/bin/env bash

BASE=$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"/.. && pwd)
cd "${BASE}" || exit ${?}

curl -LSso arch/x86/configs/wsl2_defconfig https://github.com/microsoft/WSL2-Linux-Kernel/raw/linux-msft-wsl-4.19.y/Microsoft/config-wsl

./scripts/config --file arch/x86/configs/wsl2_defconfig \
                 --disable FTRACE \
                 --disable MODULES \
                 --enable LTO_CLANG \
                 --set-val LOCALVERSION \"-microsoft-cbl\" \
                 --undefine FRAME_WARN

./bin/build.sh -u
