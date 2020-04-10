// SPDX-License-Identifier: GPL-2.0
/*
 * Shadow Call Stack support.
 *
 * Copyright (C) 2019 Google LLC
 */

#include <linux/percpu.h>
#include <linux/vmalloc.h>
#include <asm/pgtable.h>
#include <asm/scs.h>

#define DECLARE_SCS(name)						\
	DECLARE_PER_CPU(unsigned long *, name ## _ptr);			\
	DECLARE_PER_CPU(unsigned long [SCS_SIZE/sizeof(long)], name)

#ifdef CONFIG_SHADOW_CALL_STACK_VMAP
#define DEFINE_SCS(name)						\
	DEFINE_PER_CPU(unsigned long *, name ## _ptr)
#else
/* Allocate a static per-CPU shadow stack */
#define DEFINE_SCS(name)						\
	DEFINE_PER_CPU(unsigned long *, name ## _ptr);			\
	DEFINE_PER_CPU(unsigned long [SCS_SIZE/sizeof(long)], name)	\
		__aligned(SCS_SIZE)
#endif /* CONFIG_SHADOW_CALL_STACK_VMAP */

DECLARE_SCS(irq_shadow_call_stack);
DECLARE_SCS(sdei_shadow_call_stack_normal);
DECLARE_SCS(sdei_shadow_call_stack_critical);

DEFINE_SCS(irq_shadow_call_stack);
#ifdef CONFIG_ARM_SDE_INTERFACE
DEFINE_SCS(sdei_shadow_call_stack_normal);
DEFINE_SCS(sdei_shadow_call_stack_critical);
#endif

static int scs_alloc_percpu(unsigned long * __percpu *ptr, int cpu)
{
	unsigned long *p;

	p = __vmalloc_node_range(PAGE_SIZE, SCS_SIZE,
				 VMALLOC_START, VMALLOC_END,
				 GFP_SCS, PAGE_KERNEL,
				 0, cpu_to_node(cpu),
				 __builtin_return_address(0));

	if (!p)
		return -ENOMEM;
	per_cpu(*ptr, cpu) = p;

	return 0;
}

static void scs_free_percpu(unsigned long * __percpu *ptr, int cpu)
{
	unsigned long *p = per_cpu(*ptr, cpu);

	if (p) {
		per_cpu(*ptr, cpu) = NULL;
		vfree(p);
	}
}

static void scs_free_sdei(void)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		scs_free_percpu(&sdei_shadow_call_stack_normal_ptr, cpu);
		scs_free_percpu(&sdei_shadow_call_stack_critical_ptr, cpu);
	}
}

void scs_init_irq(void)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		if (IS_ENABLED(CONFIG_SHADOW_CALL_STACK_VMAP))
			WARN_ON(scs_alloc_percpu(&irq_shadow_call_stack_ptr,
						 cpu));
		else
			per_cpu(irq_shadow_call_stack_ptr, cpu) =
				per_cpu(irq_shadow_call_stack, cpu);
	}
}

int scs_init_sdei(void)
{
	int cpu;

	if (!IS_ENABLED(CONFIG_ARM_SDE_INTERFACE))
		return 0;

	for_each_possible_cpu(cpu) {
		if (IS_ENABLED(CONFIG_SHADOW_CALL_STACK_VMAP)) {
			if (scs_alloc_percpu(
				&sdei_shadow_call_stack_normal_ptr, cpu) ||
			    scs_alloc_percpu(
				&sdei_shadow_call_stack_critical_ptr, cpu)) {
				scs_free_sdei();
				return -ENOMEM;
			}
		} else {
			per_cpu(sdei_shadow_call_stack_normal_ptr, cpu) =
				per_cpu(sdei_shadow_call_stack_normal, cpu);
			per_cpu(sdei_shadow_call_stack_critical_ptr, cpu) =
				per_cpu(sdei_shadow_call_stack_critical, cpu);
		}
	}

	return 0;
}
