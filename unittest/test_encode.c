// SPDX-License-Identifier: GPL-2.0
/*
 * Userspace unit test for the FFB slot command encoder (hid-ftecff-encode.h).
 *
 * Expected bytes follow the layout used by the Windows driver
 * (FAWForceFeedbackDrv64.dll, GetFFBHidReportConstant/Spring/Damper), with
 * Linux ff units (coefficients/levels in +-0x7fff, saturation 0..0xffff).
 *
 * Build and run: make test (from the repository root)
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/input.h> /* FF_CONSTANT, FF_SPRING, ... (uapi) */

typedef unsigned char u8;

#include "../hid-ftecff-encode.h"

static int failures;
static int checks;

#define CMD_CONSTANT 0x08
#define CMD_SPRING 0x0b
#define CMD_DAMPER 0x0c

static void expect(const char *name, int id, u8 command, int type,
		   struct ftecff_effect_parameters p, bool highres,
		   bool highres_dynamic, bool want_enabled,
		   const u8 want[7])
{
	u8 got[7];
	bool enabled;
	int i;

	memset(got, 0xaa, sizeof(got)); /* encoder must overwrite all bytes */
	enabled = ftecff_encode_slot_cmd(got, id, command, type, &p, highres,
					 highres_dynamic);
	checks++;
	if (enabled == want_enabled && memcmp(got, want, 7) == 0)
		return;

	failures++;
	printf("FAIL %s\n  want (%s):", name, want_enabled ? "on " : "off");
	for (i = 0; i < 7; i++)
		printf(" %02x", want[i]);
	printf("\n  got  (%s):", enabled ? "on " : "off");
	for (i = 0; i < 7; i++)
		printf(" %02x", got[i]);
	printf("\n");
}

#define P(...) ((struct ftecff_effect_parameters){ __VA_ARGS__ })
#define B(...) ((const u8[7]){ __VA_ARGS__ })

static void test_constant(void)
{
	/* 8-bit: centred on 0x80 */
	expect("constant 8bit max", 0, CMD_CONSTANT, FF_CONSTANT,
	       P(.level = 0x7fff), false, false, true,
	       B(0x01, 0x08, 0xff, 0, 0, 0, 0));
	expect("constant 8bit min", 0, CMD_CONSTANT, FF_CONSTANT,
	       P(.level = -0x8000), false, false, true,
	       B(0x01, 0x08, 0x00, 0, 0, 0, 0));
	expect("constant 8bit small positive", 0, CMD_CONSTANT, FF_CONSTANT,
	       P(.level = 0x100), false, false, true,
	       B(0x01, 0x08, 0x81, 0, 0, 0, 0));

	/* 16-bit: little endian in [2],[3], marker 0x01 in [6] */
	expect("constant 16bit", 0, CMD_CONSTANT, FF_CONSTANT,
	       P(.level = 0x1234), true, false, true,
	       B(0x01, 0x08, 0x34, 0x92, 0, 0, 0x01));
	expect("constant 16bit min", 0, CMD_CONSTANT, FF_CONSTANT,
	       P(.level = -0x8000), true, false, true,
	       B(0x01, 0x08, 0x00, 0x00, 0, 0, 0x01));

	/* level 0 disables the slot: (id<<4)|3, everything else zero */
	expect("constant off", 0, CMD_CONSTANT, FF_CONSTANT, P(.level = 0),
	       true, false, false, B(0x03, 0x08, 0, 0, 0, 0, 0));
}

static void test_spring(void)
{
	/* centred, full coefficients, full saturation */
	expect("spring positive", 1, CMD_SPRING, FF_SPRING,
	       P(.d1 = 0, .d2 = 0, .k1 = 0x7fff, .k2 = 0x7fff, .clip = 0xffff),
	       false, false, true, B(0x11, 0x0b, 0x80, 0x80, 0xff, 0x00, 0xff));

	/* positions span the full byte; coefficient nibbles: hi = k2, lo = k1 */
	expect("spring positions/nibbles", 1, CMD_SPRING, FF_SPRING,
	       P(.d1 = -0x8000, .d2 = 0x7fff, .k1 = 0x4000, .k2 = 0x1000,
		 .clip = 0x8000),
	       false, false, true, B(0x11, 0x0b, 0x00, 0xff, 0x28, 0x00, 0x80));

	/* sign nibble [5] = ((k2 < 0) << 4) | (k1 < 0); magnitude unchanged */
	expect("spring k1 negative", 1, CMD_SPRING, FF_SPRING,
	       P(.k1 = -0x7fff, .k2 = 0x7fff, .clip = 0xffff), false, false,
	       true, B(0x11, 0x0b, 0x80, 0x80, 0xff, 0x01, 0xff));
	expect("spring k2 negative", 1, CMD_SPRING, FF_SPRING,
	       P(.k1 = 0x7fff, .k2 = -0x7fff, .clip = 0xffff), false, false,
	       true, B(0x11, 0x0b, 0x80, 0x80, 0xff, 0x10, 0xff));
	expect("spring both negative", 1, CMD_SPRING, FF_SPRING,
	       P(.k1 = -0x4000, .k2 = -0x1000, .clip = 0xffff), false, false,
	       true, B(0x11, 0x0b, 0x80, 0x80, 0x28, 0x11, 0xff));

	/* disabled spring: all parameters zero, sign bits not leaked */
	expect("spring off", 1, CMD_SPRING, FF_SPRING,
	       P(.k1 = -0x7fff, .k2 = -0x7fff, .clip = 0), false, false, false,
	       B(0x13, 0x0b, 0, 0, 0, 0, 0));
}

static void test_damper_family(void)
{
	static const int types[] = { FF_DAMPER, FF_INERTIA, FF_FRICTION };
	static const char *const names[] = { "damper", "inertia", "friction" };
	char name[64];
	int t;

	for (t = 0; t < 3; t++) {
		int id = 2 + t;
		u8 slot = (id << 4);

		/* positive coefficients: sign bytes [3],[5] stay zero */
		snprintf(name, sizeof(name), "%s 4bit positive", names[t]);
		expect(name, id, CMD_DAMPER, types[t],
		       P(.k1 = 0x7fff, .k2 = 0x4000, .clip = 0x8000), false,
		       false, true,
		       B(slot | 1, 0x0c, 0x0f, 0x00, 0x08, 0x00, 0x80));

		snprintf(name, sizeof(name), "%s 8bit positive", names[t]);
		expect(name, id, CMD_DAMPER, types[t],
		       P(.k1 = 0x7fff, .k2 = 0x4000, .clip = 0xffff), false,
		       true, true,
		       B(slot | 1, 0x0c, 0xff, 0x00, 0x80, 0x00, 0xff));

		/* negative coefficients: magnitude + sign bit */
		snprintf(name, sizeof(name), "%s 4bit k1 negative", names[t]);
		expect(name, id, CMD_DAMPER, types[t],
		       P(.k1 = -0x7fff, .k2 = 0x4000, .clip = 0xffff), false,
		       false, true,
		       B(slot | 1, 0x0c, 0x0f, 0x01, 0x08, 0x00, 0xff));

		snprintf(name, sizeof(name), "%s 8bit both negative", names[t]);
		expect(name, id, CMD_DAMPER, types[t],
		       P(.k1 = -0x7fff, .k2 = -0x4000, .clip = 0xffff), false,
		       true, true,
		       B(slot | 1, 0x0c, 0xff, 0x01, 0x80, 0x01, 0xff));

		/* disabled: 00 00 00 00 ff, matches Windows sat(0) == 0xff */
		snprintf(name, sizeof(name), "%s off", names[t]);
		expect(name, id, CMD_DAMPER, types[t],
		       P(.k1 = -0x7fff, .k2 = -0x7fff, .clip = 0), false, true,
		       false, B(slot | 3, 0x0c, 0, 0, 0, 0, 0xff));
	}
}

int main(void)
{
	test_constant();
	test_spring();
	test_damper_family();

	printf("%d/%d checks passed\n", checks - failures, checks);
	return failures ? 1 : 0;
}
