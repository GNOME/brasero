/***************************************************************************
 *            burn-error.h
 *
 *  Fri Oct 20 12:23:20 2006
 *  Copyright  2006  Rouquier Philippe
 *  <Rouquier Philippe@localhost.localdomain>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>

#include <errno.h>
#include <string.h>

#include <glib.h>

#ifndef _BURN_ERROR_H
#define _BURN_ERROR_H

G_BEGIN_DECLS

typedef enum {
	BRASERO_SCSI_ERR_UNKNOWN = 0,
	BRASERO_SCSI_SIZE_MISMATCH,
	BRASERO_SCSI_TYPE_MISMATCH,
	BRASERO_SCSI_BAD_ARGUMENT,
	BRASERO_SCSI_NOT_READY,
	BRASERO_SCSI_OUTRANGE_ADDRESS,
	BRASERO_SCSI_INVALID_ADDRESS,
	BRASERO_SCSI_INVALID_COMMAND,
	BRASERO_SCSI_INVALID_PARAMETER,
	BRASERO_SCSI_INVALID_FIELD,
	BRASERO_SCSI_TIMEOUT,
	BRASERO_SCSI_KEY_NOT_ESTABLISHED,
	BRASERO_SCSI_INVALID_TRACK_MODE,
	BRASERO_SCSI_ERRNO,
	BRASERO_SCSI_ERROR_LAST
} BraseroScsiErrCode;

typedef enum {
	BRASERO_SCSI_OK,
	BRASERO_SCSI_FAILURE,
	BRASERO_SCSI_RECOVERABLE,
} BraseroScsiResult;

const gchar *
brasero_scsi_strerror (BraseroScsiErrCode code);

void
brasero_scsi_set_error (GError **error, BraseroScsiErrCode code);

G_END_DECLS

#endif /* _BURN_ERROR_H */
