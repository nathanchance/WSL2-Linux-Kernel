/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2019, Microsoft Corporation.
 *
 * Author:
 *   Iouri Tarassov <iourit@linux.microsoft.com>
 *
 * Dxgkrnl Graphics Driver
 * Headers for internal objects
 *
 */

#ifndef _DXGKRNL_H
#define _DXGKRNL_H

#include <linux/uuid.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include <linux/refcount.h>
#include <linux/rwsem.h>
#include <linux/atomic.h>
#include <linux/spinlock.h>
#include <linux/gfp.h>
#include <linux/miscdevice.h>
#include <linux/pci.h>
#include <linux/hyperv.h>

struct dxgadapter;

#include "misc.h"
#include <uapi/misc/d3dkmthk.h>

struct dxgvmbuschannel {
	struct vmbus_channel	*channel;
	struct hv_device	*hdev;
	spinlock_t		packet_list_mutex;
	struct list_head	packet_list_head;
	struct kmem_cache	*packet_cache;
	atomic64_t		packet_request_id;
};

int dxgvmbuschannel_init(struct dxgvmbuschannel *ch, struct hv_device *hdev);
void dxgvmbuschannel_destroy(struct dxgvmbuschannel *ch);
void dxgvmbuschannel_receive(void *ctx);

/*
 * The structure defines an offered vGPU vm bus channel.
 */
struct dxgvgpuchannel {
	struct list_head	vgpu_ch_list_entry;
	struct winluid		adapter_luid;
	struct hv_device	*hdev;
};

struct dxgglobal {
	struct dxgvmbuschannel	channel;
	struct delayed_work	dwork;
	struct hv_device	*hdev;
	u32			num_adapters;
	u32			vmbus_ver;	/* Interface version */
	struct resource		*mem;
	u64			mmiospace_base;
	u64			mmiospace_size;
	struct miscdevice	dxgdevice;
	struct mutex		device_mutex;

	/*  list of created  processes */
	struct list_head	plisthead;
	struct mutex		plistmutex;

	/*
	 * List of the vGPU VM bus channels (dxgvgpuchannel)
	 * Protected by device_mutex
	 */
	struct list_head	vgpu_ch_list_head;

	/* protects acces to the global VM bus channel */
	struct rw_semaphore	channel_lock;

	bool			dxg_dev_initialized;
	bool			vmbus_registered;
	bool			pci_registered;
	bool			global_channel_initialized;
	bool			async_msg_enabled;
};

extern struct dxgglobal		*dxgglobal;

int dxgglobal_init_global_channel(void);
void dxgglobal_destroy_global_channel(void);
struct vmbus_channel *dxgglobal_get_vmbus(void);
struct dxgvmbuschannel *dxgglobal_get_dxgvmbuschannel(void);
int dxgglobal_acquire_channel_lock(void);
void dxgglobal_release_channel_lock(void);

struct dxgprocess {
	/* Placeholder */
};

void init_ioctls(void);
long dxgk_compat_ioctl(struct file *f, unsigned int p1, unsigned long p2);
long dxgk_unlocked_ioctl(struct file *f, unsigned int p1, unsigned long p2);

static inline void guid_to_luid(guid_t *guid, struct winluid *luid)
{
	*luid = *(struct winluid *)&guid->b[0];
}

/*
 * VM bus interface
 *
 */

/*
 * The interface version is used to ensure that the host and the guest use the
 * same VM bus protocol. It needs to be incremented every time the VM bus
 * interface changes. DXGK_VMBUS_LAST_COMPATIBLE_INTERFACE_VERSION is
 * incremented each time the earlier versions of the interface are no longer
 * compatible with the current version.
 */
#define DXGK_VMBUS_INTERFACE_VERSION_OLD		27
#define DXGK_VMBUS_INTERFACE_VERSION			40
#define DXGK_VMBUS_LAST_COMPATIBLE_INTERFACE_VERSION	16

void dxgvmb_initialize(void);
int dxgvmb_send_set_iospace_region(u64 start, u64 len,
				   struct vmbus_gpadl *shared_mem_gpadl);

int ntstatus2int(struct ntstatus status);

#endif
