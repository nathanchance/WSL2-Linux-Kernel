/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_COMPILER_H
#define __ASM_COMPILER_H

#ifdef CONFIG_CFI_CLANG
/*
 * With CONFIG_CFI_CLANG, the compiler replaces function address
 * references with the address of the function's CFI jump table
 * entry. The function_nocfi macro always returns the address of the
 * actual function instead.
 */
#define function_nocfi(x) ({						\
	void *addr;							\
	asm("leaq " __stringify(x) "(%%rip), %0\n\t" : "=r" (addr));	\
	addr;								\
})
#endif

#endif
