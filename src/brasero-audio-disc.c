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

#include "burn-basics.h"
#include "burn-debug.h"
#include "brasero-app.h"
#include "brasero-disc.h"
#include "brasero-audio-disc.h"
#include "brasero-metadata.h"
#include "brasero-utils.h"
#include "brasero-multi-song-props.h"
#include "brasero-song-properties.h"
#include "brasero-io.h"
#include "brasero-split-dialog.h"
#include "brasero-session-cfg.h"
#include "eggtreemultidnd.h"

#include "brasero-tags.h"

#ifdef BUILD_INOTIFY

#include "sys/inotify.h"

#endif

static void brasero_audio_disc_class_init (BraseroAudioDiscClass *klass);
static void brasero_audio_disc_init (BraseroAudioDisc *sp);
static void brasero_audio_disc_finalize (GObject *object);
static void brasero_audio_disc_iface_disc_init (BraseroDiscIface *iface);

static void brasero_audio_disc_get_property (GObject * object,
					     guint prop_id,
					     GValue * value,
					     GParamSpec * pspec);
static void brasero_audio_disc_set_property (GObject * object,
					     guint prop_id,
					     const GValue * value,
					     GParamSpec * spec);

static BraseroDiscResult
brasero_audio_disc_get_status (BraseroDisc *disc,
			       gint *remaining,
			       gchar **current_task);

static BraseroDiscResult
brasero_audio_disc_get_track (BraseroDisc *disc,
			      BraseroDiscTrack *track);
static BraseroDiscResult
brasero_audio_disc_load_track (BraseroDisc *disc,
			       BraseroDiscTrack *track);

static BraseroDiscResult
brasero_audio_disc_set_session_contents (BraseroDisc *disc,
					 BraseroBurnSession *session);

static BraseroDiscResult
brasero_audio_disc_add_uri (BraseroDisc *disc,
			    const char *uri);
static void
brasero_audio_disc_delete_selected (BraseroDisc *disc);
static void
brasero_audio_disc_clear (BraseroDisc *disc);
static void
brasero_audio_disc_reset (BraseroDisc *disc);

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
brasero_audio_disc_drag_data_received_cb (GtkTreeView *tree,
					  GdkDragContext *drag_context,
					  gint x,
					  gint y,
					  GtkSelectionData *selection_data,
					  guint info,
					  guint time,
					  BraseroAudioDisc *disc);
static gboolean
brasero_audio_disc_drag_drop_cb (GtkWidget *widget,
				 GdkDragContext*drag_context,
				 gint x,
				 gint y,
				 guint time,
				 BraseroAudioDisc *disc);
static gboolean
brasero_audio_disc_drag_motion_cb (GtkWidget *tree,
				   GdkDragContext *drag_context,
				   gint x,
				   gint y,
				   guint time,
				   BraseroAudioDisc *disc);
static void
brasero_audio_disc_drag_leave_cb (GtkWidget *tree,
				  GdkDragContext *drag_context,
				  guint time,
				  BraseroAudioDisc *disc);

static void
brasero_audio_disc_drag_begin_cb (GtkWidget *widget,
				  GdkDragContext *drag_context,
				  BraseroAudioDisc *disc);
static void
brasero_audio_disc_drag_end_cb (GtkWidget *tree,
			        GdkDragContext *drag_context,
			        BraseroAudioDisc *disc);

static void
brasero_audio_disc_row_deleted_cb (GtkTreeModel *model,
				   GtkTreePath *arg1,
				   BraseroAudioDisc *disc);
static void
brasero_audio_disc_row_inserted_cb (GtkTreeModel *model,
				    GtkTreePath *arg1,
				    GtkTreeIter *arg2,
				    BraseroAudioDisc *disc);
static void
brasero_audio_disc_row_changed_cb (GtkTreeModel *model,
				   GtkTreePath *path,
				   GtkTreeIter *iter,
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
static void
brasero_audio_disc_decrease_activity_counter (BraseroAudioDisc *disc);

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

#ifdef BUILD_INOTIFY

typedef struct {
	gchar *uri;
	guint32 cookie;
	gint id;
} BraseroInotifyMovedData;

static gboolean
brasero_audio_disc_inotify_monitor_cb (GIOChannel *channel,
				       GIOCondition condition,
				       BraseroAudioDisc *disc);
static void
brasero_audio_disc_start_monitoring (BraseroAudioDisc *disc,
				     const char *uri);
static void
brasero_audio_disc_cancel_monitoring (BraseroAudioDisc *disc,
				      const char *uri);
#endif

struct _BraseroAudioDiscPrivate {
	BraseroIO *io;
	BraseroIOJobBase *attr_changed;
	BraseroIOJobBase *add_dir;
	BraseroIOJobBase *add_uri;
	BraseroIOJobBase *add_playlist;

	GtkWidget *notebook;
	GtkWidget *tree;

	GtkUIManager *manager;
	GtkActionGroup *disc_group;

	GtkTreePath *selected_path;

#ifdef BUILD_INOTIFY

	BraseroIOJobBase *reload_uri;

	int notify_id;
	GIOChannel *notify;
	GHashTable *monitored;

	GSList *moved_list;

#endif

	gint64 sectors;

       	GdkDragContext *drag_context;

	gint activity_counter;

	/* only used at start time when loading a project */
	guint loading;

	guint editing:1;
	guint dragging:1;
	guint reject_files:1;
};

enum {
	TRACK_NUM_COL,
	ICON_COL,
	NAME_COL,
	SIZE_COL,
	ARTIST_COL,
	URI_COL,
	START_COL,
	END_COL,
	LENGTH_COL,
	COMPOSER_COL,
	ISRC_COL,
	BACKGROUND_COL,
	SONG_COL,
	EDITABLE_COL,
	LENGTH_SET_COL, /* if start/end were set through scanning or not */
	TITLE_SET_COL,
	ARTIST_SET_COL,
	COMPOSER_SET_COL,
	ISRC_SET_COL,
	NB_COL
};

static GtkActionEntry entries[] = {
	{"ContextualMenu", NULL, N_("Menu")},
	{"OpenSong", GTK_STOCK_OPEN, NULL, NULL, N_("Open the selected files"),
	 G_CALLBACK (brasero_audio_disc_open_activated_cb)},
	{"EditSong", GTK_STOCK_PROPERTIES, N_("_Edit Information..."), NULL, N_("Edit the track information (start, end, author, ...)"),
	 G_CALLBACK (brasero_audio_disc_edit_information_cb)},
	{"DeleteAudio", GTK_STOCK_REMOVE, NULL, NULL, N_("Remove the selected files from the project"),
	 G_CALLBACK (brasero_audio_disc_delete_activated_cb)},
	{"PasteAudio", GTK_STOCK_PASTE, NULL, NULL, N_("Add the files stored in the clipboard"),
	 G_CALLBACK (brasero_audio_disc_paste_activated_cb)},
	{"Pause", "insert-pause", N_("I_nsert a Pause"), NULL, N_("Add a 2 second pause after the track"),
	 G_CALLBACK (brasero_audio_disc_add_pause_cb)},
	{"Split", "transform-crop-and-resize", N_("_Split Track..."), NULL, N_("Split the selected track"),
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

/* 1 sec = 75 sectors, len is in nanosecond */
#define BRASERO_SECTORS_TO_TIME(sectors)	(gint64) (sectors * GST_SECOND / 75)
#define COL_KEY "column_key"
#define COL_KEY_SET "column_key_set"

GType
brasero_audio_disc_get_type()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroAudioDiscClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_audio_disc_class_init,
			NULL,
			NULL,
			sizeof (BraseroAudioDisc),
			0,
			(GInstanceInitFunc) brasero_audio_disc_init,
		};

		static const GInterfaceInfo disc_info =
		{
			(GInterfaceInitFunc) brasero_audio_disc_iface_disc_init,
			NULL,
			NULL
		};

		type = g_type_register_static(GTK_TYPE_VBOX, 
					      "BraseroAudioDisc",
					      &our_info,
					      0);

		g_type_add_interface_static (type,
					     BRASERO_TYPE_DISC,
					     &disc_info);
	}

	return type;
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

static void
brasero_audio_disc_iface_disc_init (BraseroDiscIface *iface)
{
	iface->add_uri = brasero_audio_disc_add_uri;
	iface->delete_selected = brasero_audio_disc_delete_selected;
	iface->clear = brasero_audio_disc_clear;
	iface->reset = brasero_audio_disc_reset;
	iface->get_track = brasero_audio_disc_get_track;
	iface->set_session_contents = brasero_audio_disc_set_session_contents;
	iface->load_track = brasero_audio_disc_load_track;
	iface->get_status = brasero_audio_disc_get_status;
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

	if (!audio_disc->priv->disc_group) {
		audio_disc->priv->disc_group = gtk_action_group_new (BRASERO_DISC_ACTION);
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
		BRASERO_BURN_LOG ("Adding ui elements failed: %s", error->message);
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
	GtkTreeIter iter;
	gboolean is_song = FALSE;

	gtk_tree_model_get_iter (model, &iter, treepath);
	gtk_tree_model_get (model, &iter,
			    SONG_COL, &is_song,
			    -1);

	if (is_song)
		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				    EDITABLE_COL, (is_selected == FALSE),
				    -1);
	return TRUE;
}

static void
brasero_audio_disc_init (BraseroAudioDisc *obj)
{
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeModel *model;
	GtkWidget *scroll;

	obj->priv = g_new0 (BraseroAudioDiscPrivate, 1);
	gtk_box_set_spacing (GTK_BOX (obj), 0);

	/* notebook to display information about how to use the tree */
	obj->priv->notebook = brasero_disc_get_use_info_notebook ();
	gtk_box_pack_start (GTK_BOX (obj), obj->priv->notebook, TRUE, TRUE, 0);

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

	model = (GtkTreeModel*) gtk_list_store_new (NB_COL,
						    G_TYPE_STRING,
						    G_TYPE_STRING,
						    G_TYPE_STRING,
						    G_TYPE_STRING,
						    G_TYPE_STRING,
						    G_TYPE_STRING,
						    G_TYPE_INT64,
						    G_TYPE_INT64,
						    G_TYPE_INT64, 
						    G_TYPE_STRING,
						    G_TYPE_INT,
						    G_TYPE_STRING,
						    G_TYPE_BOOLEAN,
						    G_TYPE_BOOLEAN,
						    G_TYPE_BOOLEAN,
						    G_TYPE_BOOLEAN,
						    G_TYPE_BOOLEAN,
						    G_TYPE_BOOLEAN,
						    G_TYPE_BOOLEAN);

	g_signal_connect (G_OBJECT (model),
			  "row-deleted",
			  G_CALLBACK (brasero_audio_disc_row_deleted_cb),
			  obj);
	g_signal_connect (G_OBJECT (model),
			  "row-inserted",
			  G_CALLBACK (brasero_audio_disc_row_inserted_cb),
			  obj);
	g_signal_connect (G_OBJECT (model),
			  "row-changed",
			  G_CALLBACK (brasero_audio_disc_row_changed_cb),
			  obj);

	gtk_tree_view_set_model (GTK_TREE_VIEW (obj->priv->tree),
				 GTK_TREE_MODEL (model));
	g_object_unref (model);

	/* Track num column */
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Track"), renderer,
							   "text", TRACK_NUM_COL,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (obj->priv->tree), column);
	gtk_tree_view_column_set_resizable (column, FALSE);

	/* Other columns */
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_min_width (column, 200);

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "icon-name", ICON_COL);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set_data (G_OBJECT (renderer), COL_KEY, GINT_TO_POINTER (NAME_COL));
	g_object_set_data (G_OBJECT (renderer), COL_KEY_SET, GINT_TO_POINTER (TITLE_SET_COL));
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
					    "markup", NAME_COL);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "background", BACKGROUND_COL);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "editable", EDITABLE_COL);
	gtk_tree_view_column_set_title (column, _("Title"));
	g_object_set (G_OBJECT (column),
		      "expand", TRUE,
		      "spacing", 4,
		      NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (obj->priv->tree),
				     column);

	gtk_tree_view_set_expander_column (GTK_TREE_VIEW (obj->priv->tree),
					   column);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set_data (G_OBJECT (renderer), COL_KEY, GINT_TO_POINTER (ARTIST_COL));
	g_object_set_data (G_OBJECT (renderer), COL_KEY_SET, GINT_TO_POINTER (ARTIST_SET_COL));
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
							   "text", ARTIST_COL,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (obj->priv->tree),
				     column);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_min_width (column, 200);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Length"), renderer,
							   "text", SIZE_COL,
							   "background", BACKGROUND_COL,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (obj->priv->tree),
				     column);
	gtk_tree_view_column_set_resizable (column, FALSE);

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scroll);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll),
					     GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scroll), obj->priv->tree);

	gtk_notebook_append_page (GTK_NOTEBOOK (obj->priv->notebook),
				  scroll,
				  NULL);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (obj->priv->notebook), 0);

	/* dnd */
	gtk_tree_view_enable_model_drag_dest (GTK_TREE_VIEW (obj->priv->tree),
					      ntables_cd,
					      nb_targets_cd,
					      GDK_ACTION_COPY|
					      GDK_ACTION_MOVE);

	g_signal_connect (G_OBJECT (obj->priv->tree),
			  "drag-data-received",
			  G_CALLBACK (brasero_audio_disc_drag_data_received_cb),
			  obj);
	g_signal_connect (G_OBJECT (obj->priv->tree),
			  "drag-drop",
			  G_CALLBACK (brasero_audio_disc_drag_drop_cb),
			  obj);
	g_signal_connect (G_OBJECT (obj->priv->tree),
			  "drag_motion",
			  G_CALLBACK (brasero_audio_disc_drag_motion_cb),
			  obj);
	g_signal_connect (G_OBJECT (obj->priv->tree),
			  "drag_leave",
			  G_CALLBACK (brasero_audio_disc_drag_leave_cb),
			  obj);

	g_signal_connect (G_OBJECT (obj->priv->tree),
			  "drag-begin",
			  G_CALLBACK (brasero_audio_disc_drag_begin_cb),
			  obj);
	g_signal_connect (G_OBJECT (obj->priv->tree),
			  "drag_end",
			  G_CALLBACK (brasero_audio_disc_drag_end_cb),
			  obj);

	gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (obj->priv->tree),
						GDK_BUTTON1_MASK,
						ntables_source,
						nb_targets_source,
						GDK_ACTION_COPY |
						GDK_ACTION_MOVE);

#ifdef BUILD_INOTIFY
	int fd;

	fd = inotify_init ();
	if (fd != -1) {
		obj->priv->notify = g_io_channel_unix_new (fd);

		g_io_channel_set_encoding (obj->priv->notify, NULL, NULL);
		g_io_channel_set_close_on_unref (obj->priv->notify, TRUE);
		obj->priv->notify_id = g_io_add_watch (obj->priv->notify,
						       G_IO_IN | G_IO_HUP | G_IO_PRI,
						       (GIOFunc) brasero_audio_disc_inotify_monitor_cb,
						       obj);
		g_io_channel_unref (obj->priv->notify);
	}
	else
		g_warning ("Failed to open /dev/inotify: %s\n",
			   g_strerror (errno));
#endif
}

static void
brasero_audio_disc_reset_real (BraseroAudioDisc *disc)
{
	if (disc->priv->io) {
		brasero_io_cancel_by_base (disc->priv->io, disc->priv->attr_changed);
		brasero_io_cancel_by_base (disc->priv->io, disc->priv->add_dir);
		brasero_io_cancel_by_base (disc->priv->io, disc->priv->add_uri);
		brasero_io_cancel_by_base (disc->priv->io, disc->priv->add_playlist);
	}

#ifdef BUILD_INOTIFY

	GSList *iter;

	/* we remove all the moved events waiting in the list */
	for (iter = disc->priv->moved_list; iter; iter = iter->next) {
		BraseroInotifyMovedData *data;

		data = iter->data;
		g_source_remove (data->id);
		g_free (data->uri);
		g_free (data);
	}
	g_slist_free (disc->priv->moved_list);
	disc->priv->moved_list = NULL;

	/* destroy monitoring hash */
	if (disc->priv->monitored) {
		g_hash_table_destroy (disc->priv->monitored);
		disc->priv->monitored = NULL;
	}

	if (disc->priv->reload_uri && disc->priv->io)
		brasero_io_cancel_by_base (disc->priv->io, disc->priv->reload_uri);

#endif

	disc->priv->sectors = 0;

	disc->priv->activity_counter = 1;
	brasero_audio_disc_decrease_activity_counter (disc);

	if (disc->priv->selected_path) {
		gtk_tree_path_free (disc->priv->selected_path);
		disc->priv->selected_path = NULL;
	}
}

static void
brasero_audio_disc_finalize (GObject *object)
{
	BraseroAudioDisc *cobj;
	cobj = BRASERO_AUDIO_DISC(object);
	
	brasero_audio_disc_reset_real (cobj);

#ifdef BUILD_INOTIFY

	if (cobj->priv->notify_id)
		g_source_remove (cobj->priv->notify_id);

	if (cobj->priv->monitored) {
		g_hash_table_destroy (cobj->priv->monitored);
		cobj->priv->monitored = NULL;
	}

#endif
	
	if (cobj->priv->io) {
		brasero_io_cancel_by_base (cobj->priv->io, cobj->priv->attr_changed);
		brasero_io_cancel_by_base (cobj->priv->io, cobj->priv->add_dir);
		brasero_io_cancel_by_base (cobj->priv->io, cobj->priv->add_uri);
		brasero_io_cancel_by_base (cobj->priv->io, cobj->priv->add_playlist);
		g_free (cobj->priv->attr_changed);
		g_free (cobj->priv->add_dir);
		g_free (cobj->priv->add_uri);
		g_free (cobj->priv->add_playlist);
		cobj->priv->attr_changed = NULL;
		cobj->priv->add_dir = NULL;
		cobj->priv->add_uri = NULL;
		cobj->priv->add_playlist = NULL;

		g_object_unref (cobj->priv->io);
		cobj->priv->io = NULL;
	}

	if (cobj->priv->manager) {
		g_object_unref (cobj->priv->manager);
		cobj->priv->manager = NULL;
	}

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkWidget *
brasero_audio_disc_new ()
{
	BraseroAudioDisc *obj;
	
	obj = BRASERO_AUDIO_DISC (g_object_new (BRASERO_TYPE_AUDIO_DISC, NULL));
	
	return GTK_WIDGET (obj);
}

/******************************** activity *************************************/
static BraseroDiscResult
brasero_audio_disc_get_status (BraseroDisc *disc,
			       gint *remaining,
			       gchar **current_task)
{
	GtkTreeModel *model;

	if (BRASERO_AUDIO_DISC (disc)->priv->activity_counter) {
		if (remaining)
			*remaining = BRASERO_AUDIO_DISC (disc)->priv->activity_counter;

		if (current_task)
			*current_task = g_strdup (_("Analysing audio files"));

		return BRASERO_DISC_NOT_READY;
	}

	if (BRASERO_AUDIO_DISC (disc)->priv->loading)
		return BRASERO_DISC_LOADING;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (BRASERO_AUDIO_DISC (disc)->priv->tree));
	if (!gtk_tree_model_iter_n_children (model, NULL))
		return BRASERO_DISC_ERROR_EMPTY_SELECTION;

	return BRASERO_DISC_OK;
}

static void
brasero_audio_disc_increase_activity_counter (BraseroAudioDisc *disc)
{
	GdkCursor *cursor;

	if (disc->priv->activity_counter == 0 && GTK_WIDGET (disc)->window) {
		cursor = gdk_cursor_new (GDK_WATCH);
		gdk_window_set_cursor (GTK_WIDGET (disc)->window, cursor);
		gdk_cursor_unref (cursor);
	}

	disc->priv->activity_counter++;
}

static void
brasero_audio_disc_decrease_activity_counter (BraseroAudioDisc *disc)
{

	if (disc->priv->activity_counter == 1 && GTK_WIDGET (disc)->window)
		gdk_window_set_cursor (GTK_WIDGET (disc)->window, NULL);

	disc->priv->activity_counter--;
}

/******************************** utility functions ****************************/
static void
brasero_audio_disc_size_changed (BraseroAudioDisc *disc)
{
	brasero_disc_size_changed (BRASERO_DISC (disc),
				   disc->priv->sectors);
}

static gboolean
brasero_audio_disc_has_gap (BraseroAudioDisc *disc,
			    GtkTreeIter *row,
			    GtkTreeIter *gap)
{
	GtkTreeModel *model;
	gboolean is_song;

	*gap = *row;
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (disc->priv->tree));
	if (!gtk_tree_model_iter_next (model, gap))
		return FALSE;

	gtk_tree_model_get (model, gap,
			    SONG_COL, &is_song,
			    -1);

	return (is_song != TRUE);
}

static void
brasero_audio_disc_add_gap (BraseroAudioDisc *disc,
			    GtkTreeIter *iter,
			    gint64 gap)
{
	GtkTreeIter gap_iter;
	GtkTreeModel *model;
	gint64 length;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (disc->priv->tree));
	if (gap) {
		gchar *size;

		if (brasero_audio_disc_has_gap (disc, iter, &gap_iter)) {
			gtk_tree_model_get (model, &gap_iter,
					    LENGTH_COL, &length,
					    -1);

			disc->priv->sectors -= BRASERO_DURATION_TO_SECTORS (length);
		}
		else {
			gchar *string;

			gtk_list_store_insert_after (GTK_LIST_STORE (model),
						     &gap_iter,
						     iter);

			string = g_strdup_printf ("<i><b>%s</b></i>", _("Pause"));
			gtk_list_store_set (GTK_LIST_STORE (model), &gap_iter,
					    NAME_COL, string,
					    SONG_COL, FALSE,
					    //BACKGROUND_COL, "green yellow",
					    ICON_COL, GTK_STOCK_MEDIA_PAUSE,
					    -1);
			g_free (string);
		}

		size = brasero_units_get_time_string (gap, TRUE, FALSE);
		gtk_list_store_set (GTK_LIST_STORE (model), &gap_iter,
				    SIZE_COL, size,
				    LENGTH_COL, gap,
				    -1);
		g_free (size);

		disc->priv->sectors += BRASERO_DURATION_TO_SECTORS (gap);
		brasero_audio_disc_size_changed (disc);
	}
	else if (brasero_audio_disc_has_gap (disc, iter, &gap_iter)) {
		gtk_tree_model_get (model, &gap_iter,
				    LENGTH_COL, &length,
				    -1);

		disc->priv->sectors -= BRASERO_DURATION_TO_SECTORS (length);
		brasero_audio_disc_size_changed (disc);
		gtk_list_store_remove (GTK_LIST_STORE (model), &gap_iter);
	}
}

static void
brasero_audio_disc_re_index_track_num (BraseroAudioDisc *disc,
				       GtkTreeModel *model)
{
	GtkTreeIter iter;
	gboolean is_song;
	gchar *text;
	gint num = 0;

	if (!gtk_tree_model_get_iter_first (model, &iter))
		return;

	g_signal_handlers_block_by_func (model,
					 brasero_audio_disc_row_changed_cb,
					 disc);
	do {
		gtk_tree_model_get (model, &iter,
				    SONG_COL, &is_song,
				    -1);

		/* This is gap row */
		if (!is_song)
			continue;

		num ++;
		text = g_strdup_printf ("%02i", num);

		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				    TRACK_NUM_COL, text,
				    -1);

		g_free (text);
	} while (gtk_tree_model_iter_next (model, &iter));

	g_signal_handlers_unblock_by_func (model,
					   brasero_audio_disc_row_changed_cb,
					   disc);
}

static gint
brasero_audio_disc_get_track_num (BraseroAudioDisc *disc,
				  GtkTreeModel *model)
{
	GtkTreeIter iter;
	gboolean is_song;
	gint num = 0;

	if (!gtk_tree_model_get_iter_first (model, &iter))
		return 0;

	g_signal_handlers_block_by_func (model,
					 brasero_audio_disc_row_changed_cb,
					 disc);
	do {
		gtk_tree_model_get (model, &iter,
				    SONG_COL, &is_song,
				    -1);

		if (is_song)
			num ++;

	} while (gtk_tree_model_iter_next (model, &iter));

	g_signal_handlers_unblock_by_func (model,
					   brasero_audio_disc_row_changed_cb,
					   disc);

	return num;		
}

static void
brasero_audio_disc_row_deleted_cb (GtkTreeModel *model,
				   GtkTreePath *path,
				   BraseroAudioDisc *disc)
{
	brasero_disc_contents_changed (BRASERO_DISC (disc),
				       brasero_audio_disc_get_track_num (disc, model));
	brasero_audio_disc_re_index_track_num (disc, model);
}

static void
brasero_audio_disc_row_inserted_cb (GtkTreeModel *model,
				    GtkTreePath *path,
				    GtkTreeIter *iter,
				    BraseroAudioDisc *disc)
{
	brasero_disc_contents_changed (BRASERO_DISC (disc),
				       brasero_audio_disc_get_track_num (disc, model));
}

static void
brasero_audio_disc_row_changed_cb (GtkTreeModel *model,
				   GtkTreePath *path,
				   GtkTreeIter *iter,
				   BraseroAudioDisc *disc)
{
	brasero_audio_disc_re_index_track_num (disc, model);
	brasero_disc_contents_changed (BRASERO_DISC (disc),
				       brasero_audio_disc_get_track_num (disc, model));
}

static void
brasero_audio_disc_short_track_dialog (BraseroAudioDisc *disc)
{
	brasero_app_alert (brasero_app_get_default (),
			   _("The track will be padded at its end."),
			   _("The track is shorter than 6 seconds"),
			   GTK_MESSAGE_WARNING);
}

static gchar *
brasero_audio_disc_validate_utf8_name (const gchar *name)
{
	gchar *retval, *ptr;
	const gchar *invalid;

	if (!name)
		return NULL;

	if (g_utf8_validate (name, -1, &invalid))
		return g_markup_escape_text (name, -1);

	retval = g_strdup (name);
	ptr = retval + (invalid - name);
	*ptr = '_';
	ptr++;

	while (!g_utf8_validate (ptr, -1, &invalid)) {
		ptr = (gchar*) invalid;
		*ptr = '?';
		ptr ++;
	}

	ptr = retval;
	retval = g_markup_escape_text (retval, -1);
	g_free (ptr);

	return retval;
}

static gboolean
brasero_audio_disc_set_row_from_metadata (BraseroAudioDisc *disc,
					  GtkTreeModel *model,
					  GtkTreeIter *iter,
					  GFileInfo *info)
{
	const gchar *icon_string = BRASERO_DEFAULT_ICON;
	gint64 current_length;
	gboolean composer_set;
	GtkTreeIter gap_iter;
	gboolean artist_set;
	gchar *size_string;
	gboolean title_set;
	gboolean isrc_set;
	gint64 length;
	gint64 start;
	GIcon *icon;
	gint64 end;

	/* NOTE: implemented in glib 2.15.6 (not for windows though) */
	icon = g_content_type_get_icon (g_file_info_get_content_type (info));
	if (G_IS_THEMED_ICON (icon)) {
		const gchar * const *names = NULL;

		names = g_themed_icon_get_names (G_THEMED_ICON (icon));
		if (names) {
			gint i;
			GtkIconTheme *theme;

			theme = gtk_icon_theme_get_default ();
			for (i = 0; names [i]; i++) {
				if (gtk_icon_theme_has_icon (theme, names [i])) {
					icon_string = names [i];
					break;
				}
			}
		}
	}

	gtk_tree_model_get (model, iter,
			    START_COL, &start,
			    END_COL, &end,
			    -1);

	/* make sure there is a length and it's not over the real one */
	if (start >= 0 && end > 0)
		current_length = BRASERO_STREAM_LENGTH (start, end);
	else
		current_length = 0;

	if (current_length > g_file_info_get_attribute_uint64 (info, BRASERO_IO_LEN)) {
		guint64 len;

		len = g_file_info_get_attribute_uint64 (info, BRASERO_IO_LEN);
		end = len - start;
		gtk_list_store_set (GTK_LIST_STORE (model), iter,
				    END_COL, (gint64) end,
				    -1);
	}
	/* make sure a length was set if not (and then start is 0) then set it */
	else if (end <= 0) {
		end = g_file_info_get_attribute_uint64 (info, BRASERO_IO_LEN);
		gtk_list_store_set (GTK_LIST_STORE (model), iter,
				    END_COL, (gint64) end,
				    -1);
	}

	/* Just in case */
	if (start > end) {
		/* problem */
		start = end - BRASERO_MIN_STREAM_LENGTH;
		if (start < 0)
			start = 0;

		gtk_list_store_set (GTK_LIST_STORE (model), iter,
				    START_COL, (gint64) start,
				    -1);
	}

	/* check the track size and warn the user just in case */
	if (brasero_audio_disc_has_gap (disc, iter, &gap_iter)) {
		gint64 gap;

		gtk_tree_model_get (model, &gap_iter,
				    LENGTH_COL, &gap,
				    -1);

		/* gap counts as length */
		if (end - start + BRASERO_SECTORS_TO_TIME (gap) < BRASERO_MIN_STREAM_LENGTH)
			brasero_audio_disc_short_track_dialog (disc);
	}
	else if (end - start < BRASERO_MIN_STREAM_LENGTH) {
		brasero_audio_disc_short_track_dialog (disc);
	}

	length = BRASERO_STREAM_LENGTH (start, end);
	if (length != current_length) {
		/* update global size */
		if (current_length > 0)
			disc->priv->sectors -= BRASERO_DURATION_TO_SECTORS (current_length);

		disc->priv->sectors += BRASERO_DURATION_TO_SECTORS (length);
		brasero_audio_disc_size_changed (disc);
	}

	size_string = brasero_units_get_time_string (length, TRUE, FALSE);
	gtk_list_store_set (GTK_LIST_STORE (model), iter,
			    SIZE_COL, size_string,
			    ICON_COL, icon_string,
			    LENGTH_COL, g_file_info_get_attribute_uint64 (info, BRASERO_IO_LEN),
			    SONG_COL, TRUE,
			    -1);
	g_free (size_string);

	/* Set all informations provided they were not set already */
	gtk_tree_model_get (model, iter,
			    TITLE_SET_COL, &title_set,
			    ARTIST_SET_COL, &artist_set,
			    COMPOSER_SET_COL, &composer_set,
			    ISRC_SET_COL, &isrc_set,
			    -1);

	if (!title_set && g_file_info_get_attribute_string (info, BRASERO_IO_TITLE)) {
		gchar *name;

		name = brasero_audio_disc_validate_utf8_name (g_file_info_get_attribute_string (info, BRASERO_IO_TITLE));
		gtk_list_store_set (GTK_LIST_STORE (model), iter,
				    NAME_COL, name,
				    -1);
		g_free (name);
	}

	if (!artist_set && g_file_info_get_attribute_string (info, BRASERO_IO_ARTIST)) {
		gtk_list_store_set (GTK_LIST_STORE (model), iter,
				    ARTIST_COL, g_file_info_get_attribute_string (info, BRASERO_IO_ARTIST),
				    -1);
	}
	else if (!artist_set)
		gtk_list_store_set (GTK_LIST_STORE (model), iter,
				    ARTIST_COL, ("Unknown"),
				    -1);

	if (!composer_set && g_file_info_get_attribute_string (info, BRASERO_IO_COMPOSER)) {
		gtk_list_store_set (GTK_LIST_STORE (model), iter,
				    COMPOSER_COL, g_file_info_get_attribute_string (info, BRASERO_IO_COMPOSER),
				    -1);
	}

	if (!isrc_set && g_file_info_get_attribute_int32 (info, BRASERO_IO_ISRC)) {
		gtk_list_store_set (GTK_LIST_STORE (model), iter,
				    ISRC_COL, g_file_info_get_attribute_int32 (info, BRASERO_IO_ISRC),
				    -1);
	}

	if (icon)
		g_object_unref (icon);

	return TRUE;
}

/*************** shared code for dir/playlist addition *************************/
static void
brasero_audio_disc_file_type_error_dialog (BraseroAudioDisc *disc,
					   const gchar *uri)
{
	gchar *primary;
	gchar *name;

    	BRASERO_GET_BASENAME_FOR_DISPLAY (uri, name);
	primary = g_strdup_printf (_("\"%s\" could not be handled by Gstreamer."), name);
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
	string = g_strdup_printf (_("Do you want to add \"%s\" which is a video file?"), name);
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

	return (answer == GTK_RESPONSE_OK);
}
static void
brasero_audio_disc_add_file (BraseroAudioDisc *disc,
			     const gchar *uri)
{
#ifdef BUILD_INOTIFY
	brasero_audio_disc_start_monitoring (disc, uri);
#endif
}

static void
brasero_audio_disc_result (GObject *obj,
			   GError *error,
			   const gchar *uri,
			   GFileInfo *info,
			   gpointer null_data)
{
	gchar *name;
	gchar *markup;
	GtkTreeIter iter;
	GtkTreeModel *model;
	gchar *escaped_name;
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

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (disc->priv->tree));
	gtk_list_store_append (GTK_LIST_STORE (model), &iter);

	escaped_name = g_path_get_basename (uri);
	name = g_uri_unescape_string (escaped_name, NULL);
	g_free (escaped_name);

	markup = brasero_audio_disc_validate_utf8_name (name);
	g_free (name);

	/* Set a default name here */
    	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			    NAME_COL, markup,
			    URI_COL, uri,
			    -1);
	g_free (markup);

	if (!brasero_audio_disc_set_row_from_metadata (disc,
						       model,
						       &iter,
						       info))
		return;

	brasero_audio_disc_add_file (disc, uri);
}

static void
brasero_audio_disc_vfs_operation_finished (GObject *object,
					   gboolean cancelled,
					   gpointer null_data)
{
	BraseroAudioDisc *disc = BRASERO_AUDIO_DISC (object);

	brasero_audio_disc_decrease_activity_counter (disc);

	if (disc->priv->loading)
		disc->priv->loading --;
}

/*********************** directories exploration *******************************/
static BraseroDiscResult
brasero_audio_disc_visit_dir_async (BraseroAudioDisc *disc,
				    const gchar *uri)
{
	if (!disc->priv->io)
		disc->priv->io = brasero_io_get_default ();

	if (!disc->priv->add_dir)
		disc->priv->add_dir = brasero_io_register (G_OBJECT (disc),
							   brasero_audio_disc_result,
							   brasero_audio_disc_vfs_operation_finished,
							   NULL);

	brasero_audio_disc_increase_activity_counter (disc);

	/* we have to pass a dummy value here otherwise finished is never called */
	brasero_io_load_directory (disc->priv->io,
				   uri,
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
						  _("Directories cannot be added to an audio disc."));

	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				_("Search _Directory"), GTK_RESPONSE_OK,
				NULL);

	gtk_widget_show_all (dialog);
	answer = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	if (answer != GTK_RESPONSE_OK)
		return TRUE;

	return brasero_audio_disc_visit_dir_async (disc, uri);
}

/************************** playlist parsing ***********************************/

#if BUILD_PLAYLIST

static BraseroDiscResult
brasero_audio_disc_add_playlist (BraseroAudioDisc *disc,
				 const gchar *uri)
{
	if (!disc->priv->io)
		disc->priv->io = brasero_io_get_default ();

	if (!disc->priv->add_playlist)
		disc->priv->add_playlist = brasero_io_register (G_OBJECT (disc),
								brasero_audio_disc_result,
								brasero_audio_disc_vfs_operation_finished,
								NULL);

	brasero_audio_disc_increase_activity_counter (disc);
	brasero_io_parse_playlist (disc->priv->io,
				   uri,
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
	gchar *name = NULL;

	BRASERO_GET_BASENAME_FOR_DISPLAY (uri, name);
	primary = g_strdup_printf (_("\"%s\" could not be opened."), name);
	brasero_app_alert (brasero_app_get_default (),
			   primary,
			   error->message,
			   GTK_MESSAGE_ERROR);
	g_free (primary);
	g_free (name);
}

static void
brasero_audio_disc_new_row_cb (GObject *obj,
			       GError *error,
			       const gchar *uri,
			       GFileInfo *info,
			       gpointer user_data)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeIter gap_iter;
	GtkTreePath *treepath;
	GtkTreeRowReference *ref = user_data;
	BraseroAudioDisc *disc = BRASERO_AUDIO_DISC (obj);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (disc->priv->tree));
	treepath = gtk_tree_row_reference_get_path (ref);
	gtk_tree_row_reference_free (ref);
	if (!treepath)
		return;

	gtk_tree_model_get_iter (model, &iter, treepath);
	gtk_tree_path_free (treepath);

	if (error) {
		brasero_audio_disc_unreadable_dialog (disc,
						      uri,
						      error);

		if (brasero_audio_disc_has_gap (disc, &iter, &gap_iter))
			brasero_audio_disc_add_gap (disc, &gap_iter, 0);

		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
		return;
	}

	if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
		if (brasero_audio_disc_has_gap (disc, &iter, &gap_iter))
			brasero_audio_disc_add_gap (disc, &gap_iter, 0);

		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
		brasero_audio_disc_add_dir (disc, uri);
		return;
	}

#if BUILD_PLAYLIST

	const gchar *mime;

	/* see if it a playlist */
	mime = g_file_info_get_content_type (info);
	if (g_file_info_get_file_type (info) == G_FILE_TYPE_REGULAR
	&&  (!strcmp (mime, "audio/x-scpls")
	||   !strcmp (mime, "audio/x-ms-asx")
	||   !strcmp (mime, "audio/x-mp3-playlist")
	||   !strcmp (mime, "audio/x-mpegurl"))) {
		/* This is a supported playlist */
		if (brasero_audio_disc_has_gap (disc, &iter, &gap_iter))
			brasero_audio_disc_add_gap (disc, &gap_iter, 0);

		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
		brasero_audio_disc_add_playlist (disc, uri);
		return;
	}

#endif

	if (g_file_info_get_file_type (info) != G_FILE_TYPE_REGULAR
	|| !g_file_info_get_attribute_boolean (info, BRASERO_IO_HAS_AUDIO)) {
		if (brasero_audio_disc_has_gap (disc, &iter, &gap_iter))
			brasero_audio_disc_add_gap (disc, &gap_iter, 0);

		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
		brasero_audio_disc_file_type_error_dialog (disc, uri);
		return;
	}

	if (g_file_info_get_attribute_boolean (info, BRASERO_IO_HAS_VIDEO)
	&& !brasero_audio_disc_video_file_dialog (disc, uri)) {
		if (brasero_audio_disc_has_gap (disc, &iter, &gap_iter))
			brasero_audio_disc_add_gap (disc, &gap_iter, 0);

		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
		return;
	}

	if (g_file_info_get_is_symlink (info)) {
		uri = g_strconcat ("file://", g_file_info_get_symlink_target (info), NULL);
		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				    URI_COL, uri, -1);
	}

	if (!brasero_audio_disc_set_row_from_metadata (disc,
						       model,
						       &iter,
						       info))
		return;

	brasero_audio_disc_add_file (disc, uri);
}

static BraseroDiscResult
brasero_audio_disc_add_uri_real (BraseroAudioDisc *disc,
				 const gchar *uri,
				 gint pos,
				 gint64 gap_sectors,
				 gint64 start,
				 gint64 end,
				 BraseroStreamInfo *info,
				 GtkTreePath **path_return)
{
	GtkTreeRowReference *ref;
	GtkTreePath *treepath;
	GtkTreeModel *store;
	GtkTreeIter iter;
	gchar *markup;
	gchar *name;

	g_return_val_if_fail (uri != NULL, BRASERO_DISC_ERROR_UNKNOWN);

	if (disc->priv->reject_files)
		return BRASERO_DISC_NOT_READY;

	gtk_notebook_set_current_page (GTK_NOTEBOOK (BRASERO_AUDIO_DISC (disc)->priv->notebook), 1);

	store = gtk_tree_view_get_model (GTK_TREE_VIEW (disc->priv->tree));
	if (pos > -1)
		gtk_list_store_insert (GTK_LIST_STORE (store), &iter, pos);
	else
		gtk_list_store_append (GTK_LIST_STORE (store), &iter);

	BRASERO_GET_BASENAME_FOR_DISPLAY (uri, name);
	markup = g_markup_escape_text (name, -1);
	g_free (name);

	if (info)
		gtk_list_store_set (GTK_LIST_STORE (store), &iter,
				    URI_COL, uri,
				    SONG_COL, TRUE,
				    ICON_COL, "image-loading",
				    NAME_COL, info->title? info->title:markup,
				    TITLE_SET_COL, info->title? TRUE:FALSE,
				    ARTIST_COL, info->artist? info->artist:_("(loading ...)"),
				    ARTIST_SET_COL, info->artist? TRUE:FALSE,
				    COMPOSER_COL, info->composer? info->composer:NULL,
				    COMPOSER_SET_COL, info->composer? TRUE:FALSE,
				    ISRC_COL, info->isrc,
				    ISRC_SET_COL, info->isrc? TRUE:FALSE,
				    -1);
	else
		gtk_list_store_set (GTK_LIST_STORE (store), &iter,
				    NAME_COL, markup,
				    ICON_COL, "image-loading",
				    URI_COL, uri,
				    ARTIST_COL, _("(loading ...)"),
				    SONG_COL, TRUE,
				    -1);
	g_free (markup);

	start = start > 0 ? start:0;
	if (end > 0 && end > start) {
		gchar *string;
		gint64 length;

		/* update global size */
		length = BRASERO_STREAM_LENGTH (start, end);
		disc->priv->sectors += BRASERO_DURATION_TO_SECTORS (length);
		brasero_audio_disc_size_changed (disc);

		string = brasero_units_get_time_string (length, TRUE, FALSE);
		gtk_list_store_set (GTK_LIST_STORE (store), &iter,
				    START_COL, start,
				    END_COL, end,
				    SIZE_COL, string,
				    -1);
		g_free (string);
	}
	else
		gtk_list_store_set (GTK_LIST_STORE (store), &iter,
				    SIZE_COL, _("(loading ...)"),
				    -1);

	if (gap_sectors > 0)
		brasero_audio_disc_add_gap (disc,
					    &iter,
					    BRASERO_SECTORS_TO_TIME (gap_sectors));

	treepath = gtk_tree_model_get_path (store, &iter);
	ref = gtk_tree_row_reference_new (store, treepath);

	if (path_return)
		*path_return = treepath;
	else
		gtk_tree_path_free (treepath);

	/* get info async for the file */
	if (!disc->priv->io)
		disc->priv->io = brasero_io_get_default ();

	if (!disc->priv->add_uri)
		disc->priv->add_uri = brasero_io_register (G_OBJECT (disc),
							   brasero_audio_disc_new_row_cb,
							   brasero_audio_disc_vfs_operation_finished,
							   NULL);
	/* FIXME: if cancelled ref won't be destroyed ? 
	 * no, since the callback is always called even if there is an error */
	brasero_audio_disc_increase_activity_counter (disc);
	brasero_io_get_file_info (disc->priv->io,
				  uri,
				  disc->priv->add_uri,
				  BRASERO_IO_INFO_PERM|
				  BRASERO_IO_INFO_MIME|
				  BRASERO_IO_INFO_URGENT|
				  BRASERO_IO_INFO_METADATA|
				  BRASERO_IO_INFO_METADATA_MISSING_CODEC,
				  ref);

	return BRASERO_DISC_OK;
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
						  NULL,
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

/******************************** Row removing *********************************/
static void
brasero_audio_disc_remove (BraseroAudioDisc *disc,
			   GtkTreePath *treepath)
{
	GtkTreeIter gap_iter;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gint64 sectors;
	gint64 length;
	gint64 start;
	gint64 end;
	gchar *uri;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (disc->priv->tree));
	gtk_tree_model_get_iter (model, &iter, treepath);
	gtk_tree_model_get (model, &iter,
			    URI_COL, &uri,
			    START_COL, &start,
			    END_COL, &end,
			    LENGTH_COL, &length,
			    -1);

	sectors = 0;
	if (uri) {
		if (end - start > 0)
			sectors = BRASERO_DURATION_TO_SECTORS (BRASERO_STREAM_LENGTH (start, end));
	}
	else if (length) /* gap */
		sectors = BRASERO_DURATION_TO_SECTORS (length);

	if (brasero_audio_disc_has_gap (disc, &iter, &gap_iter)) {
		gint64 gap_length = 0;

		gtk_tree_model_get (model, &gap_iter,
				    LENGTH_COL, &gap_length,
				    -1);

		sectors += BRASERO_DURATION_TO_SECTORS (gap_length);
		gtk_list_store_remove (GTK_LIST_STORE (model), &gap_iter);
	}

	gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

	if (sectors > 0) {
		disc->priv->sectors -= sectors;
		brasero_audio_disc_size_changed (disc);
	}

	if (!uri)
		return;

#ifdef BUILD_INOTIFY
	brasero_audio_disc_cancel_monitoring (disc, uri);
#endif
	g_free (uri);
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

static void
brasero_audio_disc_clear (BraseroDisc *disc)
{
	BraseroAudioDisc *audio;
	GtkTreeModel *model;

	audio = BRASERO_AUDIO_DISC (disc);
	brasero_audio_disc_reset_real (audio);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (audio->priv->tree));
	gtk_list_store_clear (GTK_LIST_STORE (model));

	gtk_notebook_set_current_page (GTK_NOTEBOOK (BRASERO_AUDIO_DISC (disc)->priv->notebook), 0);
	brasero_disc_size_changed (disc, 0);
}

static void
brasero_audio_disc_reset (BraseroDisc *disc)
{
	brasero_audio_disc_clear (disc);
}

/********************************* create track ********************************/
static BraseroDiscResult
brasero_audio_disc_get_track (BraseroDisc *disc,
			      BraseroDiscTrack *track)
{
	gchar *uri;
	GtkTreeIter iter;
	GtkTreeModel *model;
	BraseroDiscSong *song;
	BraseroAudioDisc *audio;

	audio = BRASERO_AUDIO_DISC (disc);
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (audio->priv->tree));
	if (!gtk_tree_model_get_iter_first (model, &iter))
		return BRASERO_DISC_ERROR_EMPTY_SELECTION;

	track->type = BRASERO_PROJECT_TYPE_AUDIO;
	song = NULL;

	do {
		gint isrc;
		gint64 end;
		gint64 start;
		gchar *title;
		gchar *artist;
		gchar *composer;
		gboolean isrc_set;
		gboolean title_set;
		gboolean artist_set;
		gboolean composer_set;
		gboolean length_set;

		gtk_tree_model_get (model, &iter,
				    URI_COL, &uri,
				    START_COL, &start,
				    END_COL, &end,
				    LENGTH_SET_COL, &length_set,
				    TITLE_SET_COL, &title_set,
				    ARTIST_SET_COL, &artist_set,
				    COMPOSER_SET_COL, &composer_set,
				    ISRC_SET_COL, &isrc_set,
				    NAME_COL, &title,
				    ARTIST_COL, &artist,
				    COMPOSER_COL, &composer,
				    ISRC_COL, &isrc,
				    -1);

		if (!uri) {
			gint64 length;

			/* this is a gap */
			gtk_tree_model_get (model, &iter,
					    LENGTH_COL, &length,
					    -1);
			if (length && song)
				song->gap += BRASERO_DURATION_TO_SECTORS (length);
		}
		else {
			BraseroStreamInfo *info;

			song = g_new0 (BraseroDiscSong, 1);
			song->uri = uri;

			if (length_set) {
				song->start = start > 0 ? start:0;
				song->end = end > 0 ? end:0;
			}

			info = g_new0 (BraseroStreamInfo, 1);
			if (title_set)
				info->title = title;
			else
				g_free (title);

			if (artist_set)
				info->artist = artist;
			else
				g_free (artist);

			if (composer_set)
				info->composer = composer;
			else
				g_free (composer);

			if (isrc_set)
				info->isrc = isrc;

			song->info = info;

			track->contents.tracks = g_slist_append (track->contents.tracks, song);
		}

	} while (gtk_tree_model_iter_next (model, &iter));

	return BRASERO_DISC_OK;
}

static BraseroDiscResult
brasero_audio_disc_set_session_contents (BraseroDisc *disc,
					 BraseroBurnSession *session)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	BraseroAudioDisc *audio;
	BraseroTrackStream *track;

	audio = BRASERO_AUDIO_DISC (disc);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (audio->priv->tree));
	if (!gtk_tree_model_get_iter_first (model, &iter))
		return BRASERO_DISC_ERROR_EMPTY_SELECTION;

	track = NULL;
	do {
		gchar *uri;
		gchar *title;
		gchar *artist;
		gchar *composer;
		gint isrc;
		gint64 end;
		gint64 start;
		gint64 length;

		gtk_tree_model_get (model, &iter,
				    URI_COL, &uri,
				    NAME_COL, &title,
				    ARTIST_COL, &artist,
				    COMPOSER_COL, &composer,
				    ISRC_COL, &isrc,
				    LENGTH_COL, &length,
				    START_COL, &start,
				    END_COL, &end,
				    -1);

		if (!uri) {
			/* This is a gap so sectors refers to its size */
			brasero_track_stream_set_boundaries (BRASERO_TRACK_STREAM (track),
							     -1,
							     -1,
							     length);
			continue;
		}

		track = brasero_track_stream_new ();
		brasero_track_stream_set_source (track, uri);
		brasero_track_stream_set_format (track,
						 BRASERO_AUDIO_FORMAT_UNDEFINED|
						 BRASERO_METADATA_INFO);

		brasero_track_stream_set_boundaries (track, start, end, -1);

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
			brasero_track_tag_add_int (BRASERO_TRACK (track),
						   BRASERO_TRACK_STREAM_ISRC_TAG,
						   isrc);

		brasero_burn_session_add_track (session, BRASERO_TRACK (track));

		/* It's good practice to unref the track afterwards as we don't
		 * need it anymore. BraseroBurnSession refs it. */
		g_object_unref (track);
		g_free (uri);
	} while (gtk_tree_model_iter_next (model, &iter));

	return BRASERO_DISC_OK;
}

/********************************* load track **********************************/
static void
brasero_audio_disc_add_track (BraseroAudioDisc *disc,
			      BraseroDiscSong *song)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (disc->priv->tree));
	gtk_list_store_append (GTK_LIST_STORE (model), &iter);

	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			    URI_COL, song->uri,
			    START_COL, (guint64) song->start,
			    END_COL, (guint64) song->end,
			    SONG_COL, TRUE,
			    -1);

	disc->priv->sectors += BRASERO_DURATION_TO_SECTORS (BRASERO_STREAM_LENGTH (song->start, song->end));

	if (song->info) {
		if (song->info->title)
			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					    NAME_COL, song->info->title,
					    TITLE_SET_COL, TRUE,
					    -1);
		if (song->info->artist)
			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					    ARTIST_COL, song->info->artist,
					    ARTIST_SET_COL, TRUE,
					    -1);
		if (song->info->composer)
			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					    COMPOSER_COL, song->info->composer,
					    COMPOSER_SET_COL, TRUE,
					    -1);
		if (song->info->isrc)
			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					    ISRC_COL, song->info->isrc,
					    ISRC_SET_COL, TRUE,
					    -1);
	}

	if (song->gap) {
		gtk_list_store_append (GTK_LIST_STORE (model), &iter);
		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				    LENGTH_COL, (guint64) song->gap,
				    -1);
		disc->priv->sectors += BRASERO_DURATION_TO_SECTORS (song->gap);
	}
}

static BraseroDiscResult
brasero_audio_disc_load_track (BraseroDisc *disc,
			       BraseroDiscTrack *track)
{
	GSList *iter;

	g_return_val_if_fail (track->type == BRASERO_PROJECT_TYPE_AUDIO, FALSE);

	if (track->contents.tracks == NULL)
		return BRASERO_DISC_ERROR_EMPTY_SELECTION;

	for (iter = track->contents.tracks; iter; iter = iter->next) {
		BraseroDiscSong *song;
		BraseroStreamInfo *info;

		song = iter->data;
		info = song->info;

		if (song->end > 0 && !brasero_app_is_running (brasero_app_get_default ())) {
			/* Set the minimum information */
			brasero_audio_disc_add_track (BRASERO_AUDIO_DISC (disc), song);
		}
		else {
			BRASERO_AUDIO_DISC (disc)->priv->loading ++;
			brasero_audio_disc_add_uri_real (BRASERO_AUDIO_DISC (disc),
							 song->uri,
							 -1,
							 song->gap,
							 song->start,
							 song->end,
							 info,
							 NULL);
		}
		
	}

	return BRASERO_DISC_OK;
}

/********************************* DND *****************************************/
static gint
brasero_audio_disc_get_dest_path (BraseroAudioDisc *disc,
				  gint x,
				  gint y)
{
	gint position;
	GtkTreeModel *model;
	GtkTreeViewDropPosition pos;
	GtkTreePath *treepath = NULL;

	/* while the treeview is still under the information pane, it is not 
	 * realized yet and the following function will fail */
	if (GTK_WIDGET_DRAWABLE (disc->priv->tree))
		gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW (disc->priv->tree),
						   x,
						   y,
						   &treepath,
						   &pos);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (disc->priv->tree));

	if (treepath) {
		if (pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE
		||  pos == GTK_TREE_VIEW_DROP_BEFORE) {
			if (!gtk_tree_path_prev (treepath)) {
				gtk_tree_path_free (treepath);
				treepath = NULL;
			}
		}

		if (treepath) {
			gint *indices;
			gboolean is_song;
			GtkTreeIter iter;

			/* we check that the dest is not in between a song
			 * and its pause/gap/silence */
			gtk_tree_model_get_iter (model, &iter, treepath);
			gtk_tree_model_get (model, &iter,
					    SONG_COL, &is_song,
					    -1);

			if (is_song
			&&  gtk_tree_model_iter_next (model, &iter)) {
				gtk_tree_model_get (model, &iter,
						    SONG_COL, &is_song,
						    -1);

				if (!is_song)
					gtk_tree_path_next (treepath);
			}

			indices = gtk_tree_path_get_indices (treepath);
			position = indices [0];

			gtk_tree_path_free (treepath);
		}
		else
			position = -1;
	}
	else
		position = gtk_tree_model_iter_n_children (model, NULL) - 1;

	return position;
}

static void
brasero_audio_disc_merge_gaps (BraseroAudioDisc *disc,
			       GtkTreeModel *model,
			       GtkTreeIter *iter,
			       GtkTreeIter *pos)
{
	GtkTreePath *iter_path, *pos_path;
	gint64 length_iter, length_pos;
	gchar *size;
	gint equal;

	iter_path = gtk_tree_model_get_path (model, iter);
	pos_path = gtk_tree_model_get_path (model, pos);
	equal = gtk_tree_path_compare (iter_path, pos_path);
	gtk_tree_path_free (iter_path);
	gtk_tree_path_free (pos_path);

	if (!equal)
		return;

	gtk_tree_model_get (model, pos,
			    LENGTH_COL, &length_pos,
			    -1);
	gtk_tree_model_get (model, iter,
			    LENGTH_COL, &length_iter,
			    -1);

	length_pos += length_iter;
	size = brasero_units_get_time_string (length_pos, TRUE, FALSE);
	gtk_list_store_set (GTK_LIST_STORE (model), pos,
			    SIZE_COL, size,
			    LENGTH_COL, length_pos,
			    -1);
	g_free (size);

	gtk_list_store_remove (GTK_LIST_STORE (model), iter);
}

static GtkTreePath *
brasero_audio_disc_move_to_dest (BraseroAudioDisc *disc,
				 GtkTreeSelection *selection,
				 GtkTreeModel *model,
				 GtkTreePath *dest,
				 GtkTreeIter *iter,
				 gboolean is_gap)
{
	GtkTreeIter position;
	GtkTreeIter gap_iter;
	gboolean has_gap = FALSE;

	gtk_tree_model_get_iter (model, &position, dest);
	if (is_gap) {
		gboolean is_song;
		GtkTreeIter next_pos;

		/* Check that the row and the row under the destination
		 * row are not gaps as well and if so merge both of them */

		gtk_tree_model_get (model, &position,
				    SONG_COL, &is_song,
				    -1);

		if (!is_song) {
			/* NOTE: if the path we're moving is above the
			 * dest the dest path won't be valid once we
			 * have merged the two gaps */
			gtk_tree_path_free (dest);
			brasero_audio_disc_merge_gaps (disc,
						       model,
						       iter,
						       &position);
			return gtk_tree_model_get_path (model, &position);
		}

		gtk_tree_path_next (dest);
		if (gtk_tree_model_get_iter (model, &next_pos, dest)) {
			gtk_tree_model_get (model, &next_pos,
					    SONG_COL, &is_song,
					    -1);

			if (!is_song) {
				/* NOTE: if the path we're moving is above the
				 * dest the dest path won't be valid once we
				 * have merged the two gaps */
				gtk_tree_path_free (dest);
				brasero_audio_disc_merge_gaps (disc,
							       model,
							       iter,
							       &next_pos);
				return gtk_tree_model_get_path (model, &next_pos);
			}
		}
	}
	else
		has_gap = brasero_audio_disc_has_gap (disc, iter, &gap_iter);

	gtk_tree_path_free (dest);
	gtk_list_store_move_after (GTK_LIST_STORE (model),
				   iter,
				   &position);

	if (has_gap && !gtk_tree_selection_iter_is_selected (selection, &gap_iter)) {
		gtk_list_store_move_after (GTK_LIST_STORE (model),
					   &gap_iter,
					   iter);

		return gtk_tree_model_get_path (model, &gap_iter);
	}

	return gtk_tree_model_get_path (model, iter);
}

static void
brasero_audio_disc_move_to_first_pos (BraseroAudioDisc *disc,
				      GtkTreeSelection *selection,
				      GtkTreeModel *model,
				      GtkTreeIter *iter,
				      gboolean is_gap)
{
	GtkTreeIter position;
	GtkTreeIter gap_iter;
	gboolean has_gap = FALSE;

	gtk_tree_model_get_iter_first (model, &position);
	if (is_gap) {
		gboolean is_song;

		gtk_tree_model_get (model, &position,
				    SONG_COL, &is_song,
				    -1);

		if (!is_song) {
			brasero_audio_disc_merge_gaps (disc,
						       model,
						       iter,
						       &position);
			return;
		}
	}
	else
		has_gap = brasero_audio_disc_has_gap (disc, iter, &gap_iter);

	gtk_list_store_move_before (GTK_LIST_STORE (model),
				    iter,
				    &position);

	if (has_gap && !gtk_tree_selection_iter_is_selected (selection, &gap_iter))
		gtk_list_store_move_after (GTK_LIST_STORE (model),
					   &gap_iter,
					   iter);
}

static void
brasero_audio_disc_drag_data_received_cb (GtkTreeView *tree,
					  GdkDragContext *drag_context,
					  gint x,
					  gint y,
					  GtkSelectionData *selection_data,
					  guint info,
					  guint time,
					  BraseroAudioDisc *disc)
{
	GtkTreeSelection *selection;
	BraseroDiscResult success;
	gboolean result = TRUE;
	GtkTreePath *treepath;
	GtkTreeModel *model;
	gchar **uri, **uris;
	GtkTreeIter iter;
	gboolean is_song;
	gint pos;

	if (info == TREE_MODEL_ROW) {
		GList *references = NULL;
		GtkTreePath *dest;
		GList *list_iter;
		GList *selected;

		/* this signal gets emitted during dragging */
		if (disc->priv->dragging) {
			gdk_drag_status (drag_context,
					 GDK_ACTION_MOVE,
					 time);
			g_signal_stop_emission_by_name (tree, "drag-data-received");
			return;
		}

		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (disc->priv->tree));
		selected = gtk_tree_selection_get_selected_rows (selection, &model);

		for (list_iter = selected; list_iter; list_iter = list_iter->next) {
			GtkTreeRowReference *reference;

			reference = gtk_tree_row_reference_new (model, list_iter->data);
			references = g_list_prepend (references, reference);
			gtk_tree_path_free (list_iter->data);
		}
		g_list_free (selected);

		pos = brasero_audio_disc_get_dest_path (disc, x, y);
		if (pos != -1) {
			references = g_list_reverse (references);
			dest = gtk_tree_path_new_from_indices (pos, -1);
		}
		else
			dest = NULL;

		for (list_iter = references; list_iter; list_iter = list_iter->next) {
			treepath = gtk_tree_row_reference_get_path (list_iter->data);
			gtk_tree_row_reference_free (list_iter->data);

			if (!gtk_tree_model_get_iter (model, &iter, treepath)) {
				gtk_tree_path_free (treepath);
				continue;
			}
			gtk_tree_path_free (treepath);

			gtk_tree_model_get (model, &iter,
					    SONG_COL, &is_song,
					    -1);

			if (dest)
				dest = brasero_audio_disc_move_to_dest (disc,
									selection,
									model,
									dest,
									&iter,
									is_song == FALSE);
			else
				brasero_audio_disc_move_to_first_pos (disc,
								      selection,
								      model,
								      &iter,
								      is_song == FALSE);
		}
		g_list_free (references);

		if (dest)
			gtk_tree_path_free (dest);

		gtk_drag_finish (drag_context,
				 TRUE,
				 FALSE,
				 time);
		g_signal_stop_emission_by_name (tree, "drag-data-received");

		brasero_audio_disc_re_index_track_num (disc, model);
		return;
	}

	if (selection_data->length <= 0
	||  selection_data->format != 8) {
		gtk_drag_finish (drag_context, FALSE, FALSE, time);
		return;
	}

	/* get pos and URIS */
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (disc->priv->tree));
	gtk_tree_selection_unselect_all (selection);

	pos = brasero_audio_disc_get_dest_path (disc, x, y);
	pos ++;

	uris = gtk_selection_data_get_uris (selection_data);
	for (uri = uris; *uri != NULL; uri++) {
		treepath = NULL;
		success = brasero_audio_disc_add_uri_real (disc,
							   *uri,
							   pos,
							   0,
							   -1,
							   -1,
							   NULL,
							   &treepath);
		if (success == BRASERO_DISC_OK) {
			pos ++;
			if (treepath) {
				gtk_tree_selection_select_path (selection, treepath);
				gtk_tree_path_free (treepath);
			}
		}

		result = result ? (success == BRASERO_DISC_OK) : FALSE;
		g_free (*uri);
	}
	g_free (uris);

	gtk_drag_finish (drag_context,
			 result,
			 (drag_context->action == GDK_ACTION_MOVE),
			 time);
}

static gboolean
brasero_audio_disc_drag_drop_cb (GtkWidget *widget,
				 GdkDragContext*drag_context,
				 gint x,
				 gint y,
				 guint time,
				 BraseroAudioDisc *disc)
{
	disc->priv->dragging = 0;
	return FALSE;
}

static void
brasero_audio_disc_drag_leave_cb (GtkWidget *tree,
				  GdkDragContext *drag_context,
				  guint time,
				  BraseroAudioDisc *disc)
{
	if (disc->priv->drag_context) {
		GdkPixbuf *pixbuf;
		GdkPixmap *pixmap;
		GdkBitmap *mask;

		pixbuf = gtk_widget_render_icon (tree,
						 GTK_STOCK_DELETE,
						 GTK_ICON_SIZE_DND,
						 NULL);
		gdk_pixbuf_render_pixmap_and_mask_for_colormap (pixbuf,
								gtk_widget_get_colormap (tree),
								&pixmap,
								&mask,
								128);
		g_object_unref (pixbuf);

		gtk_drag_set_icon_pixmap (disc->priv->drag_context,
					  gdk_drawable_get_colormap (pixmap),
					  pixmap,
					  mask,
					  -2,
					  -2);
	}
}

static gboolean
brasero_audio_disc_drag_motion_cb (GtkWidget *tree,
				   GdkDragContext *drag_context,
				   gint x,
				   gint y,
				   guint time,
				   BraseroAudioDisc *disc)
{
	GdkAtom target;

	target = gtk_drag_dest_find_target (tree,
					    drag_context,
					    gtk_drag_dest_get_target_list(tree));

	if (target == gdk_atom_intern ("GTK_TREE_MODEL_ROW", FALSE)
	&&  disc->priv->drag_context)
		gtk_drag_set_icon_default (disc->priv->drag_context);

	return FALSE;
}

static void
brasero_audio_disc_drag_begin_cb (GtkWidget *widget,
				  GdkDragContext *drag_context,
				  BraseroAudioDisc *disc)
{
	disc->priv->drag_context = drag_context;
	g_object_ref (drag_context);

	disc->priv->dragging = 1;
}

static void
brasero_audio_disc_drag_end_cb (GtkWidget *tree,
			        GdkDragContext *drag_context,
			        BraseroAudioDisc *disc)
{
	gint x, y;

	if (disc->priv->drag_context) {
		g_object_unref (disc->priv->drag_context);
		disc->priv->drag_context = NULL;
	}

	gtk_widget_get_pointer (tree, &x, &y);
	if (x < 0 || y < 0 || x > tree->allocation.width || y > tree->allocation.height)
		brasero_audio_disc_delete_selected (BRASERO_DISC (disc));
}

/********************************** Cell Editing *******************************/
static void
brasero_audio_disc_display_edited_cb (GtkCellRendererText *renderer,
				      gchar *path_string,
				      gchar *text,
				      BraseroAudioDisc *disc)
{
	GtkTreePath *treepath;
	GtkTreeModel *model;
	gint col_set_num;
	GtkTreeIter row;
	gint col_num;

	col_num = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (renderer), COL_KEY));
	col_set_num = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (renderer), COL_KEY_SET));
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (disc->priv->tree));
	treepath = gtk_tree_path_new_from_string (path_string);
	gtk_tree_model_get_iter (model, &row, treepath);
	gtk_list_store_set (GTK_LIST_STORE (model), &row,
			    col_num, text,
			    col_set_num, TRUE,
			    -1);
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
	GtkTreeRowReference *ref;
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
		treepath = iter->data;
		ref = gtk_tree_row_reference_new (model, treepath);
		references = g_list_append (references, ref);
	}

	g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (selected);

	for (iter = references; iter; iter = iter->next) {
		GtkTreeIter row;

		ref = iter->data;
		treepath = gtk_tree_row_reference_get_path (ref);
		gtk_tree_row_reference_free (ref);

		if (gtk_tree_model_get_iter (model, &row, treepath)) {
			gboolean is_song;

			gtk_tree_model_get (model, &row,
					    SONG_COL, &is_song,
					    -1);

			if (is_song)
				brasero_audio_disc_add_gap (disc, &row, 2 * GST_SECOND);
		}

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
			       GtkTreeIter *parent,
			       GSList *slices)
{
	BraseroAudioSlice *slice;
	GtkTreeModel *model;
	GtkTreeIter row;
	gchar *string;
	gint64 length;
	GSList *iter;

	gboolean title_set;
	gboolean artist_set;
	gboolean composer_set;
	gboolean isrc_set;

	gint64 start;
	gint64 end;

	gchar *uri;
	gchar *isrc;
	gchar *name;
	gchar *artist;
	gchar *composer;
	gchar *icon_string;

	if (!slices)
		return;

	/* the first slice is used for the existing row */
	slice = slices->data;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (disc->priv->tree));

	gtk_tree_model_get (model, parent,
			    NAME_COL, &name,
			    TITLE_SET_COL, &title_set,
			    ICON_COL, &icon_string,
			    ARTIST_COL, &artist,
			    ARTIST_SET_COL, &artist_set,
			    COMPOSER_COL, &composer,
			    COMPOSER_SET_COL, &composer_set,
			    ISRC_COL, &isrc,
			    ISRC_SET_COL, &isrc_set,
			    URI_COL, &uri,
			    LENGTH_COL, &length,
			    START_COL, &start,
			    END_COL, &end,
			    -1);
	disc->priv->sectors -= BRASERO_DURATION_TO_SECTORS (BRASERO_STREAM_LENGTH (start, end));

	string = brasero_units_get_time_string (BRASERO_STREAM_LENGTH (slice->start, slice->end), TRUE, FALSE); 
	gtk_list_store_set (GTK_LIST_STORE (model), parent,
			    LENGTH_SET_COL, TRUE,
			    START_COL, slice->start,
			    END_COL, slice->end,
			    SIZE_COL, string,
			    -1);
	g_free (string);
	disc->priv->sectors += BRASERO_DURATION_TO_SECTORS (BRASERO_STREAM_LENGTH (slice->start, slice->end));

	for (iter = slices->next; iter; iter = iter->next) {
		slice = iter->data;

		gtk_list_store_insert_after (GTK_LIST_STORE (model), &row, parent);
		gtk_tree_model_iter_next (model, parent);

		string = brasero_units_get_time_string (BRASERO_STREAM_LENGTH (slice->start, slice->end), TRUE, FALSE); 
		gtk_list_store_set (GTK_LIST_STORE (model), &row,
				    URI_COL, uri,
				    NAME_COL, name,
				    TITLE_SET_COL, title_set,
				    ICON_COL, icon_string,
				    ARTIST_COL, artist,
				    ARTIST_SET_COL, artist_set,
				    COMPOSER_COL, composer,
				    COMPOSER_SET_COL, composer_set,
				    ISRC_COL, isrc,
				    ISRC_SET_COL, isrc_set,
				    SONG_COL, TRUE,
				    LENGTH_SET_COL, TRUE,
				    START_COL, slice->start,
				    END_COL, slice->end,
				    SIZE_COL, string,
				    LENGTH_COL, length,
				    -1);
		g_free (string);

		disc->priv->sectors += BRASERO_DURATION_TO_SECTORS (BRASERO_STREAM_LENGTH (slice->start, slice->end));
	}

	g_free (icon_string);
	g_free (composer);
	g_free (artist);
	g_free (name);
	g_free (uri);

	brasero_disc_size_changed (BRASERO_DISC (disc), disc->priv->sectors);
}

static void
brasero_audio_disc_split (BraseroAudioDisc *disc)
{
	GtkTreeSelection *selection;
	GtkTreePath *treepath;
	GtkTreeModel *model;
	GtkTreeIter row;

	GtkResponseType response;
	GtkWidget *toplevel;
	GtkWidget *dialog;
	GList *selected;
	GSList *slices;
	gint64 start;
	gint64 end;
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

	if (!gtk_tree_model_get_iter (model, &row, treepath)) {
		gtk_tree_path_free (treepath);
		return;
	}
	gtk_tree_path_free (treepath);

	/* NOTE: this is necessarily a song since otherwise button is grey */
	gtk_tree_model_get (model, &row,
			    URI_COL, &uri,
			    START_COL, &start,
			    END_COL, &end,
			    -1);

	dialog = brasero_split_dialog_new ();
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (disc));
	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
	gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_position (GTK_WINDOW (toplevel), GTK_WIN_POS_CENTER_ON_PARENT);

	brasero_split_dialog_set_uri (BRASERO_SPLIT_DIALOG (dialog), uri);
	brasero_split_dialog_set_boundaries (BRASERO_SPLIT_DIALOG (dialog),
					     start,
					     end);
	g_free (uri);

	response = gtk_dialog_run (GTK_DIALOG (dialog));
	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy (dialog);
		return;
	}

	slices = brasero_split_dialog_get_slices (BRASERO_SPLIT_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	brasero_audio_disc_add_slices (disc, &row, slices);
	g_slist_foreach (slices, (GFunc) g_free, NULL);
	g_slist_free (slices);
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
	GtkTreeView *treeview;
	GtkTreeModel *model;
	GList *selected;
	GList *iter;

	treeview = gtk_tree_selection_get_tree_view (selection);
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
			gboolean is_song;

			gtk_tree_model_get (model, &row,
					    SONG_COL, &is_song,
					    -1);
			if (is_song) {
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
	gboolean success;
	GtkTreeIter iter;
	GList *item, *list;
	GSList *uris = NULL;
	GtkTreeModel *model;
	GtkTreePath *treepath;
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (disc->priv->tree));
	list = gtk_tree_selection_get_selected_rows (selection, &model);

	for (item = list; item; item = item->next) {
		treepath = item->data;
		success = gtk_tree_model_get_iter (model, &iter, treepath);
		gtk_tree_path_free (treepath);

		if (!success)
			continue;

		gtk_tree_model_get (model, &iter,
				    URI_COL, &uri, -1);

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
	gtk_list_store_set (GTK_LIST_STORE (model), iter,
			    NAME_COL, new_name,
			    TITLE_SET_COL, TRUE,
			    -1);
	return TRUE;
}

static void
brasero_audio_disc_edit_multi_song_properties (BraseroAudioDisc *disc,
					       GList *list)
{
	gint isrc;
	gint64 gap;
	GList *copy;
	GList *item;
	GtkWidget *props;
	GtkWidget *toplevel;
	GtkTreeModel *model;
	gchar *artist = NULL;
	GtkResponseType result;
	gchar *composer = NULL;

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
						      NAME_COL,
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
		GtkTreeIter iter;
		gboolean is_song;

		treepath = item->data;
		if (!gtk_tree_model_get_iter (model, &iter, treepath))
			continue;

		gtk_tree_model_get (model, &iter,
				    SONG_COL, &is_song,
				    -1);

		if (!is_song)
			continue;

		if (artist)
			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					    ARTIST_COL, artist,
					    ARTIST_SET_COL, TRUE,
					    -1);

		if (composer)
			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					    COMPOSER_COL, composer,
					    COMPOSER_SET_COL, TRUE,
					    -1);

		if (isrc > 0)
			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					    ISRC_COL, isrc,
					    ISRC_SET_COL, TRUE,
					    -1);

		if (gap > -1)
			brasero_audio_disc_add_gap (disc, &iter, gap);
	}
	g_list_free (copy);
	g_free (artist);
	g_free (composer);

	gtk_widget_destroy (props);
}

static void
brasero_audio_disc_edit_single_song_properties (BraseroAudioDisc *disc,
						GtkTreePath *treepath)
{
	gint64 gap;
	gint isrc;
	gint64 end;
	gint64 start;
	gint track_num;
	GtkWidget *props;
	gchar *length_str;
	GtkWidget *toplevel;
	GtkTreeModel *model;
	GtkResponseType result;
	gint64 length;
	gboolean is_song;
	gboolean success;
	gchar *title;
	gchar *artist;
	gchar *composer;
	GtkTreeIter iter;
	gchar *track_num_str;
	GtkTreeIter gap_iter;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (disc->priv->tree));
	success = gtk_tree_model_get_iter (model, &iter, treepath);
	if (!success)
		return;

	gtk_tree_model_get (model, &iter,
			    SONG_COL, &is_song,
			    -1);
	if (!is_song)
		return;

	/* get all information */
	gtk_tree_model_get (model, &iter,
			    NAME_COL, &title,
			    ARTIST_COL, &artist,
			    COMPOSER_COL, &composer,
			    TRACK_NUM_COL, &track_num_str,
			    ISRC_COL, &isrc,
			    END_COL, &end,
			    START_COL, &start,
			    LENGTH_COL, &length,
			    -1);

	if (brasero_audio_disc_has_gap (disc, &iter, &gap_iter))
		gtk_tree_model_get (model, &gap_iter,
				    LENGTH_COL, &gap,
				    -1);
	else
		gap = 0;

	if (track_num_str) {
		track_num = (gint) g_strtod (track_num_str + 6 /* (ignore markup) */, NULL);
		g_free (track_num_str);
	}
	else
		track_num = 0;

	/* set up dialog */
	props = brasero_song_props_new ();
	brasero_song_props_set_properties (BRASERO_SONG_PROPS (props),
					   track_num,
					   artist,
					   title,
					   composer,
					   isrc,
					   length,
					   start,
					   end,
					   gap);
	if (artist)
		g_free (artist);
	if (title)
		g_free (title);
	if (composer)
		g_free (composer);

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

	disc->priv->sectors -= BRASERO_DURATION_TO_SECTORS (BRASERO_STREAM_LENGTH (start, end));
	brasero_song_props_get_properties (BRASERO_SONG_PROPS (props),
					   &artist,
					   &title,
					   &composer,
					   &isrc,
					   &start,
					   &end,
					   &gap);

	length_str = brasero_units_get_time_string (BRASERO_STREAM_LENGTH (start, end), TRUE, FALSE);

	gtk_tree_model_get_iter (model, &iter, treepath);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			    LENGTH_SET_COL, TRUE,
			    START_COL, start,
			    END_COL, end,
			    SIZE_COL, length_str,
			    -1);
	g_free (length_str);

	if (title) {
		gchar *markup;

		markup = g_markup_escape_text (title, -1);
		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				    NAME_COL, markup,
				    TITLE_SET_COL, TRUE,
				    -1);
		g_free (markup);
	}

	if (artist)
		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				    ARTIST_COL, artist,
				    ARTIST_SET_COL, TRUE,
				    -1);

	if (composer)
		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				    COMPOSER_COL, composer,
				    COMPOSER_SET_COL, TRUE,
				    -1);

	if (isrc)
		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				    ISRC_COL, isrc,
				    ISRC_SET_COL, TRUE,
				    -1);

	if (end - start + BRASERO_SECTORS_TO_TIME (gap) < BRASERO_MIN_STREAM_LENGTH)
		brasero_audio_disc_short_track_dialog (disc);

	if (gap)
		brasero_audio_disc_add_gap (disc, &iter, gap);

	disc->priv->sectors += BRASERO_DURATION_TO_SECTORS (BRASERO_STREAM_LENGTH (start, end));
	brasero_audio_disc_size_changed (disc);

	g_free (title);
	g_free (artist);
	g_free (composer);
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
		GtkTreeIter iter;
		gboolean is_song;

		tmp = item->data;

		gtk_tree_model_get_iter (model, &iter, tmp);
		gtk_tree_model_get (model, &iter, 
				    SONG_COL, &is_song,
				    -1);

		if (is_song) {
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

	array = g_strsplit_set (text, "\n\r", 0);
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
							 NULL,
							 NULL);
			g_free (uri);
		}

		item++;
	}
}

static void
brasero_audio_disc_clipboard_targets_cb (GtkClipboard *clipboard,
					 GdkAtom *atoms,
					 gint n_atoms,
					 BraseroAudioDisc *disc)
{
	GdkAtom *iter;
	gchar *target;

	iter = atoms;
	while (n_atoms > 0) {
		target = gdk_atom_name (*iter);

		if (!strcmp (target, "x-special/gnome-copied-files")
		||  !strcmp (target, "UTF8_STRING")) {
			gtk_clipboard_request_text (clipboard,
						    (GtkClipboardTextReceivedFunc) brasero_audio_disc_clipboard_text_cb,
						    disc);
			g_free (target);
			return;
		}

		g_free (target);
		iter++;
		n_atoms--;
	}
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

	if (event->keyval == GDK_KP_Delete || event->keyval == GDK_Delete) {
		brasero_audio_disc_delete_selected (BRASERO_DISC (disc));
	}
	else if (event->keyval == GDK_F2)
		brasero_audio_disc_rename_activated (disc);

	return FALSE;
}

/**********************************               ******************************/
static gboolean
brasero_audio_disc_get_selected_uri (BraseroDisc *disc,
				     gchar **uri)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	BraseroAudioDisc *audio;

	audio = BRASERO_AUDIO_DISC (disc);
	if (!audio->priv->selected_path)
		return FALSE;

	if (!uri)
		return TRUE;

	/* we are asked for just one uri so return the first one */
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (audio->priv->tree));
	if (!gtk_tree_model_get_iter (model, &iter, audio->priv->selected_path)) {
		gtk_tree_path_free (audio->priv->selected_path);
		audio->priv->selected_path = NULL;
		return FALSE;
	}

	gtk_tree_model_get (model, &iter, URI_COL, uri, -1);
	return TRUE;
}

static gboolean
brasero_audio_disc_get_boundaries (BraseroDisc *disc,
				   gint64 *start,
				   gint64 *end)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	BraseroAudioDisc *audio;

	audio = BRASERO_AUDIO_DISC (disc);
	if (!audio->priv->selected_path)
		return FALSE;

	/* we are asked for just one uri so return the first one */
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (audio->priv->tree));
	if (!gtk_tree_model_get_iter (model, &iter, audio->priv->selected_path)) {
		gtk_tree_path_free (audio->priv->selected_path);
		audio->priv->selected_path = NULL;
		return FALSE;
	}

	gtk_tree_model_get (model, &iter,
			    START_COL, start,
			    END_COL, end,
			    -1);
	return TRUE;
}

/********************************** Monitoring *********************************/
#ifdef BUILD_INOTIFY

struct _BraseroMonitoredRemoveData {
	BraseroAudioDisc *disc;
	gchar *uri;
};
typedef struct _BraseroMonitoredRemoveData BraseroMonitoredRemoveData;

struct _BraseroMonitoredDir {
	gint ref;
	gchar *uri;
};
typedef struct _BraseroMonitoredDir BraseroMonitoredDir;

static void
brasero_audio_disc_inotify_free_monitored (gpointer data)
{
	BraseroMonitoredDir *dir = data;

	g_free (dir->uri);
	g_free (dir);
}

static void
brasero_audio_disc_start_monitoring (BraseroAudioDisc *disc,
				     const gchar *uri)
{
	if (!disc->priv->monitored)
		disc->priv->monitored = g_hash_table_new_full (g_direct_hash,
							       g_direct_equal,
							       NULL,
							       brasero_audio_disc_inotify_free_monitored);

	if (disc->priv->notify && !strncmp (uri, "file://", 7)) {
		BraseroMonitoredDir *dir;
		gchar *parent;
		int dev_fd;
		char *path;
		uint32_t mask;
		int wd;

		/* we want to be able to catch files being renamed in the same
		 * directory that's why we watch the parent directory instead 
		 * of the file itself. Another advantage is that it saves wds
		 * if several files are in the same directory. */
		parent = g_path_get_dirname (uri);
		dir = g_hash_table_lookup (disc->priv->monitored, parent);
		if (dir) {
			/* no need to add a watch, parent directory
			 * is already being monitored */
			g_free (parent);
			dir->ref ++;
			return;
		}

		dir = g_new0 (BraseroMonitoredDir, 1);
		dir->uri = parent;
		dir->ref = 1;

		dev_fd = g_io_channel_unix_get_fd (disc->priv->notify);
		mask = IN_MODIFY |
		       IN_ATTRIB |
		       IN_MOVED_FROM |
		       IN_MOVED_TO |
		       IN_DELETE |
		       IN_DELETE_SELF |
		       IN_MOVE_SELF |
		       IN_UNMOUNT;

	    	path = g_filename_from_uri (parent, NULL, NULL);
		wd = inotify_add_watch (dev_fd, path, mask);
		if (wd == -1) {
			g_warning ("ERROR creating watch for local file %s : %s\n",
				   parent,
				   g_strerror (errno));

			g_free (parent);
			g_free (dir);
		}
		else
			g_hash_table_insert (disc->priv->monitored,
					     GINT_TO_POINTER (wd),
					     dir);
		g_free (path);
	}
}

static gboolean
_foreach_monitored_remove_uri_cb (int wd,
				  BraseroMonitoredDir *dir,
				  BraseroMonitoredRemoveData *data)
{
	gint dev_fd;

	if (strcmp (data->uri, dir->uri))
		return FALSE;

	dir->ref --;
	if (dir->ref)
		return FALSE;

	/* we can now safely remove the watch */
	dev_fd = g_io_channel_unix_get_fd (data->disc->priv->notify);
	inotify_rm_watch (dev_fd, wd);
	return TRUE;
}

static void
brasero_audio_disc_cancel_monitoring (BraseroAudioDisc *disc,
				      const char *uri)
{
	BraseroMonitoredRemoveData callback_data;

	if (!disc->priv->notify)
		return;

	callback_data.uri = g_path_get_dirname (uri);
	callback_data.disc = disc;

	if (disc->priv->monitored)
		g_hash_table_foreach_remove (disc->priv->monitored,
					     (GHRFunc) _foreach_monitored_remove_uri_cb,
					     &callback_data);

	g_free (callback_data.uri);
}

static void
brasero_audio_disc_inotify_removal_warning (BraseroAudioDisc *disc,
					    const gchar *uri)
{
	gchar *name;
	gchar *primary;

	BRASERO_GET_BASENAME_FOR_DISPLAY (uri, name);

	/* Translators: "%s" is the name of a file here */
	primary = g_strdup_printf (_("\"%s\" was removed from the file system."), name);
	brasero_app_alert (brasero_app_get_default (),
			   primary,
			   /* Translators: This is when brasero detects that a file
			    * in the audio project was removed from its original 
			    * location (on a hard drive, USB stick, whatever) so
			    * it removes the file from the project (not from its
			    * original location) and lets the user know. The "It"
			    * refers to the file and this string is coupled with
			    * previous string:
			    * ""\"%s\" was removed from the file system."*/
			   _("It will be removed from the project"),
			   GTK_MESSAGE_WARNING);
	g_free (primary);
	g_free (name);
}

static void
brasero_audio_disc_inotify_remove_all (BraseroAudioDisc *disc,
				       BraseroMonitoredDir *dir,
				       gint32 wd)
{
	gint len;
	gint dev_fd;
	GtkTreeIter iter;
	GtkTreeModel *model;

	brasero_audio_disc_inotify_removal_warning (disc, dir->uri);

	/* it's the same as below except that we remove all children of uri */
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (disc->priv->tree));
	if (gtk_tree_model_get_iter_first (model, &iter))
		return;

	len = strlen (dir->uri);
	do {
		gint64 end;
		gint64 start;
		gchar *row_uri;

		gtk_tree_model_get (model, &iter,
				    URI_COL, &row_uri,
				    START_COL, &start,
				    END_COL, &end,
				    -1);

		if (row_uri && !strncmp (row_uri, dir->uri, len)) {
			if (end - start > 0)
				disc->priv->sectors -= BRASERO_DURATION_TO_SECTORS (BRASERO_STREAM_LENGTH (start, end));

			if (!gtk_list_store_remove (GTK_LIST_STORE (model), &iter)) {
				g_free (row_uri);
				break;
			}
		}

		g_free (row_uri);
	} while (gtk_tree_model_iter_next (model, &iter));

	brasero_audio_disc_size_changed (disc);

	/* remove the monitored directory and stop the watch */
	g_hash_table_remove (disc->priv->monitored, GINT_TO_POINTER (wd));

	dev_fd = g_io_channel_unix_get_fd (disc->priv->notify);
	inotify_rm_watch (dev_fd, wd);
}

static GSList *
brasero_audio_disc_inotify_find_rows (BraseroAudioDisc *disc,
				      const char *uri)
{
	GtkTreePath *treepath;
	GtkTreeModel *model;
	GSList *list = NULL;
	GtkTreeIter iter;
	gchar *row_uri;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (disc->priv->tree));
	if (!gtk_tree_model_get_iter_first (model, &iter))
		return NULL;

	do {
		gtk_tree_model_get (model, &iter,
				    URI_COL, &row_uri,
				    -1);

		if (row_uri && !strcmp (uri, row_uri)) {
			treepath = gtk_tree_model_get_path (model, &iter);

			/* NOTE: prepend is better here since last found will be
			 * the first in the list. That way when deleting rows the
			 * paths we delete last will be valid since they are at 
			 * the top in treeview */
			list = g_slist_prepend (list, treepath);
		}

		g_free (row_uri);
	} while (gtk_tree_model_iter_next (model, &iter));

	return list;
}

static void
brasero_audio_disc_inotify_remove (BraseroAudioDisc *disc,
				   const gchar *uri)
{
	GtkTreeModel *model;
	GSList *list,*iter_list;

	list = brasero_audio_disc_inotify_find_rows (disc, uri);
	if (!list)
		return;

	brasero_audio_disc_inotify_removal_warning (disc, uri);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (disc->priv->tree));
	for (iter_list = list; iter_list; iter_list = iter_list->next) {
		GtkTreePath *treepath;
		GtkTreeIter iter;
		gint64 start;
		gint64 end;

		treepath = iter_list->data;

		gtk_tree_model_get_iter (model, &iter, treepath);
		gtk_tree_model_get (model, &iter,
				    START_COL, &start,
				    END_COL, &end,
				    -1);

		if (end - start > 0)
			disc->priv->sectors -= BRASERO_DURATION_TO_SECTORS (BRASERO_STREAM_LENGTH (start, end));

		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
	}

	g_slist_foreach (list, (GFunc) gtk_tree_path_free, NULL);
	g_slist_free (list);

	brasero_audio_disc_size_changed (disc);
	brasero_audio_disc_cancel_monitoring (disc, uri);
}

static void
brasero_audio_disc_inotify_modify_result (GObject *object,
					  GError *error,
					  const gchar *uri,
					  GFileInfo *info,
					  gpointer callback_data)
{
	BraseroAudioDisc *disc = BRASERO_AUDIO_DISC (object);
	GSList *list, *list_iter;
	GtkTreeModel *model;

	list = brasero_audio_disc_inotify_find_rows (disc, uri);
	if (!list)
		return;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (disc->priv->tree));
	for (list_iter = list; list_iter; list_iter = list_iter->next) {
		GtkTreePath *treepath;
		GtkTreeIter iter;

		treepath = list_iter->data;
		gtk_tree_model_get_iter (model, &iter, treepath);

		if (error)
			brasero_audio_disc_remove (disc, treepath);
		else
			brasero_audio_disc_set_row_from_metadata (disc,
								  model,
								  &iter,
								  info);
	}
	g_slist_foreach (list, (GFunc) gtk_tree_path_free, NULL);
	g_slist_free (list);
}

static gboolean
brasero_audio_disc_inotify_move_timeout (BraseroAudioDisc *audio)
{
	BraseroInotifyMovedData *data;

	/* an IN_MOVED_FROM timed out. It is the first in the queue. */
	data = audio->priv->moved_list->data;
	audio->priv->moved_list = g_slist_remove (audio->priv->moved_list, data);
	brasero_audio_disc_inotify_remove (audio, data->uri);

	/* clean up */
	g_free (data->uri);
	g_free (data);
	
	return FALSE;
}

static void
brasero_audio_disc_inotify_move (BraseroAudioDisc *disc,
				 struct inotify_event *event,
				 const gchar *uri)
{
	BraseroInotifyMovedData *data = NULL;

	if (!event->cookie) {
		brasero_audio_disc_inotify_remove (disc, uri);
		return;
	}

	if (event->mask & IN_MOVED_FROM) {
		data = g_new0 (BraseroInotifyMovedData, 1);
		data->cookie = event->cookie;
		data->uri = g_strdup (uri);
			
		/* we remember this move for 5s. If 5s later we haven't received
		 * a corresponding MOVED_TO then we consider the file was
		 * removed. */
		data->id = g_timeout_add_seconds (5,
						  (GSourceFunc) brasero_audio_disc_inotify_move_timeout,
						  disc);

		/* NOTE: the order is important, we _must_ append them */
		disc->priv->moved_list = g_slist_append (disc->priv->moved_list, data);
	}
	else {
		GSList *iter;

		for (iter = disc->priv->moved_list; iter; iter = iter->next) {
			data = iter->data;
			if (data->cookie == event->cookie)
				break;

			data = NULL;
		}

		if (data) {
			GSList *paths;

			/* we've got one match:
			 * - remove from the list
			 * - remove the timeout
			 * - change all the uris with the new one */
			disc->priv->moved_list = g_slist_remove (disc->priv->moved_list, data);
			paths = brasero_audio_disc_inotify_find_rows (disc, data->uri);

			/* we are only interested if the destination is in our tree
			 * then that means the file was modified */
			if (!disc->priv->io)
				disc->priv->io = brasero_io_get_default ();

			if (!disc->priv->reload_uri)
				disc->priv->reload_uri = brasero_io_register (G_OBJECT (disc),
									      brasero_audio_disc_inotify_modify_result,
									      NULL,
									      NULL);

			brasero_io_get_file_info (disc->priv->io,
						  uri,
						  disc->priv->reload_uri,
						  BRASERO_IO_INFO_PERM|
						  BRASERO_IO_INFO_MIME|
						  BRASERO_IO_INFO_URGENT|
						  BRASERO_IO_INFO_METADATA|
						  BRASERO_IO_INFO_METADATA_MISSING_CODEC,
						  NULL);

			/* clean up the mess */
			g_slist_foreach (paths, (GFunc) gtk_tree_path_free, NULL);
			g_slist_free (paths);

			g_source_remove (data->id);
			g_free (data->uri);
			g_free (data);
		}
	}
}

static void
brasero_audio_disc_inotify_attributes_changed_cb (GObject *obj,
						  GError *error,
						  const gchar *uri,
						  GFileInfo *info,
						  gpointer null_data)
{
	gboolean readable;
	BraseroAudioDisc *disc = BRASERO_AUDIO_DISC (obj);

	if (error)
		readable = FALSE;
	else if (g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_READ))
		readable = TRUE;
	else
		readable = FALSE;

	if (!readable)
		brasero_audio_disc_inotify_remove (disc, uri);
}

static gboolean
brasero_audio_disc_inotify_attributes_changed (BraseroAudioDisc *disc,
					       const gchar *uri)
{
	if (!disc->priv->io)
		disc->priv->io = brasero_io_get_default ();

	if (!disc->priv->attr_changed)
		disc->priv->attr_changed = brasero_io_register (G_OBJECT (disc),
								brasero_audio_disc_inotify_attributes_changed_cb,
								brasero_audio_disc_vfs_operation_finished,
								NULL);

	brasero_audio_disc_increase_activity_counter (disc);
	brasero_io_get_file_info (disc->priv->io,
				  uri,
				  disc->priv->attr_changed,
				  BRASERO_IO_INFO_PERM,
				  NULL);
	return TRUE;
}

static gboolean
brasero_audio_disc_inotify_is_in_selection (BraseroAudioDisc *disc,
					    const char *uri)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (disc->priv->tree));
	if (gtk_tree_model_get_iter_first (model, &iter))
		return FALSE;

	do {
		gchar *row_uri;

		gtk_tree_model_get (model, &iter,
				    URI_COL, &row_uri,
				    -1);

		if (!strcmp (uri, row_uri)) {
			g_free (row_uri);
			return TRUE;
		}

		g_free (row_uri);
	} while (gtk_tree_model_iter_next (model, &iter));

	return FALSE;
}

static gboolean
brasero_audio_disc_inotify_monitor_cb (GIOChannel *channel,
				       GIOCondition condition,
				       BraseroAudioDisc *disc)
{
	struct inotify_event event;
	BraseroMonitoredDir *dir;
	GError *err = NULL;
	GIOStatus status;
	gchar *monitored;
	gchar *name;
	gsize size;

	while (condition & G_IO_IN) {
		monitored = NULL;

		status = g_io_channel_read_chars (channel,
						  (char *) &event,
						  sizeof (struct inotify_event),
						  &size,
						  &err);

		if (status == G_IO_STATUS_EOF)
			return TRUE;

		if (event.len) {
			name = g_new (char, event.len + 1);
			name[event.len] = '\0';
			status = g_io_channel_read_chars (channel,
							  name,
							  event.len,
							  &size,
							  &err);
			if (status != G_IO_STATUS_NORMAL) {
				g_warning ("Error reading inotify: %s\n",
					   err ? "Unknown error" : err->message);
				g_error_free (err);
				return TRUE;
			}
		}
		else
			name = NULL;

		/* look for ignored signal usually following deletion */
		if (event.mask & IN_IGNORED) {
			g_hash_table_remove (disc->priv->monitored,
					     GINT_TO_POINTER (event.wd));

			if (name)
				g_free (name);

			condition = g_io_channel_get_buffer_condition (channel);
			continue;
		}

		dir = g_hash_table_lookup (disc->priv->monitored,
					   GINT_TO_POINTER (event.wd));
		if (!dir) {
			condition = g_io_channel_get_buffer_condition (channel);
			continue;
		}

		if (dir->uri && name) {
			gchar *escaped_name;

			escaped_name = g_uri_escape_string (name,
							    G_URI_RESERVED_CHARS_ALLOWED_IN_PATH,
							    FALSE);
			monitored = g_strconcat (dir->uri, "/", name, NULL);
			g_free (escaped_name);
			g_free (name);
		}
		else
			monitored = NULL;

		/* This is a parent directory of at least
		 * one of the files in the selection */
		if (event.mask & (IN_DELETE_SELF | IN_MOVE_SELF | IN_UNMOUNT)) {
			/* The parent directory was moved or deleted so there is
			 * no other choice but to remove all the children :( */
			brasero_audio_disc_inotify_remove_all (disc, dir, event.wd);
		}
		else if (event.mask & IN_DELETE) {
			/* a child was deleted */
			brasero_audio_disc_inotify_remove (disc, monitored);
		}
		else if (event.mask & IN_MOVED_FROM) {
			/* a child was moved from the directory or renamed:
			 * wait 5s for a MOVED_TO signals that would mean
			 * it was simply renamed */
			brasero_audio_disc_inotify_move (disc, &event, monitored);
		}
		else if (event.mask & IN_MOVED_TO) {
			/* a file was either moved to this directory or it's a
			 * renaming (see above) */
			brasero_audio_disc_inotify_move (disc, &event, monitored);
		}
		else if (event.mask & IN_ATTRIB) {
			/* a file attributes were changed */
			brasero_audio_disc_inotify_attributes_changed (disc, monitored);
		}
		else if (event.mask & IN_MODIFY
		     &&  brasero_audio_disc_inotify_is_in_selection (disc, monitored)) {
			if (!disc->priv->io)
				disc->priv->io = brasero_io_get_default ();

			if (!disc->priv->reload_uri)
				disc->priv->reload_uri = brasero_io_register (G_OBJECT (disc),
									      brasero_audio_disc_inotify_modify_result,
									      NULL,
									      NULL);

			brasero_io_get_file_info (disc->priv->io,
						  monitored,
						  disc->priv->reload_uri,
						  BRASERO_IO_INFO_PERM|
						  BRASERO_IO_INFO_MIME|
						  BRASERO_IO_INFO_URGENT|
						  BRASERO_IO_INFO_METADATA|
						  BRASERO_IO_INFO_METADATA_MISSING_CODEC,
						  NULL);
		}

		if (monitored) {
			g_free (monitored);
			monitored = NULL;
		}

		condition = g_io_channel_get_buffer_condition (channel);
	}

	return TRUE;
}
#endif
