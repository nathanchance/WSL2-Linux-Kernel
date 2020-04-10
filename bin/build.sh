#!/usr/bin/env bash

set -eu

BASE=$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"/.. && pwd)

# Get parameters
while ((${#})); do
    case ${1} in
        *=*) export "${1?}" ;;
        -i | --incremental) INCREMENTAL=true ;;
        -j | --jobs) JOBS=${1} ;;
        -u | --update-config-only) UPDATE_CONFIG_ONLY=true ;;
        -v | --verbose) VERBOSE=true ;;
    esac
    shift
done

# Add toolchain folders to PATH and request path override (PO environment variable)
export PATH="${PO:+${PO}:}${CBL_LLVM:+${CBL_LLVM}:}${BASE}/bin:${CBL_BNTL:+${CBL_BNTL}:}${PATH}"

# Handle architecture specific variables
case ${ARCH:=x86} in
    x86)
        ARCH=x86
        O_ARCH=x86_64
        KERNEL_IMAGE=bzImage
        ;;

    # We only support x86 at this point but that might change eventually
    *)
        echo "\${ARCH} value of '${ARCH}' is not supported!" 2>&1
        exit 22
        ;;
esac

# Set default values if user did not supply them above
true \
    "${AR:=llvm-ar}" \
    "${CC:=clang}" \
    "${HOSTAR:=llvm-ar}" \
    "${HOSTCC:=clang}" \
    "${HOSTLD:=ld.lld}" \
    "${HOSTLDFLAGS:=-fuse-ld=lld}" \
    "${JOBS:="$(nproc)"}" \
    "${LD:=ld.lld}" \
    "${LLVM_IAS:=1}" \
    "${NM:=llvm-nm}" \
    "${O:=${BASE}/out/${O_ARCH}}" \
    "${OBJCOPY:=llvm-objcopy}" \
    "${OBJDUMP:=llvm-objdump}" \
    "${OBJSIZE:=llvm-size}" \
    "${STRIP:=llvm-strip}"

printf '\n\e[01;32mToolchain location:\e[0m %s\n\n' "$(dirname "$(command -v "${CC}")")"
printf '\e[01;32mToolchain version:\e[0m %s \n\n' "$("${CC}" --version | head -n1)"

# Build the kernel
CONFIG=wsl2_defconfig
if ${UPDATE_CONFIG_ONLY:=false}; then
    FINAL_TARGET=savedefconfig
else
    FINAL_TARGET=all
fi
MAKE_TARGETS=("${CONFIG}" olddefconfig "${FINAL_TARGET}")
${INCREMENTAL:=false} || MAKE_TARGETS=(distclean "${MAKE_TARGETS[@]}")
${VERBOSE:=false} || COND_MAKE_ARGS=(-s)
set -x
time make \
    -C "${BASE}" \
    -j"${JOBS}" \
    ${COND_MAKE_ARGS:+"${COND_MAKE_ARGS[@]}"} \
    AR="${AR}" \
    ARCH="${ARCH}" \
    CC="${CC}" \
    HOSTAR="${AR}" \
    HOSTCC="${HOSTCC}" \
    HOSTLD="${HOSTLD}" \
    HOSTLDFLAGS="${HOSTLDFLAGS}" \
    LD="${LD}" \
    LLVM_IAS="${LLVM_IAS}" \
    NM="${NM}" \
    O="$(realpath -m --relative-to="${BASE}" "${O}")" \
    OBJCOPY="${OBJCOPY}" \
    OBJDUMP="${OBJDUMP}" \
    OBJSIZE="${OBJSIZE}" \
    STRIP="${STRIP}" \
    ${V:+V=${V}} \
    "${MAKE_TARGETS[@]}"
set +x
if ${UPDATE_CONFIG_ONLY}; then
    cp -v "${O}"/defconfig "${BASE}"/arch/x86/configs/${CONFIG}
    exit 0
fi

# Let the user know where the kernel will be (if we built one)
KERNEL=$(readlink -f "${O}")/arch/${ARCH}/boot/${KERNEL_IMAGE}
[[ -f ${KERNEL} ]] && printf '\n\e[01;32mKernel is now available at:\e[0m %s\n' "${KERNEL}"
