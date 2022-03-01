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

struct dxgprocess;
struct dxgadapter;

#include "misc.h"
#include "hmgr.h"
#include <uapi/misc/d3dkmthk.h>

struct dxgk_device_types {
	u32 post_device:1;
	u32 post_device_certain:1;
	u32 software_device:1;
	u32 soft_gpu_device:1;
	u32 warp_device:1;
	u32 bdd_device:1;
	u32 support_miracast:1;
	u32 mismatched_lda:1;
	u32 indirect_display_device:1;
	u32 xbox_one_device:1;
	u32 child_id_support_dwm_clone:1;
	u32 child_id_support_dwm_clone2:1;
	u32 has_internal_panel:1;
	u32 rfx_vgpu_device:1;
	u32 virtual_render_device:1;
	u32 support_preserve_boot_display:1;
	u32 is_uefi_frame_buffer:1;
	u32 removable_device:1;
	u32 virtual_monitor_device:1;
};

enum dxgobjectstate {
	DXGOBJECTSTATE_CREATED,
	DXGOBJECTSTATE_ACTIVE,
	DXGOBJECTSTATE_STOPPED,
	DXGOBJECTSTATE_DESTROYED,
};

struct dxgvmbuschannel {
	struct vmbus_channel	*channel;
	struct hv_device	*hdev;
	struct dxgadapter	*adapter;
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

	/* list of created adapters */
	struct list_head	adapter_list_head;
	struct rw_semaphore	adapter_list_lock;

	/* List of all current threads for lock order tracking. */
	struct mutex		thread_info_mutex;
	struct list_head	thread_info_list_head;

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

int dxgglobal_create_adapter(struct pci_dev *dev, guid_t *guid,
			     struct winluid host_vgpu_luid);
void dxgglobal_acquire_adapter_list_lock(enum dxglockstate state);
void dxgglobal_release_adapter_list_lock(enum dxglockstate state);
int dxgglobal_init_global_channel(void);
void dxgglobal_destroy_global_channel(void);
struct vmbus_channel *dxgglobal_get_vmbus(void);
struct dxgvmbuschannel *dxgglobal_get_dxgvmbuschannel(void);
int dxgglobal_acquire_channel_lock(void);
void dxgglobal_release_channel_lock(void);

/*
 * The structure represents a process, which opened the /dev/dxg device.
 * A corresponding object is created on the host.
 */
struct dxgprocess {
	/*
	 * Process list entry in dxgglobal.
	 * Protected by the dxgglobal->plistmutex.
	 */
	struct list_head	plistentry;
	struct task_struct	*process;
	pid_t			pid;
	pid_t			tgid;
	/* how many time the process was opened */
	struct kref		process_kref;
	/*
	 * This handle table is used for all objects except dxgadapter
	 * The handle table lock order is higher than the local_handle_table
	 * lock
	 */
	struct hmgrtable	handle_table;
	/*
	 * This handle table is used for dxgadapter objects.
	 * The handle table lock order is lowest.
	 */
	struct hmgrtable	local_handle_table;
	/* Handle of the corresponding objec on the host */
	struct d3dkmthandle	host_handle;
};

struct dxgprocess *dxgprocess_create(void);
void dxgprocess_destroy(struct dxgprocess *process);
void dxgprocess_release(struct kref *refcount);
void dxgprocess_ht_lock_shared_down(struct dxgprocess *process);
void dxgprocess_ht_lock_shared_up(struct dxgprocess *process);
void dxgprocess_ht_lock_exclusive_down(struct dxgprocess *process);
void dxgprocess_ht_lock_exclusive_up(struct dxgprocess *process);

enum dxgadapter_state {
	DXGADAPTER_STATE_ACTIVE		= 0,
	DXGADAPTER_STATE_STOPPED	= 1,
	DXGADAPTER_STATE_WAITING_VMBUS	= 2,
};

/*
 * This object represents the grapchis adapter.
 * Objects, which take reference on the adapter:
 * - dxgglobal
 * - adapter handle (struct d3dkmthandle)
 */
struct dxgadapter {
	struct rw_semaphore	core_lock;
	struct kref		adapter_kref;
	/* Entry in the list of adapters in dxgglobal */
	struct list_head	adapter_list_entry;
	struct pci_dev		*pci_dev;
	struct hv_device	*hv_dev;
	struct dxgvmbuschannel	channel;
	struct d3dkmthandle	host_handle;
	enum dxgadapter_state	adapter_state;
	struct winluid		host_adapter_luid;
	struct winluid		host_vgpu_luid;
	struct winluid		luid;	/* VM bus channel luid */
	u16			device_description[80];
	u16			device_instance_id[WIN_MAX_PATH];
	bool			stopping_adapter;
};

int dxgadapter_set_vmbus(struct dxgadapter *adapter, struct hv_device *hdev);
bool dxgadapter_is_active(struct dxgadapter *adapter);
void dxgadapter_start(struct dxgadapter *adapter);
void dxgadapter_stop(struct dxgadapter *adapter);
void dxgadapter_release(struct kref *refcount);
int dxgadapter_acquire_lock_shared(struct dxgadapter *adapter);
void dxgadapter_release_lock_shared(struct dxgadapter *adapter);
int dxgadapter_acquire_lock_exclusive(struct dxgadapter *adapter);
void dxgadapter_acquire_lock_forced(struct dxgadapter *adapter);
void dxgadapter_release_lock_exclusive(struct dxgadapter *adapter);

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
int dxgvmb_send_create_process(struct dxgprocess *process);
int dxgvmb_send_destroy_process(struct d3dkmthandle process);
int dxgvmb_send_open_adapter(struct dxgadapter *adapter);
int dxgvmb_send_close_adapter(struct dxgadapter *adapter);
int dxgvmb_send_get_internal_adapter_info(struct dxgadapter *adapter);
int dxgvmb_send_async_msg(struct dxgvmbuschannel *channel,
			  void *command,
			  u32 cmd_size);

int ntstatus2int(struct ntstatus status);

#endif
