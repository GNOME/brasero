/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2005-2008 <bonfire-app@wanadoo.fr>
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

#include <gdk/gdkkeysyms.h>

#include <gtk/gtk.h>

#include "eggtreemultidnd.h"

#include "burn-debug.h"
#include "brasero-disc.h"
#include "brasero-io.h"
#include "brasero-utils.h"
#include "brasero-video-disc.h"
#include "brasero-video-project.h"
#include "brasero-video-tree-model.h"
#include "brasero-multi-song-props.h"
#include "brasero-song-properties.h"

typedef struct _BraseroVideoDiscPrivate BraseroVideoDiscPrivate;
struct _BraseroVideoDiscPrivate
{
	GtkWidget *notebook;
	GtkWidget *tree;

	GtkWidget *message;
	GtkUIManager *manager;
	GtkActionGroup *disc_group;

	guint reject_files:1;
	guint editing:1;
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
	PROP_NONE,
	PROP_REJECT_FILE,
};

static void
brasero_video_disc_edit_information_cb (GtkAction *action,
					BraseroVideoDisc *disc);
static void
brasero_video_disc_open_activated_cb (GtkAction *action,
				      BraseroVideoDisc *disc);
static void
brasero_video_disc_delete_activated_cb (GtkAction *action,
					BraseroVideoDisc *disc);
static void
brasero_video_disc_paste_activated_cb (GtkAction *action,
				       BraseroVideoDisc *disc);

static GtkActionEntry entries[] = {
	{"ContextualMenu", NULL, N_("Menu")},
	{"OpenVideo", GTK_STOCK_OPEN, NULL, NULL, N_("Open the selected video"),
	 G_CALLBACK (brasero_video_disc_open_activated_cb)},
	{"EditVideo", GTK_STOCK_PROPERTIES, N_("_Edit Information..."), NULL, N_("Edit the video information (start, end, author, ...)"),
	 G_CALLBACK (brasero_video_disc_edit_information_cb)},
	{"DeleteVideo", GTK_STOCK_REMOVE, NULL, NULL, N_("Remove the selected videos from the project"),
	 G_CALLBACK (brasero_video_disc_delete_activated_cb)},
	{"PasteVideo", GTK_STOCK_PASTE, NULL, NULL, N_("Add the files stored in the clipboard"),
	 G_CALLBACK (brasero_video_disc_paste_activated_cb)},
/*	{"Split", "transform-crop-and-resize", N_("_Split Track..."), NULL, N_("Split the selected track"),
	 G_CALLBACK (brasero_video_disc_split_cb)} */
};

static const gchar *description = {
	"<ui>"
	"<menubar name='menubar' >"
		"<menu action='EditMenu'>"
/*		"<placeholder name='EditPlaceholder'>"
			"<menuitem action='Split'/>"
		"</placeholder>"
*/		"</menu>"
	"</menubar>"
	"<popup action='ContextMenu'>"
		"<menuitem action='OpenVideo'/>"
		"<menuitem action='DeleteVideo'/>"
		"<separator/>"
		"<menuitem action='PasteVideo'/>"
/*		"<separator/>"
		"<menuitem action='Split'/>"
*/		"<separator/>"
		"<menuitem action='EditVideo'/>"
	"</popup>"
/*	"<toolbar name='Toolbar'>"
		"<placeholder name='DiscButtonPlaceholder'>"
			"<separator/>"
			"<toolitem action='Split'/>"
		"</placeholder>"
	"</toolbar>"
*/	"</ui>"
};

enum {
	TREE_MODEL_ROW		= 150,
	TARGET_URIS_LIST,
};

static GtkTargetEntry ntables_cd [] = {
	{BRASERO_DND_TARGET_SELF_FILE_NODES, GTK_TARGET_SAME_WIDGET, TREE_MODEL_ROW},
	{"text/uri-list", 0, TARGET_URIS_LIST}
};
static guint nb_targets_cd = sizeof (ntables_cd) / sizeof (ntables_cd[0]);

static GtkTargetEntry ntables_source [] = {
	{BRASERO_DND_TARGET_SELF_FILE_NODES, GTK_TARGET_SAME_WIDGET, TREE_MODEL_ROW},
};

static guint nb_targets_source = sizeof (ntables_source) / sizeof (ntables_source[0]);


/**
 * Row name edition
 */

static void
brasero_video_disc_name_editing_started_cb (GtkCellRenderer *renderer,
					    GtkCellEditable *editable,
					    gchar *path,
					    BraseroVideoDisc *disc)
{
	BraseroVideoDiscPrivate *priv;

	priv = BRASERO_VIDEO_DISC_PRIVATE (disc);
	priv->editing = 1;
}

static void
brasero_video_disc_name_editing_canceled_cb (GtkCellRenderer *renderer,
					     BraseroVideoDisc *disc)
{
	BraseroVideoDiscPrivate *priv;

	priv = BRASERO_VIDEO_DISC_PRIVATE (disc);
	priv->editing = 0;
}

static void
brasero_video_disc_name_edited_cb (GtkCellRendererText *cellrenderertext,
				   gchar *path_string,
				   gchar *text,
				   BraseroVideoDisc *self)
{
	BraseroVideoDiscPrivate *priv;
	BraseroVideoProject *project;
	BraseroVideoFile *file;
	GtkTreePath *path;
	GtkTreeIter row;

	priv = BRASERO_VIDEO_DISC_PRIVATE (self);

	priv->editing = 0;

	path = gtk_tree_path_new_from_string (path_string);
	project = BRASERO_VIDEO_PROJECT (gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree)));

	/* see if this is still a valid path. It can happen a user removes it
	 * while the name of the row is being edited */
	if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (project), &row, path)) {
		gtk_tree_path_free (path);
		return;
	}

	file = brasero_video_tree_model_path_to_file (BRASERO_VIDEO_TREE_MODEL (project), path);
	gtk_tree_path_free (path);

	brasero_video_project_rename (project, file, text);
}

static void
brasero_video_disc_vfs_activity_changed (BraseroVideoProject *project,
					 gboolean activity,
					 BraseroVideoDisc *self)
{
	GdkCursor *cursor;
	BraseroVideoDiscPrivate *priv;

	priv = BRASERO_VIDEO_DISC_PRIVATE (self);

	if (!GTK_WIDGET (self)->window)
		return;

	if (activity) {
		cursor = gdk_cursor_new (GDK_WATCH);
		gdk_window_set_cursor (GTK_WIDGET (self)->window, cursor);
		gdk_cursor_unref (cursor);
	}
	else
		gdk_window_set_cursor (GTK_WIDGET (self)->window, NULL);
}

static gboolean
brasero_video_disc_directory_dialog (BraseroVideoProject *project,
				     const gchar *uri,
				     BraseroVideoDisc *self)
{
	gint answer;
	GtkWidget *dialog;
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
	dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					 GTK_DIALOG_DESTROY_WITH_PARENT |
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_WARNING,
					 GTK_BUTTONS_NONE,
					 _("Do you want to search for video files inside the directory?"));

	gtk_window_set_title (GTK_WINDOW (dialog), _("Directory Search"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  _("Directories can't be added to video disc."));

	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				_("Search directory"), GTK_RESPONSE_OK,
				NULL);

	gtk_widget_show_all (dialog);
	answer = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	if (answer != GTK_RESPONSE_OK)
		return FALSE;

	return TRUE;
}

static void
brasero_video_disc_unreadable_uri_dialog (BraseroVideoProject *project,
					  GError *error,
					  const gchar *uri,
					  BraseroVideoDisc *self)
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
brasero_video_disc_not_video_dialog (BraseroVideoProject *project,
				     const gchar *uri,
				     BraseroVideoDisc *self)
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

static BraseroDiscResult
brasero_video_disc_add_uri_real (BraseroVideoDisc *self,
				 const gchar *uri,
				 gint pos,
				 gint64 start,
				 gint64 end,
				 GtkTreePath **path_return)
{
	BraseroVideoFile *file;
	BraseroVideoProject *project;
	BraseroVideoDiscPrivate *priv;
	BraseroVideoFile *sibling = NULL;

	priv = BRASERO_VIDEO_DISC_PRIVATE (self);
	if (priv->reject_files)
		return BRASERO_DISC_NOT_READY;

	project = BRASERO_VIDEO_PROJECT (gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree)));

	if (pos > 0) {
		GtkTreePath *treepath;

		treepath = gtk_tree_path_new ();
		gtk_tree_path_append_index (treepath, pos);
		sibling = brasero_video_tree_model_path_to_file (BRASERO_VIDEO_TREE_MODEL (project), treepath);
		gtk_tree_path_free (treepath);
	}

	file = brasero_video_project_add_uri (project,
					      uri,
					      sibling,
					      start,
					      end);
	if (path_return && file)
		*path_return = brasero_video_tree_model_file_to_path (BRASERO_VIDEO_TREE_MODEL (project), file);

	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), 1);

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
		BraseroVideoFile *file;
		GtkTreePath *treepath;

		treepath = iter->data;

		file = brasero_video_tree_model_path_to_file (BRASERO_VIDEO_TREE_MODEL (model), treepath);
		gtk_tree_path_free (treepath);

		if (!file)
			continue;

		brasero_video_project_remove_file (BRASERO_VIDEO_PROJECT (model), file);
	}
	g_list_free (selected);
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
		BraseroVideoFile *file;
		GtkTreePath *treepath;

		treepath = selected->data;
		file = brasero_video_tree_model_path_to_file (BRASERO_VIDEO_TREE_MODEL (model), treepath);
		if (file)
			*uri = g_strdup (file->uri);
		else
			*uri = NULL;
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

static gboolean
brasero_video_disc_selection_function (GtkTreeSelection *selection,
				       GtkTreeModel *model,
				       GtkTreePath *treepath,
				       gboolean path_currently_selected,
				       gpointer NULL_data)
{
	BraseroVideoFile *file;

	file = brasero_video_tree_model_path_to_file (BRASERO_VIDEO_TREE_MODEL (model), treepath);
	if (file)
		file->editable = !path_currently_selected;

	return TRUE;
}


/**
 * Callback for menu
 */

static gboolean
brasero_video_disc_rename_songs (GtkTreeModel *model,
				 GtkTreeIter *iter,
				 GtkTreePath *treepath,
				 const gchar *old_name,
				 const gchar *new_name)
{
	BraseroVideoFile *file;

	file = brasero_video_tree_model_path_to_file (BRASERO_VIDEO_TREE_MODEL (model), treepath);
	if (!file)
		return FALSE;

	if (file->name)
		g_free (file->name);

	file->name = g_strdup (new_name);
	return TRUE;
}

static void
brasero_video_disc_edit_song_properties_list (BraseroVideoDisc *self,
					      GList *list)
{
	GList *item;
	gint isrc;
	GList *copy;
	GtkWidget *props;
	GtkWidget *toplevel;
	GtkTreeModel *model;
	gchar *artist = NULL;
	gchar *composer = NULL;
	GtkResponseType result;
	BraseroVideoDiscPrivate *priv;

	priv = BRASERO_VIDEO_DISC_PRIVATE (self);

	if (!g_list_length (list))
		return;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree));

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
	props = brasero_multi_song_props_new ();
	brasero_multi_song_props_set_show_gap (BRASERO_MULTI_SONG_PROPS (props), FALSE);

	gtk_window_set_transient_for (GTK_WINDOW (props),
				      GTK_WINDOW (toplevel));
	gtk_window_set_modal (GTK_WINDOW (props), TRUE);
	gtk_window_set_position (GTK_WINDOW (props),
				 GTK_WIN_POS_CENTER_ON_PARENT);

	gtk_widget_show (GTK_WIDGET (props));
	result = gtk_dialog_run (GTK_DIALOG (props));
	gtk_widget_hide (GTK_WIDGET (props));
	if (result != GTK_RESPONSE_ACCEPT)
		goto end;

	brasero_multi_song_props_set_rename_callback (BRASERO_MULTI_SONG_PROPS (props),
						      gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree)),
						      BRASERO_VIDEO_TREE_MODEL_NAME,
						      brasero_video_disc_rename_songs);

	brasero_multi_song_props_get_properties (BRASERO_MULTI_SONG_PROPS (props),
						 &artist,
						 &composer,
						 &isrc,
						 NULL);

	/* start by the end in case we add silences since then the next
	 * treepaths will be wrong */
	copy = g_list_copy (list);
	copy = g_list_reverse (copy);

	for (item = copy; item; item = item->next) {
		GtkTreePath *treepath;
		BraseroVideoFile *file;

		treepath = item->data;
		file = brasero_video_tree_model_path_to_file (BRASERO_VIDEO_TREE_MODEL (model), treepath);
		if (!file)
			continue;

		if (artist) {
			g_free (file->info->artist);
			file->info->artist = g_strdup (artist);
		}

		if (composer) {
			g_free (file->info->composer);
			file->info->composer = g_strdup (composer);
		}

		if (isrc > 0)
			file->info->isrc = isrc;
	}

	g_list_free (copy);
	g_free (artist);
	g_free (composer);
end:

	gtk_widget_destroy (props);
}

static void
brasero_video_disc_edit_song_properties_file (BraseroVideoDisc *self,
					      BraseroVideoFile *file)
{
	gint64 end;
	gint64 start;
	GtkWidget *props;
	GtkWidget *toplevel;
	GtkTreeModel *model;
	GtkResponseType result;
	BraseroVideoDiscPrivate *priv;

	priv = BRASERO_VIDEO_DISC_PRIVATE (self);


	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree));
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));

	props = brasero_song_props_new ();
	brasero_song_props_set_properties (BRASERO_SONG_PROPS (props),
					   -1,
					   file->info->artist,
					   file->info->title,
					   file->info->composer,
					   file->info->isrc,
					   file->end - file->start,
					   file->start,
					   file->end,
					   -1);

	gtk_window_set_transient_for (GTK_WINDOW (props),
				      GTK_WINDOW (toplevel));
	gtk_window_set_modal (GTK_WINDOW (props), TRUE);
	gtk_window_set_position (GTK_WINDOW (props),
				 GTK_WIN_POS_CENTER_ON_PARENT);

	gtk_widget_show (GTK_WIDGET (props));
	result = gtk_dialog_run (GTK_DIALOG (props));
	gtk_widget_hide (GTK_WIDGET (props));
	if (result != GTK_RESPONSE_ACCEPT)
		goto end;

	brasero_song_info_free (file->info);
	file->info = g_new0 (BraseroSongInfo, 1);

	brasero_song_props_get_properties (BRASERO_SONG_PROPS (props),
					   &file->info->artist,
					   &file->info->title,
					   &file->info->composer,
					   &file->info->isrc,
					   &start,
					   &end,
					   NULL);

	brasero_video_project_resize_file (BRASERO_VIDEO_PROJECT (model), file, start, end);

end:

	gtk_widget_destroy (props);
}
static void
brasero_video_disc_edit_information_cb (GtkAction *action,
					BraseroVideoDisc *self)
{
	GList *list;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	BraseroVideoDiscPrivate *priv;

	priv = BRASERO_VIDEO_DISC_PRIVATE (self);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));
	list = gtk_tree_selection_get_selected_rows (selection, &model);

	if (!list)
		return;

	if (g_list_length (list) == 1) {
		BraseroVideoFile *file;
		GtkTreePath *treepath;

		treepath = list->data;

		file = brasero_video_tree_model_path_to_file (BRASERO_VIDEO_TREE_MODEL (model), treepath);
		if (file)
			brasero_video_disc_edit_song_properties_file (self, file);
	}
	else
		brasero_video_disc_edit_song_properties_list (self, list);

	g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (list);
}

static void
brasero_video_disc_open_file (BraseroVideoDisc *self)
{
	GList *item, *list;
	GSList *uris = NULL;
	GtkTreeModel *model;
	GtkTreePath *treepath;
	GtkTreeSelection *selection;
	BraseroVideoDiscPrivate *priv;

	priv = BRASERO_VIDEO_DISC_PRIVATE (self);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));
	list = gtk_tree_selection_get_selected_rows (selection, &model);

	for (item = list; item; item = item->next) {
		BraseroVideoFile *file;

		treepath = item->data;
		file = brasero_video_tree_model_path_to_file (BRASERO_VIDEO_TREE_MODEL (model), treepath);
		gtk_tree_path_free (treepath);

		if (!file)
			continue;

		if (file->uri)
			uris = g_slist_prepend (uris, file->uri);
	}
	g_list_free (list);

	brasero_utils_launch_app (GTK_WIDGET (self), uris);
	g_slist_free (uris);
}

static void
brasero_video_disc_open_activated_cb (GtkAction *action,
				      BraseroVideoDisc *self)
{
	brasero_video_disc_open_file (self);
}

static void
brasero_video_disc_clipboard_text_cb (GtkClipboard *clipboard,
				      const gchar *text,
				      BraseroVideoDisc *self)
{
	gchar **array;
	gchar **item;

	array = g_strsplit_set (text, "\n\r", 0);
	item = array;
	while (*item) {
		if (**item != '\0') {
			GFile *file;
			gchar *uri;

			file = g_file_new_for_commandline_arg (*item);
			uri = g_file_get_uri (file);
			g_object_unref (file);

			brasero_video_disc_add_uri_real (self,
							 uri,
							 -1,
							 -1,
							 -1,
							 NULL);
			g_free (uri);
		}

		item++;
	}
}

static void
brasero_video_disc_clipboard_targets_cb (GtkClipboard *clipboard,
					 GdkAtom *atoms,
					 gint n_atoms,
					 BraseroVideoDisc *self)
{
	GdkAtom *iter;
	gchar *target;

	iter = atoms;
	while (n_atoms) {
		target = gdk_atom_name (*iter);

		if (!strcmp (target, "x-special/gnome-copied-files")
		||  !strcmp (target, "UTF8_STRING")) {
			gtk_clipboard_request_text (clipboard,
						    (GtkClipboardTextReceivedFunc) brasero_video_disc_clipboard_text_cb,
						    self);
			g_free (target);
			return;
		}

		g_free (target);
		iter++;
		n_atoms--;
	}
}

static void
brasero_video_disc_paste_activated_cb (GtkAction *action,
				       BraseroVideoDisc *self)
{
	GtkClipboard *clipboard;

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_request_targets (clipboard,
				       (GtkClipboardTargetsReceivedFunc) brasero_video_disc_clipboard_targets_cb,
				       self);
}

static void
brasero_video_disc_delete_activated_cb (GtkAction *action,
					BraseroVideoDisc *self)
{
	brasero_video_disc_delete_selected (BRASERO_DISC (self));
}

static gboolean
brasero_video_disc_button_pressed_cb (GtkTreeView *tree,
				      GdkEventButton *event,
				      BraseroVideoDisc *self)
{
	GtkWidgetClass *widget_class;
	BraseroVideoDiscPrivate *priv;

	priv = BRASERO_VIDEO_DISC_PRIVATE (self);

	widget_class = GTK_WIDGET_GET_CLASS (tree);

	if (event->button == 3) {
		GtkTreeSelection *selection;
		GtkTreePath *path = NULL;
		GtkWidget *widget;

		gtk_tree_view_get_path_at_pos (tree,
					       event->x,
					       event->y,
					       &path,
					       NULL,
					       NULL,
					       NULL);

		selection = gtk_tree_view_get_selection (tree);

		/* Don't update the selection if the right click was on one of
		 * the already selected rows */
		if (!path || !gtk_tree_selection_path_is_selected (selection, path))
			widget_class->button_press_event (GTK_WIDGET (tree), event);

		widget = gtk_ui_manager_get_widget (priv->manager, "/ContextMenu/PasteAudio");
		if (widget) {
			if (gtk_clipboard_wait_is_text_available (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD)))
				gtk_widget_set_sensitive (widget, TRUE);
			else
				gtk_widget_set_sensitive (widget, FALSE);
		}

		widget = gtk_ui_manager_get_widget (priv->manager,"/ContextMenu");
		gtk_menu_popup (GTK_MENU (widget),
				NULL,
				NULL,
				NULL,
				NULL,
				event->button,
				event->time);
		return TRUE;
	}
	else if (event->button == 1) {
		gboolean result;
		GtkTreePath *treepath = NULL;

		result = gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (tree),
							event->x,
							event->y,
							&treepath,
							NULL,
							NULL,
							NULL);

		/* we call the default handler for the treeview before everything else
		 * so it can update itself (paticularly its selection) before we have
		 * a look at it */
		widget_class->button_press_event (GTK_WIDGET (tree), event);
		
		if (!treepath) {
			GtkTreeSelection *selection;

			/* This is to deselect any row when selecting a 
			 * row that cannot be selected or in an empty
			 * part */
			selection = gtk_tree_view_get_selection (tree);
			gtk_tree_selection_unselect_all (selection);
			return FALSE;
		}
	
		if (!result)
			return FALSE;

		brasero_disc_selection_changed (BRASERO_DISC (self));
		if (event->type == GDK_2BUTTON_PRESS) {
			BraseroVideoFile *file;
			GtkTreeModel *model;

			model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree));
			file = brasero_video_tree_model_path_to_file (BRASERO_VIDEO_TREE_MODEL (model), treepath);
			if (file)
				brasero_video_disc_edit_song_properties_file (self, file);
		}
	}

	return TRUE;
}

static guint
brasero_video_disc_add_ui (BraseroDisc *disc,
			   GtkUIManager *manager,
			   GtkWidget *message)
{
	BraseroVideoDiscPrivate *priv;
	GError *error = NULL;
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
		gtk_action_group_add_actions (priv->disc_group,
					      entries,
					      G_N_ELEMENTS (entries),
					      disc);
/*		gtk_action_group_add_toggle_actions (priv->disc_group,
						     toggle_entries,
						     G_N_ELEMENTS (toggle_entries),
						     disc);	*/
		gtk_ui_manager_insert_action_group (manager,
						    priv->disc_group,
						    0);
	}

	merge_id = gtk_ui_manager_add_ui_from_string (manager,
						      description,
						      -1,
						      &error);
	if (!merge_id) {
		BRASERO_BURN_LOG ("Adding ui elements failed: %s", error->message);
		g_error_free (error);
		return 0;
	}

	priv->manager = manager;
	g_object_ref (manager);
	return merge_id;
}

static void
brasero_video_disc_rename_activated (BraseroVideoDisc *self)
{
	BraseroVideoDiscPrivate *priv;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkTreePath *treepath;
	GtkTreeModel *model;
	GList *list;

	priv = BRASERO_VIDEO_DISC_PRIVATE (self);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));
	list = gtk_tree_selection_get_selected_rows (selection, &model);

	for (; list; list = g_list_remove (list, treepath)) {
		treepath = list->data;

		gtk_widget_grab_focus (priv->tree);
		column = gtk_tree_view_get_column (GTK_TREE_VIEW (priv->tree), 0);
		gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (priv->tree),
					      treepath,
					      NULL,
					      TRUE,
					      0.5,
					      0.5);
		gtk_tree_view_set_cursor (GTK_TREE_VIEW (priv->tree),
					  treepath,
					  column,
					  TRUE);

		gtk_tree_path_free (treepath);
	}
}

static gboolean
brasero_video_disc_key_released_cb (GtkTreeView *tree,
				    GdkEventKey *event,
				    BraseroVideoDisc *self)
{
	BraseroVideoDiscPrivate *priv;

	priv = BRASERO_VIDEO_DISC_PRIVATE (self);
	if (priv->editing)
		return FALSE;

	if (event->keyval == GDK_KP_Delete || event->keyval == GDK_Delete) {
		brasero_video_disc_delete_selected (BRASERO_DISC (self));
	}
	else if (event->keyval == GDK_F2)
		brasero_video_disc_rename_activated (self);

	return FALSE;
}

static void
brasero_video_disc_row_deleted_cb (GtkTreeModel *model,
				   GtkTreePath *path,
				   BraseroVideoDisc *self)
{
	BraseroVideoProject *project;
	BraseroVideoDiscPrivate *priv;

	priv = BRASERO_VIDEO_DISC_PRIVATE (self);
	project = BRASERO_VIDEO_PROJECT (gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree)));
	brasero_disc_contents_changed (BRASERO_DISC (self),
				       brasero_video_project_get_file_num (BRASERO_VIDEO_PROJECT (model)));
}

static void
brasero_video_disc_row_inserted_cb (GtkTreeModel *model,
				    GtkTreePath *path,
				    GtkTreeIter *iter,
				    BraseroVideoDisc *self)
{
	BraseroVideoProject *project;
	BraseroVideoDiscPrivate *priv;

	priv = BRASERO_VIDEO_DISC_PRIVATE (self);
	project = BRASERO_VIDEO_PROJECT (gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree)));
	brasero_disc_contents_changed (BRASERO_DISC (self),
				       brasero_video_project_get_file_num (BRASERO_VIDEO_PROJECT (model)));
}

static void
brasero_video_disc_row_changed_cb (GtkTreeModel *model,
				   GtkTreePath *path,
				   GtkTreeIter *iter,
				   BraseroVideoDisc *self)
{
	BraseroVideoProject *project;
	BraseroVideoDiscPrivate *priv;

	priv = BRASERO_VIDEO_DISC_PRIVATE (self);
	project = BRASERO_VIDEO_PROJECT (gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree)));
	brasero_disc_contents_changed (BRASERO_DISC (self),
				       brasero_video_project_get_file_num (BRASERO_VIDEO_PROJECT (model)));
}

static void
brasero_video_disc_size_changed_cb (BraseroVideoProject *project,
				    BraseroVideoDisc *self)
{
	brasero_disc_size_changed (BRASERO_DISC (self), brasero_video_project_get_size (project));
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
	model = GTK_TREE_MODEL (brasero_video_tree_model_new ());

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

	g_signal_connect (G_OBJECT (model),
			  "size-changed",
			  G_CALLBACK (brasero_video_disc_size_changed_cb),
			  object);
	g_signal_connect (G_OBJECT (model),
			  "not-video-uri",
			  G_CALLBACK (brasero_video_disc_not_video_dialog),
			  object);
	g_signal_connect (G_OBJECT (model),
			  "directory-uri",
			  G_CALLBACK (brasero_video_disc_directory_dialog),
			  object);
	g_signal_connect (G_OBJECT (model),
			  "unreadable-uri",
			  G_CALLBACK (brasero_video_disc_unreadable_uri_dialog),
			  object);
	g_signal_connect (G_OBJECT (model),
			  "vfs-activity",
			  G_CALLBACK (brasero_video_disc_vfs_activity_changed),
			  object);

	priv->tree = gtk_tree_view_new_with_model (model);
	egg_tree_multi_drag_add_drag_support (GTK_TREE_VIEW (priv->tree));
	g_object_unref (G_OBJECT (model));
	gtk_widget_show (priv->tree);

	g_signal_connect (priv->tree,
			  "button-press-event",
			  G_CALLBACK (brasero_video_disc_button_pressed_cb),
			  object);
	g_signal_connect (priv->tree,
			  "key-release-event",
			  G_CALLBACK (brasero_video_disc_key_released_cb),
			  object);

	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (priv->tree), TRUE);
	gtk_tree_view_set_rubber_banding (GTK_TREE_VIEW (priv->tree), TRUE);

	/* columns */
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_min_width (column, 200);

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "pixbuf", BRASERO_VIDEO_TREE_MODEL_MIME_ICON);

	renderer = gtk_cell_renderer_text_new ();
	g_signal_connect (G_OBJECT (renderer), "edited",
			  G_CALLBACK (brasero_video_disc_name_edited_cb), object);
	g_signal_connect (G_OBJECT (renderer), "editing-started",
			  G_CALLBACK (brasero_video_disc_name_editing_started_cb), object);
	g_signal_connect (G_OBJECT (renderer), "editing-canceled",
			  G_CALLBACK (brasero_video_disc_name_editing_canceled_cb), object);

	g_object_set (G_OBJECT (renderer),
		      "mode", GTK_CELL_RENDERER_MODE_EDITABLE,
		      "ellipsize-set", TRUE,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      NULL);

	gtk_tree_view_column_pack_end (column, renderer, TRUE);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "markup", BRASERO_VIDEO_TREE_MODEL_NAME);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "editable", BRASERO_VIDEO_TREE_MODEL_EDITABLE);
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
					    "text", BRASERO_VIDEO_TREE_MODEL_SIZE);
	gtk_tree_view_column_set_title (column, _("Size"));

	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->tree), column);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_expand (column, FALSE);

	/* selection */
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	g_signal_connect (selection,
			  "changed",
			  G_CALLBACK (brasero_video_disc_selection_changed_cb),
			  object);
	gtk_tree_selection_set_select_function (selection,
						brasero_video_disc_selection_function,
						NULL,
						NULL);

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

	/* dnd */
	gtk_tree_view_enable_model_drag_dest (GTK_TREE_VIEW
					      (priv->tree),
					      ntables_cd, nb_targets_cd,
					      GDK_ACTION_COPY |
					      GDK_ACTION_MOVE);

	gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (priv->tree),
						GDK_BUTTON1_MASK,
						ntables_source,
						nb_targets_source,
						GDK_ACTION_MOVE);
}

static void
brasero_video_disc_reset_real (BraseroVideoDisc *self)
{
	BraseroVideoProject *project;
	BraseroVideoDiscPrivate *priv;

	priv = BRASERO_VIDEO_DISC_PRIVATE (self);
	project = BRASERO_VIDEO_PROJECT (gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree)));
	brasero_video_project_reset (project);
	brasero_video_disc_vfs_activity_changed (project, FALSE, self);
}

static void
brasero_video_disc_clear (BraseroDisc *disc)
{
	BraseroVideoDiscPrivate *priv;

	priv = BRASERO_VIDEO_DISC_PRIVATE (disc);

	brasero_video_disc_reset_real (BRASERO_VIDEO_DISC (disc));

	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), 0);
	brasero_disc_size_changed (disc, 0);
}

static void
brasero_video_disc_reset (BraseroDisc *disc)
{
	brasero_video_disc_reset_real (BRASERO_VIDEO_DISC (disc));
}

static void
brasero_video_disc_finalize (GObject *object)
{
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
brasero_video_disc_get_status (BraseroDisc *self)
{
	BraseroVideoProject *project;
	BraseroVideoDiscPrivate *priv;

	priv = BRASERO_VIDEO_DISC_PRIVATE (self);
	project = BRASERO_VIDEO_PROJECT (gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree)));
	return brasero_video_project_get_status (project);
}

BraseroDiscResult
brasero_video_disc_set_session_param (BraseroDisc *self,
				      BraseroBurnSession *session)
{
	BraseroTrackType type;

	type.type = BRASERO_TRACK_TYPE_AUDIO;
	type.subtype.audio_format = BRASERO_AUDIO_FORMAT_UNDEFINED|BRASERO_VIDEO_FORMAT_UNDEFINED;
	brasero_burn_session_set_input_type (session, &type);
	return BRASERO_BURN_OK;
}

BraseroDiscResult
brasero_video_disc_set_session_contents (BraseroDisc *self,
					 BraseroBurnSession *session)
{
	GSList *tracks, *iter;
	BraseroVideoProject *project;
	BraseroVideoDiscPrivate *priv;

	priv = BRASERO_VIDEO_DISC_PRIVATE (self);
	project = BRASERO_VIDEO_PROJECT (gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree)));
	tracks = brasero_video_project_get_contents (project);

	if (!tracks)
		return BRASERO_DISC_ERROR_EMPTY_SELECTION;

	for (iter = tracks; iter; iter = iter->next) {
		BraseroTrack *track;

		track = iter->data;
		brasero_burn_session_add_track (session, track);

		/* It's good practice to unref the track afterwards as we don't
		 * need it anymore. BraseroBurnSession refs it. */
		brasero_track_unref (track);

	}
	g_slist_free (tracks);
	return BRASERO_DISC_OK;
}

static BraseroDiscResult
brasero_video_disc_get_track (BraseroDisc *disc,
			      BraseroDiscTrack *disc_track)
{
	GSList *iter;
	GSList *tracks;
	BraseroVideoProject *project;
	BraseroVideoDiscPrivate *priv;

	disc_track->type = BRASERO_DISC_TRACK_VIDEO;

	priv = BRASERO_VIDEO_DISC_PRIVATE (disc);
	project = BRASERO_VIDEO_PROJECT (gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree)));
	tracks = brasero_video_project_get_contents (project);

	for (iter = tracks; iter; iter = iter->next) {
		BraseroDiscSong *song;
		BraseroTrack *track;

		track = iter->data;

		song = g_new0 (BraseroDiscSong, 1);
		song->uri = brasero_track_get_audio_source (track, TRUE);;
		song->start = brasero_track_get_audio_start (track);
		song->end = brasero_track_get_audio_end (track);
		song->info = brasero_song_info_copy (brasero_track_get_audio_info (track));
		disc_track->contents.tracks = g_slist_append (disc_track->contents.tracks, song);
	}

	g_slist_foreach (tracks, (GFunc) brasero_track_unref, NULL);
	g_slist_free (tracks);

	return BRASERO_DISC_OK;
}

static BraseroDiscResult
brasero_video_disc_load_track (BraseroDisc *disc,
			       BraseroDiscTrack *track)
{
	GSList *iter;
	BraseroVideoProject *project;
	BraseroVideoDiscPrivate *priv;

	g_return_val_if_fail (track->type == BRASERO_DISC_TRACK_VIDEO, FALSE);

	if (track->contents.tracks == NULL)
		return BRASERO_DISC_ERROR_EMPTY_SELECTION;

	priv = BRASERO_VIDEO_DISC_PRIVATE (disc);
	project = BRASERO_VIDEO_PROJECT (gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree)));
	priv->loading = g_slist_length (track->contents.tracks);

	for (iter = track->contents.tracks; iter; iter = iter->next) {
		BraseroDiscSong *song;

		song = iter->data;

		brasero_video_project_add_uri (BRASERO_VIDEO_PROJECT (project),
					       song->uri,
					       NULL,
					       song->start,
					       song->end);
	}
	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook), 1);

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

	iface->get_track = brasero_video_disc_get_track;
	iface->load_track = brasero_video_disc_load_track;

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

