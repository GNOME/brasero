/***************************************************************************
 *            recorder.h
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

#ifndef RECORDER_H
#define RECORDER_H

#include <glib.h>
#include <glib-object.h>

#include <nautilus-burn-drive.h>

#include "burn-basics.h"

G_BEGIN_DECLS

typedef enum {
	BRASERO_RECORDER_FLAG_NONE		= 0,
	BRASERO_RECORDER_FLAG_NOGRACE		= 1,
	BRASERO_RECORDER_FLAG_DAO		= 1 << 1,
	BRASERO_RECORDER_FLAG_MULTI		= 1 << 2,
	BRASERO_RECORDER_FLAG_DUMMY		= 1 << 3,
	BRASERO_RECORDER_FLAG_OVERBURN		= 1 << 4,
	BRASERO_RECORDER_FLAG_BURNPROOF		= 1 << 5,
	BRASERO_RECORDER_FLAG_FAST_BLANK	= 1 << 6
} BraseroRecorderFlag;

#define BRASERO_TYPE_RECORDER         (brasero_recorder_get_type ())
#define BRASERO_RECORDER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_RECORDER, BraseroRecorder))
#define BRASERO_IS_RECORDER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_RECORDER))
#define BRASERO_RECORDER_GET_IFACE(o) (G_TYPE_INSTANCE_GET_INTERFACE ((o), BRASERO_TYPE_RECORDER, BraseroRecorderIFace))

typedef struct _BraseroRecorder BraseroRecorder;
typedef struct _BraseroRecorderIFace BraseroRecorderIFace;

struct _BraseroRecorderIFace{
	GTypeInterface parent_class;

	/* methods */
	BraseroBurnResult	(*set_drive)		(BraseroRecorder *recorder,
							 NautilusBurnDrive *drive,
							 GError **error);
	BraseroBurnResult	(*set_flags)		(BraseroRecorder *recorder,
							 BraseroRecorderFlag flags,
							 GError **error);

	BraseroBurnResult	(*record)		(BraseroRecorder *recorder,
							 GError **error);
	BraseroBurnResult	(*blank)		(BraseroRecorder *recorder,
							 GError **error);
};

GType brasero_recorder_get_type();

BraseroBurnResult
brasero_recorder_set_flags (BraseroRecorder *recorder,
			    BraseroRecorderFlag flags,
			    GError **error);
BraseroBurnResult
brasero_recorder_set_drive (BraseroRecorder *recorder,
			    NautilusBurnDrive *drive,
			    GError **error);

BraseroBurnResult
brasero_recorder_blank (BraseroRecorder *recorder,
			GError **error);

BraseroBurnResult
brasero_recorder_record (BraseroRecorder *recorder,
			 GError **error);

#endif /* RECORDER_H */
