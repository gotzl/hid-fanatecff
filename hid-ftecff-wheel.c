#include <linux/module.h>
#include <linux/hid.h>
#include <linux/leds.h>
#include <linux/led-class-multicolor.h>

#include "hid-ftec.h"

#define FTEC_WHEEL_FLAG_MC 0x1


#ifdef CONFIG_LEDS_CLASS_MULTICOLOR
static void ftec_wheel_set_led_mc(struct hid_device *hid, u16 led_state_mc[], u8 n_leds, bool rpm_leds)
{
	u8 *buf = kmalloc_array(FTEC_TUNING_REPORT_SIZE, sizeof(u8), GFP_KERNEL | __GFP_ZERO);
	int j, ret, offset = rpm_leds ? 0 : 9;

	buf[0] = 0xff;
	buf[1] = 0x01;

	buf[2] = rpm_leds ? 0x00 : 0x01;

	for (j = 0; j < n_leds; j++) {
		buf[3+(2*j)] = led_state_mc[offset+j] >> 8;
		buf[3+(2*j)+1] = led_state_mc[offset+j] & 0xff;
	}
	ret = hid_hw_output_report(hid, &buf[0], FTEC_TUNING_REPORT_SIZE);
	kfree(buf);
}

static int ftec_wheel_led_mc_set_brightness(struct led_classdev *cdev,
        enum led_brightness brightness)
{
	struct device *dev = cdev->dev->parent->parent;
	struct hid_device *hid = to_hid_device(dev);
	struct ftec_drv_data *drv_data = hid_get_drvdata(hid);
        struct led_classdev_mc *mc_cdev = lcdev_to_mccdev(cdev);
        uint8_t red, green, blue;
	int j, n;

	if (!drv_data) {
		hid_err(hid, "Device data not found.");
		return -1;
	}

	for (j = 0; j < MAX_LEDS && j < drv_data->wheel.wheel->n_leds; j++) {
		if (mc_cdev == drv_data->wheel.led_mc[j]) {
			led_mc_calc_color_components(mc_cdev, brightness);
			red = mc_cdev->subled_info[0].brightness;
			green = mc_cdev->subled_info[1].brightness;
			blue = mc_cdev->subled_info[2].brightness;

			/* BGR565 encoding */
			drv_data->wheel.led_state_mc[j] = ((u16)(blue >> (8-5)) << (6+5)) | ((u16)(green >> (8-6)) << 5) | (u16)(red >> (8-5));
			/* FIXME: switch between RPM and FLAG leds */
			n = j < 9 ? 9 : 6;
			ftec_wheel_set_led_mc(hid, drv_data->wheel.led_state_mc, n, j < 9);
			return 0;
		}
	}
	return -1;
}

static int ftec_wheel_init_multicolor(struct hid_device *hid,
		struct ftec_wheel_classdev *ftec_wheel_cdev,
		int j, struct led_classdev_mc* led) {
        struct mc_subled *mc_led_info;
        struct led_classdev *led_cdev;
        int ret;

        mc_led_info = devm_kmalloc_array(ftec_wheel_cdev->dev, 3, sizeof(*mc_led_info), GFP_KERNEL | __GFP_ZERO);
        if (!mc_led_info)
                return -ENOMEM;

        mc_led_info[0].color_index = LED_COLOR_ID_RED;
        mc_led_info[1].color_index = LED_COLOR_ID_GREEN;
        mc_led_info[2].color_index = LED_COLOR_ID_BLUE;

        led->subled_info = mc_led_info;
        led->num_colors = 3;

        led_cdev = &led->led_cdev;
	/* FIXME: hardcoded switch bitween RPM LEDs strip above the display,
	 * and FLAG LEDs left/right of the display, found eg on F1 steering wheels */
        led_cdev->name = devm_kasprintf(ftec_wheel_cdev->dev, GFP_KERNEL,
			"%s:RGB:%s%i", dev_name(&hid->dev),
			j < 9 ? "RPM" : (j < 12 ? "LFLAG" : "RFLAG"),
			j < 9 ? j + 1 : (j % 3) + 1);
        if (!led_cdev->name)
                return -ENOMEM;
        led_cdev->brightness = 255;
        led_cdev->max_brightness = 255;
        led_cdev->brightness_set_blocking = ftec_wheel_led_mc_set_brightness;

        ret = devm_led_classdev_multicolor_register(ftec_wheel_cdev->dev, led);
        if (ret < 0) {
                hid_err(hid, "Cannot register multicolor LED device\n");
                return ret;
        }

        return 0;
}
#endif

#ifdef CONFIG_LEDS_CLASS
static void ftec_set_leds(struct hid_device *hid, u16 leds)
{
	struct ftec_drv_data *drv_data;
	unsigned long flags;
	s32 *value;

	drv_data = hid_get_drvdata(hid);
	if (!drv_data) {
		hid_err(hid, "Private driver data not found!\n");
		return;
	}
	
	spin_lock_irqsave(&drv_data->report_lock, flags);
/*
	// reshuffle, since first led is highest bit
	for( i=0; i<LEDS; i++) {
		if (leds>>i & 1) _leds |= 1 << (LEDS-i-1);
	}
*/

	value = drv_data->report->field[0]->value;

	value[0] = 0xf8;
	value[1] = 0x09;
	value[2] = 0x08;
	value[3] = (leds>>8)&0xff;
	value[4] = leds&0xff;
	value[5] = 0x00;
	value[6] = 0x00;
	
	send_report_request_to_device(drv_data);
	spin_unlock_irqrestore(&drv_data->report_lock, flags);
}
 
int _ftec_led_update_state(struct led_classdev *led_cdev, enum led_brightness value,
		struct led_classdev* leds[], int n, u16 led_state)
{
	int i, state = 0;
	for (i = 0; i < MAX_LEDS && i < n; i++) {
		if (led_cdev != leds[i])
			continue;
		state = (led_state >> i) & 1;
		if (value == LED_OFF && state) {
			led_state &= ~(1 << i);
		} else if (value != LED_OFF && !state) {
			led_state |= 1 << i;
		}
		break;
	}
	return led_state;
}

static void ftec_wheel_led_set_brightness(struct led_classdev *led_cdev,
			enum led_brightness value)
{
	struct device *dev = led_cdev->dev->parent->parent;
	struct hid_device *hid = to_hid_device(dev);
	struct ftec_drv_data *drv_data = hid_get_drvdata(hid);

	if (!drv_data) {
		hid_err(hid, "Device data not found.");
		return;
	}

	drv_data->wheel.led_state = _ftec_led_update_state(led_cdev, value,
			drv_data->wheel.led, drv_data->wheel.wheel->n_leds,
			drv_data->wheel.led_state);
	ftec_set_leds(hid, drv_data->wheel.led_state);
}

static enum led_brightness ftec_wheel_led_get_brightness(struct led_classdev *led_cdev)
{
	struct device *dev = led_cdev->dev->parent->parent;
	struct hid_device *hid = to_hid_device(dev);
	struct ftec_drv_data *drv_data = hid_get_drvdata(hid);
	int i, value = 0;

	if (!drv_data) {
		hid_err(hid, "Device data not found.");
		return LED_OFF;
	}

	for (i = 0; i < MAX_LEDS && i < drv_data->wheel.wheel->n_leds; i++)
		if (led_cdev == drv_data->led[i]) {
			value = (drv_data->led_state >> i) & 1;
			break;
		}

	return value ? LED_FULL : LED_OFF;
}
static struct led_classdev* ftec_wheel_init_led(struct hid_device *hid, 
		struct ftec_wheel_classdev *ftec_wheel_cdev, int j) 
{
	struct led_classdev *led;
	size_t name_sz;
	char *name;
	int ret;

	name_sz = strlen(dev_name(&hid->dev)) + 10;
	led = kzalloc(sizeof(struct led_classdev)+name_sz, GFP_KERNEL);

	if (!led) {
		hid_err(hid, "can't allocate memory for LED %d\n", j);
		return NULL;
	}

	name = (void *)(&led[1]);
	snprintf(name, name_sz, "%s:%02x:RPM%d", dev_name(&hid->dev), ftec_wheel_cdev->wheel->id, j+1);
	led->name = name;
	led->brightness = 0;
	led->max_brightness = 1;
	led->brightness_get = ftec_wheel_led_get_brightness;
	led->brightness_set = ftec_wheel_led_set_brightness;

	ret = led_classdev_register(ftec_wheel_cdev->dev, led);
	if (ret) {
		hid_err(hid, "failed to register LED.");
		kfree(led);
	}

	return led;
}
#endif


#ifdef CONFIG_LEDS_CLASS
static void ftec_wheel_deinit_leds(struct ftec_wheel_classdev *ftec_wheel_cdev)
{
	int j;

	for (j = 0; j < MAX_LEDS && j < ftec_wheel_cdev->wheel->n_leds; j++) {
		if (ftec_wheel_cdev->wheel->flags & FTEC_WHEEL_FLAG_MC) {
			ftec_wheel_cdev->led_mc[j] = NULL;
		} else {
			if (ftec_wheel_cdev->led[j]) {
				led_classdev_unregister(ftec_wheel_cdev->led[j]);
				kfree(ftec_wheel_cdev->led[j]);
				ftec_wheel_cdev->led[j] = NULL;
			}

		}
	}
}

static int ftec_wheel_init_leds(struct hid_device *hid, struct ftec_wheel_classdev *ftec_wheel_cdev) {
	int j;
	for (j = 0; j < MAX_LEDS && j < ftec_wheel_cdev->wheel->n_leds; j++) {
		if (ftec_wheel_cdev->wheel->flags & FTEC_WHEEL_FLAG_MC) {
#ifdef CONFIG_LEDS_CLASS_MULTICOLOR
			ftec_wheel_cdev->led_mc[j] = devm_kzalloc(ftec_wheel_cdev->dev, sizeof(struct led_classdev_mc), GFP_KERNEL);
			if (!ftec_wheel_cdev->led_mc[j])
				goto err;
			if (ftec_wheel_init_multicolor(hid, ftec_wheel_cdev, j, ftec_wheel_cdev->led_mc[j]))
				goto err;
#endif
		} else {
			ftec_wheel_cdev->led[j] = ftec_wheel_init_led(hid, ftec_wheel_cdev, j);
			if (!ftec_wheel_cdev->led[j])
				goto err;
		}
	}
	return 0;
err:
	ftec_wheel_deinit_leds(ftec_wheel_cdev);
	return -1;
}
#endif

static struct class ftec_wheel_class = {
	.name = "ftec_wheel",
};

static const struct wheel_id wheels[] = {
	{CSL_STEERING_WHEEL_P1_V2_ID, "CSL Steering Wheel P1 V2", FTEC_WHEEL_FLAG_MC, 1},
	{CSL_ELITE_STEERING_WHEEL_WRC_ID, "CSL Elite Steering Wheel WRC", FTEC_WHEEL_FLAG_MC, 1},
	{CSL_ELITE_STEERING_WHEEL_MCLAREN_GT3_V2_ID, "CSL Elite Steering Wheel McLaren GT3", 0, 0},
	{CLUBSPORT_STEERING_WHEEL_F1_IS_ID, "ClubSport Steering Wheel F1 IS", 0, 9},
	{CLUBSPORT_STEERING_WHEEL_FORMULA_V2_ID, "ClubSport Steering Wheel Formula V2", FTEC_WHEEL_FLAG_MC, 15},
	{PODIUM_STEERING_WHEEL_PORSCHE_911_GT3_R_ID, "Podium Steering Wheel Porsche 911 GT3 R", 0, 9},
	{}
};

int ftec_wheel_classdev_register(struct device *parent,
		struct ftec_wheel_classdev *ftec_wheel_cdev,
		u8 wheel_id)
{
	struct hid_device *hdev = to_hid_device(parent);
	int ret, j;
	
	ret = class_register(&ftec_wheel_class);
	if (ret)
		return 0;

	ftec_wheel_cdev->dev = device_create(&ftec_wheel_class, parent, 0, NULL, "%s", dev_name(&hdev->dev));
	ftec_wheel_cdev->wheel = NULL;
	for (j = 0; j < sizeof(wheels); j++) {
		if (wheels[j].id == wheel_id) {
			ftec_wheel_cdev->wheel = &wheels[j];
			break;
		}
	}

	if (!ftec_wheel_cdev->wheel) {
		hid_err(hdev, "unknown wheel_id 0x%02x\n", wheel_id);
		return 0;
	}

	ret = ftec_wheel_init_leds(hdev, ftec_wheel_cdev);

	return 0;
}


void ftec_wheel_classdev_unregister(struct ftec_wheel_classdev *ftec_wheel_cdev)
{
	ftec_wheel_deinit_leds(ftec_wheel_cdev);
	device_unregister(ftec_wheel_cdev->dev);
	class_unregister(&ftec_wheel_class);
}

