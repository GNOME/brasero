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

#include "scsi-spc1.h"

#include "scsi-error.h"
#include "scsi-utils.h"
#include "scsi-base.h"
#include "scsi-command.h"
#include "scsi-opcodes.h"
#include "scsi-mode-pages.h"

/**
 * MODE SELECT command description (defined in SPC, Scsi Primary Commands) 
 */

#if G_BYTE_ORDER == G_LITTLE_ENDIAN

struct _BraseroModeSelectCDB {
	uchar opcode		:8;

	uchar SP		:1;
	uchar reserved0		:3;
	uchar PF		:1;
	uchar reserved1		:3;

	uchar reserved2		[5];

	uchar alloc_len		[2];
	uchar ctl;
};

#else

struct _BraseroModeSelectCDB {
	uchar opcode		:8;

	uchar reserved0		:3;
	uchar PF		:1;
	uchar reserved1		:3;
	uchar SP		:1;

	uchar reserved2		[5];

	uchar alloc_len		[2];
	uchar ctl;
};

#endif

typedef struct _BraseroModeSelectCDB BraseroModeSelectCDB;

BRASERO_SCSI_COMMAND_DEFINE (BraseroModeSelectCDB,
			     MODE_SELECT,
			     BRASERO_SCSI_WRITE);

#define BRASERO_MODE_DATA(data)			((BraseroScsiModeData *) (data))

BraseroScsiResult
brasero_spc1_mode_select (BraseroDeviceHandle *handle,
			  BraseroScsiModeData *data,
			  int size,
			  BraseroScsiErrCode *error)
{
	BraseroModeSelectCDB *cdb;
	BraseroScsiResult res;

	g_return_val_if_fail (handle != NULL, BRASERO_SCSI_FAILURE);

	cdb = brasero_scsi_command_new (&info, handle);
	cdb->PF = 1;
	cdb->SP = 0;

	/* Header pages lengths should be 0 */
//	BRASERO_SET_16 (data->hdr.len, 0);
//	BRASERO_SET_16 (data->hdr.bdlen, 0);

	BRASERO_SET_16 (cdb->alloc_len, size);
	res = brasero_scsi_command_issue_sync (cdb, data, size, error);
	brasero_scsi_command_free (cdb);

	return res;
}
