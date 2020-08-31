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

#define LEDS 9
#define MIN_RANGE 90
#define MAX_RANGE 1080
int hid_debug = 0;

struct ftec_drv_data {
	spinlock_t report_lock; /* Protect output HID report */
	struct hid_report *report;
	u16 range;
#ifdef CONFIG_LEDS_CLASS
	u16 led_state;
	struct led_classdev *led[LEDS];
#endif
};

/* This is realy weird... if i put a value >0x80 into the report,
   the actual value send to the device will be 0x7f. I suspect it has
   s.t. todo with the report fields min/max range, which is -127 to 128
   but I don't know how to handle this properly... So, here a hack around 
   this issue
*/
static void fix_values(s32 *values) {
	int i;
	for(i=0;i<7;i++) {
		if (values[i]>0x80) 
			values[i] = -0x100 + values[i];
	}
}

static u8 num[11][8] = {  { 1,1,1,1,1,1,0,0 },  // 0
						{ 0,1,1,0,0,0,0,0 },  // 1
						{ 1,1,0,1,1,0,1,0 },  // 2
						{ 1,1,1,1,0,0,1,0 },  // 3
						{ 0,1,1,0,0,1,1,0 },  // 4
						{ 1,0,1,1,0,1,1,0 },  // 5
						{ 1,0,1,1,1,1,1,0 },  // 6
						{ 1,1,1,0,0,0,0,0 },  // 7
						{ 1,1,1,1,1,1,1,0 },  // 8
						{ 1,1,1,0,0,1,1,0 },  // 9
						{ 0,0,0,0,0,0,0,1}};  // dot

static u8 seg_bits(u8 value) {
	int i;
	u8 bits = 0;

	for( i=0; i<8; i++) {
		if (num[value][i]) 
			bits |= 1 << i;
	}
	return bits;
}

static void ftec_set_range(struct hid_device *hid, u16 range)
{
	struct ftec_drv_data *drv_data;
	unsigned long flags;
	s32 *value;

	drv_data = hid_get_drvdata(hid);
	if (!drv_data) {
		hid_err(hid, "Private driver data not found!\n");
		return;
	}
	value = drv_data->report->field[0]->value;
	dbg_hid("setting range to %u\n", range);

	/* Prepare "coarse" limit command */
	spin_lock_irqsave(&drv_data->report_lock, flags);
	value[0] = 0xf5;
	value[1] = 0x00;
	value[2] = 0x00;
	value[3] = 0x00;
	value[4] = 0x00;
	value[5] = 0x00;
	value[6] = 0x00;
	fix_values(value);
	hid_hw_request(hid, drv_data->report, HID_REQ_SET_REPORT);

	value[0] = 0xf8;
	value[1] = 0x09;
	value[2] = 0x01;
	value[3] = 0x06;
	value[4] = 0x01;
	value[5] = 0x00;
	value[6] = 0x00;
	fix_values(value);
	hid_hw_request(hid, drv_data->report, HID_REQ_SET_REPORT);

	value[0] = 0xf8;
	value[1] = 0x81;
	value[2] = range&0xff;
	value[3] = (range>>8)&0xff;
	value[4] = 0x00;
	value[5] = 0x00;
	value[6] = 0x00;
	fix_values(value);
	hid_hw_request(hid, drv_data->report, HID_REQ_SET_REPORT);
	spin_unlock_irqrestore(&drv_data->report_lock, flags);
}

/* Export the currently set range of the wheel */
static ssize_t ftec_range_show(struct device *dev, struct device_attribute *attr,
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

	count = scnprintf(buf, PAGE_SIZE, "%u\n", drv_data->range);
	return count;
}

/* Set range to user specified value, call appropriate function
 * according to the type of the wheel */
static ssize_t ftec_range_store(struct device *dev, struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct hid_device *hid = to_hid_device(dev);
	struct ftec_drv_data *drv_data;
	u16 range = simple_strtoul(buf, NULL, 10);

	drv_data = hid_get_drvdata(hid);
	if (!drv_data) {
		hid_err(hid, "Private driver data not found!\n");
		return -EINVAL;
	}

	if (range == 0)
		range = MAX_RANGE;

	/* Check if the wheel supports range setting
	 * and that the range is within limits for the wheel */
	if (range >= MIN_RANGE && range <= MAX_RANGE) {
		ftec_set_range(hid, range);
		drv_data->range = range;
	}

	return count;
}
static DEVICE_ATTR(range, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, ftec_range_show, ftec_range_store);


static ssize_t ftec_set_display(struct device *dev, struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct hid_device *hid = to_hid_device(dev);
	struct ftec_drv_data *drv_data;
	unsigned long flags;
	s32 *value;
	s16 val = simple_strtol(buf, NULL, 10);

	drv_data = hid_get_drvdata(hid);
	if (!drv_data) {
		hid_err(hid, "Private driver data not found!\n");
		return -EINVAL;
	}

	dbg_hid(" ... set_display %i\n", val);
	
	value = drv_data->report->field[0]->value;

	spin_lock_irqsave(&drv_data->report_lock, flags);
	value[0] = 0xf8;
	value[1] = 0x09;
	value[2] = 0x01;
	value[3] = 0x02;
	value[4] = 0x00;
	value[5] = 0x00;
	value[6] = 0x00;

	if (val>=0) {
		value[4] = seg_bits((val/100)%100);
		value[5] = seg_bits((val/10)%10);
		value[6] = seg_bits(val%10);
	}

	fix_values(value);
	hid_hw_request(hid, drv_data->report, HID_REQ_SET_REPORT);
	spin_unlock_irqrestore(&drv_data->report_lock, flags);

	return count;
}
static DEVICE_ATTR(display, S_IWUSR | S_IWGRP, NULL, ftec_set_display);


#ifdef CONFIG_LEDS_CLASS
static void ftec_set_leds(struct hid_device *hid, u16 leds)
{
	struct ftec_drv_data *drv_data;
	unsigned long flags;
	s32 *value;
	u16 _leds = 0;
	int i;

	dbg_hid(" ... set_leds %04X\n", leds);

	drv_data = hid_get_drvdata(hid);
	if (!drv_data) {
		hid_err(hid, "Private driver data not found!\n");
		return;
	}

	// reshuffle, since first led is highest bit
	for( i=0; i<LEDS; i++) {
		if (leds>>i & 1) _leds |= 1 << (LEDS-i-1);
	}

	dbg_hid(" ... set_leds actual %04X\n", _leds);

	value = drv_data->report->field[0]->value;

	spin_lock_irqsave(&drv_data->report_lock, flags);
	value[0] = 0xf8;
	value[1] = 0x09;
	value[2] = 0x08;
	value[3] = (_leds>>8)&0xff;
	value[4] = _leds&0xff;
	value[5] = 0x00;
	value[6] = 0x00;
	
	fix_values(value);
	hid_hw_request(hid, drv_data->report, HID_REQ_SET_REPORT);
	spin_unlock_irqrestore(&drv_data->report_lock, flags);
}

static void ftec_led_set_brightness(struct led_classdev *led_cdev,
			enum led_brightness value)
{
	struct device *dev = led_cdev->dev->parent;
	struct hid_device *hid = to_hid_device(dev);
	struct ftec_drv_data *drv_data = hid_get_drvdata(hid);
	int i, state = 0;

	if (!drv_data) {
		hid_err(hid, "Device data not found.");
		return;
	}

	for (i = 0; i < LEDS; i++) {
		if (led_cdev != drv_data->led[i])
			continue;
		state = (drv_data->led_state >> i) & 1;
		if (value == LED_OFF && state) {
			drv_data->led_state &= ~(1 << i);
			ftec_set_leds(hid, drv_data->led_state);
		} else if (value != LED_OFF && !state) {
			drv_data->led_state |= 1 << i;
			ftec_set_leds(hid, drv_data->led_state);
		}
		break;
	}
}

static enum led_brightness ftec_led_get_brightness(struct led_classdev *led_cdev)
{
	struct device *dev = led_cdev->dev->parent;
	struct hid_device *hid = to_hid_device(dev);
	struct ftec_drv_data *drv_data = hid_get_drvdata(hid);
	int i, value = 0;

	if (!drv_data) {
		hid_err(hid, "Device data not found.");
		return LED_OFF;
	}

	for (i = 0; i < LEDS; i++)
		if (led_cdev == drv_data->led[i]) {
			value = (drv_data->led_state >> i) & 1;
			break;
		}

	return value ? LED_FULL : LED_OFF;
}
#endif


static int ff_play_effect_memless(struct input_dev *dev, void *data, struct ff_effect *effect)
{
	struct hid_device *hid = input_get_drvdata(dev);
	struct ftec_drv_data *drv_data;	
	unsigned long flags;
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

	spin_lock_irqsave(&drv_data->report_lock, flags);
	switch (effect->type) {
		case FF_CONSTANT:
			x = effect->u.ramp.start_level  + 0x80; /* 0x80 is no force */
        	// printk(KERN_WARNING "Wheel constant: %04X\n", x);
			CLAMP(x);

			value[0] = 0x01;
			value[1] = 0x08;
			value[2] = x;
			value[3] = 0x00;
			value[4] = 0x00;
			value[5] = 0x00;
			value[6] = 0x00;

			if (x==0x80) {
				value[0] = 0x03;
				value[2] = 0x00;
			}
			fix_values(value);
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
	spin_unlock_irqrestore(&drv_data->report_lock, flags);
	return 0;	
}

int ftec_init_led(struct hid_device *hid) {
	struct led_classdev *led;
	size_t name_sz;
	char *name;
	struct ftec_drv_data *drv_data;
	int ret, j;

	drv_data = hid_get_drvdata(hid);
	if (!drv_data) {
		hid_err(hid, "Cannot add device, private driver data not allocated\n");
		return -1;
	}

	{ 
		// wheel LED initialization sequence
		// not sure what's needed 
		s32 *value;
		value = drv_data->report->field[0]->value;

		value[0] = 0xf8;
		value[1] = 0x13;
		value[2] = 0x00;
		value[3] = 0x00;
		value[4] = 0x00;
		value[5] = 0x00;
		value[6] = 0x00;
		fix_values(value);
		hid_hw_request(hid, drv_data->report, HID_REQ_SET_REPORT);

		value[1] = 0x14;
		value[2] = 0xff;
		fix_values(value);
		hid_hw_request(hid, drv_data->report, HID_REQ_SET_REPORT);

		value[1] = 0x09;
		value[2] = 0x02;		
		value[3] = 0x01;
		fix_values(value);
		hid_hw_request(hid, drv_data->report, HID_REQ_SET_REPORT);

		value[1] = 0x09;
		value[2] = 0x08;		
		value[3] = 0x01;
		fix_values(value);
		hid_hw_request(hid, drv_data->report, HID_REQ_SET_REPORT);
	}

	drv_data->led_state = 0;
	for (j = 0; j < LEDS; j++)
		drv_data->led[j] = NULL;

	name_sz = strlen(dev_name(&hid->dev)) + 8;

	for (j = 0; j < LEDS; j++) {
		led = kzalloc(sizeof(struct led_classdev)+name_sz, GFP_KERNEL);
		if (!led) {
			hid_err(hid, "can't allocate memory for LED %d\n", j);
			goto err_leds;
		}

		name = (void *)(&led[1]);
		snprintf(name, name_sz, "%s::RPM%d", dev_name(&hid->dev), j+1);
		led->name = name;
		led->brightness = 0;
		led->max_brightness = 1;
		led->brightness_get = ftec_led_get_brightness;
		led->brightness_set = ftec_led_set_brightness;

		drv_data->led[j] = led;
		ret = led_classdev_register(&hid->dev, led);

		if (ret) {
			hid_err(hid, "failed to register LED %d. Aborting.\n", j);
err_leds:
			/* Deregister LEDs (if any) */
			for (j = 0; j < LEDS; j++) {
				led = drv_data->led[j];
				drv_data->led[j] = NULL;
				if (!led)
					continue;
				led_classdev_unregister(led);
				kfree(led);
			}
			return -1;
		}
	}
	return 0;
}

int ftec_init(struct hid_device *hdev) {
    struct hid_input *hidinput = list_entry(hdev->inputs.next, struct hid_input, list);
    struct input_dev *inputdev = hidinput->input;
	struct list_head *report_list = &hdev->report_enum[HID_OUTPUT_REPORT].report_list;
	struct hid_report *report = list_entry(report_list->next, struct hid_report, list);	
	struct ftec_drv_data *drv_data;

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
	spin_lock_init(&drv_data->report_lock);

    dbg_hid(" ... setting FF bits");
	set_bit(FF_CONSTANT, inputdev->ffbit);

	return input_ff_create_memless(inputdev, NULL, ff_play_effect_memless);
}

static int ftec_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	struct usb_interface *iface = to_usb_interface(hdev->dev.parent);
	__u8 iface_num = iface->cur_altsetting->desc.bInterfaceNumber;
	struct ftec_drv_data *drv_data;
	unsigned int connect_mask = HID_CONNECT_DEFAULT;
    int ret;

	dbg_hid("%s: ifnum %d\n", __func__, iface_num);

	drv_data = kzalloc(sizeof(struct ftec_drv_data), GFP_KERNEL);
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

	ret = ftec_init(hdev);
	if (ret) {
		hid_err(hdev, "hw init failed\n");
		goto err_stop;
	}

	/* Create sysfs interface */
	ret = device_create_file(&hdev->dev, &dev_attr_display);
	if (ret)
		hid_warn(hdev, "Unable to create sysfs interface for \"display\", errno %d\n", ret);  /* Let the driver continue without display */
	
	ret = device_create_file(&hdev->dev, &dev_attr_range);
	if (ret)
		hid_warn(hdev, "Unable to create sysfs interface for \"range\", errno %d\n", ret);  /* Let the driver continue without display */

#ifdef CONFIG_LEDS_CLASS
	if (ftec_init_led(hdev))
		hid_err(hdev, "LED init failed\n"); /* Let the driver continue without LEDs */
#endif

	return 0;

err_stop:
	hid_hw_stop(hdev);
err_free:
	kfree(drv_data);
	return ret;
}

static void ftec_remove(struct hid_device *hdev)
{
	struct ftec_drv_data *drv_data = hid_get_drvdata(hdev);

	device_remove_file(&hdev->dev, &dev_attr_display);
	device_remove_file(&hdev->dev, &dev_attr_range);
#ifdef CONFIG_LEDS_CLASS
	{
		int j;
		struct led_classdev *led;

		/* Deregister LEDs (if any) */
		for (j = 0; j < LEDS; j++) {

			led = drv_data->led[j];
			drv_data->led[j] = NULL;
			if (!led)
				continue;
			led_classdev_unregister(led);
			kfree(led);
		}
	}
#endif

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

static struct hid_driver ftec_csl_elite = {
		.name = "ftec_csl_elite",
		.id_table = devices,
        .probe = ftec_probe,
        .remove = ftec_remove,        
};
module_hid_driver(ftec_csl_elite)