/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2007 <bonfire-app@wanadoo.fr>
 * 
 * brasero is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * brasero is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with brasero.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtkbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>

#include <nautilus-burn-drive.h>
#include <nautilus-burn-drive-monitor.h>
#include <nautilus-burn-drive-selection.h>

#include "burn-medium.h"
#include "brasero-ncb.h"
#include "brasero-drive-selection.h"
#include "brasero-drive-info.h"

typedef struct _BraseroDriveSelectionPrivate BraseroDriveSelectionPrivate;
struct _BraseroDriveSelectionPrivate
{
	GtkWidget *box;
	GtkWidget *info;
	GtkWidget *button;
	GtkWidget *selection;

	NautilusBurnDrive *locked_drive;
};

#define BRASERO_DRIVE_SELECTION_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_DRIVE_SELECTION, BraseroDriveSelectionPrivate))

enum {
	DRIVE_CHANGED_SIGNAL,
	LAST_SIGNAL
};
static guint brasero_drive_selection_signals [LAST_SIGNAL] = { 0 };

static GtkVBoxClass* parent_class = NULL;

G_DEFINE_TYPE (BraseroDriveSelection, brasero_drive_selection, GTK_TYPE_VBOX);

static void
brasero_drive_selection_drive_changed_cb (NautilusBurnDriveSelection *selector,
					  NautilusBurnDrive *drive,
					  BraseroDriveSelection *self)
{
	BraseroDriveSelectionPrivate *priv;

	priv = BRASERO_DRIVE_SELECTION_PRIVATE (self);

	brasero_drive_info_set_drive (BRASERO_DRIVE_INFO (priv->info), drive);

	if (priv->locked_drive
	&& !nautilus_burn_drive_equal (priv->locked_drive, drive)) {
	    	gtk_widget_set_sensitive (priv->selection, TRUE);
		nautilus_burn_drive_unlock (priv->locked_drive);
		nautilus_burn_drive_unref (priv->locked_drive);
		priv->locked_drive = NULL;
	}

	if (drive == NULL) {
	    	gtk_widget_set_sensitive (priv->selection, FALSE);
		g_signal_emit (self,
			       brasero_drive_selection_signals [DRIVE_CHANGED_SIGNAL],
			       0,
			       drive);
		return;
	}

	if (NCB_DRIVE_GET_TYPE (drive) == NAUTILUS_BURN_DRIVE_TYPE_FILE) {
		g_signal_emit (self,
			       brasero_drive_selection_signals [DRIVE_CHANGED_SIGNAL],
			       0,
			       drive);
		return;
	}

	g_signal_emit (self,
		       brasero_drive_selection_signals [DRIVE_CHANGED_SIGNAL],
		       0,
		       drive);
}

void
brasero_drive_selection_select_default_drive (BraseroDriveSelection *self,
					      BraseroMedia type)
{
	GList *iter;
	GList *drives;
	gboolean image;
	gboolean recorders;
	BraseroMedia media;
	NautilusBurnDrive *drive;
	NautilusBurnDrive *candidate = NULL;
	BraseroDriveSelectionPrivate *priv;

	priv = BRASERO_DRIVE_SELECTION_PRIVATE (self);

	if (priv->locked_drive)
		return;

	g_object_get (priv->selection,
		      "show-recorders-only",
		      &recorders,
		      NULL);
	g_object_get (priv->selection,
		      "file-image",
		      &image,
		      NULL);

	NCB_DRIVE_GET_LIST (drives, recorders, image);
	for (iter = drives; iter; iter = iter->next) {
		drive = iter->data;

		if (!drive || NCB_DRIVE_GET_TYPE (drive) == NAUTILUS_BURN_DRIVE_TYPE_FILE)
			continue;

		media = NCB_MEDIA_GET_STATUS (drive);
		if (type == BRASERO_MEDIUM_WRITABLE && (media & (BRASERO_MEDIUM_APPENDABLE|BRASERO_MEDIUM_REWRITABLE|BRASERO_MEDIUM_BLANK))) {
			/* the perfect candidate would be blank; if not keep for later and see if no better media comes up */
			if (media & BRASERO_MEDIUM_BLANK) {
				nautilus_burn_drive_selection_set_active (NAUTILUS_BURN_DRIVE_SELECTION (priv->selection), drive);
				goto end;
			}

			/* a second choice would be rewritable media and if not appendable */
			if (media & BRASERO_MEDIUM_REWRITABLE) {
				if (NCB_MEDIA_GET_STATUS (candidate) & BRASERO_MEDIUM_REWRITABLE){
					gint64 size_candidate;
					gint64 size;

					NCB_MEDIA_GET_FREE_SPACE (candidate, &size_candidate, NULL);
					NCB_MEDIA_GET_FREE_SPACE (drive, &size, NULL);
					if (size_candidate < size)
						candidate = drive;
				}
				else
					candidate = drive;

			}
			/* if both are appendable choose the one with the bigger free space */
			else if (!(NCB_MEDIA_GET_STATUS (candidate) & BRASERO_MEDIUM_REWRITABLE)) {
				gint64 size_candidate;
				gint64 size;

				NCB_MEDIA_GET_FREE_SPACE (candidate, &size_candidate, NULL);
				NCB_MEDIA_GET_FREE_SPACE (drive, &size, NULL);
				if (size_candidate < size)
					candidate = drive;
			}
		}
		else if (type == BRASERO_MEDIUM_REWRITABLE && (media & BRASERO_MEDIUM_REWRITABLE)) {
			/* the perfect candidate would have data; if not keep it for later and see if no better media comes up */
			if (media & (BRASERO_MEDIUM_HAS_DATA|BRASERO_MEDIUM_HAS_AUDIO)) {
				nautilus_burn_drive_selection_set_active (NAUTILUS_BURN_DRIVE_SELECTION (priv->selection), drive);
				goto end;
			}

			candidate = drive;
		}
		else if (type == BRASERO_MEDIUM_HAS_DATA && (media & (BRASERO_MEDIUM_HAS_DATA|BRASERO_MEDIUM_HAS_AUDIO))) {
			/* the perfect candidate would not be rewritable; if not keep it for later and see if no better media comes up */
			if (!(media & BRASERO_MEDIUM_REWRITABLE)) {
				nautilus_burn_drive_selection_set_active (NAUTILUS_BURN_DRIVE_SELECTION (priv->selection), drive);
				goto end;
			}

			candidate = drive;
		}
	}

	if (candidate)
		nautilus_burn_drive_selection_set_active (NAUTILUS_BURN_DRIVE_SELECTION (priv->selection), candidate);

end:
	g_list_foreach (drives, (GFunc) nautilus_burn_drive_unref, NULL);
	g_list_free (drives);
}

void
brasero_drive_selection_set_image_path (BraseroDriveSelection *self,
					const gchar *path)
{
	BraseroDriveSelectionPrivate *priv;

	priv = BRASERO_DRIVE_SELECTION_PRIVATE (self);
	brasero_drive_info_set_image_path (BRASERO_DRIVE_INFO (priv->info), path);
}

void
brasero_drive_selection_set_drive (BraseroDriveSelection *self,
				   NautilusBurnDrive *drive)
{
	BraseroDriveSelectionPrivate *priv;

	priv = BRASERO_DRIVE_SELECTION_PRIVATE (self);
	if (priv->locked_drive)
		return;

	nautilus_burn_drive_selection_set_active (NAUTILUS_BURN_DRIVE_SELECTION (priv->selection),
						  drive);
}

void
brasero_drive_selection_get_drive (BraseroDriveSelection *self,
				   NautilusBurnDrive **drive)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	BraseroDriveSelectionPrivate *priv;

	priv = BRASERO_DRIVE_SELECTION_PRIVATE (self);

	/* This is a hack to work around the inability of ncb to return the
	 * current selected drive while we're initting an object derived from it
	 */
	if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (priv->selection), &iter)) {
		*drive = NULL;
		return;
	}

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->selection));
	gtk_tree_model_get (model, &iter,
			    1, drive,
			    -1);

	nautilus_burn_drive_ref (*drive);
}

void
brasero_drive_selection_lock (BraseroDriveSelection *self,
			      gboolean locked)
{
	BraseroDriveSelectionPrivate *priv;

	priv = BRASERO_DRIVE_SELECTION_PRIVATE (self);

	gtk_widget_set_sensitive (priv->selection, (locked != TRUE));
	gtk_widget_queue_draw (priv->selection);
	if (priv->locked_drive) {
		nautilus_burn_drive_unlock (priv->locked_drive);
		nautilus_burn_drive_unref (priv->locked_drive);
	}

	if (locked) {
		NautilusBurnDrive *drive;

		brasero_drive_selection_get_drive (self, &drive);
		priv->locked_drive = drive;
		if (priv->locked_drive)
			nautilus_burn_drive_lock (priv->locked_drive,
						  _("ongoing burning process"),
						  NULL);
	}
}

void
brasero_drive_selection_set_button (BraseroDriveSelection *self,
				    GtkWidget *button)
{
	BraseroDriveSelectionPrivate *priv;
	GtkWidget *parent;

	priv = BRASERO_DRIVE_SELECTION_PRIVATE (self);
	parent = gtk_widget_get_parent (priv->selection);
	gtk_box_pack_start (GTK_BOX (parent), button, FALSE, FALSE, 0);
}

void
brasero_drive_selection_set_show_all_drives (BraseroDriveSelection *self,
					     gboolean show)
{
	BraseroDriveSelectionPrivate *priv;

	priv = BRASERO_DRIVE_SELECTION_PRIVATE (self);
	g_object_set (G_OBJECT (priv->selection),
		      "show-recorders-only", (show == FALSE),
		      NULL);

	/* ncb sets sensitivity on its own so we need to reset it correctly */
	if (priv->locked_drive)
		gtk_widget_set_sensitive (priv->selection, FALSE);
}

void
brasero_drive_selection_show_file_drive (BraseroDriveSelection *self,
					 gboolean show)
{
	BraseroDriveSelectionPrivate *priv;

	priv = BRASERO_DRIVE_SELECTION_PRIVATE (self);
	g_object_set (G_OBJECT (priv->selection),
		      "file-image", show,
		      NULL);

	/* ncb sets sensitivity on its own so we need to reset it correctly */
	if (priv->locked_drive)
		gtk_widget_set_sensitive (priv->selection, FALSE);
}

static void
brasero_drive_selection_init (BraseroDriveSelection *object)
{
	BraseroDriveSelectionPrivate *priv;
	NautilusBurnDrive *drive;

	priv = BRASERO_DRIVE_SELECTION_PRIVATE (object);
	gtk_box_set_spacing (GTK_BOX (object), 12);

	priv->box = gtk_hbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (object), priv->box, FALSE, FALSE, 0);

	priv->selection = nautilus_burn_drive_selection_new ();
	g_signal_connect (priv->selection,
			  "drive-changed",
			  G_CALLBACK (brasero_drive_selection_drive_changed_cb),
			  object);
	gtk_box_pack_start (GTK_BOX (priv->box),
			    priv->selection,
			    FALSE,
			    FALSE,
			    0);

	priv->info = brasero_drive_info_new ();
	gtk_box_pack_start (GTK_BOX (object),
			    priv->info,
			    FALSE,
			    FALSE,
			    0);

	brasero_drive_selection_get_drive (object, &drive);
	brasero_drive_info_set_drive (BRASERO_DRIVE_INFO (priv->info), drive);
	nautilus_burn_drive_unref (drive);

	gtk_widget_show_all (GTK_WIDGET (object));
}

static void
brasero_drive_selection_finalize (GObject *object)
{
	BraseroDriveSelectionPrivate *priv;

	priv = BRASERO_DRIVE_SELECTION_PRIVATE (object);

	if (priv->locked_drive) {
		nautilus_burn_drive_unlock (priv->locked_drive);
		nautilus_burn_drive_unref (priv->locked_drive);
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_drive_selection_class_init (BraseroDriveSelectionClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	parent_class = GTK_VBOX_CLASS (g_type_class_peek_parent (klass));

	g_type_class_add_private (klass, sizeof (BraseroDriveSelectionPrivate));
	object_class->finalize = brasero_drive_selection_finalize;

	brasero_drive_selection_signals [DRIVE_CHANGED_SIGNAL] =
	    g_signal_new ("drive_changed",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_FIRST,
			  G_STRUCT_OFFSET (BraseroDriveSelectionClass,
					   drive_changed),
			  NULL, NULL,
			  g_cclosure_marshal_VOID__OBJECT,
			  G_TYPE_NONE,
			  1,
			  NAUTILUS_BURN_TYPE_DRIVE);
}

GtkWidget *
brasero_drive_selection_new (void)
{
	return GTK_WIDGET (g_object_new (BRASERO_TYPE_DRIVE_SELECTION, NULL));
}
