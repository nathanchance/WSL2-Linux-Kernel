#!/usr/bin/env bash

set -eu

BASE=$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"/.. && pwd)

# Get parameters
function parse_parameters() {
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

    # Handle architecture specific variables
    case ${ARCH:=x86_64} in
        x86_64)
            CONFIG=arch/x86/configs/wsl2_defconfig
            KERNEL_IMAGE=bzImage
            ;;

        # We only support x86 at this point but that might change eventually
        *)
            echo "\${ARCH} value of '${ARCH}' is not supported!" 2>&1
            exit 22
            ;;
    esac
}

function set_toolchain() {
    # Add toolchain folders to PATH and request path override (PO environment variable)
    export PATH="${PO:+${PO}:}${CBL_LLVM:+${CBL_LLVM}:}${CBL_BNTL:+${CBL_BNTL}:}${PATH}"

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
        "${LLVM_IAS:=0}" \
        "${NM:=llvm-nm}" \
        "${O:=${BASE}/out/${ARCH}}" \
        "${OBJCOPY:=llvm-objcopy}" \
        "${OBJDUMP:=llvm-objdump}" \
        "${OBJSIZE:=llvm-size}" \
        "${STRIP:=llvm-strip}"

    printf '\n\e[01;32mToolchain location:\e[0m %s\n\n' "$(dirname "$(command -v "${CC}")")"
    printf '\e[01;32mToolchain version:\e[0m %s \n\n' "$("${CC}" --version | head -n1)"
}

function kmake() {
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
        "${@}"
    set +x
}

function build_kernel() {
    # Build silently by default
    ${VERBOSE:=false} || COND_MAKE_ARGS=(-s)

    # Configure the kernel
    CONFIG_MAKE_TARGETS=("${CONFIG##*/}")
    ${INCREMENTAL:=false} || CONFIG_MAKE_TARGETS=(distclean "${CONFIG_MAKE_TARGETS[@]}")
    kmake "${CONFIG_MAKE_TARGETS[@]}"

    ${UPDATE_CONFIG_ONLY:=false} && FINAL_TARGET=savedefconfig
    FINAL_MAKE_TARGETS=(olddefconfig "${FINAL_TARGET:=all}")
    kmake "${FINAL_MAKE_TARGETS[@]}"

    if ${UPDATE_CONFIG_ONLY}; then
        cp -v "${O}"/defconfig "${BASE}"/${CONFIG}
        exit 0
    fi

    # Let the user know where the kernel will be (if we built one)
    KERNEL=$(readlink -f "${O}")/arch/${ARCH}/boot/${KERNEL_IMAGE}
    [[ -f ${KERNEL} ]] && printf '\n\e[01;32mKernel is now available at:\e[0m %s\n' "${KERNEL}"
}

parse_parameters "${@}"
set_toolchain
build_kernel
