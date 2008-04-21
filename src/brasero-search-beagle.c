/***************************************************************************
*            search.c
*
*  dim mai 22 11:20:54 2005
*  Copyright  2005  Philippe Rouquier
*  brasero-app@wanadoo.fr
****************************************************************************/

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

#ifdef BUILD_SEARCH

#include <string.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gutils.h>
#include <glib-object.h>

#include <gio/gio.h>

#include <gtk/gtkvbox.h>
#include <gtk/gtkfilefilter.h>
#include <gtk/gtkselection.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtkcellrenderer.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkcellrendererprogress.h>
#include <gtk/gtktreeviewcolumn.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtktreemodelfilter.h>
#include <gtk/gtkcombobox.h>
#include <gtk/gtkcomboboxentry.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkmessagedialog.h>

#include <beagle/beagle.h>

#include "brasero-utils.h"
#include "brasero-search-entry.h"
#include "brasero-mime-filter.h"
#include "brasero-search-beagle.h"
#include "eggtreemultidnd.h"

#include "brasero-uri-container.h"
#include "brasero-layout-object.h"

static void brasero_search_class_init (BraseroSearchClass *klass);
static void brasero_search_init (BraseroSearch *sp);
static void brasero_search_iface_uri_container_init (BraseroURIContainerIFace *iface);
static void brasero_search_iface_layout_object_init (BraseroLayoutObjectIFace *iface);
static void brasero_search_finalize (GObject *object);
static void brasero_search_destroy (GtkObject *object);

struct BraseroSearchPrivate {
	BeagleClient *client;
	BeagleQuery *query;

	GtkWidget *tree;
	GtkWidget *entry;
	GtkWidget *filter;
	GtkWidget *filters;
	GtkWidget *right;
	GtkWidget *left;
	GtkWidget *results_label;

	GSList *hits;
	gint hits_num;
	gint first_hit;

	gint max_results;

	guint id;
	guint activity;
};

static GObjectClass *parent_class = NULL;

enum {
	TARGET_URIS_LIST,
};

static GtkTargetEntry ntables_find[] = {
	{"text/uri-list", 0, TARGET_URIS_LIST}
};
static guint nb_ntables_find = sizeof (ntables_find) / sizeof (ntables_find[0]);

enum {
	BRASERO_SEARCH_TREE_ICON_COL,
	BRASERO_SEARCH_TREE_TITLE_COL,
	BRASERO_SEARCH_TREE_DESCRIPTION_COL,
	BRASERO_SEARCH_TREE_SCORE_COL,
	BRASERO_SEARCH_TREE_URI_COL,
	BRASERO_SEARCH_TREE_MIME_COL,
	BRASERO_SEARCH_TREE_NB_COL
};


static void brasero_search_entry_activated_cb (BraseroSearchEntry *entry,
					       BraseroSearch *obj);
static void brasero_search_tree_activated_cb (GtkTreeView *tree,
					      GtkTreeIter *row,
					      GtkTreeViewColumn *column,
					      BraseroSearch *search);
static void brasero_search_tree_selection_changed_cb (GtkTreeSelection *selection,
						      BraseroSearch *search);
static void brasero_search_drag_data_get_cb (GtkTreeView *tree,
					     GdkDragContext *drag_context,
					     GtkSelectionData *selection_data,
					     guint info,
					     guint time,
					     BraseroSearch *search);
static void brasero_search_mime_filter_changed (GtkComboBox *combo,
						BraseroSearch *search);
static gboolean brasero_search_is_visible_cb (GtkTreeModel *model,
					      GtkTreeIter *iter,
					      BraseroSearch *search);
static void
brasero_search_right_button_clicked_cb (GtkButton *button,
					BraseroSearch *search);
static void
brasero_search_left_button_clicked_cb (GtkButton *button,
				       BraseroSearch *search);

static void
brasero_search_max_results_num_changed_cb (GtkComboBox *combo,
					   BraseroSearch *search);

static void brasero_search_empty_tree (BraseroSearch *search);
static gchar ** brasero_search_get_selected_rows (BraseroSearch *search);

static gchar **
brasero_search_get_selected_uris (BraseroURIContainer *container);
static gchar *
brasero_search_get_selected_uri (BraseroURIContainer *container);

#define BRASERO_SEARCH_SPACING 6

GType
brasero_search_get_type ()
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroSearchClass),
			NULL,
			NULL,
			(GClassInitFunc) brasero_search_class_init,
			NULL,
			NULL,
			sizeof (BraseroSearch),
			0,
			(GInstanceInitFunc) brasero_search_init,
		};

		static const GInterfaceInfo uri_container_info =
		{
			(GInterfaceInitFunc) brasero_search_iface_uri_container_init,
			NULL,
			NULL
		};
		static const GInterfaceInfo layout_object_info =
		{
			(GInterfaceInitFunc) brasero_search_iface_layout_object_init,
			NULL,
			NULL
		};

		type = g_type_register_static (GTK_TYPE_VBOX,
					       "BraseroSearch",
					       &our_info,
					       0);

		g_type_add_interface_static (type,
					     BRASERO_TYPE_URI_CONTAINER,
					     &uri_container_info);
		g_type_add_interface_static (type,
					     BRASERO_TYPE_LAYOUT_OBJECT,
					     &layout_object_info);
	}

	return type;
}

static void
brasero_search_class_init (BraseroSearchClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkObjectClass *gtkobject_class = GTK_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_search_finalize;
	gtkobject_class->destroy = brasero_search_destroy;
}

static void
brasero_search_iface_uri_container_init (BraseroURIContainerIFace *iface)
{
	iface->get_selected_uri = brasero_search_get_selected_uri;
	iface->get_selected_uris = brasero_search_get_selected_uris;
}

static void
brasero_search_get_proportion (BraseroLayoutObject *object,
			       gint *header,
			       gint *center,
			       gint *footer) 
{
	GtkRequisition requisition;

	gtk_widget_size_request (BRASERO_SEARCH (object)->priv->filters,
				 &requisition);
	*footer = requisition.height + BRASERO_SEARCH_SPACING;
}

static void
brasero_search_set_context (BraseroLayoutObject *object,
			    BraseroLayoutType type)
{
	BraseroSearch *self;

	self = BRASERO_SEARCH (object);
	brasero_search_entry_set_context (BRASERO_SEARCH_ENTRY (self->priv->entry), type);
}

static void
brasero_search_iface_layout_object_init (BraseroLayoutObjectIFace *iface)
{
	iface->get_proportion = brasero_search_get_proportion;
	iface->set_context = brasero_search_set_context;
}

static void
brasero_search_column_clicked (GtkTreeViewColumn *column,
			       BraseroSearch *search)
{
	gint model_id;
	gint column_id;
	GtkTreeModel *model;
	GtkSortType model_order;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (search->priv->tree));
	model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));

	gtk_tree_sortable_get_sort_column_id (GTK_TREE_SORTABLE (model),
					      &model_id,
					      &model_order);
	column_id = gtk_tree_view_column_get_sort_column_id (column);

	if (column_id == model_id && model_order == GTK_SORT_DESCENDING) {
		gtk_tree_view_column_set_sort_indicator (column, FALSE);
		gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model),
						      BRASERO_SEARCH_TREE_SCORE_COL,
						      GTK_SORT_DESCENDING);
	}
	else if (model_id == BRASERO_SEARCH_TREE_SCORE_COL) {
		gtk_tree_view_column_set_sort_indicator (column, TRUE);
		gtk_tree_view_column_set_sort_order (column, GTK_SORT_ASCENDING);
		gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model),
						      column_id,
						      GTK_SORT_ASCENDING);
	}
	else {
		gtk_tree_view_column_set_sort_order (column, GTK_SORT_DESCENDING);
		gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model),
						      column_id,
						      GTK_SORT_DESCENDING);
	}

	g_signal_stop_emission_by_name (column, "clicked");
}

static void
brasero_search_init (BraseroSearch *obj)
{
	GtkWidget *box;
	GtkWidget *box1;
	GtkWidget *label;
	GtkWidget *combo;
	GtkWidget *button;
	GtkWidget *scroll;
	GtkListStore *store;
	GtkTreeModel *model;
	GtkWidget *separator;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkFileFilter *file_filter;
	GtkTreeSelection *selection;

	gtk_box_set_spacing (GTK_BOX (obj), BRASERO_SEARCH_SPACING);
	obj->priv = g_new0 (BraseroSearchPrivate, 1);

	/* separator */
	separator = gtk_hseparator_new ();
	gtk_box_pack_start (GTK_BOX (obj), separator, FALSE, FALSE, 0);

	/* Entry */
	obj->priv->entry = brasero_search_entry_new ();
	g_signal_connect (G_OBJECT (obj->priv->entry), 
			  "activated",
			  G_CALLBACK (brasero_search_entry_activated_cb),
			  obj);
	gtk_box_pack_start (GTK_BOX (obj), obj->priv->entry, FALSE, FALSE, 0);

	/* separator */
	separator = gtk_hseparator_new ();
	gtk_box_pack_start (GTK_BOX (obj), separator, FALSE, FALSE, 0);

	/* results navigation */
	box = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (obj), box, FALSE, FALSE, 0);

	button = brasero_utils_make_button (_("Previous results"),
					    GTK_STOCK_GO_BACK,
					    NULL,
					    GTK_ICON_SIZE_BUTTON);
	gtk_button_set_alignment (GTK_BUTTON (button), 0.0, 0.5);
	gtk_widget_set_sensitive (button, FALSE);
	g_signal_connect (G_OBJECT (button), 
			  "clicked",
			  G_CALLBACK (brasero_search_left_button_clicked_cb),
			  obj);
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, TRUE, 0);
	obj->priv->left = button;

	label = gtk_label_new (_("<b>No results</b>"));
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);
	obj->priv->results_label = label;

	button = brasero_utils_make_button (_("Next results"),
					    GTK_STOCK_GO_FORWARD,
					    NULL,
					    GTK_ICON_SIZE_BUTTON);
	gtk_button_set_alignment (GTK_BUTTON (button), 1.0, 0.5);
	gtk_widget_set_sensitive (button, FALSE);
	g_signal_connect (G_OBJECT (button), 
			  "clicked",
			  G_CALLBACK (brasero_search_right_button_clicked_cb),
			  obj);
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, TRUE, 0);
	obj->priv->right = button;

	/* Tree */
	obj->priv->tree = gtk_tree_view_new ();
	gtk_tree_view_set_rubber_banding (GTK_TREE_VIEW (obj->priv->tree), TRUE);
	egg_tree_multi_drag_add_drag_support (GTK_TREE_VIEW (obj->priv->tree));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (obj->priv->tree));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (obj->priv->tree), TRUE);
	g_signal_connect (G_OBJECT (obj->priv->tree), 
			  "row_activated",
			  G_CALLBACK (brasero_search_tree_activated_cb),
			  obj);
	g_signal_connect (G_OBJECT (gtk_tree_view_get_selection (GTK_TREE_VIEW (obj->priv->tree))),
			  "changed",
			  G_CALLBACK (brasero_search_tree_selection_changed_cb),
			  obj);

	gtk_tree_view_set_headers_clickable (GTK_TREE_VIEW (obj->priv->tree), TRUE);
	store = gtk_list_store_new (BRASERO_SEARCH_TREE_NB_COL,
				    G_TYPE_STRING,
				    G_TYPE_STRING,
				    G_TYPE_STRING,
				    G_TYPE_INT,
				    G_TYPE_STRING,
				    G_TYPE_STRING);

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
					      BRASERO_SEARCH_TREE_SCORE_COL,
					      GTK_SORT_DESCENDING);

	model = gtk_tree_model_filter_new (GTK_TREE_MODEL (store), NULL);
	g_object_unref (G_OBJECT (store));
	gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (model),
						(GtkTreeModelFilterVisibleFunc) brasero_search_is_visible_cb,
						obj,
						NULL);

	gtk_tree_view_set_model (GTK_TREE_VIEW (obj->priv->tree), model);
	g_object_unref (G_OBJECT (model));

	column = gtk_tree_view_column_new ();
	g_signal_connect (column,
			  "clicked",
			  G_CALLBACK (brasero_search_column_clicked),
			  obj);

	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_title (column, _("Files"));
	gtk_tree_view_column_set_min_width (column, 128);
	gtk_tree_view_column_set_sort_column_id (column,
						 BRASERO_SEARCH_TREE_TITLE_COL);

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (column, renderer, "icon-name",
					    BRASERO_SEARCH_TREE_ICON_COL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_add_attribute (column, renderer, "text",
					    BRASERO_SEARCH_TREE_TITLE_COL);

	gtk_tree_view_append_column (GTK_TREE_VIEW (obj->priv->tree),
				     column);

	renderer = gtk_cell_renderer_text_new ();
	column =  gtk_tree_view_column_new_with_attributes (_("Description"),
							    renderer, "text",
						            BRASERO_SEARCH_TREE_DESCRIPTION_COL,
						            NULL);
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_column_set_min_width (column, 128);
	gtk_tree_view_column_set_sort_column_id (column,
						 BRASERO_SEARCH_TREE_DESCRIPTION_COL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (obj->priv->tree),
				     column);
	g_signal_connect (column,
			  "clicked",
			  G_CALLBACK (brasero_search_column_clicked),
			  obj);

	/* dnd */
	gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (obj->priv->tree),
						GDK_BUTTON1_MASK,
						ntables_find,
						nb_ntables_find,
						GDK_ACTION_COPY);

	g_signal_connect (G_OBJECT (obj->priv->tree), 
			  "drag-data-get",
			  G_CALLBACK (brasero_search_drag_data_get_cb),
			  obj);

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (scroll), obj->priv->tree);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll),
					     GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX (obj), scroll, TRUE, TRUE, 0);

	/* filter combo */
	box = gtk_hbox_new (FALSE, 32);
	obj->priv->filters = box;
	gtk_box_pack_end (GTK_BOX (obj), box, FALSE, FALSE, 0);

	obj->priv->filter = brasero_mime_filter_new ();
	g_signal_connect (G_OBJECT (BRASERO_MIME_FILTER (obj->priv->filter)->combo),
			  "changed",
			  G_CALLBACK (brasero_search_mime_filter_changed),
			  obj);

	file_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (file_filter, _("All files"));
	gtk_file_filter_add_pattern (file_filter, "*");
	brasero_mime_filter_add_filter (BRASERO_MIME_FILTER (obj->priv->filter),
					file_filter);
	g_object_unref (file_filter);

	gtk_box_pack_end (GTK_BOX (box), obj->priv->filter, FALSE, FALSE, 0);

	box1 = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_end (GTK_BOX (box), box1, FALSE, FALSE, 0);

	label = gtk_label_new (_("Number of results displayed"));
	gtk_box_pack_start (GTK_BOX (box1), label, FALSE, FALSE, 0);

	combo = gtk_combo_box_new_text ();
	g_signal_connect (combo,
			  "changed",
			  G_CALLBACK (brasero_search_max_results_num_changed_cb),
			  obj);
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo), "20");
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo), "50");
	gtk_combo_box_append_text (GTK_COMBO_BOX (combo), "100");

	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 1);
	obj->priv->max_results = 50;

	gtk_box_pack_start (GTK_BOX (box1), combo, FALSE, FALSE, 0);

}

static void
brasero_search_destroy (GtkObject *object)
{
	BraseroSearch *search;

	search = BRASERO_SEARCH (object);
	if (search->priv->query) {
		g_object_unref (search->priv->query);
		search->priv->query = NULL;
	}

	if (search->priv->client) {
		g_object_unref (search->priv->client);
		search->priv->client = NULL;
	}

	if (search->priv->tree) {
		brasero_search_empty_tree (search);
		search->priv->tree = NULL;
		g_signal_handlers_disconnect_by_func (search->priv->filter,
						      brasero_search_mime_filter_changed,
						      search);
		search->priv->filter = 0;
	}

	if (search->priv->id) {
		g_source_remove (search->priv->id);
		search->priv->id = 0;
	}

	if (search->priv->hits) {
		g_slist_foreach (search->priv->hits, (GFunc) beagle_hit_unref, NULL);
		g_slist_free (search->priv->hits);
		search->priv->hits = NULL;
	}

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
brasero_search_finalize (GObject *object)
{
	BraseroSearch *cobj;

	cobj = BRASERO_SEARCH (object);

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
brasero_search_try_again (BraseroSearch *search)
{
	search->priv->client = beagle_client_new (NULL);
	if (!search->priv->client)
		return TRUE;

	gtk_widget_set_sensitive (GTK_WIDGET (search), TRUE);
	search->priv->id = 0;
	return FALSE;
}

GtkWidget *
brasero_search_new ()
{
	BraseroSearch *obj;

	obj = BRASERO_SEARCH (g_object_new (BRASERO_TYPE_SEARCH, NULL));

	/* FIXME : there are better ways to do the following with the new API
	 * see beagle_util_daemon_is_running (void) */
	obj->priv->client = beagle_client_new (NULL);
	if (!obj->priv->client) {
		gtk_widget_set_sensitive (GTK_WIDGET (obj), FALSE);

		/* we will retry in 10 seconds */
		obj->priv->id = g_timeout_add (10000,
					       (GSourceFunc) brasero_search_try_again,
					       obj);
	}

	return GTK_WIDGET (obj);
}

static void
brasero_search_increase_activity (BraseroSearch *search)
{
	search->priv->activity ++;
	if (search->priv->activity == 1) {
		GdkCursor *cursor;

		cursor = gdk_cursor_new (GDK_WATCH);
		gdk_window_set_cursor (GTK_WIDGET (search)->window, cursor);
		gdk_cursor_unref (cursor);
	}
}

static void
brasero_search_decrease_activity (BraseroSearch *search)
{
	if (search->priv->activity == 0)
		return;

	search->priv->activity --;
	if (!search->priv->activity)
		gdk_window_set_cursor (GTK_WIDGET (search)->window, NULL);
}

static gchar **
brasero_search_get_selected_uris (BraseroURIContainer *container)
{
	BraseroSearch *search;

	search = BRASERO_SEARCH (container);
	return brasero_search_get_selected_rows (search);
}

static gchar *
brasero_search_get_selected_uri (BraseroURIContainer *container)
{
	BraseroSearch *search;
	gchar **uris = NULL;
	gchar *uri;

	search = BRASERO_SEARCH (container);
	uris = brasero_search_get_selected_rows (search);

	if (uris) {
		uri = g_strdup (uris [0]);
		g_strfreev (uris);
		return uri;
	}

	return NULL;
}

static void
brasero_search_empty_tree (BraseroSearch *search)
{
	GtkTreeModel *model;
	GtkTreeIter row;
	gchar *mime;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (search->priv->tree));
	model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));

	if (gtk_tree_model_get_iter_first (model, &row) == TRUE) {
		do {
			gtk_tree_model_get (model, &row,
					    BRASERO_SEARCH_TREE_MIME_COL, &mime, 
					    -1);
			brasero_mime_filter_unref_mime (BRASERO_MIME_FILTER (search->priv->filter), mime);
			g_free (mime);
		} while (gtk_list_store_remove (GTK_LIST_STORE (model), &row) == TRUE);
	}
}

static void
brasero_search_drag_data_get_cb (GtkTreeView *tree,
				 GdkDragContext *drag_context,
				 GtkSelectionData *selection_data,
				 guint info,
				 guint time, BraseroSearch *search)
{
	gchar **uris;

	uris = brasero_search_get_selected_rows (search);
	gtk_selection_data_set_uris (selection_data, uris);
	g_strfreev (uris);
}

static GSList *
brasero_search_add_hit_to_tree (BraseroSearch *search,
				GSList *list,
				gint max)
{
	GtkTreeModel *model;
	GtkTreeIter row;
	BeagleHit *hit;
	GSList *iter;
	GSList *next;

	gchar *name, *mime, *uri; 
	const gchar *icon_string = BRASERO_DEFAULT_ICON;
	const gchar *description;
	GIcon *icon;
	gint score;
	gint num;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (search->priv->tree));
	model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));

	num = 0;
	for (iter = list; iter && num < max; iter = next, num ++) {
		gchar *unescaped_uri, *uri;
		GFile *file;

		hit = iter->data;
		next = iter->next;

		uri = g_strdup (beagle_hit_get_uri (hit));
		file = g_file_new_for_uri (uri);

		/* beagle return badly formed uri not encoded in UTF-8
		 * locale charset so we check them just in case */
		unescaped_uri = g_uri_unescape_string (file, NULL);
		if (!g_utf8_validate (unescaped_uri, -1, NULL)) {
			g_free (unescaped_uri);
			g_free (uri);
			continue;
		}

		name = g_path_get_basename (unescaped_uri);
		g_free (unescaped_uri);

		mime = g_strdup (beagle_hit_get_mime_type (hit));
		if (!mime) {
			g_warning ("Strange beagle reports a URI (%s) but cannot tell the mime.\n", uri);
			g_free (name);
			g_free (uri);
			continue;
		}

		if (!strcmp (mime, "inode/directory")) {
			g_free (mime);
			mime = g_strdup ("x-directory/normal");
		}

		description = g_content_type_get_description (mime);

		icon = g_content_type_get_icon (mime);
		icon_string = NULL;
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

		score = (int) (beagle_hit_get_score (hit) * 100);

		gtk_list_store_append (GTK_LIST_STORE (model), &row);
		gtk_list_store_set (GTK_LIST_STORE (model), &row,
				    BRASERO_SEARCH_TREE_ICON_COL, icon_string,
				    BRASERO_SEARCH_TREE_TITLE_COL, name,
				    BRASERO_SEARCH_TREE_DESCRIPTION_COL, description,
				    BRASERO_SEARCH_TREE_URI_COL, uri,
				    BRASERO_SEARCH_TREE_SCORE_COL, score,
				    BRASERO_SEARCH_TREE_MIME_COL, mime,
				    -1);

		/* add the mime type to the filter combo */
		brasero_mime_filter_add_mime (BRASERO_MIME_FILTER (search->priv->filter), mime);

		g_object_unref (icon);
		g_free (name);
		g_free (mime);
		g_free (uri);
	}

	return iter;
}

static void
brasero_search_update_header (BraseroSearch *search)
{
	gchar *string;

	if (search->priv->hits_num) {
		gint last;

		last = search->priv->first_hit + search->priv->max_results;
		last = last <= search->priv->hits_num ? last : search->priv->hits_num;
		string = g_strdup_printf (_("<b>Results %i - %i (out of %i)</b>"),
					  search->priv->first_hit + 1,
					  last,
					  search->priv->hits_num);
	}
	else
		string = g_strdup (_("<b>No results</b>"));

	gtk_label_set_markup (GTK_LABEL (search->priv->results_label), string);
	g_free (string);

	if (search->priv->first_hit + search->priv->max_results < search->priv->hits_num)
		gtk_widget_set_sensitive (search->priv->right, TRUE);
	else
		gtk_widget_set_sensitive (search->priv->right, FALSE);

	if (search->priv->first_hit > 0)
		gtk_widget_set_sensitive (search->priv->left, TRUE);
	else
		gtk_widget_set_sensitive (search->priv->left, FALSE);
}

static void
brasero_search_max_results_num_changed_cb (GtkComboBox *combo,
					   BraseroSearch *search)
{
	gint index;

	index = gtk_combo_box_get_active (combo);
	switch (index) {
	case 0:
		search->priv->max_results = 20;
		break;
	case 1:
		search->priv->max_results = 50;
		break;
	case 2:
		search->priv->max_results = 100;
		break;
	}

	if (search->priv->hits_num) {
		GSList *first;

		brasero_search_empty_tree (search);
		first = g_slist_nth (search->priv->hits, search->priv->first_hit);
		brasero_search_add_hit_to_tree (search, first, search->priv->max_results);
	}

	brasero_search_update_header (search);
}

static void
brasero_search_left_button_clicked_cb (GtkButton *button,
				       BraseroSearch *search)
{
	GSList *first;

	if (!search->priv->first_hit)
		return;

	search->priv->first_hit -= search->priv->max_results;
	if (search->priv->first_hit < 0)
		search->priv->first_hit = 0;

	first = g_slist_nth (search->priv->hits, search->priv->first_hit);

	brasero_search_empty_tree (search);
	brasero_search_add_hit_to_tree (search, first, search->priv->max_results);
	brasero_search_update_header (search);
}

static void
brasero_search_right_button_clicked_cb (GtkButton *button,
					BraseroSearch *search)
{
	GSList *first;

	if (search->priv->first_hit + search->priv->max_results > search->priv->hits_num)
		return;

	search->priv->first_hit += search->priv->max_results;
	first = g_slist_nth (search->priv->hits, search->priv->first_hit);

	brasero_search_empty_tree (search);
	brasero_search_add_hit_to_tree (search, first, search->priv->max_results);
	brasero_search_update_header (search);
}

static void
brasero_search_check_for_possible_missing (BraseroSearch *search)
{
	gint num_missing;
	gint num_displayed;
	gint num_remaining;
	GtkTreeModel *model;

	/* not let's see if we should append new results */
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (search->priv->tree));
	model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));

	num_displayed = gtk_tree_model_iter_n_children (model, NULL);
	num_missing = search->priv->max_results - num_displayed;
	num_remaining = search->priv->hits_num - search->priv->first_hit;

	if (num_displayed == num_remaining)
		return;

	if (num_missing > 0) {
		GSList *first;

		first = g_slist_nth (search->priv->hits, search->priv->first_hit);
		brasero_search_add_hit_to_tree (search, first, num_missing);
	}
}

static gint
_sort_hits_by_score (BeagleHit *a, BeagleHit *b)
{
	gdouble score_a, score_b;

	score_a = beagle_hit_get_score (a);
	score_b = beagle_hit_get_score (b);

	if (score_b == score_a)
		return 0;

	if (score_b > score_a);
		return -1;

	return 1;
}

static void
brasero_search_beagle_hit_added_cb (BeagleQuery *query,
				    BeagleHitsAddedResponse *response,
				    BraseroSearch *search)
{
	GSList *list;

	/* NOTE : list must not be modified nor freed */
	list = beagle_hits_added_response_get_hits (response);
	search->priv->hits_num += g_slist_length (list);

	list = g_slist_copy (list);
	g_slist_foreach (list, (GFunc) beagle_hit_ref, NULL);

	if (!search->priv->hits) {
		search->priv->hits = g_slist_sort (list, (GCompareFunc) _sort_hits_by_score);
		brasero_search_add_hit_to_tree (search, search->priv->hits, search->priv->max_results);
	}
	else {
		GSList *first;

		search->priv->hits = g_slist_concat (search->priv->hits, list);
		search->priv->hits = g_slist_sort (search->priv->hits, (GCompareFunc) _sort_hits_by_score);

		brasero_search_empty_tree (search);
		first = g_slist_nth (search->priv->hits, search->priv->first_hit);
		brasero_search_add_hit_to_tree (search, first, search->priv->max_results);
	}

	brasero_search_update_header (search);
	brasero_search_check_for_possible_missing (search);
}

static void
brasero_search_beagle_hit_substracted_cb (BeagleQuery *query,
					  BeagleHitsSubtractedResponse *response,
					  BraseroSearch *search)
{
	gchar *uri;
	GSList *list, *iter;
	const gchar *removed_uri;

	GtkTreeModel *model;
	GtkTreeIter row;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (search->priv->tree));
	model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));

	list = beagle_hits_subtracted_response_get_uris (response);
	for (iter = list; iter; iter = iter->next) {
		GSList *next, *hit_iter;

		removed_uri = iter->data;

		if (gtk_tree_model_get_iter_first (model, &row)) {
			do {
				gtk_tree_model_get (model, &row,
						    BRASERO_SEARCH_TREE_URI_COL,
						    &uri, -1);
				if (!strcmp (uri, removed_uri)) {
					g_free (uri);
					gtk_list_store_remove
					    (GTK_LIST_STORE (model), &row);
					break;
				}

				g_free (uri);
			} while (gtk_tree_model_iter_next (model, &row));
		}

		/* see if it isn't in the hits that are still waiting */
		for (hit_iter = search->priv->hits; hit_iter; hit_iter = next) {
			BeagleHit *hit;
			const char *hit_uri;
	
			next = hit_iter->next;
			hit = hit_iter->data;

			hit_uri = beagle_hit_get_uri (hit);
			if (!strcmp (hit_uri, removed_uri)) {
				search->priv->hits = g_slist_remove (search->priv->hits, hit);
				beagle_hit_unref (hit);

				search->priv->hits_num --;
			}
		}
	}

	brasero_search_update_header (search);
	brasero_search_check_for_possible_missing (search);
}

static void
brasero_search_beagle_finished_cb (BeagleQuery *query,
				   BeagleFinishedResponse *response,
				   BraseroSearch *search)
{
	brasero_search_update_header (search);
	brasero_search_decrease_activity (search);
}

static void
brasero_search_beagle_error_dialog (BraseroSearch *search, GError *error)
{
	GtkWidget *dialog;
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (search));
	if (!GTK_WIDGET_TOPLEVEL (toplevel)) {
		g_warning ("Error querying beagle : %s\n", error->message);
		return;
	}

	dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					 GTK_DIALOG_DESTROY_WITH_PARENT|
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_CLOSE,
					 _("Error querying beagle:"));

	gtk_window_set_title (GTK_WINDOW (dialog), _("Search error"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  error->message);

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static void
brasero_search_beagle_error_cb (BeagleRequest *request,
				GError *error,
				BraseroSearch *search)
{
	brasero_search_update_header (search);
	if (error)
		brasero_search_beagle_error_dialog (search, error);
	brasero_search_decrease_activity (search);
}

static void
brasero_search_entry_activated_cb (BraseroSearchEntry *entry,
				   BraseroSearch *search)
{
	BeagleQuery *query;
	GError *error = NULL;

	/* we first empty everything including the filter box */
	brasero_search_empty_tree (search);
	if (search->priv->query) {
		brasero_search_decrease_activity (search);
		g_object_unref (search->priv->query);
		search->priv->query = NULL;
	}

	if (search->priv->hits) {
		g_slist_foreach (search->priv->hits, (GFunc) beagle_hit_unref, NULL);
		g_slist_free (search->priv->hits);
		search->priv->hits = NULL;
	}

	search->priv->hits_num = 0;
	search->priv->first_hit = 0;
	brasero_search_update_header (search);

	/* search itself */
	query = brasero_search_entry_get_query (entry);
	if (!query) {
		g_warning ("No query\n");
		return;
	}

	beagle_query_set_max_hits (query, 10000);
	g_signal_connect (G_OBJECT (query), "hits-added",
			  G_CALLBACK (brasero_search_beagle_hit_added_cb),
			  search);
	g_signal_connect (G_OBJECT (query), "hits-subtracted",
			  G_CALLBACK
			  (brasero_search_beagle_hit_substracted_cb),
			  search);
	g_signal_connect (G_OBJECT (query), "finished",
			  G_CALLBACK (brasero_search_beagle_finished_cb),
			  search);
	g_signal_connect (G_OBJECT (query), "error",
			  G_CALLBACK (brasero_search_beagle_error_cb),
			  search);
	beagle_client_send_request_async (search->priv->client,
					  BEAGLE_REQUEST (query),
					  &error);
	if (error) {
		brasero_search_beagle_error_dialog (search, error);
		g_error_free (error);
	}
	else {
		search->priv->query = query;
		brasero_search_increase_activity (search);
	}
}

static gboolean
brasero_search_is_visible_cb (GtkTreeModel *model,
			      GtkTreeIter *iter,
			      BraseroSearch *search)
{
	char *filename, *uri, *display_name, *mime_type;
	gboolean result;

	gtk_tree_model_get (model, iter,
			    BRASERO_SEARCH_TREE_TITLE_COL, &filename,
			    BRASERO_SEARCH_TREE_URI_COL, &uri,
			    BRASERO_SEARCH_TREE_TITLE_COL, &display_name,
			    BRASERO_SEARCH_TREE_MIME_COL, &mime_type, -1);

	result = brasero_mime_filter_filter (BRASERO_MIME_FILTER (search->priv->filter),
					     filename,
					     uri,
					     display_name,
					     mime_type);

	g_free (filename);
	g_free (uri);
	g_free (display_name);
	g_free (mime_type);
	return result;
}

static void
brasero_search_mime_filter_changed (GtkComboBox *combo,
				    BraseroSearch *search)
{
	gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (gtk_tree_view_get_model (GTK_TREE_VIEW (search->priv->tree))));
}

static void
brasero_search_tree_activated_cb (GtkTreeView *tree,
				  GtkTreeIter *row,
				  GtkTreeViewColumn *column,
				  BraseroSearch *search)
{
	brasero_uri_container_uri_activated (BRASERO_URI_CONTAINER (search));
}

static void
brasero_search_tree_selection_changed_cb (GtkTreeSelection *selection,
					  BraseroSearch *search)
{
	brasero_uri_container_uri_selected (BRASERO_URI_CONTAINER (search));
}

static char **
brasero_search_get_selected_rows (BraseroSearch *search)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter row;
	GList *rows, *iter;
	gchar **uris = NULL, *uri;
	gint i;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (search->priv->tree));
	rows = gtk_tree_selection_get_selected_rows (selection, &model);
	if (rows == NULL)
		return NULL;

	uris = g_new0 (char *, g_list_length (rows) + 1);
	for (iter = rows, i = 0; iter != NULL; iter = iter->next, i++) {
		gtk_tree_model_get_iter (model,
					 &row,
					 (GtkTreePath *) iter->data);
		gtk_tree_path_free (iter->data);
		gtk_tree_model_get (model, &row,
				    BRASERO_SEARCH_TREE_URI_COL, &uri,
				    -1);
		uris[i] = uri;
	}

	g_list_free (rows);
	return uris;
}

#endif
