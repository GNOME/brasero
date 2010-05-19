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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "scsi-spc1.h"

#include "scsi-error.h"
#include "scsi-utils.h"
#include "scsi-base.h"
#include "scsi-command.h"
#include "scsi-opcodes.h"
#include "scsi-inquiry.h"

#if G_BYTE_ORDER == G_LITTLE_ENDIAN

struct _BraseroInquiryCDB {
	uchar opcode;

	uchar evpd			:1;
	uchar cmd_dt			:1;
	uchar reserved0		:6;

	uchar op_code;

	uchar reserved1;

	uchar alloc_len;

	uchar ctl;
};

#else

struct _BraseroInquiryCDB {
	uchar opcode;

	uchar reserved0		:6;
	uchar cmd_dt			:1;
	uchar evpd			:1;

	uchar op_code;

	uchar reserved1;

	uchar alloc_len;

	uchar ctl;
};

#endif

typedef struct _BraseroInquiryCDB BraseroInquiryCDB;

BRASERO_SCSI_COMMAND_DEFINE (BraseroInquiryCDB,
			     INQUIRY,
			     BRASERO_SCSI_READ);

BraseroScsiResult
brasero_spc1_inquiry (BraseroDeviceHandle *handle,
                      BraseroScsiInquiry *hdr,
                      BraseroScsiErrCode *error)
{
	BraseroInquiryCDB *cdb;
	BraseroScsiResult res;

	g_return_val_if_fail (handle != NULL, BRASERO_SCSI_FAILURE);

	cdb = brasero_scsi_command_new (&info, handle);
	cdb->alloc_len = sizeof (BraseroScsiInquiry);

	memset (hdr, 0, sizeof (BraseroScsiInquiry));
	res = brasero_scsi_command_issue_sync (cdb,
					       hdr,
					       sizeof (BraseroScsiInquiry),
					       error);
	brasero_scsi_command_free (cdb);
	return res;
}

BraseroScsiResult
brasero_spc1_inquiry_is_optical_drive (BraseroDeviceHandle *handle,
                                       BraseroScsiErrCode *error)
{
	BraseroInquiryCDB *cdb;
	BraseroScsiInquiry hdr;
	BraseroScsiResult res;

	g_return_val_if_fail (handle != NULL, BRASERO_SCSI_FAILURE);

	cdb = brasero_scsi_command_new (&info, handle);
	cdb->alloc_len = sizeof (hdr);

	memset (&hdr, 0, sizeof (hdr));
	res = brasero_scsi_command_issue_sync (cdb,
					       &hdr,
					       sizeof (hdr),
					       error);
	brasero_scsi_command_free (cdb);

	if (res != BRASERO_SCSI_OK)
		return res;

	/* NOTE: 0x05 is for CD/DVD players */
	return hdr.type == 0x05? BRASERO_SCSI_OK:BRASERO_SCSI_RECOVERABLE;
}

 
