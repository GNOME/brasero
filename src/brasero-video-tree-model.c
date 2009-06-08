/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2007 <bonfire-app@wanadoo.fr>
 * 
 *  Brasero is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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
#include <glib/gi18n-lib.h>

#include <gtk/gtk.h>

#include "brasero-session-cfg.h"
#include "brasero-tags.h"
#include "brasero-track-stream-cfg.h"

#include "brasero-utils.h"
#include "brasero-video-tree-model.h"

#include "eggtreemultidnd.h"

typedef struct _BraseroVideoTreeModelPrivate BraseroVideoTreeModelPrivate;
struct _BraseroVideoTreeModelPrivate
{
	BraseroSessionCfg *session;

	guint stamp;
	GtkIconTheme *theme;
};

#define BRASERO_VIDEO_TREE_MODEL_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_VIDEO_TREE_MODEL, BraseroVideoTreeModelPrivate))

static void
brasero_video_tree_model_multi_drag_source_iface_init (gpointer g_iface, gpointer data);
static void
brasero_video_tree_model_drag_source_iface_init (gpointer g_iface, gpointer data);
static void
brasero_video_tree_model_drag_dest_iface_init (gpointer g_iface, gpointer data);
static void
brasero_video_tree_model_iface_init (gpointer g_iface, gpointer data);

G_DEFINE_TYPE_WITH_CODE (BraseroVideoTreeModel,
			 brasero_video_tree_model,
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
					        brasero_video_tree_model_iface_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_DEST,
					        brasero_video_tree_model_drag_dest_iface_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE,
					        brasero_video_tree_model_drag_source_iface_init)
			 G_IMPLEMENT_INTERFACE (EGG_TYPE_TREE_MULTI_DRAG_SOURCE,
					        brasero_video_tree_model_multi_drag_source_iface_init));


/**
 * This is mainly a list so the following functions are not implemented.
 * But we may need them for AUDIO models when we display GAPs
 */
static gboolean
brasero_video_tree_model_iter_parent (GtkTreeModel *model,
				      GtkTreeIter *iter,
				      GtkTreeIter *child)
{
	return FALSE;
}

static gboolean
brasero_video_tree_model_iter_nth_child (GtkTreeModel *model,
					 GtkTreeIter *iter,
					 GtkTreeIter *parent,
					 gint n)
{
	return FALSE;
}

static gint
brasero_video_tree_model_iter_n_children (GtkTreeModel *model,
					  GtkTreeIter *iter)
{
	return 0;
}

static gboolean
brasero_video_tree_model_iter_has_child (GtkTreeModel *model,
					 GtkTreeIter *iter)
{
	return FALSE;
}

static gboolean
brasero_video_tree_model_iter_children (GtkTreeModel *model,
				        GtkTreeIter *iter,
				        GtkTreeIter *parent)
{
	return FALSE;
}

static void
brasero_video_tree_model_get_value (GtkTreeModel *model,
				    GtkTreeIter *iter,
				    gint column,
				    GValue *value)
{
	BraseroVideoTreeModelPrivate *priv;
	BraseroVideoTreeModel *self;
	BraseroStatus *status;
	BraseroTrack *track;
	const gchar *string;
	GdkPixbuf *pixbuf;
	GValue *value_tag;
	gchar *text;

	self = BRASERO_VIDEO_TREE_MODEL (model);
	priv = BRASERO_VIDEO_TREE_MODEL_PRIVATE (model);

	/* make sure that iter comes from us */
	g_return_if_fail (priv->stamp == iter->stamp);
	g_return_if_fail (iter->user_data != NULL);

	track = iter->user_data;

	if (!BRASERO_IS_TRACK_STREAM (track))
		return;

	switch (column) {
	case BRASERO_VIDEO_TREE_MODEL_NAME:
		g_value_init (value, G_TYPE_STRING);

		string = brasero_track_tag_lookup_string (track, BRASERO_TRACK_STREAM_TITLE_TAG);
		if (string)
			g_value_set_string (value, string);
		else {
			gchar *uri;
			gchar *name;
			gchar *path;
			gchar *unescaped;

			uri = brasero_track_stream_get_source (BRASERO_TRACK_STREAM (track), TRUE);
			unescaped = g_uri_unescape_string (uri, NULL);
			g_free (uri);

			path = g_filename_from_uri (unescaped, NULL, NULL);
			g_free (unescaped);

			name = g_path_get_basename (path);
			g_free (path);

			g_value_set_string (value, name);
			g_free (name);
		}

		return;

	case BRASERO_VIDEO_TREE_MODEL_MIME_ICON:
		status = brasero_status_new ();
		brasero_track_get_status (track, status);

		g_value_init (value, GDK_TYPE_PIXBUF);

		value_tag = NULL;
		brasero_track_tag_lookup (BRASERO_TRACK (track),
					  BRASERO_TRACK_STREAM_THUMBNAIL_TAG,
					  &value_tag);
		if (value_tag)
			pixbuf = g_value_dup_object (value_tag);
		else if (brasero_status_get_result (status) == BRASERO_BURN_NOT_READY) {
			pixbuf = gtk_icon_theme_load_icon (priv->theme,
							   "image-loading",
							   48,
							   0,
							   NULL);
		}
		else {
			pixbuf = gtk_icon_theme_load_icon (priv->theme,
							   "image-missing",
							   48,
							   0,
							   NULL);
		}

		g_value_set_object (value, pixbuf);
		brasero_status_free (status);
		g_object_unref (pixbuf);
		return;

	case BRASERO_VIDEO_TREE_MODEL_SIZE:
		status = brasero_status_new ();
		brasero_track_get_status (track, status);

		g_value_init (value, G_TYPE_STRING);

		if (brasero_status_get_result (status) == BRASERO_BURN_OK) {
			guint64 len = 0;

			brasero_track_stream_get_length (BRASERO_TRACK_STREAM (track), &len);
			text = brasero_units_get_time_string (len, TRUE, FALSE);
			g_value_set_string (value, text);
			g_free (text);
		}
		else
			g_value_set_string (value, _("(loading ...)"));

		brasero_status_free (status);
		return;

	case BRASERO_VIDEO_TREE_MODEL_EDITABLE:
		g_value_init (value, G_TYPE_BOOLEAN);
		/* This can be used for gap lines */
		g_value_set_boolean (value, TRUE);
		//g_value_set_boolean (value, file->editable);
		return;

	case BRASERO_VIDEO_TREE_MODEL_SELECTABLE:
		g_value_init (value, G_TYPE_BOOLEAN);
		/* This can be used for gap lines */
		g_value_set_boolean (value, TRUE);
		//g_value_set_boolean (value, file->editable);
		return;

	default:
		break;
	}
}

GtkTreePath *
brasero_video_tree_model_track_to_path (BraseroVideoTreeModel *self,
				        BraseroTrack *track)
{
	BraseroVideoTreeModelPrivate *priv;
	GtkTreePath *path;
	GSList *tracks;
	guint nth;

	if (!BRASERO_IS_TRACK_STREAM (track))
		return NULL;

	priv = BRASERO_VIDEO_TREE_MODEL_PRIVATE (self);

	path = gtk_tree_path_new ();
	tracks = brasero_burn_session_get_tracks (BRASERO_BURN_SESSION (priv->session));
	nth = g_slist_index (tracks, track);
	gtk_tree_path_prepend_index (path, nth);

	return path;
}

static GtkTreePath *
brasero_video_tree_model_get_path (GtkTreeModel *model,
				   GtkTreeIter *iter)
{
	BraseroVideoTreeModelPrivate *priv;
	BraseroTrack *track;
	GtkTreePath *path;

	priv = BRASERO_VIDEO_TREE_MODEL_PRIVATE (model);

	/* make sure that iter comes from us */
	g_return_val_if_fail (priv->stamp == iter->stamp, NULL);
	g_return_val_if_fail (iter->user_data != NULL, NULL);

	track = iter->user_data;

	/* NOTE: there is only one single file without a name: root */
	path = brasero_video_tree_model_track_to_path (BRASERO_VIDEO_TREE_MODEL (model), track);
	return path;
}

BraseroTrack *
brasero_video_tree_model_path_to_track (BraseroVideoTreeModel *self,
				        GtkTreePath *path)
{
	BraseroVideoTreeModelPrivate *priv;
	const gint *indices;
	GSList *tracks;
	guint depth;

	priv = BRASERO_VIDEO_TREE_MODEL_PRIVATE (self);

	indices = gtk_tree_path_get_indices (path);
	depth = gtk_tree_path_get_depth (path);

	/* NOTE: it can happen that paths are depth 2 when there is DND but then
	 * only the first index is relevant. */
	if (depth > 2)
		return NULL;

	tracks = brasero_burn_session_get_tracks (BRASERO_BURN_SESSION (priv->session));
	return g_slist_nth_data (tracks, indices [0]);
}

static gboolean
brasero_video_tree_model_get_iter (GtkTreeModel *model,
				   GtkTreeIter *iter,
				   GtkTreePath *path)
{
	BraseroVideoTreeModelPrivate *priv;
	BraseroTrack *track;

	priv = BRASERO_VIDEO_TREE_MODEL_PRIVATE (model);
	track = brasero_video_tree_model_path_to_track (BRASERO_VIDEO_TREE_MODEL (model), path);
	if (!track)
		return FALSE;

	iter->user_data = track;
	iter->stamp = priv->stamp;

	return TRUE;
}

static gboolean
brasero_video_tree_model_iter_next (GtkTreeModel *model,
				    GtkTreeIter *iter)
{
	BraseroVideoTreeModelPrivate *priv;
	BraseroTrack *track;
	GSList *tracks;
	GSList *node;

	priv = BRASERO_VIDEO_TREE_MODEL_PRIVATE (model);

	/* make sure that iter comes from us */
	g_return_val_if_fail (priv->stamp == iter->stamp, FALSE);
	g_return_val_if_fail (iter->user_data != NULL, FALSE);

	tracks = brasero_burn_session_get_tracks (BRASERO_BURN_SESSION (priv->session));
	track = iter->user_data;
	if (!track)
		return FALSE;

	node = g_slist_find (tracks, track);
	if (!node || !node->next)
		return FALSE;

	iter->user_data = node->next->data;
	return TRUE;
}

static GType
brasero_video_tree_model_get_column_type (GtkTreeModel *model,
					 gint index)
{
	switch (index) {
	case BRASERO_VIDEO_TREE_MODEL_NAME:
		return G_TYPE_STRING;

	case BRASERO_VIDEO_TREE_MODEL_MIME_ICON:
		return GDK_TYPE_PIXBUF;

	case BRASERO_VIDEO_TREE_MODEL_SIZE:
		return G_TYPE_STRING;

	case BRASERO_VIDEO_TREE_MODEL_EDITABLE:
		return G_TYPE_BOOLEAN;

	case BRASERO_VIDEO_TREE_MODEL_SELECTABLE:
		return G_TYPE_BOOLEAN;

	default:
		break;
	}

	return G_TYPE_INVALID;
}

static gint
brasero_video_tree_model_get_n_columns (GtkTreeModel *model)
{
	return BRASERO_VIDEO_TREE_MODEL_COL_NUM;
}

static GtkTreeModelFlags
brasero_video_tree_model_get_flags (GtkTreeModel *model)
{
	return GTK_TREE_MODEL_LIST_ONLY;
}

static gboolean
brasero_video_tree_model_multi_row_draggable (EggTreeMultiDragSource *drag_source,
					      GList *path_list)
{
	/* All rows are draggable so return TRUE */
	return TRUE;
}

static gboolean
brasero_video_tree_model_multi_drag_data_get (EggTreeMultiDragSource *drag_source,
					      GList *path_list,
					      GtkSelectionData *selection_data)
{
	if (selection_data->target == gdk_atom_intern (BRASERO_DND_TARGET_SELF_FILE_NODES, TRUE)) {
		BraseroDNDVideoContext context;

		context.model = GTK_TREE_MODEL (drag_source);
		context.references = path_list;

		gtk_selection_data_set (selection_data,
					gdk_atom_intern_static_string (BRASERO_DND_TARGET_SELF_FILE_NODES),
					8,
					(void *) &context,
					sizeof (context));
	}
	else
		return FALSE;

	return TRUE;
}

static gboolean
brasero_video_tree_model_multi_drag_data_delete (EggTreeMultiDragSource *drag_source,
						 GList *path_list)
{
	/* NOTE: it's not the data in the selection_data here that should be
	 * deleted but rather the rows selected when there is a move. FALSE
	 * here means that we didn't delete anything. */
	/* return TRUE to stop other handlers */
	return TRUE;
}

static gboolean
brasero_video_tree_model_drag_data_received (GtkTreeDragDest *drag_dest,
					     GtkTreePath *dest_path,
					     GtkSelectionData *selection_data)
{
	BraseroTrack *track;
	BraseroTrack *sibling;
	BraseroVideoTreeModelPrivate *priv;

	priv = BRASERO_VIDEO_TREE_MODEL_PRIVATE (drag_dest);

	/* The new row(s) must be before dest_path but after our sibling */
	sibling = brasero_video_tree_model_path_to_track (BRASERO_VIDEO_TREE_MODEL (drag_dest), dest_path);
		
	/* Received data: see where it comes from:
	 * - from us, then that's a simple move
	 * - from another widget then it's going to be URIS and we add
	 *   them to VideoProject */
	if (selection_data->target == gdk_atom_intern (BRASERO_DND_TARGET_SELF_FILE_NODES, TRUE)) {
		BraseroDNDVideoContext *context;
		GList *iter;

		context = (BraseroDNDVideoContext *) selection_data->data;
		if (context->model != GTK_TREE_MODEL (drag_dest))
			return TRUE;

		/* That's us: move the row and its children. */
		for (iter = context->references; iter; iter = iter->next) {
			GtkTreeRowReference *reference;
			GtkTreePath *treepath;

			reference = iter->data;
			treepath = gtk_tree_row_reference_get_path (reference);

			track = brasero_video_tree_model_path_to_track (BRASERO_VIDEO_TREE_MODEL (drag_dest), treepath);
			gtk_tree_path_free (treepath);

			brasero_burn_session_move_track (BRASERO_BURN_SESSION (priv->session),
							 track,
							 sibling);
		}
	}
	else if (selection_data->target == gdk_atom_intern ("text/uri-list", TRUE)) {
		gint i;
		gchar **uris;
		gboolean success = FALSE;

		/* NOTE: there can be many URIs at the same time. One
		 * success is enough to return TRUE. */
		success = FALSE;
		uris = gtk_selection_data_get_uris (selection_data);
		if (!uris)
			return TRUE;

		for (i = 0; uris [i]; i ++) {
			BraseroTrackStreamCfg *track;

			/* Add the URIs to the project */
			track = brasero_track_stream_cfg_new ();
			brasero_track_stream_set_source (BRASERO_TRACK_STREAM (track), uris [i]);
			brasero_burn_session_add_track (BRASERO_BURN_SESSION (priv->session),
							BRASERO_TRACK (track),
							sibling);
		}
		g_strfreev (uris);
	}
	else
		return FALSE;

	return TRUE;
}

static gboolean
brasero_video_tree_model_row_drop_possible (GtkTreeDragDest *drag_dest,
					    GtkTreePath *dest_path,
					    GtkSelectionData *selection_data)
{
	/* It's always possible */
	return TRUE;
}

static gboolean
brasero_video_tree_model_drag_data_delete (GtkTreeDragSource *source,
					   GtkTreePath *treepath)
{
	return TRUE;
}

static void
brasero_video_tree_model_track_added (BraseroBurnSession *session,
				      BraseroTrack *track,
				      BraseroVideoTreeModel *model)
{
	BraseroVideoTreeModelPrivate *priv;
	BraseroStatus *status;
	GtkTreePath *path;
	GtkTreeIter iter;

	if (!BRASERO_IS_TRACK_STREAM (track))
		return;

	priv = BRASERO_VIDEO_TREE_MODEL_PRIVATE (model);

	iter.stamp = priv->stamp;
	iter.user_data = track;

	path = brasero_video_tree_model_track_to_path (model, track);

	/* if the file is reloading (because of a file system change or because
	 * it was a file that was a tmp folder) then no need to signal an added
	 * signal but a changed one */
	status = brasero_status_new ();
	brasero_track_get_status (track, status);
	gtk_tree_model_row_inserted (GTK_TREE_MODEL (model),
				     path,
				     &iter);
	gtk_tree_path_free (path);
	brasero_status_free (status);
}

static void
brasero_video_tree_model_track_removed (BraseroBurnSession *session,
					BraseroTrack *track,
					guint former_location,
					BraseroVideoTreeModel *model)
{
	BraseroVideoTreeModelPrivate *priv;
	GtkTreePath *path;

	if (!BRASERO_IS_TRACK_STREAM (track))
		return;

	priv = BRASERO_VIDEO_TREE_MODEL_PRIVATE (model);

	/* remove the file. */
	path = gtk_tree_path_new_from_indices (former_location, -1);
	gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
	gtk_tree_path_free (path);
}

static void
brasero_video_tree_model_track_changed (BraseroBurnSession *session,
					BraseroTrack *track,
					BraseroVideoTreeModel *model)
{
	BraseroVideoTreeModelPrivate *priv;
	GValue *value = NULL;
	GtkTreePath *path;
	GtkTreeIter iter;

	if (!BRASERO_IS_TRACK_STREAM (track))
		return;

	priv = BRASERO_VIDEO_TREE_MODEL_PRIVATE (model);

	/* scale the thumbnail */
	brasero_track_tag_lookup (BRASERO_TRACK (track),
				  BRASERO_TRACK_STREAM_THUMBNAIL_TAG,
				  &value);
	if (value) {
		GdkPixbuf *scaled;
		GdkPixbuf *snapshot;

		snapshot = g_value_get_object (value);
		scaled = gdk_pixbuf_scale_simple (snapshot,
						  48 * gdk_pixbuf_get_width (snapshot) / gdk_pixbuf_get_height (snapshot),
						  48,
						  GDK_INTERP_BILINEAR);

		value = g_new0 (GValue, 1);
		g_value_init (value, GDK_TYPE_PIXBUF);
		g_value_set_object (value, scaled);
		brasero_track_tag_add (track,
				       BRASERO_TRACK_STREAM_THUMBNAIL_TAG,
				       value);
	}

	/* Get the iter for the file */
	iter.stamp = priv->stamp;
	iter.user_data = track;

	path = brasero_video_tree_model_track_to_path (model, track);
	gtk_tree_model_row_changed (GTK_TREE_MODEL (model),
				    path,
				    &iter);
	gtk_tree_path_free (path);
}

void
brasero_video_tree_model_set_session (BraseroVideoTreeModel *model,
				      BraseroSessionCfg *session)
{
	BraseroVideoTreeModelPrivate *priv;

	priv = BRASERO_VIDEO_TREE_MODEL_PRIVATE (model);
	if (priv->session) {
		g_object_unref (priv->session);
		priv->session = NULL;
	}

	if (!session)
		return;

	priv->session = g_object_ref (session);
	g_signal_connect (session,
			  "track-added",
			  G_CALLBACK (brasero_video_tree_model_track_added),
			  model);
	g_signal_connect (session,
			  "track-removed",
			  G_CALLBACK (brasero_video_tree_model_track_removed),
			  model);
	g_signal_connect (session,
			  "track-changed",
			  G_CALLBACK (brasero_video_tree_model_track_changed),
			  model);
}

BraseroSessionCfg *
brasero_video_tree_model_get_session (BraseroVideoTreeModel *model)
{
	BraseroVideoTreeModelPrivate *priv;

	priv = BRASERO_VIDEO_TREE_MODEL_PRIVATE (model);
	return priv->session;
}

static void
brasero_video_tree_model_init (BraseroVideoTreeModel *object)
{
	BraseroVideoTreeModelPrivate *priv;

	priv = BRASERO_VIDEO_TREE_MODEL_PRIVATE (object);

	priv->theme = gtk_icon_theme_get_default ();

	do {
		priv->stamp = g_random_int ();
	} while (!priv->stamp);
}

static void
brasero_video_tree_model_finalize (GObject *object)
{
	BraseroVideoTreeModelPrivate *priv;

	priv = BRASERO_VIDEO_TREE_MODEL_PRIVATE (object);

	if (priv->theme) {
		g_object_unref (priv->theme);
		priv->theme = NULL;
	}

	G_OBJECT_CLASS (brasero_video_tree_model_parent_class)->finalize (object);
}

static void
brasero_video_tree_model_iface_init (gpointer g_iface, gpointer data)
{
	GtkTreeModelIface *iface = g_iface;
	static gboolean initialized = FALSE;

	if (initialized)
		return;

	initialized = TRUE;

	iface->get_flags = brasero_video_tree_model_get_flags;
	iface->get_n_columns = brasero_video_tree_model_get_n_columns;
	iface->get_column_type = brasero_video_tree_model_get_column_type;
	iface->get_iter = brasero_video_tree_model_get_iter;
	iface->get_path = brasero_video_tree_model_get_path;
	iface->get_value = brasero_video_tree_model_get_value;
	iface->iter_next = brasero_video_tree_model_iter_next;
	iface->iter_children = brasero_video_tree_model_iter_children;
	iface->iter_has_child = brasero_video_tree_model_iter_has_child;
	iface->iter_n_children = brasero_video_tree_model_iter_n_children;
	iface->iter_nth_child = brasero_video_tree_model_iter_nth_child;
	iface->iter_parent = brasero_video_tree_model_iter_parent;
}

static void
brasero_video_tree_model_multi_drag_source_iface_init (gpointer g_iface, gpointer data)
{
	EggTreeMultiDragSourceIface *iface = g_iface;
	static gboolean initialized = FALSE;

	if (initialized)
		return;

	initialized = TRUE;

	iface->row_draggable = brasero_video_tree_model_multi_row_draggable;
	iface->drag_data_get = brasero_video_tree_model_multi_drag_data_get;
	iface->drag_data_delete = brasero_video_tree_model_multi_drag_data_delete;
}

static void
brasero_video_tree_model_drag_source_iface_init (gpointer g_iface, gpointer data)
{
	GtkTreeDragSourceIface *iface = g_iface;
	static gboolean initialized = FALSE;

	if (initialized)
		return;

	initialized = TRUE;

	iface->drag_data_delete = brasero_video_tree_model_drag_data_delete;
}

static void
brasero_video_tree_model_drag_dest_iface_init (gpointer g_iface, gpointer data)
{
	GtkTreeDragDestIface *iface = g_iface;
	static gboolean initialized = FALSE;

	if (initialized)
		return;

	initialized = TRUE;

	iface->drag_data_received = brasero_video_tree_model_drag_data_received;
	iface->row_drop_possible = brasero_video_tree_model_row_drop_possible;
}

static void
brasero_video_tree_model_class_init (BraseroVideoTreeModelClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroVideoTreeModelPrivate));

	object_class->finalize = brasero_video_tree_model_finalize;
}

BraseroVideoTreeModel *
brasero_video_tree_model_new (void)
{
	return g_object_new (BRASERO_TYPE_VIDEO_TREE_MODEL, NULL);
}
