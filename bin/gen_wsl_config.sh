#!/usr/bin/env bash

BIN_DIR=$(dirname "$(readlink -f "${0}")")
cd "${BIN_DIR%/*}" || exit ${?}

CONFIG=arch/x86/configs/wsl2_defconfig

curl -LSso "${CONFIG}" https://github.com/microsoft/WSL2-Linux-Kernel/raw/linux-msft-wsl-5.10.y/Microsoft/config-wsl

./bin/build.sh -u
