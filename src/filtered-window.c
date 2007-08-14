/***************************************************************************
 *            filtered-window.c
 *
 *  dim oct 30 12:25:50 2005
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


#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <gtk/gtkdialog.h>
#include <gtk/gtklabel.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtktreeviewcolumn.h>
#include <gtk/gtkcellrenderer.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gtk/gtkcellrenderertoggle.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkmessagedialog.h>

#include <libgnomevfs/gnome-vfs.h>

#include <gconf/gconf-client.h>

#include "filtered-window.h"
#include "utils.h"

static void brasero_filtered_dialog_class_init (BraseroFilteredDialogClass *klass);
static void brasero_filtered_dialog_init (BraseroFilteredDialog *sp);
static void brasero_filtered_dialog_finalize (GObject *object);

struct BraseroFilteredDialogPrivate {
	GtkWidget *tree;
	GConfClient *client;

	guint broken_sym_notify;
	guint hidden_notify;
	guint notify_notify;

	int broken_state:1;
	int hidden_state:1;
};

enum  {
	STOCK_ID_COL,
	UNESCAPED_URI_COL,
	URI_COL,
	TYPE_COL,
	STATUS_COL,
	ACTIVABLE_COL,
	NB_COL,
};

typedef enum {
	REMOVED_SIGNAL,
	RESTORED_SIGNAL,
	LAST_SIGNAL
} BraseroFilteredDialogSignalType;

static guint brasero_filtered_dialog_signals [LAST_SIGNAL] = { 0 };
static GObjectClass *parent_class = NULL;

static void
brasero_filtered_dialog_gconf_notify_cb (GConfClient *client,
					 guint cnxn_id,
					 GConfEntry *entry,
					 gpointer user_data);

static void
brasero_filtered_dialog_filter_hidden_cb (GtkToggleButton *button,
					  BraseroFilteredDialog *dialog);
static void
brasero_filtered_dialog_filter_broken_sym_cb (GtkToggleButton *button,
					      BraseroFilteredDialog *dialog);
static void
brasero_filtered_dialog_filter_notify_cb (GtkToggleButton *button,
					  BraseroFilteredDialog *dialog);

GType
brasero_filtered_dialog_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroFilteredDialogClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_filtered_dialog_class_init,
			NULL,
			NULL,
			sizeof (BraseroFilteredDialog),
			0,
			(GInstanceInitFunc)brasero_filtered_dialog_init,
		};

		type = g_type_register_static(GTK_TYPE_DIALOG, 
			"BraseroFilteredDialog", &our_info, 0);
	}

	return type;
}

static void
brasero_filtered_dialog_class_init (BraseroFilteredDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_filtered_dialog_finalize;
	
	
	brasero_filtered_dialog_signals[REMOVED_SIGNAL] =
	    g_signal_new ("removed",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_ACTION | G_SIGNAL_RUN_FIRST,
			  G_STRUCT_OFFSET (BraseroFilteredDialogClass, removed),
			  NULL, NULL,
			  g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1,
			  G_TYPE_STRING);

	brasero_filtered_dialog_signals[RESTORED_SIGNAL] =
	    g_signal_new ("restored",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_ACTION | G_SIGNAL_RUN_FIRST,
			  G_STRUCT_OFFSET (BraseroFilteredDialogClass, restored),
			  NULL, NULL,
			  g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1,
			  G_TYPE_STRING);
}

static void
brasero_filtered_dialog_init (BraseroFilteredDialog *obj)
{
	gboolean active;
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *frame;
	GtkWidget *scroll;
	GtkListStore *model;
	GError *error = NULL;
	GtkWidget *button_sym;
	GtkWidget *button_notify;
	GtkWidget *button_hidden;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	obj->priv = g_new0 (BraseroFilteredDialogPrivate, 1);
	gtk_window_set_title (GTK_WINDOW (obj), _("Removed files"));
	gtk_dialog_set_has_separator (GTK_DIALOG (obj), FALSE);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (obj)->vbox), 10);

	frame = gtk_frame_new ("");
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
	gtk_container_set_border_width (GTK_CONTAINER (frame), 6);
	gtk_widget_show (frame);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (obj)->vbox),
			    frame,
			    TRUE,
			    TRUE,
			    0);

	vbox = gtk_vbox_new (FALSE, 10);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (frame), vbox);

	label = gtk_frame_get_label_widget (GTK_FRAME (frame));
	gtk_label_set_markup (GTK_LABEL (label),
			      _("<span weight=\"bold\" size=\"larger\">The following files were removed automatically from the project.</span>"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

	label = gtk_label_new (_("Select the files you want to restore:"));
	gtk_misc_set_padding (GTK_MISC (label), 0, 2);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox),
			    label,
			    FALSE,
			    TRUE,
			    0);

	model = gtk_list_store_new (NB_COL,
				    G_TYPE_STRING,
				    G_TYPE_STRING,
				    G_TYPE_STRING,
				    G_TYPE_STRING,
				    G_TYPE_INT,
				    G_TYPE_BOOLEAN);

	obj->priv->tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (obj->priv->tree), TRUE);
	gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (obj->priv->tree)),
				     GTK_SELECTION_MULTIPLE);
	gtk_tree_view_set_rubber_banding (GTK_TREE_VIEW (obj->priv->tree), TRUE);
	g_object_unref (model);

	column = gtk_tree_view_column_new ();
	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "stock-id", STOCK_ID_COL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_end (column, renderer, TRUE);
	gtk_tree_view_column_add_attribute (column, renderer,
					    "text", URI_COL);
	gtk_tree_view_column_set_title (column, _("Files"));
	gtk_tree_view_append_column (GTK_TREE_VIEW (obj->priv->tree), column);
	gtk_tree_view_column_set_sort_column_id (column, URI_COL);
	gtk_tree_view_column_set_clickable (column, TRUE);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_min_width (column, 450);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Type"), renderer,
							   "text", TYPE_COL, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (obj->priv->tree), column);
	gtk_tree_view_column_set_sort_column_id (column, TYPE_COL);
	gtk_tree_view_column_set_clickable (column, TRUE);

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll),
					     GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scroll), obj->priv->tree);

	gtk_box_pack_start (GTK_BOX (vbox),
			    scroll,
			    TRUE,
			    TRUE,
			    0);

	/* options */
	obj->priv->client = gconf_client_get_default ();

	active = gconf_client_get_bool (obj->priv->client,
					BRASERO_FILTER_HIDDEN_KEY,
					NULL);

	button_hidden = gtk_check_button_new_with_label (_("Filter hidden files"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_hidden), active);
	g_signal_connect (button_hidden,
			  "toggled",
			  G_CALLBACK (brasero_filtered_dialog_filter_hidden_cb),
			  obj);

	obj->priv->hidden_notify = gconf_client_notify_add (obj->priv->client,
							    BRASERO_FILTER_HIDDEN_KEY,
							    brasero_filtered_dialog_gconf_notify_cb,
							    button_hidden, NULL, &error);
	if (error) {
		g_warning ("GConf : %s\n", error->message);
		g_error_free (error);
	}

	active = gconf_client_get_bool (obj->priv->client,
					BRASERO_FILTER_BROKEN_SYM_KEY,
					NULL);
	
	button_sym = gtk_check_button_new_with_label (_("Filter broken symlinks"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_sym), active);
	g_signal_connect (button_sym,
			  "toggled",
			  G_CALLBACK (brasero_filtered_dialog_filter_broken_sym_cb),
			  obj);

	obj->priv->broken_sym_notify = gconf_client_notify_add (obj->priv->client,
								BRASERO_FILTER_BROKEN_SYM_KEY,
								brasero_filtered_dialog_gconf_notify_cb,
								button_sym, NULL, &error);
	if (error) {
		g_warning ("GConf : %s\n", error->message);
		g_error_free (error);
	}

	active = gconf_client_get_bool (obj->priv->client,
					BRASERO_FILTER_NOTIFY_KEY,
					NULL);
	
	button_notify = gtk_check_button_new_with_label (_("Notify when files are filtered"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_notify), active);
	g_signal_connect (button_notify,
			  "toggled",
			  G_CALLBACK (brasero_filtered_dialog_filter_notify_cb),
			  obj);

	obj->priv->notify_notify = gconf_client_notify_add (obj->priv->client,
							    BRASERO_FILTER_NOTIFY_KEY,
							    brasero_filtered_dialog_gconf_notify_cb,
							    button_notify, NULL, &error);
	if (error) {
		g_warning ("GConf : %s\n", error->message);
		g_error_free (error);
	}

	frame = brasero_utils_pack_properties (_("<b>Filtering options</b>"),
					       button_notify,
					       NULL);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (obj)->vbox),
			    frame,
			    FALSE,
			    FALSE,
			    0);

	/* buttons */
	gtk_dialog_add_button (GTK_DIALOG (obj),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (obj),
				GTK_STOCK_OK, GTK_RESPONSE_OK);
}

static void
brasero_filtered_dialog_finalize (GObject *object)
{
	BraseroFilteredDialog *cobj;

	cobj = BRASERO_FILTERED_DIALOG (object);

	if (cobj->priv->notify_notify) {
		gconf_client_notify_remove (cobj->priv->client,
					    cobj->priv->notify_notify);
		cobj->priv->notify_notify = 0;
	}

	if (cobj->priv->hidden_notify) {
		gconf_client_notify_remove (cobj->priv->client,
					    cobj->priv->hidden_notify);
		cobj->priv->hidden_notify = 0;
	}

	if (cobj->priv->broken_sym_notify) {
		gconf_client_notify_remove (cobj->priv->client,
					    cobj->priv->broken_sym_notify);
		cobj->priv->broken_sym_notify = 0;
	}
	
	if (cobj->priv->client) {
		g_object_unref (cobj->priv->client);
		cobj->priv->client = NULL;
	}

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkWidget *
brasero_filtered_dialog_new ()
{
	BraseroFilteredDialog *obj;
	
	obj = BRASERO_FILTERED_DIALOG (g_object_new (BRASERO_TYPE_FILTERED_DIALOG, NULL));
	gtk_window_set_default_size (GTK_WINDOW (obj), 640, 500);
	return GTK_WIDGET (obj);
}

static void
brasero_filtered_dialog_filter_hidden_cb (GtkToggleButton *button,
					  BraseroFilteredDialog *dialog)
{
	gconf_client_set_bool (dialog->priv->client,
			       BRASERO_FILTER_HIDDEN_KEY,
			       gtk_toggle_button_get_active (button),
			       NULL);
}

static void
brasero_filtered_dialog_filter_broken_sym_cb (GtkToggleButton *button,
					      BraseroFilteredDialog *dialog)
{
	gconf_client_set_bool (dialog->priv->client,
			       BRASERO_FILTER_BROKEN_SYM_KEY,
			       gtk_toggle_button_get_active (button),
			       NULL);
}

static void
brasero_filtered_dialog_filter_notify_cb (GtkToggleButton *button,
					  BraseroFilteredDialog *dialog)
{
	gconf_client_set_bool (dialog->priv->client,
			       BRASERO_FILTER_NOTIFY_KEY,
			       gtk_toggle_button_get_active (button),
			       NULL);
}

static void
brasero_filtered_dialog_gconf_notify_cb (GConfClient *client,
					 guint cnxn_id,
					 GConfEntry *entry,
					 gpointer user_data)
{
	GConfValue *value;
	GtkToggleButton *button = user_data;

	value = gconf_entry_get_value (entry);
	gtk_toggle_button_set_active (button, gconf_value_get_bool (value));
}

void
brasero_filtered_dialog_add (BraseroFilteredDialog *dialog,
			     const gchar *uri,
			     gboolean restored,
			     BraseroFilterStatus status)
{
	gchar *labels [] = { N_("hidden file"),
			     N_("unreadable file"),
			     N_("broken symlink"),
			     N_("recursive symlink"),
			     NULL };
	const gchar *stock_id;
	gchar *unescaped_uri;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *type;

	type = labels [ status - 1 ];
	if (status == BRASERO_FILTER_UNREADABLE)
		stock_id = GTK_STOCK_CANCEL;
	else
		stock_id = NULL;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->tree));
	gtk_list_store_append (GTK_LIST_STORE (model), &iter);

	unescaped_uri = gnome_vfs_unescape_string_for_display (uri);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			    STOCK_ID_COL, stock_id,
			    UNESCAPED_URI_COL, unescaped_uri,
			    URI_COL, uri,
			    TYPE_COL, _(type),
			    STATUS_COL, status,
			    ACTIVABLE_COL, (status == BRASERO_FILTER_UNREADABLE || status == BRASERO_FILTER_RECURSIVE_SYM),
			    -1);

	g_free (unescaped_uri);
}

void
brasero_filtered_dialog_get_status (BraseroFilteredDialog *dialog,
				    GSList **restored,
				    GSList **removed)
{
	GtkTreeSelection *selection;
	BraseroFilterStatus status;
	GSList *retval_restored;
	GSList *retval_removed;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *uri;

	if (removed)
		*removed = NULL;
	if (restored)
		*restored = NULL;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->priv->tree));
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->priv->tree));
	if (!gtk_tree_model_get_iter_first (model, &iter))
		return;

	retval_restored = NULL;
	retval_removed = NULL;
	do {
		gtk_tree_model_get (model, &iter,
				    URI_COL, &uri,
				    STATUS_COL, &status, -1);

		if (status == BRASERO_FILTER_UNREADABLE
		||  status == BRASERO_FILTER_RECURSIVE_SYM) {
			g_free (uri);
			continue;
		}

		if (gtk_tree_selection_iter_is_selected (selection, &iter))
			retval_restored = g_slist_prepend (retval_restored, uri);
		else
			retval_removed = g_slist_prepend (retval_removed, uri);
	} while (gtk_tree_model_iter_next (model, &iter));

	if (restored)
		*restored = retval_restored;

	if (removed)
		*removed = retval_removed;
}
