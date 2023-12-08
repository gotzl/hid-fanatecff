#ifndef __HID_FTEC_H
#define __HID_FTEC_H

#define FANATEC_VENDOR_ID 0x0eb7

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

#define CSL_STEERING_WHEEL_P1_V2 0x0008
#define CSL_ELITE_STEERING_WHEEL_WRC_ID 0x0112
#define CSL_ELITE_STEERING_WHEEL_MCLAREN_GT3_V2_ID 0x280b
#define CLUBSPORT_STEERING_WHEEL_F1_IS_ID 0x1102
#define CLUBSPORT_STEERING_WHEEL_FORMULA_V2_ID 0x280a
#define PODIUM_STEERING_WHEEL_PORSCHE_911_GT3_R_ID 0x050c

#define PORSCHE_TURBO_S_WHEEL_ID 0x0197

#define LEDS 9
#define NUM_TUNING_SLOTS 5
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
};

struct ftec_drv_data {
	unsigned long quirks;
    spinlock_t report_lock; /* Protect output HID report */
	spinlock_t timer_lock;
	struct hrtimer hrtimer;
	struct hid_device *hid;
	struct hid_report *report;
	struct ftecff_slot slots[4];
	struct ftecff_effect_state states[FTECFF_MAX_EFFECTS];
	int effects_used;	
	u16 range;
	u16 max_range;
	u16 min_range;
#ifdef CONFIG_LEDS_CLASS
	u16 led_state;
	struct led_classdev *led[LEDS];
#endif    
};

#endif
