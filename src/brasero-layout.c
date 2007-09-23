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
#include <gtk/gtklabel.h>
#include <gtk/gtkvpaned.h>
#include <gtk/gtkimage.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtkhseparator.h>
#include <gtk/gtkalignment.h>

#include <gconf/gconf-client.h>

#include "brasero-layout.h"
#include "brasero-layout-object.h"

static void brasero_layout_class_init (BraseroLayoutClass *klass);
static void brasero_layout_init (BraseroLayout *sp);
static void brasero_layout_finalize (GObject *object);

static void
brasero_layout_preview_toggled_cb (GtkToggleAction *action,
				   BraseroLayout *layout);
static void
brasero_layout_radio_toggled_cb (GtkRadioAction *action,
				 GtkRadioAction *current,
				 BraseroLayout *layout);

static void
brasero_layout_page_showed (GtkWidget *widget,
			    BraseroLayout *layout);

static void
brasero_layout_show (GtkWidget *widget);
static void
brasero_layout_hide (GtkWidget *widget);

typedef struct {
	gchar *id;
	GtkWidget *widget;
	BraseroLayoutType types;

	gint is_active:1;
} BraseroLayoutItem;

struct BraseroLayoutPrivate {
	GtkActionGroup *action_group;

	gint accel;

	BraseroLayoutType type;
	GSList *items;
	BraseroLayoutItem *active_item;

	GConfClient *client;
	gint radio_notify;
	gint preview_notify;

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
#define BRASERO_LAYOUT_NONE_MENU	N_("_No Selection Pane")
#define BRASERO_LAYOUT_NONE_TOOLTIP	N_("Show the project only")
#define BRASERO_LAYOUT_NONE_ICON	NULL

/* GCONF keys */
#define BRASERO_KEY_DISPLAY_DIR		"/apps/brasero/display/"
#define BRASERO_KEY_SHOW_PREVIEW	BRASERO_KEY_DISPLAY_DIR "preview"
#define BRASERO_KEY_LAYOUT_AUDIO	BRASERO_KEY_DISPLAY_DIR "audio_pane"
#define BRASERO_KEY_LAYOUT_DATA		BRASERO_KEY_DISPLAY_DIR "data_pane"

GType
brasero_layout_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroLayoutClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_layout_class_init,
			NULL,
			NULL,
			sizeof (BraseroLayout),
			0,
			(GInstanceInitFunc)brasero_layout_init,
		};

		type = g_type_register_static (GTK_TYPE_HPANED, 
					       "BraseroLayout",
					       &our_info,
					       0);
	}

	return type;
}

static void
brasero_layout_class_init (BraseroLayoutClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *gtk_widget_class = GTK_WIDGET_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_layout_finalize;

	gtk_widget_class->hide = brasero_layout_hide;
	gtk_widget_class->show = brasero_layout_show;
}

static void
brasero_layout_init (BraseroLayout *obj)
{
	GtkWidget *alignment;

	obj->priv = g_new0 (BraseroLayoutPrivate, 1);

	obj->priv->action_group = gtk_action_group_new ("BraseroLayoutActions");
	gtk_action_group_set_translation_domain (obj->priv->action_group, GETTEXT_PACKAGE);

	/* init GConf */
	obj->priv->client = gconf_client_get_default ();

	/* set up containers */
	alignment = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
	gtk_widget_show (alignment);
	gtk_paned_pack2 (GTK_PANED (obj), alignment, TRUE, FALSE);

	obj->priv->main_box = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (alignment), obj->priv->main_box);
	gtk_widget_show (obj->priv->main_box);

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

static void
brasero_layout_finalize (GObject *object)
{
	BraseroLayout *cobj;
	GSList *iter;

	cobj = BRASERO_LAYOUT(object);

	for (iter = cobj->priv->items; iter; iter = iter->next) {
		BraseroLayoutItem *item;

		item = iter->data;
		g_free (item->id);
		g_free (item);
	}

	g_slist_free (cobj->priv->items);
	cobj->priv->items = NULL;

	/* close GConf */
	gconf_client_notify_remove (cobj->priv->client, cobj->priv->preview_notify);
	gconf_client_notify_remove (cobj->priv->client, cobj->priv->radio_notify);
	g_object_unref (cobj->priv->client);

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkWidget *
brasero_layout_new ()
{
	BraseroLayout *obj;
	
	obj = BRASERO_LAYOUT (g_object_new (BRASERO_TYPE_LAYOUT, NULL));

	return GTK_WIDGET (obj);
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

static void
brasero_layout_set_active_item (BraseroLayout *layout,
				BraseroLayoutItem *item,
				gboolean active)
{
	gboolean preview_in_project;
	GtkWidget *toplevel;
	GtkWidget *project;
	gint width, height;
	GtkAction *action;
	GList *children;

	action = gtk_action_group_get_action (layout->priv->action_group, item->id);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), active);

	if (!active) {
		if (item->widget)
			gtk_widget_hide (item->widget);

		return;
	}

	layout->priv->active_item = item;

    	children = gtk_container_get_children (GTK_CONTAINER (layout->priv->main_box));
	preview_in_project = (g_list_find (children, layout->priv->preview_pane) == NULL);
	g_list_free (children);

	project = gtk_paned_get_child1 (GTK_PANED (layout));

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (layout));
	gtk_window_get_size (GTK_WINDOW (toplevel), &width, &height);

	if (item->widget) {
		gboolean add_size = TRUE;

		if (preview_in_project) {
			/* we need to unparent the preview widget
			 * and set it back where it was */
			g_object_ref (layout->priv->preview_pane);
			gtk_container_remove (GTK_CONTAINER (project),
					      layout->priv->preview_pane);

			gtk_box_pack_end (GTK_BOX (layout->priv->main_box),
					  layout->priv->preview_pane, 
					  FALSE,
					  FALSE,
					  0);
			g_object_unref (layout->priv->preview_pane);
		}

		if (GTK_WIDGET_REALIZED (layout->priv->main_box))
			add_size = FALSE;

		gtk_widget_show (item->widget);
		gtk_widget_show (layout->priv->main_box->parent);

		width = layout->priv->main_box->allocation.width;
		if (add_size)
			width += toplevel->allocation.width;

		height = MAX (height, layout->priv->main_box->allocation.height);
	}
	else {
		if (!preview_in_project) {
			/* we need to unparent the preview widget
			 * and set it under the project */
			g_object_ref (layout->priv->preview_pane);
			gtk_container_remove (GTK_CONTAINER (layout->priv->main_box),
					      layout->priv->preview_pane);

			gtk_box_pack_end (GTK_BOX (project),
					  layout->priv->preview_pane,
					  FALSE,
					  FALSE,
					  0);
			g_object_unref (layout->priv->preview_pane);
		}

		width -= layout->priv->main_box->allocation.width;
		gtk_widget_hide (layout->priv->main_box->parent);
	}

	gtk_window_resize (GTK_WINDOW (toplevel), width, height);
}

static void
brasero_layout_add_pressed_cb (GtkWidget *project,
			       BraseroLayout *layout)
{
	GtkAction *action;

	action = gtk_action_group_get_action (layout->priv->action_group,
					      BRASERO_LAYOUT_NONE_ID);

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action))) {
		GSList *iter;

		for (iter = layout->priv->items; iter; iter = iter->next) {
			BraseroLayoutItem *item;

			item = iter->data;

			if ((item->types & layout->priv->type)
			&&   strcmp (item->id, BRASERO_LAYOUT_NONE_ID)) {
				action = gtk_action_group_get_action (layout->priv->action_group, item->id);
				gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
				break;
			}
		}
	}
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
	GtkWidget *project;

	alignment = layout->priv->main_box->parent;

	project = gtk_paned_get_child1 (GTK_PANED (layout));
	if (!project)
		return;

	brasero_layout_object_get_proportion (BRASERO_LAYOUT_OBJECT (project),
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
	/* we connect to project to know when add button is clicked. That
	 * way if nothing but the project is shown we'll show the first
	 * pane in the list */
	g_signal_connect (project,
			  "add-pressed",
			  G_CALLBACK (brasero_layout_add_pressed_cb),
			  layout);
	g_signal_connect (project,
			  "size-allocate",
			  G_CALLBACK (brasero_layout_project_size_allocated_cb),
			  layout);

	gtk_paned_pack1 (GTK_PANED (layout), project, TRUE, FALSE);
}

static GtkWidget *
_make_pane (GtkWidget *widget,
	    const gchar *stock_id,
	    const gchar *icon_name,
	    const gchar *text,
	    const gchar *subtitle,
	    gboolean fill)
{
	GtkWidget *alignment;
	GtkWidget *retval;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *image;

	retval = gtk_vbox_new (FALSE, 1);
	gtk_widget_show (retval);

	gtk_box_pack_end (GTK_BOX (retval), widget, fill, fill, 0);

	alignment = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 0, 0, 0, 0);
	gtk_widget_show (alignment);
	gtk_box_pack_start (GTK_BOX (retval), alignment, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox);
	gtk_container_add (GTK_CONTAINER (alignment), hbox);

	if (stock_id)
		image = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_LARGE_TOOLBAR);
	else
		image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_LARGE_TOOLBAR);

	gtk_widget_show (image);
	gtk_misc_set_alignment (GTK_MISC (image), 0.0, 0.0);
	gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, TRUE, 0);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, TRUE, 0);

	label = gtk_label_new (text);
	gtk_widget_show (label);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	label = gtk_label_new (subtitle);
	gtk_widget_show (label);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

	return retval;
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

	gtk_box_pack_end (GTK_BOX (layout->priv->main_box),
			  layout->priv->preview_pane,
			  FALSE,
			  FALSE,
			  0);

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
brasero_layout_pane_changed (BraseroLayout *layout, const gchar *id)
{
	GSList *iter;
	BraseroLayoutItem *item;

	for (iter = layout->priv->items; iter; iter = iter->next) {
		item = iter->data;

		if (!strcmp (id, item->id))
			brasero_layout_set_active_item (layout, item, TRUE);
		else
			brasero_layout_set_active_item (layout, item, FALSE);
	}
}

static void
brasero_layout_displayed_item_changed_cb (GConfClient *client,
					  guint cxn,
					  GConfEntry *entry,
					  gpointer data)
{
	BraseroLayout *layout;
	GConfValue *value;
	const gchar *id;

	layout = BRASERO_LAYOUT (data);

	/* this is only called if the changed gconf key is the
	 * one corresponding to the current type of layout */
	if (!strcmp (entry->key, BRASERO_KEY_LAYOUT_AUDIO)
	&&  layout->priv->type == BRASERO_LAYOUT_AUDIO)
		return;

	if (!strcmp (entry->key, BRASERO_KEY_LAYOUT_DATA)
	&&  layout->priv->type == BRASERO_LAYOUT_DATA)
		return;

	value = gconf_entry_get_value (entry);
	if (value->type != GCONF_VALUE_STRING)
		return;

	id = gconf_value_get_string (value);

	/* we need to set the active radio button */
	brasero_layout_pane_changed (layout, id);
}

static BraseroLayoutItem*
brasero_layout_find_item_by_id (BraseroLayout *layout, const gchar *id)
{
	GSList *iter;
	BraseroLayoutItem *item;

	for (iter = layout->priv->items; iter; iter = iter->next) {
		item = iter->data;

		if (!strcmp (item->id, id))
			return item;
	}

	return NULL;
}

static void
brasero_layout_radio_toggled_cb (GtkRadioAction *action,
				 GtkRadioAction *current,
				 BraseroLayout *layout)
{
	BraseroLayoutItem *item;
	GError *error = NULL;
	const gchar *id;

	/* NOTE:	this signal is emitted on every radio button
	* 		when the current active button changes */
	id = gtk_action_get_name (GTK_ACTION (action));
	item = brasero_layout_find_item_by_id (layout, id);

	if (current != action) {
		brasero_layout_set_active_item (layout, item, FALSE);
		return;
	}

	brasero_layout_set_active_item (layout, item, TRUE);

	/* update gconf value */
	if (layout->priv->radio_notify)
		gconf_client_notify_remove (layout->priv->client,
					    layout->priv->radio_notify);

	if (layout->priv->type == BRASERO_LAYOUT_AUDIO) {
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
	else if (layout->priv->type == BRASERO_LAYOUT_DATA) {
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
			   const gchar *name,
			   const gchar *subtitle,
			   const gchar *menu,
			   const gchar *tooltip,
			   const gchar *stock_id,
			   const gchar *icon_name,
			   BraseroLayoutType types)
{
	GtkWidget *pane;
	gchar *accelerator;
	BraseroLayoutItem *item;
	GtkRadioActionEntry entries;

	pane = _make_pane (source, stock_id, icon_name, name, subtitle, TRUE);
	g_signal_connect (pane,
			  "show",
			  G_CALLBACK (brasero_layout_page_showed),
			  layout);
	gtk_notebook_append_page (GTK_NOTEBOOK (layout->priv->notebook),
				  pane,
				  NULL);

	/* add menu radio entry in display */
	accelerator = g_strdup_printf ("F%i", (layout->priv->accel ++) + 8);
	entries.name = id;
	entries.stock_id = stock_id?stock_id:icon_name;
	entries.label = menu;
	entries.accelerator = accelerator;
	entries.tooltip = tooltip;
	entries.value = 1;

	gtk_action_group_add_radio_actions (layout->priv->action_group,
					    &entries,
					    1,
					    1,
					    G_CALLBACK (brasero_layout_radio_toggled_cb),
					    layout);
	g_free (accelerator);
					    
	/* add it to the items list */
	item = g_new0 (BraseroLayoutItem, 1);
	item->id = g_strdup (id);
	item->widget = pane;
	item->types = types;

	layout->priv->items = g_slist_append (layout->priv->items, item);
}

/**************************** empty view callback ******************************/
static void
brasero_layout_add_empty_view (BraseroLayout *layout,
			       GtkUIManager *manager)
{
	BraseroLayoutItem *item;
	GtkRadioActionEntry entry = { BRASERO_LAYOUT_NONE_ID,
					BRASERO_LAYOUT_NONE_ICON,
					N_(BRASERO_LAYOUT_NONE_MENU),
					"F7",
					N_(BRASERO_LAYOUT_NONE_TOOLTIP),
					1 };

	/* add empty view */
	gtk_action_group_add_radio_actions (layout->priv->action_group,
					    &entry,
					    1,
					    1,
					    G_CALLBACK (brasero_layout_radio_toggled_cb),
					    layout);

	gtk_ui_manager_insert_action_group (manager,
					    layout->priv->action_group,
					    0);

	item = g_new0 (BraseroLayoutItem, 1);
	item->id = g_strdup (BRASERO_LAYOUT_NONE_ID);
	item->widget = NULL;
	item->types = BRASERO_LAYOUT_AUDIO|BRASERO_LAYOUT_DATA;

	layout->priv->items = g_slist_prepend (layout->priv->items, item);
}

void
brasero_layout_load (BraseroLayout *layout, BraseroLayoutType type)
{
	gboolean right_pane_visible = FALSE;
	gchar *layout_id = NULL;
	GError *error = NULL;
	GSList *iter;

	/* remove GCONF notification if any */
	if (layout->priv->radio_notify)
		gconf_client_notify_remove (layout->priv->client,
					    layout->priv->radio_notify);

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

	layout->priv->type = type;
	for (iter = layout->priv->items; iter; iter = iter->next) {
		GtkAction *action;
		BraseroLayoutObject *object;
		BraseroLayoutItem *item = NULL;

	    	item = iter->data;

		/* tell all the object what context we are in */
		object = brasero_layout_item_get_object (item);
		if (object)
			brasero_layout_object_set_context (object, type);

		action = gtk_action_group_get_action (layout->priv->action_group, item->id);
		if (!(item->types & type)) {
			gtk_widget_hide (item->widget);
			gtk_action_set_visible (action, FALSE);
			continue;
		}

	    	gtk_action_set_visible (action, TRUE);
		if (layout_id && !strcmp (layout_id, item->id)) {
			/* this is it! we found the pane to display */
			if (strcmp (layout_id, BRASERO_LAYOUT_NONE_ID))
				right_pane_visible = TRUE;

			brasero_layout_set_active_item (layout, item, TRUE);
		}
		else
			brasero_layout_set_active_item (layout, item, FALSE);
	}

	if (!right_pane_visible) {
		GtkAction *action;

		gtk_widget_hide (layout->priv->main_box->parent);

		action = gtk_action_group_get_action (layout->priv->action_group, BRASERO_LAYOUT_NONE_ID);
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
	}
	else
		gtk_widget_show (layout->priv->main_box->parent);

	g_free (layout_id);
}

void
brasero_layout_register_menu (BraseroLayout *layout,
			      GtkUIManager *manager)
{
	GSList *iter;
	GtkAction *action;
	GSList *group = NULL;
	GString *description;
	GError *error = NULL;
	BraseroLayoutItem *item;

	brasero_layout_add_empty_view (layout, manager);

	/* build the description of the menu */
	description = g_string_new ("<ui>"
				    "<menubar name='menubar' >"
				    "<menu action='EditMenu'>"
				    "</menu>"
				    "<menu action='ViewMenu'>"
				    "<placeholder name='ViewPlaceholder'/>"
				    "<menuitem action='EmptyView'/>");

	for (iter = layout->priv->items; iter; iter = iter->next) {
		item = iter->data;
		g_string_append_printf (description,
					"<menuitem action='%s'/>",
					item->id);

		/* we set all the radio buttons to belong to the same group */
		action = gtk_action_group_get_action (layout->priv->action_group, item->id);
		gtk_radio_action_set_group (GTK_RADIO_ACTION (action), group);
		group = gtk_radio_action_get_group (GTK_RADIO_ACTION (action));
	}

	g_string_append_printf (description, "<separator/>");
	g_string_append_printf (description,
				"<menuitem action='%s'/>",
				BRASERO_LAYOUT_PREVIEW_ID);

	g_string_append (description,
			 "<separator/>"
			 "</menu>"
			 "</menubar>"
			 "</ui>");

	if (!gtk_ui_manager_add_ui_from_string (manager, description->str, -1, &error)) {
		g_message ("building menus failed: %s", error->message);
		g_error_free (error);
	}

	g_string_free (description, TRUE);
}
