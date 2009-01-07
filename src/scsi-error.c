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

#include "burn-media.h"
#include "scsi-error.h"

static const gchar *error_string [] = {	N_("Unknown error"),
					N_("Size mismatch"),
					N_("Type mismatch"),
					N_("Bad argument"),
					N_("The drive is busy"),
					N_("Outrange address"),
					N_("Invalid address"),
					N_("Invalid command"),
					N_("Invalid parameter in command"),
					N_("Invalid field in command"),
					N_("The device timed out"),
					N_("Key not established"),
					N_("Invalid track mode"),
					NULL	};	/* errno */

const gchar *
brasero_scsi_strerror (BraseroScsiErrCode code)
{
	if (code > BRASERO_SCSI_ERROR_LAST || code < 0)
		return NULL;

	if (code == BRASERO_SCSI_ERRNO)
		return g_strerror (errno);

	return _(error_string [code]);
}

void
brasero_scsi_set_error (GError **error, BraseroScsiErrCode code)
{
	g_set_error (error,
		     BRASERO_MEDIA_ERROR,
		     BRASERO_MEDIA_ERROR_GENERAL,
		     "%s",
		     brasero_scsi_strerror (code));
}
