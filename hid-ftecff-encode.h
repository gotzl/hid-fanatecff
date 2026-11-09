/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Encoding of Fanatec FFB slot commands.
 *
 * This header has no kernel-only dependencies beyond u8/bool/abs and the
 * FF_* effect type constants, so the same code can be compiled into the
 * kernel module and into the userspace unit test in tests/.
 *
 * Slot command layout (7 bytes, sent after the report id):
 *   [0] (slot id << 4) | 0x1 = enabled, | 0x3 = disabled
 *   [1] command: 0x08 constant, 0x0b spring, 0x0c damper/inertia/friction
 *   [2..6] effect parameters, see ftecff_encode_slot_cmd()
 */
#ifndef __HID_FTECFF_ENCODE_H
#define __HID_FTECFF_ENCODE_H

#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#endif

struct ftecff_effect_parameters {
	int level;
	int d1;
	int d2;
	int k1;
	int k2;
	unsigned int clip;
};

#define CLAMP_VALUE_U16(x) ((unsigned short)((x) > 0xffff ? 0xffff : (x)))
#define CLAMP_VALUE_S16(x)                           \
	((unsigned short)((x) <= -0x8000 ? -0x8000 : \
					   ((x) > 0x7fff ? 0x7fff : (x))))
#define TRANSLATE_FORCE(x, bits) ((CLAMP_VALUE_S16(x) + 0x8000) >> (16 - bits))
#define SCALE_VALUE_U16(x, bits) (CLAMP_VALUE_U16(x) >> (16 - bits))
#define SCALE_COEFF(x, bits) SCALE_VALUE_U16(abs(x) * 2, bits)

/*
 * Fill cmd[0..6] for one effect slot.
 *
 * @highres:         16-bit constant force
 * @highres_dynamic: 8-bit instead of 4-bit damper/inertia/friction coefficients
 *
 * Returns false if the slot was encoded as disabled.
 *
 * Coefficient signs: the base expects the magnitude and the sign of each
 * coefficient separately. A set sign bit means a negative coefficient, i.e.
 * a force that pushes in the direction of displacement/motion rather than
 * against it. The Windows driver (FAWForceFeedbackDrv64.dll,
 * GetFFBHidReportSpring/GetFFBHidReportDamper) encodes them as:
 *   spring:            cmd[5] = ((k2 < 0) << 4) | (k1 < 0)
 *   damper/inertia/
 *   friction:          cmd[3] = k1 < 0, cmd[5] = k2 < 0
 */
static inline bool
ftecff_encode_slot_cmd(u8 cmd[7], int id, u8 command, int effect_type,
		       const struct ftecff_effect_parameters *parameters,
		       bool highres, bool highres_dynamic)
{
	int d1;
	int d2;
	int s1;
	int s2;
	int i;

	for (i = 0; i < 7; i++)
		cmd[i] = 0;

	// select slot
	cmd[0] = (id << 4) | 0x1;
	// set command
	cmd[1] = command;

	if ((effect_type == FF_CONSTANT && parameters->level == 0) ||
	    (effect_type != FF_CONSTANT && parameters->clip == 0)) {
		// disable slot
		cmd[0] |= 0x2;
		// reset values
		if (effect_type != FF_CONSTANT && effect_type != FF_SPRING) {
			cmd[2] = 0x00;
			cmd[4] = 0x00;
			cmd[6] = 0xff;
		}
		return false;
	}

	switch (effect_type) {
	case FF_CONSTANT:
		if (highres) {
			d1 = TRANSLATE_FORCE(parameters->level, 16);
			cmd[2] = d1 & 0xff;
			cmd[3] = (d1 >> 8) & 0xff;
			cmd[6] = 0x01;
		} else {
			cmd[2] = TRANSLATE_FORCE(parameters->level, 8);
		}
		break;
	case FF_SPRING:
		d1 = SCALE_VALUE_U16(((parameters->d1) + 0x8000) & 0xffff, 11);
		d2 = SCALE_VALUE_U16(((parameters->d2) + 0x8000) & 0xffff, 11);
		s1 = parameters->k1 < 0;
		s2 = parameters->k2 < 0;
		cmd[2] = d1 >> 3;
		cmd[3] = d2 >> 3;
		cmd[4] = (SCALE_COEFF(parameters->k2, 4) << 4) +
			 SCALE_COEFF(parameters->k1, 4);
		cmd[5] = (s2 << 4) | s1;
		cmd[6] = SCALE_VALUE_U16(parameters->clip, 8);
		break;
	case FF_DAMPER:
	case FF_INERTIA:
	case FF_FRICTION:
		// On DD1, the legacy 4-bit coeff (max 0x0f) was too weak to feel; an
		// 8-bit coeff in byte 2/4 produces clear resistance. Bytes 3/5 are
		// the coefficient sign bits, not extra magnitude: setting them for a
		// positive coefficient inverts the force, which is the oscillation
		// observed earlier. Windows uses the 8-bit coefficients only on
		// Podium DD1/DD2 with recent firmware.
		if (highres_dynamic) {
			cmd[2] = SCALE_COEFF(parameters->k1, 8);
			cmd[4] = SCALE_COEFF(parameters->k2, 8);
		} else {
			cmd[2] = SCALE_COEFF(parameters->k1, 4);
			cmd[4] = SCALE_COEFF(parameters->k2, 4);
		}
		cmd[3] = parameters->k1 < 0;
		cmd[5] = parameters->k2 < 0;
		cmd[6] = SCALE_VALUE_U16(parameters->clip, 8);
		break;
	}

	return true;
}

#endif
