/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Brasero
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
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

#include <string.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

#include "brasero-misc.h"

#include "brasero-medium.h"
#include "brasero-volume.h"

#include "brasero-tags.h"
#include "brasero-session.h"
#include "brasero-track-data-cfg.h"

#include "brasero-project-name.h"
#include "brasero-project-type-chooser.h"

typedef struct _BraseroProjectNamePrivate BraseroProjectNamePrivate;
struct _BraseroProjectNamePrivate
{
	BraseroBurnSession *session;

	BraseroProjectType type;

	guint label_modified:1;
};

#define BRASERO_PROJECT_NAME_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_PROJECT_NAME, BraseroProjectNamePrivate))

typedef enum {
	CHANGED_SIGNAL,
	LAST_SIGNAL
} BraseroDiscSignalType;

static guint brasero_project_name_signals [LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (BraseroProjectName, brasero_project_name, GTK_TYPE_ENTRY);

enum {
	PROP_0,
	PROP_SESSION
};

static void
brasero_project_name_data_icon_error (BraseroProjectName *project,
				      GError *error)
{
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (project));
	brasero_utils_message_dialog (toplevel,
				      /* Translators: this is a picture not
				       * a disc image */
				      C_("picture", "Please select another image."),
				      error? error->message:_("Unknown error"),
				      GTK_MESSAGE_ERROR);
}

static void
brasero_project_name_icon_update (BraseroProjectName *self,
				  BraseroTrackDataCfg *track)
{
	GIcon *icon; 

	icon = brasero_track_data_cfg_get_icon (track);
	if (!icon) {
		gtk_entry_set_icon_from_icon_name (GTK_ENTRY (self),
						   GTK_ENTRY_ICON_PRIMARY,
						   "media-optical");
		return;
	}

	gtk_entry_set_icon_from_gicon (GTK_ENTRY (self),
	                               GTK_ENTRY_ICON_PRIMARY,
	                               icon);

	g_object_unref (icon);
}

static void
brasero_project_name_icon_changed_cb (BraseroTrackDataCfg *track,
				      BraseroProjectName *self)
{
	brasero_project_name_icon_update (self, track);
}

static BraseroTrackDataCfg *
brasero_project_name_get_track_data_cfg (BraseroProjectName *self)
{
	BraseroProjectNamePrivate *priv;
	GSList *tracks;

	priv = BRASERO_PROJECT_NAME_PRIVATE (self);

	tracks = brasero_burn_session_get_tracks (priv->session);
	for (; tracks; tracks = tracks->next) {
		BraseroTrackDataCfg *track;

		track = tracks->data;
		if (BRASERO_IS_TRACK_DATA_CFG (track))
			return BRASERO_TRACK_DATA_CFG (track);
	}

	return NULL;
}

static void
brasero_project_name_icon_button_clicked (BraseroProjectName *project,
					  GtkEntryIconPosition position,
					  GdkEvent *event,
					  gpointer NULL_data)
{
	BraseroTrackDataCfg *track;
	GtkFileFilter *filter;
	gchar *filename;
	GError *error = NULL;
	GtkWidget *chooser;
	gchar *path;
	gint res;

	track = brasero_project_name_get_track_data_cfg (project);
	if (!track)
		return;

	chooser = gtk_file_chooser_dialog_new (_("Medium Icon"),
					       GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (project))),
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

	filename = brasero_track_data_cfg_get_icon_path (track);
	if (filename) {
		gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (chooser), filename);
		g_free (filename);
	}

	gtk_widget_show (chooser);
	res = gtk_dialog_run (GTK_DIALOG (chooser));
	if (res != GTK_RESPONSE_OK) {
		gtk_widget_destroy (chooser);
		return;
	}

	path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
	gtk_widget_destroy (chooser);

	/* Get the BraseroTrackDataCfg if any and set the icon */
	if (!brasero_track_data_cfg_set_icon (track, path, &error)) {
		if (error) {
			brasero_project_name_data_icon_error (project, error);
			g_error_free (error);
		}
	}
	g_free (path);
}

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
	BraseroBurnFlag flags;
	gchar *title_str = NULL;
	BraseroProjectNamePrivate *priv;

	priv = BRASERO_PROJECT_NAME_PRIVATE (self);

	if (priv->type == BRASERO_PROJECT_TYPE_INVALID)
		return g_strdup ("");

	flags = brasero_burn_session_get_flags (priv->session);
	if (flags & BRASERO_BURN_FLAG_MERGE) {
		BraseroMedium *medium;
		BraseroDrive *burner;

		burner = brasero_burn_session_get_burner (priv->session);
		medium = brasero_drive_get_medium (burner);

		if (medium) {
			title_str = brasero_volume_get_name (BRASERO_VOLUME (medium));
			goto end;
		}
	}

	t = time (NULL);
	strftime (buffer, sizeof (buffer), "%d %b %y", localtime (&t));

	if (priv->type == BRASERO_PROJECT_TYPE_DATA) {
		if (!title_str || title_str [0] == '\0') {
			/* NOTE to translators: the final string must not be over
			 * 32 _bytes_ otherwise it gets truncated.
			 * The %s is the date */
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
			 * 32 _bytes_.
			 * The %s is the date */
			title_str = g_strdup_printf (_("Video disc (%s)"), buffer);
		else if (priv->type == BRASERO_PROJECT_TYPE_AUDIO) {
			GSList *tracks;
			const gchar *album = NULL;
			const gchar *artist = NULL;
			gboolean default_album_name = TRUE;

			/* Go through all audio tracks and see if they have the 
			 * same album and artist name. If so set the album name */
			tracks = brasero_burn_session_get_tracks (priv->session);
			for (; tracks; tracks = tracks->next) {
				BraseroTrack *track;
				const gchar *tmp_album;
				const gchar *tmp_artist;

				track = tracks->data;

				tmp_album = brasero_track_tag_lookup_string (track, BRASERO_TRACK_STREAM_ALBUM_TAG);

				if (!tmp_album) {
					default_album_name = FALSE;
					break;
				}

				if (album) {
					if (strcmp (tmp_album, album)) {
						default_album_name = FALSE;
						break;
					}
				}
				else
					album = tmp_album;

				tmp_artist = brasero_track_tag_lookup_string (track, BRASERO_TRACK_STREAM_ARTIST_TAG);
				if (!tmp_artist) {
					default_album_name = FALSE;
					break;
				}

				if (artist) {
					if (strcmp (tmp_artist, artist)) {
						default_album_name = FALSE;
						break;
					}
				}
				else
					artist = tmp_artist;
			}

			if (!artist || !album || !default_album_name) {
				/* NOTE to translators: the final string must not be over
				 * 32 _bytes_ .
				 * The %s is the date */
				title_str = g_strdup_printf (_("Audio disc (%s)"), buffer);
			}
			else
				title_str = g_strdup (album);
		}

		if (strlen (title_str) > 32) {
			g_free (title_str);
			strftime (buffer, sizeof (buffer), "%F", localtime (&t));
			if (priv->type == BRASERO_PROJECT_TYPE_VIDEO)
				title_str = g_strdup_printf ("Video disc %s", buffer);
			else
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
	priv->label_modified = TRUE;
	g_signal_emit (editable,
		       brasero_project_name_signals [CHANGED_SIGNAL],
		       0);
}

static void
brasero_project_name_set_type (BraseroProjectName *self)
{
	BraseroProjectNamePrivate *priv;
	BraseroTrackType *track_type;
	BraseroProjectType type;
	gchar *title_str = NULL;
	BraseroStatus *status;

	priv = BRASERO_PROJECT_NAME_PRIVATE (self);

	status = brasero_status_new ();
	brasero_burn_session_get_status (priv->session, status);
	if (brasero_status_get_result (status) != BRASERO_BURN_OK) {
		g_object_unref (status);
		return;
	}
	g_object_unref (status);

	track_type = brasero_track_type_new ();
	brasero_burn_session_get_input_type (priv->session, track_type);

	if (brasero_track_type_get_has_data (track_type))
		type = BRASERO_PROJECT_TYPE_DATA;
	else if (brasero_track_type_get_has_stream (track_type)) {
		if (BRASERO_STREAM_FORMAT_HAS_VIDEO (brasero_track_type_get_stream_format (track_type)))
			type = BRASERO_PROJECT_TYPE_VIDEO;
		else
			type = BRASERO_PROJECT_TYPE_AUDIO;
	}
	else
		type = BRASERO_PROJECT_TYPE_INVALID;

	brasero_track_type_free (track_type);

	/* This is not necessarily true for audio projects as those can have the
	 * name of their album set as default; so it could easily change */
	if (type != BRASERO_PROJECT_TYPE_AUDIO) {
		if (priv->type == type)
			return;
	}

	priv->type = type;
	if (brasero_burn_session_get_label (priv->session)) {
		priv->label_modified = TRUE;
		g_signal_handlers_block_by_func (self, brasero_project_name_label_changed, NULL);
		gtk_entry_set_text (GTK_ENTRY (self), brasero_burn_session_get_label (priv->session));
		g_signal_handlers_unblock_by_func (self, brasero_project_name_label_changed, NULL);
		return;
	}

	if (priv->label_modified) {
		return;
	}

	title_str = brasero_project_name_get_default_label (self);

	g_signal_handlers_block_by_func (self, brasero_project_name_label_changed, NULL);
	gtk_entry_set_text (GTK_ENTRY (self), title_str);
	g_signal_handlers_unblock_by_func (self, brasero_project_name_label_changed, NULL);

	g_free (title_str);
}

static void
brasero_project_name_flags_changed (BraseroBurnSession *session,
                                    GParamSpec *pspec,
				    BraseroProjectName *self)
{
	BraseroProjectNamePrivate *priv;
	gchar *title_str;

	priv = BRASERO_PROJECT_NAME_PRIVATE (self);

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

	priv->label_modified = FALSE;
	g_signal_connect (object,
			  "icon-release",
			  G_CALLBACK (brasero_project_name_icon_button_clicked),
			  NULL);

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
brasero_project_name_session_changed (BraseroProjectName *self)
{
	BraseroTrackType *type;
	BraseroProjectNamePrivate *priv;

	priv = BRASERO_PROJECT_NAME_PRIVATE (self);

	type = brasero_track_type_new ();
	brasero_burn_session_get_input_type (priv->session, type);
	if (brasero_track_type_get_has_data (type)) {
		BraseroTrackDataCfg *track;

		track = brasero_project_name_get_track_data_cfg (self);
		if (track) {
			g_signal_connect (track,
					  "icon-changed",
					  G_CALLBACK (brasero_project_name_icon_changed_cb),
					  self);
			brasero_project_name_icon_update (self, track);
		}
	}
	else
		gtk_entry_set_icon_from_gicon (GTK_ENTRY (self),
		                               GTK_ENTRY_ICON_PRIMARY,
		                               NULL);

	brasero_track_type_free (type);

	brasero_project_name_set_type (self);
}

static void
brasero_project_name_track_added (BraseroBurnSession *session,
				  BraseroTrack *track,
				  BraseroProjectName *self)
{
	brasero_project_name_session_changed (self);
}

static void
brasero_project_name_track_changed (BraseroBurnSession *session,
				    BraseroTrack *track,
				    BraseroProjectName *self)
{
	/* It can happen that stream tracks change */
	brasero_project_name_set_type (self);
}

static void
brasero_project_name_track_removed (BraseroBurnSession *session,
				    BraseroTrack *track,
				    guint former_position,
				    BraseroProjectName *self)
{
	/* Make sure we don't remain connected */
	if (BRASERO_IS_TRACK_DATA_CFG (track))
		g_signal_handlers_disconnect_by_func (track,
						      brasero_project_name_icon_changed_cb,
						      self);

	brasero_project_name_session_changed (self);
}

static void
brasero_project_name_unset_session (BraseroProjectName *project)
{
	BraseroProjectNamePrivate *priv;

	priv = BRASERO_PROJECT_NAME_PRIVATE (project);

	if (!priv->session)
		return;

	g_signal_handlers_disconnect_by_func (priv->session,
					      brasero_project_name_track_added,
					      project);
	g_signal_handlers_disconnect_by_func (priv->session,
					      brasero_project_name_track_changed,
					      project);
	g_signal_handlers_disconnect_by_func (priv->session,
					      brasero_project_name_track_removed,
					      project);
	g_signal_handlers_disconnect_by_func (priv->session,
					      brasero_project_name_flags_changed,
					      project);

	g_object_unref (priv->session);
	priv->session = NULL;
}

void
brasero_project_name_set_session (BraseroProjectName *project,
				  BraseroBurnSession *session)
{
	BraseroProjectNamePrivate *priv;

	priv = BRASERO_PROJECT_NAME_PRIVATE (project);

	brasero_project_name_unset_session (project);
	if (!session)
		return;

	priv->session = g_object_ref (session);

	g_signal_connect (priv->session,
			  "track-added",
			  G_CALLBACK (brasero_project_name_track_added),
			  project);
	g_signal_connect (priv->session,
			  "track-changed",
			  G_CALLBACK (brasero_project_name_track_changed),
			  project);
	g_signal_connect (priv->session,
			  "track-removed",
			  G_CALLBACK (brasero_project_name_track_removed),
			  project);
	g_signal_connect (priv->session,
			  "notify::flags",
			  G_CALLBACK (brasero_project_name_flags_changed),
			  project);

	brasero_project_name_session_changed (project);
}

static void
brasero_project_name_set_property (GObject *object,
				   guint property_id,
				   const GValue *value,
				   GParamSpec *pspec)
{
	switch (property_id) {
	case PROP_SESSION:
		brasero_project_name_set_session (BRASERO_PROJECT_NAME (object),
						  g_value_get_object (value));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
brasero_project_name_get_property (GObject *object,
				   guint property_id,
				   GValue *value,
				   GParamSpec *pspec)
{
	BraseroProjectNamePrivate *priv;

	priv = BRASERO_PROJECT_NAME_PRIVATE (object);

	switch (property_id) {
	case PROP_SESSION:
		g_value_set_object (value, G_OBJECT (priv->session));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
brasero_project_name_finalize (GObject *object)
{
	brasero_project_name_unset_session (BRASERO_PROJECT_NAME (object));

	G_OBJECT_CLASS (brasero_project_name_parent_class)->finalize (object);
}

static void
brasero_project_name_class_init (BraseroProjectNameClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroProjectNamePrivate));

	object_class->finalize = brasero_project_name_finalize;
	object_class->set_property = brasero_project_name_set_property;
	object_class->get_property = brasero_project_name_get_property;

	brasero_project_name_signals [CHANGED_SIGNAL] =
	    g_signal_new ("name_changed",
			  BRASERO_TYPE_PROJECT_NAME,
			  G_SIGNAL_RUN_FIRST|G_SIGNAL_ACTION|G_SIGNAL_NO_RECURSE,
			  0,
			  NULL,
			  NULL,
			  g_cclosure_marshal_VOID__VOID,
			  G_TYPE_NONE,
			  0);

	g_object_class_install_property (object_class,
					 PROP_SESSION,
					 g_param_spec_object ("session",
							      "The session",
							      "The session to work with",
							      BRASERO_TYPE_BURN_SESSION,
							      G_PARAM_READWRITE));
}

GtkWidget *
brasero_project_name_new (BraseroBurnSession *session)
{
	return g_object_new (BRASERO_TYPE_PROJECT_NAME,
			     "session", session,
			     NULL);
}

