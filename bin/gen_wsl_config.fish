#!/usr/bin/env fish

set bin_folder (realpath (status dirname))
set krnl_src (dirname $bin_folder)

set i 1
while test $i -le (count $argv)
    set arg $argv[$i]
    switch $arg
        case -k --kernel-src
            set next (math $i + 1)
            set krnl_src $argv[$next]
            set i $next
    end
    set i (math $i + 1)
end

cd $krnl_src; or exit

set config arch/x86/configs/wsl2_defconfig

curl -LSso $config https://github.com/microsoft/WSL2-Linux-Kernel/raw/linux-msft-wsl-5.10.y/Microsoft/config-wsl

# Initial tuning
#   * FTRACE: Limit attack surface and avoids a warning at boot.
#   * MODULES: Limit attack surface and we don't support them anyways.
#   * LTO_CLANG: Optimization.
#   * CFI_CLANG: Hardening.
#   * LOCALVERSION_AUTO: Helpful when running development builds.
#   * LOCALVERSION: Replace 'standard' with 'cbl' since this is a Clang built kernel.
#   * FRAME_WARN: The 64-bit default is 2048. Clang uses more stack space so this avoids build-time warnings.
$krnl_src/scripts/config \
    --file $config \
    -d FTRACE \
    -d MODULES \
    -d LTO_NONE \
    -e LTO_CLANG \
    -e LTO_CLANG_THIN \
    -e CFI_CLANG \
    -e LOCALVERSION_AUTO \
    --set-str LOCALVERSION "-microsoft-cbl" \
    -u FRAME_WARN

# Enable/disable a bunch of checks based on kconfig-hardened-check
# https://github.com/a13xp0p0v/kconfig-hardened-check
$krnl_src/scripts/config \
    --file $config \
    -d AIO \
    -d DEBUG_FS \
    -d DEVMEM \
    -d HARDENED_USERCOPY_FALLBACK \
    -d INIT_STACK_NONE \
    -d KSM \
    -d LEGACY_PTYS \
    -d PROC_KCORE \
    -d VT \
    -d X86_IOPL_IOPERM \
    -e BUG_ON_DATA_CORRUPTION \
    -e DEBUG_CREDENTIALS \
    -e DEBUG_LIST \
    -e DEBUG_NOTIFIERS \
    -e DEBUG_SG \
    -e DEBUG_VIRTUAL \
    -e DEBUG_WX \
    -e FORTIFY_SOURCE \
    -e HARDENED_USERCOPY \
    -e INIT_STACK_ALL \
    -e INIT_STACK_ALL_ZERO \
    -e INTEGRITY \
    -e LOCK_DOWN_KERNEL_FORCE_CONFIDENTIALITY \
    -e SECURITY_LOADPIN \
    -e SECURITY_LOADPIN_ENFORCE \
    -e SECURITY_LOCKDOWN_LSM \
    -e SECURITY_LOCKDOWN_LSM_EARLY \
    -e SECURITY_SAFESETID \
    -e SECURITY_YAMA \
    -e SLAB_FREELIST_HARDENED \
    -e SLAB_FREELIST_RANDOM \
    -e SLUB_DEBUG \
    -e SHUFFLE_PAGE_ALLOCATOR \
    --set-val ARCH_MMAP_RND_BITS 32

# Enable F2FS support for direct mounting
$krnl_src/scripts/config \
    --file $config \
    -e F2FS_FS \
    -e FS_ENCRYPTION

# Enable WireGuard support
$krnl_src/scripts/config \
    --file $config \
    -e NETFILTER_XT_MATCH_CONNMARK \
    -e WIREGUARD

# Enable io_uring support
$krnl_src/scripts/config \
    --file $config \
    -e IO_URING

mka -C $krnl_src LLVM=1 LLVM_IAS=1 O=build/x86_64 distclean wsl2_defconfig savedefconfig
cp -v build/x86_64/defconfig $config
