#!/usr/bin/env bash

set -eu

KRNL_SRC=$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"/.. && pwd)

# Get parameters
function parse_parameters() {
    MY_TARGETS=()
    while ((${#})); do
        case ${1} in
            */ | *.i | *.ko | *.o | vmlinux | zImage | modules) MY_TARGETS=("${MY_TARGETS[@]}" "${1}") ;;
            *=*) export "${1?}" ;;
            -i | --incremental) INCREMENTAL=true ;;
            -j | --jobs) JOBS=${1} ;;
            -k | --kernel-src) shift && KRNL_SRC=$(readlink -f "${1}") ;;
            -r | --release) RELEASE=true ;;
            -u | --update-config-only) UPDATE_CONFIG_ONLY=true ;;
            -v | --verbose) VERBOSE=true ;;
        esac
        shift
    done
    [[ -z ${MY_TARGETS[*]} ]] && MY_TARGETS=(all)

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
        nathan@ubuntu-*) TC_PATH=${CBL_LLVM:?}:${CBL_BNTL:?} ;;
        nathan@Ryzen-9-3900X) TC_PATH=${HOME}/src/tc-build/install/bin ;;
    esac
    export PATH="${PO:+${PO}:}${KRNL_SRC}/bin:${TC_PATH:+${TC_PATH}:}${PATH}"

    # Use ccache if it exists
    CCACHE=$(command -v ccache)

    # Set default values if user did not supply them above
    true \
        "${AR:=llvm-ar}" \
        "${CC:=${CCACHE:+ccache }clang}" \
        "${HOSTAR:=llvm-ar}" \
        "${HOSTCC:=${CCACHE:+ccache }clang}" \
        "${HOSTCXX:=${CCACHE:+ccache }clang++}" \
        "${HOSTLD:=ld.lld}" \
        "${HOSTLDFLAGS:=-fuse-ld=lld}" \
        "${JOBS:="$(nproc)"}" \
        "${LD:=ld.lld}" \
        "${LLVM_IAS:=1}" \
        "${NM:=llvm-nm}" \
        "${O:=${KRNL_SRC}/out/${ARCH}}" \
        "${OBJCOPY:=llvm-objcopy}" \
        "${OBJDUMP:=llvm-objdump}" \
        "${OBJSIZE:=llvm-size}" \
        "${READELF:=llvm-readelf}" \
        "${STRIP:=llvm-strip}"

    # Resolve O=
    O=$(readlink -f -m "${O}")

    printf '\n\e[01;32mToolchain location:\e[0m %s\n\n' "$(dirname "$(command -v "${CC##* }")")"
    printf '\e[01;32mToolchain version:\e[0m %s \n\n' "$("${CC##* }" --version | head -n1)"
}

function kmake() {
    set -x
    time make \
        -C "${KRNL_SRC}" \
        -"${SILENT_MAKE_FLAG:-}"kj"${JOBS}" \
        AR="${AR}" \
        ARCH="${ARCH}" \
        CC="${CC}" \
        HOSTAR="${AR}" \
        HOSTCC="${HOSTCC}" \
        HOSTCXX="${HOSTCXX}" \
        HOSTLD="${HOSTLD}" \
        HOSTLDFLAGS="${HOSTLDFLAGS}" \
        LD="${LD}" \
        LLVM_IAS="${LLVM_IAS}" \
        NM="${NM}" \
        O="$(realpath -m --relative-to="${KRNL_SRC}" "${O}")" \
        OBJCOPY="${OBJCOPY}" \
        OBJDUMP="${OBJDUMP}" \
        OBJSIZE="${OBJSIZE}" \
        READELF="${READELF}" \
        STRIP="${STRIP}" \
        ${V:+V=${V}} \
        "${@}"
    set +x
}

function build_kernel() {
    # Build silently by default
    ${VERBOSE:=false} || SILENT_MAKE_FLAG=s

    # Configure the kernel
    CONFIG_MAKE_TARGETS=("${CONFIG##*/}")
    ${INCREMENTAL:=false} || CONFIG_MAKE_TARGETS=(distclean "${CONFIG_MAKE_TARGETS[@]}")
    kmake "${CONFIG_MAKE_TARGETS[@]}"

    if ${UPDATE_CONFIG_ONLY:=false}; then
        FINAL_MAKE_TARGETS=(savedefconfig)
    elif ! ${RELEASE:=false}; then
        case "$(id -un)@$(uname -n)" in
            nathan@ubuntu-* | nathan@Ryzen-9-3900X)
                set -x
                "${KRNL_SRC}"/scripts/config \
                    --file "${O}"/.config \
                    -d MCORE2 \
                    -e MZEN2 \
                    -d CC_OPTIMIZE_FOR_PERFORMANCE \
                    -e CC_OPTIMIZE_FOR_PERFORMANCE_O3
                set +x
                rg "CONFIG_MZEN2|CONFIG_CC_OPTIMIZE_FOR_PERFORMANCE_O3" "${O}"/.config
                ;;
        esac
    fi
    [[ -z ${FINAL_MAKE_TARGETS[*]} ]] && FINAL_MAKE_TARGETS=("${MY_TARGETS[@]}")
    kmake olddefconfig "${FINAL_MAKE_TARGETS[@]}"

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
