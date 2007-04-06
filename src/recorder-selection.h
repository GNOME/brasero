/***************************************************************************
*            recorder-selection.h
*
*  mer jun 15 12:40:07 2005
*  Copyright  2005  Philippe Rouquier
*  brasero-app@wanadoo.fr
****************************************************************************/

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

#ifndef RECORDER_SELECTION_H
#define RECORDER_SELECTION_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtkhbox.h>

#include <nautilus-burn-drive.h>

#include "burn.h"

G_BEGIN_DECLS
#define BRASERO_TYPE_RECORDER_SELECTION         (brasero_recorder_selection_get_type ())
#define BRASERO_RECORDER_SELECTION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_RECORDER_SELECTION, BraseroRecorderSelection))
#define BRASERO_RECORDER_SELECTION_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_RECORDER_SELECTION, BraseroRecorderSelectionClass))
#define BRASERO_IS_RECORDER_SELECTION(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_RECORDER_SELECTION))
#define BRASERO_IS_RECORDER_SELECTION_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_RECORDER_SELECTION))
#define BRASERO_RECORDER_SELECTION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_RECORDER_SELECTION, BraseroRecorderSelectionClass))
typedef struct BraseroRecorderSelectionPrivate BraseroRecorderSelectionPrivate;

typedef struct {
	union {
		gint drive_speed;
		BraseroImageFormat image_format;
	} props;

	gchar *output_path;
	BraseroBurnFlag flags;
} BraseroDriveProp;

typedef struct {
	GtkHBox parent;
	BraseroRecorderSelectionPrivate *priv;
} BraseroRecorderSelection;

typedef struct {
	GtkHBoxClass parent_class;

	/* signal */
	void		(*media_changed)	(BraseroRecorderSelection *selection,
						 BraseroMediumInfo media);

} BraseroRecorderSelectionClass;

GType brasero_recorder_selection_get_type ();

GtkWidget *
brasero_recorder_selection_new (void);

void
brasero_recorder_selection_set_drive (BraseroRecorderSelection *selection,
				      NautilusBurnDrive *drive);
void
brasero_recorder_selection_lock (BraseroRecorderSelection *selection,
				 gboolean locked);

void
brasero_recorder_selection_select_default_drive (BraseroRecorderSelection *selection,
						 BraseroMediumInfo type);
void
brasero_recorder_selection_set_source_track (BraseroRecorderSelection *selection,
					     const BraseroTrackSource *source);
void
brasero_recorder_selection_get_drive (BraseroRecorderSelection *selection,
				      NautilusBurnDrive **drive,
				      BraseroDriveProp *props);
void
brasero_recorder_selection_get_media (BraseroRecorderSelection *selection,
				      BraseroMediumInfo *media);

#endif				/* RECORDER_SELECTION_H */
