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
#include <gtk/gtktoolbar.h>

#include <eggtreemultidnd.h>

#include <nautilus-burn-drive.h>
#include <nautilus-burn-drive-monitor.h>

#include <gconf/gconf-client.h>

#include <libgnomeui/libgnomeui.h>

#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-file-info.h>

#ifdef HAVE_LIBNOTIFY

#include <libnotify/notification.h>

#endif

#ifdef BUILD_INOTIFY

#include "inotify.h"
#include "inotify-syscalls.h"

#endif

#include "brasero-ncb.h"
#include "brasero-disc.h"
#include "brasero-data-disc.h"
#include "brasero-filtered-window.h"
#include "brasero-utils.h"
#include "brasero-vfs.h"
#include "burn-session.h"
#include "burn-volume.h"
#include "burn-caps.h"
#include "burn-track.h"
#include "burn-debug.h"

typedef enum {
	STATUS_NO_DRAG,
	STATUS_DRAGGING,
	STATUS_DRAG_DROP
} BraseroDragStatus;

struct BraseroDataDiscPrivate {
	GtkWidget *tree;
	GtkTreeModel *model;
	GtkTreeModel *sort;
       	GtkWidget *filter_dialog;
	GtkWidget *notebook;

	GtkUIManager *manager;
	GtkActionGroup *disc_group;

	BraseroDragStatus drag_status;
	GdkDragContext *drag_context;

	GSList *drag_src;

	gint press_start_x;
	gint press_start_y;
	gint scroll_timeout;
	gint expand_timeout;

	gint activity_counter;

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

	BraseroVFS *vfs;
	BraseroVFSDataID attr_changed;
	BraseroVFSDataID restore_data;
	BraseroVFSDataID expose_grafted;
	BraseroVFSDataID remove_user;
	BraseroVFSDataID restore_excluded;
	BraseroVFSDataID replace_symlink;
	BraseroVFSDataID add_uri;
	BraseroVFSDataID load_restored;
	BraseroVFSDataID load_graft;
	BraseroVFSDataID move_row;
	BraseroVFSDataID create_file;
	BraseroVFSDataID modify_file;
	BraseroVFSDataID directory_contents;
	BraseroVFSDataID expose_type;
	BraseroVFSDataID load_type;
	BraseroVFSDataID check_graft;
	BraseroVFSDataID check_parent_graft;
	BraseroVFSDataID find_first_graft_parent;

	GHashTable *dirs;
	GHashTable *files;
	GHashTable *paths;
	GHashTable *grafts;
	GHashTable *excluded;
	GHashTable *symlinks;
	GHashTable *unreadable;

	GHashTable *joliet_non_compliant;
	GSList *joliet_incompat_uris;

	GHashTable *path_refs;
	GHashTable *references;
	GHashTable *restored;

	NautilusBurnDrive *drive;

	GSList *expose;
	gint expose_id;

	GSList *exposing;

	BraseroVolFile *session;

#ifdef HAVE_LIBNOTIFY

	NotifyNotification *notification;

#endif

	GSList *libnotify;
	guint libnotify_id;

	gint editing:1;
	gint is_loading:1;
	gint reject_files:1;
};

typedef enum {
	ROW_NEW,
	ROW_NOT_EXPLORED,
	ROW_EXPLORING,
	ROW_EXPLORED,
	ROW_EXPANDED,
} BraseroRowStatus;

typedef enum {
	ROW_BOGUS		= 0x00,
	ROW_SESSION		= 0x01,
	ROW_FILE		= 0x02,
} BraseroRowType;

enum {
	ICON_COL,
	NAME_COL,
	SIZE_COL,
	MIME_COL,
	DSIZE_COL,
	ROW_STATUS_COL,
	ROW_TYPE_COL,
	ISDIR_COL,
	EDITABLE_COL,
	STYLE_COL,
	NB_COL
};

struct _BraseroDirectoryContents {
	gchar *uri;
	GSList *entries;
};
typedef struct _BraseroDirectoryContents BraseroDirectoryContents;

struct _BraseroDirectoryEntry {
	gchar *uri;
	GnomeVFSFileInfo *info;
};
typedef struct _BraseroDirectoryEntry BraseroDirectoryEntry;

static void brasero_data_disc_base_init (gpointer g_class);
static void brasero_data_disc_class_init (BraseroDataDiscClass *klass);
static void brasero_data_disc_init (BraseroDataDisc *sp);
static void brasero_data_disc_finalize (GObject *object);
static void brasero_data_disc_iface_disc_init (BraseroDiscIface *iface);
static void brasero_data_disc_get_property (GObject *object,
					    guint prop_id,
					    GValue *value,
					    GParamSpec *pspec);
static void brasero_data_disc_set_property (GObject *object,
					    guint prop_id,
					    const GValue *value,
					    GParamSpec *spec);
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
brasero_data_disc_can_add_uri (BraseroDisc *disc, const gchar *uri);

static BraseroDiscResult
brasero_data_disc_add_uri (BraseroDisc *disc, const gchar *uri);

static void
brasero_data_disc_delete_selected (BraseroDisc *disc);

static void
brasero_data_disc_clear (BraseroDisc *disc);
static void
brasero_data_disc_reset (BraseroDisc *disc);

static guint
brasero_data_disc_add_ui (BraseroDisc *disc,
			  GtkUIManager *manager);

static BraseroDiscResult
brasero_data_disc_load_track (BraseroDisc *disc,
			      BraseroDiscTrack *track);
static BraseroDiscResult
brasero_data_disc_get_track (BraseroDisc *disc,
			     BraseroDiscTrack *track);
static BraseroDiscResult
brasero_data_disc_set_session_param (BraseroDisc *disc,
				     BraseroBurnSession *session);
static BraseroDiscResult
brasero_data_disc_set_session_contents (BraseroDisc *disc,
					BraseroBurnSession *session);

static BraseroDiscResult
brasero_data_disc_get_status (BraseroDisc *disc);

static void
brasero_data_disc_selection_changed_cb (GtkTreeSelection *selection,
					BraseroDataDisc *disc);

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
static void
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
brasero_data_disc_row_expanded_cb (GtkTreeView *tree,
				     GtkTreeIter *sortparent,
				     GtkTreePath *sortpath,
				     BraseroDataDisc *disc);

static void
brasero_data_disc_import_session_cb (GtkToggleAction *action,
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

static gchar *
brasero_data_disc_graft_get (BraseroDataDisc *disc,
			     const gchar *path);
static void
brasero_data_disc_graft_remove_all (BraseroDataDisc *disc,
				    const gchar *uri);
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
			       const gchar *path,
			       const gchar *uri);
static gboolean
brasero_data_disc_is_excluded (BraseroDataDisc *disc,
			       const gchar *uri,
			       BraseroFile *top);

static void
brasero_data_disc_directory_entry_error (BraseroDataDisc *disc,
					 const gchar *uri,
					 BraseroFilterStatus status);

static BraseroDiscResult
brasero_data_disc_expose_path (BraseroDataDisc *disc,
			       const gchar *path);
static void
brasero_data_disc_directory_priority (BraseroDataDisc *disc,
				      BraseroFile *file);
static BraseroDiscResult
brasero_data_disc_directory_load (BraseroDataDisc *disc,
				  BraseroFile *dir,
				  gboolean append);
static BraseroFile *
brasero_data_disc_directory_new (BraseroDataDisc *disc,
				 gchar *uri,
				 gboolean append);

static void
brasero_data_disc_unreadable_new (BraseroDataDisc *disc,
				  gchar *uri,
				  BraseroFilterStatus status);

static gboolean
brasero_data_disc_get_selected_uri (BraseroDisc *disc,
				    gchar **array);

static void
brasero_data_disc_set_drive (BraseroDisc *disc,
			     NautilusBurnDrive *drive);

static void
brasero_data_disc_replace_file (BraseroDataDisc *disc,
				GtkTreePath *parent,
				GtkTreeIter *row,
				const gchar *name);

static gboolean
brasero_data_disc_tree_select_function (GtkTreeSelection *selection,
					GtkTreeModel *model,
					GtkTreePath *treepath,
					gboolean is_selected,
					gpointer null_data);

static void
brasero_data_disc_update_multi_button_state (BraseroDataDisc *disc);

static gchar *BRASERO_CREATED_DIR = "created";
static gchar *BRASERO_IMPORTED_FILE = "imported";

#define BRASERO_ADD_TO_EXPOSE_QUEUE(disc, contents)	\
	disc->priv->expose = g_slist_append (disc->priv->expose, contents);	\
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

static gboolean filter_notify = FALSE;
static gboolean filter_hidden = FALSE;
static gboolean filter_broken_sym = FALSE;
static guint notify_notify = 0;
static guint hidden_notify = 0;
static guint broken_sym_notify = 0;

static GObjectClass *parent_class = NULL;

static GtkActionEntry entries [] = {
	{"ContextualMenu", NULL, N_("Menu")},
	{"OpenFile", GTK_STOCK_OPEN, NULL, NULL, N_("Open the selected files"),
	 G_CALLBACK (brasero_data_disc_open_activated_cb)},
	{"RenameData", NULL, N_("R_ename..."), NULL, N_("Rename the selected file"),
	 G_CALLBACK (brasero_data_disc_rename_activated_cb)},
	{"DeleteData", GTK_STOCK_REMOVE, NULL, NULL, N_("Remove the selected files from the project"),
	 G_CALLBACK (brasero_data_disc_delete_activated_cb)},
	{"PasteData", GTK_STOCK_PASTE, NULL, NULL, N_("Add the files stored in the clipboard"),
	 G_CALLBACK (brasero_data_disc_paste_activated_cb)},
	{"NewFolder", "folder-new", N_("New folder"), NULL, N_("Create a new empty folder"),
	 G_CALLBACK (brasero_data_disc_new_folder_clicked_cb)},
	{"FileFilter", GTK_STOCK_UNDELETE, N_("Removed Files"), NULL, N_("Display the files filtered from the project"),
	 G_CALLBACK (brasero_data_disc_filtered_files_clicked_cb)},	
};

static GtkToggleActionEntry toggle_entries [] = {
	{"ImportSession", "drive-optical", N_("Import Session"), NULL, N_("Import session"),
	 G_CALLBACK (brasero_data_disc_import_session_cb), FALSE},
};

static const gchar *description = {
	"<ui>"
	"<menubar name='menubar' >"
		"<menu action='EditMenu'>"
		"<placeholder name='EditPlaceholder'>"
			"<menuitem action='NewFolder'/>"
			"<menuitem action='ImportSession'/>"
		"</placeholder>"
		"</menu>"
		"<menu action='ViewMenu'>"
		"<placeholder name='ViewPlaceholder'>"
			"<menuitem action='FileFilter'/>"
		"</placeholder>"
		"</menu>"
	"</menubar>"
	"<popup action='ContextMenu'>"
		"<menuitem action='OpenFile'/>"
		"<menuitem action='DeleteData'/>"
		"<menuitem action='RenameData'/>"
		"<separator/>"
		"<menuitem action='PasteData'/>"
	"</popup>"
	"<toolbar name='Toolbar'>"
		"<placeholder name='DiscButtonPlaceholder'>"
			"<separator/>"
			"<toolitem action='NewFolder'/>"
			"<toolitem action='ImportSession'/>"
			"<separator/>"
			"<toolitem action='FileFilter'/>"
			"<separator/>"
		"</placeholder>"
	"</toolbar>"
	"</ui>"
};


/* Like mkisofs we count in sectors (= 2048 bytes). So we need to divide the 
 * size of each file by 2048 and if it is not a multiple we add one sector. That
 * means that on the CD/DVD a file might occupy more space than its real space
 * since it will need nb_sectors * 2048. */
#define GET_SIZE_IN_SECTORS(size) (size % 2048 ? size / 2048 + 1 : size / 2048)

GType
brasero_data_disc_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroDataDiscClass),
			brasero_data_disc_base_init,
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
brasero_data_disc_filter_notify_cb (GConfClient *client,
				    guint cnxn_id,
				    GConfEntry *entry,
				    gpointer user_data)
{
	GConfValue *value;

	value = gconf_entry_get_value (entry);
	filter_notify = gconf_value_get_bool (value);
}

static void
brasero_data_disc_filter_hidden_cb (GConfClient *client,
				    guint cnxn_id,
				    GConfEntry *entry,
				    gpointer user_data)
{
	GConfValue *value;

	value = gconf_entry_get_value (entry);
	filter_hidden = gconf_value_get_bool (value);
}

static void
brasero_data_disc_filter_broken_sym_cb (GConfClient *client,
					guint cnxn_id,
					GConfEntry *entry,
					gpointer user_data)
{
	GConfValue *value;

	value = gconf_entry_get_value (entry);
	filter_broken_sym = gconf_value_get_bool (value);
}

static void
brasero_data_disc_base_init (gpointer g_class)
{
	GConfClient *client;
	GError *error = NULL;

	client = gconf_client_get_default ();

	filter_notify = gconf_client_get_bool (client,
					       BRASERO_FILTER_NOTIFY_KEY,
					       NULL);
	notify_notify = gconf_client_notify_add (client,
						 BRASERO_FILTER_NOTIFY_KEY,
						 brasero_data_disc_filter_notify_cb,
						 NULL, NULL, &error);
	if (error) {
		g_warning ("GConf : %s\n", error->message);
		g_error_free (error);
	}

	filter_hidden = gconf_client_get_bool (client,
					       BRASERO_FILTER_HIDDEN_KEY,
					       NULL);
	hidden_notify = gconf_client_notify_add (client,
						 BRASERO_FILTER_HIDDEN_KEY,
						 brasero_data_disc_filter_hidden_cb,
						 NULL, NULL, &error);
	if (error) {
		g_warning ("GConf : %s\n", error->message);
		g_error_free (error);
	}

	filter_broken_sym = gconf_client_get_bool (client,
						   BRASERO_FILTER_BROKEN_SYM_KEY,
						   NULL);
	broken_sym_notify = gconf_client_notify_add (client,
						     BRASERO_FILTER_BROKEN_SYM_KEY,
						     brasero_data_disc_filter_broken_sym_cb,
						     NULL, NULL, &error);
	if (error) {
		g_warning ("GConf : %s\n", error->message);
		g_error_free (error);
	}

	g_object_unref (client);
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
	iface->can_add_uri = brasero_data_disc_can_add_uri;
	iface->add_uri = brasero_data_disc_add_uri;
	iface->delete_selected = brasero_data_disc_delete_selected;
	iface->clear = brasero_data_disc_clear;
	iface->reset = brasero_data_disc_reset;
	iface->get_track = brasero_data_disc_get_track;
	iface->set_session_param = brasero_data_disc_set_session_param;
	iface->set_session_contents = brasero_data_disc_set_session_contents;
	iface->load_track = brasero_data_disc_load_track;
	iface->get_status = brasero_data_disc_get_status;
	iface->get_selected_uri = brasero_data_disc_get_selected_uri;
	iface->add_ui = brasero_data_disc_add_ui;
	iface->set_drive = brasero_data_disc_set_drive;
}

static void
brasero_data_disc_get_property (GObject * object,
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

static void
brasero_data_disc_set_property (GObject * object,
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
		gint nba, nbb;

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
			       gint column)
{
	gboolean isdira, isdirb;
	gchar *stringa, *stringb;
	GtkSortType order;
	gint retval;

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

/******************************* notifications    ******************************/
struct _BraseroNotification {
	gchar *primary;
	gchar *secondary;
	GtkWidget *widget;
};
typedef struct _BraseroNotification BraseroNotification;

static void
brasero_data_disc_notification_free (BraseroNotification *notification)
{
	g_free (notification->primary);
	g_free (notification->secondary);
	g_object_unref (notification->widget);
	g_free (notification);
}

#ifdef HAVE_LIBNOTIFY

static void
brasero_data_disc_notification_closed (NotifyNotification *notification,
				       BraseroDataDisc *disc)
{
	g_object_unref (notification);
	disc->priv->notification = NULL;
}

#endif

static gboolean
brasero_data_disc_notify_user_real (gpointer data)
{
	BraseroNotification *notification;
	BraseroDataDisc *disc;

	disc = data;
	if (!disc->priv->libnotify) {
		disc->priv->libnotify_id = 0;
		return FALSE;
	}

	notification = disc->priv->libnotify->data;

#ifdef HAVE_LIBNOTIFY

	GtkWidget *toplevel;

	/* see if we should notify the user. What we want to avoid is to have
	 * two notifications at the same time */
	NotifyNotification *notify;

	/* see if the previous notification has finished to be displayed */
	if (disc->priv->notification)
		return TRUE;

	/* is the widget ready and is the toplevel window active ? */
	toplevel = gtk_widget_get_toplevel (notification->widget);
	if (!GTK_WIDGET_REALIZED (notification->widget)
	||  !gtk_window_has_toplevel_focus (GTK_WINDOW (toplevel)))
		return TRUE;

	/* Good to go */
	notify = notify_notification_new (notification->primary,
					  notification->secondary,
					  NULL,
					  notification->widget);
	notify_notification_set_timeout (notify, 5000);
	g_signal_connect (notify,
			  "closed",
			  G_CALLBACK (brasero_data_disc_notification_closed),
			  disc);
	notify_notification_show (notify, NULL);
	disc->priv->notification = notify;

#endif

	disc->priv->libnotify = g_slist_remove (disc->priv->libnotify, notification);
	brasero_data_disc_notification_free (notification);

	return TRUE;
}

static void
brasero_data_disc_notify_user (BraseroDataDisc *disc,
			       const gchar *primary_message,
			       const gchar *secondary_message,
			       GtkWidget *focus)
{
	BraseroNotification *notification;

	/* if the widget doesn't even exist, no need to display a notification */
	if (!focus)
		return;

	/* we delay notifications since they are sometimes generated just before
	 * a widget is shown and therefore appear in the right corner and not 
	 * focused on the widget */
	notification = g_new0 (BraseroNotification, 1);
	notification->primary = g_strdup (primary_message);
	notification->secondary = g_strdup (secondary_message);
	notification->widget = focus;
	g_object_ref (focus);

	disc->priv->libnotify = g_slist_prepend (disc->priv->libnotify, notification);
	if (!disc->priv->libnotify_id)
		disc->priv->libnotify_id = g_timeout_add (500,
							  brasero_data_disc_notify_user_real,
							  disc);
}

static guint
brasero_data_disc_add_ui (BraseroDisc *disc, GtkUIManager *manager)
{
	BraseroDataDisc *data_disc;
	GError *error = NULL;
	GtkAction *action;
	guint merge_id;

	data_disc = BRASERO_DATA_DISC (disc);

	if (!data_disc->priv->disc_group) {
		data_disc->priv->disc_group = gtk_action_group_new (BRASERO_DISC_ACTION);
		gtk_action_group_set_translation_domain (data_disc->priv->disc_group, GETTEXT_PACKAGE);
		gtk_action_group_add_actions (data_disc->priv->disc_group,
					      entries,
					      G_N_ELEMENTS (entries),
					      disc);
		gtk_action_group_add_toggle_actions (data_disc->priv->disc_group,
						     toggle_entries,
						     G_N_ELEMENTS (toggle_entries),
						     disc);
		gtk_ui_manager_insert_action_group (manager,
						    data_disc->priv->disc_group,
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

	action = gtk_action_group_get_action (data_disc->priv->disc_group, "ImportSession");
	g_object_set (action,
		      "short-label", _("Import"), /* for toolbar buttons */
		      NULL);

	action = gtk_action_group_get_action (data_disc->priv->disc_group, "FileFilter");
	g_object_set (action,
		      "short-label", _("Filtered Files"), /* for toolbar buttons */
		      NULL);
	action = gtk_action_group_get_action (data_disc->priv->disc_group, "NewFolder");
	g_object_set (action,
		      "short-label", _("New Folder"), /* for toolbar buttons */
		      NULL);

	data_disc->priv->manager = manager;
	g_object_ref (manager);

	brasero_data_disc_update_multi_button_state (data_disc);
	return merge_id;
}

static void
brasero_data_disc_init (BraseroDataDisc *obj)
{
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeModel *model;
	GtkWidget *scroll;

	obj->priv = g_new0 (BraseroDataDiscPrivate, 1);
	gtk_box_set_spacing (GTK_BOX (obj), 6);

	obj->priv->vfs = brasero_vfs_get_default ();

	/* the information displayed about how to use this tree */
	obj->priv->notebook = brasero_utils_get_use_info_notebook ();
	gtk_box_pack_start (GTK_BOX (obj), obj->priv->notebook, TRUE, TRUE, 0);

	/* Tree */
	obj->priv->tree = gtk_tree_view_new ();
	gtk_tree_view_set_enable_tree_lines (GTK_TREE_VIEW (obj->priv->tree), TRUE);
	gtk_tree_view_set_rubber_banding (GTK_TREE_VIEW (obj->priv->tree), TRUE);

	/* This must be before connecting to button press event */
	egg_tree_multi_drag_add_drag_support (GTK_TREE_VIEW (obj->priv->tree));

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

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (obj->priv->tree));
	g_signal_connect (selection,
			  "changed",
			  G_CALLBACK (brasero_data_disc_selection_changed_cb),
			  obj);

	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	gtk_tree_selection_set_select_function (selection,
						brasero_data_disc_tree_select_function,
						NULL,
						NULL);

	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (obj->priv->tree), TRUE);

	model = (GtkTreeModel*) gtk_tree_store_new (NB_COL,
						    G_TYPE_STRING,
						    G_TYPE_STRING,
						    G_TYPE_STRING,
						    G_TYPE_STRING,
						    G_TYPE_INT64,
						    G_TYPE_INT,
						    G_TYPE_INT,
						    G_TYPE_BOOLEAN,
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
					    "icon-name", ICON_COL);

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
					    "style", STYLE_COL);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "editable", EDITABLE_COL);

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
							   renderer,
							   "text", SIZE_COL,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (obj->priv->tree),
				     column);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_sort_column_id (column,
						 SIZE_COL);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Description"),
							   renderer,
							   "text", MIME_COL,
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
			  G_CALLBACK (brasero_data_disc_row_expanded_cb),
			  obj);
	g_signal_connect (G_OBJECT (obj->priv->tree),
			  "row-collapsed",
			  G_CALLBACK (brasero_data_disc_row_collapsed_cb),
			  obj);

	/* useful things for directory exploration */
	obj->priv->dirs = g_hash_table_new (g_str_hash, g_str_equal);
	obj->priv->files = g_hash_table_new (g_str_hash, g_str_equal);
	obj->priv->grafts = g_hash_table_new (g_str_hash, g_str_equal);
	obj->priv->paths = g_hash_table_new (g_str_hash, g_str_equal);

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
	cobj = BRASERO_DATA_DISC (object);

	brasero_data_disc_clean (cobj);

	if (cobj->priv->vfs) {
		brasero_vfs_cancel (cobj->priv->vfs, cobj);
		g_object_unref (cobj->priv->vfs);
		cobj->priv->vfs = NULL;
	}

#ifdef HAVE_LIBNOTIFY

	if (cobj->priv->notification) {
		NotifyNotification *notification;

		notification = cobj->priv->notification;
		cobj->priv->notification = NULL;

		notify_notification_close (notification, NULL);
		g_object_unref (notification);
	}

#endif

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

	g_hash_table_destroy (cobj->priv->grafts);
	g_hash_table_destroy (cobj->priv->paths);
	g_hash_table_destroy (cobj->priv->dirs);
	g_hash_table_destroy (cobj->priv->files);

	if (cobj->priv->path_refs)
		g_hash_table_destroy (cobj->priv->path_refs);

	if (cobj->priv->manager) {
		g_object_unref (cobj->priv->manager);
		cobj->priv->manager = NULL;
	}

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
	if (BRASERO_DATA_DISC (disc)->priv->loading)
		return BRASERO_DISC_LOADING;

	if (BRASERO_DATA_DISC (disc)->priv->activity_counter)
		return BRASERO_DISC_NOT_READY;

	if (!g_hash_table_size (BRASERO_DATA_DISC (disc)->priv->paths))
		return BRASERO_DISC_ERROR_EMPTY_SELECTION;

	return BRASERO_DISC_OK;
}

/**************************** burn button **************************************/
static void
brasero_data_disc_selection_changed (BraseroDataDisc *disc, gboolean notempty)
{
	brasero_disc_contents_changed (BRASERO_DISC (disc), notempty);
}

static void
brasero_data_disc_selection_changed_cb (GtkTreeSelection *selection,
					BraseroDataDisc *disc)
{
	GtkTreeView *treeview;
	GtkTreeModel *model;
	GList *selected;

	treeview = gtk_tree_selection_get_tree_view (selection);
	selected = gtk_tree_selection_get_selected_rows (selection, &model);

	if (disc->priv->selected_path)
		gtk_tree_path_free (disc->priv->selected_path);
	
	disc->priv->selected_path = NULL;

	if (selected) {
		GtkTreeIter iter;
		gint type;

		/* we need to make sure that this is not a bogus row */
		gtk_tree_model_get_iter (model, &iter, selected->data);
		gtk_tree_model_get (model, &iter,
				    ROW_TYPE_COL, &type,
				    -1);

		if (type != ROW_BOGUS && type != ROW_SESSION)
			disc->priv->selected_path = gtk_tree_path_copy (selected->data);

		g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
		g_list_free (selected);
	}

	brasero_disc_selection_changed (BRASERO_DISC (disc));
}

/*************************** GtkTreeView functions *****************************/
static gboolean
brasero_data_disc_name_exist_dialog (BraseroDataDisc *disc,
				     const gchar *name)
{
	gint answer;
	GtkWidget *dialog;
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (disc));
	dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					 GTK_DIALOG_DESTROY_WITH_PARENT |
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_WARNING,
					 GTK_BUTTONS_YES_NO,
					 _("\"%s\" already exists in the directory:"),
					 name);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Already existing file"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  _("Do you want to replace it?"));

	gtk_widget_show_all (dialog);
	answer = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	return (answer == GTK_RESPONSE_YES);
}

static BraseroDiscResult
brasero_data_disc_tree_check_name_validity (BraseroDataDisc *disc,
					    const gchar *name,
					    GtkTreePath *treepath,
					    gboolean usedialog)
{
	gchar *row_name;
	GtkTreeIter iter;
	GtkTreeIter child;
	GtkTreeModel *model;

	if (name && name [0] == '\0')
		return BRASERO_DISC_ERROR_UNKNOWN;

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
			gboolean answer;

			g_free (row_name);

			if (!usedialog)
				return BRASERO_DISC_ERROR_ALREADY_IN_TREE;

			/* A file with a same name already exists, ask
			 * the user if he wants to replace it */
			answer = brasero_data_disc_name_exist_dialog (disc, name);
			if (!answer)
				return BRASERO_DISC_ERROR_ALREADY_IN_TREE;

			brasero_data_disc_replace_file (disc, treepath, &child, name);
			break;
		}

		g_free (row_name);
	} while (gtk_tree_model_iter_next (model, &child));
	
	return BRASERO_DISC_OK;
}

static void
brasero_data_disc_remove_bogus_child (BraseroDataDisc *disc,
				      GtkTreeIter *iter)
{
	gint type;
	GtkTreeIter child;
	GtkTreeModel *model;

	/* see if there is a bogus row lingering */
	model = disc->priv->model;
	if (!gtk_tree_model_iter_children (model, &child, iter))
		return;

	do {
		gtk_tree_model_get (model, &child,
				    ROW_TYPE_COL, &type,
				    -1);
	
		if (type == ROW_BOGUS) {
			if (!gtk_tree_store_remove (GTK_TREE_STORE (model), &child))
				break;
		}
		else if (!gtk_tree_model_iter_next (model, &child))
			break;

	} while (1);
}

static void
brasero_data_disc_tree_update_directory_real (BraseroDataDisc *disc,
					      GtkTreeIter *iter)
{
	gchar *nb_items_string;
	GtkTreeModel *model;
	gint nb_items;

	model = disc->priv->model;

	nb_items = gtk_tree_model_iter_n_children (model, iter);
	if (nb_items == 0) {
		GtkTreeIter child;

		/* put collapsed state icon */
		gtk_tree_store_set (GTK_TREE_STORE (model), iter,
				    ICON_COL, "folder",
				    -1);

		nb_items_string = g_strdup (_("empty"));
		gtk_tree_store_prepend (GTK_TREE_STORE (model), &child, iter);
		gtk_tree_store_set (GTK_TREE_STORE (model), &child,
				    NAME_COL, _("(empty)"),
				    STYLE_COL, PANGO_STYLE_ITALIC,
				    ROW_TYPE_COL, ROW_BOGUS,
				    EDITABLE_COL, FALSE,
				    -1);
	}
	else if (nb_items == 1) {
		gint type;
		GtkTreeIter child;

		gtk_tree_model_iter_children (model, &child, iter);
		gtk_tree_model_get (model, &child,
				    ROW_TYPE_COL, &type,
				    -1);

		if (type == ROW_BOGUS)
			nb_items_string = g_strdup (_("empty"));
		else
			nb_items_string = g_strdup (_("1 item"));
	}
	else
		nb_items_string = g_strdup_printf (ngettext ("%d item", "%d items", nb_items), nb_items);

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

	if (!path)
		return;

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
					  gchar **discpath)
{
	gint i;
	gchar *name;
	GString *path;
	GtkTreeIter row;
	GtkTreePath *iter;
	GtkTreeModel *model;
	gint *indices, depth;

	if (!treepath || gtk_tree_path_get_depth (treepath) < 1) {
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

static const gchar *
brasero_data_disc_add_path_item_position (GtkTreeModel *model,
					  GtkTreeIter *row,
					  GtkTreePath *path,
					  const gchar *ptr)
{
	GtkTreeIter child;
	gint position;
	gchar *next;
	gchar *name;
	gint len;

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
					  const gchar *path,
					  GtkTreePath **treepath,
					  const gchar **end)
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
				    const gchar *path)
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
	gchar *path;

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
	gchar *name;

	if (!parent_treepath) {
		gchar *parent;

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

		brasero_data_disc_remove_bogus_child (disc, &iter);
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
			    ROW_STATUS_COL, ROW_NEW,
			    ROW_TYPE_COL, ROW_FILE,
			    EDITABLE_COL, TRUE,
			    -1);
	g_free (name);

	return TRUE;
}

static gboolean
brasero_data_disc_tree_set_path_from_info (BraseroDataDisc *disc,
					   const gchar *path,
					   const GtkTreePath *treepath,
					   const GnomeVFSFileInfo *info)
{
	const gchar *description;
	GtkTreeModel *model;
	GtkTreeIter parent;
	gchar *icon_string;
	GtkTreeIter iter;
	gboolean result;
	gboolean isdir;
	gint64 dsize;
	gchar *name;
	gchar *size;

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
		icon_string = gnome_icon_lookup (gtk_icon_theme_get_default (), NULL,
						 NULL, NULL, NULL, info->mime_type,
						 GNOME_ICON_LOOKUP_FLAGS_NONE, NULL);
		description = gnome_vfs_mime_get_description (info->mime_type);
	}
	else {
		icon_string = NULL;
		description = NULL;
	}

	name = g_path_get_basename (path);
	gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
			    ICON_COL, icon_string,
			    NAME_COL, name,
			    DSIZE_COL, dsize,
			    SIZE_COL, size,
			    MIME_COL, description,
			    ISDIR_COL, isdir,
			    ROW_TYPE_COL, ROW_FILE,
			    EDITABLE_COL, TRUE,
			    -1);
	g_free (name);
	g_free (size);
	g_free (icon_string);

	if (info->type != GNOME_VFS_FILE_TYPE_DIRECTORY) {
		gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
				    ROW_STATUS_COL, ROW_EXPANDED,
				    -1);
		return TRUE;
	}

	/* see if this directory should be explored */
	if (gtk_tree_model_iter_parent (model, &parent, &iter)) {
		gint status;

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
					      const gchar *path,
					      gint state,
					      gboolean edit)
{
	GtkTreeViewColumn *column;
	const gchar *description;
	GtkTreePath *treepath;
	GtkTreeIter sort_iter;
	GtkTreeModel *model;
	GtkTreeIter child;
	gboolean result;
	gchar *parent;
	gchar *name;

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
		if (!result) {
			gtk_tree_path_free (treepath);
			return FALSE;
		}

		brasero_data_disc_remove_bogus_child (disc, &iter);
		gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);
		brasero_data_disc_tree_update_directory_real (disc, &iter);
	}
	else
		gtk_tree_store_append (GTK_TREE_STORE (model), &child, NULL);

	description = gnome_vfs_mime_get_description ("x-directory/normal");
	name = g_path_get_basename (path);

	gtk_tree_store_set (GTK_TREE_STORE (model), &child,
			    NAME_COL, name,
			    MIME_COL, description,
			    ISDIR_COL, TRUE,
			    ICON_COL, "folder",
			    ROW_STATUS_COL, state,
			    ROW_TYPE_COL, ROW_FILE,
			    EDITABLE_COL, TRUE,
			    -1);
	g_free (name);

	brasero_data_disc_tree_update_directory_real (disc, &child);

	if (edit) {
		if (treepath) {
			GtkTreePath *sort_parent;

			sort_parent = gtk_tree_model_sort_convert_child_path_to_path (GTK_TREE_MODEL_SORT (disc->priv->sort),
										      treepath);

			if (!gtk_tree_view_row_expanded (GTK_TREE_VIEW (disc->priv->tree), sort_parent))
				gtk_tree_view_expand_row (GTK_TREE_VIEW (disc->priv->tree), sort_parent, FALSE);

			gtk_tree_path_free (sort_parent);
			gtk_tree_path_free (treepath);
		}

		/* we leave the possibility to the user to edit the name */
		gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (disc->priv->sort),
								&sort_iter,
								&child);

		treepath = gtk_tree_model_get_path (disc->priv->sort, &sort_iter);

		/* grab focus must be called before next function to avoid
		 * triggering a bug where if pointer is not in the widget 
		 * any more and enter is pressed the cell will remain editable */
		column = gtk_tree_view_get_column (GTK_TREE_VIEW (disc->priv->tree), 0);
		gtk_widget_grab_focus (disc->priv->tree);
		gtk_tree_view_set_cursor (GTK_TREE_VIEW (disc->priv->tree),
					  treepath,
					  column,
					  TRUE);
		gtk_tree_path_free (treepath);
	}

	return TRUE;
}

static gboolean
brasero_data_disc_tree_new_loading_row (BraseroDataDisc *disc,
					const gchar *path)
{
	GtkTreePath *treepath;
	GtkTreeModel *model;
	GtkTreeIter child;
	gboolean result;
	gchar *parent;
	gchar *name;

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

		brasero_data_disc_remove_bogus_child (disc, &iter);
		gtk_tree_store_append (GTK_TREE_STORE (model), &child, &iter);
		brasero_data_disc_tree_update_directory_real (disc, &iter);
	}
	else
		gtk_tree_store_append (GTK_TREE_STORE (model), &child, NULL);

	name = g_path_get_basename (path);
	gtk_tree_store_set (GTK_TREE_STORE (model), &child,
			    NAME_COL, name,
			    ICON_COL, "image-loading",
			    SIZE_COL, _("(loading ...)"),
			    MIME_COL, _("(loading ...)"),
			    ROW_STATUS_COL, ROW_NEW,
			    ROW_TYPE_COL, ROW_FILE,
			    EDITABLE_COL, TRUE,
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
	gchar *value;

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
	gchar *path;
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
	gchar *path;
	gint len;
	GSList *list;
};
typedef struct _MakeReferencesListData MakeReferencesListData;

static void
_foreach_make_references_list_cb (BraseroDataDiscReference num,
				  gchar *path,
				  MakeReferencesListData *data)
{
	if (!strncmp (path, data->path, data->len)
	&& (*(path + data->len) == G_DIR_SEPARATOR || *(path + data->len) == '\0'))
		data->list = g_slist_prepend (data->list, GINT_TO_POINTER (num));
}

static void
brasero_data_disc_move_references (BraseroDataDisc *disc,
				   const gchar *oldpath,
				   const gchar *newpath)
{
	MakeReferencesListData callback_data;
	BraseroDataDiscReference ref;
	gchar *newvalue;
	gchar *value;
	gint len;

	if (!disc->priv->path_refs)
		return;

	len = strlen (oldpath);
	callback_data.path = (gchar*) oldpath;
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
	gchar *value;

	if (!disc->priv->path_refs)
		return;

	callback_data.path = (gchar*) path;
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
_foreach_add_to_list_cb (BraseroDataDiscReference num,
			 gchar *path,
			 gpointer callback_data)
{
	GSList **list = callback_data;

	if (path == BRASERO_INVALID_REFERENCE)
		return;

	*list = g_slist_prepend (*list, GINT_TO_POINTER (num));
}

static void
brasero_data_disc_reference_invalidate_all (BraseroDataDisc *disc)
{
	BraseroDataDiscReference ref;
	GSList *list = NULL;
	gchar *value;

	if (!disc->priv->path_refs)
		return;


	if (!disc->priv->path_refs)
		return;

	g_hash_table_foreach (disc->priv->path_refs,
			      (GHFunc) _foreach_add_to_list_cb,
			      &list);

	for (; list; list = g_slist_remove (list, GINT_TO_POINTER (ref))) {
		ref = GPOINTER_TO_INT (list->data);

		value = g_hash_table_lookup (disc->priv->path_refs,
					     GINT_TO_POINTER (ref));
		g_hash_table_replace (disc->priv->path_refs,
				      GINT_TO_POINTER (ref),
				      BRASERO_INVALID_REFERENCE);
		g_free (value);
	}
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
	gchar *dot;

	/* key is equal to the parent path and the 64 first characters (always 
	 * including the extension) of the name */
	parent = g_path_get_dirname (path);
	name = g_path_get_basename (path);
	dot = g_utf8_strrchr (name, -1, '.');

	if (dot && strlen (dot) > 1 && strlen (dot) < 5) {
		if (strcmp (parent, G_DIR_SEPARATOR_S))
			key = g_strdup_printf ("%s" G_DIR_SEPARATOR_S "%.*s%s",
					       parent,
					       64 - strlen (dot),
					       name,
					       dot);
		else
			key = g_strdup_printf (G_DIR_SEPARATOR_S "%.*s%s",
					       64 - strlen (dot),
					       name,
					       dot);
	}
	else if (strcmp (parent, G_DIR_SEPARATOR_S))
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
	gchar *dot;
	gint width;
	gint num;

	key = brasero_data_disc_joliet_get_key (path, &name, parent);
	list = g_hash_table_lookup (disc->priv->joliet_non_compliant, key);

	if (!list)
		return name;

	if (g_slist_length (list) == 1) {
		gchar *retval;

		/* simply return joliet name truncated to 64 chars.
		 * try to keep the extension. */
		dot = g_utf8_strrchr (name, -1, '.');
		if (dot && strlen (dot) < 5 && strlen (dot) > 1 )
			retval = g_strdup_printf ("%.*s%s",
						  64 - strlen (dot),
						  name,
						  dot);
	        else
			retval = g_strdup_printf ("%.64s", name);

		g_free (name);
		return retval;		
	}

	node = g_slist_find_custom (list, name, (GCompareFunc) strcmp);
	num = g_slist_index (list, node->data);

	width = 1;
	while (num / (width * 10)) width ++;
	width = 64 - width;

	/* try to keep the extension */
	dot = g_utf8_strrchr (name, -1, '.');
	if (dot && strlen (dot) < 5 && strlen (dot) > 1 )
		retval = g_strdup_printf ("%.*s%i%s",
					  width - strlen (dot),
					  name,
					  num,
					  dot);
	else
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
	g_free (parent);
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

struct _BraseroJolietMakeList {
	GSList *list;
	const gchar *path;
	gint len;
};
typedef struct _BraseroJolietMakeList BraseroJolietMakeList;

static void
brasero_data_disc_joliet_incompat_find_cb (gchar *path,
					   GSList *list,
					   BraseroJolietMakeList *callback_data)
{
	if (!strncmp (path, callback_data->path, callback_data->len)
	&&  *(path + callback_data->len) == G_DIR_SEPARATOR)
		callback_data->list = g_slist_prepend (callback_data->list, path);
}

static gboolean
brasero_data_disc_joliet_incompat_free (BraseroDataDisc *disc,
					const gchar *path)
{
	GSList *list, *node;
	gchar *name;
	gchar *key;

	/* remove the exact path if it is a joliet non compliant file */
	key = brasero_data_disc_joliet_get_key (path, &name, NULL);
	list = g_hash_table_lookup (disc->priv->joliet_non_compliant, key);
	if (!list) {
		g_free (key);
		return FALSE;
	}

	node = g_slist_find_custom (list, name, (GCompareFunc) strcmp);
	if (node) {
		list = g_slist_remove (list, node->data);
		if (!list) {
			/* NOTE: we don't free the hash table now if it's empty,
			 * since this function could have been called by move
			 * function and in this case a path could probably be
			 * re-inserted */
			g_hash_table_remove (disc->priv->joliet_non_compliant, key);
		}
		else
			g_hash_table_insert (disc->priv->joliet_non_compliant, key, list);
	}

	return TRUE;
}

static void
brasero_data_disc_joliet_incompat_remove_path (BraseroDataDisc *disc,
					       const gchar *path)
{
	BraseroJolietMakeList callback_data;

	if (!disc->priv->joliet_non_compliant)
		return;

	/* remove the children of the path if that's a directory */
	callback_data.list = NULL;
	callback_data.path = path;
	callback_data.len = strlen (path);

	g_hash_table_foreach (disc->priv->joliet_non_compliant,
			      (GHFunc) brasero_data_disc_joliet_incompat_find_cb,
			      &callback_data);

	if (callback_data.list) {
		GSList *iter;

		for (iter = callback_data.list; iter; iter = iter->next) {
			gchar *key;
			GSList *list;

			key = iter->data;

			list = g_hash_table_lookup (disc->priv->joliet_non_compliant, key);
			g_slist_foreach (list, (GFunc) g_free, NULL);
			g_slist_free (list);

			g_hash_table_remove (disc->priv->joliet_non_compliant, key);
		}
		g_slist_free (callback_data.list);

		if (!g_hash_table_size (disc->priv->joliet_non_compliant)) {
			g_hash_table_destroy (disc->priv->joliet_non_compliant);
			disc->priv->joliet_non_compliant = NULL;
			return;
		}
	}

	/* remove the exact path if it is a joliet non compliant file */
	if (brasero_data_disc_joliet_incompat_free (disc, path)
	&& !g_hash_table_size (disc->priv->joliet_non_compliant)) {
		g_hash_table_destroy (disc->priv->joliet_non_compliant);
		disc->priv->joliet_non_compliant = NULL;
	}
}

static void
brasero_data_disc_joliet_incompat_move (BraseroDataDisc *disc,
					const gchar *old_path,
					const gchar *new_path)
{
	gchar *name;
	BraseroJolietMakeList callback_data;

	if (!disc->priv->joliet_non_compliant)
		return;

	/* move the children of the path if that's a directory */
	callback_data.list = NULL;
	callback_data.path = old_path;
	callback_data.len = strlen (old_path);

	g_hash_table_foreach (disc->priv->joliet_non_compliant,
			      (GHFunc) brasero_data_disc_joliet_incompat_find_cb,
			      &callback_data);

	if (callback_data.list) {
		GSList *iter;

		for (iter = callback_data.list; iter; iter = iter->next) {
			gchar *new_key;
			gchar *old_key;
			GSList *list;

			old_key = iter->data;

			/* create new path */
			new_key = g_strconcat (new_path,
					       old_key + strlen (old_path),
					       NULL);

			/* retrieve the data associated with old key */
			list = g_hash_table_lookup (disc->priv->joliet_non_compliant, old_key);

			/* remove it */
			g_hash_table_remove (disc->priv->joliet_non_compliant, old_key);

			/* reinsert it with new key */
			g_hash_table_insert (disc->priv->joliet_non_compliant,
					     new_key,
					     list);
		}

		g_slist_free (callback_data.list);
	}

	/* move the exact path if that's a joliet non compliant file.
	 * First, see if that's possible to remove it and then re-insert it */
	if (!brasero_data_disc_joliet_incompat_free (disc, old_path))
		return;

	/* see if the new path should be inserted (the name could be changed) */
	name = g_path_get_basename (new_path);
	if (strlen (name) <= 64) {
		/* no need to re-insert the path so if hash is empty free it */
		if (!g_hash_table_size (disc->priv->joliet_non_compliant)) {
			g_hash_table_destroy (disc->priv->joliet_non_compliant);
			disc->priv->joliet_non_compliant = NULL;
		}
		g_free (name);
	}
	else {
		gchar *key;
		GSList *list;

		key = brasero_data_disc_joliet_get_key (new_path, NULL, NULL);

		list = g_hash_table_lookup (disc->priv->joliet_non_compliant, key);
		list = g_slist_prepend (list, name);
		g_hash_table_insert (disc->priv->joliet_non_compliant, key, list);
	}
}

static void
brasero_data_disc_joliet_incompat_restore (BraseroDataDisc *disc,
					   BraseroFile *file,
					   const gchar *path)
{
	gint len;
	GSList *iter;

	len = strlen (file->uri);
	for (iter = disc->priv->joliet_incompat_uris; iter; iter = iter->next) {
		gchar *unescaped;
		gchar *newpath;
		GSList *list;
		gchar *name;
		gchar *uri;
		gchar *key;

		uri = iter->data;

		/* see if that's a child of the directory */
		if (strncmp (file->uri, uri, len)
		|| *(uri + len) != G_DIR_SEPARATOR)
			continue;

		/* add the path */
		unescaped = gnome_vfs_unescape_string_for_display (uri + len);
		newpath = g_strconcat (path, unescaped, NULL);
		g_free (unescaped);

		key = brasero_data_disc_joliet_get_key (newpath, &name, NULL);
		list = g_hash_table_lookup (disc->priv->joliet_non_compliant, key);
		list = g_slist_prepend (list, name);

		g_hash_table_insert (disc->priv->joliet_non_compliant, key, list);
		g_free (newpath);
	}
}

static void
brasero_data_disc_joliet_incompat_restore_children (BraseroDataDisc *disc,
						    BraseroFile *file,
						    GSList *paths)
{
	GSList *iter;

	for (iter = paths; iter; iter = iter->next) {
		gchar *path;

		path = iter->data;
		brasero_data_disc_joliet_incompat_restore (disc, file, path);
	}
}

static void
brasero_data_disc_joliet_incompat_add_uri (BraseroDataDisc *disc,
					   const gchar *uri)
{
	GSList *node;

	if (!uri)
		return;

	/* add uri to remove it later. That's also useful when the uri is added
	 * a second time later in the tree (see restore) */
	node = g_slist_find_custom (disc->priv->joliet_incompat_uris,
				    uri,
				    (GCompareFunc) strcmp);
	if (!node)
		disc->priv->joliet_incompat_uris = g_slist_prepend (disc->priv->joliet_incompat_uris,
								    g_strdup (uri));
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
_foreach_remove_grafts_cb (const gchar *uri,
			   GSList *grafts,
			   BraseroDataDisc *disc)
{
	g_slist_foreach (grafts, (GFunc) g_free, NULL);
	g_slist_free (grafts);
	return TRUE;
}

static void
_foreach_remove_created_dirs_cb (gchar *graft, 
				 const gchar *uri,
				 BraseroDataDisc *disc)
{
	if (uri == BRASERO_CREATED_DIR
	||  uri == BRASERO_IMPORTED_FILE)
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
	if (disc->priv->joliet_incompat_uris) {
		g_slist_foreach (disc->priv->joliet_incompat_uris,
				 (GFunc) g_free,
				 NULL);
		g_slist_free (disc->priv->joliet_incompat_uris);
		disc->priv->joliet_incompat_uris = NULL;
	}

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
	if (disc->priv->vfs)
		brasero_vfs_cancel (disc->priv->vfs, disc);

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

	/* free imported session files */
	brasero_volume_file_free (disc->priv->session);
	disc->priv->session = NULL;

	if (disc->priv->libnotify_id) {
		g_source_remove (disc->priv->libnotify_id);
		disc->priv->libnotify_id = 0;
	}

	g_slist_foreach (disc->priv->libnotify,
			(GFunc) brasero_data_disc_notification_free,
			 NULL);
	g_slist_free (disc->priv->libnotify);
	disc->priv->libnotify = NULL;
}

static void
brasero_data_disc_reset_real (BraseroDataDisc *disc)
{
	GtkAction *action;

	brasero_data_disc_clean (disc);

	disc->priv->activity_counter = 1;
	brasero_data_disc_decrease_activity_counter (disc);

	if (disc->priv->disc_group) {
		action = gtk_action_group_get_action (GTK_ACTION_GROUP (disc->priv->disc_group), "ImportSession");
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), FALSE);
	}

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
				  gchar *uri,
				  BraseroFilterStatus status)
{
	gchar *parenturi;
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
		if (filter_notify) {
			GtkWidget *widget;

			widget = gtk_ui_manager_get_widget (disc->priv->manager,
							    "/Toolbar/DiscButtonPlaceholder/FileFilter");
			brasero_data_disc_notify_user (disc,
						       _("Some files were filtered:"),
						       _("click here to see the list."),
						       widget);
		}
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
brasero_data_disc_unreadable_free (BraseroDataDisc *disc, const gchar *uri)
{
	BraseroFilterStatus status;

	if (!disc->priv->unreadable)
		return 0;

	status = GPOINTER_TO_INT (g_hash_table_lookup (disc->priv->unreadable, uri));
	g_hash_table_remove (disc->priv->unreadable, uri);
	if (!g_hash_table_size (disc->priv->unreadable)) {
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

	if (!disc->priv->restored)
		disc->priv->restored = g_hash_table_new (g_str_hash, g_str_equal);

	if (!status)
		status = brasero_data_disc_unreadable_free (disc, uri);

	if ((file = g_hash_table_lookup (disc->priv->files, uri))
	||  (file = g_hash_table_lookup (disc->priv->dirs, uri)))
		key = file->uri;
	else
		key = g_strdup (uri);

	g_hash_table_insert (disc->priv->restored,
			     key,
			     GINT_TO_POINTER (status));
}

static BraseroFilterStatus
brasero_data_disc_restored_free (BraseroDataDisc *disc,
				 const gchar *uri)
{
	gpointer key = NULL;
	gpointer status;

	if (!disc->priv->restored)
		return 0;

	g_hash_table_lookup_extended (disc->priv->restored,
				      uri,
				      &key,
				      &status);
	g_hash_table_remove (disc->priv->restored, uri);

	if (!g_hash_table_lookup (disc->priv->dirs, uri)
	&&  !g_hash_table_lookup (disc->priv->files, uri))
		g_free (key);

	if (!g_hash_table_size (disc->priv->restored)) {
		g_hash_table_destroy (disc->priv->restored);
		disc->priv->restored = NULL;
	}

	return GPOINTER_TO_INT (status);
}

/****************************** filtered dialog ********************************/
static gboolean
brasero_data_disc_unreadable_dialog (BraseroDataDisc *disc,
				     const gchar *uri,
				     GnomeVFSResult result,
				     gboolean isdir)
{
	gchar *name;
	guint answer;
	GtkWidget *dialog;
	gchar *escaped_name;
	gchar *message_disc;
	GtkWidget *toplevel;
    	GnomeVFSURI *vfsuri;

    	vfsuri = gnome_vfs_uri_new (uri);
    	escaped_name = gnome_vfs_uri_extract_short_path_name (vfsuri);
    	gnome_vfs_uri_unref (vfsuri);

	name = gnome_vfs_unescape_string_for_display (escaped_name);
	g_free (escaped_name);

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

static void
brasero_data_disc_restore_unreadable_destroy (GObject *object,
					      gpointer callback_data,
					      gboolean cancelled)
{
	BraseroDataDisc *disc = BRASERO_DATA_DISC (object);
	GSList *list = callback_data;

	brasero_data_disc_reference_free_list (disc, list);
	brasero_data_disc_decrease_activity_counter (disc);
}

static gboolean
brasero_data_disc_restore_unreadable (BraseroDataDisc *disc,
				      const gchar *uri,
				      GnomeVFSResult result,
				      GnomeVFSFileInfo *info,
				      gpointer user_data)
{
	GSList *references = user_data;
	GSList *paths;
	gchar *path;

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

	/* add it to joliet incompatible list if need be */
	paths = brasero_data_disc_reference_get_list (disc, references, FALSE);
	if (strlen (info->name) > 64) {
		brasero_data_disc_joliet_incompat_add_uri (disc, uri);
		brasero_data_disc_joliet_incompat_add_paths (disc, paths);
	}

	/* now let's see the tree */
	for (; paths; paths = g_slist_remove (paths, path)) {
		path = paths->data;
		brasero_data_disc_tree_set_path_from_info (disc, path, NULL, info);
		g_free (path);
	}

	return TRUE;
}

static void
brasero_data_disc_restore_unreadable_cb (BraseroVFS *self,
					 GObject *owner,
					 GnomeVFSResult result,
					 const gchar *uri,
					 GnomeVFSFileInfo *info,
					 gpointer callback_data)
{
	BraseroDataDisc *disc = BRASERO_DATA_DISC (owner);
	GSList *references = callback_data;
	gboolean success;

	success = brasero_data_disc_restore_unreadable (disc,
							uri,
							result,
							info,
							references);

	if (!success) {
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
		g_warning ("ERROR : file \"%s\" couldn't be restored : %s\n",
			   uri,
			   gnome_vfs_result_to_string (result));

		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	}
	else
		brasero_data_disc_selection_changed (disc, TRUE);
}

static void
brasero_data_disc_filtered_restore (BraseroDataDisc *disc,
				    GSList *restored)
{
	for (; restored; restored = restored->next) {
		GSList *references;
		GSList *paths;
		GSList *iter;
		gchar *uri;

		uri = restored->data;

		/* we filter out those that haven't changed */
		if (disc->priv->restored
		&&  g_hash_table_lookup (disc->priv->restored, uri))
			continue;

		references = NULL;
		paths = brasero_data_disc_uri_to_paths (disc, uri, TRUE);
		for (iter = paths; iter; iter = iter->next) {
			BraseroDataDiscReference ref;
			GtkTreePath *treepath;
			gchar *path_uri;
			gchar *path;

			path = iter->data;
			path_uri = g_hash_table_lookup (disc->priv->paths, path);
			if (path_uri) {
				gchar *graft;
				gchar *parent;
		
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

			/* update parent directory */
			if (brasero_data_disc_disc_path_to_tree_path (disc,
								      path,
								      &treepath,
								      NULL)) {
				brasero_data_disc_tree_update_parent (disc, treepath);
				gtk_tree_path_free (treepath);
			}

			ref = brasero_data_disc_reference_new (disc, path);
			references = g_slist_prepend (references, GINT_TO_POINTER (ref));
		}
		g_slist_foreach (paths, (GFunc) g_free, NULL);
		g_slist_free (paths);

		if (references) {
			gboolean success;
			GList *uris;

			brasero_data_disc_restored_new (disc, uri, 0);

			if (!disc->priv->restore_data)
				disc->priv->restore_data = brasero_vfs_register_data_type (disc->priv->vfs,
											   G_OBJECT (disc),
											   G_CALLBACK (brasero_data_disc_restore_unreadable_cb),
											   brasero_data_disc_restore_unreadable_destroy);

			/* NOTE: uri in uris are destroyed by calling function */
			uris = g_list_prepend (NULL, uri);
			success = brasero_vfs_get_info (disc->priv->vfs,
							uris,
							TRUE,
							GNOME_VFS_FILE_INFO_GET_ACCESS_RIGHTS |
							GNOME_VFS_FILE_INFO_GET_MIME_TYPE |
							GNOME_VFS_FILE_INFO_FORCE_SLOW_MIME_TYPE,
							disc->priv->restore_data,
							references);
			g_list_free (uris);

			if (success)
				brasero_data_disc_increase_activity_counter (disc);
		}
	}
}

static void
_foreach_add_unreadable (gchar *uri,
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

	disc->priv->filter_dialog = brasero_filtered_dialog_new ();
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
					    NULL);
	gtk_widget_destroy (disc->priv->filter_dialog);
	disc->priv->filter_dialog = NULL;

	brasero_data_disc_filtered_restore (disc, restored);
	g_slist_foreach (restored, (GFunc) g_free, NULL);
	g_slist_free (restored);
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
		g_hash_table_foreach_remove (disc->priv->restored,
					     (GHRFunc) _foreach_restored_remove,
					     disc);

		if (!g_hash_table_size (disc->priv->restored)) {
			g_hash_table_destroy (disc->priv->restored);
			disc->priv->restored = NULL;
		}
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
		g_hash_table_remove (disc->priv->references, dir->uri);
		if (!g_hash_table_size (disc->priv->references)) {
			g_hash_table_destroy (disc->priv->references);
			disc->priv->references = NULL;
		}

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
	gint len;
	BraseroFile *dir;
	GSList *dirs;
	GList *files;
	gchar *graft;
	BraseroDataDisc *disc;
};
typedef struct _BraseroRemoveChildrenData BraseroRemoveChildrenData;

static void
_foreach_remove_children_dirs_cb (const gchar *uri,
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
	gchar *path;

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
brasero_data_disc_remove_children_destroy_cb (GObject *object,
					      gpointer callback_data,
					      gboolean cancelled)
{
	gpointer uri = callback_data;
	BraseroDataDisc *disc = BRASERO_DATA_DISC (object);

	brasero_data_disc_decrease_activity_counter (disc);
	g_free (uri);
}

static void
brasero_data_disc_remove_children_async_cb (BraseroVFS *self,
					    GObject *owner,
					    GnomeVFSResult result,
					    const gchar *uri,
					    GnomeVFSFileInfo *info,
					    gpointer callback_data)
{
	BraseroDataDisc *disc = BRASERO_DATA_DISC (owner);
	gchar *uri_dir = callback_data;
	BraseroFile *dir;
	gint64 sectors;

	dir = g_hash_table_lookup (disc->priv->dirs, uri_dir);
	if (!dir && dir->sectors < 0)
		return;

	/* NOTE: we don't care if it is still excluded or not:
	 * if it's no longer excluded it means that one of his parents
	 * has been added again and so there is a function that is going
	 * to add or has added its size already */
	dir = g_hash_table_lookup (disc->priv->dirs, uri);

	if (!dir || dir->sectors < 0)
		return;

	if (result == GNOME_VFS_ERROR_NOT_FOUND) {
		brasero_data_disc_add_rescan (disc, dir);
		return;
	}

	/* There shouldn't be any symlink so no need to check for loop */
	if (result != GNOME_VFS_OK) {
		/* we don't remove it from excluded in case it appears again */
		brasero_data_disc_add_rescan (disc, dir);
		brasero_data_disc_unreadable_new (disc,
						  g_strdup (uri),
						  BRASERO_FILTER_UNREADABLE);
		return;
	}

	/* we update the parent directory */
	sectors = GET_SIZE_IN_SECTORS (info->size);
	dir->sectors -= sectors;
	brasero_data_disc_size_changed (disc, sectors * (-1));

	/* Free callback_data */
	return;
}

static void
_foreach_remove_children_files_cb (gchar *uri,
				   GSList *excluding,
				   BraseroRemoveChildrenData *data)
{
	BraseroFile *dir;
	gint excluding_num;
	gint grafts_num;
	gchar *parent;
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

	parent = g_path_get_dirname (uri);
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
		data->files = g_list_prepend (data->files, g_strdup (uri));
}

static void
brasero_data_disc_remove_children (BraseroDataDisc *disc,
				   BraseroFile *dir,
				   const gchar *graft)
{
	BraseroRemoveChildrenData callback_data;
	BraseroDiscResult result;
	BraseroFile *file;
	GSList *iter;
	gchar *uri;

	if (!disc->priv->excluded)
		return;

	/* we remove all children from dirs hash table */
	callback_data.dir = dir;
	callback_data.len = strlen (dir->uri);
	callback_data.graft = (gchar *) graft;
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

	if (!disc->priv->restore_data)
		disc->priv->restore_data = brasero_vfs_register_data_type (disc->priv->vfs,
									   G_OBJECT (disc),
									   G_CALLBACK (brasero_data_disc_remove_children_async_cb),
									   brasero_data_disc_remove_children_destroy_cb);

	/* NOTE: uri in uris are destroyed by calling function */
	uri = g_strdup (dir->uri);
	result = brasero_vfs_get_info (disc->priv->vfs,
				       callback_data.files,
				       TRUE,
				       GNOME_VFS_FILE_INFO_DEFAULT,
				       disc->priv->restore_data,
				       uri);

	g_list_foreach (callback_data.files, (GFunc) g_free, NULL);
	g_list_free (callback_data.files);

	if (result)
		brasero_data_disc_increase_activity_counter (disc);
	else
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
static const gchar *
brasero_data_disc_graft_get_real (BraseroDataDisc *disc,
				  const gchar *path)
{
	gchar *tmp;
	gchar *parent;
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

static gchar *
brasero_data_disc_graft_get (BraseroDataDisc *disc,
			     const gchar *path)
{
	return g_strdup (brasero_data_disc_graft_get_real (disc, path));
}

static gboolean
brasero_data_disc_graft_new (BraseroDataDisc *disc,
			     const gchar *uri,
			     const gchar *graft)
{
	gchar *realgraft;
	GSList *grafts;
	gchar *realuri;

	if (g_hash_table_lookup (disc->priv->paths, graft))
		return FALSE;

	realgraft = g_strdup (graft);

	if (uri == BRASERO_IMPORTED_FILE)
		realuri = BRASERO_IMPORTED_FILE;
	else if (uri) {
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

	if (realuri == BRASERO_CREATED_DIR
	||  realuri == BRASERO_IMPORTED_FILE)
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
				  const gchar *uri,
				  GSList *grafts)
{
	gchar *graft;
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
	gchar *graft;
	GSList *list;
};
typedef struct _BraseroRemoveGraftPointersData BraseroRemoveGraftPointersData;

static void
_foreach_remove_graft_pointers_cb (gchar *key,
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
					const gchar *graft)
{
	gchar *uri;
	GSList *iter;
	BraseroRemoveGraftPointersData callback_data;

	if (!disc->priv->excluded)
		return;

	/* we need to remove any pointer to the graft point from the excluded hash */
	callback_data.graft = (gchar *) graft;
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
				const gchar *path)
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

	if (uri == BRASERO_IMPORTED_FILE)
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
				    const gchar *uri)
{
	GSList *grafts;
	gchar *graft;

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
_foreach_graft_changed_cb (gchar *key,
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
		gchar *newgraft;

		newgraft = g_strconcat (data->newgraft,
					graft + data->len,
					NULL);

		if (uri != BRASERO_CREATED_DIR
		&&  uri != BRASERO_IMPORTED_FILE) {
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
				       const gchar *oldpath,
				       const gchar *newpath)
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
_foreach_unreference_grafted_cb (gchar *graft,
				 gchar *uri,
				 BraseroRemoveGraftedData *data)
{
	GSList *iter;
	gchar *path;
	gint len;

	if (graft == BRASERO_CREATED_DIR
	||  graft == BRASERO_IMPORTED_FILE)
		return FALSE;

	for (iter = data->paths; iter; iter = iter->next) {
		path = iter->data;
		len = strlen (path);

		if (!strncmp (path, graft, len) && *(graft + len) == G_DIR_SEPARATOR) {
			BraseroFile *file;
			GSList *grafts;

			if (uri == BRASERO_CREATED_DIR
			||  uri == BRASERO_IMPORTED_FILE) {
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

	if (uri != BRASERO_CREATED_DIR
	&&  uri != BRASERO_IMPORTED_FILE) {
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
		gchar *unescaped;

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

			/* path are always unescaped */
			unescaped = gnome_vfs_unescape_string_for_display (uri + strlen (parent));
			path = g_strconcat (graft,
					    unescaped,
					    NULL);
			g_free (unescaped);
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

static gchar *
brasero_data_disc_path_to_uri (BraseroDataDisc *disc,
			       const gchar *path)
{
	gchar *escaped_graft;
	gchar *escaped_path;
	const gchar *graft;
	gchar *graft_uri;
	gchar *retval;

	graft = brasero_data_disc_graft_get_real (disc, path);
	if (!graft)
		return NULL;

	graft_uri = g_hash_table_lookup (disc->priv->paths, graft);

	if (graft_uri == BRASERO_CREATED_DIR)
		return NULL;

	if (graft_uri == BRASERO_IMPORTED_FILE)
		return BRASERO_IMPORTED_FILE;

	escaped_graft = gnome_vfs_escape_path_string (graft);
	escaped_path = gnome_vfs_escape_path_string (path);
	retval = g_strconcat (graft_uri, escaped_path + strlen (escaped_graft), NULL);
	g_free (escaped_graft);
	g_free (escaped_path);

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
	gchar *path;
	gchar *name;
	gint nb;

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
		gint explored;
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

		if (!isdir || explored < ROW_EXPLORED) {
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
	brasero_data_disc_tree_new_empty_folder_real (disc,
						      path,
						      ROW_EXPLORED,
						      TRUE);
	g_free (path);

	brasero_data_disc_selection_changed (disc, TRUE);
}

/************************************ files excluded ***************************/
#define BRASERO_URI_EXCLUDED_FROM_GRAFT(disc, graft, uri)	\
	(disc->priv->excluded && g_slist_find (g_hash_table_lookup (disc->priv->excluded, uri), graft) != NULL)

#define BRASERO_URI_EXCLUDED_FROM_PATH(disc, path, uri)		\
	BRASERO_URI_EXCLUDED_FROM_GRAFT (disc, brasero_data_disc_graft_get_real (disc, path), uri)

/* this is done in order:
 * - to minimize memory usage as much as possible ??? if we've got just one excluded ....
 * - to help building of lists of excluded files
 * downside: we need to be careful when we remove a file from the dirs 
 * hash or from the file hash and check that it is not in the excluded hash */
static void
brasero_data_disc_exclude_uri (BraseroDataDisc *disc,
			       const gchar *path,
			       const gchar *uri)
{
	BraseroFile *file;
	gpointer key = NULL;
	gpointer graft = NULL;
	gpointer excluding = NULL;

	/* make sure the path given is actually a graft */
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
			       const gchar *path,
			       const gchar *uri)
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
	gint excluding_num;
	BraseroFile *dir;
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
brasero_data_disc_expose_grafted_destroy_cb (GObject *object,
					     gpointer callback_data,
					     gboolean cancelled)
{
	BraseroDataDiscReference reference = GPOINTER_TO_INT (callback_data);
	BraseroDataDisc *disc = BRASERO_DATA_DISC (object);

	brasero_data_disc_reference_free (disc, reference);
	brasero_data_disc_decrease_activity_counter (disc);
}

static void
brasero_data_disc_expose_grafted_cb (BraseroVFS *self,
				     GObject *owner,
				     GnomeVFSResult result,
				     const gchar *uri,
				     GnomeVFSFileInfo *info,
				     gpointer callback_data)
{
	BraseroDataDiscReference reference = GPOINTER_TO_INT (callback_data);
	BraseroDataDisc *disc = BRASERO_DATA_DISC (owner);
	gchar *path;

	path = brasero_data_disc_reference_get (disc, reference);
	if (!path)
		return;

	/* NOTE: we don't need to check if path still corresponds to 
	 * a graft point since the only way for a graft point to be
	 * removed is if notification announced a removal or if the
	 * user explicitly removed the graft in the tree. either way
	 * the references hash table will be updated */
	if (result != GNOME_VFS_OK) {
		brasero_data_disc_remove_uri (disc, uri, TRUE);
		if (result != GNOME_VFS_ERROR_NOT_FOUND)
			brasero_data_disc_unreadable_new (disc,
							  g_strdup (uri),
							  BRASERO_FILTER_UNREADABLE);
		g_free (path);
		return;
	}

	brasero_data_disc_tree_set_path_from_info (disc,
						   path,
						   NULL,
						   info);
	g_free (path);
}

static void
brasero_data_disc_expose_grafted (BraseroDataDisc *disc,
				  GSList *grafts)
{
	gchar *uri;
	const gchar *path;
	GSList *created = NULL;

	if (!disc->priv->expose_grafted)
		disc->priv->expose_grafted = brasero_vfs_register_data_type (disc->priv->vfs,
									     G_OBJECT (disc),
									     G_CALLBACK (brasero_data_disc_expose_grafted_cb),
									     brasero_data_disc_expose_grafted_destroy_cb);

	for (; grafts; grafts = grafts->next) {
		gboolean result;
		GList *uris = NULL;
		BraseroDataDiscReference reference;

		path = grafts->data;

		uri = g_hash_table_lookup (disc->priv->paths, path);
		if (uri == BRASERO_CREATED_DIR
		||  uri == BRASERO_IMPORTED_FILE) {
			created = g_slist_prepend (created, (gchar *) path);
			continue;
		}

		if (!uri)
			continue;

		brasero_data_disc_tree_new_loading_row (disc, path);

		reference = brasero_data_disc_reference_new (disc, path);

		uris = g_list_prepend (uris, uri);
		result = brasero_vfs_get_info (disc->priv->vfs,
					       uris,
					       TRUE,
					       GNOME_VFS_FILE_INFO_GET_MIME_TYPE|
					       GNOME_VFS_FILE_INFO_FORCE_SLOW_MIME_TYPE,
					       disc->priv->expose_grafted,
					       GINT_TO_POINTER (reference));
		g_list_free (uris);

		if (result)
			brasero_data_disc_increase_activity_counter (disc);
	}

	for (; created; created = g_slist_remove (created, path)) {
		path = created->data;
		brasero_data_disc_tree_new_empty_folder_real (disc,
							      path,
							      ROW_NOT_EXPLORED,
							      FALSE);
	}
}

static void
brasero_data_disc_tree_new_imported_session_file (BraseroDataDisc *disc,
						  BraseroVolFile *file,
						  GtkTreeIter *row)
{
	GtkTreeModel *model;
	gchar *markup;
	gint64 dsize;
	gchar *size;

	if (!file)
		return;

	model = disc->priv->model;

	if (!file->isdir) {
		/* we don't display file sizes in sectors and dsize is used to
		 * compare the files when sorting them by sizes */
		size = brasero_utils_get_size_string (file->specific.file.size_bytes, TRUE, TRUE);
		dsize = file->specific.file.size_bytes;
	}
	else {
		size = g_strdup (_("(loading ...)"));
		dsize = 0;
	}

	markup = g_strdup_printf ("<span foreground='grey50'>%s</span>", file->name);
	gtk_tree_store_set (GTK_TREE_STORE (model), row,
			    ICON_COL, file->isdir ? "folder-visiting":"media-cdrom",
			    NAME_COL, BRASERO_VOLUME_FILE_NAME (file),
			    STYLE_COL, PANGO_STYLE_ITALIC,
			    MIME_COL, _("Disc file"),
			    DSIZE_COL, (gint64) 0,
			    SIZE_COL, size,
			    ISDIR_COL, file->isdir ? TRUE:FALSE,
			    ROW_TYPE_COL, ROW_SESSION,
			    ROW_STATUS_COL, file->isdir ? ROW_NOT_EXPLORED:ROW_EXPANDED,
			    -1);
	g_free (markup);
	g_free (size);
}

static void
brasero_data_disc_expose_imported_session_file (BraseroDataDisc *disc,
						BraseroVolFile *file,
						const gchar *path)
{
	GtkTreePath *treepath;
	GtkTreeModel *model;
	GtkTreeIter parent;
	GtkTreeIter row;
	GList *iter;

	if (!file->isdir)
		return;

	if (!brasero_data_disc_disc_path_to_tree_path (disc,
						       path,
						       &treepath,
						       NULL))
		return;

	model = disc->priv->model;
	gtk_tree_model_get_iter (model, &parent, treepath);
	gtk_tree_path_free (treepath);

	for (iter = file->specific.dir.children; iter; iter = iter->next) {
		file = iter->data;

		gtk_tree_store_append (GTK_TREE_STORE (model), &row, &parent);
		brasero_data_disc_tree_new_imported_session_file (disc,
								  file,
								  &row);
	}

	brasero_data_disc_tree_update_directory_real (disc, &parent);
	gtk_tree_store_set (GTK_TREE_STORE (model), &parent,
			    ROW_STATUS_COL, ROW_EXPLORED,
			    -1);
}

static void
brasero_data_disc_directory_entry_free (gpointer user_data)
{
	BraseroDirectoryEntry *entry = user_data;

	gnome_vfs_file_info_unref (entry->info);
	g_free (entry->uri);
	g_free (entry);
}

static void
brasero_data_disc_directory_contents_free (BraseroDataDisc *disc,
					   BraseroDirectoryContents *contents)
{
	brasero_data_disc_decrease_activity_counter (disc);

	g_slist_foreach (contents->entries, (GFunc) brasero_data_disc_directory_entry_free, NULL);
	g_slist_free (contents->entries);
	g_free (contents->uri);
	g_free (contents);
}

static void
brasero_data_disc_directory_entry_error (BraseroDataDisc *disc,
					 const gchar *uri,
					 BraseroFilterStatus status)
{
	BraseroFile *file;
	gchar *parent;

	if (disc->priv->unreadable
	&&  g_hash_table_lookup (disc->priv->unreadable, uri))
		return;

	/* remove it wherever it is */
	if ((file = g_hash_table_lookup (disc->priv->dirs, uri)))
		brasero_data_disc_directory_remove_from_tree (disc, file);
	else if ((file = g_hash_table_lookup (disc->priv->files, uri)))
		brasero_data_disc_file_remove_from_tree (disc, file);
	else if (disc->priv->symlinks
	     &&  g_hash_table_lookup (disc->priv->symlinks, uri)) {
		g_hash_table_remove (disc->priv->symlinks, uri);

		if (!g_hash_table_size (disc->priv->symlinks)) {
			g_hash_table_destroy (disc->priv->symlinks);
			disc->priv->symlinks = NULL;
		}
	}
	else if (!brasero_data_disc_is_excluded (disc, uri, NULL))
		brasero_data_disc_remove_uri_from_tree (disc, uri, TRUE);

	/* insert it in the unreadable hash table if need be
	 * (if it has a parent directory) */
	parent = g_path_get_dirname (uri);
	file = g_hash_table_lookup (disc->priv->dirs, parent);
	g_free (parent);

	if (file
	&&  file >= 0
	&&  status != BRASERO_FILTER_UNKNOWN)
		brasero_data_disc_unreadable_new (disc,
						  g_strdup (uri),
						  status);
}

static gboolean
brasero_data_disc_expose_path_real (BraseroDataDisc *disc)
{
	gchar *path;
	GSList *iter;
	GSList *paths;
	GSList *entries;
	GSList *references;
	GtkTreePath *treepath;
	GSList *treepaths = NULL;
	BraseroDirectoryContents *contents;

next:
	if (!disc->priv->expose) {
		disc->priv->expose_id = 0;
		return FALSE;
	}

	contents = disc->priv->expose->data;
	disc->priv->expose = g_slist_remove (disc->priv->expose, contents);

	/* convert all references to paths ignoring the invalid ones */
	if (disc->priv->references
	&& (references = g_hash_table_lookup (disc->priv->references, contents->uri))) {
		paths = brasero_data_disc_reference_get_list (disc,
							      references,
							      TRUE);
	}
	else
		paths = NULL;

	if (!paths) {
		brasero_data_disc_directory_contents_free (disc, contents);
		goto next;
	}

	g_hash_table_remove (disc->priv->references, contents->uri);
	if (!g_hash_table_size (disc->priv->references)) {
		g_hash_table_destroy (disc->priv->references);
		disc->priv->references = NULL;
	}

	/* for every path we look for the corresponding tree paths in treeview */
	for (iter = paths; iter; iter = iter->next) {
		gchar *path;

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

	for (entries = contents->entries; entries; entries = entries->next) {
		BraseroDirectoryEntry *entry;
		GnomeVFSFileInfo *info;
		GSList *treepath_iter;
		GSList *path_iter;
		gchar *uri;

		entry = entries->data;
		info = entry->info;
		uri = entry->uri;

		/* see if this file is not in unreadable. it could happen for files we
		 * couldn't read.
		 * but in the latter case if we can read them again inotify will tell
		 * us and we'll add them later so for the moment ignore all unreadable */
		if (disc->priv->unreadable
		&&  g_hash_table_lookup (disc->priv->unreadable, uri))
			continue;

		/* symlinks should be handled by this function but by the one 
		 * handling the graft points */
		if (disc->priv->symlinks
		&& g_hash_table_lookup (disc->priv->symlinks, uri))
			continue;

		path_iter = paths;
		treepath_iter = treepaths;
		for (; path_iter; path_iter = path_iter->next, treepath_iter = treepath_iter->next) {
			gchar *parent;
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
	}

	brasero_data_disc_directory_contents_free (disc, contents);

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

	return TRUE;
}

static void
brasero_data_disc_expose_end (GObject *object,
			      gpointer callback_data,
			      gboolean cancelled)
{
	BraseroDataDisc *disc = BRASERO_DATA_DISC (object);
	BraseroDirectoryContents *contents = callback_data;

	BRASERO_ADD_TO_EXPOSE_QUEUE (disc, contents);
}

/* this used to be done async */
static void
brasero_data_disc_expose_result (BraseroVFS *self,
				 GObject *owner,
				 GnomeVFSResult result,
				 const gchar *uri,
				 GnomeVFSFileInfo *info,
				 gpointer callback_data)
{
	BraseroDirectoryEntry *entry;
	BraseroDataDisc *disc = BRASERO_DATA_DISC (owner);
	BraseroDirectoryContents *contents = callback_data;

	if (result != GNOME_VFS_OK) {
		brasero_data_disc_directory_entry_error (disc,
							 uri,
							 BRASERO_FILTER_UNREADABLE);

		g_warning ("Cannot open %s : %s\n",
			   uri,
			   gnome_vfs_result_to_string (result));
		return;
	}

	/* We don't check the type to see if it is a symlink since load function
	 * replaces the information in the structure (including the type) by the
	 * information of the target when it meets a symlink. The symlinks are
	 * treated in a different function (the one dealing with graft points) 
	 */
	if (disc->priv->symlinks
	&&  g_hash_table_lookup (disc->priv->symlinks, uri))
		return;

	entry = g_new0 (BraseroDirectoryEntry, 1);
	entry->uri = g_strdup (uri);
	entry->info = info;
	gnome_vfs_file_info_ref (info);

	contents->entries = g_slist_prepend (contents->entries, entry);
	return;
}

static BraseroDiscResult
brasero_data_disc_expose_insert_path_real (BraseroDataDisc *disc,
					   const gchar *uri)
{
	gboolean result;
	BraseroDirectoryContents *contents;

	if (!disc->priv->expose_type)
		disc->priv->expose_type = brasero_vfs_register_data_type (disc->priv->vfs,
									  G_OBJECT (disc),
									  G_CALLBACK (brasero_data_disc_expose_result),
									  brasero_data_disc_expose_end);

	contents = g_new0 (BraseroDirectoryContents, 1);
	contents->uri = g_strdup (uri);

	result = brasero_vfs_load_directory (disc->priv->vfs,
					     uri,
					     GNOME_VFS_FILE_INFO_GET_MIME_TYPE|
					     GNOME_VFS_FILE_INFO_FORCE_SLOW_MIME_TYPE,
					     disc->priv->expose_type,
					     contents);

	if (!result) {
		brasero_data_disc_directory_contents_free (disc, contents);
		return BRASERO_DISC_ERROR_THREAD;
	}

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
	if (uri == BRASERO_IMPORTED_FILE) {
		BraseroVolFile *file;

		file = brasero_volume_file_from_path (path, disc->priv->session);
		if (file)
			brasero_data_disc_expose_imported_session_file (disc, file, path);
		return BRASERO_DISC_OK;
	}
	else if (!uri) {
		if (!g_hash_table_lookup (disc->priv->paths, path))
			return BRASERO_DISC_NOT_IN_TREE;

		return BRASERO_DISC_OK;
	}

	/* no need to check for dummies here as we only expose children */
	if (!(dir = g_hash_table_lookup (disc->priv->dirs, uri))) {
		g_free (uri);
		return BRASERO_DISC_NOT_IN_TREE;
	}

	/* add a reference */
	ref = brasero_data_disc_reference_new (disc, path);
	if (!disc->priv->references) {
		references = g_slist_prepend (NULL, GINT_TO_POINTER (ref));

		disc->priv->references = g_hash_table_new (g_str_hash,
							   g_str_equal);
		g_hash_table_insert (disc->priv->references,
				     dir->uri,
				     references);
	}
	else {
		references = g_hash_table_lookup (disc->priv->references, uri);
		references = g_slist_prepend (references, GINT_TO_POINTER (ref));
		g_hash_table_insert (disc->priv->references, dir->uri, references);
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
	GtkTreeModel *model;
	GtkTreeModel *sort;
	GtkTreeIter parent;
	gint type;

	model = disc->priv->model;
	sort = disc->priv->sort;

	/* only directories can be collapsed */
	gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (sort),
							&parent,
							sortparent);

	gtk_tree_model_get (model, &parent,
			    ROW_TYPE_COL, &type,
			    -1);

	gtk_tree_store_set (GTK_TREE_STORE (model), &parent,
			    ICON_COL, (type == ROW_SESSION) ? "folder-visiting":"folder",
			    -1);
}

static void
brasero_data_disc_row_expanded_cb (GtkTreeView *tree,
				    GtkTreeIter *sortparent,
				    GtkTreePath *sortpath,
				    BraseroDataDisc *disc)
{
	gchar *parent_discpath;
	GtkTreeModel *model;
	GtkTreeModel *sort;
	GtkTreeIter parent;
	GtkTreeIter child;
	GtkTreePath *path;
	gchar *discpath;
	gboolean isdir;
	gint explored;
	gchar *name;

	model = disc->priv->model;
	sort = disc->priv->sort;

	gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (sort),
							&parent,
							sortparent);

	gtk_tree_store_set (GTK_TREE_STORE (model), &parent,
			    ROW_STATUS_COL, ROW_EXPANDED,
			    ICON_COL, "folder-open",
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

		discpath = g_build_filename (parent_discpath, name, NULL);
		g_free (name);

		brasero_data_disc_expose_path (disc, discpath);
		gtk_tree_store_set (GTK_TREE_STORE (model), &child,
				    ROW_STATUS_COL, ROW_EXPLORING,
				    -1);

		g_free (discpath);
	} while (gtk_tree_model_iter_next (model, &child));

	g_free (parent_discpath);
}

/************************** files, directories handling ************************/
static gint64
brasero_data_disc_file_info (BraseroDataDisc *disc,
			     const gchar *uri,
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
		if (key != file->uri) {
			g_hash_table_replace (disc->priv->excluded, file->uri, value);
			g_free (key);
		}
	}

	if (disc->priv->restored
	&&  g_hash_table_lookup_extended (disc->priv->restored,
					  file->uri,
					  &key,
					  &value)) {
		g_hash_table_replace (disc->priv->restored, file->uri, value);
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
	/* That's mostly when loading a project */
	else if (!g_slist_find (disc->priv->loading, parent))
		parent->sectors -= sectors;

	/* because of above we only insert it at the end */
	g_hash_table_insert (disc->priv->files, file->uri, file);
	brasero_data_disc_obj_new (disc, file);

	return file;
}

struct _BraseroSymlinkChildrenData {
	BraseroDataDisc *disc;
	gint len;
	gchar *uri;
	GList *list;
};
typedef struct _BraseroSymlinkChildrenData BraseroSymlinkChildrenData;

static void
_foreach_lookup_symlink_children_cb (gchar *symlink,
				     const gchar *targets,
				     BraseroSymlinkChildrenData *data)
{
	/* symlink must be a child of uri 
	 * NOTE: can't be uri itself since we found it in dirs hash */
	if (strncmp (data->uri, symlink, data->len)
	|| (*(symlink + data->len) != G_DIR_SEPARATOR && *(symlink + data->len) != '\0'))
		return;

	data->list = g_list_prepend (data->list, symlink);
}

static GList *
brasero_data_disc_symlink_get_uri_children (BraseroDataDisc *disc,
					    const gchar *uri)
{
	BraseroSymlinkChildrenData callback_data;

	if (!disc->priv->symlinks)
		return NULL;

	callback_data.disc = disc;
	callback_data.uri = (gchar *) uri;
	callback_data.len = strlen (uri);
	callback_data.list = NULL;

	g_hash_table_foreach (disc->priv->symlinks,
			      (GHFunc) _foreach_lookup_symlink_children_cb,
			      &callback_data);

	return callback_data.list;
}

static gboolean
brasero_data_disc_symlink_is_recursive (BraseroDataDisc *disc,
					const gchar *uri,
					const gchar *target)
{
	gint len;
	gchar *symlink;
	gboolean result;
	GList *symlinks;

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

	for (; symlinks; symlinks = g_list_remove (symlinks, symlink)) {
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
	g_list_free (symlinks);
	return TRUE;
}

static gint
_find_name_in_info_list (gconstpointer a, gconstpointer b)
{
	const BraseroDirectoryEntry *entry = a;
	const gchar *name = b;
	return strcmp (entry->info->name, name);
}

#define BRASERO_FIND_NAME_IN_INFO_LIST(infos, name)	\
	(g_slist_find_custom (infos, name, (GCompareFunc) _find_name_in_info_list))

static gchar *
brasero_data_disc_get_unique_valid_utf8_name (const gchar *name,
					      GSList *entries)
{
	gchar *utf8_name;

	/* create a new valid name and make sure it doesn't
	 * already exist in the directory */
	utf8_name = brasero_utils_validate_utf8 (name);

	if (BRASERO_FIND_NAME_IN_INFO_LIST (entries, utf8_name)) {
		gchar *new_name;
		gint attempts;

		attempts = 0;
		new_name = g_strdup_printf ("%s%i", utf8_name, attempts);
		while (BRASERO_FIND_NAME_IN_INFO_LIST (entries, new_name)) {
			g_free (new_name);

			new_name = g_strdup_printf ("%s%i", utf8_name, attempts);
			attempts++;
		}

		g_free (utf8_name);
		utf8_name = new_name;
	}

	return utf8_name;
}

static gchar *
brasero_data_disc_get_unique_valid_utf8_path (BraseroDataDisc *disc,
					      const gchar *name,
					      const gchar *parent_path,
					      GnomeVFSFileInfo *info,
					      GSList *entries)
{
	gchar *utf8_name = NULL;
	gint attempts;
	gchar *path;

	if (!name)
		utf8_name = brasero_data_disc_get_unique_valid_utf8_name (info->name,
									  entries);
	
	path = g_build_path (G_DIR_SEPARATOR_S,
			     parent_path,
			     name ? name:utf8_name,
			     NULL);

	attempts = 0;
	while (g_hash_table_lookup (disc->priv->paths, path)) {
		gchar *new_name;

		new_name = g_strdup_printf ("%s%i", name ? name:utf8_name, attempts);

		/* check that this new path doesn't conflict with an existing
		 * grafted one (it'll be in paths). If it does recheck that the
		 * name doesn't exist in the directory (unlikely but who knows) */
		while (BRASERO_FIND_NAME_IN_INFO_LIST (entries, new_name)) {
			g_free (new_name);

			new_name = g_strdup_printf ("%s%i", name ? name:utf8_name, attempts);
			attempts++;
		}

		g_free (path);
		path = g_build_path (G_DIR_SEPARATOR_S,
				     parent_path,
				     new_name,
				     NULL);
		g_free (new_name);
		attempts ++;
	}

	if (!utf8_name)
		g_free (utf8_name);

	return path;
}

static GSList *
brasero_data_disc_symlink_new (BraseroDataDisc *disc,
			       const BraseroDirectoryContents *contents,
			       const BraseroDirectoryEntry *entry,
			       GSList *paths)
{
	BraseroFile *file = NULL;
	GnomeVFSFileInfo *info;
	GSList *next;
	GSList *iter;
	gchar *path;
	gchar *uri;

	/* we don't want paths overlapping already grafted paths. This might
	 * happen when we are loading a project or when we are notified of the
	 * creation of a new file i.e. rescanning a directory. Since all the
	 * paths of a symlink are graft (it's replaced by its target) they are
	 * easy to spot. */
	for (iter = paths; iter; iter = next) {
		next = iter->next;
		path = iter->data;

		/* see comment in function underneath about UTF8 names:
		 * since it's a symlink all already spotted paths ARE grafts and
		 * therefore have been validated. The new and yet unvalidated 
		 * won't be in the paths hash table. */
		if (g_hash_table_lookup (disc->priv->paths, path)) {
			paths = g_slist_remove (paths, path);
			g_free (path);
		}
	}

	info = entry->info;

	if (!paths)
		goto end;

	/* make sure the target was explored or is about to be explored  */
	if ((file = g_hash_table_lookup (disc->priv->dirs, info->symlink_name))
	&&   file->sectors >= 0) {
		if (!g_slist_find (disc->priv->loading, file)) {
			brasero_data_disc_restore_excluded_children (disc, file);
			brasero_data_disc_replace_symlink_children (disc, file, paths);
			brasero_data_disc_joliet_incompat_restore_children (disc, file, paths);
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
	
	uri = entry->uri;

	if (!disc->priv->symlinks)
		disc->priv->symlinks = g_hash_table_new_full (g_str_hash,
							      g_str_equal,
							      (GDestroyNotify) g_free,
							      (GDestroyNotify) g_free);

	if (info->symlink_name == NULL) 
		g_warning ("SYMLINK IS NULL %s\n", uri);

	if (!g_hash_table_lookup (disc->priv->symlinks, uri))
		g_hash_table_insert (disc->priv->symlinks,
				     g_strdup (uri),
				     g_strdup (info->symlink_name));

	/* add graft points to the target */
	for (iter = paths; iter; iter = iter->next) {
		path = iter->data;

		if (!g_utf8_validate (info->name, -1, NULL)) {
			gchar *dirname;

			dirname = g_path_get_dirname (path);
			path = brasero_data_disc_get_unique_valid_utf8_path (disc,
									     NULL,
									     dirname,
									     info,
									     contents->entries);
			g_free (dirname);
		}

		brasero_data_disc_graft_new (disc,
					     file->uri,
					     path);
	}

	return paths;
}

/* NOTE: it has a parent so if it is strictly excluded, it MUST have excluding */
#define EXPLORE_IS_NOT_STRICTLY_EXCLUDED(disc, uri)	\
	(!disc->priv->excluded	\
	 || !g_hash_table_lookup (disc->priv->excluded, uri) \
	 || !brasero_data_disc_is_excluded (disc, uri, NULL))

static void
brasero_data_disc_symlink_list_new (BraseroDataDisc *disc,
				    BraseroDirectoryContents *contents,
				    GSList *symlinks)
{
	GSList *iter;
	GSList *paths;
	GSList *grafts = NULL;

	for (iter = symlinks; iter; iter = iter->next) {
		BraseroDirectoryEntry *entry;
		GnomeVFSFileInfo *info;
		gchar *current;

		entry = iter->data;
		contents->entries = g_slist_remove (contents->entries, entry);

		info = entry->info;

		if (disc->priv->unreadable
		&&  g_hash_table_lookup (disc->priv->unreadable, info->symlink_name))
			continue;

		current = entry->uri;

		if (disc->priv->symlinks
		&&  g_hash_table_lookup (disc->priv->symlinks, current))
			continue;

		if (info->type == GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK
		|| !brasero_data_disc_is_readable (info)) {
			brasero_data_disc_unreadable_new (disc,
							  g_strdup (current),
							  BRASERO_FILTER_BROKEN_SYM);
			continue;
		}

		if (brasero_data_disc_symlink_is_recursive (disc,
							    current,
							    info->symlink_name)) {
			brasero_data_disc_unreadable_new (disc,
							  g_strdup (current),
							  BRASERO_FILTER_RECURSIVE_SYM);
			continue;
		}

		/* NOTE: the paths ARE utf8 valid since for a symlink they are
		 * all grafts and are excluded from their parent. So all paths
		 * have been validated before being added to graft hash table.
		 * If it's the first time that a symlink is spotted then all 
		 * paths are not UTF8 valid but they will be. */
		paths = brasero_data_disc_uri_to_paths (disc, current, TRUE);
		paths = brasero_data_disc_symlink_new (disc,
						       contents,
						       entry,
						       paths);

		grafts = g_slist_concat (grafts, paths);
	}

	if (grafts) {
		if (disc->priv->references
		&&  g_hash_table_lookup (disc->priv->references, contents->uri))
			brasero_data_disc_expose_grafted (disc, grafts);

		g_slist_foreach (grafts, (GFunc) g_free, NULL);
		g_slist_free (grafts);
	}

	/* we free the entries since we removed them from the contents->entries list */
	g_slist_foreach (symlinks, (GFunc) brasero_data_disc_directory_entry_free, NULL);
	g_slist_free (symlinks);
}

static void
brasero_data_disc_invalid_utf8_new (BraseroDataDisc *disc,
				    BraseroDirectoryContents *contents,
				    BraseroDirectoryEntry *entry,
				    GSList *parent_paths)
{
	gchar *uri;
	GSList *iter;
	gchar *utf8_name;
	GSList *grafts = NULL;
	GnomeVFSFileInfo *info = entry->info;

	/* create a new valid name and make sure it doesn't
	 * already exist in the directory */
	utf8_name = brasero_data_disc_get_unique_valid_utf8_name (info->name,
								  contents->entries);

	/* create URI (note: it is made from the invalid name) */
	uri = entry->uri;
	info = entry->info;

	for (iter = parent_paths; iter; iter = iter->next) {
		const gchar *graft;
		gchar *parent_path;
		gchar *name;
		gchar *path;

		parent_path = iter->data;
		graft = brasero_data_disc_graft_get_real (disc, parent_path);

		/* We must make sure that this invalid name hasn't already been
		 * processed. This can happen if we load a project (URI is then
		 * a graft point with a valid UTF8 name) or when we rescan a 
		 * directory after an inotify call. If it has been processed,
		 * it is excluded from its parent path. */
		if (BRASERO_URI_EXCLUDED_FROM_GRAFT (disc, graft, uri))
			continue;

		path = brasero_data_disc_get_unique_valid_utf8_path (disc,
								     utf8_name,
								     parent_path,
								     info,
								     contents->entries);

		/* check if the new path should go into joliet hash.
		 * NOTE: since this URI is a graft no need to add it to the
		 * joliet non compliant uri list */
		name = g_path_get_basename (path);
		if (strlen (name) > 64)
			brasero_data_disc_joliet_incompat_add_path (disc, path);
		g_free (name);

		/* create the new graft point and file/directory if needed */
		if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
			BraseroFile *tmp;

			tmp = g_hash_table_lookup (disc->priv->dirs, uri);
			if ((tmp == NULL || tmp->sectors < 0)
			&&   EXPLORE_IS_NOT_STRICTLY_EXCLUDED (disc, uri)) {
				brasero_data_disc_directory_new (disc,
								 g_strdup (uri),
								 TRUE);
			}
		}
		else if (!g_hash_table_lookup (disc->priv->files, uri)
		     &&  EXPLORE_IS_NOT_STRICTLY_EXCLUDED (disc, uri)) {
			gint64 sectors;

			sectors = GET_SIZE_IN_SECTORS (info->size);
			brasero_data_disc_file_new (disc,
						    g_strdup (uri),
						    sectors);
		}
	
		brasero_data_disc_graft_new (disc, uri, path);
		grafts = g_slist_prepend (grafts, path);

		/* exclude it */
		brasero_data_disc_exclude_uri (disc, graft, uri);
	}

	/* expose new grafted files if need be */
	if (grafts) {
		if (disc->priv->references
		&&  g_hash_table_lookup (disc->priv->references, contents->uri))
			brasero_data_disc_expose_grafted (disc, grafts);

		g_slist_foreach (grafts, (GFunc) g_free, NULL);
		g_slist_free (grafts);
	}

	g_free (utf8_name);
}

static void
brasero_data_disc_invalid_utf8_list_new (BraseroDataDisc *disc,
					 BraseroDirectoryContents *contents,
					 GSList *list)
{
	GSList *paths;
	GSList *iter;

	/* get all the paths where the parent could appear */
	paths = brasero_data_disc_uri_to_paths (disc, contents->uri, TRUE);
	for (iter = list; iter; iter = iter->next) {
		BraseroDirectoryEntry *entry;
		GnomeVFSFileInfo *info;

		entry = iter->data;
		contents->entries = g_slist_remove (contents->entries, entry);

		info = entry->info;
		brasero_data_disc_invalid_utf8_new (disc,
						    contents,
						    entry,
						    paths);
	}

	g_slist_foreach (paths, (GFunc) g_free, NULL);
	g_slist_free (paths);

	/* we unref the infos since we removed them from the contents->entries list */
	g_slist_foreach (list, (GFunc) brasero_data_disc_directory_entry_free, NULL);
	g_slist_free (list);
}

static void
brasero_data_disc_dir_contents_end (GObject *object,
				    gpointer data,
				    gboolean cancelled)
{
	BraseroDataDisc *disc = BRASERO_DATA_DISC (object);
	BraseroDirectoryContents *contents = data;
	GSList *invalid_utf8 = NULL;
	GSList *symlinks = NULL;
	GnomeVFSFileInfo *info;
	gint64 dir_sectors = 0;
	gint64 diffsectors;
	BraseroFile *dir;
	GSList *iter;

	dir = g_hash_table_lookup (disc->priv->dirs, contents->uri);
	if (!dir || dir->sectors < 0) {
		brasero_data_disc_directory_contents_free (disc, contents);
		return;
	}

	disc->priv->loading = g_slist_remove (disc->priv->loading, dir);
	disc->priv->rescan = g_slist_remove (disc->priv->rescan, dir);
	for (iter = contents->entries; iter; iter = iter->next) {
		BraseroDirectoryEntry *entry;
		gchar *current;

		entry = iter->data;
		info = entry->info;

		if (GNOME_VFS_FILE_INFO_SYMLINK (info)) {
			symlinks = g_slist_prepend (symlinks, entry);
			continue;
		}

		if (!g_utf8_validate (info->name, -1, NULL)) {
			invalid_utf8 = g_slist_prepend (invalid_utf8, entry);
			continue;
		}

		current = entry->uri;
		if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
			BraseroFile *tmp;

			tmp = g_hash_table_lookup (disc->priv->dirs, current);
			if ((tmp == NULL || tmp->sectors < 0)
			&&   EXPLORE_IS_NOT_STRICTLY_EXCLUDED (disc, current)) {
				brasero_data_disc_directory_new (disc,
								 g_strdup (current),
								 TRUE);
			}
		}
		else if (!g_hash_table_lookup (disc->priv->files, current)
		     &&   EXPLORE_IS_NOT_STRICTLY_EXCLUDED (disc, current)) {
			dir_sectors += GET_SIZE_IN_SECTORS (info->size);
		}

		if (strlen (info->name) > 64) {
			GSList *paths;

			/* this file could conflict with joliet compatibility so
			 * we add all its paths to joliet_incompat hash */
			paths = brasero_data_disc_uri_to_paths (disc,
								current,
								FALSE);
			brasero_data_disc_joliet_incompat_add_uri (disc, current);
			brasero_data_disc_joliet_incompat_add_paths (disc, paths);
			g_slist_foreach (paths, (GFunc) g_free, NULL);
			g_slist_free (paths);
		}
	}

	diffsectors = dir_sectors - dir->sectors;
	if (diffsectors) {
		dir->sectors += diffsectors;
		brasero_data_disc_size_changed (disc, diffsectors);
	}

	/* process invalid utf8 filenames */
	brasero_data_disc_invalid_utf8_list_new (disc,
						 contents,
						 invalid_utf8);

	/* process symlinks */
	brasero_data_disc_symlink_list_new (disc,
					    contents,
					    symlinks);

	if (disc->priv->references
	&&  g_hash_table_lookup (disc->priv->references, dir->uri)) {
		BraseroDirectoryEntry *entry;

		if (!contents->entries) {
			/* empty directory to be exposed */
			BRASERO_ADD_TO_EXPOSE_QUEUE (disc, contents);
			return;
		}

		/* make sure that when we explored we asked for mime types :
		 * a directory could be explored and then its parent expanded
		 * which adds references but unfortunately we won't have any
		 * mime types */
		entry = contents->entries->data;
		if (entry->info && entry->info->mime_type) {
			/* there are references put that in the queue for it to
			 * be exposed */
			brasero_data_disc_increase_activity_counter (disc);
			BRASERO_ADD_TO_EXPOSE_QUEUE (disc, contents);
			return;
		}

		brasero_data_disc_expose_insert_path_real (disc, contents->uri);
	}

	brasero_data_disc_directory_contents_free (disc, contents);
}

struct _BraseroCheckRestoredData {
	BraseroFilterStatus status;
	BraseroDataDisc *disc;
	gchar *uri;
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
		     BraseroFilterStatus status)
{
	BraseroFilterStatus current_status;
	gboolean retval;

	retval = FALSE;
	
	if (status == BRASERO_FILTER_BROKEN_SYM && !filter_broken_sym)
		retval = TRUE;
	else if (status == BRASERO_FILTER_HIDDEN && !filter_hidden)
		retval = TRUE;

	if (!disc->priv->restored)
		goto end;

	current_status = GPOINTER_TO_INT (g_hash_table_lookup (disc->priv->restored, uri));
	if (!current_status)
		goto end;

	if (status == BRASERO_FILTER_UNREADABLE
	||  status == BRASERO_FILTER_RECURSIVE_SYM) {
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
	return retval;
}

static gboolean
brasero_data_disc_load_result (BraseroVFS *self,
			       GObject *owner,
			       GnomeVFSResult result,
			       const gchar *uri,
			       GnomeVFSFileInfo *info,
			       gpointer callback_data)
{
	BraseroDirectoryContents *contents = callback_data;
	BraseroDataDisc *disc = BRASERO_DATA_DISC (owner);
	BraseroDirectoryEntry *entry;

	if (result != GNOME_VFS_OK) {
		BraseroFilterStatus status;

		if (result == GNOME_VFS_ERROR_NOT_FOUND)
			status = BRASERO_FILTER_UNKNOWN;
		else if (result == GNOME_VFS_ERROR_LOOP) {
			_check_for_restored (disc,
					     uri,
					     BRASERO_FILTER_RECURSIVE_SYM);

			status = BRASERO_FILTER_RECURSIVE_SYM;
		}
		else {
			_check_for_restored (disc,
					     uri,
					     BRASERO_FILTER_UNREADABLE);

			status = BRASERO_FILTER_UNREADABLE;
		}

		brasero_data_disc_directory_entry_error (disc, uri, status);

		g_warning ("Can't open directory %s : %s\n",
			   uri,
			   gnome_vfs_result_to_string (result));
		return TRUE;
	}

	if (!brasero_data_disc_is_readable (info)) {
		_check_for_restored (disc,
				     uri,
				     BRASERO_FILTER_UNREADABLE);

		brasero_data_disc_directory_entry_error (disc,
							 uri,
							 BRASERO_FILTER_UNREADABLE);
		return TRUE;
	}

	if (info->type == GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK && !info->symlink_name) {
		if (!_check_for_restored (disc, uri, BRASERO_FILTER_BROKEN_SYM)) {
			brasero_data_disc_directory_entry_error (disc,
								 uri,
								 BRASERO_FILTER_BROKEN_SYM);
			return TRUE;
		}
	}

	/* a new hidden file ? */
	if (*info->name == '.'
	&& !_check_for_restored (disc, uri, BRASERO_FILTER_HIDDEN)) {
		brasero_data_disc_directory_entry_error (disc,
							 uri,
							 BRASERO_FILTER_HIDDEN);
		return TRUE;
	}

	/* we accumulate all the directory contents till the end and will
	 * process all of them at once, when it's finished. We have to do
	 * that to calculate the whole size. */
	entry = g_new0 (BraseroDirectoryEntry, 1);
	entry->uri = g_strdup (uri);
	entry->info = info;
	gnome_vfs_file_info_ref (info);

	contents->entries = g_slist_prepend (contents->entries, entry);

	return TRUE;
}

static BraseroDiscResult
brasero_data_disc_directory_load (BraseroDataDisc *disc,
				  BraseroFile *dir,
				  gboolean append)
{
	BraseroDirectoryContents *contents;
	gboolean result;

	/* start exploration */
	disc->priv->loading = g_slist_prepend (disc->priv->loading, dir);

	if (!disc->priv->load_type)
		disc->priv->load_type = brasero_vfs_register_data_type (disc->priv->vfs,
								        G_OBJECT (disc),
									G_CALLBACK (brasero_data_disc_load_result),
									brasero_data_disc_dir_contents_end);

	contents = g_new0 (BraseroDirectoryContents, 1);
	contents->uri = g_strdup (dir->uri);

	result = brasero_vfs_load_directory (disc->priv->vfs,
					     dir->uri,
					     GNOME_VFS_FILE_INFO_GET_ACCESS_RIGHTS,
					     disc->priv->load_type,
					     contents);

	if (!result)
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
	BraseroDirectoryContents *contents = data;
	BraseroFile *dir = user_data;

	if (!strcmp (dir->uri, contents->uri))
		return TRUE;

	return FALSE;
}

static void
brasero_data_disc_directory_priority (BraseroDataDisc *disc,
				      BraseroFile *dir)
{
	brasero_vfs_find_urgent (disc->priv->vfs,
				 disc->priv->load_type,
				 brasero_data_disc_directory_priority_cb,
				 dir);
}

/******************************* Row removal ***********************************/
static void
brasero_data_disc_remove_row_in_dirs_hash (BraseroDataDisc *disc,
					   BraseroFile *dir,
					   const gchar *path)
{
	GSList *grafts;

	/* remove all the children graft point of this path */
	grafts = g_slist_append (NULL, (gpointer) path);
	brasero_data_disc_graft_children_remove (disc, grafts);
	g_slist_free (grafts);

	/* remove graft point if path == graft point */
	if (!brasero_data_disc_graft_remove (disc, path)) {
		gchar *graft;

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
					    const gchar *path)
{
	/* see if path == graft point. If so, remove it */
	if (!brasero_data_disc_graft_remove (disc, path)) {
		gchar *graft;

		/* the path was not of the graft points of the file so 
		 * it has a parent graft point, find it and exclude it */
		graft = brasero_data_disc_graft_get (disc, path);
		brasero_data_disc_exclude_uri (disc, graft, file->uri);
		g_free (graft);
	}
}

static void
brasero_data_disc_delete_row_cb (BraseroVFS *self,
				 GObject *owner,
				 GnomeVFSResult result,
				 const gchar *uri,
				 GnomeVFSFileInfo *info,
				 gpointer callback_data)
{
	BraseroDataDisc *disc = BRASERO_DATA_DISC (owner);
	BraseroFile *parent;
	gchar *parenturi;
	gint64 sectors;

	parenturi = g_path_get_dirname (uri);
	parent = g_hash_table_lookup (disc->priv->dirs, parenturi);
	g_free (parenturi);

	if (!parent || parent->sectors < 0)
		return;

	if (result == GNOME_VFS_ERROR_NOT_FOUND) {
		brasero_data_disc_add_rescan (disc, parent);
		return;
	}

	if (result == GNOME_VFS_ERROR_LOOP) {
		brasero_data_disc_unreadable_new (disc,
						  g_strdup (uri),
						  BRASERO_FILTER_RECURSIVE_SYM);
		brasero_data_disc_add_rescan (disc, parent);
		return;
	}

	if (result != GNOME_VFS_OK
	|| !brasero_data_disc_is_readable (info)) {
		brasero_data_disc_unreadable_new (disc,
						  g_strdup (uri),
						  BRASERO_FILTER_UNREADABLE);
		brasero_data_disc_add_rescan (disc, parent);
		return;
	}

	sectors = GET_SIZE_IN_SECTORS (info->size);
	brasero_data_disc_size_changed (disc, sectors * (-1));
	parent->sectors -= sectors;
}

static void
braseri_data_disc_delete_row_end (GObject *object,
				  gpointer callback_data,
				  gboolean cancelled)
{
	BraseroDataDisc *disc = BRASERO_DATA_DISC (object);

	brasero_data_disc_decrease_activity_counter (disc);
}

static void
brasero_data_disc_is_session_path_deleted (BraseroDataDisc *disc,
					   const gchar *path)
{
	GtkTreeIter parent_iter;
	GtkTreePath *treepath;
	GtkTreeModel *model;
	BraseroVolFile *file;
	gchar *parent_path;
	GtkTreeIter row;

	if (!disc->priv->session)
		return;

	file = brasero_volume_file_from_path (path, disc->priv->session);
	if (!file)
		return;

	parent_path = g_path_get_dirname (path);

	/* if this file is at root add a graft point */
	if (!strcmp (parent_path, G_DIR_SEPARATOR_S))
		brasero_data_disc_graft_new (disc,
					     BRASERO_IMPORTED_FILE,
					     path);

	if (!brasero_data_disc_disc_path_to_tree_path (disc,
						       parent_path,
						       &treepath,
						       NULL)) {
		g_free (parent_path);
		return;
	}
	g_free (parent_path);

	model = disc->priv->model;

	if (treepath) {
		gtk_tree_model_get_iter (model, &parent_iter, treepath);
		gtk_tree_path_free (treepath);

		gtk_tree_store_append (GTK_TREE_STORE (model), &row, &parent_iter);
	}
	else
		gtk_tree_store_append (GTK_TREE_STORE (model), &row, NULL);

	brasero_data_disc_tree_new_imported_session_file (disc,
							  file,
							  &row);

	if (file->isdir)
		brasero_data_disc_expose_path (disc, path);
}

static void
brasero_data_disc_path_remove_user (BraseroDataDisc *disc,
				    const gchar *path)
{
	BraseroFile *file;
	gchar *uri;

	brasero_data_disc_joliet_incompat_remove_path (disc, path);
	brasero_data_disc_reference_remove_path (disc, path);

	/* uri can be NULL if uri is an empty directory
	 * added or if the row hasn't been loaded yet */
	uri = brasero_data_disc_path_to_uri (disc, path);

	if (uri == BRASERO_IMPORTED_FILE)
		return;

	if (!uri) {
		const gchar *graft = NULL;

		/* created directories */
		graft = brasero_data_disc_graft_get_real (disc, path);
		if (graft) {
			GSList *paths;

			/* remove all the grafted chidren */
			paths = g_slist_append (NULL, (gchar *) graft);
			brasero_data_disc_graft_children_remove (disc, paths);
			g_slist_free (paths);

			if (!strcmp (path, graft)) {
				/* remove the graft point at the same time */
				g_hash_table_remove (disc->priv->paths, graft);
				g_free ((gchar *)graft);
			}
		}

		/* make an imported session file re-appear if need be */
		brasero_data_disc_is_session_path_deleted (disc, path);
		return;

#if 0
		BraseroVolFile *file;

		/* could be used the day when a library allows full editing 
		 * of multisession  */

		if (uri != BRASERO_IMPORTED_FILE)
			return;

		/* get the volfile and add it to the list of excluded session files */
		file = brasero_volume_file_from_path (path,
						      disc->priv->session);
		if (!file)
			return;

		/* update the size */
		brasero_data_disc_size_changed (disc, (-1) * brasero_volume_file_size (file));

		/* since it could a file with a graft point, it could already be
		 * in the list. We check that and and eventually add it if it is
		 * not in this list */
		if (!g_slist_find (disc->priv->session_file_excluded, file))
			disc->priv->session_file_excluded = g_slist_prepend (disc->priv->session_file_excluded,
									     file);

		return;
#endif

	}

	if (!disc->priv->remove_user)
		disc->priv->remove_user = brasero_vfs_register_data_type (disc->priv->vfs,
									  G_OBJECT (disc),
									  G_CALLBACK (brasero_data_disc_delete_row_cb),
									  braseri_data_disc_delete_row_end);

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
		gchar *graft;

		/* exclude it from parent */
		graft = brasero_data_disc_graft_get (disc, path);
		brasero_data_disc_exclude_uri (disc, graft, uri);
		g_free (graft);

		/* if it is excluded in all parent graft points exclude it
		 * and update the selection and the parent size */
		if (brasero_data_disc_is_excluded (disc, uri, NULL)) {
			GList *uris;
			gboolean result;

			uris = g_list_prepend (NULL, (gchar*) uri);
			result = brasero_vfs_get_info (disc->priv->vfs,
						       uris,
						       TRUE,
						       GNOME_VFS_FILE_INFO_DEFAULT,
						       disc->priv->remove_user,
						       NULL);
			g_list_free (uris);

			if (result)
				brasero_data_disc_increase_activity_counter (disc);
		}
	}
	g_free (uri);

	brasero_data_disc_is_session_path_deleted (disc, path);
}

static void
brasero_data_disc_replace_file (BraseroDataDisc *disc,
				GtkTreePath *parent,
				GtkTreeIter *row,
				const gchar *name)
{
	gchar *parent_path, *path;
	GtkTreeModel *model;
	gboolean isdir;
	gint type;

	/* get the path */
	if (!brasero_data_disc_tree_path_to_disc_path (disc, parent, &parent_path))
		return;

	path = g_build_path (G_DIR_SEPARATOR_S, parent_path, name, NULL);

	model = disc->priv->model;
	gtk_tree_model_get (model, row,
			    ROW_TYPE_COL, &type,
			    ISDIR_COL, &isdir,
			    -1);

	if (type == ROW_SESSION) {
		brasero_data_disc_joliet_incompat_remove_path (disc, path);
		brasero_data_disc_reference_remove_path (disc, path);

		/* see if it isn't an imported session file, if so simply remove
		 * all potential children grafts and remove it from the tree */
		if (isdir) {
			GSList *paths;

			paths = g_slist_prepend (NULL, path);
			brasero_data_disc_graft_children_remove (disc, paths);
			g_slist_free (paths);
		}

		/* just in case it is at the root (it then has a graft point) */
		brasero_data_disc_graft_remove (disc, path);
	}
	else
		brasero_data_disc_path_remove_user (disc, path);


  	gtk_tree_store_remove (GTK_TREE_STORE (model), row);
	g_free (parent_path);
	g_free (path);
}

static void
brasero_data_disc_delete_selected (BraseroDisc *disc)
{
	GtkTreeSelection *selection;
	GtkTreePath *cursorpath;
	GtkTreePath *realpath;
	GtkTreePath *treepath;
	BraseroDataDisc *data;
	GtkTreeModel *model;
	GtkTreeModel *sort;
	GList *list, *iter;
	GtkTreeIter row;
	gchar *discpath;

	data = BRASERO_DATA_DISC (disc);
	if (data->priv->is_loading)
		return;

	model = data->priv->model;
	sort = data->priv->sort;
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (data->priv->tree));

	/* we must start by the end for the treepaths to point to valid rows */
	list = gtk_tree_selection_get_selected_rows (selection, &sort);
	list = g_list_reverse (list);

	gtk_tree_view_get_cursor (GTK_TREE_VIEW (BRASERO_DATA_DISC (disc)->priv->tree),
				  &cursorpath,
				  NULL);
	
	for (iter = list; iter; iter = iter->next) {
		treepath = iter->data;

		if (cursorpath && !gtk_tree_path_compare (cursorpath, treepath)) {
			GtkTreePath *tmp_path;

			/* this is to silence a warning with SortModel when
			 * removing a row being edited. We can only hope that
			 * there won't be G_MAXINT rows =) */
			tmp_path = gtk_tree_path_new_from_indices (G_MAXINT, -1);
			gtk_tree_view_set_cursor (GTK_TREE_VIEW (BRASERO_DATA_DISC (disc)->priv->tree),
						  tmp_path,
						  NULL,
						  FALSE);
			gtk_tree_path_free (tmp_path);
		}

		realpath = gtk_tree_model_sort_convert_path_to_child_path (GTK_TREE_MODEL_SORT (sort),
									   treepath);
		gtk_tree_path_free (treepath);

		brasero_data_disc_tree_path_to_disc_path (data,
							  realpath,
							  &discpath);

		if (gtk_tree_model_get_iter (model, &row, realpath)) {
			GtkTreeIter parent;
			gint status;
			gint type;

			gtk_tree_model_get (model, &row,
 					    ROW_TYPE_COL, &type,
					    ROW_STATUS_COL, &status,
 					    -1);
  
 			if (type != ROW_BOGUS && type != ROW_SESSION) {
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

	if (cursorpath)
		gtk_tree_path_free (cursorpath);

	if (g_hash_table_size (data->priv->paths) == 0)
		brasero_data_disc_selection_changed (data, FALSE);
	else
		brasero_data_disc_selection_changed (data, TRUE);

	/* warn that the selection changed (there are no more selected paths) */
	if (data->priv->selected_path) {
		gtk_tree_path_free (data->priv->selected_path);
		data->priv->selected_path = NULL;
	}
	brasero_disc_selection_changed (disc);
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
	gint len;
	gchar *uri;
	GList *list;
	BraseroDataDisc *disc;
};
typedef struct _BraseroRestoreChildrenData BraseroRestoreChildrenData;

static void
brasero_data_disc_restore_excluded_children_destroy_cb (GObject *object,
							gpointer callback_data,
							gboolean cancelled)
{
	BraseroDataDisc *disc = BRASERO_DATA_DISC (object);

	brasero_data_disc_decrease_activity_counter (disc);
	g_free (callback_data);
}

static void
brasero_data_disc_restore_excluded_children_cb (BraseroVFS *self,
						GObject *owner,
						GnomeVFSResult result,
						const gchar *uri,
						GnomeVFSFileInfo *info,
						gpointer callback_data)
{
	gint64 sectors;
	BraseroFile *dir;
	gchar *dir_uri = callback_data;
	BraseroDataDisc *disc = BRASERO_DATA_DISC (owner);

	dir = g_hash_table_lookup (disc->priv->dirs, dir_uri);
	if (!dir || dir->sectors < 0)
		return;

	/* see if it has not been excluded in between */
	if (brasero_data_disc_is_excluded (disc, uri, NULL))
		return;

	/* as an excluded file it is not in the hashes
	 * and its size is not taken into account */
	if (result == GNOME_VFS_ERROR_NOT_FOUND) {
		brasero_data_disc_remove_uri_from_tree (disc, uri, TRUE);
		return;
	}

	if (result == GNOME_VFS_ERROR_LOOP) {
		brasero_data_disc_remove_uri_from_tree (disc, uri, TRUE);
		brasero_data_disc_unreadable_new (disc,
						  g_strdup (uri),
						  BRASERO_FILTER_RECURSIVE_SYM);
		return;
	}

	if (result != GNOME_VFS_OK
	|| !brasero_data_disc_is_readable (info)) {
		brasero_data_disc_remove_uri_from_tree (disc, uri, TRUE);
		brasero_data_disc_unreadable_new (disc,
						  g_strdup (uri),
						  BRASERO_FILTER_UNREADABLE);
		return;
	}

	sectors = brasero_data_disc_file_info (disc, uri, info);
	if (sectors) {
		gchar *parent;
		BraseroFile *dir;
	
		parent = g_path_get_dirname (uri);
		dir = g_hash_table_lookup (disc->priv->dirs, parent);
		g_free (parent);

		dir->sectors += sectors;
		brasero_data_disc_size_changed (disc, sectors);
	}
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
		data->list = g_list_prepend (data->list, key);
	g_free (parent_uri);
}

static void
brasero_data_disc_restore_excluded_children (BraseroDataDisc *disc,
					     BraseroFile *dir)
{
	BraseroRestoreChildrenData callback_data;
	BraseroDiscResult result;
	gchar *dir_uri;

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

	if (!disc->priv->restore_excluded)
		disc->priv->restore_excluded = brasero_vfs_register_data_type (disc->priv->vfs,
									       G_OBJECT (disc),
									       G_CALLBACK (brasero_data_disc_restore_excluded_children_cb),
									       brasero_data_disc_restore_excluded_children_destroy_cb);

	dir_uri = g_strdup (dir->uri);
	result = brasero_vfs_get_info (disc->priv->vfs,
				       callback_data.list,
				       TRUE,
				       GNOME_VFS_FILE_INFO_GET_ACCESS_RIGHTS,
				       disc->priv->restore_excluded,
				       dir_uri);
	g_list_free (callback_data.list);

	if (result)
		brasero_data_disc_increase_activity_counter (disc);
}

struct _ReplaceSymlinkChildrenData {
	GSList *grafts;
	gchar *parent;
};
typedef struct _ReplaceSymlinkChildrenData ReplaceSymlinkChildrenData;

static GSList *
brasero_data_disc_get_target_grafts (BraseroDataDisc *disc,
				     const gchar *sym_parent,
				     GSList *grafts,
				     const gchar *symlink)
{
	gint len;
	gchar *path;
	gchar *graft;
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
brasero_data_disc_replace_symlink_children_destroy_cb (GObject *object,
						       gpointer callback_data,
						       gboolean cancelled)
{
	ReplaceSymlinkChildrenData *async_data = callback_data;
	BraseroDataDisc *disc = BRASERO_DATA_DISC (object);
	GSList *iter;

	brasero_data_disc_decrease_activity_counter (disc);

	for (iter = async_data->grafts; iter; iter = iter->next) {
		BraseroDataDiscReference ref;

		ref = GPOINTER_TO_INT (async_data->grafts->data);
		brasero_data_disc_reference_free (disc, ref);
	}
	g_slist_free (async_data->grafts);
	g_free (async_data->parent);
	g_free (async_data);
}

static void
brasero_data_disc_replace_symlink_children_cb (BraseroVFS *self,
					       GObject *owner,
					       GnomeVFSResult result,
					       const gchar *symlink,
					       GnomeVFSFileInfo *info,
					       gpointer data)
{
	BraseroDataDisc *disc = BRASERO_DATA_DISC (owner);
	ReplaceSymlinkChildrenData *callback_data = data;
	GSList *paths = NULL;
	BraseroFile *parent;
	BraseroFile *file;
	GSList *grafts;
	gchar *target;

	grafts = brasero_data_disc_reference_get_list (disc,
						       callback_data->grafts,
						       FALSE);

	parent = g_hash_table_lookup (disc->priv->dirs, callback_data->parent);
	if (!grafts || !parent || parent->sectors < 0)
		goto cleanup;

	if (result != GNOME_VFS_OK)
		goto cleanup;

	target = info->symlink_name;

	if (!target)
		goto cleanup;

	if (result != GNOME_VFS_OK
	|| !brasero_data_disc_is_readable (info))
		goto cleanup;

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
		brasero_data_disc_joliet_incompat_restore_children (disc,
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

cleanup:

	g_slist_foreach (paths, (GFunc) g_free, NULL);
	g_slist_free (paths);

	g_slist_foreach (grafts, (GFunc) g_free, NULL);
	g_slist_free (grafts);
}

static void
brasero_data_disc_replace_symlink_children (BraseroDataDisc *disc,
					    BraseroFile *dir,
					    GSList *grafts)
{
	ReplaceSymlinkChildrenData *async_data;
	BraseroDataDiscReference ref;
	BraseroDiscResult result;
	GList *list;

	list = brasero_data_disc_symlink_get_uri_children (disc, dir->uri);

	if (!list)
		return;

	async_data = g_new0 (ReplaceSymlinkChildrenData, 1);
	async_data->parent = g_strdup (dir->uri);

	if (!disc->priv->replace_symlink)
		disc->priv->replace_symlink = brasero_vfs_register_data_type (disc->priv->vfs,
									      G_OBJECT (disc),
									      G_CALLBACK (brasero_data_disc_replace_symlink_children_cb),
									      brasero_data_disc_replace_symlink_children_destroy_cb);

	for (; grafts; grafts = grafts->next) {
		gchar *graft;

		graft = grafts->data;
		ref = brasero_data_disc_reference_new (disc, graft);
		async_data->grafts = g_slist_prepend (async_data->grafts,
						      GINT_TO_POINTER (ref));
	}

	result = brasero_vfs_get_info (disc->priv->vfs,
				       list,
				       TRUE,
				       GNOME_VFS_FILE_INFO_GET_ACCESS_RIGHTS,
				       disc->priv->replace_symlink,
				       async_data);
	g_list_free (list);

	if (result)
		brasero_data_disc_increase_activity_counter (disc);
}

static gchar *
brasero_data_disc_new_file (BraseroDataDisc *disc,
			    const gchar *uri,
			    const gchar *path,
			    const GnomeVFSFileInfo *info)
{
	gchar *graft;

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

static gchar *
brasero_data_disc_new_row_added (BraseroDataDisc *disc,
				 const gchar *uri,
				 const gchar *path)
{
	gchar *graft = NULL;

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
	gchar *excluded_uri;
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

	/* make sure it is joliet compatible */
	if (strlen (info->name) > 64)
		brasero_data_disc_joliet_incompat_add_uri (disc, uri);

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

			paths = g_slist_prepend (NULL, (gchar *) path);

			/* replace all its children which are symlinks */
			brasero_data_disc_replace_symlink_children (disc,
								    file,
								    paths);

			/* add to joliet non compliant table all the children 
			 * which were already in this table but under other
			 * paths. */
			brasero_data_disc_joliet_incompat_restore_children (disc,
									    file,
									    paths);

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
brasero_data_disc_new_row_destroy_cb (GObject *object,
				      gpointer callback_data,
				      gboolean cancelled)
{
	BraseroDataDisc *disc = BRASERO_DATA_DISC (object);
	BraseroDataDiscReference reference = GPOINTER_TO_INT (callback_data);

	brasero_data_disc_decrease_activity_counter (disc);
	brasero_data_disc_reference_free (disc, reference);
}

static void
brasero_data_disc_new_row_cb (BraseroVFS *self,
			      GObject *owner,
			      GnomeVFSResult result,
			      const gchar *uri,
			      GnomeVFSFileInfo *info,
			      gpointer callback_data)
{
	gchar *path;
	BraseroDiscResult success;
	BraseroDataDisc *disc = BRASERO_DATA_DISC (owner);
	BraseroDataDiscReference reference = GPOINTER_TO_INT (callback_data);

	/* we check wether the row still exists and if everything went well */
	path = brasero_data_disc_reference_get (disc, reference);
	if (!path)
		return;

	/* create an entry in the tree */
	success = brasero_data_disc_new_row_real (disc,
						  uri,
						  info,
						  result,
						  path,
						  NULL);

	if (success == BRASERO_DISC_OK)
		brasero_data_disc_selection_changed (disc, TRUE);

	g_free (path);
}

static void
brasero_data_disc_get_dir_contents_destroy (GObject *object,
					    gpointer callback_data,
					    gboolean cancelled)
{
	BraseroDataDisc *disc = BRASERO_DATA_DISC (object);
	BraseroDataDiscReference reference = GPOINTER_TO_INT (callback_data);

	brasero_data_disc_decrease_activity_counter (disc);
	brasero_data_disc_reference_free (disc, reference);
}

static gboolean
brasero_data_disc_get_dir_contents_results (BraseroVFS *self,
					    GObject *owner,
					    GnomeVFSResult result,
					    const gchar *uri,
					    GnomeVFSFileInfo *info,
					    gpointer user_data)
{
	BraseroDataDiscReference reference = GPOINTER_TO_INT (user_data);
	BraseroDataDisc *disc = BRASERO_DATA_DISC (owner);
	GtkTreePath *treeparent = NULL;
	BraseroDiscResult success;
	gchar *uri_path = NULL;
	gchar *path = NULL;
	gchar *utf8_name;

	/* see if the directory could be opened */
	if (result != GNOME_VFS_OK) {
		/* NOTE: uri can be the parent directory uri or a child (most
		 * probably a looping symlink) */
		brasero_data_disc_unreadable_dialog (disc,
						     uri,
						     result,
						     FALSE);
		goto end;
	}

	/* see if the reference is still valid */
	path = brasero_data_disc_reference_get (disc, reference);
	if (!path)
		goto end;

	if (!brasero_data_disc_disc_path_to_tree_path (disc,
						       path,
						       &treeparent,
						       NULL)) {
		/* that's very unlikely since path reference exists 
		 * and this path had to be visible for drag'n'drop */
		goto end;
	}

	/* check for invalid utf8 characters */
	utf8_name = brasero_utils_validate_utf8 (info->name);

	/* check no other file has the same name in the directory */
	success = brasero_data_disc_tree_check_name_validity (disc,
							      utf8_name ? utf8_name:info->name,
							      treeparent,
							      TRUE);
	if (success != BRASERO_DISC_OK) {
		if (utf8_name)
			g_free (utf8_name);

		goto end;
	}

	/* here we must keep the exact same name */
	uri_path = g_build_path (G_DIR_SEPARATOR_S,
				 path,
				 utf8_name ? utf8_name:info->name,
				 NULL);

	if (strlen (utf8_name ? utf8_name:info->name) > 64)
		brasero_data_disc_joliet_incompat_add_path (disc, uri_path);

	g_free (utf8_name);

	if (!brasero_data_disc_tree_new_path (disc,
					      uri_path,
					      treeparent,
					      NULL))
		goto end;

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

end:

	if (treeparent)
		gtk_tree_path_free (treeparent);

	if (uri_path)
		g_free (uri_path);

	if (path)
		g_free (path);

	brasero_data_disc_decrease_activity_counter (disc);

	/* free user_data */
	return TRUE;
}

static BraseroDiscResult
brasero_data_disc_add_directory_contents (BraseroDataDisc *disc,
					  const gchar *uri,
					  GnomeVFSFileInfoOptions flags,
					  const gchar *path)
{
	BraseroDataDiscReference reference;
	gboolean result;

	/* NOTE: we'll mostly use this function for non local uri since
	 * I don't see a user adding file:///. That's another reason to
	 * seek asynchronously */
	if (!disc->priv->directory_contents)
		disc->priv->directory_contents = brasero_vfs_register_data_type (disc->priv->vfs,
										 G_OBJECT (disc),
										 G_CALLBACK (brasero_data_disc_get_dir_contents_results),
										 brasero_data_disc_get_dir_contents_destroy);

	reference = brasero_data_disc_reference_new (disc, path);
	result = brasero_vfs_load_directory (disc->priv->vfs,
					     uri,
					     flags,
					     disc->priv->directory_contents,
					     GINT_TO_POINTER (reference));

	/* NOTE : if an error occurs the callback_data will be freed by async_job_manager */
	if (!result)
		return BRASERO_DISC_ERROR_THREAD;

	brasero_data_disc_increase_activity_counter (disc);
	return BRASERO_DISC_OK;
}

static BraseroDiscResult
brasero_data_disc_add_uri_real (BraseroDataDisc *disc,
				const gchar *uri,
				GtkTreePath *treeparent,
				GtkTreePath **treepath)
{
	BraseroDataDiscReference reference;
	BraseroDiscResult success;
	gchar *unescaped_name;
	GnomeVFSURI *vfs_uri;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *utf8_name;
	gboolean result;
	GList *uris;
	gchar *name;
	gchar *path;

	/* NOTE: this function receives URIs only from utf8 origins (not from
	 * gnome-vfs for example) so we can assume that this is safe */
	g_return_val_if_fail (uri != NULL, BRASERO_DISC_ERROR_UNKNOWN);

	if (disc->priv->reject_files || disc->priv->is_loading)
		return BRASERO_DISC_NOT_READY;

	/* g_path_get_basename is not comfortable with uri related
	 * to the root directory so check that before */
	vfs_uri = gnome_vfs_uri_new (uri);
	name = gnome_vfs_uri_extract_short_path_name (vfs_uri);
	gnome_vfs_uri_unref (vfs_uri);

	unescaped_name = gnome_vfs_unescape_string_for_display (name);
	g_free (name);
	name = unescaped_name;

	utf8_name = brasero_utils_validate_utf8 (name);
	if (utf8_name) {
		g_free (name);
		name = utf8_name;
	}

	if (!name)
		return BRASERO_DISC_ERROR_FILE_NOT_FOUND;

	/* create the path */
	if (treeparent && gtk_tree_path_get_depth (treeparent) > 0) {
		gchar *parent;

		brasero_data_disc_tree_path_to_disc_path (disc,
							  treeparent,
							  &parent);

		path = g_build_path (G_DIR_SEPARATOR_S,
				     parent,
				     name,
				     NULL);
		g_free (parent);
	}
	else if (strcmp (name, G_DIR_SEPARATOR_S))
		path = g_build_path (G_DIR_SEPARATOR_S, G_DIR_SEPARATOR_S, name, NULL);
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
		g_free (name);
		g_free (path);
		return success;
	}

	gtk_notebook_set_current_page (GTK_NOTEBOOK (BRASERO_DATA_DISC (disc)->priv->notebook), 1);

	reference = brasero_data_disc_reference_new (disc, path);

	/* if info->name is not compatible with joliet it'll be added later */
	if (strlen (name) > 64)
		brasero_data_disc_joliet_incompat_add_path (disc, path);
	g_free (path);

	if (!disc->priv->add_uri)
		disc->priv->add_uri = brasero_vfs_register_data_type (disc->priv->vfs,
								      G_OBJECT (disc),
								      G_CALLBACK (brasero_data_disc_new_row_cb),
								      brasero_data_disc_new_row_destroy_cb);

	uris = g_list_prepend (NULL, (gchar *) uri);
	result = brasero_vfs_get_info (disc->priv->vfs,
				       uris,
				       TRUE,
				       GNOME_VFS_FILE_INFO_GET_ACCESS_RIGHTS|
				       GNOME_VFS_FILE_INFO_GET_MIME_TYPE|
				       GNOME_VFS_FILE_INFO_FORCE_SLOW_MIME_TYPE,
				       disc->priv->add_uri,
				       GINT_TO_POINTER (reference));
	g_list_free (uris);

	if (!result) {
		g_free (name);
		return success;
	}

	brasero_data_disc_increase_activity_counter (disc);

	/* make it appear in the tree */
	model = disc->priv->model;
	if (treeparent && gtk_tree_path_get_depth (treeparent) > 0) {
		GtkTreeIter parent;
		GtkTreePath *sort_parent;

		sort_parent = gtk_tree_model_sort_convert_child_path_to_path (GTK_TREE_MODEL_SORT (disc->priv->sort),
									      treeparent);
		if (!gtk_tree_view_row_expanded (GTK_TREE_VIEW (disc->priv->tree), sort_parent))
			gtk_tree_view_expand_row (GTK_TREE_VIEW (disc->priv->tree),
						  sort_parent,
						  FALSE);
		gtk_tree_path_free (sort_parent);

		gtk_tree_model_get_iter (model, &parent, treeparent);
		brasero_data_disc_remove_bogus_child (disc, &parent);
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
			    ROW_TYPE_COL, ROW_FILE,
			    EDITABLE_COL, TRUE,
			    -1);

	if (treepath)
		*treepath = gtk_tree_model_get_path (model, &iter);

	g_free (name);

	return BRASERO_DISC_OK;
}

static BraseroDiscResult
brasero_data_disc_add_uri (BraseroDisc *disc, const gchar *uri)
{
	GList *selected;
	BraseroDiscResult success;
	GtkTreePath *parent = NULL;
	BraseroDataDisc *data_disc;
	GtkTreeSelection *selection;

	data_disc = BRASERO_DATA_DISC (disc);

	if (data_disc->priv->is_loading)
		return BRASERO_DISC_LOADING;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (data_disc->priv->tree));
	selected = gtk_tree_selection_get_selected_rows (selection, NULL);
	if (g_list_length (selected) == 1) {
		GtkTreePath *treepath;
		gboolean is_directory;
		GtkTreeIter iter;

		treepath = selected->data;
		gtk_tree_model_get_iter (data_disc->priv->sort, &iter, treepath);
		gtk_tree_model_get (data_disc->priv->sort, &iter,
				    ISDIR_COL, &is_directory,
				    -1);

		if (is_directory)
			parent = gtk_tree_model_sort_convert_path_to_child_path (GTK_TREE_MODEL_SORT (data_disc->priv->sort),
										 treepath);
	}
	g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (selected);

	success = brasero_data_disc_add_uri_real (BRASERO_DATA_DISC (disc),
						  uri,
						  parent,
						  NULL);

	if (parent)
		gtk_tree_path_free (parent);

	return success;
}

BraseroDiscResult
brasero_data_disc_can_add_uri (BraseroDisc *disc,
			       const gchar *uri)
{
	gchar *name;
	GList *selected;
	gchar *utf8_name;
	GnomeVFSURI *vfs_uri;
	gchar *unescaped_name;
	BraseroDiscResult success;
	GtkTreePath *parent = NULL;
	BraseroDataDisc *data_disc;
	GtkTreeSelection *selection;

	data_disc = BRASERO_DATA_DISC (disc);

	if (data_disc->priv->is_loading)
		return BRASERO_DISC_LOADING;

	if (data_disc->priv->reject_files)
		return BRASERO_DISC_NOT_READY;

	/* g_path_get_basename is not comfortable with uri related
	 * to the root directory so check that before */
	vfs_uri = gnome_vfs_uri_new (uri);
	name = gnome_vfs_uri_extract_short_path_name (vfs_uri);
	gnome_vfs_uri_unref (vfs_uri);

	unescaped_name = gnome_vfs_unescape_string_for_display (name);
	g_free (name);
	name = unescaped_name;

	utf8_name = brasero_utils_validate_utf8 (name);
	if (utf8_name) {
		g_free (name);
		name = utf8_name;
	}

	if (!name)
		return BRASERO_DISC_ERROR_FILE_NOT_FOUND;

	/* create the path */
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (data_disc->priv->tree));
	selected = gtk_tree_selection_get_selected_rows (selection, NULL);
	if (g_list_length (selected) == 1) {
		GtkTreePath *treepath;
		gboolean is_directory;
		GtkTreeIter iter;

		treepath = selected->data;
		gtk_tree_model_get_iter (data_disc->priv->sort, &iter, treepath);
		gtk_tree_model_get (data_disc->priv->sort, &iter,
				    ISDIR_COL, &is_directory,
				    -1);

		if (is_directory)
			parent = gtk_tree_model_sort_convert_path_to_child_path (GTK_TREE_MODEL_SORT (data_disc->priv->sort),
										 treepath);
	}
	g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (selected);

	/* We make sure there isn't the same file in the directory
	 * and it is joliet compatible */
	success = brasero_data_disc_tree_check_name_validity (data_disc,
							      name,
							      parent,
							      FALSE);

	g_free (name);
	return success;
}

/************************ multisession handling *******************************/
static void
brasero_data_disc_remove_imported_session (BraseroDataDisc *disc)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	brasero_disc_flags_changed (BRASERO_DISC (disc), BRASERO_BURN_FLAG_NONE);

	if (!disc->priv->session)
		return;

	/* first find all the imported files at root */
	model = disc->priv->model;
	if (!gtk_tree_model_get_iter_first (model, &iter))
		return;

	do {
		gint type;
		gchar *path;
		gchar *name;
		GSList *paths;

		gtk_tree_model_get (model, &iter,
				    ROW_TYPE_COL, &type,
				    -1);

		if (type != ROW_SESSION) {
			if (!gtk_tree_model_iter_next (model, &iter))
				break;

			continue;
		}

		gtk_tree_model_get (model, &iter,
				    NAME_COL, &name,
				    -1);

		path = g_strconcat (G_DIR_SEPARATOR_S, name, NULL);
		g_free (name);

		/* remove all the grafted chidren */
		brasero_data_disc_joliet_incompat_remove_path (disc, path);
		brasero_data_disc_reference_remove_path (disc, path);

		paths = g_slist_append (NULL, (gchar *) path);
		brasero_data_disc_graft_children_remove (disc, paths);
		g_slist_free (paths);

		brasero_data_disc_graft_remove (disc, path);
		g_free (path);

		if (!gtk_tree_store_remove (GTK_TREE_STORE (model), &iter))
			break;
	} while (1);

	brasero_volume_file_free (disc->priv->session);
	disc->priv->session = NULL;
}

static void
brasero_data_disc_import_session_error (BraseroDataDisc *disc,
					gchar *message)
{
	GtkWidget *dialog;
	GtkAction *action;
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (disc));
	dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					 GTK_DIALOG_DESTROY_WITH_PARENT |
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_WARNING,
					 GTK_BUTTONS_CLOSE,
					 _("The session couldn't be imported:"));

	gtk_window_set_title (GTK_WINDOW (dialog), _("Import session error"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  message);

	gtk_widget_show_all (dialog);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	action = gtk_action_group_get_action (disc->priv->disc_group, "ImportSession");
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), FALSE);
}

static void
brasero_data_disc_import_session_cb (GtkToggleAction *action,
				     BraseroDataDisc *disc)
{
	const gchar *device = NULL;
	BraseroVolFile *volume;
	GError *error = NULL;
	GtkTreeModel *model;
	gint64 data_blocks;
	GtkTreeIter row;
	gint64 block;
	GList *iter;

	if (!gtk_toggle_action_get_active (action)) {
		/* the user asked to remove the imported session if any */
		brasero_data_disc_remove_imported_session (disc);
		return;
	}

	if (disc->priv->is_loading) {
		brasero_data_disc_import_session_error (disc, _("loading project"));
		return;
	}

	if (!disc->priv->drive) {
		brasero_data_disc_import_session_error (disc, _("there is no selected disc"));
		return;
	}

	/* check that there isn't already an imported session. */
	if (disc->priv->session) {
		brasero_data_disc_remove_imported_session (disc);
	}

	/* get the address for the last track and retrieve the file list */
	NCB_MEDIA_GET_LAST_DATA_TRACK_ADDRESS (disc->priv->drive,
					       NULL,
					       &block);
	if (block == -1) {
		brasero_data_disc_import_session_error (disc, _("there isn't any available session on the disc"));
		return;
	}

	device = NCB_DRIVE_GET_DEVICE (disc->priv->drive);
	volume = brasero_volume_get_files (device,
					   block,
					   NULL,
					   NULL,
					   &data_blocks,
					   &error);
	if (error) {
		brasero_data_disc_import_session_error (disc, error->message);
		if (volume)
			brasero_volume_file_free (volume);
		return;
	}

	if (!volume) {
		brasero_data_disc_import_session_error (disc, _("unknown volume type"));
		return;
	}

	model = disc->priv->model;
	gtk_notebook_set_current_page (GTK_NOTEBOOK (disc->priv->notebook), 1);

	/* add all the files/folders at the root of the session */
	for (iter = volume->specific.dir.children; iter; iter = iter->next) {
		BraseroDiscResult success;
		BraseroVolFile *file;
		gchar *path;

		file = iter->data;

		/* Make sure there isn't the same file */
		/* FIXME: it could be good to check if a directory with the 
		 * same filename doesn't exist and "merge" them */
		success = brasero_data_disc_tree_check_name_validity (disc,
								      BRASERO_VOLUME_FILE_NAME (file),
								      NULL,
								      TRUE);
		if (success != BRASERO_DISC_OK)
			continue;

		/* add the files to the tree */
		gtk_tree_store_append (GTK_TREE_STORE (model), &row, NULL);
		brasero_data_disc_tree_new_imported_session_file (disc,
								  file,
								  &row);

		path = g_build_filename (G_DIR_SEPARATOR_S,
					 BRASERO_VOLUME_FILE_NAME (file),
					 NULL);

		/* add a graft point */
		brasero_data_disc_graft_new (disc,
					     BRASERO_IMPORTED_FILE,
					     path);
		brasero_data_disc_expose_imported_session_file (disc, file, path);
		g_free (path);
	}

	/* put this here in case we have to replace one file at the root
	 * brasero_data_disc_is_session_path_deleted would think it needs
	 * to restore a session file */
	disc->priv->session = volume;

	/* add the size of the session files */
	brasero_disc_flags_changed (BRASERO_DISC (disc), BRASERO_BURN_FLAG_MERGE);
}

static void
brasero_data_disc_update_multi_button_state (BraseroDataDisc *disc)
{
	BraseroMedia media_status;
	gboolean multisession;
	BraseroBurnCaps *caps;
	BraseroMedia media;
	GtkAction *action;

	action = gtk_action_group_get_action (disc->priv->disc_group, "ImportSession");
	if (!disc->priv->drive) {
		gtk_action_set_sensitive (action, FALSE);
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), FALSE);
		return;
	}

	media = NCB_MEDIA_GET_STATUS (disc->priv->drive);

	caps = brasero_burn_caps_get_default ();
	media_status = brasero_burn_caps_media_capabilities (caps, media);
	g_object_unref (caps);

	multisession = (media_status & BRASERO_MEDIUM_WRITABLE) &&
		       (media & BRASERO_MEDIUM_HAS_DATA) &&
		       (NCB_MEDIA_GET_LAST_DATA_TRACK_ADDRESS (disc->priv->drive, NULL, NULL) != -1);

	if (multisession) {
		GtkWidget *widget;

		gtk_action_set_sensitive (action, TRUE);
		widget = gtk_ui_manager_get_widget (disc->priv->manager,
						    "/Toolbar/DiscButtonPlaceholder/ImportSession");
		brasero_data_disc_notify_user (disc,
					       _("A multisession disc is inserted:"),
					       _("Click here to import its contents"),
					       widget);
	}
	else {
		gtk_action_set_sensitive (action, FALSE);
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), FALSE);
	}
}

static void
brasero_data_disc_set_drive (BraseroDisc *disc, NautilusBurnDrive *drive)
{
	BraseroDataDisc *data_disc;

	data_disc = BRASERO_DATA_DISC (disc);
	if (data_disc->priv->drive)
		nautilus_burn_drive_unref (data_disc->priv->drive);

	data_disc->priv->drive = drive;

	brasero_data_disc_update_multi_button_state (data_disc);

	if (drive)
		nautilus_burn_drive_ref (drive);
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
_foreach_excluded_make_list_cb (const gchar *uri,
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
_foreach_grafts_make_list_cb (gchar *path,
			      const gchar *uri,
			      MakeListData *data)
{
	MakeExcludedListData callback_data;
	BraseroGraftPt *graft;

	if (uri == BRASERO_IMPORTED_FILE)
		return;

	graft = g_new0 (BraseroGraftPt, 1);
	graft->uri = (uri != BRASERO_CREATED_DIR) ? g_strdup (uri) : NULL;
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

		if (joliet_compat) {
			GSList *iter;

			for (iter = disc->priv->joliet_incompat_uris; iter; iter = iter->next) {
				gchar *uri;

				uri = iter->data;
				callback_data.list = g_slist_prepend (callback_data.list, g_strdup (uri));
			}
		}

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

    	if (BRASERO_DATA_DISC (disc)->priv->loading)
		return BRASERO_DISC_LOADING;

	brasero_data_disc_get_track_real (BRASERO_DATA_DISC (disc),
					  &grafts,
					  NULL,
					  &restored,
					  FALSE);
	if (!restored && !grafts)
		return BRASERO_DISC_ERROR_EMPTY_SELECTION;

	track->type = BRASERO_DISC_TRACK_DATA;
	track->contents.data.grafts = grafts;
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
brasero_data_disc_set_session_param (BraseroDisc *disc,
				     BraseroBurnSession *session)
{
	BraseroTrackType type;
	BraseroImageFS fs_type;

	fs_type = BRASERO_IMAGE_FS_ISO;
	if (!BRASERO_DATA_DISC (disc)->priv->joliet_non_compliant)
		fs_type |= BRASERO_IMAGE_FS_JOLIET;

	if (brasero_data_disc_is_video_DVD (BRASERO_DATA_DISC (disc)))
		fs_type |= BRASERO_IMAGE_FS_VIDEO;

	type.type = BRASERO_TRACK_TYPE_DATA;
	type.subtype.fs_type = fs_type;
	brasero_burn_session_set_input_type (session, &type);

	if (BRASERO_DATA_DISC (disc)->priv->session) {
		/* remove the following flag just in case */
		brasero_burn_session_remove_flag (session,
						  BRASERO_BURN_FLAG_FAST_BLANK|
						  BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE);
		brasero_burn_session_add_flag (session, BRASERO_BURN_FLAG_MERGE);
		brasero_burn_session_set_burner (session,
						 BRASERO_DATA_DISC (disc)->priv->drive);
	}

	return BRASERO_DISC_OK;
}

static BraseroDiscResult
brasero_data_disc_set_session_contents (BraseroDisc *disc,
					BraseroBurnSession *session)
{
	BraseroTrack *track;
	BraseroTrackType type;
	GSList *grafts = NULL;
	gboolean joliet_compat;
	GSList *unreadable = NULL;

	/* there should be only one data track */
	brasero_burn_session_get_input_type (session, &type);
	track = brasero_track_new (BRASERO_TRACK_TYPE_DATA);

	/* FIXME! now we should set the number of files and not the size */
/*	brasero_track_set_estimated_size (track,
					  2048,
					  BRASERO_DATA_DISC (disc)->priv->sectors,
					  -1);
*/
	joliet_compat = (type.subtype.fs_type & BRASERO_IMAGE_FS_JOLIET);
	brasero_track_add_data_fs (track, type.subtype.fs_type);
	brasero_data_disc_get_track_real (BRASERO_DATA_DISC (disc),
					  &grafts,
					  &unreadable,
					  NULL,
					  joliet_compat); 

	if (!grafts)
		return BRASERO_DISC_ERROR_EMPTY_SELECTION;

	brasero_track_set_data_source (track, grafts, unreadable);
	brasero_burn_session_add_track (session, track);

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

typedef enum {
	BRASERO_GRAFT_CHECK_OK,
	BRASERO_GRAFT_CHECK_PARENT_FILE,
	BRASERO_GRAFT_CHECK_PARENT_UNREADABLE,
	BRASERO_GRAFT_CHECK_PARENT_NOT_FOUND,
	BRASERO_GRAFT_CHECK_FILE_WITH_SAME_NAME, 
	BRASERO_GRAFT_CHECK_DIR_WITH_SAME_NAME
} BraseroCheckGraftResult;

struct _BraseroCheckGraftResultData {
	BraseroDataDiscReference ref;
	gchar *path;
	gint status;

	gint keep;
};
typedef struct _BraseroCheckGraftResultData BraseroCheckGraftResultData;

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
		brasero_data_disc_tree_new_empty_folder_real (disc,
							      tmp_path,
							      ROW_NOT_EXPLORED,
							      FALSE);

		/* Here we are creating fake directories: no need to add the uri
		 * (and there isn't any) to the joliet non compliant list */
		BRASERO_GET_BASENAME_FOR_DISPLAY (tmp_path, name);
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

static void
brasero_data_disc_graft_check_destroy (GObject *object,
				       gpointer callback_data,
				       gboolean cancelled)
{
	BraseroCheckGraftResultData *graft;
	BraseroDataDisc *disc = BRASERO_DATA_DISC (object);

	graft = callback_data;
	if (graft->keep)
		return;

	brasero_data_disc_decrease_activity_counter (disc);
	brasero_data_disc_reference_free (disc, graft->ref);
	g_free (graft->path);
	g_free (graft);
}

static void
brasero_data_disc_graft_check_result (BraseroDataDisc *disc,
				      BraseroCheckGraftResultData *graft,
				      BraseroCheckGraftResult result,
				      const gchar *parent_found)
{
	const gchar *graft_path;
	gchar *graft_uri;
	gchar *last_path;
	gchar *ref_path;
	gchar *parent;

	graft->keep = 0;

	if (graft->status == BRASERO_GRAFT_CHECK_OK)
		return;

	/* check that we still care about it */
	ref_path = brasero_data_disc_reference_get (disc, graft->ref);
	if (!ref_path)
		return;

	if (strcmp (ref_path, graft->path)) {
		g_free (ref_path);
		return;
	}
	g_free (ref_path);

	/* NOTE: graft_path has to exist since we checked the reference */
	parent = g_path_get_dirname (graft->path);
	graft_path = brasero_data_disc_graft_get_real (disc, parent);

	if (result != BRASERO_GRAFT_CHECK_PARENT_NOT_FOUND) {
		GSList *excluding = NULL;

		/* see that it isn't already excluded if not do it */
		if (disc->priv->excluded)
			excluding = g_hash_table_lookup (disc->priv->excluded,
							 parent_found);

		if (excluding
		&& !g_slist_find (excluding, graft_path)) {
			brasero_data_disc_exclude_uri (disc,
						       graft_path,
						       parent_found);
		}
	}

	if (result == BRASERO_GRAFT_CHECK_FILE_WITH_SAME_NAME
	||  result == BRASERO_GRAFT_CHECK_DIR_WITH_SAME_NAME) {
		g_free (parent);
		return;
	}

	/* we need to create all the directories until last */
	/* NOTE : graft_uri can't be a created dir as we checked that before */
	graft_uri = g_hash_table_lookup (disc->priv->paths, graft_path);
	last_path = g_strconcat (graft_path,
				 parent_found + strlen (graft_uri),
				 NULL);

	while (strcmp (parent, last_path) && strcmp (parent, G_DIR_SEPARATOR_S)) {
		gchar *tmp;
		gchar *name;

		brasero_data_disc_graft_new (disc,
					     NULL,
					     parent);

		/* this is a graft and we still need to add its name to the
		 * joliet non compliant list in case it is added somewhere else
		 * later. */
		BRASERO_GET_BASENAME_FOR_DISPLAY (parent, name);
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

static void
brasero_data_disc_graft_find_first_parent (BraseroVFS *self,
					   GObject *owner,
					   GnomeVFSResult result,
					   const gchar *uri,
					   GnomeVFSFileInfo *info,
					   gpointer callback_data)
{
	GList *uris;
	gchar *parent;
	gboolean success;
	BraseroCheckGraftResultData *graft;
	BraseroDataDisc *disc = BRASERO_DATA_DISC (owner);

	graft = callback_data;

	if (result == GNOME_VFS_ERROR_NOT_FOUND)
		parent = g_path_get_dirname (uri);
	else if (info->type == GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK) {
		/* symlink: do as if it didn't exist since it will
		 * be replaced when met during exploration at this
		 * point we'll check if the paths of the symlink
		 * doesn't overlap a graft point and if so nothing
		 * will happen */
		/* BRASERO_GRAFT_CHECK_PARENT_NOT_FOUND; */
		parent = g_path_get_dirname (uri);
	}
	else if (result != GNOME_VFS_OK) {
		brasero_data_disc_graft_check_result (disc,
						      graft,
						      BRASERO_GRAFT_CHECK_PARENT_UNREADABLE,
						      uri);
		return;
	}
	else if (info->type != GNOME_VFS_FILE_TYPE_DIRECTORY) {
		brasero_data_disc_graft_check_result (disc,
						      graft,
						      BRASERO_GRAFT_CHECK_PARENT_FILE,
						      uri);
		return;
	}
	else {
		/* means we have finally reached a parent directory
		 * (might be root) */
		brasero_data_disc_graft_check_result (disc,
						      graft,
						      BRASERO_GRAFT_CHECK_PARENT_NOT_FOUND,
						      uri);
		return;
	}

	parent = g_path_get_dirname (uri);
	uris = g_list_prepend (NULL, parent);
	success = brasero_vfs_get_info (disc->priv->vfs,
				        uris,
				        TRUE,
				        GNOME_VFS_FILE_INFO_DEFAULT,
				        disc->priv->find_first_graft_parent,
				        graft);
	g_list_free (uris);
	g_free (parent);

	/* NOTE: no need to increase activity */
	if (!success) {
		/* it failed so cancel everything */
		brasero_data_disc_reset_real (disc);

		/* warn the user */
		brasero_data_disc_load_error_dialog (disc);

		/* callback_data must be freed */
		graft->keep = 0;
	}
}

static void
brasero_data_disc_graft_check_parent (BraseroVFS *self,
				      GObject *owner,
				      GnomeVFSResult result,
				      const gchar *uri,
				      GnomeVFSFileInfo *info,
				      gpointer callback_data)
{
	BraseroDataDisc *disc = BRASERO_DATA_DISC (owner);
	BraseroCheckGraftResultData *graft;
	gboolean success;
	gchar *parent;
	GList *uris;

	graft = callback_data;
	
	if (result == GNOME_VFS_OK && info->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
		/* that's ok a parent directory exists for this graft */
		graft->keep = 0;
		return;
	}

	/* NOTE: uri is the URI of its direct parent: since it failed, we
	 * need to find the first parent if any */
	if (!disc->priv->find_first_graft_parent)
		disc->priv->find_first_graft_parent = brasero_vfs_register_data_type (disc->priv->vfs,
										      G_OBJECT (disc),
										      G_CALLBACK (brasero_data_disc_graft_find_first_parent),
										      brasero_data_disc_graft_check_destroy);
	
	parent = g_path_get_dirname (uri);
	uris = g_list_prepend (NULL, parent);
	success = brasero_vfs_get_info (disc->priv->vfs,
				        uris,
				        TRUE,
				        GNOME_VFS_FILE_INFO_DEFAULT,
				        disc->priv->find_first_graft_parent,
				        graft);
	g_list_free (uris);
	g_free (parent);

	/* NOTE: no need to increase activity */
	if (!success) {
		/* it failed so cancel everything */
		brasero_data_disc_reset_real (disc);

		/* warn the user */
		brasero_data_disc_load_error_dialog (disc);

		/* callback_data must be freed */
		graft->keep = 0;
	}
}

static void
brasero_data_disc_graft_check_existence (BraseroVFS *self,
					 GObject *owner,
					 GnomeVFSResult result,
					 const gchar *uri,
					 GnomeVFSFileInfo *info,
					 gpointer callback_data)
{
	BraseroDataDisc *disc = BRASERO_DATA_DISC (owner);
	BraseroCheckGraftResultData *graft;
	gboolean success;
	gchar *parent;
	GList *uris;

	graft = callback_data;

	/* check that this uri doesn't collide with an existing uri */
	if (result != GNOME_VFS_ERROR_NOT_FOUND) {
		BraseroCheckGraftResult status;

		if (info->type == GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK) {
			/* we ignore this path since when the symlink is met
			 * we'll check its path doesn't overlap a graft point
			 * and if so, nothing will happen */
			graft->keep = 0;
			return;
		}

		if (info->type == GNOME_VFS_FILE_TYPE_DIRECTORY)
			status = BRASERO_GRAFT_CHECK_DIR_WITH_SAME_NAME;
		else
			status = BRASERO_GRAFT_CHECK_FILE_WITH_SAME_NAME;

		brasero_data_disc_graft_check_result (disc, graft, status, uri);
		return;
	}

	if (!disc->priv->check_parent_graft)
		disc->priv->check_parent_graft = brasero_vfs_register_data_type (disc->priv->vfs,
										 G_OBJECT (disc),
										 G_CALLBACK (brasero_data_disc_graft_check_parent),
										 brasero_data_disc_graft_check_destroy);

	/* now we check the existence of a proper parent */
	parent = g_path_get_dirname (uri);
	uris = g_list_prepend (NULL, parent);
	success = brasero_vfs_get_info (disc->priv->vfs,
				        uris,
				        TRUE,
				        GNOME_VFS_FILE_INFO_DEFAULT,
				        disc->priv->check_parent_graft,
				        graft);
	g_list_free (uris);
	g_free (parent);

	/* NOTE: no need to increase activity */
	if (!result) {
		/* it failed so cancel everything */
		brasero_data_disc_reset_real (disc);

		/* warn the user */
		brasero_data_disc_load_error_dialog (disc);

		/* callback_data must be freed */
		graft->keep = 0;
	}
}

/* This function checks graft point consistency:
 * First, when graft points are added as children of another graft points
 * we need to make sure that a child file of the top graft points on the 
 * file system doesn't have the same name as the child graft point.
 * If so, we exclude child file.
 * A second problem might be that one of the parent directory doesn't exist
 * on the file system. we'll have therefore to add empty directories */
static void
brasero_data_disc_graft_check (BraseroDataDisc *disc,
			       const gchar *path)
{
	GList *uris;
	gchar *parent;
	gboolean result;
	const gchar *graft_uri;
	const gchar *graft_path;
	BraseroCheckGraftResultData *graft;

	/* we make sure that the path has at least one parent graft path,
	 * except of course if it is at root */
	/* search for the first parent graft path available */
	parent = g_path_get_dirname (path);
	graft_path = brasero_data_disc_graft_get_real (disc, parent);
	g_free (parent);

	if (!graft_path) {
		/* no parent (maybe it was unreadable) so we need to 
		 * create empty directories but we don't need to check
		 * if it overlaps anything */
		brasero_data_disc_path_create (disc, path);
		return;
	}

	graft_uri = g_hash_table_lookup (disc->priv->paths, graft_path);
	if (!graft_uri
	||   graft_uri == BRASERO_CREATED_DIR
	||   graft_uri == BRASERO_IMPORTED_FILE) {
		/* NOTE: we make sure in this case that this directory
		 * is the direct parent of our path, otherwise create 
		 * the missing parent directories. One graft parent
		 * in between could have gone unreadable. */
		brasero_data_disc_path_create (disc, path);
		return;
	}

	/* make sure that the parent is still a directory */
	if (!g_hash_table_lookup (disc->priv->dirs, graft_uri)) {
		/* HOUSTON we've got a problem */

		return;
	}

	graft = g_new0 (BraseroCheckGraftResultData, 1);
	graft->keep = 1;
	graft->path = g_strdup (path);
	graft->ref = brasero_data_disc_reference_new (disc, path);
	parent = g_strconcat (graft_uri,
			      path + strlen (graft_path),
			      NULL);

	if (!disc->priv->check_graft)
		disc->priv->check_graft = brasero_vfs_register_data_type (disc->priv->vfs,
									  G_OBJECT (disc),
									  G_CALLBACK (brasero_data_disc_graft_check_existence),
									  brasero_data_disc_graft_check_destroy);

	uris = g_list_prepend (NULL, parent);
	result = brasero_vfs_get_info (disc->priv->vfs,
				       uris,
				       TRUE,
				       GNOME_VFS_FILE_INFO_DEFAULT,
				       disc->priv->check_graft,
				       graft);
	g_free (parent);
	g_list_free (uris);

	if (!result) {
		/* clean */
		brasero_data_disc_reference_free (disc, graft->ref);
		g_free (graft->path);
		g_free (graft);

		/* it failed so cancel everything */
		brasero_data_disc_reset_real (disc);

		/* warn the user */
		brasero_data_disc_load_error_dialog (disc);
	}
	else
		brasero_data_disc_increase_activity_counter (disc);
}

struct _BraseroLoadGraftData {
	GSList *grafts;
	GSList *paths;
};
typedef struct _BraseroLoadGraftData BraseroLoadGraftData;

static void
brasero_data_disc_load_graft_end (GObject *object,
				  gpointer callback_data,
				  gboolean cancelled)
{
	BraseroLoadGraftData *data = callback_data;
	BraseroDataDisc *disc = BRASERO_DATA_DISC (object);

	if (data->paths) {
		GSList *iter;

		for (iter = data->paths; iter; iter = iter->next) {
			gchar *path;

			path = iter->data;
			brasero_data_disc_graft_check (disc, path);
			g_free (path);
		}

		g_slist_free (data->paths);
	}

	/* clean everything */
	g_slist_foreach (data->grafts, (GFunc) brasero_graft_point_free, NULL);
	g_slist_free (data->grafts);

	g_free (data);

	disc->priv->is_loading = FALSE;
	brasero_data_disc_decrease_activity_counter (disc);
	brasero_data_disc_selection_changed (disc, (g_hash_table_size (disc->priv->paths) > 0));
}

static void
brasero_data_disc_load_graft_result (BraseroVFS *self,
				     GObject *owner,
				     GnomeVFSResult result,
				     const gchar *uri,
				     GnomeVFSFileInfo *info,
				     gpointer callback_data)
{
	gchar *parent;
	BraseroGraftPt *graft;
	BraseroDiscResult success;
	BraseroLoadGraftData *data = callback_data;
	BraseroDataDisc *disc = BRASERO_DATA_DISC (owner);

	/* see if the parent was added to the tree at the root
	 * of the disc or if it's itself at the root of the disc.
	 * if so, show it in the tree */
	graft = data->grafts->data;
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
	else if (!strcmp (parent, G_DIR_SEPARATOR_S))
		brasero_data_disc_tree_new_path (disc,
						 graft->path,
						 NULL,
						 NULL);

	/* the following function will create a graft point */
	success = brasero_data_disc_new_row_real (disc,
						  graft->uri,
						  info,
						  result,
						  graft->path,
						  graft->excluded);

	if (success == BRASERO_DISC_OK) {
		gchar *name;

		if (strlen (info->name) > 64)
			brasero_data_disc_joliet_incompat_add_uri (disc, graft->uri);
		
		BRASERO_GET_BASENAME_FOR_DISPLAY (graft->path, name);
		if (strlen (name) > 64)
			brasero_data_disc_joliet_incompat_add_path (disc, graft->path);

		g_free (name);

		/* This is for additional checks (see above function) */
		if (strcmp (parent, G_DIR_SEPARATOR_S))
			data->paths = g_slist_prepend (data->paths,
						       g_strdup (graft->path));
	}

	data->grafts = g_slist_remove (data->grafts, graft);
	brasero_graft_point_free (graft);
	g_free (parent);
}

/* we now check if the graft points are still valid files. */
static void
brasero_data_disc_load_restored_end (GObject *owner,
				     gpointer callback_data,
				     gboolean cancelled)
{
	BraseroDataDisc *disc = BRASERO_DATA_DISC (owner);
	GSList *grafts = callback_data;
	BraseroLoadGraftData *data;
	GSList *paths = NULL;
	GSList *iter, *next;
	GList *uris = NULL;
	gboolean success;

	if (!disc->priv->load_graft)
		disc->priv->load_graft = brasero_vfs_register_data_type (disc->priv->vfs,
									 G_OBJECT (disc),
									 G_CALLBACK (brasero_data_disc_load_graft_result),
									 brasero_data_disc_load_graft_end);

	for (iter = grafts; iter; iter = next) {
		BraseroGraftPt *graft;

		graft = iter->data;
		next = iter->next;

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
			if (!strcmp (parent, G_DIR_SEPARATOR_S)) {
				/* we can expose its contents right away (won't be explored) */
				if (brasero_data_disc_tree_new_empty_folder_real (disc,
										  graft->path,
										  ROW_EXPLORED,
										  FALSE))
					brasero_data_disc_expose_path (disc, graft->path);
			}
			else if (g_hash_table_lookup (disc->priv->paths, parent)) {
				gchar *tmp;

				tmp = parent;
				parent = g_path_get_dirname (tmp);
				g_free (tmp);

				if (!strcmp (parent, G_DIR_SEPARATOR_S))
					brasero_data_disc_tree_new_empty_folder_real (disc,
										      graft->path,
										      ROW_NOT_EXPLORED,
										      FALSE);
			}

			/* NOTE: joliet is checked in results */

			/* This is for additional checks (see above function) */
			if (strcmp (parent, G_DIR_SEPARATOR_S))
				paths = g_slist_prepend (paths, g_strdup (graft->path));

			grafts = g_slist_remove (grafts, graft);
			brasero_graft_point_free (graft);
			g_free (parent);
		}
		else
			uris = g_list_prepend (uris, graft->uri);
	}

	data = g_new0 (BraseroLoadGraftData, 1);
	data->grafts = grafts;
	data->paths = paths;

	uris = g_list_reverse (uris);
	success = brasero_vfs_get_info (disc->priv->vfs,
				        uris,
				        TRUE,
				        GNOME_VFS_FILE_INFO_GET_ACCESS_RIGHTS|
				        GNOME_VFS_FILE_INFO_GET_MIME_TYPE|
				        GNOME_VFS_FILE_INFO_FORCE_SLOW_MIME_TYPE,
				        disc->priv->load_graft,
				        data);
	g_list_free (uris);

	/* NOTE: no need to increase activity was already done */
	if (!success) {
		/* clean */
		g_slist_foreach (data->grafts, (GFunc) brasero_graft_point_free, NULL);
		g_slist_free (data->grafts);

		g_slist_foreach (data->paths, (GFunc) g_free, NULL);
		g_slist_free (data->grafts);

		g_free (data);

		/* warn the user */
		brasero_data_disc_load_error_dialog (disc);
		brasero_data_disc_reset_real (disc);
	}
	else
		brasero_data_disc_increase_activity_counter (disc);
}

static void
brasero_data_disc_load_restored_result (BraseroVFS *self,
					GObject *owner,
					GnomeVFSResult result,
					const gchar *uri,
					GnomeVFSFileInfo *info,
					gpointer callback_data)
{
	BraseroDataDisc *disc = BRASERO_DATA_DISC (owner);

	/* see if restored file are still valid. If so, add them to restored hash */
	if (result == GNOME_VFS_ERROR_NOT_FOUND)
		return;

	brasero_data_disc_restored_new (disc,
					uri,
					BRASERO_FILTER_UNKNOWN);
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
	gchar *uri;
	GSList *iter;
	GList *uris = NULL;
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

	if (!BRASERO_DATA_DISC (disc)->priv->load_restored)
		BRASERO_DATA_DISC (disc)->priv->load_restored = brasero_vfs_register_data_type (BRASERO_DATA_DISC (disc)->priv->vfs,
												G_OBJECT (disc),
												G_CALLBACK (brasero_data_disc_load_restored_result),
												brasero_data_disc_load_restored_end);

	/* add restored : we must make sure that they still exist 
	 * before doing the exploration of graft points so that 
	 * they won't be added to unreadable list */
	for (iter = track->contents.data.restored; iter; iter = iter->next) {
		uri = iter->data;
		uris = g_list_prepend (uris, uri);
	}

	success = brasero_vfs_get_info (BRASERO_DATA_DISC (disc)->priv->vfs,
				        uris,
				        TRUE,
				        GNOME_VFS_FILE_INFO_GET_ACCESS_RIGHTS,
				        BRASERO_DATA_DISC (disc)->priv->load_restored,
				        grafts);
	g_list_free (uris);

	if (success != BRASERO_DISC_OK)
		return success;

	BRASERO_DATA_DISC (disc)->priv->is_loading = TRUE;
	return BRASERO_DISC_LOADING;
}

/******************************* row moving ************************************/
static BraseroDiscResult
brasero_data_disc_restore_row (BraseroDataDisc *disc,
			       const gchar *uri,
			       const gchar *oldpath,
			       const gchar *newpath)
{
	BraseroFile *file;
	gchar *newgraft;
	gchar *oldgraft;

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
		g_warning ("ERROR: This file (%s) must have a graft point.\n", uri);
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
					 const gchar *oldpath,
					 const gchar *newpath)
{
	gchar *oldgraft;

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
					  const gchar *oldpath,
					  const gchar *newpath)
{
	gchar *oldgraft;

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
	BraseroDataDiscReference new_path_ref;
	BraseroDataDiscReference old_parent_ref;
};
typedef struct _MoveRowSimpleFileData MoveRowSimpleFileData;

static void
brasero_data_disc_move_row_simple_file_destroy_cb (GObject *object,
						   gpointer data,
						   gboolean cancelled)
{
	BraseroDataDisc *disc = BRASERO_DATA_DISC (object);
	MoveRowSimpleFileData *callback_data = data;

	brasero_data_disc_decrease_activity_counter (disc);

	brasero_data_disc_reference_free (disc, callback_data->old_parent_ref);
	brasero_data_disc_reference_free (disc, callback_data->new_path_ref);
	g_free (callback_data);
}

static void
brasero_data_disc_move_row_simple_file_cb (BraseroVFS *self,
					   GObject *owner,
					   GnomeVFSResult result,
					   const gchar *uri,
					   GnomeVFSFileInfo *info,
					   gpointer user_data)
{
	BraseroDataDisc *disc = BRASERO_DATA_DISC (owner);
	MoveRowSimpleFileData *callback_data = user_data;
	BraseroFile *file;
	gchar *parenturi;
	gchar *newpath;
	gint64 sectors;
	gchar *parent;
	gchar *graft;

	/* see if the parent still exists and we are still valid */
	parenturi = g_path_get_dirname (uri);
	file = g_hash_table_lookup (disc->priv->dirs, parenturi);
	g_free (parenturi);

	if (!file || file->sectors < 0)
		return;

	if (result == GNOME_VFS_ERROR_NOT_FOUND) {
		brasero_data_disc_remove_uri_from_tree (disc, uri, TRUE);
		brasero_data_disc_add_rescan (disc, file);
		return;
	}

	if (result == GNOME_VFS_ERROR_LOOP) {
		brasero_data_disc_remove_uri_from_tree (disc, uri, TRUE);
		brasero_data_disc_add_rescan (disc, file);
		brasero_data_disc_unreadable_new (disc,
						  g_strdup (uri),
						  BRASERO_FILTER_RECURSIVE_SYM);
	}
	
	if (result != GNOME_VFS_OK
	||  !brasero_data_disc_is_readable (info)) {
		brasero_data_disc_remove_uri_from_tree (disc, uri, TRUE);
		brasero_data_disc_add_rescan (disc, file);
		brasero_data_disc_unreadable_new (disc,
						  g_strdup (uri),
						  BRASERO_FILTER_UNREADABLE);
		return;
	}

	/* it's a simple file. Make a file structure and insert
	 * it in files hash and finally exclude it from its parent */
	newpath = brasero_data_disc_reference_get (disc, callback_data->new_path_ref);
	if (!newpath)
		return;

	sectors = GET_SIZE_IN_SECTORS (info->size);
	brasero_data_disc_file_new (disc,
				    uri,
				    sectors);

	brasero_data_disc_graft_new (disc,
				     uri,
				     newpath);
	g_free (newpath);

	parent = brasero_data_disc_reference_get (disc, callback_data->old_parent_ref);
	if (parent) {
		graft = brasero_data_disc_graft_get (disc, parent);
		g_free (parent);

		brasero_data_disc_exclude_uri (disc, graft, uri);
		g_free (graft);
	}
}

static BraseroDiscResult
brasero_data_disc_move_row_simple_file (BraseroDataDisc *disc,
					const gchar *uri,
					const gchar *oldpath,
					const gchar *newpath)
{
	GList *uris;
	gchar *parent;
	BraseroDiscResult result;
	MoveRowSimpleFileData *callback_data;

	callback_data = g_new0 (MoveRowSimpleFileData, 1);
	callback_data->new_path_ref = brasero_data_disc_reference_new (disc, newpath);

	parent = g_path_get_dirname (oldpath);
	callback_data->old_parent_ref = brasero_data_disc_reference_new (disc, parent);
	g_free (parent);

	if (!disc->priv->move_row)
		disc->priv->move_row = brasero_vfs_register_data_type (disc->priv->vfs,
								       G_OBJECT (disc),
								       G_CALLBACK (brasero_data_disc_move_row_simple_file_cb),
								       brasero_data_disc_move_row_simple_file_destroy_cb);

	/* NOTE: uri in uris are destroyed by calling function */
	uris = g_list_prepend (NULL, (gchar *) uri);
	result = brasero_vfs_get_info (disc->priv->vfs,
				       uris,
				       TRUE,
				       GNOME_VFS_FILE_INFO_GET_ACCESS_RIGHTS,
				       disc->priv->move_row,
				       callback_data);
	g_list_free (uris);

	if (!result)
		return BRASERO_DISC_ERROR_THREAD;

	brasero_data_disc_increase_activity_counter (disc);
	return BRASERO_DISC_OK;
}

#if 0

/* Could be used the day a library allows full editing of previous sessions */
static BraseroDiscResult
brasero_data_disc_move_imported_session_file (BraseroDataDisc *disc,
					      const gchar *oldpath,
					      const gchar *newpath)
{
	BraseroVolFile *file;

	file = brasero_volume_file_from_path (oldpath,
					      disc->priv->session);
	if (!file) {
		g_warning ("Couldn't find session imported file %s\n", oldpath);
		return BRASERO_DISC_NOT_IN_TREE;
	}

	/* move the children graft points */
	brasero_data_disc_graft_children_move (disc,
					       oldpath,
					       newpath);

	if (g_hash_table_lookup (disc->priv->paths, oldpath)) {
		BraseroVolFile *parent;
		gpointer value = NULL;
		gchar *parentpath;

		/* the file was already grafted: check that it wasn't
		 * moved back to its right place in hierarchy. If so
		 * then remove the graft point otherwise just update it.
		 */
		parentpath = g_path_get_dirname (newpath);
		if (strcmp (parentpath, G_DIR_SEPARATOR_S))
			parent = brasero_volume_file_from_path (parentpath,
								disc->priv->session);
		else
			parent = disc->priv->session;

		g_free (parentpath);

		g_hash_table_lookup_extended (disc->priv->paths,
					      oldpath,
					      &value,
					      NULL);
		g_hash_table_remove (disc->priv->paths, value);
		g_free (value);

		if (file->parent == parent)
			return BRASERO_DISC_OK;
	}

	/* Since we're going to create a new graft point for this session file
	 * we need to exclude it. Since it could be a root element (with a graft
	 * point but not excluded) we have to check if it is in the list */
	if (!g_slist_find (disc->priv->session_file_excluded, file))
		disc->priv->session_file_excluded = g_slist_prepend (disc->priv->session_file_excluded,
								     file);

	/* no graft point add one for it */
	g_hash_table_insert (disc->priv->paths,
			     g_strdup (newpath),
			     BRASERO_IMPORTED_FILE);
	return BRASERO_DISC_OK;
}

#endif

static BraseroDiscResult
brasero_data_disc_move_row (BraseroDataDisc *disc,
			    const gchar *oldpath,
			    const gchar *newpath)
{
	BraseroDiscResult result;
	BraseroFile *file;
	gchar *uri;

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
	else if (uri == BRASERO_IMPORTED_FILE)
		return BRASERO_DISC_OK;

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
static GtkTreeViewDropPosition
brasero_data_disc_set_dest_row (BraseroDataDisc *disc,
				gint x,
				gint y)
{
	GtkTreeViewDropPosition pos;
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

	if (sort_dest) {
		GtkTreePath *old_dest = NULL;

		gtk_tree_view_get_drag_dest_row (GTK_TREE_VIEW (disc->priv->tree),
						 &old_dest,
						 NULL);

		/* if the destination is located between a directory 
		 * and its children we change the pos to set it into */
		if (pos != GTK_TREE_VIEW_DROP_BEFORE) {
			gboolean is_directory;
			GtkTreeModel *model;
			GtkTreeIter iter;

			model = disc->priv->sort;

			gtk_tree_model_get_iter (model, &iter, sort_dest);
			gtk_tree_model_get (model, &iter,
					    ISDIR_COL, &is_directory,
					    -1);

			if (pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER && !is_directory)
				pos = GTK_TREE_VIEW_DROP_AFTER;
			else if (pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE && !is_directory)
				pos = GTK_TREE_VIEW_DROP_BEFORE;
			else if (pos == GTK_TREE_VIEW_DROP_AFTER &&
				 gtk_tree_view_row_expanded (GTK_TREE_VIEW (disc->priv->tree), sort_dest) &&
				 is_directory)
				pos = GTK_TREE_VIEW_DROP_INTO_OR_AFTER;
			else if (pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE && is_directory)
				pos = GTK_TREE_VIEW_DROP_BEFORE;
		}

		if (old_dest
		&&  sort_dest
		&&  disc->priv->expand_timeout
		&&  (gtk_tree_path_compare (old_dest, sort_dest) != 0
		||   pos != GTK_TREE_VIEW_DROP_INTO_OR_AFTER 
		||   pos != GTK_TREE_VIEW_DROP_INTO_OR_BEFORE)) {
			g_source_remove (disc->priv->expand_timeout);
			disc->priv->expand_timeout = 0;
		}

		if (old_dest)
			gtk_tree_path_free (old_dest);

		gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (disc->priv->tree),
						 sort_dest,
						 pos);
	}
	else {
		gint n_children;
		GtkTreeModel *sort;

		sort = disc->priv->sort;
		n_children = gtk_tree_model_iter_n_children (sort, NULL);
		if (n_children) {
			pos = GTK_TREE_VIEW_DROP_AFTER;
			sort_dest = gtk_tree_path_new_from_indices (n_children, -1);
		}
		else {
			/* NOTE: this case shouldn't exist since source = dest
			 * so there must be at least one row */
			pos = GTK_TREE_VIEW_DROP_BEFORE;
			sort_dest = gtk_tree_path_new_from_indices (0, -1);
		}

		gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (disc->priv->tree),
						 sort_dest,
						 GTK_TREE_VIEW_DROP_BEFORE);

		if (disc->priv->expand_timeout) {
			g_source_remove (disc->priv->expand_timeout);
			disc->priv->expand_timeout = 0;
		}
	}

	gtk_tree_path_free (sort_dest);

	return pos;
}

static GtkTreePath *
brasero_data_disc_get_dest_path (BraseroDataDisc *disc, gint x, gint y)
{
	GtkTreeViewDropPosition pos = 0;
	GtkTreePath *dest = NULL, *sort_dest = NULL;

	brasero_data_disc_set_dest_row (disc, x, y);
	gtk_tree_view_get_drag_dest_row (GTK_TREE_VIEW (disc->priv->tree),
					 &sort_dest,
					 &pos);

	if (sort_dest) {
		dest = gtk_tree_model_sort_convert_path_to_child_path (GTK_TREE_MODEL_SORT (disc->priv->sort), sort_dest);
		gtk_tree_path_free (sort_dest);
	}

	/* the following means that this is not a directory */
	if (pos == GTK_TREE_VIEW_DROP_AFTER)
		gtk_tree_path_next (dest);

	/* the following means it's a directory */
	if (pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER
	||  pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE) {
		GtkTreeIter parent;
		guint num_children;

		gtk_tree_model_get_iter (disc->priv->model, &parent, dest);
		num_children = gtk_tree_model_iter_n_children (disc->priv->model, &parent);
		gtk_tree_path_append_index (dest, num_children);
	}

	return dest;
}

static gchar*
brasero_data_disc_new_disc_path (BraseroDataDisc *disc,
				 const gchar *display,
				 GtkTreePath *dest)
{
	gchar *newparentpath = NULL;
	gchar *newpath;

	brasero_data_disc_tree_path_to_disc_path (disc,
						  dest,
						  &newparentpath);

	newpath = g_build_path (G_DIR_SEPARATOR_S,
				newparentpath,
				display,
				NULL);
	g_free (newparentpath);

	return newpath;
}

static GdkDragAction 
brasero_data_disc_drag_dest_drop_row_possible (BraseroDataDisc *disc,
					       GtkTreePath *src,
					       GtkSelectionData *selection_tmp)
{
	gint type;
	GtkTreeIter iter;
	GtkTreePath *dest;
	GtkTreeModel *model;
	GtkTreePath *sort_dest;
	GtkTreePath *src_parent;
	GtkTreePath *dest_parent;
	GtkTreeSelection *selection;
	GtkTreeViewDropPosition pos;
	GdkDragAction action = GDK_ACTION_MOVE;

	model = disc->priv->model;
	src_parent = NULL;
	dest_parent = NULL;

	gtk_tree_view_get_drag_dest_row (GTK_TREE_VIEW (disc->priv->tree),
					 &sort_dest,
					 &pos);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (disc->priv->tree));
	if (sort_dest && gtk_tree_selection_path_is_selected (selection, sort_dest)) {
		gtk_tree_path_free (sort_dest);
		return GDK_ACTION_DEFAULT;
	}

	if (!sort_dest) {
		pos = GTK_TREE_VIEW_DROP_AFTER;
		dest = gtk_tree_path_new_from_indices (gtk_tree_model_iter_n_children (model, NULL) - 1, -1);
	}
	else {
		dest = gtk_tree_model_sort_convert_path_to_child_path (GTK_TREE_MODEL_SORT (disc->priv->sort), sort_dest);
		gtk_tree_path_free (sort_dest);
	}

	/* if we drop into make sure it is a directory */
	if (gtk_tree_model_get_iter (model, &iter, dest)
	&& (pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER || pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE)) {
		gboolean isdir;
		gint explored;

		gtk_tree_model_get (model, &iter,
				    ISDIR_COL, &isdir,
				    ROW_STATUS_COL, &explored,
				    -1);

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

	src_parent = gtk_tree_path_copy (src);
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

	gtk_tree_model_get_iter (model, &iter, src);
	gtk_tree_model_get (model, &iter,
			    ROW_TYPE_COL, &type,
			    -1);

	if (type == ROW_BOGUS) {
		action = GDK_ACTION_DEFAULT;
		goto end;
	}

	gtk_tree_set_row_drag_data (selection_tmp, model, src);
	if (!gtk_tree_drag_dest_row_drop_possible (GTK_TREE_DRAG_DEST (model),
						   dest,
						   selection_tmp))
		action = GDK_ACTION_DEFAULT;

end:
	gtk_tree_path_free (dest);
	gtk_tree_path_free (src_parent);
	gtk_tree_path_free (dest_parent);

	return action;
}

static void
brasero_data_disc_move_to_dest (BraseroDataDisc *disc,
				GtkTreeModel *model,
				GtkTreeRowReference *srcref,
				GtkTreeRowReference *destref,
				GtkSelectionData *selection_data)
{
	GtkTreeSelection *selection;
	BraseroDiscResult result;
	GtkTreePath *sort_path;
	GdkDragAction action;
	GtkTreePath *dest;
	GtkTreePath *src;
	GtkTreeIter row;
	gchar *oldpath;
	gchar *newpath;
	gchar *name;

	/* Make sure one last time it is droppable: we must do it here
	 * in case there are two files (in different directories) with
	 * the same name. That way the first is moved and when the time
	 * has come to move the second we'll notice that there is already
	 * a file with the same name in the target directory */
	src = gtk_tree_row_reference_get_path (srcref);
	action = brasero_data_disc_drag_dest_drop_row_possible (disc,
								src,
								selection_data);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (disc->priv->tree));
	if (action != GDK_ACTION_MOVE) {
		sort_path = gtk_tree_model_sort_convert_child_path_to_path (GTK_TREE_MODEL_SORT (disc->priv->sort), src);
		gtk_tree_selection_unselect_path (selection, sort_path);
		gtk_tree_path_free (sort_path);
		gtk_tree_path_free (src);
		return;
	}

	/* NOTE: dest refers here to the parent of
	 * the destination or NULL if at root */
	if (destref)
		dest = gtk_tree_row_reference_get_path (destref);
	else
		dest = NULL;

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

	/* move it in GtkTreeView */
	gtk_tree_set_row_drag_data (selection_data, model, src);
	gtk_tree_path_free (src);
	src = NULL;

	if (dest) {
		gint nb_children;

		/* open the directory if it isn't */
		sort_path = gtk_tree_model_sort_convert_child_path_to_path (GTK_TREE_MODEL_SORT (disc->priv->sort), dest);
		if (!gtk_tree_view_row_expanded (GTK_TREE_VIEW (disc->priv->tree), sort_path))
			gtk_tree_view_expand_row (GTK_TREE_VIEW (disc->priv->tree), sort_path, FALSE);
		gtk_tree_path_free (sort_path);

		gtk_tree_model_get_iter (model, &row, dest);
		nb_children = gtk_tree_model_iter_n_children (model, &row);
		gtk_tree_path_append_index (dest, nb_children);
	}
	else {
		gint nb_children;

		nb_children = gtk_tree_model_iter_n_children (model, NULL);
		dest = gtk_tree_path_new_from_indices (nb_children, -1);
	}

	if (!gtk_tree_drag_dest_drag_data_received (GTK_TREE_DRAG_DEST (model),
						    dest,
						    selection_data)) {
		brasero_data_disc_move_row (disc,
					    newpath,
					    oldpath);
		goto end;
	}

	/* update parent directories */
	sort_path = gtk_tree_model_sort_convert_child_path_to_path (GTK_TREE_MODEL_SORT (disc->priv->sort), dest);
	gtk_tree_selection_select_path (selection, sort_path);
	gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (disc->priv->tree),
				      sort_path,
				      NULL,
				      TRUE,
				      0.5,
				      0.5);
	gtk_tree_path_free (sort_path);

	src = gtk_tree_row_reference_get_path (srcref);

	gtk_tree_drag_source_drag_data_delete (GTK_TREE_DRAG_SOURCE (model), src);
	brasero_data_disc_tree_update_parent (disc, src);

end:

	if (src)
		gtk_tree_path_free (src);

	if (dest)
		gtk_tree_path_free (dest);

	g_free (oldpath);
	g_free (newpath);
}

static gboolean
brasero_data_disc_native_data_received (BraseroDataDisc *disc,
					GtkSelectionData *selection_data,
					gint x,
					gint y)
{
	GtkTreeRowReference *destref = NULL;
	GtkSelectionData *tmp = NULL;
	GtkTreeModel *model;
	GtkTreePath *dest;
	GSList *iter;

	model = disc->priv->model;
	dest = brasero_data_disc_get_dest_path (disc, x, y);

	/* keep some necessary references for later */
	if (dest && gtk_tree_path_get_depth (dest) > 1) {
		/* we can only put a reference on the parent
		 * since the child doesn't exist yet */
		gtk_tree_path_up (dest);
		destref = gtk_tree_row_reference_new (model, dest);
		gtk_tree_path_free (dest);
	}
	else {
		if (dest)
			gtk_tree_path_free (dest);

		destref = NULL;
	}

	tmp = gtk_selection_data_copy (selection_data);
	for (iter = disc->priv->drag_src; iter; iter = iter->next) {
		GtkTreeRowReference *reference;

		reference = iter->data;
		brasero_data_disc_move_to_dest (disc,
						model,
						reference,
						destref,
						tmp);
	}
	gtk_selection_data_free (tmp);

	if (destref) {
		GtkTreeIter iter;

		dest = gtk_tree_row_reference_get_path (destref);
		if (gtk_tree_model_get_iter (disc->priv->model, &iter, dest)) {
			brasero_data_disc_remove_bogus_child (disc, &iter);
			brasero_data_disc_tree_update_directory_real (disc, &iter);
		}
		gtk_tree_path_free (dest);
	}

	return TRUE;
}

static GdkDragAction
brasero_data_disc_drag_data_received_dragging (BraseroDataDisc *disc,
					       GtkSelectionData *selection_data)
{
	GdkDragAction action = GDK_ACTION_DEFAULT;
	GtkSelectionData *selection_tmp;
	GSList *iter;

	selection_tmp = gtk_selection_data_copy (selection_data);
	for (iter = disc->priv->drag_src; iter; iter = iter->next) {
		GtkTreeRowReference *reference;
		GtkTreePath *treepath;

		reference = iter->data;
		treepath = gtk_tree_row_reference_get_path (reference);
		action = brasero_data_disc_drag_dest_drop_row_possible (disc,
									treepath,
									selection_tmp);
		gtk_tree_path_free (treepath);

		/* we stop if we can simply move at least one */
		if (action == GDK_ACTION_MOVE)
			break;
	}
	gtk_selection_data_free (selection_tmp);

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

		if (!disc->priv->is_loading && disc->priv->drag_src)
			action = brasero_data_disc_drag_data_received_dragging (disc,
										selection_data);
		else
			action = GDK_ACTION_DEFAULT;

		gdk_drag_status (drag_context, action, time);

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

	if (!disc->priv->drag_src
	&& (selection_data->length <= 0 ||  selection_data->format != 8)) {
		gtk_drag_finish (drag_context, FALSE, FALSE, time);
		disc->priv->drag_status = STATUS_NO_DRAG;
		g_signal_stop_emission_by_name (tree, "drag-data-received");
		return;
	}

	/* we get URIS */
	if (info == TARGET_URIS_LIST) {
		GtkTreeSelection *selection;
		gboolean func_results;
		gchar **uri, **uris;
		GtkTreePath *dest;

		uris = gtk_selection_data_get_uris (selection_data);

		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (disc->priv->tree));
		gtk_tree_selection_unselect_all (selection);

		dest = brasero_data_disc_get_dest_path (disc, x, y);
		if (dest)
			gtk_tree_path_up (dest);

		for (uri = uris; *uri != NULL; uri++) {
			GtkTreePath *treepath = NULL;

			func_results = brasero_data_disc_add_uri_real (disc,
								       *uri,
								       dest,
								       &treepath);

			if (func_results == BRASERO_DISC_OK && treepath) {
				GtkTreePath *sort_path;

				sort_path = gtk_tree_model_sort_convert_child_path_to_path (GTK_TREE_MODEL_SORT (disc->priv->sort),
											    treepath);
				gtk_tree_path_free (treepath);

				gtk_tree_selection_select_path (selection, sort_path);
				gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (disc->priv->tree),
							      sort_path,
							      NULL,
							      TRUE,
							      0.5,
							      0.5);
				gtk_tree_path_free (sort_path);
			}

			result = (result ? TRUE : (func_results == BRASERO_DISC_OK));
		}

		gtk_tree_path_free (dest);
		g_strfreev (uris);

		gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (disc->priv->tree),
						 NULL,
						 0);
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

/* in the following functions there are quick and
 * dirty cut'n pastes from gtktreeview.c shame on me */
static gboolean
brasero_data_disc_drag_drop_cb (GtkTreeView *tree,
				GdkDragContext *drag_context,
				gint x,
				gint y,
				guint time,
				BraseroDataDisc *disc)
{
	GdkAtom target = GDK_NONE;

	if (disc->priv->drag_context) {
		g_object_unref (disc->priv->drag_context);
		disc->priv->drag_context = NULL;
	}

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
		return FALSE;
	}

	gtk_drag_finish (drag_context,
			 FALSE,
			 FALSE,
			 time);

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
}

static gboolean
brasero_data_disc_scroll_timeout_cb (BraseroDataDisc *data)
{
	gint y;
	gdouble value;
	gint scroll_area;
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
	GtkTreeViewDropPosition pos;
	GtkTreePath *sort_dest = NULL;

	gtk_tree_view_get_drag_dest_row (GTK_TREE_VIEW (disc->priv->tree),
					 &sort_dest,
					 &pos);

	/* we don't need to check if it's a directory because:
	   - a file wouldn't have children anyway
	   - we check while motion if it's a directory and if not remove the INTO from pos */

	if (sort_dest
	&& (pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER || pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE)) {
		if (!gtk_tree_view_row_expanded (GTK_TREE_VIEW (disc->priv->tree), sort_dest))
			gtk_tree_view_expand_row (GTK_TREE_VIEW (disc->priv->tree),
						  sort_dest,
						  FALSE);

		disc->priv->expand_timeout = 0;
		gtk_tree_path_free (sort_dest);
		result = FALSE;
	}
	else {
		if (sort_dest)
			gtk_tree_path_free (sort_dest);
	
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
	GtkTreeViewDropPosition pos;

	g_signal_stop_emission_by_name (tree, "drag-motion");

	target = gtk_drag_dest_find_target (tree,
					    drag_context,
					    gtk_drag_dest_get_target_list (tree));

	/* see if we accept anything */
	if (target == GDK_NONE
	||  disc->priv->is_loading
	|| (disc->priv->reject_files && target != gdk_atom_intern ("GTK_TREE_MODEL_ROW", FALSE))) {
		if (disc->priv->expand_timeout) {
			g_source_remove (disc->priv->expand_timeout);
			disc->priv->expand_timeout = 0;
		}

		gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (tree),
						 NULL,
						 GTK_TREE_VIEW_DROP_BEFORE);
		gdk_drag_status (drag_context, GDK_ACTION_DEFAULT, time);
		return TRUE;
	}

	pos = brasero_data_disc_set_dest_row (disc, x, y);

	/* since we mess with the model we have to re-implement the following two */
	if (!disc->priv->expand_timeout
	&&  (pos == GTK_TREE_VIEW_DROP_INTO_OR_AFTER || pos == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE)) {
		disc->priv->expand_timeout = g_timeout_add (500,
							    (GSourceFunc) brasero_data_disc_expand_timeout_cb,
							    disc);
	}
	else if (!disc->priv->scroll_timeout) {
		disc->priv->scroll_timeout = g_timeout_add (150,
							    (GSourceFunc) brasero_data_disc_scroll_timeout_cb,
							    disc);
	}

	if (target == gdk_atom_intern ("GTK_TREE_MODEL_ROW", FALSE)) {
		if (disc->priv->drag_context)
			gtk_drag_set_icon_default (disc->priv->drag_context);
	
		gtk_drag_get_data (tree,
				   drag_context,
				   target,
				   time);
	}
	else
		gdk_drag_status (drag_context,
				 drag_context->suggested_action,
				 time);

	return TRUE;
}

static void
brasero_data_disc_drag_leave_cb (GtkWidget *tree,
				 GdkDragContext *drag_context,
				 guint time,
				 BraseroDataDisc *disc)
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

	g_signal_stop_emission_by_name (tree, "drag-leave");
}

static void
brasero_data_disc_drag_begin_cb (GtkTreeView *tree,
				 GdkDragContext *drag_context,
				 BraseroDataDisc *disc)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model, *sort;
	GList *selected;
	GList *iter;

	disc->priv->drag_context = drag_context;
	g_object_ref (drag_context);

	model = disc->priv->model;
	sort = disc->priv->sort;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (disc->priv->tree));
	selected = gtk_tree_selection_get_selected_rows (selection, NULL);
	for (iter = selected; iter; iter = iter->next) {
		gint type;
		GtkTreeIter row;
		GtkTreePath *src, *treepath;
		GtkTreeRowReference *reference;

		treepath = iter->data;
		src = gtk_tree_model_sort_convert_path_to_child_path (GTK_TREE_MODEL_SORT (sort),
								      treepath);
		gtk_tree_path_free (treepath);

		gtk_tree_model_get_iter (model, &row, src);
		gtk_tree_model_get (model, &row,
				    ROW_TYPE_COL, &type,
				    -1);

		if (type == ROW_BOGUS || type == ROW_SESSION) {
			gtk_tree_path_free (src);
			continue;
		}

		reference = gtk_tree_row_reference_new (model, src);
		disc->priv->drag_src = g_slist_prepend (disc->priv->drag_src,
							reference);
	}
	g_list_free (selected);

	g_signal_stop_emission_by_name (tree, "drag-begin");
	disc->priv->drag_status = STATUS_DRAGGING;
}

static void
brasero_data_disc_drag_end_cb (GtkWidget *tree,
			       GdkDragContext *drag_context,
			       BraseroDataDisc *disc)
{
	gint x, y;

	if (disc->priv->drag_context) {
		g_object_unref (disc->priv->drag_context);
		disc->priv->drag_context = NULL;
	}

	g_signal_stop_emission_by_name (tree, "drag-end");

	gtk_widget_get_pointer (tree, &x, &y);
	if (x < 0 || y < 0 || x > tree->allocation.width || y > tree->allocation.height)
		brasero_data_disc_delete_selected (BRASERO_DISC (disc));

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

	if (disc->priv->drag_src) {
		g_slist_foreach (disc->priv->drag_src,
				(GFunc) gtk_tree_row_reference_free,
				 NULL);
		g_slist_free (disc->priv->drag_src);
		disc->priv->drag_src = NULL;
	}
}

/**************************** MENUS ********************************************/
static void
brasero_data_disc_open_file (BraseroDataDisc *disc, GList *list)
{
	gint type;
	gchar *uri;
	gchar *path;
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

		gtk_tree_model_get (sort, &iter,
				    ROW_TYPE_COL, &type,
				    -1);
		if (type == ROW_BOGUS || type == ROW_SESSION) {
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
	gint type;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (disc->priv->tree));
	model = disc->priv->sort;

	list = gtk_tree_selection_get_selected_rows (selection, &model);
	for (; list; list = g_list_remove (list, treepath)) {
		treepath = list->data;

		gtk_tree_model_get_iter (model, &iter, treepath);
		gtk_tree_model_get (model, &iter,
				    ROW_TYPE_COL, &type,
				    -1);

		if (type == ROW_BOGUS || type == ROW_SESSION) {
			gtk_tree_path_free (treepath);
			continue;
		}

		column = gtk_tree_view_get_column (GTK_TREE_VIEW (disc->priv->tree),
						   0);

		/* grab focus must be called before next function to avoid
		 * triggering a bug where if pointer is not in the widget 
		 * any more and enter is pressed the cell will remain editable */
		gtk_widget_grab_focus (disc->priv->tree);
		gtk_tree_view_set_cursor_on_cell (GTK_TREE_VIEW (disc->priv->tree),
						  treepath,
						  column,
						  NULL,
						  TRUE);

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
	gchar **array;
	gchar **item;

	model = data->disc->priv->sort;
	if (data->reference) {
		parent = gtk_tree_row_reference_get_path (data->reference);
		gtk_tree_model_get_iter (model, &row, parent);
	}

	array = g_strsplit_set (text, "\n\r", 0);
	item = array;
	while (*item) {
		if (**item != '\0') {
			gchar *uri;

			if (parent) {
				treepath = gtk_tree_path_copy (parent);
				gtk_tree_path_append_index (treepath,
							    gtk_tree_model_iter_n_children
							    (model, &row));
			}

			uri = gnome_vfs_make_uri_from_input (*item);
			brasero_data_disc_add_uri_real (data->disc,
							uri,
							treepath,
							NULL);
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
	gchar *target;

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
brasero_data_disc_tree_select_function (GtkTreeSelection *selection,
					GtkTreeModel *model,
					GtkTreePath *treepath,
					gboolean is_selected,
					gpointer null_data)
{
	GtkTreeIter row;
	gint type;

	if (gtk_tree_model_get_iter (model, &row, treepath)) {
		gtk_tree_model_get (model, &row,
				    ROW_TYPE_COL, &type,
				    -1);

		if (type == ROW_BOGUS || type == ROW_SESSION) {
			if (is_selected)
				return TRUE;

			return FALSE;
		}
	}

	return TRUE;
}

void
brasero_data_disc_show_menu (int nb_selected,
			     GtkUIManager *manager,
			     GdkEventButton *event)
{
	GtkWidget *item;

	if (nb_selected == 1) {
		item = gtk_ui_manager_get_widget (manager, "/ContextMenu/OpenFile");
		if (item)
			gtk_widget_set_sensitive (item, TRUE);
		item = gtk_ui_manager_get_widget (manager, "/ContextMenu/RenameData");
		if (item)
			gtk_widget_set_sensitive (item, TRUE);
		item = gtk_ui_manager_get_widget (manager, "/ContextMenu/DeleteData");
		if (item)
			gtk_widget_set_sensitive (item, TRUE);
	}
	else if (!nb_selected) {
		item = gtk_ui_manager_get_widget (manager, "/ContextMenu/OpenFile");
		if (item)
			gtk_widget_set_sensitive (item, FALSE);

		item = gtk_ui_manager_get_widget (manager, "/ContextMenu/RenameData");
		if (item)
			gtk_widget_set_sensitive (item, FALSE);
		item = gtk_ui_manager_get_widget (manager, "/ContextMenu/DeleteData");
		if (item)
			gtk_widget_set_sensitive (item, FALSE);
	}
	else {
		item = gtk_ui_manager_get_widget (manager, "/ContextMenu/OpenFile");
		if (item)
			gtk_widget_set_sensitive (item, TRUE);
		item = gtk_ui_manager_get_widget (manager, "/ContextMenu/RenameData");
		if (item)
			gtk_widget_set_sensitive (item, FALSE);
		item = gtk_ui_manager_get_widget (manager, "/ContextMenu/DeleteData");
		if (item)
			gtk_widget_set_sensitive (item, TRUE);
	}

	item = gtk_ui_manager_get_widget (manager, "/ContextMenu/PasteData");
	if (item) {
		if (gtk_clipboard_wait_is_text_available (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD)))
			gtk_widget_set_sensitive (item, TRUE);
		else
			gtk_widget_set_sensitive (item, FALSE);
	}

	item = gtk_ui_manager_get_widget (manager,"/ContextMenu");
	gtk_menu_popup (GTK_MENU (item),
		        NULL,
			NULL,
			NULL,
			NULL,
			event->button,
			event->time);
}

static gboolean
brasero_data_disc_button_pressed_cb (GtkTreeView *tree,
				     GdkEventButton *event,
				     BraseroDataDisc *disc)
{
	GtkTreePath *treepath = NULL;
	gboolean result;

	GtkWidgetClass *widget_class;

	if (GTK_WIDGET_REALIZED (disc->priv->tree)) {
		GtkTreeIter row;

		result = gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (disc->priv->tree),
							event->x,
							event->y,
							&treepath,
							NULL,
							NULL,
							NULL);

		if (treepath
		&&  gtk_tree_model_get_iter (disc->priv->sort, &row, treepath)) {
			gint type;

			gtk_tree_model_get (disc->priv->sort, &row,
					    ROW_TYPE_COL, &type,
					    -1);

			/* nothing should happen on this type of rows */
			if (type == ROW_BOGUS) {
				gtk_tree_path_free (treepath);
				return TRUE;
			}
		}
	}
	else
		result = FALSE;

	/* we call the default handler for the treeview before everything else
	 * so it can update itself (particularly its selection) before we use it
	 * NOTE: since the event has been processed here we need to return TRUE
	 * to avoid having the treeview processing this event a second time. */
	widget_class = GTK_WIDGET_GET_CLASS (tree);
	widget_class->button_press_event (GTK_WIDGET (tree), event);

	if (disc->priv->is_loading) {
		gtk_tree_path_free (treepath);
		return TRUE;
	}

	if ((event->state & (GDK_CONTROL_MASK|GDK_SHIFT_MASK)) == 0) {
		if (disc->priv->selected_path)
			gtk_tree_path_free (disc->priv->selected_path);

		disc->priv->selected_path = NULL;

		if (result && treepath) {
			GtkTreeModel *model;
			GtkTreeIter iter;
			gint type;

			/* we need to make sure that this is not a bogus row */
			model = disc->priv->sort;
			gtk_tree_model_get_iter (model, &iter, treepath);
			gtk_tree_model_get (model, &iter,
					    ROW_TYPE_COL, &type,
					    -1);

			if (type != ROW_BOGUS && type != ROW_SESSION) {
				if (event->state & GDK_CONTROL_MASK)
					disc->priv->selected_path = gtk_tree_path_copy (treepath);
				else if ((event->state & GDK_SHIFT_MASK) == 0)
					disc->priv->selected_path = gtk_tree_path_copy (treepath);
			}
		}

		brasero_disc_selection_changed (BRASERO_DISC (disc));
	}

	if (event->button == 1) {
		disc->priv->press_start_x = event->x;
		disc->priv->press_start_y = event->y;

		if (event->type == GDK_2BUTTON_PRESS) {
			if (treepath) {
				GList *list;

				list = g_list_prepend (NULL, gtk_tree_path_copy (treepath));
				brasero_data_disc_open_file (disc, list);
				g_list_free (list);
			}
		}
	}
	else if (event->button == 3) {
		GtkTreeSelection *selection;

		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (disc->priv->tree));
		brasero_data_disc_show_menu (gtk_tree_selection_count_selected_rows (selection),
					     disc->priv->manager,
					     event);
	}

	gtk_tree_path_free (treepath);

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
	gchar *oldpath;
	gchar *newpath;
	gchar *parent;
	gchar *name = NULL;

	disc->priv->editing = 0;

	sort = disc->priv->sort;
	path = gtk_tree_path_new_from_string (path_string);

	/* see if this is still a valid path. It can happen a user removes it
	 * while the name of the row is being edited */
	if (!gtk_tree_model_get_iter (disc->priv->sort, &row, path)) {
		gtk_tree_path_free (path);
		return;
	}

	realpath = gtk_tree_model_sort_convert_path_to_child_path (GTK_TREE_MODEL_SORT(sort),
								   path);
	gtk_tree_path_free (path);

	model = disc->priv->model;
	if (!gtk_tree_model_get_iter (model, &row, realpath))
		goto end;

	gtk_tree_model_get (model, &row, NAME_COL, &name, -1);

	/* make sure it actually changed */
	if (!strcmp (name, text))
		goto end;

	/* make sure there isn't the same name in the directory and it is joliet
	 * compatible.
	 * NOTE: this has to be a UTF8 name since it's a GTK+ gift */
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
			    NAME_COL, text,
			    -1);

	g_free (parent);
	g_free (oldpath);
	g_free (newpath);

	brasero_data_disc_selection_changed (disc, TRUE);

end:
	g_free (name);
	gtk_tree_path_free (realpath);
}

/*******************************            ************************************/
static gboolean
brasero_data_disc_get_selected_uri (BraseroDisc *disc,
				    gchar **uri)
{
	gchar *path;
	GtkTreePath *realpath;
	BraseroDataDisc *data;

	data = BRASERO_DATA_DISC (disc);

	if (!data->priv->selected_path)
		return FALSE;

	if (!uri)
		return TRUE;

	realpath = gtk_tree_model_sort_convert_path_to_child_path (GTK_TREE_MODEL_SORT (data->priv->sort),
								   data->priv->selected_path);
	brasero_data_disc_tree_path_to_disc_path (data, realpath, &path);
	gtk_tree_path_free (realpath);

	*uri = brasero_data_disc_path_to_uri (data, path);
	if (*uri == BRASERO_IMPORTED_FILE) {
		*uri = NULL;
		return TRUE;
	}

	g_free (path);
	return TRUE;
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
		brasero_data_disc_joliet_incompat_remove_path (disc, path);
		brasero_data_disc_reference_remove_path (disc, path);
	}

	g_slist_foreach (paths, (GFunc) g_free, NULL);
	g_slist_free (paths);
}

static void
brasero_data_disc_inotify_create_file_event_destroy_cb (GObject *object,
							gpointer callback_data,
							gboolean cancelled)
{
	BraseroDataDisc *disc = BRASERO_DATA_DISC (object);
	GSList *references = callback_data;

	brasero_data_disc_reference_free_list (disc, references);
	brasero_data_disc_decrease_activity_counter (disc);
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
		BraseroDirectoryContents *contents;
		BraseroDirectoryEntry *entry;

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
		/* FIXME: we don't allow to check a possible new UTF8 name
		 * against the other filenames in the directory */
		contents = g_new0 (BraseroDirectoryContents, 1);
		contents->uri = g_path_get_dirname (uri);

		entry = g_new0 (BraseroDirectoryEntry, 1);
		entry->uri =  (gchar *) uri;
		entry->info = info;

		paths = brasero_data_disc_symlink_new (disc,
						       contents,
						       entry,
						       paths);

		g_free (entry);

		g_free (contents->uri);
		g_free (contents);
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

		/* This is a create event so the file doesn't exist yet in all 
		 * the tables: check its name. */
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

static void
brasero_data_disc_inotify_create_file_event_cb (BraseroVFS *self,
					        GObject *owner,
					        GnomeVFSResult result,
					        const gchar *uri,
					        GnomeVFSFileInfo *info,
					        gpointer callback_data)
{
	gchar *name = NULL;
	GSList *paths = NULL;
	GSList *references = callback_data;
	BraseroDataDisc *disc = BRASERO_DATA_DISC (owner);

	/* NOTE: there is just one URI */
	if (disc->priv->unreadable
	&&  g_hash_table_lookup (disc->priv->unreadable, uri)) {
		brasero_data_disc_remove_uri (disc, uri, TRUE);
		goto cleanup;
	}

	if (result == GNOME_VFS_ERROR_NOT_FOUND) {
		brasero_data_disc_remove_uri (disc, uri, TRUE);
		goto cleanup;
	}

	if (info->type == GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK) {
		brasero_data_disc_remove_uri (disc, uri, TRUE);
		brasero_data_disc_unreadable_new (disc,
						  g_strdup (uri),
						  BRASERO_FILTER_BROKEN_SYM);
		goto cleanup;
	}

	if (result == GNOME_VFS_ERROR_LOOP) {
		brasero_data_disc_remove_uri (disc, uri, TRUE);
		brasero_data_disc_unreadable_new (disc,
						  g_strdup (uri),
						  BRASERO_FILTER_RECURSIVE_SYM);
		goto cleanup;
	}

	if (result != GNOME_VFS_OK
	|| !brasero_data_disc_is_readable (info)) {
		brasero_data_disc_remove_uri (disc, uri, TRUE);
		brasero_data_disc_unreadable_new (disc,
						  g_strdup (uri),
						  BRASERO_FILTER_UNREADABLE);
		goto cleanup;
	}

	/* make sure we still care about this change. At least that it wasn't 
	 * put in the unreadable hash */
	if (brasero_data_disc_is_excluded (disc, uri, NULL))
		goto cleanup;

	/* check that this file is not hidden */
	BRASERO_GET_BASENAME_FOR_DISPLAY (uri, name);
	if (name [0] == '.') {
		brasero_data_disc_unreadable_new (disc,
						  g_strdup (uri),
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

	/* add it to joliet incompatible list if need be */
	if (strlen (info->name) > 64)
		brasero_data_disc_joliet_incompat_add_uri (disc, uri);

	brasero_data_disc_inotify_create_paths (disc,
						name,
						paths,
						uri,
						info);
	g_free (name);

cleanup:

	if (paths) {
		g_slist_foreach (paths, (GFunc) g_free, NULL);
		g_slist_free (paths);
	}
}

static void
brasero_data_disc_inotify_create_file_event (BraseroDataDisc *disc,
					     BraseroFile *parent,
					     const gchar *uri)
{
	GList *uris;
	GSList *paths, *iter;
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
	if (!disc->priv->create_file)
		disc->priv->create_file = brasero_vfs_register_data_type (disc->priv->vfs,
									  G_OBJECT (disc),
									  G_CALLBACK (brasero_data_disc_inotify_create_file_event_cb),
									  brasero_data_disc_inotify_create_file_event_destroy_cb);

	/* NOTE: uri in uris are destroyed by calling function */
	uris = g_list_prepend (NULL, (gchar *) uri);
	result = brasero_vfs_get_info (disc->priv->vfs,
				       uris,
				       TRUE,
				       GNOME_VFS_FILE_INFO_GET_ACCESS_RIGHTS |
				       GNOME_VFS_FILE_INFO_GET_MIME_TYPE |
				       GNOME_VFS_FILE_INFO_FORCE_SLOW_MIME_TYPE,
				       disc->priv->create_file,
				       references);
	g_list_free (uris);

	if (!result) {
		brasero_data_disc_unreadable_new (disc,
						  g_strdup (uri),
						  BRASERO_FILTER_UNREADABLE);
		return;
	}

	brasero_data_disc_increase_activity_counter (disc);
}

static void
brasero_data_disc_inotify_attributes_event_cb (BraseroVFS *self,
					       GObject *owner,
					       GnomeVFSResult result,
					       const gchar *uri,
					       GnomeVFSFileInfo *info,
					       gpointer callback_data)
{
	BraseroDataDisc *disc = BRASERO_DATA_DISC (owner);
	BraseroFilterStatus status;

	if (result == GNOME_VFS_OK
	&&  brasero_data_disc_is_readable (info)) {
		if (disc->priv->unreadable
		&& (status = GPOINTER_TO_INT (g_hash_table_lookup (disc->priv->unreadable, uri)))
		&&  status == BRASERO_FILTER_UNREADABLE) {
			brasero_data_disc_unreadable_free (disc, uri);
			brasero_data_disc_inotify_create_file_event_cb (self,
									owner,
									result,
									uri,
									info,
									NULL);
		}		
		return;
	}

	/* the file couldn't be a symlink anyway don't check for loop */
	brasero_data_disc_remove_uri (disc, uri, TRUE);
	if (result != GNOME_VFS_ERROR_NOT_FOUND)
		brasero_data_disc_unreadable_new (disc,
						  g_strdup (uri),
						  BRASERO_FILTER_UNREADABLE);
}

static void
brasero_data_disc_inotify_attributes_event_destroy_cb (GObject *object,
						       gpointer callback_data,
						       gboolean cancelled)
{
	BraseroDataDisc *disc = BRASERO_DATA_DISC (object);

	brasero_data_disc_decrease_activity_counter (disc);
}

static void
brasero_data_disc_inotify_attributes_event (BraseroDataDisc *disc,
					    const gchar *uri)
{
	GList *uris;
	gboolean result;

	if (!disc->priv->attr_changed)
		disc->priv->attr_changed = brasero_vfs_register_data_type (disc->priv->vfs,
									   G_OBJECT (disc),
									   G_CALLBACK (brasero_data_disc_inotify_attributes_event_cb),
									   brasero_data_disc_inotify_attributes_event_destroy_cb);

	/* NOTE: uri in uris are destroyed by calling function */
	uris = g_list_prepend (NULL, (gchar *) uri);
	result = brasero_vfs_get_info (disc->priv->vfs,
				       uris,
				       TRUE,
				       GNOME_VFS_FILE_INFO_GET_ACCESS_RIGHTS,
				       disc->priv->attr_changed,
				       NULL);
	g_list_free (uris);

	if (!result)
		return;

	brasero_data_disc_increase_activity_counter (disc);
}

static void
brasero_data_disc_inotify_modify_file_cb (BraseroVFS *self,
					  GObject *owner,
					  GnomeVFSResult result,
					  const gchar *uri,
					  GnomeVFSFileInfo *info,
					  gpointer callback_data)
{
	BraseroDataDisc *disc = BRASERO_DATA_DISC (owner);
	BraseroFile *file;
	GSList *paths;
	GSList *iter;

	if (result == GNOME_VFS_ERROR_NOT_FOUND) {
		brasero_data_disc_remove_uri (disc, uri, TRUE);
		return;
	}

	/* the file couldn't be a symlink so no need to check for loop */
	if (result != GNOME_VFS_OK
	||  !brasero_data_disc_is_readable (info)) {
		brasero_data_disc_remove_uri (disc, uri, TRUE);
		brasero_data_disc_unreadable_new (disc,
						  g_strdup (uri),
						  result);
		return;
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
		gchar *parent;

		parent = g_path_get_dirname (uri);
		file = g_hash_table_lookup (disc->priv->dirs, parent);
		g_free (parent);

		if (file && file->sectors >= 0) {
			/* its parent exists and must be rescanned */
			brasero_data_disc_add_rescan (disc, file);
		}
		else {
			/* the parent doesn't exist any more so nothing happens */
			return;
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

static void
brasero_data_disc_inotify_modify_file_end (GObject *object,
					   gpointer callback_data,
					   gboolean cancelled)
{
	BraseroDataDisc *disc = BRASERO_DATA_DISC (object);

	brasero_data_disc_decrease_activity_counter (disc);
}

static void
brasero_data_disc_inotify_modify_file (BraseroDataDisc *disc,
				       const gchar *uri)
{
	GList *uris;
	gboolean result;

	if (!disc->priv->modify_file)
		disc->priv->modify_file = brasero_vfs_register_data_type (disc->priv->vfs,
									  G_OBJECT (disc),
									  G_CALLBACK (brasero_data_disc_inotify_modify_file_cb),
									  brasero_data_disc_inotify_modify_file_end);

	uris = g_list_prepend (NULL, (gchar *) uri);
	result = brasero_vfs_get_info (disc->priv->vfs,
				       uris,
				       TRUE,
				       GNOME_VFS_FILE_INFO_GET_ACCESS_RIGHTS |
				       GNOME_VFS_FILE_INFO_GET_MIME_TYPE |
				       GNOME_VFS_FILE_INFO_FORCE_SLOW_MIME_TYPE,
				       disc->priv->modify_file,
				       NULL);
	g_list_free (uris);

	if (!result)
		return;

	brasero_data_disc_increase_activity_counter (disc);
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
	 * a corresponding MOVED_TO then we consider the file was removed. */
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

		if (g_hash_table_lookup (disc->priv->dirs, file->uri))
			brasero_data_disc_cancel_monitoring_real (disc, file);
	}

	return TRUE;
}

static gboolean
brasero_data_disc_start_monitoring_real (BraseroDataDisc *disc,
					 BraseroFile *file)
{
	gchar *path;
	gint dev_fd;
	__u32 mask;

	path = gnome_vfs_get_local_path_from_uri (file->uri);

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
