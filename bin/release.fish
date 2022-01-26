#!/usr/bin/env fish

in_container; or exit

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

set next_version (cat localversion-next | string replace '-' '')

set release_tag wsl2-cbl-kernel-$next_version-v$version_number

set tag_title "Clang Built WSL2 Kernel v$version_number"
set msg_file /tmp/wsl2-release-msg.txt

if test "$tag" = true
    if test "$force" = true
        git tag -d $release_tag
    end

    set clang_version_string (echo __clang_version__ | clang -E -xc - | tail -1)
    set clang_version (string split -f 1 ' ' $clang_version_string | string split -f 2 '"')
    set clang_hash (string split -f 3 ' ' $clang_version_string | string split -f 1 ')')

    echo "* Built with clang $clang_version at https://github.com/llvm/llvm-project/commit/$clang_hash

* Updated to $next_version" >$msg_file
    vim $msg_file; or exit

    git tag \
        --annotate $release_tag \
        --message "$tag_title

"(cat $msg_file | string collect) \
        --sign; or exit

    if test "$build" != false
        bin/build.fish
    end
end

if test "$release" = true
    set kernel arch/x86/boot/bzImage
    if not test -f $kernel
        print_error "A kernel needs to be built to create a release"
        exit 1
    end
    if not git rev-parse $release_tag &>/dev/null
        print_error "Could not find $release_tag!"
        exit 1
    end

    git push -f origin $release_tag; or exit
    gh release create \
       $release_tag \
       $kernel \
       -F $msg_file \
       -t "$tag_file"; or exit
end
