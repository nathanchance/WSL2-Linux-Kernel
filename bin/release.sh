#!/usr/bin/env bash

BASE=$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"/.. && pwd)
cd "${BASE}" || exit ${?}

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

function parse_parameters() {
    while ((${#})); do
        case ${1} in
            -f | --force) FORCE=true ;;
            -n | --no-build) BUILD=false ;;
            -r | --release) RELEASE=true ;;
            -t | --tag) TAG=true ;;
            -v | --version) shift && VER_NUM=${1} ;;
            -v*) VER_NUM=${1//-v/} ;;
        esac
        shift
    done

    : "${RELEASE:=false}" "${TAG:=false}"
    [[ ${RELEASE} == false && ${TAG} == false ]] && die "Either --tag or --release needs to be specified"
    [[ -z ${VER_NUM} ]] && die "Version number must be specified"
}

function set_variables() {
    case "$(id -un)@$(uname -n)" in
        nathan@ubuntu-*) LLVM_BIN=${CBL_LLVM} ;;
        nathan@Ryzen-9-3900X) LLVM_BIN=${HOME}/src/tc-build/install/bin ;;
    esac

    [[ -n ${LLVM_BIN} && -d ${LLVM_BIN} ]] || die "LLVM folder could not be found"

    [[ -f localversion-next ]] || die "localversion-next could not be found!"
    NEXT_VERSION=$(cat localversion-next)

    MY_TAG=wsl2-cbl-kernel${NEXT_VERSION}-v${VER_NUM}
}

function do_tag_and_build() {
    ${TAG} || return 0

    ${FORCE:=false} && git tag -d "${MY_TAG}"
    git tag \
        --annotate "${MY_TAG}" \
        --edit \
        --message "Clang Built WSL2 Kernel v${VER_NUM}

* Built with clang $(clang_version "${LLVM_BIN}"/clang) at https://github.com/llvm/llvm-project/commit/$(clang_hash "${LLVM_BIN}"/clang)

* Updated to ${NEXT_VERSION/-/}" \
        --sign || die "Error creating tag"

    ${BUILD:=true} && bin/build.sh -r
}

function do_release() {
    ${RELEASE} || return 0

    KERNEL=out/x86_64/arch/x86/boot/bzImage
    [[ -f ${KERNEL} ]] || die "A kernel needs to be built to create a release"

    git rev-parse "${MY_TAG}" &>/dev/null || die "Could not find ${MY_TAG}"

    git push --force origin "${MY_TAG}" || die "Error pushing tag"
    hub release create \
        -a "${KERNEL}" \
        -m "$(git for-each-ref refs/tags/"${MY_TAG}" --format='%(contents)' | sed '/BEGIN PGP SIGNATURE/Q')" \
        "${MY_TAG}" || die "Error creating release"
}

parse_parameters "${@}"
set_variables
do_tag_and_build
do_release
