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

#include "burn-medium.h"
#include "burn-drive.h"
#include "brasero-medium-selection.h"
#include "brasero-drive-selection.h"
#include "brasero-drive-info.h"

typedef struct _BraseroDriveSelectionPrivate BraseroDriveSelectionPrivate;
struct _BraseroDriveSelectionPrivate
{
	GtkWidget *box;
	GtkWidget *info;
	GtkWidget *button;
	GtkWidget *selection;

	BraseroDrive *locked_drive;
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
brasero_drive_selection_drive_changed_cb (BraseroMediumSelection *selector,
					  BraseroDriveSelection *self)
{
	BraseroDriveSelectionPrivate *priv;
	BraseroMedium *medium;
	BraseroDrive *drive;

	priv = BRASERO_DRIVE_SELECTION_PRIVATE (self);

	medium = brasero_medium_selection_get_active (BRASERO_MEDIUM_SELECTION (priv->selection));

	if (medium)
		drive = brasero_medium_get_drive (medium);
	else
		drive = NULL;

	brasero_drive_info_set_medium (BRASERO_DRIVE_INFO (priv->info), medium);

	if (priv->locked_drive && priv->locked_drive != drive) {
		brasero_drive_unlock (priv->locked_drive);
		g_object_unref (priv->locked_drive);
		priv->locked_drive = NULL;
	}

	if (!drive) {
	    	gtk_widget_set_sensitive (priv->selection, FALSE);
	    	gtk_widget_set_sensitive (priv->info, FALSE);

		g_signal_emit (self,
			       brasero_drive_selection_signals [DRIVE_CHANGED_SIGNAL],
			       0,
			       NULL);

		if (medium)
			g_object_unref (medium);
		return;
	}

	gtk_widget_set_sensitive (priv->info, TRUE);
	gtk_widget_set_sensitive (priv->selection, (priv->locked_drive == NULL));
	g_signal_emit (self,
		       brasero_drive_selection_signals [DRIVE_CHANGED_SIGNAL],
		       0,
		       drive);

	if (medium)
		g_object_unref (medium);
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
brasero_drive_selection_set_same_src_dest (BraseroDriveSelection *self)
{
	BraseroDriveSelectionPrivate *priv;

	priv = BRASERO_DRIVE_SELECTION_PRIVATE (self);
	brasero_drive_info_set_same_src_dest (BRASERO_DRIVE_INFO (priv->info));
}

void
brasero_drive_selection_set_drive (BraseroDriveSelection *self,
				   BraseroDrive *drive)
{
	BraseroDriveSelectionPrivate *priv;
	BraseroMedium *medium;

	priv = BRASERO_DRIVE_SELECTION_PRIVATE (self);
	if (priv->locked_drive)
		return;

	medium = brasero_drive_get_medium (drive);
	brasero_medium_selection_set_active (BRASERO_MEDIUM_SELECTION (priv->selection), medium);
}

BraseroMedium *
brasero_drive_selection_get_medium (BraseroDriveSelection *self)
{
	BraseroDriveSelectionPrivate *priv;

	priv = BRASERO_DRIVE_SELECTION_PRIVATE (self);

	return brasero_medium_selection_get_active (BRASERO_MEDIUM_SELECTION (priv->selection));
}

BraseroDrive *
brasero_drive_selection_get_drive (BraseroDriveSelection *self)
{
	BraseroDrive *drive;
	BraseroMedium *medium;
	BraseroDriveSelectionPrivate *priv;

	priv = BRASERO_DRIVE_SELECTION_PRIVATE (self);

	medium = brasero_medium_selection_get_active (BRASERO_MEDIUM_SELECTION (priv->selection));
	if (!medium)
		return NULL;

	drive = brasero_medium_get_drive (medium);
	g_object_unref (medium);

	g_object_ref (drive);
	return drive;
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
		brasero_drive_unlock (priv->locked_drive);
		g_object_unref (priv->locked_drive);
	}

	if (locked) {
		BraseroDrive *drive;

		drive = brasero_drive_selection_get_drive (self);
		priv->locked_drive = drive;
		if (priv->locked_drive)
			brasero_drive_lock (priv->locked_drive,
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
brasero_drive_selection_set_type_shown (BraseroDriveSelection *self,
					BraseroMediaType type)
{
	BraseroDriveSelectionPrivate *priv;

	priv = BRASERO_DRIVE_SELECTION_PRIVATE (self);

	brasero_medium_selection_show_type (BRASERO_MEDIUM_SELECTION (priv->selection), type);
	if (priv->locked_drive)
		gtk_widget_set_sensitive (priv->selection, FALSE);
}

void
brasero_drive_selection_set_tooltip (BraseroDriveSelection *self,
				     const gchar *tooltip)
{
	BraseroDriveSelectionPrivate *priv;

	priv = BRASERO_DRIVE_SELECTION_PRIVATE (self);
	gtk_widget_set_tooltip_text (priv->selection, tooltip);
}

static void
brasero_drive_selection_init (BraseroDriveSelection *object)
{
	BraseroDriveSelectionPrivate *priv;

	priv = BRASERO_DRIVE_SELECTION_PRIVATE (object);
	gtk_box_set_spacing (GTK_BOX (object), 12);

	priv->box = gtk_hbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (object), priv->box, FALSE, FALSE, 0);

	priv->selection = brasero_medium_selection_new ();
	g_signal_connect (priv->selection,
			  "medium-changed",
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

	gtk_widget_show_all (GTK_WIDGET (object));
}

static void
brasero_drive_selection_finalize (GObject *object)
{
	BraseroDriveSelectionPrivate *priv;

	priv = BRASERO_DRIVE_SELECTION_PRIVATE (object);

	if (priv->locked_drive) {
		brasero_drive_unlock (priv->locked_drive);
		g_object_unref (priv->locked_drive);
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
			  BRASERO_TYPE_DRIVE);
}

GtkWidget *
brasero_drive_selection_new (void)
{
	return GTK_WIDGET (g_object_new (BRASERO_TYPE_DRIVE_SELECTION, NULL));
}
