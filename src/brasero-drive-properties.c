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

#include <gtk/gtkwindow.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkcombobox.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkcelllayout.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkbox.h>

#include <nautilus-burn-drive.h>

#include <libgnomevfs/gnome-vfs.h>

#include "burn-basics.h"
#include "burn-medium.h"
#include "burn-debug.h"
#include "brasero-ncb.h"
#include "brasero-utils.h"
#include "brasero-drive-properties.h"

typedef struct _BraseroDrivePropertiesPrivate BraseroDrivePropertiesPrivate;
struct _BraseroDrivePropertiesPrivate
{
	GtkWidget *speed;
	GtkWidget *dummy;
	GtkWidget *burnproof;
	GtkWidget *notmp;
	GtkWidget *eject;

	GtkWidget *tmpdir;
	GtkWidget *tmpdir_size;
};

#define BRASERO_DRIVE_PROPERTIES_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_DRIVE_PROPERTIES, BraseroDrivePropertiesPrivate))

enum {
	PROP_TEXT,
	PROP_RATE,
	PROP_NUM
};

static GtkDialogClass* parent_class = NULL;

G_DEFINE_TYPE (BraseroDriveProperties, brasero_drive_properties, GTK_TYPE_DIALOG);

BraseroBurnFlag
brasero_drive_properties_get_flags (BraseroDriveProperties *self)
{
	BraseroBurnFlag flags = BRASERO_BURN_FLAG_NONE;
	BraseroDrivePropertiesPrivate *priv;

	priv = BRASERO_DRIVE_PROPERTIES_PRIVATE (self);

	/* retrieve the flags */
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->notmp)))
		flags |= BRASERO_BURN_FLAG_NO_TMP_FILES;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->eject)))
		flags |= BRASERO_BURN_FLAG_EJECT;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->dummy)))
		flags |= BRASERO_BURN_FLAG_DUMMY;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->burnproof)))
		flags |= BRASERO_BURN_FLAG_BURNPROOF;

	return flags;
}

gint64
brasero_drive_properties_get_rate (BraseroDriveProperties *self)
{
	BraseroDrivePropertiesPrivate *priv;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gint64 rate;

	priv = BRASERO_DRIVE_PROPERTIES_PRIVATE (self);

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->speed));
	if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (priv->speed), &iter))
		gtk_tree_model_get_iter_first (model, &iter);

	gtk_tree_model_get (model, &iter,
			    PROP_RATE, &rate,
			    -1);

	return rate;
}

gchar *
brasero_drive_properties_get_tmpdir (BraseroDriveProperties *self)
{
	BraseroDrivePropertiesPrivate *priv;

	priv = BRASERO_DRIVE_PROPERTIES_PRIVATE (self);
	return gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (priv->tmpdir));
}

void
brasero_drive_properties_set_tmpdir (BraseroDriveProperties *self,
				     const gchar *path)
{
	gchar *string;
	gchar *uri_str;
	gchar *directory;
	GnomeVFSURI *uri;
	BraseroBurnResult result;
	GnomeVFSFileSize vol_size = 0;
	BraseroDrivePropertiesPrivate *priv;

	priv = BRASERO_DRIVE_PROPERTIES_PRIVATE (self);

	if (!path)
		gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (priv->tmpdir),
					       g_get_tmp_dir ());
	else
		gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (priv->tmpdir),
					       path);

	/* get the volume free space */
	directory = g_path_get_dirname (path);
	uri_str = gnome_vfs_get_uri_from_local_path (directory);
	g_free (directory);

	uri = gnome_vfs_uri_new (uri_str);
	g_free (uri_str);

	if (uri == NULL) {
		BRASERO_BURN_LOG ("impossible to retrieve size for %s", path);
		gtk_label_set_text (GTK_LABEL (priv->tmpdir_size), _("unknown"));
		return;
	}

	result = gnome_vfs_get_volume_free_space (uri, &vol_size);
	if (result != GNOME_VFS_OK) {
		BRASERO_BURN_LOG ("impossible to retrieve size for %s", path);
		gtk_label_set_text (GTK_LABEL (priv->tmpdir_size), _("unknown"));
		return;
	}

	gnome_vfs_uri_unref (uri);

	string = brasero_utils_get_size_string (vol_size, TRUE, TRUE);
	gtk_label_set_text (GTK_LABEL (priv->tmpdir_size), string);
	g_free (string);
}

static void
brasero_drive_properties_set_toggle_state (GtkWidget *toggle,
					   BraseroBurnFlag flag,
					   BraseroBurnFlag flags,
					   BraseroBurnFlag supported,
					   BraseroBurnFlag compulsory)
{
	if (!(supported & flag)) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), FALSE);
		gtk_widget_set_sensitive (toggle, FALSE);
		return;
	}

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), (flags & flag));
	gtk_widget_set_sensitive (toggle, (compulsory & flag) == 0);
}

void
brasero_drive_properties_set_flags (BraseroDriveProperties *self,
				    BraseroBurnFlag flags,
				    BraseroBurnFlag supported,
				    BraseroBurnFlag compulsory)
{
	BraseroDrivePropertiesPrivate *priv;

	priv = BRASERO_DRIVE_PROPERTIES_PRIVATE (self);

	flags &= BRASERO_DRIVE_PROPERTIES_FLAGS;
	supported &= BRASERO_DRIVE_PROPERTIES_FLAGS;
	compulsory &= BRASERO_DRIVE_PROPERTIES_FLAGS;

	/* flag properties */
	brasero_drive_properties_set_toggle_state (priv->dummy,
						   BRASERO_BURN_FLAG_DUMMY,
						   flags,
						   supported,
						   compulsory);
	brasero_drive_properties_set_toggle_state (priv->eject,
						   BRASERO_BURN_FLAG_EJECT,
						   flags,
						   supported,
						   compulsory);						   
	brasero_drive_properties_set_toggle_state (priv->burnproof,
						   BRASERO_BURN_FLAG_BURNPROOF,
						   flags,
						   supported,
						   compulsory);
	brasero_drive_properties_set_toggle_state (priv->notmp,
						   BRASERO_BURN_FLAG_NO_TMP_FILES,
						   flags,
						   supported,
						   compulsory);
}

void
brasero_drive_properties_set_drive (BraseroDriveProperties *self,
				    NautilusBurnDrive *drive,
				    gint64 default_rate)
{
	BraseroDrivePropertiesPrivate *priv;
	BraseroMedia media;
	GtkTreeModel *model;
	gchar *display_name;
	GtkTreeIter iter;
	gint64 max_rate;
	gchar *header;
	gint64 rate;
	gint64 step;

	priv = BRASERO_DRIVE_PROPERTIES_PRIVATE (self);

	/* set the header of the dialog */
	display_name = nautilus_burn_drive_get_name_for_display (drive);
	header = g_strdup_printf (_("Properties of %s"), display_name);
	g_free (display_name);

	gtk_window_set_title (GTK_WINDOW (self), header);
	g_free (header);

	/* Speed combo */
	media = NCB_MEDIA_GET_STATUS (drive);
	max_rate = NCB_MEDIA_GET_MAX_WRITE_RATE (drive);
	if (media & BRASERO_MEDIUM_CD)
		step = CD_RATE;
	else
		step = DVD_RATE;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->speed));
	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			    PROP_TEXT, _("Max speed"),
			    PROP_RATE, max_rate,
			    -1);
	gtk_combo_box_set_active (GTK_COMBO_BOX (priv->speed), 0);

	for (rate = max_rate; rate > step; rate -= step * 2) {
		GtkTreeIter iter;
		gchar *text;

		if (media & BRASERO_MEDIUM_DVD)
			text = g_strdup_printf (_("%d x (DVD)"),
						BRASERO_RATE_TO_SPEED_DVD (rate));
		else if (media & BRASERO_MEDIUM_CD)
			text = g_strdup_printf (_("%d x (CD)"),
						BRASERO_RATE_TO_SPEED_CD (rate));
		else
			text = g_strdup_printf (_("%d x (DVD) %d x (CD)"),
						BRASERO_RATE_TO_SPEED_DVD (rate),
						BRASERO_RATE_TO_SPEED_CD (rate));

		gtk_list_store_append (GTK_LIST_STORE (model), &iter);
		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				    PROP_TEXT, text,
				    PROP_RATE, rate,
				    -1);
		g_free (text);

		/* we do this to round things and get the closest possible speed */
		if ((rate / step) == (default_rate / step))
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (priv->speed), &iter);
	}
}

static void
brasero_drive_properties_init (BraseroDriveProperties *object)
{
	BraseroDrivePropertiesPrivate *priv;
	GtkCellRenderer *renderer;
	GtkTreeModel *model;
	GtkWidget *label;
	GtkWidget *box;

	priv = BRASERO_DRIVE_PROPERTIES_PRIVATE (object);

	gtk_window_set_default_size (GTK_WINDOW (object), 340, 250);
	gtk_dialog_set_has_separator (GTK_DIALOG (object), FALSE);
	gtk_dialog_add_buttons (GTK_DIALOG (object),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
				NULL);

	model = GTK_TREE_MODEL (gtk_list_store_new (PROP_NUM,
						    G_TYPE_STRING,
						    G_TYPE_INT64));

	priv->speed = gtk_combo_box_new_with_model (model);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (object)->vbox),
			    brasero_utils_pack_properties (_("<b>Burning speed</b>"),
							   priv->speed, NULL),
			    FALSE, FALSE, 0);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (priv->speed), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (priv->speed), renderer,
					"text", PROP_TEXT,
					NULL);

	priv->dummy = gtk_check_button_new_with_label (_("Simulate the burning"));
	priv->burnproof = gtk_check_button_new_with_label (_("Use burnproof (decrease the risk of failures)"));
	priv->eject = gtk_check_button_new_with_label (_("Eject after burning"));
	priv->notmp = gtk_check_button_new_with_label (_("Burn the image directly without saving it to disc"));

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (object)->vbox),
			    brasero_utils_pack_properties (_("<b>Options</b>"),
							   priv->eject,
							   priv->dummy,
							   priv->burnproof,
							   priv->notmp,
							   NULL),
			    FALSE,
			    FALSE, 0);

	priv->tmpdir = gtk_file_chooser_button_new (_("Directory for temporary files"),
						    GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);

	box = gtk_hbox_new (FALSE, 6);
	gtk_widget_show (box);

	label = gtk_label_new (_("Temporary directory free space:"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, FALSE, 0);

	priv->tmpdir_size = gtk_label_new ("");
	gtk_widget_show (priv->tmpdir_size);
	gtk_box_pack_start (GTK_BOX (box), priv->tmpdir_size, FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (object)->vbox),
			    brasero_utils_pack_properties (_("<b>Temporary files</b>"),
							   box,
							   priv->tmpdir,
							   NULL),
			    FALSE,
			    FALSE, 0);

	gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (priv->tmpdir),
				       g_get_tmp_dir ());

	gtk_widget_show_all (GTK_DIALOG (object)->vbox);
}

static void
brasero_drive_properties_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_drive_properties_class_init (BraseroDrivePropertiesClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	parent_class = GTK_DIALOG_CLASS (g_type_class_peek_parent (klass));

	g_type_class_add_private (klass, sizeof (BraseroDrivePropertiesPrivate));

	object_class->finalize = brasero_drive_properties_finalize;
}

GtkWidget *
brasero_drive_properties_new ()
{
	return GTK_WIDGET (g_object_new (BRASERO_TYPE_DRIVE_PROPERTIES, NULL));
}
