#ifndef __HID_FTEC_H
#define __HID_FTEC_H

#include <linux/module.h>
#include <linux/input.h>

#define FANATEC_VENDOR_ID 0x0eb7

// wheelbases
#define CLUBSPORT_V2_WHEELBASE_DEVICE_ID 0x0001
#define CLUBSPORT_V25_WHEELBASE_DEVICE_ID 0x0004
#define CLUBSPORT_PEDALS_V3_DEVICE_ID 0x183b
#define PODIUM_WHEELBASE_DD1_DEVICE_ID 0x0006
#define PODIUM_WHEELBASE_DD2_DEVICE_ID 0x0007
#define CSL_ELITE_WHEELBASE_DEVICE_ID 0x0E03
#define CSL_ELITE_PS4_WHEELBASE_DEVICE_ID 0x0005
#define CSL_ELITE_PEDALS_DEVICE_ID 0x6204
#define CSL_DD_WHEELBASE_DEVICE_ID 0x0020
#define CSR_ELITE_WHEELBASE_DEVICE_ID 0x0011

// wheels
#define CSL_STEERING_WHEEL_P1_V2 0x08
#define CSL_ELITE_STEERING_WHEEL_WRC_ID 0x12
#define CSL_ELITE_STEERING_WHEEL_MCLAREN_GT3_V2_ID 0x0b
#define CLUBSPORT_STEERING_WHEEL_F1_IS_ID 0x12
#define CLUBSPORT_STEERING_WHEEL_FORMULA_V2_ID 0x0a
#define PODIUM_STEERING_WHEEL_PORSCHE_911_GT3_R_ID 0x0c


// quirks
#define FTEC_FF                 0x001
#define FTEC_PEDALS             0x002
#define FTEC_WHEELBASE_LEDS     0x004
#define FTEC_HIGHRES		0x008
#define FTEC_TUNING_MENU	0x010

// report sizes
#define FTEC_TUNING_REPORT_SIZE 64
#define FTEC_WHEEL_REPORT_SIZE 34


// misc
#define LEDS 9
#define FTECFF_MAX_EFFECTS 16

struct ftecff_effect_state {
	struct ff_effect effect;
	struct ff_envelope *envelope;
	unsigned long start_at;
	unsigned long play_at;
	unsigned long stop_at;
	unsigned long flags;
	unsigned long time_playing;
	unsigned long updated_at;
	unsigned int phase;
	unsigned int phase_adj;
	unsigned int count;
	unsigned int cmd;
	unsigned int cmd_start_time;
	unsigned int cmd_start_count;
	int direction_gain;
	int slope;
};

struct ftecff_effect_parameters {
	int level;
	int d1;
	int d2;
	int k1;
	int k2;
	unsigned int clip;
};

struct ftecff_slot {
	int id;
	struct ftecff_effect_parameters parameters;
	u8 current_cmd[7];
	int is_updated;
	int effect_type;
	u8 cmd;
};

struct ftec_tuning_classdev {
	struct device *dev;
	// the data from the last update we got from the device, shifted by 1
	u8 ftec_tuning_data[FTEC_TUNING_REPORT_SIZE];
	u8 advanced_mode;
};

struct ftec_drv_data_client {
	struct hid_device *hdev;
	u8 rdesc[4096];
	size_t rsize;
	int opened;
	struct ff_effect effects[40];
	u8 current_id;
};

struct ftec_drv_data {
	unsigned long quirks;
	spinlock_t report_lock; /* Protect output HID report */
	spinlock_t timer_lock;
	struct hrtimer hrtimer;
	struct hid_device *hid;
	struct hid_report *report;
	struct ftec_drv_data_client client;
	struct ftecff_slot slots[5];
	struct ftecff_effect_state states[FTECFF_MAX_EFFECTS];
	int effects_used;	
	u16 range;
	u16 max_range;
	u16 min_range;
#if IS_REACHABLE(CONFIG_LEDS_CLASS)
	u16 led_state;
	struct led_classdev *led[LEDS];
#endif    
	u8 wheel_id;
	struct ftec_tuning_classdev tuning;
};

#define FTEC_TUNING_ATTRS \
	FTEC_TUNING_ATTR(SLOT, 0x02, "Slot", ftec_conv_noop_to, ftec_conv_noop_from, 1, 5) \
	FTEC_TUNING_ATTR(SEN, 0x03, "Sensivity", ftec_conv_sens_to, ftec_conv_sens_from, 90, 0) \
	FTEC_TUNING_ATTR(FF, 0x04, "Force Feedback Strength", ftec_conv_noop_to, ftec_conv_noop_from, 0, 100) \
	FTEC_TUNING_ATTR(SHO, 0x05, "Wheel Vibration Motor", ftec_conv_times_ten, ftec_conv_div_ten, 0, 100) \
	FTEC_TUNING_ATTR(BLI, 0x06, "Break Level Indicator", ftec_conv_noop_to, ftec_conv_noop_from, 0, 101) \
	FTEC_TUNING_ATTR(DRI, 0x09, "Drift Mode", ftec_conv_signed_to, ftec_conv_noop_from, -5, 3) \
	FTEC_TUNING_ATTR(FOR, 0x0a, "Force Effect Strength", ftec_conv_times_ten, ftec_conv_div_ten, 0, 120) \
	FTEC_TUNING_ATTR(SPR, 0x0b, "Spring Effect Strength", ftec_conv_times_ten, ftec_conv_div_ten, 0, 120) \
	FTEC_TUNING_ATTR(DPR, 0x0c, "Damper Effect Strength", ftec_conv_times_ten, ftec_conv_div_ten, 0, 120) \
	FTEC_TUNING_ATTR(NDP, 0x0d, "Natural Damber", ftec_conv_noop_to, ftec_conv_noop_from, 0, 100) \
	FTEC_TUNING_ATTR(NFR, 0x0e, "Natural Friction", ftec_conv_noop_to, ftec_conv_noop_from, 0, 100) \
	FTEC_TUNING_ATTR(FEI, 0x11, "Force Effect Intensity", ftec_conv_noop_to, ftec_conv_steps_ten, 0, 100) \
	FTEC_TUNING_ATTR(INT, 0x14, "FFB Interpolation Filter", ftec_conv_noop_to, ftec_conv_noop_from, 0, 20) \
	FTEC_TUNING_ATTR(NIN, 0x15, "Natural Inertia", ftec_conv_noop_to, ftec_conv_noop_from, 0, 100) \
	FTEC_TUNING_ATTR(FUL, 0x16, "FullForce", ftec_conv_noop_to, ftec_conv_noop_from, 0, 100) \

enum ftec_tuning_attrs_enum {
#define FTEC_TUNING_ATTR(id, addr, desc, conv_to, conv_from, min, max) \
	id,
FTEC_TUNING_ATTRS
	FTEC_TUNING_ATTR_NONE
#undef FTEC_TUNING_ATTR
};


int ftecff_init(struct hid_device*);
void ftecff_remove(struct hid_device*);
int ftecff_raw_event(struct hid_device*, struct hid_report*, u8*, int);

int ftec_tuning_classdev_register(struct device*, struct ftec_tuning_classdev*);
void ftec_tuning_classdev_unregister(struct ftec_tuning_classdev*);
ssize_t _ftec_tuning_show(struct device*, enum ftec_tuning_attrs_enum, char*);
ssize_t _ftec_tuning_store(struct device*, enum ftec_tuning_attrs_enum, const char*, size_t);


#endif
