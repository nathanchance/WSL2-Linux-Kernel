#!/usr/bin/env bash

set -eu

BASE="$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")" || return; pwd)"

# Get parameters
while (( ${#} )); do
    case ${1} in
        *=*) export "${1?}" ;;
        -j|--jobs) JOBS=${1} ;;
        -u|--update-config-only) UPDATE_CONFIG_ONLY=true ;;
        -v|--verbose) VERBOSE=true ;;
    esac
    shift
done

# Add toolchain folders to PATH and request path override (PO environment variable)
export PATH="${PO:+${PO}:}${CBL_LLVM:+${CBL_LLVM}:}${CBL_BNTL:+${CBL_BNTL}:}${PATH}"

# Set default values if user did not supply them above
: "${AR:=llvm-ar}" \
  "${CC:=clang}" \
  "${HOSTAR:=llvm-ar}" \
  "${HOSTCC:=clang}" \
  "${HOSTLD:=ld.lld}" \
  "${HOSTLDFLAGS:=-fuse-ld=lld}" \
  "${JOBS:="$(nproc)"}" \
  "${LD:=ld.lld}" \
  "${LLVM_AR:=llvm-ar}" \
  "${LLVM_NM:=llvm-nm}" \
  "${NM:=llvm-nm}" \
  "${O:=${BASE}/out.x86_64}" \
  "${OBJCOPY:=llvm-objcopy}" \
  "${OBJDUMP:=llvm-objdump}" \
  "${STRIP:=llvm-strip}"

printf '\n\e[01;32mToolchain location:\e[0m %s\n\n' "$(dirname "$(command -v "${CC}")")"
printf '\e[01;32mToolchain version:\e[0m %s \n\n' "$("${CC}" --version | head -n1)"

# Build the kernel
MAKE_TARGETS=( distclean olddefconfig )
${UPDATE_CONFIG_ONLY:=false} || MAKE_TARGETS=( "${MAKE_TARGETS[@]}" all )
${VERBOSE:=false} || COND_MAKE_ARGS=( -s )
set -x
time make -C "${BASE}" \
          -j"${JOBS}" \
          ${COND_MAKE_ARGS:+"${COND_MAKE_ARGS[@]}"} \
          AR="${AR}" \
          CC="${CC}" \
          HOSTAR="${AR}" \
          HOSTCC="${HOSTCC}" \
          HOSTLD="${HOSTLD}" \
          HOSTLDFLAGS="${HOSTLDFLAGS}" \
          KCONFIG_CONFIG="${BASE}"/Microsoft/config-wsl \
          KCONFIG_OVERWRITECONFIG=true \
          LD="${LD}" \
          LLVM_AR="${LLVM_AR}" \
          LLVM_NM="${LLVM_NM}" \
          NM="${NM}" \
          O="${O}" \
          OBJCOPY="${OBJCOPY}" \
          OBJDUMP="${OBJDUMP}" \
          STRIP="${STRIP}" \
          ${V:+V=${V}} \
          "${MAKE_TARGETS[@]}"
set +x
${UPDATE_CONFIG_ONLY} && exit 0

# Let the user know where the kernel will be (if we built one)
KERNEL=${O}/arch/x86/boot/bzImage
[[ -f ${KERNEL} ]] && printf '\n\e[01;32mKernel is now available at:\e[0m %s\n' "${KERNEL}"
