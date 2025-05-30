#include <linux/device.h>
#include <linux/usb.h>
#include <linux/hid.h>
#include <linux/hidraw.h>
#include <linux/module.h>
#include <linux/input.h>

#include "hid-ftec.h"

// adjustabel initial value for break load cell
int init_load = 4;
module_param(init_load, int, 0);
// expose PID HID descriptor via hidraw
bool hidraw_pid = true;
module_param(hidraw_pid, bool, 0);


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


#define PID_REPORT_STATE                16  // usage 0x92 (input)
#define PID_REPORT_DEVICE_CONTROL 	16  // usage 0x96
#define PID_REPORT_SET_EFFECT 		17  // usage 0x21
#define PID_REPORT_SET_ENVELOPE         18  // usage 0x5a
#define PID_REPORT_SET_CONDITION 	19  // usage 0x5f
#define PID_REPORT_CREATE_NEW_EFFECT 	20  // usage 0xab
#define PID_REPORT_SET_CONSTANT_FORCE   21  // usage 0x73
#define PID_REPORT_BLOCK_LOAD 		22  // usage 0x89 (input)
#define PID_REPORT_PID_POOL             23  // usage 0x7f
#define PID_REPORT_SET_RAMP_FORCE       24  // usage 0x74
#define PID_REPORT_DEVICE_GAIN	        25  // usage 0x7d
#define PID_REPORT_EFFECT_OPERATION     26  // usage 0x77
#define PID_REPORT_BLOCK_FREE 		27  // usage 0x90
#define PID_REPORT_SET_PERIODIC 	29  // usage 0x6e

const u8 rdesc_pid_ffb[] = {
#include "hid-ftec-pid.h"
};

static int ftec_client_rdesc_fixup(struct ftec_drv_data_client *client, const u8 *dev_rdesc, size_t dev_rsize)
{
	const u8 *rdesc = client->rdesc, *ref_pos, *ref_end, *end;
	u8 *pos;
	unsigned depth = 0;
	u8 size, report_id = 255;

	for (ref_pos = dev_rdesc,
			ref_end = dev_rdesc + dev_rsize,
			pos = (u8*)rdesc, end = rdesc + sizeof(client->rdesc);
			ref_pos != ref_end && pos != end;
			ref_pos += size + 1, pos += size + 1) {

		if (*ref_pos == 0xC0 && --depth == 0 && report_id == 1) {
			// inject the pid ffb collection
			if (pos + sizeof(rdesc_pid_ffb) > end) {
				hid_err(client->hdev, "Need %lu bytes to inject ffb\n", sizeof(rdesc_pid_ffb) );
				return 1;
			}
			memcpy(pos, rdesc_pid_ffb, sizeof(rdesc_pid_ffb));
			pos += sizeof(rdesc_pid_ffb);
		}

		size = (*ref_pos & 0x03);
		if (size == 3) size = 4;
		if (ref_pos + size > ref_end || pos + size > end)
		{
		    hid_err(client->hdev, "Need %d bytes to read item value\n", size );
		    return 1;
		}

		memcpy(pos, ref_pos, size + 1);

		if (*ref_pos == 0x85)
			report_id = *(ref_pos + 1);
		if (*ref_pos == 0xA1)
			++depth;
	}
	client->rsize = pos - rdesc;
	return 0;
}

static int ftec_client_ll_parse(struct hid_device *hdev)
{
	struct ftec_drv_data *drv_data = hdev->driver_data;
	int ret;
	if ((ret = ftec_client_rdesc_fixup(
			&drv_data->client,
			drv_data->hid->dev_rdesc,
			drv_data->hid->dev_rsize)))
		return ret;
	return hid_parse_report(hdev, drv_data->client.rdesc, drv_data->client.rsize);
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

#ifdef DEVICE_MANAGED
struct __attribute__((packed)) create_new_effect {
	u8 report_id;
	u8 type_idx;
	u16 byte_count;
};

struct __attribute__((packed)) block_load {
	u8 report_id;
	u8 id;
	u8 status;
	u16 ram_pool_avail;
};

struct __attribute__((packed)) block_free {
	u8 report_id;
	u8 id;
};
#endif

struct __attribute__((packed)) set_constant {
	u8 report_id;
	u8 id;
	s16 level;
};

struct __attribute__((packed)) set_envelope {
	u8 report_id;
	u8 id;
	u16 attack_level;
	u16 fade_level;
	u16 attack_time;
	u16 fade_time;
};

struct __attribute__((packed)) set_condition {
	u8 report_id;
	u8 id;
	u8 block_offset;
	s16 offset;
	s16 positive_coeff;
	s16 negative_coeff;
	u16 positive_saturation;
	u16 negative_saturation;
	u16 dead_band;
};

struct __attribute__((packed)) set_periodic {
	u8 report_id;
	u8 id;
	u16 magnitude;
	s16 offset;
	u16 phase;
	u16 period;
};

struct __attribute__((packed)) set_effect {
	u8 report_id;
	u8 id;
	u8 type_idx;
	u16 duration;
	u16 trigger_repeat_interval;
	u16 sample_period;
	u16 start_delay;
	u8 gain;
	u8 button;
	u8 enable;
	u16 direction[2];
};

struct __attribute__((packed)) effect_operation {
	u8 report_id;
	u8 id;
	u8 op;
	u8 count;
};

struct __attribute__((packed)) device_control {
	u8 report_id;
	u8 ctrl;
};


static int check_idx(struct ftec_drv_data *drv_data, s16 idx, unsigned char reportnum) {
	if (idx < 0 || idx >= ARRAY_SIZE(drv_data->client.effects)) {
		hid_err(drv_data->hid, "Report %u: Invalid index %i\n", reportnum, idx);
		return 0;
        }
	return 1;
}

static struct ff_effect *get_effect(struct ftec_drv_data *drv_data, size_t id, unsigned char reportnum) {
	if (!check_idx(drv_data, id - 1, reportnum))
		return NULL;
	return &drv_data->client.effects[id - 1];
}

static void ftec_client_report_state(struct hid_device *hdev, u8 device_state, u8 effect_state, u8 effect_id) {
	u8 buf[3];
	buf[0] = PID_REPORT_STATE;
	buf[1] = device_state | effect_state;
	buf[2] = effect_id;
	hidraw_report_event(hdev, buf, sizeof(buf));
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
	// supported effects, have to be in order as listed in usage 21 !!
	const u8 ff_effects[] = {
		FF_CONSTANT,
		FF_RAMP,
		FF_CUSTOM,
		FF_SQUARE,
		FF_SINE,
		FF_TRIANGLE,
		FF_SAW_UP,
		FF_SAW_DOWN,
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

#define map_pid_type( __type) \
	__type *params;       \
	if (sizeof(__type) != count) { \
		hid_err(hdev, "Report %u: Invalid report size %lu, expected %lu", reportnum, count, sizeof(__type)); \
		return 0; \
	} \
	params = (__type*)buf;

#define	get_effect \
	struct ff_effect *effect; \
	if (!(effect = get_effect(drv_data, params->id, reportnum)))  \
	    return 0; \

	switch (reportnum) {
#ifdef DEVICE_MANAGED
	case PID_REPORT_CREATE_NEW_EFFECT: {
		map_pid_type(struct create_new_effect);
		size_t type_idx = params->type_idx - 1;
		if (drv_data->client.current_id == ARRAY_SIZE(drv_data->client.effects)) {
			hid_err(hdev, "can't find free slot for new effect");
			return 0;
		}
		effect = &drv_data->client.effects[drv_data->client.current_id];
		if (ff_effects[type_idx] == FF_SINE) {
			effect->type = FF_PERIODIC;
			effect->u.periodic.waveform = ff_effects[type_idx];
		} else {
			effect->type = ff_effects[type_idx];
		}
		break;
	}
	case PID_REPORT_BLOCK_LOAD: {
		map_pid_type(struct block_load);
		params->report_id = reportnum;
		if (drv_data->client.current_id == ARRAY_SIZE(drv_data->client.effects)) {
			params->id = 0x00;
			params->status = 0x02;
		} else {
			drv_data->client.current_id++; 
			params->id = drv_data->client.current_id;
			params->status = 0x01;  // load status: 1 success, 2 full, 3 error
		}
		params->ram_pool_avail = 0xffff;
		break;
	}
	case PID_REPORT_BLOCK_FREE: {
		map_pid_type(struct block_free);
		if (!(effect = get_effect(drv_data, params->id, reportnum)))
			return 0;
		(void)ff->playback(inputdev, effect->id, 0);
		memset(effect, 0, sizeof(struct ff_effect));
		break;
	}
#endif
	case PID_REPORT_SET_CONSTANT_FORCE: {
		map_pid_type(struct set_constant);
		get_effect	
		effect->u.constant.level = params->level;
		if (effect->id)
			(void)ff->upload(inputdev, effect, NULL);
		break;
	}
	case PID_REPORT_SET_ENVELOPE: {
		struct ff_envelope *envelope;
		map_pid_type(struct set_envelope);
		get_effect	
		switch (effect->type) {
			case FF_CONSTANT:
				envelope = &effect->u.constant.envelope;
				break;
			case FF_RAMP:
				envelope = &effect->u.ramp.envelope;
				break;
			case FF_PERIODIC:
				envelope = &effect->u.periodic.envelope;
				break;
			default:
				hid_err(hdev, "invalid effect type for envelope: %u", effect->type);
				return 0;
		}
		envelope->fade_level = params->fade_level;
		envelope->fade_length = params->fade_time;
		envelope->attack_level = params->attack_level;
		envelope->attack_length = params->attack_time;
		if (effect->id)
			(void)ff->upload(inputdev, effect, NULL);
		break;
	}
	case PID_REPORT_SET_CONDITION: {
		map_pid_type(struct set_condition);
		get_effect
		struct ff_condition_effect *condition = &effect->u.condition[params->block_offset&0x1];
		condition->center = params->offset;
		condition->right_coeff = params->positive_coeff;
		condition->left_coeff = params->negative_coeff;
		condition->right_saturation = params->positive_saturation;
		condition->left_saturation = params->negative_saturation;
		condition->deadband = params->dead_band;
		if (effect->id)
			(void)ff->upload(inputdev, effect, NULL);
		break;
	}
	case PID_REPORT_SET_PERIODIC: {
		map_pid_type(struct set_periodic);
		get_effect	
		effect->u.periodic.period = params->period;
		// FIXME: what's this all about ??
		if (effect->u.periodic.period != 0) {
			effect->u.periodic.magnitude = params->magnitude;
			effect->u.periodic.offset = params->offset;
			effect->u.periodic.phase = params->phase * 0x10000 / 36000;
		} else {
			effect->type = FF_CONSTANT;
			effect->u.constant.level = params->magnitude;
		}
		if (effect->id)
			(void)ff->upload(inputdev, effect, NULL);
		break;
	}
	case PID_REPORT_DEVICE_GAIN: {
		hid_dbg(hdev, "gain not implemented");
		break;
	}
	case PID_REPORT_SET_EFFECT: {
		map_pid_type(struct set_effect);
		get_effect	
		if (!effect->type) {
			size_t type_idx = params->type_idx - 1;
			if (ff_effects[type_idx] == FF_SINE) {
				effect->type = FF_PERIODIC;
				effect->u.periodic.waveform = ff_effects[type_idx];
			} else {
				effect->type = ff_effects[type_idx];
			}
		}
		effect->replay.length = params->duration == 0xffff ? 0 : params->duration;
		effect->replay.delay = params->start_delay;
		effect->trigger.interval = params->trigger_repeat_interval;
		// buf[9]: gain
		effect->trigger.button = params->button;

		// FIXME: default effect direction
		effect->direction = 0x4000;
		// Note: needs to be aligned to Usage 0x55/0x56
		if (params->enable & 0x1) {
			effect->direction = params->direction[0] * 0x10000 / 36000;
		}
		if (effect->id)
			(void)ff->upload(inputdev, effect, NULL);
		break;
	}
	case PID_REPORT_EFFECT_OPERATION: {
		map_pid_type(struct effect_operation);
		get_effect

		if (effect->type == 0) {
			hid_err(hdev, "Invalid type %u\n", effect->type);
			return 0;
		}

		// Effect Operation: Op Effect Start/Start Solo/Stop
		if (params->op == 0x3 || params->count == 0) {
			(void)ff->playback(inputdev, effect->id, 0);
			ftec_client_report_state(drv_data->client.hdev, 0x2, 0x0, effect->id);
			effect->id = 0;
			break;
		} else {
			if (params->op == 0x2) {
				// stop all other effects
				struct ff_effect *other_effect;
				for (size_t i = 0; i < ARRAY_SIZE(drv_data->client.effects); ++i) {
					other_effect = &drv_data->client.effects[i];
					if (other_effect->id > 0 && other_effect != effect) {
						(void)ff->playback(inputdev, other_effect->id, 0);
						ftec_client_report_state(drv_data->client.hdev, 0x2, 0x0, other_effect->id);
						other_effect->id = 0;
					}
				}

			}
			if (params->op == 0x1 || params->op == 0x2) {
				if (!effect->id) {
					effect->id = params->id;
					(void)ff->upload(inputdev, effect, NULL);
					ftec_client_report_state(drv_data->client.hdev, 0x2, 0x4, effect->id);
				}
			}
		}

		(void)ff->playback(inputdev, effect->id, params->count);
		break;
	}
	case PID_REPORT_DEVICE_CONTROL: {
		map_pid_type(struct device_control);
		if (params->ctrl == 0x4) {
			// reset: stop and unload all effects
			for (size_t i = 0; i < ARRAY_SIZE(drv_data->client.effects); ++i) {
				if (drv_data->client.effects[i].id) {
					(void)ff->playback(inputdev, drv_data->client.effects[i].id, 0);
					ftec_client_report_state(drv_data->client.hdev, 0x2, 0x0, drv_data->client.effects[i].id);
				}
				memset(&drv_data->client.effects[i], 0, sizeof(struct ff_effect));
			}
			drv_data->client.current_id = 0;
		}
		break;
	}
	default:
		 hid_err(hdev, "Not implemented report %u\n", reportnum);
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
	 * Note: this technique is 'stolen' from hid-steam
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

	if (drv_data->quirks & FTEC_TUNING_MENU || hidraw_pid) {
		/* Open the device to receive reports with tuning menu data and device state */
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
	// printk("ftec_raw_event %#02x %i; client opened: %i\n", report->id, size, drv_data->client.opened);
	if (drv_data->client.opened) {
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
	{ HID_USB_DEVICE(FANATEC_VENDOR_ID, CSL_LC_PEDALS_DEVICE_ID), .driver_data = FTEC_PEDALS },
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
