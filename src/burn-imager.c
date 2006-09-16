/***************************************************************************
 *            imager.c
 *
 *  dim jan 22 17:32:17 2006
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

#include "burn-basics.h"
#include "burn-imager.h"
#include "burn-job.h"

static void brasero_imager_base_init (gpointer g_class);

GType
brasero_imager_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroImagerIFace),
			brasero_imager_base_init,
			NULL,
			NULL,
		};

		type = g_type_register_static(G_TYPE_INTERFACE, 
					      "BraseroImager",
					      &our_info,
					      0);
	}

	return type;
}

static void
brasero_imager_base_init (gpointer g_class)
{
        static gboolean initialized = FALSE;

        if (initialized)
		return;

        initialized = TRUE;
}

BraseroBurnResult
brasero_imager_get_track (BraseroImager *imager,
			  BraseroTrackSource **track,
			  GError **error)
{
	BraseroImagerIFace *iface;

	g_return_val_if_fail (BRASERO_IS_IMAGER (imager), BRASERO_BURN_ERR);
	g_return_val_if_fail (track != NULL, BRASERO_BURN_ERR);

	BRASERO_JOB_LOG (imager,"get_track");

	if (brasero_job_is_running (BRASERO_JOB (imager)))
		return BRASERO_BURN_RUNNING;
	
	iface = BRASERO_IMAGER_GET_IFACE (imager);
	if (!iface->get_track)
		BRASERO_JOB_NOT_SUPPORTED (imager);

	return (* iface->get_track) (imager,
				     track,
				     error);
}

BraseroBurnResult
brasero_imager_set_output_type (BraseroImager *imager,
				BraseroTrackSourceType type,
				BraseroImageFormat format,
				GError **error)
{
	BraseroImagerIFace *iface;

	g_return_val_if_fail (type != BRASERO_TRACK_SOURCE_UNKNOWN, BRASERO_BURN_ERR);
	g_return_val_if_fail (BRASERO_IS_IMAGER (imager), BRASERO_BURN_ERR);

	BRASERO_JOB_LOG (imager, "set_output_type");

	if (brasero_job_is_running (BRASERO_JOB (imager)))
		return BRASERO_BURN_RUNNING;

	iface = BRASERO_IMAGER_GET_IFACE (imager);
	if (!iface->set_output_type)
		BRASERO_JOB_NOT_SUPPORTED (imager);

	return (* iface->set_output_type) (imager,
					   type,
					   format,
					   error);
}

BraseroBurnResult 
brasero_imager_set_output (BraseroImager *imager,
			   const char *output,
			   gboolean overwrite,
			   gboolean clean,
			   GError **error)
{
	BraseroImagerIFace *iface;

	g_return_val_if_fail (BRASERO_IS_IMAGER (imager), BRASERO_BURN_ERR);
	
	BRASERO_JOB_LOG (imager, "set_output");

	if (brasero_job_is_running (BRASERO_JOB (imager)))
		return BRASERO_BURN_RUNNING;

	iface = BRASERO_IMAGER_GET_IFACE (imager);
	if (!iface->set_output)
		BRASERO_JOB_NOT_SUPPORTED (imager);

	return (* iface->set_output) (imager,
				       output,
				       overwrite,
				       clean,
				       error);
}

BraseroBurnResult 
brasero_imager_set_append (BraseroImager *imager,
			   NautilusBurnDrive *drive,
			   gboolean merge,
			   GError **error)
{
	BraseroImagerIFace *iface;

	g_return_val_if_fail (BRASERO_IS_IMAGER (imager), BRASERO_BURN_ERR);

	BRASERO_JOB_LOG (imager, "set_append");

	if (brasero_job_is_running (BRASERO_JOB (imager)))
		return BRASERO_BURN_RUNNING;

	iface = BRASERO_IMAGER_GET_IFACE (imager);
	if (!iface->set_append)
		BRASERO_JOB_NOT_SUPPORTED (imager);

	return (* iface->set_append) (imager,
				      drive,
				      merge,
				      error);
}

BraseroBurnResult
brasero_imager_get_size (BraseroImager *imager,
			 gint64 *size,
			 gboolean sectors,
			 GError **error)
{
	BraseroImagerIFace *iface;

	g_return_val_if_fail (BRASERO_IS_IMAGER (imager), BRASERO_BURN_ERR);
	g_return_val_if_fail (size != NULL, BRASERO_BURN_ERR);

	BRASERO_JOB_LOG (imager, "get_size");

	iface = BRASERO_IMAGER_GET_IFACE (imager);
	if (!iface->get_size)
		BRASERO_JOB_NOT_SUPPORTED (imager);

	if (!size)
		return BRASERO_BURN_OK;

	return (* iface->get_size) (imager, size, sectors, error);
}

BraseroBurnResult
brasero_imager_get_track_type (BraseroImager *imager,
			       BraseroTrackSourceType *type,
			       BraseroImageFormat *format)
{
	BraseroImagerIFace *iface;

	g_return_val_if_fail (BRASERO_IS_IMAGER (imager), BRASERO_BURN_ERR);
	g_return_val_if_fail (type != NULL, BRASERO_BURN_ERR);

	BRASERO_JOB_LOG (imager, "get_track_type");

	iface = BRASERO_IMAGER_GET_IFACE (imager);
	if (!iface->get_track_type)
		BRASERO_JOB_NOT_SUPPORTED (imager);

	return (* iface->get_track_type) (imager, type, format);
}
