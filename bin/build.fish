#!/usr/bin/env fish

set bin_folder (realpath (status dirname))
set krnl_src (dirname $bin_folder)

if test -n "$PO"
    set build_po "$PO:$bin_folder"
else
    set build_po $bin_folder
end

PO="$build_po" kmake -C $krnl_src HOSTLDFLAGS=-fuse-ld=lld KCFLAGS=-Werror LLVM=1 LLVM_IAS=1 O=build/x86_64 distclean wsl2_defconfig bzImage $argv
