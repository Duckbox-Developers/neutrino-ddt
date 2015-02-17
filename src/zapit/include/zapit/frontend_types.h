/*
 * Copyright (C) 2012 CoolStream International Ltd
 *
 * License: GPLv2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
#ifndef __FRONTEND_TYPES_H__
#define __FRONTEND_TYPES_H__

#include <linux/dvb/version.h>
#include <linux/dvb/frontend.h>

// Delivery Systems
#define ZAPIT_DS_UNKNOWN	0x00000000
#define ZAPIT_DS_DVB_S		0x00000001
#define ZAPIT_DS_DVB_S2		0x00000002
#define ZAPIT_DS_DVB_C		0x00000004
#define ZAPIT_DS_DVB_C2		0x00000008
#define ZAPIT_DS_DVB_T		0x00000010
#define ZAPIT_DS_DVB_T2		0x00000020
#define ZAPIT_DS_DTMB		0x00000040
#define ZAPIT_DS_DSS		0x00000080
#define ZAPIT_DS_TURBO		0x00000100
#define ZAPIT_DS_ISDBS		0x00000200
#define ZAPIT_DS_ISDBC		0x00000400
#define ZAPIT_DS_ISDBT		0x00000800
#define ZAPIT_DS_MASK		0x00000FFF // WARNING: update this mask if delivery systems are added.
// Delivery Method
#define ZAPIT_DM_SAT		(ZAPIT_DS_DVB_S  | \
				 ZAPIT_DS_DVB_S2 | \
				 ZAPIT_DS_DSS	 | \
				 ZAPIT_DS_TURBO  | \
				 ZAPIT_DS_ISDBS)

#define ZAPIT_DM_CABLE		(ZAPIT_DS_DVB_C  | \
				 ZAPIT_DS_DVB_C2 | \
				 ZAPIT_DS_ISDBC)

#define ZAPIT_DM_TERR		(ZAPIT_DS_DVB_T  | \
				 ZAPIT_DS_DVB_T2 | \
				 ZAPIT_DS_DTMB   | \
				 ZAPIT_DS_ISDBT)

#define ZAPIT_DS_IS_SAT(ds)	(((ds) & ZAPIT_DM_SAT) != 0)
#define ZAPIT_DS_IS_TERR(ds)	(((ds) & ZAPIT_DM_TERR) != 0)
#define ZAPIT_DS_IS_CABLE(ds)	(((ds) & ZAPIT_DM_CABLE) != 0)
#define ZAPIT_DS_BIT_MASK(ds)	((1 << (ds)) & ZAPIT_DS_MASK)
/* dvb transmission types */
typedef enum {
	UNKNOWN_DS = ZAPIT_DS_UNKNOWN,
	DVB_C	= ZAPIT_DS_DVB_C,
	DVB_C2	= ZAPIT_DS_DVB_C2,
	DVB_S	= ZAPIT_DS_DVB_S,
	DVB_S2	= ZAPIT_DS_DVB_S2,
	DVB_T	= ZAPIT_DS_DVB_T,
	DVB_T2	= ZAPIT_DS_DVB_T2,
	DTMB	= ZAPIT_DS_DTMB,
	DSS	= ZAPIT_DS_DSS,
	TURBO	= ZAPIT_DS_TURBO,
	ISDBS	= ZAPIT_DS_ISDBS,
	ISDBC	= ZAPIT_DS_ISDBC,
	ISDBT	= ZAPIT_DS_ISDBT,
	//
	ALL_SAT = ZAPIT_DM_SAT,
	ALL_CABLE = ZAPIT_DM_CABLE,
	ALL_TERR = ZAPIT_DM_TERR
} delivery_system_t;

typedef enum {
	ZPILOT_ON,
	ZPILOT_OFF,
	ZPILOT_AUTO,
	ZPILOT_AUTO_SW
} zapit_pilot_t;

typedef struct {
	delivery_system_t	delsys;
	uint32_t		frequency;
	fe_modulation_t		modulation;

	fe_spectral_inversion_t	inversion;
	fe_code_rate_t		fec_inner;
	fe_transmit_mode_t	transmission_mode;
	fe_bandwidth_t		bandwidth;
	fe_guard_interval_t	guard_interval;
	fe_hierarchy_t		hierarchy;
	uint32_t		symbol_rate;
	fe_code_rate_t		code_rate_HP;
	fe_code_rate_t		code_rate_LP;

	zapit_pilot_t		pilot;
	fe_rolloff_t		rolloff;

	enum fe_interleaving	interleaving;

	uint8_t			polarization;
} FrontendParameters;

typedef struct frontend_config {
	int diseqcRepeats;
	int diseqcType;
	int uni_scr;
	int uni_qrg;
	int uni_lnb;
	int motorRotationSpeed;
	int highVoltage;
	int diseqc_order;
	int use_usals;
	int rotor_swap;
} frontend_config_t;

#endif // __FRONTEND_TYPES_H__
