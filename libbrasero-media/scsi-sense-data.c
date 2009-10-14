/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Brasero
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <errno.h>
#include <stdio.h>

#include "brasero-media-private.h"

#include "scsi-error.h"
#include "scsi-utils.h"
#include "scsi-base.h"
#include "scsi-sense-data.h"

/**
 * defines to interpret sense data (returned after SENSE REQUEST)
 * (defined in SCSI Primary command 3 / SPC specs)
 **/

#define SENSE_DATA_KEY(sense)			((sense) ? (sense) [2] & 0x0F : 0x00)			/* Sense code itself */
#define SENSE_DATA_ASC(sense)			((sense) ? (sense) [12] : 0x00)				/* Additional Sense Code */
#define SENSE_DATA_ASCQ(sense)			((sense) ? (sense) [13] : 0x00)				/* Additional Sense Code Qualifier */
#define SENSE_DATA_ASC_ASCQ(sense)		((sense) ? (sense) [12] << 8 | (sense) [13] : 0x00)	/* ASC and ASCQ combined */

#define SENSE_CODE_NOT_READY				0x02
#define SENSE_CODE_ILLEGAL_REQUEST			0x05
#define SENSE_CODE_UNIT_ATTENTION			0x06

#define ASC_CODE_NOT_READY				0x04
#define ASC_CODE_NO_MEDIUM			0x3A
#define ASC_CODE_PARAMETER				0x26
#define ASC_CODE_PROTECTION_KEY				0x6F

#define ASC_ASCQ_CODE_INVALID_COMMAND			0x2000
#define ASC_ASCQ_CODE_OUTRANGE_ADDRESS			0x2100
#define ASC_ASCQ_CODE_INVALID_ADDRESS			0x2101
#define ASC_ASCQ_CODE_INVALID_FIELD_IN_PARAM		0x2600
#define ASC_ASCQ_CODE_INVALID_FIELD_IN_CDB		0x2400
#define ASC_ASCQ_CODE_INSUFFICIENT_TIME_FOR_OPERATION	0x2E00
#define ASC_ASCQ_CODE_KEY_NOT_ESTABLISHED		0x6F02
#define ASC_ASCQ_CODE_SCRAMBLED_SECTOR			0x6F03
#define ASC_ASCQ_CODE_INVALID_TRACK_MODE		0x6400
#define ASC_ASCQ_CODE_MEDIUM_CHANGED			0x2800

/**
 * error processing 
 */

static void
brasero_sense_data_print (uchar *sense_data)
{
	int i;

	if (!sense_data)
		return;

	/* Print that in a more sensible way */
	BRASERO_MEDIA_LOG ("SK=0x%02x ASC=0x%02x ASCQ=0x%02x",
			   SENSE_DATA_KEY (sense_data),
			   SENSE_DATA_ASC (sense_data),
			   SENSE_DATA_ASCQ (sense_data));

	printf ("Sense key: 0x%02x ", sense_data [0]);
	for (i = 1; i < BRASERO_SENSE_DATA_SIZE; i ++)
		printf ("0x%02x ", sense_data [i]);

	printf ("\n");
}

static BraseroScsiResult
brasero_sense_data_unknown (uchar *sense_data, BraseroScsiErrCode *err)
{
	BRASERO_SCSI_SET_ERRCODE (err, BRASERO_SCSI_ERR_UNKNOWN);
	brasero_sense_data_print (sense_data);

	return BRASERO_SCSI_FAILURE;
}

static BraseroScsiResult
brasero_sense_data_not_ready (uchar *sense_data, BraseroScsiErrCode *err)
{
	BraseroScsiResult res = BRASERO_SCSI_FAILURE;

	switch (SENSE_DATA_ASC (sense_data)) {
		case ASC_CODE_NO_MEDIUM:
			/* No need to use BRASERO_SCSI_SET_ERRCODE
			 * as this is not necessarily an error */
			*err = BRASERO_SCSI_NO_MEDIUM;
			break;
		case ASC_CODE_NOT_READY:
			BRASERO_SCSI_SET_ERRCODE (err, BRASERO_SCSI_NOT_READY);
			break;

		default:
			res = brasero_sense_data_unknown (sense_data, err);
			break;
	}

	return res;
}

static BraseroScsiResult
brasero_sense_data_illegal_request (uchar *sense_data, BraseroScsiErrCode *err)
{
	BraseroScsiResult res = BRASERO_SCSI_FAILURE;

	switch (SENSE_DATA_ASC_ASCQ (sense_data)) {
		case ASC_ASCQ_CODE_INVALID_COMMAND:
			BRASERO_SCSI_SET_ERRCODE (err, BRASERO_SCSI_INVALID_COMMAND);
			break;

		case ASC_ASCQ_CODE_OUTRANGE_ADDRESS:
			BRASERO_SCSI_SET_ERRCODE (err, BRASERO_SCSI_OUTRANGE_ADDRESS);
			break;

		case ASC_ASCQ_CODE_INVALID_ADDRESS:
			BRASERO_SCSI_SET_ERRCODE (err, BRASERO_SCSI_INVALID_ADDRESS);
			break;

		case ASC_ASCQ_CODE_INVALID_FIELD_IN_PARAM:
			BRASERO_SCSI_SET_ERRCODE (err, BRASERO_SCSI_INVALID_PARAMETER);
			break;

		case ASC_ASCQ_CODE_INVALID_FIELD_IN_CDB:
			BRASERO_SCSI_SET_ERRCODE (err, BRASERO_SCSI_INVALID_FIELD);
			break;

		case ASC_ASCQ_CODE_KEY_NOT_ESTABLISHED:
		case ASC_ASCQ_CODE_SCRAMBLED_SECTOR:
			BRASERO_SCSI_SET_ERRCODE (err, BRASERO_SCSI_KEY_NOT_ESTABLISHED);
			break;

		case ASC_ASCQ_CODE_INVALID_TRACK_MODE:
			BRASERO_SCSI_SET_ERRCODE (err, BRASERO_SCSI_INVALID_TRACK_MODE);
			break;

		default:
			res = brasero_sense_data_unknown (sense_data, err);
			break;
	}

	return res;
}

static BraseroScsiResult
brasero_sense_data_unit_attention (uchar *sense_data,
				   BraseroScsiErrCode *err)
{
	BraseroScsiResult res = BRASERO_SCSI_FAILURE;

	switch (SENSE_DATA_ASC_ASCQ (sense_data)) {
		case ASC_ASCQ_CODE_INSUFFICIENT_TIME_FOR_OPERATION:
			BRASERO_SCSI_SET_ERRCODE (err, BRASERO_SCSI_TIMEOUT);
			break;

		case ASC_ASCQ_CODE_MEDIUM_CHANGED:
			BRASERO_SCSI_SET_ERRCODE (err, BRASERO_SCSI_NOT_READY);
			break;

		default:
			res = brasero_sense_data_unknown (sense_data, err);
			break;
	}

	return res;
}

BraseroScsiResult
brasero_sense_data_process (uchar *sense_data,
			    BraseroScsiErrCode *err)
{
	BraseroScsiResult res = BRASERO_SCSI_FAILURE;

	errno = EIO;

	/* see if something was written to the sense buffer and if we
	 * can interpret more precisely what went wrong */
	switch (SENSE_DATA_KEY (sense_data)) {
		case SENSE_CODE_NOT_READY:
			res = brasero_sense_data_not_ready (sense_data, err);
			break;

		case SENSE_CODE_ILLEGAL_REQUEST:
			res = brasero_sense_data_illegal_request (sense_data, err);
			break;

		case SENSE_CODE_UNIT_ATTENTION:
			res = brasero_sense_data_unit_attention (sense_data, err);
			break;

		default:
			res = brasero_sense_data_unknown (sense_data, err);
			break;
	}

	return res;
}
