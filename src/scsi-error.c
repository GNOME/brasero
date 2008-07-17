/*
 * brasero
 * Copyright (C) Philippe Rouquier 2007-2008 <bonfire-app@wanadoo.fr>
 *
 * Brasero is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * Brasero is distributed in the hope that it will be useful,
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

#include <errno.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#include "burn-basics.h"
#include "scsi-error.h"

static const gchar *error_string [] = {	N_("unknown error"),
					N_("size mismatch"),
					N_("type mismatch"),
					N_("bad argument"),
					N_("the device is not ready"),
					N_("outrange address"),
					N_("invalid address"),
					N_("invalid command"),
					N_("invalid parameter in command"),
					N_("invalid field in command"),
					N_("the device timed out"),
					N_("key not established"),
					N_("invalid track mode"),
					NULL	};	/* errno */

const gchar *
brasero_scsi_strerror (BraseroScsiErrCode code)
{
	if (code > BRASERO_SCSI_ERROR_LAST || code < 0)
		return NULL;

	if (code == BRASERO_SCSI_ERRNO)
		return strerror (errno);

	return _(error_string [code]);
}

void
brasero_scsi_set_error (GError **error, BraseroScsiErrCode code)
{
	if (!error)
		return;

	g_set_error (error,
		     BRASERO_BURN_ERROR,
		     BRASERO_BURN_ERROR_GENERAL,
		     brasero_scsi_strerror (code));
}
