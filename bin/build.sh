#!/usr/bin/env bash

set -eu

KRNL_SRC=$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"/.. && pwd)

# Get parameters
function parse_parameters() {
    BUILD_TARGETS=()
    CONFIG_TARGETS=()
    while ((${#})); do
        case ${1} in
            *config) CONFIG_TARGETS+=("${1}") ;;
            */ | *.i | *.ko | *.o | vmlinux | zImage | modules) BUILD_TARGETS+=("${1}") ;;
            *=*) export "${1?}" ;;
            -i | --incremental) INCREMENTAL=true ;;
            -j | --jobs) JOBS=${1} ;;
            -k | --kernel-src) shift && KRNL_SRC=$(readlink -f "${1}") ;;
            -u | --update-config-only) UPDATE_CONFIG_ONLY=true ;;
            -v | --verbose) VERBOSE=true ;;
        esac
        shift
    done
    [[ -z ${BUILD_TARGETS[*]} ]] && BUILD_TARGETS=(all)

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
    case "$(id -un)@$(uname -n)" in
        nathan@archlinux-* | nathan@debian-* | nathan@MSI | nathan@Ryzen-9-3900X | nathan@ubuntu-*)
            [[ -n ${CBL_LLVM_BNTL:-} && -d ${CBL_LLVM_BNTL} ]] && TC_PATH=${CBL_LLVM_BNTL}
            [[ -n ${CBL_BIN:-} && ${CBL_BIN} ]] && TC_PATH=${CBL_BIN}
            ;;
    esac
    export PATH="${PO:+${PO}:}${KRNL_SRC}/bin:${TC_PATH:+${TC_PATH}:}${PATH}"

    # Use ccache if it exists
    CCACHE=$(command -v ccache)

    # Resolve O=
    O=$(readlink -f -m "${O:=${KRNL_SRC}/build/${ARCH}}")

    : "${CC:=clang}"
    printf '\n\e[01;32mToolchain location:\e[0m %s\n\n' "$(dirname "$(command -v "${CC##* }")")"
    printf '\e[01;32mToolchain version:\e[0m %s \n\n' "$("${CC##* }" --version | head -n1)"
}

function kmake() {
    set -x
    time make \
        -C "${KRNL_SRC}" \
        -"${SILENT_MAKE_FLAG:-}"kj"${JOBS:="$(nproc)"}" \
        ${AR:+AR="${AR}"} \
        ARCH="${ARCH}" \
        ${CCACHE:+CC="ccache ${CC}"} \
        ${HOSTAR:+HOSTAR="${HOSTAR}"} \
        ${CCACHE:+HOSTCC="ccache ${HOSTCC:-clang}"} \
        ${HOSTLD:+HOSTLD="${HOSTLD}"} \
        HOSTLDFLAGS="${HOSTLDFLAGS--fuse-ld=lld}" \
        KCFLAGS="${KCFLAGS--Werror}" \
        ${LD:+LD="${LD}"} \
        LLVM="${LLVM:=1}" \
        LLVM_IAS="${LLVM_IAS:=1}" \
        ${NM:+NM="${NM}"} \
        O="$(realpath -m --relative-to="${KRNL_SRC}" "${O}")" \
        ${OBJCOPY:+OBJCOPY="${OBJCOPY}"} \
        ${OBJDUMP:+OBJDUMP="${OBJDUMP}"} \
        ${OBJSIZE:+OBJSIZE="${OBJSIZE}"} \
        ${READELF:+READELF="${READELF}"} \
        ${STRIP:+STRIP="${STRIP}"} \
        ${V:+V=${V}} \
        "${@}"
    set +x
}

function build_kernel() {
    # Build silently by default
    ${VERBOSE:=false} || SILENT_MAKE_FLAG=s

    # Create list of targets
    CONFIG_MAKE_TARGETS=("${CONFIG##*/}" "${CONFIG_TARGETS[@]}")
    ${INCREMENTAL:=false} || CONFIG_MAKE_TARGETS=(distclean "${CONFIG_MAKE_TARGETS[@]}")
    ${UPDATE_CONFIG_ONLY:=false} && FINAL_MAKE_TARGETS=(savedefconfig)
    [[ -z ${FINAL_MAKE_TARGETS[*]} ]] && FINAL_MAKE_TARGETS=(olddefconfig "${BUILD_TARGETS[@]}")

    kmake "${CONFIG_MAKE_TARGETS[@]}" "${FINAL_MAKE_TARGETS[@]}"

    if ${UPDATE_CONFIG_ONLY}; then
        cp -v "${O}"/defconfig "${KRNL_SRC}"/${CONFIG}
        exit 0
    fi

    # Let the user know where the kernel will be (if we built one)
    KERNEL=${O}/arch/${ARCH}/boot/${KERNEL_IMAGE}
    [[ -f ${KERNEL} ]] && printf '\n\e[01;32mKernel is now available at:\e[0m %s\n' "${KERNEL}"
}

parse_parameters "${@}"
set_toolchain
build_kernel
