#include <linux/device.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/sched.h>
#include <linux/usb.h>
#include <linux/wait.h>

#include "usbhid/usbhid.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("gotzl");
MODULE_DESCRIPTION("A driver for the Fanatec CSL Elite Wheel Base");

int hid_debug = 1;
struct ftech_drv_data {
	struct hid_report *report;
};

static int ff_play_effect_memless(struct input_dev *dev, void *data, struct ff_effect *effect)
{
	struct hid_device *hid = input_get_drvdata(dev);
	struct ftech_drv_data *drv_data;	
	s32 *value;
    int x;

	// dbg_hid(" ... ff_play_effect_memless");

	if (!hid) {
		dbg_hid("hid_device not found!\n");
		return -EINVAL;
	}

	drv_data = hid_get_drvdata(hid);
	if (!drv_data) {
		hid_err(hid, "Private driver data not found!\n");
		return -EINVAL;
	}

	value = drv_data->report->field[0]->value;

#define CLAMP(x) do { if (x < 0) x = 0; else if (x > 0xff) x = 0xff; } while (0)

	switch (effect->type) {
		case FF_CONSTANT:
			x = effect->u.ramp.start_level  + 0x80; /* 0x80 is no force */
        	// printk(KERN_WARNING "Wheel constant: %04X\n", x);
			CLAMP(x);

			value[0] = 0x01;
			value[1] = 0x08;
			value[2] = x<0x80 ? x : -256 + x; // FIXME: why??
			value[3] = 0x00;
			value[4] = 0x00;
			value[5] = 0x00;
			value[6] = 0x00;

			if (x==0x80) {
				value[0] = 0x03;
				value[2] = 0x00;
			}
        	// printk(KERN_WARNING "Wheel constant: %04X\n", value[2]);

			hid_hw_request(hid, drv_data->report, HID_REQ_SET_REPORT);
			break;

    case FF_DAMPER:
    case FF_FRICTION:
    case FF_INERTIA:
        printk(KERN_WARNING "Wheel damper: %i %i, sat %i %i\n", effect->u.condition[0].right_coeff
                                                            , effect->u.condition[0].left_coeff
                                                            , effect->u.condition[0].right_saturation
                                                            , effect->u.condition[0].left_saturation);
        break;
    case FF_SPRING:
        printk(KERN_WARNING "Wheel spring coef: %i % i, sat: %i center: %i deadband: %i\n",
                           effect->u.condition[0].right_coeff,
                           effect->u.condition[0].left_coeff,
                           effect->u.condition[0].right_saturation,
                           effect->u.condition[0].center,
                           effect->u.condition[0].deadband);
        break;
    }
	return 0;	
}

int ftech_init(struct hid_device *hdev) {
    struct hid_input *hidinput = list_entry(hdev->inputs.next, struct hid_input, list);
    struct input_dev *inputdev = hidinput->input;
	struct list_head *report_list = &hdev->report_enum[HID_OUTPUT_REPORT].report_list;
	struct hid_report *report = list_entry(report_list->next, struct hid_report, list);	
	struct ftech_drv_data *drv_data;

	dbg_hid(" ... %i %i %i %i %i %i\n%i %i %i %i\n\n", 
		report->id, report->type, report->application,
		report->maxfield, report->size, report->maxfield,
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

    dbg_hid(" ... setting FF bits");
	set_bit(FF_CONSTANT, inputdev->ffbit);

	return input_ff_create_memless(inputdev, NULL, ff_play_effect_memless);
}

static int ftech_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	struct usb_interface *iface = to_usb_interface(hdev->dev.parent);
	__u8 iface_num = iface->cur_altsetting->desc.bInterfaceNumber;
	struct ftech_drv_data *drv_data;
	unsigned int connect_mask = HID_CONNECT_DEFAULT;
    int ret;

	dbg_hid("%s: ifnum %d\n", __func__, iface_num);

	drv_data = kzalloc(sizeof(struct ftech_drv_data), GFP_KERNEL);
	if (!drv_data) {
		hid_err(hdev, "Insufficient memory, cannot allocate driver data\n");
		return -ENOMEM;
	}	
	hid_set_drvdata(hdev, (void *)drv_data);

	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "parse failed\n");
		goto err_free;
	}

	connect_mask &= ~HID_CONNECT_FF;
	ret = hid_hw_start(hdev, connect_mask);
	if (ret) {
		hid_err(hdev, "hw start failed\n");
		goto err_free;
	}

	ret = ftech_init(hdev);
	if (ret) {
		hid_err(hdev, "hw init failed\n");
		goto err_stop;
	}	

	return 0;

err_stop:
	hid_hw_stop(hdev);
err_free:
	kfree(drv_data);
	return ret;
}

static void ftech_remove(struct hid_device *hdev)
{
	struct ftech_drv_data *drv_data = hid_get_drvdata(hdev);

	hid_hw_stop(hdev);
	kfree(drv_data);
}


#define USB_VENDOR_ID 0x0eb7
#define USB_DEVICE_ID 0x0005

static const struct hid_device_id devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID, USB_DEVICE_ID), .driver_data = 0 },
	{ }
};

MODULE_DEVICE_TABLE(hid, devices);

static struct hid_driver ftech_csl_elite = {
		.name = "ftech_csl_elite",
		.id_table = devices,
        .probe = ftech_probe,
        .remove = ftech_remove,        
};
module_hid_driver(ftech_csl_elite)