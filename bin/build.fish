#!/usr/bin/env fish

set bin_folder (realpath (status dirname))
set krnl_src (dirname $bin_folder)

PO=$bin_folder mka -C $krnl_src HOSTLDFLAGS=-fuse-ld=lld KCFLAGS=-Werror LLVM=1 LLVM_IAS=1 O=build/x86_64 distclean wsl2_defconfig bzImage $argv
