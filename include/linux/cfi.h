/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Clang Control Flow Integrity (CFI) support.
 *
 * Copyright (C) 2019 Google LLC
 */
#ifndef _LINUX_CFI_H
#define _LINUX_CFI_H

#include <linux/stringify.h>

#ifdef CONFIG_CFI_CLANG
typedef void (*cfi_check_fn)(uint64_t id, void *ptr, void *diag);

/* Compiler-generated function in each module, and the kernel */
extern void __cfi_check(uint64_t id, void *ptr, void *diag);

/*
 * Forces the compiler to generate a CFI jump table entry for a function
 * and stores the jump table address to __cfi_jt_function.
 */
#define __CFI_ADDRESSABLE(fn) \
	const void* __cfi_jt_ ## fn __visible = (void *)&fn;

#ifdef CONFIG_MODULES
#ifdef CONFIG_CFI_CLANG_SHADOW
extern void cfi_module_add(struct module *mod, unsigned long min_addr,
	unsigned long max_addr);

extern void cfi_module_remove(struct module *mod, unsigned long min_addr,
	unsigned long max_addr);
#else
static inline void cfi_module_add(struct module *mod, unsigned long min_addr,
	unsigned long max_addr)
{
}

static inline void cfi_module_remove(struct module *mod, unsigned long min_addr,
	unsigned long max_addr)
{
}
#endif /* CONFIG_CFI_CLANG_SHADOW */
#endif /* CONFIG_MODULES */

#else /* !CONFIG_CFI_CLANG */

#define __CFI_ADDRESSABLE(fn)

#endif /* CONFIG_CFI_CLANG */

#endif /* _LINUX_CFI_H */
