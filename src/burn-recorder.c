/***************************************************************************
 *            recorder.c
 *
 *  dim jan 22 17:31:49 2006
 *  Copyright  2006  Rouquier Philippe
 *  brasero-app@wanadoo.fr
 ***************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include <nautilus-burn-drive.h>

#include "brasero-marshal.h"
#include "burn-basics.h"
#include "burn-recorder.h"
#include "burn-job.h"

static void brasero_recorder_base_init (gpointer g_class);

GType
brasero_recorder_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroRecorderIFace),
			brasero_recorder_base_init,
			NULL,
			NULL,
		};

		type = g_type_register_static (G_TYPE_INTERFACE, 
					       "BraseroRecorder",
					       &our_info,
					       0);
	}

	return type;
}

static void
brasero_recorder_base_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

        if (initialized)
		return;

	initialized = TRUE;
}

BraseroBurnResult
brasero_recorder_set_drive (BraseroRecorder *recorder,
			    NautilusBurnDrive *drive,
			    GError **error)
{
	BraseroRecorderIFace *iface;

	g_return_val_if_fail (BRASERO_IS_RECORDER (recorder), BRASERO_BURN_ERR);
	g_return_val_if_fail (NAUTILUS_BURN_IS_DRIVE (drive), BRASERO_BURN_ERR);

	if (!nautilus_burn_drive_can_write (drive)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the drive cannot write CDs or DVDs"));
		return BRASERO_BURN_ERR;
	}

	if (brasero_job_is_running (BRASERO_JOB (recorder)))
		return BRASERO_BURN_RUNNING;

	iface = BRASERO_RECORDER_GET_IFACE (recorder);
	if (!iface->set_drive)
		BRASERO_JOB_NOT_SUPPORTED (recorder);

	BRASERO_JOB_LOG (recorder,"set_drive");
	return  (* iface->set_drive) (recorder,
				       drive,
				       error);
}

BraseroBurnResult
brasero_recorder_set_flags (BraseroRecorder *recorder,
			    BraseroRecorderFlag flags,
			    GError **error)
{
	BraseroRecorderIFace *iface;

	g_return_val_if_fail (BRASERO_IS_RECORDER (recorder), BRASERO_BURN_ERR);

	BRASERO_JOB_LOG (recorder, "set_flags");

	if (brasero_job_is_running (BRASERO_JOB (recorder)))
		return BRASERO_BURN_RUNNING;

	iface = BRASERO_RECORDER_GET_IFACE (recorder);
	if (!iface->set_flags)
		BRASERO_JOB_NOT_SUPPORTED (recorder);

	return  (*iface->set_flags) (recorder, flags, error);
}

BraseroBurnResult
brasero_recorder_blank (BraseroRecorder *recorder,
			GError **error)
{
	BraseroRecorderIFace *iface;

	g_return_val_if_fail (BRASERO_IS_RECORDER (recorder), BRASERO_BURN_ERR);

	BRASERO_JOB_LOG (recorder, "blank");

	if (brasero_job_is_running (BRASERO_JOB (recorder)))
		return BRASERO_BURN_RUNNING;

	iface = BRASERO_RECORDER_GET_IFACE (recorder);
	if (!iface->blank)
		BRASERO_JOB_NOT_SUPPORTED (recorder);

	return  (* iface->blank) (recorder, error);
}

BraseroBurnResult
brasero_recorder_record (BraseroRecorder *recorder,
			 GError **error)
{
	BraseroRecorderIFace *iface;
	BraseroBurnResult result;

	g_return_val_if_fail (BRASERO_IS_RECORDER (recorder), BRASERO_BURN_ERR);

	BRASERO_JOB_LOG (recorder, "record");

	if (brasero_job_is_running (BRASERO_JOB (recorder)))
		return BRASERO_BURN_RUNNING;

	iface = BRASERO_RECORDER_GET_IFACE (recorder);
	if (!iface->record)
		BRASERO_JOB_NOT_SUPPORTED (recorder);

	result = (*iface->record) (recorder, error);
	return result;
}

