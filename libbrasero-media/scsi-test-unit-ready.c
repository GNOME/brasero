/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2007-2008 <bonfire-app@wanadoo.fr>
 * 
 *  Brasero is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 * brasero is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with brasero.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "scsi-error.h"
#include "scsi-utils.h"
#include "scsi-base.h"
#include "scsi-command.h"
#include "scsi-opcodes.h"

struct _BraseroTestUnitReadyCDB {
	uchar opcode;
	uchar reserved		[4];
	uchar ctl;
};

typedef struct _BraseroTestUnitReadyCDB BraseroTestUnitReadyCDB;

BRASERO_SCSI_COMMAND_DEFINE (BraseroTestUnitReadyCDB,
			     TEST_UNIT_READY,
			     BRASERO_SCSI_READ);

BraseroScsiResult
brasero_spc1_test_unit_ready (BraseroDeviceHandle *handle,
			      BraseroScsiErrCode *error)
{
	BraseroTestUnitReadyCDB *cdb;
	BraseroScsiResult res;

	cdb = brasero_scsi_command_new (&info, handle);
	res = brasero_scsi_command_issue_sync (cdb,
					       NULL,
					       0,
					       error);
	brasero_scsi_command_free (cdb);
	return res;
}

 
