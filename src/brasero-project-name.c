/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2008 <bonfire-app@wanadoo.fr>
 * 
 * brasero is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * brasero is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

#include "burn-medium.h"
#include "burn-volume-obj.h"

#include "brasero-project-name.h"
#include "brasero-project-type-chooser.h"

typedef struct _BraseroProjectNamePrivate BraseroProjectNamePrivate;
struct _BraseroProjectNamePrivate
{
	BraseroProjectType type;
	BraseroMedium *medium;

	guint label_modified:1;
};

#define BRASERO_PROJECT_NAME_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_PROJECT_NAME, BraseroProjectNamePrivate))


G_DEFINE_TYPE (BraseroProjectName, brasero_project_name, GTK_TYPE_ENTRY);

static gchar *
brasero_project_name_truncate_label (const gchar *label)
{
	const gchar *delim;
	gchar *next_char;

	/* find last possible character. We can't just do a tmp + 32 
	 * since we don't know if we are at the start of a character */
	delim = label;
	while ((next_char = g_utf8_find_next_char (delim, NULL))) {
		if (next_char - label > 32)
			break;

		delim = next_char;
	}

	return g_strndup (label, delim - label);
}

static gchar *
brasero_project_name_get_default_label (BraseroProjectName *self)
{
	time_t t;
	gchar buffer [128];
	gchar *title_str = NULL;
	BraseroProjectNamePrivate *priv;

	priv = BRASERO_PROJECT_NAME_PRIVATE (self);

	if (priv->medium) {
		title_str = brasero_volume_get_name (BRASERO_VOLUME (priv->medium));
		goto end;
	}

	t = time (NULL);
	strftime (buffer, sizeof (buffer), "%d %b %y", localtime (&t));

	if (priv->type == BRASERO_PROJECT_TYPE_DATA) {
		if (!title_str || title_str [0] == '\0') {
			/* NOTE to translators: the final string must not be over
			 * 32 _bytes_ otherwise it gets truncated. */
			title_str = g_strdup_printf (_("Data disc (%s)"), buffer);

			if (strlen (title_str) > 32) {
				g_free (title_str);
				strftime (buffer, sizeof (buffer), "%F", localtime (&t));
				title_str = g_strdup_printf ("Data disc %s", buffer);
			}
		}
	}
	else {
		if (priv->type == BRASERO_PROJECT_TYPE_VIDEO)
			/* NOTE to translators: the final string must not be over
			 * 32 _bytes_ */
			title_str = g_strdup_printf (_("Video disc (%s)"), buffer);
		else if (priv->type == BRASERO_PROJECT_TYPE_AUDIO)
			/* NOTE to translators: the final string must not be over
			 * 32 _bytes_ */
			title_str = g_strdup_printf (_("Audio disc (%s)"), buffer);

		if (strlen (title_str) > 32) {
			g_free (title_str);
			strftime (buffer, sizeof (buffer), "%F", localtime (&t));
			title_str = g_strdup_printf ("Audio disc %s", buffer);
		}
	}

end:

	if (title_str && strlen (title_str) > 32) {
		gchar *tmp;

		tmp = brasero_project_name_truncate_label (title_str);
		g_free (title_str);

		title_str = tmp;
	}

	return title_str;
}

static void
brasero_project_name_label_insert_text (GtkEditable *editable,
				        const gchar *text,
				        gint length,
				        gint *position,
				        gpointer NULL_data)
{
	BraseroProjectNamePrivate *priv;
	const gchar *label;
	gchar *new_text;
	gint new_length;
	gchar *current;
	gint max_len;
	gchar *prev;
	gchar *next;

	priv = BRASERO_PROJECT_NAME_PRIVATE (editable);	

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
					 (gpointer) brasero_project_name_label_insert_text,
					 NULL_data);
	gtk_editable_insert_text (editable, new_text, new_length, position);
	g_signal_handlers_unblock_by_func (editable,
					   (gpointer) brasero_project_name_label_insert_text,
					   NULL_data);

	g_signal_stop_emission_by_name (editable, "insert_text");
	g_free (new_text);
}

static void
brasero_project_name_label_changed (GtkEditable *editable,
				    gpointer NULL_data)
{
	BraseroProjectNamePrivate *priv;

	priv = BRASERO_PROJECT_NAME_PRIVATE (editable);
	priv->label_modified = 1;
}

void
brasero_project_name_set_type (BraseroProjectName *self,
			       BraseroProjectType type)
{
	BraseroProjectNamePrivate *priv;
	gchar *title_str = NULL;

	priv = BRASERO_PROJECT_NAME_PRIVATE (self);

	priv->type = type;

	if (priv->medium) {
		g_object_unref (priv->medium);
		priv->medium = NULL;
	}

	priv->label_modified = FALSE;

	title_str = brasero_project_name_get_default_label (self);

	g_signal_handlers_block_by_func (self, brasero_project_name_label_changed, NULL);
	gtk_entry_set_text (GTK_ENTRY (self), title_str);
	g_signal_handlers_unblock_by_func (self, brasero_project_name_label_changed, NULL);

	g_free (title_str);
}

void
brasero_project_name_set_multisession_medium (BraseroProjectName *self,
					      BraseroMedium *medium)
{
	BraseroProjectNamePrivate *priv;
	gchar *title_str;

	priv = BRASERO_PROJECT_NAME_PRIVATE (self);
	if (priv->medium) {
		g_object_unref (priv->medium);
		priv->medium = NULL;
	}

	priv->medium = medium;

	if (medium)
		g_object_ref (medium);

	if (priv->label_modified)
		return;

	title_str = brasero_project_name_get_default_label (self);

	g_signal_handlers_block_by_func (self, brasero_project_name_label_changed, NULL);
	gtk_entry_set_text (GTK_ENTRY (self), title_str);
	g_signal_handlers_unblock_by_func (self, brasero_project_name_label_changed, NULL);

	g_free (title_str);
}

static void
brasero_project_name_init (BraseroProjectName *object)
{
	BraseroProjectNamePrivate *priv;

	priv = BRASERO_PROJECT_NAME_PRIVATE (object);

	priv->label_modified = 0;
	g_signal_connect (object,
			  "insert_text",
			  G_CALLBACK (brasero_project_name_label_insert_text),
			  NULL);
	g_signal_connect (object,
			  "changed",
			  G_CALLBACK (brasero_project_name_label_changed),
			  NULL);
}

static void
brasero_project_name_finalize (GObject *object)
{
	BraseroProjectNamePrivate *priv;

	priv = BRASERO_PROJECT_NAME_PRIVATE (object);
	if (priv->medium) {
		g_object_unref (priv->medium);
		priv->medium = NULL;
	}

	G_OBJECT_CLASS (brasero_project_name_parent_class)->finalize (object);
}

static void
brasero_project_name_class_init (BraseroProjectNameClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroProjectNamePrivate));

	object_class->finalize = brasero_project_name_finalize;
}

GtkWidget *
brasero_project_name_new (void)
{
	return g_object_new (BRASERO_TYPE_PROJECT_NAME, NULL);
}

