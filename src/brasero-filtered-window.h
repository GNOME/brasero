/***************************************************************************
 *            filtered-window.h
 *
 *  dim oct 30 12:25:50 2005
 *  Copyright  2005  Rouquier Philippe
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

#ifndef FILTERED_WINDOW_H
#define FILTERED_WINDOW_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtkdialog.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_FILTERED_DIALOG         (brasero_filtered_dialog_get_type ())
#define BRASERO_FILTERED_DIALOG(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_FILTERED_DIALOG, BraseroFilteredDialog))
#define BRASERO_FILTERED_DIALOG_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_FILTERED_DIALOG, BraseroFilteredDialogClass))
#define BRASERO_IS_FILTERED_DIALOG(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_FILTERED_DIALOG))
#define BRASERO_IS_FILTERED_DIALOG_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_FILTERED_DIALOG))
#define BRASERO_FILTERED_DIALOG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_FILTERED_DIALOG, BraseroFilteredDialogClass))

typedef struct BraseroFilteredDialogPrivate BraseroFilteredDialogPrivate;

typedef enum {
	BRASERO_FILTER_HIDDEN = 1,
	BRASERO_FILTER_UNREADABLE,
	BRASERO_FILTER_BROKEN_SYM,
	BRASERO_FILTER_RECURSIVE_SYM,
	BRASERO_FILTER_UNKNOWN
} BraseroFilterStatus;

#define BRASERO_FILTER_HIDDEN_KEY		"/apps/brasero/filter/hidden"
#define BRASERO_FILTER_BROKEN_SYM_KEY		"/apps/brasero/filter/broken_sym"
#define BRASERO_FILTER_NOTIFY_KEY		"/apps/brasero/filter/notify"

typedef struct {
	GtkDialog parent;
	BraseroFilteredDialogPrivate *priv;
} BraseroFilteredDialog;

typedef struct {
	GtkDialogClass parent_class;

	void (*removed) (BraseroFilteredDialog *window, const char *uri);
	void (*restored) (BraseroFilteredDialog *window, const char *uri);
} BraseroFilteredDialogClass;

GType brasero_filtered_dialog_get_type();
GtkWidget *brasero_filtered_dialog_new();

void
brasero_filtered_dialog_add (BraseroFilteredDialog *window,
			     const char *uri,
			     gboolean restored,
			     BraseroFilterStatus status);

void
brasero_filtered_dialog_get_status (BraseroFilteredDialog *dialog,
				    GSList **restored,
				    GSList **removed);

#endif /* FILTERED_WINDOW_H */
