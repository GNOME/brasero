/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Brasero
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
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

#include <string.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <gdk/gdkkeysyms.h>

#include <gtk/gtk.h>

#include "brasero-misc.h"

#include "eggtreemultidnd.h"
#include "baobab-cell-renderer-progress.h"

#include "brasero-data-disc.h"
#include "brasero-file-filtered.h"
#include "brasero-disc.h"
#include "brasero-utils.h"
#include "brasero-disc-message.h"
#include "brasero-rename.h"
#include "brasero-notify.h"
#include "brasero-session-cfg.h"

#include "brasero-app.h"
#include "brasero-project-manager.h"

#include "brasero-session-cfg.h"
#include "brasero-tags.h"
#include "brasero-track.h"
#include "brasero-track-data.h"
#include "brasero-track-data-cfg.h"
#include "brasero-session.h"

#include "brasero-volume.h"
#include "brasero-setting.h"

typedef struct _BraseroDataDiscPrivate BraseroDataDiscPrivate;
struct _BraseroDataDiscPrivate
{
	GtkWidget *tree;
	GtkWidget *filter;
	BraseroTrackDataCfg *project;

	GtkSizeGroup *button_size;

	GtkWidget *message;

	GtkUIManager *manager;
	GtkActionGroup *disc_group;
	GtkActionGroup *import_group;

	gint press_start_x;
	gint press_start_y;

	GtkTreeRowReference *selected;

	GSList *load_errors;

	gint size_changed_id;

	guint editing:1;
	guint reject_files:1;

	guint overburning:1;

	guint loading:1;

	guint never_replace:1;
	guint always_replace:1;
	guint accept_2G_files:1;
	guint reject_2G_files:1;
	guint accept_deep_files:1;
	guint reject_deep_files:1;
};

#define BRASERO_DATA_DISC_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_DATA_DISC, BraseroDataDiscPrivate))


static void
brasero_data_disc_new_folder_clicked_cb (GtkAction *action,
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

static GtkActionEntry entries [] = {
	{"ContextualMenu", NULL, N_("Menu")},
	{"OpenFile", GTK_STOCK_OPEN, NULL, NULL, N_("Open the selected files"),
	 G_CALLBACK (brasero_data_disc_open_activated_cb)},
	{"RenameData", NULL, N_("R_ename…"), NULL, N_("Rename the selected file"),
	 G_CALLBACK (brasero_data_disc_rename_activated_cb)},
	{"DeleteData", GTK_STOCK_REMOVE, NULL, NULL, N_("Remove the selected files from the project"),
	 G_CALLBACK (brasero_data_disc_delete_activated_cb)},
	{"PasteData", NULL, N_("Paste files"), NULL, N_("Add the files stored in the clipboard"),
	 G_CALLBACK (brasero_data_disc_paste_activated_cb)},
	{"NewFolder", "folder-new", N_("New _Folder"), NULL, N_("Create a new empty folder"),
	 G_CALLBACK (brasero_data_disc_new_folder_clicked_cb)},
};

static const gchar *description = {
	"<ui>"
	"<menubar name='menubar' >"
		"<menu action='EditMenu'>"
		"<placeholder name='EditPlaceholder'>"
			"<menuitem action='NewFolder'/>"
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
		"</placeholder>"
	"</toolbar>"
	"</ui>"
};

enum {
	TREE_MODEL_ROW		= 150,
	TARGET_URIS_LIST,
};


static GtkTargetEntry ntables_cd [] = {
	{BRASERO_DND_TARGET_DATA_TRACK_REFERENCE_LIST, GTK_TARGET_SAME_WIDGET, TREE_MODEL_ROW},
	{"text/uri-list", 0, TARGET_URIS_LIST}
};
static guint nb_targets_cd = sizeof (ntables_cd) / sizeof (ntables_cd[0]);

static GtkTargetEntry ntables_source [] = {
	{BRASERO_DND_TARGET_DATA_TRACK_REFERENCE_LIST, GTK_TARGET_SAME_WIDGET, TREE_MODEL_ROW},
};

static guint nb_targets_source = sizeof (ntables_source) / sizeof (ntables_source[0]);

enum {
	PROP_NONE,
	PROP_REJECT_FILE,
};

static void
brasero_data_disc_iface_disc_init (BraseroDiscIface *iface);

G_DEFINE_TYPE_WITH_CODE (BraseroDataDisc,
			 brasero_data_disc,
			 GTK_TYPE_BOX,
			 G_IMPLEMENT_INTERFACE (BRASERO_TYPE_DISC,
					        brasero_data_disc_iface_disc_init));

#define BRASERO_DATA_DISC_MEDIUM		"brasero-data-disc-medium"
#define BRASERO_DATA_DISC_MERGE_ID		"brasero-data-disc-merge-id"
#define BRASERO_MEDIUM_GET_UDI(medium)		(brasero_drive_get_device (brasero_medium_get_drive (medium)))

BraseroMedium *
brasero_data_disc_get_loaded_medium (BraseroDataDisc *self)
{
	BraseroDataDiscPrivate *priv;
	priv = BRASERO_DATA_DISC_PRIVATE (self);
	return brasero_track_data_cfg_get_current_medium (priv->project);
}

/**
 * Actions callbacks
 */

static void
brasero_data_disc_import_failure_dialog (BraseroDataDisc *disc,
					 GError *error)
{
	brasero_app_alert (brasero_app_get_default (),
			   _("The session could not be imported."),
			   error?error->message:_("An unknown error occurred"),
			   GTK_MESSAGE_WARNING);
}

static gboolean
brasero_data_disc_import_session (BraseroDataDisc *disc,
				  BraseroMedium *medium,
				  gboolean import)
{
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);

	if (import) {
		GError *error = NULL;

		if (!brasero_track_data_cfg_load_medium (priv->project, medium, &error)) {
			brasero_data_disc_import_failure_dialog (disc, error);
			return FALSE;
		}

		return TRUE;
	}

	brasero_track_data_cfg_unload_current_medium (priv->project);
	return FALSE;
}

static void
brasero_data_disc_import_session_cb (GtkToggleAction *action,
				     BraseroDataDisc *self)
{
	BraseroDataDiscPrivate *priv;
	BraseroMedium *medium;
	gboolean res;

	priv = BRASERO_DATA_DISC_PRIVATE (self);

	medium = g_object_get_data (G_OBJECT (action), BRASERO_DATA_DISC_MEDIUM);
	if (!medium)
		return;

	brasero_notify_message_remove (priv->message, BRASERO_NOTIFY_CONTEXT_MULTISESSION);
	res = brasero_data_disc_import_session (self,
						medium,
						gtk_toggle_action_get_active (action));

	/* make sure the button reflects the current state */
	if (gtk_toggle_action_get_active (action) != res) {
		g_signal_handlers_block_by_func (action, brasero_data_disc_import_session_cb, self);
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), res);
		g_signal_handlers_unblock_by_func (action, brasero_data_disc_import_session_cb, self);
	}
}

static GtkTreePath *
brasero_data_disc_get_parent (BraseroDataDisc *self)
{
	BraseroDataDiscPrivate *priv;
	GtkTreeSelection *selection;
	GtkTreePath *treepath;
	gboolean is_loading;
	gboolean is_file;
	GtkTreeIter iter;
	GList *list;

	priv = BRASERO_DATA_DISC_PRIVATE (self);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));
	list = gtk_tree_selection_get_selected_rows (selection, NULL);

	if (g_list_length (list) != 1) {
		g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
		g_list_free (list);
		return NULL;
	}

	treepath = list->data;
	g_list_free (list);

	gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->project), &iter, treepath);

	gtk_tree_model_get (GTK_TREE_MODEL (priv->project), &iter,
			    BRASERO_DATA_TREE_MODEL_IS_LOADING, &is_loading,
			    -1);

	if (is_loading) {
		gtk_tree_path_free (treepath);
		return gtk_tree_path_new_first ();
	}

	gtk_tree_model_get (GTK_TREE_MODEL (priv->project), &iter,
			    BRASERO_DATA_TREE_MODEL_IS_FILE, &is_file,
			    -1);

	if (is_file && !gtk_tree_path_up (treepath)) {
		gtk_tree_path_free (treepath);
		treepath = gtk_tree_path_new_first ();
	}

	return treepath;
}

static void
brasero_data_disc_new_folder_clicked_cb (GtkAction *action,
					 BraseroDataDisc *disc)
{
	BraseroDataDiscPrivate *priv;
	GtkTreeViewColumn *column;
	GtkTreePath *treepath;
	GtkTreePath *parent;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);
	if (priv->reject_files)
		return;

	parent = brasero_data_disc_get_parent (disc);
	treepath = brasero_track_data_cfg_add_empty_directory (BRASERO_TRACK_DATA_CFG (priv->project), NULL, parent);
	gtk_tree_path_free (parent);

	/* grab focus must be called before next function to avoid
	 * triggering a bug where if pointer is not in the widget 
	 * any more and enter is pressed the cell will remain editable */
	column = gtk_tree_view_get_column (GTK_TREE_VIEW (priv->tree), 0);
	gtk_widget_grab_focus (priv->tree);

	gtk_tree_view_set_cursor (GTK_TREE_VIEW (priv->tree),
				  treepath,
				  column,
				  TRUE);
	gtk_tree_path_free (treepath);
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
	BraseroDataDiscPrivate *priv;
	GtkTreePath *parent = NULL;
	gchar **array;
	gchar **item;

	if (!text)
		goto end;

	priv = BRASERO_DATA_DISC_PRIVATE (data->disc);

	if (data->reference)
		parent = gtk_tree_row_reference_get_path (data->reference);

	array = g_uri_list_extract_uris (text);
	item = array;
	while (*item) {
		if (**item != '\0') {
			gchar *uri;
			GFile *file;

			file = g_file_new_for_commandline_arg (*item);
			uri = g_file_get_uri (file);
			g_object_unref (file);

			brasero_track_data_cfg_add (BRASERO_TRACK_DATA_CFG (priv->project),
						    uri,
						    parent);

			/* NOTE: no need to care about the notebook page since 
			 * to reach this part the tree should be displayed first
			 * to have the menu. */
		}

		item++;
	}
	g_strfreev (array);


end:

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
	if (brasero_clipboard_selection_may_have_uri (atoms, n_atoms)) {
		gtk_clipboard_request_text (clipboard,
					    (GtkClipboardTextReceivedFunc)
					    brasero_data_disc_clipboard_text_cb,
					    data);
		return;
	}

	if (data->reference)
		gtk_tree_row_reference_free (data->reference);

	g_free (data);
}

static void
brasero_data_disc_paste_activated_cb (GtkAction *action,
				      BraseroDataDisc *disc)
{
	BraseroDataDiscPrivate *priv;
	GtkClipboard *clipboard;
	BraseroClipData *data;
	GtkTreePath *parent;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);

	data = g_new0 (BraseroClipData, 1);
	data->disc = disc;

	parent = brasero_data_disc_get_parent (disc);
	if (parent)
		data->reference = gtk_tree_row_reference_new (GTK_TREE_MODEL (priv->project), parent);

	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_request_targets (clipboard,
				       (GtkClipboardTargetsReceivedFunc) brasero_data_disc_clipboard_targets_cb,
				       data);
}

/**
 * Row name edition
 */

static void
brasero_data_disc_name_editing_started_cb (GtkCellRenderer *renderer,
					   GtkCellEditable *editable,
					   gchar *path,
					   BraseroDataDisc *disc)
{
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);
	priv->editing = 1;
}

static void
brasero_data_disc_name_editing_canceled_cb (GtkCellRenderer *renderer,
					    BraseroDataDisc *disc)
{
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);
	priv->editing = 0;
}

static void
brasero_data_disc_name_edited_cb (GtkCellRendererText *cellrenderertext,
				  gchar *path_string,
				  gchar *text,
				  BraseroDataDisc *self)
{
	BraseroDataDiscPrivate *priv;
	GtkTreePath *path;
	GtkTreeIter row;
	gchar *name;

	priv = BRASERO_DATA_DISC_PRIVATE (self);

	priv->editing = 0;

	path = gtk_tree_path_new_from_string (path_string);

	/* see if this is still a valid path. It can happen a user removes it
	 * while the name of the row is being edited */
	if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->project), &row, path)) {
		gtk_tree_path_free (path);
		return;
	}

	/* make sure it actually changed */
	gtk_tree_model_get (GTK_TREE_MODEL (priv->project), &row,
			    BRASERO_DATA_TREE_MODEL_NAME, &name,
			    -1);

	if (name && !strcmp (name, text)) {
		gtk_tree_path_free (path);
		g_free (name);
		return;
	}
	g_free (name);

	/* NOTE: BraseroDataProject is where we handle name collisions,
	 * UTF-8 validity, ...
	 * Here if there is a name collision then rename gets aborted. */
	brasero_track_data_cfg_rename (BRASERO_TRACK_DATA_CFG (priv->project), text, path);
	gtk_tree_path_free (path);
}

/**
 * miscellaneous callbacks
 */

static void
brasero_data_disc_project_loading_cb (BraseroTrackDataCfg *project,
				      gdouble progress,
				      BraseroDataDisc *self)
{
	BraseroDataDiscPrivate *priv;
	GtkWidget *message;

	priv = BRASERO_DATA_DISC_PRIVATE (self);
	priv->loading = TRUE;

	message = brasero_notify_get_message_by_context_id (priv->message, BRASERO_NOTIFY_CONTEXT_LOADING);
	if (!message)
		return;

	/* we're not done yet update progress. */
	brasero_disc_message_set_progress (BRASERO_DISC_MESSAGE (message), progress);
}

static void
brasero_data_disc_project_loaded_cb (BraseroTrackDataCfg *project,
				     GSList *errors,
				     BraseroDataDisc *self)
{
	BraseroDataDiscPrivate *priv;
	GtkWidget *message;

	priv = BRASERO_DATA_DISC_PRIVATE (self);
	priv->loading = FALSE;

	message = brasero_notify_get_message_by_context_id (priv->message, BRASERO_NOTIFY_CONTEXT_LOADING);
	if (!message)
		return;

	if (errors) {
		brasero_disc_message_remove_buttons (BRASERO_DISC_MESSAGE (message));

		brasero_disc_message_set_primary (BRASERO_DISC_MESSAGE (message),
						  _("The contents of the project changed since it was saved."));
		brasero_disc_message_set_secondary (BRASERO_DISC_MESSAGE (message),
						    _("Discard the current modified project ?"));

		gtk_info_bar_set_message_type (GTK_INFO_BAR (message), GTK_MESSAGE_WARNING);

		brasero_disc_message_set_progress_active (BRASERO_DISC_MESSAGE (message), FALSE);
		gtk_widget_set_tooltip_text (gtk_info_bar_add_button (GTK_INFO_BAR (message),
								      _("_Discard"),
							    	      GTK_RESPONSE_CANCEL),
					     _("Discard the current modified project"));

		gtk_widget_set_tooltip_text (gtk_info_bar_add_button (GTK_INFO_BAR (message),
								      _("_Continue"),
							    	      GTK_RESPONSE_OK),
					     _("Continue with the current modified project"));

		brasero_disc_message_add_errors (BRASERO_DISC_MESSAGE (message),
						 priv->load_errors);
		g_slist_foreach (priv->load_errors, (GFunc) g_free , NULL);
		g_slist_free (priv->load_errors);
		priv->load_errors = NULL;
	}
	else {
		gtk_widget_set_sensitive (GTK_WIDGET (priv->tree), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->filter), TRUE);

		gtk_widget_destroy (message);
	}
}

static gboolean
brasero_data_disc_launch_image (gpointer data)
{
	gchar *uri = data;

	brasero_app_image (brasero_app_get_default (), NULL, uri, FALSE);
	g_free (uri);

	return FALSE;
}

static BraseroBurnResult
brasero_data_disc_image_uri_cb (BraseroTrackDataCfg *vfs,
				const gchar *uri,
				BraseroDataDisc *self)
{
	gint answer;
	gchar *name;
	gchar *string;
	GtkWidget *button;
	GtkWidget *dialog;

	dialog = brasero_app_dialog (brasero_app_get_default (),
	                             _("Do you want to create a disc from the contents of the image or with the image file inside?"),
	                             GTK_BUTTONS_NONE,
	                             GTK_MESSAGE_QUESTION);

	name = brasero_utils_get_uri_name (uri);
	/* Translators: %s is the name of the image */
	string = g_strdup_printf (_("There is only one selected file (\"%s\"). It is the image of a disc and its contents can be burned"), name);
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
											  "%s", string);
	g_free (string);
	g_free (name);

	gtk_dialog_add_button (GTK_DIALOG (dialog), _("Burn as _Data"), GTK_RESPONSE_NO);

	button = brasero_utils_make_button (_("Burn as _Image"),
	                                    NULL,
					    "media-optical-burn",
					    GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (button);
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog),
				      button,
				      GTK_RESPONSE_YES);

	gtk_widget_show_all (dialog);
	answer = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	if (answer != GTK_RESPONSE_YES)
		return BRASERO_BURN_OK;

	/* Tell project manager to switch. */
	g_idle_add (brasero_data_disc_launch_image, g_strdup (uri));

	return BRASERO_BURN_CANCEL;
}

static void
brasero_data_disc_filter_expanded_cb (GtkExpander *expander,
				      BraseroDataDisc *self)
{
	GtkWidget *parent;

	parent = gtk_widget_get_parent (GTK_WIDGET (expander));

	if (!gtk_expander_get_expanded (expander))
		gtk_box_set_child_packing (GTK_BOX (parent), GTK_WIDGET (expander), TRUE, TRUE, 0, GTK_PACK_END);
	else
		gtk_box_set_child_packing (GTK_BOX (parent), GTK_WIDGET (expander), FALSE, TRUE, 0, GTK_PACK_END);
}

static void
brasero_data_disc_unreadable_uri_cb (BraseroTrackDataCfg *vfs,
				     const GError *error,
				     const gchar *uri,
				     BraseroDataDisc *self)
{
	gchar *name;
	gchar *primary;

	name = brasero_utils_get_uri_name (uri);
	primary = g_strdup_printf (_("\"%s\" cannot be added to the selection."), name);
	brasero_app_alert (brasero_app_get_default (),
			   primary,
			   error->message,
			   GTK_MESSAGE_ERROR);
	g_free (primary);
	g_free (name);
}

static void
brasero_data_disc_recursive_uri_cb (BraseroTrackDataCfg *vfs,
				    const gchar *uri,
				    BraseroDataDisc *self)
{
	gchar *name;
	gchar *primary;

	name = brasero_utils_get_uri_name (uri);
	primary = g_strdup_printf (_("\"%s\" cannot be added to the selection."), name);
	brasero_app_alert (brasero_app_get_default (),
			   primary,
			   _("It is a recursive symlink"),
			   GTK_MESSAGE_ERROR);
	g_free (primary);
	g_free (name);
}

static void
brasero_data_disc_unknown_uri_cb (BraseroTrackDataCfg *vfs,
				  const gchar *uri,
				  BraseroDataDisc *self)
{
	gchar *name;
	gchar *primary;

	name = brasero_utils_get_uri_name (uri);
	primary = g_strdup_printf (_("\"%s\" cannot be added to the selection."), name);
	brasero_app_alert (brasero_app_get_default (),
			   primary,
			   _("It does not exist at the specified location"),
			   GTK_MESSAGE_ERROR);
	g_free (primary);
	g_free (name);
}

static void
brasero_data_disc_joliet_rename_cb (BraseroTrackDataCfg *project,
				    BraseroDataDisc *self)
{
	BraseroDataDiscPrivate *priv;
	GtkWidget *dialog;
	gint answer;

	priv = BRASERO_DATA_DISC_PRIVATE (self);

	dialog = brasero_app_dialog (brasero_app_get_default (),
				     _("Should files be renamed to be fully Windows-compatible?"),
				     GTK_BUTTONS_NONE,
				     GTK_MESSAGE_WARNING);
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
					 "%s\n%s",
					 _("Some files don't have a suitable name for a fully Windows-compatible CD."),
				     _("Those names should be changed and truncated to 64 characters."));

	gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Rename for Full Windows Compatibility"), GTK_RESPONSE_YES);
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Disable Full Windows Compatibility"), GTK_RESPONSE_CANCEL);

	gtk_widget_show_all (dialog);
	answer = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	if (answer == GTK_RESPONSE_YES)
		brasero_track_data_add_fs (BRASERO_TRACK_DATA (priv->project),
					   BRASERO_IMAGE_FS_JOLIET);
	else
		brasero_track_data_rm_fs (BRASERO_TRACK_DATA (priv->project),
					  BRASERO_IMAGE_FS_JOLIET);
}

static gboolean
brasero_data_disc_name_collision_cb (BraseroTrackDataCfg *project,
				     const gchar *name,
				     BraseroDataDisc *self)
{
	gint answer;
	gchar *string;
	GtkWidget *dialog;
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (self);

	if (priv->always_replace)
		return FALSE;

	if (priv->never_replace)
		return TRUE;

	/* Translators: %s is the name of the file */
	string = g_strdup_printf (_("Do you want to replace \"%s\"?"), name);
	dialog = brasero_app_dialog (brasero_app_get_default (),
				     string,
				     GTK_BUTTONS_NONE,
				     GTK_MESSAGE_WARNING);
	g_free (string);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
					 "%s", _("A file with this name already exists in the folder.  Replacing it will overwrite its contents on the disc to be burnt."));

	/* Translators: Keep means we're keeping the files that already existed
	 * in the project.
	 * Keep is a verb */
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("Always K_eep"), GTK_RESPONSE_REJECT);
	/* Translators: Keep means we're keeping the files that already existed
	 * in the project.
	 * Keep is a verb */
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Keep"), GTK_RESPONSE_NO);
	/* Translators: Replace means we're replacing the file that already
	 * existed in the project with a new one with the same name.
	 * Replace is a verb */
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Replace"), GTK_RESPONSE_YES);
	/* Translators: Replace means we're replacing the file that already
	 * existed in the project with a new one with the same name.
	 * Replace is a verb */
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("Al_ways Replace"), GTK_RESPONSE_ACCEPT);

	gtk_widget_show_all (dialog);
	answer = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	priv->always_replace = (answer == GTK_RESPONSE_ACCEPT);
	priv->never_replace = (answer == GTK_RESPONSE_REJECT);

	return (answer != GTK_RESPONSE_YES && answer != GTK_RESPONSE_ACCEPT);
}

static gboolean
brasero_data_disc_2G_file_cb (BraseroTrackDataCfg *project,
			      const gchar *name,
			      BraseroDataDisc *self)
{
	gint answer;
	gchar *string;
	GtkWidget *dialog;
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (self);

	if (priv->accept_deep_files)
		return TRUE;

	if (priv->reject_deep_files)
		return FALSE;

	string = g_strdup_printf (_("Do you really want to add \"%s\" to the selection and use the third version of the ISO9660 standard to support it?"), name);
	dialog = brasero_app_dialog (brasero_app_get_default (),
				     string,
				     GTK_BUTTONS_NONE,
				     GTK_MESSAGE_WARNING);
	g_free (string);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  "%s",
						  _("The size of the file is over 2 GiB. Files larger than 2 GiB are not supported by the ISO9660 standard in its first and second versions (the most widespread ones)."
						    "\nIt is recommended to use the third version of the ISO9660 standard, which is supported by most operating systems, including Linux and all versions of Windows™."
						    "\nHowever, Mac OS X cannot read images created with version 3 of the ISO9660 standard."));

	gtk_dialog_add_button (GTK_DIALOG (dialog), _("Ne_ver Add Such File"), GTK_RESPONSE_REJECT);
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("Al_ways Add Such File"), GTK_RESPONSE_ACCEPT);

	gtk_widget_show_all (dialog);
	answer = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	priv->accept_deep_files = (answer == GTK_RESPONSE_ACCEPT);
	priv->reject_deep_files = (answer == GTK_RESPONSE_REJECT);

	return (answer != GTK_RESPONSE_YES && answer != GTK_RESPONSE_ACCEPT);
}

static gboolean
brasero_data_disc_deep_directory_cb (BraseroTrackDataCfg *project,
				     const gchar *name,
				     BraseroDataDisc *self)
{
	gint answer;
	gchar *string;
	GtkWidget *dialog;
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (self);

	if (priv->accept_2G_files)
		return TRUE;

	if (priv->reject_2G_files)
		return FALSE;

	string = g_strdup_printf (_("Do you really want to add \"%s\" to the selection?"), name);
	dialog = brasero_app_dialog (brasero_app_get_default (),
				     string,
				     GTK_BUTTONS_NONE,
				     GTK_MESSAGE_WARNING);
	g_free (string);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  "%s",
						  _("The children of this directory will have 7 parent directories."
						    "\nBrasero can create an image of such a file hierarchy and burn it but the disc may not be readable on all operating systems."
						    "\nNote: Such a file hierarchy is known to work on Linux."));

	gtk_dialog_add_button (GTK_DIALOG (dialog), _("Ne_ver Add Such File"), GTK_RESPONSE_REJECT);
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("Al_ways Add Such File"), GTK_RESPONSE_ACCEPT);

	gtk_widget_show_all (dialog);
	answer = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	priv->accept_2G_files = (answer == GTK_RESPONSE_ACCEPT);
	priv->reject_2G_files = (answer == GTK_RESPONSE_REJECT);

	return (answer != GTK_RESPONSE_YES && answer != GTK_RESPONSE_ACCEPT);
}

static gboolean
brasero_data_disc_size_changed (gpointer user_data)
{
	goffset sectors;
	BraseroDataDisc *self;
	BraseroDataDiscPrivate *priv;

	self = BRASERO_DATA_DISC (user_data);
	priv = BRASERO_DATA_DISC_PRIVATE (self);

	brasero_track_get_size (BRASERO_TRACK (priv->project),
				&sectors,
				NULL);
	priv->size_changed_id = 0;
	return FALSE;
}

static void
brasero_data_disc_size_changed_cb (BraseroTrackDataCfg *project,
				   BraseroDataDisc *self)
{
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (self);

	if (!priv->size_changed_id)
		priv->size_changed_id = g_timeout_add (500,
						       brasero_data_disc_size_changed,
						       self);
}

static void
brasero_disc_disc_session_import_response_cb (GtkButton *button,
					      GtkResponseType response,
					      BraseroDataDisc *self)
{
	gboolean res;
	GtkAction *action;
	gchar *action_name;
	BraseroMedium *medium;
	BraseroDataDiscPrivate *priv;

	if (response != GTK_RESPONSE_OK)
		return;

	priv = BRASERO_DATA_DISC_PRIVATE (self);

	medium = g_object_get_data (G_OBJECT (button), BRASERO_DATA_DISC_MEDIUM);
	res = brasero_data_disc_import_session (self, medium, TRUE);

	action_name = g_strdup_printf ("Import_%s", BRASERO_MEDIUM_GET_UDI (medium));
	action = gtk_action_group_get_action (priv->import_group, action_name);
	g_free (action_name);

	g_signal_handlers_block_by_func (action, brasero_data_disc_import_session_cb, self);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), res);
	g_signal_handlers_unblock_by_func (action, brasero_data_disc_import_session_cb, self);
}

static GtkAction *
brasero_data_disc_import_button_new (BraseroDataDisc *self,
				     BraseroMedium *medium)
{
	int merge_id;
	gchar *string;
	gchar *tooltip;
	GtkAction *action;
	gchar *action_name;
	gchar *volume_name;
	gchar *description;
	BraseroDataDiscPrivate *priv;
	GtkToggleActionEntry toggle_entry = { 0, };

	priv = BRASERO_DATA_DISC_PRIVATE (self);

	if (!priv->manager)
		return NULL;

	action_name = g_strdup_printf ("Import_%s", BRASERO_MEDIUM_GET_UDI (medium));

	tooltip = brasero_medium_get_tooltip (medium);
	/* Translators: %s is a string describing the type of medium and the 
	 * drive it is in. It's a tooltip. */
	string = g_strdup_printf (_("Import %s"), tooltip);
	g_free (tooltip);
	tooltip = string;

	volume_name = brasero_volume_get_name (BRASERO_VOLUME (medium));
	/* Translators: %s is the name of the volume to import. It's a menu
	 * entry and toolbar button (text added later). */
	string = g_strdup_printf (_("I_mport %s"), volume_name);
	g_free (volume_name);
	volume_name = string;

	toggle_entry.name = action_name;
	toggle_entry.stock_id = "drive-optical";
	toggle_entry.label = string;
	toggle_entry.tooltip = tooltip;
	toggle_entry.callback = G_CALLBACK (brasero_data_disc_import_session_cb);

	gtk_action_group_add_toggle_actions (priv->import_group,
					     &toggle_entry,
					     1,
					     self);
	g_free (volume_name);
	g_free (tooltip);

	action = gtk_action_group_get_action (priv->import_group, action_name);
	if (!action) {
		g_free (action_name);
		return NULL;
	}

	g_object_ref (medium);
	g_object_set_data (G_OBJECT (action),
			   BRASERO_DATA_DISC_MEDIUM,
			   medium);

	g_object_set (action,
			/* Translators: This is a verb. It's a toolbar button. */
		      "short-label", _("I_mport"),
		      NULL);

	description = g_strdup_printf ("<ui>"
				       "<menubar name='menubar'>"
				       "<menu action='EditMenu'>"
				       "<placeholder name='EditPlaceholder'>"
				       "<menuitem action='%s'/>"
				       "</placeholder>"
				       "</menu>"
				       "</menubar>"
				       "<toolbar name='Toolbar'>"
				       "<placeholder name='DiscButtonPlaceholder'>"
				       "<toolitem action='%s'/>"
				       "</placeholder>"
				       "</toolbar>"
				       "</ui>",
				       action_name,
				       action_name);

	merge_id = gtk_ui_manager_add_ui_from_string (priv->manager,
						      description,
						      -1,
						      NULL);
	g_object_set_data (G_OBJECT( action),
			   BRASERO_DATA_DISC_MERGE_ID,
			   GINT_TO_POINTER (merge_id));

	g_free (description);
	g_free (action_name);
	return action;
}

static void
brasero_data_disc_remove_available_medium (BraseroDataDisc *self,
                                           BraseroMedium *medium)
{
	int merge_id;
	GtkAction *action;
	gchar *action_name;
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (self);

	action_name = g_strdup_printf ("Import_%s", BRASERO_MEDIUM_GET_UDI (medium));
	action = gtk_action_group_get_action (priv->import_group, action_name);
	g_free (action_name);

	brasero_notify_message_remove (priv->message, BRASERO_NOTIFY_CONTEXT_MULTISESSION);

	merge_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (action), BRASERO_DATA_DISC_MERGE_ID));
	gtk_ui_manager_remove_ui (priv->manager, merge_id);
	gtk_action_group_remove_action (priv->import_group, action);

	/* unref it since we reffed it when it was associated with the action */
	g_object_unref (medium);
}

static void
brasero_data_disc_session_available_cb (BraseroTrackDataCfg *session,
					BraseroMedium *medium,
					gboolean available,
					BraseroDataDisc *self)
{
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (self);

	if (!priv->manager)
		return;

	if (available) {
		gchar *string;
		gchar *volume_name;
		GtkWidget *message;

		/* create button and menu entry */
		brasero_data_disc_import_button_new (self, medium);

		/* ask user */
		volume_name = brasero_volume_get_name (BRASERO_VOLUME (medium));
		/* Translators: %s is the name of the volume to import */
		string = g_strdup_printf (_("Do you want to import the session from \"%s\"?"), volume_name);
		message = brasero_notify_message_add (priv->message,
						      string,
						      _("That way, old files from previous sessions will be usable after burning."),
						      10000,
						      BRASERO_NOTIFY_CONTEXT_MULTISESSION);
		g_free (volume_name);
		g_free (string);

		gtk_info_bar_set_message_type (GTK_INFO_BAR (message), GTK_MESSAGE_INFO);
		gtk_widget_set_tooltip_text (gtk_info_bar_add_button (GTK_INFO_BAR (message),
								      _("I_mport Session"),
							    	      GTK_RESPONSE_OK),
					     _("Click here to import its contents"));

		/* no need to ref the medium since its removal would cause the
		 * hiding of the message it's associated with */
		g_object_set_data (G_OBJECT (message),
				   BRASERO_DATA_DISC_MEDIUM,
				   medium);

		g_signal_connect (BRASERO_DISC_MESSAGE (message),
				  "response",
				  G_CALLBACK (brasero_disc_disc_session_import_response_cb),
				  self);
	}
	else
		brasero_data_disc_remove_available_medium (self, medium);
}

static void
brasero_data_disc_session_loaded_cb (BraseroTrackDataCfg *session,
				     BraseroMedium *medium,
				     gboolean loaded,
				     BraseroDataDisc *self)
{
	BraseroDataDiscPrivate *priv;
	gchar *action_name;
	GtkAction *action;

	priv = BRASERO_DATA_DISC_PRIVATE (self);

	action_name = g_strdup_printf ("Import_%s", BRASERO_MEDIUM_GET_UDI (medium));
	action = gtk_action_group_get_action (priv->import_group, action_name);
	g_free (action_name);

	g_signal_handlers_block_by_func (action, brasero_data_disc_import_session_cb, self);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), loaded);
	g_signal_handlers_unblock_by_func (action, brasero_data_disc_import_session_cb, self);
}

/**
 * BraseroDisc interface implementation
 */

static void
brasero_data_disc_clear (BraseroDisc *disc)
{
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);

	priv->always_replace = FALSE;
	priv->never_replace = FALSE;
	priv->accept_deep_files = FALSE;
	priv->reject_deep_files = FALSE;
	priv->accept_2G_files = FALSE;
	priv->reject_2G_files = FALSE;

	if (priv->size_changed_id) {
		g_source_remove (priv->size_changed_id);
		priv->size_changed_id = 0;
	}

	if (brasero_track_data_cfg_get_current_medium (BRASERO_TRACK_DATA_CFG (priv->project)))
		brasero_track_data_cfg_unload_current_medium (BRASERO_TRACK_DATA_CFG (priv->project));

	if (priv->load_errors) {
		g_slist_foreach (priv->load_errors, (GFunc) g_free , NULL);
		g_slist_free (priv->load_errors);
		priv->load_errors = NULL;
	}

	priv->overburning = FALSE;

 	brasero_notify_message_remove (priv->message, BRASERO_NOTIFY_CONTEXT_SIZE);
	brasero_notify_message_remove (priv->message, BRASERO_NOTIFY_CONTEXT_LOADING);
	brasero_notify_message_remove (priv->message, BRASERO_NOTIFY_CONTEXT_MULTISESSION);

	brasero_track_data_cfg_reset (priv->project);
}

static GSList *
brasero_data_disc_convert_tree_paths_to_references (GtkTreeModel *model,
                                            	    GList *treepaths)
{
	GList *iter;
	GSList *retval = NULL;

	for (iter = treepaths; iter; iter = iter->next) {
		GtkTreePath *treepath;
		GtkTreeRowReference *reference;

		treepath = iter->data;
		reference = gtk_tree_row_reference_new (model, treepath);
		retval = g_slist_prepend (retval, reference);
	}

	return retval;
}

static void
brasero_data_disc_delete_selected (BraseroDisc *disc)
{
	BraseroDataDiscPrivate *priv;
	GtkTreeSelection *selection;
	GtkTreePath *cursorpath;
	GSList *references;
	GSList *iter;
	GList *list;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);

	/* we must start by the end for the treepaths to point to valid rows */
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));
	list = gtk_tree_selection_get_selected_rows (selection, NULL);

	gtk_tree_view_get_cursor (GTK_TREE_VIEW (priv->tree),
				  &cursorpath,
				  NULL);

	/* Since we are going to modify the model by suppressing the selected
	 * rows, take a safe approach and convert all tree paths into references */
	references = brasero_data_disc_convert_tree_paths_to_references (GTK_TREE_MODEL (priv->project), list);
	g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (list);

	for (iter = references; iter; iter = iter->next) {
		GtkTreeRowReference *reference;
		GtkTreePath *treepath;

		reference = iter->data;
		treepath = gtk_tree_row_reference_get_path (reference);

		if (cursorpath && !gtk_tree_path_compare (cursorpath, treepath)) {
			GtkTreePath *tmp_path;

			/* this is to silence a warning with SortModel when
			 * removing a row being edited. We can only hope that
			 * there won't be G_MAXINT rows =) */
			tmp_path = gtk_tree_path_new_from_indices (G_MAXINT, -1);
			gtk_tree_view_set_cursor (GTK_TREE_VIEW (priv->tree),
						  tmp_path,
						  NULL,
						  FALSE);
			gtk_tree_path_free (tmp_path);
		}

		brasero_track_data_cfg_remove (BRASERO_TRACK_DATA_CFG (priv->project), treepath);

 		gtk_tree_row_reference_free (reference);
		gtk_tree_path_free (treepath);
	}
	g_slist_free (references);

	if (cursorpath)
		gtk_tree_path_free (cursorpath);

	/* warn that the selection changed (there are no more selected paths) */
	if (priv->selected)
		priv->selected = NULL;

	brasero_disc_selection_changed (disc);
}

static BraseroDiscResult
brasero_data_disc_add_uri (BraseroDisc *disc, const gchar *uri)
{
	BraseroDataDiscPrivate *priv;
	GtkTreePath *parent = NULL;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);

	if (priv->reject_files)
		return BRASERO_DISC_LOADING;

	parent = brasero_data_disc_get_parent (BRASERO_DATA_DISC (disc));
	if (brasero_track_data_cfg_add (BRASERO_TRACK_DATA_CFG (priv->project), uri, parent)) {
		gtk_tree_path_free (parent);
		return BRASERO_DISC_OK;
	}
	gtk_tree_path_free (parent);

	return BRASERO_DISC_ERROR_UNKNOWN;
}

static void
brasero_data_disc_message_response_cb (BraseroDiscMessage *message,
				       GtkResponseType response,
				       BraseroDataDisc *self)
{
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (self);

	gtk_widget_set_sensitive (GTK_WIDGET (priv->tree), TRUE);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->filter), TRUE);

	if (response != GTK_RESPONSE_CANCEL)
		return;

	brasero_data_disc_clear (BRASERO_DISC (self));
}

static void
brasero_data_disc_sort_column_changed (GtkTreeSortable *sortable,
                                       BraseroDataDisc *disc)
{
	GtkSortType sort_order;
	gint sort_column;

	gtk_tree_sortable_get_sort_column_id (sortable, &sort_column, &sort_order);

	brasero_setting_set_value (brasero_setting_get_default (),
	                           BRASERO_SETTING_DATA_DISC_COLUMN,
	                           GINT_TO_POINTER (sort_column));
	brasero_setting_set_value (brasero_setting_get_default (),
	                           BRASERO_SETTING_DATA_DISC_COLUMN_ORDER,
	                           GINT_TO_POINTER (sort_order));
}

static BraseroDiscResult
brasero_data_disc_set_track (BraseroDataDisc *disc,
			     BraseroTrackDataCfg *track)
{
	BraseroMedium *loaded_medium;
	BraseroDataDiscPrivate *priv;
	BraseroBurnResult result;
	BraseroStatus *status;
	GtkWidget *message;
	gint sort_column;
	gpointer value;
	gint sort_order;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);

	priv->project = g_object_ref (track);

	brasero_setting_get_value (brasero_setting_get_default (),
	                           BRASERO_SETTING_DATA_DISC_COLUMN,
	                           &value);
	sort_column = GPOINTER_TO_INT (value);
	brasero_setting_get_value (brasero_setting_get_default (),
	                           BRASERO_SETTING_DATA_DISC_COLUMN_ORDER,
	                           &value);
	sort_order = GPOINTER_TO_INT (value);

	if ((sort_column == BRASERO_DATA_TREE_MODEL_SIZE
	||   sort_column == BRASERO_DATA_TREE_MODEL_MIME_DESC
	||   sort_column == BRASERO_DATA_TREE_MODEL_NAME)
	&& (sort_order == GTK_SORT_ASCENDING
	||  sort_order == GTK_SORT_DESCENDING))
		gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (track),
						      sort_column,
						      sort_order);

	/* filtered files */
	priv->filter = brasero_file_filtered_new (priv->project);
	brasero_file_filtered_set_right_button_group (BRASERO_FILE_FILTERED (priv->filter), priv->button_size);
	g_signal_connect (priv->filter,
			  "activate",
			  G_CALLBACK (brasero_data_disc_filter_expanded_cb),
			  disc);
	gtk_widget_show (priv->filter);
	gtk_box_pack_end (GTK_BOX (disc), priv->filter, FALSE, TRUE, 0);

	/* Show all actions */
	if (!gtk_action_group_get_visible (priv->disc_group))
		gtk_action_group_set_visible (priv->disc_group, TRUE);

	/* Now let's take care of all the available sessions */
	loaded_medium = brasero_track_data_cfg_get_current_medium (track);
	if (!priv->import_group) {
		GSList *iter;
		GSList *list;

		priv->import_group = gtk_action_group_new ("session_import_group");
		gtk_action_group_set_translation_domain (priv->import_group, GETTEXT_PACKAGE);
		gtk_ui_manager_insert_action_group (priv->manager,
						    priv->import_group,
						    0);

		list = brasero_track_data_cfg_get_available_media (priv->project);
		for (iter = list; iter; iter = iter->next) {
			BraseroMedium *medium;
			GtkAction *action;

			medium = iter->data;
			action = brasero_data_disc_import_button_new (BRASERO_DATA_DISC (disc), medium);
			if (!action)
				continue;

			if (medium == loaded_medium)
				gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
		}
		g_slist_foreach (list, (GFunc) g_object_unref, NULL);
		g_slist_free (list);
	}
	else
		gtk_action_group_set_visible (priv->import_group, TRUE);

	g_signal_connect (track,
	                  "sort-column-changed",
	                  G_CALLBACK (brasero_data_disc_sort_column_changed),
	                  disc);

	g_signal_connect (track,
			  "G2-file",
			  G_CALLBACK (brasero_data_disc_2G_file_cb),
			  disc);
	g_signal_connect (track,
			  "deep-directory",
			  G_CALLBACK (brasero_data_disc_deep_directory_cb),
			  disc);
	g_signal_connect (track,
			  "name-collision",
			  G_CALLBACK (brasero_data_disc_name_collision_cb),
			  disc);
	g_signal_connect (track,
			  "joliet-rename",
			  G_CALLBACK (brasero_data_disc_joliet_rename_cb),
			  disc);

	g_signal_connect (track,
			  "source-loading",
			  G_CALLBACK (brasero_data_disc_project_loading_cb),
			  disc);
	g_signal_connect (track,
			  "source-loaded",
			  G_CALLBACK (brasero_data_disc_project_loaded_cb),
			  disc);

	/* Use the BraseroTrack "changed" signal for size changes */
	g_signal_connect (track,
			  "changed",
			  G_CALLBACK (brasero_data_disc_size_changed_cb),
			  disc);

	g_signal_connect (track,
			  "image-uri",
			  G_CALLBACK (brasero_data_disc_image_uri_cb),
			  disc);
	g_signal_connect (track,
			  "unreadable-uri",
			  G_CALLBACK (brasero_data_disc_unreadable_uri_cb),
			  disc);
	g_signal_connect (track,
			  "recursive-sym",
			  G_CALLBACK (brasero_data_disc_recursive_uri_cb),
			  disc);
	g_signal_connect (track,
			  "unknown-uri",
			  G_CALLBACK (brasero_data_disc_unknown_uri_cb),
			  disc);

	g_signal_connect (track,
			  "session-available",
			  G_CALLBACK (brasero_data_disc_session_available_cb),
			  disc);
	g_signal_connect (track,
			  "session-loaded",
			  G_CALLBACK (brasero_data_disc_session_loaded_cb),
			  disc);

	gtk_tree_view_set_model (GTK_TREE_VIEW (priv->tree), GTK_TREE_MODEL (track));

	status = brasero_status_new ();
	brasero_track_get_status (BRASERO_TRACK (track), status);
	result = brasero_status_get_result (status);
	if (result == BRASERO_BURN_OK || result == BRASERO_BURN_RUNNING) {
		g_object_unref (status);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->tree), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (priv->filter), TRUE);
		return BRASERO_DISC_OK;
	}

	if (result != BRASERO_BURN_NOT_READY) {
		g_object_unref (status);
		return BRASERO_DISC_ERROR_UNKNOWN;
	}

	message = brasero_notify_message_add (priv->message,
					      _("Please wait while the project is loading."),
					      NULL,
					      -1,
					      BRASERO_NOTIFY_CONTEXT_LOADING);

	gtk_info_bar_set_message_type (GTK_INFO_BAR (message), GTK_MESSAGE_INFO);
	brasero_disc_message_set_progress (BRASERO_DISC_MESSAGE (message),
					   brasero_status_get_progress (status));

	gtk_widget_set_tooltip_text (gtk_info_bar_add_button (GTK_INFO_BAR (message),
							      _("_Cancel Loading"),
						    	      GTK_RESPONSE_CANCEL),
				     _("Cancel loading current project"));

	g_signal_connect (message,
			  "response",
			  G_CALLBACK (brasero_data_disc_message_response_cb),
			  disc);

	gtk_widget_set_sensitive (GTK_WIDGET (priv->tree), FALSE);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->filter), FALSE);

	g_object_unref (status);
	return BRASERO_DISC_OK;
}

static void
brasero_data_disc_unset_track (BraseroDataDisc *disc)
{
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);

	if (!priv->project)
		return;

	priv->always_replace = FALSE;
	priv->never_replace = FALSE;
	priv->accept_deep_files = FALSE;
	priv->reject_deep_files = FALSE;
	priv->accept_2G_files = FALSE;
	priv->reject_2G_files = FALSE;

	/* Remove filtered files widget */
	if (priv->filter) {
		gtk_widget_destroy (priv->filter);
		priv->filter = NULL;
	}

	if (priv->size_changed_id) {
		g_source_remove (priv->size_changed_id);
		priv->size_changed_id = 0;
	}

	/* Hide all actions */
	if (gtk_action_group_get_visible (priv->disc_group))
		gtk_action_group_set_visible (priv->disc_group, FALSE);

	/* Remove each button for every available session that can be imported */
	if (priv->import_group) {
		GList *actions;
		GList *iter;

		actions = gtk_action_group_list_actions (priv->import_group);
		for (iter = actions; iter; iter = iter->next) {
			BraseroMedium *medium;
			GtkAction *action;
			int merge_id;

			action = iter->data;

			/* We reffed the medium associated with the action */
			medium = g_object_get_data (G_OBJECT (action), BRASERO_DATA_DISC_MEDIUM);
			g_object_unref (medium);

			merge_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (action), BRASERO_DATA_DISC_MERGE_ID));
			gtk_ui_manager_remove_ui (priv->manager, merge_id);
		}
		g_list_free (actions);

		gtk_ui_manager_remove_action_group (priv->manager,  priv->import_group);
		g_object_unref (priv->import_group);
		priv->import_group = NULL;
	}

	if (priv->load_errors) {
		g_slist_foreach (priv->load_errors, (GFunc) g_free , NULL);
		g_slist_free (priv->load_errors);
		priv->load_errors = NULL;
	}

	g_signal_handlers_disconnect_by_func (priv->project,
					      brasero_data_disc_sort_column_changed,
					      disc);
	g_signal_handlers_disconnect_by_func (priv->project,
					      brasero_data_disc_2G_file_cb,
					      disc);
	g_signal_handlers_disconnect_by_func (priv->project,
					      brasero_data_disc_deep_directory_cb,
					      disc);
	g_signal_handlers_disconnect_by_func (priv->project,
					      brasero_data_disc_name_collision_cb,
					      disc);
	g_signal_handlers_disconnect_by_func (priv->project,
					      brasero_data_disc_joliet_rename_cb,
					      disc);
	g_signal_handlers_disconnect_by_func (priv->project,
					      brasero_data_disc_project_loading_cb,
					      disc);
	g_signal_handlers_disconnect_by_func (priv->project,
					      brasero_data_disc_project_loaded_cb,
					      disc);
	g_signal_handlers_disconnect_by_func (priv->project,
					      brasero_data_disc_size_changed_cb,
					      disc);
	g_signal_handlers_disconnect_by_func (priv->project,
					      brasero_data_disc_image_uri_cb,
					      disc);
	g_signal_handlers_disconnect_by_func (priv->project,
					      brasero_data_disc_unreadable_uri_cb,
					      disc);
	g_signal_handlers_disconnect_by_func (priv->project,
					      brasero_data_disc_recursive_uri_cb,
					      disc);
	g_signal_handlers_disconnect_by_func (priv->project,
					      brasero_data_disc_unknown_uri_cb,
					      disc);
	g_signal_handlers_disconnect_by_func (priv->project,
					      brasero_data_disc_session_available_cb,
					      disc);
	g_signal_handlers_disconnect_by_func (priv->project,
					      brasero_data_disc_session_loaded_cb,
					      disc);

	g_object_unref (priv->project);
	priv->project = NULL;

	gtk_tree_view_set_model (GTK_TREE_VIEW (priv->tree), NULL);
}

static void
brasero_data_disc_track_removed (BraseroBurnSession *session,
				 BraseroTrack *track,
				 guint former_position,
				 BraseroDataDisc *disc)
{
	g_signal_handlers_disconnect_by_func (session,
					      brasero_data_disc_track_removed,
					      disc);

	brasero_data_disc_unset_track (disc);
}

static BraseroDiscResult
brasero_data_disc_set_session_contents (BraseroDisc *self,
					BraseroBurnSession *session)
{
	BraseroBurnResult result = BRASERO_BURN_OK;
	GSList *tracks;

	brasero_data_disc_unset_track (BRASERO_DATA_DISC (self));

	if (!session)
		return BRASERO_DISC_OK;

	/* get the track data */
	tracks = brasero_burn_session_get_tracks (session);
	if (!tracks) {
		BraseroTrackDataCfg *data_track;

		/* If it's empty add one */
                data_track = brasero_track_data_cfg_new ();
                brasero_burn_session_add_track (session,
						BRASERO_TRACK (data_track),
						NULL);
		brasero_data_disc_set_track (BRASERO_DATA_DISC (self),
					     BRASERO_TRACK_DATA_CFG (data_track));

		/* NOTE: that track was reffed in brasero_data_disc_set_track () */
		g_object_unref (data_track);
	}
	else for (; tracks; tracks = tracks->next) {
		BraseroTrack *track;

		track = tracks->data;
		if (BRASERO_IS_TRACK_DATA_CFG (track))
			result = brasero_data_disc_set_track (BRASERO_DATA_DISC (self),
							      BRASERO_TRACK_DATA_CFG (track));
	}

	g_signal_connect (session,
			  "track-removed",
			  G_CALLBACK (brasero_data_disc_track_removed),
			  self);

	return result;
}

static gboolean
brasero_data_disc_get_selected_uri (BraseroDisc *disc,
				    gchar **uri)
{
	BraseroDataDiscPrivate *priv;
	GtkTreePath *path;
	GtkTreeIter iter;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);

	if (!priv->selected)
		return FALSE;

	if (!uri)
		return TRUE;

	path = gtk_tree_row_reference_get_path (priv->selected);
	if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->project), &iter, path)) {
		gtk_tree_path_free (path);
		return FALSE;
	}

	gtk_tree_path_free (path);
	gtk_tree_model_get (GTK_TREE_MODEL (priv->project), &iter,
			    BRASERO_DATA_TREE_MODEL_URI, uri,
			    -1);
	return TRUE;
}

static guint
brasero_data_disc_add_ui (BraseroDisc *disc,
			  GtkUIManager *manager,
			  GtkWidget *message)
{
	BraseroDataDiscPrivate *priv;
	GError *error = NULL;
	GtkAction *action;
	guint merge_id;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);
	if (priv->message) {
		g_object_unref (priv->message);
		priv->message = NULL;
	}

	priv->message = message;
	g_object_ref (message);

	if (!priv->disc_group) {
		priv->disc_group = gtk_action_group_new (BRASERO_DISC_ACTION "-data");
		gtk_action_group_set_translation_domain (priv->disc_group, GETTEXT_PACKAGE);
		gtk_action_group_add_actions (priv->disc_group,
					      entries,
					      G_N_ELEMENTS (entries),
					      disc);
		gtk_ui_manager_insert_action_group (manager,
						    priv->disc_group,
						    0);

		merge_id = gtk_ui_manager_add_ui_from_string (manager,
							      description,
							      -1,
							      &error);
		if (!merge_id) {
			g_error_free (error);
			return 0;
		}

		action = gtk_action_group_get_action (priv->disc_group, "NewFolder");
		g_object_set (action,
			      "short-label", _("New _Folder"), /* for toolbar buttons */
			      NULL);
	
		priv->manager = manager;
		g_object_ref (manager);
	}
	else
		gtk_action_group_set_visible (priv->disc_group, TRUE);

	return -1;
}

/**
 * Contextual menu callbacks
 */

static void
brasero_data_disc_open_file (BraseroDataDisc *disc, GList *list)
{
	GList *item;
	GSList *uris;
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);

	uris = NULL;
	for (item = list; item; item = item->next) {
		GtkTreePath *treepath;
		gboolean is_imported;
		gchar *uri = NULL;
		GtkTreeIter iter;

		treepath = item->data;
		if (!treepath)
			continue;

		if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->project), &iter, treepath))
			continue;

		gtk_tree_model_get (GTK_TREE_MODEL (priv->project), &iter,
				    BRASERO_DATA_TREE_MODEL_IS_IMPORTED, &is_imported,
				    -1);
		if (is_imported)
			continue;

		gtk_tree_model_get (GTK_TREE_MODEL (priv->project), &iter,
				    BRASERO_DATA_TREE_MODEL_URI, &uri,
				    -1);
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
	GtkTreeSelection *selection;
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));
	list = gtk_tree_selection_get_selected_rows (selection, NULL);
	brasero_data_disc_open_file (disc, list);

	g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (list);
}

static gboolean
brasero_data_disc_mass_rename_cb (GtkTreeModel *model,
				  GtkTreeIter *iter,
				  GtkTreePath *treepath,
				  const gchar *old_name,
				  const gchar *new_name)
{
	return brasero_track_data_cfg_rename (BRASERO_TRACK_DATA_CFG (model),
					      new_name,
					      treepath);
}

static void
brasero_data_disc_rename_activated (BraseroDataDisc *disc)
{
	BraseroDataDiscPrivate *priv;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkTreePath *treepath;
	GList *list;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));

	list = gtk_tree_selection_get_selected_rows (selection, NULL);
	if (g_list_length (list) == 1) {
		gboolean is_imported;
		GtkTreeIter iter;

		treepath = list->data;
		g_list_free (list);

		if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->project), &iter, treepath)) {
			gtk_tree_path_free (treepath);
			return;
		}

		gtk_tree_model_get (GTK_TREE_MODEL (priv->project), &iter,
				    BRASERO_DATA_TREE_MODEL_IS_IMPORTED, &is_imported,
				    -1);
		if (is_imported) {
			gtk_tree_path_free (treepath);
			return;
		}

		column = gtk_tree_view_get_column (GTK_TREE_VIEW (priv->tree), 0);

		/* grab focus must be called before next function to avoid
		 * triggering a bug where if pointer is not in the widget 
		 * any more and enter is pressed the cell will remain editable */
		gtk_widget_grab_focus (priv->tree);
		gtk_tree_view_set_cursor_on_cell (GTK_TREE_VIEW (priv->tree),
						  treepath,
						  column,
						  NULL,
						  TRUE);
		gtk_tree_path_free (treepath);
	}
	else {
		gchar *string;
		GtkWidget *frame;
		GtkWidget *dialog;
		GtkWidget *rename;
		GtkResponseType answer;

		dialog = gtk_dialog_new_with_buttons (_("File Renaming"),
						      GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (disc))),
						      GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
						      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						      _("_Rename"), GTK_RESPONSE_APPLY,
						      NULL);
		rename = brasero_rename_new ();
		brasero_rename_set_show_keep_default (BRASERO_RENAME (rename), FALSE);
		gtk_widget_show (rename);

		string = g_strdup_printf ("<b>%s</b>", _("Renaming mode"));
		frame = brasero_utils_pack_properties (string, rename, NULL);
		g_free (string);

		gtk_widget_show (frame);

		gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), frame, TRUE, TRUE, 0);
		gtk_widget_show (dialog);

		answer = gtk_dialog_run (GTK_DIALOG (dialog));
		if (answer != GTK_RESPONSE_APPLY) {
			gtk_widget_destroy (dialog);
			return;
		}

		brasero_rename_do (BRASERO_RENAME (rename),
				   gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree)),
				   BRASERO_DATA_TREE_MODEL_NAME,
				   brasero_data_disc_mass_rename_cb);

		gtk_widget_destroy (dialog);
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

/**
 * key/button press handling
 */

static void
brasero_data_disc_selection_changed_cb (GtkTreeSelection *selection,
					BraseroDataDisc *self)
{
	BraseroDataDiscPrivate *priv;
	GtkTreeModel *model;
	GList *selected;

	priv = BRASERO_DATA_DISC_PRIVATE (self);
	priv->selected = NULL;

	selected = gtk_tree_selection_get_selected_rows (selection, &model);
	if (selected) {
		GtkTreePath *treepath;
		GtkTreeIter iter;

		treepath = selected->data;

		/* we need to make sure that this is not a bogus row */
		if (gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->project), &iter, treepath)) {
			gboolean is_imported;

			gtk_tree_model_get (GTK_TREE_MODEL (priv->project), &iter,
					    BRASERO_DATA_TREE_MODEL_IS_IMPORTED, &is_imported,
					    -1);
			if (!is_imported)
				priv->selected = gtk_tree_row_reference_new (GTK_TREE_MODEL (priv->project), treepath);
		}

		g_list_foreach (selected, (GFunc) gtk_tree_path_free, NULL);
		g_list_free (selected);
	}

	brasero_disc_selection_changed (BRASERO_DISC (self));
}

static gboolean
brasero_data_disc_tree_select_function (GtkTreeSelection *selection,
					GtkTreeModel *model,
					GtkTreePath *treepath,
					gboolean is_selected,
					gpointer null_data)
{
	GtkTreeIter iter;
	gboolean is_imported;

	if (!gtk_tree_model_get_iter (model, &iter, treepath))
		return FALSE;

	gtk_tree_model_get (model, &iter, 
			    BRASERO_DATA_TREE_MODEL_IS_IMPORTED, &is_imported,
			    -1);

	if (is_imported) {
		if (is_selected)
			return TRUE;

		return FALSE;
	}

	/* FIXME: this should be reenable if the bug in multiDND and cell
	 * editing appears again. 
	if (is_selected)
		node->is_selected = FALSE;
	else
		node->is_selected = TRUE;
	*/

	return TRUE;
}

static void
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
			gtk_widget_set_sensitive (item, TRUE);
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
				     BraseroDataDisc *self)
{
	GtkTreeIter iter;
	gboolean result = FALSE;
	GtkTreePath *treepath = NULL;
	GtkWidgetClass *widget_class;
	BraseroDataDiscPrivate *priv;
	gboolean keep_selection = FALSE;

	priv = BRASERO_DATA_DISC_PRIVATE (self);

	/* Avoid minding signals that happen out of the tree area (like in the 
	 * headers for example) */
	if (event->window != gtk_tree_view_get_bin_window (GTK_TREE_VIEW (tree)))
		return FALSE;

	if (gtk_widget_get_realized (priv->tree)) {
		result = gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (priv->tree),
							event->x,
							event->y,
							&treepath,
							NULL,
							NULL,
							NULL);

		if (treepath) {
			if (gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->project), &iter, treepath)) {
				GtkTreeSelection *selection;
				selection = gtk_tree_view_get_selection (tree);
				keep_selection = gtk_tree_selection_path_is_selected (selection, treepath);
			}
			else {
				/* That may be a BOGUS row */
				gtk_tree_path_free (treepath);
				treepath = NULL;
				result = FALSE;
			}
		}
		else
			result = FALSE;
	}

	/* we call the default handler for the treeview before everything else
	 * so it can update itself (particularly its selection) before we use it
	 * NOTE: since the event has been processed here we need to return TRUE
	 * to avoid having the treeview processing this event a second time. */
	widget_class = GTK_WIDGET_GET_CLASS (tree);

	if (priv->loading) {
		widget_class->button_press_event (GTK_WIDGET (tree), event);
		gtk_tree_path_free (treepath);
		return TRUE;
	}

	if ((event->state & (GDK_CONTROL_MASK|GDK_SHIFT_MASK)) == 0) {
		if (result) {
			gboolean is_imported;

			gtk_tree_model_get (GTK_TREE_MODEL (priv->project), &iter,
					    BRASERO_DATA_TREE_MODEL_IS_IMPORTED, &is_imported,
					    -1);
			if (!is_imported)
				priv->selected = gtk_tree_row_reference_new (GTK_TREE_MODEL (priv->project), treepath);
		}
		else if (treepath && (event->state & GDK_SHIFT_MASK) == 0)
			priv->selected = gtk_tree_row_reference_new (GTK_TREE_MODEL (priv->project), treepath);
		else
			priv->selected = NULL;

		brasero_disc_selection_changed (BRASERO_DISC (self));
	}

	if (event->button == 1) {
		widget_class->button_press_event (GTK_WIDGET (tree), event);

		priv->press_start_x = event->x;
		priv->press_start_y = event->y;

		if (event->type == GDK_2BUTTON_PRESS) {
			if (treepath) {
				GList *list;

				list = g_list_prepend (NULL, gtk_tree_path_copy (treepath));
				brasero_data_disc_open_file (self, list);
				g_list_free (list);
			}
		}
		else if (!result) {
			GtkTreeSelection *selection;

			/* This is to deselect any row when selecting a row that cannot
			 * be selected or in an empty part */
			selection = gtk_tree_view_get_selection (tree);
			gtk_tree_selection_unselect_all (selection);
		}
	}
	else if (event->button == 3) {
		GtkTreeSelection *selection;

		/* Don't update the selection if the right click was on one of
		 * the already selected rows */
		if (!keep_selection) {
			widget_class->button_press_event (GTK_WIDGET (tree), event);

			if (!result) {
				GtkTreeSelection *selection;

				/* This is to deselect any row when selecting a row that cannot
				 * be selected or in an empty part */
				selection = gtk_tree_view_get_selection (tree);
				gtk_tree_selection_unselect_all (selection);
			}
		}

		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));
		brasero_data_disc_show_menu (gtk_tree_selection_count_selected_rows (selection),
					     priv->manager,
					     event);
	}

	gtk_tree_path_free (treepath);

	return TRUE;
}

static gboolean
brasero_data_disc_key_released_cb (GtkTreeView *tree,
				   GdkEventKey *event,
				   BraseroDataDisc *self)
{
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (self);
	
	if (priv->loading)
		return FALSE;

	if (priv->editing)
		return FALSE;

	if (event->keyval == GDK_KEY_KP_Delete || event->keyval == GDK_KEY_Delete)
		brasero_data_disc_delete_selected (BRASERO_DISC (self));
	else if (event->keyval == GDK_KEY_F2)
		brasero_data_disc_rename_activated (self);

	return FALSE;
}

/**
 * Misc functions
 */

void
brasero_data_disc_set_right_button_group (BraseroDataDisc *self,
					  GtkSizeGroup *size_group)
{
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (self);
	priv->button_size = g_object_ref (size_group);
}

/**
 * Object creation/destruction
 */
static void
brasero_data_disc_init (BraseroDataDisc *object)
{
	BraseroDataDiscPrivate *priv;
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkWidget *mainbox;
	GtkWidget *scroll;

	priv = BRASERO_DATA_DISC_PRIVATE (object);

	gtk_box_set_spacing (GTK_BOX (object), 8);
	gtk_orientable_set_orientation (GTK_ORIENTABLE (object), GTK_ORIENTATION_VERTICAL);

	mainbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (mainbox);
	gtk_box_pack_start (GTK_BOX (object), mainbox, TRUE, TRUE, 0);

	/* Tree */
	priv->tree = gtk_tree_view_new ();
	gtk_tree_view_set_rubber_banding (GTK_TREE_VIEW (priv->tree), TRUE);

	/* This must be before connecting to button press event */
	egg_tree_multi_drag_add_drag_support (GTK_TREE_VIEW (priv->tree));
	gtk_widget_show (priv->tree);
	g_signal_connect (priv->tree,
			  "button-press-event",
			  G_CALLBACK (brasero_data_disc_button_pressed_cb),
			  object);
	
	g_signal_connect (priv->tree,
			  "key-release-event",
			  G_CALLBACK (brasero_data_disc_key_released_cb),
			  object);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));
	g_signal_connect (selection,
			  "changed",
			  G_CALLBACK (brasero_data_disc_selection_changed_cb),
			  object);

	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
	gtk_tree_selection_set_select_function (selection,
						brasero_data_disc_tree_select_function,
						NULL,
						NULL);

	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (priv->tree), TRUE);

	column = gtk_tree_view_column_new ();

	gtk_tree_view_column_set_resizable (column, TRUE);

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "icon-name", BRASERO_DATA_TREE_MODEL_MIME_ICON);

	renderer = gtk_cell_renderer_text_new ();
	g_signal_connect (G_OBJECT (renderer), "edited",
			  G_CALLBACK (brasero_data_disc_name_edited_cb), object);
	g_signal_connect (G_OBJECT (renderer), "editing-started",
			  G_CALLBACK (brasero_data_disc_name_editing_started_cb), object);
	g_signal_connect (G_OBJECT (renderer), "editing-canceled",
			  G_CALLBACK (brasero_data_disc_name_editing_canceled_cb), object);

	gtk_tree_view_column_pack_end (column, renderer, TRUE);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "text", BRASERO_DATA_TREE_MODEL_NAME);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "style", BRASERO_DATA_TREE_MODEL_STYLE);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "foreground", BRASERO_DATA_TREE_MODEL_COLOR);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "editable", BRASERO_DATA_TREE_MODEL_EDITABLE);

	g_object_set (G_OBJECT (renderer),
		      "ellipsize-set", TRUE,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      NULL);

	gtk_tree_view_column_set_title (column, _("Files"));
	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_column_set_spacing (column, 4);
	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->tree), column);
	gtk_tree_view_column_set_sort_column_id (column, BRASERO_DATA_TREE_MODEL_NAME);
	gtk_tree_view_set_expander_column (GTK_TREE_VIEW (priv->tree), column);

	/* Size column */
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);

	gtk_tree_view_column_add_attribute (column, renderer,
					    "text", BRASERO_DATA_TREE_MODEL_SIZE);
	gtk_tree_view_column_set_title (column, _("Size"));

	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->tree), column);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_expand (column, FALSE);
	gtk_tree_view_column_set_sort_column_id (column, BRASERO_DATA_TREE_MODEL_SIZE);

	/* Description */
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);

	gtk_tree_view_column_add_attribute (column, renderer,
					    "text", BRASERO_DATA_TREE_MODEL_MIME_DESC);
	gtk_tree_view_column_set_title (column, _("Description"));

	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->tree), column);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_expand (column, FALSE);
	gtk_tree_view_column_set_sort_column_id (column, BRASERO_DATA_TREE_MODEL_MIME_DESC);

	/* Space column */
	renderer = baobab_cell_renderer_progress_new ();
	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);

	gtk_tree_view_column_add_attribute (column, renderer,
					    "visible", BRASERO_DATA_TREE_MODEL_SHOW_PERCENT);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "perc", BRASERO_DATA_TREE_MODEL_PERCENT);
	gtk_tree_view_column_set_title (column, _("Space"));

	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->tree), column);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_expand (column, FALSE);

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
brasero_data_disc_finalize (GObject *object)
{
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (object);

	if (priv->project) {
		g_object_unref (priv->project);
		priv->project = NULL;
	}

	if (priv->button_size) {
		g_object_unref (priv->button_size);
		priv->button_size = NULL;
	}

	if (priv->size_changed_id) {
		g_source_remove (priv->size_changed_id);
		priv->size_changed_id = 0;
	}

	if (priv->message) {
		g_object_unref (priv->message);
		priv->message = NULL;
	}

	if (priv->load_errors) {
		g_slist_foreach (priv->load_errors, (GFunc) g_free , NULL);
		g_slist_free (priv->load_errors);
		priv->load_errors = NULL;
	}

	G_OBJECT_CLASS (brasero_data_disc_parent_class)->finalize (object);
}

static gboolean
brasero_data_disc_is_empty (BraseroDisc *disc)
{
	BraseroDataDiscPrivate *priv;
	GtkTreeModel *model;

	priv = BRASERO_DATA_DISC_PRIVATE (disc);
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (priv->tree));
	if(!model)
		return FALSE;

	return gtk_tree_model_iter_n_children (model, NULL) != 0;
}

static void
brasero_data_disc_iface_disc_init (BraseroDiscIface *iface)
{
	iface->add_uri = brasero_data_disc_add_uri;
	iface->delete_selected = brasero_data_disc_delete_selected;
	iface->is_empty = brasero_data_disc_is_empty;
	iface->clear = brasero_data_disc_clear;
	iface->set_session_contents = brasero_data_disc_set_session_contents;
	iface->get_selected_uri = brasero_data_disc_get_selected_uri;
	iface->add_ui = brasero_data_disc_add_ui;
}

static void
brasero_data_disc_get_property (GObject * object,
				guint prop_id,
				GValue * value,
				GParamSpec * pspec)
{
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (object);

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
brasero_data_disc_set_property (GObject * object,
				guint prop_id,
				const GValue * value,
				GParamSpec * pspec)
{
	BraseroDataDiscPrivate *priv;

	priv = BRASERO_DATA_DISC_PRIVATE (object);

	switch (prop_id) {
	case PROP_REJECT_FILE:
		priv->reject_files = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
brasero_data_disc_class_init (BraseroDataDiscClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	g_type_class_add_private (klass, sizeof (BraseroDataDiscPrivate));

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

GtkWidget *
brasero_data_disc_new (void)
{
	return GTK_WIDGET (g_object_new (BRASERO_TYPE_DATA_DISC, NULL));
}
