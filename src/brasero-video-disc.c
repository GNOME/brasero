/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2008 <bonfire-app@wanadoo.fr>
 * 
 * brasero is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * brasero is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtk.h>

#include "brasero-disc.h"
#include "brasero-io.h"
#include "brasero-utils.h"
#include "brasero-video-disc.h"

typedef struct _BraseroVideoDiscPrivate BraseroVideoDiscPrivate;
struct _BraseroVideoDiscPrivate
{
	GtkWidget *notebook;
	GtkWidget *tree;

	GtkWidget *message;
	GtkUIManager *manager;
	GtkActionGroup *disc_group;

	BraseroIO *io;
	BraseroIOJobBase *add_uri;

	gint64 sectors;

	guint activity;

	guint reject_files:1;
	guint loading:1;
};

#define BRASERO_VIDEO_DISC_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_VIDEO_DISC, BraseroVideoDiscPrivate))

static void
brasero_video_disc_iface_disc_init (BraseroDiscIface *iface);

G_DEFINE_TYPE_WITH_CODE (BraseroVideoDisc,
			 brasero_video_disc,
			 GTK_TYPE_VBOX,
			 G_IMPLEMENT_INTERFACE (BRASERO_TYPE_DISC,
					        brasero_video_disc_iface_disc_init));

enum {
	NAME_COL,
	URI_COL,
	ICON_COL,
	SIZE_COL,
	START_COL,
	END_COL,
	EDITABLE_COL,
	NUM_COL
};

enum {
	PROP_NONE,
	PROP_REJECT_FILE,
};


static void
brasero_video_disc_increase_activity_counter (BraseroVideoDisc *self)
{
	GdkCursor *cursor;
	BraseroVideoDiscPrivate *priv;

	priv = BRASERO_VIDEO_DISC_PRIVATE (self);

	if (priv->activity == 0 && GTK_WIDGET (self)->window) {
		cursor = gdk_cursor_new (GDK_WATCH);
		gdk_window_set_cursor (GTK_WIDGET (self)->window, cursor);
		gdk_cursor_unref (cursor);
	}

	priv->activity++;
}

static void
brasero_video_disc_decrease_activity_counter (BraseroVideoDisc *self)
{
	BraseroVideoDiscPrivate *priv;

	priv = BRASERO_VIDEO_DISC_PRIVATE (self);

	if (priv->activity == 1 && GTK_WIDGET (self)->window)
		gdk_window_set_cursor (GTK_WIDGET (self)->window, NULL);

	priv->activity--;
}

static void
brasero_video_disc_io_operation_finished (GObject *object,
					  gboolean cancelled,
					  gpointer null_data)
{
	BraseroVideoDisc *self = BRASERO_VIDEO_DISC (object);

	brasero_video_disc_decrease_activity_counter (self);
}

static void
brasero_video_disc_unreadable_dialog (BraseroVideoDisc *self,
				      const gchar *uri,
				      GError *error)
{
	GtkWidget *dialog, *toplevel;
	gchar *name;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
	if (toplevel == NULL) {
		g_warning ("Can't open file %s : %s\n",
			   uri,
			   error->message);
		return;
	}

	name = g_filename_display_basename (uri);
	dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					 GTK_DIALOG_DESTROY_WITH_PARENT|
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_CLOSE,
					 _("File \"%s\" can't be opened."),
					 name);
	g_free (name);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Unreadable file"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  error->message);

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static void
brasero_video_disc_file_not_video_dialog (BraseroVideoDisc *self,
					  const gchar *uri)
{
	GtkWidget *dialog, *toplevel;
	gchar *name;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
	if (toplevel == NULL) {
		g_warning ("Content widget error : can't handle \"%s\".\n", uri);
		return ;
	}

    	BRASERO_GET_BASENAME_FOR_DISPLAY (uri, name);
	dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					 GTK_DIALOG_DESTROY_WITH_PARENT|
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_CLOSE,
					 _("\"%s\" does not have a suitable type for video projects."),
					 name);
	g_free (name);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Unhandled file"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  _("Please only add files with video contents."));

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static void
brasero_video_disc_new_row_cb (GObject *obj,
			       GError *error,
			       const gchar *uri,
			       GFileInfo *info,
			       gpointer user_data)
{
	gint64 len;
	gchar *size_str;
	GtkTreeIter iter;
	const gchar *title;
	GdkPixbuf *snapshot;
	GtkTreeModel *model;
	GtkTreePath *treepath;
	GtkTreeRowReference *ref = user_data;
	BraseroVideoDisc *self = BRASERO_VIDEO_DISC (obj);
	BraseroVideoDiscPrivate *priv = BRASERO_VIDEO_DISC_PRIVATE (self);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree));
	treepath = gtk_tree_row_reference_get_path (ref);
	gtk_tree_row_reference_free (ref);
	if (!treepath)
		return;

	gtk_tree_model_get_iter (model, &iter, treepath);
	gtk_tree_path_free (treepath);

	if (error) {
		brasero_video_disc_unreadable_dialog (self, uri, error);
		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
		return;
	}

	if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
//		brasero_video_disc_add_dir (self, uri);
		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
		return;
	}

	if (g_file_info_get_file_type (info) != G_FILE_TYPE_REGULAR
	|| !g_file_info_get_attribute_boolean (info, BRASERO_IO_HAS_VIDEO)) {
		brasero_video_disc_file_not_video_dialog (self, uri);
		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
		return;
	}

	if (g_file_info_get_is_symlink (info)) {
		uri = g_strconcat ("file://", g_file_info_get_symlink_target (info), NULL);
		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				    URI_COL, uri, -1);
	}

	/* set the snapshot */
	snapshot = GDK_PIXBUF (g_file_info_get_attribute_object (info, BRASERO_IO_SNAPSHOT));
	if (snapshot) {
		GdkPixbuf *scaled;

		scaled = gdk_pixbuf_scale_simple (snapshot,
						  96 * gdk_pixbuf_get_width (snapshot) / gdk_pixbuf_get_height (snapshot),
						  96,
						  GDK_INTERP_BILINEAR);
		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				    ICON_COL, scaled,
				    -1);
		g_object_unref (scaled);
	}

	/* */
	len = g_file_info_get_attribute_uint64 (info, BRASERO_IO_LEN);
	size_str = brasero_utils_get_time_string (len, TRUE, FALSE);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			    END_COL, len,
			    SIZE_COL, size_str,
			    -1);
	g_free (size_str);

	/* */
	title = g_file_info_get_attribute_string (info, BRASERO_IO_TITLE);
	if (title)
		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				    NAME_COL, title,
				    -1);

	/* FIXME: duration to sectors is not correct here, that's not audio... */
	priv->sectors += BRASERO_DURATION_TO_SECTORS (len);
	brasero_disc_size_changed (BRASERO_DISC (self), priv->sectors);
}

static BraseroDiscResult
brasero_video_disc_add_uri_real (BraseroVideoDisc *self,
				 const gchar *uri,
				 gint pos,
				 gint64 start,
				 gint64 end,
				 GtkTreePath **path_return)
{
	BraseroVideoDiscPrivate *priv;
	GtkTreeRowReference *ref;
	GtkTreePath *treepath;
	GtkTreeModel *store;
	GtkTreeIter iter;
	gchar *markup;
	gchar *name;

	g_return_val_if_fail (uri != NULL, BRASERO_DISC_ERROR_UNKNOWN);

	priv = BRASERO_VIDEO_DISC_PRIVATE (self);
	if (priv->reject_files)
		return BRASERO_DISC_NOT_READY;

	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), 1);

	store = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree));
	if (pos > -1)
		gtk_list_store_insert (GTK_LIST_STORE (store), &iter, pos);
	else
		gtk_list_store_append (GTK_LIST_STORE (store), &iter);

	BRASERO_GET_BASENAME_FOR_DISPLAY (uri, name);
	markup = g_markup_escape_text (name, -1);
	g_free (name);

    	gtk_list_store_set (GTK_LIST_STORE (store), &iter,
			    NAME_COL, markup,
			    URI_COL, uri,
			    -1);
	g_free (markup);

	/* set size message */
	start = start > 0 ? start:0;
	if (end > 0 && end > start) {
		gchar *string;
		gint64 length;

		/* update global size */
		length = BRASERO_AUDIO_TRACK_LENGTH (start, end);
		priv->sectors += BRASERO_DURATION_TO_SECTORS (length);
		brasero_disc_size_changed (BRASERO_DISC (self), priv->sectors);

		string = brasero_utils_get_time_string (length, TRUE, FALSE);
		gtk_list_store_set (GTK_LIST_STORE (store), &iter,
				    START_COL, start,
				    END_COL, end,
				    SIZE_COL, string,
				    -1);
		g_free (string);
	}
	else
		gtk_list_store_set (GTK_LIST_STORE (store), &iter,
				    SIZE_COL, _("loading"),
				    -1);

	/* Now load */
	treepath = gtk_tree_model_get_path (store, &iter);
	ref = gtk_tree_row_reference_new (store, treepath);

	if (path_return)
		*path_return = treepath;
	else
		gtk_tree_path_free (treepath);

	/* get info async for the file */
	if (!priv->io)
		priv->io = brasero_io_get_default ();

	if (!priv->add_uri)
		priv->add_uri = brasero_io_register (G_OBJECT (self),
						     brasero_video_disc_new_row_cb,
						     brasero_video_disc_io_operation_finished,
						     NULL);

	brasero_video_disc_increase_activity_counter (self);
	brasero_io_get_file_info (priv->io,
				  uri,
				  priv->add_uri,
				  BRASERO_IO_INFO_PERM|
				  BRASERO_IO_INFO_MIME|
				  BRASERO_IO_INFO_METADATA|
				  BRASERO_IO_INFO_METADATA_MISSING_CODEC|
				  BRASERO_IO_INFO_METADATA_SNAPSHOT,
				  ref);

	return BRASERO_DISC_OK;
}

static BraseroDiscResult
brasero_video_disc_add_uri (BraseroDisc *self,
			    const gchar *uri)
{
	BraseroVideoDiscPrivate *priv;
	GtkTreePath *treepath = NULL;
	BraseroDiscResult result;

	priv = BRASERO_VIDEO_DISC_PRIVATE (self);
	result = brasero_video_disc_add_uri_real (BRASERO_VIDEO_DISC (self),
						  uri,
						  -1,
						  -1,
						  -1,
						  &treepath);

	if (treepath) {
		gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (priv->tree),
					      treepath,
					      NULL,
					      TRUE,
					      0.5,
					      0.5);
		gtk_tree_path_free (treepath);
	}

	return result;
}

static void
brasero_video_disc_delete_selected (BraseroDisc *self)
{
	BraseroVideoDiscPrivate *priv;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GList *selected;
	GList *iter;

	priv = BRASERO_VIDEO_DISC_PRIVATE (self);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));

	selected = gtk_tree_selection_get_selected_rows (selection, &model);
	selected = g_list_reverse (selected);
	for (iter = selected; iter; iter = iter->next) {
		GtkTreePath *treepath;
		GtkTreeIter tree_iter;
		gint64 start;
		gint64 end;

		treepath = iter->data;

		gtk_tree_model_get_iter (model, &tree_iter, treepath);
		gtk_tree_model_get (model, &tree_iter,
				    START_COL, &start,
				    END_COL, &end,
				    -1);
		priv->sectors -= BRASERO_DURATION_TO_SECTORS (end - start);

		gtk_list_store_remove (GTK_LIST_STORE (model), &tree_iter);
		gtk_tree_path_free (treepath);
	}
	g_list_free (selected);

	brasero_disc_size_changed (BRASERO_DISC (self), priv->sectors);
}

static gboolean
brasero_video_disc_get_selected_uri (BraseroDisc *self,
				     gchar **uri)
{
	GList *selected;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	BraseroVideoDiscPrivate *priv;

	priv = BRASERO_VIDEO_DISC_PRIVATE (self);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));
	selected = gtk_tree_selection_get_selected_rows (selection, &model);
	if (!selected)
		return FALSE;

	if (uri) {
		GtkTreeIter iter;
		GtkTreePath *treepath;

		treepath = selected->data;
		gtk_tree_model_get_iter (model, &iter, treepath);
		gtk_tree_model_get (model, &iter,
				    URI_COL, uri,
				    -1);
	}

	g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (selected);

	return TRUE;
}

static void
brasero_video_disc_selection_changed_cb (GtkTreeSelection *selection,
					 BraseroVideoDisc *self)
{
	brasero_disc_selection_changed (BRASERO_DISC (self));
}

static guint
brasero_video_disc_add_ui (BraseroDisc *disc,
			   GtkUIManager *manager,
			   GtkWidget *message)
{
	BraseroVideoDiscPrivate *priv;
//	GError *error = NULL;
//	GtkAction *action;
	guint merge_id;

	priv = BRASERO_VIDEO_DISC_PRIVATE (disc);

	if (priv->message) {
		g_object_unref (priv->message);
		priv->message = NULL;
	}

	priv->message = message;
	g_object_ref (message);

	if (!priv->disc_group) {
		priv->disc_group = gtk_action_group_new (BRASERO_DISC_ACTION);
		gtk_action_group_set_translation_domain (priv->disc_group, GETTEXT_PACKAGE);
/*		gtk_action_group_add_actions (priv->disc_group,
					      entries,
					      G_N_ELEMENTS (entries),
					      disc);
		gtk_action_group_add_toggle_actions (priv->disc_group,
						     toggle_entries,
						     G_N_ELEMENTS (toggle_entries),
						     disc);
		gtk_ui_manager_insert_action_group (manager,
						    priv->disc_group,
						    0);
*/	}

/*	merge_id = gtk_ui_manager_add_ui_from_string (manager,
						      description,
						      -1,
						      &error);
	if (!merge_id) {
		BRASERO_BURN_LOG ("Adding ui elements failed: %s", error->message);
		g_error_free (error);
		return 0;
	}

	action = gtk_action_group_get_action (priv->disc_group, "ImportSession");
	gtk_action_set_sensitive (action, FALSE);
	g_object_set (action,
		      "short-label", _("Import"),
		      NULL);
*/

	priv->manager = manager;
	g_object_ref (manager);
	return merge_id;
}

static void
brasero_video_disc_row_deleted_cb (GtkTreeModel *model,
				   GtkTreePath *path,
				   BraseroVideoDisc *self)
{
	brasero_disc_contents_changed (BRASERO_DISC (self),
				       gtk_tree_model_iter_n_children (model, NULL));
}

static void
brasero_video_disc_row_inserted_cb (GtkTreeModel *model,
				    GtkTreePath *path,
				    GtkTreeIter *iter,
				    BraseroVideoDisc *self)
{
	brasero_disc_contents_changed (BRASERO_DISC (self),
				       gtk_tree_model_iter_n_children (model, NULL));
}

static void
brasero_video_disc_row_changed_cb (GtkTreeModel *model,
				   GtkTreePath *path,
				   GtkTreeIter *iter,
				   BraseroVideoDisc *self)
{
	brasero_disc_contents_changed (BRASERO_DISC (self),
				       gtk_tree_model_iter_n_children (model, NULL));
}

static void
brasero_video_disc_init (BraseroVideoDisc *object)
{
	BraseroVideoDiscPrivate *priv;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeModel *model;
	GtkWidget *mainbox;
	GtkWidget *scroll;

	priv = BRASERO_VIDEO_DISC_PRIVATE (object);

	/* the information displayed about how to use this tree */
	priv->notebook = brasero_disc_get_use_info_notebook ();
	gtk_widget_show (priv->notebook);
	gtk_box_pack_start (GTK_BOX (object), priv->notebook, TRUE, TRUE, 0);

	mainbox = gtk_vbox_new (FALSE, 12);
	gtk_widget_show (mainbox);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), mainbox, NULL);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), 0);

	/* Tree */
	model = GTK_TREE_MODEL (gtk_list_store_new (NUM_COL,
						    G_TYPE_STRING,
						    G_TYPE_STRING,
						    GDK_TYPE_PIXBUF,
						    G_TYPE_STRING,
						    G_TYPE_INT64,
						    G_TYPE_INT64,
						    G_TYPE_BOOLEAN));
	g_signal_connect (G_OBJECT (model),
			  "row-deleted",
			  G_CALLBACK (brasero_video_disc_row_deleted_cb),
			  object);
	g_signal_connect (G_OBJECT (model),
			  "row-inserted",
			  G_CALLBACK (brasero_video_disc_row_inserted_cb),
			  object);
	g_signal_connect (G_OBJECT (model),
			  "row-changed",
			  G_CALLBACK (brasero_video_disc_row_changed_cb),
			  object);

	priv->tree = gtk_tree_view_new_with_model (model);
	g_object_unref (G_OBJECT (model));
	gtk_widget_show (priv->tree);

	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (priv->tree), TRUE);
	gtk_tree_view_set_rubber_banding (GTK_TREE_VIEW (priv->tree), TRUE);

	/* columns */
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_min_width (column, 200);

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "pixbuf", ICON_COL);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (G_OBJECT (renderer),
		      "mode", GTK_CELL_RENDERER_MODE_EDITABLE,
		      "ellipsize-set", TRUE,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      NULL);

	gtk_tree_view_column_pack_end (column, renderer, TRUE);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "markup", NAME_COL);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "editable", EDITABLE_COL);
	gtk_tree_view_column_set_title (column, _("Title"));
	g_object_set (G_OBJECT (column),
		      "expand", TRUE,
		      "spacing", 4,
		      NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->tree),
				     column);

	gtk_tree_view_set_expander_column (GTK_TREE_VIEW (priv->tree),
					   column);


	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);

	gtk_tree_view_column_add_attribute (column, renderer,
					    "text", SIZE_COL);
	gtk_tree_view_column_set_title (column, _("Size"));

	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->tree), column);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_expand (column, FALSE);
	gtk_tree_view_column_set_sort_column_id (column, NAME_COL);

	/* selection */
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	g_signal_connect (selection,
			  "changed",
			  G_CALLBACK (brasero_video_disc_selection_changed_cb),
			  object);

	/* scroll */
	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scroll);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll),
					     GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scroll), priv->tree);
	gtk_box_pack_start (GTK_BOX (mainbox), scroll, TRUE, TRUE, 0);
}

static void
brasero_video_disc_reset_real (BraseroVideoDisc *self)
{
	BraseroVideoDiscPrivate *priv;

	priv = BRASERO_VIDEO_DISC_PRIVATE (self);

	if (priv->io)
		brasero_io_cancel_by_base (priv->io, priv->add_uri);

	priv->sectors = 0;

	priv->activity = 1;
	brasero_video_disc_decrease_activity_counter (self);
}

static void
brasero_video_disc_clear (BraseroDisc *disc)
{
	BraseroVideoDiscPrivate *priv;
	GtkTreeModel *model;

	priv = BRASERO_VIDEO_DISC_PRIVATE (disc);

	brasero_video_disc_reset_real (BRASERO_VIDEO_DISC (disc));

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree));
	gtk_list_store_clear (GTK_LIST_STORE (model));

	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), 0);
	brasero_disc_size_changed (disc, 0);
}

static void
brasero_video_disc_reset (BraseroDisc *disc)
{
	brasero_video_disc_clear (disc);
}

static void
brasero_video_disc_finalize (GObject *object)
{
	BraseroVideoDiscPrivate *priv;

	priv = BRASERO_VIDEO_DISC_PRIVATE (object);
	
	brasero_video_disc_reset_real (BRASERO_VIDEO_DISC (object));
	
	if (priv->io) {
		brasero_io_cancel_by_base (priv->io, priv->add_uri);
		g_free (priv->add_uri);
		priv->add_uri = NULL;

		g_object_unref (priv->io);
		priv->io = NULL;
	}

	G_OBJECT_CLASS (brasero_video_disc_parent_class)->finalize (object);
}

static void
brasero_video_disc_get_property (GObject * object,
				 guint prop_id,
				 GValue * value,
				 GParamSpec * pspec)
{
	BraseroVideoDiscPrivate *priv;

	priv = BRASERO_VIDEO_DISC_PRIVATE (object);

	switch (prop_id) {
	case PROP_REJECT_FILE:
		g_value_set_boolean (value, priv->reject_files);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
brasero_video_disc_set_property (GObject * object,
				 guint prop_id,
				 const GValue * value,
				 GParamSpec * pspec)
{
	BraseroVideoDiscPrivate *priv;

	priv = BRASERO_VIDEO_DISC_PRIVATE (object);

	switch (prop_id) {
	case PROP_REJECT_FILE:
		priv->reject_files = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static BraseroDiscResult
brasero_video_disc_get_status (BraseroDisc *disc)
{
	GtkTreeModel *model;
	BraseroVideoDiscPrivate *priv;

	priv = BRASERO_VIDEO_DISC_PRIVATE (disc);

	if (priv->loading)
		return BRASERO_DISC_LOADING;

	if (priv->activity)
		return BRASERO_DISC_NOT_READY;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree));
	if (!gtk_tree_model_iter_n_children (model, NULL))
		return BRASERO_DISC_ERROR_EMPTY_SELECTION;

	return BRASERO_DISC_OK;
}

static BraseroDiscResult
brasero_video_disc_set_session_param (BraseroDisc *disc,
				      BraseroBurnSession *session)
{
	BraseroTrackType type;

	type.type = BRASERO_TRACK_TYPE_AUDIO;
	type.subtype.audio_format = BRASERO_AUDIO_FORMAT_UNDEFINED|BRASERO_VIDEO_FORMAT_UNDEFINED;
	brasero_burn_session_set_input_type (session, &type);
	return BRASERO_BURN_OK;
}

static BraseroDiscResult
brasero_video_disc_set_session_contents (BraseroDisc *disc,
					 BraseroBurnSession *session)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	BraseroTrack *track;
	BraseroVideoDiscPrivate *priv;

	priv = BRASERO_VIDEO_DISC_PRIVATE (disc);
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree));
	if (!gtk_tree_model_get_iter_first (model, &iter))
		return BRASERO_DISC_ERROR_EMPTY_SELECTION;

	track = NULL;
	do {
		gchar *uri;
		gint64 end;
		gint64 start;
		gchar *title;
		BraseroSongInfo *info;

		gtk_tree_model_get (model, &iter,
				    URI_COL, &uri,
				    NAME_COL, &title,
				    START_COL, &start,
				    END_COL, &end,
				    -1);

		info = g_new0 (BraseroSongInfo, 1);
		info->title = title;

		track = brasero_track_new (BRASERO_TRACK_TYPE_AUDIO);
		brasero_track_set_audio_source (track,
						uri,
						BRASERO_AUDIO_FORMAT_UNDEFINED|
						BRASERO_VIDEO_FORMAT_UNDEFINED);

		brasero_track_set_audio_boundaries (track, start, end, -1);
		brasero_track_set_audio_info (track, info);
		brasero_burn_session_add_track (session, track);

		/* It's good practice to unref the track afterwards as we don't
		 * need it anymore. BraseroBurnSession refs it. */
		brasero_track_unref (track);

	} while (gtk_tree_model_iter_next (model, &iter));

	return BRASERO_DISC_OK;
}

static void
brasero_video_disc_iface_disc_init (BraseroDiscIface *iface)
{
	iface->add_uri = brasero_video_disc_add_uri;
	iface->delete_selected = brasero_video_disc_delete_selected;
	iface->clear = brasero_video_disc_clear;
	iface->reset = brasero_video_disc_reset;

	iface->get_status = brasero_video_disc_get_status;
	iface->set_session_param = brasero_video_disc_set_session_param;
	iface->set_session_contents = brasero_video_disc_set_session_contents;

/*
	iface->get_track = brasero_data_disc_get_track;
	iface->load_track = brasero_data_disc_load_track;
*/
	iface->get_selected_uri = brasero_video_disc_get_selected_uri;
	iface->add_ui = brasero_video_disc_add_ui;
}

static void
brasero_video_disc_class_init (BraseroVideoDiscClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroVideoDiscPrivate));

	object_class->finalize = brasero_video_disc_finalize;
	object_class->set_property = brasero_video_disc_set_property;
	object_class->get_property = brasero_video_disc_get_property;

	g_object_class_install_property (object_class,
					 PROP_REJECT_FILE,
					 g_param_spec_boolean
					 ("reject-file",
					  "Whether it accepts files",
					  "Whether it accepts files",
					  FALSE,
					  G_PARAM_READWRITE));
}

GtkWidget *
brasero_video_disc_new (void)
{
	return g_object_new (BRASERO_TYPE_VIDEO_DISC, NULL);
}

