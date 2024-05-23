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
#include <glib/gstdio.h>
#include <glib-object.h>

#include <gtk/gtk.h>

#include <cairo/cairo.h>

#include "brasero-app.h"
#include "brasero-utils.h"
#include "brasero-session.h"

#include "brasero-project-type-chooser.h"

G_DEFINE_TYPE (BraseroProjectTypeChooser, brasero_project_type_chooser, GTK_TYPE_BOX);

typedef enum {
	LAST_SAVED_CLICKED_SIGNAL,
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
       {N_("Audi_o project"),
	N_("Create a traditional audio CD"),
	N_("Create a traditional audio CD that will be playable on computers and stereos"),
	"media-optical-audio-new",
	BRASERO_PROJECT_TYPE_AUDIO},
       {N_("D_ata project"),
	N_("Create a data CD/DVD"),
	N_("Create a CD/DVD containing any type of data that can only be read on a computer"),
	"media-optical-data-new",
	BRASERO_PROJECT_TYPE_DATA},
       {N_("_Video project"),
	N_("Create a video DVD or an SVCD"),
	N_("Create a video DVD or an SVCD that is readable on TV readers"),
	"media-optical-video-new",
	BRASERO_PROJECT_TYPE_VIDEO},
       {N_("Disc _copy"),
	N_("Create 1:1 copy of a CD/DVD"),
	N_("Create a 1:1 copy of an audio CD or a data CD/DVD on your hard disk or on another CD/DVD"),
	"media-optical-copy",
	BRASERO_PROJECT_TYPE_COPY},
       {N_("Burn _image"),
	N_("Burn an existing CD/DVD image to disc"),
	N_("Burn an existing CD/DVD image to disc"),
	"iso-image-burn",
	BRASERO_PROJECT_TYPE_ISO},
};

#define ID_KEY "ID-TYPE"
#define DESCRIPTION_KEY "DESCRIPTION_KEY"
#define LABEL_KEY "LABEL_KEY"

struct BraseroProjectTypeChooserPrivate {
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
	gchar *description_str;

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

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 4);
	gtk_container_add (GTK_CONTAINER (eventbox), vbox);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, FALSE, 0);

	image = gtk_image_new_from_icon_name (description->image, GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment (GTK_MISC (image), 1.0, 0.5);
	gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, TRUE, 0);

	label = gtk_label_new (NULL);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), eventbox);
	description_str = g_strdup_printf ("<big>%s</big>", _(description->text));
	gtk_label_set_markup_with_mnemonic (GTK_LABEL (label), description_str);
	g_free (description_str);
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

	uri = g_object_get_data (G_OBJECT (button), "BraseroButtonURI");
	g_signal_emit (self,
		       brasero_project_type_chooser_signals [RECENT_CLICKED_SIGNAL],
		       0,
		       uri);
}

static void
brasero_project_type_chooser_last_unsaved_clicked_cb (GtkButton *button,
						      BraseroProjectTypeChooser *self)
{
	const gchar *uri;
	gchar *path;

	uri = g_object_get_data (G_OBJECT (button), "BraseroButtonURI");
	path = g_filename_from_uri (uri, NULL, NULL);

	g_signal_emit (self,
		       brasero_project_type_chooser_signals [LAST_SAVED_CLICKED_SIGNAL],
		       0,
		       path);

	g_remove (path);
	g_free (path);
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
	GtkSizeGroup *image_group;
	GtkSizeGroup *group;
	GList *list = NULL;
	gchar *filename;
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
		&&  strcmp (mime, "application/vnd.efi.iso")
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
	image_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);

	/* If a project was left unfinished last time then add another entry */
	filename = g_build_filename (g_get_user_config_dir (),
				     "brasero",
				     BRASERO_SESSION_TMP_PROJECT_PATH,
				     NULL);
	if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
		gchar *uri;
		GtkWidget *link;
		GtkWidget *image;

		uri = g_filename_to_uri (filename, NULL, NULL);

		image = gtk_image_new_from_icon_name ("brasero", GTK_ICON_SIZE_BUTTON);
		gtk_size_group_add_widget (image_group, image);

		link = gtk_button_new_with_label (_("Last _Unsaved Project"));
		g_object_set_data_full (G_OBJECT (link),
					"BraseroButtonURI", uri,
					g_free);

		gtk_button_set_relief (GTK_BUTTON (link), GTK_RELIEF_NONE);
		gtk_button_set_alignment (GTK_BUTTON (link), 0.0, 0.5);
		gtk_button_set_focus_on_click (GTK_BUTTON (link), FALSE);
		gtk_button_set_image (GTK_BUTTON (link), image);
		gtk_button_set_use_underline (GTK_BUTTON (link), TRUE);
		g_signal_connect (link,
				  "clicked",
				  G_CALLBACK (brasero_project_type_chooser_last_unsaved_clicked_cb),
				  self);

		gtk_widget_show (link);
		gtk_widget_set_tooltip_text (link, _("Load the last project that was not burned and not saved"));
		gtk_box_pack_start (GTK_BOX (self->priv->recent_box), link, FALSE, TRUE, 0);

		gtk_size_group_add_widget (group, link);
	}
	g_free (filename);

	for (iter = list; iter; iter = iter->next) {
		GtkRecentInfo *info;
		GList *child_iter;
		const gchar *name;
		GIcon *icon;
		GtkWidget *image;
		const gchar *uri;
		GtkWidget *child;
		GtkWidget *link;
		GList *children;
		gchar *tooltip;

		info = iter->data;

		tooltip = gtk_recent_info_get_uri_display (info);

		icon = gtk_recent_info_get_gicon (info);
		image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_BUTTON);
		g_object_unref (icon);
		gtk_size_group_add_widget (image_group, image);

		gtk_widget_show (image);
		gtk_widget_set_tooltip_text (image, tooltip);

		name = gtk_recent_info_get_display_name (info);
		uri = gtk_recent_info_get_uri (info);

		/* Don't use mnemonics with filenames */
		link = gtk_button_new_with_label (name);
		g_object_set_data_full (G_OBJECT (link),
					"BraseroButtonURI", g_strdup (uri),
					g_free);

		gtk_button_set_relief (GTK_BUTTON (link), GTK_RELIEF_NONE);
		gtk_button_set_image (GTK_BUTTON (link), image);
		gtk_button_set_alignment (GTK_BUTTON (link), 0.0, 0.5);
		gtk_button_set_focus_on_click (GTK_BUTTON (link), FALSE);
		g_signal_connect (link,
				  "clicked",
				  G_CALLBACK (brasero_project_type_chooser_recent_clicked_cb),
				  self);
		gtk_widget_show (link);

		gtk_widget_set_tooltip_text (link, tooltip);
		gtk_box_pack_start (GTK_BOX (self->priv->recent_box), link, FALSE, TRUE, 0);

		g_free (tooltip);

		gtk_size_group_add_widget (group, link);

		/* That's a tedious hack to avoid mnemonics which are hardcoded
		 * when you add an image to a button. BUG? */
		if (!GTK_IS_BIN (link))
			continue;

		child = gtk_bin_get_child (GTK_BIN (link));
		if (!GTK_IS_ALIGNMENT (child))
			continue;

		gtk_alignment_set (GTK_ALIGNMENT (child),
				   0.0,
				   0.5,
				   1.0,
				   1.0);

		child = gtk_bin_get_child (GTK_BIN (child));
		if (!GTK_IS_BOX (child))
			continue;

		children = gtk_container_get_children (GTK_CONTAINER (child));
		for (child_iter = children; child_iter; child_iter = child_iter->next) {
			GtkWidget *widget;

			widget = child_iter->data;
			if (GTK_IS_LABEL (widget)) {
				gtk_label_set_use_underline (GTK_LABEL (widget), FALSE);
				gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);

				/* Make sure that the name is not too long */
				gtk_box_set_child_packing (GTK_BOX (child),
							   widget,
							   TRUE,
							   TRUE,
							   0,
							   GTK_PACK_START);
				gtk_label_set_ellipsize (GTK_LABEL (widget),
							 PANGO_ELLIPSIZE_END);
				break;
			}
		}
		g_list_free (children);
	}
	g_object_unref (image_group);
	g_object_unref (group);

	if (!g_list_length (list)) {
		GtkWidget *label;
		gchar *string;

		string = g_strdup_printf ("<i>%s</i>", _("No recently used project"));
		label = gtk_label_new (string);
		gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
		g_free (string);

		gtk_widget_show (label);
		gtk_box_pack_start (GTK_BOX (self->priv->recent_box), label, FALSE, FALSE, 0);
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

	/* Project box */
	project_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_show (project_box);
	gtk_box_pack_start (GTK_BOX (obj), project_box, FALSE, TRUE, 0);

	string = g_strdup_printf ("<span size='x-large'><b>%s</b></span>", _("Create a new project:"));
	label = gtk_label_new (string);
	g_free (string);

	gtk_widget_show (label);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	gtk_misc_set_padding (GTK_MISC (label), 6.0, 0.0);
	gtk_box_pack_start (GTK_BOX (project_box), label, FALSE, TRUE, 0);

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

	separator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
	gtk_widget_show (separator);
	gtk_box_pack_start (GTK_BOX (obj), separator, FALSE, TRUE, 2);

	/* The recent files part */
	recent_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (recent_box);
	gtk_box_pack_start (GTK_BOX (obj), recent_box, TRUE, TRUE, 0);

	string = g_strdup_printf ("<span size='x-large'><b>%s</b></span>", _("Recent projects:"));
	label = gtk_label_new (string);
	g_free (string);

	gtk_widget_show (label);
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.0);
	gtk_misc_set_padding (GTK_MISC (label), 6.0, 0.0);
	gtk_box_pack_start (GTK_BOX (recent_box), label, FALSE, TRUE, 0);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
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

static void
brasero_project_type_chooser_finalize (GObject *object)
{
	BraseroProjectTypeChooser *cobj;

	cobj = BRASERO_PROJECT_TYPE_CHOOSER (object);

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_project_type_chooser_class_init (BraseroProjectTypeChooserClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_project_type_chooser_finalize;

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
	brasero_project_type_chooser_signals[LAST_SAVED_CLICKED_SIGNAL] =
	    g_signal_new ("last_saved", G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_ACTION | G_SIGNAL_RUN_FIRST,
			  G_STRUCT_OFFSET (BraseroProjectTypeChooserClass, last_saved_clicked),
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
