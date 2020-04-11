#!/usr/bin/env bash

function clang_version() {
    echo __clang_version__ | "${1:?}" -E -xc - | tail -n1 | awk '{print $1}' | cut -d \" -f 2
}

function clang_hash() {
    echo __clang_version__ | "${1:?}" -E -xc - | tail -n1 | awk '{print $3}' | cut -d \) -f 1
}

function die() {
    printf "\n\033[01;31m%s\033[0m\n" "${1}"
    exit "${2:-33}"
}

[[ -n ${CBL_LLVM} && -d ${CBL_LLVM} ]] || die "LLVM folder could not be found"

KERNEL=out/x86_64/arch/x86/boot/bzImage
[[ -f ${KERNEL} ]] || die "A kernel needs to be built to create a release"

NEXT_TAG=$(git describe --abbrev=0 next/master)
[[ -z ${NEXT_TAG} ]] && die "next tag could not be found!"

VER_NUM=${1}
[[ -z ${VER_NUM} ]] && die "Version number needs to be the first parameter"

MY_TAG=wsl2-cbl-kernel-${NEXT_TAG}-v${VER_NUM}
git tag --annotate "${MY_TAG}" \
        --edit \
        --force \
        --message "Clang Built WSL2 Kernel v${VER_NUM}

* Build with clang $(clang_version "${CBL_LLVM}"/clang) at https://github.com/llvm/llvm-project/commit/$(clang_hash "${CBL_LLVM}"/clang)" \
        --sign || die "Error creating tag"

git push --force origin "${MY_TAG}" || die "Error pushing tag"

hub release create -a "${KERNEL}" \
                   -m "$(git for-each-ref refs/tags/"${MY_TAG}" --format='%(contents)' | sed '/BEGIN PGP SIGNATURE/Q')" \
                   "${MY_TAG}" || die "Error creating release"
