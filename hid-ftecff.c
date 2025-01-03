#include <linux/module.h>
#include <linux/device.h>
#include <linux/hid.h>
#include <linux/usb.h>
#include <linux/input.h>
#include <linux/moduleparam.h>
#include <linux/hrtimer.h>
#include <linux/fixp-arith.h>

#include "hid-ftec.h"

// parameter to set the initial range
// if unset, the wheels max range is used
int init_range = 0;
module_param(init_range, int, 0);

#define DEFAULT_TIMER_PERIOD 2

#define FF_EFFECT_STARTED 0
#define FF_EFFECT_ALLSET 1
#define FF_EFFECT_PLAYING 2
#define FF_EFFECT_UPDATING 3

#define STOP_EFFECT(state) ((state)->flags = 0)

#undef fixp_sin16
#define fixp_sin16(v) (((v % 360) > 180)? -(fixp_sin32((v % 360) - 180) >> 16) : fixp_sin32(v) >> 16)

#define DEBUG(...) pr_debug("ftecff: " __VA_ARGS__)
#define time_diff(a,b) ({ \
		typecheck(unsigned long, a); \
		typecheck(unsigned long, b); \
		((a) - (long)(b)); })
#define JIFFIES2MS(jiffies) ((jiffies) * 1000 / HZ)

static int timer_msecs = DEFAULT_TIMER_PERIOD;
static int spring_level = 100;
static int damper_level = 100;
static int inertia_level = 100;
static int friction_level = 100;

static int profile = 1;
module_param(profile, int, 0660);
MODULE_PARM_DESC(profile, "Enable profile debug messages.");


static const signed short ftecff_wheel_effects[] = {
	FF_CONSTANT,
	FF_SPRING,
	FF_DAMPER,
	FF_INERTIA,
	FF_FRICTION,
	FF_PERIODIC,
	FF_SINE,
	FF_SQUARE,
	FF_TRIANGLE,
	FF_SAW_UP,
	FF_SAW_DOWN,
	-1
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
		if (values[i]>=0x80)
			values[i] = -0x100 + values[i];
	}
}

/*
 bits 7 6 5 4 3 2 1 0
 7 segments bits and dot
      0
    5   1
      6
    4   2
      3   7
*/
static u8 segbits[42] ={
	 63, // 0
	  6, // 1
	 91, // 2
	 79, // 3
	102, // 4
	109, // 5
	125, // 6
	  7, // 7
	127, // 8
	103, // 9
	128, // dot
	  0, // blank
	 57, // [
	 15, // ]
	 64, // -
	  8, // _
	119, // a
	124, // b
	 88, // c
	 94, // d
	121, // e
	113, // f
	 61, // g
	118, // h
	 48, // i
	 14, // j
	  0, // k - placeholder/blank
	 56, // l
	  0, // m - placeholder/blank
	 84, // n
	 92, // o
	115, // p
	103, // q
	 80, // r
	109, // s
	120, // t
	 62, // u
	  0, // v - placeholder/blank
	  0, // w - placeholder/blank
	  0, // x - placeholder/blank
	110, // y
	 91  // z
};

static u8 seg_bits(u8 value, bool point) {
	u8 num_index = 11; // defaults to blank

	// capital letters ASCII 65 - 90 / poor mans toLower
	if(value>63 && value<91) {
		value=value+32;
	}
	// point
	if(value==46) {
		num_index = 10;
	}
	// opening square bracket
	else if(value==91) {
		num_index = 12;
	}
	// closing square bracket
	else if(value==93) {
		num_index = 13;
	}
	// hyphen
	else if(value==45) {
		num_index = 14;
	}
	// underscore
	else if(value==95) {
		num_index = 15;
	}
	// ascii numbers
	else if(value>47 && value<58) {
		num_index=value-48;
	}
	// lower case letters ASCII 98 - 117
	else if(value>96 && value<123) {
		num_index=value-81;
	}

	// if a point has to be set, flip it in the segment
	return point ? segbits[num_index]+segbits[10] : segbits[num_index];
}

static void send_report_request_to_device(struct ftec_drv_data *drv_data)
{
	struct hid_device *hdev = drv_data->hid;
	struct hid_report *report = drv_data->report;

	if (hdev->product != CSR_ELITE_WHEELBASE_DEVICE_ID)
	{
		fix_values(report->field[0]->value);
	}

	hid_hw_request(hdev, report, HID_REQ_SET_REPORT);
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
	send_report_request_to_device(drv_data);

	value[0] = 0xf8;
	value[1] = 0x09;
	value[2] = 0x01;
	value[3] = 0x06;
	value[4] = 0x01;
	value[5] = 0x00;
	value[6] = 0x00;
	send_report_request_to_device(drv_data);

	value[0] = 0xf8;
	value[1] = 0x81;
	value[2] = range&0xff;
	value[3] = (range>>8)&0xff;
	value[4] = 0x00;
	value[5] = 0x00;
	value[6] = 0x00;
	send_report_request_to_device(drv_data);
	spin_unlock_irqrestore(&drv_data->report_lock, flags);
}

/* Export the currently set range of the wheel */
static ssize_t ftec_range_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct hid_device *hid = to_hid_device(dev);
	struct ftec_drv_data *drv_data = hid_get_drvdata(hid);
	size_t count;

	/* new wheelbases have tuning menu, so use this to get the range */
	if (drv_data->quirks & FTEC_TUNING_MENU) {
		count = _ftec_tuning_show(drv_data->tuning.dev, SEN, buf);
		return count;
	}

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
	struct ftec_drv_data *drv_data = hid_get_drvdata(hid);
	u16 range;

	/* new wheelbases have tuning menu, so use this to set the range */
	if (drv_data->quirks & FTEC_TUNING_MENU) {
		count = _ftec_tuning_store(drv_data->tuning.dev, SEN, buf, count);
		return count;
	}

	if (kstrtou16(buf, 0, &range) != 0) {
		hid_err(hid, "Invalid range %s!\n", buf);
		return -EINVAL;
	}

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


/* Export the current wheel id */
static ssize_t ftec_wheel_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ftec_drv_data *drv_data = hid_get_drvdata(to_hid_device(dev));
	return scnprintf(buf, PAGE_SIZE, "0x%02x\n", drv_data->wheel_id);
}
static DEVICE_ATTR(wheel_id, S_IRUSR | S_IRGRP | S_IROTH, ftec_wheel_show, NULL);


static ssize_t ftec_set_display(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct hid_device *hid = to_hid_device(dev);
	struct ftec_drv_data *drv_data;
	unsigned long flags;
	s32 *value;
	unsigned int valueIndex, bufIndex = 0;

	// check the buffer size, note that in lack of points or commas, only the first 3 characters will be processed
	if (count > 7) {
		hid_err(hid, "Invalid value %s!\n", buf);
		return -EINVAL;
	}

	drv_data = hid_get_drvdata(hid);
	if (!drv_data) {
		hid_err(hid, "Private driver data not found!\n");
		return -EINVAL;
	}

	// dbg_hid(" ... set_display %i\n", val);
	
	value = drv_data->report->field[0]->value;

	spin_lock_irqsave(&drv_data->report_lock, flags);
	value[0] = 0xf8;
	value[1] = 0x09;
	value[2] = 0x01;
	value[3] = 0x02;
	value[4] = 0x00;
	value[5] = 0x00;
	value[6] = 0x00;

	// set each of the segments as long there is input data
	for(valueIndex = 4; valueIndex <= 6 && bufIndex < count; valueIndex++) {
		bool point = false;
		// check if next char is a point or comma if not end of the string
		if(bufIndex+1 < count) {
			point = buf[bufIndex+1] == '.' || buf[bufIndex+1] == ',';
		}
		value[valueIndex] = seg_bits(buf[bufIndex], point);
		// determinate next value, if a dot was encountered we need to step one index further
		bufIndex += point ? 2 : 1;
	}
	// 'center' values by shifting shorter inputs to the right
	if(value[4] > 0x00 && value[6] == 0x00) {
		if(value[5] == 0x00) {
			value[5] = value[4];
			value[4] = 0x00;
		}
		else if(value[5] > 0) {
			value[6] = value[5];
			value[5] = value[4];
			value[4] = 0x00;
		}
	}

	send_report_request_to_device(drv_data);
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


	drv_data = hid_get_drvdata(hid);
	if (!drv_data) {
		hid_err(hid, "Private driver data not found!\n");
		return;
	}
	
	spin_lock_irqsave(&drv_data->report_lock, flags);

	if (drv_data->quirks & FTEC_WHEELBASE_LEDS) {
		// dbg_hid(" ... set_leds base %04X\n", leds);

		value = drv_data->report->field[0]->value;

		value[0] = 0xf8;
		value[1] = 0x13;
		value[2] = leds&0xff;
		value[3] = 0x00;
		value[4] = 0x00;
		value[5] = 0x00;
		value[6] = 0x00;
		
		send_report_request_to_device(drv_data);
	}

	// reshuffle, since first led is highest bit
	for( i=0; i<LEDS; i++) {
		if (leds>>i & 1) _leds |= 1 << (LEDS-i-1);
	}

	// dbg_hid(" ... set_leds wheel %04X\n", _leds);

	value = drv_data->report->field[0]->value;

	value[0] = 0xf8;
	value[1] = 0x09;
	value[2] = 0x08;
	value[3] = (_leds>>8)&0xff;
	value[4] = _leds&0xff;
	value[5] = 0x00;
	value[6] = 0x00;
	
	send_report_request_to_device(drv_data);
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

#ifdef CONFIG_LEDS_CLASS
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
		value[3] = 0x01; // set green led to indicate driver is loaded
		value[4] = 0x00;
		value[5] = 0x00;
		value[6] = 0x00;

		send_report_request_to_device(drv_data);
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
#endif

static void ftecff_send_cmd(struct ftec_drv_data *drv_data, u8 *cmd)
{
	unsigned short i;
	unsigned long flags;
	s32 *value = drv_data->report->field[0]->value;

	spin_lock_irqsave(&drv_data->report_lock, flags);

	for(i = 0; i < 7; i++)
		value[i] = cmd[i];

	send_report_request_to_device(drv_data);
	spin_unlock_irqrestore(&drv_data->report_lock, flags);

	if (unlikely(profile))
		DEBUG("send_cmd: %02X %02X %02X %02X %02X %02X %02X", cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5], cmd[6]);
}

static __always_inline struct ff_envelope *ftecff_effect_envelope(struct ff_effect *effect)
{
	switch (effect->type) {
		case FF_CONSTANT:
			return &effect->u.constant.envelope;
		case FF_RAMP:
			return &effect->u.ramp.envelope;
		case FF_PERIODIC:
			return &effect->u.periodic.envelope;
	}

	return NULL;
}

static __always_inline void ftecff_update_state(struct ftecff_effect_state *state, const unsigned long now)
{
	struct ff_effect *effect = &state->effect;
	unsigned long phase_time;

	if (!__test_and_set_bit(FF_EFFECT_ALLSET, &state->flags)) {
		state->play_at = state->start_at + effect->replay.delay;
		if (!test_bit(FF_EFFECT_UPDATING, &state->flags)) {
			state->updated_at = state->play_at;
		}
		state->direction_gain = fixp_sin16(effect->direction * 360 / 0x10000);
		if (effect->type == FF_PERIODIC) {
			state->phase_adj = effect->u.periodic.phase * 360 / effect->u.periodic.period;
		}
		if (effect->replay.length) {
			state->stop_at = state->play_at + effect->replay.length;
		}
	}

	if (__test_and_clear_bit(FF_EFFECT_UPDATING, &state->flags)) {
		__clear_bit(FF_EFFECT_PLAYING, &state->flags);
		state->play_at = state->start_at + effect->replay.delay;
		state->direction_gain = fixp_sin16(effect->direction * 360 / 0x10000);
		if (effect->replay.length) {
			state->stop_at = state->play_at + effect->replay.length;
		}
		if (effect->type == FF_PERIODIC) {
			state->phase_adj = state->phase;
		}
	}

	state->envelope = ftecff_effect_envelope(effect);

	state->slope = 0;
	if (effect->type == FF_RAMP && effect->replay.length) {
		state->slope = ((effect->u.ramp.end_level - effect->u.ramp.start_level) << 16) / (effect->replay.length - state->envelope->attack_length - state->envelope->fade_length);
	}

	if (!test_bit(FF_EFFECT_PLAYING, &state->flags) && time_after_eq(now,
				state->play_at) && (effect->replay.length == 0 ||
					time_before(now, state->stop_at))) {
		__set_bit(FF_EFFECT_PLAYING, &state->flags);
	}

	if (test_bit(FF_EFFECT_PLAYING, &state->flags)) {
		state->time_playing = time_diff(now, state->play_at);
		if (effect->type == FF_PERIODIC) {
			phase_time = time_diff(now, state->updated_at);
			state->phase = (phase_time % effect->u.periodic.period) * 360 / effect->u.periodic.period;
			state->phase += state->phase_adj % 360;
		}
	}
}

static void ftecff_update_slot(struct ftecff_slot *slot, struct ftecff_effect_parameters *parameters, const bool highres)
{
	u8 original_cmd[7];
	unsigned short i;
	int d1;
	int d2;
	int s1;
	int s2;

	memcpy(original_cmd, slot->current_cmd, sizeof(original_cmd));
	memset(slot->current_cmd, 0, sizeof(slot->current_cmd));

	// select slot
	slot->current_cmd[0] = (slot->id<<4) | 0x1;
	// set command
	slot->current_cmd[1] = slot->cmd;

	if ((slot->effect_type == FF_CONSTANT && parameters->level == 0) ||
	    (slot->effect_type != FF_CONSTANT && parameters->clip == 0)) {
		// disable slot
		slot->current_cmd[0] |= 0x2;
		// reset values
		if (slot->effect_type != FF_CONSTANT && slot->effect_type != FF_SPRING) {
			slot->current_cmd[2] = 0x00;
			slot->current_cmd[4] = 0x00;
			slot->current_cmd[6] = 0xff;
		}
		if (original_cmd[0] != slot->current_cmd[0]) {
			slot->is_updated = 1;
		}
		return;
	}

#define CLAMP_VALUE_U16(x) ((unsigned short)((x) > 0xffff ? 0xffff : (x)))
#define CLAMP_VALUE_S16(x) ((unsigned short)((x) <= -0x8000 ? -0x8000 : ((x) > 0x7fff ? 0x7fff : (x))))
#define TRANSLATE_FORCE(x, bits) ((CLAMP_VALUE_S16(x) + 0x8000) >> (16 - bits))
#define SCALE_VALUE_U16(x, bits) (CLAMP_VALUE_U16(x) >> (16 - bits))
#define SCALE_COEFF(x, bits) SCALE_VALUE_U16(abs(x) * 2, bits)

	switch (slot->effect_type) {
		case FF_CONSTANT:
			if (highres) {
				d1 = TRANSLATE_FORCE(parameters->level, 16);
				slot->current_cmd[2] = d1&0xff;
				slot->current_cmd[3] = (d1>>8)&0xff;
				slot->current_cmd[6] = 0x01;
			} else {
				slot->current_cmd[2] = TRANSLATE_FORCE(parameters->level, 8);
			}
			// dbg_hid("constant: %i 0x%x 0x%x 0x%x\n",
			//	parameters->level, slot->current_cmd[2], slot->current_cmd[3], slot->current_cmd[6]);
			break;
		case FF_SPRING:
			d1 = SCALE_VALUE_U16(((parameters->d1) + 0x8000) & 0xffff, 11);
			d2 = SCALE_VALUE_U16(((parameters->d2) + 0x8000) & 0xffff, 11);
			s1 = parameters->k1 < 0;
			s2 = parameters->k2 < 0;
			slot->current_cmd[2] = d1 >> 3;
			slot->current_cmd[3] = d2 >> 3;
			slot->current_cmd[4] = (SCALE_COEFF(parameters->k2, 4) << 4) + SCALE_COEFF(parameters->k1, 4);
			slot->current_cmd[6] = SCALE_VALUE_U16(parameters->clip, 8);
			// dbg_hid("spring: %i %i %i %i %i %i %i %i %i\n",
			// 	parameters->d1, parameters->d2, parameters->k1, parameters->k2, parameters->clip,
			// 	slot->current_cmd[2], slot->current_cmd[3], slot->current_cmd[4], slot->current_cmd[6]);
			break;
		case FF_DAMPER:
		case FF_INERTIA:
		case FF_FRICTION:
			slot->current_cmd[2] = SCALE_COEFF(parameters->k1, 4);
			slot->current_cmd[4] = SCALE_COEFF(parameters->k2, 4);
			slot->current_cmd[6] = SCALE_VALUE_U16(parameters->clip, 8);
			// dbg_hid("damper/friction/inertia: 0x%x %i %i %i %i %i %i %i %i\n",
			//	slot->effect_type, parameters->d1, parameters->d2, parameters->k1, parameters->k2, parameters->clip,
			// 	slot->current_cmd[2], slot->current_cmd[4], slot->current_cmd[6]);
			break;
	}

	// check if slot needs to be updated
	for(i = 0; i < 7; i++) {
		if (original_cmd[i] != slot->current_cmd[i]) {
			slot->is_updated = 1;
			break;
		}
	}
}

static __always_inline int ftecff_calculate_constant(struct ftecff_effect_state *state)
{
	int level = state->effect.u.constant.level;
	int level_sign;
	long d, t;

	if (state->time_playing < state->envelope->attack_length) {
		level_sign = level < 0 ? -1 : 1;
		d = level - level_sign * state->envelope->attack_level;
		level = level_sign * state->envelope->attack_level + d * state->time_playing / state->envelope->attack_length;
	} else if (state->effect.replay.length) {
		t = state->time_playing - state->effect.replay.length + state->envelope->fade_length;
		if (t > 0) {
			level_sign = level < 0 ? -1 : 1;
			d = level - level_sign * state->envelope->fade_level;
			level = level - d * t / state->envelope->fade_length;
		}
	}

	return state->direction_gain * level / 0x7fff;
}

static __always_inline int ftecff_calculate_periodic(struct ftecff_effect_state *state)
{
	struct ff_periodic_effect *periodic = &state->effect.u.periodic;
	int level = periodic->offset;
	int magnitude = periodic->magnitude;
	int magnitude_sign = magnitude < 0 ? -1 : 1;
	long d, t;

	if (state->time_playing < state->envelope->attack_length) {
		d = magnitude - magnitude_sign * state->envelope->attack_level;
		magnitude = magnitude_sign * state->envelope->attack_level + d * state->time_playing / state->envelope->attack_length;
	} else if (state->effect.replay.length) {
		t = state->time_playing - state->effect.replay.length + state->envelope->fade_length;
		if (t > 0) {
			d = magnitude - magnitude_sign * state->envelope->fade_level;
			magnitude = magnitude - d * t / state->envelope->fade_length;
		}
	}

	switch (periodic->waveform) {
		case FF_SINE:
			level += fixp_sin16(state->phase) * magnitude / 0x7fff;
			break;
		case FF_SQUARE:
			level += (state->phase < 180 ? 1 : -1) * magnitude;
			break;
		case FF_TRIANGLE:
			level += abs(state->phase * magnitude * 2 / 360 - magnitude) * 2 - magnitude;
			break;
		case FF_SAW_UP:
			level += state->phase * magnitude * 2 / 360 - magnitude;
			break;
		case FF_SAW_DOWN:
			level += magnitude - state->phase * magnitude * 2 / 360;
			break;
	}

	return state->direction_gain * level / 0x7fff;
}

static __always_inline void ftecff_calculate_spring(struct ftecff_effect_state *state, struct ftecff_effect_parameters *parameters)
{
	struct ff_condition_effect *condition = &state->effect.u.condition[0];

	parameters->d1 = ((int)condition->center) - condition->deadband / 2;
	parameters->d2 = ((int)condition->center) + condition->deadband / 2;
	parameters->k1 = condition->left_coeff;
	parameters->k2 = condition->right_coeff;
	parameters->clip = (unsigned)condition->right_saturation;
}

static __always_inline void ftecff_calculate_resistance(struct ftecff_effect_state *state, struct ftecff_effect_parameters *parameters)
{
	struct ff_condition_effect *condition = &state->effect.u.condition[0];

	parameters->k1 = condition->left_coeff;
	parameters->k2 = condition->right_coeff;
	parameters->clip = (unsigned)condition->right_saturation;
}

static __always_inline int ftecff_timer(struct ftec_drv_data *drv_data)
{
	struct ftecff_slot *slot;
	struct ftecff_effect_state *state;
	struct ftecff_effect_parameters parameters[5];
	unsigned long jiffies_now = jiffies;
	unsigned long now = JIFFIES2MS(jiffies_now);
	unsigned long flags;
	unsigned int gain;
	int count;
	int effect_id;
	int i;


	memset(parameters, 0, sizeof(parameters));

	gain = 0xffff;

	spin_lock_irqsave(&drv_data->timer_lock, flags);

	count = drv_data->effects_used;

	for (effect_id = 0; effect_id < FTECFF_MAX_EFFECTS; effect_id++) {

		if (!count) {
			break;
		}

		state = &drv_data->states[effect_id];

		if (!test_bit(FF_EFFECT_STARTED, &state->flags)) {
			continue;
		}

		count--;

		if (test_bit(FF_EFFECT_ALLSET, &state->flags)) {
			if (state->effect.replay.length && time_after_eq(now, state->stop_at)) {
				STOP_EFFECT(state);
				if (!--state->count) {
					drv_data->effects_used--;
					continue;
				}
				__set_bit(FF_EFFECT_STARTED, &state->flags);
				state->start_at = state->stop_at;
			}
		}

		ftecff_update_state(state, now);

		if (!test_bit(FF_EFFECT_PLAYING, &state->flags)) {
			continue;
		}

		switch (state->effect.type) {
			case FF_CONSTANT:
				parameters[0].level += ftecff_calculate_constant(state);
				break;
			case FF_SPRING:
				ftecff_calculate_spring(state, &parameters[1]);
				break;
			case FF_DAMPER:
				ftecff_calculate_resistance(state, &parameters[2]);
				break;
			case FF_INERTIA:
				ftecff_calculate_resistance(state, &parameters[3]);
				break;
			case FF_FRICTION:
				ftecff_calculate_resistance(state, &parameters[4]);
				break;
			case FF_PERIODIC:
				parameters[0].level += ftecff_calculate_periodic(state);
				break;				
		}
	}

	spin_unlock_irqrestore(&drv_data->timer_lock, flags);

	parameters[0].level = (long)parameters[0].level * gain / 0xffff;
	parameters[1].clip = (long)parameters[1].clip * spring_level / 100;
	parameters[2].clip = (long)parameters[2].clip * damper_level / 100;
	parameters[3].clip = (long)parameters[3].clip * inertia_level / 100;
	parameters[4].clip = (long)parameters[4].clip * friction_level / 100;

	for (i = 1; i < 5; i++) {
		parameters[i].k1 = (long)parameters[i].k1 * gain / 0xffff;
		parameters[i].k2 = (long)parameters[i].k2 * gain / 0xffff;
		parameters[i].clip = (long)parameters[i].clip * gain / 0xffff;
	}

	for (i = 0; i < 5; i++) {
		slot = &drv_data->slots[i];
		ftecff_update_slot(slot, &parameters[i], drv_data->quirks & FTEC_HIGHRES);
		if (slot->is_updated) {
			ftecff_send_cmd(drv_data, slot->current_cmd);
			slot->is_updated = 0;
		}
	}

	return 0;
}

static enum hrtimer_restart ftecff_timer_hires(struct hrtimer *t)
{
	struct ftec_drv_data *drv_data = container_of(t, struct ftec_drv_data, hrtimer);
	int delay_timer;
	int overruns;

	delay_timer = ftecff_timer(drv_data);

	if (delay_timer) {
		hrtimer_forward_now(&drv_data->hrtimer, ms_to_ktime(delay_timer));
		return HRTIMER_RESTART;
	}

	if (drv_data->effects_used) {
		overruns = hrtimer_forward_now(&drv_data->hrtimer, ms_to_ktime(timer_msecs));
		overruns--;
		if (unlikely(profile && overruns > 0))
			DEBUG("Overruns: %d", overruns);
		return HRTIMER_RESTART;
	} else {
		if (unlikely(profile))
			DEBUG("Stop timer.");
		return HRTIMER_NORESTART;
	}
}

static void ftecff_init_slots(struct ftec_drv_data *drv_data)
{
	struct ftecff_effect_parameters parameters;
	int i;

	memset(&drv_data->states, 0, sizeof(drv_data->states));
	memset(&drv_data->slots, 0, sizeof(drv_data->slots));
	memset(&parameters, 0, sizeof(parameters));

	drv_data->slots[0].effect_type = FF_CONSTANT;
	drv_data->slots[1].effect_type = FF_SPRING;
	drv_data->slots[2].effect_type = FF_DAMPER;
	drv_data->slots[3].effect_type = FF_INERTIA;
	drv_data->slots[4].effect_type = FF_FRICTION;

	drv_data->slots[0].cmd = 0x08;
	drv_data->slots[1].cmd = 0x0b;
	drv_data->slots[2].cmd = 0x0c;
	drv_data->slots[3].cmd = 0x0c;
	drv_data->slots[4].cmd = 0x0c;

	for (i = 0; i < 5; i++) {
		drv_data->slots[i].id = i;
		ftecff_update_slot(&drv_data->slots[i], &parameters, drv_data->quirks & FTEC_HIGHRES);
		ftecff_send_cmd(drv_data, drv_data->slots[i].current_cmd);
		drv_data->slots[i].is_updated = 0;
	}
}

static void ftecff_stop_effects(struct ftec_drv_data *drv_data)
{
	u8 cmd[7] = {0};

	cmd[0] = 0xf3;
	ftecff_send_cmd(drv_data, cmd);
}

static int ftecff_upload_effect(struct input_dev *dev, struct ff_effect *effect, struct ff_effect *old)
{
	struct hid_device *hdev = input_get_drvdata(dev);
	struct ftec_drv_data *drv_data = hid_get_drvdata(hdev);
	struct ftecff_effect_state *state;
	unsigned long now = JIFFIES2MS(jiffies);
	unsigned long flags;
	
	state = &drv_data->states[effect->id];

	if (0) {
		struct ff_condition_effect *condition = &effect->u.condition[0];
		printk("id %u, state %lu, delay %u, interval %u, type %#02x, effect direction %#04x", effect->id, state->flags, effect->replay.delay, effect->trigger.interval, effect->type, effect->direction);
		if (effect->type == FF_PERIODIC) {
			printk(KERN_CONT ", magnitude %i, offset %i, phase %#02x, period %u\n", effect->u.periodic.magnitude, effect->u.periodic.offset, effect->u.periodic.phase, effect->u.periodic.period);
		} else {
			printk(KERN_CONT ", level %i, left_coeff %i, right_coeff %i, left_saturation %i, right_saturation %i\n", effect->u.constant.level, condition->left_coeff, condition->right_coeff, condition->left_saturation, condition->right_saturation);
		}
		// return 0;
	}

	if (effect->type == FF_PERIODIC && effect->u.periodic.period == 0) {
		return -EINVAL;
	}

	if (test_bit(FF_EFFECT_STARTED, &state->flags) && effect->type != state->effect.type) {
		return -EINVAL;
	}

	spin_lock_irqsave(&drv_data->timer_lock, flags);

	state->effect = *effect;

	if (test_bit(FF_EFFECT_STARTED, &state->flags)) {
		__set_bit(FF_EFFECT_UPDATING, &state->flags);
		state->updated_at = now;
	}

	spin_unlock_irqrestore(&drv_data->timer_lock, flags);

	return 0;
}

static int ftecff_play_effect(struct input_dev *dev, int effect_id, int value)
{
	struct hid_device *hdev = input_get_drvdata(dev);
	struct ftec_drv_data *drv_data = hid_get_drvdata(hdev);
	struct ftecff_effect_state *state;
	unsigned long now = JIFFIES2MS(jiffies);
	unsigned long flags;

	state = &drv_data->states[effect_id];

	spin_lock_irqsave(&drv_data->timer_lock, flags);

	if (value > 0) {
		if (test_bit(FF_EFFECT_STARTED, &state->flags)) {
			STOP_EFFECT(state);
		} else {
			drv_data->effects_used++;
			if (!hrtimer_active(&drv_data->hrtimer)) {
				hrtimer_start(&drv_data->hrtimer, ms_to_ktime(timer_msecs), HRTIMER_MODE_REL);
				if (unlikely(profile))
					DEBUG("Start timer.");
			}
		}
		__set_bit(FF_EFFECT_STARTED, &state->flags);
		state->start_at = now;
		state->count = value;
	} else {
		if (test_bit(FF_EFFECT_STARTED, &state->flags)) {
			STOP_EFFECT(state);
			drv_data->effects_used--;
		}
	}

	spin_unlock_irqrestore(&drv_data->timer_lock, flags);

	return 0;
}

static void ftecff_destroy(struct ff_device *ff)
{
}

int ftecff_init(struct hid_device *hdev) {
	struct ftec_drv_data *drv_data = hid_get_drvdata(hdev);
	struct hid_input *hidinput = list_entry(hdev->inputs.next, struct hid_input, list);
	struct input_dev *inputdev = hidinput->input;
	struct ff_device *ff;
	unsigned long flags;
	int ret,j;
	s32 *value;

	dbg_hid(" ... setting FF bits");
	for (j = 0; ftecff_wheel_effects[j] >= 0; j++)
		set_bit(ftecff_wheel_effects[j], inputdev->ffbit);

	ret = input_ff_create(hidinput->input, FTECFF_MAX_EFFECTS);
	if (ret) {
		hid_err(hdev, "Unable to create ff: %i\n", ret);
		return ret;
	}

	ff = hidinput->input->ff;
	ff->upload = ftecff_upload_effect;
	ff->playback = ftecff_play_effect;
	ff->destroy = ftecff_destroy;

	/* init sequence */
	{
		/* tuning menu initialization? */
		if (drv_data->quirks & FTEC_TUNING_MENU) {
			u8 buf[] = { 0xff, 0x08, 0x01, 0xff, 0x0, 0x0, 0x0, 0x0 };
			ret = hid_hw_output_report(hdev, &buf[0], 8);
			buf[2] = 0x2;
			buf[3] = 0x0;
			ret = hid_hw_output_report(hdev, &buf[0], 8);
			// FIXME: does this work to fetch current tuning menu values?
			buf[1] = 0x03;
			buf[2] = 0x02;
			ret = hid_hw_output_report(hdev, &buf[0], 8);
		}

		/* common initialization? */
		spin_lock_irqsave(&drv_data->report_lock, flags);
		value = drv_data->report->field[0]->value;
		value[0] = 0xf8;
		value[1] = 0x09;
		value[2] = 0x01;
		value[3] = 0x06;
		value[4] = 0xff;
		value[5] = 0x01;
		value[6] = 0x00;
		send_report_request_to_device(drv_data);
		value[5] = 0x0;
		send_report_request_to_device(drv_data);
		value[5] = 0x4;
		send_report_request_to_device(drv_data);
		spin_unlock_irqrestore(&drv_data->report_lock, flags);
	}

	/* Set range so that centering spring gets disabled */
	if (init_range > 0 && (init_range > drv_data->max_range || init_range < drv_data->min_range)) {
		hid_warn(hdev, "Invalid init_range %i; using max range of %i instead\n", init_range, drv_data->max_range);
		init_range = -1;
	}
	drv_data->range = init_range > 0 ? init_range : drv_data->max_range;
	ftec_set_range(hdev, drv_data->range);

	/* Create sysfs interface */
#define CREATE_SYSFS_FILE(name) \
	ret = device_create_file(&hdev->dev, &dev_attr_##name); \
	if (ret) \
		hid_warn(hdev, "Unable to create sysfs interface for '%s', errno %d\n", #name, ret); \
	
	CREATE_SYSFS_FILE(display)
	CREATE_SYSFS_FILE(range)
	CREATE_SYSFS_FILE(wheel_id)

	if (drv_data->quirks & FTEC_TUNING_MENU) {
		ftec_tuning_classdev_register(&hdev->dev, &drv_data->tuning);
	}

#ifdef CONFIG_LEDS_CLASS
	if (ftec_init_led(hdev))
		hid_err(hdev, "LED init failed\n"); /* Let the driver continue without LEDs */
#endif

	drv_data->effects_used = 0;

	ftecff_init_slots(drv_data);
	spin_lock_init(&drv_data->timer_lock);

	hrtimer_init(&drv_data->hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	drv_data->hrtimer.function = ftecff_timer_hires;
	hid_info(hdev, "Hires timer: period = %d ms", timer_msecs);

	return 0;
}


void ftecff_remove(struct hid_device *hdev)
{
	struct ftec_drv_data *drv_data = hid_get_drvdata(hdev);

	hrtimer_cancel(&drv_data->hrtimer);
	ftecff_stop_effects(drv_data);

	device_remove_file(&hdev->dev, &dev_attr_display);
	device_remove_file(&hdev->dev, &dev_attr_range);
	device_remove_file(&hdev->dev, &dev_attr_wheel_id);

#define REMOVE_SYSFS_FILE(name) device_remove_file(&hdev->dev, &dev_attr_##name); \

	if (drv_data->quirks & FTEC_TUNING_MENU) {
		ftec_tuning_classdev_unregister(&drv_data->tuning);
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

int ftecff_raw_event(struct hid_device *hdev, struct hid_report *report, u8 *data, int size) {
	struct ftec_drv_data *drv_data = hid_get_drvdata(hdev);
	if (data[0] == 0xff && size == FTEC_TUNING_REPORT_SIZE && data[1] == 0x03) {
		// shift by 1 so that we can use this as the buffer when writing back to the device
		memcpy(&drv_data->tuning.ftec_tuning_data[0] + 1, data, sizeof(drv_data->tuning.ftec_tuning_data) - 1);
		drv_data->tuning.advanced_mode = !(drv_data->tuning.ftec_tuning_data[3] & 0x80);
		drv_data->tuning.ftec_tuning_data[3] = drv_data->tuning.ftec_tuning_data[3]&0xf;
		// notify userspace about value change
		if (!(IS_ERR_OR_NULL(drv_data->tuning.dev)))
			kobject_uevent(&drv_data->tuning.dev->kobj, KOBJ_CHANGE);
	} else if (data[0] == 0x01) {
		// TODO: detect wheel change and react on it in some way?
		bool changed = drv_data->wheel_id != data[0x1f];
		drv_data->wheel_id = data[0x1f];
		// notify userspace about value change
		if (changed)
			kobject_uevent(&hdev->dev.kobj, KOBJ_CHANGE);
	}
	return 0;
}
