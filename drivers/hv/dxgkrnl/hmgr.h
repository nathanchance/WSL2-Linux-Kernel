/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2019, Microsoft Corporation.
 *
 * Author:
 *   Iouri Tarassov <iourit@linux.microsoft.com>
 *
 * Dxgkrnl Graphics Driver
 * Handle manager definitions
 *
 */

#ifndef _HMGR_H_
#define _HMGR_H_

#include "misc.h"

struct hmgrentry;

/*
 * Handle manager table.
 *
 * Implementation notes:
 *   A list of free handles is built on top of the array of table entries.
 *   free_handle_list_head is the index of the first entry in the list.
 *   m_FreeHandleListTail is the index of an entry in the list, which is
 *   HMGRTABLE_MIN_FREE_ENTRIES from the head. It means that when a handle is
 *   freed, the next time the handle can be re-used is after allocating
 *   HMGRTABLE_MIN_FREE_ENTRIES number of handles.
 *   Handles are allocated from the start of the list and free handles are
 *   inserted after the tail of the list.
 *
 */
struct hmgrtable {
	struct dxgprocess	*process;
	struct hmgrentry	*entry_table;
	u32			free_handle_list_head;
	u32			free_handle_list_tail;
	u32			table_size;
	u32			free_count;
	struct rw_semaphore	table_lock;
};

/*
 * Handle entry data types.
 */
#define HMGRENTRY_TYPE_BITS 5

enum hmgrentry_type {
	HMGRENTRY_TYPE_FREE				= 0,
	HMGRENTRY_TYPE_DXGADAPTER			= 1,
	HMGRENTRY_TYPE_DXGSHAREDRESOURCE		= 2,
	HMGRENTRY_TYPE_DXGDEVICE			= 3,
	HMGRENTRY_TYPE_DXGRESOURCE			= 4,
	HMGRENTRY_TYPE_DXGALLOCATION			= 5,
	HMGRENTRY_TYPE_DXGOVERLAY			= 6,
	HMGRENTRY_TYPE_DXGCONTEXT			= 7,
	HMGRENTRY_TYPE_DXGSYNCOBJECT			= 8,
	HMGRENTRY_TYPE_DXGKEYEDMUTEX			= 9,
	HMGRENTRY_TYPE_DXGPAGINGQUEUE			= 10,
	HMGRENTRY_TYPE_DXGDEVICESYNCOBJECT		= 11,
	HMGRENTRY_TYPE_DXGPROCESS			= 12,
	HMGRENTRY_TYPE_DXGSHAREDVMOBJECT		= 13,
	HMGRENTRY_TYPE_DXGPROTECTEDSESSION		= 14,
	HMGRENTRY_TYPE_DXGHWQUEUE			= 15,
	HMGRENTRY_TYPE_DXGREMOTEBUNDLEOBJECT		= 16,
	HMGRENTRY_TYPE_DXGCOMPOSITIONSURFACEOBJECT	= 17,
	HMGRENTRY_TYPE_DXGCOMPOSITIONSURFACEPROXY	= 18,
	HMGRENTRY_TYPE_DXGTRACKEDWORKLOAD		= 19,
	HMGRENTRY_TYPE_LIMIT		= ((1 << HMGRENTRY_TYPE_BITS) - 1),
	HMGRENTRY_TYPE_MONITOREDFENCE	= HMGRENTRY_TYPE_LIMIT + 1,
};

#endif
