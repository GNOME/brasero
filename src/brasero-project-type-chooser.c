/***************************************************************************
*            cd-type-chooser.c
*
*  ven mai 27 17:33:12 2005
*  Copyright  2005  Philippe Rouquier
*  <brasero-app@wanadoo.fr>
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

static void brasero_project_type_chooser_class_init (BraseroProjectTypeChooserClass *klass);
static void brasero_project_type_chooser_init (BraseroProjectTypeChooser *sp);
static void brasero_project_type_chooser_finalize (GObject *object);
static gboolean brasero_project_type_expose_event (GtkWidget *widget, GdkEventExpose *event);

typedef enum {
	CHOSEN_SIGNAL,
	LAST_SIGNAL
} BraseroProjectTypeChooserSignalType;

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
       {N_("<big>Audio project</big>"),
	N_("Create a traditional audio CD"),
	N_("Create a traditional audio CD that will be playable on computers and stereos"),
	"media-optical-audio-new",
	BRASERO_PROJECT_TYPE_AUDIO},
       {N_("<big>Data project</big>"),
	N_("Create a data CD/DVD"),
	N_("Create a CD/DVD containing any type of data that can only be read on a computer"),
	"media-optical-data-new",
	BRASERO_PROJECT_TYPE_DATA},
       {N_("<big>Disc copy</big>"),
	N_("Create 1:1 copy of a CD/DVD"),
	N_("Create a 1:1 copy of an audio CD or a data CD/DVD on your hardisk or on another CD/DVD"),
	"media-optical-copy",
	BRASERO_PROJECT_TYPE_COPY},
       {N_("<big>Burn image</big>"),
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
	GtkWidget *selected;
	GtkWidget *hovered;

};

static gboolean
brasero_project_type_chooser_enter (GtkWidget *widget,
				    GdkEventCrossing *event,
				    BraseroProjectTypeChooser *chooser);
static gboolean
brasero_project_type_chooser_leave (GtkWidget *widget,
				    GdkEventCrossing *event,
				    BraseroProjectTypeChooser *chooser);
static gboolean
brasero_project_type_chooser_button_press (GtkWidget *widget,
					   GdkEventButton *event,
					   BraseroProjectTypeChooser *chooser);
static gboolean
brasero_project_type_chooser_button_release (GtkWidget *widget,
					     GdkEventButton *event,
					     BraseroProjectTypeChooser *chooser);

static guint brasero_project_type_chooser_signals [LAST_SIGNAL] = { 0 };
static GObjectClass *parent_class = NULL;

GType
brasero_project_type_chooser_get_type ()
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroProjectTypeChooserClass),
			NULL,
			NULL,
			(GClassInitFunc) brasero_project_type_chooser_class_init,
			NULL,
			NULL,
			sizeof (BraseroProjectTypeChooser),
			0,
			(GInstanceInitFunc) brasero_project_type_chooser_init,
		};

		type = g_type_register_static (GTK_TYPE_EVENT_BOX,
					       "BraseroProjectTypeChooser",
					       &our_info, 0);
	}

	return type;
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
}

static void
brasero_project_type_chooser_item_realize (GtkWidget *widget,
					   BraseroProjectTypeChooser *chooser)
{
	GdkCursor *cursor;

	cursor = gdk_cursor_new (GDK_HAND2);
	gdk_window_set_cursor (widget->window, cursor);
	gdk_cursor_unref (cursor);
}

static GtkWidget *
brasero_project_type_chooser_new_item (BraseroProjectTypeChooser *chooser,
				       ItemDescription *description)
{
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *image;
	GtkWidget *label;
	GtkWidget *frame;
	GtkWidget *eventbox;

	frame = gtk_frame_new (NULL);
	gtk_widget_show (frame);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	gtk_container_set_border_width (GTK_CONTAINER (frame), 24);

	eventbox = gtk_event_box_new ();
	gtk_widget_show (eventbox);
	gtk_event_box_set_visible_window (GTK_EVENT_BOX (eventbox), TRUE);
	gtk_container_add (GTK_CONTAINER (frame), eventbox);

	g_signal_connect (eventbox,
			  "button-press-event",
			  G_CALLBACK (brasero_project_type_chooser_button_press),
			  chooser);
	g_signal_connect (eventbox,
			  "button-release-event",
			  G_CALLBACK (brasero_project_type_chooser_button_release),
			  chooser);

	g_signal_connect (eventbox,
			  "enter-notify-event",
			  G_CALLBACK (brasero_project_type_chooser_enter),
			  chooser);
	g_signal_connect (eventbox,
			  "leave-notify-event",
			  G_CALLBACK (brasero_project_type_chooser_leave),
			  chooser);

	g_signal_connect (eventbox,
			  "realize",
			  G_CALLBACK (brasero_project_type_chooser_item_realize),
			  chooser);

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

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, FALSE, 0);

	image = gtk_image_new_from_icon_name (description->image, GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment (GTK_MISC (image), 1.0, 0.5);
	gtk_box_pack_start (GTK_BOX (hbox), image, TRUE, TRUE, 0);

	label = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (label), _(description->text));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
	g_object_set_data (G_OBJECT (eventbox), LABEL_KEY, label);

	label = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (label), _(description->description));
	gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);

	gtk_widget_set_state (eventbox, GTK_STATE_ACTIVE);

	return frame;
}

static void
brasero_project_type_chooser_init (BraseroProjectTypeChooser *obj)
{
	GError *error = NULL;
	GtkWidget *widget;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *box;
	int nb_rows = 2;
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

	gtk_widget_modify_bg (GTK_WIDGET (obj),
			      GTK_STATE_NORMAL,
			      &(GTK_WIDGET (obj)->style->white));

	/* get the number of rows */
	nb_items = sizeof (items) / sizeof (ItemDescription);
	rows = nb_items / nb_rows;
	if (nb_items % nb_rows)
		rows ++;

	table = gtk_table_new (rows, nb_rows, TRUE);
	gtk_container_set_border_width (GTK_CONTAINER (table), 24);
	gtk_container_add (GTK_CONTAINER (obj), table);

	box = gtk_vbox_new (FALSE, 6);
	label = gtk_label_new (_("<u><span size='xx-large' foreground='grey50'><b>Create a new project:</b></span></u>"));
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, TRUE, 0);

	label = gtk_label_new (_("<span foreground='grey60'><b><i>Choose from the following options</i></b></span>"));
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, TRUE, 0);

	gtk_table_attach (GTK_TABLE (table), 
			  box,
			  0, 2,
			  0, 1,
			  GTK_FILL,
			  0,
			  0,
			  0);
			
	for (i = 2; i < nb_items + 2; i ++) {
		widget = brasero_project_type_chooser_new_item (obj, items + i - 2);
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

GtkWidget *
brasero_project_type_chooser_new ()
{
	BraseroProjectTypeChooser *obj;

	obj = BRASERO_PROJECT_TYPE_CHOOSER (g_object_new (BRASERO_TYPE_PROJECT_TYPE_CHOOSER,
					    NULL));

	return GTK_WIDGET (obj);
}

static gboolean
brasero_project_type_chooser_enter (GtkWidget *widget,
				    GdkEventCrossing *event,
				    BraseroProjectTypeChooser *chooser)
{
	chooser->priv->hovered = widget;

	if (chooser->priv->hovered == chooser->priv->selected)
		gtk_widget_set_state (widget, GTK_STATE_SELECTED);
	else
		gtk_widget_set_state (widget, GTK_STATE_PRELIGHT);

	return FALSE;
}

static gboolean
brasero_project_type_chooser_leave (GtkWidget *widget,
				    GdkEventCrossing *event,
				    BraseroProjectTypeChooser *chooser)
{
	gtk_widget_set_state (widget, GTK_STATE_ACTIVE);
	if (chooser->priv->hovered == widget)
		chooser->priv->hovered = NULL;

	return FALSE;
}

static gboolean
brasero_project_type_chooser_button_press (GtkWidget *widget,
					   GdkEventButton *event,
					   BraseroProjectTypeChooser *chooser)
{
	gtk_widget_set_state (widget, GTK_STATE_SELECTED);
	chooser->priv->selected = widget;

	return FALSE;
}

static gboolean
brasero_project_type_chooser_button_release (GtkWidget *widget,
					     GdkEventButton *event,
					     BraseroProjectTypeChooser *chooser)
{
	BraseroProjectType type;

	if (chooser->priv->selected != chooser->priv->hovered) {
		chooser->priv->selected = NULL;
		return FALSE;
	}

	gtk_widget_set_state (widget, GTK_STATE_PRELIGHT);
	chooser->priv->selected = NULL;

	type = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), ID_KEY));
	g_signal_emit (chooser,
		       brasero_project_type_chooser_signals [CHOSEN_SIGNAL],
		       0,
		       type);
	return FALSE;
}
