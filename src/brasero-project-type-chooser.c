/***************************************************************************
*            cd-type-chooser.c
*
*  ven mai 27 17:33:12 2005
*  Copyright  2005  Philippe Rouquier
*  <brasero-app@wanadoo.fr>
****************************************************************************/
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

#include <gtk/gtkwidget.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkimage.h>
#include <gtk/gtktable.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtkmisc.h>

#include "brasero-project-type-chooser.h"
#include "brasero-utils.h"


G_DEFINE_TYPE (BraseroProjectTypeChooser, brasero_project_type_chooser, GTK_TYPE_EVENT_BOX);

typedef enum {
	RECENT_CLICKED_SIGNAL,
	CHOSEN_SIGNAL,
	LAST_SIGNAL
} BraseroProjectTypeChooserSignalType;
static guint brasero_project_type_chooser_signals [LAST_SIGNAL] = { 0 };

enum {
	BRASERO_PROJECT_TYPE_CHOOSER_ICON_COL,
	BRASERO_PROJECT_TYPE_CHOOSER_TEXT_COL,
	BRASERO_PROJECT_TYPE_CHOOSER_ID_COL,
	BRASERO_PROJECT_TYPE_CHOOSER_NB_COL
};

struct _ItemDescription {
	gchar *text;
  	gchar *description;
  	gchar *tooltip;
       	gchar *image;
	BraseroProjectType type;
};
typedef struct _ItemDescription ItemDescription;

static ItemDescription items [] = {
       {N_("<big>Audi_o project</big>"),
	N_("Create a traditional audio CD"),
	N_("Create a traditional audio CD that will be playable on computers and stereos"),
	"media-optical-audio-new",
	BRASERO_PROJECT_TYPE_AUDIO},
       {N_("<big>D_ata project</big>"),
	N_("Create a data CD/DVD"),
	N_("Create a CD/DVD containing any type of data that can only be read on a computer"),
	"media-optical-data-new",
	BRASERO_PROJECT_TYPE_DATA},
       {N_("<big>_Video project</big>"),
	N_("Create a video DVD or a SVCD"),
	N_("Create a video DVD or a SVCD that are readable on TV readers"),
	"media-optical-video-new",
	BRASERO_PROJECT_TYPE_VIDEO},
       {N_("<big>Disc _copy</big>"),
	N_("Create 1:1 copy of a CD/DVD"),
	N_("Create a 1:1 copy of an audio CD or a data CD/DVD on your hardisk or on another CD/DVD"),
	"media-optical-copy",
	BRASERO_PROJECT_TYPE_COPY},
       {N_("<big>Burn _image</big>"),
	N_("Burn an existing CD/DVD image to disc"),
	N_("Burn an existing CD/DVD image to disc"),
	"iso-image-burn",
	BRASERO_PROJECT_TYPE_ISO},
};

#define ID_KEY "ID-TYPE"
#define DESCRIPTION_KEY "DESCRIPTION_KEY"
#define LABEL_KEY "LABEL_KEY"

struct BraseroProjectTypeChooserPrivate {
	GdkPixbuf *background;
	GtkWidget *recent_box;
};

static GObjectClass *parent_class = NULL;

static void
brasero_project_type_chooser_button_clicked (GtkButton *button,
					     BraseroProjectTypeChooser *chooser)
{
	BraseroProjectType type;

	type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), ID_KEY));
	g_signal_emit (chooser,
		       brasero_project_type_chooser_signals [CHOSEN_SIGNAL],
		       0,
		       type);
}

static GtkWidget *
brasero_project_type_chooser_new_item (BraseroProjectTypeChooser *chooser,
				       ItemDescription *description)
{
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *image;
	GtkWidget *label;
	GtkWidget *eventbox;

	eventbox = gtk_button_new ();
	g_signal_connect (eventbox,
			  "clicked",
			  G_CALLBACK (brasero_project_type_chooser_button_clicked),
			  chooser);
	gtk_widget_show (eventbox);

	if (description->tooltip)
		gtk_widget_set_tooltip_text (eventbox, _(description->tooltip));

	g_object_set_data (G_OBJECT (eventbox),
			   ID_KEY,
			   GINT_TO_POINTER (description->type));
	g_object_set_data (G_OBJECT (eventbox),
			   DESCRIPTION_KEY,
			   description);


	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 4);
	gtk_container_add (GTK_CONTAINER (eventbox), vbox);

	hbox = gtk_hbox_new (FALSE, 4);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, FALSE, 0);

	image = gtk_image_new_from_icon_name (description->image, GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment (GTK_MISC (image), 1.0, 0.5);
	gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);

	vbox = gtk_vbox_new (TRUE, 4);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, TRUE, 0);

	label = gtk_label_new (NULL);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), eventbox);
	gtk_label_set_markup_with_mnemonic (GTK_LABEL (label), _(description->text));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);
	g_object_set_data (G_OBJECT (eventbox), LABEL_KEY, label);

	label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_label_set_markup (GTK_LABEL (label), _(description->description));
	gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, TRUE, 0);

	return eventbox;
}

static void
brasero_project_type_chooser_recent_clicked_cb (GtkButton *button,
						BraseroProjectTypeChooser *self)
{
	const gchar *uri;

	uri = gtk_link_button_get_uri (GTK_LINK_BUTTON (button));
	g_signal_emit (self,
		       brasero_project_type_chooser_signals [RECENT_CLICKED_SIGNAL],
		       0,
		       uri);
}

static gint
brasero_project_type_chooser_sort_recent (gconstpointer a, gconstpointer b)
{
	GtkRecentInfo *recent_a = (GtkRecentInfo *) a;
	GtkRecentInfo *recent_b = (GtkRecentInfo *) b;
	time_t timestamp_a;
	time_t timestamp_b;

	/* we get the soonest timestamp */
	timestamp_a = gtk_recent_info_get_visited (recent_a) > gtk_recent_info_get_modified (recent_a) ?
		      gtk_recent_info_get_visited (recent_a):
		      gtk_recent_info_get_modified (recent_a);
	timestamp_b = gtk_recent_info_get_visited (recent_b) > gtk_recent_info_get_modified (recent_b) ?
		      gtk_recent_info_get_visited (recent_b):
		      gtk_recent_info_get_modified (recent_b);
	return timestamp_b - timestamp_a;
}

static void
brasero_project_type_chooser_build_recent (BraseroProjectTypeChooser *self,
					   GtkRecentManager *recent)
{
	GtkSizeGroup *group;
	GList *list = NULL;
	GList *recents;
	GList *iter;

	recents = gtk_recent_manager_get_items (recent);
	for (iter = recents; iter; iter = iter->next) {
		GtkRecentInfo *info;
		const gchar *mime;

		info = iter->data;
		mime = gtk_recent_info_get_mime_type (info);
		if (!mime)
			continue;

		/* filter those we want */
		if (strcmp (mime, "application/x-brasero")
		&&  strcmp (mime, "application/x-cd-image")
		&&  strcmp (mime, "application/x-cdrdao-toc")
		&&  strcmp (mime, "application/x-toc")
		&&  strcmp (mime, "application/x-cue")
		&&  strcmp (mime, "audio/x-scpls")
		&&  strcmp (mime, "audio/x-ms-asx")
		&&  strcmp (mime, "audio/x-mp3-playlist")
		&&  strcmp (mime, "audio/x-mpegurl"))
			continue;

		/* sort */
		list = g_list_insert_sorted (list,
					     info,
					     brasero_project_type_chooser_sort_recent);
		if (g_list_length (list) > 5)
			list = g_list_delete_link (list, g_list_last (list));
	}

	group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);
	for (iter = list; iter; iter = iter->next) {
		GtkRecentInfo *info;
		const gchar *name;
		GdkPixbuf *pixbuf;
		GtkWidget *image;
		const gchar *uri;
		GtkWidget *hbox;
		GtkWidget *link;
		gchar *tooltip;

		info = iter->data;

		hbox = gtk_hbox_new (FALSE, 6);
		gtk_widget_show (hbox);
		gtk_box_pack_start (GTK_BOX (self->priv->recent_box),
				    hbox,
				    FALSE,
				    TRUE,
				    0);

		tooltip = gtk_recent_info_get_uri_display (info);

		pixbuf = gtk_recent_info_get_icon (info, GTK_ICON_SIZE_BUTTON);
		image = gtk_image_new_from_pixbuf (pixbuf);
		g_object_unref (pixbuf);

		gtk_widget_show (image);
		gtk_widget_set_tooltip_text (image, tooltip);

		name = gtk_recent_info_get_display_name (info);
		uri = gtk_recent_info_get_uri (info);

		link = gtk_link_button_new_with_label (uri, name);
		gtk_button_set_alignment (GTK_BUTTON (link), 0.0, 0.5);
		gtk_button_set_focus_on_click (GTK_BUTTON (link), FALSE);
		gtk_button_set_image (GTK_BUTTON (link), image);
		g_signal_connect (link,
				  "clicked",
				  G_CALLBACK (brasero_project_type_chooser_recent_clicked_cb),
				  self);

		gtk_widget_show (link);
		gtk_widget_set_tooltip_text (link, tooltip);
		gtk_box_pack_start (GTK_BOX (hbox), link, FALSE, TRUE, 0);

		g_free (tooltip);

		gtk_size_group_add_widget (group, link);
	}
	g_object_unref (group);

	if (!g_list_length (list)) {
		GtkWidget *label;
		gchar *string;

		string = g_strdup_printf ("<i>%s</i>", _("No recently used project"));
		label = gtk_label_new (string);
		gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
		g_free (string);

		gtk_widget_show (label);
		gtk_box_pack_start (GTK_BOX (self->priv->recent_box), label, FALSE, FALSE, 6);
	}

	g_list_free (list);

	g_list_foreach (recents, (GFunc) gtk_recent_info_unref, NULL);
	g_list_free (recents);
}

static void
brasero_project_type_chooser_recent_changed_cb (GtkRecentManager *recent,
						BraseroProjectTypeChooser *self)
{
	gtk_container_foreach (GTK_CONTAINER (self->priv->recent_box),
			       (GtkCallback) gtk_widget_destroy,
			       NULL);
	brasero_project_type_chooser_build_recent (self, recent);
}

static void
brasero_project_type_chooser_init (BraseroProjectTypeChooser *obj)
{
	GtkRecentManager *recent;
	GtkWidget *project_box;
	GtkWidget *recent_box;
	GError *error = NULL;
	GtkWidget *separator;
	GtkWidget *widget;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *vbox;
	int nb_rows = 1;
	gchar *string;
	int nb_items;
	int rows;
	int i;

	obj->priv = g_new0 (BraseroProjectTypeChooserPrivate, 1);

	obj->priv->background = gdk_pixbuf_new_from_file (BRASERO_DATADIR "/logo.png", &error);
	if (error) {
		g_warning ("ERROR loading background pix : %s\n", error->message);
		g_error_free (error);
		error = NULL;
	}

	vbox = gtk_hbox_new (FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (obj), vbox);

	project_box = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (project_box);
	gtk_box_pack_start (GTK_BOX (vbox), project_box, FALSE, TRUE, 0);

	string = g_strdup_printf ("<span size='x-large'><b>%s</b></span>", _("Create a new project:"));
	label = gtk_label_new (string);
	g_free (string);

	gtk_widget_show (label);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	gtk_box_pack_start (GTK_BOX (project_box), label, FALSE, FALSE, 0);

	/* get the number of rows */
	nb_items = sizeof (items) / sizeof (ItemDescription);
	rows = nb_items / nb_rows;
	if (nb_items % nb_rows)
		rows ++;

	table = gtk_table_new (rows, nb_rows, TRUE);
	gtk_container_set_border_width (GTK_CONTAINER (table), 6);
	gtk_box_pack_start (GTK_BOX (project_box), table, FALSE, TRUE, 0);

	gtk_table_set_col_spacings (GTK_TABLE (table), 4);
	gtk_table_set_row_spacings (GTK_TABLE (table), 4);

	for (i = 0; i < nb_items; i ++) {
		widget = brasero_project_type_chooser_new_item (obj, items + i);
		gtk_table_attach (GTK_TABLE (table),
				  widget,
				  i % nb_rows,
				  i % nb_rows + 1,
				  i / nb_rows,
				  i / nb_rows + 1,
				  GTK_EXPAND|GTK_FILL,
				  GTK_FILL,
				  0,
				  0);
	}
	gtk_widget_show_all (table);

	separator = gtk_vseparator_new ();
	gtk_widget_show (separator);
	gtk_box_pack_start (GTK_BOX (vbox), separator, FALSE, TRUE, 8);

	/* The recent files part */
	recent_box = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (recent_box);
	gtk_box_pack_start (GTK_BOX (vbox), recent_box, FALSE, TRUE, 0);

	string = g_strdup_printf ("<span size='x-large'><b>%s</b></span>", _("Recent projects:"));
	label = gtk_label_new (string);
	g_free (string);

	gtk_widget_show (label);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	gtk_box_pack_start (GTK_BOX (recent_box), label, FALSE, FALSE, 0);

	vbox = gtk_vbox_new (TRUE, 0);
	gtk_widget_show (vbox);
	gtk_box_pack_start (GTK_BOX (recent_box), vbox, FALSE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
	obj->priv->recent_box = vbox;

	recent = gtk_recent_manager_get_default ();
	brasero_project_type_chooser_build_recent (obj, recent);

	g_signal_connect (recent,
			  "changed",
			  G_CALLBACK (brasero_project_type_chooser_recent_changed_cb),
			  obj);
}

/* Cut and Pasted from Gtk+ gtkeventbox.c but modified to display back image */
static gboolean
brasero_project_type_expose_event (GtkWidget *widget, GdkEventExpose *event)
{
	BraseroProjectTypeChooser *chooser;

	chooser = BRASERO_PROJECT_TYPE_CHOOSER (widget);

	if (GTK_WIDGET_DRAWABLE (widget))
	{
		(* GTK_WIDGET_CLASS (parent_class)->expose_event) (widget, event);

		if (!GTK_WIDGET_NO_WINDOW (widget)) {
			if (!GTK_WIDGET_APP_PAINTABLE (widget)
			&&  chooser->priv->background) {
				int width, offset = 150;

				width = gdk_pixbuf_get_width (chooser->priv->background);
				gdk_draw_pixbuf (widget->window,
					         widget->style->white_gc,
						 chooser->priv->background,
						 offset,
						 0,
						 0,
						 0,
						 width - offset,
						 -1,
						 GDK_RGB_DITHER_NORMAL,
						 0, 0);
			}
		}
	}

	return FALSE;
}

static void
brasero_project_type_chooser_finalize (GObject *object)
{
	BraseroProjectTypeChooser *cobj;

	cobj = BRASERO_PROJECT_TYPE_CHOOSER (object);

	if (cobj->priv->background) {
		g_object_unref (G_OBJECT (cobj->priv->background));
		cobj->priv->background = NULL;
	}

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_project_type_chooser_class_init (BraseroProjectTypeChooserClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_project_type_chooser_finalize;
	widget_class->expose_event = brasero_project_type_expose_event;

	brasero_project_type_chooser_signals[CHOSEN_SIGNAL] =
	    g_signal_new ("chosen", G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_ACTION | G_SIGNAL_RUN_FIRST,
			  G_STRUCT_OFFSET (BraseroProjectTypeChooserClass, chosen),
			  NULL, NULL,
			  g_cclosure_marshal_VOID__UINT,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_UINT);
	brasero_project_type_chooser_signals[RECENT_CLICKED_SIGNAL] =
	    g_signal_new ("recent_clicked", G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_ACTION | G_SIGNAL_RUN_FIRST,
			  G_STRUCT_OFFSET (BraseroProjectTypeChooserClass, recent_clicked),
			  NULL, NULL,
			  g_cclosure_marshal_VOID__STRING,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_STRING);
}

GtkWidget *
brasero_project_type_chooser_new ()
{
	BraseroProjectTypeChooser *obj;

	obj = BRASERO_PROJECT_TYPE_CHOOSER (g_object_new (BRASERO_TYPE_PROJECT_TYPE_CHOOSER,
					    NULL));

	return GTK_WIDGET (obj);
}
