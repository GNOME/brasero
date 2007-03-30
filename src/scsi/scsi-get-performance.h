/***************************************************************************
 *            burn-get-performance.h
 *
 *  Fri Oct 20 11:51:05 2006
 *  Copyright  2006  algernon
 *  <algernon@localhost.localdomain>
 ****************************************************************************/

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */

#include <glib.h>

#include "scsi-base.h"

#ifndef _BURN_GET_PERFORMANCE_H
#define _BURN_GET_PERFORMANCE_H

#ifdef __cplusplus
extern "C"
{
#endif

#if G_BYTE_ORDER == G_LITTLE_ENDIAN

struct _BraseroScsiGetPerfHdr {
	uchar len	[4];

	uchar except	:1;
	uchar wrt	:1;
	uchar reserved0	:6;

	uchar reserved1	[3];
};

struct _BraseroScsiWrtSpdDesc {
	uchar mrw	:1;
	uchar exact	:1;
	uchar rdd	:1;
	uchar wrc	:1;
	uchar reserved0	:3;

	uchar reserved1	[3];

	uchar capacity	[4];
	uchar rd_speed	[4];
	uchar wr_speed	[4];
};

#else

struct _BraseroScsiGetPerfHdr {
	uchar len	[4];

	uchar reserved0	:6;
	uchar wrt	:1;
	uchar except	:1;

	uchar reserved1	[3];
};

struct _BraseroScsiWrtSpdDesc {
	uchar reserved0	:3;
	uchar wrc	:1;
	uchar rdd	:1;
	uchar exact	:1;
	uchar mrw	:1;

	uchar reserved1	[3];

	uchar capacity	[4];
	uchar rd_speed	[4];
	uchar wr_speed	[4];
};

#endif

struct _BraseroScsiPerfDesc {
	uchar start_lba	[4];
	uchar start_perf	[4];
	uchar end_lba	[4];
	uchar end_perf	[4];
};

typedef struct _BraseroScsiGetPerfHdr BraseroScsiGetPerfHdr;
typedef struct _BraseroScsiPerfDesc BraseroScsiPerfDesc;
typedef struct _BraseroScsiWrtSpdDesc BraseroScsiWrtSpdDesc;

struct _BraseroScsiGetPerfData {
	BraseroScsiGetPerfHdr hdr;
	gpointer data;			/* depends on the request */
};
typedef struct _BraseroScsiGetPerfData BraseroScsiGetPerfData;

typedef enum {
	BRASERO_SCSI_PERF_READ	= 0,
	BRASERO_SCSI_PERF_WRITE = 1,
	BRASERO_SCSI_PERF_LIST	= 1 << 1,
} BraseroScsiPerfParam;

#ifdef __cplusplus
}
#endif

#endif /* _BURN_GET_PERFORMANCE_H */

 
