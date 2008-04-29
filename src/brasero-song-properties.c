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

#include <gst/gst.h>

#include "brasero-song-properties.h"
#include "brasero-time-button.h"
#include "brasero-utils.h"
#include "burn-track.h"

G_DEFINE_TYPE (BraseroSongProps, brasero_song_props, GTK_TYPE_DIALOG);

struct BraseroSongPropsPrivate {
       	GtkWidget *title;
	GtkWidget *artist;
	GtkWidget *composer;
	GtkWidget *isrc;
	GtkWidget *label;
	GtkWidget *length;
	GtkWidget *start;
	GtkWidget *end;
	GtkWidget *gap;
};

static GObjectClass *parent_class = NULL;

static void
brasero_song_props_end_changed_cb (BraseroTimeButton *button,
				   BraseroSongProps *self)
{
	gchar *length_str;
	gint64 start;
	gint64 end;
	gint64 gap;

	end = brasero_time_button_get_value (BRASERO_TIME_BUTTON (self->priv->end));
	start = brasero_time_button_get_value (BRASERO_TIME_BUTTON (self->priv->start));
	gap = gtk_spin_button_get_value (GTK_SPIN_BUTTON (self->priv->gap)) * GST_SECOND;

	brasero_time_button_set_value (BRASERO_TIME_BUTTON (self->priv->start),
				       start);
	brasero_time_button_set_max (BRASERO_TIME_BUTTON (self->priv->start),
				     end - 1);

	length_str = brasero_utils_get_time_string (BRASERO_AUDIO_TRACK_LENGTH (start, end + gap), TRUE, FALSE);
	gtk_label_set_markup (GTK_LABEL (self->priv->length), length_str);
	g_free (length_str);
}

static void
brasero_song_props_start_changed_cb (BraseroTimeButton *button,
				     BraseroSongProps *self)
{
	gchar *length_str;
	gint64 start;
	gint64 end;
	gint64 gap;

	end = brasero_time_button_get_value (BRASERO_TIME_BUTTON (self->priv->end));
	start = brasero_time_button_get_value (BRASERO_TIME_BUTTON (self->priv->start));
	gap = gtk_spin_button_get_value (GTK_SPIN_BUTTON (self->priv->gap)) * GST_SECOND;

	length_str = brasero_utils_get_time_string (BRASERO_AUDIO_TRACK_LENGTH (start, end + gap), TRUE, FALSE);
	gtk_label_set_markup (GTK_LABEL (self->priv->length), length_str);
	g_free (length_str);
}

static void
brasero_song_props_gap_changed_cb (GtkSpinButton *button,
				   BraseroSongProps *self)
{
	gchar *length_str;
	gint64 start;
	gint64 end;
	gint64 gap;

	end = brasero_time_button_get_value (BRASERO_TIME_BUTTON (self->priv->end));
	start = brasero_time_button_get_value (BRASERO_TIME_BUTTON (self->priv->start));
	gap = gtk_spin_button_get_value (GTK_SPIN_BUTTON (self->priv->gap)) * GST_SECOND;

	length_str = brasero_utils_get_time_string (BRASERO_AUDIO_TRACK_LENGTH (start, end + gap), TRUE, FALSE);
	gtk_label_set_markup (GTK_LABEL (self->priv->length), length_str);
	g_free (length_str);
}

static void
brasero_song_props_init (BraseroSongProps *obj)
{
	GtkWidget *label;
	GtkWidget *table;
	GtkWidget *frame;
	GtkWidget *alignment;

	obj->priv = g_new0 (BraseroSongPropsPrivate, 1);
	gtk_dialog_set_has_separator (GTK_DIALOG (obj), FALSE);
	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (obj)->vbox), 0);
	gtk_window_set_default_size (GTK_WINDOW (obj), 400, 300);

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
	gtk_widget_set_tooltip_text (obj->priv->title,
			      _("This information will be written to the disc using CD-TEXT technology. It can be read and displayed by some audio CD players."));

	label = gtk_label_new (_("Artist:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	obj->priv->artist = gtk_entry_new ();
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach_defaults (GTK_TABLE (table), obj->priv->artist, 1, 2, 1, 2);
	gtk_widget_set_tooltip_text (obj->priv->artist,
				     _("This information will be written to the disc using CD-TEXT technology. It can be read and displayed by some audio CD players."));

	label = gtk_label_new (_("Composer:\t"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	obj->priv->composer = gtk_entry_new ();
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach_defaults (GTK_TABLE (table), obj->priv->composer, 1, 2, 2, 3);
	gtk_widget_set_tooltip_text (obj->priv->composer,
			      _("This information will be written to the disc using CD-TEXT technology. It can be read and displayed by some audio CD players."));

	label = gtk_label_new ("ISRC:");
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	obj->priv->isrc = gtk_entry_new ();
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 3, 4, GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach_defaults (GTK_TABLE (table), obj->priv->isrc, 1, 2, 3, 4);

	gtk_widget_show_all (frame);

	/* second part of the dialog */
	table = gtk_table_new (2, 4, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 6);

	frame = brasero_utils_pack_properties (_("<big><b>Options</b></big>"),
					       table,
					       NULL);
	gtk_container_set_border_width (GTK_CONTAINER (frame), 6);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (obj)->vbox), frame, FALSE, FALSE, 0);

	label = gtk_label_new (_("Song start:\t"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	obj->priv->start = brasero_time_button_new ();
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach (GTK_TABLE (table), obj->priv->start, 1, 2, 0, 1, 0, 0, 0, 0);

	label = gtk_label_new (_("Song end:\t"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	obj->priv->end = brasero_time_button_new ();
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach (GTK_TABLE (table), obj->priv->end, 1, 2, 1, 2, 0, 0, 0, 0);

	label = gtk_label_new (_("Pause length:\t"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	obj->priv->gap = gtk_spin_button_new_with_range (0.0, 100.0, 1.0);
	alignment = gtk_alignment_new (0.0, 0.5, 0.0, 0.0);
	gtk_container_add (GTK_CONTAINER (alignment), obj->priv->gap);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach (GTK_TABLE (table), alignment, 1, 2, 2, 3, GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_set_tooltip_text (obj->priv->gap,
				     _("Gives the length of the pause that should follow the track"));

	label = gtk_label_new (_("Track length:\t"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (table), label, 0, 1, 3, 4, GTK_FILL, GTK_FILL, 0, 0);
	obj->priv->length = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (obj->priv->length), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (table), obj->priv->length, 1, 2, 3, 4, GTK_FILL, GTK_FILL, 0, 0);

	gtk_widget_show_all (frame);

	/* monitor since there must be 4 sec at least for a track */
	g_signal_connect (obj->priv->end,
			  "value-changed",
			  G_CALLBACK (brasero_song_props_end_changed_cb),
			  obj);
	g_signal_connect (obj->priv->start,
			  "value-changed",
			  G_CALLBACK (brasero_song_props_start_changed_cb),
			  obj);
	g_signal_connect (obj->priv->gap,
			  "value-changed",
			  G_CALLBACK (brasero_song_props_gap_changed_cb),
			  obj);

	/* buttons */
	gtk_dialog_add_buttons (GTK_DIALOG (obj),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_APPLY, GTK_RESPONSE_ACCEPT,
				NULL);

	gtk_window_set_title (GTK_WINDOW (obj), _("Song information"));
}

void
brasero_song_props_get_properties (BraseroSongProps *self,
				   gchar **artist,
				   gchar **title,
				   gchar **composer,
				   gint *isrc,
				   gint64 *start,
				   gint64 *end,
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

	if (start)
		*start = brasero_time_button_get_value (BRASERO_TIME_BUTTON (self->priv->start));
	if (end)
		*end = brasero_time_button_get_value (BRASERO_TIME_BUTTON (self->priv->end));
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
				   gint64 length,
				   gint64 start,
				   gint64 end,
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

	brasero_time_button_set_max (BRASERO_TIME_BUTTON (self->priv->start), end - 1);
	brasero_time_button_set_value (BRASERO_TIME_BUTTON (self->priv->start), start);
	brasero_time_button_set_max (BRASERO_TIME_BUTTON (self->priv->end), length);
	brasero_time_button_set_value (BRASERO_TIME_BUTTON (self->priv->end), end);
}

static void
brasero_song_props_finalize (GObject *object)
{
	BraseroSongProps *cobj;

	cobj = BRASERO_SONG_PROPS(object);

	g_free (cobj->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_song_props_class_init (BraseroSongPropsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_song_props_finalize;
}

GtkWidget *
brasero_song_props_new ()
{
	BraseroSongProps *obj;
	
	obj = BRASERO_SONG_PROPS (g_object_new (BRASERO_TYPE_SONG_PROPS, NULL));
	
	return GTK_WIDGET (obj);
}
