/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2007-2008 <bonfire-app@wanadoo.fr>
 *
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>

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

	cdb = brasero_scsi_command_new (&info, handle);
	cdb->PF = 1;
	cdb->SP = 0;

	/* Header pages lengths should be 0 */
	BRASERO_SET_16 (data->hdr.len, 0);
	BRASERO_SET_16 (data->hdr.bdlen, 0);

	BRASERO_SET_16 (cdb->alloc_len, size);
	res = brasero_scsi_command_issue_sync (cdb, data, size, error);
	brasero_scsi_command_free (cdb);

	return res;
}
