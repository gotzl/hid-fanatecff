#include <linux/module.h>
#include <linux/hid.h>

#include "hid-ftec.h" 

struct ftec_tuning_attr_t {
	const char* name;
	const enum ftec_tuning_attrs_enum id;
	const u8 addr;
	const char* description;
	int (*conv_to)(struct ftec_drv_data *, u8);
	u8 (*conv_from)(struct ftec_drv_data *, int);
	const int min;
	const int max;
};

static int ftec_conv_sens_to(struct ftec_drv_data *drv_data, u8 val) {
	if (drv_data->max_range <= 1090) {
		return val * 10;
	}
	if (val < (u8)0x8a) {
		return 1080 + 10 * (0x100 + val - 0xed);
	} else if (val >= (u8)0xed) {
		return 1080 + 10 * (val - 0xed);
	}
	return 90 + 10 * (val - 0x8a);
};

static u8 ftec_conv_sens_from(struct ftec_drv_data *drv_data, int val) {
	if (drv_data->max_range <= 1090) {
		return val / 10;
	}
	if (val >= 1080) {
		// overflow of u8 is expected behavior
		return 0xed + ((val - 1080) / 10);
	}
	return 0x8a + (val - 90) / 10;
};

static int ftec_conv_times_ten(struct ftec_drv_data *drv_data, u8 val) {
	return val * 10;
};

static u8 ftec_conv_div_ten(struct ftec_drv_data *drv_data, int val) {
	return val / 10;
};

static u8 ftec_conv_steps_ten(struct ftec_drv_data *drv_data, int val) {
	return 10 * (val / 10);
}

static int ftec_conv_signed_to(struct ftec_drv_data *drv_data, u8 val) {
	return (s8)val;
};

static int ftec_conv_noop_to(struct ftec_drv_data *drv_data, u8 val) {
	return val;
};

static u8 ftec_conv_noop_from(struct ftec_drv_data *drv_data, int val) {
	return val;
};


static const struct ftec_tuning_attr_t ftec_tuning_attrs[] = {
#define FTEC_TUNING_ATTR(id, addr, desc, conv_to, conv_from, min, max) \
	{ #id, id, addr, desc, conv_to, conv_from, min, max },
FTEC_TUNING_ATTRS
#undef FTEC_TUNING_ATTR
};


static int ftec_tuning_write(struct hid_device *hid, int addr, int val) {
	struct ftec_drv_data *drv_data = hid_get_drvdata(hid);
	drv_data->tuning.ftec_tuning_data[0] = 0xff;
	drv_data->tuning.ftec_tuning_data[1] = 0x03;
	drv_data->tuning.ftec_tuning_data[2] = 0x00;
	drv_data->tuning.ftec_tuning_data[addr+1] = val;
	return hid_hw_output_report(hid, &drv_data->tuning.ftec_tuning_data[0], FTEC_TUNING_REPORT_SIZE);
}

static int ftec_tuning_select(struct hid_device *hid, int slot) {
	u8 *buf = kcalloc(FTEC_TUNING_REPORT_SIZE, sizeof(u8), GFP_KERNEL);
	int ret;
    
	buf[0] = 0xff;
	buf[1] = 0x03;
	buf[2] = 0x01;
	buf[3] = slot&0xff;

	ret = hid_hw_output_report(hid, buf, FTEC_TUNING_REPORT_SIZE);
	kfree(buf);
	return ret;
}

static enum ftec_tuning_attrs_enum ftec_tuning_get_id(struct device_attribute *attr) {
	int idx = 0;
	for (; idx < sizeof(ftec_tuning_attrs); idx++) {
		if (strcmp(attr->attr.name, ftec_tuning_attrs[idx].name) == 0) {
			return ftec_tuning_attrs[idx].id;
		}
	}
	dbg_hid("Unknown attribute %s\n", attr->attr.name);
	return FTEC_TUNING_ATTR_NONE;
}

ssize_t _ftec_tuning_show(struct device *dev, enum ftec_tuning_attrs_enum id, char *buf)
{
	struct hid_device *hid = to_hid_device(dev->parent);
	struct ftec_drv_data *drv_data = hid_get_drvdata(hid);
	const struct ftec_tuning_attr_t *tuning_attr = &ftec_tuning_attrs[id];
	return scnprintf(buf, PAGE_SIZE, "%i\n", tuning_attr->conv_to(drv_data, drv_data->tuning.ftec_tuning_data[tuning_attr->addr+1]));
}

static ssize_t ftec_tuning_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	enum ftec_tuning_attrs_enum id = ftec_tuning_get_id(attr);
	if (id == FTEC_TUNING_ATTR_NONE) {
		return -EINVAL;
	}

	return _ftec_tuning_show(dev, id, buf);
}

ssize_t _ftec_tuning_store(struct device *dev, enum ftec_tuning_attrs_enum id,
				  const char *buf, size_t count)
{
	struct hid_device *hid = to_hid_device(dev->parent);
	struct ftec_drv_data *drv_data = hid_get_drvdata(hid);
	const struct ftec_tuning_attr_t *tuning_attr = &ftec_tuning_attrs[id];
	int val, _max = tuning_attr->max;

	if (kstrtos32(buf, 0, &val) != 0) {
		hid_err(hid, "Invalid value %s!\n", buf);
		return -EINVAL;
	}

	/* special case for SEN, max value is device specific */
	if (id == SEN) {
		_max = drv_data->max_range;
		/* set max value if 0 is given */
		if (val == 0) {
			val = _max;
		}
	}

	/* check if value is in range */
	if (val < tuning_attr->min || val > _max) {
		hid_err(hid, "Value %i out of range [%i, %i]!\n", val, tuning_attr->min, _max);
		return -EINVAL;
	}
	
	/* convert value to device specific value */
	val = tuning_attr->conv_from(drv_data, val);
	dbg_hid(" ... ftec_tuning_store %s %i\n", tuning_attr->name, val);

	if (id == SLOT) {
		if (ftec_tuning_select(hid, val) < 0) {
			return -EIO;
		}
	} else {
		if (ftec_tuning_write(hid, tuning_attr->addr, val) < 0) {
			return -EIO;
		}
	}
	return count;
}


static ssize_t ftec_tuning_store(struct device *dev, struct device_attribute *attr,
				 const char *buf, size_t count)
{
	enum ftec_tuning_attrs_enum id = ftec_tuning_get_id(attr);
	if (id == FTEC_TUNING_ATTR_NONE) {
		return -EINVAL;
	}
	return _ftec_tuning_store(dev, id, buf, count);
}

static ssize_t ftec_tuning_reset(struct device *dev, struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct hid_device *hid = to_hid_device(dev->parent);
	u8 *buffer = kcalloc(FTEC_TUNING_REPORT_SIZE, sizeof(u8), GFP_KERNEL);
	int ret;
    	
	// request current values
	buffer[0] = 0xff;
	buffer[1] = 0x03;
	buffer[2] = 0x04;

	ret = hid_hw_output_report(hid, buffer, FTEC_TUNING_REPORT_SIZE);
	
	return count;
}

static DEVICE_ATTR(RESET, S_IWUSR | S_IWGRP, NULL, ftec_tuning_reset);
#define FTEC_TUNING_ATTR(id, addr, desc, conv_to, conv_from, min, max) \
	static DEVICE_ATTR(id, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, ftec_tuning_show, ftec_tuning_store);
FTEC_TUNING_ATTRS
#undef FTEC_TUNING_ATTR


static const struct class ftec_tuning_class = {
	.name = "ftec_tuning",
};

int ftec_tuning_classdev_register(struct device *parent,
		struct ftec_tuning_classdev *ftec_tuning_cdev)
{
	struct hid_device *hdev = to_hid_device(parent);
	int ret;

	ftec_tuning_cdev->dev = device_create(&ftec_tuning_class, parent, 0, NULL, "tuning_menu");

#define CREATE_SYSFS_FILE(name) \
	ret = device_create_file(ftec_tuning_cdev->dev, &dev_attr_##name); \
	if (ret) \
		hid_warn(hdev, "Unable to create sysfs interface for '%s', errno %d\n", #name, ret); \

	CREATE_SYSFS_FILE(RESET)
	CREATE_SYSFS_FILE(SLOT)
	CREATE_SYSFS_FILE(SEN)
	CREATE_SYSFS_FILE(FF)
	CREATE_SYSFS_FILE(FEI)
	CREATE_SYSFS_FILE(FOR)
	CREATE_SYSFS_FILE(SPR)
	CREATE_SYSFS_FILE(DPR)

	if (hdev->product == CSL_ELITE_WHEELBASE_DEVICE_ID || 
	    hdev->product == CSL_ELITE_PS4_WHEELBASE_DEVICE_ID) {
		CREATE_SYSFS_FILE(DRI)
	}
	if (hdev->product == CSL_ELITE_WHEELBASE_DEVICE_ID || 
	    hdev->product == CSL_ELITE_PS4_WHEELBASE_DEVICE_ID ||
	    hdev->product == PODIUM_WHEELBASE_DD1_DEVICE_ID ||
	    hdev->product == PODIUM_WHEELBASE_DD2_DEVICE_ID) {
		CREATE_SYSFS_FILE(BLI)
		CREATE_SYSFS_FILE(SHO)
	}
	if (hdev->product == CSL_DD_WHEELBASE_DEVICE_ID ||
	    hdev->product == PODIUM_WHEELBASE_DD1_DEVICE_ID ||
	    hdev->product == PODIUM_WHEELBASE_DD2_DEVICE_ID) {
		CREATE_SYSFS_FILE(NDP)
		CREATE_SYSFS_FILE(NFR)
		CREATE_SYSFS_FILE(NIN)
		CREATE_SYSFS_FILE(INT)
	}	
	if (hdev->product == CSL_DD_WHEELBASE_DEVICE_ID) {
		CREATE_SYSFS_FILE(FUL)
	}

	return 0;
}

void ftec_tuning_classdev_unregister(struct ftec_tuning_classdev *ftec_tuning_cdev)
{
	if (IS_ERR_OR_NULL(ftec_tuning_cdev->dev))
		return;

	struct hid_device *hdev = to_hid_device(ftec_tuning_cdev->dev->parent);
	
#define REMOVE_SYSFS_FILE(name) device_remove_file(&hdev->dev, &dev_attr_##name); \

	REMOVE_SYSFS_FILE(RESET)
	REMOVE_SYSFS_FILE(SLOT)
	REMOVE_SYSFS_FILE(SEN)
	REMOVE_SYSFS_FILE(FF)
	REMOVE_SYSFS_FILE(FEI)
	REMOVE_SYSFS_FILE(FOR)
	REMOVE_SYSFS_FILE(SPR)
	REMOVE_SYSFS_FILE(DPR)

	if (hdev->product == CSL_ELITE_WHEELBASE_DEVICE_ID || 
	    hdev->product == CSL_ELITE_PS4_WHEELBASE_DEVICE_ID) {
		REMOVE_SYSFS_FILE(DRI)
	}
	if (hdev->product == CSL_ELITE_WHEELBASE_DEVICE_ID || 
	    hdev->product == CSL_ELITE_PS4_WHEELBASE_DEVICE_ID ||
	    hdev->product == PODIUM_WHEELBASE_DD1_DEVICE_ID ||
	    hdev->product == PODIUM_WHEELBASE_DD2_DEVICE_ID) {
		REMOVE_SYSFS_FILE(BLI)
		REMOVE_SYSFS_FILE(SHO)
	}
	if (hdev->product == CSL_DD_WHEELBASE_DEVICE_ID ||
	    hdev->product == PODIUM_WHEELBASE_DD1_DEVICE_ID ||
	    hdev->product == PODIUM_WHEELBASE_DD2_DEVICE_ID) {
		REMOVE_SYSFS_FILE(NDP)
		REMOVE_SYSFS_FILE(NFR)
		REMOVE_SYSFS_FILE(NIN)
		REMOVE_SYSFS_FILE(INT)
	}	
	if (hdev->product == CSL_DD_WHEELBASE_DEVICE_ID) {
		REMOVE_SYSFS_FILE(FUL)
	}

	device_unregister(ftec_tuning_cdev->dev);
}
/*
static int __init ftec_tuning_init(void)
{
	return class_register(&ftec_tuning_class);
}

static void __exit ftec_tuning_exit(void)
{
	class_unregister(&ftec_tuning_class);
}

subsys_initcall(ftec_tuning_init);
module_exit(ftec_tuning_exit);
*/
