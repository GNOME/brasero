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
#include "burn-basics.h"
#include "burn-sum.h"
#include "brasero-ncb.h"
#include "burn-md5.h"
#include "burn-xfer.h"
#include "brasero-sum-check.h"
#include "utils.h"

static void brasero_sum_dialog_class_init (BraseroSumDialogClass *klass);
static void brasero_sum_dialog_init (BraseroSumDialog *sp);
static void brasero_sum_dialog_finalize (GObject *object);

struct _FileSumData {
	GPid pid;
	gint channel_id;

	GMainLoop *loop;

	GSList *wrong_sums;

	gint files_nb;
	gint files_num;
};
typedef struct _FileSumData FileSumData;

struct _DiscSumData {
	BraseroMD5Ctx *md5_ctx;
	gchar *device;
	gchar *md5;

	gint64 total_bytes;

	GMainLoop *loop;

	gboolean success;
	GError *error;
};
typedef struct _DiscSumData DiscSumData;

struct _BraseroSumDialogPrivate {
	GtkWidget *md5_chooser;
	GtkWidget *md5_check;

	BraseroSumCheckCtx *file_ctx;
	BraseroXferCtx *xfer_ctx;
	DiscSumData *disc_data;

	gboolean mounted_by_us;
	gchar *mount_point;
};

static GtkDialogClass *parent_class = NULL;

static gboolean brasero_sum_dialog_cancel (BraseroToolDialog *dialog);
static gboolean brasero_sum_dialog_activate (BraseroToolDialog *dialog,
					       NautilusBurnDrive *drive);
static void brasero_sum_dialog_media_changed (BraseroToolDialog *dialog,
					       NautilusBurnMediaType media);

static void
brasero_sum_dialog_md5_toggled (GtkToggleButton *button,
				BraseroSumDialog *self);

GType
brasero_sum_dialog_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroSumDialogClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_sum_dialog_class_init,
			NULL,
			NULL,
			sizeof (BraseroSumDialog),
			0,
			(GInstanceInitFunc)brasero_sum_dialog_init,
		};

		type = g_type_register_static (BRASERO_TYPE_TOOL_DIALOG, 
					       "BraseroSumDialog",
					       &our_info,
					       0);
	}

	return type;
}

static void
brasero_sum_dialog_class_init (BraseroSumDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroToolDialogClass *tool_dialog_class = BRASERO_TOOL_DIALOG_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_sum_dialog_finalize;

	tool_dialog_class->activate = brasero_sum_dialog_activate;
	tool_dialog_class->media_changed = brasero_sum_dialog_media_changed;
	tool_dialog_class->cancel = brasero_sum_dialog_cancel;
}

static void
brasero_sum_dialog_init (BraseroSumDialog *obj)
{
	GtkWidget *box;

	obj->priv = g_new0 (BraseroSumDialogPrivate, 1);

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
					GTK_STOCK_FIND);
}

static void
brasero_sum_dialog_stop_disc_ops (DiscSumData *data)
{
	if (data->md5_ctx)
		brasero_md5_cancel (data->md5_ctx);

	if (data->loop && g_main_loop_is_running (data->loop))
		g_main_loop_quit (data->loop);
}

static void
brasero_sum_dialog_stop (BraseroSumDialog *self)
{
	if (self->priv->file_ctx)
		brasero_sum_check_cancel (self->priv->file_ctx);

	if (self->priv->xfer_ctx)
		brasero_xfer_cancel (self->priv->xfer_ctx);

	if (self->priv->disc_data)
		brasero_sum_dialog_stop_disc_ops (self->priv->disc_data);
}

static void
brasero_sum_dialog_finalize (GObject *object)
{
	BraseroSumDialog *cobj;

	cobj = BRASERO_SUM_DIALOG (object);

	brasero_sum_dialog_stop (cobj);

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkWidget *
brasero_sum_dialog_new ()
{
	BraseroSumDialog *obj;
	
	obj = BRASERO_SUM_DIALOG (g_object_new (BRASERO_TYPE_SUM_DIALOG, NULL));
	
	return GTK_WIDGET (obj);
}

static void
brasero_sum_dialog_md5_toggled (GtkToggleButton *button,
				BraseroSumDialog *self)
{
	gtk_widget_set_sensitive (self->priv->md5_chooser,
				  gtk_toggle_button_get_active (button));  
}

static gboolean
brasero_sum_dialog_cancel (BraseroToolDialog *dialog)
{
	BraseroSumDialog *self;

	self = BRASERO_SUM_DIALOG (dialog);

	/* cancel spawned process and don't return a success dialog */
	brasero_sum_dialog_stop (self);

	return TRUE;
}

static void
brasero_sum_dialog_media_changed (BraseroToolDialog *dialog,
				  NautilusBurnMediaType media)
{
	
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
brasero_sum_dialog_drive_umount_error (BraseroSumDialog *self,
				       const gchar *error)
{
	brasero_sum_dialog_message (self,
				    _("Error while unmounting disc"),
				    _("The file integrity check cannot be performed:"),
				    error,
				    GTK_MESSAGE_ERROR);
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

static void
brasero_sum_dialog_disc_corrupted_error (BraseroSumDialog *self)
{
	brasero_sum_dialog_message (self,
				    _("Disc data corrupted"),
				    _("The file integrity check showed errors:"),
				    _("some files may be corrupted on the disc"),
				    GTK_MESSAGE_ERROR);
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

	self = BRASERO_SUM_DIALOG (user_data);

	if (self->priv->disc_data) {
		DiscSumData *disc_data = self->priv->disc_data;
		gint64 bytes;

		bytes = brasero_md5_get_written (disc_data->md5_ctx);
		progress = (gdouble) bytes / (gdouble) disc_data->total_bytes;
	}
	else if (self->priv->xfer_ctx) {
		gint64 written, total;

		brasero_xfer_get_progress (self->priv->xfer_ctx,
					   &written,
					   &total);

		progress = (gdouble) written / (gdouble) total;
	}
	else if (self->priv->file_ctx) {
		gint checked, total;

		brasero_sum_check_progress (self->priv->file_ctx,
					    &checked,
					    &total);

		progress = (gdouble) checked / (gdouble) total;
	}

	brasero_tool_dialog_set_progress (BRASERO_TOOL_DIALOG (self),
					  progress,
					  -1.0,
					  -1,
					  -1,
					  -1);
	return TRUE;
}

static gchar *
brasero_sum_dialog_download (BraseroSumDialog *self,
			     GnomeVFSURI *vfsuri,
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
		return NULL;
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
		return NULL;
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

	if (result != BRASERO_BURN_OK) {
		g_remove (tmppath);
		g_free (tmppath);
		return NULL;
	}

	g_source_remove (id);
	brasero_xfer_free (self->priv->xfer_ctx);
	self->priv->xfer_ctx = NULL;

	return tmppath;
}

static gboolean
brasero_sum_dialog_from_file (BraseroSumDialog *self,
			      const gchar *file_path,
			      gchar *buffer,
			      GError **error)
{
	BraseroBurnResult result;
	GnomeVFSURI *vfsuri;
	gchar *tmppath;
	gchar *uri;
	gchar *src;
	FILE *file;
	int c;

	/* see if this file needs downloading */
	uri = gnome_vfs_make_uri_from_input (file_path);
	if (!uri) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("URI is not valid"));
		return BRASERO_BURN_ERR;
	}

	vfsuri = gnome_vfs_uri_new (uri);
	if (!gnome_vfs_uri_is_local (vfsuri)) {
		g_free (uri);

		tmppath = brasero_sum_dialog_download (self,
						       vfsuri,
						       error);
		gnome_vfs_uri_unref (vfsuri);

		if (!tmppath)
			return FALSE;

		src = tmppath;
	}
	else {
		tmppath = NULL;

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
		return FALSE;
	}

	result = TRUE;
	while ((c = fgetc (file)) != EOF) {
		if (c == ' ' || c == '\t' || c =='\n')
			break;

		*(buffer ++) = (unsigned char) c;
	}

	if (tmppath)
		g_remove (tmppath);

	g_free (src);

	if (ferror (file)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     strerror (errno));

		fclose (file);
		return FALSE;
	}

	fclose (file);
	return TRUE;
}

static gpointer
brasero_sum_dialog_disc_thread (gpointer user_data)
{
	DiscSumData *data = user_data;
	
	brasero_md5_sum_to_string (data->md5_ctx,
				   data->device,
				   data->md5,
				   &data->error);

	g_main_loop_quit (data->loop);
	g_thread_exit (NULL);

	return NULL;
}

static gboolean
brasero_sum_dialog_get_disc_md5 (BraseroSumDialog *self,
				 NautilusBurnDrive *drive,
				 gchar *md5,
				 GError **error)
{
	gboolean unmounted_by_us = FALSE;
	DiscSumData data;
	GThread *thread;
	gint id;

	/* unmount the drive is need be */
	if (nautilus_burn_drive_is_mounted (drive)) {
		GError *error = NULL;

		if (!NCB_DRIVE_UNMOUNT (drive, &error)) {
			brasero_sum_dialog_drive_umount_error (self, error ? error->message : _("Unknown error"));
			return FALSE;
		}

		unmounted_by_us = TRUE;
	}

	/* get the sum of the disc */
	data.total_bytes = NCB_MEDIA_GET_SIZE (drive);
	data.device = (gchar*) NCB_DRIVE_GET_DEVICE (drive);
	data.md5_ctx = brasero_md5_new ();
	data.error = NULL;
	data.md5 = md5;

	id = g_timeout_add (500,
			    brasero_sum_dialog_progress_poll,
			    self);

	brasero_tool_dialog_set_action (BRASERO_TOOL_DIALOG (self),
					BRASERO_BURN_ACTION_CHECKSUM,
					_("Creating disc checksum"));

	thread = g_thread_create (brasero_sum_dialog_disc_thread,
				  &data,
				  TRUE,
				  error);

	if (!thread) {
		brasero_md5_free (data.md5_ctx);

		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("a thread could not a created"));
		return FALSE;
	}

	data.loop = g_main_loop_new (NULL, FALSE);

	self->priv->disc_data = &data;
	g_main_loop_run (data.loop);
	self->priv->disc_data = NULL;

	g_source_remove (id);
	g_main_loop_unref (data.loop);
	brasero_md5_free (data.md5_ctx);

	if (unmounted_by_us)
		NCB_DRIVE_MOUNT (drive, NULL);

	if (data.error) {
		g_propagate_error (error, data.error);
		
		return FALSE;
	}

	return TRUE;
}

static gboolean
brasero_sum_dialog_check_md5_file (BraseroSumDialog *self,
				   NautilusBurnDrive *drive)
{
	gchar file_sum [MD5_STRING_LEN + 1] = {0,}, disc_sum [MD5_STRING_LEN + 1] = {0,};
	GError *error = NULL;
	gboolean result;
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

	result = brasero_sum_dialog_from_file (self,
					       uri,
					       file_sum,
					       &error);
	g_free (uri);

	brasero_tool_dialog_set_action (BRASERO_TOOL_DIALOG (self),
					BRASERO_BURN_ACTION_NONE,
					NULL);
	brasero_tool_dialog_set_progress (BRASERO_TOOL_DIALOG (self),
					  0.0,
					  0.0,
					  -1,
					  -1,
					  -1);

	if (!result) {
		brasero_sum_dialog_message_error (self, error);

		g_error_free (error);
		return FALSE;
	}

	result = brasero_sum_dialog_get_disc_md5 (self,
						  drive,
						  disc_sum,
						  &error);

	brasero_tool_dialog_set_action (BRASERO_TOOL_DIALOG (self),
					BRASERO_BURN_ACTION_NONE,
					NULL);
	brasero_tool_dialog_set_progress (BRASERO_TOOL_DIALOG (self),
					  -1.0,
					  -1.0,
					  -1,
					  -1,
					  -1);

	if (!result) {
		brasero_sum_dialog_message_error (self, error);
		g_error_free (error);
		return FALSE;
	}

	/* both sums must be equal */
	if (strcmp (disc_sum, file_sum)) {
		brasero_sum_dialog_disc_corrupted_error (self);
		return FALSE;
	}

	brasero_sum_dialog_success (self);
	return TRUE;
}

static void
brasero_sum_dialog_mount_cb (NautilusBurnDrive *drive,
			     const gchar *mount_point,
			     gboolean mounted_by_us,
			     const GError *error,
			     gpointer callback_data)
{
	BraseroSumDialog *self = BRASERO_SUM_DIALOG (callback_data);
	GError *local_error = NULL;
	GSList *wrong_sums = NULL;
	BraseroBurnResult result;
	gint id;

	if (error) {
		brasero_sum_dialog_message_error (self, error);
		gtk_dialog_response (GTK_DIALOG (self), GTK_RESPONSE_CLOSE);
		return;
	}

	/* get the checksum */
	id = g_timeout_add (500,
			    brasero_sum_dialog_progress_poll,
			    self);

	brasero_tool_dialog_set_action (BRASERO_TOOL_DIALOG (self),
					BRASERO_BURN_ACTION_CHECKSUM,
					_("Checking files integrity"));

	/* check the sum of every file */
	self->priv->file_ctx = brasero_sum_check_new ();
	result = brasero_sum_check (self->priv->file_ctx,
				    mount_point,
				    &wrong_sums,
				    &local_error);

	/* clean up */
	brasero_sum_check_free (self->priv->file_ctx);
	self->priv->file_ctx = NULL;
	g_source_remove (id);

	if (mounted_by_us)
		NCB_DRIVE_UNMOUNT (drive, NULL);

	if (result == BRASERO_BURN_CANCEL) {
		if (local_error)
			g_error_free (local_error);
	}
	else if (local_error) {
		brasero_sum_dialog_message_error (self, local_error);
		g_error_free (local_error);					  
	}
	else if (wrong_sums) {
		brasero_sum_dialog_corruption_warning (self, wrong_sums);
		g_slist_foreach (wrong_sums, (GFunc) g_free, NULL);
		g_slist_free (wrong_sums);
	}
	else
		brasero_sum_dialog_success (self);

	gtk_dialog_response (GTK_DIALOG (self), GTK_RESPONSE_OK);
}

static gboolean
brasero_sum_dialog_check_brasero_sum (BraseroSumDialog *self,
				      NautilusBurnDrive *drive)
{
	BraseroMountHandle handle = NULL;
	gint answer;

	/* get the mount point */
	brasero_tool_dialog_set_action (BRASERO_TOOL_DIALOG (self),
					BRASERO_BURN_ACTION_CHECKSUM,
					_("Mounting disc"));

	handle = NCB_DRIVE_GET_MOUNT_POINT (drive,
					    brasero_sum_dialog_mount_cb,
					    self);

	answer = gtk_dialog_run (GTK_DIALOG (self));
	if (answer == GTK_RESPONSE_CANCEL) {
		NCB_DRIVE_GET_MOUNT_POINT_CANCEL (handle);
		return FALSE;
	}

	if (answer != GTK_RESPONSE_OK)
		return FALSE;

	return TRUE;
}

static gboolean
brasero_sum_dialog_activate (BraseroToolDialog *dialog,
			     NautilusBurnDrive *drive)
{
	BraseroSumDialog *self;

	self = BRASERO_SUM_DIALOG (dialog);

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->priv->md5_check)))
		brasero_sum_dialog_check_brasero_sum (self, drive);
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
