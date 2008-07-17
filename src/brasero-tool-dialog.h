/***************************************************************************
 *            brasero-tool-dialog.h
 *
 *  ven sep  1 19:45:01 2006
 *  Copyright  2006  Rouquier Philippe
 *  bonfire-app@wanadoo.fr
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

#ifndef BRASERO_TOOL_DIALOG_H
#define BRASERO_TOOL_DIALOG_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtkdialog.h>

#include "burn-basics.h"
#include "burn-job.h"
#include "burn.h"
#include "burn-medium.h"
#include "burn-session.h"

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

	gboolean	(*activate)		(BraseroToolDialog *dialog,
						 BraseroMedium *medium);
	void		(*cancel)		(BraseroToolDialog *dialog);
	void		(*drive_changed)	(BraseroToolDialog *dialog,
						 BraseroMedium *medium);
};

GType brasero_tool_dialog_get_type ();

void
brasero_tool_dialog_pack_options (BraseroToolDialog *dialog, ...);

void
brasero_tool_dialog_set_button (BraseroToolDialog *dialog,
				const gchar *text,
				const gchar *image,
				const gchar *theme);
void
brasero_tool_dialog_set_valid (BraseroToolDialog *dialog,
			       gboolean valid);

void
brasero_tool_dialog_set_progress (BraseroToolDialog *self,
				  gdouble overall_progress,
				  gdouble task_progress,
				  glong remaining,
				  gint size_mb,
				  gint written_mb);
void
brasero_tool_dialog_set_action (BraseroToolDialog *self,
				BraseroBurnAction action,
				const gchar *string);

BraseroBurn *
brasero_tool_dialog_get_burn (BraseroToolDialog *dialog);

BraseroMedium *
brasero_tool_dialog_get_medium (BraseroToolDialog *dialog);

G_END_DECLS

#endif /* BRASERO_TOOL_DIALOG_H */
