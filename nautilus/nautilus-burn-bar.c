/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2008 Philippe Rouquier <bonfire-app@wanadoo.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Authors: William Jon McCann <mccann@jhu.edu>
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "nautilus-burn-bar.h"

static void nautilus_disc_burn_bar_finalize   (GObject *object);

#define NAUTILUS_DISC_BURN_BAR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NAUTILUS_TYPE_DISC_BURN_BAR, NautilusDiscBurnBarPrivate))

struct NautilusDiscBurnBarPrivate
{
        GtkWidget  *button;
        GtkWidget  *title;
        gchar      *icon_path;
};

enum {
        TITLE_CHANGED,
        ICON_CHANGED,
        ACTIVATE,
        LAST_SIGNAL
};

static guint           signals [LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (NautilusDiscBurnBar, nautilus_disc_burn_bar, GTK_TYPE_BOX)

const gchar *
nautilus_disc_burn_bar_get_icon (NautilusDiscBurnBar *bar)
{
        g_return_val_if_fail (bar != NULL, NULL);
        return bar->priv->icon_path;
}

void
nautilus_disc_burn_bar_set_icon (NautilusDiscBurnBar *bar,
                                 const gchar *icon_path)
{
        g_return_if_fail (bar != NULL);

        if (bar->priv->icon_path)
                g_free (bar->priv->icon_path);

        bar->priv->icon_path = g_strdup (icon_path);

        if (bar->priv->icon_path) {
                GIcon *icon;
                GFile *file;

                file = g_file_new_for_path (bar->priv->icon_path);
		icon = g_file_icon_new (file);
                g_object_unref (file);
                gtk_entry_set_icon_from_gicon (GTK_ENTRY (bar->priv->title),
                                               GTK_ENTRY_ICON_PRIMARY,
                                               icon);
                g_object_unref (icon);
        }
	else
                gtk_entry_set_icon_from_icon_name (GTK_ENTRY (bar->priv->title),
						   GTK_ENTRY_ICON_PRIMARY,
						   "media-optical");
}

const gchar *
nautilus_disc_burn_bar_get_title (NautilusDiscBurnBar *bar)
{
        g_return_val_if_fail (bar != NULL, NULL);
        return gtk_entry_get_text (GTK_ENTRY (bar->priv->title));
}

void
nautilus_disc_burn_bar_set_title (NautilusDiscBurnBar *bar,
                                  const gchar *title)
{
        g_return_if_fail (bar != NULL);

        if (!title) {
                time_t  t;
                gchar  *title_str;
                gchar   buffer [128];

                t = time (NULL);
                strftime (buffer, sizeof (buffer), "%d %b %y", localtime (&t));

	        /* NOTE to translators: the final string must not be over
		 * 32 _bytes_ otherwise it gets truncated.
		 * The %s is the date */
		title_str = g_strdup_printf (_("Data disc (%s)"), buffer);

		if (strlen (title_str) > 32) {
			g_free (title_str);
			strftime (buffer, sizeof (buffer), "%F", localtime (&t));
			title_str = g_strdup_printf ("Data disc %s", buffer);
		}

                gtk_entry_set_text (GTK_ENTRY (bar->priv->title), title_str);
        }
        else
                gtk_entry_set_text (GTK_ENTRY (bar->priv->title), title);
}

GtkWidget *
nautilus_disc_burn_bar_get_button (NautilusDiscBurnBar *bar)
{
        GtkWidget *button;

        g_return_val_if_fail (bar != NULL, NULL);

        button = bar->priv->button;

        return button;
}

static void
nautilus_disc_burn_bar_set_property (GObject            *object,
                                guint               prop_id,
                                const GValue       *value,
                                GParamSpec         *pspec)
{
        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
nautilus_disc_burn_bar_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
        switch (prop_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
nautilus_disc_burn_bar_class_init (NautilusDiscBurnBarClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize     = nautilus_disc_burn_bar_finalize;
        object_class->get_property = nautilus_disc_burn_bar_get_property;
        object_class->set_property = nautilus_disc_burn_bar_set_property;

        g_type_class_add_private (klass, sizeof (NautilusDiscBurnBarPrivate));

        signals [TITLE_CHANGED] = g_signal_new ("title_changed",
                                                G_TYPE_FROM_CLASS (klass),
                                                G_SIGNAL_RUN_LAST,
                                                G_STRUCT_OFFSET (NautilusDiscBurnBarClass, title_changed),
                                                NULL, NULL,
                                                g_cclosure_marshal_VOID__VOID,
                                                G_TYPE_NONE, 0);
        signals [ICON_CHANGED] = g_signal_new ("icon_changed",
                                               G_TYPE_FROM_CLASS (klass),
                                               G_SIGNAL_RUN_LAST,
                                               G_STRUCT_OFFSET (NautilusDiscBurnBarClass, icon_changed),
                                               NULL, NULL,
                                               g_cclosure_marshal_VOID__VOID,
                                               G_TYPE_NONE, 0);
        signals [ACTIVATE] = g_signal_new ("activate",
                                           G_TYPE_FROM_CLASS (klass),
                                           G_SIGNAL_RUN_LAST,
                                           G_STRUCT_OFFSET (NautilusDiscBurnBarClass, activate),
                                           NULL, NULL,
                                           g_cclosure_marshal_VOID__VOID,
                                           G_TYPE_NONE, 0);
}

static void
button_clicked_cb (GtkWidget       *button,
                   NautilusDiscBurnBar *bar)
{
        g_signal_emit (bar, signals [ACTIVATE], 0);
}

static void
nautilus_disc_burn_bar_title_changed (GtkEditable *editable,
                                      NautilusDiscBurnBar *bar)
{
	g_signal_emit (bar,
		       signals [TITLE_CHANGED],
		       0);
}

static void
nautilus_disc_burn_bar_icon_button_clicked (GtkEntry *entry,
                                            GtkEntryIconPosition position,
                                            GdkEvent *event,
                                            NautilusDiscBurnBar *bar)
{
        GtkFileFilter *filter;
	GtkWidget *chooser;
	gchar *path;
	gint res;

	chooser = gtk_file_chooser_dialog_new (_("Medium Icon"),
					       GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (bar))),
					       GTK_FILE_CHOOSER_ACTION_OPEN,
					       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					       GTK_STOCK_OK, GTK_RESPONSE_OK,
					       NULL);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("All files"));
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

	filter = gtk_file_filter_new ();
	/* Translators: this is an image, a picture, not a "Disc Image" */
	gtk_file_filter_set_name (filter, C_("picture", "Image files"));
	gtk_file_filter_add_mime_type (filter, "image/*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (chooser), filter);

        if (bar->priv->icon_path)
		gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (chooser), bar->priv->icon_path);

	gtk_widget_show (chooser);
	res = gtk_dialog_run (GTK_DIALOG (chooser));
	if (res != GTK_RESPONSE_OK) {
		gtk_widget_destroy (chooser);
		return;
	}

	path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
	gtk_widget_destroy (chooser);

        nautilus_disc_burn_bar_set_icon (bar, path);
        g_free (path);

        g_signal_emit (bar,
                       signals [ICON_CHANGED],
                       0);
}

static void
nautilus_disc_burn_bar_title_insert_text (GtkEditable *editable,
                                          const gchar *text,
                                          gint length,
                                          gint *position,
                                          NautilusDiscBurnBar *bar)
{
	const gchar *label;
	gchar *new_text;
	gint new_length;
	gchar *current;
	gint max_len;
	gchar *prev;
	gchar *next;

	/* check if this new text will fit in 32 _bytes_ long buffer */
	label = gtk_entry_get_text (GTK_ENTRY (editable));
	max_len = 32 - strlen (label) - length;
	if (max_len >= 0)
		return;

	gdk_beep ();

	/* get the last character '\0' of the text to be inserted */
	new_length = length;
	new_text = g_strdup (text);
	current = g_utf8_offset_to_pointer (new_text, g_utf8_strlen (new_text, -1));

	/* don't just remove one character in case there was many more
	 * that were inserted at the same time through DND, paste, ... */
	prev = g_utf8_find_prev_char (new_text, current);
	if (!prev) {
		/* no more characters so no insertion */
		g_signal_stop_emission_by_name (editable, "insert_text"); 
		g_free (new_text);
		return;
	}

	do {
		next = current;
		current = prev;

		prev = g_utf8_find_prev_char (new_text, current);
		if (!prev) {
			/* no more characters so no insertion */
			g_signal_stop_emission_by_name (editable, "insert_text"); 
			g_free (new_text);
			return;
		}

		new_length -= next - current;
		max_len += next - current;
	} while (max_len < 0 && new_length > 0);

	*current = '\0';
	g_signal_handlers_block_by_func (editable,
					 (gpointer) nautilus_disc_burn_bar_title_insert_text,
					 bar);
	gtk_editable_insert_text (editable, new_text, new_length, position);
	g_signal_handlers_unblock_by_func (editable,
					   (gpointer) nautilus_disc_burn_bar_title_insert_text,
					   bar);

	g_signal_stop_emission_by_name (editable, "insert_text");
	g_free (new_text);
}

static void
nautilus_disc_burn_bar_init (NautilusDiscBurnBar *bar)
{
        GtkWidget   *table;
        GtkWidget   *label;
        GtkWidget   *hbox;
        GtkWidget   *image;
        GtkWidget   *entry;
        gchar       *string;

        bar->priv = NAUTILUS_DISC_BURN_BAR_GET_PRIVATE (bar);

        hbox = GTK_WIDGET (bar);
        table = gtk_table_new (3, 2, FALSE);       

        gtk_table_set_col_spacings (GTK_TABLE (table), 6);
        gtk_table_set_row_spacings (GTK_TABLE (table), 6);
        gtk_widget_show (table);
        gtk_box_pack_start (GTK_BOX (hbox), table, TRUE, TRUE, 0);

        label = gtk_label_new (_("CD/DVD Creator Folder"));
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        gtk_widget_show (label);
        gtk_table_attach (GTK_TABLE (table),
                          label,
                          0, 2,
                          0, 1,
                          GTK_FILL,
                          GTK_FILL,
                          0,
                          0);

        label = gtk_label_new (_("Disc Name:"));
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        gtk_widget_show (label);
        gtk_table_attach (GTK_TABLE (table),
                          label,
                          0, 1,
                          1, 2,
                          GTK_FILL,
                          GTK_FILL,
                          0,
                          0);

        entry = gtk_entry_new ();
        bar->priv->title = entry;
        gtk_widget_show (entry);
        gtk_table_attach (GTK_TABLE (table),
                          entry,
                          1, 2,
                          1, 2,
                          GTK_FILL|GTK_EXPAND,
                          GTK_FILL|GTK_EXPAND,
                          0,
                          0);

        g_signal_connect (entry,
			  "icon-release",
			  G_CALLBACK (nautilus_disc_burn_bar_icon_button_clicked),
			  bar);
	g_signal_connect (entry,
			  "insert_text",
			  G_CALLBACK (nautilus_disc_burn_bar_title_insert_text),
			  bar);
	g_signal_connect (entry,
			  "changed",
			  G_CALLBACK (nautilus_disc_burn_bar_title_changed),
			  bar);

        /* Translators: be careful, anything longer than the English will likely
         * not fit on small Nautilus windows */
        string = g_strdup_printf ("<i>%s</i>", _("Drag or copy files below to write them to disc"));
        label = gtk_label_new (string);
        g_free (string);
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
        gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
        gtk_widget_show (label);
        gtk_table_attach (GTK_TABLE (table),
                          label,
                          0, 2,
                          2, 3,
                          GTK_FILL|GTK_EXPAND,
                          GTK_FILL|GTK_EXPAND,
                          0,
                          0);

        bar->priv->button = gtk_button_new_with_label (_("Write to Disc"));
        gtk_widget_show (bar->priv->button);
        gtk_table_attach (GTK_TABLE (table),
                          bar->priv->button,
                          2, 3,
                          1, 2,
                          GTK_FILL,
                          GTK_FILL,
                          0,
                          0);

        image = gtk_image_new_from_icon_name ("media-optical-burn", GTK_ICON_SIZE_BUTTON);
        gtk_widget_show (image);
        gtk_button_set_image (GTK_BUTTON (bar->priv->button), image);

        g_signal_connect (bar->priv->button, "clicked",
                          G_CALLBACK (button_clicked_cb),
                          bar);

        gtk_widget_set_tooltip_text (bar->priv->button, _("Write contents to a CD or DVD"));
}

static void
nautilus_disc_burn_bar_finalize (GObject *object)
{
        NautilusDiscBurnBar *bar;

        g_return_if_fail (object != NULL);
        g_return_if_fail (NAUTILUS_IS_DISC_BURN_BAR (object));

        bar = NAUTILUS_DISC_BURN_BAR (object);

        g_return_if_fail (bar->priv != NULL);

        if (bar->priv->icon_path) {
                g_free (bar->priv->icon_path);
                bar->priv->icon_path = NULL;
        }

        G_OBJECT_CLASS (nautilus_disc_burn_bar_parent_class)->finalize (object);
}

GtkWidget *
nautilus_disc_burn_bar_new (void)
{
        GObject *result;

        result = g_object_new (NAUTILUS_TYPE_DISC_BURN_BAR,
                               "spacing", 6,
                               NULL);

        return GTK_WIDGET (result);
}
