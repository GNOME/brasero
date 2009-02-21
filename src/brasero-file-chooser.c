/***************************************************************************
 *            brasero-file-chooser.c
 *
 *  lun mai 29 08:53:18 2006
 *  Copyright  2006  Rouquier Philippe
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

#include <string.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

#include "eggtreemultidnd.h"

#include "brasero-file-chooser.h"
#include "brasero-uri-container.h"
#include "brasero-layout-object.h"

static void brasero_file_chooser_class_init (BraseroFileChooserClass *klass);
static void brasero_file_chooser_init (BraseroFileChooser *sp);
static void brasero_file_chooser_iface_uri_container_init (BraseroURIContainerIFace *iface);
static void brasero_file_chooser_iface_layout_object_init (BraseroLayoutObjectIFace *iface);
static void brasero_file_chooser_finalize (GObject *object);

static void
brasero_file_chooser_uri_activated_cb (GtkFileChooser *widget,
				       BraseroFileChooser *chooser);
static void
brasero_file_chooser_uri_selection_changed_cb (GtkFileChooser *widget,
					       BraseroFileChooser *chooser);
struct BraseroFileChooserPrivate {
	GtkWidget *chooser;

	GtkFileFilter *filter_any;
	GtkFileFilter *filter_audio;
	GtkFileFilter *filter_video;

	BraseroLayoutType type;
};

static GObjectClass *parent_class = NULL;

GType
brasero_file_chooser_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroFileChooserClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_file_chooser_class_init,
			NULL,
			NULL,
			sizeof (BraseroFileChooser),
			0,
			(GInstanceInitFunc)brasero_file_chooser_init,
		};

		static const GInterfaceInfo uri_container_info =
		{
			(GInterfaceInitFunc) brasero_file_chooser_iface_uri_container_init,
			NULL,
			NULL
		};

		static const GInterfaceInfo layout_object =
		{
			(GInterfaceInitFunc) brasero_file_chooser_iface_layout_object_init,
			NULL,
			NULL
		};

		type = g_type_register_static (GTK_TYPE_ALIGNMENT, 
					       "BraseroFileChooser",
					       &our_info,
					       0);

		g_type_add_interface_static (type,
					     BRASERO_TYPE_URI_CONTAINER,
					     &uri_container_info);
		g_type_add_interface_static (type,
					     BRASERO_TYPE_LAYOUT_OBJECT,
					     &layout_object);
	}

	return type;
}

static void
brasero_file_chooser_class_init (BraseroFileChooserClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_file_chooser_finalize;
}

void
brasero_file_chooser_customize (GtkWidget *widget, gpointer null_data)
{
	/* we explore everything until we reach a treeview (there are two) */
	if (GTK_IS_TREE_VIEW (widget)) {
		GtkTargetList *list;
		GdkAtom target;
		gboolean found;
		guint num;

		list = gtk_drag_source_get_target_list (widget);
		target = gdk_atom_intern ("text/uri-list", TRUE);
		found = gtk_target_list_find (list, target, &num);
		/* FIXME: should we unref them ? apparently not according to 
		 * the warning messages we get if we do */

		if (found
		&&  gtk_tree_selection_get_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (widget))) == GTK_SELECTION_MULTIPLE) {
			gtk_tree_view_set_rubber_banding (GTK_TREE_VIEW (widget), TRUE);
			egg_tree_multi_drag_add_drag_support (GTK_TREE_VIEW (widget));
		}
	}
	else if (GTK_IS_BUTTON (widget)) {
		GtkWidget *image;
		gchar *stock_id = NULL;

		image = gtk_button_get_image (GTK_BUTTON (widget));
		if (!GTK_IS_IMAGE (image))
			return;

		gtk_image_get_stock (GTK_IMAGE (image), &stock_id, NULL);
		if (stock_id
		&& (!strcmp (stock_id,GTK_STOCK_ADD)
		||  !strcmp (stock_id, GTK_STOCK_REMOVE))) {
			GtkRequisition request;
			gint width, height;
			GtkWidget *parent;

			/* This is to avoid having the left part too small */
			parent = widget->parent;
			width = parent->requisition.width;
			height = parent->requisition.height;
			gtk_widget_size_request (parent, &request);
			if (request.width >= width)
				gtk_widget_set_size_request (parent,
							     request.width,
							     request.height);
			
			gtk_widget_hide (widget);
		}
	}
	else if (GTK_IS_CONTAINER (widget)) {
		gtk_container_foreach (GTK_CONTAINER (widget),
				       brasero_file_chooser_customize,
				       NULL);
	}
}

static void
brasero_file_chooser_init (BraseroFileChooser *obj)
{
	GtkFileFilter *filter;

	obj->priv = g_new0 (BraseroFileChooserPrivate, 1);

	obj->priv->chooser = gtk_file_chooser_widget_new (GTK_FILE_CHOOSER_ACTION_OPEN);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (obj->priv->chooser), FALSE);

	gtk_widget_show (obj->priv->chooser);

	gtk_container_add (GTK_CONTAINER (obj), obj->priv->chooser);

	g_signal_connect (obj->priv->chooser,
			  "file-activated",
			  G_CALLBACK (brasero_file_chooser_uri_activated_cb),
			  obj);
	g_signal_connect (obj->priv->chooser,
			  "selection-changed",
			  G_CALLBACK (brasero_file_chooser_uri_selection_changed_cb),
			  obj);

	gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (obj->priv->chooser), TRUE);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("All files"));
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (obj->priv->chooser), filter);

	obj->priv->filter_any = filter;

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("Audio files only"));
	gtk_file_filter_add_mime_type (filter, "audio/*");
	gtk_file_filter_add_mime_type (filter, "application/ogg");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (obj->priv->chooser), filter);

	obj->priv->filter_audio = filter;

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("Movies only"));
	gtk_file_filter_add_mime_type (filter, "video/*");
	gtk_file_filter_add_mime_type (filter, "application/ogg");
	gtk_file_filter_add_mime_type (filter, "application/x-flash-video");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (obj->priv->chooser), filter);

	obj->priv->filter_video = filter;

	filter = gtk_file_filter_new ();
	/* Translators: this is an image, a picture, not a "Disc Image" */
	gtk_file_filter_set_name (filter, _("Image files only"));
	gtk_file_filter_add_mime_type (filter, "image/*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (obj->priv->chooser), filter);

	/* this is a hack/workaround to add support for multi DND */
	gtk_container_foreach (GTK_CONTAINER (obj->priv->chooser),
			       brasero_file_chooser_customize,
			       NULL);
}

static void
brasero_file_chooser_finalize (GObject *object)
{
	BraseroFileChooser *cobj;

	cobj = BRASERO_FILE_CHOOSER (object);
	g_free (cobj->priv);

	G_OBJECT_CLASS(parent_class)->finalize(object);
}

GtkWidget *
brasero_file_chooser_new ()
{
	gpointer *obj;
	
	obj = g_object_new (BRASERO_TYPE_FILE_CHOOSER, NULL);
	
	return GTK_WIDGET (obj);
}

static void
brasero_file_chooser_find_pane (GtkWidget *child,
				gpointer footer)
{
	if (GTK_IS_PANED (child)) {
		GList *children_vbox;
		GList *iter_vbox;
		GtkWidget *vbox;

		vbox = gtk_paned_get_child2 (GTK_PANED (child));
		children_vbox = gtk_container_get_children (GTK_CONTAINER (vbox));
		for (iter_vbox = children_vbox; iter_vbox; iter_vbox = iter_vbox->next) {
			if (GTK_IS_HBOX (iter_vbox->data)) {
				GtkPackType packing;

				gtk_box_query_child_packing (GTK_BOX (vbox),
							     GTK_WIDGET (iter_vbox->data),
							     NULL,
							     NULL,
							     NULL,
							     &packing);

				if (packing == GTK_PACK_START) {
					GtkRequisition total_request, footer_request;

					gtk_widget_size_request (GTK_WIDGET (vbox),
								 &total_request);
					gtk_widget_size_request (GTK_WIDGET (iter_vbox->data),
								 &footer_request);
					*((gint *) footer) = total_request.height - footer_request.height;
					break;
				}
			}
		}
		g_list_free (children_vbox);
	}
	else if (GTK_IS_CONTAINER (child)) {
		gtk_container_foreach (GTK_CONTAINER (child),
				       brasero_file_chooser_find_pane,
				       footer);
	}
}

static void
brasero_file_chooser_uri_activated_cb (GtkFileChooser *widget,
				       BraseroFileChooser *chooser)
{
	brasero_uri_container_uri_activated (BRASERO_URI_CONTAINER (chooser));
}

static void
brasero_file_chooser_uri_selection_changed_cb (GtkFileChooser *widget,
					       BraseroFileChooser *chooser)
{
	brasero_uri_container_uri_selected (BRASERO_URI_CONTAINER (chooser));
}

static gchar *
brasero_file_chooser_get_selected_uri (BraseroURIContainer *container)
{
	BraseroFileChooser *chooser;

	chooser = BRASERO_FILE_CHOOSER (container);
	return gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (chooser->priv->chooser));
}

static gchar **
brasero_file_chooser_get_selected_uris (BraseroURIContainer *container)
{
	BraseroFileChooser *chooser;
	GSList *list, *iter;
	gchar **uris;
	gint i;

	chooser = BRASERO_FILE_CHOOSER (container);
	list = gtk_file_chooser_get_uris (GTK_FILE_CHOOSER (chooser->priv->chooser));

	uris = g_new0 (gchar*, g_slist_length (list) + 1);
	i = 0;

	for (iter = list; iter; iter = iter->next) {
		uris [i] = iter->data;
		i++;
	}

	g_slist_free (list);

	return uris;
}

static void
brasero_file_chooser_iface_uri_container_init (BraseroURIContainerIFace *iface)
{
	iface->get_selected_uri = brasero_file_chooser_get_selected_uri;
	iface->get_selected_uris = brasero_file_chooser_get_selected_uris;
}

static void
brasero_file_chooser_set_context (BraseroLayoutObject *object,
				  BraseroLayoutType type)
{
	BraseroFileChooser *self;

	self = BRASERO_FILE_CHOOSER (object);
	if (type == self->priv->type)
		return;

	if (type == BRASERO_LAYOUT_AUDIO)
		gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (self->priv->chooser),
					     self->priv->filter_audio);
	else if (type == BRASERO_LAYOUT_VIDEO)
		gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (self->priv->chooser),
					     self->priv->filter_video);
	else
		gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (self->priv->chooser),
					     self->priv->filter_any);

	self->priv->type = type;
}

static void
brasero_file_chooser_get_proportion (BraseroLayoutObject *object,
				     gint *header,
				     gint *center,
				     gint *footer)
{
	gtk_container_foreach (GTK_CONTAINER (object),
			       brasero_file_chooser_find_pane,
			       footer);
}

static void
brasero_file_chooser_iface_layout_object_init (BraseroLayoutObjectIFace *iface)
{
	iface->get_proportion = brasero_file_chooser_get_proportion;
	iface->set_context = brasero_file_chooser_set_context;
}
