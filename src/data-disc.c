/***************************************************************************
 *            data-disc.c
 *
 *  dim nov 27 15:34:04 2005
 *  Copyright  2005  Rouquier Philippe
 *  brasero-app@wanadoo.fr
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

#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <gdk/gdkkeysyms.h>

#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtktreestore.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gtk/gtkcellrenderer.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtktreeviewcolumn.h>
#include <gtk/gtkentry.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtkcombobox.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtktooltips.h>

#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-file-info.h>

#ifdef BUILD_INOTIFY
#include "inotify.h"
#include "inotify-syscalls.h"
#endif

#include "disc.h"
#include "async-job-manager.h"
#include "data-disc.h"
#include "filtered-window.h"
#include "utils.h"

typedef enum {
	STATUS_NO_DRAG,
	STATUS_DRAGGING,
	STATUS_DRAG_DROP
} BraseroDragStatus;

struct BraseroDataDiscPrivate {
	GtkWidget *tree;
	GtkTreeModel *model;
	GtkTreeModel *sort;
	GtkTooltips *tooltip;
	GtkWidget *filter_dialog;
	GtkWidget *filter_button;
	GtkWidget *notebook;

	GtkUIManager *manager;

	BraseroDragStatus drag_status;
	GtkTreePath *drag_source;
	int press_start_x;
	int press_start_y;
	gint scroll_timeout;
	gint expand_timeout;

	int activity_counter;

	gint64 sectors;
	GSList *rescan;
	GSList *loading;

	GtkCellRenderer *editable_cell;
	GtkTreePath *selected_path;

#ifdef BUILD_INOTIFY

	int notify_id;
	GIOChannel *notify;
	GHashTable *monitored;
	GSList *moved_list;

	int inotify_type;

#endif

	BraseroAsyncJobManager *jobs;
	int file_type;
	int load_type;
	int expose_type;
	int check_graft;
	int dir_contents_type;

	GHashTable *dirs;
	GHashTable *files;
	GHashTable *paths;
	GHashTable *grafts;
	GHashTable *excluded;
	GHashTable *symlinks;
	GHashTable *unreadable;
	GHashTable *joliet_non_compliant;

	GHashTable *path_refs;

	GMutex *references_lock;
	GHashTable *references;

	GMutex *restored_lock;
	GHashTable *restored;

	GSList *expose;
	gint expose_id;

	GSList *exposing;

	int editing:1;
	int is_loading:1;
	int reject_files:1;
};

typedef enum {
	ROW_BOGUS,
	ROW_NEW,
	ROW_NOT_EXPLORED,
	ROW_EXPLORING,
	ROW_EXPLORED,
	ROW_EXPANDED
} BraseroRowStatus;

enum {
	ICON_COL,
	NAME_COL,
	SIZE_COL,
	MIME_COL,
	DSIZE_COL,
	ROW_STATUS_COL,
	ISDIR_COL,
	MARKUP_COL,
	NB_COL
};


struct _BraseroLoadDirError {
	gchar *uri;
	BraseroFilterStatus status;
};
typedef struct _BraseroLoadDirError BraseroLoadDirError;

struct _BraseroDirectoryContentsData {
	gchar *uri;
	GSList *infos;
	GSList *errors;
	gint cancel:1;
};
typedef struct _BraseroDirectoryContentsData BraseroDirectoryContentsData;

static void brasero_data_disc_class_init (BraseroDataDiscClass *klass);
static void brasero_data_disc_init (BraseroDataDisc *sp);
static void brasero_data_disc_finalize (GObject *object);
static void brasero_data_disc_iface_disc_init (BraseroDiscIface *iface);
static void brasero_data_disc_get_property (GObject * object,
					    guint prop_id,
					    GValue * value,
					    GParamSpec * pspec);
static void brasero_data_disc_set_property (GObject * object,
					    guint prop_id,
					    const GValue * value,
					    GParamSpec * spec);
#ifdef BUILD_INOTIFY

typedef union {
	gint wd;
	GnomeVFSMonitorHandle *hvfs;
} BraseroMonitorHandle;

struct _BraseroFile {
	gchar *uri;
	gint64 sectors;
	gint references;
	BraseroMonitorHandle handle;
};
typedef struct _BraseroFile BraseroFile;

struct _BraseroInotifyMovedData {
	gchar *uri;
	guint32 cookie;
	gint id;
};
typedef struct _BraseroInotifyMovedData BraseroInotifyMovedData;

static BraseroMonitorHandle
brasero_data_disc_start_monitoring (BraseroDataDisc *disc,
				    BraseroFile *file);
static gboolean
brasero_data_disc_cancel_monitoring (BraseroDataDisc *disc,
				     BraseroFile *file);
static gboolean
brasero_data_disc_inotify_monitor_cb (GIOChannel *channel,
				      GIOCondition condition,
				      BraseroDataDisc *disc);
#else

struct _BraseroFile {
	gchar *uri;
	gint64 sectors;
};
typedef struct _BraseroFile BraseroFile;

#endif /* BUILD_INOTIFY */

static BraseroDiscResult
brasero_data_disc_add_uri (BraseroDisc *disc, const char *uri);

static void
brasero_data_disc_delete_selected (BraseroDisc *disc);

static void
brasero_data_disc_clear (BraseroDisc *disc);
static void
brasero_data_disc_reset (BraseroDisc *disc);

static BraseroDiscResult
brasero_data_disc_load_track (BraseroDisc *disc,
			      BraseroDiscTrack *track);
static BraseroDiscResult
brasero_data_disc_get_track (BraseroDisc *disc,
			     BraseroDiscTrack *track);
static BraseroDiscResult
brasero_data_disc_get_track_source (BraseroDisc *disc,
				    BraseroTrackSource **source,
				    BraseroImageFormat format);
static BraseroDiscResult
brasero_data_disc_get_status (BraseroDisc *disc);

static gboolean
brasero_data_disc_button_pressed_cb (GtkTreeView *tree,
				     GdkEventButton *event,
				     BraseroDataDisc *disc);
static gboolean
brasero_data_disc_button_released_cb (GtkTreeView *tree,
				     GdkEventButton *event,
				     BraseroDataDisc *disc);
static gboolean
brasero_data_disc_key_released_cb (GtkTreeView *tree,
				   GdkEventKey *event,
				   BraseroDataDisc *disc);

static void
brasero_data_disc_name_edited_cb (GtkCellRendererText *cellrenderertext,
				  gchar *path_string,
				  gchar *text,
				  BraseroDataDisc *disc);
static void
brasero_data_disc_name_editing_started_cb (GtkCellRenderer *renderer,
					   GtkCellEditable *editable,
					   gchar *path,
					   BraseroDataDisc *disc);
static void
brasero_data_disc_name_editing_canceled_cb (GtkCellRenderer *renderer,
					    BraseroDataDisc *disc);

static gboolean
brasero_data_disc_drag_motion_cb(GtkWidget *tree,
				 GdkDragContext *drag_context,
				 gint x,
				 gint y,
				 guint time,
				 BraseroDataDisc *disc);
void
brasero_data_disc_drag_leave_cb (GtkWidget *tree,
				 GdkDragContext *drag_context,
				 guint time,
				 BraseroDataDisc *disc);
static gboolean
brasero_data_disc_drag_drop_cb (GtkTreeView *tree,
				GdkDragContext *drag_context,
				gint x,
				gint y,
				guint time,
				BraseroDataDisc *disc);
static void
brasero_data_disc_drag_data_received_cb (GtkTreeView *tree,
					 GdkDragContext *drag_context,
					 gint x,
					 gint y,
					 GtkSelectionData *selection_data,
					 guint info,
					 guint time,
					 BraseroDataDisc *disc);
static void
brasero_data_disc_drag_begin_cb (GtkTreeView *tree,
				 GdkDragContext *drag_context,
				 BraseroDataDisc *disc);
static void
brasero_data_disc_drag_get_cb (GtkWidget *tree,
                               GdkDragContext *context,
                               GtkSelectionData *selection_data,
                               guint info,
                               guint time,
			       BraseroDataDisc *disc);
static void
brasero_data_disc_drag_end_cb (GtkWidget *tree,
			       GdkDragContext *drag_context,
			       BraseroDataDisc *disc);

static void
brasero_data_disc_row_collapsed_cb (GtkTreeView *tree,
				     GtkTreeIter *sortparent,
				     GtkTreePath *sortpath,
				     BraseroDataDisc *disc);

static void
brasero_data_disc_new_folder_clicked_cb (GtkButton *button,
					 BraseroDataDisc *disc);
static void
brasero_data_disc_filtered_files_clicked_cb (GtkButton *button,
					     BraseroDataDisc *disc);

static void
brasero_data_disc_open_activated_cb (GtkAction *action,
				     BraseroDataDisc *disc);
static void
brasero_data_disc_rename_activated_cb (GtkAction *action,
				       BraseroDataDisc *disc);
static void
brasero_data_disc_delete_activated_cb (GtkAction *action,
				       BraseroDataDisc *disc);
static void
brasero_data_disc_paste_activated_cb (GtkAction *action,
				      BraseroDataDisc *disc);

static void
brasero_data_disc_clean (BraseroDataDisc *disc);
static void
brasero_data_disc_reset_real (BraseroDataDisc *disc);

static gchar *
brasero_data_disc_path_to_uri (BraseroDataDisc *disc,
			       const gchar *path);
static GSList *
brasero_data_disc_uri_to_paths (BraseroDataDisc *disc,
				const gchar *uri,
				gboolean include_grafts);
static GSList *
brasero_data_disc_path_find_children_grafts (BraseroDataDisc *disc,
					     const gchar *path);

static char *
brasero_data_disc_graft_get (BraseroDataDisc *disc,
			     const char *path);
static void
brasero_data_disc_graft_remove_all (BraseroDataDisc *disc,
				    const char *uri);
static void
brasero_data_disc_graft_children_remove (BraseroDataDisc *disc,
					 GSList *paths);

static void
brasero_data_disc_remove_uri (BraseroDataDisc *disc,
			      const gchar *uri,
			      gboolean include_grafted);
static void
brasero_data_disc_restore_excluded_children (BraseroDataDisc *disc,
					     BraseroFile *dir);
static void
brasero_data_disc_replace_symlink_children (BraseroDataDisc *disc,
					    BraseroFile *dir,
					    GSList *grafts);
static void
brasero_data_disc_exclude_uri (BraseroDataDisc *disc,
			       const char *path,
			       const char *uri);
static gboolean
brasero_data_disc_is_excluded (BraseroDataDisc *disc,
			       const char *uri,
			       BraseroFile *top);

static void
brasero_data_disc_load_dir_error (BraseroDataDisc *disc, GSList *errors);

static BraseroDiscResult
brasero_data_disc_expose_path (BraseroDataDisc *disc,
			       const char *path);
static void
brasero_data_disc_directory_priority (BraseroDataDisc *disc,
				      BraseroFile *file);
static BraseroDiscResult
brasero_data_disc_directory_load (BraseroDataDisc *disc,
				  BraseroFile *dir,
				  gboolean append);
static BraseroFile *
brasero_data_disc_directory_new (BraseroDataDisc *disc,
				 char *uri,
				 gboolean append);

static void
brasero_data_disc_unreadable_new (BraseroDataDisc *disc,
				  char *uri,
				  BraseroFilterStatus status);

static char *
brasero_data_disc_get_selected_uri (BraseroDisc *disc);

static gchar *BRASERO_CREATED_DIR = "created";

#define BRASERO_ADD_TO_EXPOSE_QUEUE(disc, data)	\
	disc->priv->expose = g_slist_append (disc->priv->expose, data);	\
	if (!disc->priv->expose_id)	\
		disc->priv->expose_id = g_idle_add ((GSourceFunc) brasero_data_disc_expose_path_real,	\
						    disc);

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

enum {
	PROP_NONE,
	PROP_REJECT_FILE,
};

static GObjectClass *parent_class = NULL;

static GtkActionEntry entries [] = {
	{"ContextualMenu", NULL, N_("Menu")},
	{"Open", GTK_STOCK_OPEN, N_("Open"), NULL, NULL,
	 G_CALLBACK (brasero_data_disc_open_activated_cb)},
	{"Rename", NULL, N_("Rename"), NULL, NULL,
	 G_CALLBACK (brasero_data_disc_rename_activated_cb)},
	{"Delete", GTK_STOCK_REMOVE, N_("Remove"), NULL, NULL,
	 G_CALLBACK (brasero_data_disc_delete_activated_cb)},
	{"Paste", GTK_STOCK_PASTE, N_("Paste"), NULL, NULL,
	 G_CALLBACK (brasero_data_disc_paste_activated_cb)},
};

static const gchar *menu_description = {
	"<ui>"
	"<popup action='ContextMenu'>"
		"<menuitem action='Open'/>"
		"<menuitem action='Delete'/>"
		"<menuitem action='Rename'/>"
		"<separator/>"
		"<menuitem action='Paste'/>"
	"</popup>"
	"</ui>"
};

/* Like mkisofs we count in sectors (= 2048 bytes). So we need to divide the 
 * size of each file by 2048 and if it is not a multiple we add one sector. That
 * means that on the CD/DVD a file might occupy more space than its real space
 * since it will need nb_sectors * 2048. */
#define GET_SIZE_IN_SECTORS(size) (size % 2048 ? size / 2048 + 1 : size / 2048)

#define BRASERO_NAME_IS_JOLIET_COMPATIBLE(name)	0

GType
brasero_data_disc_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroDataDiscClass),
			NULL,
			NULL,
			(GClassInitFunc) brasero_data_disc_class_init,
			NULL,
			NULL,
			sizeof (BraseroDataDisc),
			0,
			(GInstanceInitFunc) brasero_data_disc_init,
		};

		static const GInterfaceInfo disc_info =
		{
			(GInterfaceInitFunc) brasero_data_disc_iface_disc_init,
			NULL,
			NULL
		};

		type = g_type_register_static (GTK_TYPE_VBOX, 
					       "BraseroDataDisc",
					       &our_info, 0);

		g_type_add_interface_static (type,
					     BRASERO_TYPE_DISC,
					     &disc_info);
	}

	return type;
}

static void
brasero_data_disc_class_init (BraseroDataDiscClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_data_disc_finalize;
	object_class->set_property = brasero_data_disc_set_property;
	object_class->get_property = brasero_data_disc_get_property;

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
brasero_data_disc_iface_disc_init (BraseroDiscIface *iface)
{
	iface->add_uri = brasero_data_disc_add_uri;
	iface->delete_selected = brasero_data_disc_delete_selected;
	iface->clear = brasero_data_disc_clear;
	iface->reset = brasero_data_disc_reset;
	iface->get_track = brasero_data_disc_get_track;
	iface->get_track_source = brasero_data_disc_get_track_source;
	iface->load_track = brasero_data_disc_load_track;
	iface->get_status = brasero_data_disc_get_status;
	iface->get_selected_uri = brasero_data_disc_get_selected_uri;
}

static void brasero_data_disc_get_property (GObject * object,
					    guint prop_id,
					    GValue * value,
					    GParamSpec * pspec)
{
	BraseroDataDisc *disc;

	disc = BRASERO_DATA_DISC (object);

	switch (prop_id) {
	case PROP_REJECT_FILE:
		g_value_set_boolean (value, disc->priv->reject_files);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void brasero_data_disc_set_property (GObject * object,
					    guint prop_id,
					    const GValue * value,
					    GParamSpec * pspec)
{
	BraseroDataDisc *disc;

	disc = BRASERO_DATA_DISC (object);

	switch (prop_id) {
	case PROP_REJECT_FILE:
		disc->priv->reject_files = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static int
brasero_data_disc_sort_default (GtkTreeModel *model,
				GtkTreeIter *a,
				GtkTreeIter *b,
				BraseroDataDisc *disc)
{
	GtkTreePath *patha, *pathb;
	gboolean isdira, isdirb;
	int retval;

	gtk_tree_model_get (model, a,
			    ISDIR_COL, &isdira, -1);
	gtk_tree_model_get (model, b,
			    ISDIR_COL, &isdirb, -1);

	if (isdira && !isdirb)
		return -1;
	else if (!isdira && isdirb)
		return 1;

	patha = gtk_tree_model_get_path(model, a);
	pathb = gtk_tree_model_get_path(model, b);

	retval = gtk_tree_path_compare(patha, pathb);
	gtk_tree_path_free(patha);
	gtk_tree_path_free(pathb);

	return retval;
}

static int
brasero_data_disc_sort_size (GtkTreeModel *model,
			     GtkTreeIter *a,
			     GtkTreeIter *b,
			     gpointer data)
{
	gboolean isdira, isdirb;
	gint64 sizea, sizeb;

	gtk_tree_model_get (model, a,
			    ISDIR_COL, &isdira, -1);
	gtk_tree_model_get (model, b,
			    ISDIR_COL, &isdirb, -1);

	if (isdira && !isdirb)
		return -1;
	else if (!isdira && isdirb)
		return 1;

	if (isdira) {
		int nba, nbb;

		nba = gtk_tree_model_iter_n_children (model, a);
		nbb = gtk_tree_model_iter_n_children (model, b);
		return nbb - nba;
	}

	gtk_tree_model_get (model, a,
			    DSIZE_COL,
			    &sizea, -1);
	gtk_tree_model_get (model, b,
			    DSIZE_COL,
			    &sizeb, -1);
	return sizeb - sizea;
}

static int
brasero_data_disc_sort_string (GtkTreeModel *model,
			       GtkTreeIter *a,
			       GtkTreeIter *b,
			       int column)
{
	gboolean isdira, isdirb;
	char *stringa, *stringb;
	GtkSortType order;
	int retval;

	gtk_tree_sortable_get_sort_column_id (GTK_TREE_SORTABLE (model),
					      NULL, &order);

	gtk_tree_model_get (model, a,
			    ISDIR_COL, &isdira, -1);
	gtk_tree_model_get (model, b,
			    ISDIR_COL, &isdirb, -1);

	if (isdira && !isdirb)
		return -1;
	else if (!isdira && isdirb)
		return 1;

	gtk_tree_model_get (model, a, column, &stringa, -1);
	gtk_tree_model_get (model, b, column, &stringb, -1);

	if(stringa && !stringb) {
		g_free(stringa);
		return -1;
	}
	else if(!stringa && stringb) {
		g_free(stringb);
		return 1;
	}
	else if(!stringa && !stringb)
		return 0;
	
	retval = strcmp (stringa, stringb);
	g_free (stringa);
	g_free (stringb);

	return retval;
}

static int
brasero_data_disc_sort_display (GtkTreeModel *model,
				GtkTreeIter *a,
				GtkTreeIter *b,
				gpointer data)
{
	return brasero_data_disc_sort_string(model, a, b, NAME_COL);
}

static int
brasero_data_disc_sort_description (GtkTreeModel *model,
				    GtkTreeIter *a,
				    GtkTreeIter *b,
				    gpointer data)
{
	return brasero_data_disc_sort_string(model, a, b, MIME_COL);
}

static void
brasero_data_disc_build_context_menu (BraseroDataDisc *disc)
{
	GtkActionGroup *action_group;
	GError *error = NULL;

	action_group = gtk_action_group_new ("MenuAction");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (action_group,
				      entries,
				      G_N_ELEMENTS (entries),
				      disc);

	disc->priv->manager = gtk_ui_manager_new ();
	gtk_ui_manager_insert_action_group (disc->priv->manager,
					    action_group,
					    0);

	if (!gtk_ui_manager_add_ui_from_string (disc->priv->manager,
						menu_description,
						-1,
						&error)) {
		g_message ("building menus failed: %s", error->message);
		g_error_free (error);
	}
}

static void
brasero_data_disc_init (BraseroDataDisc *obj)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeModel *model;
	GtkWidget *scroll;
	GtkWidget *button;
	GtkWidget *hbox;

	obj->priv = g_new0 (BraseroDataDiscPrivate, 1);
	gtk_box_set_spacing (GTK_BOX (obj), 6);

	obj->priv->tooltip = gtk_tooltips_new ();

	/* the information displayed about how to use this tree */
	obj->priv->notebook = brasero_utils_get_use_info_notebook ();
	gtk_box_pack_start (GTK_BOX (obj), obj->priv->notebook, TRUE, TRUE, 0);

	/* Tree */
	obj->priv->tree = gtk_tree_view_new ();
	gtk_widget_show (obj->priv->tree);
	g_signal_connect (G_OBJECT (obj->priv->tree),
			  "button-press-event",
			  G_CALLBACK (brasero_data_disc_button_pressed_cb),
			  obj);
	g_signal_connect (G_OBJECT (obj->priv->tree),
			  "button-release-event",
			  G_CALLBACK (brasero_data_disc_button_released_cb),
			  obj);
	g_signal_connect (G_OBJECT (obj->priv->tree),
			  "key-release-event",
			  G_CALLBACK (brasero_data_disc_key_released_cb),
			  obj);

	gtk_tree_selection_set_mode (gtk_tree_view_get_selection
				     (GTK_TREE_VIEW (obj->priv->tree)),
				     GTK_SELECTION_MULTIPLE);
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (obj->priv->tree),
				      TRUE);

	model = (GtkTreeModel*) gtk_tree_store_new (NB_COL,
						    GDK_TYPE_PIXBUF,
						    G_TYPE_STRING,
						    G_TYPE_STRING,
						    G_TYPE_STRING,
						    G_TYPE_INT64,
						    G_TYPE_INT,
						    G_TYPE_BOOLEAN,
						    PANGO_TYPE_STYLE);

	obj->priv->model = GTK_TREE_MODEL (model);

	model = gtk_tree_model_sort_new_with_model (model);
	g_object_unref (obj->priv->model);

	gtk_tree_view_set_model (GTK_TREE_VIEW (obj->priv->tree),
				 GTK_TREE_MODEL (model));
	obj->priv->sort = model;
	g_object_unref (G_OBJECT (model));

	column = gtk_tree_view_column_new ();

	gtk_tree_view_column_set_resizable (column, TRUE);
//	gtk_tree_view_column_set_min_width (column, 128);

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "pixbuf", ICON_COL);

	renderer = gtk_cell_renderer_text_new ();
	obj->priv->editable_cell = renderer;
	g_signal_connect (G_OBJECT (renderer), "edited",
			  G_CALLBACK (brasero_data_disc_name_edited_cb), obj);
	g_signal_connect (G_OBJECT (renderer), "editing-started",
			  G_CALLBACK (brasero_data_disc_name_editing_started_cb), obj);
	g_signal_connect (G_OBJECT (renderer), "editing-canceled",
			  G_CALLBACK (brasero_data_disc_name_editing_canceled_cb), obj);

	gtk_tree_view_column_pack_end (column, renderer, TRUE);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "text", NAME_COL);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "style", MARKUP_COL);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "editable", ROW_STATUS_COL);

	g_object_set (G_OBJECT (renderer),
		      "ellipsize-set", TRUE,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      NULL);

	gtk_tree_view_column_set_title (column, _("Files"));
	g_object_set (G_OBJECT (column),
		      "expand", TRUE,
		      "spacing", 4,
		      NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (obj->priv->tree), column);
	gtk_tree_view_column_set_sort_column_id (column, NAME_COL);
	gtk_tree_view_set_expander_column (GTK_TREE_VIEW (obj->priv->tree),
					   column);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Size"),
							   renderer, "text",
							   SIZE_COL,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (obj->priv->tree),
				     column);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_sort_column_id (column,
						 SIZE_COL);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Description"),
							   renderer, "text",
							   MIME_COL,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (obj->priv->tree),
				     column);
	gtk_tree_view_column_set_sort_column_id (column,
						 MIME_COL);

	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (obj->priv->sort),
					 NAME_COL,
					 brasero_data_disc_sort_display,
					 NULL, NULL);
	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (obj->priv->sort),
					 SIZE_COL,
					 brasero_data_disc_sort_size,
					 NULL, NULL);
	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (obj->priv->sort),
					 MIME_COL,
					 brasero_data_disc_sort_description,
					 NULL, NULL);

	gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE(obj->priv->sort),
						 (GtkTreeIterCompareFunc) brasero_data_disc_sort_default,
						 obj, NULL);
	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll),
					     GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scroll), obj->priv->tree);
	gtk_notebook_append_page (GTK_NOTEBOOK (obj->priv->notebook), scroll, NULL);

	/* dnd */
	gtk_tree_view_enable_model_drag_dest (GTK_TREE_VIEW
					      (obj->priv->tree),
					      ntables_cd, nb_targets_cd,
					      GDK_ACTION_COPY |
					      GDK_ACTION_MOVE);

	g_signal_connect (G_OBJECT (obj->priv->tree), "drag_motion",
			  G_CALLBACK (brasero_data_disc_drag_motion_cb),
			  obj);
	g_signal_connect (G_OBJECT (obj->priv->tree), "drag_leave",
			  G_CALLBACK (brasero_data_disc_drag_leave_cb),
			  obj);
	g_signal_connect (G_OBJECT (obj->priv->tree), "drag_drop",
			  G_CALLBACK (brasero_data_disc_drag_drop_cb),
			  obj);
	g_signal_connect (G_OBJECT (obj->priv->tree), "drag_data_received",
			  G_CALLBACK (brasero_data_disc_drag_data_received_cb),
			  obj);

	gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (obj->priv->tree),
						GDK_BUTTON1_MASK,
						ntables_source,
						nb_targets_source,
						GDK_ACTION_COPY |
						GDK_ACTION_MOVE);
	g_signal_connect (G_OBJECT (obj->priv->tree), "drag_begin",
			  G_CALLBACK (brasero_data_disc_drag_begin_cb),
			  obj);
	g_signal_connect (G_OBJECT (obj->priv->tree), "drag_data_get",
			  G_CALLBACK (brasero_data_disc_drag_get_cb),
			  obj);
	g_signal_connect (G_OBJECT (obj->priv->tree), "drag_end",
			  G_CALLBACK (brasero_data_disc_drag_end_cb),
			  obj);

	g_signal_connect (G_OBJECT (obj->priv->tree),
			  "row-expanded",
			  G_CALLBACK (brasero_data_disc_row_collapsed_cb),
			  obj);

	brasero_data_disc_build_context_menu (obj);

	/* new folder button */
	hbox = gtk_hbox_new (FALSE, 10);

	button = brasero_utils_make_button (_("New folder"),
					    GTK_STOCK_DIRECTORY);
	g_signal_connect (G_OBJECT (button),
			  "clicked",
			  G_CALLBACK (brasero_data_disc_new_folder_clicked_cb),
			  obj);
	gtk_tooltips_set_tip (obj->priv->tooltip,
			      button,
			      _("Create a new empty folder"),
			      NULL);
	gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);

	obj->priv->filter_button = gtk_button_new_with_label (_("Filtered files"));
	gtk_widget_set_sensitive (obj->priv->filter_button, FALSE);
	g_signal_connect (G_OBJECT (obj->priv->filter_button),
			  "clicked",
			  G_CALLBACK (brasero_data_disc_filtered_files_clicked_cb),
			  obj);
	gtk_box_pack_start (GTK_BOX (hbox),
			    obj->priv->filter_button,
			    FALSE,
			    FALSE,
			    0);

	gtk_tooltips_set_tip (obj->priv->tooltip,
			      obj->priv->filter_button,
			      _("Some files were removed from the project. Clik here to see them."),
			      NULL);

	gtk_box_pack_start (GTK_BOX (obj), hbox, FALSE, FALSE, 0);

	/* useful things for directory exploration */
	obj->priv->dirs = g_hash_table_new (g_str_hash, g_str_equal);
	obj->priv->files = g_hash_table_new (g_str_hash, g_str_equal);
	obj->priv->grafts = g_hash_table_new (g_str_hash, g_str_equal);
	obj->priv->paths = g_hash_table_new (g_str_hash, g_str_equal);

	obj->priv->restored_lock = g_mutex_new ();
	obj->priv->references_lock = g_mutex_new ();


#ifdef BUILD_INOTIFY
	int fd;

	obj->priv->monitored = g_hash_table_new (g_direct_hash, g_direct_equal);

	/* start inotify monitoring backend */
	fd = inotify_init ();
	if (fd != -1) {
		obj->priv->notify = g_io_channel_unix_new (fd);
		g_io_channel_set_encoding (obj->priv->notify, NULL, NULL);
		g_io_channel_set_close_on_unref (obj->priv->notify, TRUE);
		obj->priv->notify_id = g_io_add_watch (obj->priv->notify,
						       G_IO_IN | G_IO_HUP | G_IO_PRI,
						       (GIOFunc) brasero_data_disc_inotify_monitor_cb,
						       obj);
		g_io_channel_unref (obj->priv->notify);
	}
	else
		g_warning ("Failed to open inotify: %s\n",
			   strerror (errno));
#endif
}

static void
brasero_data_disc_finalize (GObject *object)
{
	BraseroDataDisc *cobj;
	cobj = BRASERO_DATA_DISC(object);

	brasero_data_disc_clean (cobj);

	if (cobj->priv->jobs) {
		brasero_async_job_manager_unregister_type (cobj->priv->jobs,
							   cobj->priv->file_type);
		brasero_async_job_manager_unregister_type (cobj->priv->jobs,
							   cobj->priv->load_type);
		brasero_async_job_manager_unregister_type (cobj->priv->jobs,
							   cobj->priv->expose_type);
		brasero_async_job_manager_unregister_type (cobj->priv->jobs,
							   cobj->priv->check_graft);
		brasero_async_job_manager_unregister_type (cobj->priv->jobs,
							   cobj->priv->dir_contents_type);
		g_object_unref (cobj->priv->jobs);
		cobj->priv->jobs = NULL;
	}

#ifdef BUILD_INOTIFY

	if (cobj->priv->notify_id)
		g_source_remove (cobj->priv->notify_id);
	g_hash_table_destroy (cobj->priv->monitored);

#endif

	if (cobj->priv->scroll_timeout) {
		g_source_remove (cobj->priv->scroll_timeout);
		cobj->priv->scroll_timeout = 0;
	}

	if (cobj->priv->expand_timeout) {
		g_source_remove (cobj->priv->expand_timeout);
		cobj->priv->expand_timeout = 0;
	}

	g_mutex_free (cobj->priv->references_lock);
	g_mutex_free (cobj->priv->restored_lock);

	g_hash_table_destroy (cobj->priv->grafts);
	g_hash_table_destroy (cobj->priv->paths);
	g_hash_table_destroy (cobj->priv->dirs);
	g_hash_table_destroy (cobj->priv->files);

	if (cobj->priv->tooltip)
		g_object_ref_sink (GTK_OBJECT (cobj->priv->tooltip));

	if (cobj->priv->path_refs)
		g_hash_table_destroy (cobj->priv->path_refs);

	g_object_unref (cobj->priv->manager);

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkWidget *
brasero_data_disc_new ()
{
	BraseroDataDisc *obj;
	
	obj = BRASERO_DATA_DISC (g_object_new (BRASERO_TYPE_DATA_DISC, NULL));
	
	return GTK_WIDGET (obj);
}

/*************************** activity ******************************************/
static void
brasero_data_disc_increase_activity_counter (BraseroDataDisc *disc)
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
brasero_data_disc_decrease_activity_counter (BraseroDataDisc *disc)
{
	if (disc->priv->activity_counter == 1 && GTK_WIDGET (disc)->window)
		gdk_window_set_cursor (GTK_WIDGET (disc)->window, NULL);

	disc->priv->activity_counter--;
}

static BraseroDiscResult
brasero_data_disc_get_status (BraseroDisc *disc)
{
	if (BRASERO_DATA_DISC (disc)->priv->activity_counter)
		return BRASERO_DISC_NOT_READY;

	return BRASERO_DISC_OK;
}

/**************************** burn button **************************************/
static void
brasero_data_disc_selection_changed (BraseroDataDisc *disc, gboolean notempty)
{
	brasero_disc_contents_changed (BRASERO_DISC (disc), notempty);
}

/*************************** GtkTreeView functions *****************************/
static void
brasero_data_disc_name_exist_dialog (BraseroDataDisc *disc,
				     const char *name)
{
	GtkWidget *dialog;
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (disc));
	dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					 GTK_DIALOG_DESTROY_WITH_PARENT |
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_WARNING,
					 GTK_BUTTONS_CLOSE,
					 _("\"%s\" already exists in the directory:"),
					 name);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Already existing file"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  _("it won't be added."));

	gtk_widget_show_all (dialog);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static BraseroDiscResult
brasero_data_disc_tree_check_name_validity (BraseroDataDisc *disc,
					    const gchar *name,
					    GtkTreePath *treepath,
					    gboolean usedialog)
{
	char *row_name;
	GtkTreeIter iter;
	GtkTreeIter child;
	GtkTreeModel *model;

	model = disc->priv->model;
	if (!treepath || gtk_tree_path_get_depth (treepath) < 1) {
		if (!gtk_tree_model_get_iter_first (model, &child))
			return BRASERO_DISC_OK;
	}
	else {
		if (!gtk_tree_model_get_iter (model, &iter, treepath))
			return BRASERO_DISC_OK;

		if (!gtk_tree_model_iter_children (model, &child, &iter))
			return BRASERO_DISC_OK;
	}

	do {
		gtk_tree_model_get (model, &child,
				    NAME_COL, &row_name,
				    -1);

		if (!row_name)
			continue;

		if (!strcmp (name, row_name)) {
			
			if (usedialog)
				brasero_data_disc_name_exist_dialog (disc, name);

			g_free (row_name);
			return BRASERO_DISC_ERROR_ALREADY_IN_TREE;
		}

		g_free (row_name);
	} while (gtk_tree_model_iter_next (model, &child));
	
	return BRASERO_DISC_OK;
}

static void
brasero_data_disc_tree_update_directory_real (BraseroDataDisc *disc,
					      GtkTreeIter *iter)
{
	char *nb_items_string;
	GtkTreeModel *model;
	int nb_items;

	model = disc->priv->model;

	nb_items = gtk_tree_model_iter_n_children (model, iter);
	if (nb_items == 0) {
		GtkTreeIter child;

		nb_items_string = g_strdup (_("empty"));
		gtk_tree_store_prepend (GTK_TREE_STORE (model), &child, iter);
		gtk_tree_store_set (GTK_TREE_STORE (model), &child,
				    NAME_COL, _("(empty)"),
				    MARKUP_COL, PANGO_STYLE_ITALIC,
				    ROW_STATUS_COL, ROW_BOGUS, -1);
	}
	else if (nb_items == 1) {
		int status;
		GtkTreeIter child;

		gtk_tree_model_iter_children (model, &child, iter);
		gtk_tree_model_get (model, &child,
				    ROW_STATUS_COL, &status, -1);

		if (status == ROW_BOGUS)
			nb_items_string = g_strdup (_("empty"));
		else
			nb_items_string = g_strdup (_("1 item"));
	}
	else {
		int status;
		GtkTreeIter child;

		gtk_tree_model_iter_children (model, &child, iter);
		gtk_tree_model_get (model, &child,
				    ROW_STATUS_COL, &status, -1);

		if (status == ROW_BOGUS) {
			gtk_tree_store_remove (GTK_TREE_STORE (model), &child);
			nb_items --;
		}

		nb_items_string = g_strdup_printf (ngettext ("%d item", "%d items", nb_items), nb_items);
	}

	gtk_tree_store_set (GTK_TREE_STORE (disc->priv->model), iter,
			    SIZE_COL, nb_items_string,
			    -1);

	g_free (nb_items_string);
}

static void
brasero_data_disc_tree_update_directory (BraseroDataDisc *disc,
					 const GtkTreePath *path)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = disc->priv->model;
	gtk_tree_model_get_iter (model, &iter, (GtkTreePath *) path);
	brasero_data_disc_tree_update_directory_real (disc, &iter);
}

static void
brasero_data_disc_tree_update_parent (BraseroDataDisc *disc,
				     const GtkTreePath *path)
{
	GtkTreePath *parent_path;

	parent_path = gtk_tree_path_copy (path);
	gtk_tree_path_up (parent_path);

	if (gtk_tree_path_get_depth (parent_path) < 1) {
		gtk_tree_path_free (parent_path);
		return;
	}

	brasero_data_disc_tree_update_directory (disc, parent_path);
	gtk_tree_path_free (parent_path);
}

static gboolean
brasero_data_disc_tree_path_to_disc_path (BraseroDataDisc *disc,
					  GtkTreePath *treepath,
					  char **discpath)
{
	int i;
	char *name;
	GString *path;
	GtkTreeIter row;
	GtkTreePath *iter;
	GtkTreeModel *model;
	gint *indices, depth;

	if (treepath == NULL
	||  gtk_tree_path_get_depth (treepath) < 1) {
		*discpath = g_strdup (G_DIR_SEPARATOR_S);
		return TRUE;
	}

	model = disc->priv->model;
	depth = gtk_tree_path_get_depth (treepath);
	indices = gtk_tree_path_get_indices (treepath);

	path = g_string_new_len (NULL, 16);
	iter = gtk_tree_path_new ();

	for (i = 0; i < depth; i++) {
		gtk_tree_path_append_index (iter, indices[i]);
		if (!gtk_tree_model_get_iter (model, &row, iter)) {
			gtk_tree_path_free (iter);
			g_string_free (path, TRUE);
			return FALSE;
		}

		gtk_tree_model_get (model, &row,
				    NAME_COL, &name,
				    -1);

		g_string_append_c (path, G_DIR_SEPARATOR);
		g_string_append (path, name);
		g_free (name);
	}

	gtk_tree_path_free (iter);
	g_string_set_size (path, path->len + 1);
	*discpath = g_string_free (path, FALSE);

	return TRUE;
}

static const char *
brasero_data_disc_add_path_item_position (GtkTreeModel *model,
					  GtkTreeIter *row,
					  GtkTreePath *path,
					  const char *ptr)
{
	GtkTreeIter child;
	int position;
	char *next;
	char *name;
	int len;

	ptr++;
	next = g_utf8_strchr (ptr, -1, G_DIR_SEPARATOR);
	if (!next)
		len = strlen (ptr);
	else
		len = next - ptr;

	position = 0;
	do {
		gtk_tree_model_get (model, row,
				    NAME_COL, &name,
				    -1);

		if (name && strlen (name) == len && !strncmp (name, ptr, len)) {
			gtk_tree_path_append_index (path, position);
			g_free (name);
			position = -1;
			break;
		}

		position++;
		g_free (name);
	} while (gtk_tree_model_iter_next (model, row));

	if (position != -1)
		return ptr;

	ptr += len;
	if (*ptr == '\0')
		return ptr;

	if (!gtk_tree_model_iter_children (model, &child, row))
		return ptr;

	return brasero_data_disc_add_path_item_position (model,
							  &child,
							  path,
							  ptr);
}

/* FIXME: this is very slow we need to come up with something else */
static gboolean
brasero_data_disc_disc_path_to_tree_path (BraseroDataDisc *disc,
					  const char *path,
					  GtkTreePath **treepath,
					  const char **end)
{
	GtkTreeModel *model;
	GtkTreePath *retval;
	GtkTreeIter row;
	const char *ptr;

	g_return_val_if_fail (path != NULL, FALSE);
	if (!strcmp (path, G_DIR_SEPARATOR_S)) {
		*treepath = NULL;
		return TRUE;
	}

	model = disc->priv->model;
	if (!gtk_tree_model_get_iter_first (model, &row))
		return FALSE;

	ptr = path;

	retval = gtk_tree_path_new ();
	ptr = brasero_data_disc_add_path_item_position (model,
							 &row,
							 retval,
							 ptr);

	if (*ptr != '\0') {
		if (!end) {
			gtk_tree_path_free (retval);
			return FALSE;
		}

		*end = ptr;
		*treepath = retval;
		return FALSE;
	}

	*treepath = retval;
	return TRUE;
}

static void
brasero_data_disc_tree_remove_path (BraseroDataDisc *disc,
				    const char *path)
{
	GtkTreePath *treepath;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean result;

	if (!path)
		return;

	result = brasero_data_disc_disc_path_to_tree_path (disc,
							   path,
							   &treepath,
							   NULL);
	if (!result)
		return;

	model = disc->priv->model;

	result = gtk_tree_model_get_iter (model, &iter, treepath);
	if (!result) {
		gtk_tree_path_free (treepath);
		return;
	}

	gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);

	brasero_data_disc_tree_update_parent (disc, treepath);
	gtk_tree_path_free (treepath);
}

static void
brasero_data_disc_remove_uri_from_tree (BraseroDataDisc *disc,
					const gchar *uri,
					gboolean include_grafts)
{
	GSList *paths;
	GSList *iter;
	char *path;

	/* remove all occurences from the tree */
	paths = brasero_data_disc_uri_to_paths (disc, uri, include_grafts);
	for (iter = paths; iter; iter = iter->next) {
		path = iter->data;

		brasero_data_disc_tree_remove_path (disc, path);
		g_free (path);
	}
	g_slist_free (paths);
}

static gboolean
brasero_data_disc_tree_new_path (BraseroDataDisc *disc,
				 const gchar *path,
				 const GtkTreePath *parent_treepath,
				 GtkTreePath **child_treepath)
{
	GtkTreePath *treepath;
	GtkTreeModel *model;
	GtkTreeIter child;
	gboolean result;
	char *name;

	if (!parent_treepath) {
		char *parent;

		parent = g_path_get_dirname (path);
		result = brasero_data_disc_disc_path_to_tree_path (disc,
								   parent,
								   &treepath,
								   NULL);
		g_free (parent);
		if (!result)
			return FALSE;
	}
	else
		treepath = (GtkTreePath *) parent_treepath;

	model = disc->priv->model;
	if (treepath) {
		GtkTreeIter iter;

		result = gtk_tree_model_get_iter (model, &iter, treepath);
		if (parent_treepath != treepath)
			gtk_tree_path_free (treepath);

		if (!result)
			return FALSE;

		gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);
		brasero_data_disc_tree_update_directory_real (disc, &iter);
	}
	else
		gtk_tree_store_append (GTK_TREE_STORE (model), &child, NULL);

	if (child_treepath)
		*child_treepath = gtk_tree_model_get_path (model, &child);

	name = g_path_get_basename (path);
	gtk_tree_store_set (GTK_TREE_STORE (model), &child,
			    NAME_COL, name,
			    ROW_STATUS_COL, ROW_NEW, -1);
	g_free (name);

	return TRUE;
}

static gboolean
brasero_data_disc_tree_set_path_from_info (BraseroDataDisc *disc,
					   const char *path,
					   const GtkTreePath *treepath,
					   const GnomeVFSFileInfo *info)
{
	const char *description;
	GtkTreeModel *model;
	GtkTreeIter parent;
	GdkPixbuf *pixbuf;
	GtkTreeIter iter;
	gboolean result;
	gboolean isdir;
	gint64 dsize;
	char *name;
	char *size;

	if (!path)
		return FALSE;

	model = disc->priv->model;

	if (!treepath) {
		GtkTreePath *tmp_treepath;

		result = brasero_data_disc_disc_path_to_tree_path (disc,
								   path,
								   &tmp_treepath,
								   NULL);
	
		if (!result)
			return FALSE;

		result = gtk_tree_model_get_iter (model, &iter, tmp_treepath);
		gtk_tree_path_free (tmp_treepath);
	
		if (!result)
			return FALSE;
	}
	else {
		result = gtk_tree_model_get_iter (model, &iter, (GtkTreePath*) treepath);

		if (!result)
			return FALSE;
	}

	if (info->type != GNOME_VFS_FILE_TYPE_DIRECTORY) {
		/* we don't display file sizes in sectors and dsize is used to
		 * compare the files when sorting them by sizes */
		size = brasero_utils_get_size_string (info->size, TRUE, TRUE);
		isdir = FALSE;
		dsize = info->size;
	}
	else {
		size = g_strdup (_("(loading ...)"));
		isdir = TRUE;
		dsize = 0;
	}

	if (info->mime_type) {
		pixbuf = brasero_utils_get_icon_for_mime (info->mime_type, 16);
		description = gnome_vfs_mime_get_description (info->mime_type);
	}
	else {
		pixbuf = NULL;
		description = NULL;
	}

	name = g_path_get_basename (path);
	gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
			    ICON_COL, pixbuf,
			    NAME_COL, name,
			    DSIZE_COL, dsize,
			    SIZE_COL, size,
			    MIME_COL, description,
			    ISDIR_COL, isdir, -1);
	g_free (name);
	g_free (size);

	if (pixbuf)
		g_object_unref (pixbuf);

	if (info->type != GNOME_VFS_FILE_TYPE_DIRECTORY) {
		gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
				    ROW_STATUS_COL, ROW_EXPANDED,
				    -1);
		return TRUE;
	}

	/* see if this directory should be explored */
	if (gtk_tree_model_iter_parent (model, &parent, &iter)) {
		int status;

		gtk_tree_model_get (model, &parent,
				    ROW_STATUS_COL, &status,
				    -1);

		if (status == ROW_EXPANDED) {
			brasero_data_disc_expose_path (disc, path);
			gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
					    ROW_STATUS_COL, ROW_EXPLORING,
					    -1);
		}
		else
			gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
					    ROW_STATUS_COL, ROW_NOT_EXPLORED,
					    -1);
	}
	else {
		brasero_data_disc_expose_path (disc, path);
		gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
				    ROW_STATUS_COL, ROW_EXPLORING,
				    -1);
	}

	return TRUE;
}

static gboolean
brasero_data_disc_tree_new_empty_folder_real (BraseroDataDisc *disc,
					      const char *path,
					      gint state)
{
	GtkTreeViewColumn *column;
	const char *description;
	GtkTreeIter sort_iter;
	GtkTreePath *treepath;
	GtkTreeModel *model;
	GdkPixbuf *pixbuf;
	GtkTreeIter child;
	gboolean result;
	char *parent;
	char *name;

	parent = g_path_get_dirname (path);
	result = brasero_data_disc_disc_path_to_tree_path (disc,
							   parent,
							   &treepath,
							   NULL);
	g_free (parent);
	if (!result)
		return FALSE;

	model = disc->priv->model;
	if (treepath) {
		GtkTreeIter iter;

		result = gtk_tree_model_get_iter (model, &iter, treepath);
		gtk_tree_path_free (treepath);
		if (!result)
			return FALSE;

		gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);
		brasero_data_disc_tree_update_directory_real (disc, &iter);
	}
	else
		gtk_tree_store_append (GTK_TREE_STORE (model), &child, NULL);

	pixbuf = brasero_utils_get_icon_for_mime ("x-directory/normal", 16);
	description = gnome_vfs_mime_get_description ("x-directory/normal");
	name = g_path_get_basename (path);

	gtk_tree_store_set (GTK_TREE_STORE (model), &child,
			    NAME_COL, name,
			    MIME_COL, description,
			    ISDIR_COL, TRUE,
			    ICON_COL, pixbuf,
			    ROW_STATUS_COL, state,
			    -1);
	g_object_unref (pixbuf);
	g_free (name);

	brasero_data_disc_tree_update_directory_real (disc, &child);

	/* we leave the possibility to the user to edit the name */
	gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (disc->priv->sort),
							&sort_iter,
							&child);

	treepath = gtk_tree_model_get_path (disc->priv->sort, &sort_iter);

	column = gtk_tree_view_get_column (GTK_TREE_VIEW (disc->priv->tree),
					   0);
	gtk_tree_view_set_cursor (GTK_TREE_VIEW (disc->priv->tree),
				  treepath,
				  column,
				  TRUE);
	gtk_widget_grab_focus (disc->priv->tree);

	return TRUE;
}

static gboolean
brasero_data_disc_tree_new_empty_folder (BraseroDataDisc *disc,
					 const gchar *path)
{
	return brasero_data_disc_tree_new_empty_folder_real (disc, path, ROW_EXPLORED);
}

static gboolean
brasero_data_disc_tree_new_loading_row (BraseroDataDisc *disc,
					const char *path)
{
	GtkTreePath *treepath;
	GtkTreeModel *model;
	GtkTreeIter child;
	gboolean result;
	char *parent;
	char *name;

	parent = g_path_get_dirname (path);
	result = brasero_data_disc_disc_path_to_tree_path (disc,
							    parent,
							    &treepath,
							    NULL);
	g_free (parent);
	if (!result)
		return FALSE;

	model = disc->priv->model;
	if (treepath) {
		GtkTreeIter iter;

		result = gtk_tree_model_get_iter (model, &iter, treepath);
		gtk_tree_path_free (treepath);
		if (!result)
			return FALSE;

		gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);
		brasero_data_disc_tree_update_directory_real (disc, &iter);
	}
	else
		gtk_tree_store_append (GTK_TREE_STORE (model), &child, NULL);

	name = g_path_get_basename (path);
	gtk_tree_store_set (GTK_TREE_STORE (model), &child,
			    NAME_COL, name,
			    SIZE_COL, _("(loading ...)"),
			    MIME_COL, _("(loading ...)"),
			    ROW_STATUS_COL, ROW_NEW,
			    -1);
	g_free (name);
	return TRUE;
}

/******************************** references ***********************************/
typedef gint BraseroDataDiscReference;
static char *BRASERO_INVALID_REFERENCE = "Invalid";

static BraseroDataDiscReference
brasero_data_disc_reference_new (BraseroDataDisc *disc,
				 const gchar *path)
{
	static BraseroDataDiscReference counter = 1;
	BraseroDataDiscReference retval;

	if (!disc->priv->path_refs)
		disc->priv->path_refs = g_hash_table_new (g_direct_hash,
							  g_direct_equal);

	retval = counter;
	while (g_hash_table_lookup (disc->priv->path_refs, GINT_TO_POINTER (retval))) {
		retval ++;

		if (retval == G_MAXINT)
			retval = 1;

		if (retval == counter)
			return 0;
	}

	g_hash_table_insert (disc->priv->path_refs,
			     GINT_TO_POINTER (retval),
			     g_strdup (path));

	counter = retval + 1;
	if (counter == G_MAXINT)
		counter = 1;

	return retval;
}

static void
brasero_data_disc_reference_free (BraseroDataDisc *disc,
				  BraseroDataDiscReference ref)
{
	char *value;

	if (!disc->priv->path_refs)
		return;

	value = g_hash_table_lookup (disc->priv->path_refs, GINT_TO_POINTER (ref));
	g_hash_table_remove (disc->priv->path_refs, GINT_TO_POINTER (ref));
	if (value != BRASERO_INVALID_REFERENCE)
		g_free (value);

	if (!g_hash_table_size (disc->priv->path_refs)) {
		g_hash_table_destroy (disc->priv->path_refs);
		disc->priv->path_refs = NULL;
	}
}

static void
brasero_data_disc_reference_free_list (BraseroDataDisc *disc,
				       GSList *list)
{
	BraseroDataDiscReference ref;

	for (; list; list = g_slist_remove (list, GINT_TO_POINTER (ref))) {
		ref = GPOINTER_TO_INT (list->data);
		brasero_data_disc_reference_free (disc, ref);
	}
}

static gchar *
brasero_data_disc_reference_get (BraseroDataDisc *disc,
				 BraseroDataDiscReference ref)
{
	gpointer value;

	if (!disc->priv->path_refs)
		return NULL;

	value = g_hash_table_lookup (disc->priv->path_refs, GINT_TO_POINTER (ref));
	if (!value || value == BRASERO_INVALID_REFERENCE)
		return NULL;

	return g_strdup (value);
}

static GSList *
brasero_data_disc_reference_get_list (BraseroDataDisc *disc,
				      GSList *references,
				      gboolean free_refs)
{
	char *path;
	GSList *iter;
	GSList *paths = NULL;
	BraseroDataDiscReference ref;

	if (!disc->priv->path_refs)
		return NULL;

	for (iter = references; iter; iter = iter->next) {
		ref = GPOINTER_TO_INT (iter->data);

		path = brasero_data_disc_reference_get (disc, ref);

		if (free_refs)
			brasero_data_disc_reference_free (disc, ref);

		if (path)
			paths = g_slist_prepend (paths, path);
	}

	if (free_refs)
		g_slist_free (references);

	return paths;
}

struct _MakeReferencesListData {
	char *path;
	int len;
	GSList *list;
};
typedef struct _MakeReferencesListData MakeReferencesListData;

static void
_foreach_make_references_list_cb (BraseroDataDiscReference num,
				  char *path,
				  MakeReferencesListData *data)
{
	if (!strncmp (path, data->path, data->len)
	&& (*(path + data->len) == G_DIR_SEPARATOR || *(path + data->len) == '\0'))
		data->list = g_slist_prepend (data->list, GINT_TO_POINTER (num));
}

static void
brasero_data_disc_move_references (BraseroDataDisc *disc,
				   const char *oldpath,
				   const char *newpath)
{
	MakeReferencesListData callback_data;
	BraseroDataDiscReference ref;
	char *newvalue;
	char *value;
	int len;

	if (!disc->priv->path_refs)
		return;

	len = strlen (oldpath);
	callback_data.path = (char*) oldpath;
	callback_data.len = len;
	callback_data.list = NULL;

	g_hash_table_foreach (disc->priv->path_refs,
			      (GHFunc) _foreach_make_references_list_cb,
			      &callback_data);

	for (; callback_data.list; callback_data.list = g_slist_remove (callback_data.list, GINT_TO_POINTER (ref))) {
		ref = GPOINTER_TO_INT (callback_data.list->data);

		value = g_hash_table_lookup (disc->priv->path_refs,
					     GINT_TO_POINTER (ref));
		newvalue = g_strconcat (newpath, value + len, NULL);
		g_hash_table_replace (disc->priv->path_refs,
				      GINT_TO_POINTER (ref),
				      newvalue);
		g_free (value);
	}
}

static void
brasero_data_disc_reference_remove_path (BraseroDataDisc *disc,
					 const gchar *path)
{
	MakeReferencesListData callback_data;
	BraseroDataDiscReference ref;
	char *value;

	if (!disc->priv->path_refs)
		return;

	callback_data.path = (char*) path;
	callback_data.len = strlen (path);
	callback_data.list = NULL;

	g_hash_table_foreach (disc->priv->path_refs,
			      (GHFunc) _foreach_make_references_list_cb,
			      &callback_data);

	for (; callback_data.list; callback_data.list = g_slist_remove (callback_data.list, GINT_TO_POINTER (ref))) {
		ref = GPOINTER_TO_INT (callback_data.list->data);

		value = g_hash_table_lookup (disc->priv->path_refs,
					     GINT_TO_POINTER (ref));
		g_hash_table_replace (disc->priv->path_refs,
				      GINT_TO_POINTER (ref),
				      BRASERO_INVALID_REFERENCE);
		g_free (value);
	}
}

static void
brasero_data_disc_reference_invalidate_all (BraseroDataDisc *disc)
{
	char *root = G_DIR_SEPARATOR_S;

	if (!disc->priv->path_refs)
		return;

	brasero_data_disc_reference_remove_path (disc, root);
}

/*********************** joliet non compliant files   **************************/
static gchar *
brasero_data_disc_joliet_get_key (const gchar *path,
				  gchar **name_ret,
				  gchar **parent_ret)
{
	gchar *parent;
	gchar *name;
	gchar *key;

	/* key is equal to the parent path and the 64 first characters of the name */
	parent = g_path_get_dirname (path);
	name = g_path_get_basename (path);
	if (strcmp (parent, G_DIR_SEPARATOR_S))
		key = g_strdup_printf ("%s" G_DIR_SEPARATOR_S "%.64s",
				       parent,
				       name);
	else
		key = g_strdup_printf (G_DIR_SEPARATOR_S "%.64s",
				       name);

	if (name_ret)
		*name_ret = name;
	else
		g_free (name);

	if (parent_ret)
		*parent_ret = parent;
	else
		g_free (parent);

	return key;
}

static gchar *
brasero_data_disc_joliet_incompat_get_joliet_compliant_name (BraseroDataDisc *disc,
							     const gchar *path,
							     gchar **parent)
{
	GSList *node;
	GSList *list;
	gchar *retval;
	gchar *name;
	gchar *key;
	gint width;
	gint num;

	key = brasero_data_disc_joliet_get_key (path, &name, parent);
	list = g_hash_table_lookup (disc->priv->joliet_non_compliant,
				    key);
	if (g_slist_length (list) < 2)
		return name;

	node = g_slist_find_custom (list, name, (GCompareFunc) strcmp);
	num = g_slist_index (list, node->data);
		
	width = 1;
	while (num / (width * 10)) width ++;
	width = 64 - width;

	retval = g_strdup_printf ("%.*s%i",
				  width,
				  name,
				  num);
	g_free (name);
	return retval;
}

static gchar *
brasero_data_disc_joliet_incompat_get_joliet_compliant_path (BraseroDataDisc *disc,
							     const gchar *path)
{
	GString *retval;
	gchar *parent;

	if (!disc->priv->joliet_non_compliant)
		return g_strdup (path);

	/* we have to make sure of two things:
	 * - the name itself is joliet compliant
	 * - one of its parents doesn't have also the same problem */
	retval = g_string_new_len (NULL, strlen (path));
	parent = g_strdup (path);
	while (parent && parent [0] == G_DIR_SEPARATOR && parent [1] != '\0') {
		gchar *tmp;
		gchar *joliet_name;

		tmp = parent;
		joliet_name = brasero_data_disc_joliet_incompat_get_joliet_compliant_name (disc,
											   tmp,
											   &parent);
		g_free (tmp);

		g_string_prepend (retval, joliet_name);
		g_string_prepend (retval, G_DIR_SEPARATOR_S);
		g_free (joliet_name);
	}
	g_strdup (parent);
	return g_string_free (retval, FALSE);
}

static void
brasero_data_disc_joliet_incompat_add_path (BraseroDataDisc *disc,
					    const gchar *path)
{
	GSList *list;
	gchar *name;
	gchar *key;

	if (!disc->priv->joliet_non_compliant)
		disc->priv->joliet_non_compliant = g_hash_table_new_full (g_str_hash,
									  g_str_equal,
									  g_free,
									  NULL);

	key = brasero_data_disc_joliet_get_key (path, &name, NULL);

	list = g_hash_table_lookup (disc->priv->joliet_non_compliant, key);
	list = g_slist_prepend (list, name);
	g_hash_table_insert (disc->priv->joliet_non_compliant, key, list);
}

static void
brasero_data_disc_joliet_incompat_add_paths (BraseroDataDisc *disc,
					     GSList *paths)
{
	GSList *iter;

	for (iter = paths; iter; iter = iter->next) {
		gchar *path;

		path = iter->data;
		brasero_data_disc_joliet_incompat_add_path (disc, path);
	}
}

static void
brasero_data_disc_joliet_incompat_free (BraseroDataDisc *disc,
					const gchar *path)
{
	GSList *list, *node;
	gchar *name;
	gchar *key;

	if (!disc->priv->joliet_non_compliant)
		return;

	key = brasero_data_disc_joliet_get_key (path, &name, NULL);
	list = g_hash_table_lookup (disc->priv->joliet_non_compliant, key);
	if (!list)
		return;

	node = g_slist_find_custom (list, name, (GCompareFunc) strcmp);
	if (node) {
		list = g_slist_remove (list, node->data);
		if (!list) {
			g_hash_table_remove (disc->priv->joliet_non_compliant, key);
			if (g_hash_table_size (disc->priv->joliet_non_compliant)) {
				g_hash_table_destroy (disc->priv->joliet_non_compliant);
				disc->priv->joliet_non_compliant = NULL;
			}
		}
		else
			g_hash_table_insert (disc->priv->joliet_non_compliant, key, list);
	}
}

static void
brasero_data_disc_joliet_incompat_move (BraseroDataDisc *disc,
					const gchar *old_path,
					const gchar *new_path)
{
	gchar *name;

	/* remove old path if any */
	brasero_data_disc_joliet_incompat_free (disc, old_path);

	/* see if the new path should be inserted */
	name = g_path_get_basename (new_path);
	if (strlen (name) > 64)
		brasero_data_disc_joliet_incompat_add_path (disc, new_path);

	g_free (name);
}

/*********************** get file info asynchronously **************************/
typedef gboolean	(*BraseroInfoAsyncResultFunc)	(BraseroDataDisc *disc,
								 GSList *results,
								 gpointer user_data);
typedef void		(*BraseroInfoAsyncDestroyFunc)		(BraseroDataDisc *disc,
								 gpointer user_data);

struct _GetFileInfoAsyncData {
	GSList *uris;
	GSList *results;
	GnomeVFSFileInfoOptions flags;
	BraseroInfoAsyncResultFunc callback_func;
	gpointer user_data;
	BraseroInfoAsyncDestroyFunc destroy_func;

	gint cancel:1;
};
typedef struct _GetFileInfoAsyncData GetFileInfoAsyncData;

struct _BraseroInfoAsyncResult {
	GnomeVFSResult result;
	GnomeVFSFileInfo *info;
	gchar *uri;
};
typedef struct _BraseroInfoAsyncResult BraseroInfoAsyncResult;

static void
brasero_data_disc_get_file_info_async_destroy_real (BraseroDataDisc *disc,
						    GetFileInfoAsyncData *callback_data,
						    gboolean call_destroy)
{
	GSList *iter;

	/* destroy the data is needed */
	if (callback_data->destroy_func && call_destroy)
		callback_data->destroy_func (disc, callback_data->user_data);

	g_slist_foreach (callback_data->uris, (GFunc) g_free, NULL);
	g_slist_free (callback_data->uris);

	for (iter = callback_data->results; iter; iter = iter->next) {
		BraseroInfoAsyncResult *result;

		result = iter->data;
		if (result->uri)
			g_free (result->uri);

		if (result->info)
			gnome_vfs_file_info_unref (result->info);

		g_free (result);
	}
	g_slist_free (callback_data->results);
	g_free (callback_data);
}

static void
brasero_data_disc_get_file_info_async_destroy (GObject *object, gpointer data)
{
	GetFileInfoAsyncData *callback_data = data;
	BraseroDataDisc *disc = BRASERO_DATA_DISC (object);

	brasero_data_disc_get_file_info_async_destroy_real (disc,
							    callback_data,
							    TRUE);
}

static void
brasero_data_disc_get_file_info_cancel (gpointer data)
{
	GetFileInfoAsyncData *callback_data = data;

	callback_data->cancel = 1;
}

static gboolean
brasero_data_disc_get_file_info_async_results (GObject *obj, gpointer data)
{
	gboolean result;
	GetFileInfoAsyncData *callback_data = data;
	BraseroDataDisc *disc = BRASERO_DATA_DISC (obj);

	result = callback_data->callback_func (disc,
					       callback_data->results,
					       callback_data->user_data);

	brasero_data_disc_decrease_activity_counter (disc);

	/* if result == TRUE we free the user_data otherwise just free data 
	 * by calling brasero_data_disc_get_file_info_async_destroy ourselves
	 * and returning FALSE */
	if (!result) {
		brasero_data_disc_get_file_info_async_destroy_real (disc,
								    callback_data,
								    FALSE);
		return FALSE;
	}

	return TRUE;
}

static gboolean
brasero_data_disc_get_file_info_async (GObject *obj, gpointer data)
{
	char *uri;
	GSList *next;
	char *escaped_uri;
	GnomeVFSFileInfo *info;
	BraseroInfoAsyncResult *result;
	GetFileInfoAsyncData *callback_data = data;

	for (; callback_data->uris; callback_data->uris = next) {

		if (callback_data->cancel)
			return FALSE;

		uri = callback_data->uris->data;
		result = g_new0 (BraseroInfoAsyncResult, 1);

		/* If we want to make sure a directory is not added twice we have to make sure
		 * that it doesn't have a symlink as parent otherwise "/home/Foo/Bar" with Foo
		 * as a symlink pointing to /tmp would be seen as a different file from /tmp/Bar 
		 * It would be much better if we could use the inode numbers provided by gnome_vfs
		 * unfortunately they are guint64 and can't be used in hash tables as keys.
		 * Therefore we check parents up to root to see if there are symlinks and if so
		 * we get a path without symlinks in it. This is done only for local file */
		result->uri = brasero_utils_check_for_parent_symlink (uri);
		next = g_slist_remove (callback_data->uris, uri);
		g_free (uri);

		escaped_uri = gnome_vfs_escape_host_and_path_string (result->uri);
		info = gnome_vfs_file_info_new ();
		result->result = gnome_vfs_get_file_info (escaped_uri,
							  info,
							  callback_data->flags);
		g_free (escaped_uri);

		callback_data->results = g_slist_prepend (callback_data->results,
							  result);

		if (result->result != GNOME_VFS_OK) {
			gnome_vfs_file_info_unref (info);
			continue;
		}

		if (info->type == GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK) {
			gnome_vfs_file_info_clear (info);

			if (!brasero_utils_get_symlink_target (result->uri,
							       info,
							       callback_data->flags)) {
				/* since we checked for the existence of the file
				 * an error means a looping symbolic link */
				result->result = GNOME_VFS_ERROR_LOOP;
			}
		}

		result->info = info;
	}

	callback_data->uris = NULL;
	return TRUE;
}

static BraseroDiscResult 
brasero_data_disc_get_info_async (BraseroDataDisc *disc,
				  GSList *uris,
				  GnomeVFSFileInfoOptions flags,
				  BraseroInfoAsyncResultFunc func,
				  gpointer user_data,
				  BraseroInfoAsyncDestroyFunc destroy_func)
{
	char *uri;
	GetFileInfoAsyncData *callback_data;

	if (!disc->priv->jobs)
		disc->priv->jobs = brasero_async_job_manager_get_default ();

	if (!disc->priv->file_type) {
		disc->priv->file_type = brasero_async_job_manager_register_type (disc->priv->jobs,
										 G_OBJECT (disc),
										 brasero_data_disc_get_file_info_async,
										 brasero_data_disc_get_file_info_async_results,
										 brasero_data_disc_get_file_info_async_destroy,
										 brasero_data_disc_get_file_info_cancel);
	}

	callback_data = g_new0 (GetFileInfoAsyncData, 1);
	for (; uris; uris = uris->next) {
		uri = uris->data;
		callback_data->uris = g_slist_prepend (callback_data->uris,
						       g_strdup (uri));
	}

	/* NOTE: we don't reverse the list on purpose as when we get infos 
	 * the results will be prepended restoring the right order of things */
	callback_data->flags = flags;
	callback_data->callback_func = func;
	callback_data->user_data = user_data;
	callback_data->destroy_func = destroy_func;

	/* NOTE : if an error occurs the callback_data will be freed by async_job_manager */
	if (!brasero_async_job_manager_queue (disc->priv->jobs,
					      disc->priv->file_type,
					      callback_data))
		return BRASERO_DISC_ERROR_THREAD;

	brasero_data_disc_increase_activity_counter (disc);
	return BRASERO_DISC_OK;
}


/**************************************** **************************************/
static gboolean
_foreach_remove_excluded_cb (char *key,
			     GSList *excluding,
			     BraseroDataDisc *disc)
{
	if (!g_hash_table_lookup (disc->priv->dirs, key)
	&&  !g_hash_table_lookup (disc->priv->files, key))
		g_free (key);

	g_slist_free (excluding);

	return TRUE;
}

static void
brasero_data_disc_empty_excluded_hash (BraseroDataDisc *disc)
{
	if (!disc->priv->excluded)
		return;

	g_hash_table_foreach_remove (disc->priv->excluded,
				     (GHRFunc) _foreach_remove_excluded_cb,
				     disc);

	g_hash_table_destroy (disc->priv->excluded);
	disc->priv->excluded = NULL;
}

static gboolean
_foreach_remove_restored_cb (char *key,
			     BraseroFilterStatus status,
			     BraseroDataDisc *disc)
{
	if (!g_hash_table_lookup (disc->priv->dirs, key)
	&&  !g_hash_table_lookup (disc->priv->files, key))
		g_free (key);

	return TRUE;
}

static void
brasero_data_disc_empty_restored_hash (BraseroDataDisc *disc)
{
	if (!disc->priv->restored)
		return;

	g_hash_table_foreach_remove (disc->priv->restored,
				     (GHRFunc) _foreach_remove_restored_cb,
				     disc);

	g_hash_table_destroy (disc->priv->restored);
	disc->priv->restored = NULL;
}

static gboolean
_foreach_remove_symlink_cb (char *symlink,
			    char *target,
			    BraseroDataDisc *disc)
{
/*
	if (!g_hash_table_lookup (disc->priv->dirs, target)
	&&  !g_hash_table_lookup (disc->priv->files, target))
		g_free (target);
*/
	return TRUE;
}

static void
brasero_data_disc_empty_symlink_hash (BraseroDataDisc *disc)
{

	if (!disc->priv->symlinks)
		return;

	g_hash_table_foreach_remove (disc->priv->symlinks,
				     (GHRFunc) _foreach_remove_symlink_cb,
				     disc);

	g_hash_table_destroy (disc->priv->symlinks);
	disc->priv->symlinks = NULL;
}

static gboolean
_foreach_remove_grafts_cb (const char *uri,
			   GSList *grafts,
			   BraseroDataDisc *disc)
{
	g_slist_foreach (grafts, (GFunc) g_free, NULL);
	g_slist_free (grafts);
	return TRUE;
}

static void
_foreach_remove_created_dirs_cb (char *graft, 
				 const char *uri,
				 BraseroDataDisc *disc)
{
	if (uri == BRASERO_CREATED_DIR)
		g_free (graft);
}

static void
brasero_data_disc_empty_grafts_hash (BraseroDataDisc *disc)
{
	g_hash_table_foreach (disc->priv->paths,
			     (GHFunc) _foreach_remove_created_dirs_cb,
			      disc);
	g_hash_table_destroy (disc->priv->paths);
	disc->priv->paths = g_hash_table_new (g_str_hash, g_str_equal);

	g_hash_table_foreach_remove (disc->priv->grafts,
				    (GHRFunc) _foreach_remove_grafts_cb,
				     disc);
}

static gboolean
_foreach_remove_files_cb (const char *key,
			  BraseroFile *file,
			  gpointer data)
{
	g_free (file->uri);
	g_free (file);

	return TRUE;
}

static void
brasero_data_disc_empty_files_hash (BraseroDataDisc *disc)
{
	g_hash_table_foreach_remove (disc->priv->files,
				     (GHRFunc) _foreach_remove_files_cb,
				     disc);
}

static void
brasero_data_disc_empty_dirs_hash (BraseroDataDisc *disc)
{
	g_hash_table_foreach_remove (disc->priv->dirs,
				     (GHRFunc) _foreach_remove_files_cb,
				     disc);
}

#ifdef BUILD_INOTIFY
static gboolean
_foreach_remove_monitored_cb (const int wd,
			      const char *uri,
			      BraseroDataDisc *disc)
{
	int dev_fd;

	dev_fd = g_io_channel_unix_get_fd (disc->priv->notify);
	inotify_rm_watch (dev_fd, wd);

	return TRUE;
}

static void
brasero_data_disc_empty_monitor_hash (BraseroDataDisc *disc)
{
	g_hash_table_foreach_remove (disc->priv->monitored,
				     (GHRFunc) _foreach_remove_monitored_cb,
				     disc);
}
#endif

static gboolean
_foreach_remove_loading_cb (const char *key,
			    GSList *loading,
			    BraseroDataDisc *disc)
{
	BraseroDataDiscReference ref;
	GSList *iter;

	for (iter = loading; iter; iter = iter->next) {
		ref = GPOINTER_TO_INT (iter->data);
		brasero_data_disc_reference_free (disc, ref);
	}

	g_slist_free (loading);
	return TRUE;
}

static void
brasero_data_disc_empty_loading_hash (BraseroDataDisc *disc)
{
	if (!disc->priv->references)
		return;

	/* we don't need a mutex here as all threads have been stopped */
	g_hash_table_foreach_remove (disc->priv->references,
				     (GHRFunc) _foreach_remove_loading_cb,
				     disc);
	g_hash_table_destroy (disc->priv->references);
	disc->priv->references = NULL;
}

static void
_foreach_empty_joliet_incompat_cb (const gchar *key,
				   GSList *list,
				   gpointer null_data)
{
	g_slist_foreach (list, (GFunc) g_free, NULL);
	g_slist_free (list);
}

static void
brasero_data_disc_empty_joliet_incompat (BraseroDataDisc *disc)
{
	if (!disc->priv->joliet_non_compliant)
		return;

	g_hash_table_foreach (disc->priv->joliet_non_compliant,
			      (GHFunc) _foreach_empty_joliet_incompat_cb,
			      NULL);
	g_hash_table_destroy (disc->priv->joliet_non_compliant);
	disc->priv->joliet_non_compliant = NULL;
}

static void
brasero_data_disc_clean (BraseroDataDisc *disc)
{
	if (disc->priv->selected_path) {
		gtk_tree_path_free (disc->priv->selected_path);
		disc->priv->selected_path = NULL;
	}

	/* set all references to paths to be invalid :
	 * this comes first as many callback functions rely on references
	 * to know whether they should still do what needs to be done */
	brasero_data_disc_reference_invalidate_all (disc);

	/* stop any job: do it here to prevent any further addition */
	if (disc->priv->jobs)
		brasero_async_job_manager_cancel_by_object (disc->priv->jobs, 
							    G_OBJECT (disc));

	/* empty expose loading, rescan queues */
	if (disc->priv->expose_id) {
		g_source_remove (disc->priv->expose_id);
		disc->priv->expose_id = 0;
	}

	g_slist_free (disc->priv->expose);
	disc->priv->expose = NULL;

	if (disc->priv->loading) {
		g_slist_free (disc->priv->loading);
		disc->priv->loading = NULL;
	}

	if (disc->priv->rescan) {
		g_slist_free (disc->priv->rescan);
		disc->priv->rescan = NULL;
	}

	/* empty restored hash table */
	brasero_data_disc_empty_restored_hash (disc);

	/* empty excluded hash IT SHOULD COME FIRST before all other hashes */
	brasero_data_disc_empty_excluded_hash (disc);

	/* empty symlinks hash comes first as well */
	brasero_data_disc_empty_symlink_hash (disc);

	/* empty grafts : it should come first as well */
	brasero_data_disc_empty_grafts_hash (disc);

	/* empty unreadable */
	if (disc->priv->unreadable) {
		g_hash_table_destroy (disc->priv->unreadable);
		disc->priv->unreadable = NULL;
	}

	/* empty files hash table */
	brasero_data_disc_empty_files_hash (disc);

	/* empty dirs hash table */
	brasero_data_disc_empty_dirs_hash (disc);

#ifdef BUILD_INOTIFY
	/* empty monitor hash table */
	brasero_data_disc_empty_monitor_hash (disc);
#endif

	/* empty loading hash table */
	brasero_data_disc_empty_loading_hash (disc);

	/* empty joliet incompatible hash */
	brasero_data_disc_empty_joliet_incompat (disc);
}

static void
brasero_data_disc_reset_real (BraseroDataDisc *disc)
{
	brasero_data_disc_clean (disc);

	if (GTK_WIDGET (disc->priv->filter_button) && disc->priv->unreadable)
		gtk_widget_set_sensitive (disc->priv->filter_button, FALSE);

	disc->priv->activity_counter = 1;
	brasero_data_disc_decrease_activity_counter (disc);

	/* reset size */
	disc->priv->sectors = 0;
}

/******************************** utility functions ****************************/
inline static gboolean
brasero_data_disc_is_readable (const GnomeVFSFileInfo *info)
{
	if (!GNOME_VFS_FILE_INFO_LOCAL (info))
		return TRUE;

	if (getuid () == info->uid && (info->permissions & GNOME_VFS_PERM_USER_READ))
		return TRUE;
	else if (brasero_utils_is_gid_in_groups (info->gid)
	      && (info->permissions & GNOME_VFS_PERM_GROUP_READ))
		return TRUE;
	else if (info->permissions & GNOME_VFS_PERM_OTHER_READ)
		return TRUE;

	return FALSE;
}


inline static void
brasero_data_disc_add_rescan (BraseroDataDisc *disc,
			      BraseroFile *dir)
{
	/* have a look at the rescan list and see if we uri was already inserted */
	if (g_slist_find (disc->priv->rescan, dir)
	||  g_slist_find (disc->priv->loading, dir))
		return;

	disc->priv->rescan = g_slist_append (disc->priv->rescan, dir);
	brasero_data_disc_directory_load (disc, dir, FALSE);
}

static void
brasero_data_disc_size_changed (BraseroDataDisc *disc,
				gint64 sectors)
{
	if (sectors == 0)
		return;

	disc->priv->sectors += sectors;
	/* if there are still uris waiting in the queue to be explored and
	 * CDContent says it's full what should we do ?
	 * the best solution is just to continue exploration but prevent
	 * any other manual addition to the selection.
	 * another solution would be to stop everything all together but then
	 * the user wouldn't necessarily know that some directories were not 
	 * added and even then he would certainly still have to remove some
	 * files. It's better for him making the choice of which files and 
	 * directories to remove */
	brasero_disc_size_changed (BRASERO_DISC (disc), disc->priv->sectors);
}

static gboolean
brasero_data_disc_original_parent (BraseroDataDisc *disc,
				   const char *uri,
				   const char *path)
{
	int result;
	char *graft;
	char *path_uri;
	const char *graft_uri;

	graft = brasero_data_disc_graft_get (disc, path);
	if (!graft)
		return FALSE;

	graft_uri = g_hash_table_lookup (disc->priv->paths, graft);
	path_uri = g_strconcat (graft_uri, path + strlen (graft), NULL);
	g_free (graft);

	result = strcmp (path_uri, uri);
	g_free (path_uri);
	return (result == 0);
}

/******************************* unreadable files ******************************/
static void
brasero_data_disc_unreadable_new (BraseroDataDisc *disc,
				  char *uri,
				  BraseroFilterStatus status)
{
	char *parenturi;
	BraseroFile *parent;

	parenturi = g_path_get_dirname (uri);
	parent = g_hash_table_lookup (disc->priv->dirs, parenturi);
	g_free (parenturi);

	if (!parent || parent->sectors < 0)  {
		g_free (uri);
		return;
	}

	if (!disc->priv->unreadable) {
		disc->priv->unreadable = g_hash_table_new_full (g_str_hash,
							        g_str_equal,
							        g_free,
							        NULL);

		/* we can now signal the user that some files were removed */
		gtk_widget_set_sensitive (disc->priv->filter_button, TRUE);
		gtk_tooltips_enable (disc->priv->tooltip);
	}

	if (disc->priv->filter_dialog
	&&  !g_hash_table_lookup (disc->priv->unreadable, uri)) {
		brasero_filtered_dialog_add (BRASERO_FILTERED_DIALOG (disc->priv->filter_dialog),
					     uri,
					     FALSE,
					     status);
	}

	g_hash_table_replace (disc->priv->unreadable,
			      uri,
			      GINT_TO_POINTER(status));
}

static BraseroFilterStatus
brasero_data_disc_unreadable_free (BraseroDataDisc *disc,
				   const char *uri)
{
	BraseroFilterStatus status;

	if (!disc->priv->unreadable)
		return 0;

	status = GPOINTER_TO_INT (g_hash_table_lookup (disc->priv->unreadable, uri));
	g_hash_table_remove (disc->priv->unreadable, uri);
	if (!g_hash_table_size (disc->priv->unreadable)) {
		if (!disc->priv->restored) {
			gtk_widget_set_sensitive (disc->priv->filter_button,
						  FALSE);
		}

		g_hash_table_destroy (disc->priv->unreadable);
		disc->priv->unreadable = NULL;
	}

	return status;
}

static void
brasero_data_disc_restored_new (BraseroDataDisc *disc, 
				const char *uri, 
				BraseroFilterStatus status)
{
	BraseroFile *file;
	gchar *key;

	if (!disc->priv->restored) {
		disc->priv->restored = g_hash_table_new (g_str_hash, g_str_equal);
		gtk_widget_set_sensitive (disc->priv->filter_button, TRUE);
	}

	if (!status)
		status = brasero_data_disc_unreadable_free (disc, uri);

	g_mutex_lock (disc->priv->restored_lock);

	if ((file = g_hash_table_lookup (disc->priv->files, uri))
	||  (file = g_hash_table_lookup (disc->priv->dirs, uri)))
		key = file->uri;
	else
		key = g_strdup (uri);

	g_hash_table_insert (disc->priv->restored,
			     key,
			     GINT_TO_POINTER (status));
	g_mutex_unlock (disc->priv->restored_lock);
}

static BraseroFilterStatus
brasero_data_disc_restored_free (BraseroDataDisc *disc,
				 const gchar *uri)
{

	gpointer key = NULL;
	gpointer status;

	if (!disc->priv->restored)
		return 0;

	g_mutex_lock (disc->priv->restored_lock);
	g_hash_table_lookup_extended (disc->priv->restored,
				      uri,
				      &key,
				      &status);
	g_hash_table_remove (disc->priv->restored, uri);
	g_mutex_unlock (disc->priv->restored_lock);

	if (!g_hash_table_lookup (disc->priv->dirs, uri)
	&&  !g_hash_table_lookup (disc->priv->files, uri))
		g_free (key);

	if (!g_hash_table_size (disc->priv->restored)) {
		if (!disc->priv->unreadable) {
			gtk_widget_set_sensitive (disc->priv->filter_button,
						  FALSE);
		}
		g_hash_table_destroy (disc->priv->restored);
		disc->priv->restored = NULL;
	}

	return GPOINTER_TO_INT (status);
}

/****************************** filtered dialog ********************************/
static gboolean
brasero_data_disc_unreadable_dialog (BraseroDataDisc *disc,
				     const char *uri,
				     GnomeVFSResult result,
				     gboolean isdir)
{
	char *name;
	char *message_disc;
	GtkWidget *toplevel;
	GtkWidget *dialog;
	guint answer;

	name = g_filename_display_basename (uri);

	if (!isdir)
		message_disc = g_strdup_printf (_("The file \"%s\" is unreadable:"), name);
	else
		message_disc = g_strdup_printf (_("The directory \"%s\" is unreadable:"), name);

	g_free (name);

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (disc));
	dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					 GTK_DIALOG_DESTROY_WITH_PARENT |
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_CLOSE,
					 message_disc);
	g_free (message_disc);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Unreadable file"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  "%s.",
						  gnome_vfs_result_to_string (result));

	gtk_widget_show_all (dialog);
	answer = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	return TRUE;
}
static gboolean
brasero_data_disc_restore_unreadable (BraseroDataDisc *disc,
				      const gchar *uri,
				      GnomeVFSResult result,
				      GnomeVFSFileInfo *info,
				      GSList *references)
{
	GSList *paths;
	char *path;

	/* NOTE: it can only be unreadable here or perhaps a
	 * recursive symlink if it was broken symlinks and a
	 * target was added afterwards */
	if (result != GNOME_VFS_OK
	|| !brasero_data_disc_is_readable (info)) {
		brasero_data_disc_remove_uri (disc, uri, TRUE);
		brasero_data_disc_unreadable_new (disc,
						  g_strdup (uri),
						  BRASERO_FILTER_UNREADABLE);
		return FALSE;
	}

	/* start restoring : see if we still want to update */
	if (brasero_data_disc_is_excluded (disc, uri, NULL))
		return TRUE;

	/* NOTE : the file could be in dirs or files (for example if it's a 
	 * hidden file) if the user has explicitly grafted it in the tree */
	if (info->type != GNOME_VFS_FILE_TYPE_DIRECTORY
	&& !g_hash_table_lookup (disc->priv->files, uri)) {
		gchar *parent;
		gint64 sectors;
		BraseroFile *file;

		parent = g_path_get_dirname (uri);
		file = g_hash_table_lookup (disc->priv->dirs, parent);
		g_free (parent);

		sectors = GET_SIZE_IN_SECTORS (info->size);

		if (file && file->sectors >= 0)
			file->sectors += sectors;

		brasero_data_disc_size_changed (disc, sectors);
	}
	else if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY
	      && !g_hash_table_lookup (disc->priv->dirs, uri))
		brasero_data_disc_directory_new (disc,
						 g_strdup (uri),
						 TRUE);

	/* now let's see the tree */
	paths = brasero_data_disc_reference_get_list (disc, references, FALSE);
	for (; paths; paths = g_slist_remove (paths, path)) {
		path = paths->data;
		brasero_data_disc_tree_set_path_from_info (disc, path, NULL, info);
		g_free (path);
	}

	return TRUE;
}

static gboolean
brasero_data_disc_restore_unreadable_cb (BraseroDataDisc *disc,
					 GSList *results,
					 gpointer callback_data)
{
	GSList *referencess = callback_data;
	BraseroInfoAsyncResult *result;
	GSList *references;
	GSList *unreadable;
	gboolean success;
	GSList *iter;

	unreadable = NULL;
	for (iter = referencess ; results && iter; results = results->next, iter = iter->next) {
		references = iter->data;
		result = results->data;

		success = brasero_data_disc_restore_unreadable (disc,
								result->uri,
								result->result,
								result->info,
								references);

		if (!success)	/* keep for later to tell the user it didn't go on well */
			unreadable = g_slist_prepend (unreadable, result);
		else
			brasero_data_disc_selection_changed (disc, TRUE);
	}

	if (unreadable) {
		GtkWidget *dialog;
		GtkWidget *toplevel;

		/* tell the user about the files that are definitely unreadable */
		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (disc));
		dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
						 GTK_DIALOG_DESTROY_WITH_PARENT |
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_CLOSE,
						 _("Some files couldn't be restored."));

		gtk_window_set_title (GTK_WINDOW (dialog), _("File restoration failure"));

		for (; unreadable; unreadable = g_slist_remove (unreadable, unreadable->data)) {
			result = unreadable->data;
			g_warning ("ERROR : file \"%s\" couldn't be restored : %s\n",
				   result->uri,
				   gnome_vfs_result_to_string (result->result));
		}

		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	}

	/* free callback_data */
	return TRUE;
}

static void
brasero_data_disc_filtered_destroy_cb (BraseroDataDisc *disc,
				       gpointer callback_data)
{
	GSList *referencess = callback_data;
	GSList *iter;

	for (iter = referencess; iter; iter = iter->next) {
		GSList *references;

		references = iter->data;
		brasero_data_disc_reference_free_list (disc, references);
	}
	g_slist_free (referencess);
}

static void
brasero_data_disc_filtered_restore (BraseroDataDisc *disc,
				    GSList *restored)
{
	GSList *referencess;
	GSList *references;
	GSList *paths;
	GSList *iter;
	GSList *uris;
	char *uri;

	referencess = NULL;
	uris = NULL;
	for (; restored; restored = restored->next) {
		uri = restored->data;

		/* we filter out those that haven't changed */
		if (disc->priv->restored
		&&  g_hash_table_lookup (disc->priv->restored, uri))
			continue;

		references = NULL;
		paths = brasero_data_disc_uri_to_paths (disc, uri, TRUE);
		for (iter = paths; iter; iter = iter->next) {
			BraseroDataDiscReference ref;
			char *path_uri;
			char *path;

			path = iter->data;
			path_uri = g_hash_table_lookup (disc->priv->paths, path);
			if (path_uri) {
				char *graft;
				char *parent;
		
				/* see if it's not this uri that is grafted there */
				if (!strcmp (path_uri, uri))
					continue;

				parent = g_path_get_dirname (path);
				graft = brasero_data_disc_graft_get (disc, parent);
				g_free (parent);
		
				brasero_data_disc_exclude_uri (disc, graft, uri);
				g_free (graft);
				continue;
			}

			brasero_data_disc_tree_new_loading_row (disc, path);
			ref = brasero_data_disc_reference_new (disc, path);
			references = g_slist_prepend (references, GINT_TO_POINTER (ref));
		}
		g_slist_foreach (paths, (GFunc) g_free, NULL);
		g_slist_free (paths);

		if (references) {
			brasero_data_disc_restored_new (disc, uri, 0);
			uris = g_slist_prepend (uris, uri);
			referencess = g_slist_prepend (referencess, references);
		}
	}

	if (uris) {
		BraseroDiscResult success;

		/* NOTE: uri in uris are destroyed by calling function */
		success = brasero_data_disc_get_info_async (disc,
							    uris,
							    GNOME_VFS_FILE_INFO_GET_ACCESS_RIGHTS |
							    GNOME_VFS_FILE_INFO_GET_MIME_TYPE |
							    GNOME_VFS_FILE_INFO_FORCE_SLOW_MIME_TYPE,
							    brasero_data_disc_restore_unreadable_cb,
							    referencess,
							    brasero_data_disc_filtered_destroy_cb);
		g_slist_free (uris);
	}
}

static void
_foreach_add_restored (char *uri,
		       BraseroFilterStatus status,
		       BraseroFilteredDialog *dialog)
{
	if (status == BRASERO_FILTER_UNKNOWN)
		return;

	brasero_filtered_dialog_add (dialog, uri, TRUE, status);
}

static void
_foreach_add_unreadable (char *uri,
			 BraseroFilterStatus status,
			 BraseroFilteredDialog *dialog)
{
	brasero_filtered_dialog_add (dialog, uri, FALSE, status);
}

static void
brasero_data_disc_filtered_files_clicked_cb (GtkButton *button,
					     BraseroDataDisc *disc)
{
	GSList *restored;
	GSList *removed;

	disc->priv->filter_dialog = brasero_filtered_dialog_new ();
	if (disc->priv->restored)
		g_hash_table_foreach (disc->priv->restored,
				      (GHFunc) _foreach_add_restored,
				      disc->priv->filter_dialog);

	if (disc->priv->unreadable)
		g_hash_table_foreach (disc->priv->unreadable,
				      (GHFunc) _foreach_add_unreadable,
				      disc->priv->filter_dialog);

	gtk_window_set_transient_for (GTK_WINDOW (disc->priv->filter_dialog), 
				      GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (disc))));
	gtk_window_set_modal (GTK_WINDOW (disc->priv->filter_dialog), TRUE);
	gtk_widget_show_all (disc->priv->filter_dialog);
	gtk_dialog_run (GTK_DIALOG (disc->priv->filter_dialog));

	/* lets get the list of uri that were restored */
	brasero_filtered_dialog_get_status (BRASERO_FILTERED_DIALOG (disc->priv->filter_dialog),
					    &restored,
					    &removed);
	gtk_widget_destroy (disc->priv->filter_dialog);
	disc->priv->filter_dialog = NULL;

	brasero_data_disc_filtered_restore (disc, restored);
	g_slist_foreach (restored, (GFunc) g_free, NULL);
	g_slist_free (restored);

	for (; removed; removed = g_slist_remove (removed, removed->data)) {
		BraseroFilterStatus status;
		gchar *uri;

		uri = removed->data;

		if (disc->priv->unreadable
		&&  g_hash_table_lookup (disc->priv->unreadable, uri)) {
			g_free (uri);
			continue;
		}

		status = brasero_data_disc_restored_free (disc, uri);
		brasero_data_disc_remove_uri (disc, uri, TRUE);
		brasero_data_disc_unreadable_new (disc, uri, status);
		brasero_data_disc_selection_changed (disc, TRUE);
	}
}

/**************************** file/dir freeing *********************************/
static gboolean
brasero_data_disc_file_object_free (BraseroDataDisc *disc,
				    BraseroFile *file)
{
	GSList *excluding;
	BraseroFilterStatus status;

	if (disc->priv->restored
	&&  (status = GPOINTER_TO_INT (g_hash_table_lookup (disc->priv->restored, file->uri))))
		g_hash_table_replace (disc->priv->restored,
				      g_strdup (file->uri),
				      GINT_TO_POINTER (status));

	/* see if any excluding key points to it */
	if (disc->priv->excluded
	&& (excluding = g_hash_table_lookup (disc->priv->excluded, file->uri)))
		g_hash_table_replace (disc->priv->excluded,
				      g_strdup (file->uri),
				      excluding);

#ifdef BUILD_INOTIFY

	if (!brasero_data_disc_cancel_monitoring (disc, file)) {
		/* we still need this directory for monitoring children files
		 * as a dummy */
		return FALSE;
	}

#endif
	
	/* now we can safely free the structure */
	g_free (file->uri);
	g_free (file);
	return TRUE;
}

static void
brasero_data_disc_file_free (BraseroDataDisc *disc,
			     BraseroFile *file)
{
	if (!brasero_data_disc_is_excluded (disc, file->uri, NULL)) {
		BraseroFile *parent;
		char *parent_uri;

		parent_uri = g_path_get_dirname (file->uri);
		parent = g_hash_table_lookup (disc->priv->dirs, parent_uri);
		g_free (parent_uri);

		/* NOTE: no need to check whether it's a dummy directory,
		 * brasero_data_is_excluded already did it */

		parent->sectors += file->sectors;
	}
	else
		brasero_data_disc_size_changed (disc, file->sectors * (-1));

	brasero_data_disc_file_object_free (disc, file);
}

static void
brasero_data_disc_file_remove_from_tree (BraseroDataDisc *disc,
					 BraseroFile *file)
{
	brasero_data_disc_remove_uri_from_tree (disc, file->uri, TRUE);

	/* remove all file graft points if any and free */
	brasero_data_disc_graft_remove_all (disc, file->uri);
	g_hash_table_remove (disc->priv->files, file->uri);
	brasero_data_disc_file_free (disc, file);
}

static gboolean
_foreach_unreadable_remove (char *uri,
			    BraseroFilterStatus status,
			    BraseroDataDisc *disc)
{
	char *parent_uri;
	BraseroFile *parent;

	parent_uri = g_path_get_dirname (uri);
	parent = g_hash_table_lookup (disc->priv->dirs, parent_uri);
	g_free (parent_uri);

	if (!parent || parent->sectors < 0)
		return TRUE;

	return FALSE;
}

static gboolean
_foreach_remove_symlink_children_cb (char *symlink,
				     int value,
				     BraseroDataDisc *disc)
{
	char *parent_uri;
	BraseroFile *parent;

	parent_uri = g_path_get_dirname (symlink);
	parent = g_hash_table_lookup (disc->priv->dirs, parent_uri);
	g_free (parent_uri);

	if (!parent || parent->sectors < 0)
		return TRUE;

	return FALSE;
}

static gboolean
_foreach_restored_remove (char *uri,
			  BraseroFilterStatus status,
			  BraseroDataDisc *disc)
{
	char *parent_uri;
	BraseroFile *parent;

	parent_uri = g_path_get_dirname (uri);
	parent = g_hash_table_lookup (disc->priv->dirs, parent_uri);
	g_free (parent_uri);

	if (!parent || parent->sectors < 0) {
		if (!g_hash_table_lookup (disc->priv->files, uri)
		&&  !g_hash_table_lookup (disc->priv->dirs, uri))
			g_free (uri);

		return TRUE;
	}

	return FALSE;
}

static void
brasero_data_disc_update_hashes (BraseroDataDisc *disc)
{
	/* NOTE: if we wanted to get rid of references then that would be the 
	 * best place to see in grafts hash table the grafted children that 
	 * lack a parent directory for monitoring */

	/* remove unreadable children */
	if (disc->priv->unreadable) {
		g_hash_table_foreach_remove (disc->priv->unreadable,
					     (GHRFunc) _foreach_unreadable_remove,
					     disc);

		if (!g_hash_table_size (disc->priv->unreadable)) {
			g_hash_table_destroy (disc->priv->unreadable);
			disc->priv->unreadable = NULL;
			if (!disc->priv->restored) {
				gtk_widget_set_sensitive (disc->priv->filter_button,
							  FALSE);
			}
		}
	}

	/* remove symlink children */
	if (disc->priv->symlinks) {
		g_hash_table_foreach_remove (disc->priv->symlinks,
					     (GHRFunc) _foreach_remove_symlink_children_cb,
					     disc);

		if (!g_hash_table_size (disc->priv->symlinks)) {
			g_hash_table_destroy (disc->priv->symlinks);
			disc->priv->symlinks = NULL;
		}
	}

	/* remove restored file children */
	if (disc->priv->restored) {
		g_mutex_lock (disc->priv->restored_lock);
		g_hash_table_foreach_remove (disc->priv->restored,
					     (GHRFunc) _foreach_restored_remove,
					     disc);

		if (!g_hash_table_size (disc->priv->restored)) {
			g_hash_table_destroy (disc->priv->restored);
			disc->priv->restored = NULL;
			if (!disc->priv->unreadable) {
				gtk_widget_set_sensitive (disc->priv->filter_button,
							  FALSE);
			}
		}
		g_mutex_unlock (disc->priv->restored_lock);
	}
}

static gboolean
brasero_data_disc_dir_free (BraseroDataDisc *disc,
			    BraseroFile *dir)
{
	GSList *references;

	/* remove potential references */
	if (disc->priv->references
	&& (references = g_hash_table_lookup (disc->priv->references, dir->uri))) {
		BraseroDataDiscReference ref;

		references = g_hash_table_lookup (disc->priv->references, dir->uri);
		g_mutex_lock (disc->priv->references_lock);
		g_hash_table_remove (disc->priv->references, dir->uri);
		if (!g_hash_table_size (disc->priv->references)) {
			g_hash_table_destroy (disc->priv->references);
			disc->priv->references = NULL;
		}
		g_mutex_unlock (disc->priv->references_lock);

		for (; references; references = g_slist_remove (references, GINT_TO_POINTER (ref))) {
			ref = GPOINTER_TO_INT (references->data);
			brasero_data_disc_reference_free (disc, ref);
		}
	}

	/* remove it from the rescan list */
	disc->priv->rescan = g_slist_remove (disc->priv->rescan, dir);

	/* it could be in the waiting list remove it */
	disc->priv->loading = g_slist_remove (disc->priv->loading, dir);

	/* update the size */
	brasero_data_disc_size_changed (disc, dir->sectors * (-1));

	return brasero_data_disc_file_object_free (disc, dir);
}


struct _BraseroRemoveChildrenData {
	int len;
	BraseroFile *dir;
	GSList *dirs;
	GSList *files;
	char *graft;
	BraseroDataDisc *disc;
};
typedef struct _BraseroRemoveChildrenData BraseroRemoveChildrenData;

static void
_foreach_remove_children_dirs_cb (const char *uri,
				  BraseroFile *dir,
				  BraseroRemoveChildrenData *data)
{
	/* dummy directory: since they are always top directories,
	 * it can't be a child. It is only removed when all its 
	 * children are removed */
	if (dir->sectors < 0)
		return;

	if (!strncmp (dir->uri, data->dir->uri, data->len)
	&& *(dir->uri + data->len) == G_DIR_SEPARATOR) {
		/* make sure that this children is not grafted somewhere else
		 * this can't be under dir since we removed all children grafted
		 * points so just check that it hasn't graft points neither any
		 * parent of this directory up to data->uri */
		if (!brasero_data_disc_is_excluded (data->disc,
						    dir->uri,
						    data->dir))
			return;

		/* no need to g_strdup the uri, the hash won't be touched in
		 * between. The list is just to workaround the problem raised
		 * by g_hash_table_foreach_remove (which would work fine
		 * otherwise). See below. */
		data->dirs = g_slist_prepend (data->dirs, dir);
	}
}

static void
brasero_data_disc_remove_dir_and_children (BraseroDataDisc *disc,
					   BraseroFile *dir)
{
	BraseroRemoveChildrenData callback_data;
	GSList *iter;

	/* we remove all children from dirs hash table */
	/* NOTE: we don't use g_hash_table_foreach_remove on purpose here:
	 * if the directory is going to become a dummy the latter function
	 * would remove it after brasero_data_disc_cancel_monitoring has
	 * called g_hash_table_insert which wouldn't work and lead to leak */
	callback_data.dir = dir;
	callback_data.len = strlen (dir->uri);
	callback_data.disc = disc;
	callback_data.dirs = NULL;
	g_hash_table_foreach (disc->priv->dirs,
			      (GHFunc) _foreach_remove_children_dirs_cb,
			      &callback_data);

	for (iter = callback_data.dirs; iter; iter = iter->next) {
		BraseroFile *dir;

		dir = iter->data;
		g_hash_table_remove (disc->priv->dirs, dir->uri);
		brasero_data_disc_dir_free (disc, dir);
	}
	g_slist_free (callback_data.dirs);

	/* remove the directory itself from the hash table */
	g_hash_table_remove (disc->priv->dirs, dir->uri);
	brasero_data_disc_dir_free (disc, dir);
	brasero_data_disc_update_hashes (disc);
}

static void
brasero_data_disc_directory_remove_from_tree (BraseroDataDisc *disc,
					      BraseroFile *dir)
{
	GSList *paths;
	GSList *iter;
	char *path;

	/* we need to remove all occurence of file in the tree */
	paths = brasero_data_disc_uri_to_paths (disc, dir->uri, TRUE);

	/* remove all graft points for this dir 
	 * NOTE: order is important otherwise the above function would not 
	 * add the graft points path.
	 * NOTE: we don't care for children since we will have a notification */
	brasero_data_disc_graft_children_remove (disc, paths);
	brasero_data_disc_graft_remove_all (disc, dir->uri);

	for (iter = paths; iter; iter = iter->next) {
		path = iter->data;

		brasero_data_disc_tree_remove_path (disc, path);
		g_free (path);
	}

	g_slist_free (paths);
	brasero_data_disc_remove_dir_and_children (disc, dir);
}

static void
brasero_data_disc_remove_children_destroy_cb (BraseroDataDisc *disc,
					      gpointer callback_data)
{
	g_free (callback_data);
}

static gboolean
brasero_data_disc_remove_children_async_cb (BraseroDataDisc *disc,
					    GSList *results,
					    gpointer callback_data)
{
	BraseroInfoAsyncResult *result;
	gchar *uri_dir = callback_data;
	GnomeVFSFileInfo *info;
	BraseroFile *dir;
	char *parent;
	char *uri;

	dir = g_hash_table_lookup (disc->priv->dirs, uri_dir);
	if (!dir && dir->sectors < 0)
		return TRUE;

	for (; results; results = results->next) {
		gint64 sectors;

		result = results->data;

		/* NOTE: we don't care if it is still excluded or not:
		 * if it's no longer excluded it means that one of his parents
		 * has been added again and so there is a function that is going
		 * to add or has added its size already */
		uri = result->uri;

		parent = g_path_get_dirname (uri);
		dir = g_hash_table_lookup (disc->priv->dirs, uri);
		g_free (parent);

		if (!dir || dir->sectors < 0)
			continue;

		info = result->info;
		if (result->result == GNOME_VFS_ERROR_NOT_FOUND) {
			brasero_data_disc_add_rescan (disc, dir);
			continue;
		}

		/* There shouldn't be any symlink so no need to check for loop */
		if (result->result != GNOME_VFS_OK) {
			/* we don't remove it from excluded in case it appears again */
			brasero_data_disc_add_rescan (disc, dir);
			brasero_data_disc_unreadable_new (disc,
							  g_strdup (uri),
							  BRASERO_FILTER_UNREADABLE);
			continue;
		}

		/* we update the parent directory */
		sectors = GET_SIZE_IN_SECTORS (info->size);
		dir->sectors -= sectors;
		brasero_data_disc_size_changed (disc, sectors * (-1));
	}

	/* Free callback_data */
	return TRUE;
}

static void
_foreach_remove_children_files_cb (char *uri,
				   GSList *excluding,
				   BraseroRemoveChildrenData *data)
{
	BraseroFile *dir;
	int excluding_num;
	int grafts_num;
	char *parent;
	GSList *list;

	if (data->disc->priv->unreadable
	&&  g_hash_table_lookup (data->disc->priv->unreadable, uri))
		return;

	/* we only want the children */
	if (strncmp (uri, data->dir->uri, data->len)
	||  *(uri + data->len) != G_DIR_SEPARATOR)
		return;

	/* we don't want those with graft points (they can't be excluded
	 * and won't be even if we remove one of their parent graft point */
	if (g_hash_table_lookup (data->disc->priv->grafts, uri))
		return;

	/* we make a list of the graft points that exclude the file
	 * or one of its parents : for this file to be strictly excluded,
	 * the list must contains all parent graft points but the one we
	 * are going to remove */
	if (g_slist_find (excluding, data->graft))
		return;

	excluding_num = g_slist_length (excluding);
	grafts_num = 0;

	parent = g_path_get_basename (uri);
	dir = g_hash_table_lookup (data->disc->priv->dirs, parent);
	g_free (parent);

	while (dir && dir->sectors < 0) {
		list = g_hash_table_lookup (data->disc->priv->excluded, dir->uri);
		if (list) {
			if (g_slist_find (list, data->graft))
				return;

			excluding_num += g_slist_length (list);
		}

		/* NOTE: data->graft should have been previously removed from grafts */
		list = g_hash_table_lookup (data->disc->priv->grafts, dir->uri);
		if (list) {
			grafts_num += g_slist_length (list);
			if (grafts_num > excluding_num)
				return;
		}

		parent = g_path_get_dirname (dir->uri);
		dir = g_hash_table_lookup (data->disc->priv->dirs, parent);
		g_free (parent);
	}

	/* NOTE: it can't be uris in files as they couldn't 
	 * be excluded anyway since they have graft points */
	dir = g_hash_table_lookup (data->disc->priv->dirs, uri);
	if (dir && dir->sectors < 0)
		data->dirs = g_slist_prepend (data->dirs, g_strdup (uri));
	else 
		data->files = g_slist_prepend (data->files, g_strdup (uri));
}

static void
brasero_data_disc_remove_children (BraseroDataDisc *disc,
				   BraseroFile *dir,
				   const char *graft)
{
	BraseroRemoveChildrenData callback_data;
	BraseroDiscResult result;
	BraseroFile *file;
	GSList *iter;
	char *uri;

	if (!disc->priv->excluded)
		return;

	/* we remove all children from dirs hash table */
	callback_data.dir = dir;
	callback_data.len = strlen (dir->uri);
	callback_data.graft = (char *) graft;
	callback_data.disc = disc;
	callback_data.dirs = NULL;
	callback_data.files = NULL;

	g_hash_table_foreach (disc->priv->excluded,
			      (GHFunc) _foreach_remove_children_files_cb,
			      &callback_data);

	for (iter = callback_data.dirs; iter; iter = g_slist_remove (iter, uri)) {
		uri = iter->data;

		/* make sure it still exists (it could have been destroyed 
		 * with a parent in brasero_data_disc_remove_dir_and_children */
		if ((file = g_hash_table_lookup (disc->priv->dirs, uri))
		&&   file->sectors >= 0) {
			g_hash_table_remove (disc->priv->dirs, file->uri);
			brasero_data_disc_dir_free (disc, file);
		}
	
		g_free (uri);
	}
	brasero_data_disc_update_hashes (disc);

	if (!callback_data.files)
		return;

	uri = g_strdup (dir->uri);
	result = brasero_data_disc_get_info_async (disc,
						   callback_data.files,
						   0,
						   brasero_data_disc_remove_children_async_cb,
						   g_strdup (dir->uri),
						   brasero_data_disc_remove_children_destroy_cb);
	g_slist_foreach (callback_data.files, (GFunc) g_free, NULL);
	g_slist_free (callback_data.files);

	if (result != BRASERO_DISC_OK)
		g_free (uri);
}

static void
brasero_data_disc_remove_uri (BraseroDataDisc *disc,
			      const gchar *uri,
			      gboolean include_grafted)
{
	BraseroFile *file;

	if ((file = g_hash_table_lookup (disc->priv->dirs, uri))
	&&   file->sectors >= 0) {
		if (include_grafted
		|| !g_hash_table_lookup (disc->priv->grafts, uri))
			brasero_data_disc_directory_remove_from_tree (disc, file);
		else
			brasero_data_disc_remove_uri_from_tree (disc, uri, FALSE);
	}
	else if ((file = g_hash_table_lookup (disc->priv->files, uri))) {
		if  (include_grafted)
			brasero_data_disc_file_remove_from_tree (disc, file);
		else
			brasero_data_disc_remove_uri_from_tree (disc, uri, FALSE);
	}
	else if (disc->priv->unreadable
	      &&  g_hash_table_lookup (disc->priv->unreadable, uri)) {
		/* it's an unreadable file */
		brasero_data_disc_unreadable_free (disc, uri);
	}
	else if (disc->priv->symlinks
	     &&  g_hash_table_lookup (disc->priv->symlinks, uri)) {
		g_hash_table_remove (disc->priv->symlinks, uri);
	
		if (!g_hash_table_size (disc->priv->symlinks)) {
			g_hash_table_destroy (disc->priv->symlinks);
			disc->priv->symlinks = NULL;
		}
	}
	else if (!brasero_data_disc_is_excluded (disc, uri, NULL)) {
		BraseroFile *parent;
		char *parent_uri;

		/* NOTE: excluded files are already removed */
		parent_uri = g_path_get_dirname (uri);
		parent = g_hash_table_lookup (disc->priv->dirs, parent_uri);
		g_free (parent_uri);

		if (!parent || parent->sectors < 0)
			return;

		brasero_data_disc_remove_uri_from_tree (disc,
							uri,
							include_grafted);

		brasero_data_disc_add_rescan (disc, parent);
	}

	/* NOTE: if file was excluded and so not taken into account
	 * anyway => no need for rescanning. we don't care if it is
	 * deleted.
	 * NOTE: we don't delete them from excluded list in case
	 * they come up again i.e in case they are moved back */
}

/******************************** graft points *********************************/
static const char *
brasero_data_disc_graft_get_real (BraseroDataDisc *disc,
				  const char *path)
{
	char *tmp;
	char *parent;
	gpointer key = NULL;

	if (g_hash_table_lookup_extended (disc->priv->paths,
					  path,
					  &key,
					  NULL))
		return key;

	parent = g_path_get_dirname (path);
	while (parent [1] != '\0'
	&& !g_hash_table_lookup_extended (disc->priv->paths,
					  parent,
					  &key,
					  NULL)) {
	
		tmp = parent;
		parent = g_path_get_dirname (parent);
		g_free (tmp);
	}
	g_free (parent);

	return key;
}

static char *
brasero_data_disc_graft_get (BraseroDataDisc *disc,
			     const char *path)
{
	return g_strdup (brasero_data_disc_graft_get_real (disc, path));
}

static gboolean
brasero_data_disc_graft_new (BraseroDataDisc *disc,
			     const char *uri,
			     const char *graft)
{
	gchar *realgraft;
	GSList *grafts;
	gchar *realuri;

	if (g_hash_table_lookup (disc->priv->paths, graft))
		return FALSE;

	realgraft = g_strdup (graft);

	if (uri) {
		BraseroFile *file;

		if (!(file = g_hash_table_lookup (disc->priv->dirs, uri)))
			file = g_hash_table_lookup (disc->priv->files, uri);

		realuri = file->uri;
	}
	else
		realuri = BRASERO_CREATED_DIR;

	g_hash_table_insert (disc->priv->paths,
			     realgraft,
			     realuri);

	if (realuri == BRASERO_CREATED_DIR)
		return TRUE;

	grafts = g_hash_table_lookup (disc->priv->grafts, realuri);
	grafts = g_slist_prepend (grafts, realgraft);
	g_hash_table_insert (disc->priv->grafts,
			     realuri,
			     grafts);
	
	return TRUE;
}

static GSList *
brasero_data_disc_graft_new_list (BraseroDataDisc *disc,
				  const char *uri,
				  GSList *grafts)
{
	char *graft;
	GSList *next;
	GSList *iter;

	for (iter = grafts; iter; iter = next) {
		graft = iter->data;
		next = iter->next;

		if (!brasero_data_disc_graft_new (disc, uri, graft)) {
			grafts = g_slist_remove (grafts, graft);
			g_free (graft);
		}	
	}

	return grafts;
}

struct _BraseroRemoveGraftPointersData {
	char *graft;
	GSList *list;
};
typedef struct _BraseroRemoveGraftPointersData BraseroRemoveGraftPointersData;

static void
_foreach_remove_graft_pointers_cb (char *key,
				   GSList *excluding,
				   BraseroRemoveGraftPointersData *data)
{
	/* we can't change anything in this function
	 * we simply make a list of keys with this graft */
	if (g_slist_find (excluding, data->graft))
		data->list = g_slist_prepend (data->list, key);
}

static void
brasero_data_disc_graft_clean_excluded (BraseroDataDisc *disc,
					      const char *graft)
{
	char *uri;
	GSList *iter;
	BraseroRemoveGraftPointersData callback_data;

	if (!disc->priv->excluded)
		return;

	/* we need to remove any pointer to the graft point from the excluded hash */
	callback_data.graft = (char *) graft;
	callback_data.list = NULL;
	g_hash_table_foreach (disc->priv->excluded,
			      (GHFunc) _foreach_remove_graft_pointers_cb,
			      &callback_data);

	for (iter = callback_data.list; iter; iter = iter->next) {
		GSList *excluding;
		BraseroFile *dir;

		uri = iter->data;
		excluding = g_hash_table_lookup (disc->priv->excluded,
						 uri);
		excluding = g_slist_remove (excluding, graft);

		if (!excluding) {
			g_hash_table_remove (disc->priv->excluded,
					     uri);

			dir = g_hash_table_lookup (disc->priv->dirs, uri);
			if ((dir == NULL || dir->sectors < 0)
			&&  !g_hash_table_lookup (disc->priv->files, uri))
				g_free (uri);
		}
		else
			g_hash_table_insert (disc->priv->excluded,
					     uri, excluding);
	}

	if (!g_hash_table_size (disc->priv->excluded)) {
		g_hash_table_destroy (disc->priv->excluded);
		disc->priv->excluded = NULL;
	}

	g_slist_free (callback_data.list);
}

static gboolean
brasero_data_disc_graft_remove (BraseroDataDisc *disc,
				const char *path)
{
	BraseroFile *file;
	gpointer oldgraft = NULL;
	gpointer uri = NULL;
	GSList *grafts;

	if (!g_hash_table_lookup_extended (disc->priv->paths,
					   path,
					   &oldgraft,
					   &uri))
		return FALSE;

	if (uri == BRASERO_CREATED_DIR)
		goto end;

	grafts = g_hash_table_lookup (disc->priv->grafts, uri);
	grafts = g_slist_remove (grafts, oldgraft);

	/* NOTE: in this function we don't check for dummy directory since
	 * we get the uri from paths hash table so this is a bug if it
	 * returns the uri of dummy directory */
	if (grafts) {
		g_hash_table_insert (disc->priv->grafts, uri, grafts);
		if ((file = g_hash_table_lookup (disc->priv->dirs, uri))) {
			brasero_data_disc_remove_children (disc,
							   file,
							   oldgraft);
			brasero_data_disc_graft_clean_excluded (disc, oldgraft);
		}

		goto end;
	}

	g_hash_table_remove (disc->priv->grafts, uri);
	file = g_hash_table_lookup (disc->priv->files, uri);
	if (file) {
		g_hash_table_remove (disc->priv->files, file->uri);
		brasero_data_disc_file_free (disc, file);
		goto end;
	}

	file = g_hash_table_lookup (disc->priv->dirs, uri);
	if (brasero_data_disc_is_excluded (disc, uri, NULL)) {
		brasero_data_disc_graft_clean_excluded (disc, oldgraft);
		brasero_data_disc_remove_dir_and_children (disc, file);
	}
	else {
		brasero_data_disc_remove_children (disc, file, oldgraft);
		brasero_data_disc_graft_clean_excluded (disc, oldgraft);
	}

end:

	g_hash_table_remove (disc->priv->paths, oldgraft);
	g_free (oldgraft);

	return TRUE;
}

static void
brasero_data_disc_graft_remove_all (BraseroDataDisc *disc,
				    const char *uri)
{
	GSList *grafts;
	char *graft;

	grafts = g_hash_table_lookup (disc->priv->grafts, uri);
	if (!grafts)
		return;

	g_hash_table_remove (disc->priv->grafts, uri);
	for (; grafts; grafts = g_slist_remove (grafts, graft)) {
		graft = grafts->data;

		brasero_data_disc_graft_clean_excluded (disc, graft);
		g_hash_table_remove (disc->priv->paths, graft);
		g_free (graft);
	}
}

struct _BraseroMoveGraftChildData {
	gint len;
	gchar *uri;
	gchar *oldgraft;
	gchar *newgraft;
	GSList *paths;
	GSList *uris;
	BraseroDataDisc *disc;
};
typedef struct _BraseroMoveGraftChildData BraseroMoveGraftChildData;

static void
_foreach_graft_changed_cb (char *key,
			   GSList *excluding,
			   BraseroMoveGraftChildData *data)
{
	/* the old graft can only be once in excluding */
	for (; excluding; excluding = excluding->next) {
		if (excluding->data == data->oldgraft) {
			excluding->data = data->newgraft;
			return;
		}
	}
}

static gboolean
_foreach_move_children_paths_cb (gchar *graft,
				 gchar *uri,
				 BraseroMoveGraftChildData *data)
{
	if (!strncmp (graft, data->oldgraft, data->len)
	&&  *(graft + data->len) == G_DIR_SEPARATOR) {
		char *newgraft;

		newgraft = g_strconcat (data->newgraft,
					graft + data->len,
					NULL);

		if (uri != BRASERO_CREATED_DIR) {
			GSList *grafts, *node;

			/* be careful with excluded hash */
			if (data->disc->priv->excluded) {
				BraseroMoveGraftChildData callback_data;

				callback_data.oldgraft = graft;
				callback_data.newgraft = newgraft;
				g_hash_table_foreach (data->disc->priv->excluded,
						      (GHFunc) _foreach_graft_changed_cb,
						      &callback_data);
			}

			/* update grafts hash table */
			grafts = g_hash_table_lookup (data->disc->priv->grafts, uri);
			node = g_slist_find (grafts, graft);
			node->data = newgraft;
		}

		/* remove it from this table and add it to the list to insert
		 * path later when finished */
		data->paths = g_slist_prepend (data->paths, newgraft);
		data->uris = g_slist_prepend (data->uris, uri);
		g_free (graft);
		return TRUE;
	}

	return FALSE;
}

static void
brasero_data_disc_graft_children_move (BraseroDataDisc *disc,
				       const char *oldpath,
				       const char *newpath)
{
	BraseroMoveGraftChildData callback_data;
	GSList *paths, *uris;

	callback_data.disc = disc;
	callback_data.paths = NULL;
	callback_data.uris = NULL;
	callback_data.len = strlen (oldpath);
	callback_data.newgraft = (char *) newpath;
	callback_data.oldgraft = (char *) oldpath;

	g_hash_table_foreach_remove (disc->priv->paths,
				     (GHRFunc) _foreach_move_children_paths_cb,
				     &callback_data);

	paths = callback_data.paths;
	uris = callback_data.uris;
	for (; paths && uris; paths = paths->next, uris = uris->next) {
		gchar *path, *uri;

		/* these are the new paths that replace the old ones */
		path = paths->data;
		uri = uris->data;
		g_hash_table_insert (disc->priv->paths,
				     path,
				     uri);
	}

	g_slist_free (callback_data.paths);
	g_slist_free (callback_data.uris);
}

struct _BraseroRemoveGraftedData {
	GSList *paths;
	BraseroDataDisc *disc;
};
typedef struct _BraseroRemoveGraftedData BraseroRemoveGraftedData;

static gboolean
_foreach_unreference_grafted_cb (char *graft,
				 char *uri,
				 BraseroRemoveGraftedData *data)
{
	GSList *iter;
	char *path;
	int len;

	if (graft == BRASERO_CREATED_DIR)
		return FALSE;

	for (iter = data->paths; iter; iter = iter->next) {
		path = iter->data;
		len = strlen (path);

		if (!strncmp (path, graft, len) && *(graft + len) == G_DIR_SEPARATOR) {
			BraseroFile *file;
			GSList *grafts;

			if (uri == BRASERO_CREATED_DIR) {
				g_free (graft);
				return TRUE;
			}

			/* NOTE: the order is important here for 
			 * brasero_data_disc_remove_children */
			grafts = g_hash_table_lookup (data->disc->priv->grafts, uri);
			grafts = g_slist_remove (grafts, graft);
			if (grafts) {
				g_hash_table_insert (data->disc->priv->grafts,
						     uri,
						     grafts);

				/* No need to check for a dummy directory since
				 * we are sure it has grafts and a path */
				if ((file = g_hash_table_lookup (data->disc->priv->dirs, uri))) {
					brasero_data_disc_remove_children (data->disc, file, graft);
					brasero_data_disc_graft_clean_excluded (data->disc, graft);
				}

				g_free (graft);
				return TRUE;
			}
			g_hash_table_remove (data->disc->priv->grafts, uri);

			if ((file = g_hash_table_lookup (data->disc->priv->dirs, uri))) {
				if (brasero_data_disc_is_excluded (data->disc, file->uri, NULL)) {
					brasero_data_disc_graft_clean_excluded (data->disc, graft);
					brasero_data_disc_remove_dir_and_children (data->disc, file);
				}
				else { 
					brasero_data_disc_remove_children (data->disc, file, graft);
					brasero_data_disc_graft_clean_excluded (data->disc, graft);
				}
			}
			else if ((file = g_hash_table_lookup (data->disc->priv->files, uri))) {
				g_hash_table_remove (data->disc->priv->files, uri);
				brasero_data_disc_file_free (data->disc, file);
			}

			g_free (graft);
			return TRUE;
		}
	}

	return FALSE;
}

static void
brasero_data_disc_graft_children_remove (BraseroDataDisc *disc,
					 GSList *paths)
{
	BraseroRemoveGraftedData callback_data;

	callback_data.disc = disc;
	callback_data.paths = paths;

	/* we remove all dirs /files which were grafted inside and which don't have any
	 * more reference. There is no need to see if there are grafted inside the
	 * grafted since they are necessarily the children of path */
	g_hash_table_foreach_remove (disc->priv->paths,
				     (GHRFunc) _foreach_unreference_grafted_cb,
				     &callback_data);
}

static void
brasero_data_disc_graft_changed (BraseroDataDisc *disc,
				 const char *oldpath,
				 const char *newpath)
{
	gpointer oldgraft = NULL;
	gpointer newgraft = NULL;
	gpointer uri = NULL;

	newgraft = g_strdup (newpath);
	g_hash_table_lookup_extended (disc->priv->paths,
				      oldpath,
				      &oldgraft,
				      &uri);

	g_hash_table_remove (disc->priv->paths, oldgraft);
	g_hash_table_insert (disc->priv->paths, newgraft, uri);

	if (uri != BRASERO_CREATED_DIR) {
		GSList *graft_node;
		GSList *grafts;

		grafts = g_hash_table_lookup (disc->priv->grafts, uri);
		graft_node = g_slist_find (grafts, oldgraft);
		graft_node->data = newgraft;
	}

	/* No need to see if it's a dummy since it comes from paths hash table */
	if (disc->priv->excluded
	&&  g_hash_table_lookup (disc->priv->dirs, uri)) {
		BraseroMoveGraftChildData callback_data;

		callback_data.oldgraft = oldgraft;
		callback_data.newgraft = newgraft;
		g_hash_table_foreach (disc->priv->excluded,
				      (GHFunc) _foreach_graft_changed_cb,
				      &callback_data);
	}

	g_free (oldgraft);
}

static void
_foreach_transfer_excluded_cb (const char *uri,
			       GSList *excluding,
			       BraseroMoveGraftChildData *data)
{
	/* only the children of data->uri are interesting */
	if (strncmp (uri, data->uri, data->len)
	||  *(uri + data->len) != G_DIR_SEPARATOR)
		return;

	for (; excluding; excluding = excluding->next) {
		if (excluding->data == data->oldgraft) {
			/* there can only be one */
			excluding->data = data->newgraft;
			return;
		}
	}
}

static void
brasero_data_disc_graft_transfer_excluded (BraseroDataDisc *disc,
					   const char *oldpath,
					   const char *newpath)
{
	BraseroMoveGraftChildData callback_data;
	gpointer newgraft = NULL;
	gpointer oldgraft = NULL;
	gpointer uri = NULL;

	/* NOTE : there shouldn't be nothing else but children here
	 * since we can't exclude anything else but children */

	if (!disc->priv->excluded)
		return;

	if (!g_hash_table_lookup_extended (disc->priv->paths,
					   oldpath,
					   &oldgraft,
					   NULL))
		return;

	if (!g_hash_table_lookup_extended (disc->priv->paths,
					   newpath,
					   &newgraft,
					   &uri))
		return;

	callback_data.uri = uri;
	callback_data.len = strlen (uri);
	callback_data.oldgraft = oldgraft;
	callback_data.newgraft = newgraft;

	g_hash_table_foreach (disc->priv->excluded,
			      (GHFunc) _foreach_transfer_excluded_cb,
			      &callback_data);
}

/********************* convert path to uri and vice versa **********************/
struct _MakeChildrenListData  {
	gchar *parent;
	gint len;
	GSList *children;
};
typedef struct _MakeChildrenListData MakeChildrenListData;

static GSList *
brasero_data_disc_uri_to_paths (BraseroDataDisc *disc,
				const gchar *uri,
				gboolean include_grafts)
{
	gchar *tmp;
	gchar *path;
	gchar *graft;
	gchar *parent;
	GSList *list;
	GSList *iter;
	GSList *grafts;
	GSList *excluding;
	GSList *paths = NULL;

	/* these are the normal paths, that is when the uri's parent is the 
	 * same as on the file system */
	excluding = NULL;
	parent = g_path_get_dirname (uri);
	while (parent [1] != '\0') {
		if (disc->priv->excluded) {
			list = g_hash_table_lookup (disc->priv->excluded, parent);
			list = g_slist_copy (list);
			excluding = g_slist_concat (excluding, list);
		}

		grafts = g_hash_table_lookup (disc->priv->grafts, parent);
		for (; grafts; grafts = grafts->next) {
			graft = grafts->data;

			if (g_slist_find (excluding, graft))
				continue;

			path = g_strconcat (graft,
					    uri + strlen (parent),
					    NULL);

			paths = g_slist_prepend (paths, path);
		}

		tmp = parent;
		parent = g_path_get_dirname (tmp);
		g_free (tmp);
	}
	g_free (parent);
	g_slist_free (excluding);

	if (!include_grafts)
		return paths;

	/* we were asked to add all the grafted paths so here we do */
	grafts = g_hash_table_lookup (disc->priv->grafts, uri);
	for (iter = grafts; iter; iter = iter->next) {
		const gchar *graft;

		graft = iter->data;
		paths = g_slist_prepend (paths, g_strdup (graft));
	}

	return paths;
}

static char *
brasero_data_disc_path_to_uri (BraseroDataDisc *disc,
			       const char *path)
{
	const char *graft;
	char *graft_uri;
	char *retval;

	graft = brasero_data_disc_graft_get_real (disc, path);
	if (!graft)
		return NULL;

	graft_uri = g_hash_table_lookup (disc->priv->paths, graft);

	if (graft_uri == BRASERO_CREATED_DIR)
		return NULL;

	retval = g_strconcat (graft_uri, path + strlen (graft), NULL);
	return retval;
}

static void
_foreach_make_grafted_files_list_cb (gchar *path,
				     const gchar *uri,
				     MakeChildrenListData *data)
{
	if (!strncmp (path, data->parent, data->len)
	&&   path [data->len] == G_DIR_SEPARATOR
	&&  !g_utf8_strchr (path + data->len + 1, -1, G_DIR_SEPARATOR) )
		data->children = g_slist_prepend (data->children, path);
}

static GSList *
brasero_data_disc_path_find_children_grafts (BraseroDataDisc *disc,
					     const gchar *path)
{
	MakeChildrenListData callback_data;

	callback_data.len = strlen (path);
	callback_data.children = NULL;
	callback_data.parent = (gchar *) path;
	g_hash_table_foreach (disc->priv->paths,
			      (GHFunc) _foreach_make_grafted_files_list_cb,
			      &callback_data);

	return callback_data.children;
}

/********************************** new folder *********************************/
static void
brasero_data_disc_new_folder_clicked_cb (GtkButton *button,
					 BraseroDataDisc *disc)
{
	GtkTreeSelection *selection;
	BraseroDiscResult success;
	GtkTreePath *treepath;
	GtkTreeModel *model;
	GtkTreeModel *sort;
	GtkTreeIter iter;
	GList *list;
	char *path;
	char *name;
	int nb;

	if (disc->priv->is_loading)
		return;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (disc->priv->tree));
	sort = disc->priv->sort;
	model = disc->priv->model;
	list = gtk_tree_selection_get_selected_rows (selection, &sort);

	if (g_list_length (list) > 1) {
		g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
		g_list_free (list);
		treepath = NULL;
	}
	else if (!list) {
		treepath = NULL;
	}
	else {
		int explored;
		gboolean isdir;
		GtkTreePath *tmp;

		treepath = list->data;
		g_list_free (list);

		tmp = gtk_tree_model_sort_convert_path_to_child_path (GTK_TREE_MODEL_SORT(sort),
								      treepath);
		gtk_tree_path_free (treepath);
		treepath = tmp;

		gtk_tree_model_get_iter (model, &iter, treepath);
		gtk_tree_model_get (model, &iter,
				    ISDIR_COL, &isdir,
				    ROW_STATUS_COL, &explored,
				    -1);

		if (!isdir 
		||  explored < ROW_EXPLORED) {
			gtk_tree_path_up (treepath);

			if (gtk_tree_path_get_depth (treepath) < 1) {
				gtk_tree_path_free (treepath);
				treepath = NULL;
			}
		}
	}

	name = g_strdup_printf (_("New folder"));
	nb = 1;

      newname:
	success = brasero_data_disc_tree_check_name_validity (disc,
							      name,
							      treepath,
							      FALSE);
	if (success != BRASERO_DISC_OK) {
		g_free (name);
		name = g_strdup_printf (_("New folder %i"), nb);
		nb++;
		goto newname;
	}

	if (treepath) {
		gchar *parent;

		brasero_data_disc_tree_path_to_disc_path (disc,
							  treepath,
							  &parent);

		path = g_build_path (G_DIR_SEPARATOR_S, parent, name, NULL);
		gtk_tree_path_free (treepath);
		g_free (parent);
	}
	else
		path = g_strconcat (G_DIR_SEPARATOR_S, name, NULL);

	if (G_UNLIKELY (strlen (name) > 64))
		brasero_data_disc_joliet_incompat_add_path (disc, path);

	g_free (name);

	brasero_data_disc_graft_new (disc, NULL, path);

	/* just to make sure that tree is not hidden behind info */
	gtk_notebook_set_current_page (GTK_NOTEBOOK (BRASERO_DATA_DISC (disc)->priv->notebook), 1);

	brasero_data_disc_tree_new_empty_folder (disc, path);
	g_free (path);

	brasero_data_disc_selection_changed (disc, TRUE);
}

/************************************ files excluded ***************************/
/* this is done in order:
 * - to minimize memory usage as much as possible ??? if we've got just one excluded ....
 * - to help building of lists of excluded files
 * downside: we need to be careful when we remove a file from the dirs 
 * hash or from the file hash and check that it is not in the excluded hash */
static void
brasero_data_disc_exclude_uri (BraseroDataDisc *disc,
			       const char *path,
			       const char *uri)
{
	gpointer key = NULL;
	gpointer graft = NULL;
	gpointer excluding = NULL;
	BraseroFile *file;

	if (!g_hash_table_lookup_extended (disc->priv->paths,
					   path,
					   &graft,
					   NULL))
		return;

	if (!disc->priv->excluded) {
		/* we create the hash table on demand */
		disc->priv->excluded = g_hash_table_new (g_str_hash, g_str_equal);
	}
	else if (g_hash_table_lookup_extended (disc->priv->excluded,
					       uri,
					       &key,
					       &excluding)) {
		excluding = g_slist_prepend (excluding, graft);
		g_hash_table_insert (disc->priv->excluded,
				     key,
				     excluding);
		return;
	}

	excluding = g_slist_prepend (NULL, graft);

	/* NOTE: we don't need to check for dummy directory as we can 
	 * only exclude children directories and dummy directories are
	 * always top directories */
	if ((file = g_hash_table_lookup (disc->priv->dirs, uri)))
		g_hash_table_insert (disc->priv->excluded,
				     file->uri,
				     excluding);
	else if ((file = g_hash_table_lookup (disc->priv->files, uri)))
		g_hash_table_insert (disc->priv->excluded,
				     file->uri,
				     excluding);
	else
		g_hash_table_insert (disc->priv->excluded,
				     g_strdup (uri),
				     excluding);
}

static void
brasero_data_disc_restore_uri (BraseroDataDisc *disc,
			       const char *path,
			       const char *uri)
{
	gpointer excluding = NULL;
	gpointer graft = NULL;
	gpointer key = NULL;

	if (!disc->priv->excluded)
		return;

	if (!g_hash_table_lookup_extended (disc->priv->paths,
					   path,
					   &graft,
					   NULL))
		return;

	if (!g_hash_table_lookup_extended (disc->priv->excluded,
					   uri, 
					   &key,
					   &excluding))
		return;

	excluding = g_slist_remove (excluding, graft);
	if (excluding) {
		g_hash_table_insert (disc->priv->excluded,
				     key,
				     excluding);
		return;
	}

	if (!g_hash_table_lookup (disc->priv->dirs, key)
	&&  !g_hash_table_lookup (disc->priv->files, key))
		g_free (key);

	g_hash_table_remove (disc->priv->excluded, uri);
	if (g_hash_table_size (disc->priv->excluded))
		return;

	g_hash_table_destroy (disc->priv->excluded);
	disc->priv->excluded = NULL;
}

static gboolean
brasero_data_disc_has_parent (BraseroDataDisc *disc,
			      const gchar *uri,
			      BraseroFile *top)
{
	BraseroFile *dir;
	gint excluding_num;
	gint grafts_num;
	gchar *parent;
	GSList *list;

	parent = g_path_get_dirname (uri);
	dir = g_hash_table_lookup (disc->priv->dirs, parent);
	g_free (parent);

	/* to be strictly excluded it mustn't have parent dir */
	if (!dir || dir->sectors < 0)
		return TRUE;

	/* we make a list of all exclusions up to the top existing directory */
	if (disc->priv->excluded) {
		list = g_hash_table_lookup (disc->priv->excluded, uri);
		excluding_num = g_slist_length (list);
	}
	else
		excluding_num = 0;

	grafts_num = 0;
	while (dir && dir->sectors >= 0 && dir != top) {
		if (disc->priv->excluded) {
			list = g_hash_table_lookup (disc->priv->excluded, dir->uri);
			excluding_num += g_slist_length (list);
		}

		if ((list = g_hash_table_lookup (disc->priv->grafts, dir->uri))) {
			grafts_num += g_slist_length (list);
			if (grafts_num > excluding_num) 
				return FALSE;
		}

		parent = g_path_get_dirname (dir->uri);
		dir = g_hash_table_lookup (disc->priv->dirs, parent);
		g_free (parent);
	}

	grafts_num -= excluding_num;
	if (grafts_num > 0)
		return FALSE;

	return TRUE;
}

static gboolean
brasero_data_disc_is_excluded (BraseroDataDisc *disc,
			       const gchar *uri,
			       BraseroFile *top)
{
	/* to be strictly excluded a files mustn't have graft points */
	if (g_hash_table_lookup (disc->priv->grafts, uri))
		return FALSE;

	return brasero_data_disc_has_parent (disc, uri, top);
}

/************************************** expose row *****************************/
#define EXPOSE_EXCLUDE_FILE(data, uri, path)	\
		(disc->priv->excluded	&& \
		(g_slist_find (g_hash_table_lookup (disc->priv->excluded, uri),	\
			       brasero_data_disc_graft_get_real (data, path)) != NULL))

static void
brasero_data_disc_expose_grafted_destroy_cb (BraseroDataDisc *disc,
					     gpointer callback_data)
{
	GSList *paths = callback_data;
	GSList *iter;

	for (iter = paths; iter; iter = iter->next) {
		BraseroDataDiscReference ref;

		ref = GPOINTER_TO_INT (paths->data);
		brasero_data_disc_reference_free (disc, ref);
	}
	g_slist_free (paths);
}

static gboolean
brasero_data_disc_expose_grafted_cb (BraseroDataDisc *disc,
				     GSList *results,
				     gpointer callback_data)
{
	BraseroDataDiscReference ref;
	BraseroInfoAsyncResult *result;
	GSList *refs = callback_data;
	GnomeVFSFileInfo *info;
	char *path;

	for (; results && refs; results = results->next, refs = refs->next) {
		result = results->data;

		ref = GPOINTER_TO_INT (refs->data);
		info = result->info;

		path = brasero_data_disc_reference_get (disc, ref);
		if (!path)
			continue;

		/* NOTE: we don't need to check if path still corresponds to 
		 * a graft point since the only way for a graft point to be
		 * removed is if notification announced a removal or if the
		 * user explicitly removed the graft in the tree. either way
		 * the references hash table will be updated */

		if (result->result != GNOME_VFS_OK) {
			brasero_data_disc_remove_uri (disc, result->uri, TRUE);
			if (result->result != GNOME_VFS_ERROR_NOT_FOUND)
				brasero_data_disc_unreadable_new (disc,
								  g_strdup (result->uri),
								  BRASERO_FILTER_UNREADABLE);
			g_free (path);
			continue;
		}

		brasero_data_disc_tree_set_path_from_info (disc,
							   path,
							   NULL,
							   info);
		g_free (path);
	}

	/* free callback_data */
	return TRUE;
}

static void
brasero_data_disc_expose_grafted (BraseroDataDisc *disc,
				  GSList *grafts)
{
	char *uri;
	const char *path;
	GSList *uris = NULL;
	GSList *paths = NULL;
	GSList *created = NULL;
	BraseroDataDiscReference ref;

	for (; grafts; grafts = grafts->next) {
		path = grafts->data;

		uri = g_hash_table_lookup (disc->priv->paths, path);
		if (uri == BRASERO_CREATED_DIR) {
			created = g_slist_prepend (created, (char *) path);
			continue;
		}
		else if (!uri)
			continue;

		brasero_data_disc_tree_new_loading_row (disc, path);
		uris = g_slist_prepend (uris, uri);
		ref = brasero_data_disc_reference_new (disc, path);
		paths = g_slist_prepend (paths, GINT_TO_POINTER (ref));
	}

	if (uris) {
		gboolean success;

		/* NOTE: uris come from the hashes => don't free them */
		success = brasero_data_disc_get_info_async (disc,
							    uris,
							    GNOME_VFS_FILE_INFO_GET_MIME_TYPE|
							    GNOME_VFS_FILE_INFO_FORCE_SLOW_MIME_TYPE,
							    brasero_data_disc_expose_grafted_cb,
							    paths,
							    brasero_data_disc_expose_grafted_destroy_cb);
		g_slist_free (uris);
	}

	for (; created; created = g_slist_remove (created, path)) {
		path = created->data;
		brasero_data_disc_tree_new_empty_folder (disc, path);
	}
}

static void
brasero_data_disc_dir_contents_cancel (gpointer data)
{
	BraseroDirectoryContentsData *callback_data = data;

	callback_data->cancel = 1;
}

static void
_free_dir_contents_error (BraseroLoadDirError *error, gpointer null_data)
{
	g_free (error->uri);
	g_free (error);
}

static void
brasero_data_disc_dir_contents_destroy (GObject *object, gpointer data)
{
	BraseroDirectoryContentsData *callback_data = data;

	g_slist_foreach (callback_data->infos, (GFunc) gnome_vfs_file_info_clear, NULL);
	g_slist_foreach (callback_data->infos, (GFunc) gnome_vfs_file_info_unref, NULL);
	g_slist_free (callback_data->infos);

	g_slist_foreach (callback_data->errors, (GFunc) _free_dir_contents_error, NULL);
	g_slist_free (callback_data->errors);

	g_free (callback_data->uri);
	g_free (callback_data);
}

static gboolean
brasero_data_disc_expose_path_real (BraseroDataDisc *disc)
{
	char *uri;
	char *path;
	GSList *iter;
	GSList *paths;
	GSList *infos;
	GSList *references;
	GtkTreePath *treepath;
	GnomeVFSFileInfo *info;
	GSList *treepaths = NULL;
	BraseroDirectoryContentsData *data;

next:
	if (!disc->priv->expose) {
		disc->priv->expose_id = 0;
		return FALSE;
	}

	data = disc->priv->expose->data;
	disc->priv->expose = g_slist_remove (disc->priv->expose, data);

	/* convert all references to paths ignoring the invalid ones */
	if (disc->priv->references
	&& (references = g_hash_table_lookup (disc->priv->references, data->uri))) {
		paths = brasero_data_disc_reference_get_list (disc,
							      references,
							      TRUE);
	}
	else
		paths = NULL;

	if (!paths) {
		brasero_data_disc_decrease_activity_counter (disc);
		brasero_data_disc_dir_contents_destroy (G_OBJECT (disc), data);
		goto next;
	}

	g_mutex_lock (disc->priv->references_lock);
	g_hash_table_remove (disc->priv->references, data->uri);
	if (!g_hash_table_size (disc->priv->references)) {
		g_hash_table_destroy (disc->priv->references);
		disc->priv->references = NULL;
	}
	g_mutex_unlock (disc->priv->references_lock);

	/* for every path we look for the corresponding tree paths in treeview */
	for (iter = paths; iter; iter = iter->next) {
		char *path;

		path = iter->data;

		if (brasero_data_disc_disc_path_to_tree_path (disc,
							      path,
							      &treepath,
							      NULL)) {
			if (!treepath)
				gtk_tree_path_new_from_indices (-1);

			treepaths = g_slist_prepend (treepaths, treepath);
		}
	}

	for (infos = data->infos; infos; infos = g_slist_remove (infos, info)) {
		GSList *treepath_iter;
		GSList *path_iter;

		info = infos->data;
		/* FIXME: we won't be able to display the root since the path */
		uri = g_build_path (G_DIR_SEPARATOR_S,
				    data->uri,
				    info->name,
				    NULL);

		/* see if this file is not in unreadable. it could happen for files we
		 * couldn't read.
		 * but in the latter case if we can read them again inotify will tell
		 * us and we'll add them later so for the moment ignore all unreadable */
		if (disc->priv->unreadable
		&&  g_hash_table_lookup (disc->priv->unreadable, uri)) {
			gnome_vfs_file_info_clear(info);
			gnome_vfs_file_info_unref(info);
			g_free (uri);
			continue;
		}

		/* NOTE: that can't be a symlink as they are handled
		 * separately (or rather their targets) */
		path_iter = paths;
		treepath_iter = treepaths;
		for (; path_iter; path_iter = path_iter->next, treepath_iter = treepath_iter->next) {
			char *parent;
			GtkTreePath *tmp_treepath = NULL;

			/* make sure this file wasn't excluded */
			parent = path_iter->data;
			path = g_build_path (G_DIR_SEPARATOR_S,
					     parent,
					     info->name,
					     NULL);

			if (EXPOSE_EXCLUDE_FILE (disc, uri, path)) {
				g_free (path);
				continue;
			}

			treepath = treepath_iter->data;
			brasero_data_disc_tree_new_path (disc,
							 path,
							 treepath,
							 &tmp_treepath);
			brasero_data_disc_tree_set_path_from_info (disc,
								   path,
								   tmp_treepath,
								   info);

			gtk_tree_path_free (tmp_treepath);
			g_free (path);
		}

		gnome_vfs_file_info_clear(info);
		gnome_vfs_file_info_unref(info);
		g_free (uri);
	}

	/* free disc */
	g_free (data->uri);
	g_free (data);

	/* free tree paths */
	g_slist_foreach (paths, (GFunc) g_free, NULL);
	g_slist_free (paths);
	paths = NULL;

	for (; treepaths; treepaths = g_slist_remove (treepaths, treepaths->data)) {
		GtkTreePath *treepath;
		GtkTreeModel *model;
		GtkTreeIter iter;

		treepath = treepaths->data;
		model = disc->priv->model;
		if (gtk_tree_model_get_iter (model, &iter, treepath)) {
			gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
					    ROW_STATUS_COL, ROW_EXPLORED,
					    -1);

			brasero_data_disc_tree_update_directory_real (disc, &iter);
		}

		gtk_tree_path_free (treepath);
	}
	
	brasero_data_disc_decrease_activity_counter (disc);
	return TRUE;
}

/* this used to be done async */
static gboolean
brasero_data_disc_expose_result (GObject *object, gpointer data)
{
	BraseroDirectoryContentsData *callback_data = data;
	BraseroDataDisc *disc = BRASERO_DATA_DISC (object);

	if (callback_data->errors) {
		brasero_data_disc_load_dir_error (disc, callback_data->errors);
		return TRUE;
	}

	BRASERO_ADD_TO_EXPOSE_QUEUE (disc, callback_data);
	return FALSE;
}

static gboolean
brasero_data_disc_expose_thread (GObject *object, gpointer data)
{
	BraseroDirectoryContentsData *callback_data = data;
	GnomeVFSDirectoryHandle *handle;
	GnomeVFSFileInfo *info;
	GnomeVFSResult result;
	GSList *infos = NULL;
	char *escaped_uri;

	handle = NULL;
	escaped_uri = gnome_vfs_escape_host_and_path_string (callback_data->uri);
	result = gnome_vfs_directory_open (&handle,
					   escaped_uri,
					   GNOME_VFS_FILE_INFO_GET_MIME_TYPE |
					   GNOME_VFS_FILE_INFO_FORCE_SLOW_MIME_TYPE);
	g_free (escaped_uri);

	if (result != GNOME_VFS_OK || handle == NULL) {
		BraseroLoadDirError *error;

		error = g_new0 (BraseroLoadDirError, 1);
		error->uri = callback_data->uri;
		callback_data->uri = NULL;

		error->status = BRASERO_FILTER_UNREADABLE;
		callback_data->errors = g_slist_prepend (callback_data->errors, error);

		if (handle) {
			gnome_vfs_directory_close (handle);
			handle = NULL;
		}

		g_warning ("Cannot open dir %s : %s\n",
			   error->uri,
			   gnome_vfs_result_to_string(result));

		return TRUE;
	}

	infos = NULL;
	info = gnome_vfs_file_info_new ();
	while (1) {
		if (callback_data->cancel)
			break;

		result = gnome_vfs_directory_read_next (handle, info);

		if (result != GNOME_VFS_OK)
			break;

		if (info->name [0] == '.' && (info->name [1] == 0
		|| (info->name [1] == '.' && info->name [2] == 0)))
			continue;

		/* symlinks are exposed through expose_grafted */
		if (info->type == GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK)
			continue;

		infos = g_slist_prepend (infos, info);
		info = gnome_vfs_file_info_new ();
	}

	gnome_vfs_directory_close (handle);
	gnome_vfs_file_info_unref (info);
	callback_data->infos = infos;

	if (callback_data->cancel)
		return FALSE;

	return TRUE;
}

static BraseroDiscResult
brasero_data_disc_expose_insert_path_real (BraseroDataDisc *disc,
					   const char *uri)
{
	BraseroDirectoryContentsData *callback_data;

	if (!disc->priv->jobs)
		brasero_async_job_manager_get_default ();

	if (!disc->priv->expose_type) {
		disc->priv->expose_type = brasero_async_job_manager_register_type (disc->priv->jobs,
										   G_OBJECT (disc),
										   brasero_data_disc_expose_thread,
										   brasero_data_disc_expose_result,
										   brasero_data_disc_dir_contents_destroy,
										   brasero_data_disc_dir_contents_cancel);
	}

	callback_data = g_new0 (BraseroDirectoryContentsData, 1);
	callback_data->uri = g_strdup (uri);

	if (!brasero_async_job_manager_queue (disc->priv->jobs,
					      disc->priv->expose_type,
					      callback_data))
		return BRASERO_DISC_ERROR_THREAD;

	brasero_data_disc_increase_activity_counter (disc);
	return BRASERO_DISC_OK;
}

static BraseroDiscResult
brasero_data_disc_expose_path (BraseroDataDisc *disc,
			       const gchar *path)
{
	BraseroDataDiscReference ref;
	GSList *references;
	BraseroFile *dir;
	GSList *grafted;
	gchar *uri;

	/* find any grafted file that should be exposed under this path */
	grafted = brasero_data_disc_path_find_children_grafts (disc, path);
	brasero_data_disc_expose_grafted (disc, grafted);
	g_slist_free (grafted);

	uri = brasero_data_disc_path_to_uri (disc, path);
	if (!uri) {
		if (!g_hash_table_lookup (disc->priv->paths, path))
			return BRASERO_DISC_NOT_IN_TREE;

		return BRASERO_DISC_OK;
	}

	/* no need to check for dummies here are we only expose children */
	if (!(dir = g_hash_table_lookup(disc->priv->dirs, uri))) {
		g_free (uri);
		return BRASERO_DISC_NOT_IN_TREE;
	}

	/* add a reference */
	ref = brasero_data_disc_reference_new (disc, path);
	if (!disc->priv->references) {
		references = g_slist_prepend (NULL, GINT_TO_POINTER (ref));

		g_mutex_lock (disc->priv->references_lock);
		disc->priv->references = g_hash_table_new (g_str_hash,
							   g_str_equal);
		g_hash_table_insert (disc->priv->references,
				     dir->uri,
				     references);
		g_mutex_unlock (disc->priv->references_lock);
	}
	else {
		references = g_hash_table_lookup (disc->priv->references, uri);
		references = g_slist_prepend (references, GINT_TO_POINTER (ref));
		g_mutex_lock (disc->priv->references_lock);
		g_hash_table_insert (disc->priv->references, dir->uri, references);
		g_mutex_unlock (disc->priv->references_lock);
	}

	/* if this directory is waiting vfs exploration, then when 
	 * the exploration is finished, its disc will be exposed */
	if (g_slist_find (disc->priv->loading, dir))
		brasero_data_disc_directory_priority (disc, dir);
	else
		brasero_data_disc_expose_insert_path_real (disc, uri);

	g_free (uri);
	return BRASERO_DISC_OK;
}

static void
brasero_data_disc_row_collapsed_cb (GtkTreeView *tree,
				    GtkTreeIter *sortparent,
				    GtkTreePath *sortpath,
				    BraseroDataDisc *disc)
{
	char *parent_discpath;
	GtkTreeModel *model;
	GtkTreeModel *sort;
	GtkTreeIter parent;
	GtkTreeIter child;
	GtkTreePath *path;
	gboolean isdir;
	char *discpath;
	int explored;
	char *name;

	model = disc->priv->model;
	sort = disc->priv->sort;

	gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (sort),
							&parent,
							sortparent);

	gtk_tree_store_set (GTK_TREE_STORE (model), &parent,
			    ROW_STATUS_COL, ROW_EXPANDED,
			    -1);

	if (gtk_tree_model_iter_children (model, &child, &parent) == FALSE)
		return;

	path = gtk_tree_model_get_path (model, &parent);
	brasero_data_disc_tree_path_to_disc_path (disc,
						  path,
						  &parent_discpath);
	gtk_tree_path_free (path);

	do {
		gtk_tree_model_get (model, &child,
				    ISDIR_COL, &isdir,
				    ROW_STATUS_COL, &explored,
				    NAME_COL, &name, -1);

		if (explored != ROW_NOT_EXPLORED || !isdir) {
			g_free (name);
			continue;
		}

		discpath = g_build_path (G_DIR_SEPARATOR_S,
					 parent_discpath,
					 name,
					 NULL);
		g_free (name);

		brasero_data_disc_expose_path (disc, discpath);
		gtk_tree_store_set (GTK_TREE_STORE (model), &child,
				    ROW_STATUS_COL, ROW_EXPLORING,
				    -1);

		g_free (discpath);
	} while (gtk_tree_model_iter_next (model, &child));

	g_free(parent_discpath);
}
/************************** files, directories handling ************************/
static gint64
brasero_data_disc_file_info (BraseroDataDisc *disc,
			     const char *uri,
			     GnomeVFSFileInfo *info)
{
	gint64 retval = 0;
	BraseroFile *file;

	/* deal with symlinks */
	if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
		/* same as below for directories */
		if (!(file = g_hash_table_lookup (disc->priv->dirs, uri))
		||    file->sectors < 0)
			file = brasero_data_disc_directory_new (disc,
								g_strdup (uri),
								TRUE);
	}
	else if (!g_hash_table_lookup (disc->priv->files, uri))
		retval = GET_SIZE_IN_SECTORS (info->size);

	return retval;
} 

static void
brasero_data_disc_obj_new (BraseroDataDisc *disc,
			   BraseroFile *file)
{
	gpointer value = NULL;
	gpointer key = NULL;

	if (disc->priv->excluded
	&&  g_hash_table_lookup_extended (disc->priv->excluded,
					  file->uri,
					  &key,
					  &value)) {
		g_hash_table_replace (disc->priv->excluded, file->uri, value);
		g_free (key);
	}

	if (disc->priv->restored
	&&  g_hash_table_lookup_extended (disc->priv->restored,
					  file->uri,
					  &key,
					  &value)) {
		g_mutex_lock (disc->priv->restored_lock);
		g_hash_table_replace (disc->priv->restored, file->uri, value);
		g_mutex_unlock (disc->priv->restored_lock);
		g_free (key);
	}
}

static BraseroFile *
brasero_data_disc_file_new (BraseroDataDisc *disc,
			    const gchar *uri,
			    gint64 sectors)
{
	BraseroFile *parent;
	BraseroFile *file;
	gchar *parent_uri;

	file = g_new0 (BraseroFile, 1);
	file->uri = g_strdup (uri);
	file->sectors = sectors;

#ifdef BUILD_INOTIFY
	file->handle.wd = -1;
#endif

	/* see if it needs monitoring */
	parent_uri = g_path_get_dirname (uri);
	parent = g_hash_table_lookup (disc->priv->dirs, parent_uri);
	g_free (parent_uri);

	if (!parent || parent->sectors < 0) {
#ifdef BUILD_INOTIFY
		brasero_data_disc_start_monitoring (disc, file);
#endif
		brasero_data_disc_size_changed (disc, sectors);
	}
	else if (brasero_data_disc_is_excluded (disc, file->uri, NULL))
		brasero_data_disc_size_changed (disc, sectors);
	else
		parent->sectors -= sectors;

	/* because of above we only insert it at the end */
	g_hash_table_insert (disc->priv->files, file->uri, file);
	brasero_data_disc_obj_new (disc, file);

	return file;
}

struct _BraseroSymlinkChildrenData {
	BraseroDataDisc *disc;
	int len;
	char *uri;
	GSList *list;
};
typedef struct _BraseroSymlinkChildrenData BraseroSymlinkChildrenData;

static void
_foreach_replace_symlink_children_cb (char *symlink,
				      int value,
				      BraseroSymlinkChildrenData *data)
{
	/* symlink must be a child of uri 
	 * NOTE: can't be uri itself since we found it in dirs hash */
	if (strncmp (data->uri, symlink, data->len)
	|| (*(symlink + data->len) != G_DIR_SEPARATOR && *(symlink + data->len) != '\0'))
		return;

	data->list = g_slist_prepend (data->list, symlink);
}

static GSList *
brasero_data_disc_symlink_get_uri_children (BraseroDataDisc *disc,
					    const char *uri)
{
	BraseroSymlinkChildrenData callback_data;

	if (!disc->priv->symlinks)
		return NULL;

	callback_data.disc = disc;
	callback_data.uri = (char *) uri;
	callback_data.len = strlen (uri);
	callback_data.list = NULL;

	g_hash_table_foreach (disc->priv->symlinks,
			      (GHFunc) _foreach_replace_symlink_children_cb,
			      &callback_data);

	return callback_data.list;
}

static gboolean
brasero_data_disc_symlink_is_recursive (BraseroDataDisc *disc,
					const char *uri,
					const char *target)
{
	int len;
	char *symlink;
	gboolean result;
	GSList *symlinks;

	/* 1. we get a list of all the symlinks under the target
	 * 2. for each of their targets we check:
	 * - the target doesn't point to symlink or one of his parents
	 * - for each target we start back at 1
	 * 3. we stop if :
	 * - the target is a file not a directory
	 * - the target doesn't have symlinks children
	 * NOTE: if the target hasn't all of its subdirectories explored
	 * or if it's not explored itself it doesn't matter since when it
	 * will be explored for each of its symlinks we will do the same
	 * and if a symlink is recursive then we'll notice it then */

	symlinks = brasero_data_disc_symlink_get_uri_children (disc, target);
	if (!symlinks)
		return FALSE;

	for (; symlinks; symlinks = g_slist_remove (symlinks, symlink)) {
		symlink = symlinks->data;

		target = g_hash_table_lookup (disc->priv->symlinks, symlink);
		if (!g_hash_table_lookup (disc->priv->dirs, target))
			continue;

		len = strlen (target);
		if (!strncmp (uri, target, len) &&  *(uri + len) == G_DIR_SEPARATOR)
			goto recursive;

		result = brasero_data_disc_symlink_is_recursive (disc,
								 uri,
								 target);
		if (result == TRUE)
			goto recursive;
	}

	return FALSE;

recursive:
	g_slist_free (symlinks);
	return TRUE;
}

static GSList *
brasero_data_disc_symlink_new (BraseroDataDisc *disc,
			       const char *uri,
			       GnomeVFSFileInfo *info,
			       GSList *paths)
{
	BraseroFile *file = NULL;
	GSList *next;
	GSList *iter;
	char *path;

	/* we don't want paths overlapping already grafted paths.
	 * This might happen when we are loading a project or when
	 * we are notified of the creation of a new file */
	for (iter = paths; iter; iter = next) {
		next = iter->next;
		path = iter->data;

		if (g_hash_table_lookup (disc->priv->paths, path)) {
			paths = g_slist_remove (paths, path);
			g_free (path);
		}
	}

	if (!paths)
		goto end;

	/* make sure the target was explored or is about to be explored  */
	if ((file = g_hash_table_lookup (disc->priv->dirs, info->symlink_name))
	&&   file->sectors >= 0) {
		if (!g_slist_find (disc->priv->loading, file)) {
			brasero_data_disc_restore_excluded_children (disc, file);
			brasero_data_disc_replace_symlink_children (disc, file, paths);
		}
	}
	else if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
		file = brasero_data_disc_directory_new (disc,
							g_strdup (info->symlink_name),
							FALSE);
	}
	else if (!(file = g_hash_table_lookup (disc->priv->files, info->symlink_name))) 
		file = brasero_data_disc_file_new (disc,
						   info->symlink_name,
						   GET_SIZE_IN_SECTORS (info->size));


end :
	if (!disc->priv->symlinks)
		disc->priv->symlinks = g_hash_table_new_full (g_str_hash,
							      g_str_equal,
							      (GDestroyNotify) g_free,
							      (GDestroyNotify) g_free);
	
	if (!g_hash_table_lookup (disc->priv->symlinks, uri))
		g_hash_table_insert (disc->priv->symlinks,
				     g_strdup (uri),
				     g_strdup (info->symlink_name));

	/* add graft points to the target */
	for (iter = paths; iter; iter = iter->next) {
		path = iter->data;
		brasero_data_disc_graft_new (disc,
					     file->uri,
					     path);
	}

	return paths;
}

/* NOTE: it has a parent so if it is strictly excluded, it MUST have excluding */
#define EXPLORE_EXCLUDED_FILE(disc, uri)	\
	(!disc->priv->excluded	\
	 || !g_hash_table_lookup (disc->priv->excluded, uri) \
	 || !brasero_data_disc_is_excluded (disc, uri, NULL))

static void
brasero_data_disc_symlink_list_new (BraseroDataDisc *disc,
				    BraseroDirectoryContentsData *content,
				    const char *parent,
				    GSList *symlinks)
{
	GSList *iter;
	GSList *paths;
	char *current;
	GSList *grafts = NULL;
	GnomeVFSFileInfo *info;

	for (iter = symlinks; iter; iter = iter->next) {
		info = iter->data;
		content->infos = g_slist_remove (content->infos, info);

		if (disc->priv->unreadable
		&&  g_hash_table_lookup (disc->priv->unreadable, info->symlink_name))
			continue;

		current = g_build_path (G_DIR_SEPARATOR_S,
					parent,
					info->name,
					NULL);

		if (disc->priv->symlinks
		&&  g_hash_table_lookup (disc->priv->symlinks, current)) {
			g_free (current);
			continue;
		}

		if (info->type == GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK
		|| !brasero_data_disc_is_readable (info)) {
			brasero_data_disc_unreadable_new (disc,
							  current,
							  BRASERO_FILTER_BROKEN_SYM);
			continue;
		}

		if (brasero_data_disc_symlink_is_recursive (disc,
							    current,
							    info->symlink_name)) {
			brasero_data_disc_unreadable_new (disc,
							  current,
							  BRASERO_FILTER_RECURSIVE_SYM);
			continue;
		}

		paths = brasero_data_disc_uri_to_paths (disc, current, TRUE);
		paths = brasero_data_disc_symlink_new (disc,
						       current,
						       info,
						       paths);
		g_free (current);

		grafts = g_slist_concat (grafts, paths);
	}

	if (grafts) {
		if (disc->priv->references
		&&  g_hash_table_lookup (disc->priv->references, parent))
			brasero_data_disc_expose_grafted (disc, grafts);

		g_slist_foreach (grafts, (GFunc) g_free, NULL);
		g_slist_free (grafts);
	}

	g_slist_foreach (symlinks, (GFunc) gnome_vfs_file_info_clear, NULL);
	g_slist_foreach (symlinks, (GFunc) gnome_vfs_file_info_unref, NULL);
	g_slist_free (symlinks);
}

static gboolean
brasero_data_disc_load_result (GObject *object, gpointer data)
{
	BraseroDirectoryContentsData *callback_data = data;
	BraseroDataDisc *disc = BRASERO_DATA_DISC (object);
	GSList *symlinks = NULL;
	GnomeVFSFileInfo *info;
	gint64 dir_sectors = 0;
	gint64 diffsectors;
	BraseroFile *dir;
	gchar *current;
	GSList *next;
	GSList *iter;

	brasero_data_disc_decrease_activity_counter (disc);

	/* process errors */
	if (callback_data->errors) {
		brasero_data_disc_load_dir_error (disc, callback_data->errors);

		/* we don't want the errors to be processed twice */
		g_slist_foreach (callback_data->errors,
				 (GFunc) _free_dir_contents_error,
				 NULL);
		g_slist_free (callback_data->errors);
		callback_data->errors = NULL;

		/* the following means that we couldn't open the directory */
		if (!callback_data->uri)
			return TRUE;
	}

	dir = g_hash_table_lookup (disc->priv->dirs, callback_data->uri);
	if (!dir || dir->sectors < 0)
		return TRUE;

	disc->priv->loading = g_slist_remove (disc->priv->loading, dir);
	disc->priv->rescan = g_slist_remove (disc->priv->rescan, dir);
	for (iter = callback_data->infos; iter; iter = next) {
		info = iter->data;
		next = iter->next;
		
		if (GNOME_VFS_FILE_INFO_SYMLINK (info)) {
			symlinks = g_slist_prepend (symlinks, info);
			continue;
		}

		current = g_build_path (G_DIR_SEPARATOR_S,
					dir->uri,
					info->name,
					NULL);

		if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
			BraseroFile *tmp;

			tmp = g_hash_table_lookup (disc->priv->dirs, current);
			if ((tmp == NULL || tmp->sectors < 0)
			&&  EXPLORE_EXCLUDED_FILE (disc, current)) {
				brasero_data_disc_directory_new (disc,
								 g_strdup (current),
								 TRUE);
			}
		}
		else if (!g_hash_table_lookup (disc->priv->files, current)
		      &&  EXPLORE_EXCLUDED_FILE (disc, current)) {
			dir_sectors += GET_SIZE_IN_SECTORS (info->size);
		}

		if (strlen (info->name) > 64) {
			GSList *paths;

			/* this file could conflict with joliet compatibility so
			 * we add all its paths to joliet_incompat hash */
			paths = brasero_data_disc_uri_to_paths (disc,
								current,
								FALSE);
			brasero_data_disc_joliet_incompat_add_paths (disc, paths);
			g_slist_foreach (paths, (GFunc) g_free, NULL);
			g_slist_free (paths);
		}

		g_free (current);
	}

	diffsectors = dir_sectors - dir->sectors;
	if (diffsectors) {
		dir->sectors += diffsectors;
		brasero_data_disc_size_changed (disc, diffsectors);
	}

	/* we need to check that they are not symlinks */
	brasero_data_disc_symlink_list_new (disc,
					    callback_data,
					    dir->uri,
					    symlinks);

	if (disc->priv->references
	&&  g_hash_table_lookup (disc->priv->references, dir->uri)) {
		if (!callback_data->infos) {
			/* empty directory to be exposed */
			brasero_data_disc_increase_activity_counter (disc);
			BRASERO_ADD_TO_EXPOSE_QUEUE (disc, callback_data);
			return FALSE;
		}

		/* make sure that when we explored we asked for mime types :
		 * a directory could be explored and then its parent expanded
		 * which adds references but unfortunately we won't have any
		 * mime types */
		info = callback_data->infos->data;
		if (info->mime_type) {
			/* there are references put that in the queue for it to
			 * be exposed */
			brasero_data_disc_increase_activity_counter (disc);
			BRASERO_ADD_TO_EXPOSE_QUEUE (disc, callback_data);
			return FALSE;
		}

		brasero_data_disc_expose_insert_path_real (disc, callback_data->uri);
	}

	return TRUE;
}

static void
brasero_data_disc_load_dir_error (BraseroDataDisc *disc, GSList *errors)
{
	BraseroLoadDirError *error;
	BraseroFile *file;
	char *parent;

	for (; errors; errors = errors->next) {
		error = errors->data;

		if (disc->priv->unreadable
		&&  g_hash_table_lookup (disc->priv->unreadable, error->uri))
			continue;

		/* remove it wherever it is */
		if ((file = g_hash_table_lookup (disc->priv->dirs, error->uri)))
			brasero_data_disc_directory_remove_from_tree (disc, file);
		else if ((file = g_hash_table_lookup (disc->priv->files, error->uri)))
			brasero_data_disc_file_remove_from_tree (disc, file);
		else if (disc->priv->symlinks
		      &&  g_hash_table_lookup (disc->priv->symlinks, error->uri)) {
			g_hash_table_remove (disc->priv->symlinks, error->uri);
		
			if (!g_hash_table_size (disc->priv->symlinks)) {
				g_hash_table_destroy (disc->priv->symlinks);
				disc->priv->symlinks = NULL;
			}
		}
		else if (!brasero_data_disc_is_excluded (disc, error->uri, NULL))
			brasero_data_disc_remove_uri_from_tree (disc, error->uri, TRUE);

		/* insert it in the unreadable hash table if need be
		 * (if it has a parent directory) */
		parent = g_path_get_dirname (error->uri);
		file = g_hash_table_lookup (disc->priv->dirs, parent);
		g_free (parent);

		if (file
		&&  file >= 0
		&&  error->status != BRASERO_FILTER_UNKNOWN) {
			brasero_data_disc_unreadable_new (disc,
							  error->uri,
							  error->status);
			error->uri = NULL;
		}
	}
}

struct _BraseroCheckRestoredData {
	BraseroFilterStatus status;
	BraseroDataDisc *disc;
	char *uri;
};
typedef struct _BraseroCheckRestoredData BraseroCheckRestoredData;

static gboolean
_check_for_restored_modify_state (BraseroCheckRestoredData *data)
{
	g_hash_table_insert (data->disc->priv->restored,
			     data->uri,
			     GINT_TO_POINTER (data->status));

	if (!data->disc->priv->filter_dialog)
		goto end;

	brasero_filtered_dialog_add (BRASERO_FILTERED_DIALOG (data->disc->priv->filter_dialog),
				     data->uri,
				     TRUE,
				     data->status);

end:
	g_free (data->uri);
	g_free (data);
	return FALSE;
}

static gboolean
_check_for_restored (BraseroDataDisc *disc,
		     const gchar *uri,
		     BraseroFilterStatus status) {
	BraseroFilterStatus current_status;
	gboolean retval;

	retval = FALSE;
	g_mutex_lock (disc->priv->restored_lock);
	if (!disc->priv->restored)
		goto end;

	current_status = GPOINTER_TO_INT (g_hash_table_lookup (disc->priv->restored, uri));
	if (!current_status)
		goto end;

	if (status == BRASERO_FILTER_UNREADABLE
	||  status == BRASERO_FILTER_RECURSIVE_SYM) {
		g_mutex_unlock (disc->priv->restored_lock);
		brasero_data_disc_restored_free (disc, uri);		
		goto end;
	}

	if (current_status == BRASERO_FILTER_UNKNOWN) {
		BraseroCheckRestoredData *data;

		data = g_new0 (BraseroCheckRestoredData, 1);
		data->disc = disc;
		data->uri = g_strdup (uri);
		data->status = status;

		g_idle_add ((GSourceFunc) _check_for_restored_modify_state,
			    data);
	}
	retval = TRUE;


end:
	g_mutex_unlock (disc->priv->restored_lock);
	return retval;
}

static gboolean
brasero_data_disc_load_thread (GObject *object, gpointer data)
{
	BraseroDirectoryContentsData *callback_data = data;
	BraseroDataDisc *disc = BRASERO_DATA_DISC (object);
	BraseroLoadDirError *error = NULL;
	GnomeVFSDirectoryHandle *handle = NULL;
	GnomeVFSFileInfoOptions flags;
	GnomeVFSFileInfo *info;
	GnomeVFSResult result;
	char *escaped_uri;
	GSList *infos;
	char *current;

	flags = GNOME_VFS_FILE_INFO_GET_ACCESS_RIGHTS;

	g_mutex_lock (disc->priv->references_lock);
	if (disc->priv->references
	&&  g_hash_table_lookup (disc->priv->references, callback_data->uri))
		flags |= GNOME_VFS_FILE_INFO_GET_MIME_TYPE|
			 GNOME_VFS_FILE_INFO_FORCE_SLOW_MIME_TYPE;
	g_mutex_unlock (disc->priv->references_lock);

	handle = NULL;
	escaped_uri = gnome_vfs_escape_host_and_path_string (callback_data->uri);
	result = gnome_vfs_directory_open (&handle,
					   escaped_uri,
					   flags);
	g_free (escaped_uri);

	if (result != GNOME_VFS_OK || handle == NULL) {
		error = g_new0 (BraseroLoadDirError, 1);
		error->uri = callback_data->uri;
		callback_data->uri = NULL;

		if (result != GNOME_VFS_ERROR_NOT_FOUND) {
			_check_for_restored (disc,
					     error->uri,
					     BRASERO_FILTER_UNREADABLE);

			error->status = BRASERO_FILTER_UNREADABLE;
		}
		else
			error->status = BRASERO_FILTER_UNKNOWN;

		callback_data->errors = g_slist_prepend (callback_data->errors, error);

		if (handle) {
			gnome_vfs_directory_close (handle);
			handle = NULL;
		}

		g_warning ("Can't open directory %s : %s\n",
			   error->uri,
			   gnome_vfs_result_to_string (result));
		return TRUE;
	}

	infos = NULL;
	info = gnome_vfs_file_info_new();
	while (gnome_vfs_directory_read_next (handle, info) == GNOME_VFS_OK) {
		if (callback_data->cancel)
			break;

		if (*info->name == '.' && (info->name[1] == 0
		||  (info->name[1] == '.' && info->name[2] == 0)))
			continue;

		current = g_build_path (G_DIR_SEPARATOR_S,
					callback_data->uri,
					info->name,
					NULL);

		if (!brasero_data_disc_is_readable (info)) {
			_check_for_restored (disc,
					     current,
					     BRASERO_FILTER_UNREADABLE);

			error = g_new0 (BraseroLoadDirError, 1);
 			error->uri = current;
 			error->status = BRASERO_FILTER_UNREADABLE;
			callback_data->errors = g_slist_prepend (callback_data->errors, error);
			continue;
		}

		if (info->type == GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK) {
			gnome_vfs_file_info_clear (info);

			if(!brasero_utils_get_symlink_target (current, info, flags)) {
				BraseroFilterStatus status;

				if (info->symlink_name)
					status = BRASERO_FILTER_RECURSIVE_SYM;
				else
					status = BRASERO_FILTER_BROKEN_SYM;

				if (!_check_for_restored (disc, current, status)) {
					error = g_new0 (BraseroLoadDirError, 1);
					error->uri = current;
					error->status = status;
					callback_data->errors = g_slist_prepend (callback_data->errors, error);
					continue;
				}
			}
		}

		/* a new hidden file ? */
		if (*info->name == '.'
		&& !_check_for_restored (disc, current, BRASERO_FILTER_HIDDEN)) {
 			error = g_new0 (BraseroLoadDirError, 1);
 			error->uri = current;
 			error->status = BRASERO_FILTER_HIDDEN;
			callback_data->errors = g_slist_prepend (callback_data->errors, error);
 			continue;
		}

		g_free (current);
		infos = g_slist_prepend (infos, info);
		info = gnome_vfs_file_info_new ();
	}

	gnome_vfs_file_info_unref (info);
	gnome_vfs_directory_close (handle);

	callback_data->infos = infos;

	if (callback_data->cancel)
		return FALSE;

	return TRUE;
}

static BraseroDiscResult
brasero_data_disc_directory_load (BraseroDataDisc *disc,
				  BraseroFile *dir,
				  gboolean append)
{
	BraseroDirectoryContentsData *callback_data;

	/* start exploration */
	disc->priv->loading = g_slist_prepend (disc->priv->loading, dir);

	if (!disc->priv->jobs)
		disc->priv->jobs = brasero_async_job_manager_get_default ();

	if (!disc->priv->load_type) {
		disc->priv->load_type = brasero_async_job_manager_register_type (disc->priv->jobs,
										 G_OBJECT (disc),
										 brasero_data_disc_load_thread,
										 brasero_data_disc_load_result,
										 brasero_data_disc_dir_contents_destroy,
										 brasero_data_disc_dir_contents_cancel);
	}

	callback_data = g_new0 (BraseroDirectoryContentsData, 1);
	callback_data->uri = g_strdup (dir->uri);

	if (!brasero_async_job_manager_queue (disc->priv->jobs,
					      disc->priv->load_type,
					      callback_data))
		return BRASERO_DISC_ERROR_THREAD;

	brasero_data_disc_increase_activity_counter (disc);
	return BRASERO_DISC_OK;
}

static BraseroFile *
brasero_data_disc_directory_new (BraseroDataDisc *disc,
				 gchar *uri,
				 gboolean append)
{
	BraseroFile *dir;

	/* we make sure a dummy directory doesn't already exist */
	dir = g_hash_table_lookup (disc->priv->dirs, uri);
	if (dir == NULL) {
		dir = g_new0 (BraseroFile, 1);
		dir->uri = uri;
		g_hash_table_insert (disc->priv->dirs, dir->uri, dir);

#ifdef BUILD_INOTIFY

		dir->handle.wd = -1;
		brasero_data_disc_start_monitoring (disc, dir);

#endif

	}
	else if (dir->sectors < 0) {
		/* a dummy directory already exists: use it */
		g_free (uri);
	}

	dir->sectors = 1;
	brasero_data_disc_size_changed (disc, 1);
	brasero_data_disc_obj_new (disc, dir);
	brasero_data_disc_directory_load (disc, dir, append);

	return dir;
}

static gboolean
brasero_data_disc_directory_priority_cb (gpointer data, gpointer user_data)
{
	BraseroDirectoryContentsData *callback_data = data;
	BraseroFile *dir = user_data;

	if (!strcmp (dir->uri, callback_data->uri))
		return TRUE;

	return FALSE;
}

static void
brasero_data_disc_directory_priority (BraseroDataDisc *disc,
				      BraseroFile *dir)
{
	brasero_async_job_manager_find_urgent_job (disc->priv->jobs,
						   disc->priv->load_type,
						   brasero_data_disc_directory_priority_cb,
						   dir);
}

/******************************* Row removal ***********************************/
static void
brasero_data_disc_remove_row_in_dirs_hash (BraseroDataDisc *disc,
					   BraseroFile *dir,
					   const char *path)
{
	GSList *grafts;

	/* remove all the children graft point of this path */
	grafts = g_slist_append (NULL, (gpointer) path);
	brasero_data_disc_graft_children_remove (disc, grafts);
	g_slist_free (grafts);

	/* remove graft point if path == graft point */
	if (!brasero_data_disc_graft_remove (disc, path)) {
		char *graft;

		/* otherwise we exclude dir */
		graft = brasero_data_disc_graft_get (disc, path);
		brasero_data_disc_exclude_uri (disc, graft, dir->uri);
		g_free (graft);

		if (brasero_data_disc_is_excluded (disc, dir->uri, NULL))
			brasero_data_disc_remove_dir_and_children (disc, dir);
		return;
	}
}

static void
brasero_data_disc_remove_row_in_files_hash (BraseroDataDisc *disc,
					    BraseroFile *file,
					    const char *path)
{
	/* see if path == graft point. If so, remove it */
	if (!brasero_data_disc_graft_remove (disc, path)) {
		char *graft;

		/* the path was not of the graft points of the file so 
		 * it has a parent graft point, find it and exclude it */
		graft = brasero_data_disc_graft_get (disc, path);
		brasero_data_disc_exclude_uri (disc, graft, file->uri);
		g_free (graft);
	}
}

static gboolean
brasero_data_disc_delete_row_cb (BraseroDataDisc *disc,
				 GSList *results,
				 gpointer null_data)
{
	BraseroInfoAsyncResult *result;
	GnomeVFSFileInfo *info;
	BraseroFile *parent;
	gchar *parenturi;
	gchar *uri;

	for (; results; results = results->next) {
		gint64 sectors;

		result = results->data;

		uri = result->uri;
		info = result->info;

		parenturi = g_path_get_dirname (uri);
		parent = g_hash_table_lookup (disc->priv->dirs, parenturi);
		g_free (parenturi);

		if (!parent || parent->sectors < 0)
			continue;

		if (result->result == GNOME_VFS_ERROR_NOT_FOUND) {
			brasero_data_disc_add_rescan (disc, parent);
			continue;
		}

		if (result->result == GNOME_VFS_ERROR_LOOP) {
			brasero_data_disc_unreadable_new (disc,
							  g_strdup (uri),
							  BRASERO_FILTER_RECURSIVE_SYM);
			brasero_data_disc_add_rescan (disc, parent);
			continue;
		}

		if (result->result != GNOME_VFS_OK
		||  !brasero_data_disc_is_readable (info)) {
			brasero_data_disc_unreadable_new (disc,
							  g_strdup (uri),
							  BRASERO_FILTER_UNREADABLE);
			brasero_data_disc_add_rescan (disc, parent);
			continue;
		}

		sectors = GET_SIZE_IN_SECTORS (info->size);
		brasero_data_disc_size_changed (disc, sectors * (-1));
		parent->sectors -= sectors;
	}

	return TRUE;
}

static void
brasero_data_disc_path_remove_user (BraseroDataDisc *disc,
				    const gchar *path)
{
	BraseroFile *file;
	gchar *uri;

	brasero_data_disc_joliet_incompat_free (disc, path);
	brasero_data_disc_reference_remove_path (disc, path);

	/* uri can be NULL if uri is an empty directory
	 * added or if the row hasn't been loaded yet */
	uri = brasero_data_disc_path_to_uri (disc, path);
	if (!uri) {
		gpointer value = NULL;

		g_hash_table_lookup_extended (disc->priv->paths,
					      path,
					      &value,
					      NULL);

		if (value) {
			GSList *paths;

			paths = g_slist_append (NULL, (char *) value);
			brasero_data_disc_graft_children_remove (disc, paths);
			g_slist_free (paths);

			g_hash_table_remove (disc->priv->paths, value);
			g_free (value);
		}

		return;
	}

	/* see if this file is not already in the hash tables */
	/* no need to check for a dummy directory */
	if ((file = g_hash_table_lookup (disc->priv->dirs, uri))) {
		brasero_data_disc_remove_row_in_dirs_hash (disc,
							   file,
							   path);
	} 
	else if ((file = g_hash_table_lookup (disc->priv->files, uri))) {
		brasero_data_disc_remove_row_in_files_hash (disc,
							    file,
							    path);
	}
	else {
		char *graft;

		/* exclude it from parent */
		graft = brasero_data_disc_graft_get (disc, path);
		brasero_data_disc_exclude_uri (disc, graft, uri);
		g_free (graft);

		/* if it is excluded in all parent graft points exclude it
		 * and update the selection and the parent size */
		if (brasero_data_disc_is_excluded (disc, uri, NULL)) {
			GSList *uris;

			uris = g_slist_prepend (NULL, (char*) uri);
			brasero_data_disc_get_info_async (disc,
							  uris, 
							  0,
							  brasero_data_disc_delete_row_cb,
							  NULL,
							  NULL);
			g_slist_free (uris);
		}
	}
	g_free (uri);
}

static void
brasero_data_disc_delete_selected (BraseroDisc *disc)
{
	GtkTreeSelection *selection;
	GtkTreePath *realpath;
	GtkTreePath *treepath;
	BraseroDataDisc *data;
	GtkTreeModel *model;
	GtkTreeModel *sort;
	GList *list, *iter;
	GtkTreeIter row;
	char *discpath;

	data = BRASERO_DATA_DISC (disc);
	if (data->priv->is_loading)
		return;

	model = data->priv->model;
	sort = data->priv->sort;
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (data->priv->tree));

	/* we must start by the end for the treepaths to point to valid rows */
	list = gtk_tree_selection_get_selected_rows (selection, &sort);
	list = g_list_reverse (list);

	for (iter = list; iter; iter = iter->next) {
		treepath = iter->data;

		realpath = gtk_tree_model_sort_convert_path_to_child_path (GTK_TREE_MODEL_SORT (sort),
									   treepath);
		gtk_tree_path_free (treepath);

		brasero_data_disc_tree_path_to_disc_path (data,
							  realpath,
							  &discpath);

		if (gtk_tree_model_get_iter (model, &row, realpath)) {
			GtkTreeIter parent;
			int status;

			gtk_tree_model_get (model, &row,
 					    ROW_STATUS_COL, &status,
 					    -1);
  
 			if (status != ROW_BOGUS) {
 				gboolean is_valid;
 
 				is_valid = gtk_tree_model_iter_parent (model, &parent, &row);
  				gtk_tree_store_remove (GTK_TREE_STORE (model), &row);
 	
 				if (is_valid)
 					brasero_data_disc_tree_update_directory_real (data, &parent);
 
  				if (status != ROW_NEW)
  					brasero_data_disc_path_remove_user (data, discpath);
  			}
  		}

 		gtk_tree_path_free (realpath);
		g_free (discpath);
	}

	g_list_free (list);

	if (g_hash_table_size (data->priv->paths) == 0)
		brasero_data_disc_selection_changed (data, FALSE);
	else
		brasero_data_disc_selection_changed (data, TRUE);
}

static void
brasero_data_disc_clear (BraseroDisc *disc)
{
	BraseroDataDisc *data;

	data = BRASERO_DATA_DISC (disc);
	if (data->priv->is_loading)
		return;

	gtk_tree_store_clear (GTK_TREE_STORE (data->priv->model));
	brasero_data_disc_reset_real (data);

	brasero_disc_size_changed (disc, 0);
	brasero_data_disc_selection_changed (data, FALSE);
}

static void
brasero_data_disc_reset (BraseroDisc *disc)
{
	BraseroDataDisc *data;

	data = BRASERO_DATA_DISC (disc);

	gtk_tree_store_clear (GTK_TREE_STORE (data->priv->model));
	brasero_data_disc_reset_real (data);

	brasero_disc_size_changed (disc, 0);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (BRASERO_DATA_DISC (disc)->priv->notebook), 0);
	brasero_data_disc_selection_changed (data, FALSE);
}

/*************************************** new row *******************************/
struct _BraseroRestoreChildrenData {
	int len;
	char *uri;
	GSList *list;
	BraseroDataDisc *disc;
};
typedef struct _BraseroRestoreChildrenData BraseroRestoreChildrenData;

static void
brasero_data_disc_restore_excluded_children_destroy_cb (BraseroDataDisc *disc,
							gpointer callback_data)
{
	g_free (callback_data);
}

static gboolean
brasero_data_disc_restore_excluded_children_cb (BraseroDataDisc *disc,
						GSList *results,
						gpointer callback_data)
{
	gchar *uri;
	BraseroFile *dir;
	GnomeVFSFileInfo *info;
	gchar *dir_uri = callback_data;
	BraseroInfoAsyncResult *result;

	dir = g_hash_table_lookup (disc->priv->dirs, dir_uri);
	if (!dir || dir->sectors < 0)
		return TRUE;

	for (; results ; results = results->next) {
		gint64 sectors;

		result = results->data;
		uri = result->uri;

		/* see if it has not been excluded in between */
		if (brasero_data_disc_is_excluded (disc, uri, NULL))
			continue;

		/* as an excluded file it is not in the hashes
		 * and its size is not taken into account */
		if (result->result == GNOME_VFS_ERROR_NOT_FOUND) {
			brasero_data_disc_remove_uri_from_tree (disc, uri, TRUE);
			continue;
		}

		info = result->info;
		if (result->result == GNOME_VFS_ERROR_LOOP) {
			brasero_data_disc_remove_uri_from_tree (disc, uri, TRUE);
			brasero_data_disc_unreadable_new (disc,
							  g_strdup (uri),
							  BRASERO_FILTER_RECURSIVE_SYM);
			continue;
		}

		if (result->result != GNOME_VFS_OK
		||  !brasero_data_disc_is_readable (info)) {
			brasero_data_disc_remove_uri_from_tree (disc, uri, TRUE);
			brasero_data_disc_unreadable_new (disc,
							  g_strdup (uri),
							  BRASERO_FILTER_UNREADABLE);
			continue;
		}

		sectors = brasero_data_disc_file_info (disc, uri, info);
		if (sectors) {
			char *parent;
			BraseroFile *dir;
	
			parent = g_path_get_dirname (uri);
			dir = g_hash_table_lookup (disc->priv->dirs, parent);
			g_free (parent);

			dir->sectors += sectors;
			brasero_data_disc_size_changed (disc, sectors);
		}
	}

	/* free callback_data */
	return TRUE;
}

static void
_foreach_restore_strictly_excluded_children_cb (gchar *key,
						GSList *grafts,
						BraseroRestoreChildrenData *data)
{
	gchar *parent_uri;
	BraseroFile *parent;

	/* keep only the children of data->uri */
	if (strncmp (data->uri, key, data->len)
	|| *(key + data->len) != G_DIR_SEPARATOR)
		return;

	/* keep only those that are stricly excluded by all parent graft points */
	if (!brasero_data_disc_is_excluded (data->disc, key, NULL))
		return;

	/* keep only those who have a parent. As for the others when the parent
	 * will be restored later (it has to be a directory), at this point 
	 * they won't be excluded any more and will be "naturally restored" */
	parent_uri = g_path_get_dirname (key);
	parent = g_hash_table_lookup (data->disc->priv->dirs, parent_uri);
	if (parent && parent->sectors >= 0)
		data->list = g_slist_prepend (data->list, key);
	g_free (parent_uri);
}

static void
brasero_data_disc_restore_excluded_children (BraseroDataDisc *disc,
					     BraseroFile *dir)
{
	BraseroRestoreChildrenData callback_data;
	BraseroDiscResult result;
	char *dir_uri;

	if (!disc->priv->excluded)
		return;

	callback_data.disc = disc;
	callback_data.uri = dir->uri;
	callback_data.len = strlen (dir->uri);
	callback_data.list = NULL;

	/* find all children excluded by all parent graft points and restore them */
	g_hash_table_foreach (disc->priv->excluded,
			      (GHFunc) _foreach_restore_strictly_excluded_children_cb,
			      &callback_data);

	if (!callback_data.list)
		return;

	dir_uri = g_strdup (dir->uri);
	result = brasero_data_disc_get_info_async (disc,
						   callback_data.list,
						   GNOME_VFS_FILE_INFO_GET_ACCESS_RIGHTS,
						   brasero_data_disc_restore_excluded_children_cb,
						   dir_uri,
						   brasero_data_disc_restore_excluded_children_destroy_cb);
	g_slist_free (callback_data.list);
}

struct _ReplaceSymlinkChildrenData {
	GSList *grafts;
	char *parent;
};
typedef struct _ReplaceSymlinkChildrenData ReplaceSymlinkChildrenData;

static GSList *
brasero_data_disc_get_target_grafts (BraseroDataDisc *disc,
				     const char *sym_parent,
				     GSList *grafts,
				     const char *symlink)
{
	int len;
	char *path;
	char *graft;
	GSList *newgrafts = NULL;

	len = strlen (sym_parent);

	/* we add the graft point */
	for (; grafts; grafts = grafts->next) {
		graft = grafts->data;
		path = g_strconcat (graft, symlink + len, NULL);
		newgrafts = g_slist_append (newgrafts, path);
	}

	return newgrafts;
}

static void
brasero_data_disc_replace_symlink_children_destroy_cb (BraseroDataDisc *disc,
						       gpointer callback_data)
{
	ReplaceSymlinkChildrenData *async_data = callback_data;
	GSList *iter;

	for (iter = async_data->grafts; iter; iter = iter->next) {
		BraseroDataDiscReference ref;

		ref = GPOINTER_TO_INT (async_data->grafts->data);
		brasero_data_disc_reference_free (disc, ref);
	}
	g_slist_free (async_data->grafts);
	g_free (async_data->parent);
	g_free (async_data);
}

static gboolean
brasero_data_disc_replace_symlink_children_cb (BraseroDataDisc *disc,
					       GSList *results,
					       gpointer data)
{
	ReplaceSymlinkChildrenData *callback_data = data;
	BraseroInfoAsyncResult *result;
	GnomeVFSFileInfo *info;
	BraseroFile *parent;
	BraseroFile *file;
	GSList *grafts;
	gchar *symlink;
	GSList *paths;
	gchar *target;

	grafts = brasero_data_disc_reference_get_list (disc,
						       callback_data->grafts,
						       FALSE);

	parent = g_hash_table_lookup (disc->priv->dirs, callback_data->parent);
	if (!grafts || !parent || parent->sectors < 0)
		goto cleanup;

	for (; results; results = results->next) {
		result = results->data;

		info = result->info;
		symlink = result->uri;
		target = info->symlink_name;

		if (result->result != GNOME_VFS_OK
		||  !brasero_data_disc_is_readable (info))
			continue;

		/* if target is in unreadable remove it */
		if (disc->priv->unreadable
		&&  g_hash_table_lookup (disc->priv->unreadable, target))
			brasero_data_disc_unreadable_free (disc, target);

		paths = brasero_data_disc_get_target_grafts (disc,
							     callback_data->parent,
							     grafts,
							     symlink);

		if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
			file = g_hash_table_lookup (disc->priv->dirs, target);
			if (!file || file->sectors < 0)
				file = brasero_data_disc_directory_new (disc,
								        g_strdup (target),
									FALSE);

			paths = brasero_data_disc_graft_new_list (disc,
								  target,
								  paths);

			brasero_data_disc_restore_excluded_children (disc, file);
			brasero_data_disc_replace_symlink_children (disc,
								    file,
								    paths);
		}
		else {
			if (!g_hash_table_lookup (disc->priv->files, target)) {
				gint64 sectors;

				sectors = GET_SIZE_IN_SECTORS (info->size);
				file = brasero_data_disc_file_new (disc,
								   target,
								   sectors);
			}

			paths = brasero_data_disc_graft_new_list (disc,
								  target,
								  paths);
		}

		g_slist_foreach (paths, (GFunc) g_free, NULL);
		g_slist_free (paths);
	}

cleanup:

	g_slist_foreach (grafts, (GFunc) g_free, NULL);
	g_slist_free (grafts);

	/* free callback_data / data */
	return TRUE;
}

static void
brasero_data_disc_replace_symlink_children (BraseroDataDisc *disc,
					    BraseroFile *dir,
					    GSList *grafts)
{
	ReplaceSymlinkChildrenData *async_data;
	BraseroDataDiscReference ref;
	BraseroDiscResult result;
	GSList *list;

	list = brasero_data_disc_symlink_get_uri_children (disc, dir->uri);

	if (!list)
		return;

	async_data = g_new0 (ReplaceSymlinkChildrenData, 1);
	async_data->parent = g_strdup (dir->uri);

	for (; grafts; grafts = grafts->next) {
		char *graft;

		graft = grafts->data;
		ref = brasero_data_disc_reference_new (disc, graft);
		async_data->grafts = g_slist_prepend (async_data->grafts,
						      GINT_TO_POINTER (ref));
	}

	result = brasero_data_disc_get_info_async (disc,
						   list,
						   GNOME_VFS_FILE_INFO_GET_ACCESS_RIGHTS,
						   brasero_data_disc_replace_symlink_children_cb,
						   async_data,
						   brasero_data_disc_replace_symlink_children_destroy_cb);
	g_slist_free (list);
}

static char *
brasero_data_disc_new_file (BraseroDataDisc *disc,
			    const gchar *uri,
			    const gchar *path,
			    const GnomeVFSFileInfo *info)
{
	char *graft;

	if (brasero_data_disc_original_parent (disc, uri, path)) {
		if (brasero_data_disc_is_excluded (disc, uri, NULL)) {
			BraseroFile *parent;
			gchar *parent_uri;
			gint64 sectors;
	
			parent_uri = g_path_get_dirname (uri);
			parent = g_hash_table_lookup (disc->priv->dirs, parent_uri);
			g_free (parent_uri);

			/* no need to check if parent is dummy. It
			 * is done in brasero_data_disc_is_excluded */
			sectors = GET_SIZE_IN_SECTORS (info->size);
			parent->sectors += sectors;
			brasero_data_disc_size_changed (disc, sectors);
		}

		graft = brasero_data_disc_graft_get (disc, path);
		brasero_data_disc_restore_uri (disc,
					       graft,
					       uri);
	}
	else {
		gint64 sectors;

		sectors = GET_SIZE_IN_SECTORS (info->size);
		brasero_data_disc_file_new (disc,
					    uri,
					    sectors);

		brasero_data_disc_graft_new (disc,
					     uri,
					     path);
		graft = g_strdup (path);
	}

	return graft;
}

static char *
brasero_data_disc_new_row_added (BraseroDataDisc *disc,
				 const char *uri,
				 const char *path)
{
	char *graft = NULL;

	/* create and add a graft point if need be, that is if the file wasn't
	 * added to a directory which is its parent in the file system */
	if (brasero_data_disc_original_parent (disc, uri, path)) {
		graft = brasero_data_disc_graft_get (disc, path);
		brasero_data_disc_restore_uri (disc,
					       graft,
					       uri);
	}
	else {
		brasero_data_disc_graft_new (disc,
					     uri,
					     path);
		graft = g_strdup (path);
	}

	return graft;
}

static BraseroDiscResult
brasero_data_disc_new_row_real (BraseroDataDisc *disc,
				const gchar *uri,
				const GnomeVFSFileInfo *info,
				GnomeVFSResult result,
				const gchar *path,
				GSList *excluded)
{
	BraseroFilterStatus status;
	char *excluded_uri;
	BraseroFile *file;
	GSList *iter;
	gchar *graft;

	if (result != GNOME_VFS_OK || !brasero_data_disc_is_readable (info)) {
		brasero_data_disc_tree_remove_path (disc, path);
		brasero_data_disc_unreadable_dialog (disc,
						     uri,
						     result,
						     FALSE);
		return BRASERO_DISC_ERROR_UNREADABLE;
	}

	if (GNOME_VFS_FILE_INFO_SYMLINK (info)) {
		if (info->type == GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK) {
			brasero_data_disc_tree_remove_path (disc, path);
			brasero_data_disc_unreadable_dialog (disc,
							     uri,
							     GNOME_VFS_ERROR_TOO_MANY_LINKS,
							     FALSE);
			return BRASERO_DISC_BROKEN_SYMLINK;
		}

		uri = info->symlink_name;
	}

	if (info->type != GNOME_VFS_FILE_TYPE_DIRECTORY) {
		if (!g_hash_table_lookup (disc->priv->files, uri)) {
			graft = brasero_data_disc_new_file (disc,
							    uri,
							    path,
							    info);
		}
		else
			graft = brasero_data_disc_new_row_added (disc,
								 uri,
								 path);
	}
	else if ((file = g_hash_table_lookup (disc->priv->dirs, uri))
	      &&   file->sectors >= 0) {
		if (!g_slist_find (disc->priv->loading, file)) {
			GSList *paths;

			/* the problem here is that despite the fact this directory was explored
			 * one or various subdirectories could have been removed because excluded */
			brasero_data_disc_restore_excluded_children (disc, file);
			paths = g_slist_prepend (NULL, (char*) path);
			brasero_data_disc_replace_symlink_children (disc, file, paths);
			g_slist_free (paths);
		}
		else
			brasero_data_disc_directory_priority (disc, file);

		graft = brasero_data_disc_new_row_added (disc,
							 uri,
							 path);
	}
	else {
		brasero_data_disc_directory_new (disc,
						 g_strdup (uri),
						 FALSE);
		graft = brasero_data_disc_new_row_added (disc,
							 uri,
							 path);
	}

	for (iter = excluded; iter; iter = iter->next) {
		excluded_uri = iter->data;
		brasero_data_disc_exclude_uri (disc, graft, excluded_uri);
	}
	g_free (graft);

	/* very unlikely case */
	if (disc->priv->unreadable
	&& (status = GPOINTER_TO_INT (g_hash_table_lookup (disc->priv->unreadable, uri)))
	&&  status == BRASERO_FILTER_UNREADABLE)
		/* remove the file from unreadable */
		brasero_data_disc_unreadable_free (disc, uri);
	else
		brasero_data_disc_tree_set_path_from_info (disc, path, NULL, info);

	return BRASERO_DISC_OK;
}

static void
brasero_data_disc_new_row_destroy_cb (BraseroDataDisc *disc,
				      gpointer callback_data)
{
	GSList *references = callback_data;
	brasero_data_disc_reference_free_list (disc, references);
}

static gboolean
brasero_data_disc_new_row_cb (BraseroDataDisc *disc,
			      GSList *results,
			      gpointer callback_data)
{
	char *path;
	GSList *iter;
	BraseroDiscResult success;
	GSList *graft_infos = callback_data;

	for (iter = graft_infos; results && iter; iter = iter->next, results = results->next) {
		BraseroInfoAsyncResult * result;
		BraseroDataDiscReference reference;

		result = results->data;
		reference = GPOINTER_TO_INT (iter->data);

		/* we check wether the row still exists and if everything went well */
		path = brasero_data_disc_reference_get (disc, reference);
		if (!path)
			continue;

		/* create an entry in the tree */
		success = brasero_data_disc_new_row_real (disc,
							  result->uri,
							  result->info,
							  result->result,
							  path,
							  NULL);

		if (success == BRASERO_DISC_OK)
			brasero_data_disc_selection_changed (disc, TRUE);

		g_free (path);
	}

	/* free callback_data */
	return TRUE;
}

struct _DirContentsAsyncData {
	gchar *uri;
	GSList *list;
	BraseroDataDiscReference ref;
	GnomeVFSFileInfoOptions flags;
	GnomeVFSResult result;
	gboolean cancel;
};
typedef struct _DirContentsAsyncData DirContentsAsyncData;

static void
brasero_data_disc_get_dir_contents_cancel (gpointer user_data)
{
	DirContentsAsyncData *callback_data = user_data;

	callback_data->cancel = 1;
}

static void
brasero_data_disc_get_dir_contents_destroy (GObject *object, gpointer user_data)
{
	BraseroDataDisc *disc = BRASERO_DATA_DISC (object);
	DirContentsAsyncData *callback_data = user_data;

	g_slist_foreach (callback_data->list, (GFunc) gnome_vfs_file_info_unref, NULL);
	g_slist_free (callback_data->list);

	brasero_data_disc_reference_free (disc, callback_data->ref);
	g_free (callback_data->uri);
	g_free (callback_data);
}

static gboolean
brasero_data_disc_get_dir_contents_results (GObject *object, gpointer user_data)
{
	BraseroDataDisc *disc = BRASERO_DATA_DISC (object);
	DirContentsAsyncData *callback_data = user_data;
	GtkTreePath *treeparent;
	GSList *iter;
	gchar *path;

	/* see if the directory could be opened */
	if (callback_data->result != GNOME_VFS_OK) {
		brasero_data_disc_unreadable_dialog (disc,
						     callback_data->uri,
						     callback_data->result,
						     FALSE);
		return TRUE;
	}

	/* see if the reference is still valid */
	path = brasero_data_disc_reference_get (disc, callback_data->ref);
	if (!path)
		return TRUE;

	if (!brasero_data_disc_disc_path_to_tree_path (disc,
						       path,
						       &treeparent,
						       NULL)) {
		/* that's very unlikely since path reference exists 
		 * and this path had to be visible for drag'n'drop */
		g_free (path);
		return TRUE;		
	}

	for (iter = callback_data->list; iter; iter = iter->next) {
		BraseroDiscResult success;
		GnomeVFSFileInfo *info;
		gchar *uri_path;
		gchar *uri;

		info = iter->data;

		/* check joliet compatibility for this path inside the parent
		 * directory in the selection */
		success = brasero_data_disc_tree_check_name_validity (disc,
								      info->name,
								      treeparent,
								      TRUE);
		if (success != BRASERO_DISC_OK)
			continue;

		uri = g_build_path (G_DIR_SEPARATOR_S,
				    callback_data->uri,
				    info->name,
				    NULL);

		uri_path = g_build_path (G_DIR_SEPARATOR_S,
					 path,
					 info->name,
					 NULL);

		if (strlen (info->name) > 64)
			brasero_data_disc_joliet_incompat_add_path (disc, uri_path);

		if (!brasero_data_disc_tree_new_path (disc,
						      uri_path,
						      treeparent,
						      NULL)) {
			g_free (uri_path);
			g_free (uri);
			continue;
		}

		success = brasero_data_disc_new_row_real (disc,
							  uri,
							  info,
							  GNOME_VFS_OK,
							  uri_path,
							  NULL);

		if (success == BRASERO_DISC_OK) {
			gtk_notebook_set_current_page (GTK_NOTEBOOK (BRASERO_DATA_DISC (disc)->priv->notebook), 1);
			brasero_data_disc_selection_changed (disc, TRUE);
		}

		g_free (uri_path);
		g_free (uri);
	}
	
	gtk_tree_path_free (treeparent);
	g_free (path);

	brasero_data_disc_decrease_activity_counter (disc);

	/* free user_data */
	return TRUE;
}

static gboolean
brasero_data_disc_get_dir_contents_thread (GObject *object, gpointer user_data)
{
	DirContentsAsyncData *callback_data = user_data;
	GnomeVFSDirectoryHandle *handle = NULL;
	GnomeVFSFileInfo *info;
	GnomeVFSResult res;
	gchar *escaped_uri;

	escaped_uri = gnome_vfs_escape_host_and_path_string (callback_data->uri);
	res = gnome_vfs_directory_open (&handle,
					escaped_uri,
					callback_data->flags);

	g_free (escaped_uri);
	if (res != GNOME_VFS_OK || !handle) {
		/* we want to signal the user something went wrong */
		callback_data->result = res;
		return TRUE;
	}

	info = gnome_vfs_file_info_new ();
	while (1) {
		if (callback_data->cancel)
			break;

		res = gnome_vfs_directory_read_next (handle, info);
		if (res != GNOME_VFS_OK)
			break;

		/* special case for symlinks */
		if (info->type == GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK) {
			gchar *uri;

			uri = g_build_path (G_DIR_SEPARATOR_S,
					    callback_data->uri,
					    info->name,
					    NULL);
			gnome_vfs_file_info_clear (info);

			if (!brasero_utils_get_symlink_target (uri,
							       info,
							       callback_data->flags)) {
				/* since we checked for the existence of the file
				 * an error means a looping symbolic link */
				g_free (uri);
				continue;
			}

			g_free (uri);
		}

		callback_data->list = g_slist_append (callback_data->list, info);
		info = gnome_vfs_file_info_new ();
	}
	gnome_vfs_file_info_unref (info);
	gnome_vfs_directory_close (handle);

	if (callback_data->cancel)
		return FALSE;

	return TRUE;
}

static BraseroDiscResult
brasero_data_disc_add_directory_contents (BraseroDataDisc *disc,
					  const gchar *uri,
					  GnomeVFSFileInfoOptions flags,
					  const gchar *path)
{
	DirContentsAsyncData *callback_data;

	/* NOTE: we'll mostly use this function for non local uri since
	 * I don't see a user adding file:///. That's another reason to
	 * seek asynchronously */
	if (!disc->priv->jobs)
		disc->priv->jobs = brasero_async_job_manager_get_default ();

	if (!disc->priv->dir_contents_type) {
		disc->priv->dir_contents_type = brasero_async_job_manager_register_type (disc->priv->jobs,
											 G_OBJECT (disc),
											 brasero_data_disc_get_dir_contents_thread,
											 brasero_data_disc_get_dir_contents_results,
											 brasero_data_disc_get_dir_contents_destroy,
											 brasero_data_disc_get_dir_contents_cancel);
	}

	callback_data = g_new0 (DirContentsAsyncData, 1);
	callback_data->flags = flags;
	callback_data->uri = g_strdup (uri);
	callback_data->ref = brasero_data_disc_reference_new (disc, path);

	/* NOTE : if an error occurs the callback_data will be freed by async_job_manager */
	if (!brasero_async_job_manager_queue (disc->priv->jobs,
					      disc->priv->dir_contents_type,
					      callback_data))
		return BRASERO_DISC_ERROR_THREAD;

	brasero_data_disc_increase_activity_counter (disc);
	return BRASERO_DISC_OK;
}

static BraseroDiscResult
brasero_data_disc_add_uri_real (BraseroDataDisc *disc,
				const char *uri_arg,
				GtkTreePath *treeparent)
{
	BraseroDataDiscReference reference;
	BraseroDiscResult success;
	GSList *references = NULL;
	GnomeVFSURI *vfs_uri;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GSList *uris;
	gchar *name;
	gchar *path;
	gchar *uri;

	g_return_val_if_fail (uri_arg != NULL, BRASERO_DISC_ERROR_UNKNOWN);

	if (disc->priv->reject_files || disc->priv->is_loading)
		return BRASERO_DISC_NOT_READY;

	uri = brasero_utils_validate_uri (uri_arg, TRUE);

	/* g_path_get_basename is not comfortable with uri related
	 * to the root directory so check that before */
	vfs_uri = gnome_vfs_uri_new (uri);
	name = gnome_vfs_uri_extract_short_path_name (vfs_uri);
	gnome_vfs_uri_unref (vfs_uri);

	if (!name) {
		g_free (uri);
		return BRASERO_DISC_ERROR_FILE_NOT_FOUND;
	}

	/* create the path */
	if (treeparent && gtk_tree_path_get_depth (treeparent) > 0) {
		gchar *parent;
		gchar *unescaped_name;

		unescaped_name = gnome_vfs_unescape_string_for_display (name);
		g_free (name);
		name = unescaped_name;

		brasero_data_disc_tree_path_to_disc_path (disc,
							  treeparent,
							  &parent);

		path = g_build_path (G_DIR_SEPARATOR_S,
				     parent,
				     name,
				     NULL);
		g_free (parent);
	}
	else if (strcmp (name, G_DIR_SEPARATOR_S)) {
		gchar *unescaped_name;

		unescaped_name = gnome_vfs_unescape_string_for_display (name);
		g_free (name);
		name = unescaped_name;

		path = g_build_path (G_DIR_SEPARATOR_S, G_DIR_SEPARATOR_S, name, NULL);
	}
	else
		path = g_strdup (G_DIR_SEPARATOR_S);

	if (!strcmp (name, GNOME_VFS_URI_PATH_STR)) {
		/* this is a root directory: we don't add it since a child of 
		 * the root directory can't be a root itself. So we add all its
		 * contents. */
		success = brasero_data_disc_add_directory_contents (disc,
								    uri,
								    GNOME_VFS_FILE_INFO_GET_ACCESS_RIGHTS|
								    GNOME_VFS_FILE_INFO_GET_MIME_TYPE|
								    GNOME_VFS_FILE_INFO_FORCE_SLOW_MIME_TYPE,
								    path);

		g_free (uri);
		g_free (name);
		g_free (path);
		return success;
	}

	/* We make sure there isn't the same file in the directory
	 * and it is joliet compatible */
	success = brasero_data_disc_tree_check_name_validity (disc,
							      name,
							      treeparent,
							      TRUE);

	if (success != BRASERO_DISC_OK) {
		g_free (uri);
		g_free (name);
		g_free (path);
		return success;
	}

	gtk_notebook_set_current_page (GTK_NOTEBOOK (BRASERO_DATA_DISC (disc)->priv->notebook), 1);

	reference = brasero_data_disc_reference_new (disc, path);

	if (strlen (name) > 64)
		brasero_data_disc_joliet_incompat_add_path (disc, path);

	g_free (path);

	references = g_slist_prepend (NULL, GINT_TO_POINTER (reference));

	uris = g_slist_prepend (NULL, (gchar *) uri);
	success = brasero_data_disc_get_info_async (disc,
						    uris,
						    GNOME_VFS_FILE_INFO_GET_ACCESS_RIGHTS|
						    GNOME_VFS_FILE_INFO_GET_MIME_TYPE|
						    GNOME_VFS_FILE_INFO_FORCE_SLOW_MIME_TYPE,
						    brasero_data_disc_new_row_cb,
						    references,
						    brasero_data_disc_new_row_destroy_cb);
	g_slist_free (uris);
	g_free (uri);

	if (success != BRASERO_DISC_OK) {
		g_free (name);
		return success;
	}

	/* make it appear in the tree */
	model = disc->priv->model;
	if (treeparent
	&&  gtk_tree_path_get_depth (treeparent) > 0) {
		GtkTreeIter parent;

		gtk_tree_model_get_iter (model, &parent, treeparent);
		gtk_tree_store_append (GTK_TREE_STORE (model), &iter, &parent);
		brasero_data_disc_tree_update_directory_real (disc, &parent);
	}
	else
		gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);

	gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
			    NAME_COL, name,
			    SIZE_COL, _("(loading ...)"),
			    MIME_COL, _("(loading ...)"),
			    ROW_STATUS_COL, ROW_NEW,
			    -1);

	g_free (name);

	return BRASERO_DISC_OK;
}

static BraseroDiscResult
brasero_data_disc_add_uri (BraseroDisc *disc, const char *uri)
{
	char *unescaped_uri;
	BraseroDiscResult success;

	if (BRASERO_DATA_DISC (disc)->priv->is_loading)
		return BRASERO_DISC_LOADING;

	unescaped_uri = gnome_vfs_unescape_string_for_display (uri);
	success = brasero_data_disc_add_uri_real (BRASERO_DATA_DISC (disc),
						  unescaped_uri,
						  NULL);
	g_free (unescaped_uri);
	return success;
}

/********************************* export internal tracks *********************/
struct _MakeExcludedListData {
	GSList *list;
	gchar *graft;
};
typedef struct _MakeExcludedListData MakeExcludedListData;
struct _MakeListData {
	GSList *list;
	BraseroDataDisc *disc;
	gboolean joliet_compat;
};
typedef struct _MakeListData MakeListData;

static void
_foreach_unreadable_make_list_cb (const gchar *uri,
				  BraseroFilterStatus status,
				  MakeListData *data)
{
	data->list = g_slist_prepend (data->list, g_strdup (uri));
}

static void
_foreach_symlink_make_list_cb (const gchar *symlink,
			       gint value,
			       MakeListData *data)
{
	data->list = g_slist_prepend (data->list, g_strdup (symlink));
}

static void
_foreach_restored_make_list_cb (const char *restored,
			        BraseroFilterStatus status,
			        MakeListData *data)
{
	data->list = g_slist_prepend (data->list, g_strdup (restored));
}

static void
_foreach_excluded_make_list_cb (const char *uri,
				GSList *grafts,
				MakeExcludedListData *data)
{
	for (; grafts; grafts = grafts->next) {
		if (data->graft == grafts->data) {
			data->list = g_slist_prepend (data->list, g_strdup (uri));
			return;
		}
	}
}

static void
_foreach_grafts_make_list_cb (char *path,
			      const gchar *uri,
			      MakeListData *data)
{
	MakeExcludedListData callback_data;
	BraseroGraftPt *graft;

	graft = g_new0 (BraseroGraftPt, 1);
	graft->uri = uri != BRASERO_CREATED_DIR ? g_strdup (uri) : NULL;

	if (data->joliet_compat)
		/* make sure that this path is joliet compatible */
		graft->path = brasero_data_disc_joliet_incompat_get_joliet_compliant_path (data->disc, path);
	else
		graft->path = g_strdup (path);

	/* no need to check for dummy since we are in the paths hash table */
	if (uri
	&&  data->disc->priv->excluded
	&&  g_hash_table_lookup (data->disc->priv->dirs, uri)) {
		callback_data.list = NULL;
		callback_data.graft = path;

		g_hash_table_foreach (data->disc->priv->excluded,
				      (GHFunc) _foreach_excluded_make_list_cb,
				      &callback_data);
		graft->excluded = callback_data.list;
	}

	data->list = g_slist_prepend (data->list, graft);
}

static void
_foreach_joliet_non_compliant_cb (gchar *key,
				  GSList *list,
				  MakeListData *data)
{
	BraseroGraftPt *graft;
	gchar *parent;
	gint i = 0;

	parent = g_path_get_dirname (key);
	for (; list; list = list->next, i++) {
		gchar *name;
		gchar *path;

		name = list->data;
		path = g_build_path (G_DIR_SEPARATOR_S,
				     parent,
				     name,
				     NULL);

		if (g_hash_table_lookup (data->disc->priv->paths, path)) {
			/* this one is already in grafts list */
			g_free (path);
			continue;
		}

		graft = g_new0 (BraseroGraftPt, 1);

		/* no need to look for excluded here */
		graft->path = brasero_data_disc_joliet_incompat_get_joliet_compliant_path (data->disc, path);
		graft->uri = brasero_data_disc_path_to_uri (data->disc, path);
		g_free (path);

		data->list = g_slist_prepend (data->list, graft);
	}
	g_free (parent);
}

static BraseroDiscResult
brasero_data_disc_get_track_real (BraseroDataDisc *disc,
				  GSList **grafts,
				  GSList **unreadable,
				  GSList **restored,
				  gboolean joliet_compat)
{
	MakeListData callback_data;

	if (!g_hash_table_size (disc->priv->paths))
		return BRASERO_DISC_ERROR_EMPTY_SELECTION;

	callback_data.disc = disc;
	callback_data.joliet_compat = joliet_compat;

	if (unreadable) {
		callback_data.list = NULL;
		if (disc->priv->unreadable)
			g_hash_table_foreach (disc->priv->unreadable,
					      (GHFunc) _foreach_unreadable_make_list_cb,
					      &callback_data);
		if (disc->priv->symlinks)
			g_hash_table_foreach (disc->priv->symlinks,
					      (GHFunc) _foreach_symlink_make_list_cb,
					      &callback_data);
		*unreadable = callback_data.list;
	}

	if (restored) {
		callback_data.list = NULL;
		if (disc->priv->restored)
			g_hash_table_foreach (disc->priv->restored,
					      (GHFunc) _foreach_restored_make_list_cb,
					      &callback_data);
		*restored = callback_data.list;
	}

	if (grafts) {
		callback_data.list = NULL;
		g_hash_table_foreach (disc->priv->paths,
				      (GHFunc) _foreach_grafts_make_list_cb,
				      &callback_data);

		if (joliet_compat && disc->priv->joliet_non_compliant) {
			/* we have to change the name of the remaining files */
			g_hash_table_foreach (disc->priv->joliet_non_compliant,
					      (GHFunc) _foreach_joliet_non_compliant_cb,
					      &callback_data);
		}

		*grafts = callback_data.list;
	}

	return BRASERO_DISC_OK;
}

static BraseroDiscResult
brasero_data_disc_get_track (BraseroDisc *disc,
			     BraseroDiscTrack *track)
{
	GSList *grafts= NULL;
	GSList *restored = NULL;
	GSList *unreadable = NULL;

	brasero_data_disc_get_track_real (BRASERO_DATA_DISC (disc),
					  &grafts,
					  &unreadable,
					  &restored,
					  FALSE);
	if (!restored && !grafts)
		return BRASERO_DISC_ERROR_EMPTY_SELECTION;

	track->type = BRASERO_DISC_TRACK_DATA;
	track->contents.data.grafts = grafts;
	track->contents.data.unreadable = unreadable;
	track->contents.data.restored = restored;

	return BRASERO_DISC_OK;
}

static gboolean
brasero_data_disc_is_video_DVD (BraseroDataDisc *disc)
{
	GtkTreeIter iter;
	GtkTreeIter parent;
	GtkTreeModel *model;
	GtkTreePath *treepath;
	gboolean has_ifo, has_vob, has_bup;

	/* here we check that the selection can be burnt as a video DVD.
	 * It must have :
	 * - a VIDEO_TS and AUDIO_TS at its root
	 * - the VIDEO_TS directory must have VIDEO_TS.IFO, VIDEO_TS.VOB
	     and VIDEO_TS.BUP inside */

	/* NOTE: since VIDEO_TS and AUDIO_TS must be at the root of the DVD
	 * they are necessarily graft points and are in paths. Moreover we can
	 * easily check in the model if the other files exist since being at 
	 * the root VIDEO_TS has also been necessarily explored */
	if (!g_hash_table_lookup (disc->priv->paths, "/VIDEO_TS")
	||  !g_hash_table_lookup (disc->priv->paths, "/AUDIO_TS"))
		return FALSE;

	model = disc->priv->model;
	brasero_data_disc_disc_path_to_tree_path (disc, "/VIDEO_TS", &treepath, NULL);
	gtk_tree_model_get_iter (model, &parent, treepath);
	if (!gtk_tree_model_iter_children (model, &iter, &parent))
		return FALSE;

	has_ifo = has_vob = has_bup = FALSE;

	do {
		gchar *name;

		gtk_tree_model_get (model, &iter,
				    NAME_COL, &name,
				    -1);

		if (!strcmp (name, "VIDEO_TS.IFO"))
			has_ifo = TRUE;
		else if (!strcmp (name, "VIDEO_TS.VOB"))
			has_vob = TRUE;
		else if (!strcmp (name, "VIDEO_TS.BUP"))
			has_bup = TRUE;

		if (has_ifo && has_vob && has_bup)
			return TRUE;

	} while (gtk_tree_model_iter_next (model, &iter));

	return FALSE;
}

static BraseroDiscResult
brasero_data_disc_get_track_source (BraseroDisc *disc,
				    BraseroTrackSource **source,
				    BraseroImageFormat format)
{
	GSList *grafts= NULL;
	gboolean joliet_compat;
	GSList *unreadable = NULL;
	BraseroTrackSource *src = NULL;

	/* the rule here is that if we have default type then we see if 
	 * the track can be joliet compliant without any name changes.
	 * If the joliet extension is forced then make the track joliet
	 * compliant even if we have to change the names */
	if (format == BRASERO_IMAGE_FORMAT_ANY) {
		if (BRASERO_DATA_DISC (disc)->priv->joliet_non_compliant)
			joliet_compat = FALSE;
		else
			joliet_compat = TRUE;
	}
	else if (format & BRASERO_IMAGE_FORMAT_JOLIET)
		joliet_compat = TRUE;
	else
		joliet_compat = FALSE;

	brasero_data_disc_get_track_real (BRASERO_DATA_DISC (disc),
					  &grafts,
					  &unreadable,
					  NULL,
					  joliet_compat); 

	if (!grafts)
		return BRASERO_DISC_ERROR_EMPTY_SELECTION;

	src = g_new0 (BraseroTrackSource, 1);
	src->type = BRASERO_TRACK_SOURCE_DATA;
	src->contents.data.grafts = grafts;
	src->contents.data.excluded = unreadable;

	if (joliet_compat)
		src->format = BRASERO_IMAGE_FORMAT_JOLIET | BRASERO_IMAGE_FORMAT_ISO;
	else
		src->format = BRASERO_IMAGE_FORMAT_ISO;

	if (brasero_data_disc_is_video_DVD (BRASERO_DATA_DISC (disc)))
		src->format |= BRASERO_IMAGE_FORMAT_VIDEO;

	*source = src;
	return BRASERO_DISC_OK;
}

/******************************** load track ***********************************/
static void
brasero_data_disc_load_error_dialog (BraseroDataDisc *disc)
{
	GtkWidget *dialog;
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (disc));
	dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					 GTK_DIALOG_DESTROY_WITH_PARENT |
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_WARNING,
					 GTK_BUTTONS_CLOSE,
					 _("Project couldn't be loaded:"));

	gtk_window_set_title (GTK_WINDOW (dialog), _("Project loading failure"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  _("A thread couldn't be created"));

	gtk_widget_show_all (dialog);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

enum {
	BRASERO_GRAFT_CHECK_OK,
	BRASERO_GRAFT_CHECK_PARENT_FILE,
	BRASERO_GRAFT_CHECK_PARENT_UNREADABLE,
	BRASERO_GRAFT_CHECK_PARENT_NOT_FOUND,
	BRASERO_GRAFT_CHECK_FILE_WITH_SAME_NAME, 
	BRASERO_GRAFT_CHECK_DIR_WITH_SAME_NAME
};

struct _BraseroCheckGraftResultData {
	BraseroDataDiscReference ref;
	gchar *parent;
	gchar *path;
	gint status;

	gint cancel:1;
};
typedef struct _BraseroCheckGraftResultData BraseroCheckGraftResultData;

static void
brasero_data_disc_graft_check_cancel (gpointer callback_data)
{
	GSList *graft = callback_data;

	for (graft = callback_data; graft; graft = graft->next) {
		BraseroCheckGraftResultData *result;

		result = graft->data;
		result->cancel = 1;
	}
}

static void
brasero_data_disc_graft_check_destroy (GObject *object, gpointer callback_data)
{
	GSList *iter;
	BraseroCheckGraftResultData *graft;
	BraseroDataDisc *disc = BRASERO_DATA_DISC (object);

	for (iter = callback_data; iter; iter = iter->next) {
		graft = iter->data;

		brasero_data_disc_reference_free (disc, graft->ref);
		g_free (graft->parent);
		g_free (graft->path);
		g_free (graft);
	}
}

static gboolean
brasero_data_disc_graft_check_result (GObject *object, gpointer callback_data)
{
	BraseroDataDisc *disc = BRASERO_DATA_DISC (object);
	BraseroCheckGraftResultData *graft;
	const gchar *graft_path;
	gchar *graft_uri;
	gchar *last_path;
	gchar *ref_path;
	GSList *iter;
	gchar *parent;

	for (iter = callback_data; iter; iter = iter->next) {
		graft = iter->data;

		if (graft->status == BRASERO_GRAFT_CHECK_OK)
			continue;

		/* check that we still care about it */
		ref_path = brasero_data_disc_reference_get (disc, graft->ref);
		if (!ref_path)
			continue;

		if (strcmp (ref_path, graft->path)) {
			g_free (ref_path);
			continue;
		}
		g_free (ref_path);

		parent = g_path_get_dirname (graft->path);

		/* NOTE: graft_path has to exist since we checked the reference */
		graft_path = brasero_data_disc_graft_get_real (disc, parent);
		if (graft->status != BRASERO_GRAFT_CHECK_PARENT_NOT_FOUND) {
			GSList *excluding = NULL;

			/* see that it isn't already excluded if not do it */
			if (disc->priv->excluded)
				excluding = g_hash_table_lookup (disc->priv->excluded,
								 graft->parent);

			if (excluding
			&& !g_slist_find (excluding, graft_path)) {
				brasero_data_disc_exclude_uri (disc,
							       graft_path,
							       graft->parent);
			}
		}

		if (graft->status == BRASERO_GRAFT_CHECK_FILE_WITH_SAME_NAME
		||  graft->status == BRASERO_GRAFT_CHECK_DIR_WITH_SAME_NAME) {
			g_free (parent);
			continue;
		}

		/* we need to create all the directories until last */
		/* NOTE : graft_uri can't be a created dir as we checked that before */
		graft_uri = g_hash_table_lookup (disc->priv->paths, graft_path);
		last_path = g_strconcat (graft_path,
					 graft->parent + strlen (graft_uri),
					 NULL);

		while (strcmp (parent, last_path) && strcmp (parent, G_DIR_SEPARATOR_S)) {
			gchar *tmp;
			gchar *name;

			brasero_data_disc_graft_new (disc,
						     NULL,
						     parent);

			name = g_path_get_basename (parent);
			if (strlen (name) > 64)
				brasero_data_disc_joliet_incompat_add_path (disc, parent);
			g_free (name);

			tmp = parent;
			parent = g_path_get_dirname (parent);
			g_free (tmp);
		}

		/* NOTE: the last directory exists or was excluded */
		g_free (last_path);
		g_free (parent);
	}

	return TRUE;
}

static gboolean
brasero_data_disc_graft_check_thread (GObject *object, gpointer callback_data)
{
	BraseroCheckGraftResultData *graft;
	GnomeVFSFileInfo *info;
	GnomeVFSResult result;
	char *escaped_uri;
	GSList *iter;
	char *uri;
	char *tmp;

	info = gnome_vfs_file_info_new ();
	for (iter = callback_data; iter; iter = iter->next) {
		graft = iter->data;

		if (graft->cancel)
			return FALSE;

		/* check a file with the same name doesn't exist */
		escaped_uri = gnome_vfs_escape_host_and_path_string (graft->parent);
		result = gnome_vfs_get_file_info (escaped_uri, info, 0);
		g_free (escaped_uri);

		if (result != GNOME_VFS_ERROR_NOT_FOUND) {
			if (info->type == GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK) {
				/* we ignore this path since when the symlink is met
				 * we'll check its path doesn't overlap a graft point
				 * and if so, nothing will happen */
				graft->status = BRASERO_GRAFT_CHECK_OK;
			}
			else if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
				graft->status = BRASERO_GRAFT_CHECK_DIR_WITH_SAME_NAME;
			else
				graft->status = BRASERO_GRAFT_CHECK_FILE_WITH_SAME_NAME;

			gnome_vfs_file_info_clear (info);
			continue;
		}

		/* now we check that we have an existing directory as parent on
		 * the disc */
		uri = g_path_get_dirname (graft->parent);
		g_free (graft->parent);

		gnome_vfs_file_info_clear (info);
		escaped_uri = gnome_vfs_escape_host_and_path_string (uri);
		result = gnome_vfs_get_file_info (escaped_uri, info, 0);		
		g_free (escaped_uri);

		if (result == GNOME_VFS_OK && info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
			graft->status = BRASERO_GRAFT_CHECK_OK;
			gnome_vfs_file_info_clear (info);
			graft->parent = uri;
			continue;
		}

		while (1) {
			if (result == GNOME_VFS_ERROR_NOT_FOUND) {
				tmp = uri;
				uri = g_path_get_dirname (uri);
				g_free (tmp);
			}
			else if (info->type == GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK) {
				/* symlink: do as if it didn't exist since it will
				 * be replaced when met during exploration at this
				 * point we'll check if the paths of the symlink
				 * doesn't overlap a graft point and if so nothing
				 * will happen */
				graft->status = BRASERO_GRAFT_CHECK_PARENT_NOT_FOUND;

				tmp = uri;
				uri = g_path_get_dirname (uri);
				g_free (tmp);
			}
			else if (result != GNOME_VFS_OK) {
				graft->status = BRASERO_GRAFT_CHECK_PARENT_UNREADABLE;
				break;
			}
			else if (info->type != GNOME_VFS_FILE_TYPE_DIRECTORY) {
				graft->status = BRASERO_GRAFT_CHECK_PARENT_FILE;
				break;
			}
			else {
				graft->status = BRASERO_GRAFT_CHECK_PARENT_NOT_FOUND;
				break;
			}
	
			gnome_vfs_file_info_clear (info);
			escaped_uri = gnome_vfs_escape_host_and_path_string (uri);
			result = gnome_vfs_get_file_info (escaped_uri, info, 0);
			g_free (escaped_uri);
		}
		graft->parent = uri;
		gnome_vfs_file_info_clear (info);
	}

	gnome_vfs_file_info_unref (info);
	return TRUE;
}

static void
brasero_data_disc_path_create (BraseroDataDisc *disc,
			       const gchar *path)
{
	gchar *tmp;
	gchar *tmp_path;

	tmp_path = g_path_get_dirname (path);
	while (strcmp (tmp_path, G_DIR_SEPARATOR_S) && !g_hash_table_lookup (disc->priv->paths, tmp_path)) {
		gchar *name;

		brasero_data_disc_graft_new (disc, NULL, tmp_path);
		brasero_data_disc_tree_new_empty_folder (disc, tmp_path);

		name = g_path_get_basename (tmp_path);
		if (strlen (name) > 64)
			brasero_data_disc_joliet_incompat_add_path (disc, tmp_path);
		g_free (name);

		tmp = tmp_path;
		tmp_path = g_path_get_dirname (tmp);
		if (!strcmp (tmp_path, G_DIR_SEPARATOR_S))
			brasero_data_disc_expose_path (disc, tmp);

		g_free (tmp);
	}

	g_free (tmp_path);
}

/* This function checks that graft points consistency:
 * First, when graft points are added as children of another graft points
 * we need to make sure that a child file of the top graft points on the 
 * file system doesn't have the same name as the child graft point.
 * If so, we exclude child file.
 * A second problem might be that one of the parent directory doesn't exist
 * on the file system. we'll have therefore to add empty directories */
static void
brasero_data_disc_graft_check (BraseroDataDisc *disc,
			       GSList *paths)
{
	gchar *path;
	gchar *parent;
	GSList *iter;
	GSList *grafts = NULL;
	const gchar *graft_uri;
	const gchar *graft_path;
	BraseroCheckGraftResultData *graft;

	/* we make sure that the path has at least one parent graft path,
	 * except of course if it is at root */
	for (iter = paths; iter; iter = iter->next) {
		path = iter->data;

		/* search for the first parent graft path available */
		parent = g_path_get_dirname (path);
		graft_path = brasero_data_disc_graft_get_real (disc, parent);
		g_free (parent);

		if (!graft_path) {
			/* no parent (maybe it was unreadable) so we need to 
			 * create empty directories but we don't need to check
			 * if it overlaps anything */
			brasero_data_disc_path_create (disc, path);
			continue;
		}

		graft_uri = g_hash_table_lookup (disc->priv->paths, graft_path);
		if (!graft_uri || graft_uri == BRASERO_CREATED_DIR) {
			/* NOTE: we make sure in this case that this directory
			 * is the direct parent of our path, otherwise create 
			 * the missing parent directories. One graft parent
			 * in between could have gone unreadable. */
			brasero_data_disc_path_create (disc, path);
			continue;
		}

		graft = g_new0 (BraseroCheckGraftResultData, 1);
		graft->path = g_strdup (path);
		graft->ref = brasero_data_disc_reference_new (disc, path);
		graft->parent = g_strconcat (graft_uri,
					     path + strlen (graft_path),
					     NULL);
		grafts = g_slist_prepend (grafts, graft);
	}

	if (!grafts)
		return;

	if (!disc->priv->jobs)
		disc->priv->jobs = brasero_async_job_manager_get_default ();

	if (!disc->priv->check_graft) {
		disc->priv->check_graft = brasero_async_job_manager_register_type (disc->priv->jobs,
										   G_OBJECT (disc),
										   brasero_data_disc_graft_check_thread,
										   brasero_data_disc_graft_check_result,
										   brasero_data_disc_graft_check_destroy,
										   brasero_data_disc_graft_check_cancel);
	}

	if (!brasero_async_job_manager_queue (disc->priv->jobs,
					      disc->priv->check_graft,
					      grafts)) {
		/* it failed so cancel everything */
		brasero_data_disc_reset_real (disc);

		/* warn the user */
		brasero_data_disc_load_error_dialog (disc);
	}
}

static void
brasero_data_disc_load_destroy_cb (BraseroDataDisc *disc,
				   gpointer callback_data)
{
	GSList *grafts = callback_data;

	g_slist_foreach (grafts, (GFunc) brasero_graft_point_free, NULL);
	g_slist_free (grafts);
}

static gboolean
brasero_data_disc_load_step_2 (BraseroDataDisc *disc,
			       GSList *results,
			       gpointer callback_data)
{
	GSList *iter;
	GSList *paths = NULL;
	BraseroGraftPt *graft;
	BraseroDiscResult success;
	GSList *grafts = callback_data;
	BraseroInfoAsyncResult *result;

	/* whenever a graft point is valid add it to the hash */
	for (iter = grafts; iter; iter = iter->next) {
		graft = iter->data;

		if (!graft->uri) {
			gchar *parent;

			/* these are created directories no need to check results */
			brasero_data_disc_graft_new (disc,
						     NULL,
						     graft->path);

			/* see if the parent was added to the tree at the root
			 * of the disc or if it's itself at the root of the disc.
			 * if so, show it in the tree */
			parent = g_path_get_dirname (graft->path);

			if (!strcmp (parent, G_DIR_SEPARATOR_S)
			&&  brasero_data_disc_tree_new_empty_folder (disc, graft->path))
				/* we can expose its contents right away (won't be explored) */
				brasero_data_disc_expose_path (disc, graft->path);
			else if (g_hash_table_lookup (disc->priv->paths, parent)) {
				gchar *tmp;

				tmp = parent;
				parent = g_path_get_dirname (tmp);
				g_free (tmp);

				if (!strcmp (parent, G_DIR_SEPARATOR_S))
					brasero_data_disc_tree_new_empty_folder_real (disc,
										      graft->path,
										      ROW_NOT_EXPLORED); 
			}
			g_free (parent);

			success = BRASERO_DISC_OK;
		}
		else {
			gchar *parent;

			/* see if the parent was added to the tree at the root
			 * of the disc or if it's itself at the root of the disc.
			 * if so, show it in the tree */
			parent = g_path_get_dirname (graft->path);
			if (g_hash_table_lookup (disc->priv->paths, parent)) {
				gchar *tmp;

				tmp = parent;
				parent = g_path_get_dirname (parent);
				g_free (tmp);

				if (!strcmp (parent, G_DIR_SEPARATOR_S))
					brasero_data_disc_tree_new_path (disc,
									 graft->path,
									 NULL,
									 NULL);
			}
			else if (!strcmp (parent, G_DIR_SEPARATOR_S)) {
				brasero_data_disc_tree_new_path (disc,
								 graft->path,
								 NULL,
								 NULL);
			}
			g_free (parent);

			if (results) {
				result = results->data;
				/* the following function will create a graft point */
				success = brasero_data_disc_new_row_real (disc,
									  result->uri,
									  result->info,
									  result->result,
									  graft->path,
									  graft->excluded);
				results = results->next;
			}
			else {
				/* we shouldn't reach this since there should be
				 * as many results as there are graft files */
				g_slist_foreach (paths, (GFunc) g_free, NULL);
				g_slist_free (paths);

				/* warn the user */
				brasero_data_disc_load_error_dialog (disc);
				brasero_data_disc_reset_real (disc);

				/* free callback_data/grafts */
				return TRUE;
			}
		}

		if (success == BRASERO_DISC_OK) {
			gchar *parent;
			gchar *name;

			name = g_path_get_basename (graft->path);
			if (strlen (name) > 64)
				brasero_data_disc_joliet_incompat_add_path (disc, graft->path);
			g_free (name);

			/* This is for additional checks (see above function) */
			parent = g_path_get_dirname (graft->path);
			if (strcmp (parent, G_DIR_SEPARATOR_S)) 
				paths = g_slist_prepend (paths, g_strdup (graft->path));

			g_free (parent);
		}
	}

	if (paths) {
		brasero_data_disc_graft_check (disc, paths);
		g_slist_foreach (paths, (GFunc) g_free, NULL);
		g_slist_free (paths);
	}

	disc->priv->is_loading = FALSE;
	brasero_data_disc_selection_changed (disc, (g_hash_table_size (disc->priv->paths) > 0));

	/* we don't need grafts/callback_data any more free the list */
	return TRUE;
}

/* we now check if the graft points are still valid files. */
static gboolean
brasero_data_disc_load_step_1 (BraseroDataDisc *disc,
			       GSList *results,
			       gpointer callback_data)
{
	BraseroInfoAsyncResult *result;
	GSList *grafts = callback_data;
	BraseroDiscResult success;
	BraseroGraftPt *graft;
	GSList *iter;
	GSList *next;
	GSList *uris;

	/* see if restored file are still valid. If so, add them to restored hash */
	for (; results; results = results->next) {
		result = results->data;

		if (result->result == GNOME_VFS_ERROR_NOT_FOUND)
			continue;

		brasero_data_disc_restored_new (disc,
						result->uri,
						BRASERO_FILTER_UNKNOWN);
	}

	uris = NULL;
	for (iter = grafts; iter; iter = next) {
		graft = iter->data;
		next = iter->next;

		if (graft->uri) {
			/* NOTE: it might happen that the same uri will be put
			 * several times in the list but it doesn't matter as
			 * gnome-vfs caches the results so it won't really hurt */
			uris = g_slist_prepend (uris, graft->uri);
		}
	}

	uris = g_slist_reverse (uris);
	success = brasero_data_disc_get_info_async (disc,
						    uris,
						    GNOME_VFS_FILE_INFO_GET_ACCESS_RIGHTS |
						    GNOME_VFS_FILE_INFO_GET_MIME_TYPE |
						    GNOME_VFS_FILE_INFO_FORCE_SLOW_MIME_TYPE,
						    brasero_data_disc_load_step_2,
						    grafts,
						    brasero_data_disc_load_destroy_cb);
	g_slist_free (uris);

	if (success != BRASERO_DISC_OK) {
		/* warn the user */
		brasero_data_disc_load_error_dialog (disc);
		brasero_data_disc_reset_real (disc);

		/* it failed free callback_data/grafts */
		return TRUE;
	}

	/* we return FALSE to tell we don't want callback_data/grafts to be
	 * freed now since we re-use it */
	return FALSE;
}

/* first, we make list and copy graft points and restored files:
 * we'll then check first the existence and the state of restored 
 * files. we must add them to the list before the graft points since
 * we don't want to see them added to the unreadable list when we'll
 * explore the graft points */
static BraseroDiscResult
brasero_data_disc_load_track (BraseroDisc *disc,
			      BraseroDiscTrack *track)
{
	char *uri;
	GSList *iter;
	GSList *uris = NULL;
	GSList *grafts = NULL;
	BraseroGraftPt *graft;
	BraseroDiscResult success;

	g_return_val_if_fail (track->type == BRASERO_DISC_TRACK_DATA, BRASERO_DISC_OK);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (BRASERO_DATA_DISC (disc)->priv->notebook), 1);

	if (track->contents.data.grafts == NULL)
		return BRASERO_DISC_ERROR_EMPTY_SELECTION;

	/* we don't really need to add the unreadable files since 
	 * the important thing is those that must be restored.
	 * that's the same for the symlinks both types of files
	 * will be added as exploration of graft points goes on */
	for (iter = track->contents.data.grafts; iter; iter = iter->next) {
		graft = iter->data;
		grafts = g_slist_prepend (grafts,
					  brasero_graft_point_copy (graft));
	}

	grafts = g_slist_reverse (grafts);

	/* add restored : we must make sure that they still exist 
	 * before doing the exploration of graft points so that 
	 * they won't be added to unreadable list */
	for (iter = track->contents.data.restored; iter; iter = iter->next) {
		uri = iter->data;
		uris = g_slist_prepend (uris, uri);
	}

	success = brasero_data_disc_get_info_async (BRASERO_DATA_DISC (disc),
						    uris,
						    GNOME_VFS_FILE_INFO_GET_ACCESS_RIGHTS,
						    brasero_data_disc_load_step_1,
						    grafts,
						    brasero_data_disc_load_destroy_cb);
	g_slist_free (uris);

	if (success != BRASERO_DISC_OK)
		return success;

	BRASERO_DATA_DISC (disc)->priv->is_loading = TRUE;
	return BRASERO_DISC_LOADING;
}

/******************************* row moving ************************************/
static BraseroDiscResult
brasero_data_disc_restore_row (BraseroDataDisc *disc,
			       const char *uri,
			       const char *oldpath,
			       const char *newpath)
{
	BraseroFile *file;
	char *newgraft;
	char *oldgraft;

	/* the file is no longer excluded since it came back to the right place */
	newgraft = brasero_data_disc_graft_get (disc, newpath);
	brasero_data_disc_restore_uri (disc, newgraft, uri);

	/* NOTE: we don't know for sure that oldpath is a graft:
	 * indeed it could be that the same directory was grafted
	 * twice and one of his subdirectories is moved between
	 * the two graft points */
	oldgraft = brasero_data_disc_graft_get (disc, oldpath);

	/* now we need to find the old graft point and remove it */
	/* no need to see if it's a dummy since this files was 
	 * already in the selection and just his path has changed */
	if ((file = g_hash_table_lookup (disc->priv->dirs, uri))) {
		/* we move all children graft point as well */
		brasero_data_disc_graft_children_move (disc,
						       oldpath,
						       newpath);

		/* no need for mutex here since it doesn't change */
		if (!g_slist_find (disc->priv->loading, file->uri))
			brasero_data_disc_graft_transfer_excluded (disc,
								   oldgraft,
								   newgraft);
	}
	else if (!g_hash_table_lookup (disc->priv->files, uri)) {
		g_free (newgraft);
		g_free (oldgraft);
		g_error ("ERROR: This file (%s) must have a graft point.\n",
			 uri);
		/* ERROR : this file must have a graft point since it was moved 
		 * back to place. Now the graft points are either in loading,
		 * dirs, files */
	}

	/* find the old graft points if it exists
	 * otherwise exclude it (see NOTE above) */
	/* NOTE : in case of directories no need to update 
	 * the excluded hash (excluded were moved) */
	if (strcmp (oldgraft, oldpath))
		brasero_data_disc_exclude_uri (disc, oldgraft, uri);
	else
		brasero_data_disc_graft_remove (disc, oldgraft);

	g_free (newgraft);
	g_free (oldgraft);

	return BRASERO_DISC_OK;
}

static void
brasero_data_disc_move_row_in_dirs_hash (BraseroDataDisc *disc,
					 BraseroFile *dir,
					 const char *oldpath,
					 const char *newpath)
{
	char *oldgraft;

	/* move all children graft points */
	brasero_data_disc_graft_children_move (disc,
					       oldpath,
					       newpath);

	/* see if the dir the user is moving was already grafted at oldpath */
	if (g_hash_table_lookup (disc->priv->paths, oldpath)) {
		brasero_data_disc_graft_changed (disc,
						 oldpath,
						 newpath);
		return;
	}

	/* we exclude it from his previous graft point */
	oldgraft = brasero_data_disc_graft_get (disc, oldpath);
	brasero_data_disc_exclude_uri (disc, oldgraft, dir->uri);

	/* apparently the old path did not correspond to 
	 * a graft point so we make a new graft point */
	brasero_data_disc_graft_new (disc,
				     dir->uri,
				     newpath);

	/* now since it became a graft point we must remove
	 * from the old parent the excluded that were pointing
	 * to children of uri and add them for the new graft
	 * point we created. NOTE : that's only for directories
	 * which are not loading */
	/* no need for mutex hash doesn't change */
	if (!g_slist_find (disc->priv->loading, dir->uri))
		brasero_data_disc_graft_transfer_excluded (disc,
							   oldgraft,
							   newpath);

	g_free (oldgraft);
}

static void
brasero_data_disc_move_row_in_files_hash (BraseroDataDisc *disc,
					  BraseroFile *file,
					  const char *oldpath,
					  const char *newpath)
{
	char *oldgraft;

	/* see if the old path was already grafted */
	if (g_hash_table_lookup (disc->priv->paths, oldpath)) {
		brasero_data_disc_graft_changed (disc,
						 oldpath,
						 newpath);
		return;
	}

	/* we exclude it from his previous graft point */
	oldgraft = brasero_data_disc_graft_get (disc, oldpath);
	brasero_data_disc_exclude_uri (disc, oldgraft, file->uri);
	g_free (oldgraft);

	/* apparently the old path did not correspond to a graft point so we
	 * make a new graft point since a moved file becomes a graft point 
	 * we exclude it from his previous graft point as well */
	brasero_data_disc_graft_new (disc, file->uri, newpath);
}

struct _MoveRowSimpleFileData {
	char *newpath;
	char *oldpath;
};
typedef struct _MoveRowSimpleFileData MoveRowSimpleFileData;

static void
brasero_data_disc_move_row_simple_file_destroy_cb (BraseroDataDisc *disc,
						   gpointer data)
{
	MoveRowSimpleFileData *callback_data = data;

	g_free (callback_data->newpath);
	g_free (callback_data->oldpath);
	g_free (callback_data);
}

static gboolean
brasero_data_disc_move_row_simple_file_cb (BraseroDataDisc *disc,
					   GSList *results,
					   gpointer user_data)
{
	MoveRowSimpleFileData *callback_data = user_data;
	BraseroInfoAsyncResult *result;
	GnomeVFSFileInfo *info;
	BraseroFile *file;
	char *parenturi;
	char *graft;
	char *uri;

	for (; results; results = results->next) {
		gint64 sectors;

		result = results->data;

		uri = result->uri;
		info = result->info;

		/* see if the parent still exists and we are still valid */
		parenturi = g_path_get_dirname (uri);
		file = g_hash_table_lookup (disc->priv->dirs, parenturi);
		g_free (parenturi);

		if (!file || file->sectors < 0)
			continue;

		if (result->result == GNOME_VFS_ERROR_NOT_FOUND) {
			brasero_data_disc_remove_uri_from_tree (disc, uri, TRUE);
			brasero_data_disc_add_rescan (disc, file);
			continue;
		}
		if (result->result == GNOME_VFS_ERROR_LOOP) {
			brasero_data_disc_remove_uri_from_tree (disc, uri, TRUE);
			brasero_data_disc_add_rescan (disc, file);
			brasero_data_disc_unreadable_new (disc,
								g_strdup (uri),
								BRASERO_FILTER_RECURSIVE_SYM);
		}
	
		if (result->result != GNOME_VFS_OK
		||  !brasero_data_disc_is_readable (info)) {
			brasero_data_disc_remove_uri_from_tree (disc, uri, TRUE);
			brasero_data_disc_add_rescan (disc, file);
			brasero_data_disc_unreadable_new (disc,
								g_strdup (uri),
								BRASERO_FILTER_UNREADABLE);
			continue;
		}
		
		/* it's a simple file. Make a file structure and insert
		 * it in files hash and finally exclude it from its parent */
		sectors = GET_SIZE_IN_SECTORS (info->size);
		brasero_data_disc_file_new (disc,
					    uri,
					    sectors);
		brasero_data_disc_graft_new (disc,
					     uri,
					     callback_data->newpath);
	
		graft = brasero_data_disc_graft_get (disc, callback_data->oldpath);
		brasero_data_disc_exclude_uri (disc, graft, uri);
		g_free (graft);
	}

	/* free user_data */
	return TRUE;
}

static BraseroDiscResult
brasero_data_disc_move_row_simple_file (BraseroDataDisc *disc,
					const char *uri,
					const char *oldpath,
					const char *newpath)
{
	GSList *uris;
	BraseroDiscResult result;
	MoveRowSimpleFileData *callback_data;

	callback_data = g_new0 (MoveRowSimpleFileData, 1);
	callback_data->newpath = g_strdup (newpath);
	callback_data->oldpath = g_strdup (oldpath);

	uris = g_slist_prepend (NULL, (char *) uri);
	result = brasero_data_disc_get_info_async (disc,
						   uris,
						   GNOME_VFS_FILE_INFO_GET_ACCESS_RIGHTS,
						   brasero_data_disc_move_row_simple_file_cb,
						   callback_data,
						   brasero_data_disc_move_row_simple_file_destroy_cb);
	g_slist_free (uris);

	if (result != BRASERO_DISC_OK)
		return result;

	return BRASERO_DISC_OK;
}

static BraseroDiscResult
brasero_data_disc_move_row (BraseroDataDisc *disc,
			    const char *oldpath,
			    const char *newpath)
{
	BraseroDiscResult result;
	BraseroFile *file;
	char *uri;

	/* update all path references */
	brasero_data_disc_joliet_incompat_move (disc, oldpath, newpath);
	brasero_data_disc_move_references (disc, oldpath, newpath);

	/* uri can be NULL if it is a new created directory */
	uri = brasero_data_disc_path_to_uri (disc, oldpath);
	if (!uri) {
		gpointer value = NULL;

		brasero_data_disc_graft_children_move (disc,
						       oldpath,
						       newpath);

		g_hash_table_lookup_extended (disc->priv->paths,
					      oldpath,
					      &value,
					      NULL);
		g_hash_table_remove (disc->priv->paths, oldpath);
		g_free (value);

		g_hash_table_insert (disc->priv->paths,
				     g_strdup (newpath),
				     BRASERO_CREATED_DIR);
		return BRASERO_DISC_OK;
	}

	/* the file has been moved to what would be its original place in the 
	 * file system, in other words, the disc tree hierarchy matches the file
	 * system hierarchy as well as the names which are similar. so we don't 
	 * need a graft point any more and drop the exclusion */
	/* no need to check for a dummy since it is already in the selection */
	result = BRASERO_DISC_OK;
	if (brasero_data_disc_original_parent (disc, uri, newpath))
		result = brasero_data_disc_restore_row (disc,
							uri,
							oldpath,
							newpath);
	else if ((file = g_hash_table_lookup (disc->priv->dirs, uri)))
		brasero_data_disc_move_row_in_dirs_hash (disc,
							 file,
							 oldpath,
							 newpath);
	else if ((file = g_hash_table_lookup (disc->priv->files, uri)))
		brasero_data_disc_move_row_in_files_hash (disc,
							  file,
							  oldpath,
							  newpath);
	else	/* this one could fail */
		result = brasero_data_disc_move_row_simple_file (disc,
								 uri,
								 oldpath,
								 newpath);

	if (result == BRASERO_DISC_OK)
		brasero_data_disc_selection_changed (disc, TRUE);

	g_free (uri);
	return result;
}

/************************************** DND ************************************/
static GtkTreePath *
brasero_data_disc_get_dest_path (BraseroDataDisc *disc,
				 gint x,
				 gint y)
{
	GtkTreeViewDropPosition pos = 0;
	GtkTreePath *realpath = NULL;
	GtkTreePath *path = NULL;
	GtkTreeModel *sort;

	sort = disc->priv->sort;

	/* while the treeview is still under the information pane, it is not 
	 * realized yet and the following function will fail */
	if (GTK_WIDGET_DRAWABLE (disc->priv->tree))
		gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW (disc->priv->tree),
						   x,
						   y,
						   &path,
						   &pos);

	if (path) {
		gboolean isdir;

		if (pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER
		||  pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE) {
			GtkTreeIter iter;

			/* the parent is the row we're dropping into 
			 * we make sure that the parent is a directory
			 * otherwise put it before or after */
			gtk_tree_model_get_iter (sort, &iter, path);
			gtk_tree_model_get (sort, &iter,
					    ISDIR_COL,
					    &isdir, -1);
		}
		else
			isdir = FALSE;

		if (!isdir) {
			if (pos == GTK_TREE_VIEW_DROP_AFTER
			||  pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER)
				gtk_tree_path_next (path);
		}
		else {
			GtkTreeIter parent;

			gtk_tree_model_get_iter (sort, &parent, path);
			pos = gtk_tree_model_iter_n_children (sort, &parent);
			gtk_tree_path_append_index (path, pos);
		}
	}
	else
		path = gtk_tree_path_new_from_indices (gtk_tree_model_iter_n_children (sort, NULL), -1);

	realpath = gtk_tree_model_sort_convert_path_to_child_path (GTK_TREE_MODEL_SORT (disc->priv->sort),
								   path);

	/* realpath can be NULL if the row has been dropped into an empty directory */
	if(!realpath) {
		GtkTreePath *path_parent;

		path_parent = gtk_tree_path_copy(path);
		gtk_tree_path_up(path_parent);

		if(gtk_tree_path_get_depth(path_parent)) {
			GtkTreeIter iter;

			realpath = gtk_tree_model_sort_convert_path_to_child_path(GTK_TREE_MODEL_SORT(sort),
										path_parent);
			gtk_tree_model_get_iter(sort, &iter, path_parent);
			gtk_tree_path_append_index(realpath, gtk_tree_model_iter_n_children(sort, &iter));
		}
		else
			realpath = gtk_tree_path_new_from_indices(gtk_tree_model_iter_n_children(sort, NULL),
								-1);

		gtk_tree_path_free (path_parent);
	}
	gtk_tree_path_free (path);

	return realpath;
}

static char*
brasero_data_disc_new_disc_path (BraseroDataDisc *disc,
				 const char *display,
				 GtkTreePath *dest)
{
	GtkTreePath *parent;
	char *newparentpath;
	char *newpath;

	parent = gtk_tree_path_copy (dest);
	gtk_tree_path_up (parent);
	brasero_data_disc_tree_path_to_disc_path (disc,
						  parent,
						  &newparentpath);
	gtk_tree_path_free (parent);

	newpath = g_build_path (G_DIR_SEPARATOR_S,
				newparentpath,
				display,
				NULL);
	g_free (newparentpath);

	return newpath;
}

static gboolean
brasero_data_disc_native_data_received (BraseroDataDisc *disc,
					GtkSelectionData *selection_data,
					gint x,
					gint y)
{
	GtkTreeRowReference *destref = NULL;
	GtkTreeRowReference *srcref = NULL;
	BraseroDiscResult result;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreePath *dest;
	GtkTreePath *src;
	GtkTreeIter row;
	char *oldpath;
	char *newpath;
	char *name;

	model = disc->priv->model;

	/* check again if move is possible */
	dest = brasero_data_disc_get_dest_path (disc, x, y);
	if (gtk_tree_drag_dest_row_drop_possible (GTK_TREE_DRAG_DEST (model),
						  dest,
						  selection_data) == FALSE) {
		gtk_tree_path_free (dest);
		return FALSE;
	}

	gtk_tree_get_row_drag_data (selection_data,
				    &model,
				    &src);

	/* move it in the backend */
	gtk_tree_model_get_iter (model, &row, src);
	gtk_tree_model_get (model, &row, NAME_COL, &name, -1);
	newpath = brasero_data_disc_new_disc_path (disc, name, dest);
	g_free (name);

	brasero_data_disc_tree_path_to_disc_path (disc, src, &oldpath);
	result = brasero_data_disc_move_row (disc,
					     oldpath,
					     newpath);

	if (result != BRASERO_DISC_OK)
		goto end;

	/* keep some necessary references for later */
	srcref = gtk_tree_row_reference_new (model, src);
	if (gtk_tree_path_get_depth (dest) > 1) {
		int nb_children;

		/* we can only put a reference on the parent
		 * since the child doesn't exist yet */
		gtk_tree_path_up (dest);
		destref = gtk_tree_row_reference_new (model, dest);

		gtk_tree_model_get_iter (model, &row, dest);
		nb_children = gtk_tree_model_iter_n_children (model, &row);
		gtk_tree_path_append_index (dest, nb_children);
	}
	else
		destref = NULL;

	/* move it */
	if (!gtk_tree_drag_dest_drag_data_received (GTK_TREE_DRAG_DEST (model),
						    dest,
						    selection_data)) {
		brasero_data_disc_move_row (disc,
					    newpath,
					    oldpath);
		goto end;
	}

	/* update parent directories */
	path = gtk_tree_row_reference_get_path (srcref);
	gtk_tree_drag_source_drag_data_delete (GTK_TREE_DRAG_SOURCE (model),
					       path);
	brasero_data_disc_tree_update_parent (disc, path);
	gtk_tree_path_free (path);

	if (destref
	&& (path = gtk_tree_row_reference_get_path (destref))) {
		brasero_data_disc_tree_update_directory (disc, path);
		gtk_tree_path_free (path);
	}

end:

	if (srcref)
		gtk_tree_row_reference_free (srcref);
	if (destref)
		gtk_tree_row_reference_free (destref);

	gtk_tree_path_free (dest);
	gtk_tree_path_free (src);
	g_free (oldpath);
	g_free (newpath);

	return TRUE;
}

static GdkDragAction 
brasero_data_disc_drag_data_received_dragging (BraseroDataDisc *disc,
					       GtkSelectionData *selection_data)
{
	char *name;
	GtkTreeIter iter;
	GtkTreePath *dest;
	GtkTreeModel *sort;
	GtkTreeModel *model;
	GtkTreePath *sort_dest;
	GtkTreePath *src_parent;
	GtkTreePath *dest_parent;
	BraseroDiscResult result;
	GtkTreeViewDropPosition pos;
	GdkDragAction action = GDK_ACTION_MOVE;

	if (!disc->priv->drag_source)
		return GDK_ACTION_DEFAULT;

	model = disc->priv->model;
	sort = disc->priv->sort;

	src_parent = NULL;
	dest_parent = NULL;

	gtk_tree_view_get_drag_dest_row (GTK_TREE_VIEW (disc->priv->tree),
					 &sort_dest,
					 &pos);

	if (!sort_dest) {
		pos = GTK_TREE_VIEW_DROP_AFTER;
		dest = gtk_tree_path_new_from_indices (gtk_tree_model_iter_n_children (model, NULL) - 1, -1);
	}
	else {
		dest = gtk_tree_model_sort_convert_path_to_child_path (GTK_TREE_MODEL_SORT (sort),
								       sort_dest);
		gtk_tree_path_free (sort_dest);
	}

	/* if we drop into make sure it is a directory */
	if (gtk_tree_model_get_iter (model, &iter, dest)
	&& (pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER
	||  pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE)) {
		gboolean isdir;
		int explored;

		gtk_tree_model_get (model, &iter,
				    ISDIR_COL, &isdir,
				    ROW_STATUS_COL, &explored, -1);

		if (!isdir) {
			if (GTK_TREE_VIEW_DROP_INTO_OR_AFTER)
				pos = GTK_TREE_VIEW_DROP_AFTER;
			else if (GTK_TREE_VIEW_DROP_INTO_OR_BEFORE)
				pos = GTK_TREE_VIEW_DROP_BEFORE;
		}
		else if (explored < ROW_EXPLORED) {
			/* we prevent any addition to a row not yet explored
			 * as we could have two files with the same name */
			action = GDK_ACTION_DEFAULT;
			goto end;
		}
	}

	if (pos == GTK_TREE_VIEW_DROP_AFTER
	||  pos == GTK_TREE_VIEW_DROP_BEFORE) {
		dest_parent = gtk_tree_path_copy (dest);
		gtk_tree_path_up (dest_parent);
	}
	else
		dest_parent = gtk_tree_path_copy (dest);

	src_parent = gtk_tree_path_copy (disc->priv->drag_source);
	gtk_tree_path_up (src_parent);

	/* check that we are actually changing the directory */
	if(!gtk_tree_path_get_depth (dest_parent)
	&& !gtk_tree_path_get_depth (src_parent)) {
		action = GDK_ACTION_DEFAULT;
		goto end;
	}

	if (gtk_tree_path_get_depth (dest_parent)
	&&  gtk_tree_path_get_depth (src_parent)
	&& !gtk_tree_path_compare (src_parent, dest_parent)) {
		action = GDK_ACTION_DEFAULT;
		goto end;
	}

	/* make sure that a row doesn't exist with the same name
	 * and it is joliet compatible */
	gtk_tree_model_get_iter (model, &iter, disc->priv->drag_source);
	gtk_tree_model_get (model, &iter,
			    NAME_COL, &name,
			    -1);

	result = brasero_data_disc_tree_check_name_validity (disc,
							     name,
							     dest_parent,
							     FALSE);
	g_free (name);

	if (result != BRASERO_DISC_OK) {
		action = GDK_ACTION_DEFAULT;
		goto end;
	}

	if (!gtk_tree_drag_dest_row_drop_possible (GTK_TREE_DRAG_DEST (model),
						   dest,
						   selection_data))
		action = GDK_ACTION_DEFAULT;

end:
	gtk_tree_path_free (dest);
	gtk_tree_path_free (src_parent);
	gtk_tree_path_free (dest_parent);

	return action;
}

static void
brasero_data_disc_drag_data_received_cb (GtkTreeView *tree,
					 GdkDragContext *drag_context,
					 gint x,
					 gint y,
					 GtkSelectionData *selection_data,
					 guint info,
					 guint time,
					 BraseroDataDisc *disc)
{
	gboolean result = FALSE;

	if (disc->priv->drag_status == STATUS_DRAGGING) {
		GdkDragAction action;

		if (!disc->priv->is_loading)
			action = brasero_data_disc_drag_data_received_dragging (disc,
										selection_data);
		else
			action = GDK_ACTION_DEFAULT;

		gdk_drag_status (drag_context, action, time);
		if(action == GDK_ACTION_DEFAULT)
			gtk_tree_view_set_drag_dest_row (tree,
							 NULL,
							 GTK_TREE_VIEW_DROP_BEFORE);

		g_signal_stop_emission_by_name (tree, "drag-data-received");
		return;
	}

	if (disc->priv->scroll_timeout) {
		g_source_remove (disc->priv->scroll_timeout);
		disc->priv->scroll_timeout = 0;
	}

	if (disc->priv->expand_timeout) {
		g_source_remove (disc->priv->expand_timeout);
		disc->priv->expand_timeout = 0;
	}

	if (selection_data->length <= 0
	||  selection_data->format != 8) {
		gtk_drag_finish (drag_context, FALSE, FALSE, time);
		disc->priv->drag_status = STATUS_NO_DRAG;
		g_signal_stop_emission_by_name (tree, "drag-data-received");
		return;
	}

	/* we get URIS */
	if (info == TARGET_URIS_LIST) {
		gboolean func_results;
		char **uri, **uris;
		GtkTreePath *dest;

		uris = gtk_selection_data_get_uris (selection_data);
		dest = brasero_data_disc_get_dest_path (disc, x, y);
		gtk_tree_path_up (dest);

		for (uri = uris; *uri != NULL; uri++) {
			char *unescaped_uri;

			unescaped_uri = gnome_vfs_unescape_string_for_display (*uri);
			func_results = brasero_data_disc_add_uri_real (disc,
								       unescaped_uri,
								       dest);
			result = (result ? TRUE : func_results);
			g_free (unescaped_uri);
		}

		gtk_tree_path_free (dest);
		g_strfreev (uris);
	} 
	else if (info == TREE_MODEL_ROW)
		result = brasero_data_disc_native_data_received (disc,
								 selection_data,
								 x,
								 y);

	gtk_drag_finish (drag_context,
			 result,
			 (drag_context->action == GDK_ACTION_MOVE),
			 time);

	g_signal_stop_emission_by_name (tree, "drag-data-received");
	disc->priv->drag_status = STATUS_NO_DRAG;
}

static void
brasero_data_disc_drag_begin_cb (GtkTreeView *tree,
				 GdkDragContext *drag_context,
				 BraseroDataDisc *disc)
{
	GtkTreePath *sort_src;
	GtkTreeModel *model;
	GtkTreeModel *sort;
	GdkPixmap *row_pix;
	GtkTreePath *src;
	GtkTreeIter iter;
	gint cell_y;
	int status;

	disc->priv->drag_status = STATUS_DRAGGING;
	g_signal_stop_emission_by_name (tree, "drag-begin");

	/* Put the icon */
	gtk_tree_view_get_path_at_pos (tree,
				       disc->priv->press_start_x,
				       disc->priv->press_start_y,
				       &sort_src,
				       NULL,
				       NULL,
				       &cell_y);
	
	g_return_if_fail (sort_src != NULL);
	sort = disc->priv->sort;
	src = gtk_tree_model_sort_convert_path_to_child_path (GTK_TREE_MODEL_SORT (sort),
							      sort_src);

	model = disc->priv->model;
	gtk_tree_model_get_iter (model, &iter, src);
	gtk_tree_model_get (model, &iter, ROW_STATUS_COL, &status, -1);
	if (status == ROW_BOGUS) {
		disc->priv->drag_source = NULL;
		gtk_tree_path_free (sort_src);
		gtk_tree_path_free (src);
		return;
	}

	disc->priv->drag_source = src;

	row_pix = gtk_tree_view_create_row_drag_icon (tree, sort_src);
	gtk_drag_set_icon_pixmap (drag_context,
				  gdk_drawable_get_colormap (row_pix),
				  row_pix,
				  NULL,
				  /* the + 1 is for the black border in the icon */
				  disc->priv->press_start_x + 1,
				  cell_y + 1);
	
	gtk_tree_path_free(sort_src);
	g_object_unref (row_pix);
}

static gboolean
brasero_data_disc_drag_drop_cb (GtkTreeView *tree,
				GdkDragContext *drag_context,
				gint x,
				gint y,
				guint time,
				BraseroDataDisc *disc)
{
	GdkAtom target = GDK_NONE;

	if (disc->priv->scroll_timeout) {
		g_source_remove (disc->priv->scroll_timeout);
		disc->priv->scroll_timeout = 0;
	}

	if (disc->priv->expand_timeout) {
		g_source_remove (disc->priv->expand_timeout);
		disc->priv->expand_timeout = 0;
	}

	g_signal_stop_emission_by_name (tree, "drag-drop");
	disc->priv->drag_status = STATUS_DRAG_DROP;

	target = gtk_drag_dest_find_target (GTK_WIDGET (tree),
					    drag_context,
					    gtk_drag_dest_get_target_list (GTK_WIDGET (tree)));

	if (target != GDK_NONE) {
		gtk_drag_get_data (GTK_WIDGET (tree),
				   drag_context,
				   target,
				   time);
		return TRUE;
	}

	return FALSE;
}

/* in the following functions there are quick and dirty cut'n pastes from gtktreeview.c shame on me */
static GtkTreeViewDropPosition
brasero_data_disc_set_dest_row (BraseroDataDisc *disc,
				gint x,
				gint y)
{
	GtkTreeViewDropPosition pos;
	GtkTreePath *old_dest = NULL;
	GtkTreePath *sort_dest = NULL;

	/* while the treeview is still under the information pane, it is not 
	 * realized yet and the following function will fail. Here we shouldn't
	 * need the test since it's called when treeview is a drag source and
	 * therefore already mapped and realized. */
	if (GTK_WIDGET_DRAWABLE (disc->priv->tree))
		gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW (disc->priv->tree),
						   x,
						   y,
						   &sort_dest,
						   &pos);

	if (!sort_dest) {
		gint n_children;
		GtkTreeModel *sort;

		sort = disc->priv->sort;
		n_children = gtk_tree_model_iter_n_children (sort, NULL);
		if (n_children) {
			pos = GTK_TREE_VIEW_DROP_AFTER;
			sort_dest = gtk_tree_path_new_from_indices (n_children - 1, -1);
		}
		else {
			pos = GTK_TREE_VIEW_DROP_BEFORE;
			sort_dest = gtk_tree_path_new_from_indices (0, -1);
		}
	}

	gtk_tree_view_get_drag_dest_row (GTK_TREE_VIEW (disc->priv->tree),
					 &old_dest,
					 NULL);

	if (old_dest
	&&  sort_dest
	&&  (gtk_tree_path_compare (old_dest, sort_dest) != 0
	||  !(pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER 
	||    pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE))
	&&  disc->priv->expand_timeout) {
		g_source_remove (disc->priv->expand_timeout);
		disc->priv->expand_timeout = 0;
	}

	gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW(disc->priv->tree),
					 sort_dest,
					 pos);
	gtk_tree_path_free (sort_dest);

	if (old_dest)
		gtk_tree_path_free (old_dest);
	return pos;
}

static gboolean
brasero_data_disc_scroll_timeout_cb (BraseroDataDisc *data)
{
	int y;
	double value;
	int scroll_area;
	GdkWindow *window;
	GdkRectangle area;
	GtkAdjustment *adjustment;

	window = gtk_tree_view_get_bin_window (GTK_TREE_VIEW (data->priv->tree));
	gdk_window_get_pointer (window, NULL, &y, NULL);
	gtk_tree_view_get_visible_rect (GTK_TREE_VIEW (data->priv->tree), &area);

	/* height */
	scroll_area = area.height / 6;
	value = y - scroll_area;
	if (value >= 0) {
		value = y - (area.height - scroll_area);
		if (value <= 0)
			return TRUE;
	}

	g_object_get (data->priv->tree, "vadjustment", &adjustment, NULL);
	value = CLAMP (adjustment->value + value,
		       0.0,
		       adjustment->upper - adjustment->page_size);
	gtk_adjustment_set_value (adjustment, value);

	return TRUE;
}

static gboolean
brasero_data_disc_expand_timeout_cb (BraseroDataDisc *disc)
{
	gboolean result;
	GtkTreePath *dest;
	GtkTreeViewDropPosition pos;

	gtk_tree_view_get_drag_dest_row (GTK_TREE_VIEW (disc->priv->tree),
					 &dest,
					 &pos);

	/* we don't need to check if it's a directory because:
	   - a file wouldn't have children anyway
	   - we check while motion if it's a directory and if not remove the INTO from pos */
	if (dest
	&&  (pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER || pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE)) {
		gtk_tree_view_expand_row (GTK_TREE_VIEW (disc->priv->tree), dest, FALSE);
		disc->priv->expand_timeout = 0;
	
		gtk_tree_path_free (dest);
	}
	else {
		if (dest)
			gtk_tree_path_free (dest);
	
		result = TRUE;
	}

	return result;
}

static gboolean
brasero_data_disc_drag_motion_cb (GtkWidget *tree,
				  GdkDragContext *drag_context,
				  gint x,
				  gint y,
				  guint time,
				  BraseroDataDisc *disc)
{
	GdkAtom target;

	target = gtk_drag_dest_find_target (tree,
					    drag_context,
					    gtk_drag_dest_get_target_list(tree));

	if (disc->priv->is_loading
	|| (disc->priv->reject_files
	&&  target != gdk_atom_intern ("GTK_TREE_MODEL_ROW", FALSE))) {
		g_signal_stop_emission_by_name (tree, "drag-motion");

		gdk_drag_status (drag_context, GDK_ACTION_DEFAULT, time);
		gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (tree),
						 NULL,
						 GTK_TREE_VIEW_DROP_BEFORE);
		return FALSE;
	}

	if (target == gdk_atom_intern ("GTK_TREE_MODEL_ROW", FALSE)) {
		GtkTreeViewDropPosition pos;

		pos = brasero_data_disc_set_dest_row (disc, x, y);

		/* since we mess with the model we have to re-implement the following two */
		if (!disc->priv->expand_timeout
		&&  (pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER || pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE)) {
			disc->priv->expand_timeout = g_timeout_add (500,
								    (GSourceFunc) brasero_data_disc_expand_timeout_cb,
								    disc);
		}
		else if (disc->priv->scroll_timeout == 0) {
			disc->priv->scroll_timeout = g_timeout_add (150,
								    (GSourceFunc) brasero_data_disc_scroll_timeout_cb,
								    disc);
		}
		gtk_drag_get_data (tree,
				   drag_context,
				   target,
				   time);
		g_signal_stop_emission_by_name (tree, "drag-motion");
		return TRUE;
	}

	return FALSE;
}

static void
brasero_data_disc_drag_get_cb (GtkWidget *tree,
                               GdkDragContext *context,
                               GtkSelectionData *selection_data,
                               guint info,
                               guint time,
			       BraseroDataDisc *disc)
{
	g_signal_stop_emission_by_name (tree, "drag-data-get");

	/* that could go into begin since we only accept GTK_TREE_MODEL_ROW as our source target */
	if (selection_data->target == gdk_atom_intern ("GTK_TREE_MODEL_ROW", FALSE)
	&&  disc->priv->drag_source) {
		GtkTreeModel *model;

		model = disc->priv->model;
		gtk_tree_set_row_drag_data (selection_data,
					    model,
					    disc->priv->drag_source);
	}
}

static void
brasero_data_disc_drag_end_cb (GtkWidget *tree,
			       GdkDragContext *drag_context,
			       BraseroDataDisc *disc)
{
	g_signal_stop_emission_by_name (tree, "drag-end");

	gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (tree),
					 NULL,
					 GTK_TREE_VIEW_DROP_BEFORE);

	if (disc->priv->scroll_timeout) {
		g_source_remove (disc->priv->scroll_timeout);
		disc->priv->scroll_timeout = 0;
	}

	if (disc->priv->expand_timeout) {
		g_source_remove (disc->priv->expand_timeout);
		disc->priv->expand_timeout = 0;
	}

	gtk_tree_path_free(disc->priv->drag_source);
	disc->priv->drag_source = NULL;
}

void
brasero_data_disc_drag_leave_cb (GtkWidget *tree,
				 GdkDragContext *drag_context,
				 guint time,
				 BraseroDataDisc *disc)
{
	if (disc->priv->scroll_timeout) {
		g_signal_stop_emission_by_name (tree, "drag-leave");

		g_source_remove (disc->priv->scroll_timeout);
		disc->priv->scroll_timeout = 0;
	}
	if (disc->priv->expand_timeout) {
		g_signal_stop_emission_by_name (tree, "drag-leave");

		g_source_remove (disc->priv->expand_timeout);
		disc->priv->expand_timeout = 0;
	}
}

/**************************** MENUS ********************************************/
static void
brasero_data_disc_open_file (BraseroDataDisc *disc, GList *list)
{
	char *uri;
	char *path;
	int status;
	GList *item;
	GSList *uris;
	GtkTreeIter iter;
	GtkTreeModel *sort;
	GtkTreePath *realpath;
	GtkTreePath *treepath;

	sort = disc->priv->sort;

	uris = NULL;
	for (item = list; item; item = item->next) {
		treepath = item->data;

		if (!treepath)
			continue;

		if (!gtk_tree_model_get_iter (sort, &iter, treepath)) {
			gtk_tree_path_free (treepath);
			continue;
		}

		gtk_tree_model_get (sort, &iter, ROW_STATUS_COL, &status, -1);
		if (status == ROW_BOGUS) {
			gtk_tree_path_free (treepath);
			continue;
		}

		realpath = gtk_tree_model_sort_convert_path_to_child_path (GTK_TREE_MODEL_SORT (sort),
									   treepath);

		brasero_data_disc_tree_path_to_disc_path (disc, realpath, &path);
		gtk_tree_path_free (realpath);

		uri = brasero_data_disc_path_to_uri (disc, path);
		g_free (path);
		if (uri)
			uris = g_slist_prepend (uris, uri);

	}

	if (!uris)
		return;

	brasero_utils_launch_app (GTK_WIDGET (disc), uris);
	g_slist_foreach (uris, (GFunc) g_free, NULL);
	g_slist_free (uris);
}

static void
brasero_data_disc_open_activated_cb (GtkAction *action,
				     BraseroDataDisc *disc)
{
	GList *list;
	GtkTreeModel *sort;
	GtkTreeSelection *selection;

	sort = disc->priv->sort;
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (disc->priv->tree));
	list = gtk_tree_selection_get_selected_rows (selection, &sort);
	brasero_data_disc_open_file (disc, list);

	g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (list);
}

static void
brasero_data_disc_rename_activated (BraseroDataDisc *disc)
{
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkTreePath *treepath;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GList *list;
	int status;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (disc->priv->tree));
	model = disc->priv->sort;

	list = gtk_tree_selection_get_selected_rows (selection, &model);
	for (; list; list = g_list_remove (list, treepath)) {
		treepath = list->data;

		gtk_tree_model_get_iter (model, &iter, treepath);
		gtk_tree_model_get (model, &iter,
				    ROW_STATUS_COL, &status,
				    -1);

		if (status == ROW_BOGUS) {
			gtk_tree_path_free (treepath);
			continue;
		}

		column = gtk_tree_view_get_column (GTK_TREE_VIEW (disc->priv->tree),
						   0);
		gtk_tree_view_set_cursor_on_cell (GTK_TREE_VIEW (disc->priv->tree),
						  treepath,
						  column,
						  NULL,
						  TRUE);
		gtk_widget_grab_focus (disc->priv->tree);

		gtk_tree_path_free (treepath);
	}
}

static void
brasero_data_disc_rename_activated_cb (GtkAction *action,
				       BraseroDataDisc *disc)
{
	brasero_data_disc_rename_activated (disc);
}

static void
brasero_data_disc_delete_activated_cb (GtkAction *action,
				       BraseroDataDisc *disc)
{
	brasero_data_disc_delete_selected (BRASERO_DISC (disc));
}

struct _BraseroClipData {
	BraseroDataDisc *disc;
	GtkTreeRowReference *reference;
};
typedef struct _BraseroClipData BraseroClipData;

static void
brasero_data_disc_clipboard_text_cb (GtkClipboard *clipboard,
				     const char *text,
				     BraseroClipData *data)
{
	GtkTreePath *treepath = NULL;
	GtkTreeModel *model = NULL;
	GtkTreePath *parent = NULL;
	GtkTreeIter row;
	char **array;
	char **item;
	char *uri;

	model = data->disc->priv->sort;
	if (data->reference) {
		parent = gtk_tree_row_reference_get_path (data->reference);
		gtk_tree_model_get_iter (model, &row, parent);
	}

	array = g_strsplit_set (text, "\n\r", 0);
	item = array;
	while (*item) {
		if (**item != '\0') {
			char *escaped_uri;

			if (parent) {
				treepath = gtk_tree_path_copy (parent);
				gtk_tree_path_append_index (treepath,
							    gtk_tree_model_iter_n_children
							    (model, &row));
			}

			escaped_uri = gnome_vfs_make_uri_canonical (*item);
			uri = gnome_vfs_unescape_string_for_display (escaped_uri);
			g_free (escaped_uri);
			brasero_data_disc_add_uri_real (data->disc,
							uri,
							treepath);
			g_free (uri);
			if (treepath)
				gtk_tree_path_free (treepath);
		}

		item++;
	}

	if (parent)
		gtk_tree_path_free (parent);
	g_strfreev (array);

	if (data->reference)
		gtk_tree_row_reference_free (data->reference);

	g_free (data);
}

static void
brasero_data_disc_clipboard_targets_cb (GtkClipboard *clipboard,
					 GdkAtom *atoms,
					 gint n_atoms,
					 BraseroClipData *data)
{
	GdkAtom *iter;
	char *target;

	iter = atoms;
	while (n_atoms) {
		target = gdk_atom_name (*iter);

		if (!strcmp (target, "x-special/gnome-copied-files")
		    || !strcmp (target, "UTF8_STRING")) {
			gtk_clipboard_request_text (clipboard,
						    (GtkClipboardTextReceivedFunc)
						    brasero_data_disc_clipboard_text_cb,
						    data);
			g_free (target);
			return;
		}

		g_free (target);
		iter++;
		n_atoms--;
	}

	if (data->reference)
		gtk_tree_row_reference_free (data->reference);
	g_free (data);
}

static void
brasero_data_disc_paste_activated_cb (GtkAction *action,
				      BraseroDataDisc *disc)
{
	BraseroClipData *data;
	GtkTreeSelection *selection;
	GtkClipboard *clipboard;
	GtkTreeModel *model;
	GList *list;

	data = g_new0 (BraseroClipData, 1);
	data->disc = disc;

	/* we must keep a reference to the row selected */
	model = disc->priv->sort;
	selection =  gtk_tree_view_get_selection (GTK_TREE_VIEW (disc->priv->tree));
	list = gtk_tree_selection_get_selected_rows (selection, &model);
	if (list) {
		GtkTreePath *treepath;
		GtkTreeIter row;
		gboolean isdir;

		treepath = list->data;
		g_list_free (list);

		/* see if it a dir or a file */
		gtk_tree_model_get_iter (model, &row, treepath);
		gtk_tree_model_get (model, &row,
				    ISDIR_COL, &isdir,
				    -1);

		if (isdir 
		|| (gtk_tree_path_up (treepath)
		&&  gtk_tree_path_get_depth (treepath) > 0))
			data->reference = gtk_tree_row_reference_new (model, treepath);

		gtk_tree_path_free (treepath);
	}

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_request_targets (clipboard,
				       (GtkClipboardTargetsReceivedFunc)
				       brasero_data_disc_clipboard_targets_cb,
				       data);
}

static gboolean
brasero_data_disc_button_pressed_cb (GtkTreeView *tree,
				     GdkEventButton *event,
				     BraseroDataDisc *disc)
{
	GtkTreePath *treepath = NULL;
	gboolean result;

	GtkWidgetClass *widget_class;

	/* we call the default handler for the treeview before everything else
	 * so it can update itself (paticularly its selection) before we use it
	 * NOTE: since the event has been treated here we need to return TRUE to
	 * avoid having the treeview treating this event a second time */
	widget_class = GTK_WIDGET_GET_CLASS (tree);
	widget_class->button_press_event (GTK_WIDGET (tree), event);

	if (disc->priv->is_loading)
		return TRUE;

	if (GTK_WIDGET_REALIZED (disc->priv->tree))
		result = gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (disc->priv->tree),
							event->x,
							event->y,
							&treepath,
							NULL,
							NULL,
							NULL);
	else
		result = FALSE;

	if ((event->state & (GDK_CONTROL_MASK|GDK_SHIFT_MASK)) == 0) {
		if (disc->priv->selected_path)
			gtk_tree_path_free (disc->priv->selected_path);

		disc->priv->selected_path = NULL;

		if (result && treepath) {
			GtkTreeModel *model;
			GtkTreeIter iter;
			gint status;

			/* we need to make sure that this is not a bogus row */
			model = disc->priv->sort;
			gtk_tree_model_get_iter (model, &iter, treepath);
			gtk_tree_model_get (model, &iter,
					    ROW_STATUS_COL, &status,
					    -1);

			if (status != ROW_BOGUS) {
				if (event->state & GDK_CONTROL_MASK)
					disc->priv->selected_path = treepath;
				else if ((event->state & GDK_SHIFT_MASK) == 0)
					disc->priv->selected_path = treepath;
				else {
					gtk_tree_path_free (treepath);
					treepath = NULL;
				}
			}
			else {
				gtk_tree_path_free (treepath);
				treepath = NULL;
			}
		}

		brasero_disc_selection_changed (BRASERO_DISC (disc));
	}

	if (event->button == 1) {
		disc->priv->press_start_x = event->x;
		disc->priv->press_start_y = event->y;

		if (event->type == GDK_2BUTTON_PRESS) {
			GList *list;

			list = g_list_prepend (NULL, treepath);
			brasero_data_disc_open_file (disc, list);
			g_list_free (list);
		}

		return TRUE;
	}
	else if (event->button == 3) {
		GtkTreeSelection *selection;

		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (disc->priv->tree));
		brasero_utils_show_menu (gtk_tree_selection_count_selected_rows (selection),
					 disc->priv->manager,
					 event);
		return TRUE;
	}

	return TRUE;
}

static gboolean
brasero_data_disc_button_released_cb (GtkTreeView *tree,
				      GdkEventButton *event,
				      BraseroDataDisc *disc)
{
	if (disc->priv->is_loading)
		return FALSE;



	return FALSE;
}

static gboolean
brasero_data_disc_key_released_cb (GtkTreeView *tree,
				   GdkEventKey *event,
				   BraseroDataDisc *disc)
{
	if (disc->priv->is_loading)
		return FALSE;

	if (disc->priv->editing)
		return FALSE;

	if (event->keyval == GDK_KP_Delete || event->keyval == GDK_Delete) {
		brasero_data_disc_delete_selected (BRASERO_DISC (disc));
	}
	else if (event->keyval == GDK_F2)
		brasero_data_disc_rename_activated (disc);

	return FALSE;
}

/*********************************** CELL EDITING ******************************/
static void
brasero_data_disc_name_editing_started_cb (GtkCellRenderer *renderer,
					   GtkCellEditable *editable,
					   gchar *path,
					   BraseroDataDisc *disc)
{
	disc->priv->editing = 1;
}

static void
brasero_data_disc_name_editing_canceled_cb (GtkCellRenderer *renderer,
					    BraseroDataDisc *disc)
{
	disc->priv->editing = 0;
}

static void
brasero_data_disc_name_edited_cb (GtkCellRendererText *cellrenderertext,
				  gchar *path_string,
				  gchar *text,
				  BraseroDataDisc *disc)
{
	BraseroDiscResult res;
	GtkTreePath *realpath;
	GtkTreeModel *model;
	GtkTreeModel *sort;
	GtkTreePath *path;
	GtkTreeIter row;
	char *oldpath;
	char *newpath;
	char *parent;
	char *name;

	disc->priv->editing = 0;

	sort = disc->priv->sort;
	path = gtk_tree_path_new_from_string (path_string);
	realpath = gtk_tree_model_sort_convert_path_to_child_path (GTK_TREE_MODEL_SORT(sort),
								   path);
	gtk_tree_path_free (path);

	model = disc->priv->model;
	gtk_tree_model_get_iter (model, &row, realpath);
	gtk_tree_model_get (model, &row, NAME_COL, &name, -1);

	/* make sure it actually changed */
	if (!strcmp (name, text))
		goto end;

	/* make sure there isn't the same name in the directory and it is joliet
	 * compatible */
	gtk_tree_path_up (realpath);
	res = brasero_data_disc_tree_check_name_validity (disc,
							  text,
							  realpath,
							  TRUE);
	if (res != BRASERO_DISC_OK)
		goto end;

	brasero_data_disc_tree_path_to_disc_path (disc, realpath, &parent);

	oldpath = g_build_path (G_DIR_SEPARATOR_S, parent, name, NULL);
	newpath = g_build_path (G_DIR_SEPARATOR_S, parent, text, NULL);
	brasero_data_disc_move_row (disc, oldpath, newpath);
	gtk_tree_store_set (GTK_TREE_STORE (model), &row,
			    NAME_COL, text, -1);

	g_free (parent);
	g_free (oldpath);
	g_free (newpath);

	brasero_data_disc_selection_changed (disc, TRUE);

end:
	g_free (name);
	gtk_tree_path_free (realpath);
}

/*******************************            ************************************/
static char *
brasero_data_disc_get_selected_uri (BraseroDisc *disc)
{
	gchar *uri;
	gchar *path;
	gchar *escaped_uri;
	GtkTreePath *realpath;
	BraseroDataDisc *data;

	data = BRASERO_DATA_DISC (disc);

	if (!data->priv->selected_path)
		return NULL;

	realpath = gtk_tree_model_sort_convert_path_to_child_path (GTK_TREE_MODEL_SORT (data->priv->sort),
								   data->priv->selected_path);
	brasero_data_disc_tree_path_to_disc_path (data, realpath, &path);
	gtk_tree_path_free (realpath);

	uri = brasero_data_disc_path_to_uri (data, path);
	g_free (path);

	escaped_uri = gnome_vfs_escape_host_and_path_string (uri);
	g_free (uri);

	return escaped_uri;
}

/******************************* monitoring ************************************/
#ifdef BUILD_INOTIFY

static void
_uri_find_children_cb (gchar *uri,
		       gpointer data,
		       MakeChildrenListData *callback_data)
{
	if (!strncmp (uri, callback_data->parent, callback_data->len)
	&&   uri [callback_data->len] == G_DIR_SEPARATOR)
		callback_data->children = g_slist_prepend (callback_data->children, uri);
}

static GSList *
brasero_data_disc_uri_find_children_uris (BraseroDataDisc *disc,
					  const gchar *uri)
{
	MakeChildrenListData callback_data;

	callback_data.len = strlen (uri);
	callback_data.children = NULL;
	callback_data.parent = (gchar *) uri;

	g_hash_table_foreach (disc->priv->dirs,
			      (GHFunc) _uri_find_children_cb,
			      &callback_data);

	g_hash_table_foreach (disc->priv->files,
			      (GHFunc) _uri_find_children_cb,
			      &callback_data);

	if (disc->priv->restored)
		g_hash_table_foreach (disc->priv->restored,
				      (GHFunc) _uri_find_children_cb,
				      &callback_data);

	if (disc->priv->unreadable)
		g_hash_table_foreach (disc->priv->unreadable,
				      (GHFunc) _uri_find_children_cb,
				      &callback_data);

	if (disc->priv->symlinks)
		g_hash_table_foreach (disc->priv->symlinks,
				      (GHFunc) _uri_find_children_cb,
				      &callback_data);

	return callback_data.children;
}

static void
brasero_data_disc_uri_move (BraseroDataDisc *disc,
			    const gchar *new_uri,
			    const gchar *old_uri)
{
	GSList *iter, *grafts;
	BraseroFile *file;
	gchar *key = NULL;

	/* NOTE: we only update the URI not the name in the tree
	 * that's too much work with this crappy implementation */
	if ((file = g_hash_table_lookup (disc->priv->dirs, old_uri))) {
		/* it's a grafted directory change its URI */
		g_hash_table_remove (disc->priv->dirs, old_uri);
		key = file->uri;
		file->uri = g_strdup (new_uri);
		g_hash_table_insert (disc->priv->dirs, file->uri, file);
	}
	else if ((file = g_hash_table_lookup (disc->priv->files, old_uri))) {
		/* it's a file grafted */
		g_hash_table_remove (disc->priv->files, old_uri);
		key = file->uri;
		file->uri = g_strdup (new_uri);
		g_hash_table_insert (disc->priv->files, file->uri, file);
	}
	else if (disc->priv->unreadable
	      &&  g_hash_table_lookup (disc->priv->unreadable, old_uri)) {
		BraseroFilterStatus status;

		/* it's an unreadable file simply remove the old URI and add the new one */
		status = brasero_data_disc_unreadable_free (disc, old_uri);
		brasero_data_disc_unreadable_new (disc, g_strdup (new_uri), status);
		return;
	}
	else if (disc->priv->restored
	      &&  g_hash_table_lookup (disc->priv->restored, old_uri)) {
		BraseroFilterStatus status;

		/* it's a restored file simply remove the old URI and add the new one */
		status = brasero_data_disc_restored_free (disc, old_uri);
		brasero_data_disc_restored_new (disc, new_uri, status);
		return;
	}
	else if (disc->priv->symlinks
	      &&  g_hash_table_lookup (disc->priv->symlinks, old_uri)) {
		gchar *target;

		/* it's a symlink */
		target = g_hash_table_lookup (disc->priv->symlinks, old_uri);
		g_hash_table_insert (disc->priv->symlinks,
				     g_strdup (new_uri),
				     g_strdup (target));
		g_hash_table_remove (disc->priv->symlinks, old_uri);
		return;
	}
	else
		return;

	/* update the paths hash table */
	grafts = g_hash_table_lookup (disc->priv->grafts, old_uri);
	for (iter = grafts; iter; iter = iter->next)
		g_hash_table_replace (disc->priv->paths,
				      iter->data,
				      file->uri);

	/* update the grafts hash table */
	g_hash_table_remove (disc->priv->grafts, old_uri);
	g_hash_table_insert (disc->priv->grafts,
			     file->uri,
			     grafts);

	if (disc->priv->excluded) {
		GSList *excluding;

		/* Update a potential entry in excluded hash table */
		excluding = g_hash_table_lookup (disc->priv->excluded, old_uri);
		if (excluding) {
			if (!file) {
				/* we need to free the old key */
				g_hash_table_lookup_extended (disc->priv->excluded,
							      old_uri,
							      (gpointer) &old_uri,
							      NULL);
				g_hash_table_remove (disc->priv->excluded, old_uri);
				g_hash_table_insert (disc->priv->excluded,
						     g_strdup (new_uri),
						     excluding);
			}
			else {
				g_hash_table_remove (disc->priv->excluded, old_uri);
				g_hash_table_insert (disc->priv->excluded,
						     file->uri,
						     excluding);
			}
		}
	}

	if (key)
		g_free (key);
}

/******************************* check joliet compatilibility async ************/
/* This function is mainly to catch a delete event following a create event */
static void
brasero_data_disc_reference_remove_uri (BraseroDataDisc *disc,
					const gchar *uri)
{
	GSList *paths, *iter;

	/* get all its paths */
	paths = brasero_data_disc_uri_to_paths (disc, uri, TRUE);
	for (iter = paths; iter; iter = iter->next) {
		gchar *path;

		path = iter->data;
		brasero_data_disc_joliet_incompat_free (disc, path);
		brasero_data_disc_reference_remove_path (disc, path);
	}

	g_slist_foreach (paths, (GFunc) g_free, NULL);
	g_slist_free (paths);
}

static void
brasero_data_disc_inotify_create_file_event_destroy_cb (BraseroDataDisc *disc,
							gpointer callback_data)
{
	GSList *references = callback_data;
	brasero_data_disc_reference_free_list (disc, references);
}

static void
brasero_data_disc_inotify_create_paths (BraseroDataDisc *disc,
					gchar *name,
					GSList *paths,
					const gchar *uri,
					GnomeVFSFileInfo *info)
{
	GSList *iter;
	BraseroFile *file;

	if (GNOME_VFS_FILE_INFO_SYMLINK (info)) {
		if (brasero_data_disc_symlink_is_recursive (disc,
							    uri,
							    info->symlink_name)) {
			brasero_data_disc_remove_uri (disc, uri, TRUE);
			brasero_data_disc_unreadable_new (disc,
							  g_strdup (uri),
							  BRASERO_FILTER_RECURSIVE_SYM);
			return;
		}

		/* NOTE: normally brasero_data_disc_symlink_new can free some
		 * paths but it does it only if there is an overlap which can't
		 * be the case here since it is checked above */
		paths = brasero_data_disc_symlink_new (disc,
						       uri,
						       info,
						       paths);
	}

	file = g_hash_table_lookup (disc->priv->dirs, uri);
	if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY
	&& (!file || file->sectors < 0)) {
		brasero_data_disc_directory_new (disc,
						 g_strdup (uri),
						 FALSE);
	}
	else if (info->type != GNOME_VFS_FILE_TYPE_DIRECTORY
	      && !g_hash_table_lookup (disc->priv->files, uri)) {
		gchar *parent;
		gint64 sectors;
	
		parent = g_path_get_dirname (uri);
		file = g_hash_table_lookup (disc->priv->dirs, parent);
		g_free (parent);

		sectors = GET_SIZE_IN_SECTORS (info->size);
		file->sectors += sectors;
		brasero_data_disc_size_changed (disc, sectors);
	}

	for (iter = paths; iter; iter = iter->next) {
		const gchar *path;
		gchar *path_name;

		/* make it appear in the tree */
		path = iter->data;

		/* make sure the path didn't have its name changed */
		path_name = g_path_get_basename (path);
		if (strcmp (path_name, name)) {
			g_free (path_name);
			continue;
		}

		if (strlen (info->name) > 64)
			brasero_data_disc_joliet_incompat_add_path (disc, path);

		g_free (path_name);

		brasero_data_disc_tree_new_path (disc,
						 path,
						 NULL,
						 NULL);

		brasero_data_disc_tree_set_path_from_info (disc,
							   path,
							   NULL,
							   info);
	}
}

static gboolean
brasero_data_disc_inotify_create_file_event_cb (BraseroDataDisc *disc,
						GSList *results,
						gpointer callback_data)
{
	gchar *name = NULL;
	GSList *paths = NULL;
	GnomeVFSFileInfo *info;
	BraseroInfoAsyncResult *result;
	GSList *references = callback_data;

	/* NOTE: there is just one URI */
	result = results->data;

	if (disc->priv->unreadable
	&&  g_hash_table_lookup (disc->priv->unreadable, result->uri)) {
		brasero_data_disc_remove_uri (disc, result->uri, TRUE);
		goto cleanup;
	}

	if (result->result == GNOME_VFS_ERROR_NOT_FOUND) {
		brasero_data_disc_remove_uri (disc, result->uri, TRUE);
		goto cleanup;
	}

	info = result->info;
	if (info->type == GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK) {
		brasero_data_disc_remove_uri (disc, result->uri, TRUE);
		brasero_data_disc_unreadable_new (disc,
						  g_strdup (result->uri),
						  BRASERO_FILTER_BROKEN_SYM);
		goto cleanup;
	}

	if (result->result == GNOME_VFS_ERROR_LOOP) {
		brasero_data_disc_remove_uri (disc, result->uri, TRUE);
		brasero_data_disc_unreadable_new (disc,
						  g_strdup (result->uri),
						  BRASERO_FILTER_RECURSIVE_SYM);
		goto cleanup;
	}

	if (result->result != GNOME_VFS_OK
	||  !brasero_data_disc_is_readable (info)) {
		brasero_data_disc_remove_uri (disc, result->uri, TRUE);
		brasero_data_disc_unreadable_new (disc,
						  g_strdup (result->uri),
						  BRASERO_FILTER_UNREADABLE);
		goto cleanup;
	}

	/* make sure we still care about this change. At least that it wasn't 
	 * put in the unreadable hash */
	if (brasero_data_disc_is_excluded (disc, result->uri, NULL))
		goto cleanup;

	/* check that this file is not hidden */
	name = g_path_get_basename (result->uri);
	if (name [0] == '.') {
		brasero_data_disc_unreadable_new (disc,
						  g_strdup (result->uri),
						  BRASERO_FILTER_HIDDEN);
		g_free (name);
		goto cleanup;
	}

	/* get the paths that are still valid */
	paths = brasero_data_disc_reference_get_list (disc, references, FALSE);
	references = NULL;
	if (!paths) {
		g_free (name);
		goto cleanup;
	}

	brasero_data_disc_inotify_create_paths (disc,
						name,
						paths,
						result->uri,
						result->info);
	g_free (name);

cleanup:

	if (paths) {
		g_slist_foreach (paths, (GFunc) g_free, NULL);
		g_slist_free (paths);
	}

	/* free callback_data */
	return TRUE;
}

static void
brasero_data_disc_inotify_create_file_event (BraseroDataDisc *disc,
					     BraseroFile *parent,
					     const gchar *uri)
{
	GSList *paths, *iter, *uris;
	BraseroDiscResult result;
	GSList *references = NULL;

	/* make sure the parent of this file is not in loading queue */
	if (g_slist_find (disc->priv->loading, parent))
		return;

	/* here we ask for all paths but the grafted ones */
	paths = brasero_data_disc_uri_to_paths (disc, uri, FALSE);

	/* very unlikely but who knows: no path => its parent was removed */
	if (!paths)
		return;

	/* convert the paths into references that way we'll know if a path is
	 * removed or if the uri is deleted while or just after we explored it
	 */
	for (iter = paths; iter; iter = iter->next) {
		BraseroDataDiscReference ref;

		ref = brasero_data_disc_reference_new (disc, iter->data);
		references = g_slist_prepend (references, GINT_TO_POINTER (ref));
	}
	g_slist_foreach (paths, (GFunc) g_free, NULL);
	g_slist_free (paths);
	
	/* check the status of the file */
	uris = g_slist_prepend (NULL, (gchar*) uri);
	result = brasero_data_disc_get_info_async (disc,
						   uris,
						   GNOME_VFS_FILE_INFO_GET_ACCESS_RIGHTS |
						   GNOME_VFS_FILE_INFO_GET_MIME_TYPE |
						   GNOME_VFS_FILE_INFO_FORCE_SLOW_MIME_TYPE,
						   brasero_data_disc_inotify_create_file_event_cb,
						   references,
						   brasero_data_disc_inotify_create_file_event_destroy_cb);
	g_slist_free (uris);

	if (result != BRASERO_DISC_OK)
		brasero_data_disc_unreadable_new (disc,
						  g_strdup (uri),
						  BRASERO_FILTER_UNREADABLE);
}

static gboolean
brasero_data_disc_inotify_attributes_event_cb (BraseroDataDisc *disc,
					       GSList *results,
					       gpointer null_data)
{
	BraseroInfoAsyncResult *result;
	BraseroFilterStatus status;
	GnomeVFSFileInfo *info;
	char *uri;

	for (; results; results = results->next) {
		result = results->data;

		uri = result->uri;
		info = result->info;
		if (result->result == GNOME_VFS_OK
		&&  brasero_data_disc_is_readable (info)) {
			if (disc->priv->unreadable
			&& (status = GPOINTER_TO_INT (g_hash_table_lookup (disc->priv->unreadable, uri)))
			&&  status == BRASERO_FILTER_UNREADABLE) {
				GSList *tmp;

				brasero_data_disc_unreadable_free (disc, uri);
				tmp = g_slist_prepend (NULL, result);
				brasero_data_disc_inotify_create_file_event_cb (disc,
										tmp,
										NULL);
				g_slist_free (tmp);
			}		
			continue;
		}

		/* the file couldn't be a symlink anyway don't check for loop */
		brasero_data_disc_remove_uri (disc, uri, TRUE);
		if (result->result != GNOME_VFS_ERROR_NOT_FOUND)
			brasero_data_disc_unreadable_new (disc,
							  g_strdup (uri),
							  BRASERO_FILTER_UNREADABLE);
	}

	/* doesn't really matter */
	return TRUE;
}

static void
brasero_data_disc_inotify_attributes_event (BraseroDataDisc *disc,
					    const gchar *uri)
{
	GSList *uris;

	uris = g_slist_prepend (NULL, (char *) uri);
	brasero_data_disc_get_info_async (disc,
					  uris,
					  GNOME_VFS_FILE_INFO_GET_ACCESS_RIGHTS,
					  brasero_data_disc_inotify_attributes_event_cb,
					  NULL,
					  NULL);
	g_slist_free (uris);
}

static gboolean
brasero_data_disc_inotify_modify_file_cb (BraseroDataDisc *disc,
					  GSList *results,
					  gpointer null_data)
{
	BraseroInfoAsyncResult *result;
	BraseroFile *file;
	GnomeVFSFileInfo *info;
	GSList *paths;
	GSList *iter;
	char *uri;

	for (; results; results = results->next) {
		result = results->data;

		uri = result->uri;
		if (result->result == GNOME_VFS_ERROR_NOT_FOUND) {
			brasero_data_disc_remove_uri (disc, uri, TRUE);
			continue;
		}

		info = result->info;
		/* the file couldn't be a symlink so no need to check for loop */
		if (result->result != GNOME_VFS_OK
		||  !brasero_data_disc_is_readable (info)) {
			brasero_data_disc_remove_uri (disc, uri, TRUE);
			brasero_data_disc_unreadable_new (disc,
							  g_strdup (uri),
							  result->result);
			continue;
		}

		/* see if this file has already been looked up */
		if ((file = g_hash_table_lookup (disc->priv->files, uri))) {
			gint64 sectors;

			sectors = GET_SIZE_IN_SECTORS (info->size);
			if (sectors != file->sectors) {
				brasero_data_disc_size_changed (disc, sectors - file->sectors);
				file->sectors = sectors;
			}
		}
		else {
			char *parent;

			parent = g_path_get_dirname (uri);
			file = g_hash_table_lookup (disc->priv->dirs, parent);
			g_free (parent);

			if (file && file->sectors >= 0) {
				/* its parent exists and must be rescanned */
				brasero_data_disc_add_rescan (disc, file);
			}
			else {
				/* the parent doesn't exist any more so nothing happens */
				continue;
			}
		}

		/* search for all the paths it could appear at */
		paths = brasero_data_disc_uri_to_paths (disc, uri, TRUE);
		for (iter = paths; iter; iter = iter->next) {
			gchar *path;

			path = iter->data;
			brasero_data_disc_tree_set_path_from_info (disc,
								   path,
								   NULL,
								   info);
			g_free (path);
		}
		g_slist_free (paths);
	}

	/* doesn't really matter */
	return TRUE;
}

static void
brasero_data_disc_inotify_modify_file (BraseroDataDisc *disc,
				       const gchar *uri)
{
	GSList *uris;

	uris = g_slist_prepend (NULL, (char *) uri);
	brasero_data_disc_get_info_async (disc,
					  uris,
					  GNOME_VFS_FILE_INFO_GET_ACCESS_RIGHTS |
					  GNOME_VFS_FILE_INFO_GET_MIME_TYPE |
					  GNOME_VFS_FILE_INFO_FORCE_SLOW_MIME_TYPE,
					  brasero_data_disc_inotify_modify_file_cb,
					  NULL,
					  NULL);
	g_slist_free (uris);
}

static void
brasero_data_disc_inotify_modify_event (BraseroDataDisc *disc,
					const gchar *uri)
{
	BraseroFile *file;

	/* NOTE: we don't care about modify event in the following cases:
	 * - the file is unreadable
	 * - the file is a symlink
	 * - the file is excluded
	 */

	if ((file = g_hash_table_lookup (disc->priv->dirs, uri))
	&&   file->sectors >= 0)
		brasero_data_disc_add_rescan (disc, file);
	else if ((file = g_hash_table_lookup (disc->priv->files, uri)))
		brasero_data_disc_inotify_modify_file (disc, file->uri);
	else if (disc->priv->unreadable
	      &&  g_hash_table_lookup (disc->priv->unreadable, uri)) {
		return;
	}
	else if (disc->priv->symlinks
	      &&  g_hash_table_lookup (disc->priv->symlinks, uri)) {
		return;
	}
	else if (brasero_data_disc_is_excluded (disc, uri, NULL)) {
		return;
	}
	else
		brasero_data_disc_inotify_modify_file (disc, uri);
}

static gboolean
brasero_data_disc_inotify_moved_timeout_cb (BraseroDataDisc *disc)
{
	BraseroInotifyMovedData *data;

	/* an IN_MOVED_FROM timed out. It is the first in the queue. */
	data = disc->priv->moved_list->data;
	disc->priv->moved_list = g_slist_remove (disc->priv->moved_list, data);
	brasero_data_disc_remove_uri (disc, data->uri, TRUE);

	/* clean up */
	g_free (data->uri);
	g_free (data);
	return FALSE;
}

static void
brasero_data_disc_inotify_moved_from_event (BraseroDataDisc *disc,
					    BraseroFile *parent,
					    const gchar *uri,
					    guint32 cookie)
{
	BraseroInotifyMovedData *data = NULL;

	/* we shouldn't really care if parent directory is not
	 * a dummy and if it hasn't been loaded yet */
	if (g_slist_find (disc->priv->loading, parent))
		return;

	if (!cookie) {
		brasero_data_disc_remove_uri (disc, uri, TRUE);
		return;
	}

	data = g_new0 (BraseroInotifyMovedData, 1);
	data->cookie = cookie;
	data->uri = g_strdup (uri);
			
	/* we remember this move for 5s. If 5s later we haven't received
	 * a corresponding MOVED_TO then we consider the file was
	 * removed. */
	data->id = g_timeout_add (5000,
				  (GSourceFunc) brasero_data_disc_inotify_moved_timeout_cb,
				  disc);

	/* NOTE: the order is important, we _must_ append them */
	disc->priv->moved_list = g_slist_append (disc->priv->moved_list, data);
}

static void
brasero_data_disc_inotify_rename_path (BraseroDataDisc *disc,
				       const gchar *new_uri,
				       const gchar *old_path,
				       const gchar *new_path)
{
	GtkTreePath *treepath;
	GtkTreeModel *model;
	gpointer key = NULL;
	gpointer uri = NULL;
	GtkTreeIter iter;
	gboolean result;
	gchar *name;

	/* update the references */
	brasero_data_disc_joliet_incompat_move (disc, old_path, new_path);
	brasero_data_disc_move_references (disc, old_path, new_path);
	name = g_path_get_basename (new_path);

	if (g_hash_table_lookup_extended (disc->priv->paths,
					  old_path,
					  &key,
					  &uri)) {
		gchar *graft;
		GSList *grafts;

		graft = g_strdup (new_path);

		/* we move the children paths */
		if (g_hash_table_lookup (disc->priv->dirs, uri))
			brasero_data_disc_graft_children_move (disc,
							       old_path,
							       new_path);

		/* it is a graft point update the graft path in hashes */
		g_hash_table_remove (disc->priv->paths, old_path);
		g_hash_table_insert (disc->priv->paths, graft, uri);

		grafts = g_hash_table_lookup (disc->priv->grafts, uri);
		grafts = g_slist_remove (grafts, key);
		grafts = g_slist_prepend (grafts, graft);
		g_hash_table_insert (disc->priv->grafts, uri, grafts);
		g_free (key);
	}
	else if (name [0] == '.') {
		BraseroFile *parent;
		gchar *parent_uri;

		/* check that this file is not hidden otherwise we'll
		 * put it in unreadable wherever it was not grafted */
		brasero_data_disc_tree_remove_path (disc, old_path);

		parent_uri = g_path_get_dirname (new_uri);
		parent = g_hash_table_lookup (disc->priv->dirs, parent_uri);
		g_free (parent_uri);

		brasero_data_disc_add_rescan (disc, parent);
		brasero_data_disc_unreadable_new (disc,
						  g_strdup (new_uri),
						  BRASERO_FILTER_HIDDEN);
		g_free (name);
		return;
	}

	result = brasero_data_disc_disc_path_to_tree_path (disc,
							   old_path,
							   &treepath,
							   NULL);
	if (!result) {
		/* apparently it's not in the tree */
		g_free (name);
		return;
	}

	model = disc->priv->model;
	gtk_tree_model_get_iter (model, &iter, treepath);
	gtk_tree_path_free (treepath);

	gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
			    NAME_COL, name,
			    -1);
	g_free (name);
}

static void
brasero_data_disc_inotify_rename (BraseroDataDisc *disc,
				  GSList *paths,
				  const gchar *old_uri,
				  const gchar *new_uri)
{
	gchar *old_name;
	gchar *new_name;
	GSList *iter;

	new_name = g_path_get_basename (new_uri);
	old_name = g_path_get_basename (old_uri);

	if (!strcmp (new_name, old_name)) {
		/* The file was moved but the names remained the same.
		 * Therefore there is not point in changing the graft
		 * points. */
		g_free (new_name);
		g_free (old_name);
		return;
	}

	for (iter = paths; iter; iter = iter->next) {
		gchar *path_parent;
		gchar *path_name;
		gchar *old_path;
		gchar *new_path;

		old_path = iter->data;

		/* see if the user changed the name. In this case do nothing.
		 * NOTE: it has a graft point already anyway. */
		path_name = g_path_get_basename (old_path);
		if (strcmp (path_name, old_name)) {
			g_free (path_name);
			continue;
		}
		g_free (path_name);

		path_parent = g_path_get_dirname (old_path);
		new_path = g_build_path (G_DIR_SEPARATOR_S,
					 path_parent,
					 new_name,
					 NULL);
		g_free (path_parent);

		/* we want to check if the name already exists in the directory:
		 * since it is a file system event, a file with the same name 
		 * can't already exists on the file system. But the user could
		 * have renamed already in the selection a file with the same
		 * name. So we only have to check the paths hash table. */
		if (g_hash_table_lookup (disc->priv->paths, new_path)) {
			/* the name already exists so we leave the old name but
			 * add a graft point with the old name if hasn't already
			 * a graft point */
			if (!g_hash_table_lookup (disc->priv->paths, old_path))
				brasero_data_disc_graft_new (disc,
							     new_uri,
							     old_path);

			return;
		}

		brasero_data_disc_inotify_rename_path (disc,
						       new_uri,
						       old_path,
						       new_path);

		g_free (new_path);
	}

	/* clean up */
	g_free (old_name);
	g_free (new_name);
}

static gint warned = 0;

static void
brasero_data_disc_cancel_monitoring_real (BraseroDataDisc *disc,
					  BraseroFile *file)
{
	int dev_fd;

	dev_fd = g_io_channel_unix_get_fd (disc->priv->notify);
	inotify_rm_watch (dev_fd, file->handle.wd);
	g_hash_table_remove (disc->priv->monitored,
			     GINT_TO_POINTER (file->handle.wd));

	file->handle.wd = -1;
	warned = 0;
}

static void
brasero_data_disc_remove_top_reference (BraseroDataDisc *disc,
					BraseroFile *file)
{
	gchar *parent;
	BraseroFile *top;

	parent = g_path_get_dirname (file->uri);
	top = g_hash_table_lookup (disc->priv->dirs, parent);
	g_free (parent);

	if (top) {
		/* Decrease the reference counter of the parent */
		top->references --;
		if (top->sectors < 0 && top->references == 0) {
			/* The parent directory is a dummy remove it */
			brasero_data_disc_cancel_monitoring_real (disc, top);
			g_hash_table_remove (disc->priv->dirs, top->uri);
			g_free (top->uri);
			g_free (top);
		}
	}
}

static void
brasero_data_disc_inotify_moved_to_event (BraseroDataDisc *disc,
					  const gchar *uri,
					  guint32 cookie)
{
	BraseroInotifyMovedData *data = NULL;
	BraseroFile *file = NULL;
	gchar *old_parent = NULL;
	gchar *new_parent = NULL;
	GSList *paths;
	GSList *iter;

	if (!cookie) {
		gchar *parent;

		parent = g_path_get_dirname (uri);
		file = g_hash_table_lookup (disc->priv->dirs, parent);
		g_free (parent);

		brasero_data_disc_inotify_create_file_event (disc, file, uri);
		return;
	}

	/* look for a matching cookie */
	for (iter = disc->priv->moved_list; iter; iter = iter->next) {
		data = iter->data;
		if (data->cookie == cookie)
			break;
	}

	if (!data)
		return;

	/* we need to see if it's simple renaming or a real move of a file in
	 * another directory. In the case of a move we can't decide where an
	 * old path (and all its children grafts) should go */
	old_parent = g_path_get_dirname (data->uri);
	new_parent = g_path_get_dirname (uri);
	if (strcmp (new_parent, old_parent)) {
		GSList *grafts;

		/* the file was moved so we simply remove it from the tree
		 * except where it was explicitly grafted and add it again
		 * with its new uri */
		brasero_data_disc_remove_uri (disc, data->uri, FALSE);

		/* it necessarily has a parent otherwise we wouldn't receive
		 * the MOVED_TO event */
		file = g_hash_table_lookup (disc->priv->dirs, new_parent);
		brasero_data_disc_inotify_create_file_event (disc, file, uri);
		
		/* for the graft points we just want to rename them if they have
		 * the same name as the original name */
		paths = NULL;
		grafts = g_hash_table_lookup (disc->priv->grafts, data->uri);
		for (iter = grafts; iter; iter = iter->next)
			paths = g_slist_prepend (paths, g_strdup (iter->data));

		if (!paths)
			goto cleanup;

		/* we update the notification references for directories */
		file->references ++;
		file = g_hash_table_lookup (disc->priv->dirs, old_parent);
		file->references --;
		if (file->sectors < 0 && file->references == 0) {
			/* that was a dummy, remove it */
			brasero_data_disc_remove_top_reference (disc, file);
			brasero_data_disc_cancel_monitoring (disc, file);
			g_hash_table_remove (disc->priv->dirs, file->uri);
			g_free (file->uri);
			g_free (file);
		}			
	}
	else 	/* Do this before we've changed the uri everywhere. */
		paths = brasero_data_disc_uri_to_paths (disc, data->uri, TRUE);

	brasero_data_disc_uri_move (disc, uri, data->uri);
	if (g_hash_table_lookup (disc->priv->dirs, uri)) {
		gint len;
		GSList *children;

		len = strlen (data->uri);

		/* in this case we also need to change all the children URI */
		children = brasero_data_disc_uri_find_children_uris (disc, data->uri);
		for (iter = children; iter; iter = iter->next) {
			gchar *child_new_uri;
			const gchar *child_old_uri;

			child_old_uri = iter->data;
			child_new_uri = g_strconcat (uri,
						     child_old_uri + len,
						     NULL);

			brasero_data_disc_uri_move (disc,
						    child_new_uri,
						    child_old_uri);

			g_free (child_new_uri);
		}
		g_slist_free (children);
	}

	brasero_data_disc_inotify_rename (disc, paths, data->uri, uri);
	g_slist_foreach (paths, (GFunc) g_free, NULL);
	g_slist_free (paths);


cleanup:

	if (old_parent)
		g_free (old_parent);

	if (new_parent)
		g_free (new_parent);

	/* remove the event from the queue */
	disc->priv->moved_list = g_slist_remove (disc->priv->moved_list, data);
	g_source_remove (data->id);
	g_free (data->uri);
	g_free (data);
}

static void
brasero_data_disc_inotify_event (BraseroDataDisc *disc,
				 const gchar *uri,
				 struct inotify_event *event,
				 BraseroFile *parent)
{
	/* NOTE: SELF events are only possible here for dummy directories.
	 * As a general rule we don't take heed of the events happening on
	 * the file being monitored, only those that happen inside a directory.
	 * This is done to avoid treating events twice.
	 * IN_DELETE_SELF or IN_MOVE_SELF are therefore not possible here. */
	if (event->mask & IN_ATTRIB)
		brasero_data_disc_inotify_attributes_event (disc, uri);
	else if (event->mask & IN_MODIFY)
		brasero_data_disc_inotify_modify_event (disc, uri);
	else if (event->mask & IN_MOVED_FROM)
		brasero_data_disc_inotify_moved_from_event (disc, parent, uri, event->cookie);
	else if (event->mask & IN_MOVED_TO)
		brasero_data_disc_inotify_moved_to_event (disc, uri, event->cookie);
	else if (event->mask & IN_DELETE) {
		brasero_data_disc_reference_remove_uri (disc, uri);
		brasero_data_disc_remove_uri (disc, uri, TRUE);
	}
	else if (event->mask & IN_CREATE)
		brasero_data_disc_inotify_create_file_event (disc, parent, uri);
}

static void
brasero_data_disc_inotify_top_directory_event (BraseroDataDisc *disc,
					       BraseroFile *monitored,
					       struct inotify_event *event,
					       const gchar *name)
{
	/* this is a dummy directory used to watch top files so we check
	 * that the file for which that event happened is indeed in 
	 * our selection whether as a single file or as a top directory.
	 * NOTE: since these dummy directories are the real top directories
	 * that's the only case where we treat SELF events, otherwise
	 * to avoid treating events twice, we only choose to treat events
	 * that happened from the parent directory point of view */
	if (!name) {
		/* the event must have happened on this directory */
		if (event->mask & (IN_DELETE_SELF|IN_MOVE_SELF)) {
			/* The directory was moved or removed : find all its
			 * children that are in the selection and remove them. */
			brasero_data_disc_remove_dir_and_children (disc, monitored);
		}
		else if (event->mask & IN_ATTRIB) {
			/* This is just in case this directory
			 * would become unreadable */
			brasero_data_disc_inotify_modify_event (disc, monitored->uri);
		}
	}
	else {
		gchar *uri;

		/* we just check that this is one of our file (one in
		 * the selection). It must be in a hash table then.
		 * There is an exception though for MOVED_TO event.
		 * indeed that can be the future name of file in the
		 * selection so it isn't yet in the hash tables. */
		uri = g_build_path (G_DIR_SEPARATOR_S,
				    monitored->uri,
				    name,
				    NULL);
		if (event->mask & IN_MOVED_TO)
			brasero_data_disc_inotify_moved_to_event (disc, uri, event->cookie);
		else if (g_hash_table_lookup (disc->priv->files, uri) ||
			  g_hash_table_lookup (disc->priv->dirs, uri))
			brasero_data_disc_inotify_event (disc, uri, event, NULL);

		g_free (uri);
	}
}

static gboolean
brasero_data_disc_inotify_monitor_cb (GIOChannel *channel,
				      GIOCondition condition,
				      BraseroDataDisc *disc)
{
	BraseroFile *monitored;
	struct inotify_event event;
	GError *err = NULL;
	GIOStatus status;
	guint size;
	char *name;

	while (condition & G_IO_IN) {
		monitored = NULL;

		status = g_io_channel_read_chars (channel,
						  (char *) &event,
						  sizeof (struct inotify_event),
						  &size, &err);
		if (status == G_IO_STATUS_EOF)
			return TRUE;

		if (event.len) {
			name = g_new (char, event.len + 1);

			name [event.len] = '\0';

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

			if (name) {
				g_free (name);
				name = NULL;
			}

			condition = g_io_channel_get_buffer_condition (channel);
			continue;
		}

		/* FIXME: see if we can use IN_ISDIR */
		monitored = g_hash_table_lookup (disc->priv->monitored,
						 GINT_TO_POINTER (event.wd));

		if (!monitored) {
			g_warning ("Unknown (or already deleted) monitored directory = > ignored \n");
			condition = g_io_channel_get_buffer_condition (channel);
		}
		else if (g_slist_find (disc->priv->rescan, monitored)) {
			/* no need to heed modifications inside a 
			 * directory that is going to be rescanned */
		}
		else if (monitored->sectors < 0)
			brasero_data_disc_inotify_top_directory_event (disc,
								       monitored,
								       &event,
								       name);

		else if (event.mask & IN_UNMOUNT)
			brasero_data_disc_directory_remove_from_tree (disc, monitored);
		else if (name) {
			gchar *uri;

			/* For directories we don't take heed of the SELF events.
			 * All events are treated through the parent directory
			 * events. */

			uri = g_build_path (G_DIR_SEPARATOR_S,
					    monitored->uri,
					    name,
					    NULL);
			brasero_data_disc_inotify_event (disc,
							 uri,
							 &event,
							 monitored);
			g_free (uri);
		}

		if (name) {
			g_free (name);
			name = NULL;
		}

		condition = g_io_channel_get_buffer_condition (channel);
	}

	return TRUE;
}

static gboolean
brasero_data_disc_cancel_monitoring (BraseroDataDisc *disc,
				     BraseroFile *file)
{
	if (disc->priv->notify
	&& !strncmp (file->uri, "file://", 7)
	&&  file->handle.wd != -1) {
		/* see what is the reference counter of this file */
		if (file->references > 0) {
			/* There are still grafted files from this directory.
			 * Make a dummy out of it to carry on with monitoring
			 * the children files. */
			file->sectors = -1;
			g_hash_table_insert (disc->priv->dirs, file->uri, file);
			brasero_data_disc_remove_top_reference (disc, file);
			return FALSE;
		}

		brasero_data_disc_remove_top_reference (disc, file);
		brasero_data_disc_cancel_monitoring_real (disc, file);
	}

	return TRUE;
}

static gboolean
brasero_data_disc_start_monitoring_real (BraseroDataDisc *disc,
					 BraseroFile *file)
{
	gchar *escaped_uri;
	gchar *path;
	gint dev_fd;
	__u32 mask;

	/* NOTE: gnome_vfs_get_local_path_from_uri only works on escaped
	 * uris */
	escaped_uri = gnome_vfs_escape_host_and_path_string (file->uri);
	path = gnome_vfs_get_local_path_from_uri (escaped_uri);
	g_free (escaped_uri);

	dev_fd = g_io_channel_unix_get_fd (disc->priv->notify);
	mask = IN_MODIFY |
	       IN_ATTRIB |
	       IN_MOVED_FROM |
	       IN_MOVED_TO |
	       IN_CREATE |
	       IN_DELETE |
	       IN_DELETE_SELF |
	       IN_MOVE_SELF;

	file->handle.wd = inotify_add_watch (dev_fd, path, mask);
	if (file->handle.wd == -1) {
		if (!warned) {
			g_warning ("ERROR creating watch for local file %s : %s\n",
				   path,
				   strerror (errno));
			warned = 1;
		}

		g_free (path);
		return FALSE;
	}

	g_hash_table_insert (disc->priv->monitored,
			     GINT_TO_POINTER (file->handle.wd),
			     file);

	g_free (path);
	return TRUE;
}

static BraseroMonitorHandle
brasero_data_disc_start_monitoring (BraseroDataDisc *disc,
				    BraseroFile *file)
{
	if (disc->priv->notify && !strncmp (file->uri, "file://", 7)) {
		BraseroFile *top;
		gchar *parent;

		/* we first make sure that his parent directory is being
		 * monitored to handle a renaming of this file */
		parent = g_path_get_dirname (file->uri);
		top = g_hash_table_lookup (disc->priv->dirs, parent);

		if (!top) {
			/* No top directory : create a dummy one */
			top = g_new0 (BraseroFile, 1);
			top->sectors = -1;
			top->uri = parent;
			top->references = 1;
			g_hash_table_insert (disc->priv->dirs, top->uri, top);
			brasero_data_disc_start_monitoring_real (disc, top);
		}
		else {
			/* A parent directory is already watched: add a reference. */
			top->references ++;
			g_free (parent);
		}

		/* we only monitor directories. Files are watched through their
		 * parent directory. We give them the same handle as their parent
		 * directory to find it more easily and mark it as being watched */
		if (g_hash_table_lookup (disc->priv->dirs, file->uri))
			brasero_data_disc_start_monitoring_real (disc, file);
		else if (top)
			file->handle.wd = top->handle.wd;
	}

	return file->handle;
}

#endif
