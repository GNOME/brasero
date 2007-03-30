/***************************************************************************
 *            burn-get-performance.c
 *
 *  Thu Oct 19 16:35:28 2006
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

#include <fcntl.h>
#include <glib.h>

#include "scsi-error.h"
#include "scsi-utils.h"
#include "scsi-command.h"
#include "scsi-opcodes.h"
#include "scsi-get-performance.h"

/**
 * GET PERMORMANCE command description	(MMC2)
 */

#if G_BYTE_ORDER == G_LITTLE_ENDIAN

struct _BraseroGetPerformanceCDB {
	uchar opcode		:8;

	uchar except		:2;
	uchar write		:1;
	uchar tolerance		:2;
	uchar reserved0		:3;

	uchar start_lba		[4];

	uchar reserved1		[2];

	uchar max_desc		[2];

	uchar type;
	uchar ctl;
};

#else

struct _BraseroGetPerformanceCDB {
	uchar opcode		:8;

	uchar reserved0		:3;
	uchar tolerance		:2;
	uchar write		:1;
	uchar except		:2;

	uchar start_lba		[4];

	uchar reserved1		[2];

	uchar max_desc		[2];

	uchar type;
	uchar ctl;
};

#endif

typedef struct _BraseroGetPerformanceCDB BraseroGetPerformanceCDB;

BRASERO_SCSI_COMMAND_DEFINE (BraseroGetPerformanceCDB,
			     GET_PERFORMANCE,
			     O_RDONLY,
			     BRASERO_SCSI_READ);

/* used to choose which GET PERFORMANCE response we want */
#define BRASERO_GET_PERFORMANCE_PERF_TYPE		0x00
#define BRASERO_GET_PERFORMANCE_UNUSABLE_AREA_TYPE	0x01
#define BRASERO_GET_PERFORMANCE_DEFECT_STATUS_TYPE	0x02
#define BRASERO_GET_PERFORMANCE_WR_SPEED_TYPE		0x03
#define BRASERO_GET_PERFORMANCE_DBI_TYPE		0x04
#define BRASERO_GET_PERFORMANCE_DBI_CACHE_TYPE		0x05


static BraseroScsiResult
brasero_get_performance (BraseroGetPerformanceCDB *cdb,
			 gint sizeof_descriptors,
			 BraseroScsiGetPerfData **data,
			 int *data_size,
			 BraseroScsiErrCode *error)
{
	BraseroScsiGetPerfData *buffer;
	BraseroScsiGetPerfHdr hdr;
	BraseroScsiResult res;
	int request_size;
	int buffer_size;
	int desc_num;

	if (!data || !data_size) {
		BRASERO_SCSI_SET_ERRCODE (error, BRASERO_SCSI_BAD_ARGUMENT);
		return BRASERO_SCSI_FAILURE;
	}

	memset (&hdr, 0, sizeof (hdr));
	BRASERO_SET_16 (cdb->max_desc, 0);
	res = brasero_scsi_command_issue_sync (cdb, &hdr, sizeof (hdr), error);
	if (res)
		return res;

	/* ... allocate a buffer and re-issue the command */
	request_size = BRASERO_GET_32 (hdr.len) +
		       G_STRUCT_OFFSET (BraseroScsiGetPerfHdr, len) +
		       sizeof (hdr.len);

	desc_num = (request_size - sizeof (BraseroScsiGetPerfHdr)) / sizeof_descriptors;
	BRASERO_SET_16 (cdb->max_desc, desc_num);

	buffer = (BraseroScsiGetPerfData *) g_new0 (uchar, request_size);

	res = brasero_scsi_command_issue_sync (cdb, buffer, request_size, error);
	if (res) {
		g_free (buffer);
		return res;
	}

	/* make sure the response has the requested size */
	buffer_size = BRASERO_GET_32 (buffer->hdr.len) +
		      G_STRUCT_OFFSET (BraseroScsiGetPerfHdr, len) +
		      sizeof (buffer->hdr.len);

	if (buffer_size != request_size) {
		g_free (buffer);

		BRASERO_SCSI_SET_ERRCODE (error, BRASERO_SCSI_SIZE_MISMATCH);
		return BRASERO_SCSI_FAILURE;
	}

	*data = buffer;
	*data_size = request_size;

	return res;
}

/**
 * MMC2 command extension
 */

BraseroScsiResult
brasero_mmc2_get_performance_perf (int fd,
				   BraseroScsiPerfParam param,
				   BraseroScsiGetPerfData **data,
				   int *size,
				   BraseroScsiErrCode *error)
{
	BraseroGetPerformanceCDB *cdb;
	BraseroScsiResult res;

	cdb = brasero_scsi_command_new (&info, fd);
	cdb->type = BRASERO_GET_PERFORMANCE_PERF_TYPE;

	if (param & BRASERO_SCSI_PERF_LIST)
		cdb->except= 0x01;

	if (param & BRASERO_SCSI_PERF_WRITE)
		cdb->write = 0x01;

	res = brasero_get_performance (cdb, sizeof (BraseroScsiPerfDesc), data, size, error);
	brasero_scsi_command_free (cdb);
	return res;
}

/**
 * MMC3 command extension
 */

BraseroScsiResult
brasero_mmc3_get_performance_wrt_spd_desc (int fd,
					   BraseroScsiGetPerfData **data,
					   int *size,
					   BraseroScsiErrCode *error)
{
	BraseroGetPerformanceCDB *cdb;
	BraseroScsiResult res;

	cdb = brasero_scsi_command_new (&info, fd);
	cdb->type = BRASERO_GET_PERFORMANCE_WR_SPEED_TYPE;

	res = brasero_get_performance (cdb, sizeof (BraseroScsiWrtSpdDesc), data, size, error);
	brasero_scsi_command_free (cdb);
	return res;
}
