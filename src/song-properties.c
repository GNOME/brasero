/***************************************************************************
 *            song-properties.c
 *
 *  lun avr 10 18:39:17 2006
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n.h>

#include <gtk/gtkdialog.h>
#include <gtk/gtkbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkstock.h>
#include <gtk/gtktable.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkspinbutton.h>
#include <gtk/gtktooltips.h>

#include <gst/gst.h>

#include "song-properties.h"
#include "utils.h"

static void brasero_song_props_class_init (BraseroSongPropsClass *klass);
static void brasero_song_props_init (BraseroSongProps *sp);
static void brasero_song_props_finalize (GObject *object);

struct BraseroSongPropsPrivate {
	GtkTooltips *tooltips;

	GtkWidget *title;
	GtkWidget *artist;
	GtkWidget *composer;
	GtkWidget *isrc;
	GtkWidget *label;
	GtkWidget *gap;
};

static GObjectClass *parent_class = NULL;

GType
brasero_song_props_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroSongPropsClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_song_props_class_init,
			NULL,
			NULL,
			sizeof (BraseroSongProps),
			0,
			(GInstanceInitFunc)brasero_song_props_init,
		};

		type = g_type_register_static (GTK_TYPE_DIALOG, 
					       "BraseroSongProps",
					       &our_info,
					       0);
	}

	return type;
}

static void
brasero_song_props_class_init (BraseroSongPropsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_song_props_finalize;
}

static void
brasero_song_props_init (BraseroSongProps *obj)
{
	GtkWidget *label;
	GtkWidget *table;
	GtkWidget *frame;

	obj->priv = g_new0 (BraseroSongPropsPrivate, 1);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (obj)->vbox), 0);
	gtk_window_set_default_size (GTK_WINDOW (obj), 400, 300);

	obj->priv->tooltips = gtk_tooltips_new ();
	g_object_ref (obj->priv->tooltips);
	g_object_ref_sink (GTK_OBJECT (obj->priv->tooltips));

	table = gtk_table_new (4, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 4);
	gtk_table_set_col_spacings (GTK_TABLE (table), 6);

	frame = brasero_utils_pack_properties ("", table, NULL);
	gtk_container_set_border_width (GTK_CONTAINER (frame), 6);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (obj)->vbox),
			    frame,
			    FALSE,
			    FALSE,
			    0);

	obj->priv->label = gtk_frame_get_label_widget (GTK_FRAME (frame));
	gtk_label_set_single_line_mode (GTK_LABEL (obj->priv->label), FALSE);
	gtk_label_set_use_markup (GTK_LABEL (obj->priv->label), TRUE);
	gtk_label_set_line_wrap (GTK_LABEL (obj->priv->label), TRUE);

	label = gtk_label_new (_("Title:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	obj->priv->title = gtk_entry_new ();
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach_defaults (GTK_TABLE (table), obj->priv->title, 1, 2, 0, 1);
	gtk_tooltips_set_tip (obj->priv->tooltips,
			      obj->priv->title,
			      _("This information will be written to the disc using CD-TEXT technology. It can be read and displayed by some audio CD players."),
			      NULL);

	label = gtk_label_new (_("Artist:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	obj->priv->artist = gtk_entry_new ();
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach_defaults (GTK_TABLE (table), obj->priv->artist, 1, 2, 1, 2);
	gtk_tooltips_set_tip (obj->priv->tooltips,
			      obj->priv->artist,
			      _("This information will be written to the disc using CD-TEXT technology. It can be read and displayed by some audio CD players."),
			      NULL);

	label = gtk_label_new (_("Composer:\t"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	obj->priv->composer = gtk_entry_new ();
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach_defaults (GTK_TABLE (table), obj->priv->composer, 1, 2, 2, 3);
	gtk_tooltips_set_tip (obj->priv->tooltips,
			      obj->priv->composer,
			      _("This information will be written to the disc using CD-TEXT technology. It can be read and displayed by some audio CD players."),
			      NULL);

	label = gtk_label_new ("ISRC:");
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	obj->priv->isrc = gtk_entry_new ();
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 3, 4, GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach_defaults (GTK_TABLE (table), obj->priv->isrc, 1, 2, 3, 4);

	/* second part of the dialog */
	table = gtk_table_new (2, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 6);

	frame = brasero_utils_pack_properties (_("<big><b>Options</b></big>"),
					       table,
					       NULL);
	gtk_container_set_border_width (GTK_CONTAINER (frame), 6);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (obj)->vbox), frame, FALSE, FALSE, 0);

	label = gtk_label_new (_("Pause length:\t"));
	obj->priv->gap = gtk_spin_button_new_with_range (0.0, 100.0, 1.0);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach (GTK_TABLE (table), obj->priv->gap, 1, 2, 0, 1, 0, 0, 0, 0);
	gtk_tooltips_set_tip (obj->priv->tooltips,
			      obj->priv->gap,
			      _("Gives the length of the pause that should follow the track"),
			      NULL);

	/* buttons */
	gtk_dialog_add_buttons (GTK_DIALOG (obj),
				GTK_STOCK_APPLY, GTK_RESPONSE_ACCEPT,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				NULL);

	gtk_window_set_title (GTK_WINDOW (obj), _("Song information"));
}

static void
brasero_song_props_finalize (GObject *object)
{
	BraseroSongProps *cobj;

	cobj = BRASERO_SONG_PROPS(object);

	g_object_unref (cobj->priv->tooltips);
	g_free (cobj->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkWidget *
brasero_song_props_new ()
{
	BraseroSongProps *obj;
	
	obj = BRASERO_SONG_PROPS (g_object_new (BRASERO_TYPE_SONG_PROPS, NULL));
	
	return GTK_WIDGET (obj);
}

void
brasero_song_props_get_properties (BraseroSongProps *self,
				   gchar **artist,
				   gchar **title,
				   gchar **composer,
				   gint *isrc,
				   gint64 *gap)
{
	if (artist)
		*artist = g_strdup (gtk_entry_get_text (GTK_ENTRY (self->priv->artist)));
	if (title)
		*title = g_strdup (gtk_entry_get_text (GTK_ENTRY (self->priv->title)));
	if (composer)
		*composer = g_strdup (gtk_entry_get_text (GTK_ENTRY (self->priv->composer)));
	if (isrc) {
		const gchar *string;

		string = gtk_entry_get_text (GTK_ENTRY (self->priv->isrc));
		*isrc = (gint) g_strtod (string, NULL);
	}

	if (gap)
		*gap = gtk_spin_button_get_value (GTK_SPIN_BUTTON (self->priv->gap)) * GST_SECOND;
}

void
brasero_song_props_set_properties (BraseroSongProps *self,
				   gint track_num,
				   const gchar *artist,
				   const gchar *title,
				   const gchar *composer,
				   gint isrc,
				   gint64 gap)
{
	gchar *string;
	gdouble secs;

	string = g_strdup_printf (_("<b><big>Song information for track %02i</big></b>"), track_num);
	gtk_label_set_markup (GTK_LABEL (self->priv->label), string);
	g_free (string);

	if (artist)
		gtk_entry_set_text (GTK_ENTRY (self->priv->artist), artist);
	if (title)
		gtk_entry_set_text (GTK_ENTRY (self->priv->title), title);
	if (composer)
		gtk_entry_set_text (GTK_ENTRY (self->priv->composer), composer);
	if (isrc) {
		string = g_strdup_printf ("%i", isrc);
		gtk_entry_set_text (GTK_ENTRY (self->priv->isrc), string);
		g_free (string);
	}

	secs = gap / GST_SECOND;
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (self->priv->gap), secs);
}
