/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/***************************************************************************
*            play-list.c
*
*  mer mai 25 22:22:53 2005
*  Copyright  2005  Philippe Rouquier
*  brasero-app@wanadoo.fr
****************************************************************************/

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


#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib-object.h>

#include <gio/gio.h>

#include <gtk/gtk.h>

#include <totem-pl-parser.h>

#include "brasero-misc.h"

#include "brasero-units.h"

#include "brasero-app.h"
#include "brasero-playlist.h"
#include "brasero-search-engine.h"
#include "brasero-utils.h"
#include "brasero-metadata.h"
#include "brasero-io.h"
#include "eggtreemultidnd.h"

#include "brasero-uri-container.h"
#include "brasero-layout-object.h"


struct BraseroPlaylistPrivate {
	BraseroSearchEngine *engine;
	int id;

	GtkWidget *tree;
	GtkWidget *button_add;
	GtkWidget *button_remove;
	guint activity_counter;

	BraseroIOJobBase *parse_type;

	gint searched:1;
};

enum {
	BRASERO_PLAYLIST_DISPLAY_COL,
	BRASERO_PLAYLIST_NB_SONGS_COL,
	BRASERO_PLAYLIST_LEN_COL,
	BRASERO_PLAYLIST_GENRE_COL,
	BRASERO_PLAYLIST_URI_COL,
	BRASERO_PLAYLIST_DSIZE_COL,
	BRASERO_PLAYLIST_NB_COL,
};

enum {
	TARGET_URIS_LIST,
};

static GtkTargetEntry ntables[] = {
	{"text/uri-list", 0, TARGET_URIS_LIST}
};
static guint nb_ntables = sizeof (ntables) / sizeof (ntables[0]);

static void brasero_playlist_iface_uri_container_init (BraseroURIContainerIFace *iface);
static void brasero_playlist_iface_layout_object_init (BraseroLayoutObjectIFace *iface);

G_DEFINE_TYPE_WITH_CODE (BraseroPlaylist,
			 brasero_playlist,
			 GTK_TYPE_BOX,
			 G_IMPLEMENT_INTERFACE (BRASERO_TYPE_URI_CONTAINER,
					        brasero_playlist_iface_uri_container_init)
			 G_IMPLEMENT_INTERFACE (BRASERO_TYPE_LAYOUT_OBJECT,
					        brasero_playlist_iface_layout_object_init));

#define BRASERO_PLAYLIST_SPACING 6

static void
brasero_playlist_get_proportion (BraseroLayoutObject *object,
				 gint *header,
				 gint *center,
				 gint *footer)
{
	GtkRequisition requisition;

	gtk_widget_get_preferred_size (gtk_widget_get_parent (BRASERO_PLAYLIST (object)->priv->button_add),
				 &requisition, NULL);
	(*footer) = requisition.height + BRASERO_PLAYLIST_SPACING;
}

static void
brasero_playlist_increase_activity_counter (BraseroPlaylist *playlist)
{
	GdkWindow *window;

	window = gtk_widget_get_window (GTK_WIDGET (playlist->priv->tree));
	if (!window)
		return;

	if (playlist->priv->activity_counter == 0) {
		GdkCursor *cursor;

		cursor = gdk_cursor_new (GDK_WATCH);
		gdk_window_set_cursor (window,
				       cursor);
		g_object_unref (cursor);
	}
	playlist->priv->activity_counter++;
}

static void
brasero_playlist_decrease_activity_counter (BraseroPlaylist *playlist)
{
	GdkWindow *window;

	if (playlist->priv->activity_counter > 0)
		playlist->priv->activity_counter--;

	window = gtk_widget_get_window (GTK_WIDGET (playlist->priv->tree));
	if (!window)
		return;

	if (playlist->priv->activity_counter == 0)
		gdk_window_set_cursor (window, NULL);
}

static void
brasero_playlist_search_playlists_rhythmbox (BraseroPlaylist *playlist)
{
/*	RBSource *source;

	manager = rb_playlist_manager_new ();
	lists = rb_playlist_manager_get_playlists (manager);
*/
}

static void
brasero_playlist_search (BraseroPlaylist *playlist)
{
	const gchar* mimes [] = {"audio/x-ms-asx",
		"audio/x-mpegurl",
		"audio/x-scpls",
		"audio/x-mp3-playlist",
		NULL};

	brasero_search_engine_new_query (playlist->priv->engine, NULL);
	brasero_search_engine_set_query_mime (playlist->priv->engine, mimes);
	brasero_search_engine_start_query (playlist->priv->engine);
	brasero_playlist_increase_activity_counter (playlist);
}

static gboolean
brasero_playlist_try_again (BraseroPlaylist *playlist)
{
	if (!brasero_search_engine_is_available (playlist->priv->engine))
		return TRUE;

	brasero_playlist_search (playlist);

	playlist->priv->id = 0;
	return FALSE;
}

static void
brasero_playlist_start_search (BraseroPlaylist *playlist)
{
	if (!playlist->priv->engine)
		return;

	if (!brasero_search_engine_is_available (playlist->priv->engine)) {
		/* we will retry in 10 seconds */
		playlist->priv->id = g_timeout_add_seconds (10,
							    (GSourceFunc) brasero_playlist_try_again,
							    playlist);
		return;
	}

	brasero_playlist_search (playlist);
}

static gboolean
brasero_playlist_draw_cb (GtkWidget *widget,
			  cairo_t *cr,
			  gpointer null_data)
{
	BraseroPlaylist *playlist = BRASERO_PLAYLIST (widget);

	/* we only want to load playlists if the user is going to use them that
	 * is if they become apparent. That will avoid overhead */
	if (!playlist->priv->searched) {
		playlist->priv->searched = 1;
		brasero_playlist_start_search (playlist);
		brasero_playlist_search_playlists_rhythmbox (playlist);
	}

	return FALSE;
}

static gchar **
brasero_playlist_get_selected_uris_real (BraseroPlaylist *playlist)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreePath *path;
	GList *rows, *iter;
	GtkTreeIter row;
	gchar **uris = NULL, *uri;
	GPtrArray *array;
	gboolean valid;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (playlist->priv->tree));
	rows = gtk_tree_selection_get_selected_rows (selection, &model);

	if (rows == NULL)
		return NULL;

	array = g_ptr_array_sized_new (g_list_length (rows) + 1);
	for (iter = rows; iter; iter = iter->next) {
		path = iter->data;
		valid = gtk_tree_model_get_iter (model, &row, path);
		gtk_tree_path_free (path);

		if (valid == FALSE)
			continue;

		/* FIXME : we must find a way to reverse the list */
		/* check if it is a list or not */
		if (gtk_tree_model_iter_has_child (model, &row)) {
			GtkTreeIter child;

			if (gtk_tree_model_iter_children (model, &child, &row) == FALSE)
				continue;

			do {
				/* first check if the row is selected to prevent to put it in the list twice */
				if (gtk_tree_selection_iter_is_selected (selection, &child) == TRUE)
					continue;

				gtk_tree_model_get (model, &child,
						    BRASERO_PLAYLIST_URI_COL, &uri,
						    -1);
				g_ptr_array_add (array, uri);
			} while (gtk_tree_model_iter_next (model, &child));

			continue;
		}

		gtk_tree_model_get (model, &row,
				    BRASERO_PLAYLIST_URI_COL, &uri,
				    -1);
		g_ptr_array_add (array, uri);
	}

	g_list_free (rows);

	g_ptr_array_set_size (array, array->len + 1);
	uris = (gchar **) array->pdata;
	g_ptr_array_free (array, FALSE);
	return uris;
}

static gchar **
brasero_playlist_get_selected_uris (BraseroURIContainer *container)
{
	BraseroPlaylist *playlist;

	playlist = BRASERO_PLAYLIST (container);
	return brasero_playlist_get_selected_uris_real (playlist);
}

static gchar *
brasero_playlist_get_selected_uri (BraseroURIContainer *container)
{
	BraseroPlaylist *playlist;
	gchar **uris = NULL;
	gchar *uri;

	playlist = BRASERO_PLAYLIST (container);
	uris = brasero_playlist_get_selected_uris_real (playlist);

	if (uris) {
		uri = g_strdup (uris [0]);
		g_strfreev (uris);
		return uri;
	}

	return NULL;
}

static void
brasero_playlist_remove_cb (GtkButton *button, BraseroPlaylist *playlist)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter row;
	GList *rows, *iter;
	gboolean valid;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (playlist->priv->tree));
	rows = gtk_tree_selection_get_selected_rows (selection, &model);

	if (rows == NULL)
		return;

	/* we just remove the lists removing particular songs would be a nonsense */
	/* we must reverse the list otherwise the last paths wouldn't be valid */
	for (iter = g_list_last (rows); iter; iter = iter->prev) {
		path = iter->data;
		valid = gtk_tree_model_get_iter (model, &row, path);
		gtk_tree_path_free (path);

		if (valid == FALSE)	/* if we remove the whole list it could happen that we try to remove twice a song */
			continue;

		if (gtk_tree_model_iter_has_child (model, &row)) {
			GtkTreeIter child;

			/* we remove the songs if it's a list */
			gtk_tree_model_iter_children (model, &child, &row);
			while (gtk_tree_store_remove (GTK_TREE_STORE (model), &child) == TRUE);
			gtk_tree_store_remove (GTK_TREE_STORE (model),
					       &row);
		}
	}

	g_list_free (rows);
}

static void
brasero_playlist_drag_data_get_cb (GtkTreeView *tree,
				   GdkDragContext *drag_context,
				   GtkSelectionData *selection_data,
				   guint info,
				   guint time,
				   BraseroPlaylist *playlist)
{
	gchar **uris;

	uris = brasero_playlist_get_selected_uris_real (playlist);
	gtk_selection_data_set_uris (selection_data, uris);
	g_strfreev (uris);
}

struct _BraseroPlaylistParseData {
	GtkTreeRowReference *reference;

	guint title:1;
	guint quiet:1;
};
typedef struct _BraseroPlaylistParseData BraseroPlaylistParseData;

static void
brasero_playlist_dialog_error (BraseroPlaylist *playlist, const gchar *uri)
{
	gchar *name;
	gchar *primary;

	BRASERO_GET_BASENAME_FOR_DISPLAY (uri, name);

	primary = g_strdup_printf (_("Error parsing playlist \"%s\"."), name);
	brasero_app_alert (brasero_app_get_default (),
			   primary,
			   _("An unknown error occurred"),
			   GTK_MESSAGE_ERROR);
	g_free (primary);
	g_free (name);
}

static void
brasero_playlist_parse_end (GObject *object,
			    gboolean cancelled,
			    gpointer callback_data)
{
	BraseroPlaylistParseData *data = callback_data;
	BraseroPlaylist *playlist = BRASERO_PLAYLIST (object);

	brasero_playlist_decrease_activity_counter (playlist);

	gtk_tree_row_reference_free (data->reference);
	g_free (data);
}

static void
brasero_playlist_parse_result (GObject *object,
			       GError *error,
			       const gchar *uri,
			       GFileInfo *info,
			       gpointer callback_data)
{
	gint num;
	gint64 total_length;
	GtkTreeModel *model;
	GtkTreePath *treepath;
	GtkTreeIter parent, row;
	gchar *len_string, *num_string;
	BraseroPlaylistParseData *data = callback_data;
	BraseroPlaylist *playlist = BRASERO_PLAYLIST (object);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (playlist->priv->tree));
	treepath = gtk_tree_row_reference_get_path (data->reference);
	gtk_tree_model_get_iter (model, &parent, treepath);
	gtk_tree_path_free (treepath);

	if (info && g_file_info_get_attribute_boolean (info, BRASERO_IO_IS_PLAYLIST)) {
		const gchar *playlist_title = NULL;

		/* The first entry returned is always the playlist as a whole:
		 * if it was successfully parsed uri is the title if any. If not
		 * it's simply the URI */

		/* this is for the playlist as a whole */
		if (error) {
			if (!data->quiet)
				brasero_playlist_dialog_error (playlist, uri);

			gtk_list_store_remove (GTK_LIST_STORE (model), &parent);
			data->title = 1;
			return;
		}

		playlist_title = g_file_info_get_attribute_string (info, BRASERO_IO_PLAYLIST_TITLE);
		if (playlist_title)
			gtk_tree_store_set (GTK_TREE_STORE (model), &parent,
					    BRASERO_PLAYLIST_DISPLAY_COL, playlist_title,
					    -1);

		data->title = 1;
		return;
	}

    	/* See if the song can be added */
	if (!error && info && g_file_info_get_attribute_boolean (info, BRASERO_IO_HAS_AUDIO)) {
		gchar *name;
		guint64 len;
		const gchar *title;
		const gchar *genre;

		gtk_tree_store_append (GTK_TREE_STORE (model), &row, &parent);

		len = g_file_info_get_attribute_uint64 (info, BRASERO_IO_LEN);
		title = g_file_info_get_attribute_string (info, BRASERO_IO_TITLE);
		genre = g_file_info_get_attribute_string (info, BRASERO_IO_GENRE);

		if (len > 0)
			len_string = brasero_units_get_time_string (len, TRUE, FALSE);
		else
			len_string = NULL;

		BRASERO_GET_BASENAME_FOR_DISPLAY (uri, name);
		gtk_tree_store_set (GTK_TREE_STORE (model), &row,
				    BRASERO_PLAYLIST_DISPLAY_COL, title ? title : name,
				    BRASERO_PLAYLIST_LEN_COL, len_string,
				    BRASERO_PLAYLIST_GENRE_COL, genre,
				    BRASERO_PLAYLIST_URI_COL, uri,
				    BRASERO_PLAYLIST_DSIZE_COL, len,
				    -1);
		g_free (name);
		g_free (len_string);

		if (len)
			total_length += len;
	}

	/* update the playlist information */
	num = gtk_tree_model_iter_n_children (model, &parent);
	if (!num)
		num_string = g_strdup (_("Empty"));
	else	/* Translators: %d is the number of songs */
		num_string = g_strdup_printf (ngettext ("%d song", "%d songs", num), num);

	/* get total length in time of the playlist */
	gtk_tree_model_get (model, &parent,
			    BRASERO_PLAYLIST_DSIZE_COL, &total_length,
			    -1);

  	if (total_length > 0)
		len_string = brasero_units_get_time_string (total_length, TRUE, FALSE);
	else
		len_string = NULL;

	gtk_tree_store_set (GTK_TREE_STORE (model), &parent,
			    BRASERO_PLAYLIST_NB_SONGS_COL, num_string,
			    BRASERO_PLAYLIST_LEN_COL, len_string,
			    BRASERO_PLAYLIST_DSIZE_COL, total_length,
			    -1);
	g_free (len_string);
	g_free (num_string);
}

static GtkTreeRowReference *
brasero_playlist_insert (BraseroPlaylist *playlist, const gchar *uri)
{
	gchar *name;
	GtkTreeIter parent;
	GtkTreeModel *model;
	GtkTreePath *treepath;
	GtkTreeRowReference *reference;

	/* actually add it */
	BRASERO_GET_BASENAME_FOR_DISPLAY (uri, name);
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (playlist->priv->tree));
	gtk_tree_store_append (GTK_TREE_STORE (model), &parent, NULL);
	gtk_tree_store_set (GTK_TREE_STORE (model), &parent,
			    BRASERO_PLAYLIST_DISPLAY_COL, name,
			    BRASERO_PLAYLIST_URI_COL, uri,
			    BRASERO_PLAYLIST_NB_SONGS_COL, _("(loading…)"),
			    -1);
	g_free (name);

	treepath = gtk_tree_model_get_path (model, &parent);
	reference = gtk_tree_row_reference_new (model, treepath);
	gtk_tree_path_free (treepath);

	return reference;
}

static void
brasero_playlist_add_uri_playlist (BraseroPlaylist *playlist,
				   const gchar *uri,
				   gboolean quiet)
{
	BraseroPlaylistParseData *data;

	data = g_new0 (BraseroPlaylistParseData, 1);
	data->reference = brasero_playlist_insert (playlist, uri);

	if (!playlist->priv->parse_type)
		playlist->priv->parse_type = brasero_io_register (G_OBJECT (playlist),
								  brasero_playlist_parse_result,
								  brasero_playlist_parse_end, 
								  NULL);

	brasero_io_parse_playlist (uri,
				   playlist->priv->parse_type,
				   BRASERO_IO_INFO_PERM|
				   BRASERO_IO_INFO_MIME|
				   BRASERO_IO_INFO_METADATA,
				   data);
	brasero_playlist_increase_activity_counter (playlist);
}

static void
brasero_playlist_add_cb (GtkButton *button, BraseroPlaylist *playlist)
{
	GtkWidget *dialog, *toplevel;
	gchar *uri;
	GSList *uris, *iter;
	gint result;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (playlist));
	if (!gtk_widget_is_toplevel (toplevel))
		return;

	dialog = gtk_file_chooser_dialog_new (_("Select Playlist"),
					      GTK_WINDOW (toplevel),
					      GTK_FILE_CHOOSER_ACTION_OPEN,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					      NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog),
					 GTK_RESPONSE_ACCEPT);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), FALSE);
	gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (dialog), TRUE);
	gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (dialog),
						 g_get_home_dir ());

	gtk_widget_show_all (dialog);
	result = gtk_dialog_run (GTK_DIALOG (dialog));
	if (result == GTK_RESPONSE_CANCEL) {
		gtk_widget_destroy (dialog);
		return;
	}

	uris = gtk_file_chooser_get_uris (GTK_FILE_CHOOSER (dialog));
	gtk_widget_destroy (dialog);

	for (iter = uris; iter; iter = iter->next) {
		uri = iter->data;
		brasero_playlist_add_uri_playlist (playlist, uri, FALSE);
		g_free (uri);
	}
	g_slist_free (uris);
}

static void
brasero_playlist_search_hit_added_cb (BraseroSearchEngine *engine,
                                      gpointer hit,
                                      BraseroPlaylist *playlist)
{
	const char *uri;

	uri = brasero_search_engine_uri_from_hit (engine, hit);
	brasero_playlist_add_uri_playlist (playlist, uri, TRUE);
	brasero_playlist_decrease_activity_counter (playlist);
}

static void
brasero_playlist_search_hit_substracted_cb (BraseroSearchEngine *engine,
                                            gpointer hit,
                                            BraseroPlaylist *playlist)
{
	GtkTreeModel *model;
	GtkTreeIter row;
	const gchar *uri;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (playlist->priv->tree));
	uri = brasero_search_engine_uri_from_hit (engine, hit);

	if (!gtk_tree_model_get_iter_first (model, &row))
		return;

	do {
		char *row_uri;

		gtk_tree_model_get (model, &row,
				    BRASERO_PLAYLIST_URI_COL, &row_uri,
				    -1);

		if (!strcmp (row_uri, uri)) {
			gtk_tree_store_remove (GTK_TREE_STORE (model), &row);
			g_free (row_uri);
			return;
		}

		g_free (row_uri);
	} while (gtk_tree_model_iter_next (model, &row));
}

static void
brasero_playlist_search_finished_cb (BraseroSearchEngine *engine,
                                     BraseroPlaylist *playlist)
{
	brasero_playlist_decrease_activity_counter (playlist);
}

static void
brasero_playlist_search_error_cb (BraseroSearchEngine *engine,
                                  BraseroPlaylist *playlist)
{

}

static void
brasero_playlist_row_activated_cb (GtkTreeView *tree,
				   GtkTreeIter *row,
				   GtkTreeViewColumn *column,
				   BraseroPlaylist *playlist)
{
	brasero_uri_container_uri_activated (BRASERO_URI_CONTAINER (playlist));
}

static void
brasero_playlist_selection_changed_cb (GtkTreeSelection *selection,
				       BraseroPlaylist *playlist)
{
	brasero_uri_container_uri_selected (BRASERO_URI_CONTAINER (playlist));
}

static void
brasero_playlist_init (BraseroPlaylist *obj)
{
	GtkWidget *hbox, *scroll;
	GtkTreeStore *store = NULL;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	obj->priv = g_new0 (BraseroPlaylistPrivate, 1);
	gtk_box_set_spacing (GTK_BOX (obj), BRASERO_PLAYLIST_SPACING);
	gtk_orientable_set_orientation (GTK_ORIENTABLE (obj), GTK_ORIENTATION_VERTICAL);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox);

	obj->priv->button_add = gtk_button_new_from_stock (GTK_STOCK_ADD);
	gtk_widget_show (obj->priv->button_add);
	gtk_box_pack_end (GTK_BOX (hbox),
			  obj->priv->button_add,
			  FALSE,
			  FALSE,
			  0);
	g_signal_connect (G_OBJECT (obj->priv->button_add),
			  "clicked",
			  G_CALLBACK (brasero_playlist_add_cb),
			  obj);

	obj->priv->button_remove = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
	gtk_widget_show (obj->priv->button_remove);
	gtk_box_pack_end (GTK_BOX (hbox),
			  obj->priv->button_remove,
			  FALSE,
			  FALSE,
			  0);
	g_signal_connect (G_OBJECT (obj->priv->button_remove),
			  "clicked",
			  G_CALLBACK (brasero_playlist_remove_cb),
			  obj);

	gtk_box_pack_end (GTK_BOX (obj), hbox, FALSE, FALSE, 0);

	store = gtk_tree_store_new (BRASERO_PLAYLIST_NB_COL,
				    G_TYPE_STRING,
				    G_TYPE_STRING,
				    G_TYPE_STRING,
				    G_TYPE_STRING, 
				    G_TYPE_STRING,
				    G_TYPE_INT64);

	obj->priv->tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
	egg_tree_multi_drag_add_drag_support (GTK_TREE_VIEW (obj->priv->tree));

	gtk_tree_view_set_enable_tree_lines (GTK_TREE_VIEW (obj->priv->tree), TRUE);
	gtk_tree_view_set_rubber_banding (GTK_TREE_VIEW (obj->priv->tree), TRUE);
	gtk_tree_view_set_headers_clickable (GTK_TREE_VIEW (obj->priv->tree), TRUE);
	gtk_tree_view_set_enable_search (GTK_TREE_VIEW (obj->priv->tree), TRUE);
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (obj->priv->tree), TRUE);
	gtk_tree_view_set_expander_column (GTK_TREE_VIEW (obj->priv->tree),
					   BRASERO_PLAYLIST_DISPLAY_COL);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Playlists"),
							   renderer, "text",
							   BRASERO_PLAYLIST_DISPLAY_COL,
							   NULL);
	gtk_tree_view_column_set_sort_column_id (column,
						 BRASERO_PLAYLIST_DISPLAY_COL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (obj->priv->tree),
				     column);
	gtk_tree_view_column_set_expand (column, TRUE);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Number of Songs"),
							   renderer, "text",
							   BRASERO_PLAYLIST_NB_SONGS_COL,
							   NULL);
	gtk_tree_view_column_set_sort_column_id (column,
						 BRASERO_PLAYLIST_NB_SONGS_COL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (obj->priv->tree),
				     column);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Length"),
							   renderer, "text",
							   BRASERO_PLAYLIST_LEN_COL,
							   NULL);
	gtk_tree_view_column_set_sort_column_id (column,
						 BRASERO_PLAYLIST_LEN_COL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (obj->priv->tree),
				     column);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Genre"), renderer,
							   "text",
							   BRASERO_PLAYLIST_GENRE_COL,
							   NULL);
	gtk_tree_view_column_set_sort_column_id (column,
						 BRASERO_PLAYLIST_GENRE_COL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (obj->priv->tree),
				     column);

	gtk_tree_view_set_search_column (GTK_TREE_VIEW (obj->priv->tree),
					 BRASERO_PLAYLIST_DISPLAY_COL);
	gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (obj->priv->tree),
						GDK_BUTTON1_MASK, ntables,
						nb_ntables,
						GDK_ACTION_COPY |
						GDK_ACTION_MOVE);

	g_signal_connect (G_OBJECT (obj->priv->tree), "drag_data_get",
			  G_CALLBACK (brasero_playlist_drag_data_get_cb),
			  obj);

	g_signal_connect (G_OBJECT (obj->priv->tree), "row_activated",
			  G_CALLBACK (brasero_playlist_row_activated_cb),
			  obj);

	g_signal_connect (G_OBJECT (gtk_tree_view_get_selection (GTK_TREE_VIEW (obj->priv->tree))),
			  "changed",
			  G_CALLBACK (brasero_playlist_selection_changed_cb),
			  obj);

	gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (obj->priv->tree)),
				     GTK_SELECTION_MULTIPLE);

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll),
					     GTK_SHADOW_IN);

	gtk_container_add (GTK_CONTAINER (scroll), obj->priv->tree);
	gtk_box_pack_start (GTK_BOX (obj), scroll, TRUE, TRUE, 0);

	obj->priv->engine = brasero_search_engine_new_default ();
	if (!obj->priv->engine)
		return;

	g_signal_connect (G_OBJECT (obj->priv->engine), "hit-added",
			  G_CALLBACK (brasero_playlist_search_hit_added_cb),
			  obj);
	g_signal_connect (G_OBJECT (obj->priv->engine), "hit-removed",
			  G_CALLBACK (brasero_playlist_search_hit_substracted_cb),
			  obj);
	g_signal_connect (G_OBJECT (obj->priv->engine), "search-finished",
			  G_CALLBACK (brasero_playlist_search_finished_cb),
			  obj);
	g_signal_connect (G_OBJECT (obj->priv->engine), "search-error",
			  G_CALLBACK (brasero_playlist_search_error_cb),
			  obj);
}

static void
brasero_playlist_destroy (GtkWidget *object)
{
	BraseroPlaylist *playlist = BRASERO_PLAYLIST (object);

	if (playlist->priv->id) {
		g_source_remove (playlist->priv->id);
		playlist->priv->id = 0;
	}

	if (playlist->priv->engine) {
		g_object_unref (playlist->priv->engine);
		playlist->priv->engine = NULL;
	}

	/* NOTE: we must do it here since cancel could call brasero_playlist_end
	 * itself calling decrease_activity_counter. In finalize the latter will
	 * raise problems since the GdkWindow has been destroyed */
	if (playlist->priv->parse_type) {
		brasero_io_cancel_by_base (playlist->priv->parse_type);
		brasero_io_job_base_free (playlist->priv->parse_type);
		playlist->priv->parse_type = NULL;
	}

	if (GTK_WIDGET_CLASS (brasero_playlist_parent_class)->destroy)
		GTK_WIDGET_CLASS (brasero_playlist_parent_class)->destroy (object);
}

static void
brasero_playlist_finalize (GObject *object)
{
	BraseroPlaylist *cobj;

	cobj = BRASERO_PLAYLIST (object);

	g_free (cobj->priv);

	G_OBJECT_CLASS (brasero_playlist_parent_class)->finalize (object);
}

static void
brasero_playlist_class_init (BraseroPlaylistClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->finalize = brasero_playlist_finalize;
	widget_class->destroy = brasero_playlist_destroy;
}

static void
brasero_playlist_iface_uri_container_init (BraseroURIContainerIFace *iface)
{
	iface->get_selected_uri = brasero_playlist_get_selected_uri;
	iface->get_selected_uris = brasero_playlist_get_selected_uris;
}

static void
brasero_playlist_iface_layout_object_init (BraseroLayoutObjectIFace *iface)
{
	iface->get_proportion = brasero_playlist_get_proportion;
}

GtkWidget *
brasero_playlist_new ()
{
	BraseroPlaylist *obj;

	obj = BRASERO_PLAYLIST (g_object_new (BRASERO_TYPE_PLAYLIST, NULL));

	g_signal_connect (obj,
			  "draw",
			  G_CALLBACK (brasero_playlist_draw_cb),
			  NULL);

	return GTK_WIDGET (obj);
}
