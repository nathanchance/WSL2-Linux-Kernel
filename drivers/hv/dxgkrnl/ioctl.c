// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019, Microsoft Corporation.
 *
 * Author:
 *   Iouri Tarassov <iourit@linux.microsoft.com>
 *
 * Dxgkrnl Graphics Driver
 * Ioctl implementation
 *
 */

#include <linux/eventfd.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/anon_inodes.h>
#include <linux/mman.h>

#include "dxgkrnl.h"
#include "dxgvmbus.h"

#undef pr_fmt
#define pr_fmt(fmt)	"dxgk: " fmt

struct ioctl_desc {
	int (*ioctl_callback)(struct dxgprocess *p, void __user *arg);
	u32 ioctl;
	u32 arg_size;
};
static struct ioctl_desc ioctls[LX_IO_MAX + 1];

static char *errorstr(int ret)
{
	return ret < 0 ? "err" : "";
}

static int dxgk_open_adapter_from_luid(struct dxgprocess *process,
						   void *__user inargs)
{
	struct d3dkmt_openadapterfromluid args;
	int ret;
	struct dxgadapter *entry;
	struct dxgadapter *adapter = NULL;
	struct d3dkmt_openadapterfromluid *__user result = inargs;

	pr_debug("ioctl: %s", __func__);

	ret = copy_from_user(&args, inargs, sizeof(args));
	if (ret) {
		pr_err("%s Faled to copy input args", __func__);
		ret = -EINVAL;
		goto cleanup;
	}

	dxgglobal_acquire_adapter_list_lock(DXGLOCK_SHARED);
	dxgglobal_acquire_process_adapter_lock();

	list_for_each_entry(entry, &dxgglobal->adapter_list_head,
			    adapter_list_entry) {
		if (dxgadapter_acquire_lock_shared(entry) == 0) {
			pr_debug("Compare luids: %d:%d  %d:%d",
				    entry->luid.b, entry->luid.a,
				    args.adapter_luid.b, args.adapter_luid.a);
			if (*(u64 *) &entry->luid ==
			    *(u64 *) &args.adapter_luid) {
				ret =
				    dxgprocess_open_adapter(process, entry,
						    &args.adapter_handle);

				if (ret >= 0) {
					ret = copy_to_user(
						&result->adapter_handle,
						&args.adapter_handle,
						sizeof(struct d3dkmthandle));
					if (ret)
						ret = -EINVAL;
				}
				adapter = entry;
			}
			dxgadapter_release_lock_shared(entry);
			if (adapter)
				break;
		}
	}

	dxgglobal_release_process_adapter_lock();
	dxgglobal_release_adapter_list_lock(DXGLOCK_SHARED);

	if (args.adapter_handle.v == 0)
		ret = -EINVAL;

cleanup:

	if (ret < 0)
		dxgprocess_close_adapter(process, args.adapter_handle);

	pr_debug("ioctl:%s %s %d", errorstr(ret), __func__, ret);
	return ret;
}

static int
dxgkp_enum_adapters(struct dxgprocess *process,
		    union d3dkmt_enumadapters_filter filter,
		    u32 adapter_count_max,
		    struct d3dkmt_adapterinfo *__user info_out,
		    u32 * __user adapter_count_out)
{
	int ret = 0;
	struct dxgadapter *entry;
	struct d3dkmt_adapterinfo *info = NULL;
	struct dxgadapter **adapters = NULL;
	int adapter_count = 0;
	int i;

	pr_debug("ioctl: %s", __func__);
	if (info_out == NULL || adapter_count_max == 0) {
		pr_debug("buffer is NULL");
		ret = copy_to_user(adapter_count_out,
				   &dxgglobal->num_adapters, sizeof(u32));
		if (ret) {
			pr_err("%s copy_to_user faled",	__func__);
			ret = -EINVAL;
		}
		goto cleanup;
	}

	if (adapter_count_max > 0xFFFF) {
		pr_err("too many adapters");
		ret = -EINVAL;
		goto cleanup;
	}

	info = vzalloc(sizeof(struct d3dkmt_adapterinfo) * adapter_count_max);
	if (info == NULL) {
		ret = -ENOMEM;
		goto cleanup;
	}

	adapters = vzalloc(sizeof(struct dxgadapter *) * adapter_count_max);
	if (adapters == NULL) {
		ret = -ENOMEM;
		goto cleanup;
	}

	dxgglobal_acquire_adapter_list_lock(DXGLOCK_SHARED);
	dxgglobal_acquire_process_adapter_lock();

	list_for_each_entry(entry, &dxgglobal->adapter_list_head,
			    adapter_list_entry) {
		if (dxgadapter_acquire_lock_shared(entry) == 0) {
			struct d3dkmt_adapterinfo *inf = &info[adapter_count];

			ret = dxgprocess_open_adapter(process, entry,
						      &inf->adapter_handle);
			if (ret >= 0) {
				inf->adapter_luid = entry->luid;
				adapters[adapter_count] = entry;
				pr_debug("adapter: %x %x:%x",
					    inf->adapter_handle.v,
					    inf->adapter_luid.b,
					    inf->adapter_luid.a);
				adapter_count++;
			}
			dxgadapter_release_lock_shared(entry);
		}
		if (ret < 0)
			break;
	}

	dxgglobal_release_process_adapter_lock();
	dxgglobal_release_adapter_list_lock(DXGLOCK_SHARED);

	if (adapter_count > adapter_count_max) {
		ret = STATUS_BUFFER_TOO_SMALL;
		pr_debug("Too many adapters");
		ret = copy_to_user(adapter_count_out,
				   &dxgglobal->num_adapters, sizeof(u32));
		if (ret) {
			pr_err("%s copy_to_user failed", __func__);
			ret = -EINVAL;
		}
		goto cleanup;
	}

	ret = copy_to_user(adapter_count_out, &adapter_count,
			   sizeof(adapter_count));
	if (ret) {
		pr_err("%s failed to copy adapter_count", __func__);
		ret = -EINVAL;
		goto cleanup;
	}
	ret = copy_to_user(info_out, info, sizeof(info[0]) * adapter_count);
	if (ret) {
		pr_err("%s failed to copy adapter info", __func__);
		ret = -EINVAL;
	}

cleanup:

	if (ret >= 0) {
		pr_debug("found %d adapters", adapter_count);
		goto success;
	}
	if (info) {
		for (i = 0; i < adapter_count; i++)
			dxgprocess_close_adapter(process,
						 info[i].adapter_handle);
	}
success:
	if (info)
		vfree(info);
	if (adapters)
		vfree(adapters);

	pr_debug("ioctl:%s %s %d", errorstr(ret), __func__, ret);
	return ret;
}

static int
dxgk_enum_adapters(struct dxgprocess *process, void *__user inargs)
{
	struct d3dkmt_enumadapters2 args;
	int ret;
	struct dxgadapter *entry;
	struct d3dkmt_adapterinfo *info = NULL;
	struct dxgadapter **adapters = NULL;
	int adapter_count = 0;
	int i;

	pr_debug("ioctl: %s", __func__);

	ret = copy_from_user(&args, inargs, sizeof(args));
	if (ret) {
		pr_err("%s failed to copy input args", __func__);
		ret = -EINVAL;
		goto cleanup;
	}

	if (args.adapters == NULL) {
		pr_debug("buffer is NULL");
		args.num_adapters = dxgglobal->num_adapters;
		ret = copy_to_user(inargs, &args, sizeof(args));
		if (ret) {
			pr_err("%s failed to copy args to user", __func__);
			ret = -EINVAL;
		}
		goto cleanup;
	}
	if (args.num_adapters < dxgglobal->num_adapters) {
		args.num_adapters = dxgglobal->num_adapters;
		pr_debug("buffer is too small");
		ret = -EOVERFLOW;
		goto cleanup;
	}

	if (args.num_adapters > D3DKMT_ADAPTERS_MAX) {
		pr_debug("too many adapters");
		ret = -EINVAL;
		goto cleanup;
	}

	info = vzalloc(sizeof(struct d3dkmt_adapterinfo) * args.num_adapters);
	if (info == NULL) {
		ret = -ENOMEM;
		goto cleanup;
	}

	adapters = vzalloc(sizeof(struct dxgadapter *) * args.num_adapters);
	if (adapters == NULL) {
		ret = -ENOMEM;
		goto cleanup;
	}

	dxgglobal_acquire_adapter_list_lock(DXGLOCK_SHARED);
	dxgglobal_acquire_process_adapter_lock();

	list_for_each_entry(entry, &dxgglobal->adapter_list_head,
			    adapter_list_entry) {
		if (dxgadapter_acquire_lock_shared(entry) == 0) {
			struct d3dkmt_adapterinfo *inf = &info[adapter_count];

			ret = dxgprocess_open_adapter(process, entry,
						      &inf->adapter_handle);
			if (ret >= 0) {
				inf->adapter_luid = entry->luid;
				adapters[adapter_count] = entry;
				pr_debug("adapter: %x %llx",
					    inf->adapter_handle.v,
					    *(u64 *) &inf->adapter_luid);
				adapter_count++;
			}
			dxgadapter_release_lock_shared(entry);
		}
		if (ret < 0)
			break;
	}

	dxgglobal_release_process_adapter_lock();
	dxgglobal_release_adapter_list_lock(DXGLOCK_SHARED);

	args.num_adapters = adapter_count;

	ret = copy_to_user(inargs, &args, sizeof(args));
	if (ret) {
		pr_err("%s failed to copy args to user", __func__);
		ret = -EINVAL;
		goto cleanup;
	}
	ret = copy_to_user(args.adapters, info,
			   sizeof(info[0]) * args.num_adapters);
	if (ret) {
		pr_err("%s failed to copy adapter info to user", __func__);
		ret = -EINVAL;
	}

cleanup:

	if (ret < 0) {
		if (info) {
			for (i = 0; i < args.num_adapters; i++) {
				dxgprocess_close_adapter(process,
							info[i].adapter_handle);
			}
		}
	} else {
		pr_debug("found %d adapters", args.num_adapters);
	}

	if (info)
		vfree(info);
	if (adapters)
		vfree(adapters);

	pr_debug("ioctl:%s %s %d", errorstr(ret), __func__, ret);
	return ret;
}

static int
dxgk_enum_adapters3(struct dxgprocess *process, void *__user inargs)
{
	struct d3dkmt_enumadapters3 args;
	int ret;

	pr_debug("ioctl: %s", __func__);

	ret = copy_from_user(&args, inargs, sizeof(args));
	if (ret) {
		pr_err("%s failed to copy input args", __func__);
		ret = -EINVAL;
		goto cleanup;
	}

	ret = dxgkp_enum_adapters(process, args.filter,
				  args.adapter_count,
				  args.adapters,
				  &((struct d3dkmt_enumadapters3 *)inargs)->
				  adapter_count);

cleanup:

	pr_debug("ioctl:%s %s %d", errorstr(ret), __func__, ret);
	return ret;
}

static int
dxgk_close_adapter(struct dxgprocess *process, void *__user inargs)
{
	struct d3dkmthandle args;
	int ret;

	pr_debug("ioctl: %s", __func__);

	ret = copy_from_user(&args, inargs, sizeof(args));
	if (ret) {
		pr_err("%s failed to copy input args", __func__);
		ret = -EINVAL;
		goto cleanup;
	}

	ret = dxgprocess_close_adapter(process, args);
	if (ret < 0)
		pr_err("%s failed", __func__);

cleanup:

	pr_debug("ioctl:%s %s %d", errorstr(ret), __func__, ret);
	return ret;
}

static int
dxgk_query_adapter_info(struct dxgprocess *process, void *__user inargs)
{
	struct d3dkmt_queryadapterinfo args;
	int ret;
	struct dxgadapter *adapter = NULL;

	pr_debug("ioctl: %s", __func__);

	ret = copy_from_user(&args, inargs, sizeof(args));
	if (ret) {
		pr_err("%s failed to copy input args", __func__);
		ret = -EINVAL;
		goto cleanup;
	}

	if (args.private_data_size > DXG_MAX_VM_BUS_PACKET_SIZE ||
	    args.private_data_size == 0) {
		pr_err("invalid private data size");
		ret = -EINVAL;
		goto cleanup;
	}

	pr_debug("Type: %d Size: %x",
		args.type, args.private_data_size);

	adapter = dxgprocess_adapter_by_handle(process, args.adapter);
	if (adapter == NULL) {
		ret = -EINVAL;
		goto cleanup;
	}

	ret = dxgadapter_acquire_lock_shared(adapter);
	if (ret < 0)
		goto cleanup;

	ret = dxgvmb_send_query_adapter_info(process, adapter, &args);

	dxgadapter_release_lock_shared(adapter);

cleanup:

	if (adapter)
		kref_put(&adapter->adapter_kref, dxgadapter_release);

	pr_debug("ioctl:%s %s %d", errorstr(ret), __func__, ret);
	return ret;
}

static int
dxgk_create_device(struct dxgprocess *process, void *__user inargs)
{
	struct d3dkmt_createdevice args;
	int ret;
	struct dxgadapter *adapter = NULL;
	struct dxgdevice *device = NULL;
	struct d3dkmthandle host_device_handle = {};
	bool adapter_locked = false;

	pr_debug("ioctl: %s", __func__);

	ret = copy_from_user(&args, inargs, sizeof(args));
	if (ret) {
		pr_err("%s failed to copy input args", __func__);
		ret = -EINVAL;
		goto cleanup;
	}

	/* The call acquires reference on the adapter */
	adapter = dxgprocess_adapter_by_handle(process, args.adapter);
	if (adapter == NULL) {
		ret = -EINVAL;
		goto cleanup;
	}

	device = dxgdevice_create(adapter, process);
	if (device == NULL) {
		ret = -ENOMEM;
		goto cleanup;
	}

	ret = dxgadapter_acquire_lock_shared(adapter);
	if (ret < 0)
		goto cleanup;

	adapter_locked = true;

	host_device_handle = dxgvmb_send_create_device(adapter, process, &args);
	if (host_device_handle.v) {
		ret = copy_to_user(&((struct d3dkmt_createdevice *)inargs)->
				   device, &host_device_handle,
				   sizeof(struct d3dkmthandle));
		if (ret) {
			pr_err("%s failed to copy device handle", __func__);
			ret = -EINVAL;
			goto cleanup;
		}

		hmgrtable_lock(&process->handle_table, DXGLOCK_EXCL);
		ret = hmgrtable_assign_handle(&process->handle_table, device,
					      HMGRENTRY_TYPE_DXGDEVICE,
					      host_device_handle);
		if (ret >= 0) {
			device->handle = host_device_handle;
			device->handle_valid = 1;
			device->object_state = DXGOBJECTSTATE_ACTIVE;
		}
		hmgrtable_unlock(&process->handle_table, DXGLOCK_EXCL);
	}

cleanup:

	if (ret < 0) {
		if (host_device_handle.v)
			dxgvmb_send_destroy_device(adapter, process,
						   host_device_handle);
		if (device)
			dxgdevice_destroy(device);
	}

	if (adapter_locked)
		dxgadapter_release_lock_shared(adapter);

	if (adapter)
		kref_put(&adapter->adapter_kref, dxgadapter_release);

	pr_debug("ioctl:%s %s %d", errorstr(ret), __func__, ret);
	return ret;
}

static int
dxgk_destroy_device(struct dxgprocess *process, void *__user inargs)
{
	struct d3dkmt_destroydevice args;
	int ret;
	struct dxgadapter *adapter = NULL;
	struct dxgdevice *device = NULL;

	pr_debug("ioctl: %s", __func__);

	ret = copy_from_user(&args, inargs, sizeof(args));
	if (ret) {
		pr_err("%s failed to copy input args", __func__);
		ret = -EINVAL;
		goto cleanup;
	}

	hmgrtable_lock(&process->handle_table, DXGLOCK_EXCL);
	device = hmgrtable_get_object_by_type(&process->handle_table,
					      HMGRENTRY_TYPE_DXGDEVICE,
					      args.device);
	if (device) {
		hmgrtable_free_handle(&process->handle_table,
				      HMGRENTRY_TYPE_DXGDEVICE, args.device);
		device->handle_valid = 0;
	}
	hmgrtable_unlock(&process->handle_table, DXGLOCK_EXCL);

	if (device == NULL) {
		pr_err("invalid device handle: %x", args.device.v);
		ret = -EINVAL;
		goto cleanup;
	}

	adapter = device->adapter;

	dxgdevice_destroy(device);

	if (dxgadapter_acquire_lock_shared(adapter) == 0) {
		dxgvmb_send_destroy_device(adapter, process, args.device);
		dxgadapter_release_lock_shared(adapter);
	}

cleanup:

	pr_debug("ioctl:%s %s %d", errorstr(ret), __func__, ret);
	return ret;
}

/*
 * IOCTL processing
 * The driver IOCTLs return
 * - 0 in case of success
 * - positive values, which are Windows NTSTATUS (for example, STATUS_PENDING).
 *   Positive values are success codes.
 * - Linux negative error codes
 */
static int dxgk_ioctl(struct file *f, unsigned int p1, unsigned long p2)
{
	int code = _IOC_NR(p1);
	int status;
	struct dxgprocess *process;

	if (code < 1 || code > LX_IO_MAX) {
		pr_err("bad ioctl %x %x %x %x",
			   code, _IOC_TYPE(p1), _IOC_SIZE(p1), _IOC_DIR(p1));
		return -ENOTTY;
	}
	if (ioctls[code].ioctl_callback == NULL) {
		pr_err("ioctl callback is NULL %x", code);
		return -ENOTTY;
	}
	if (ioctls[code].ioctl != p1) {
		pr_err("ioctl mismatch. Code: %x User: %x Kernel: %x",
			   code, p1, ioctls[code].ioctl);
		return -ENOTTY;
	}
	process = (struct dxgprocess *)f->private_data;
	if (process->tgid != current->tgid) {
		pr_err("Call from a wrong process: %d %d",
			   process->tgid, current->tgid);
		return -ENOTTY;
	}
	status = ioctls[code].ioctl_callback(process, (void *__user)p2);
	return status;
}

long dxgk_compat_ioctl(struct file *f, unsigned int p1, unsigned long p2)
{
	pr_debug("  compat ioctl %x", p1);
	return dxgk_ioctl(f, p1, p2);
}

long dxgk_unlocked_ioctl(struct file *f, unsigned int p1, unsigned long p2)
{
	pr_debug("   unlocked ioctl %x Code:%d", p1, _IOC_NR(p1));
	return dxgk_ioctl(f, p1, p2);
}

#define SET_IOCTL(callback, v)				\
	ioctls[_IOC_NR(v)].ioctl_callback = callback;	\
	ioctls[_IOC_NR(v)].ioctl = v

void init_ioctls(void)
{
	SET_IOCTL(/*0x1 */ dxgk_open_adapter_from_luid,
		  LX_DXOPENADAPTERFROMLUID);
	SET_IOCTL(/*0x2 */ dxgk_create_device,
		  LX_DXCREATEDEVICE);
	SET_IOCTL(/*0x9 */ dxgk_query_adapter_info,
		  LX_DXQUERYADAPTERINFO);
	SET_IOCTL(/*0x14 */ dxgk_enum_adapters,
		  LX_DXENUMADAPTERS2);
	SET_IOCTL(/*0x15 */ dxgk_close_adapter,
		  LX_DXCLOSEADAPTER);
	SET_IOCTL(/*0x19 */ dxgk_destroy_device,
		  LX_DXDESTROYDEVICE);
	SET_IOCTL(/*0x3e */ dxgk_enum_adapters3,
		  LX_DXENUMADAPTERS3);
}
