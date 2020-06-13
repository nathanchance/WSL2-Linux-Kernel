#!/usr/bin/env bash

BASE=$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"/.. && pwd)
cd "${BASE}" || exit ${?}

CONFIG=arch/x86/configs/wsl2_defconfig

curl -LSso "${CONFIG}" https://github.com/microsoft/WSL2-Linux-Kernel/raw/linux-msft-wsl-4.19.y/Microsoft/config-wsl

# Quality of Life configs
#   * RAID6_PQ_BENCHMARK: This is disbled in the stock Microsoft kernel (under a different name)
#   * DXGKRNL: After build 20150, GPU compute can be used
#   * KVM: After build 19619, nested virtualization can be used
#   * NET_9P_VIRTIO: Needed after build 19640, as drvfs uses this by default
./scripts/config \
    --file "${CONFIG}" \
    -d RAID6_PQ_BENCHMARK \
    -e DXGKRNL \
    -e KVM \
    -e KVM_AMD \
    -e KVM_GUEST \
    -e KVM_INTEL \
    -e NET_9P_VIRTIO

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
    -e LTO_CLANG \
    -e CFI_CLANG \
    -e LOCALVERSION_AUTO \
    --set-str LOCALVERSION "-microsoft-cbl" \
    -u FRAME_WARN

./bin/build.sh -u
