/***************************************************************************
 *            burn-caps.h
 *
 *  mar avr 18 20:58:42 2006
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

#ifndef BURN_CAPS_H
#define BURN_CAPS_H

#include <glib.h>
#include <glib-object.h>

#include <nautilus-burn-drive.h>

#include "burn-basics.h"
#include "burn-recorder.h"
#include "burn-imager.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_BURNCAPS         (brasero_burn_caps_get_type ())
#define BRASERO_BURNCAPS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_BURNCAPS, BraseroBurnCaps))
#define BRASERO_BURNCAPS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_BURNCAPS, BraseroBurnCapsClass))
#define BRASERO_IS_BURNCAPS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_BURNCAPS))
#define BRASERO_IS_BURNCAPS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_BURNCAPS))
#define BRASERO_BURNCAPS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_BURNCAPS, BraseroBurnCapsClass))

typedef enum {
	BRASERO_BURN_FLAG_NONE			= 0,
	BRASERO_BURN_FLAG_EJECT			= 1,
	BRASERO_BURN_FLAG_NOGRACE		= 1 << 1,

	BRASERO_BURN_FLAG_DAO			= 1 << 2,
	BRASERO_BURN_FLAG_OVERBURN		= 1 << 3,
	BRASERO_BURN_FLAG_BURNPROOF		= 1 << 4,
	BRASERO_BURN_FLAG_ON_THE_FLY		= 1 << 5,

	BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE	= 1 << 6,
	BRASERO_BURN_FLAG_DONT_CLEAN_OUTPUT	= 1 << 7,
	BRASERO_BURN_FLAG_DONT_OVERWRITE	= 1 << 8,

	BRASERO_BURN_FLAG_DONT_CLOSE		= 1 << 9,
	BRASERO_BURN_FLAG_APPEND		= 1 << 10,
	BRASERO_BURN_FLAG_MERGE			= 1 << 11,

	BRASERO_BURN_FLAG_DUMMY			= 1 << 12,
	BRASERO_BURN_FLAG_DEBUG			= 1 << 13,

	BRASERO_BURN_FLAG_CHECK_SIZE		= 1 << 14,
} BraseroBurnFlag;

typedef struct BraseroBurnCapsPrivate BraseroBurnCapsPrivate;

typedef struct {
	GObject parent;
	BraseroBurnCapsPrivate *priv;
} BraseroBurnCaps;

typedef struct {
	GObjectClass parent_class;
} BraseroBurnCapsClass;

GType brasero_burn_caps_get_type();

BraseroBurnCaps *brasero_burn_caps_get_default ();

BraseroBurnResult
brasero_burn_caps_get_flags (BraseroBurnCaps *caps,
			     const BraseroTrackSource *source,
			     NautilusBurnDrive *drive,
			     BraseroBurnFlag *default_retval,
			     BraseroBurnFlag *compulsory_retval,
			     BraseroBurnFlag *supported_retval);

BraseroBurnResult
brasero_burn_caps_blanking_get_default_flags (BraseroBurnCaps *caps,
					      NautilusBurnMediaType media_type,
					      BraseroBurnFlag *flags,
					      gboolean *fast_default);
BraseroBurnResult
brasero_burn_caps_blanking_get_supported_flags (BraseroBurnCaps *caps,
						NautilusBurnMediaType media_type,
						BraseroBurnFlag *flags,
						gboolean *fast_supported);

BraseroBurnFlag
brasero_burn_caps_check_flags_consistency (BraseroBurnCaps *caps,
					   const BraseroTrackSource *source,
					   NautilusBurnDrive *drive,
					   BraseroBurnFlag flags);

BraseroBurnResult
brasero_burn_caps_create_imager (BraseroBurnCaps *caps,
				 BraseroImager **imager,
				 const BraseroTrackSource *source,
				 BraseroTrackSourceType target,
				 NautilusBurnMediaType src_media_type,
				 NautilusBurnMediaType dest_media_type,
				 GError **error);

BraseroBurnResult
brasero_burn_caps_get_imager_available_formats (BraseroBurnCaps *caps,
						NautilusBurnDrive *drive,
						BraseroTrackSourceType type,
						BraseroImageFormat **formats);
BraseroImageFormat
brasero_burn_caps_get_imager_default_format (BraseroBurnCaps *caps,
					     const BraseroTrackSource *source);

BraseroBurnResult
brasero_burn_caps_create_recorder (BraseroBurnCaps *caps,
				   BraseroRecorder **recorder,
				   const BraseroTrackSource *source,
				   NautilusBurnMediaType media_type,
				   GError **error);

BraseroBurnResult
brasero_burn_caps_create_recorder_for_blanking (BraseroBurnCaps *caps,
						BraseroRecorder **recorder,
						NautilusBurnMediaType media_type,
						gboolean fast,
						GError **error);

BraseroMediaType
brasero_burn_caps_get_required_media_type (BraseroBurnCaps *caps,
					   const BraseroTrackSource *source);

#endif /* BURN_CAPS_H */
