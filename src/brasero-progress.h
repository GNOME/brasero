/***************************************************************************
 *            progress.h
 *
 *  ven mar 10 15:42:01 2006
 *  Copyright  2006  Rouquier Philippe
 *  brasero-app@wanadoo.fr
 ***************************************************************************/

/*
 *  Brasero is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Brasero is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifndef PROGRESS_H
#define PROGRESS_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

#include "burn-basics.h"
#include "burn-media.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_BURN_PROGRESS         (brasero_burn_progress_get_type ())
#define BRASERO_BURN_PROGRESS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_BURN_PROGRESS, BraseroBurnProgress))
#define BRASERO_BURN_PROGRESS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_BURN_PROGRESS, BraseroBurnProgressClass))
#define BRASERO_IS_BURN_PROGRESS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_BURN_PROGRESS))
#define BRASERO_IS_BURN_PROGRESS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_BURN_PROGRESS))
#define BRASERO_BURN_PROGRESS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_BURN_PROGRESS, BraseroBurnProgressClass))

typedef struct BraseroBurnProgressPrivate BraseroBurnProgressPrivate;

typedef struct {
	GtkVBox parent;
	BraseroBurnProgressPrivate *priv;
} BraseroBurnProgress;

typedef struct {
	GtkVBoxClass parent_class;
} BraseroBurnProgressClass;

GType brasero_burn_progress_get_type();

GtkWidget *brasero_burn_progress_new();

void
brasero_burn_progress_reset (BraseroBurnProgress *progress);

void
brasero_burn_progress_set_status (BraseroBurnProgress *progress,
				  BraseroMedia media,
				  gdouble overall_progress,
				  gdouble action_progress,
				  glong remaining,
				  gint mb_isosize,
				  gint mb_written,
				  gint64 rate);
void
brasero_burn_progress_display_session_info (BraseroBurnProgress *progress,
					    glong time,
					    gint64 rate,
					    BraseroMedia media,
					    gint mb_written);

void
brasero_burn_progress_set_action (BraseroBurnProgress *progress,
				  BraseroBurnAction action,
				  const gchar *string);

#endif /* PROGRESS_H */
