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

#include <glib.h>

#include "scsi-error.h"
#include "scsi-utils.h"
#include "scsi-base.h"
#include "scsi-command.h"
#include "scsi-opcodes.h"

#if G_BYTE_ORDER == G_LITTLE_ENDIAN

struct _BraseroLoadCDCDB {
	uchar opcode;

	uchar immed		:1;
	uchar res2		:4;
	uchar res1		:3;

	uchar res3		[2];

	uchar start		:1;
	uchar load		:1;
	uchar res4		:6;

	uchar res5		[3];
	uchar slot;
	uchar res6		[2];

	uchar ctl;
};

#else

struct _BraseroLoadCDCDB {
	uchar opcode;

	uchar res1		:3;
	uchar res2		:4;
	uchar immed		:1;

	uchar res3		[2];

	uchar res4		:6;
	uchar load		:1;
	uchar start		:1;

	uchar res5		[3];
	uchar slot;
	uchar res6		[2];

	uchar ctl;
};

#endif

typedef struct _BraseroLoadCDCDB BraseroLoadCDCDB;

BRASERO_SCSI_COMMAND_DEFINE (BraseroLoadCDCDB,
			     LOAD_CD,
			     BRASERO_SCSI_READ);

BraseroScsiResult
brasero_mmc1_load_cd (BraseroDeviceHandle *handle,
		      BraseroScsiErrCode *error)
{
	BraseroLoadCDCDB *cdb;
	BraseroScsiResult res;

	cdb = brasero_scsi_command_new (&info, handle);
	res = brasero_scsi_command_issue_sync (cdb,
					       NULL,
					       0,
					       error);
	brasero_scsi_command_free (cdb);
	return res;
}
