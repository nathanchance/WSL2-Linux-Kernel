#!/usr/bin/env fish

set krnl_root (dirname (status dirname))
cd $krnl_root; or exit

set i 1
while test $i -le (count $argv)
    set arg $argv[$i]
    switch $arg
        case -f --force
            set force true

        case -n --no-build
            set build false

        case -r --release
            set release true

        case -t --tag
            set tag true

        case -v --version
            set next (math $i + 1)
            set version_number $argv[$next]
            set i $next

        case -v'*'
            set version_number (string replace -- '-v' '' $arg)
    end
    set i (math $i + 1)
end

if test "$release" != true; and test "$tag" != true
    print_error "Either --tag or --release needs to be specified"
    exit 1
end

if not set -q version_number
    print_error "Version number must be specified!"
    exit 1
end

if not test -f localversion-next
    print_error "localversion-next could not be found!"
    exit 1
end

if not test -x $CBL_BIN/clang
    print_error "$CBL_BIN/clang does not exist!"
    exit 1
end

set next_version (cat localversion-next | string replace '-' '')

set release_tag wsl2-cbl-kernel-$next_version-v$version_number

if test "$tag" = true
    if test "$force" = true
        git tag -d $release_tag
    end

    set clang_version (echo __clang_version__ | $CBL_BIN/clang -E -xc - | tail -1 | string split -f 1 ' ' | string split -f 2 '"')
    set clang_hash (echo __clang_version__ | $CBL_BIN/clang -E -xc - | tail -1 | string split -f 3 ' ' | string split -f 1 ')')

    git tag \
       --annotate $release_tag \
       --edit \
       --message "Clang Built WSL2 Kernel v$version_number

* Built with clang $clang_version at https://github.com/llvm/llvm-project/commit/$clang_hash

* Updated to $next_version" \
       --sign; or exit

    if test "$build" != "false"
        bin/build.fish
    end
end

if test "$release" = true
    set kernel build/x86_64/arch/x86/boot/bzImage
    if not test -f $kernel
        print_error "A kernel needs to be built to create a release"
        exit 1
    end
    if not git rev-parse $release_tag &>/dev/null
        print_error "Could not find $release_tag!"
        exit 1
    end

    git push -f origin $release_tag; or exit
    hub release create \
       -a $kernel \
       -m (git for-each-ref refs/tags/$release_tag --format='%(contents)' | sed '/BEGIN PGP SIGNATURE/Q' | string collect) \
       $release_tag; or exit
end
