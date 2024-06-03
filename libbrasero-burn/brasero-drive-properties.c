/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-burn
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-burn is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-burn authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-burn. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-burn is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include <gio/gio.h>

#include <gtk/gtk.h>

#include "brasero-medium.h"
#include "brasero-drive.h"

#include "brasero-misc.h"

#include "burn-basics.h"
#include "burn-debug.h"
#include "brasero-drive-properties.h"

typedef struct _BraseroDrivePropertiesPrivate BraseroDrivePropertiesPrivate;
struct _BraseroDrivePropertiesPrivate
{
	BraseroSessionCfg *session;
	gulong valid_sig;
	gulong output_sig;

	GtkWidget *speed;
	GtkWidget *dummy;
	GtkWidget *multi;
	GtkWidget *burnproof;
	GtkWidget *notmp;

	GtkWidget *tmpdir;
};

#define BRASERO_DRIVE_PROPERTIES_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_DRIVE_PROPERTIES, BraseroDrivePropertiesPrivate))

enum {
	TEXT_COL,
	RATE_COL,
	COL_NUM
};

enum {
	PROP_0,
	PROP_SESSION
};

G_DEFINE_TYPE (BraseroDriveProperties, brasero_drive_properties, GTK_TYPE_ALIGNMENT);

static void
brasero_drive_properties_no_tmp_toggled (GtkToggleButton *button,
					 BraseroDriveProperties *self)
{
	BraseroDrivePropertiesPrivate *priv;

	priv = BRASERO_DRIVE_PROPERTIES_PRIVATE (self);

	/* retrieve the flags */
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->notmp)))
		brasero_session_cfg_add_flags (priv->session,
					       BRASERO_BURN_FLAG_NO_TMP_FILES);
	else
		brasero_session_cfg_remove_flags (priv->session,
						  BRASERO_BURN_FLAG_NO_TMP_FILES);
}

static void
brasero_drive_properties_dummy_toggled (GtkToggleButton *button,
					BraseroDriveProperties *self)
{
	BraseroDrivePropertiesPrivate *priv;

	priv = BRASERO_DRIVE_PROPERTIES_PRIVATE (self);

	/* retrieve the flags */
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->dummy)))
		brasero_session_cfg_add_flags (priv->session,
					       BRASERO_BURN_FLAG_DUMMY);
	else
		brasero_session_cfg_remove_flags (priv->session,
						  BRASERO_BURN_FLAG_DUMMY);
}

static void
brasero_drive_properties_burnproof_toggled (GtkToggleButton *button,
					    BraseroDriveProperties *self)
{
	BraseroDrivePropertiesPrivate *priv;

	priv = BRASERO_DRIVE_PROPERTIES_PRIVATE (self);

	/* retrieve the flags */
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->burnproof)))
		brasero_session_cfg_add_flags (priv->session,
					       BRASERO_BURN_FLAG_BURNPROOF);
	else
		brasero_session_cfg_remove_flags (priv->session,
						  BRASERO_BURN_FLAG_BURNPROOF);
}

static void
brasero_drive_properties_multi_toggled (GtkToggleButton *button,
					BraseroDriveProperties *self)
{
	BraseroDrivePropertiesPrivate *priv;

	priv = BRASERO_DRIVE_PROPERTIES_PRIVATE (self);

	/* retrieve the flags */
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->multi)))
		brasero_session_cfg_add_flags (priv->session,
					       BRASERO_BURN_FLAG_MULTI);
	else
		brasero_session_cfg_remove_flags (priv->session,
						  BRASERO_BURN_FLAG_MULTI);
}

static void
brasero_drive_properties_set_tmpdir_info (BraseroDriveProperties *self,
					  const gchar *path)
{
	GFile *file;
	gchar *string;
	GFileInfo *info;
	gchar *string_size;
	guint64 vol_size = 0;
	BraseroDrivePropertiesPrivate *priv;

	priv = BRASERO_DRIVE_PROPERTIES_PRIVATE (self);

	/* get the volume free space */
	file = g_file_new_for_commandline_arg (path);
	if (!file) {
		BRASERO_BURN_LOG ("Impossible to retrieve size for %s", path);
		gtk_label_set_text (GTK_LABEL (priv->tmpdir), path);
		return;
	}

	info = g_file_query_filesystem_info (file,
					     G_FILE_ATTRIBUTE_FILESYSTEM_FREE,
					     NULL,
					     NULL);
	g_object_unref (file);

	if (!info) {
		BRASERO_BURN_LOG ("Impossible to retrieve size for %s", path);
		gtk_label_set_text (GTK_LABEL (priv->tmpdir), path);
		return;
	}

	vol_size = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE);
	g_object_unref (info);

	string_size = g_format_size (vol_size);
	/* Translators: the first %s is the path of the directory where brasero
	 * will store its temporary files; the second one is the size available */
	string = g_strdup_printf (_("%s: %s free"), path, string_size);
	g_free (string_size);

	gtk_label_set_text (GTK_LABEL (priv->tmpdir), string);
	g_free (string);
}

static gboolean
brasero_drive_properties_check_tmpdir (BraseroDriveProperties *self,
				       const gchar *path)
{
	GFile *file;
	GFileInfo *info;
	GError *error = NULL;
	const gchar *filesystem;

	file = g_file_new_for_commandline_arg (path);
	if (!file)
		return TRUE;

	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE,
				  G_FILE_QUERY_INFO_NONE,
				  NULL,
				  &error);
	if (error) {
		gint answer;
		gchar *string;
		GtkWidget *dialog;
		GtkWidget *toplevel;

		if (error)
			return TRUE;

		/* Tell the user what went wrong */
		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
		dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
						 GTK_DIALOG_DESTROY_WITH_PARENT |
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_WARNING,
						 GTK_BUTTONS_NONE,
						 _("Do you really want to choose this location?"));

		gtk_window_set_icon_name (GTK_WINDOW (dialog),
					  gtk_window_get_icon_name (GTK_WINDOW (toplevel)));

		string = g_strdup_printf ("%s.", error->message);
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", string);
		g_error_free (error);
		g_free (string);

		gtk_dialog_add_buttons (GTK_DIALOG (dialog),
					_("_Keep Current Location"), GTK_RESPONSE_CANCEL,
					_("_Change Location"), GTK_RESPONSE_OK,
					NULL);

		gtk_widget_show_all (dialog);
		answer = gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_object_unref (info);
		g_object_unref (file);
		if (answer != GTK_RESPONSE_OK)
			return TRUE;

		return FALSE;
	}

	if (!g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE)) {
		gint answer;
		gchar *string;
		GtkWidget *dialog;
		GtkWidget *toplevel;

		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
		dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
						 GTK_DIALOG_DESTROY_WITH_PARENT |
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_WARNING,
						 GTK_BUTTONS_NONE,
						 _("Do you really want to choose this location?"));

		gtk_window_set_icon_name (GTK_WINDOW (dialog),
					  gtk_window_get_icon_name (GTK_WINDOW (toplevel)));

		string = g_strdup_printf ("%s.", _("You do not have the required permission to write at this location"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", string);
		g_free (string);

		gtk_dialog_add_buttons (GTK_DIALOG (dialog),
					_("_Keep Current Location"), GTK_RESPONSE_CANCEL,
					_("_Change Location"), GTK_RESPONSE_OK,
					NULL);

		gtk_widget_show_all (dialog);
		answer = gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_object_unref (info);
		g_object_unref (file);
		if (answer != GTK_RESPONSE_OK)
			return TRUE;

		return FALSE;
	}

	g_object_unref (info);
	info = g_file_query_filesystem_info (file,
					     G_FILE_ATTRIBUTE_FILESYSTEM_TYPE,
					     NULL,
					     &error);
	g_object_unref (file);

	/* NOTE/FIXME: also check, probably best at start or in a special dialog
	 * whether quotas or any other limitation enforced on the system may not
	 * get in out way. Think getrlimit (). */

	/* check the filesystem type: the problem here is that some
	 * filesystems have a maximum file size limit of 4 GiB and more than
	 * often we need a temporary file size of 4 GiB or more. */
	filesystem = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_FILESYSTEM_TYPE);
	if (!g_strcmp0 (filesystem, "msdos")) {
		gint answer;
		GtkWidget *dialog;
		GtkWidget *toplevel;

		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
		dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
						 GTK_DIALOG_DESTROY_WITH_PARENT |
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_WARNING,
						 GTK_BUTTONS_NONE,
						 _("Do you really want to choose this location?"));

		gtk_window_set_icon_name (GTK_WINDOW (dialog),
					  gtk_window_get_icon_name (GTK_WINDOW (toplevel)));

		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  _("The filesystem on this volume does not support large files (size over 2 GiB)."
							    "\nThis can be a problem when writing DVDs or large images."));

		gtk_dialog_add_buttons (GTK_DIALOG (dialog),
					_("_Keep Current Location"), GTK_RESPONSE_CANCEL,
					_("_Change Location"), GTK_RESPONSE_OK,
					NULL);

		gtk_widget_show_all (dialog);
		answer = gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_object_unref (info);
		if (answer != GTK_RESPONSE_OK)
			return TRUE;
	}
	else if (info)
		g_object_unref (info);

	return FALSE;
}

static void
brasero_drive_properties_tmpdir_clicked (GtkButton *button,
					 BraseroDriveProperties *self)
{
	GtkWidget *parent;
	const gchar *path;
	GtkWidget *chooser;
	GtkResponseType res;
	const gchar *new_path;
	BraseroDrivePropertiesPrivate *priv;

	priv = BRASERO_DRIVE_PROPERTIES_PRIVATE (self);

	parent = gtk_widget_get_toplevel (GTK_WIDGET (button));
	chooser = gtk_file_chooser_dialog_new (_("Location for Temporary Files"),
					       GTK_WINDOW (parent),
					       GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
					       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					       GTK_STOCK_OK, GTK_RESPONSE_OK,
					       NULL);

	path = brasero_burn_session_get_tmpdir (BRASERO_BURN_SESSION (priv->session));
	gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (chooser), path);
	res = gtk_dialog_run (GTK_DIALOG (chooser));
	if (res != GTK_RESPONSE_OK) {
		gtk_widget_destroy (chooser);
		return;
	}

	new_path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
	if (!new_path || !strcmp (new_path, path)) {
		gtk_widget_destroy (chooser);
		return;
	}

	if (!brasero_drive_properties_check_tmpdir (self, new_path)) {
		brasero_burn_session_set_tmpdir (BRASERO_BURN_SESSION (priv->session), new_path);
		brasero_drive_properties_set_tmpdir_info (self, new_path);
	}

	gtk_widget_destroy (chooser);
}

static void
brasero_drive_properties_set_tmpdir (BraseroDriveProperties *self,
				     const gchar *path)
{
	if (!path)
		path = g_get_tmp_dir ();

	brasero_drive_properties_set_tmpdir_info (self, path);
}

static void
brasero_drive_properties_set_flags (BraseroDriveProperties *self,
				    BraseroBurnFlag flags,
				    BraseroBurnFlag supported,
				    BraseroBurnFlag compulsory);

static void
brasero_drive_properties_set_toggle_state (BraseroDriveProperties *self,
					   GtkWidget *toggle,
					   BraseroBurnFlag flag,
					   BraseroBurnFlag flags,
					   BraseroBurnFlag supported,
					   BraseroBurnFlag compulsory)
{
	if (!(supported & flag)) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), FALSE);
		gtk_widget_set_sensitive (toggle, FALSE);
		gtk_widget_hide (toggle);
		return;
	}

	gtk_widget_show (toggle);
	g_signal_handlers_block_by_func (toggle,
					 brasero_drive_properties_set_flags,
					 self);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), (flags & flag));
	g_signal_handlers_unblock_by_func (toggle,
					   brasero_drive_properties_set_flags,
					   self);

	gtk_widget_set_sensitive (toggle, (compulsory & flag) == 0);
}

static void
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
	brasero_drive_properties_set_toggle_state (self,
						   priv->dummy,
						   BRASERO_BURN_FLAG_DUMMY,
						   flags,
						   supported,
						   compulsory);
	brasero_drive_properties_set_toggle_state (self,
						   priv->burnproof,
						   BRASERO_BURN_FLAG_BURNPROOF,
						   flags,
						   supported,
						   compulsory);
	brasero_drive_properties_set_toggle_state (self,
						   priv->notmp,
						   BRASERO_BURN_FLAG_NO_TMP_FILES,
						   flags,
						   supported,
						   compulsory);
	brasero_drive_properties_set_toggle_state (self,
						   priv->multi,
						   BRASERO_BURN_FLAG_MULTI,
						   flags,
						   supported,
						   compulsory);
}

static gint64
brasero_drive_properties_get_rate (BraseroDriveProperties *self)
{
	BraseroDrivePropertiesPrivate *priv;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gint64 rate;

	priv = BRASERO_DRIVE_PROPERTIES_PRIVATE (self);

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->speed));
	if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (priv->speed), &iter)
	&&  !gtk_tree_model_get_iter_first (model, &iter))
		return 0;

	gtk_tree_model_get (model, &iter,
			    RATE_COL, &rate,
			    -1);
	return rate;
}

static void
brasero_drive_properties_rate_changed_cb (GtkComboBox *combo,
					  BraseroDriveProperties *self)
{
	BraseroDrivePropertiesPrivate *priv;
	guint64 rate;

	priv = BRASERO_DRIVE_PROPERTIES_PRIVATE (self);

	rate = brasero_drive_properties_get_rate (self);
	if (!rate)
		return;

	brasero_burn_session_set_rate (BRASERO_BURN_SESSION (priv->session), rate);
}

static gchar *
brasero_drive_properties_format_disc_speed (BraseroMedia media,
					    gint64 rate)
{
	gchar *text;

	if (media & BRASERO_MEDIUM_DVD)
		/* Translators %s.1f is the speed used to burn */
		text = g_strdup_printf (_("%.1f\303\227 (DVD)"),
					BRASERO_RATE_TO_SPEED_DVD (rate));
	else if (media & BRASERO_MEDIUM_CD)
		/* Translators %s.1f is the speed used to burn */
		text = g_strdup_printf (_("%.1f\303\227 (CD)"),
					BRASERO_RATE_TO_SPEED_CD (rate));
	else if (media & BRASERO_MEDIUM_BD)
		/* Translators %s.1f is the speed used to burn. BD = Blu Ray*/
		text = g_strdup_printf (_("%.1f\303\227 (BD)"),
					BRASERO_RATE_TO_SPEED_BD (rate));
	else
		/* Translators %s.1f is the speed used to burn for every medium
		 * type. BD = Blu Ray*/
		text = g_strdup_printf (_("%.1f\303\227 (BD) %.1f\303\227 (DVD) %.1f\303\227 (CD)"),
					BRASERO_RATE_TO_SPEED_BD (rate),
					BRASERO_RATE_TO_SPEED_DVD (rate),
					BRASERO_RATE_TO_SPEED_CD (rate));

	return text;
}

static void
brasero_drive_properties_set_drive (BraseroDriveProperties *self,
				    BraseroDrive *drive,
				    gint64 default_rate)
{
	BraseroDrivePropertiesPrivate *priv;
	BraseroMedium *medium;
	BraseroMedia media;
	GtkTreeModel *model;
	GtkTreeIter iter;
	guint64 *rates;
	gchar *text;
	guint i;

	priv = BRASERO_DRIVE_PROPERTIES_PRIVATE (self);

	/* Speed combo */
	medium = brasero_drive_get_medium (drive);
	media = brasero_medium_get_status (medium);
	if (media & BRASERO_MEDIUM_FILE)
		return;

	rates = brasero_medium_get_write_speeds (medium);
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->speed));
	gtk_list_store_clear (GTK_LIST_STORE (model));

	if (!rates) {
		gtk_widget_set_sensitive (priv->speed, FALSE);
		gtk_list_store_append (GTK_LIST_STORE (model), &iter);
		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				    TEXT_COL, _("Impossible to retrieve speeds"),
				    RATE_COL, 1764, /* Speed 1 */
				    -1);
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (priv->speed), &iter);
		return;
	}

	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			    TEXT_COL, _("Maximum speed"),
			    RATE_COL, rates [0],
			    -1);

	/* fill model */
	for (i = 0; rates [i] != 0; i ++) {
		text = brasero_drive_properties_format_disc_speed (media, rates [i]);
		gtk_list_store_append (GTK_LIST_STORE (model), &iter);
		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				    TEXT_COL, text,
				    RATE_COL, rates [i],
				    -1);
		g_free (text);
	}
	g_free (rates);

	/* Set active one preferably max speed */
	gtk_tree_model_get_iter_first (model, &iter);
	do {
		gint64 rate;

		gtk_tree_model_get (model, &iter,
				    RATE_COL, &rate,
				    -1);

		/* we do this to round things and get the closest possible speed */
		if ((rate / 1024) == (default_rate / 1024)) {
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (priv->speed), &iter);
			break;
		}

	} while (gtk_tree_model_iter_next (model, &iter));

	/* make sure at least one is active */
	if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX (priv->speed), &iter)) {
		gtk_tree_model_get_iter_first (model, &iter);
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (priv->speed), &iter);
	}
}

static void
brasero_drive_properties_update (BraseroDriveProperties *self)
{
	BraseroBurnFlag compulsory = BRASERO_BURN_FLAG_NONE;
	BraseroBurnFlag supported = BRASERO_BURN_FLAG_NONE;
	BraseroDrivePropertiesPrivate *priv;
	BraseroBurnFlag flags;

	priv = BRASERO_DRIVE_PROPERTIES_PRIVATE (self);
	brasero_drive_properties_set_drive (self,
					    brasero_burn_session_get_burner (BRASERO_BURN_SESSION (priv->session)),
					    brasero_burn_session_get_rate (BRASERO_BURN_SESSION (priv->session)));

	flags = brasero_burn_session_get_flags (BRASERO_BURN_SESSION (priv->session));
	brasero_burn_session_get_burn_flags (BRASERO_BURN_SESSION (priv->session),
					     &supported,
					     &compulsory);
	brasero_drive_properties_set_flags (self,
					    flags,
					    supported,
					    compulsory);
	brasero_drive_properties_set_tmpdir (self, brasero_burn_session_get_tmpdir (BRASERO_BURN_SESSION (priv->session)));
}

static void
brasero_drive_properties_is_valid_cb (BraseroSessionCfg *session,
				      BraseroDriveProperties *self)
{
	BraseroDrivePropertiesPrivate *priv;
	BraseroBurnFlag compulsory;
	BraseroBurnFlag supported;
	BraseroBurnFlag flags;

	priv = BRASERO_DRIVE_PROPERTIES_PRIVATE (self);

	flags = brasero_burn_session_get_flags (BRASERO_BURN_SESSION (priv->session));
	brasero_burn_session_get_burn_flags (BRASERO_BURN_SESSION (priv->session),
					     &supported,
					     &compulsory);
	brasero_drive_properties_set_flags (self,
					    flags,
					    supported,
					    compulsory);
}

static void
brasero_drive_properties_output_changed_cb (BraseroSessionCfg *session,
					    BraseroMedium *former,
					    BraseroDriveProperties *self)
{
	BraseroDrivePropertiesPrivate *priv;

	priv = BRASERO_DRIVE_PROPERTIES_PRIVATE (self);

	/* if the drive changed update rate but only if the drive changed that's
	 * why we don't do it when the is-valid signal is emitted. */
	brasero_drive_properties_set_drive (self,
					    brasero_burn_session_get_burner (BRASERO_BURN_SESSION (priv->session)),
					    brasero_burn_session_get_rate (BRASERO_BURN_SESSION (priv->session)));
}

static void
brasero_drive_properties_init (BraseroDriveProperties *object)
{
	BraseroDrivePropertiesPrivate *priv;
	GtkCellRenderer *renderer;
	GtkTreeModel *model;
	GtkWidget *button;
	GtkWidget *image;
	GtkWidget *label;
	GtkWidget *vbox;
	GtkWidget *box;
	gchar *string;

	priv = BRASERO_DRIVE_PROPERTIES_PRIVATE (object);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (object), vbox);

	model = GTK_TREE_MODEL (gtk_list_store_new (COL_NUM,
						    G_TYPE_STRING,
						    G_TYPE_INT64));

	priv->speed = gtk_combo_box_new_with_model (model);
	gtk_widget_show (priv->speed);
	string = g_strdup_printf ("<b>%s</b>", _("Burning speed"));
	gtk_box_pack_start (GTK_BOX (vbox),
			    brasero_utils_pack_properties (string,
							   priv->speed, NULL),
			    FALSE, FALSE, 0);
	g_free (string);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (priv->speed), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (priv->speed), renderer,
					"text", TEXT_COL,
					NULL);

	priv->dummy = gtk_check_button_new_with_mnemonic (_("_Simulate before burning"));
	gtk_widget_set_tooltip_text (priv->dummy, _("Brasero will simulate the burning and, if it is successful, go on with actual burning after 10 seconds"));
	gtk_widget_show (priv->dummy);
	priv->burnproof = gtk_check_button_new_with_mnemonic (_("Use burn_proof (decrease the risk of failures)"));
	gtk_widget_show (priv->burnproof);
	priv->notmp = gtk_check_button_new_with_mnemonic (_("Burn the image directly _without saving it to disc"));
	gtk_widget_show (priv->notmp);
	priv->multi = gtk_check_button_new_with_mnemonic (_("Leave the disc _open to add other files later"));
	gtk_widget_set_tooltip_text (priv->multi, _("Allow to add more data to the disc later"));
	gtk_widget_show (priv->multi);

	g_signal_connect (priv->dummy,
			  "toggled",
			  G_CALLBACK (brasero_drive_properties_dummy_toggled),
			  object);
	g_signal_connect (priv->burnproof,
			  "toggled",
			  G_CALLBACK (brasero_drive_properties_burnproof_toggled),
			  object);
	g_signal_connect (priv->multi,
			  "toggled",
			  G_CALLBACK (brasero_drive_properties_multi_toggled),
			  object);
	g_signal_connect (priv->notmp,
			  "toggled",
			  G_CALLBACK (brasero_drive_properties_no_tmp_toggled),
			  object);

	string = g_strdup_printf ("<b>%s</b>", _("Options"));
	gtk_box_pack_start (GTK_BOX (vbox),
			    brasero_utils_pack_properties (string,
							   priv->dummy,
							   priv->burnproof,
							   priv->multi,
							   priv->notmp,
							   NULL),
			    FALSE,
			    FALSE, 0);
	g_free (string);

	label = gtk_label_new_with_mnemonic (_("Location for _Temporary Files"));
	gtk_widget_show (label);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_START);
	gtk_widget_show (label);

	priv->tmpdir = label;

	image = gtk_image_new_from_icon_name ("folder", GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (image);

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_show (box);
	gtk_box_pack_start (GTK_BOX (box), image, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

	button = gtk_button_new ();
	gtk_widget_show (button);
	gtk_container_add (GTK_CONTAINER (button), box);
	gtk_widget_set_tooltip_text (button, _("Set the directory where to store temporary files"));

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_show (box);

	string = g_strdup_printf ("<b>%s</b>", _("Temporary files"));
	gtk_box_pack_start (GTK_BOX (vbox),
			    brasero_utils_pack_properties (string,
							   box,
							   button,
							   NULL),
			    FALSE,
			    FALSE, 0);
	g_free (string);
	gtk_widget_show (vbox);

	g_signal_connect (button,
			  "clicked",
			  G_CALLBACK (brasero_drive_properties_tmpdir_clicked),
			  object);
	g_signal_connect (priv->speed,
			  "changed",
			  G_CALLBACK (brasero_drive_properties_rate_changed_cb),
			  object);
}

static void
brasero_drive_properties_finalize (GObject *object)
{
	BraseroDrivePropertiesPrivate *priv;
	
	priv = BRASERO_DRIVE_PROPERTIES_PRIVATE (object);
	if (priv->valid_sig) {
		g_signal_handler_disconnect (priv->session, priv->valid_sig);
		priv->valid_sig = 0;
	}
	if (priv->output_sig) {
		g_signal_handler_disconnect (priv->session, priv->output_sig);
		priv->output_sig = 0;
	}
	if (priv->session) {
		g_object_unref (priv->session);
		priv->session = NULL;
	}

	G_OBJECT_CLASS (brasero_drive_properties_parent_class)->finalize (object);
}

static void
brasero_drive_properties_set_property (GObject *object,
				       guint property_id,
				       const GValue *value,
				       GParamSpec *pspec)
{
	BraseroDrivePropertiesPrivate *priv;

	priv = BRASERO_DRIVE_PROPERTIES_PRIVATE (object);

	switch (property_id) {
	case PROP_SESSION: /* Readable and only writable at creation time */
		priv->session = g_object_ref (g_value_get_object (value));

		brasero_drive_properties_update (BRASERO_DRIVE_PROPERTIES (object));
		priv->valid_sig = g_signal_connect (priv->session,
						    "is-valid",
						    G_CALLBACK (brasero_drive_properties_is_valid_cb),
						    object);
		priv->output_sig = g_signal_connect (priv->session,
						     "output-changed",
						     G_CALLBACK (brasero_drive_properties_output_changed_cb),
						     object);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
brasero_drive_properties_get_property (GObject *object,
				       guint property_id,
				       GValue *value,
				       GParamSpec *pspec)
{
	BraseroDrivePropertiesPrivate *priv;

	priv = BRASERO_DRIVE_PROPERTIES_PRIVATE (object);

	switch (property_id) {
	case PROP_SESSION:
		g_object_ref (priv->session);
		g_value_set_object (value, priv->session);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
brasero_drive_properties_class_init (BraseroDrivePropertiesClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroDrivePropertiesPrivate));

	object_class->finalize = brasero_drive_properties_finalize;
	object_class->set_property = brasero_drive_properties_set_property;
	object_class->get_property = brasero_drive_properties_get_property;

	g_object_class_install_property (object_class,
					 PROP_SESSION,
					 g_param_spec_object ("session",
							      "The session",
							      "The session to work with",
							      BRASERO_TYPE_BURN_SESSION,
							      G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY));
}

GtkWidget *
brasero_drive_properties_new (BraseroSessionCfg *session)
{
	return g_object_new (BRASERO_TYPE_DRIVE_PROPERTIES,
			     "session", session,
			     NULL);
}
