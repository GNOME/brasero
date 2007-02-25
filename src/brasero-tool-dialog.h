/***************************************************************************
 *            brasero-tool-dialog.h
 *
 *  ven sep  1 19:45:01 2006
 *  Copyright  2006  Rouquier Philippe
 *  bonfire-app@wanadoo.fr
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

#ifndef BRASERO_TOOL_DIALOG_H
#define BRASERO_TOOL_DIALOG_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtkdialog.h>

#include "burn-basics.h"
#include "burn-job.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_TOOL_DIALOG         (brasero_tool_dialog_get_type ())
#define BRASERO_TOOL_DIALOG(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_TOOL_DIALOG, BraseroToolDialog))
#define BRASERO_TOOL_DIALOG_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_TOOL_DIALOG, BraseroToolDialogClass))
#define BRASERO_IS_TOOL_DIALOG(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_TOOL_DIALOG))
#define BRASERO_IS_TOOL_DIALOG_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_TOOL_DIALOG))
#define BRASERO_TOOL_DIALOG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_TOOL_DIALOG, BraseroToolDialogClass))

typedef struct _BraseroToolDialog BraseroToolDialog;
typedef struct _BraseroToolDialogPrivate BraseroToolDialogPrivate;
typedef struct _BraseroToolDialogClass BraseroToolDialogClass;

struct _BraseroToolDialog {
	GtkDialog parent;
	BraseroToolDialogPrivate *priv;
};

struct _BraseroToolDialogClass {
	GtkDialogClass parent_class;

	gboolean	(*cancel)		(BraseroToolDialog *self);
	gboolean	(*activate)		(BraseroToolDialog *self,
						 NautilusBurnDrive *drive);
	void		(*media_changed)	(BraseroToolDialog *self,
						 NautilusBurnMediaType media);
};

GType brasero_tool_dialog_get_type ();

void brasero_tool_dialog_pack_options (BraseroToolDialog *self,
				       ...);
void brasero_tool_dialog_set_button (BraseroToolDialog *self,
				     const gchar *text,
				     const gchar *image,
				     const gchar *theme);
void brasero_tool_dialog_set_action (BraseroToolDialog *self,
				     BraseroBurnAction action,
				     const gchar *string);
void brasero_tool_dialog_set_progress (BraseroToolDialog *self,
				       gdouble overall_progress,
				       gdouble task_progress,
				       glong remaining,
				       gint size_mb,
				       gint written_mb);

BraseroBurnResult brasero_tool_dialog_run_job (BraseroToolDialog *self,
					       BraseroJob *job,
					       const BraseroTrackSource *track,
					       BraseroTrackSource **retval,
					       GError **error);

NautilusBurnMediaType
brasero_tool_dialog_get_media (BraseroToolDialog *self);

NautilusBurnDrive *
brasero_tool_dialog_get_drive (BraseroToolDialog *self);

/* methods to be used */
void brasero_tool_dialog_run (BraseroToolDialog *self);

void brasero_tool_dialog_set_active (BraseroToolDialog *self,
				     NautilusBurnDrive *drive);

G_END_DECLS

#endif /* BRASERO_TOOL_DIALOG_H */
