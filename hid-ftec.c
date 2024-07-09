#include <linux/device.h>
#include <linux/usb.h>
#include <linux/hid.h>
#include <linux/hidraw.h>
#include <linux/module.h>
#include <linux/input.h>

#include "hid-ftec.h"
#include "hid-ftec-pid.h"

// adjustabel initial value for break load cell
int init_load = 4;
module_param(init_load, int, 0);
// expose PID HID descriptor via hidraw
bool hidraw_pid = true;
module_param(hidraw_pid, bool, 1);


static u8 ftec_get_load(struct hid_device *hid)
{
    struct list_head *report_list = &hid->report_enum[HID_INPUT_REPORT].report_list;
    struct hid_report *report = list_entry(report_list->next, struct hid_report, list);	
    struct ftec_drv_data *drv_data;
	unsigned long flags;
	s32 *value;

	dbg_hid(" ... get_load; %i\n", hid_report_len(report));

	drv_data = hid_get_drvdata(hid);
	if (!drv_data) {
		hid_err(hid, "Private driver data not found!\n");
		return -EINVAL;
	}

	value = drv_data->report->field[0]->value;

	spin_lock_irqsave(&drv_data->report_lock, flags);
	value[0] = 0xf8;
	value[1] = 0x09;
	value[2] = 0x01;
	value[3] = 0x06;
	value[4] = 0x00;
	value[5] = 0x00;
	value[6] = 0x00;
	
	hid_hw_request(hid, drv_data->report, HID_REQ_SET_REPORT);
	spin_unlock_irqrestore(&drv_data->report_lock, flags);

    hid_hw_request(hid, report, HID_REQ_GET_REPORT);
    hid_hw_wait(hid);

    return report->field[0]->value[10];
}

static void ftec_set_load(struct hid_device *hid, u8 val)
{
	struct ftec_drv_data *drv_data;
	unsigned long flags;
	s32 *value;

	dbg_hid(" ... set_load %02X\n", val);

	drv_data = hid_get_drvdata(hid);
	if (!drv_data) {
		hid_err(hid, "Private driver data not found!\n");
		return;
	}

	value = drv_data->report->field[0]->value;

	spin_lock_irqsave(&drv_data->report_lock, flags);
	value[0] = 0xf8;
	value[1] = 0x09;
	value[2] = 0x01;
	value[3] = 0x16;
	value[4] = val+1; // actual value has an offset of 1
	value[5] = 0x00;
	value[6] = 0x00;
	
	hid_hw_request(hid, drv_data->report, HID_REQ_SET_REPORT);
	spin_unlock_irqrestore(&drv_data->report_lock, flags);
}

static void ftec_set_rumble(struct hid_device *hid, u32 val)
{
	struct ftec_drv_data *drv_data;
	unsigned long flags;
	s32 *value;
	int i;

	dbg_hid(" ... set_rumble %02X\n", val);

	drv_data = hid_get_drvdata(hid);
	if (!drv_data) {
		hid_err(hid, "Private driver data not found!\n");
		return;
	}

	value = drv_data->report->field[0]->value;

	spin_lock_irqsave(&drv_data->report_lock, flags);
	value[0] = 0xf8;
	value[1] = 0x09;
	value[2] = 0x01;
	value[3] = drv_data->quirks & FTEC_PEDALS ? 0x04 : 0x03;
	value[4] = (val>>16)&0xff;
	value[5] = (val>>8)&0xff;
	value[6] = (val)&0xff;

	// TODO: see ftecff.c::fix_values
	if (!(drv_data->quirks & FTEC_PEDALS)) {
		for(i=0;i<7;i++) {
			if (value[i]>=0x80)
				value[i] = -0x100 + value[i];
		}
	}

	hid_hw_request(hid, drv_data->report, HID_REQ_SET_REPORT);
	spin_unlock_irqrestore(&drv_data->report_lock, flags);
}

static ssize_t ftec_load_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct hid_device *hid = to_hid_device(dev);
	struct ftec_drv_data *drv_data;
	size_t count;

	drv_data = hid_get_drvdata(hid);
	if (!drv_data) {
		hid_err(hid, "Private driver data not found!\n");
		return 0;
	}

	count = scnprintf(buf, PAGE_SIZE, "%u\n", ftec_get_load(hid));
	return count;
}

static ssize_t ftec_load_store(struct device *dev, struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct hid_device *hid = to_hid_device(dev);
	u8 load;
	if (kstrtou8(buf, 0, &load) == 0) {
		ftec_set_load(hid, load);
	}
	return count;
}
static DEVICE_ATTR(load, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, ftec_load_show, ftec_load_store);

static ssize_t ftec_rumble_store(struct device *dev, struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct hid_device *hid = to_hid_device(dev);
	u32 rumble;
	if (kstrtou32(buf, 0, &rumble) == 0) {
		ftec_set_rumble(hid, rumble);
	}
	return count;
}
static DEVICE_ATTR(rumble, S_IWUSR | S_IWGRP, NULL, ftec_rumble_store);

static int ftec_init(struct hid_device *hdev) {
	struct list_head *report_list = &hdev->report_enum[HID_OUTPUT_REPORT].report_list;
	struct hid_report *report = list_entry(report_list->next, struct hid_report, list);	
	struct ftec_drv_data *drv_data;

	dbg_hid(" ... %i %i %i %i\n%i %i %i %i\n\n", 
		report->id, report->type, // report->application,
		report->maxfield, report->size,
		report->field[0]->logical_minimum,report->field[0]->logical_maximum,
		report->field[0]->physical_minimum,report->field[0]->physical_maximum
		);

	/* Check that the report looks ok */
	if (!hid_validate_values(hdev, HID_OUTPUT_REPORT, 0, 0, 7))
		return -1;

	drv_data = hid_get_drvdata(hdev);
	if (!drv_data) {
		hid_err(hdev, "Cannot add device, private driver data not allocated\n");
		return -1;
	}

	drv_data->report = report;
	drv_data->hid = hdev;
	spin_lock_init(&drv_data->report_lock);
	return 0;
}

static int ftec_client_ll_parse(struct hid_device *hdev)
{
	struct ftec_drv_data *drv_data = hdev->driver_data;
	u8 *rdesc = drv_data->client.rdesc, *ref_pos, *ref_end, *pos, *end;
	unsigned rsize, depth = 0;
	u8 size, report_id = 255;

	for (ref_pos = drv_data->hid->dev_rdesc,
			ref_end = drv_data->hid->dev_rdesc + drv_data->hid->dev_rsize,
			pos = rdesc, end = rdesc + sizeof(drv_data->client.rdesc);
			ref_pos != ref_end && pos != end;
			ref_pos += size + 1, pos += size + 1) {

		if (*ref_pos == 0xC0 && --depth == 0 && report_id < 2) {
			// inject the pid ffb collection
			if (pos + sizeof(rdesc_pid_ffb) > end) {
				hid_err(hdev, "Need %lu bytes to inject ffb\n", sizeof(rdesc_pid_ffb) );
				return 0;
			}
			memcpy(pos, rdesc_pid_ffb, sizeof(rdesc_pid_ffb));
			pos += sizeof(rdesc_pid_ffb);
		}

		size = (*ref_pos & 0x03);
		if (size == 3) size = 4;
		if (ref_pos + size > ref_end || pos + size > end)
		{
		    hid_err(hdev, "Need %d bytes to read item value\n", size );
		    return 0;
		}

		memcpy(pos, ref_pos, size + 1);

		if (*ref_pos == 0x85)
			report_id = *(ref_pos + 1);
		if (*ref_pos == 0xA1)
			++depth;
	}

	rsize = pos - rdesc;
	return hid_parse_report(hdev, rdesc, rsize);
}

static int ftec_client_ll_start(struct hid_device *hdev)
{
	return 0;
}

static void ftec_client_ll_stop(struct hid_device *hdev)
{
}

static int ftec_client_ll_open(struct hid_device *hdev)
{
	struct ftec_drv_data *drv_data = hdev->driver_data;
	drv_data->client.opened++;
	return 0;
}

static void ftec_client_ll_close(struct hid_device *hdev)
{
	struct ftec_drv_data *drv_data = hdev->driver_data;
	drv_data->client.opened--;
}

static int ftec_client_ll_raw_request(struct hid_device *hdev,
				unsigned char reportnum, u8 *buf,
				size_t count, unsigned char report_type,
				int reqtype)
{
	struct ftec_drv_data *drv_data = hdev->driver_data;
	struct hid_input *hidinput = list_entry(drv_data->hid->inputs.next, struct hid_input, list);
	struct input_dev *inputdev = hidinput->input;
	struct ff_device *ff = hidinput->input->ff;
	struct ff_effect *effect;
	// effects as listed in usage 20
	const u8 effects[] = {
		FF_CONSTANT,
		FF_SINE,
		FF_SPRING,
		FF_DAMPER,
		FF_INERTIA,
		FF_FRICTION,
	};

	if (reportnum <= 2 || reportnum == 255) {
		// forward these reports directly to the device
		return hid_hw_output_report(drv_data->hid, buf, count);
	}

	if (0) {
		printk(KERN_CONT "ftec_client_ll_raw_request num %2u, count %2lu, rep type %#02x, req type %#02x, data ", reportnum, count, report_type, reqtype);
		// only print set reports
		if (reqtype == 0x9) {
			for (int i = 0; i < count; ++i)
				printk(KERN_CONT " %#02x", buf[i]);
		}
		printk(KERN_CONT "\n");
	}

#define PID_REPORT_SET_EFFECT 		17  // usage 0x21
#define PID_REPORT_SET_CONDITION 	19  // usage 0x5f
#define PID_REPORT_CREATE_NEW_EFFECT 	20  // usage 0xab
#define PID_REPORT_SET_CONSTANT_FORCE   21  // usage 0x73
#define PID_REPORT_BLOCK_LOAD 		22  // usage 0x89
#define PID_REPORT_EFFECT_OPERATION     26  // usage 0x77
#define PID_REPORT_BLOCK_FREE 		27  // usage 0x90
#define PID_REPORT_DEVICE_CONTROL 	28  // usage 0x96
#define PID_REPORT_SET_PERIODIC 	29  // usage 0x6e
#define check_idx if (idx >= ARRAY_SIZE(drv_data->client.effects)) { \
			hid_err(hdev, "Invalid index %lu\n", idx);   \
			return 0;                                    \
                  }
#define get_effect check_idx \
	effect = &drv_data->client.effects[idx];
#define check_count(num) if (count != num) { \
			hid_err(hdev, "Invalid count %lu\n", count);  \
			return 0;                                     \
                  }

	switch (reportnum) {
	case PID_REPORT_CREATE_NEW_EFFECT: {
		size_t type = buf[1];
		size_t idx = type-1;

		get_effect

		if (effect->id != 0) {
			// already allocated effect, stop the old in favor of the new one?
			(void)ff->playback(inputdev, effect->id, 0);
		}

		effect->id = type;
		if (effects[idx] == FF_SINE) {
			effect->type = FF_PERIODIC;
			effect->u.periodic.waveform = effects[idx];
		} else {
			effect->type = effects[idx];
		}

		// store current effect to continue with it in report 22
		drv_data->client.current_effect = effect;
		break;
	}
	case PID_REPORT_BLOCK_LOAD: {
		check_count(5)

		buf[0] = reportnum;
		effect = drv_data->client.current_effect;
		if (effect == NULL || effect->id == 0) {
			buf[1] = 0x00;
			buf[2] = 0x02;
		} else {
			buf[1] = effect->id;  // effect id
			buf[2] = 0x01;        // load status: 1 success, 2 full, 3 error
			drv_data->client.current_effect = NULL;
		}
		buf[3] = 0xff;
		buf[4] = 0xff;
		break;
	}
	case PID_REPORT_SET_CONSTANT_FORCE: {
		size_t idx = buf[1]-1;
		check_count(4)

		get_effect

		effect->u.constant.level = ((s16)buf[3]) << 8 | buf[2];
		(void)ff->upload(inputdev, effect, NULL);
		break;
	}
	case PID_REPORT_SET_CONDITION: {
		size_t idx = buf[1]-1;
		check_count(15)

		get_effect

		struct ff_condition_effect *condition = &effect->u.condition[buf[2]&0x1];
		condition->center = ((s16)buf[4]) << 8 | buf[3];
		condition->right_coeff = ((s16)buf[6]) << 8 | buf[5];
		condition->left_coeff = ((s16)buf[8]) << 8 | buf[7];
		condition->right_saturation = ((u16)buf[10]) << 8 | buf[9];
		condition->left_saturation = ((u16)buf[12]) << 8 | buf[11];
		condition->deadband = ((u16)buf[14]) << 8 | buf[13];
		(void)ff->upload(inputdev, effect, NULL);
		break;
	}
	case PID_REPORT_SET_PERIODIC: {
		size_t idx = buf[1]-1;
		check_count(11)
		get_effect

		effect->u.periodic.period = ((u16)buf[10]) << 8 | buf[9];
		// FIXME: what's this all about ??
		if (effect->u.periodic.period != 0) {
			effect->u.periodic.magnitude = ((s16)buf[4]) << 8 | buf[3];
			effect->u.periodic.offset = ((s16)buf[6]) << 8 | buf[5];
			effect->u.periodic.phase = ((u16)buf[8] << 8 | buf[7]) * 0x10000 / 18000;
		} else {
			effect->type =  FF_CONSTANT;
			effect->u.constant.level = ((s16)buf[4]) << 8 | buf[3];
		}
		(void)!ff->upload(inputdev, effect, NULL);
		break;
	}
	case PID_REPORT_SET_EFFECT: {
		size_t idx = buf[1]-1;
		check_count(16)
		get_effect

		u16 duration = buf[4] << 8 | buf[3];
		effect->replay.length = duration == 0xffff ? 0 : duration;
		effect->trigger.interval = buf[6] << 8 | buf[5];
		effect->trigger.button = buf[10];

		// FIXME: hardcoded for now, not yet figured out what games do here
		effect->direction = 0x4000;  // ((u16)buf[13] << 8 | buf[12]) * 0x10000 / 18000;
		(void)ff->upload(inputdev, effect, NULL);
		break;
	}
	case PID_REPORT_EFFECT_OPERATION: {
		size_t idx = buf[1]-1;
		check_count(4)
		get_effect

		// Effect Operation: Op Effect Start/Start Solo/Stop
		if (buf[2] == 0x2) {
			// stop all other effects
			struct ff_effect *other_effect;
			for (size_t i = 0; i < ARRAY_SIZE(drv_data->client.effects); ++i) {
				other_effect = &drv_data->client.effects[i];
				if (other_effect->id > 0 && other_effect != effect) {
					(void)ff->playback(inputdev, other_effect->id, 0);
				}
			}

		}
		if (effect->id == 0) {
			hid_err(hdev, "Invalid effect id %u\n", effect->id);
			return 0;
		}
		if (effect->type == 0) {
			hid_err(hdev, "Invalid type %u\n", effect->type);
			return 0;
		}
		(void)ff->playback(inputdev, effect->id, buf[3]);
		break;
	}
	case PID_REPORT_DEVICE_CONTROL: {
		if (buf[1] == 0x4) {
			// reset: stop and unload all effects
			for (size_t i = 0; i < ARRAY_SIZE(drv_data->client.effects); ++i) {
				effect = &drv_data->client.effects[i];
				if (effect->id > 0) {
					(void)ff->playback(inputdev, effect->id, 0);
					effect->id = 0;
				}
			}
		}
		break;
	}
	case PID_REPORT_BLOCK_FREE: {
		size_t idx = buf[1]-1;
		get_effect

		if (effect->id > 0) {
			// FIXME: also stop playing the effect??
			(void)ff->playback(inputdev, effect->id, 0);
			effect->id = 0;
		}
		break;
	}
	default:
		 printk("Not implemented report %u\n", reportnum);
	}
	return count;
}

static const struct hid_ll_driver ftec_client_ll_driver = {
	.parse = ftec_client_ll_parse,
	.start = ftec_client_ll_start,
	.stop = ftec_client_ll_stop,
	.open = ftec_client_ll_open,
	.close = ftec_client_ll_close,
	.raw_request = ftec_client_ll_raw_request,
};

static struct hid_device *ftec_create_client_hid(struct hid_device *hdev)
{
	struct hid_device *client_hdev;

	client_hdev = hid_allocate_device();
	if (IS_ERR(client_hdev))
		return client_hdev;

	client_hdev->ll_driver = &ftec_client_ll_driver;
	client_hdev->dev.parent = hdev->dev.parent;
	client_hdev->bus = hdev->bus;
	client_hdev->vendor = hdev->vendor;
	client_hdev->product = hdev->product;
	client_hdev->version = hdev->version;
	client_hdev->type = hdev->type;
	client_hdev->country = hdev->country;
	strscpy(client_hdev->name, hdev->name,
			sizeof(client_hdev->name));
	strscpy(client_hdev->phys, hdev->phys,
			sizeof(client_hdev->phys));
	/*
	 * Since we use the same device info than the real interface to
	 * trick userspace, we will be calling ftec_probe recursively.
	 * We need to recognize the client interface somehow.
	 */
	client_hdev->group = HID_GROUP_STEAM;
	return client_hdev;
}

static int ftec_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	struct usb_interface *iface = to_usb_interface(hdev->dev.parent);
	__u8 iface_num = iface->cur_altsetting->desc.bInterfaceNumber;
	unsigned int connect_mask = HID_CONNECT_HIDINPUT | HID_CONNECT_HIDDEV;
	struct ftec_drv_data *drv_data;
	int ret;

	dbg_hid("%s: ifnum %d\n", __func__, iface_num);

	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "%s:parse of hid interface failed\n", __func__);
		return ret;
	}

	/*
	 * The virtual client_dev is only used for hidraw.
	 * Also avoid the recursive probe.
	 * Note: this technique is 'stolen' from hid-steam
	 */
	if (hdev->group == HID_GROUP_STEAM) {
		return hid_hw_start(hdev, HID_CONNECT_HIDRAW);
	}

	drv_data = kzalloc(sizeof(struct ftec_drv_data), GFP_KERNEL);
	if (!drv_data) {
		hid_err(hdev, "Insufficient memory, cannot allocate driver data\n");
		return -ENOMEM;
	}
	memset(&drv_data->client, 0, sizeof(drv_data->client));
	drv_data->quirks = id->driver_data;
	drv_data->min_range = 90;
	drv_data->max_range = 1090; // technically max_range is 1080, but 1090 is used as 'auto'
	if (hdev->product == CLUBSPORT_V2_WHEELBASE_DEVICE_ID || 
	    hdev->product == CLUBSPORT_V25_WHEELBASE_DEVICE_ID ||
	    hdev->product == CSR_ELITE_WHEELBASE_DEVICE_ID) {
		drv_data->max_range = 900;
	} else if (hdev->product == PODIUM_WHEELBASE_DD1_DEVICE_ID ||
		   hdev->product == PODIUM_WHEELBASE_DD2_DEVICE_ID ||
		   hdev->product == CSL_DD_WHEELBASE_DEVICE_ID) {
		drv_data->max_range = 2530;  // technically max_range is 2520, but 2530 is used as 'auto'
	}

	hid_set_drvdata(hdev, (void *)drv_data);

	if (drv_data->quirks & FTEC_PEDALS || !hidraw_pid) {
		connect_mask |= HID_CONNECT_HIDRAW;
	}

	if (drv_data->quirks & FTEC_FF) {
		connect_mask |= HID_CONNECT_FF;
	}

	ret = hid_hw_start(hdev, connect_mask);
	if (ret) {
		hid_err(hdev, "hw start failed\n");
		goto err_free;
	}

	ret = ftec_init(hdev);
	if (ret) {
		hid_err(hdev, "hw init failed\n");
		goto err_stop;
	}

	if (drv_data->quirks & FTEC_TUNING_MENU) {
		/* Open the device to receive reports with tuning menu data */
		ret = hid_hw_open(hdev);
		if (ret < 0) {
			hid_err(hdev, "hw open failed\n");
			goto err_client;
		}
	}

	if (drv_data->quirks & FTEC_FF && hidraw_pid) {
		drv_data->client.hdev = ftec_create_client_hid(hdev);
		if (IS_ERR(drv_data->client.hdev)) {
			ret = PTR_ERR(drv_data->client.hdev);
			goto err_stop;
		}
		drv_data->client.hdev->driver_data = drv_data;

		ret = hid_add_device(drv_data->client.hdev);
		if (ret)
			goto err_client;
	}

	if (drv_data->quirks & FTEC_FF) {
		ret = ftecff_init(hdev);
		if (ret) {
			hid_err(hdev, "ff init failed\n");
			goto err_client;
		}
	}

	if (hdev->product == CSL_ELITE_WHEELBASE_DEVICE_ID ||
	    hdev->product == CSL_ELITE_PS4_WHEELBASE_DEVICE_ID ||
	    hdev->product == CLUBSPORT_PEDALS_V3_DEVICE_ID) {
		ret = device_create_file(&hdev->dev, &dev_attr_rumble);
		if (ret)
			hid_warn(hdev, "Unable to create sysfs interface for \"rumble\", errno %d\n", ret);
	}
	
	if (drv_data->quirks & FTEC_PEDALS) {
		struct hid_input *hidinput = list_entry(hdev->inputs.next, struct hid_input, list);
		struct input_dev *inputdev = hidinput->input;

		// if these bits are not set, the pedals are not recognized in newer proton/wine verisons
		set_bit(EV_KEY, inputdev->evbit);
		set_bit(BTN_WHEEL, inputdev->keybit);

		if (init_load >= 0 && init_load < 10) {
			ftec_set_load(hdev, init_load);
		}

		ret = device_create_file(&hdev->dev, &dev_attr_load);
		if (ret)
			hid_warn(hdev, "Unable to create sysfs interface for \"load\", errno %d\n", ret);
	}

	return 0;

err_client:
	if (drv_data->client.hdev)
		hid_destroy_device(drv_data->client.hdev);
err_stop:
	hid_hw_stop(hdev);
err_free:
	kfree(drv_data);
	return ret;
}

static void ftec_remove(struct hid_device *hdev)
{
	struct ftec_drv_data *drv_data = hid_get_drvdata(hdev);

	if (!drv_data || hdev->group == HID_GROUP_STEAM) {
		hid_hw_stop(hdev);
		return;
	}

	if (drv_data->client.hdev) {
		hid_destroy_device(drv_data->client.hdev);
		drv_data->client.hdev = NULL;
	}
    
	if (drv_data->quirks & FTEC_PEDALS) {
		device_remove_file(&hdev->dev, &dev_attr_load);
	}

	if (hdev->product == CSL_ELITE_WHEELBASE_DEVICE_ID ||
	    hdev->product == CSL_ELITE_PS4_WHEELBASE_DEVICE_ID ||
	    hdev->product == CLUBSPORT_PEDALS_V3_DEVICE_ID) {
		device_remove_file(&hdev->dev, &dev_attr_rumble);
	}
	
	if (drv_data->quirks & FTEC_FF) {
		ftecff_remove(hdev);
	}

	hid_hw_close(hdev);
	hid_hw_stop(hdev);
	kfree(drv_data);
}

static int ftec_raw_event(struct hid_device *hdev, struct hid_report *report, u8 *data, int size) {
	struct ftec_drv_data *drv_data = hid_get_drvdata(hdev);
	if (drv_data->client.opened) {
		// printk("ftec_raw_event %#02x %i\n", report->id, size);
		hidraw_report_event(drv_data->client.hdev, data, size);
	}
	if (drv_data->quirks & FTEC_FF) {
		ftecff_raw_event(hdev, report, data, size);
	}
	return 0;
}

static const struct hid_device_id devices[] = {
	{ HID_USB_DEVICE(FANATEC_VENDOR_ID, CLUBSPORT_V2_WHEELBASE_DEVICE_ID), .driver_data = FTEC_FF },
	{ HID_USB_DEVICE(FANATEC_VENDOR_ID, CLUBSPORT_V25_WHEELBASE_DEVICE_ID), .driver_data = FTEC_FF },
	{ HID_USB_DEVICE(FANATEC_VENDOR_ID, CLUBSPORT_PEDALS_V3_DEVICE_ID), .driver_data = FTEC_PEDALS },
	{ HID_USB_DEVICE(FANATEC_VENDOR_ID, CSL_ELITE_WHEELBASE_DEVICE_ID), .driver_data = FTEC_FF | FTEC_TUNING_MENU | FTEC_WHEELBASE_LEDS},
	{ HID_USB_DEVICE(FANATEC_VENDOR_ID, CSL_ELITE_PS4_WHEELBASE_DEVICE_ID), .driver_data = FTEC_FF | FTEC_TUNING_MENU | FTEC_WHEELBASE_LEDS},
	{ HID_USB_DEVICE(FANATEC_VENDOR_ID, CSL_ELITE_PEDALS_DEVICE_ID), .driver_data = FTEC_PEDALS },
	{ HID_USB_DEVICE(FANATEC_VENDOR_ID, PODIUM_WHEELBASE_DD1_DEVICE_ID), .driver_data = FTEC_FF | FTEC_TUNING_MENU | FTEC_HIGHRES },
	{ HID_USB_DEVICE(FANATEC_VENDOR_ID, PODIUM_WHEELBASE_DD2_DEVICE_ID), .driver_data = FTEC_FF | FTEC_TUNING_MENU | FTEC_HIGHRES },
	{ HID_USB_DEVICE(FANATEC_VENDOR_ID, CSL_DD_WHEELBASE_DEVICE_ID), .driver_data = FTEC_FF | FTEC_TUNING_MENU | FTEC_HIGHRES },
	{ HID_USB_DEVICE(FANATEC_VENDOR_ID, CSR_ELITE_WHEELBASE_DEVICE_ID), .driver_data = FTEC_FF },
    { }
};

MODULE_DEVICE_TABLE(hid, devices);

static struct hid_driver fanatec_driver = {
	.name = "fanatec",
	.id_table = devices,
        .probe = ftec_probe,
        .remove = ftec_remove,
	.raw_event = ftec_raw_event,
};
module_hid_driver(fanatec_driver)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("gotzl");
MODULE_DESCRIPTION("A driver for the Fanatec CSL Elite");
