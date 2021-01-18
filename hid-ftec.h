#ifndef __HID_FTEC_H
#define __HID_FTEC_H

#define FANATEC_VENDOR_ID 0x0eb7

#define CLUBSPORT_V2_WHEELBASE_DEVICE_ID 0x0001
#define CLUBSPORT_V25_WHEELBASE_DEVICE_ID 0x0004
#define CSL_ELITE_WHEELBASE_DEVICE_ID 0x0E03
#define CSL_ELITE_PS4_WHEELBASE_DEVICE_ID 0x0005
#define CSL_ELITE_PEDALS_DEVICE_ID 0x6204

#define LEDS 9
#define NUM_TUNING_SLOTS 5

struct ftec_drv_data {
	unsigned long quirks;
    spinlock_t report_lock; /* Protect output HID report */
	struct hid_report *report;
	u16 range;
	u16 max_range;
	u16 min_range;
#ifdef CONFIG_LEDS_CLASS
	u16 led_state;
	struct led_classdev *led[LEDS];
#endif    
};

#endif