#ifndef __HID_FTEC_H
#define __HID_FTEC_H

#define LEDS 9

struct ftec_drv_data {
	unsigned long quirks;
    spinlock_t report_lock; /* Protect output HID report */
	struct hid_report *report;
	u16 range;
#ifdef CONFIG_LEDS_CLASS
	u16 led_state;
	struct led_classdev *led[LEDS];
#endif    
};

#endif