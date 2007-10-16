/***************************************************************************
 *            brasero-layout.c
 *
 *  mer mai 24 15:14:42 2006
 *  Copyright  2006  Rouquier Philippe
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtkuimanager.h>
#include <gtk/gtktoggleaction.h>
#include <gtk/gtkradioaction.h>
#include <gtk/gtkaction.h>

#include <gtk/gtkstock.h>

#include <gtk/gtkbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkvpaned.h>
#include <gtk/gtkimage.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtkhseparator.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtkcombobox.h>
#include <gtk/gtkcellrenderer.h>
#include <gtk/gtkcellrendererpixbuf.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkcelllayout.h>

#include <gconf/gconf-client.h>

#include "burn-debug.h"
#include "brasero-layout.h"
#include "brasero-preview.h"
#include "brasero-project.h"
#include "brasero-uri-container.h"
#include "brasero-layout-object.h"

G_DEFINE_TYPE (BraseroLayout, brasero_layout, GTK_TYPE_VBOX);

enum {
	TEXT_COL,
	ICON_COL,
	ITEM_COL,
	VISIBLE_COL,
	NB_COL
};

typedef struct {
	gchar *id;
	GtkWidget *widget;
	BraseroLayoutType types;

	guint is_active:1;
} BraseroLayoutItem;

typedef enum {
	BRASERO_LAYOUT_RIGHT = 0,
	BRASERO_LAYOUT_LEFT,
	BRASERO_LAYOUT_TOP,
	BRASERO_LAYOUT_BOTTOM,
} BraseroLayoutLocation;

struct BraseroLayoutPrivate {
	GtkActionGroup *action_group;

	gint accel;

	GtkUIManager *manager;

	BraseroLayoutLocation layout_type;
	GtkWidget *pane;

	GtkWidget *project;

	GtkWidget *combo;
	BraseroLayoutType ctx_type;
	BraseroLayoutItem *active_item;

	GConfClient *client;
	gint radio_notify;
	gint preview_notify;
	gint layout_notify;

	GtkWidget *notebook;
	GtkWidget *main_box;
	GtkWidget *preview_pane;
};

static GObjectClass *parent_class = NULL;

#define BRASERO_LAYOUT_PREVIEW_ID	"Viewer"
#define BRASERO_LAYOUT_PREVIEW_NAME	N_("Preview")
#define BRASERO_LAYOUT_PREVIEW_MENU	N_("P_review")
#define BRASERO_LAYOUT_PREVIEW_TOOLTIP	N_("Display video, audio and image preview")
#define BRASERO_LAYOUT_PREVIEW_ICON	GTK_STOCK_FILE

#define BRASERO_LAYOUT_NONE_ID		"EmptyView"
#define BRASERO_LAYOUT_NONE_MENU	N_("_Show side panel")
#define BRASERO_LAYOUT_NONE_TOOLTIP	N_("Show a side pane along the project")
#define BRASERO_LAYOUT_NONE_ICON	NULL

static void
brasero_layout_empty_toggled_cb (GtkToggleAction *action,
				 BraseroLayout *layout);

const GtkToggleActionEntry entries [] = {
	{ BRASERO_LAYOUT_NONE_ID, BRASERO_LAYOUT_NONE_ICON, N_(BRASERO_LAYOUT_NONE_MENU),
	  "F7", N_(BRASERO_LAYOUT_NONE_TOOLTIP), G_CALLBACK (brasero_layout_empty_toggled_cb), 1 }
};

const gchar description [] = "<ui>"
				"<menubar name='menubar' >"
				"<menu action='ViewMenu'>"
				"<placeholder name='ViewPlaceholder'>"
				"<menuitem action='"BRASERO_LAYOUT_NONE_ID"'/>"
				"<separator/>"
				"</placeholder>"
				"</menu>"
				"</menubar>"
			     "</ui>";

/* GCONF keys */
#define BRASERO_KEY_DISPLAY_LAYOUT	"/apps/brasero/display/layout"
#define BRASERO_KEY_DISPLAY_POSITION	"/apps/brasero/display/position"


#define BRASERO_KEY_DISPLAY_DIR		"/apps/brasero/display/"
#define BRASERO_KEY_SHOW_PREVIEW	BRASERO_KEY_DISPLAY_DIR "preview"
#define BRASERO_KEY_LAYOUT_AUDIO	BRASERO_KEY_DISPLAY_DIR "audio_pane"
#define BRASERO_KEY_LAYOUT_DATA		BRASERO_KEY_DISPLAY_DIR "data_pane"

static void
brasero_layout_pack_preview (BraseroLayout *layout)
{
	if (layout->priv->layout_type == BRASERO_LAYOUT_LEFT) {
		gtk_box_pack_end (GTK_BOX (layout->priv->main_box),
				  layout->priv->preview_pane,
				  FALSE,
				  FALSE,
				  0);
	}
	else if (layout->priv->layout_type == BRASERO_LAYOUT_TOP) {
		gtk_box_pack_end (GTK_BOX (layout->priv->main_box),
				  layout->priv->preview_pane,
				  FALSE,
				  FALSE,
				  0);
	}
	else if (layout->priv->layout_type == BRASERO_LAYOUT_RIGHT) {
		gtk_box_pack_end (GTK_BOX (layout->priv->main_box),
				  layout->priv->preview_pane,
				  FALSE,
				  FALSE,
				  0);
	}
	else if (layout->priv->layout_type == BRASERO_LAYOUT_BOTTOM) {
		gtk_box_pack_start (GTK_BOX (layout->priv->project),
				    layout->priv->preview_pane,
				    FALSE,
				    FALSE,
				    0);
	}
}

static void
brasero_layout_show (GtkWidget *widget)
{
	BraseroLayout *layout;

	layout = BRASERO_LAYOUT (widget);

	gtk_action_group_set_visible (layout->priv->action_group, TRUE);
	gtk_action_group_set_sensitive (layout->priv->action_group, TRUE);
	gtk_widget_set_sensitive (widget, TRUE);

	if (GTK_WIDGET_CLASS (parent_class)->show)
		GTK_WIDGET_CLASS (parent_class)->show (widget);
}

static void
brasero_layout_hide (GtkWidget *widget)
{
	BraseroLayout *layout;

	layout = BRASERO_LAYOUT (widget);

	gtk_action_group_set_visible (layout->priv->action_group, FALSE);
	gtk_action_group_set_sensitive (layout->priv->action_group, FALSE);
	gtk_widget_set_sensitive (widget, FALSE);

	if (GTK_WIDGET_CLASS (parent_class)->hide)
		GTK_WIDGET_CLASS (parent_class)->hide (widget);
}

static BraseroLayoutObject *
brasero_layout_item_get_object (BraseroLayoutItem *item)
{
	BraseroLayoutObject *source;
	GList *children;
	GList *child;

	source = NULL;
	if (!item || !GTK_IS_CONTAINER (item->widget))
		return NULL;

	children = gtk_container_get_children (GTK_CONTAINER (item->widget));
	for (child = children; child; child = child->next) {
		if (BRASERO_IS_LAYOUT_OBJECT (child->data)) {
			source = child->data;
			break;
		}
	}
	g_list_free (children);

	if (!source || !BRASERO_IS_LAYOUT_OBJECT (source)) 
		return NULL;

	return source;
}

static void
brasero_layout_size_reallocate (BraseroLayout *layout)
{
	gint pr_header, pr_center, pr_footer;
	gint header, center, footer;
	BraseroLayoutObject *source;
	GtkWidget *alignment;

	alignment = layout->priv->main_box->parent;

	if (layout->priv->layout_type == BRASERO_LAYOUT_TOP
	||  layout->priv->layout_type == BRASERO_LAYOUT_BOTTOM) {
		gtk_alignment_set_padding (GTK_ALIGNMENT (alignment),
					   0.0,	
					   0.0,
					   0.0,
					   0.0);
		return;
	}

	brasero_layout_object_get_proportion (BRASERO_LAYOUT_OBJECT (layout->priv->project),
					      &pr_header,
					      &pr_center,
					      &pr_footer);

	source = brasero_layout_item_get_object (layout->priv->active_item);
	if (!source)
		return;

	header = 0;
	center = 0;
	footer = 0;
	brasero_layout_object_get_proportion (BRASERO_LAYOUT_OBJECT (source),
					      &header,
					      &center,
					      &footer);

	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment),
				   0.0,	
				   pr_footer - footer,
				   0.0,
				   0.0);
}

static void
brasero_layout_page_showed (GtkWidget *widget,
			    BraseroLayout *layout)
{
	brasero_layout_size_reallocate (layout);
}

static void
brasero_layout_project_size_allocated_cb (GtkWidget *widget,
					  GtkAllocation *allocation,
					  BraseroLayout *layout)
{
	brasero_layout_size_reallocate (layout);
}

void
brasero_layout_add_project (BraseroLayout *layout,
			    GtkWidget *project)
{
	GtkWidget *box;

	g_signal_connect (project,
			  "size-allocate",
			  G_CALLBACK (brasero_layout_project_size_allocated_cb),
			  layout);

	if (layout->priv->layout_type == BRASERO_LAYOUT_TOP
	||  layout->priv->layout_type == BRASERO_LAYOUT_LEFT)
		box = gtk_paned_get_child1 (GTK_PANED (layout->priv->pane));
	else
		box = gtk_paned_get_child2 (GTK_PANED (layout->priv->pane));

	gtk_box_pack_end (GTK_BOX (box), project, TRUE, TRUE, 0);
	layout->priv->project = project;
}

static void
brasero_layout_preview_toggled_cb (GtkToggleAction *action, BraseroLayout *layout)
{
	gboolean active;

	active = gtk_toggle_action_get_active (action);
	if (active)
		gtk_widget_show (layout->priv->preview_pane);
	else
		gtk_widget_hide (layout->priv->preview_pane);
	
	/* we set the correct value in GConf */
	gconf_client_set_bool (layout->priv->client,
			       BRASERO_KEY_SHOW_PREVIEW,
			       active,
			       NULL);
}

static void
brasero_layout_preview_changed_cb (GConfClient *client,
				   guint cxn,
				   GConfEntry *entry,
				   gpointer data)
{
	BraseroLayout *layout;
	GtkAction *action;
	GConfValue *value;
	gboolean active;

	layout = BRASERO_LAYOUT (data);

	value = gconf_entry_get_value (entry);
	if (value->type != GCONF_VALUE_BOOL)
		return;

	active = gconf_value_get_bool (value);
	action = gtk_action_group_get_action (layout->priv->action_group,
					      BRASERO_LAYOUT_PREVIEW_ID);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), active);

 	if (active)
		gtk_widget_show (layout->priv->preview_pane);
	else
		gtk_widget_hide (layout->priv->preview_pane);
}

void
brasero_layout_add_preview (BraseroLayout *layout,
			    GtkWidget *preview)
{
	gboolean active;
	GtkAction *action;
	gchar *accelerator;
	GError *error = NULL;
	GtkToggleActionEntry entry;

	layout->priv->preview_pane = preview;
	brasero_layout_pack_preview (layout);

	/* add menu entry in display */
	accelerator = g_strdup ("F11");

	entry.name = BRASERO_LAYOUT_PREVIEW_ID;
	entry.stock_id = BRASERO_LAYOUT_PREVIEW_ICON;
	entry.label = BRASERO_LAYOUT_PREVIEW_MENU;
	entry.accelerator = accelerator;
	entry.tooltip = BRASERO_LAYOUT_PREVIEW_TOOLTIP;
	entry.is_active = FALSE;
	entry.callback = G_CALLBACK (brasero_layout_preview_toggled_cb);

	gtk_action_group_add_toggle_actions (layout->priv->action_group,
					     &entry,
					     1,
					     layout);
	g_free (accelerator);

	/* initializes the display */
	active = gconf_client_get_bool (layout->priv->client,
					BRASERO_KEY_SHOW_PREVIEW,
					&error);
	if (error) {
		g_warning ("Can't access GConf key %s. This is probably harmless (first launch of brasero).\n", error->message);
		g_error_free (error);
		error = NULL;
	}

	action = gtk_action_group_get_action (layout->priv->action_group, BRASERO_LAYOUT_PREVIEW_ID);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), active);

	if (active)
		gtk_widget_show (layout->priv->preview_pane);
	else
		gtk_widget_hide (layout->priv->preview_pane);

	if (!layout->priv->preview_notify)
		layout->priv->preview_notify = gconf_client_notify_add (layout->priv->client,
									BRASERO_KEY_SHOW_PREVIEW,
									brasero_layout_preview_changed_cb,
									layout,
									NULL,
									&error);
	if (error) {
		g_warning ("Could set notify for GConf key %s.\n", error->message);
		g_error_free (error);
		error = NULL;
	}
}

/**************************** for the source panes *****************************/
static void
brasero_layout_set_side_pane_visible (BraseroLayout *layout,
				      gboolean visible)
{
	gboolean preview_in_project;
	GList *children;

	children = gtk_container_get_children (GTK_CONTAINER (layout->priv->main_box));
	preview_in_project = (g_list_find (children, layout->priv->preview_pane) == NULL);
	g_list_free (children);

	if (!visible) {
		/* No side pane should be visible */
		if (!preview_in_project) {
			/* we need to unparent the preview widget
			 * and set it under the project */
			g_object_ref (layout->priv->preview_pane);
			gtk_container_remove (GTK_CONTAINER (layout->priv->main_box),
					      layout->priv->preview_pane);

			gtk_box_pack_start (GTK_BOX (layout->priv->project),
					    layout->priv->preview_pane,
					    FALSE,
					    FALSE,
					    0);
			g_object_unref (layout->priv->preview_pane);
		}

		brasero_project_set_source (BRASERO_PROJECT (layout->priv->project), NULL);
		gtk_widget_hide (layout->priv->main_box->parent);
	}
	else {
		BraseroLayoutObject *source;

		/* The side pane should be visible */
		if (preview_in_project) {
			/* we need to unparent the preview widget
			 * and set it back where it was */
			g_object_ref (layout->priv->preview_pane);
			gtk_container_remove (GTK_CONTAINER (layout->priv->project),
					      layout->priv->preview_pane);

			brasero_layout_pack_preview (layout);
			g_object_unref (layout->priv->preview_pane);
		}

		/* Now tell the project which source it gets URIs from */
		source = brasero_layout_item_get_object (layout->priv->active_item);
		if (!BRASERO_IS_URI_CONTAINER (source)) {
			BRASERO_BURN_LOG ("Item is not an URI container");
			brasero_project_set_source (BRASERO_PROJECT (layout->priv->project),
						    NULL);
		}
		else
			brasero_project_set_source (BRASERO_PROJECT (layout->priv->project),
						    BRASERO_URI_CONTAINER (source));

		gtk_widget_show (layout->priv->main_box->parent);
	}
}

static void
brasero_layout_item_set_active (BraseroLayout *layout,
				BraseroLayoutItem *item)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (layout->priv->combo));
	if (!gtk_tree_model_get_iter_first (model, &iter))
		return;

	do {
		BraseroLayoutItem *tree_item;

		gtk_tree_model_get (model, &iter,
				    ITEM_COL, &tree_item,
				    -1);

		if (tree_item == item) {
			gtk_combo_box_set_active_iter (GTK_COMBO_BOX (layout->priv->combo), &iter);
			return;
		}
	} while (gtk_tree_model_iter_next (model, &iter));

	gtk_widget_show (item->widget);
}

static void
brasero_layout_displayed_item_changed_cb (GConfClient *client,
					  guint cxn,
					  GConfEntry *entry,
					  gpointer data)
{
	BraseroLayout *layout;
	GConfValue *value;
	GtkTreeModel *model;
	GtkAction *action;
	GtkTreeIter iter;
	const gchar *id;

	layout = BRASERO_LAYOUT (data);

	/* this is only called if the changed gconf key is the
	 * one corresponding to the current type of layout */
	if (!strcmp (entry->key, BRASERO_KEY_LAYOUT_AUDIO)
	&&  layout->priv->ctx_type == BRASERO_LAYOUT_AUDIO)
		return;

	if (!strcmp (entry->key, BRASERO_KEY_LAYOUT_DATA)
	&&  layout->priv->ctx_type == BRASERO_LAYOUT_DATA)
		return;

	value = gconf_entry_get_value (entry);
	if (value->type != GCONF_VALUE_STRING)
		return;

	id = gconf_value_get_string (value);

	action = gtk_action_group_get_action (layout->priv->action_group, BRASERO_LAYOUT_NONE_ID);
	if (!id || !strcmp (id, BRASERO_LAYOUT_NONE_ID)) {
		/* nothing should be displayed */
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
		brasero_layout_set_side_pane_visible (layout, FALSE);
		return;
	}

	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), FALSE);
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (layout->priv->combo));
	model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));

	if (!gtk_tree_model_get_iter_first (model, &iter))
		return;

	do {
		BraseroLayoutItem *item;

		gtk_tree_model_get (model, &iter,
				    ITEM_COL, &item,
				    -1);
		if (!strcmp (id, item->id))
			brasero_layout_item_set_active (layout, item);
	} while (gtk_tree_model_iter_next (model, &iter));
}

static void
brasero_layout_save (BraseroLayout *layout,
		     const gchar *id)
{
	GError *error = NULL;

	/* update gconf value */
	if (layout->priv->radio_notify)
		gconf_client_notify_remove (layout->priv->client,
					    layout->priv->radio_notify);

	if (layout->priv->ctx_type == BRASERO_LAYOUT_AUDIO) {
		gconf_client_set_string (layout->priv->client,
					 BRASERO_KEY_LAYOUT_AUDIO,
					 id,
					 &error);

		if (error) {
			g_warning ("Can't set GConf key %s. \n", error->message);
			g_error_free (error);
			error = NULL;
		}

		layout->priv->radio_notify = gconf_client_notify_add (layout->priv->client,
								      BRASERO_KEY_LAYOUT_AUDIO,
								      brasero_layout_displayed_item_changed_cb,
								      layout,
								      NULL,
								      &error);
	}
	else if (layout->priv->ctx_type == BRASERO_LAYOUT_DATA) {
		gconf_client_set_string (layout->priv->client,
					 BRASERO_KEY_LAYOUT_DATA,
					 id,
					 &error);

		if (error) {
			g_warning ("Can't set GConf key %s. \n", error->message);
			g_error_free (error);
			error = NULL;
		}

		layout->priv->radio_notify = gconf_client_notify_add (layout->priv->client,
								      BRASERO_KEY_LAYOUT_DATA,
								      brasero_layout_displayed_item_changed_cb,
								      layout,
								      NULL,
								      &error);
	}

	if (error) {
		g_warning ("Can't set GConf notify on key %s. \n", error->message);
		g_error_free (error);
		error = NULL;
	}
}

void
brasero_layout_add_source (BraseroLayout *layout,
			   GtkWidget *source,
			   const gchar *id,
			   const gchar *subtitle,
			   const gchar *tooltip,
			   const gchar *icon_name,
			   BraseroLayoutType types)
{
	GtkWidget *pane;
	GtkTreeIter iter;
	GtkTreeModel *model;
	BraseroLayoutItem *item;

	pane = gtk_vbox_new (FALSE, 1);
	gtk_widget_hide (pane);
	gtk_box_pack_end (GTK_BOX (pane), source, TRUE, TRUE, 0);
	g_signal_connect (pane,
			  "show",
			  G_CALLBACK (brasero_layout_page_showed),
			  layout);
	gtk_notebook_append_page (GTK_NOTEBOOK (layout->priv->notebook),
				  pane,
				  NULL);

	/* add it to the items list */
	item = g_new0 (BraseroLayoutItem, 1);
	item->id = g_strdup (id);
	item->widget = pane;
	item->types = types;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (layout->priv->combo));
	model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
	gtk_list_store_append (GTK_LIST_STORE (model), &iter);
	gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			    ICON_COL,icon_name,
			    TEXT_COL,subtitle,
			    ITEM_COL, item,
			    VISIBLE_COL, TRUE,
			    -1);
}

/**************************** empty view callback ******************************/
static void
brasero_layout_combo_changed_cb (GtkComboBox *combo,
				 BraseroLayout *layout)
{
	BraseroLayoutObject *source;
	BraseroLayoutItem *item;
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_combo_box_get_model (combo);
	if (!gtk_combo_box_get_active_iter (combo, &iter))
		return;

	gtk_tree_model_get (model, &iter,
			    ITEM_COL, &item,
			    -1);

	if (layout->priv->active_item)
		gtk_widget_hide (layout->priv->active_item->widget);

	layout->priv->active_item = item;
	gtk_widget_show (item->widget);

	source = brasero_layout_item_get_object (item);
	if (!BRASERO_IS_URI_CONTAINER (source)) {
		BRASERO_BURN_LOG ("Item is not an URI container");
		brasero_project_set_source (BRASERO_PROJECT (layout->priv->project),
					    NULL);
	}
	else
		brasero_project_set_source (BRASERO_PROJECT (layout->priv->project),
					    BRASERO_URI_CONTAINER (source));

	brasero_layout_save (layout, item->id);
}

static void
brasero_layout_item_set_visible (BraseroLayout *layout,
				 BraseroLayoutItem *item,
				 gboolean visible)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (layout->priv->combo));
	model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
	if (!gtk_tree_model_get_iter_first (model, &iter))
		return;

	do {
		BraseroLayoutItem *tree_item;

		gtk_tree_model_get (model, &iter,
				    ITEM_COL, &tree_item,
				    -1);

		if (tree_item == item) {
			gtk_list_store_set (GTK_LIST_STORE (model), &iter,
					    VISIBLE_COL, visible,
					    -1);
			break;
		}

	} while (gtk_tree_model_iter_next (model, &iter));

	if (visible)
		gtk_widget_show (item->widget);
	else
		gtk_widget_hide (item->widget);
}

void
brasero_layout_load (BraseroLayout *layout, BraseroLayoutType type)
{
	gchar *layout_id = NULL;
	GError *error = NULL;
	GtkTreeModel *model;
	GtkAction *action;
	GtkTreeIter iter;

	/* remove GCONF notification if any */
	if (layout->priv->radio_notify)
		gconf_client_notify_remove (layout->priv->client,
					    layout->priv->radio_notify);

	if (layout->priv->preview_pane)
		brasero_preview_hide (BRASERO_PREVIEW (layout->priv->preview_pane));

	if (type == BRASERO_LAYOUT_NONE) {
		gtk_widget_hide (GTK_WIDGET (layout));
		return;
	}

	gtk_widget_show (GTK_WIDGET (layout));

	/* takes care of other panes */
	if (type == BRASERO_LAYOUT_AUDIO)
		layout_id = gconf_client_get_string (layout->priv->client,
						     BRASERO_KEY_LAYOUT_AUDIO,
						     &error);
	else if (type == BRASERO_LAYOUT_DATA)
		layout_id = gconf_client_get_string (layout->priv->client,
						     BRASERO_KEY_LAYOUT_DATA,
						     &error);

	if (error) {
		g_warning ("Can't access GConf key %s. This is probably harmless (first launch of brasero).\n", error->message);
		g_error_free (error);
		error = NULL;
	}

	/* add new notify for the new */
	if (type == BRASERO_LAYOUT_AUDIO)
		layout->priv->radio_notify = gconf_client_notify_add (layout->priv->client,
								      BRASERO_KEY_LAYOUT_AUDIO,
								      brasero_layout_displayed_item_changed_cb,
								      layout,
								      NULL,
								      &error);
	else if (type == BRASERO_LAYOUT_DATA)
		layout->priv->radio_notify = gconf_client_notify_add (layout->priv->client,
								      BRASERO_KEY_LAYOUT_DATA,
								      brasero_layout_displayed_item_changed_cb,
								      layout,
								      NULL,
								      &error);

	if (error) {
		g_warning ("Could not set notify for GConf key %s.\n", error->message);
		g_error_free (error);
		error = NULL;
	}

	/* even if we're not showing a side pane go through all items to make 
	 * sure they have the proper state in case the user wants to activate
	 * side pane again */
	layout->priv->ctx_type = type;
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (layout->priv->combo));
	model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
	if (gtk_tree_model_get_iter_first (model, &iter)) {
		do {
			BraseroLayoutObject *object;
			BraseroLayoutItem *item = NULL;

			gtk_tree_model_get (model, &iter,
					    ITEM_COL, &item,
					    -1);

			/* tell all the object what context we are in */
			object = brasero_layout_item_get_object (item);
			if (object)
				brasero_layout_object_set_context (object, type);

			/* check if that pane should be displayed in such a context */
			if (!(item->types & type)) {
				brasero_layout_item_set_visible (layout, item, FALSE);
				continue;
			}

			brasero_layout_item_set_visible (layout, item, TRUE);

			if (layout_id && !strcmp (layout_id, item->id))
				brasero_layout_item_set_active (layout, item);
			else
				gtk_widget_hide (item->widget);
		} while (gtk_tree_model_iter_next (model, &iter));
	}

	/* make sure there is a default for the pane */
	if (!layout->priv->active_item)
		gtk_combo_box_set_active (GTK_COMBO_BOX (layout->priv->combo), 0);

	/* hide or show side pane */
	action = gtk_action_group_get_action (layout->priv->action_group, BRASERO_LAYOUT_NONE_ID);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				     (layout_id && strcmp (layout_id, BRASERO_LAYOUT_NONE_ID)));

	g_free (layout_id);
}

static void
brasero_layout_pane_moved_cb (GtkWidget *paned,
			      GParamSpec *pspec,
			      BraseroLayout *layout)
{
	gconf_client_set_int (layout->priv->client,
			      BRASERO_KEY_DISPLAY_POSITION,
			      gtk_paned_get_position (GTK_PANED (paned)),
			      NULL);
}

static void
brasero_layout_type_changed_cb (GConfClient *client,
				guint cxn,
				GConfEntry *entry,
				gpointer data)
{
	GConfValue *value;
	GtkWidget *source_pane;
	GtkWidget *project_pane;
	BraseroLayoutType layout_type;
	BraseroLayout *layout = BRASERO_LAYOUT (data);

	value = gconf_entry_get_value (entry);
	if (value->type != GCONF_VALUE_INT)
		return;

	g_object_ref (layout->priv->preview_pane);
	if (layout->priv->layout_type == BRASERO_LAYOUT_BOTTOM)
		gtk_container_remove (GTK_CONTAINER (layout->priv->project), layout->priv->preview_pane);
	else
		gtk_container_remove (GTK_CONTAINER (layout->priv->main_box), layout->priv->preview_pane);

	if (layout->priv->layout_type == BRASERO_LAYOUT_TOP
	||  layout->priv->layout_type == BRASERO_LAYOUT_LEFT) {
		project_pane = gtk_paned_get_child1 (GTK_PANED (layout->priv->pane));
		source_pane = gtk_paned_get_child2 (GTK_PANED (layout->priv->pane));
	}
	else {
		source_pane = gtk_paned_get_child1 (GTK_PANED (layout->priv->pane));
		project_pane = gtk_paned_get_child2 (GTK_PANED (layout->priv->pane));
	}

	g_object_ref (source_pane);
	gtk_container_remove (GTK_CONTAINER (layout->priv->pane), source_pane);

	g_object_ref (project_pane);
	gtk_container_remove (GTK_CONTAINER (layout->priv->pane), project_pane);

	gtk_widget_destroy (layout->priv->pane);
	layout->priv->pane = NULL;

	layout_type = gconf_value_get_int (value);
	if (layout_type > BRASERO_LAYOUT_BOTTOM
	||  layout_type < BRASERO_LAYOUT_RIGHT)
		layout_type = BRASERO_LAYOUT_RIGHT;

	switch (layout_type) {
		case BRASERO_LAYOUT_TOP:
		case BRASERO_LAYOUT_BOTTOM:
			layout->priv->pane = gtk_vpaned_new ();
			break;

		case BRASERO_LAYOUT_RIGHT:
		case BRASERO_LAYOUT_LEFT:
			layout->priv->pane = gtk_hpaned_new ();
			break;

		default:
			break;
	}

	gtk_widget_show (layout->priv->pane);
	gtk_box_pack_end (GTK_BOX (layout), layout->priv->pane, TRUE, TRUE, 0);

	switch (layout_type) {
		case BRASERO_LAYOUT_TOP:
		case BRASERO_LAYOUT_LEFT:
			gtk_paned_pack2 (GTK_PANED (layout->priv->pane), source_pane, FALSE, FALSE);
			gtk_paned_pack1 (GTK_PANED (layout->priv->pane), project_pane, TRUE, FALSE);
			break;

		case BRASERO_LAYOUT_BOTTOM:
		case BRASERO_LAYOUT_RIGHT:
			gtk_paned_pack2 (GTK_PANED (layout->priv->pane), project_pane, TRUE, FALSE);
			gtk_paned_pack1 (GTK_PANED (layout->priv->pane), source_pane, FALSE, FALSE);
			break;

		default:
			break;
	}

	g_signal_connect (layout->priv->pane,
			  "notify::position",
			  G_CALLBACK (brasero_layout_pane_moved_cb),
			  layout);

	layout->priv->layout_type = layout_type;
	g_object_unref (project_pane);
	g_object_unref (source_pane);

	brasero_layout_pack_preview (layout);
	g_object_unref (layout->priv->preview_pane);

	brasero_layout_size_reallocate (layout);
}

static void
brasero_layout_close_button_clicked_cb (GtkWidget *button,
					BraseroLayout *layout)
{
	GtkAction *action;

	action = gtk_action_group_get_action (layout->priv->action_group,
					      BRASERO_LAYOUT_NONE_ID);

	if (!action)
		return;

	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), FALSE);
}

static void
brasero_layout_empty_toggled_cb (GtkToggleAction *action,
				 BraseroLayout *layout)
{
	gboolean active;

	active = gtk_toggle_action_get_active (action);
	brasero_layout_set_side_pane_visible (layout, active);

	if (!active)
		brasero_layout_save (layout, BRASERO_LAYOUT_NONE_ID);
	else if (layout->priv->active_item)
		brasero_layout_save (layout, layout->priv->active_item->id);
}

void
brasero_layout_register_ui (BraseroLayout *layout,
			    GtkUIManager *manager)
{
	GtkWidget *toolbar;
	GError *error = NULL;

	/* should be called only once */
	gtk_ui_manager_insert_action_group (manager,
					    layout->priv->action_group,
					    0);

	if (!gtk_ui_manager_add_ui_from_string (manager, description, -1, &error)) {
		g_message ("building menus failed: %s", error->message);
		g_error_free (error);
	}

	/* get the toolbar */
	toolbar = gtk_ui_manager_get_widget (manager, "/Toolbar");
	if (toolbar)
		gtk_box_pack_start (GTK_BOX (layout), toolbar, FALSE, FALSE, 0);

	layout->priv->manager = manager;
}

static gboolean
brasero_layout_foreach_item_cb (GtkTreeModel *model,
				GtkTreePath *path,
				GtkTreeIter *iter,
				gpointer NULL_data)
{
	BraseroLayoutItem *item;

	gtk_tree_model_get (model, iter,
			    ITEM_COL, &item,
			    -1);
	g_free (item->id);
	g_free (item);

	return FALSE;
}

static void
brasero_layout_destroy (GtkObject *object)
{
	BraseroLayout *cobj;
	GtkTreeModel *model;

	cobj = BRASERO_LAYOUT(object);

	if (!cobj->priv->client) {
		GTK_OBJECT_CLASS (parent_class)->destroy (object);
		return;
	}

	/* close GConf */
	gconf_client_notify_remove (cobj->priv->client, cobj->priv->layout_notify);
	gconf_client_notify_remove (cobj->priv->client, cobj->priv->preview_notify);
	gconf_client_notify_remove (cobj->priv->client, cobj->priv->radio_notify);
	g_object_unref (cobj->priv->client);
	cobj->priv->client = NULL;

	/* empty tree */
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (cobj->priv->combo));
	model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
	gtk_tree_model_foreach (model,
				brasero_layout_foreach_item_cb,
				NULL);

	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
brasero_layout_finalize (GObject *object)
{
	BraseroLayout *cobj;

	cobj = BRASERO_LAYOUT(object);

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_layout_class_init (BraseroLayoutClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (klass);
	GtkWidgetClass *gtk_widget_class = GTK_WIDGET_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_layout_finalize;

	gtk_widget_class->hide = brasero_layout_hide;
	gtk_widget_class->show = brasero_layout_show;

	gtk_object_class->destroy = brasero_layout_destroy;
}

static void
brasero_layout_init (BraseroLayout *obj)
{
	GtkCellRenderer *renderer;
	GtkWidget *alignment;
	GtkListStore *store;
	GtkTreeModel *model;
	GtkWidget *button;
	GtkWidget *box;
	gint position;

	obj->priv = g_new0 (BraseroLayoutPrivate, 1);

	/* menu */
	obj->priv->action_group = gtk_action_group_new ("BraseroLayoutActions");
	gtk_action_group_set_translation_domain (obj->priv->action_group, 
						 GETTEXT_PACKAGE);
	gtk_action_group_add_toggle_actions (obj->priv->action_group,
					     entries,
					     1,
					     obj);

	/* init GConf */
	obj->priv->client = gconf_client_get_default ();

	/* get our layout */
	obj->priv->layout_type = gconf_client_get_int (obj->priv->client,
						       BRASERO_KEY_DISPLAY_LAYOUT,
						       NULL);

	if (obj->priv->layout_type > BRASERO_LAYOUT_BOTTOM
	||  obj->priv->layout_type < BRASERO_LAYOUT_RIGHT)
		obj->priv->layout_type = BRASERO_LAYOUT_RIGHT;

	switch (obj->priv->layout_type) {
		case BRASERO_LAYOUT_TOP:
		case BRASERO_LAYOUT_BOTTOM:
			obj->priv->pane = gtk_vpaned_new ();
			break;

		case BRASERO_LAYOUT_RIGHT:
		case BRASERO_LAYOUT_LEFT:
			obj->priv->pane = gtk_hpaned_new ();
			break;

		default:
			break;
	}

	g_signal_connect (obj->priv->pane,
			  "notify::position",
			  G_CALLBACK (brasero_layout_pane_moved_cb),
			  obj);

	gtk_widget_show (obj->priv->pane);
	gtk_box_pack_end (GTK_BOX (obj), obj->priv->pane, TRUE, TRUE, 0);

	/* remember the position */
	position = gconf_client_get_int (obj->priv->client,
					 BRASERO_KEY_DISPLAY_POSITION,
					 NULL);
	if (position > 0)
		gtk_paned_set_position (GTK_PANED (obj->priv->pane), position);

	/* set up pane for project */
	box = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (box);

	if (obj->priv->layout_type == BRASERO_LAYOUT_TOP
	||  obj->priv->layout_type == BRASERO_LAYOUT_LEFT)
		gtk_paned_pack1 (GTK_PANED (obj->priv->pane), box, TRUE, FALSE);
	else
		gtk_paned_pack2 (GTK_PANED (obj->priv->pane), box, TRUE, FALSE);

	obj->priv->layout_notify = gconf_client_notify_add (obj->priv->client,
							    BRASERO_KEY_DISPLAY_LAYOUT,
							    brasero_layout_type_changed_cb,
							    obj,
							    NULL,
							    NULL);

	/* set up containers */
	alignment = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
	gtk_widget_show (alignment);

	if (obj->priv->layout_type == BRASERO_LAYOUT_TOP
	||  obj->priv->layout_type == BRASERO_LAYOUT_LEFT)
		gtk_paned_pack2 (GTK_PANED (obj->priv->pane), alignment, FALSE, FALSE);
	else
		gtk_paned_pack1 (GTK_PANED (obj->priv->pane), alignment, FALSE, FALSE);

	obj->priv->main_box = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (alignment), obj->priv->main_box);
	gtk_widget_show (obj->priv->main_box);

	/* close button and  combo */
	box = gtk_hbox_new (FALSE, 6);
	gtk_widget_show (box);
	gtk_box_pack_start (GTK_BOX (obj->priv->main_box),
			    box,
			    FALSE,
			    FALSE,
			    3);

	store = gtk_list_store_new (NB_COL,
				    G_TYPE_STRING,
				    G_TYPE_STRING,
				    G_TYPE_POINTER,
				    G_TYPE_BOOLEAN);
	model = gtk_tree_model_filter_new (GTK_TREE_MODEL (store), NULL);
	gtk_tree_model_filter_set_visible_column (GTK_TREE_MODEL_FILTER (model), VISIBLE_COL);
	g_object_unref (G_OBJECT (store));

	obj->priv->combo = gtk_combo_box_new_with_model (model);
	g_object_set (obj->priv->combo,
		      "has-frame", FALSE,
		      NULL);
	g_signal_connect (obj->priv->combo,
			  "changed",
			  G_CALLBACK (brasero_layout_combo_changed_cb),
			  obj);
	gtk_widget_show (obj->priv->combo);
	g_object_unref (G_OBJECT (model));

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (obj->priv->combo), renderer,
				    FALSE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (obj->priv->combo),
				       renderer, "icon-name",
				       ICON_COL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (obj->priv->combo), renderer,
				    FALSE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (obj->priv->combo),
				       renderer, "markup",
				       TEXT_COL);

	gtk_box_pack_start (GTK_BOX (box), obj->priv->combo, TRUE, TRUE, 0);


	button = gtk_button_new ();
	gtk_button_set_image (GTK_BUTTON (button), gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_BUTTON));
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	gtk_widget_set_tooltip_text (button, _("Click to close the side pane"));
	gtk_widget_show (button);
	g_signal_connect (button,
			  "clicked",
			  G_CALLBACK (brasero_layout_close_button_clicked_cb),
			  obj);
	gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);


	obj->priv->notebook = gtk_notebook_new ();
	gtk_widget_show (obj->priv->notebook);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (obj->priv->notebook), FALSE);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (obj->priv->notebook), FALSE);
	gtk_box_pack_start (GTK_BOX (obj->priv->main_box),
			    obj->priv->notebook,
			    TRUE,
			    TRUE,
			    0);
}

GtkWidget *
brasero_layout_new ()
{
	BraseroLayout *obj;
	
	obj = BRASERO_LAYOUT (g_object_new (BRASERO_TYPE_LAYOUT, NULL));
	return GTK_WIDGET (obj);
}
