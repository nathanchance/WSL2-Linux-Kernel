// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019, Microsoft Corporation.
 *
 * Author:
 *   Iouri Tarassov <iourit@linux.microsoft.com>
 *
 * Dxgkrnl Graphics Driver
 * Interface with Linux kernel, PCI driver and the VM bus driver
 *
 */

#include <linux/module.h>
#include <linux/eventfd.h>
#include <linux/hyperv.h>
#include <linux/pci.h>

#include "dxgkrnl.h"

/*
 * Pointer to the global device data. By design
 * there is a single vGPU device on the VM bus and a single /dev/dxg device
 * is created.
 */
struct dxgglobal *dxgglobal;

#define DXGKRNL_VERSION			0x2216
#define PCI_VENDOR_ID_MICROSOFT		0x1414
#define PCI_DEVICE_ID_VIRTUAL_RENDER	0x008E

#undef pr_fmt
#define pr_fmt(fmt)	"dxgk: " fmt

//
// Interface from dxgglobal
//

struct vmbus_channel *dxgglobal_get_vmbus(void)
{
	return dxgglobal->channel.channel;
}

struct dxgvmbuschannel *dxgglobal_get_dxgvmbuschannel(void)
{
	return &dxgglobal->channel;
}

int dxgglobal_acquire_channel_lock(void)
{
	down_read(&dxgglobal->channel_lock);
	if (dxgglobal->channel.channel == NULL) {
		pr_err("Failed to acquire global channel lock");
		return -ENODEV;
	} else {
		return 0;
	}
}

void dxgglobal_release_channel_lock(void)
{
	up_read(&dxgglobal->channel_lock);
}

/*
 * File operations for the /dev/dxg device
 */

static int dxgk_open(struct inode *n, struct file *f)
{
	return 0;
}

static int dxgk_release(struct inode *n, struct file *f)
{
	return 0;
}

static ssize_t dxgk_read(struct file *f, char __user *s, size_t len,
			 loff_t *o)
{
	pr_debug("file read\n");
	return 0;
}

static ssize_t dxgk_write(struct file *f, const char __user *s, size_t len,
			  loff_t *o)
{
	pr_debug("file write\n");
	return len;
}

const struct file_operations dxgk_fops = {
	.owner = THIS_MODULE,
	.open = dxgk_open,
	.release = dxgk_release,
	.write = dxgk_write,
	.read = dxgk_read,
};

/*
 * Interface with the PCI driver
 */

/*
 * Part of the PCI config space of the vGPU device is used for vGPU
 * configuration data. Reading/writing of the PCI config space is forwarded
 * to the host.
 */

/* vGPU VM bus channel instance ID */
#define DXGK_VMBUS_CHANNEL_ID_OFFSET 192
/* DXGK_VMBUS_INTERFACE_VERSION (u32) */
#define DXGK_VMBUS_VERSION_OFFSET	(DXGK_VMBUS_CHANNEL_ID_OFFSET + \
					sizeof(guid_t))
/* Luid of the virtual GPU on the host (struct winluid) */
#define DXGK_VMBUS_VGPU_LUID_OFFSET	(DXGK_VMBUS_VERSION_OFFSET + \
					sizeof(u32))
/* The guest writes its capavilities to this adderss */
#define DXGK_VMBUS_GUESTCAPS_OFFSET	(DXGK_VMBUS_VERSION_OFFSET + \
					sizeof(u32))

/* Capabilities of the guest driver, reported to the host */
struct dxgk_vmbus_guestcaps {
	union {
		struct {
			u32	wsl2		: 1;
			u32	reserved	: 31;
		};
		u32 guest_caps;
	};
};

/*
 * A helper function to read PCI config space.
 */
static int dxg_pci_read_dwords(struct pci_dev *dev, int offset, int size,
			       void *val)
{
	int off = offset;
	int ret;
	int i;

	for (i = 0; i < size / sizeof(int); i++) {
		ret = pci_read_config_dword(dev, off, &((int *)val)[i]);
		if (ret) {
			pr_err("Failed to read PCI config: %d", off);
			return ret;
		}
		off += sizeof(int);
	}
	return 0;
}

static int dxg_pci_probe_device(struct pci_dev *dev,
				const struct pci_device_id *id)
{
	int ret;
	guid_t guid;
	u32 vmbus_interface_ver = DXGK_VMBUS_INTERFACE_VERSION;
	struct winluid vgpu_luid = {};
	struct dxgk_vmbus_guestcaps guest_caps = {.wsl2 = 1};

	mutex_lock(&dxgglobal->device_mutex);

	if (dxgglobal->vmbus_ver == 0)  {
		/* Report capabilities to the host */

		ret = pci_write_config_dword(dev, DXGK_VMBUS_GUESTCAPS_OFFSET,
					guest_caps.guest_caps);
		if (ret)
			goto cleanup;

		/* Negotiate the VM bus version */

		ret = pci_read_config_dword(dev, DXGK_VMBUS_VERSION_OFFSET,
					&vmbus_interface_ver);
		if (ret == 0 && vmbus_interface_ver != 0)
			dxgglobal->vmbus_ver = vmbus_interface_ver;
		else
			dxgglobal->vmbus_ver = DXGK_VMBUS_INTERFACE_VERSION_OLD;

		if (dxgglobal->vmbus_ver < DXGK_VMBUS_INTERFACE_VERSION)
			goto read_channel_id;

		ret = pci_write_config_dword(dev, DXGK_VMBUS_VERSION_OFFSET,
					DXGK_VMBUS_INTERFACE_VERSION);
		if (ret)
			goto cleanup;

		if (dxgglobal->vmbus_ver > DXGK_VMBUS_INTERFACE_VERSION)
			dxgglobal->vmbus_ver = DXGK_VMBUS_INTERFACE_VERSION;
	}

read_channel_id:

	/* Get the VM bus channel ID for the virtual GPU */
	ret = dxg_pci_read_dwords(dev, DXGK_VMBUS_CHANNEL_ID_OFFSET,
				sizeof(guid), (int *)&guid);
	if (ret)
		goto cleanup;

	if (dxgglobal->vmbus_ver >= DXGK_VMBUS_INTERFACE_VERSION) {
		ret = dxg_pci_read_dwords(dev, DXGK_VMBUS_VGPU_LUID_OFFSET,
					  sizeof(vgpu_luid), &vgpu_luid);
		if (ret)
			goto cleanup;
	}

	pr_debug("Adapter channel: %pUb\n", &guid);
	pr_debug("Vmbus interface version: %d\n",
		dxgglobal->vmbus_ver);
	pr_debug("Host vGPU luid: %x-%x\n",
		vgpu_luid.b, vgpu_luid.a);

cleanup:

	mutex_unlock(&dxgglobal->device_mutex);

	if (ret)
		pr_debug("err: %s %d", __func__, ret);
	return ret;
}

static void dxg_pci_remove_device(struct pci_dev *dev)
{
	/* Placeholder */
}

static struct pci_device_id dxg_pci_id_table = {
	.vendor = PCI_VENDOR_ID_MICROSOFT,
	.device = PCI_DEVICE_ID_VIRTUAL_RENDER,
	.subvendor = PCI_ANY_ID,
	.subdevice = PCI_ANY_ID
};

static struct pci_driver dxg_pci_drv = {
	.name = KBUILD_MODNAME,
	.id_table = &dxg_pci_id_table,
	.probe = dxg_pci_probe_device,
	.remove = dxg_pci_remove_device
};

/*
 * Interface with the VM bus driver
 */

static int dxgglobal_getiospace(struct dxgglobal *dxgglobal)
{
	/* Get mmio space for the global channel */
	struct hv_device *hdev = dxgglobal->hdev;
	struct vmbus_channel *channel = hdev->channel;
	resource_size_t pot_start = 0;
	resource_size_t pot_end = -1;
	int ret;

	dxgglobal->mmiospace_size = channel->offermsg.offer.mmio_megabytes;
	if (dxgglobal->mmiospace_size == 0) {
		pr_debug("zero mmio space is offered\n");
		return -ENOMEM;
	}
	dxgglobal->mmiospace_size <<= 20;
	pr_debug("mmio offered: %llx\n",
		dxgglobal->mmiospace_size);

	ret = vmbus_allocate_mmio(&dxgglobal->mem, hdev, pot_start, pot_end,
				  dxgglobal->mmiospace_size, 0x10000, false);
	if (ret) {
		pr_err("Unable to allocate mmio memory: %d\n", ret);
		return ret;
	}
	dxgglobal->mmiospace_size = dxgglobal->mem->end -
	    dxgglobal->mem->start + 1;
	dxgglobal->mmiospace_base = dxgglobal->mem->start;
	pr_info("mmio allocated %llx  %llx %llx %llx\n",
		dxgglobal->mmiospace_base,
		dxgglobal->mmiospace_size,
		dxgglobal->mem->start, dxgglobal->mem->end);

	return 0;
}

int dxgglobal_init_global_channel(void)
{
	int ret = 0;

	ret = dxgvmbuschannel_init(&dxgglobal->channel, dxgglobal->hdev);
	if (ret) {
		pr_err("dxgvmbuschannel_init failed: %d\n", ret);
		goto error;
	}

	ret = dxgglobal_getiospace(dxgglobal);
	if (ret) {
		pr_err("getiospace failed: %d\n", ret);
		goto error;
	}

	ret = dxgvmb_send_set_iospace_region(dxgglobal->mmiospace_base,
					     dxgglobal->mmiospace_size, 0);
	if (ret < 0) {
		pr_err("send_set_iospace_region failed");
		goto error;
	}

	hv_set_drvdata(dxgglobal->hdev, dxgglobal);

	dxgglobal->dxgdevice.minor = MISC_DYNAMIC_MINOR;
	dxgglobal->dxgdevice.name = "dxg";
	dxgglobal->dxgdevice.fops = &dxgk_fops;
	dxgglobal->dxgdevice.mode = 0666;
	ret = misc_register(&dxgglobal->dxgdevice);
	if (ret) {
		pr_err("misc_register failed: %d", ret);
		goto error;
	}
	dxgglobal->dxg_dev_initialized = true;

error:
	return ret;
}

void dxgglobal_destroy_global_channel(void)
{
	down_write(&dxgglobal->channel_lock);

	dxgglobal->global_channel_initialized = false;

	if (dxgglobal->dxg_dev_initialized) {
		misc_deregister(&dxgglobal->dxgdevice);
		dxgglobal->dxg_dev_initialized = false;
	}

	if (dxgglobal->mem) {
		vmbus_free_mmio(dxgglobal->mmiospace_base,
				dxgglobal->mmiospace_size);
		dxgglobal->mem = NULL;
	}

	dxgvmbuschannel_destroy(&dxgglobal->channel);

	if (dxgglobal->hdev) {
		hv_set_drvdata(dxgglobal->hdev, NULL);
		dxgglobal->hdev = NULL;
	}

	up_write(&dxgglobal->channel_lock);
}

static const struct hv_vmbus_device_id id_table[] = {
	/* Per GPU Device GUID */
	{ HV_GPUP_DXGK_VGPU_GUID },
	/* Global Dxgkgnl channel for the virtual machine */
	{ HV_GPUP_DXGK_GLOBAL_GUID },
	{ }
};

static int dxg_probe_vmbus(struct hv_device *hdev,
			   const struct hv_vmbus_device_id *dev_id)
{
	int ret = 0;
	struct winluid luid;
	struct dxgvgpuchannel *vgpuch;

	mutex_lock(&dxgglobal->device_mutex);

	if (uuid_le_cmp(hdev->dev_type, id_table[0].guid) == 0) {
		/* This is a new virtual GPU channel */
		guid_to_luid(&hdev->channel->offermsg.offer.if_instance, &luid);
		pr_debug("vGPU channel: %pUb",
			    &hdev->channel->offermsg.offer.if_instance);
		vgpuch = vzalloc(sizeof(struct dxgvgpuchannel));
		if (vgpuch == NULL) {
			ret = -ENOMEM;
			goto error;
		}
		vgpuch->adapter_luid = luid;
		vgpuch->hdev = hdev;
		list_add_tail(&vgpuch->vgpu_ch_list_entry,
			      &dxgglobal->vgpu_ch_list_head);
	} else if (uuid_le_cmp(hdev->dev_type, id_table[1].guid) == 0) {
		/* This is the global Dxgkgnl channel */
		pr_debug("Global channel: %pUb",
			    &hdev->channel->offermsg.offer.if_instance);
		if (dxgglobal->hdev) {
			/* This device should appear only once */
			pr_err("global channel already present\n");
			ret = -EBADE;
			goto error;
		}
		dxgglobal->hdev = hdev;
	} else {
		/* Unknown device type */
		pr_err("probe: unknown device type\n");
		ret = -EBADE;
		goto error;
	}

error:

	mutex_unlock(&dxgglobal->device_mutex);

	if (ret)
		pr_debug("err: %s %d", __func__, ret);
	return ret;
}

static int dxg_remove_vmbus(struct hv_device *hdev)
{
	int ret = 0;
	struct dxgvgpuchannel *vgpu_channel;

	mutex_lock(&dxgglobal->device_mutex);

	if (uuid_le_cmp(hdev->dev_type, id_table[0].guid) == 0) {
		pr_debug("Remove virtual GPU channel\n");
		list_for_each_entry(vgpu_channel,
				    &dxgglobal->vgpu_ch_list_head,
				    vgpu_ch_list_entry) {
			if (vgpu_channel->hdev == hdev) {
				list_del(&vgpu_channel->vgpu_ch_list_entry);
				vfree(vgpu_channel);
				break;
			}
		}
	} else if (uuid_le_cmp(hdev->dev_type, id_table[1].guid) == 0) {
		pr_debug("Remove global channel device\n");
		dxgglobal_destroy_global_channel();
	} else {
		/* Unknown device type */
		pr_err("remove: unknown device type\n");
		ret = -EBADE;
	}

	mutex_unlock(&dxgglobal->device_mutex);
	if (ret)
		pr_debug("err: %s %d", __func__, ret);
	return ret;
}

MODULE_DEVICE_TABLE(vmbus, id_table);

static struct hv_driver dxg_drv = {
	.name = KBUILD_MODNAME,
	.id_table = id_table,
	.probe = dxg_probe_vmbus,
	.remove = dxg_remove_vmbus,
	.driver = {
		   .probe_type = PROBE_PREFER_ASYNCHRONOUS,
		    },
};

/*
 * Interface with Linux kernel
 */

static int dxgglobal_create(void)
{
	int ret = 0;

	dxgglobal = vzalloc(sizeof(struct dxgglobal));
	if (!dxgglobal)
		return -ENOMEM;

	INIT_LIST_HEAD(&dxgglobal->plisthead);
	mutex_init(&dxgglobal->plistmutex);
	mutex_init(&dxgglobal->device_mutex);

	INIT_LIST_HEAD(&dxgglobal->vgpu_ch_list_head);

	init_rwsem(&dxgglobal->channel_lock);

	pr_debug("dxgglobal_init end\n");
	return ret;
}

static void dxgglobal_destroy(void)
{
	if (dxgglobal) {
		if (dxgglobal->vmbus_registered)
			vmbus_driver_unregister(&dxg_drv);

		dxgglobal_destroy_global_channel();

		if (dxgglobal->pci_registered)
			pci_unregister_driver(&dxg_pci_drv);

		vfree(dxgglobal);
		dxgglobal = NULL;
	}
}

/*
 * Driver entry points
 */

static int __init dxg_drv_init(void)
{
	int ret;


	ret = dxgglobal_create();
	if (ret) {
		pr_err("dxgglobal_init failed");
		return -ENOMEM;
	}

	ret = vmbus_driver_register(&dxg_drv);
	if (ret) {
		pr_err("vmbus_driver_register failed: %d", ret);
		return ret;
	}
	dxgglobal->vmbus_registered = true;

	pr_info("%s  Version: %x", __func__, DXGKRNL_VERSION);

	ret = pci_register_driver(&dxg_pci_drv);
	if (ret) {
		pr_err("pci_driver_register failed: %d", ret);
		return ret;
	}
	dxgglobal->pci_registered = true;

	init_ioctls();

	return 0;
}

static void __exit dxg_drv_exit(void)
{
	dxgglobal_destroy();
}

module_init(dxg_drv_init);
module_exit(dxg_drv_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Microsoft Dxgkrnl virtual GPU Driver");
