/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Shadow Call Stack support.
 *
 * Copyright (C) 2019 Google LLC
 */

#ifndef _LINUX_SCS_H
#define _LINUX_SCS_H

#include <linux/gfp.h>
#include <linux/poison.h>
#include <linux/sched.h>
#include <asm/page.h>

#ifdef CONFIG_SHADOW_CALL_STACK

/*
 * In testing, 1 KiB shadow stack size (i.e. 128 stack frames on a 64-bit
 * architecture) provided ~40% safety margin on stack usage while keeping
 * memory allocation overhead reasonable.
 */
#define SCS_SIZE	1024UL
#define GFP_SCS		(GFP_KERNEL | __GFP_ZERO)

/* An illegal pointer value to mark the end of the shadow stack. */
#define SCS_END_MAGIC	(0x5f6UL + POISON_POINTER_DELTA)

#define task_scs(tsk)	(task_thread_info(tsk)->shadow_call_stack)

static inline void task_set_scs(struct task_struct *tsk, void *s)
{
	task_scs(tsk) = s;
}

extern void scs_init(void);

static inline void *__scs_base(struct task_struct *tsk)
{
	/*
	 * To minimize the risk of exposure, architectures may clear a
	 * task's thread_info::shadow_call_stack while that task is
	 * running, and only save/restore the active shadow call stack
	 * pointer when the usual register may be clobbered (e.g. across
	 * context switches).
	 *
	 * The shadow call stack is aligned to SCS_SIZE, and grows
	 * upwards, so we can mask out the low bits to extract the base
	 * when the task is not running.
	 */
	return (void *)((unsigned long)task_scs(tsk) & ~(SCS_SIZE - 1));
}

static inline void scs_task_reset(struct task_struct *tsk)
{
	/*
	 * Reset the shadow stack to the base address in case the task
	 * is reused.
	 */
	task_set_scs(tsk, __scs_base(tsk));
}

extern int scs_prepare(struct task_struct *tsk, int node);

static inline unsigned long *__scs_magic(void *s)
{
	return (unsigned long *)(s + SCS_SIZE) - 1;
}

static inline bool scs_corrupted(struct task_struct *tsk)
{
	unsigned long *magic = __scs_magic(__scs_base(tsk));

	return READ_ONCE_NOCHECK(*magic) != SCS_END_MAGIC;
}

extern void scs_release(struct task_struct *tsk);

#else /* CONFIG_SHADOW_CALL_STACK */

#define task_scs(tsk)	NULL

static inline void task_set_scs(struct task_struct *tsk, void *s) {}
static inline void scs_init(void) {}
static inline void scs_task_reset(struct task_struct *tsk) {}
static inline int scs_prepare(struct task_struct *tsk, int node) { return 0; }
static inline bool scs_corrupted(struct task_struct *tsk) { return false; }
static inline void scs_release(struct task_struct *tsk) {}

#endif /* CONFIG_SHADOW_CALL_STACK */

#endif /* _LINUX_SCS_H */
