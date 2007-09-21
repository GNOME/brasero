/***************************************************************************
 *            brasero-sum-dialog.c
 *
 *  ven sep  1 19:35:13 2006
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <gtk/gtk.h>

#include <gtk/gtktreeview.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkmessagedialog.h>

#include <libgnomevfs/gnome-vfs.h>
#include <nautilus-burn-drive.h>

#include "brasero-sum-dialog.h"
#include "brasero-tool-dialog.h"
#include "brasero-ncb.h"
#include "brasero-xfer.h"
#include "burn-basics.h"
#include "burn-debug.h"
#include "burn.h"
#include "brasero-utils.h"

G_DEFINE_TYPE (BraseroSumDialog, brasero_sum_dialog, BRASERO_TYPE_TOOL_DIALOG);

struct _BraseroSumDialogPrivate {
	BraseroBurnSession *session;

	GtkWidget *md5_chooser;
	GtkWidget *md5_check;

	BraseroXferCtx *xfer_ctx;
};

static GtkDialogClass *parent_class = NULL;

static void
brasero_sum_dialog_md5_toggled (GtkToggleButton *button,
				BraseroSumDialog *self)
{
	gtk_widget_set_sensitive (self->priv->md5_chooser,
				  gtk_toggle_button_get_active (button));  
}

static void
brasero_sum_dialog_stop (BraseroSumDialog *self)
{
	if (self->priv->xfer_ctx)
		brasero_xfer_cancel (self->priv->xfer_ctx);
}

static void
brasero_sum_dialog_message (BraseroSumDialog *self,
			    const gchar *title,
			    const gchar *primary_message,
			    const gchar *secondary_message,
			    GtkMessageType type)
{
	GtkWidget *message;

	message = gtk_message_dialog_new (GTK_WINDOW (self),
					  GTK_DIALOG_MODAL |
					  GTK_DIALOG_DESTROY_WITH_PARENT,
					  type,
					  GTK_BUTTONS_CLOSE,
					  primary_message);

	gtk_window_set_title (GTK_WINDOW (message), title);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
						  secondary_message);
	gtk_dialog_run (GTK_DIALOG (message));
	gtk_widget_destroy (message);
}

static void
brasero_sum_dialog_message_error (BraseroSumDialog *self,
				  const GError *error)
{
	brasero_sum_dialog_message (self,
				    _("File integrity check error"),
				    _("The file integrity check cannot be performed:"),
				    error ? error->message:_("unknown error"),
				    GTK_MESSAGE_ERROR);
}

static void
brasero_sum_dialog_success (BraseroSumDialog *self)
{
	brasero_sum_dialog_message (self,
				    _("File integrity check success"),
				    _("The file integrity was performed successfully:"),
				    _("there seems to be no corrupted file on the disc."),
				    GTK_MESSAGE_INFO);
}

enum {
	BRASERO_SUM_DIALOG_PATH,
	BRASERO_SUM_DIALOG_NB_COL
};

static void
brasero_sum_dialog_corruption_warning (BraseroSumDialog *self,
				       GSList *wrong_sums)
{
	GSList *iter;
	GtkWidget *scroll;
	GtkWidget *tree;
	GtkWidget *message;
	GtkTreeModel *model;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	message = gtk_message_dialog_new_with_markup (GTK_WINDOW (self),
						      GTK_DIALOG_MODAL |
						      GTK_DIALOG_DESTROY_WITH_PARENT,
						      GTK_MESSAGE_ERROR,
						      GTK_BUTTONS_CLOSE,
						      _("<b><big>The following files appear to be corrupted:</big></b>"));

	gtk_window_set_title (GTK_WINDOW (message),  _("File integrity check error"));
	gtk_window_set_resizable (GTK_WINDOW (message), TRUE);
	gtk_widget_set_size_request (GTK_WIDGET (message), 440, 300);

	/* build a list */
	model = GTK_TREE_MODEL (gtk_list_store_new (BRASERO_SUM_DIALOG_NB_COL, G_TYPE_STRING));
	for (iter = wrong_sums; iter; iter = iter->next) {
		gchar *path;
		GtkTreeIter tree_iter;

		path = iter->data;
		gtk_list_store_append (GTK_LIST_STORE (model), &tree_iter);
		gtk_list_store_set (GTK_LIST_STORE (model), &tree_iter,
				    BRASERO_SUM_DIALOG_PATH, path,
				    -1);
	}

	tree = gtk_tree_view_new_with_model (model);
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (tree), TRUE);

	column = gtk_tree_view_column_new ();
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "text", BRASERO_SUM_DIALOG_PATH);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
	gtk_tree_view_column_set_title (column, _("Corrupted files"));

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll),
					     GTK_SHADOW_ETCHED_IN);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scroll), tree);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (message)->vbox),
			    scroll, 
			    TRUE,
			    TRUE,
			    0);

	gtk_widget_show_all (scroll);

	gtk_dialog_run (GTK_DIALOG (message));
	gtk_widget_destroy (message);
}

static gboolean
brasero_sum_dialog_progress_poll (gpointer user_data)
{
	BraseroSumDialog *self;
	gdouble progress = 0.0;
	gint64 written, total;

	self = BRASERO_SUM_DIALOG (user_data);

	if (!self->priv->xfer_ctx)
		return TRUE;

	brasero_xfer_get_progress (self->priv->xfer_ctx,
				   &written,
				   &total);

	progress = (gdouble) written / (gdouble) total;

	brasero_tool_dialog_set_progress (BRASERO_TOOL_DIALOG (self),
					  progress,
					  -1.0,
					  -1,
					  -1,
					  -1);
	return TRUE;
}

static BraseroBurnResult
brasero_sum_dialog_download (BraseroSumDialog *self,
			     GnomeVFSURI *vfsuri,
			     gchar **retval,
			     GError **error)
{
	BraseroBurnResult result;
	GnomeVFSURI *tmpuri;
	gchar *tmppath;
	gchar *uri;
	gint id;
	int fd;

	/* create the temp destination */
	tmppath = g_strdup_printf ("%s/"BRASERO_BURN_TMP_FILE_NAME,
				   g_get_tmp_dir ());
	fd = g_mkstemp (tmppath);
	if (fd < 0) {
		g_free (tmppath);
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("a temporary file couldn't be created"));
		return BRASERO_BURN_ERR;
	}
	close (fd);

	uri = gnome_vfs_get_uri_from_local_path (tmppath);
	if (!uri) {
		g_remove (tmppath);
		g_free (tmppath);

		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("URI is not valid"));
		return BRASERO_BURN_ERR;
	}

	tmpuri = gnome_vfs_uri_new (uri);
	g_free (uri);

	brasero_tool_dialog_set_action (BRASERO_TOOL_DIALOG (self),
					BRASERO_BURN_ACTION_FILE_COPY,
					_("Downloading md5 file"));

	id = g_timeout_add (500,
			    brasero_sum_dialog_progress_poll,
			    self);

	self->priv->xfer_ctx = brasero_xfer_new ();
	result = brasero_xfer (self->priv->xfer_ctx,
			       vfsuri,
			       tmpuri,
			       error);

	gnome_vfs_uri_unref (tmpuri);

	g_source_remove (id);
	brasero_xfer_free (self->priv->xfer_ctx);
	self->priv->xfer_ctx = NULL;

	if (result != BRASERO_BURN_OK) {
		g_remove (tmppath);
		g_free (tmppath);
		return result;
	}

	*retval = tmppath;
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_sum_dialog_get_file_checksum (BraseroSumDialog *self,
				      const gchar *file_path,
				      gchar **checksum,
				      GError **error)
{
	BraseroBurnResult result;
	GnomeVFSURI *vfsuri;
	gchar buffer [33];
	gchar *tmppath;
	gchar *uri;
	gchar *src;
	FILE *file;
	int read;

	/* see if this file needs downloading */
	uri = gnome_vfs_make_uri_from_input (file_path);
	if (!uri) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("URI is not valid"));
		return BRASERO_BURN_ERR;
	}

	tmppath = NULL;
	vfsuri = gnome_vfs_uri_new (uri);
	if (!gnome_vfs_uri_is_local (vfsuri)) {
		g_free (uri);

		result = brasero_sum_dialog_download (self,
						      vfsuri,
						      &tmppath,
						      error);
		gnome_vfs_uri_unref (vfsuri);

		if (result != BRASERO_BURN_CANCEL)
			return result;

		src = tmppath;
	}
	else {
		src = gnome_vfs_get_local_path_from_uri (uri);
		g_free (uri);

		gnome_vfs_uri_unref (vfsuri);
	}

	/* now get the md5 sum from the file */
	file = fopen (src, "r");
	if (!file) {
		if (tmppath)
			g_remove (tmppath);

		g_free (src);

		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     strerror (errno));
		return BRASERO_BURN_ERR;
	}

	read = fread (buffer, 1, sizeof (buffer) - 1, file);
	if (read)
		buffer [read] = '\0';

	if (tmppath)
		g_remove (tmppath);

	g_free (src);

	if (ferror (file)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     strerror (errno));

		fclose (file);
		return BRASERO_BURN_ERR;
	}

	fclose (file);

	*checksum = strdup (buffer);
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_sum_dialog_get_disc_checksum (BraseroSumDialog *self,
				      NautilusBurnDrive *drive,
				      gchar *checksum,
				      GError **error)
{
	BraseroTrack *track = NULL;
	BraseroBurnResult result;
	BraseroBurn *burn;

	track = brasero_track_new (BRASERO_TRACK_TYPE_DISC);
	brasero_track_set_drive_source (track, drive);
	brasero_track_set_checksum (track, BRASERO_CHECKSUM_MD5, checksum);
	brasero_burn_session_add_track (self->priv->session, track);

	burn = brasero_tool_dialog_get_burn (BRASERO_TOOL_DIALOG (self));
	result = brasero_burn_check (burn, self->priv->session, error);

	return result;
}

static gboolean
brasero_sum_dialog_check_md5_file (BraseroSumDialog *self,
				   NautilusBurnDrive *drive)
{
	BraseroBurnResult result;
	gchar *file_sum = NULL;
	GError *error = NULL;
	gchar *uri;

	/* get the sum from the file */
    	uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (self->priv->md5_chooser));
	if (!uri) {
		brasero_sum_dialog_message (self,
					    _("File integrity check error"),
					    _("The file integrity check cannot be performed:"),
					    error ? error->message:_("no md5 file was given."),
					    GTK_MESSAGE_ERROR);
		return FALSE;
	}

	result = brasero_sum_dialog_get_file_checksum (self, uri, &file_sum, &error);
	g_free (uri);

	if (result == BRASERO_BURN_CANCEL)
		return FALSE;

	if (result != BRASERO_BURN_OK) {
		brasero_sum_dialog_message_error (self, error);

		if (error)
			g_error_free (error);

		return FALSE;
	}

	result = brasero_sum_dialog_get_disc_checksum (self, drive, file_sum, &error);
	if (result == BRASERO_BURN_CANCEL) {
		g_free (file_sum);
		return FALSE;
	}

	if (result != BRASERO_BURN_OK) {
		g_free (file_sum);

		brasero_sum_dialog_message_error (self, error);

		if (error)
			g_error_free (error);
		return FALSE;
	}

	brasero_sum_dialog_success (self);
	return TRUE;
}

static gboolean
brasero_sum_dialog_check_disc_sum (BraseroSumDialog *self,
				   NautilusBurnDrive *drive)
{
	GSList *wrong_sums = NULL;
	BraseroBurnResult result;
	GError *error = NULL;
	BraseroTrack *track;
	BraseroBurn *burn;

	/* get the checksum */
	track = brasero_track_new (BRASERO_TRACK_TYPE_DISC);
	brasero_track_set_drive_source (track, drive);
	brasero_track_set_checksum (track,
				    BRASERO_CHECKSUM_MD5_FILE,
				    BRASERO_MD5_FILE);
	brasero_burn_session_add_track (self->priv->session, track);

	burn = brasero_tool_dialog_get_burn (BRASERO_TOOL_DIALOG (self));
	result = brasero_burn_check (burn, self->priv->session, &error);

	if (result == BRASERO_BURN_CANCEL) {
		if (error)
			g_error_free (error);

		return FALSE;
	}

	if (result == BRASERO_BURN_OK) {
		brasero_sum_dialog_success (self);
		return TRUE;
	}

	if (!error || error->code != BRASERO_BURN_ERROR_BAD_CHECKSUM) {
		brasero_sum_dialog_message_error (self, error);

		if (error)
			g_error_free (error);

		return FALSE;
	}

	g_error_free (error);

	wrong_sums = brasero_burn_session_get_wrong_checksums (self->priv->session);
	brasero_sum_dialog_corruption_warning (self, wrong_sums);
	g_slist_foreach (wrong_sums, (GFunc) g_free, NULL);
	g_slist_free (wrong_sums);

	return FALSE;
}

static gboolean
brasero_sum_dialog_activate (BraseroToolDialog *dialog,
			     NautilusBurnDrive *drive)
{
	BraseroSumDialog *self;

	self = BRASERO_SUM_DIALOG (dialog);

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->priv->md5_check)))
		brasero_sum_dialog_check_disc_sum (self, drive);
	else
		brasero_sum_dialog_check_md5_file (self, drive);

	brasero_tool_dialog_set_action (BRASERO_TOOL_DIALOG (self),
					BRASERO_BURN_ACTION_NONE,
					NULL);
	brasero_tool_dialog_set_progress (BRASERO_TOOL_DIALOG (self),
					  0.0,
					  0.0,
					  -1,
					  -1,
					  -1);
	return FALSE;
}

static void
brasero_sum_dialog_finalize (GObject *object)
{
	BraseroSumDialog *cobj;

	cobj = BRASERO_SUM_DIALOG (object);

	brasero_sum_dialog_stop (cobj);

	if (cobj->priv->session) {
		g_object_unref (cobj->priv->session);
		cobj->priv->session = NULL;
	}

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_sum_dialog_class_init (BraseroSumDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroToolDialogClass *tool_dialog_class = BRASERO_TOOL_DIALOG_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_sum_dialog_finalize;

	tool_dialog_class->activate = brasero_sum_dialog_activate;
}

static void
brasero_sum_dialog_init (BraseroSumDialog *obj)
{
	GtkWidget *box;

	obj->priv = g_new0 (BraseroSumDialogPrivate, 1);

	obj->priv->session = brasero_burn_session_new ();

	box = gtk_vbox_new (FALSE, 6);

	obj->priv->md5_check = gtk_check_button_new_with_label (_("Use a md5 file to check the disc"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (obj->priv->md5_check), FALSE);
	g_signal_connect (obj->priv->md5_check,
			  "toggled",
			  G_CALLBACK (brasero_sum_dialog_md5_toggled),
			  obj);

	gtk_box_pack_start (GTK_BOX (box),
			    obj->priv->md5_check,
			    TRUE,
			    TRUE,
			    0);

	obj->priv->md5_chooser = gtk_file_chooser_button_new (_("Open a md5 file"), GTK_FILE_CHOOSER_ACTION_OPEN);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (obj->priv->md5_chooser), FALSE);
	gtk_widget_set_sensitive (obj->priv->md5_chooser, FALSE);
	gtk_box_pack_start (GTK_BOX (box),
			    obj->priv->md5_chooser,
			    TRUE,
			    TRUE,
			    0);

	gtk_widget_show_all (box);
	brasero_tool_dialog_pack_options (BRASERO_TOOL_DIALOG (obj),
					  box,
					  NULL);

	brasero_tool_dialog_set_button (BRASERO_TOOL_DIALOG (obj),
					_("Check"),
					GTK_STOCK_FIND,
					NULL);
}

GtkWidget *
brasero_sum_dialog_new ()
{
	BraseroSumDialog *obj;
	
	obj = BRASERO_SUM_DIALOG (g_object_new (BRASERO_TYPE_SUM_DIALOG, NULL));
	gtk_window_set_title (GTK_WINDOW (obj), "Disc checking");
	
	return GTK_WIDGET (obj);
}
