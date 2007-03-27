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

#include <errno.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>

#include "burn-basics.h"
#include "scsi/scsi-error.h"

void
brasero_scsi_set_error (GError **error, BraseroScsiErrCode code)
{
	if (!error)
		return;

	switch (code) {
	case BRASERO_SCSI_ERR_UNKNOWN:
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("unknown error"));
		break;

	case BRASERO_SCSI_SIZE_MISMATCH:
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("size mismatch"));
		break;

	case BRASERO_SCSI_TYPE_MISMATCH:
		break;

	case BRASERO_SCSI_BAD_ARGUMENT:
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("bad argument"));
		break;

	case BRASERO_SCSI_ERRNO:
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     strerror (errno));
		break;

	case BRASERO_SCSI_NOT_READY:
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the device is not ready"));
		break;

	case BRASERO_SCSI_OUTRANGE_ADDRESS:
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("outrange address"));
	     break;

	case BRASERO_SCSI_INVALID_ADDRESS:
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("invalid address"));
		break;

	case BRASERO_SCSI_INVALID_COMMAND:
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("invalid SCSI command"));
		break;

	case BRASERO_SCSI_INVALID_PARAMETER:
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("invalid parameter in command"));
		break;

	case BRASERO_SCSI_INVALID_FIELD:
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("invalid field in command"));
		break;

	case BRASERO_SCSI_TIMEOUT:
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the device timed out"));
		break;

	case BRASERO_SCSI_KEY_NOT_ESTABLISHED:
		break;
	}
}
