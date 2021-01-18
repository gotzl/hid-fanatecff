#include <linux/device.h>
#include <linux/hid.h>
#include <linux/usb.h>
#include <linux/input.h>
#include <linux/moduleparam.h>

#include "hid-ftec.h"

// parameter to set the initial range
// if unset, the wheels max range is used
int init_range = 0;
module_param(init_range, int, 0);


#define FTEC_TUNING_REPORT_SIZE 64

#define ADDR_SLOT 	0x02
#define ADDR_SEN 	0x03
#define ADDR_FF 	0x04
#define ADDR_SHO 	0x05
#define ADDR_BLI 	0x06											
#define ADDR_DRI 	0x09
#define ADDR_FOR 	0x0a
#define ADDR_SPR 	0x0b
#define ADDR_DPR 	0x0c
#define ADDR_FEI 	0x11

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
		range = drv_data->max_range;

	/* Check if the wheel supports range setting
	 * and that the range is within limits for the wheel */
	if (range >= drv_data->min_range && range <= drv_data->max_range) {
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

static int ftec_tuning_read(struct hid_device *hid, u8 *buf) {
	struct usb_device *dev = interface_to_usbdev(to_usb_interface(hid->dev.parent));
	int ret, actual_len;
    	
	// request current values
	buf[0] = 0xff;
	buf[1] = 0x03;
	buf[2] = 0x02;

	ret = hid_hw_output_report(hid, buf, FTEC_TUNING_REPORT_SIZE);
	if (ret < 0)
		goto out;

	// reset memory
	memset((void*)buf, 0, FTEC_TUNING_REPORT_SIZE); 

	// read values
	ret = usb_interrupt_msg(dev, usb_rcvintpipe(dev, 81),
				buf, FTEC_TUNING_REPORT_SIZE, &actual_len,
				USB_CTRL_SET_TIMEOUT);
out:
	return ret;
}

static int ftec_tuning_write(struct hid_device *hid, int addr, int val) {
	u8 *buf = kcalloc(FTEC_TUNING_REPORT_SIZE+1, sizeof(u8), GFP_KERNEL);
	int ret;
    	
	// shift by 1 so that values are at correct location for write back
	if (ftec_tuning_read(hid, buf+1) < 0)
		goto out;

	dbg_hid(" ... ftec_tuning_write %i; current: %i; new:%i\n", addr, buf[addr+1], val);

	// update requested value and write back
	buf[0] = 0xff;
	buf[1] = 0x03;
	buf[2] = 0x00;
	buf[addr+1] = val;
	ret = hid_hw_output_report(hid, buf, FTEC_TUNING_REPORT_SIZE);

out:
    kfree(buf);
	return 0;
}

static int ftec_tuning_select(struct hid_device *hid, int slot) {
	u8 *buf = kcalloc(FTEC_TUNING_REPORT_SIZE, sizeof(u8), GFP_KERNEL);
    int ret;
    	
	if (ftec_tuning_read(hid, buf) < 0)
		goto out;
		
	// return if already selected
	if (buf[ADDR_SLOT] == slot || slot<=0 || slot>NUM_TUNING_SLOTS) {
		dbg_hid(" ... ftec_tuning_select slot already selected or invalid value; current: %i; new:%i\n", buf[ADDR_SLOT], slot);
		goto out;
	}

	dbg_hid(" ... ftec_tuning_select current: %i; new:%i\n", buf[ADDR_SLOT], slot);

	// reset memory
	memset((void*)buf, 0, FTEC_TUNING_REPORT_SIZE); 

	buf[0] = 0xff;
	buf[1] = 0x03;
	buf[2] = 0x01;
	buf[3] = slot&0xff;

	ret = hid_hw_output_report(hid, buf, FTEC_TUNING_REPORT_SIZE);
	if (ret < 0)
		goto out;

out:
    kfree(buf);
	return 0;
}


static int ftec_tuning_get_addr(struct device_attribute *attr) {
	int type = 0;
	if(strcmp(attr->attr.name, "SLOT") == 0)
		type = ADDR_SLOT;
	else if(strcmp(attr->attr.name, "SEN") == 0)
		type = ADDR_SEN;
	else if(strcmp(attr->attr.name, "FF") == 0)
		type = ADDR_FF;
	else if(strcmp(attr->attr.name, "DRI") == 0)
		type = ADDR_DRI;
	else if(strcmp(attr->attr.name, "FEI") == 0)
		type = ADDR_FEI;
	else if(strcmp(attr->attr.name, "FOR") == 0)
		type = ADDR_FOR;
	else if(strcmp(attr->attr.name, "SPR") == 0)
		type = ADDR_SPR;
	else if(strcmp(attr->attr.name, "DPR") == 0)
		type = ADDR_DPR;
	else if(strcmp(attr->attr.name, "BLI") == 0)
		type = ADDR_BLI;												
	else if(strcmp(attr->attr.name, "SHO") == 0)
		type = ADDR_SHO;
	else {
		dbg_hid("Unknown attribute %s\n", attr->attr.name);
	}
	return type;
}

static ssize_t ftec_tuning_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct hid_device *hid = to_hid_device(dev);
	u8 *buffer = kcalloc(FTEC_TUNING_REPORT_SIZE, sizeof(u8), GFP_KERNEL);
	int addr = ftec_tuning_get_addr(attr);
	size_t count = 0;
	s8 value = 0;

	dbg_hid(" ... ftec_tuning_show %s, %x\n", attr->attr.name, addr);

	if (addr > 0 && ftec_tuning_read(hid, buffer) >= 0) {
		memcpy((void*)&value, &buffer[addr], sizeof(s8));
		count = scnprintf(buf, PAGE_SIZE, "%i\n", value);
	}
	kfree(buffer);
	return count;
}

static ssize_t ftec_tuning_store(struct device *dev, struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct hid_device *hid = to_hid_device(dev);
	int addr;

	s16 val = simple_strtol(buf, NULL, 10);
	dbg_hid(" ... ftec_tuning_store %s %i\n", attr->attr.name, val);

	addr = ftec_tuning_get_addr(attr);
	if (addr == ADDR_SLOT) {
		ftec_tuning_select(hid, val);
	} else if (addr > 0) {
		ftec_tuning_write(hid, addr, val);
	}
	return count;
}

static ssize_t ftec_tuning_reset(struct device *dev, struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct hid_device *hid = to_hid_device(dev);
	u8 *buffer = kcalloc(FTEC_TUNING_REPORT_SIZE, sizeof(u8), GFP_KERNEL);
	int ret;
    	
	// request current values
	buffer[0] = 0xff;
	buffer[1] = 0x03;
	buffer[2] = 0x04;

	ret = hid_hw_output_report(hid, buffer, FTEC_TUNING_REPORT_SIZE);
	
	return count;
}

static DEVICE_ATTR(RESET, S_IWUSR  | S_IWGRP, NULL, ftec_tuning_reset);
static DEVICE_ATTR(SLOT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, ftec_tuning_show, ftec_tuning_store);
static DEVICE_ATTR(SEN, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, ftec_tuning_show, ftec_tuning_store);
static DEVICE_ATTR(FF, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, ftec_tuning_show, ftec_tuning_store);
static DEVICE_ATTR(DRI, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, ftec_tuning_show, ftec_tuning_store);
static DEVICE_ATTR(FEI, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, ftec_tuning_show, ftec_tuning_store);
static DEVICE_ATTR(FOR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, ftec_tuning_show, ftec_tuning_store);
static DEVICE_ATTR(SPR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, ftec_tuning_show, ftec_tuning_store);
static DEVICE_ATTR(DPR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, ftec_tuning_show, ftec_tuning_store);
static DEVICE_ATTR(BLI, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, ftec_tuning_show, ftec_tuning_store);
static DEVICE_ATTR(SHO, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH, ftec_tuning_show, ftec_tuning_store);


#ifdef CONFIG_LEDS_CLASS
static void ftec_set_leds(struct hid_device *hid, u16 leds)
{
	struct ftec_drv_data *drv_data;
	unsigned long flags;
	s32 *value;
	u16 _leds = 0;
	int i;

	dbg_hid(" ... set_leds base %04X\n", leds);

	drv_data = hid_get_drvdata(hid);
	if (!drv_data) {
		hid_err(hid, "Private driver data not found!\n");
		return;
	}

	value = drv_data->report->field[0]->value;

	spin_lock_irqsave(&drv_data->report_lock, flags);
	value[0] = 0xf8;
	value[1] = 0x13;
	value[2] = leds&0xff;
	value[3] = 0x00;
	value[4] = 0x00;
	value[5] = 0x00;
	value[6] = 0x00;
	
	fix_values(value);
	hid_hw_request(hid, drv_data->report, HID_REQ_SET_REPORT);

	// reshuffle, since first led is highest bit
	for( i=0; i<LEDS; i++) {
		if (leds>>i & 1) _leds |= 1 << (LEDS-i-1);
	}

	dbg_hid(" ... set_leds wheel %04X\n", _leds);

	value = drv_data->report->field[0]->value;

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

static int ftec_init_led(struct hid_device *hid) {
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
		value[1] = 0x09;
		value[2] = 0x08;		
		value[3] = 0x01;
		value[4] = 0x00;
		value[5] = 0x00;
		value[6] = 0x00;

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

int ftecff_init(struct hid_device *hdev) {
    struct hid_input *hidinput = list_entry(hdev->inputs.next, struct hid_input, list);
    struct input_dev *inputdev = hidinput->input;
	struct ftec_drv_data *drv_data = hid_get_drvdata(hdev);
	int ret;

    dbg_hid(" ... setting FF bits");
	set_bit(FF_CONSTANT, inputdev->ffbit);

	ret = input_ff_create_memless(inputdev, NULL, ff_play_effect_memless);
	if (ret) {
		hid_err(hdev, "Unable to create ff memless: %i\n", ret);
		return ret;
	}

	/* Set range so that centering spring gets disabled */
	if (init_range > 0 && (init_range > drv_data->max_range || init_range < drv_data->min_range)) {
		hid_warn(hdev, "Invalid init_range %i; using max range of %i instead\n", init_range, drv_data->max_range);
		init_range = -1;
	}
	ftec_set_range(hdev, init_range > 0 ? init_range : drv_data->max_range);

	/* Create sysfs interface */
#define CREATE_SYSFS_FILE(name) \
	ret = device_create_file(&hdev->dev, &dev_attr_##name); \
	if (ret) \
		hid_warn(hdev, "Unable to create sysfs interface for '%s', errno %d\n", #name, ret); \
	
	CREATE_SYSFS_FILE(display)
	CREATE_SYSFS_FILE(range)

	if (hdev->product == CSL_ELITE_WHEELBASE_DEVICE_ID || hdev->product == CSL_ELITE_PS4_WHEELBASE_DEVICE_ID) {
		CREATE_SYSFS_FILE(RESET)
		CREATE_SYSFS_FILE(SLOT)
		CREATE_SYSFS_FILE(SEN)
		CREATE_SYSFS_FILE(FF)
		CREATE_SYSFS_FILE(DRI)
		CREATE_SYSFS_FILE(FEI)
		CREATE_SYSFS_FILE(FOR)
		CREATE_SYSFS_FILE(SPR)
		CREATE_SYSFS_FILE(DPR)
		CREATE_SYSFS_FILE(BLI)
		CREATE_SYSFS_FILE(SHO)
	}

#ifdef CONFIG_LEDS_CLASS
	if (ftec_init_led(hdev))
		hid_err(hdev, "LED init failed\n"); /* Let the driver continue without LEDs */
#endif

	return 0;
}


void ftecff_remove(struct hid_device *hdev)
{
	struct ftec_drv_data *drv_data = hid_get_drvdata(hdev);

	device_remove_file(&hdev->dev, &dev_attr_display);
	device_remove_file(&hdev->dev, &dev_attr_range);

	if (hdev->product == CSL_ELITE_WHEELBASE_DEVICE_ID || hdev->product == CSL_ELITE_PS4_WHEELBASE_DEVICE_ID) {
		device_remove_file(&hdev->dev, &dev_attr_RESET);
		device_remove_file(&hdev->dev, &dev_attr_SLOT);
		device_remove_file(&hdev->dev, &dev_attr_SEN);
		device_remove_file(&hdev->dev, &dev_attr_FF);
		device_remove_file(&hdev->dev, &dev_attr_DRI);
		device_remove_file(&hdev->dev, &dev_attr_FEI);
		device_remove_file(&hdev->dev, &dev_attr_FOR);
		device_remove_file(&hdev->dev, &dev_attr_SPR);
		device_remove_file(&hdev->dev, &dev_attr_DPR);
		device_remove_file(&hdev->dev, &dev_attr_BLI);
		device_remove_file(&hdev->dev, &dev_attr_SHO);
	}

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
}
