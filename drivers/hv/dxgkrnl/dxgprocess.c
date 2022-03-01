// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019, Microsoft Corporation.
 *
 * Author:
 *   Iouri Tarassov <iourit@linux.microsoft.com>
 *
 * Dxgkrnl Graphics Driver
 * DXGPROCESS implementation
 *
 */

#include "dxgkrnl.h"

#undef pr_fmt
#define pr_fmt(fmt)	"dxgk: " fmt

/*
 * Creates a new dxgprocess object
 * Must be called when dxgglobal->plistmutex is held
 */
struct dxgprocess *dxgprocess_create(void)
{
	struct dxgprocess *process;
	int ret;

	process = vzalloc(sizeof(struct dxgprocess));
	if (process != NULL) {
		pr_debug("new dxgprocess created\n");
		process->process = current;
		process->pid = current->pid;
		process->tgid = current->tgid;
		ret = dxgvmb_send_create_process(process);
		if (ret < 0) {
			pr_debug("send_create_process failed\n");
			vfree(process);
			process = NULL;
		} else {
			INIT_LIST_HEAD(&process->plistentry);
			kref_init(&process->process_kref);

			mutex_lock(&dxgglobal->plistmutex);
			list_add_tail(&process->plistentry,
				      &dxgglobal->plisthead);
			mutex_unlock(&dxgglobal->plistmutex);

			hmgrtable_init(&process->handle_table, process);
			hmgrtable_init(&process->local_handle_table, process);
		}
	}
	return process;
}

void dxgprocess_destroy(struct dxgprocess *process)
{
	hmgrtable_destroy(&process->handle_table);
	hmgrtable_destroy(&process->local_handle_table);
}

void dxgprocess_release(struct kref *refcount)
{
	struct dxgprocess *process;

	process = container_of(refcount, struct dxgprocess, process_kref);

	mutex_lock(&dxgglobal->plistmutex);
	list_del(&process->plistentry);
	process->plistentry.next = NULL;
	mutex_unlock(&dxgglobal->plistmutex);

	dxgprocess_destroy(process);

	if (process->host_handle.v)
		dxgvmb_send_destroy_process(process->host_handle);
	vfree(process);
}

void dxgprocess_ht_lock_shared_down(struct dxgprocess *process)
{
	hmgrtable_lock(&process->handle_table, DXGLOCK_SHARED);
}

void dxgprocess_ht_lock_shared_up(struct dxgprocess *process)
{
	hmgrtable_unlock(&process->handle_table, DXGLOCK_SHARED);
}

void dxgprocess_ht_lock_exclusive_down(struct dxgprocess *process)
{
	hmgrtable_lock(&process->handle_table, DXGLOCK_EXCL);
}

void dxgprocess_ht_lock_exclusive_up(struct dxgprocess *process)
{
	hmgrtable_unlock(&process->handle_table, DXGLOCK_EXCL);
}
