#include <linux/module.h>
#include <linux/device.h>
#include <linux/hid.h>
#include <linux/hrtimer.h>
#include <linux/fixp-arith.h>

#include "usbhid/usbhid.h"
#include "hid-ftec.h"

#define MIN_RANGE 90
#define MAX_RANGE 1080

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
static int spring_level = 30;
static int damper_level = 30;
static int friction_level = 30;

static int profile = 1;
module_param(profile, int, 0660);
MODULE_PARM_DESC(profile, "Enable profile debug messages.");


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

void ftecff_send_cmd(struct ftec_drv_data *drv_data, u8 *cmd)
{
	unsigned long flags;
	s32 *value = drv_data->report->field[0]->value;

	spin_lock_irqsave(&drv_data->report_lock, flags);

	value[0] = cmd[0];
	value[1] = cmd[1];
	value[2] = cmd[2];
	value[3] = cmd[3];
	value[4] = cmd[4];
	value[5] = cmd[5];
	value[6] = cmd[6];
	fix_values(value);

	hid_hw_request(drv_data->hid, drv_data->report, HID_REQ_SET_REPORT);
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

void ftecff_update_slot(struct ftecff_slot *slot, struct ftecff_effect_parameters *parameters)
{
	u8 original_cmd[7];
	int d1;
	int d2;
	int s1;
	int s2;

	memcpy(original_cmd, slot->current_cmd, sizeof(original_cmd));

	slot->current_cmd[0] = 0;
	slot->current_cmd[1] = 0;
	slot->current_cmd[2] = 0;
	slot->current_cmd[3] = 0;
	slot->current_cmd[4] = 0;
	slot->current_cmd[5] = 0;
	slot->current_cmd[6] = 0;

	if (slot->effect_type == FF_CONSTANT && parameters->level == 0) {
		slot->current_cmd[0] = 0x03;
		slot->current_cmd[1] = 0x08;

	} else if (slot->effect_type != FF_CONSTANT && parameters->clip == 0) {
		slot->current_cmd[0] = 0x13;
		slot->current_cmd[1] = slot->effect_type == FF_SPRING ? 0x0b : 0x0c;

#define CLAMP_VALUE_U16(x) ((unsigned short)((x) > 0xffff ? 0xffff : (x)))
#define CLAMP_VALUE_S16(x) ((unsigned short)((x) <= -0x8000 ? -0x8000 : ((x) > 0x7fff ? 0x7fff : (x))))
#define TRANSLATE_FORCE(x) ((CLAMP_VALUE_S16(x) + 0x8000) >> 8)
#define SCALE_COEFF(x, bits) SCALE_VALUE_U16(abs(x) * 2, bits)
#define SCALE_VALUE_U16(x, bits) (CLAMP_VALUE_U16(x) >> (16 - bits))

	} else {
		switch (slot->effect_type) {
			case FF_CONSTANT:				
				slot->current_cmd[0] = 0x01;
				slot->current_cmd[1] = 0x08;
				slot->current_cmd[2] = TRANSLATE_FORCE(parameters->level);			
				break;
			case FF_SPRING:
				d1 = SCALE_VALUE_U16(((parameters->d1) + 0x8000) & 0xffff, 11);
				d2 = SCALE_VALUE_U16(((parameters->d2) + 0x8000) & 0xffff, 11);
				s1 = parameters->k1 < 0;
				s2 = parameters->k2 < 0;
				slot->current_cmd[0] = 0x11;
				slot->current_cmd[1] = 0x0b;
				slot->current_cmd[2] = d1 >> 3;
				slot->current_cmd[3] = d2 >> 3;
				slot->current_cmd[4] = (SCALE_COEFF(parameters->k2, 4) << 4) + SCALE_COEFF(parameters->k1, 4);
				// slot->current_cmd[5] = ((d2 & 7) << 5) + ((d1 & 7) << 1) + (s2 << 4) + s1;
				slot->current_cmd[6] = SCALE_VALUE_U16(parameters->clip, 8);
				break;
			case FF_DAMPER:
				s1 = parameters->k1 < 0;
				s2 = parameters->k2 < 0;
				slot->current_cmd[0] = 0x11;
				slot->current_cmd[1] = 0x0c;
				slot->current_cmd[2] = SCALE_COEFF(parameters->k1, 4);
				// slot->current_cmd[3] = s1;
				slot->current_cmd[4] = SCALE_COEFF(parameters->k2, 4);
				// slot->current_cmd[5] = s2;
				slot->current_cmd[6] = SCALE_VALUE_U16(parameters->clip, 8);
				break;
			case FF_FRICTION:
				// s1 = parameters->k1 < 0;
				// s2 = parameters->k2 < 0;
				// slot->current_cmd[1] = 0x0e;
				// slot->current_cmd[2] = SCALE_COEFF(parameters->k1, 8);
				// slot->current_cmd[3] = SCALE_COEFF(parameters->k2, 8);
				// slot->current_cmd[4] = SCALE_VALUE_U16(parameters->clip, 8);
				// slot->current_cmd[5] = (s2 << 4) + s1;
				// slot->current_cmd[6] = 0;
				break;
		}
	}

	if (memcmp(original_cmd, slot->current_cmd, sizeof(original_cmd))) {
		slot->is_updated = 1;
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

static __always_inline void ftecff_calculate_spring(struct ftecff_effect_state *state, struct ftecff_effect_parameters *parameters)
{
	struct ff_condition_effect *condition = &state->effect.u.condition[0];
	int d1;
	int d2;

	d1 = condition->center - condition->deadband / 2;
	d2 = condition->center + condition->deadband / 2;
	if (d1 < parameters->d1) {
		parameters->d1 = d1;
	}
	if (d2 > parameters->d2) {
		parameters->d2 = d2;
	}
	parameters->k1 += condition->left_coeff;
	parameters->k2 += condition->right_coeff;
	parameters->clip = max(parameters->clip, (unsigned)max(condition->left_saturation, condition->right_saturation));
}

static __always_inline void ftecff_calculate_resistance(struct ftecff_effect_state *state, struct ftecff_effect_parameters *parameters)
{
	struct ff_condition_effect *condition = &state->effect.u.condition[0];

	parameters->k1 += condition->left_coeff;
	parameters->k2 += condition->right_coeff;
	parameters->clip = max(parameters->clip, (unsigned)max(condition->left_saturation, condition->right_saturation));
}

static __always_inline int ftecff_timer(struct ftec_drv_data *drv_data)
{
	struct usbhid_device *usbhid = drv_data->hid->driver_data;
	struct ftecff_slot *slot;
	struct ftecff_effect_state *state;
	struct ftecff_effect_parameters parameters[4];
	unsigned long jiffies_now = jiffies;
	unsigned long now = JIFFIES2MS(jiffies_now);
	unsigned long flags;
	unsigned int gain;
	int current_period;
	int count;
	int effect_id;
	int i;


	if (usbhid->outhead != usbhid->outtail) {
		current_period = timer_msecs;
		timer_msecs *= 2;
		hid_info(drv_data->hid, "Commands stacking up, increasing timer period to %d ms.", timer_msecs);
		return current_period;
	}

	memset(parameters, 0, sizeof(parameters));

	gain = 0xffff; //(unsigned long)entry->wdata.master_gain * entry->wdata.gain / 0xffff;

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
		}
	}

	spin_unlock_irqrestore(&drv_data->timer_lock, flags);

	parameters[0].level = (long)parameters[0].level * gain / 0xffff;
	parameters[1].clip = (long)parameters[1].clip * spring_level / 100;
	parameters[2].clip = (long)parameters[2].clip * damper_level / 100;
	parameters[3].clip = (long)parameters[3].clip * friction_level / 100;

	for (i = 1; i < 4; i++) {
		parameters[i].k1 = (long)parameters[i].k1 * gain / 0xffff;
		parameters[i].k2 = (long)parameters[i].k2 * gain / 0xffff;
		parameters[i].clip = (long)parameters[i].clip * gain / 0xffff;
	}

	for (i = 0; i < 4; i++) {
		slot = &drv_data->slots[i];
		ftecff_update_slot(slot, &parameters[i]);
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
	drv_data->slots[3].effect_type = FF_FRICTION;

	for (i = 0; i < 4; i++) {
		drv_data->slots[i].id = i;
		ftecff_update_slot(&drv_data->slots[i], &parameters);
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

	if (effect->type == FF_PERIODIC && effect->u.periodic.period == 0) {
		return -EINVAL;
	}

	state = &drv_data->states[effect->id];

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
	int ret;

	set_bit(FF_CONSTANT, inputdev->ffbit);
	set_bit(FF_DAMPER, inputdev->ffbit);
	set_bit(FF_SPRING, inputdev->ffbit);

	ret = input_ff_create(hidinput->input, FTECFF_MAX_EFFECTS);
	if (ret) {
		hid_err(hdev, "Unable to create ff: %i\n", ret);
		return ret;
	}

	ff = hidinput->input->ff;
	ff->upload = ftecff_upload_effect;
	ff->playback = ftecff_play_effect;
	ff->destroy = ftecff_destroy;	

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
