/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-media
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-media is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-media authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-media. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-media is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-media is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#include <glib.h>

#include "scsi-base.h"

#ifndef _SCSI_GET_CONFIGURATION_H
#define _SCSI_GET_CONFIGURATION_H

G_BEGIN_DECLS

typedef enum {
BRASERO_SCSI_PROF_EMPTY				= 0x0000,
BRASERO_SCSI_PROF_NON_REMOVABLE		= 0x0001,
BRASERO_SCSI_PROF_REMOVABLE		= 0x0002,
BRASERO_SCSI_PROF_MO_ERASABLE		= 0x0003,
BRASERO_SCSI_PROF_MO_WRITE_ONCE		= 0x0004,
BRASERO_SCSI_PROF_MO_ADVANCED_STORAGE	= 0x0005,
	/* reserved */
BRASERO_SCSI_PROF_CDROM			= 0x0008,
BRASERO_SCSI_PROF_CDR			= 0x0009,
BRASERO_SCSI_PROF_CDRW			= 0x000A,
	/* reserved */
BRASERO_SCSI_PROF_DVD_ROM		= 0x0010,
BRASERO_SCSI_PROF_DVD_R			= 0x0011,
BRASERO_SCSI_PROF_DVD_RAM		= 0x0012,
BRASERO_SCSI_PROF_DVD_RW_RESTRICTED	= 0x0013,
BRASERO_SCSI_PROF_DVD_RW_SEQUENTIAL	= 0x0014,
BRASERO_SCSI_PROF_DVD_R_DL_SEQUENTIAL	= 0x0015,
BRASERO_SCSI_PROF_DVD_R_DL_JUMP		= 0x0016,
	/* reserved */
BRASERO_SCSI_PROF_DVD_RW_PLUS		= 0x001A,
BRASERO_SCSI_PROF_DVD_R_PLUS		= 0x001B,
	/* reserved */
BRASERO_SCSI_PROF_DDCD_ROM		= 0x0020,
BRASERO_SCSI_PROF_DDCD_R		= 0x0021,
BRASERO_SCSI_PROF_DDCD_RW		= 0x0022,
	/* reserved */
BRASERO_SCSI_PROF_DVD_RW_PLUS_DL	= 0x002A,
BRASERO_SCSI_PROF_DVD_R_PLUS_DL		= 0x002B,
	/* reserved */
BRASERO_SCSI_PROF_BD_ROM		= 0x0040,
BRASERO_SCSI_PROF_BR_R_SEQUENTIAL	= 0x0041,
BRASERO_SCSI_PROF_BR_R_RANDOM		= 0x0042,
BRASERO_SCSI_PROF_BD_RW			= 0x0043,
BRASERO_SCSI_PROF_HD_DVD_ROM		= 0x0050,
BRASERO_SCSI_PROF_HD_DVD_R		= 0x0051,
BRASERO_SCSI_PROF_HD_DVD_RAM		= 0x0052,
	/* reserved */
} BraseroScsiProfile;

typedef enum {
BRASERO_SCSI_INTERFACE_NONE		= 0x00000000,
BRASERO_SCSI_INTERFACE_SCSI		= 0x00000001,
BRASERO_SCSI_INTERFACE_ATAPI		= 0x00000002,
BRASERO_SCSI_INTERFACE_FIREWIRE_95	= 0x00000003,
BRASERO_SCSI_INTERFACE_FIREWIRE_A	= 0x00000004,
BRASERO_SCSI_INTERFACE_FCP		= 0x00000005,
BRASERO_SCSI_INTERFACE_FIREWIRE_B	= 0x00000006,
BRASERO_SCSI_INTERFACE_SERIAL_ATAPI	= 0x00000007,
BRASERO_SCSI_INTERFACE_USB		= 0x00000008
} BraseroScsiInterface;

typedef enum {
BRASERO_SCSI_LOADING_CADDY		= 0x000,
BRASERO_SCSI_LOADING_TRAY		= 0x001,
BRASERO_SCSI_LOADING_POPUP		= 0x002,
BRASERO_SCSI_LOADING_EMBED_CHANGER_IND	= 0X004,
BRASERO_SCSI_LOADING_EMBED_CHANGER_MAG	= 0x005
} BraseroScsiLoadingMech;

typedef enum {
BRASERO_SCSI_FEAT_PROFILES		= 0x0000,
BRASERO_SCSI_FEAT_CORE			= 0x0001,
BRASERO_SCSI_FEAT_MORPHING		= 0x0002,
BRASERO_SCSI_FEAT_REMOVABLE		= 0x0003,
BRASERO_SCSI_FEAT_WRT_PROTECT		= 0x0004,
	/* reserved */
BRASERO_SCSI_FEAT_RD_RANDOM		= 0x0010,
	/* reserved */
BRASERO_SCSI_FEAT_RD_MULTI		= 0x001D,
BRASERO_SCSI_FEAT_RD_CD			= 0x001E,
BRASERO_SCSI_FEAT_RD_DVD		= 0x001F,
BRASERO_SCSI_FEAT_WRT_RANDOM		= 0x0020,
BRASERO_SCSI_FEAT_WRT_INCREMENT		= 0x0021,
BRASERO_SCSI_FEAT_WRT_ERASE		= 0x0022,
BRASERO_SCSI_FEAT_WRT_FORMAT		= 0x0023,
BRASERO_SCSI_FEAT_DEFECT_MNGT		= 0x0024,
BRASERO_SCSI_FEAT_WRT_ONCE		= 0x0025,
BRASERO_SCSI_FEAT_RESTRICT_OVERWRT	= 0x0026,
BRASERO_SCSI_FEAT_WRT_CAV_CDRW		= 0x0027,
BRASERO_SCSI_FEAT_MRW			= 0x0028,
BRASERO_SCSI_FEAT_DEFECT_REPORT		= 0x0029,
BRASERO_SCSI_FEAT_WRT_DVDRW_PLUS	= 0x002A,
BRASERO_SCSI_FEAT_WRT_DVDR_PLUS		= 0x002B,
BRASERO_SCSI_FEAT_RIGID_OVERWRT		= 0x002C,
BRASERO_SCSI_FEAT_WRT_TAO		= 0x002D,
BRASERO_SCSI_FEAT_WRT_SAO_RAW		= 0x002E,
BRASERO_SCSI_FEAT_WRT_DVD_LESS		= 0x002F,
BRASERO_SCSI_FEAT_RD_DDCD		= 0x0030,
BRASERO_SCSI_FEAT_WRT_DDCD		= 0x0031,
BRASERO_SCSI_FEAT_RW_DDCD		= 0x0032,
BRASERO_SCSI_FEAT_LAYER_JUMP		= 0x0033,
BRASERO_SCSI_FEAT_WRT_CDRW		= 0x0037,
BRASERO_SCSI_FEAT_BDR_POW		= 0x0038,
	/* reserved */
BRASERO_SCSI_FEAT_WRT_DVDRW_PLUS_DL		= 0x003A,
BRASERO_SCSI_FEAT_WRT_DVDR_PLUS_DL		= 0x003B,
	/* reserved */
BRASERO_SCSI_FEAT_RD_BD			= 0x0040,
BRASERO_SCSI_FEAT_WRT_BD		= 0x0041,
BRASERO_SCSI_FEAT_TSR			= 0x0042,
	/* reserved */
BRASERO_SCSI_FEAT_RD_HDDVD		= 0x0050,
BRASERO_SCSI_FEAT_WRT_HDDVD		= 0x0051,
	/* reserved */
BRASERO_SCSI_FEAT_HYBRID_DISC		= 0x0080,
	/* reserved */
BRASERO_SCSI_FEAT_PWR_MNGT		= 0x0100,
BRASERO_SCSI_FEAT_SMART			= 0x0101,
BRASERO_SCSI_FEAT_EMBED_CHNGR		= 0x0102,
BRASERO_SCSI_FEAT_AUDIO_PLAY		= 0x0103,
BRASERO_SCSI_FEAT_FIRM_UPGRADE		= 0x0104,
BRASERO_SCSI_FEAT_TIMEOUT		= 0x0105,
BRASERO_SCSI_FEAT_DVD_CSS		= 0x0106,
BRASERO_SCSI_FEAT_REAL_TIME_STREAM	= 0x0107,
BRASERO_SCSI_FEAT_DRIVE_SERIAL_NB	= 0x0108,
BRASERO_SCSI_FEAT_MEDIA_SERIAL_NB	= 0x0109,
BRASERO_SCSI_FEAT_DCB			= 0x010A,
BRASERO_SCSI_FEAT_DVD_CPRM		= 0x010B,
BRASERO_SCSI_FEAT_FIRMWARE_INFO		= 0x010C,
BRASERO_SCSI_FEAT_AACS			= 0x010D,
	/* reserved */
BRASERO_SCSI_FEAT_VCPS			= 0x0110,
} BraseroScsiFeatureType;


#if G_BYTE_ORDER == G_LITTLE_ENDIAN

struct _BraseroScsiFeatureDesc {
	uchar code		[2];

	uchar current		:1;
	uchar persistent	:1;
	uchar version		:4;
	uchar reserved		:2;

	uchar add_len;
	uchar data		[0];
};

struct _BraseroScsiCoreDescMMC4 {
	/* this is for MMC4 & MMC5 */
	uchar dbe		:1;
	uchar inq2		:1;
	uchar reserved0		:6;

	uchar reserved1		[3];
};

struct _BraseroScsiCoreDescMMC3 {
	uchar interface		[4];
};

struct _BraseroScsiProfileDesc {
	uchar number		[2];

	uchar currentp		:1;
	uchar reserved0		:7;

	uchar reserved1;
};

struct _BraseroScsiMorphingDesc {
	uchar async		:1;
	uchar op_chge_event	:1;
	uchar reserved0		:6;

	uchar reserved1		[3];
};

struct _BraseroScsiMediumDesc {
	uchar lock		:1;
	uchar reserved		:1;
	uchar prevent_jmp	:1;
	uchar eject		:1;
	uchar reserved1		:1;
	uchar loading_mech	:3;

	uchar reserved2		[3];
};

struct _BraseroScsiWrtProtectDesc {
	uchar sswpp		:1;
	uchar spwp		:1;
	uchar wdcb		:1;
	uchar dwp		:1;
	uchar reserved0		:4;

	uchar reserved1		[3];
};

struct _BraseroScsiRandomReadDesc {
	uchar block_size	[4];
	uchar blocking		[2];

	uchar pp		:1;
	uchar reserved0		:7;

	uchar reserved1;
};

struct _BraseroScsiCDReadDesc {
	uchar cdtext		:1;
	uchar c2flags		:1;
	uchar reserved0		:5;
	uchar dap		:1;

	uchar reserved1		[3];
};

/* MMC5 only otherwise just the header */
struct _BraseroScsiDVDReadDesc {
	uchar multi110		:1;
	uchar reserved0		:7;

	uchar reserved1;

	uchar dual_R		:1;
	uchar reserved2		:7;

	uchar reserved3;
};

struct _BraseroScsiRandomWriteDesc {
	/* MMC4/MMC5 only */
	uchar last_lba		[4];

	uchar block_size	[4];
	uchar blocking		[2];

	uchar pp		:1;
	uchar reserved0		:7;

	uchar reserved1;
};

struct _BraseroScsiIncrementalWrtDesc {
	uchar block_type	[2];

	uchar buf		:1;
	uchar arsv		:1;		/* MMC5 */
	uchar trio		:1;		/* MMC5 */
	uchar reserved0		:5;

	uchar num_link_sizes;
	uchar links		[0];
};

/* MMC5 only */
struct _BraseroScsiFormatDesc {
	uchar cert		:1;
	uchar qcert		:1;
	uchar expand		:1;
	uchar renosa		:1;
	uchar reserved0		:4;

	uchar reserved1		[3];

	uchar rrm		:1;
	uchar reserved2		:7;

	uchar reserved3		[3];
};

struct _BraseroScsiDefectMngDesc {
	uchar reserved0		:7;
	uchar ssa		:1;

	uchar reserved1		[3];
};

struct _BraseroScsiWrtOnceDesc {
	uchar lba_size		[4];
	uchar blocking		[2];

	uchar pp		:1;
	uchar reserved0		:7;

	uchar reserved1;
};

struct _BraseroScsiMRWDesc {
	uchar wrt_CD		:1;
	uchar rd_DVDplus	:1;
	uchar wrt_DVDplus	:1;
	uchar reserved0		:5;

	uchar reserved1		[3];
};

struct _BraseroScsiDefectReportDesc {
	uchar drt_dm		:1;
	uchar reserved0		:7;

	uchar dbi_zones_num;
	uchar num_entries	[2];
};

struct _BraseroScsiDVDRWplusDesc {
	uchar write		:1;
	uchar reserved0		:7;

	uchar close		:1;
	uchar quick_start	:1;
	uchar reserved1		:6;

	uchar reserved2		[2];
};

struct _BraseroScsiDVDRplusDesc {
	uchar write		:1;
	uchar reserved0		:7;

	uchar reserved1		[3];
};

struct _BraseroScsiRigidOverwrtDesc {
	uchar blank		:1;
	uchar intermediate	:1;
	uchar dsdr		:1;
	uchar dsdg		:1;
	uchar reserved0		:4;

	uchar reserved1		[3];
};

struct _BraseroScsiCDTAODesc {
	uchar RW_subcode	:1;
	uchar CDRW		:1;
	uchar dummy		:1;
	uchar RW_pack		:1;
	uchar RW_raw		:1;
	uchar reserved0		:1;
	uchar buf		:1;
	uchar reserved1		:1;

	uchar reserved2;

	uchar data_type		[2];
};

struct _BraseroScsiCDSAODesc {
	uchar rw_sub_chan	:1;
	uchar rw_CD		:1;
	uchar dummy		:1;
	uchar raw		:1;
	uchar raw_multi		:1;
	uchar sao		:1;
	uchar buf		:1;
	uchar reserved		:1;

	uchar max_cue_size	[3];
};

struct _BraseroScsiDVDRWlessWrtDesc {
	uchar reserved0		:1;
	uchar rw_DVD		:1;
	uchar dummy		:1;
	uchar dual_layer_r	:1;
	uchar reserved1		:2;
	uchar buf		:1;
	uchar reserved2		:1;

	uchar reserved3		[3];
};

struct _BraseroScsiCDRWWrtDesc {
	uchar reserved0;

	uchar sub0		:1;
	uchar sub1		:1;
	uchar sub2		:1;
	uchar sub3		:1;
	uchar sub4		:1;
	uchar sub5		:1;
	uchar sub6		:1;
	uchar sub7		:1;

	uchar reserved1		[2];
};

struct _BraseroScsiDVDRWDLDesc {
	uchar write		:1;
	uchar reserved0		:7;

	uchar close		:1;
	uchar quick_start	:1;
	uchar reserved1		:6;

	uchar reserved2		[2];
};

struct _BraseroScsiDVDRDLDesc {
	uchar write		:1;
	uchar reserved0		:7;

	uchar reserved1		[3];
};

struct _BraseroScsiBDReadDesc {
	uchar reserved		[4];

	uchar class0_RE_v8	:1;
	uchar class0_RE_v9	:1;
	uchar class0_RE_v10	:1;
	uchar class0_RE_v11	:1;
	uchar class0_RE_v12	:1;
	uchar class0_RE_v13	:1;
	uchar class0_RE_v14	:1;
	uchar class0_RE_v15	:1;

	uchar class0_RE_v0	:1;
	uchar class0_RE_v1	:1;
	uchar class0_RE_v2	:1;
	uchar class0_RE_v3	:1;
	uchar class0_RE_v4	:1;
	uchar class0_RE_v5	:1;
	uchar class0_RE_v6	:1;
	uchar class0_RE_v7	:1;
	
	uchar class1_RE_v8	:1;
	uchar class1_RE_v9	:1;
	uchar class1_RE_v10	:1;
	uchar class1_RE_v11	:1;
	uchar class1_RE_v12	:1;
	uchar class1_RE_v13	:1;
	uchar class1_RE_v14	:1;
	uchar class1_RE_v15	:1;
	
	uchar class1_RE_v0	:1;
	uchar class1_RE_v1	:1;
	uchar class1_RE_v2	:1;
	uchar class1_RE_v3	:1;
	uchar class1_RE_v4	:1;
	uchar class1_RE_v5	:1;
	uchar class1_RE_v6	:1;
	uchar class1_RE_v7	:1;
	
	uchar class2_RE_v8	:1;
	uchar class2_RE_v9	:1;
	uchar class2_RE_v10	:1;
	uchar class2_RE_v11	:1;
	uchar class2_RE_v12	:1;
	uchar class2_RE_v13	:1;
	uchar class2_RE_v14	:1;
	uchar class2_RE_v15	:1;
	
	uchar class2_RE_v0	:1;
	uchar class2_RE_v1	:1;
	uchar class2_RE_v2	:1;
	uchar class2_RE_v3	:1;
	uchar class2_RE_v4	:1;
	uchar class2_RE_v5	:1;
	uchar class2_RE_v6	:1;
	uchar class2_RE_v7	:1;
	
	uchar class3_RE_v8	:1;
	uchar class3_RE_v9	:1;
	uchar class3_RE_v10	:1;
	uchar class3_RE_v11	:1;
	uchar class3_RE_v12	:1;
	uchar class3_RE_v13	:1;
	uchar class3_RE_v14	:1;
	uchar class3_RE_v15	:1;
	
	uchar class3_RE_v0	:1;
	uchar class3_RE_v1	:1;
	uchar class3_RE_v2	:1;
	uchar class3_RE_v3	:1;
	uchar class3_RE_v4	:1;
	uchar class3_RE_v5	:1;
	uchar class3_RE_v6	:1;
	uchar class3_RE_v7	:1;

	uchar class0_R_v8	:1;
	uchar class0_R_v9	:1;
	uchar class0_R_v10	:1;
	uchar class0_R_v11	:1;
	uchar class0_R_v12	:1;
	uchar class0_R_v13	:1;
	uchar class0_R_v14	:1;
	uchar class0_R_v15	:1;
	
	uchar class0_R_v0	:1;
	uchar class0_R_v1	:1;
	uchar class0_R_v2	:1;
	uchar class0_R_v3	:1;
	uchar class0_R_v4	:1;
	uchar class0_R_v5	:1;
	uchar class0_R_v6	:1;
	uchar class0_R_v7	:1;
	
	uchar class1_R_v8	:1;
	uchar class1_R_v9	:1;
	uchar class1_R_v10	:1;
	uchar class1_R_v11	:1;
	uchar class1_R_v12	:1;
	uchar class1_R_v13	:1;
	uchar class1_R_v14	:1;
	uchar class1_R_v15	:1;
	
	uchar class1_R_v0	:1;
	uchar class1_R_v1	:1;
	uchar class1_R_v2	:1;
	uchar class1_R_v3	:1;
	uchar class1_R_v4	:1;
	uchar class1_R_v5	:1;
	uchar class1_R_v6	:1;
	uchar class1_R_v7	:1;
	
	uchar class2_R_v8	:1;
	uchar class2_R_v9	:1;
	uchar class2_R_v10	:1;
	uchar class2_R_v11	:1;
	uchar class2_R_v12	:1;
	uchar class2_R_v13	:1;
	uchar class2_R_v14	:1;
	uchar class2_R_v15	:1;
	
	uchar class2_R_v0	:1;
	uchar class2_R_v1	:1;
	uchar class2_R_v2	:1;
	uchar class2_R_v3	:1;
	uchar class2_R_v4	:1;
	uchar class2_R_v5	:1;
	uchar class2_R_v6	:1;
	uchar class2_R_v7	:1;
	
	uchar class3_R_v8	:1;
	uchar class3_R_v9	:1;
	uchar class3_R_v10	:1;
	uchar class3_R_v11	:1;
	uchar class3_R_v12	:1;
	uchar class3_R_v13	:1;
	uchar class3_R_v14	:1;
	uchar class3_R_v15	:1;
	
	uchar class3_R_v0	:1;
	uchar class3_R_v1	:1;
	uchar class3_R_v2	:1;
	uchar class3_R_v3	:1;
	uchar class3_R_v4	:1;
	uchar class3_R_v5	:1;
	uchar class3_R_v6	:1;
	uchar class3_R_v7	:1;
};

struct _BraseroScsiBDWriteDesc {
	uchar reserved		[4];

	uchar class0_RE_v8	:1;
	uchar class0_RE_v9	:1;
	uchar class0_RE_v10	:1;
	uchar class0_RE_v11	:1;
	uchar class0_RE_v12	:1;
	uchar class0_RE_v13	:1;
	uchar class0_RE_v14	:1;
	uchar class0_RE_v15	:1;
	
	uchar class0_RE_v0	:1;
	uchar class0_RE_v1	:1;
	uchar class0_RE_v2	:1;
	uchar class0_RE_v3	:1;
	uchar class0_RE_v4	:1;
	uchar class0_RE_v5	:1;
	uchar class0_RE_v6	:1;
	uchar class0_RE_v7	:1;
	
	uchar class1_RE_v8	:1;
	uchar class1_RE_v9	:1;
	uchar class1_RE_v10	:1;
	uchar class1_RE_v11	:1;
	uchar class1_RE_v12	:1;
	uchar class1_RE_v13	:1;
	uchar class1_RE_v14	:1;
	uchar class1_RE_v15	:1;
	
	uchar class1_RE_v0	:1;
	uchar class1_RE_v1	:1;
	uchar class1_RE_v2	:1;
	uchar class1_RE_v3	:1;
	uchar class1_RE_v4	:1;
	uchar class1_RE_v5	:1;
	uchar class1_RE_v6	:1;
	uchar class1_RE_v7	:1;
	
	uchar class2_RE_v8	:1;
	uchar class2_RE_v9	:1;
	uchar class2_RE_v10	:1;
	uchar class2_RE_v11	:1;
	uchar class2_RE_v12	:1;
	uchar class2_RE_v13	:1;
	uchar class2_RE_v14	:1;
	uchar class2_RE_v15	:1;
	
	uchar class2_RE_v0	:1;
	uchar class2_RE_v1	:1;
	uchar class2_RE_v2	:1;
	uchar class2_RE_v3	:1;
	uchar class2_RE_v4	:1;
	uchar class2_RE_v5	:1;
	uchar class2_RE_v6	:1;
	uchar class2_RE_v7	:1;
	
	uchar class3_RE_v8	:1;
	uchar class3_RE_v9	:1;
	uchar class3_RE_v10	:1;
	uchar class3_RE_v11	:1;
	uchar class3_RE_v12	:1;
	uchar class3_RE_v13	:1;
	uchar class3_RE_v14	:1;
	uchar class3_RE_v15	:1;
	
	uchar class3_RE_v0	:1;
	uchar class3_RE_v1	:1;
	uchar class3_RE_v2	:1;
	uchar class3_RE_v3	:1;
	uchar class3_RE_v4	:1;
	uchar class3_RE_v5	:1;
	uchar class3_RE_v6	:1;
	uchar class3_RE_v7	:1;

	uchar class0_R_v8	:1;
	uchar class0_R_v9	:1;
	uchar class0_R_v10	:1;
	uchar class0_R_v11	:1;
	uchar class0_R_v12	:1;
	uchar class0_R_v13	:1;
	uchar class0_R_v14	:1;
	uchar class0_R_v15	:1;
	
	uchar class0_R_v0	:1;
	uchar class0_R_v1	:1;
	uchar class0_R_v2	:1;
	uchar class0_R_v3	:1;
	uchar class0_R_v4	:1;
	uchar class0_R_v5	:1;
	uchar class0_R_v6	:1;
	uchar class0_R_v7	:1;
	
	uchar class1_R_v8	:1;
	uchar class1_R_v9	:1;
	uchar class1_R_v10	:1;
	uchar class1_R_v11	:1;
	uchar class1_R_v12	:1;
	uchar class1_R_v13	:1;
	uchar class1_R_v14	:1;
	uchar class1_R_v15	:1;
	
	uchar class1_R_v0	:1;
	uchar class1_R_v1	:1;
	uchar class1_R_v2	:1;
	uchar class1_R_v3	:1;
	uchar class1_R_v4	:1;
	uchar class1_R_v5	:1;
	uchar class1_R_v6	:1;
	uchar class1_R_v7	:1;
	
	uchar class2_R_v8	:1;
	uchar class2_R_v9	:1;
	uchar class2_R_v10	:1;
	uchar class2_R_v11	:1;
	uchar class2_R_v12	:1;
	uchar class2_R_v13	:1;
	uchar class2_R_v14	:1;
	uchar class2_R_v15	:1;
	
	uchar class2_R_v0	:1;
	uchar class2_R_v1	:1;
	uchar class2_R_v2	:1;
	uchar class2_R_v3	:1;
	uchar class2_R_v4	:1;
	uchar class2_R_v5	:1;
	uchar class2_R_v6	:1;
	uchar class2_R_v7	:1;
	
	uchar class3_R_v8	:1;
	uchar class3_R_v9	:1;
	uchar class3_R_v10	:1;
	uchar class3_R_v11	:1;
	uchar class3_R_v12	:1;
	uchar class3_R_v13	:1;
	uchar class3_R_v14	:1;
	uchar class3_R_v15	:1;
	
	uchar class3_R_v0	:1;
	uchar class3_R_v1	:1;
	uchar class3_R_v2	:1;
	uchar class3_R_v3	:1;
	uchar class3_R_v4	:1;
	uchar class3_R_v5	:1;
	uchar class3_R_v6	:1;
	uchar class3_R_v7	:1;
};

struct _BraseroScsiHDDVDReadDesc {
	uchar hd_dvd_r		:1;
	uchar reserved0		:7;

	uchar reserved1;

	uchar hd_dvd_ram	:1;
	uchar reserved2		:7;

	uchar reserved3;
};

struct _BraseroScsiHDDVDWriteDesc {
	uchar hd_dvd_r		:1;
	uchar reserved0		:7;

	uchar reserved1;

	uchar hd_dvd_ram	:1;
	uchar reserved2		:7;

	uchar reserved3;
};

struct _BraseroScsiHybridDiscDesc {
	uchar ri		:1;
	uchar reserved0		:7;

	uchar reserved1		[3];
};

struct _BraseroScsiSmartDesc {
	uchar pp		:1;
	uchar reserved0		:7;

	uchar reserved1		[3];
};

struct _BraseroScsiEmbedChngDesc {
	uchar reserved0		:1;
	uchar sdp		:1;
	uchar reserved1		:1;
	uchar scc		:1;
	uchar reserved2		:3;

	uchar reserved3		[2];

	uchar slot_num		:5;
	uchar reserved4		:3;
};

struct _BraseroScsiExtAudioPlayDesc {
	uchar separate_vol	:1;
	uchar separate_chnl_mute:1;
	uchar scan_command	:1;
	uchar reserved0		:5;

	uchar reserved1;

	uchar number_vol	[2];
};

struct _BraseroScsiFirmwareUpgrDesc {
	uchar m5		:1;
	uchar reserved0		:7;

	uchar reserved1		[3];
};

struct _BraseroScsiTimeoutDesc {
	uchar group3		:1;
	uchar reserved0		:7;

	uchar reserved1;
	uchar unit_len		[2];
};

struct _BraseroScsiRTStreamDesc {
	uchar stream_wrt	:1;
	uchar wrt_spd		:1;
	uchar mp2a		:1;
	uchar set_cd_spd	:1;
	uchar rd_buf_caps_block	:1;
	uchar reserved0		:3;

	uchar reserved1		[3];
};

struct _BraseroScsiAACSDesc {
	uchar bng		:1;
	uchar reserved0		:7;

	uchar block_count;

	uchar agids_num		:4;
	uchar reserved1		:4;

	uchar version;
};

#else

struct _BraseroScsiFeatureDesc {
	uchar code		[2];

	uchar current		:1;
	uchar persistent	:1;
	uchar version		:4;
	uchar reserved		:2;

	uchar add_len;
	uchar data		[0];
};

struct _BraseroScsiProfileDesc {
	uchar number		[2];

	uchar reserved0		:7;
	uchar currentp		:1;

	uchar reserved1;
};

struct _BraseroScsiCoreDescMMC4 {
	uchar reserved0		:6;
	uchar inq2		:1;
	uchar dbe		:1;

  	uchar mmc4		[0];
	uchar reserved1		[3];
};

struct _BraseroScsiCoreDescMMC3 {
	uchar interface		[4];
};

struct _BraseroScsiMorphingDesc {
	uchar reserved0		:6;
	uchar op_chge_event	:1;
	uchar async		:1;

	uchar reserved1		[3];
};

struct _BraseroScsiMediumDesc {
	uchar loading_mech	:3;
	uchar reserved1		:1;
	uchar eject		:1;
	uchar prevent_jmp	:1;
	uchar reserved		:1;
	uchar lock		:1;

	uchar reserved2		[3];
};

struct _BraseroScsiWrtProtectDesc {
	uchar reserved0		:4;
	uchar dwp		:1;
	uchar wdcb		:1;
	uchar spwp		:1;
	uchar sswpp		:1;

	uchar reserved1		[3];
};

struct _BraseroScsiRandomReadDesc {
	uchar block_size	[4];
	uchar blocking		[2];

	uchar reserved0		:7;
	uchar pp		:1;

	uchar reserved1;
};

struct _BraseroScsiCDReadDesc {
	uchar dap		:1;
	uchar reserved0		:5;
	uchar c2flags		:1;
	uchar cdtext		:1;

	uchar reserved1		[3];
};

struct _BraseroScsiDVDReadDesc {
	uchar reserved0		:7;
	uchar multi110		:1;

	uchar reserved1;

	uchar reserved2		:7;
	uchar dual_R		:1;

	uchar reserved3;
};

struct _BraseroScsiRandomWriteDesc {
	uchar last_lba		[4];
	uchar block_size	[4];
	uchar blocking		[2];

	uchar reserved0		:7;
	uchar pp		:1;

	uchar reserved1;
};

struct _BraseroScsiIncrementalWrtDesc {
	uchar block_type	[2];

	uchar reserved0		:5;
	uchar trio		:1;
	uchar arsv		:1;
	uchar buf		:1;

	uchar num_link_sizes;
	uchar links;
};

struct _BraseroScsiFormatDesc {
	uchar reserved0		:4;
	uchar renosa		:1;
	uchar expand		:1;
	uchar qcert		:1;
	uchar cert		:1;

	uchar reserved1		[3];

	uchar reserved2		:7;
	uchar rrm		:1;

	uchar reserved3		[3];
};

struct _BraseroScsiDefectMngDesc {
	uchar ssa		:1;
	uchar reserved0		:7;

	uchar reserved1		[3];
};

struct _BraseroScsiWrtOnceDesc {
	uchar lba_size		[4];
	uchar blocking		[2];

	uchar reserved0		:7;
	uchar pp		:1;

	uchar reserved1;
};

struct _BraseroScsiMRWDesc {
	uchar reserved0		:5;
	uchar wrt_DVDplus	:1;
	uchar rd_DVDplus	:1;
	uchar wrt_CD		:1;

	uchar reserved1		[3];
};

struct _BraseroScsiDefectReportDesc {
	uchar reserved0		:7;
	uchar drt_dm		:1;

	uchar dbi_zones_num;
	uchar num_entries	[2];
};

struct _BraseroScsiDVDRWplusDesc {
	uchar reserved0		:7;
	uchar write		:1;

	uchar reserved1		:6;
	uchar quick_start	:1;
	uchar close		:1;

	uchar reserved2		[2];
};

struct _BraseroScsiDVDRplusDesc {
	uchar reserved0		:7;
	uchar write		:1;

	uchar reserved1		[3];
};

struct _BraseroScsiRigidOverwrtDesc {
	uchar reserved0		:4;
	uchar dsdg		:1;
	uchar dsdr		:1;
	uchar intermediate	:1;
	uchar blank		:1;

	uchar reserved1		[3];
};

struct _BraseroScsiCDTAODesc {
	uchar reserved1		:1;
	uchar buf		:1;
	uchar reserved0		:1;
	uchar RW_raw		:1;
	uchar RW_pack		:1;
	uchar dummy		:1;
	uchar CDRW		:1;
	uchar RW_subcode	:1;

	uchar reserved2;

	uchar data_type		[2];
};

struct _BraseroScsiCDSAODesc {
	uchar reserved		:1;
	uchar buf		:1;
	uchar sao		:1;
	uchar raw_multi		:1;
	uchar raw		:1;
	uchar dummy		:1;
	uchar rw_CD		:1;
	uchar rw_sub_chan	:1;

	uchar max_cue_size	[3];
};

struct _BraseroScsiDVDRWlessWrtDesc {
	uchar reserved2		:1;
	uchar buf		:1;
	uchar reserved1		:2;
	uchar dual_layer_r	:1;
	uchar dummy		:1;
	uchar rw_DVD		:1;
	uchar reserved0		:1;

	uchar reserved3		[3];
};

struct _BraseroScsiCDRWWrtDesc {
	uchar reserved0;

	uchar sub7		:1;
	uchar sub6		:1;
	uchar sub5		:1;
	uchar sub4		:1;
	uchar sub3		:1;
	uchar sub2		:1;
	uchar sub1		:1;
	uchar sub0		:1;

	uchar reserved1		[2];
};

struct _BraseroScsiDVDRWDLDesc {
	uchar reserved0		:7;
	uchar write		:1;

	uchar reserved1		:6;
	uchar quick_start	:1;
	uchar close		:1;

	uchar reserved2		[2];
};

struct _BraseroScsiDVDRDLDesc {
	uchar reserved0		:7;
	uchar write		:1;

	uchar reserved1		[3];
};

struct _BraseroScsiBDReadDesc {
	uchar reserved		[4];

	uchar class0_RE_v15	:1;
	uchar class0_RE_v14	:1;
	uchar class0_RE_v13	:1;
	uchar class0_RE_v12	:1;
	uchar class0_RE_v11	:1;
	uchar class0_RE_v10	:1;
	uchar class0_RE_v9	:1;
	uchar class0_RE_v8	:1;

	uchar class0_RE_v7	:1;
	uchar class0_RE_v6	:1;
	uchar class0_RE_v5	:1;
	uchar class0_RE_v4	:1;
	uchar class0_RE_v3	:1;
	uchar class0_RE_v2	:1;
	uchar class0_RE_v1	:1;
	uchar class0_RE_v0	:1;

	uchar class1_RE_v15	:1;
	uchar class1_RE_v14	:1;
	uchar class1_RE_v13	:1;
	uchar class1_RE_v12	:1;
	uchar class1_RE_v11	:1;
	uchar class1_RE_v10	:1;
	uchar class1_RE_v9	:1;
	uchar class1_RE_v8	:1;
	
	uchar class1_RE_v7	:1;
	uchar class1_RE_v6	:1;
	uchar class1_RE_v5	:1;
	uchar class1_RE_v4	:1;
	uchar class1_RE_v3	:1;
	uchar class1_RE_v2	:1;
	uchar class1_RE_v1	:1;
	uchar class1_RE_v0	:1;
	
	uchar class2_RE_v15	:1;
	uchar class2_RE_v14	:1;
	uchar class2_RE_v13	:1;
	uchar class2_RE_v12	:1;
	uchar class2_RE_v11	:1;
	uchar class2_RE_v10	:1;
	uchar class2_RE_v9	:1;
	uchar class2_RE_v8	:1;
	
	uchar class2_RE_v7	:1;
	uchar class2_RE_v6	:1;
	uchar class2_RE_v5	:1;
	uchar class2_RE_v4	:1;
	uchar class2_RE_v3	:1;
	uchar class2_RE_v2	:1;
	uchar class2_RE_v1	:1;
	uchar class2_RE_v0	:1;
	
	uchar class3_RE_v15	:1;
	uchar class3_RE_v14	:1;
	uchar class3_RE_v13	:1;
	uchar class3_RE_v12	:1;
	uchar class3_RE_v11	:1;
	uchar class3_RE_v10	:1;
	uchar class3_RE_v9	:1;
	uchar class3_RE_v8	:1;
	
	uchar class3_RE_v7	:1;
	uchar class3_RE_v6	:1;
	uchar class3_RE_v5	:1;
	uchar class3_RE_v4	:1;
	uchar class3_RE_v3	:1;
	uchar class3_RE_v2	:1;
	uchar class3_RE_v1	:1;
	uchar class3_RE_v0	:1;

	uchar class0_R_v15	:1;
	uchar class0_R_v14	:1;
	uchar class0_R_v13	:1;
	uchar class0_R_v12	:1;
	uchar class0_R_v11	:1;
	uchar class0_R_v10	:1;
	uchar class0_R_v9	:1;
	uchar class0_R_v8	:1;

	uchar class0_R_v7	:1;
	uchar class0_R_v6	:1;
	uchar class0_R_v5	:1;
	uchar class0_R_v4	:1;
	uchar class0_R_v3	:1;
	uchar class0_R_v2	:1;
	uchar class0_R_v1	:1;
	uchar class0_R_v0	:1;

	uchar class1_R_v15	:1;
	uchar class1_R_v14	:1;
	uchar class1_R_v13	:1;
	uchar class1_R_v12	:1;
	uchar class1_R_v11	:1;
	uchar class1_R_v10	:1;
	uchar class1_R_v9	:1;
	uchar class1_R_v8	:1;
	
	uchar class1_R_v7	:1;
	uchar class1_R_v6	:1;
	uchar class1_R_v5	:1;
	uchar class1_R_v4	:1;
	uchar class1_R_v3	:1;
	uchar class1_R_v2	:1;
	uchar class1_R_v1	:1;
	uchar class1_R_v0	:1;
	
	uchar class2_R_v15	:1;
	uchar class2_R_v14	:1;
	uchar class2_R_v13	:1;
	uchar class2_R_v12	:1;
	uchar class2_R_v11	:1;
	uchar class2_R_v10	:1;
	uchar class2_R_v9	:1;
	uchar class2_R_v8	:1;
	
	uchar class2_R_v7	:1;
	uchar class2_R_v6	:1;
	uchar class2_R_v5	:1;
	uchar class2_R_v4	:1;
	uchar class2_R_v3	:1;
	uchar class2_R_v2	:1;
	uchar class2_R_v1	:1;
	uchar class2_R_v0	:1;
	
	uchar class3_R_v15	:1;
	uchar class3_R_v14	:1;
	uchar class3_R_v13	:1;
	uchar class3_R_v12	:1;
	uchar class3_R_v11	:1;
	uchar class3_R_v10	:1;
	uchar class3_R_v9	:1;
	uchar class3_R_v8	:1;
	
	uchar class3_R_v7	:1;
	uchar class3_R_v6	:1;
	uchar class3_R_v5	:1;
	uchar class3_R_v4	:1;
	uchar class3_R_v3	:1;
	uchar class3_R_v2	:1;
	uchar class3_R_v1	:1;
	uchar class3_R_v0	:1;
};

struct _BraseroScsiBDWriteDesc {
	uchar reserved		[4];

	uchar class0_RE_v15	:1;
	uchar class0_RE_v14	:1;
	uchar class0_RE_v13	:1;
	uchar class0_RE_v12	:1;
	uchar class0_RE_v11	:1;
	uchar class0_RE_v10	:1;
	uchar class0_RE_v9	:1;
	uchar class0_RE_v8	:1;

	uchar class0_RE_v7	:1;
	uchar class0_RE_v6	:1;
	uchar class0_RE_v5	:1;
	uchar class0_RE_v4	:1;
	uchar class0_RE_v3	:1;
	uchar class0_RE_v2	:1;
	uchar class0_RE_v1	:1;
	uchar class0_RE_v0	:1;

	uchar class1_RE_v15	:1;
	uchar class1_RE_v14	:1;
	uchar class1_RE_v13	:1;
	uchar class1_RE_v12	:1;
	uchar class1_RE_v11	:1;
	uchar class1_RE_v10	:1;
	uchar class1_RE_v9	:1;
	uchar class1_RE_v8	:1;
	
	uchar class1_RE_v7	:1;
	uchar class1_RE_v6	:1;
	uchar class1_RE_v5	:1;
	uchar class1_RE_v4	:1;
	uchar class1_RE_v3	:1;
	uchar class1_RE_v2	:1;
	uchar class1_RE_v1	:1;
	uchar class1_RE_v0	:1;
	
	uchar class2_RE_v15	:1;
	uchar class2_RE_v14	:1;
	uchar class2_RE_v13	:1;
	uchar class2_RE_v12	:1;
	uchar class2_RE_v11	:1;
	uchar class2_RE_v10	:1;
	uchar class2_RE_v9	:1;
	uchar class2_RE_v8	:1;
	
	uchar class2_RE_v7	:1;
	uchar class2_RE_v6	:1;
	uchar class2_RE_v5	:1;
	uchar class2_RE_v4	:1;
	uchar class2_RE_v3	:1;
	uchar class2_RE_v2	:1;
	uchar class2_RE_v1	:1;
	uchar class2_RE_v0	:1;
	
	uchar class3_RE_v15	:1;
	uchar class3_RE_v14	:1;
	uchar class3_RE_v13	:1;
	uchar class3_RE_v12	:1;
	uchar class3_RE_v11	:1;
	uchar class3_RE_v10	:1;
	uchar class3_RE_v9	:1;
	uchar class3_RE_v8	:1;
	
	uchar class3_RE_v7	:1;
	uchar class3_RE_v6	:1;
	uchar class3_RE_v5	:1;
	uchar class3_RE_v4	:1;
	uchar class3_RE_v3	:1;
	uchar class3_RE_v2	:1;
	uchar class3_RE_v1	:1;
	uchar class3_RE_v0	:1;

	uchar class0_R_v15	:1;
	uchar class0_R_v14	:1;
	uchar class0_R_v13	:1;
	uchar class0_R_v12	:1;
	uchar class0_R_v11	:1;
	uchar class0_R_v10	:1;
	uchar class0_R_v9	:1;
	uchar class0_R_v8	:1;

	uchar class0_R_v7	:1;
	uchar class0_R_v6	:1;
	uchar class0_R_v5	:1;
	uchar class0_R_v4	:1;
	uchar class0_R_v3	:1;
	uchar class0_R_v2	:1;
	uchar class0_R_v1	:1;
	uchar class0_R_v0	:1;

	uchar class1_R_v15	:1;
	uchar class1_R_v14	:1;
	uchar class1_R_v13	:1;
	uchar class1_R_v12	:1;
	uchar class1_R_v11	:1;
	uchar class1_R_v10	:1;
	uchar class1_R_v9	:1;
	uchar class1_R_v8	:1;
	
	uchar class1_R_v7	:1;
	uchar class1_R_v6	:1;
	uchar class1_R_v5	:1;
	uchar class1_R_v4	:1;
	uchar class1_R_v3	:1;
	uchar class1_R_v2	:1;
	uchar class1_R_v1	:1;
	uchar class1_R_v0	:1;
	
	uchar class2_R_v15	:1;
	uchar class2_R_v14	:1;
	uchar class2_R_v13	:1;
	uchar class2_R_v12	:1;
	uchar class2_R_v11	:1;
	uchar class2_R_v10	:1;
	uchar class2_R_v9	:1;
	uchar class2_R_v8	:1;
	
	uchar class2_R_v7	:1;
	uchar class2_R_v6	:1;
	uchar class2_R_v5	:1;
	uchar class2_R_v4	:1;
	uchar class2_R_v3	:1;
	uchar class2_R_v2	:1;
	uchar class2_R_v1	:1;
	uchar class2_R_v0	:1;
	
	uchar class3_R_v15	:1;
	uchar class3_R_v14	:1;
	uchar class3_R_v13	:1;
	uchar class3_R_v12	:1;
	uchar class3_R_v11	:1;
	uchar class3_R_v10	:1;
	uchar class3_R_v9	:1;
	uchar class3_R_v8	:1;
	
	uchar class3_R_v7	:1;
	uchar class3_R_v6	:1;
	uchar class3_R_v5	:1;
	uchar class3_R_v4	:1;
	uchar class3_R_v3	:1;
	uchar class3_R_v2	:1;
	uchar class3_R_v1	:1;
	uchar class3_R_v0	:1;
};

struct _BraseroScsiHDDVDReadDesc {
	uchar reserved0		:7;
	uchar hd_dvd_r		:1;

	uchar reserved1;

	uchar reserved2		:7;
	uchar hd_dvd_ram	:1;

	uchar reserved3;
};

struct _BraseroScsiHDDVDWriteDesc {
	uchar reserved0		:7;
	uchar hd_dvd_r		:1;

	uchar reserved1;

	uchar reserved2		:7;
	uchar hd_dvd_ram	:1;

	uchar reserved3;
};

struct _BraseroScsiHybridDiscDesc {
	uchar reserved0		:7;
	uchar ri		:1;

	uchar reserved1		[3];
};

struct _BraseroScsiSmartDesc {
	uchar reserved0		:7;
	uchar pp		:1;

	uchar reserved1		[3];
};

struct _BraseroScsiEmbedChngDesc {
	uchar reserved2		:3;
	uchar scc		:1;
	uchar reserved1		:1;
	uchar sdp		:1;
	uchar reserved0		:1;

	uchar reserved3		[2];

	uchar reserved4		:3;
	uchar slot_num		:5;
};

struct _BraseroScsiExtAudioPlayDesc {
	uchar reserved0		:5;
	uchar scan_command	:1;
	uchar separate_chnl_mute:1;
	uchar separate_vol	:1;

	uchar reserved1;

	uchar number_vol	[2];
};

struct _BraseroScsiFirmwareUpgrDesc {
	uchar reserved0		:7;
	uchar m5		:1;

	uchar reserved1		[3];
};

struct _BraseroScsiTimeoutDesc {
	uchar reserved0		:7;
	uchar group3		:1;

	uchar reserved1;
	uchar unit_len		[2];
};

struct _BraseroScsiRTStreamDesc {
	uchar reserved0		:3;
	uchar rd_buf_caps_block	:1;
	uchar set_cd_spd	:1;
	uchar mp2a		:1;
	uchar wrt_spd		:1;
	uchar stream_wrt	:1;

	uchar reserved1		[3];
};

struct _BraseroScsiAACSDesc {
	uchar reserved0		:7;
	uchar bng		:1;

	uchar block_count;

	uchar reserved1		:4;
	uchar agids_num		:4;

	uchar version;
};

#endif

struct _BraseroScsiInterfaceDesc {
	uchar code		[4];
};

struct _BraseroScsiCDrwCavDesc {
	uchar reserved		[4];
};

/* NOTE: this structure is extendable with padding to have a multiple of 4 */
struct _BraseroScsiLayerJmpDesc {
	uchar reserved0		[3];
	uchar num_link_sizes;
	uchar links		[0];
};

struct _BraseroScsiPOWDesc {
	uchar reserved		[4];
};

struct _BraseroScsiDVDCssDesc {
	uchar reserved		[3];
	uchar version;
};

/* NOTE: this structure is extendable with padding to have a multiple of 4 */
struct _BraseroScsiDriveSerialNumDesc {
	uchar serial		[4];
};

struct _BraseroScsiMediaSerialNumDesc {
	uchar serial		[4];
};

/* NOTE: this structure is extendable with padding to have a multiple of 4 */
struct _BraseroScsiDiscCtlBlocksDesc {
	uchar entry		[1][4];
};

struct _BraseroScsiDVDCprmDesc {
	uchar reserved0 	[3];
	uchar version;
};

struct _BraseroScsiFirmwareDesc {
	uchar century		[2];
	uchar year		[2];
	uchar month		[2];
	uchar day		[2];
	uchar hour		[2];
	uchar minute		[2];
	uchar second		[2];
	uchar reserved		[2];
};

struct _BraseroScsiVPSDesc {
	uchar reserved		[4];
};

typedef struct _BraseroScsiFeatureDesc BraseroScsiFeatureDesc;
typedef struct _BraseroScsiProfileDesc BraseroScsiProfileDesc;
typedef struct _BraseroScsiCoreDescMMC3 BraseroScsiCoreDescMMC3;
typedef struct _BraseroScsiCoreDescMMC4 BraseroScsiCoreDescMMC4;
typedef struct _BraseroScsiInterfaceDesc BraseroScsiInterfaceDesc;
typedef struct _BraseroScsiMorphingDesc BraseroScsiMorphingDesc;
typedef struct _BraseroScsiMediumDesc BraseroScsiMediumDesc;
typedef struct _BraseroScsiWrtProtectDesc BraseroScsiWrtProtectDesc;
typedef struct _BraseroScsiRandomReadDesc BraseroScsiRandomReadDesc;
typedef struct _BraseroScsiCDReadDesc BraseroScsiCDReadDesc;
typedef struct _BraseroScsiDVDReadDesc BraseroScsiDVDReadDesc;
typedef struct _BraseroScsiRandomWriteDesc BraseroScsiRandomWriteDesc;
typedef struct _BraseroScsiIncrementalWrtDesc BraseroScsiIncrementalWrtDesc;
typedef struct _BraseroScsiFormatDesc BraseroScsiFormatDesc;
typedef struct _BraseroScsiDefectMngDesc BraseroScsiDefectMngDesc;
typedef struct _BraseroScsiWrtOnceDesc BraseroScsiWrtOnceDesc;
typedef struct _BraseroScsiCDrwCavDesc BraseroScsiCDrwCavDesc;
typedef struct _BraseroScsiMRWDesc BraseroScsiMRWDesc;
typedef struct _BraseroScsiDefectReportDesc BraseroScsiDefectReportDesc;
typedef struct _BraseroScsiDVDRWplusDesc BraseroScsiDVDRWplusDesc;
typedef struct _BraseroScsiDVDRplusDesc BraseroScsiDVDRplusDesc;
typedef struct _BraseroScsiRigidOverwrtDesc BraseroScsiRigidOverwrtDesc;
typedef struct _BraseroScsiCDTAODesc BraseroScsiCDTAODesc;
typedef struct _BraseroScsiCDSAODesc BraseroScsiCDSAODesc;
typedef struct _BraseroScsiDVDRWlessWrtDesc BraseroScsiDVDRWlessWrtDesc;
typedef struct _BraseroScsiLayerJmpDesc BraseroScsiLayerJmpDesc;
typedef struct _BraseroScsiCDRWWrtDesc BraseroScsiCDRWWrtDesc;
typedef struct _BraseroScsiDVDRWDLDesc BraseroScsiDVDRWDLDesc;
typedef struct _BraseroScsiDVDRDLDesc BraseroScsiDVDRDLDesc;
typedef struct _BraseroScsiBDReadDesc BraseroScsiBDReadDesc;
typedef struct _BraseroScsiBDWriteDesc BraseroScsiBDWriteDesc;
typedef struct _BraseroScsiHDDVDReadDesc BraseroScsiHDDVDReadDesc;
typedef struct _BraseroScsiHDDVDWriteDesc BraseroScsiHDDVDWriteDesc;
typedef struct _BraseroScsiHybridDiscDesc BraseroScsiHybridDiscDesc;
typedef struct _BraseroScsiSmartDesc BraseroScsiSmartDesc;
typedef struct _BraseroScsiEmbedChngDesc BraseroScsiEmbedChngDesc;
typedef struct _BraseroScsiExtAudioPlayDesc BraseroScsiExtAudioPlayDesc;
typedef struct _BraseroScsiFirmwareUpgrDesc BraseroScsiFirmwareUpgrDesc;
typedef struct _BraseroScsiTimeoutDesc BraseroScsiTimeoutDesc;
typedef struct _BraseroScsiRTStreamDesc BraseroScsiRTStreamDesc;
typedef struct _BraseroScsiAACSDesc BraseroScsiAACSDesc;
typedef struct _BraseroScsiPOWDesc BraseroScsiPOWDesc;
typedef struct _BraseroScsiDVDCssDesc BraseroScsiDVDCssDesc;
typedef struct _BraseroScsiDriveSerialNumDesc BraseroScsiDriveSerialNumDesc;
typedef struct _BraseroScsiMediaSerialNumDesc BraseroScsiMediaSerialNumDesc;
typedef struct _BraseroScsiDiscCtlBlocksDesc BraseroScsiDiscCtlBlocksDesc;
typedef struct _BraseroScsiDVDCprmDesc BraseroScsiDVDCprmDesc;
typedef struct _BraseroScsiFirmwareDesc BraseroScsiFirmwareDesc;
typedef struct _BraseroScsiVPSDesc BraseroScsiVPSDesc;

struct _BraseroScsiGetConfigHdr {
	uchar len			[4];
	uchar reserved			[2];
	uchar current_profile		[2];

	BraseroScsiFeatureDesc desc 	[0];
};
typedef struct _BraseroScsiGetConfigHdr BraseroScsiGetConfigHdr;

G_END_DECLS

#endif /* _SCSI_GET_CONFIGURATION_H */

 
