/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/***************************************************************************
 *            audio-disc.c
 *
 *  dim nov 27 15:34:32 2005
 *  Copyright  2005  Rouquier Philippe
 *  brasero-app@wanadoo.fr
 ***************************************************************************/

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

#include <time.h>
#include <errno.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <gdk/gdkkeysyms.h>

#include <gtk/gtk.h>

#include "brasero-misc.h"
#include "brasero-io.h"
#include "brasero-notify.h"

#include "brasero-units.h"

#include "brasero-tags.h"
#include "brasero-track-stream-cfg.h"
#include "brasero-session-cfg.h"

#include "brasero-app.h"
#include "brasero-disc.h"
#include "brasero-audio-disc.h"
#include "brasero-metadata.h"
#include "brasero-utils.h"
#include "brasero-multi-song-props.h"
#include "brasero-song-properties.h"
#include "brasero-split-dialog.h"
#include "brasero-video-tree-model.h"

#include "eggtreemultidnd.h"

static BraseroDiscResult
brasero_audio_disc_set_session_contents (BraseroDisc *disc,
					 BraseroBurnSession *session);

static BraseroDiscResult
brasero_audio_disc_add_uri (BraseroDisc *disc,
			    const char *uri);
static void
brasero_audio_disc_delete_selected (BraseroDisc *disc);


static guint
brasero_audio_disc_add_ui (BraseroDisc *disc,
			   GtkUIManager *manager,
			   GtkWidget *message);

static gboolean
brasero_audio_disc_button_pressed_cb (GtkTreeView *tree,
				      GdkEventButton *event,
				      BraseroAudioDisc *disc);
static gboolean
brasero_audio_disc_key_released_cb (GtkTreeView *tree,
				    GdkEventKey *event,
				    BraseroAudioDisc *disc);
static void
brasero_audio_disc_display_edited_cb (GtkCellRendererText *cellrenderertext,
				      gchar *path_string,
				      gchar *text,
				      BraseroAudioDisc *disc);
static void
brasero_audio_disc_display_editing_started_cb (GtkCellRenderer *renderer,
					       GtkCellEditable *editable,
					       gchar *path,
					       BraseroAudioDisc *disc);
static void
brasero_audio_disc_display_editing_canceled_cb (GtkCellRenderer *renderer,
						 BraseroAudioDisc *disc);

static void
brasero_audio_disc_edit_information_cb (GtkAction *action,
					BraseroAudioDisc *disc);
static void
brasero_audio_disc_open_activated_cb (GtkAction *action,
				      BraseroAudioDisc *disc);
static void
brasero_audio_disc_delete_activated_cb (GtkAction *action,
					BraseroDisc *disc);
static void
brasero_audio_disc_paste_activated_cb (GtkAction *action,
				       BraseroAudioDisc *disc);
static gboolean
brasero_audio_disc_get_selected_uri (BraseroDisc *disc,
				     gchar **uri);
static gboolean
brasero_audio_disc_get_boundaries (BraseroDisc *disc,
				   gint64 *start,
				   gint64 *end);

static void
brasero_audio_disc_add_pause_cb (GtkAction *action, BraseroAudioDisc *disc);
static void
brasero_audio_disc_split_cb (GtkAction *action, BraseroAudioDisc *disc);

static void
brasero_audio_disc_selection_changed (GtkTreeSelection *selection, BraseroAudioDisc *disc);

struct _BraseroAudioDiscPrivate {
	BraseroIOJobBase *add_dir;
	BraseroIOJobBase *add_playlist;

	GtkWidget *tree;

	GtkWidget *message;
	GtkUIManager *manager;
	GtkActionGroup *disc_group;

	GtkTreePath *selected_path;

       	GdkDragContext *drag_context;

	/* only used at start time when loading a project */
	guint loading;

	guint editing:1;
	guint dragging:1;
	guint reject_files:1;
};

static GtkActionEntry entries[] = {
	{"ContextualMenu", NULL, N_("Menu")},
	{"OpenSong", GTK_STOCK_OPEN, NULL, NULL, N_("Open the selected files"),
	 G_CALLBACK (brasero_audio_disc_open_activated_cb)},
	{"EditSong", GTK_STOCK_PROPERTIES, N_("_Edit Information…"), NULL, N_("Edit the track information (start, end, author, etc.)"),
	 G_CALLBACK (brasero_audio_disc_edit_information_cb)},
	{"DeleteAudio", GTK_STOCK_REMOVE, NULL, NULL, N_("Remove the selected files from the project"),
	 G_CALLBACK (brasero_audio_disc_delete_activated_cb)},
	{"PasteAudio", NULL, N_("Paste files"), NULL, N_("Add the files stored in the clipboard"),
	 G_CALLBACK (brasero_audio_disc_paste_activated_cb)},
	{"Pause", "insert-pause", N_("I_nsert a Pause"), NULL, N_("Add a 2 second pause after the track"),
	 G_CALLBACK (brasero_audio_disc_add_pause_cb)},
	{"Split", "transform-crop-and-resize", N_("_Split Track…"), NULL, N_("Split the selected track"),
	 G_CALLBACK (brasero_audio_disc_split_cb)}
};

static const gchar *description = {
	"<ui>"
	"<menubar name='menubar' >"
		"<menu action='EditMenu'>"
		"<placeholder name='EditPlaceholder'>"
			"<menuitem action='Pause'/>"
			"<menuitem action='Split'/>"
		"</placeholder>"
		"</menu>"
	"</menubar>"
	"<popup action='ContextMenu'>"
		"<menuitem action='OpenSong'/>"
		"<menuitem action='DeleteAudio'/>"
		"<separator/>"
		"<menuitem action='PasteAudio'/>"
		"<separator/>"
		"<menuitem action='Pause'/>"
		"<menuitem action='Split'/>"
		"<separator/>"
		"<menuitem action='EditSong'/>"
	"</popup>"
	"<toolbar name='Toolbar'>"
		"<placeholder name='DiscButtonPlaceholder'>"
			"<separator/>"
			"<toolitem action='Pause'/>"
			"<toolitem action='Split'/>"
		"</placeholder>"
	"</toolbar>"
	"</ui>"
};

enum {
	TREE_MODEL_ROW = 150,
	TARGET_URIS_LIST,
};

static GtkTargetEntry ntables_cd [] = {
	{"GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, TREE_MODEL_ROW},
	{"text/uri-list", 0, TARGET_URIS_LIST}
};
static guint nb_targets_cd = sizeof (ntables_cd) / sizeof (ntables_cd[0]);

static GtkTargetEntry ntables_source [] = {
	{"GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, TREE_MODEL_ROW},
};
static guint nb_targets_source = sizeof (ntables_source) / sizeof (ntables_source[0]);

static GObjectClass *parent_class = NULL;

enum {
	PROP_NONE,
	PROP_REJECT_FILE,
};

#define COL_KEY "column_key"

#define BRASERO_AUDIO_DISC_CONTEXT		1000

static void brasero_audio_disc_iface_disc_init (BraseroDiscIface *iface);

G_DEFINE_TYPE_WITH_CODE (BraseroAudioDisc,
			 brasero_audio_disc,
			 GTK_TYPE_BOX,
			 G_IMPLEMENT_INTERFACE (BRASERO_TYPE_DISC,
					        brasero_audio_disc_iface_disc_init));
static gboolean
brasero_audio_disc_is_empty (BraseroDisc *disc)
{
	GtkTreeModel *model;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (BRASERO_AUDIO_DISC (disc)->priv->tree));
	if (!model)
		return FALSE;

	return gtk_tree_model_iter_n_children (model, NULL) != 0;
}

static void
brasero_audio_disc_iface_disc_init (BraseroDiscIface *iface)
{
	iface->add_uri = brasero_audio_disc_add_uri;
	iface->delete_selected = brasero_audio_disc_delete_selected;

	iface->is_empty = brasero_audio_disc_is_empty;

	iface->set_session_contents = brasero_audio_disc_set_session_contents;
	iface->get_selected_uri = brasero_audio_disc_get_selected_uri;
	iface->get_boundaries = brasero_audio_disc_get_boundaries;
	iface->add_ui = brasero_audio_disc_add_ui;
}

static void
brasero_audio_disc_get_property (GObject * object,
				 guint prop_id,
				 GValue * value,
				 GParamSpec * pspec)
{
	BraseroAudioDisc *disc;

	disc = BRASERO_AUDIO_DISC (object);

	switch (prop_id) {
	case PROP_REJECT_FILE:
		g_value_set_boolean (value, disc->priv->reject_files);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
brasero_audio_disc_set_property (GObject * object,
				 guint prop_id,
				 const GValue * value,
				 GParamSpec * pspec)
{
	BraseroAudioDisc *disc;

	disc = BRASERO_AUDIO_DISC (object);

	switch (prop_id) {
	case PROP_REJECT_FILE:
		disc->priv->reject_files = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static guint
brasero_audio_disc_add_ui (BraseroDisc *disc,
			   GtkUIManager *manager,
			   GtkWidget *message)
{
	BraseroAudioDisc *audio_disc;
	GError *error = NULL;
	GtkAction *action;
	guint merge_id;

	audio_disc = BRASERO_AUDIO_DISC (disc);

	if (audio_disc->priv->message) {
		g_object_unref (audio_disc->priv->message);
		audio_disc->priv->message = NULL;
	}

	audio_disc->priv->message = g_object_ref (message);

	if (!audio_disc->priv->disc_group) {
		audio_disc->priv->disc_group = gtk_action_group_new (BRASERO_DISC_ACTION "-audio");
		gtk_action_group_set_translation_domain (audio_disc->priv->disc_group, GETTEXT_PACKAGE);
		gtk_action_group_add_actions (audio_disc->priv->disc_group,
					      entries,
					      G_N_ELEMENTS (entries),
					      disc);
		gtk_ui_manager_insert_action_group (manager,
						    audio_disc->priv->disc_group,
						    0);
	}

	merge_id = gtk_ui_manager_add_ui_from_string (manager,
						      description,
						      -1,
						      &error);
	if (!merge_id) {
		g_error_free (error);
		return 0;
	}

	action = gtk_action_group_get_action (audio_disc->priv->disc_group, "Pause");
	g_object_set (action,
		      "short-label", _("Pause"), /* for toolbar buttons */
		      NULL);
	gtk_action_set_sensitive (action, FALSE);

	action = gtk_action_group_get_action (audio_disc->priv->disc_group, "Split");
	g_object_set (action,
		      "short-label", _("Split"), /* for toolbar buttons */
		      NULL);
	gtk_action_set_sensitive (action, FALSE);

	audio_disc->priv->manager = manager;
	g_object_ref (manager);

	return merge_id;
}

static gboolean
brasero_audio_disc_selection_function (GtkTreeSelection *selection,
				       GtkTreeModel *model,
				       GtkTreePath *treepath,
				       gboolean is_selected,
				       gpointer NULL_data)
{
/*	BraseroTrack *track;

	track = brasero_video_tree_model_path_to_track (BRASERO_VIDEO_TREE_MODEL (model), treepath);
	if (track)
		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				    BRASERO_VIDEO_TREE_MODEL_EDITABLE, (is_selected == FALSE),
				    -1);
*/
	return TRUE;
}

static void
brasero_audio_disc_init (BraseroAudioDisc *obj)
{
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkWidget *scroll;

	obj->priv = g_new0 (BraseroAudioDiscPrivate, 1);
	gtk_box_set_spacing (GTK_BOX (obj), 0);
	gtk_orientable_set_orientation (GTK_ORIENTABLE (obj), GTK_ORIENTATION_VERTICAL);

	/* Tree */
	obj->priv->tree = gtk_tree_view_new ();
	gtk_tree_view_set_rubber_banding (GTK_TREE_VIEW (obj->priv->tree), TRUE);

	g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (obj->priv->tree)),
			  "changed",
			  G_CALLBACK (brasero_audio_disc_selection_changed),
			  obj);

	/* This must be before connecting to button press event */
	egg_tree_multi_drag_add_drag_support (GTK_TREE_VIEW (obj->priv->tree));

	gtk_widget_show (obj->priv->tree);
	g_signal_connect (G_OBJECT (obj->priv->tree),
			  "button-press-event",
			  G_CALLBACK (brasero_audio_disc_button_pressed_cb),
			  obj);
	g_signal_connect (G_OBJECT (obj->priv->tree),
			  "key-release-event",
			  G_CALLBACK (brasero_audio_disc_key_released_cb),
			  obj);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (obj->priv->tree));
	gtk_tree_selection_set_mode (selection,GTK_SELECTION_MULTIPLE);
	gtk_tree_selection_set_select_function (selection, brasero_audio_disc_selection_function, NULL, NULL);
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (obj->priv->tree), TRUE);

	/* Track num column */
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Track"), renderer,
							   "text", BRASERO_VIDEO_TREE_MODEL_INDEX,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (obj->priv->tree), column);
	gtk_tree_view_column_set_resizable (column, TRUE);

	/* Other columns */
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_expand (column, TRUE);

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "icon-name", BRASERO_VIDEO_TREE_MODEL_ICON_NAME);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set_data (G_OBJECT (renderer), COL_KEY, BRASERO_TRACK_STREAM_TITLE_TAG);
	g_object_set (G_OBJECT (renderer),
		      "mode", GTK_CELL_RENDERER_MODE_EDITABLE,
		      "ellipsize-set", TRUE,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      NULL);

	g_signal_connect (G_OBJECT (renderer), "edited",
			  G_CALLBACK (brasero_audio_disc_display_edited_cb), obj);
	g_signal_connect (G_OBJECT (renderer), "editing-started",
			  G_CALLBACK (brasero_audio_disc_display_editing_started_cb), obj);
	g_signal_connect (G_OBJECT (renderer), "editing-canceled",
			  G_CALLBACK (brasero_audio_disc_display_editing_canceled_cb), obj);

	gtk_tree_view_column_pack_end (column, renderer, TRUE);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "text", BRASERO_VIDEO_TREE_MODEL_NAME);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "style", BRASERO_VIDEO_TREE_MODEL_STYLE);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "weight", BRASERO_VIDEO_TREE_MODEL_WEIGHT);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "editable", BRASERO_VIDEO_TREE_MODEL_EDITABLE);
	gtk_tree_view_column_set_title (column, _("Title"));
	g_object_set (G_OBJECT (column),
		      "spacing", 4,
		      NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (obj->priv->tree), column);
	gtk_tree_view_set_expander_column (GTK_TREE_VIEW (obj->priv->tree), column);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set_data (G_OBJECT (renderer), COL_KEY, BRASERO_TRACK_STREAM_ARTIST_TAG);
	g_object_set (G_OBJECT (renderer),
		      /* "editable", TRUE, disable this for the time being it doesn't play well with DND and double click */
		      /* "mode", GTK_CELL_RENDERER_MODE_EDITABLE,*/
		      "ellipsize-set", TRUE,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      NULL);

	g_signal_connect (G_OBJECT (renderer), "edited",
			  G_CALLBACK (brasero_audio_disc_display_edited_cb), obj);
	g_signal_connect (G_OBJECT (renderer), "editing-started",
			  G_CALLBACK (brasero_audio_disc_display_editing_started_cb), obj);
	g_signal_connect (G_OBJECT (renderer), "editing-canceled",
			  G_CALLBACK (brasero_audio_disc_display_editing_canceled_cb), obj);
	column = gtk_tree_view_column_new_with_attributes (_("Artist"), renderer,
							   "text", BRASERO_VIDEO_TREE_MODEL_ARTIST,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (obj->priv->tree), column);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_min_width (column, 200);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Length"), renderer,
							   "text", BRASERO_VIDEO_TREE_MODEL_SIZE,
							  /* "background", BACKGROUND_COL, */
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (obj->priv->tree), column);
	gtk_tree_view_column_set_resizable (column, TRUE);

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scroll);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll),
					     GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scroll), obj->priv->tree);

	gtk_box_pack_start (GTK_BOX (obj), scroll, TRUE, TRUE, 0);

	/* dnd */
	gtk_tree_view_enable_model_drag_dest (GTK_TREE_VIEW (obj->priv->tree),
					      ntables_cd,
					      nb_targets_cd,
					      GDK_ACTION_COPY|
					      GDK_ACTION_MOVE);

	gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (obj->priv->tree),
						GDK_BUTTON1_MASK,
						ntables_source,
						nb_targets_source,
						GDK_ACTION_COPY |
						GDK_ACTION_MOVE);
}

static void
brasero_audio_disc_reset_real (BraseroAudioDisc *disc)
{
	brasero_io_cancel_by_base (disc->priv->add_dir);
	brasero_io_cancel_by_base (disc->priv->add_playlist);

	if (disc->priv->selected_path) {
		gtk_tree_path_free (disc->priv->selected_path);
		disc->priv->selected_path = NULL;
	}

	if (disc->priv->message)
		brasero_notify_message_remove (disc->priv->message, BRASERO_AUDIO_DISC_CONTEXT);
}

static void
brasero_audio_disc_finalize (GObject *object)
{
	BraseroAudioDisc *cobj;
	cobj = BRASERO_AUDIO_DISC(object);
	
	brasero_audio_disc_reset_real (cobj);

	brasero_io_job_base_free (cobj->priv->add_dir);
	brasero_io_job_base_free (cobj->priv->add_playlist);
	cobj->priv->add_dir = NULL;
	cobj->priv->add_playlist = NULL;

	if (cobj->priv->message) {
		g_object_unref (cobj->priv->message);
		cobj->priv->message = NULL;
	}

	if (cobj->priv->manager) {
		g_object_unref (cobj->priv->manager);
		cobj->priv->manager = NULL;
	}

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

/******************************** utility functions ****************************/

static void
brasero_audio_disc_add_gap (BraseroAudioDisc *disc,
			    GtkTreePath *treepath,
			    gint64 gap)
{
	GtkTreeModel *model;
	BraseroTrack *track;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (disc->priv->tree));
	track = brasero_video_tree_model_path_to_track (BRASERO_VIDEO_TREE_MODEL (model), treepath);
	brasero_track_stream_set_boundaries (BRASERO_TRACK_STREAM (track),
					     -1,
					     -1,
					     gap);
}

static void
brasero_audio_disc_short_track_dialog (BraseroAudioDisc *disc)
{
	brasero_app_alert (brasero_app_get_default (),
			   _("The track will be padded at its end."),
			   _("The track is shorter than 6 seconds"),
			   GTK_MESSAGE_WARNING);
}

static BraseroDiscResult
brasero_audio_disc_add_uri_real (BraseroAudioDisc *disc,
				 const gchar *uri,
				 gint pos,
				 gint64 gap_sectors,
				 gint64 start,
				 gint64 end,
				 GtkTreePath **path_return)
{
	BraseroTrackStreamCfg *track;
	BraseroTrack *sibling = NULL;
	BraseroSessionCfg *session;
	GtkTreeModel *store;

	g_return_val_if_fail (uri != NULL, BRASERO_DISC_ERROR_UNKNOWN);

	if (disc->priv->reject_files)
		return BRASERO_DISC_NOT_READY;

	store = gtk_tree_view_get_model (GTK_TREE_VIEW (disc->priv->tree));
	session = brasero_video_tree_model_get_session (BRASERO_VIDEO_TREE_MODEL (store));

	track = brasero_track_stream_cfg_new ();
	brasero_track_stream_set_source (BRASERO_TRACK_STREAM (track), uri);
	brasero_track_stream_set_boundaries (BRASERO_TRACK_STREAM (track),
					     start,
					     end,
					     BRASERO_SECTORS_TO_DURATION (gap_sectors));

	session = brasero_video_tree_model_get_session (BRASERO_VIDEO_TREE_MODEL (store));
	if (pos > 0) {
		GSList *tracks;

		tracks = brasero_burn_session_get_tracks (BRASERO_BURN_SESSION (session));
		sibling = g_slist_nth_data (tracks, pos - 1);
	}

	brasero_burn_session_add_track (BRASERO_BURN_SESSION (session), BRASERO_TRACK (track), sibling);
	if (path_return)
		*path_return = brasero_video_tree_model_track_to_path (BRASERO_VIDEO_TREE_MODEL (store), BRASERO_TRACK (track));

	return BRASERO_DISC_OK;
}

/*************** shared code for dir/playlist addition *************************/
static void
brasero_audio_disc_file_type_error_dialog (BraseroAudioDisc *disc,
					   const gchar *uri)
{
	gchar *primary;
	gchar *name;

    	BRASERO_GET_BASENAME_FOR_DISPLAY (uri, name);
	primary = g_strdup_printf (_("\"%s\" could not be handled by GStreamer."), name);
	brasero_app_alert (brasero_app_get_default (),
			   primary,
			   _("Make sure the appropriate codec is installed"),
			   GTK_MESSAGE_ERROR);
	g_free (primary);
	g_free (name);
}

static gboolean
brasero_audio_disc_video_file_dialog (BraseroAudioDisc *disc,
				      const gchar *uri)
{
	GtkWidget *dialog;
	GtkResponseType answer;
	gchar *string;
	gchar *name;

    	BRASERO_GET_BASENAME_FOR_DISPLAY (uri, name);
	string = g_strdup_printf (_("Do you want to add \"%s\", which is a video file?"), name);
	dialog = brasero_app_dialog (brasero_app_get_default (),
				     string,
				     GTK_BUTTONS_NONE,
				     GTK_MESSAGE_QUESTION);
	g_free (string);
	g_free (name);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  _("This file is a video and therefore only the audio part can be written to the disc."));

	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       _("_Discard File"),
			       GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       _("_Add File"),
			       GTK_RESPONSE_OK);

	answer = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	return (answer != GTK_RESPONSE_OK);
}

static void
brasero_audio_disc_result (GObject *obj,
			   GError *error,
			   const gchar *uri,
			   GFileInfo *info,
			   gpointer null_data)
{
	BraseroAudioDisc *disc = BRASERO_AUDIO_DISC (obj);

	if (error)
		return;

	/* we silently ignore the title and any error */
	if (g_file_info_get_attribute_string (info, BRASERO_IO_PLAYLIST_TITLE))
		return;

	if (!g_file_info_get_attribute_boolean (info, BRASERO_IO_HAS_AUDIO))
		return;

	if (g_file_info_get_attribute_boolean (info, BRASERO_IO_HAS_VIDEO)
	&& !brasero_audio_disc_video_file_dialog (disc, uri))
		return;

	brasero_audio_disc_add_uri_real (disc,
					 uri,
					 -1,
					 -1,
					 -1,
					 -1,
					 NULL);
}

/*********************** directories exploration *******************************/
static BraseroDiscResult
brasero_audio_disc_add_directory_contents (BraseroAudioDisc *disc,
					   const gchar *uri)
{
	if (!disc->priv->add_dir)
		disc->priv->add_dir = brasero_io_register (G_OBJECT (disc),
							   brasero_audio_disc_result,
							   NULL,
							   NULL);

	/* we have to pass a dummy value here otherwise finished is never called */
	brasero_io_load_directory (uri,
				   disc->priv->add_dir,
				   BRASERO_IO_INFO_MIME|
				   BRASERO_IO_INFO_PERM|
				   BRASERO_IO_INFO_METADATA|
				   BRASERO_IO_INFO_METADATA_MISSING_CODEC|
				   BRASERO_IO_INFO_RECURSIVE,
				   disc);
	return BRASERO_DISC_OK;
}

static gboolean
brasero_audio_disc_add_dir (BraseroAudioDisc *disc, const gchar *uri)
{
	gint answer;
	GtkWidget *dialog;

	dialog = brasero_app_dialog (brasero_app_get_default (),
				     _("Do you want to search for audio files inside the directory?"),
				     GTK_BUTTONS_NONE,
				     GTK_MESSAGE_WARNING);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  "%s.",
						  _("Directories cannot be added to video or audio discs"));

	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				_("Search _Directory"), GTK_RESPONSE_OK,
				NULL);

	gtk_widget_show_all (dialog);
	answer = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	if (answer == GTK_RESPONSE_OK)
		return TRUE;

	return FALSE;
}

/************************** playlist parsing ***********************************/

#if BUILD_PLAYLIST

static BraseroDiscResult
brasero_audio_disc_add_playlist (BraseroAudioDisc *disc,
				 const gchar *uri)
{
	if (!disc->priv->add_playlist)
		disc->priv->add_playlist = brasero_io_register (G_OBJECT (disc),
								brasero_audio_disc_result,
								NULL,
								NULL);

	brasero_io_parse_playlist (uri,
				   disc->priv->add_playlist,
				   BRASERO_IO_INFO_PERM|
				   BRASERO_IO_INFO_MIME|
				   BRASERO_IO_INFO_METADATA|
				   BRASERO_IO_INFO_METADATA_MISSING_CODEC,
				   disc); /* It's necessary to have a callback_data != from NULL */

	return BRASERO_DISC_OK;
}

#endif

/**************************** New Row ******************************************/
static void
brasero_audio_disc_unreadable_dialog (BraseroAudioDisc *disc,
				      const gchar *uri,
				      GError *error)
{
	gchar *primary;
	gchar *name;
	GFile *file;

	file = g_file_new_for_uri (uri);
	name = g_file_get_basename (file);
	g_object_unref (file);

	primary = g_strdup_printf (_("\"%s\" could not be opened."), name);
	brasero_app_alert (brasero_app_get_default (),
			   primary,
			   error->message,
			   GTK_MESSAGE_ERROR);
	g_free (primary);
	g_free (name);
}

static void
brasero_audio_disc_wav_dts_response_cb (GtkButton *button,
                                        GtkResponseType response,
                                        BraseroAudioDisc *disc)
{
	BraseroSessionCfg *session;
	GtkTreeModel *model;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (disc->priv->tree));
	session = brasero_video_tree_model_get_session (BRASERO_VIDEO_TREE_MODEL (model));

	if (response == GTK_RESPONSE_OK)
		brasero_burn_session_tag_add_int (BRASERO_BURN_SESSION (session),
		                                  BRASERO_SESSION_STREAM_AUDIO_FORMAT,
		                                  BRASERO_AUDIO_FORMAT_DTS);
}

static void
brasero_audio_disc_wav_dts_file_dialog (BraseroAudioDisc *disc)
{
	GtkWidget *message;
	BraseroSessionCfg *session;
	GtkTreeModel *model;

	if (brasero_notify_get_message_by_context_id (disc->priv->message,
	                                              BRASERO_AUDIO_DISC_CONTEXT))
		return;

	/* Add a tag (RAW by default) so that we won't try to display this message again */
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (disc->priv->tree));
	session = brasero_video_tree_model_get_session (BRASERO_VIDEO_TREE_MODEL (model));
	brasero_burn_session_tag_add_int (BRASERO_BURN_SESSION (session),
	                                  BRASERO_SESSION_STREAM_AUDIO_FORMAT,
	                                  BRASERO_AUDIO_FORMAT_RAW);

	message = brasero_notify_message_add (disc->priv->message,
					      _("Do you want to create an audio CD with DTS tracks?"),
					      _("Some of the selected songs are suitable for creating DTS tracks."
					        "\nThis type of audio CD track provides a higher quality of sound but can only be played by specific digital players."
					        "\nNote: if you agree, normalization will not be applied to these tracks."),
					      0,
					      BRASERO_AUDIO_DISC_CONTEXT);

	gtk_info_bar_set_message_type (GTK_INFO_BAR (message), GTK_MESSAGE_INFO);

	gtk_widget_set_tooltip_text (gtk_info_bar_add_button (GTK_INFO_BAR (message),
							    						  _("Create _Regular Tracks"),
							    						  GTK_RESPONSE_NO),
					     	     _("Click here to burn all songs as regular tracks"));

	gtk_widget_set_tooltip_text (gtk_info_bar_add_button (GTK_INFO_BAR (message),
							    						  _("Create _DTS Tracks"),
							    						  GTK_RESPONSE_OK),
					     	     _("Click here to burn all suitable songs as DTS tracks"));

	g_signal_connect (BRASERO_DISC_MESSAGE (message),
			  "response",
			  G_CALLBACK (brasero_audio_disc_wav_dts_response_cb),
			  disc);
}

static void
brasero_audio_disc_session_changed (BraseroSessionCfg *session,
				    BraseroAudioDisc *self)
{
	GSList *next;
	GSList *tracks;
	BraseroStatus *status;
	gboolean should_use_dts;

	if (!gtk_widget_get_window (GTK_WIDGET (self)))
		return;

	/* make sure all tracks have video */
	should_use_dts = FALSE;
	status = brasero_status_new ();
	tracks = brasero_burn_session_get_tracks (BRASERO_BURN_SESSION (session));
	for (; tracks; tracks = next) {
		BraseroStreamFormat format;
		BraseroTrackStream *track;
		BraseroBurnResult result;

		track = tracks->data;
		next = tracks->next;

		if (!BRASERO_IS_TRACK_STREAM (track))
			continue;

		result = brasero_track_get_status (BRASERO_TRACK (track), status);
		if (result == BRASERO_BURN_ERR) {
			GError *error;
			gboolean res;
			gchar *uri;

			uri = brasero_track_stream_get_source (track, TRUE);

			/* Remove the track now otherwise on each session change we'll get the
			 * same message over and over again. */
			brasero_burn_session_remove_track (BRASERO_BURN_SESSION (session),
							   BRASERO_TRACK (track));

			error = brasero_status_get_error (status);
			if (!error)
				brasero_audio_disc_file_type_error_dialog (self, uri);
			else if (error->code == BRASERO_BURN_ERROR_FILE_FOLDER) {
				res = brasero_audio_disc_add_dir (self, uri);
				if (res)
					brasero_audio_disc_add_directory_contents (self, uri);
			}

#if BUILD_PLAYLIST
			else if (error->code == BRASERO_BURN_ERROR_FILE_PLAYLIST) {
				/* This is a supported playlist */
				brasero_audio_disc_add_playlist (self, uri);
			}
#endif

			else if (error->code == BRASERO_BURN_ERROR_FILE_NOT_FOUND) {
				/* It could be a file that was deleted */
				brasero_audio_disc_file_type_error_dialog (self, uri);
			}
			else
				brasero_audio_disc_unreadable_dialog (self,
								      uri,
								      error);

			g_error_free (error);
			g_free (uri);
			continue;
		}

		if (result == BRASERO_BURN_NOT_READY || result == BRASERO_BURN_RUNNING)
			continue;

		if (result != BRASERO_BURN_OK)
			continue;

		format = brasero_track_stream_get_format (track);
		if (!BRASERO_STREAM_FORMAT_AUDIO (format)) {
			gchar *uri;

			uri = brasero_track_stream_get_source (track, TRUE);
			brasero_audio_disc_file_type_error_dialog (self, uri);
			brasero_burn_session_remove_track (BRASERO_BURN_SESSION (session),
							   BRASERO_TRACK (track));
			g_free (uri);
			continue;
		}

		if ((format & BRASERO_AUDIO_FORMAT_DTS) != 0)
			should_use_dts = TRUE;

		if (BRASERO_STREAM_FORMAT_HAS_VIDEO (format)) {
			gboolean res;
			gchar *uri;

			uri = brasero_track_stream_get_source (track, TRUE);
			res = brasero_audio_disc_video_file_dialog (self, uri);
			if (res)
				brasero_burn_session_remove_track (BRASERO_BURN_SESSION (session),
								   BRASERO_TRACK (track));
			g_free (uri);
		}
	}
	g_object_unref (status);

	if (should_use_dts
	&&  brasero_burn_session_tag_lookup (BRASERO_BURN_SESSION (session), BRASERO_SESSION_STREAM_AUDIO_FORMAT, NULL) != BRASERO_BURN_OK)
		brasero_audio_disc_wav_dts_file_dialog (self);
}

static BraseroDiscResult
brasero_audio_disc_add_uri (BraseroDisc *disc,
			    const gchar *uri)
{
	GtkTreePath *treepath = NULL;
	BraseroAudioDisc *audio_disc;
	BraseroDiscResult result;

	audio_disc = BRASERO_AUDIO_DISC (disc);
	result = brasero_audio_disc_add_uri_real (audio_disc,
						  uri,
						  -1,
						  0,
						  -1,
						  -1,
						  &treepath);

	if (treepath) {
		gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (audio_disc->priv->tree),
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
brasero_audio_disc_remove (BraseroAudioDisc *disc,
			   GtkTreePath *treepath)
{
	BraseroSessionCfg *session;
	GtkTreeModel *model;
	BraseroTrack *track;
	GtkTreeIter iter;
	gboolean is_gap;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (disc->priv->tree));
	gtk_tree_model_get_iter (model, &iter, treepath);
	gtk_tree_model_get (model, &iter,
			    BRASERO_VIDEO_TREE_MODEL_IS_GAP, &is_gap,
			    -1);

	session = brasero_video_tree_model_get_session (BRASERO_VIDEO_TREE_MODEL (model));
	track = brasero_video_tree_model_path_to_track (BRASERO_VIDEO_TREE_MODEL (model), treepath);

	if (is_gap)
		brasero_track_stream_set_boundaries (BRASERO_TRACK_STREAM (track),
						     -1,
						     -1,
						     0);
	else
		brasero_burn_session_remove_track (BRASERO_BURN_SESSION (session), track);
}

static void
brasero_audio_disc_delete_selected (BraseroDisc *disc)
{
	GtkTreeSelection *selection;
	BraseroAudioDisc *audio;
	GtkTreePath *treepath;
	GtkTreeModel *model;
	GList *list;

	audio = BRASERO_AUDIO_DISC (disc);
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (audio->priv->tree));

	/* we must start by the end for the treepaths to point to valid rows */
	list = gtk_tree_selection_get_selected_rows (selection, &model);
	list = g_list_reverse (list);
	for (; list; list = g_list_remove (list, treepath)) {
		treepath = list->data;
		brasero_audio_disc_remove (audio, treepath);
		gtk_tree_path_free (treepath);
	}

	/* warn that the selection changed (there are no more selected paths) */
	if (audio->priv->selected_path) {
		gtk_tree_path_free (audio->priv->selected_path);
		audio->priv->selected_path = NULL;
	}
	brasero_disc_selection_changed (disc);
}

static BraseroDiscResult
brasero_audio_disc_set_session_contents (BraseroDisc *disc,
					 BraseroBurnSession *session)
{
	BraseroAudioDisc *audio;
	GtkTreeModel *current_model;
	BraseroVideoTreeModel *model;

	audio = BRASERO_AUDIO_DISC (disc);

	if (audio->priv->add_dir)
		brasero_io_cancel_by_base (audio->priv->add_dir);

	if (audio->priv->add_playlist)
		brasero_io_cancel_by_base (audio->priv->add_playlist);

	/* disconnect some signals */
	current_model = gtk_tree_view_get_model (GTK_TREE_VIEW (audio->priv->tree));
	if (current_model) {
		BraseroSessionCfg *current_session;

		current_session = brasero_video_tree_model_get_session (BRASERO_VIDEO_TREE_MODEL (current_model));
		if (current_session)
			g_signal_handlers_disconnect_by_func (current_session,
							      brasero_audio_disc_session_changed,
							      disc);
	}

	if (!session) {
		gtk_tree_view_set_model (GTK_TREE_VIEW (audio->priv->tree), NULL);
		return BRASERO_DISC_OK;
	}

	model = brasero_video_tree_model_new ();
	brasero_video_tree_model_set_session (model, BRASERO_SESSION_CFG (session));
	gtk_tree_view_set_model (GTK_TREE_VIEW (audio->priv->tree),
				 GTK_TREE_MODEL (model));
	g_object_unref (model);

	g_signal_connect (session,
			  "is-valid",
			  G_CALLBACK (brasero_audio_disc_session_changed),
			  disc);

	return BRASERO_DISC_OK;
}

/********************************** Cell Editing *******************************/
static void
brasero_audio_disc_display_edited_cb (GtkCellRendererText *renderer,
				      gchar *path_string,
				      gchar *text,
				      BraseroAudioDisc *disc)
{
	const gchar *tag;
	GtkTreeModel *model;
	BraseroTrack *track;
	GtkTreePath *treepath;

	tag = g_object_get_data (G_OBJECT (renderer), COL_KEY);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (disc->priv->tree));
	treepath = gtk_tree_path_new_from_string (path_string);
	track = brasero_video_tree_model_path_to_track (BRASERO_VIDEO_TREE_MODEL (model), treepath);
	brasero_track_tag_add_string (BRASERO_TRACK (track),
				      tag,
				      text);
	disc->priv->editing = 0;
}

static void
brasero_audio_disc_display_editing_started_cb (GtkCellRenderer *renderer,
					       GtkCellEditable *editable,
					       gchar *path,
					       BraseroAudioDisc *disc)
{
	disc->priv->editing = 1;
}

static void
brasero_audio_disc_display_editing_canceled_cb (GtkCellRenderer *renderer,
						BraseroAudioDisc *disc)
{
	disc->priv->editing = 0;
}

/********************************** Pause **************************************/
static void
brasero_audio_disc_add_pause (BraseroAudioDisc *disc)
{
	GtkTreeSelection *selection;
	GtkTreePath *treepath;
	GtkTreeModel *model;
	GList *references = NULL;
	GList *selected;
	GList *iter;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (disc->priv->tree));
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (disc->priv->tree));
	selected = gtk_tree_selection_get_selected_rows (selection, &model);

	/* since we are going to modify the model, we need to convert all these
	 * into row references */
	for (iter = selected; iter; iter = iter->next) {
		GtkTreeRowReference *ref;

		treepath = iter->data;
		ref = gtk_tree_row_reference_new (model, treepath);
		references = g_list_append (references, ref);
	}

	g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (selected);

	for (iter = references; iter; iter = iter->next) {
		GtkTreeRowReference *ref;

		ref = iter->data;
		treepath = gtk_tree_row_reference_get_path (ref);
		gtk_tree_row_reference_free (ref);

		brasero_audio_disc_add_gap (disc, treepath, 2 * GST_SECOND);
		gtk_tree_path_free (treepath);
	}

	g_list_free (references);
}

static void
brasero_audio_disc_add_pause_cb (GtkAction *action,
				 BraseroAudioDisc *disc)
{
	brasero_audio_disc_add_pause (disc);
}

static void
brasero_audio_disc_add_slices (BraseroAudioDisc *disc,
			       GtkTreePath *treepath,
			       GSList *slices)
{
	BraseroSessionCfg *session;
	BraseroStreamFormat format;
	BraseroAudioSlice *slice;
	BraseroTrack *track;
	GtkTreeModel *model;
	GSList *iter;
	gchar *uri;

	if (!slices)
		return;

	/* the first slice is used for the existing row */
	slice = slices->data;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (disc->priv->tree));
	session = brasero_video_tree_model_get_session (BRASERO_VIDEO_TREE_MODEL (model));
	track = brasero_video_tree_model_path_to_track (BRASERO_VIDEO_TREE_MODEL (model), treepath);

	brasero_track_stream_set_boundaries (BRASERO_TRACK_STREAM (track),
					     slice->start,
					     slice->end,
					     -1);

	uri = brasero_track_stream_get_source (BRASERO_TRACK_STREAM (track), TRUE);
	format = brasero_track_stream_get_format (BRASERO_TRACK_STREAM (track));

	for (iter = slices->next; iter; iter = iter->next) {
		BraseroTrackStream *new_track;

		slice = iter->data;

		new_track = brasero_track_stream_new ();
		brasero_track_stream_set_source (new_track, uri);
		brasero_track_stream_set_format (new_track, format);
		brasero_track_stream_set_boundaries (new_track,
						     slice->start,
						     slice->end,
						     brasero_track_stream_get_gap (BRASERO_TRACK_STREAM (track)));
		brasero_track_tag_copy_missing (BRASERO_TRACK (new_track),
						BRASERO_TRACK (track));
		brasero_burn_session_add_track (BRASERO_BURN_SESSION (session),
						BRASERO_TRACK (new_track),
						BRASERO_TRACK (track));
	}

	g_free (uri);
}

static void
brasero_audio_disc_split (BraseroAudioDisc *disc)
{
	GtkTreeSelection *selection;
	GtkTreePath *treepath;
	GtkTreeModel *model;
	BraseroTrack *track;

	GtkResponseType response;
	GtkWidget *toplevel;
	GtkWidget *dialog;
	GList *selected;
	GSList *slices;
	gchar *uri;

	/* get the URIs */
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (disc->priv->tree));
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (disc->priv->tree));
	selected = gtk_tree_selection_get_selected_rows (selection, &model);

	/* don't check g_slist_length == 0 since then the button is greyed */
	if (g_list_length (selected) > 1) {
		brasero_app_alert (brasero_app_get_default (),
				   _("Select one song only please."),
				   _("Impossible to split more than one song at a time"),
				   GTK_MESSAGE_ERROR);

		g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
		g_list_free (selected);
		return;
	}

	treepath = selected->data;
	g_list_free (selected);

	/* NOTE: this is necessarily a song since otherwise button is grey */
	track = brasero_video_tree_model_path_to_track (BRASERO_VIDEO_TREE_MODEL (model), treepath);

	dialog = brasero_split_dialog_new ();
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (disc));
	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
	gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_position (GTK_WINDOW (toplevel), GTK_WIN_POS_CENTER_ON_PARENT);

	uri = brasero_track_stream_get_source (BRASERO_TRACK_STREAM (track), TRUE);
	brasero_split_dialog_set_uri (BRASERO_SPLIT_DIALOG (dialog),
	                              uri,
	                              brasero_track_tag_lookup_string (track, BRASERO_TRACK_STREAM_TITLE_TAG),
	                              brasero_track_tag_lookup_string (track, BRASERO_TRACK_STREAM_ARTIST_TAG));
	g_free (uri);

	brasero_split_dialog_set_boundaries (BRASERO_SPLIT_DIALOG (dialog),
					     brasero_track_stream_get_start (BRASERO_TRACK_STREAM (track)),
					     brasero_track_stream_get_end (BRASERO_TRACK_STREAM (track)));

	response = gtk_dialog_run (GTK_DIALOG (dialog));
	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy (dialog);
		gtk_tree_path_free (treepath);
		return;
	}

	slices = brasero_split_dialog_get_slices (BRASERO_SPLIT_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	brasero_audio_disc_add_slices (disc, treepath, slices);
	g_slist_foreach (slices, (GFunc) g_free, NULL);
	g_slist_free (slices);
	gtk_tree_path_free (treepath);
}

static void
brasero_audio_disc_split_cb (GtkAction *action,
			     BraseroAudioDisc *disc)
{
	brasero_audio_disc_split (disc);
}

static void
brasero_audio_disc_selection_changed (GtkTreeSelection *selection,
				      BraseroAudioDisc *disc)
{
	GtkAction *action_delete;
	GtkAction *action_pause;
	GtkAction *action_split;
	GtkAction *action_edit;
	GtkAction *action_open;
	guint selected_num = 0;
	GtkTreeModel *model;
	GList *selected;
	GList *iter;

	selected = gtk_tree_selection_get_selected_rows (selection, &model);

	if (disc->priv->selected_path)
		gtk_tree_path_free (disc->priv->selected_path);

	if (selected)
		disc->priv->selected_path = gtk_tree_path_copy (selected->data);
	else
		disc->priv->selected_path = NULL;

	brasero_disc_selection_changed (BRASERO_DISC (disc));

	if (!disc->priv->disc_group)
		return;

	action_delete = gtk_action_group_get_action (disc->priv->disc_group, "DeleteAudio");
	action_open = gtk_action_group_get_action (disc->priv->disc_group, "OpenSong");
	action_edit = gtk_action_group_get_action (disc->priv->disc_group, "EditSong");
	action_split = gtk_action_group_get_action (disc->priv->disc_group, "Split");
	action_pause = gtk_action_group_get_action (disc->priv->disc_group, "Pause");

	gtk_action_set_sensitive (action_split, FALSE);
	gtk_action_set_sensitive (action_pause, FALSE);
	gtk_action_set_sensitive (action_edit, FALSE);
	gtk_action_set_sensitive (action_open, FALSE);

	if (selected)
		gtk_action_set_sensitive (action_delete, TRUE);
	else
		gtk_action_set_sensitive (action_delete, FALSE);

	for (iter = selected; iter; iter = iter->next) {
		GtkTreeIter row;
		GtkTreePath *treepath;

		treepath = iter->data;
		if (gtk_tree_model_get_iter (model, &row, treepath)) {
			gboolean is_gap;

			gtk_tree_model_get (model, &row,
					    BRASERO_VIDEO_TREE_MODEL_IS_GAP, &is_gap,
					    -1);
			if (!is_gap) {
				selected_num ++;

				gtk_action_set_sensitive (action_open, TRUE);
				gtk_action_set_sensitive (action_edit, TRUE);
				gtk_action_set_sensitive (action_pause, TRUE);
				if (selected_num != 1) {
					gtk_action_set_sensitive (action_split, FALSE);
					break;
				}
				else
					gtk_action_set_sensitive (action_split, TRUE);
			}
		}
	}

	g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (selected);
}

/********************************** Menus **************************************/
static void
brasero_audio_disc_open_file (BraseroAudioDisc *disc)
{
	char *uri;
	GList *item, *list;
	GSList *uris = NULL;
	GtkTreeModel *model;
	GtkTreePath *treepath;
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (disc->priv->tree));
	list = gtk_tree_selection_get_selected_rows (selection, &model);

	for (item = list; item; item = item->next) {
                BraseroTrack *track;

		treepath = item->data;
                track = brasero_video_tree_model_path_to_track (BRASERO_VIDEO_TREE_MODEL (model), treepath);
		gtk_tree_path_free (treepath);

		if (!track)
			continue;

		uri = brasero_track_stream_get_source (BRASERO_TRACK_STREAM (track), TRUE);

		if (uri)
			uris = g_slist_prepend (uris, uri);
	}
	g_list_free (list);

	brasero_utils_launch_app (GTK_WIDGET (disc), uris);
	g_slist_foreach (uris, (GFunc) g_free, NULL);
	g_slist_free (uris);
}

static gboolean
brasero_audio_disc_rename_songs (GtkTreeModel *model,
				 GtkTreeIter *iter,
				 GtkTreePath *treepath,
				 const gchar *old_name,
				 const gchar *new_name)
{
	BraseroTrack *track;

	track = brasero_video_tree_model_path_to_track (BRASERO_VIDEO_TREE_MODEL (model), treepath);
	brasero_track_tag_add_string (track,
				      BRASERO_TRACK_STREAM_TITLE_TAG,
				      new_name);

	/* Update the view */
	brasero_track_changed (track);
	return TRUE;
}

static void
brasero_audio_disc_edit_multi_song_properties (BraseroAudioDisc *disc,
					       GList *list)
{
	gint64 gap;
	GList *copy;
	GList *item;
	GtkWidget *props;
	GtkWidget *toplevel;
	GtkTreeModel *model;
	gchar *artist = NULL;
	GtkResponseType result;
	gchar *composer = NULL;
	gchar *isrc = NULL;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (disc->priv->tree));
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (disc));

	props = brasero_multi_song_props_new ();
	gtk_window_set_transient_for (GTK_WINDOW (props),
				      GTK_WINDOW (toplevel));
	gtk_window_set_modal (GTK_WINDOW (props), TRUE);
	gtk_window_set_position (GTK_WINDOW (props),
				 GTK_WIN_POS_CENTER_ON_PARENT);

	gtk_widget_show (GTK_WIDGET (props));
	result = gtk_dialog_run (GTK_DIALOG (props));
	gtk_widget_hide (GTK_WIDGET (props));
	if (result != GTK_RESPONSE_ACCEPT) {
		gtk_widget_destroy (props);
		return;
	}

	brasero_multi_song_props_set_rename_callback (BRASERO_MULTI_SONG_PROPS (props),
						      gtk_tree_view_get_selection (GTK_TREE_VIEW (disc->priv->tree)),
						      BRASERO_VIDEO_TREE_MODEL_NAME,
						      brasero_audio_disc_rename_songs);

	brasero_multi_song_props_get_properties (BRASERO_MULTI_SONG_PROPS (props),
						 &artist,
						 &composer,
						 &isrc,
						 &gap);

	/* start by the end in case we add silences since then the next
	 * treepaths will be wrong */
	copy = g_list_copy (list);
	copy = g_list_reverse (copy);
	for (item = copy; item; item = item->next) {
		GtkTreePath *treepath;
		BraseroTrack *track;
		GtkTreeIter iter;
		gboolean is_gap;

		treepath = item->data;
                gtk_tree_model_get_iter (model, &iter, treepath);
		gtk_tree_model_get (model, &iter,
				    BRASERO_VIDEO_TREE_MODEL_IS_GAP, &is_gap,
				    -1);

		if (is_gap)
			continue;

                track = brasero_video_tree_model_path_to_track (BRASERO_VIDEO_TREE_MODEL (model), treepath);
		if (!track)
			continue;

		if (artist)
                        brasero_track_tag_add_string (BRASERO_TRACK (track),
                                                      BRASERO_TRACK_STREAM_ARTIST_TAG,
                                                      artist);

		if (composer)
                        brasero_track_tag_add_string (BRASERO_TRACK (track),
                                                      BRASERO_TRACK_STREAM_COMPOSER_TAG,
                                                      composer);

		if (isrc)
                        brasero_track_tag_add_string (BRASERO_TRACK (track),
                                                      BRASERO_TRACK_STREAM_ISRC_TAG,
                                                      isrc);

                if (gap > -1)
                        brasero_track_stream_set_boundaries (BRASERO_TRACK_STREAM (track),
                                                                                       -1,
                                                                                       -1,
                                                                                       gap);
	}
	g_list_free (copy);
	g_free (artist);
	g_free (composer);
	g_free (isrc);

	gtk_widget_destroy (props);
}

static void
brasero_audio_disc_edit_single_song_properties (BraseroAudioDisc *disc,
						GtkTreePath *treepath)
{
	gint64 gap;
	gint64 end;
	gint64 start;
	guint track_num;
	GtkWidget *props;
	GtkWidget *toplevel;
	GtkTreeModel *model;
	BraseroTrack *track;
	GtkResponseType result;
	guint64 length;
	gchar *title;
	gchar *artist;
	gchar *composer;
	gchar *isrc;
	GtkTreeIter iter;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (disc->priv->tree));
        track = brasero_video_tree_model_path_to_track (BRASERO_VIDEO_TREE_MODEL (model), treepath);
	if (!track)
		return;

        /* information about the track */
        gtk_tree_model_get_iter (model, &iter, treepath);
        gtk_tree_model_get (model, &iter,
                            BRASERO_VIDEO_TREE_MODEL_INDEX_NUM, &track_num,
                            -1);
        brasero_track_stream_get_length (BRASERO_TRACK_STREAM (track), &length);

	/* set up dialog */
	props = brasero_song_props_new ();
	brasero_song_props_set_properties (BRASERO_SONG_PROPS (props),
					   track_num,
					   brasero_track_tag_lookup_string (BRASERO_TRACK (track), BRASERO_TRACK_STREAM_ARTIST_TAG),
					   brasero_track_tag_lookup_string (BRASERO_TRACK (track), BRASERO_TRACK_STREAM_TITLE_TAG),
					   brasero_track_tag_lookup_string (BRASERO_TRACK (track), BRASERO_TRACK_STREAM_COMPOSER_TAG),
					   brasero_track_tag_lookup_string (BRASERO_TRACK (track), BRASERO_TRACK_STREAM_ISRC_TAG),
					   length,
					   brasero_track_stream_get_start (BRASERO_TRACK_STREAM (track)),
					   brasero_track_stream_get_end (BRASERO_TRACK_STREAM (track)),
					   brasero_track_stream_get_gap (BRASERO_TRACK_STREAM (track)));

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (disc));
	gtk_window_set_transient_for (GTK_WINDOW (props),
				      GTK_WINDOW (toplevel));
	gtk_window_set_modal (GTK_WINDOW (props), TRUE);
	gtk_window_set_position (GTK_WINDOW (props),
				 GTK_WIN_POS_CENTER_ON_PARENT);

	gtk_widget_show (GTK_WIDGET (props));
	result = gtk_dialog_run (GTK_DIALOG (props));
	gtk_widget_hide (GTK_WIDGET (props));
	if (result != GTK_RESPONSE_ACCEPT) {
		gtk_widget_destroy (props);
		return;
	}

	brasero_song_props_get_properties (BRASERO_SONG_PROPS (props),
					   &artist,
					   &title,
					   &composer,
					   &isrc,
					   &start,
					   &end,
					   &gap);

	brasero_track_stream_set_boundaries (BRASERO_TRACK_STREAM (track),
                                             start,
                                             end,
                                             gap);

	if (title)
		brasero_track_tag_add_string (BRASERO_TRACK (track),
					      BRASERO_TRACK_STREAM_TITLE_TAG,
					      title);

	if (artist)
		brasero_track_tag_add_string (BRASERO_TRACK (track),
					      BRASERO_TRACK_STREAM_ARTIST_TAG,
					      artist);

	if (composer)
		brasero_track_tag_add_string (BRASERO_TRACK (track),
					      BRASERO_TRACK_STREAM_COMPOSER_TAG,
					      composer);

	if (isrc)
		brasero_track_tag_add_string (BRASERO_TRACK (track),
					      BRASERO_TRACK_STREAM_ISRC_TAG,
					      isrc);

	if (end - start + BRASERO_SECTORS_TO_DURATION (gap) < BRASERO_MIN_STREAM_LENGTH)
		brasero_audio_disc_short_track_dialog (disc);

	g_free (title);
	g_free (artist);
	g_free (composer);
	g_free (isrc);
	gtk_widget_destroy (props);
}

static void
brasero_audio_disc_edit_song_properties (BraseroAudioDisc *disc,
					 GList *list)
{
	GList *item;
	gint song_num;
	GtkTreeModel *model;
	GList *real_list = NULL;

	if (!g_list_length (list))
		return;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (disc->priv->tree));

	/* count the number of selected songs */
	song_num = 0;
	for (item = list; item; item = item->next) {
		GtkTreePath *tmp;
		gboolean is_gap;
		GtkTreeIter iter;

		tmp = item->data;

		gtk_tree_model_get_iter (model, &iter, tmp);
		gtk_tree_model_get (model, &iter, 
				    BRASERO_VIDEO_TREE_MODEL_IS_GAP, &is_gap,
				    -1);

		if (!is_gap) {
			song_num ++;
			real_list = g_list_prepend (real_list, tmp);
		}
	}

	if (!song_num)
		return;

	if (song_num == 1)
		brasero_audio_disc_edit_single_song_properties (disc, real_list->data);
	else
		brasero_audio_disc_edit_multi_song_properties (disc, real_list);

	g_list_free (real_list);
}

static void
brasero_audio_disc_open_activated_cb (GtkAction *action,
				      BraseroAudioDisc *disc)
{
	brasero_audio_disc_open_file (disc);
}

static void
brasero_audio_disc_edit_information_cb (GtkAction *action,
					BraseroAudioDisc *disc)
{
	GList *list;
	GtkTreeModel *model;
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (disc->priv->tree));
	list = gtk_tree_selection_get_selected_rows (selection, &model);

	brasero_audio_disc_edit_song_properties (disc, list);

	g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (list);
}

static void
brasero_audio_disc_delete_activated_cb (GtkAction *action,
					BraseroDisc *disc)
{
	brasero_audio_disc_delete_selected (disc);
}

static void
brasero_audio_disc_clipboard_text_cb (GtkClipboard *clipboard,
				      const gchar *text,
				      BraseroAudioDisc *disc)
{
	gchar **array;
	gchar **item;

	if (!text)
		return;

	array = g_uri_list_extract_uris (text);
	item = array;
	while (*item) {
		if (**item != '\0') {
			GFile *file;
			gchar *uri;

			file = g_file_new_for_commandline_arg (*item);
			uri = g_file_get_uri (file);
			g_object_unref (file);

			brasero_audio_disc_add_uri_real (disc,
							 uri,
							 -1,
							 0,
							 -1,
							 -1,
							 NULL);
			g_free (uri);
		}

		item++;
	}
	g_strfreev (array);
}

static void
brasero_audio_disc_clipboard_targets_cb (GtkClipboard *clipboard,
					 GdkAtom *atoms,
					 gint n_atoms,
					 BraseroAudioDisc *disc)
{
	if (brasero_clipboard_selection_may_have_uri (atoms, n_atoms))
		gtk_clipboard_request_text (clipboard,
					    (GtkClipboardTextReceivedFunc) brasero_audio_disc_clipboard_text_cb,
					    disc);
}

static void
brasero_audio_disc_paste_activated_cb (GtkAction *action,
				       BraseroAudioDisc *disc)
{
	GtkClipboard *clipboard;

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_request_targets (clipboard,
				       (GtkClipboardTargetsReceivedFunc) brasero_audio_disc_clipboard_targets_cb,
				       disc);
}

static gboolean
brasero_audio_disc_button_pressed_cb (GtkTreeView *tree,
				      GdkEventButton *event,
				      BraseroAudioDisc *disc)
{
	GtkWidgetClass *widget_class;

	/* Avoid minding signals that happen out of the tree area (like in the 
	 * headers for example) */
	if (event->window != gtk_tree_view_get_bin_window (GTK_TREE_VIEW (tree)))
		return FALSE;

	widget_class = GTK_WIDGET_GET_CLASS (tree);

	/* Check that the click happened in the main window with rows. */
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
		if (!path || !gtk_tree_selection_path_is_selected (selection, path)) {
			/* Don't update the selection if the right click was on one of
			 * the already selected rows */
			widget_class->button_press_event (GTK_WIDGET (tree), event);

			if (!path) {
				GtkTreeSelection *selection;

				/* This is to deselect any row when selecting a 
				 * row that cannot be selected or in an empty
				 * part */
				selection = gtk_tree_view_get_selection (tree);
				gtk_tree_selection_unselect_all (selection);
			}
		}

		widget = gtk_ui_manager_get_widget (disc->priv->manager, "/ContextMenu/PasteAudio");
		if (widget) {
			if (gtk_clipboard_wait_is_text_available (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD)))
				gtk_widget_set_sensitive (widget, TRUE);
			else
				gtk_widget_set_sensitive (widget, FALSE);
		}

		widget = gtk_ui_manager_get_widget (disc->priv->manager,"/ContextMenu");
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

		result = gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (disc->priv->tree),
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

		if (disc->priv->selected_path)
			gtk_tree_path_free (disc->priv->selected_path);

		disc->priv->selected_path = treepath;
		brasero_disc_selection_changed (BRASERO_DISC (disc));

		if (event->type == GDK_2BUTTON_PRESS) {
			GList *list;

			list = g_list_prepend (NULL, treepath);
			brasero_audio_disc_edit_song_properties (disc, list);
			g_list_free (list);
		}
	}

	return TRUE;
}

/********************************** key press event ****************************/
static void
brasero_audio_disc_rename_activated (BraseroAudioDisc *disc)
{
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkTreePath *treepath;
	GtkTreeModel *model;
	GList *list;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (disc->priv->tree));
	list = gtk_tree_selection_get_selected_rows (selection, &model);

	for (; list; list = g_list_remove (list, treepath)) {
		treepath = list->data;

		gtk_widget_grab_focus (disc->priv->tree);
		column = gtk_tree_view_get_column (GTK_TREE_VIEW (disc->priv->tree), 0);
		gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (disc->priv->tree),
					      treepath,
					      NULL,
					      TRUE,
					      0.5,
					      0.5);
		gtk_tree_view_set_cursor (GTK_TREE_VIEW (disc->priv->tree),
					  treepath,
					  column,
					  TRUE);

		gtk_tree_path_free (treepath);
	}
}

static gboolean
brasero_audio_disc_key_released_cb (GtkTreeView *tree,
				    GdkEventKey *event,
				    BraseroAudioDisc *disc)
{
	if (disc->priv->editing)
		return FALSE;

	if (event->keyval == GDK_KEY_KP_Delete || event->keyval == GDK_KEY_Delete) {
		brasero_audio_disc_delete_selected (BRASERO_DISC (disc));
	}
	else if (event->keyval == GDK_KEY_F2)
		brasero_audio_disc_rename_activated (disc);

	return FALSE;
}

/**********************************               ******************************/
static gboolean
brasero_audio_disc_get_selected_uri (BraseroDisc *disc,
				     gchar **uri)
{
	BraseroTrack *track;
	GtkTreeModel *model;
	BraseroAudioDisc *audio;

	audio = BRASERO_AUDIO_DISC (disc);
	if (!audio->priv->selected_path)
		return FALSE;

	if (!uri)
		return TRUE;

	/* we are asked for just one uri so return the first one */
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (audio->priv->tree));
        track = brasero_video_tree_model_path_to_track (BRASERO_VIDEO_TREE_MODEL (model),
                                                                                       audio->priv->selected_path);
        if (!track) {
		gtk_tree_path_free (audio->priv->selected_path);
		audio->priv->selected_path = NULL;
		return FALSE;
	}

	*uri = brasero_track_stream_get_source (BRASERO_TRACK_STREAM (track), TRUE);
	return TRUE;
}

static gboolean
brasero_audio_disc_get_boundaries (BraseroDisc *disc,
				   gint64 *start,
				   gint64 *end)
{
	BraseroTrack *track;
	GtkTreeModel *model;
	BraseroAudioDisc *audio;

	audio = BRASERO_AUDIO_DISC (disc);
	if (!audio->priv->selected_path)
		return FALSE;

	/* we are asked for just one uri so return the first one */
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (audio->priv->tree));
        track = brasero_video_tree_model_path_to_track (BRASERO_VIDEO_TREE_MODEL (model),
                                                                                       audio->priv->selected_path);
        if (!track) {
		gtk_tree_path_free (audio->priv->selected_path);
		audio->priv->selected_path = NULL;
		return FALSE;
	}

        *start = brasero_track_stream_get_start (BRASERO_TRACK_STREAM (track));
        *end = brasero_track_stream_get_end (BRASERO_TRACK_STREAM (track));
	return TRUE;
}

static void
brasero_audio_disc_class_init (BraseroAudioDiscClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_audio_disc_finalize;
	object_class->set_property = brasero_audio_disc_set_property;
	object_class->get_property = brasero_audio_disc_get_property;

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
brasero_audio_disc_new ()
{
	BraseroAudioDisc *obj;
	
	obj = BRASERO_AUDIO_DISC (g_object_new (BRASERO_TYPE_AUDIO_DISC, NULL));
	
	return GTK_WIDGET (obj);
}

