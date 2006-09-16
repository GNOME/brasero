/***************************************************************************
 *            imager.h
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

#ifndef IMAGER_H
#define IMAGER_H

#include <glib.h>
#include <glib-object.h>

#include <nautilus-burn-drive.h>

#include "burn-basics.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_IMAGER         (brasero_imager_get_type ())
#define BRASERO_IMAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_IMAGER, BraseroImager))
#define BRASERO_IS_IMAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_IMAGER))
#define BRASERO_IMAGER_GET_IFACE(o) (G_TYPE_INSTANCE_GET_INTERFACE ((o), BRASERO_TYPE_IMAGER, BraseroImagerIFace))

typedef struct _BraseroImagerIFace BraseroImagerIFace;

struct _BraseroImagerIFace {
	GTypeInterface parent_class;

	/* virtual functions */
	BraseroBurnResult	(*get_track_type)	(BraseroImager *imager,
							 BraseroTrackSourceType *type,
							 BraseroImageFormat *format);

	BraseroBurnResult	(*get_track)		(BraseroImager *imager,
							 BraseroTrackSource **track,
							 GError **error);
	BraseroBurnResult	(*get_size)		(BraseroImager *imager,
							 gint64 *size,
							 gboolean sectors,
							 GError **error);

	BraseroBurnResult	(*set_append)          	(BraseroImager *imager,
							 NautilusBurnDrive *drive,
							 gboolean merge,
							 GError **error);

	BraseroBurnResult	(*set_output_type)	(BraseroImager *imager,
							 BraseroTrackSourceType type,
							 BraseroImageFormat format,
							 GError **error);

	BraseroBurnResult	(*set_output)          	(BraseroImager *imager,
							 const gchar *output,
							 gboolean overwrite,
							 gboolean clean,
							 GError **error);
};

GType brasero_imager_get_type ();

BraseroBurnResult
brasero_imager_set_output (BraseroImager *imager,
			   const char *output,
			   gboolean overwrite,
			   gboolean clean,
			   GError **error);
BraseroBurnResult
brasero_imager_set_output_type (BraseroImager *imager,
				BraseroTrackSourceType type,
				BraseroImageFormat format,
				GError **error);
BraseroBurnResult
brasero_imager_set_append (BraseroImager *imager,
			   NautilusBurnDrive *drive,
			   gboolean merge,
			   GError **error);

BraseroBurnResult
brasero_imager_get_track (BraseroImager *imager,
			  BraseroTrackSource **track,
			  GError **error);
BraseroBurnResult
brasero_imager_get_size (BraseroImager *imager,
			 gint64 *size,
			 gboolean sectors,
			 GError **error);

BraseroBurnResult
brasero_imager_get_track_type (BraseroImager *imager,
			       BraseroTrackSourceType *type,
			       BraseroImageFormat *format);

#endif /* IMAGER_H */
