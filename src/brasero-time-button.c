/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2007-2008 <bonfire-app@wanadoo.fr>
 * 
 *  Brasero is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 * brasero is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with brasero.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n.h>

#include <gtk/gtk.h>

#include "brasero-time-button.h"

typedef struct _BraseroTimeButtonPrivate BraseroTimeButtonPrivate;
struct _BraseroTimeButtonPrivate
{
	GtkWidget *hrs;
	GtkWidget *min;
	GtkWidget *sec;
	GtkWidget *frame;

	gint max_hrs;
	gint max_min;
	gint max_sec;
	gint max_frame;
};

#define BRASERO_TIME_BUTTON_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_TIME_BUTTON, BraseroTimeButtonPrivate))

enum
{
	VALUE_CHANGED_SIGNAL,
	LAST_SIGNAL
};
static guint time_button_signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (BraseroTimeButton, brasero_time_button, GTK_TYPE_BOX);

static void
brasero_time_button_hrs_changed (GtkSpinButton *button,
				 BraseroTimeButton *self)
{
	BraseroTimeButtonPrivate *priv;
	gint hrs, min;

	priv = BRASERO_TIME_BUTTON_PRIVATE (self);

	hrs = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (button));
	if (hrs == priv->max_hrs) {
		min = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->min));
		if (min > priv->max_min)
			gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->min), priv->max_min);
		else
			g_signal_emit (self,
				       time_button_signals [VALUE_CHANGED_SIGNAL],
				       0);

		gtk_spin_button_set_range (GTK_SPIN_BUTTON (priv->min), 0.0, priv->max_min);
	}
	else {
		gtk_spin_button_set_range (GTK_SPIN_BUTTON (priv->min), 0.0, 60.0);
		g_signal_emit (self,
			       time_button_signals [VALUE_CHANGED_SIGNAL],
			       0);
	}
}

static void
brasero_time_button_min_changed (GtkSpinButton *button,
				 BraseroTimeButton *self)
{
	BraseroTimeButtonPrivate *priv;
	gint hrs, min, sec;

	priv = BRASERO_TIME_BUTTON_PRIVATE (self);

	hrs = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->hrs));
	min = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->min));
	if (min == priv->max_min && hrs == priv->max_hrs) {
		sec = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->sec));
		if (sec > priv->max_sec)
			gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->sec), priv->max_sec);
		else
			g_signal_emit (self,
				       time_button_signals [VALUE_CHANGED_SIGNAL],
				       0);

		gtk_spin_button_set_range (GTK_SPIN_BUTTON (priv->sec), 0.0, priv->max_sec);
	}
	else {
		gtk_spin_button_set_range (GTK_SPIN_BUTTON (priv->sec), 0.0, 60.0);
		g_signal_emit (self,
			       time_button_signals [VALUE_CHANGED_SIGNAL],
			       0);	
	}
}

static void
brasero_time_button_sec_changed (GtkSpinButton *button,
				 BraseroTimeButton *self)
{
	BraseroTimeButtonPrivate *priv;
	gint hrs, min, sec, frame;

	priv = BRASERO_TIME_BUTTON_PRIVATE (self);

	hrs = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->hrs));
	min = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->min));
	sec = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->sec));
	if (min == priv->max_min && hrs == priv->max_hrs && sec == priv->max_sec) {
		frame = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->frame));
		if (frame > priv->max_frame)
			gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->sec), priv->max_frame);
		else
			g_signal_emit (self,
				       time_button_signals [VALUE_CHANGED_SIGNAL],
				       0);

		gtk_spin_button_set_range (GTK_SPIN_BUTTON (priv->frame), 0.0, priv->max_frame);
	}
	else {
		gtk_spin_button_set_range (GTK_SPIN_BUTTON (priv->frame), 0.0, 74.0);
		g_signal_emit (self,
			       time_button_signals [VALUE_CHANGED_SIGNAL],
			       0);	
	}
}

static void
brasero_time_button_frame_changed (GtkSpinButton *button,
				   BraseroTimeButton *self)
{
	g_signal_emit (self,
		       time_button_signals [VALUE_CHANGED_SIGNAL],
		       0);
}

void
brasero_time_button_set_show_frames (BraseroTimeButton *self,
				     gboolean show)
{
	BraseroTimeButtonPrivate *priv;

	priv = BRASERO_TIME_BUTTON_PRIVATE (self);

	if (show)
		gtk_widget_show (priv->frame);
	else
		gtk_widget_hide (priv->frame);
}

void
brasero_time_button_set_max (BraseroTimeButton *self,
			     gint64 max)
{
	BraseroTimeButtonPrivate *priv;
	gint64 frames;

	priv = BRASERO_TIME_BUTTON_PRIVATE (self);

	if (max >= 1000000000)
		frames = (max % 1000000000) * 75;
	else
		frames = max * 75;

	priv->max_frame = frames / 1000000000 + ((frames % 1000000000) ? 1:0);

	max /= 1000000000;
	priv->max_hrs = max / 3600;
	max %= 3600;
	priv->max_min = max / 60;
	max %= 60;
	priv->max_sec = max;

	gtk_spin_button_set_range (GTK_SPIN_BUTTON (priv->hrs), 0.0, priv->max_hrs);
	if (priv->max_hrs) {
		gtk_widget_set_sensitive (priv->hrs, TRUE);
		gtk_widget_set_sensitive (priv->min, TRUE);
		gtk_widget_set_sensitive (priv->sec, TRUE);
		gtk_widget_set_sensitive (priv->frame, TRUE);
		gtk_spin_button_set_range (GTK_SPIN_BUTTON (priv->hrs), 0.0, priv->max_hrs);
		gtk_spin_button_set_range (GTK_SPIN_BUTTON (priv->min), 0.0, 60.0);
		gtk_spin_button_set_range (GTK_SPIN_BUTTON (priv->sec), 0.0, 60.0);
		gtk_spin_button_set_range (GTK_SPIN_BUTTON (priv->frame), 0.0, 74.0);
		return;
	}
	else
		gtk_widget_set_sensitive (priv->hrs, FALSE);

	gtk_spin_button_set_range (GTK_SPIN_BUTTON (priv->min), 0.0, priv->max_min);
	if (priv->max_min) {
		gtk_widget_set_sensitive (priv->min, TRUE);
		gtk_widget_set_sensitive (priv->sec, TRUE);
		gtk_widget_set_sensitive (priv->frame, TRUE);
		gtk_spin_button_set_range (GTK_SPIN_BUTTON (priv->min), 0.0, priv->max_min);
		gtk_spin_button_set_range (GTK_SPIN_BUTTON (priv->sec), 0.0, 60.0);
		gtk_spin_button_set_range (GTK_SPIN_BUTTON (priv->frame), 0.0, 74.0);
		return;
	}
	else
		gtk_widget_set_sensitive (priv->min, FALSE);

	if (priv->max_sec) {
		gtk_widget_set_sensitive (priv->sec, TRUE);
		gtk_widget_set_sensitive (priv->frame, TRUE);
		gtk_spin_button_set_range (GTK_SPIN_BUTTON (priv->sec), 0.0, priv->max_sec);
		gtk_spin_button_set_range (GTK_SPIN_BUTTON (priv->frame), 0.0, 74.0);
		return;
	}
	else
		gtk_widget_set_sensitive (priv->sec, FALSE);

	gtk_spin_button_set_range (GTK_SPIN_BUTTON (priv->frame), 0.0, priv->max_frame);
}

void
brasero_time_button_set_value (BraseroTimeButton *self,
			       gint64 value)
{
	BraseroTimeButtonPrivate *priv;
	gint64 frames;

	priv = BRASERO_TIME_BUTTON_PRIVATE (self);

	frames = (value % 1000000000) * 75;
	frames = frames / 1000000000 + ((frames % 1000000000) ? 1:0);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->frame), frames);

	value /= 1000000000;
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->hrs), value / 3600);
	value %= 3600;
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->min), value / 60);
	value %= 60;
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->sec), value);
}

gint64
brasero_time_button_get_value (BraseroTimeButton *self)
{
	BraseroTimeButtonPrivate *priv;
	gint64 value;

	priv = BRASERO_TIME_BUTTON_PRIVATE (self);

	value = gtk_spin_button_get_value (GTK_SPIN_BUTTON (priv->hrs)) * 3600;
	value += gtk_spin_button_get_value (GTK_SPIN_BUTTON (priv->min)) * 60;
	value += gtk_spin_button_get_value (GTK_SPIN_BUTTON (priv->sec));
	value *= 1000000000;

	value += gtk_spin_button_get_value (GTK_SPIN_BUTTON (priv->frame)) * 1000000000 / 75;

	return value;
}

static void
brasero_time_button_init (BraseroTimeButton *object)
{
	BraseroTimeButtonPrivate *priv;
	GtkWidget *label;

	priv = BRASERO_TIME_BUTTON_PRIVATE (object);

	gtk_box_set_spacing (GTK_BOX (object), 6);

	priv->hrs = gtk_spin_button_new_with_range (0, 24, 1);
	gtk_widget_set_tooltip_text (priv->hrs, _("Hours"));
	gtk_widget_show (priv->hrs);
	gtk_box_pack_start (GTK_BOX (object), priv->hrs, FALSE, FALSE, 0);

	/* Translators: separating hours and minutes */
	label = gtk_label_new (_(":"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (object), label, FALSE, FALSE, 0);

	priv->min = gtk_spin_button_new_with_range (0, 60, 1);
	gtk_widget_set_tooltip_text (priv->min, _("Minutes"));
	gtk_widget_show (priv->min);
	gtk_box_pack_start (GTK_BOX (object), priv->min, FALSE, FALSE, 0);

	/* Translators: separating minutes and seconds */
	label = gtk_label_new (_(":"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (object), label, FALSE, FALSE, 0);

	priv->sec = gtk_spin_button_new_with_range (0, 60, 1);
	gtk_widget_set_tooltip_text (priv->sec, _("Seconds"));
	gtk_widget_show (priv->sec);
	gtk_box_pack_start (GTK_BOX (object), priv->sec, FALSE, FALSE, 0);

	/* Translators: separating seconds and frames */
	label = gtk_label_new (_(":"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (object), label, FALSE, FALSE, 0);

	priv->frame = gtk_spin_button_new_with_range (0, 74, 1);
	gtk_widget_set_tooltip_text (priv->frame, _("Frames (1 second = 75 frames)"));
	gtk_widget_show (priv->frame);
	gtk_box_pack_start (GTK_BOX (object), priv->frame, FALSE, FALSE, 0);

	g_signal_connect (priv->hrs,
			  "value-changed",
			  G_CALLBACK (brasero_time_button_hrs_changed),
			  object);
	g_signal_connect (priv->min,
			  "value-changed",
			  G_CALLBACK (brasero_time_button_min_changed),
			  object);
	g_signal_connect (priv->sec,
			  "value-changed",
			  G_CALLBACK (brasero_time_button_sec_changed),
			  object);
	g_signal_connect (priv->frame,
			  "value-changed",
			  G_CALLBACK (brasero_time_button_frame_changed),
			  object);
}

static void
brasero_time_button_finalize (GObject *object)
{
	G_OBJECT_CLASS (brasero_time_button_parent_class)->finalize (object);
}

static void
brasero_time_button_class_init (BraseroTimeButtonClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroTimeButtonPrivate));

	object_class->finalize = brasero_time_button_finalize;

	time_button_signals[VALUE_CHANGED_SIGNAL] =
		g_signal_new ("value-changed",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE,
		              G_STRUCT_OFFSET (BraseroTimeButtonClass, value_changed),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0,
		              G_TYPE_NONE);
}

GtkWidget *
brasero_time_button_new (void)
{
	return g_object_new (BRASERO_TYPE_TIME_BUTTON, NULL);
}
