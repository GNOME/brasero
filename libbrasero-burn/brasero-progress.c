/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-burn
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-burn is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-burn authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-burn. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-burn is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#include <math.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

#include "brasero-progress.h"
#include "burn-basics.h"
#include "brasero-medium.h"

static void brasero_burn_progress_class_init (BraseroBurnProgressClass *klass);
static void brasero_burn_progress_init (BraseroBurnProgress *sp);
static void brasero_burn_progress_finalize (GObject *object);

static void
brasero_burn_progress_set_property (GObject *object,
				    guint property_id,
				    const GValue *value,
				    GParamSpec *pspec);
static void
brasero_burn_progress_get_property (GObject *object,
				    guint property_id,
				    GValue *value,
				    GParamSpec *pspec);

struct BraseroBurnProgressPrivate {
	GtkWidget *progress;
	GtkWidget *action;
	GtkWidget *speed;
	GtkWidget *speed_label;
	GtkWidget *speed_table;
	GtkWidget *bytes_written;

	BraseroBurnAction current;
	gdouble current_progress;

	guint pulse_id;
};

static GObjectClass *parent_class = NULL;

enum {
	PROP_NONE,
	PROP_SHOW_INFO,
};

GType
brasero_burn_progress_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroBurnProgressClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_burn_progress_class_init,
			NULL,
			NULL,
			sizeof (BraseroBurnProgress),
			0,
			(GInstanceInitFunc)brasero_burn_progress_init,
		};

		type = g_type_register_static(GTK_TYPE_BOX, 
					      "BraseroBurnProgress",
					      &our_info,
					      0);
	}

	return type;
}

static void
brasero_burn_progress_class_init (BraseroBurnProgressClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_burn_progress_finalize;
	object_class->set_property = brasero_burn_progress_set_property;
	object_class->get_property = brasero_burn_progress_get_property;

	g_object_class_install_property (object_class,
					 PROP_SHOW_INFO,
					 g_param_spec_boolean ("show-info", NULL, NULL,
							       TRUE, G_PARAM_READWRITE));
}

static void
brasero_burn_progress_create_info (BraseroBurnProgress *obj)
{
	GtkWidget *label;
	GtkWidget *table;

	if (obj->priv->speed_table) {
		gtk_widget_destroy (obj->priv->speed_table);
		obj->priv->speed_table = NULL;
		obj->priv->speed_label = NULL;
		obj->priv->speed = NULL;
	}

	table = gtk_table_new (1, 2, FALSE);
	obj->priv->speed_table = table;
	gtk_container_set_border_width (GTK_CONTAINER (table), 0);

	label = gtk_label_new ("");
	obj->priv->speed_label = label;
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 1.0);
	gtk_table_attach (GTK_TABLE (table), label,
			  0,
			  1,
			  0,
			  1, 
			  GTK_EXPAND|GTK_FILL,
			  GTK_EXPAND|GTK_FILL,
			  0,
			  0);

	obj->priv->speed = gtk_label_new (" ");
	gtk_misc_set_alignment (GTK_MISC (obj->priv->speed), 1.0, 0.0);
	gtk_table_attach (GTK_TABLE (table), obj->priv->speed,
			  1,
			  2,
			  0,
			  1, 
			  GTK_FILL,
			  GTK_FILL,
			  0,
			  0);
	gtk_box_pack_start (GTK_BOX (obj), table, FALSE, TRUE, 12);
	gtk_widget_show_all (table);
}

void
brasero_burn_progress_display_session_info (BraseroBurnProgress *obj,
					    glong time,
					    gint64 rate,
					    BraseroMedia media,
					    gint64 written)
{
	GtkWidget *label;
	int hrs, mn, sec;
	gdouble speed;
	gchar *markup;
	gchar *text;

	if (obj->priv->speed_table) {
		gtk_widget_destroy (obj->priv->speed_table);
		obj->priv->speed_table = NULL;
		obj->priv->speed_label = NULL;
		obj->priv->speed = NULL;
	}

	hrs = time / 3600;
	time = ((int) time) % 3600;
	mn = time / 60;
	sec = ((int) time) % 60;

	/* Translators: first %02i is hours, the second one is minutes and the
	 * third one is seconds. */
	text = g_strdup_printf (_("Total time: %02i:%02i:%02i"), hrs, mn, sec);
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (obj->priv->progress), text);
	g_free (text);

	if (rate > 0) {
		GtkWidget *table;

		table = gtk_table_new (1, 2, FALSE);
		obj->priv->speed_table = table;
		gtk_container_set_border_width (GTK_CONTAINER (table), 0);

		label = gtk_label_new (_("Average drive speed:"));
		gtk_misc_set_alignment (GTK_MISC (label), 0.0, 1.0);
		gtk_table_attach (GTK_TABLE (table), label,
				  0,
				  1,
				  0,
				  1, 
				  GTK_EXPAND|GTK_FILL,
				  GTK_EXPAND|GTK_FILL,
				  0,
				  0);

		if (media & BRASERO_MEDIUM_DVD)
			speed = (gfloat) BRASERO_RATE_TO_SPEED_DVD (rate);
		else if (media & BRASERO_MEDIUM_BD)
			speed = (gfloat) BRASERO_RATE_TO_SPEED_BD (rate);
		else
			speed = (gfloat) BRASERO_RATE_TO_SPEED_CD (rate);

		text = g_strdup_printf ("%"G_GINT64_FORMAT" KiB/s (%.1f\303\227)", rate / 1024, speed);
		obj->priv->speed = gtk_label_new (text);
		g_free (text);

		gtk_misc_set_alignment (GTK_MISC (obj->priv->speed), 1.0, 0.0);
		gtk_table_attach (GTK_TABLE (table), obj->priv->speed,
				  1,
				  2,
				  0,
				  1, 
				  GTK_FILL,
				  GTK_FILL,
				  0,
				  0);

		gtk_box_pack_start (GTK_BOX (obj), table, FALSE, TRUE, 12);
		gtk_widget_show_all (table);
	}

	text = g_format_size (written);
	markup = g_strconcat ("<i>", text, "</i>", NULL);
	g_free (text);

	gtk_label_set_markup (GTK_LABEL (obj->priv->bytes_written), markup);
	g_free (markup);
	gtk_widget_show (obj->priv->bytes_written);
}

static void
brasero_burn_progress_set_property (GObject *object,
				    guint property_id,
				    const GValue *value,
				    GParamSpec *pspec)
{
	BraseroBurnProgress *progress;

	progress = BRASERO_BURN_PROGRESS (object);
	switch (property_id) {
	case PROP_SHOW_INFO:
		if (!g_value_get_boolean (value)) {
			if (progress->priv->speed_table) {
				gtk_widget_destroy (progress->priv->speed_table);
				progress->priv->speed_table = NULL;
				progress->priv->speed_label = NULL;
				progress->priv->speed = NULL;
			}
		}
		else if (progress->priv->speed_table)
			brasero_burn_progress_create_info (progress);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
brasero_burn_progress_get_property (GObject *object,
				    guint property_id,
				    GValue *value,
				    GParamSpec *pspec)
{
	BraseroBurnProgress *progress;

	progress = BRASERO_BURN_PROGRESS (object);
	switch (property_id) {
	case PROP_SHOW_INFO:
		g_value_set_boolean (value, (progress->priv->speed_table != NULL));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
brasero_burn_progress_init (BraseroBurnProgress *obj)
{
	GtkWidget *box;

	obj->priv = g_new0 (BraseroBurnProgressPrivate, 1);
	gtk_box_set_spacing (GTK_BOX (obj), 2);
	gtk_orientable_set_orientation (GTK_ORIENTABLE (obj), GTK_ORIENTATION_VERTICAL);

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	obj->priv->progress = gtk_progress_bar_new ();
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (obj->priv->progress), " ");
	gtk_box_pack_start (GTK_BOX (box), obj->priv->progress, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (obj), box, TRUE, TRUE, 2);

	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 24);
	gtk_box_pack_start (GTK_BOX (obj), box, FALSE, FALSE, 0);

	obj->priv->action = gtk_label_new (NULL);
	gtk_label_set_ellipsize (GTK_LABEL (obj->priv->action), PANGO_ELLIPSIZE_END);
	gtk_label_set_max_width_chars (GTK_LABEL (obj->priv->action), 32);
	gtk_misc_set_alignment (GTK_MISC (obj->priv->action), 0, 0.5);
	gtk_misc_set_padding (GTK_MISC (obj->priv->action), 0, 0);
	gtk_box_pack_start (GTK_BOX (box), obj->priv->action, TRUE, TRUE, 0);

	obj->priv->bytes_written = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (obj->priv->bytes_written), 1.0, 0.5);
	gtk_misc_set_padding (GTK_MISC (obj->priv->bytes_written), 0, 0);
	gtk_box_pack_start (GTK_BOX (box), obj->priv->bytes_written, TRUE, TRUE, 0);

	brasero_burn_progress_create_info (obj);
	
	gtk_widget_show_all (GTK_WIDGET (obj));
}

static void
brasero_burn_progress_stop_blinking (BraseroBurnProgress *self)
{
	if (self->priv->pulse_id) {
		g_source_remove (self->priv->pulse_id);
		self->priv->pulse_id = 0;
	
		gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->priv->progress),
					       self->priv->current_progress);
	}
}

static void
brasero_burn_progress_finalize (GObject *object)
{
	BraseroBurnProgress *cobj;

	cobj = BRASERO_BURN_PROGRESS (object);
	if (cobj->priv->pulse_id) {
		g_source_remove (cobj->priv->pulse_id);
		cobj->priv->pulse_id = 0;
	}

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkWidget *
brasero_burn_progress_new ()
{
	BraseroBurnProgress *obj;
	
	obj = BRASERO_BURN_PROGRESS (g_object_new (BRASERO_TYPE_BURN_PROGRESS, NULL));
	
	return GTK_WIDGET (obj);
}

static gboolean
brasero_burn_progress_pulse_cb (BraseroBurnProgress *self)
{
	gtk_progress_bar_pulse (GTK_PROGRESS_BAR (self->priv->progress));
	return TRUE;
}

static void
brasero_burn_progress_start_blinking (BraseroBurnProgress *self)
{
	self->priv->current_progress = gtk_progress_bar_get_fraction (GTK_PROGRESS_BAR (self->priv->progress));
	if (!self->priv->pulse_id)
		self->priv->pulse_id = g_timeout_add (150,
						      (GSourceFunc) brasero_burn_progress_pulse_cb,
						      self);
}

void
brasero_burn_progress_set_status (BraseroBurnProgress *self,
				  BraseroMedia media,
				  gdouble overall_progress,
				  gdouble action_progress,
				  glong remaining,
				  gint mb_isosize,
				  gint mb_written,
				  gint64 rate)
{
	gchar *text;

	if (action_progress < 0.0) {
		gtk_progress_bar_set_text (GTK_PROGRESS_BAR (self->priv->progress), " ");
		brasero_burn_progress_start_blinking (self);
		return;
	}

	if (self->priv->current == BRASERO_BURN_ACTION_NONE) {
		gtk_progress_bar_set_text (GTK_PROGRESS_BAR (self->priv->progress), " ");

		if (self->priv->bytes_written)
			gtk_label_set_text (GTK_LABEL (self->priv->bytes_written), " ");
		if (self->priv->speed)
			gtk_label_set_text (GTK_LABEL (self->priv->speed), " ");

		return;
	}

	if (self->priv->pulse_id)
		brasero_burn_progress_stop_blinking (self);

	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (self->priv->progress), 
				       action_progress);

	if (remaining >= 0) {
		int hrs, mn, sec;

		hrs = remaining / 3600;
		remaining = ((int) remaining) % 3600;
		mn = remaining / 60;
		sec = ((int) remaining) % 60;

		/* Translators: first %02i is hours, the second one is minutes
		 * and the third one is seconds. */
		text = g_strdup_printf (_("Estimated remaining time: %02i:%02i:%02i"), hrs, mn, sec);
		gtk_progress_bar_set_text (GTK_PROGRESS_BAR (self->priv->progress), text);
		g_free (text);
	}
	else if (self->priv->progress)
		gtk_progress_bar_set_text (GTK_PROGRESS_BAR (self->priv->progress), " ");

	if (self->priv->current == BRASERO_BURN_ACTION_BLANKING) {
		if (self->priv->bytes_written)
			gtk_label_set_text (GTK_LABEL (self->priv->bytes_written), " ");
		if (self->priv->speed)
			gtk_label_set_text (GTK_LABEL (self->priv->speed), " ");
		return;
	}

	if (rate > 0 && self->priv->speed) {
		gfloat speed;

		if (media & BRASERO_MEDIUM_DVD)
			speed = (gfloat) BRASERO_RATE_TO_SPEED_DVD (rate);
		else if (media & BRASERO_MEDIUM_BD)
			speed = (gfloat) BRASERO_RATE_TO_SPEED_BD (rate);
		else
			speed = (gfloat) BRASERO_RATE_TO_SPEED_CD (rate);

		text = g_strdup_printf ("%"G_GINT64_FORMAT" KiB/s (%.1f\303\227)", rate / 1024, speed);
		gtk_label_set_text (GTK_LABEL (self->priv->speed), text);
		g_free (text);
	}
	else if (self->priv->speed)
		gtk_label_set_text (GTK_LABEL (self->priv->speed), " ");

	if (mb_isosize > 0 || mb_written > 0) {
		gchar *markup;

		/* if we have just one, we can find the other */
		if (mb_isosize <= 0)
			mb_isosize = mb_written / action_progress;

		if (mb_written <= 0)
			mb_written = mb_isosize * action_progress;

		text = g_strdup_printf (_("%i MiB of %i MiB"), mb_written, mb_isosize);
		markup = g_strconcat ("<i>", text, "</i>", NULL);
		g_free (text);

		gtk_label_set_markup (GTK_LABEL (self->priv->bytes_written), markup);
		g_free (markup);
	}
	else if (self->priv->bytes_written)
		gtk_label_set_text (GTK_LABEL (self->priv->bytes_written), " ");
}

void
brasero_burn_progress_set_action (BraseroBurnProgress *self,
				  BraseroBurnAction action,
				  const gchar *string)
{
	gchar *final_text;

	if (action != BRASERO_BURN_ACTION_NONE) {
		if (!string)
			string = brasero_burn_action_to_string (action);

		if (self->priv->speed_label) {
			if (action == BRASERO_BURN_ACTION_RECORDING
			||  action == BRASERO_BURN_ACTION_DRIVE_COPY)
				gtk_label_set_text (GTK_LABEL (self->priv->speed_label),
						    _("Estimated drive speed:"));
			else
				gtk_label_set_text (GTK_LABEL (self->priv->speed_label), " ");
		}

		final_text = g_strconcat ("<i>", string, "</i>", NULL);
		gtk_label_set_markup (GTK_LABEL (self->priv->action), final_text);
		g_free (final_text);

		if (self->priv->current != action) {
			gtk_label_set_text (GTK_LABEL (self->priv->bytes_written), " ");
			gtk_progress_bar_set_text (GTK_PROGRESS_BAR (self->priv->progress), " ");
			if (self->priv->speed)
				gtk_label_set_text (GTK_LABEL (self->priv->speed), " ");
		}
	}
	else
		gtk_label_set_text (GTK_LABEL (self->priv->action), NULL);

	self->priv->current = action;

	if (action == BRASERO_BURN_ACTION_BLANKING)
		brasero_burn_progress_start_blinking (self);
	else if (action == BRASERO_BURN_ACTION_FINISHED)
		brasero_burn_progress_stop_blinking (self);
}

void
brasero_burn_progress_reset (BraseroBurnProgress *progress)
{
	brasero_burn_progress_stop_blinking (progress);

	progress->priv->current = BRASERO_BURN_ACTION_NONE;
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (progress->priv->progress), " ");

	if (progress->priv->speed_label)
		gtk_label_set_text (GTK_LABEL (progress->priv->speed_label), " ");
	if (progress->priv->speed)
		gtk_label_set_text (GTK_LABEL (progress->priv->speed), " ");

	gtk_label_set_text (GTK_LABEL (progress->priv->action), NULL);
	gtk_label_set_text (GTK_LABEL (progress->priv->bytes_written), NULL);

	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progress->priv->progress), 0.0);
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (progress->priv->progress), NULL);
}
