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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include "brasero-misc.h"

#include "burn-debug.h"

#include "brasero-track-stream-cfg.h"
#include "brasero-io.h"
#include "brasero-tags.h"

static BraseroIOJobCallbacks *io_methods = NULL;

typedef struct _BraseroTrackStreamCfgPrivate BraseroTrackStreamCfgPrivate;
struct _BraseroTrackStreamCfgPrivate
{
	BraseroIOJobBase *load_uri;

	GFileMonitor *monitor;

	GError *error;

	guint loading:1;
};

#define BRASERO_TRACK_STREAM_CFG_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_TRACK_STREAM_CFG, BraseroTrackStreamCfgPrivate))

G_DEFINE_TYPE (BraseroTrackStreamCfg, brasero_track_stream_cfg, BRASERO_TYPE_TRACK_STREAM);

static void
brasero_track_stream_cfg_file_changed (GFileMonitor *monitor,
								GFile *file,
								GFile *other_file,
								GFileMonitorEvent event,
								BraseroTrackStream *track)
{
	BraseroTrackStreamCfgPrivate *priv;
	gchar *name;

	priv = BRASERO_TRACK_STREAM_CFG_PRIVATE (track);

        switch (event) {
 /*               case G_FILE_MONITOR_EVENT_CHANGED:
                        return;
*/
                case G_FILE_MONITOR_EVENT_DELETED:
                        g_object_unref (priv->monitor);
                        priv->monitor = NULL;

			name = g_file_get_basename (file);
			priv->error = g_error_new (BRASERO_BURN_ERROR,
								  BRASERO_BURN_ERROR_FILE_NOT_FOUND,
								  /* Translators: %s is the name of the file that has just been deleted */
								  _("\"%s\" was removed from the file system."),
								 name);
			g_free (name);
                        break;

                default:
                        return;
        }

        brasero_track_changed (BRASERO_TRACK (track));
}

static void
brasero_track_stream_cfg_results_cb (GObject *obj,
				     GError *error,
				     const gchar *uri,
				     GFileInfo *info,
				     gpointer user_data)
{
	GFile *file;
	guint64 len;
	GObject *snapshot;
	BraseroTrackStreamCfgPrivate *priv;

	priv = BRASERO_TRACK_STREAM_CFG_PRIVATE (obj);
	priv->loading = FALSE;

	/* Check the return status for this file */
	if (error) {
		priv->error = g_error_copy (error);
		brasero_track_changed (BRASERO_TRACK (obj));
		return;
	}

	/* FIXME: we don't know whether it's audio or video that is required */
	if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
		/* This error is special as it can be recovered from */
		priv->error = g_error_new (BRASERO_BURN_ERROR,
					   BRASERO_BURN_ERROR_FILE_FOLDER,
					   _("Directories cannot be added to video or audio discs"));
		brasero_track_changed (BRASERO_TRACK (obj));
		return;
	}

	if (g_file_info_get_file_type (info) == G_FILE_TYPE_REGULAR
	&&  (!strcmp (g_file_info_get_content_type (info), "audio/x-scpls")
	||   !strcmp (g_file_info_get_content_type (info), "audio/x-ms-asx")
	||   !strcmp (g_file_info_get_content_type (info), "audio/x-mp3-playlist")
	||   !strcmp (g_file_info_get_content_type (info), "audio/x-mpegurl"))) {
		/* This error is special as it can be recovered from */
		priv->error = g_error_new (BRASERO_BURN_ERROR,
					   BRASERO_BURN_ERROR_FILE_PLAYLIST,
					   _("Playlists cannot be added to video or audio discs"));

		brasero_track_changed (BRASERO_TRACK (obj));
		return;
	}

	if (g_file_info_get_file_type (info) != G_FILE_TYPE_REGULAR
	|| (!g_file_info_get_attribute_boolean (info, BRASERO_IO_HAS_VIDEO)
	&&  !g_file_info_get_attribute_boolean (info, BRASERO_IO_HAS_AUDIO))) {
		gchar *name;

		BRASERO_GET_BASENAME_FOR_DISPLAY (uri, name);
		priv->error = g_error_new (BRASERO_BURN_ERROR,
					   BRASERO_BURN_ERROR_GENERAL,
					   /* Translators: %s is the name of the file */
					   _("\"%s\" is not suitable for audio or video media"),
					   name);
		g_free (name);

		brasero_track_changed (BRASERO_TRACK (obj));
		return;
	}

	/* Also make sure it's duration is appropriate (!= 0) */
	len = g_file_info_get_attribute_uint64 (info, BRASERO_IO_LEN);
	if (len <= 0) {
		gchar *name;

		BRASERO_GET_BASENAME_FOR_DISPLAY (uri, name);
		priv->error = g_error_new (BRASERO_BURN_ERROR,
					   BRASERO_BURN_ERROR_GENERAL,
					   /* Translators: %s is the name of the file */
					   _("\"%s\" is not suitable for audio or video media"),
					   name);
		g_free (name);

		brasero_track_changed (BRASERO_TRACK (obj));
		return;
	}

	if (g_file_info_get_is_symlink (info)) {
		gchar *sym_uri;

		sym_uri = g_strconcat ("file://", g_file_info_get_symlink_target (info), NULL);
		if (BRASERO_TRACK_STREAM_CLASS (brasero_track_stream_cfg_parent_class)->set_source)
			BRASERO_TRACK_STREAM_CLASS (brasero_track_stream_cfg_parent_class)->set_source (BRASERO_TRACK_STREAM (obj), sym_uri);

		g_free (sym_uri);
	}

	/* Check whether the stream is wav+dts as it can be burnt as such */
	if (g_file_info_get_attribute_boolean (info, BRASERO_IO_HAS_DTS)) {
		BRASERO_BURN_LOG ("Track has DTS");
		BRASERO_TRACK_STREAM_CLASS (brasero_track_stream_cfg_parent_class)->set_format (BRASERO_TRACK_STREAM (obj), 
		                                                                                BRASERO_AUDIO_FORMAT_DTS|
		                                                                                BRASERO_METADATA_INFO);
	}
	else if (BRASERO_TRACK_STREAM_CLASS (brasero_track_stream_cfg_parent_class)->set_format)
		BRASERO_TRACK_STREAM_CLASS (brasero_track_stream_cfg_parent_class)->set_format (BRASERO_TRACK_STREAM (obj),
		                                                                                (g_file_info_get_attribute_boolean (info, BRASERO_IO_HAS_VIDEO)?
		                                                                                 BRASERO_VIDEO_FORMAT_UNDEFINED:BRASERO_AUDIO_FORMAT_NONE)|
		                                                                                (g_file_info_get_attribute_boolean (info, BRASERO_IO_HAS_AUDIO)?
		                                                                                 BRASERO_AUDIO_FORMAT_UNDEFINED:BRASERO_AUDIO_FORMAT_NONE)|
		                                                                                BRASERO_METADATA_INFO);

	/* Size/length. Only set when end value has not been already set.
	 * Fix #607752 -  audio track start and end points are overwritten after
	 * being read from a project file.
	 * We don't want to set a new len if one has been set already. Nevertheless
	 * if the length we detected is smaller than the one that was set we go
	 * for the new one. */
	if (BRASERO_TRACK_STREAM_CLASS (brasero_track_stream_cfg_parent_class)->set_boundaries) {
		gint64 min_start;

		/* Make sure that the start value is coherent */
		min_start = (len - BRASERO_MIN_STREAM_LENGTH) >= 0? (len - BRASERO_MIN_STREAM_LENGTH):0;
		if (min_start && brasero_track_stream_get_start (BRASERO_TRACK_STREAM (obj)) > min_start) {
			BRASERO_TRACK_STREAM_CLASS (brasero_track_stream_cfg_parent_class)->set_boundaries (BRASERO_TRACK_STREAM (obj),
													    min_start,
													    -1,
													    -1);
		}

		if (brasero_track_stream_get_end (BRASERO_TRACK_STREAM (obj)) > len
		||  brasero_track_stream_get_end (BRASERO_TRACK_STREAM (obj)) <= 0) {
			/* Don't set either gap or start to make sure we don't remove
			 * values set by project parser or values set from the beginning
			 * Fix #607752 -  audio track start and end points are overwritten
			 * after being read from a project file */
			BRASERO_TRACK_STREAM_CLASS (brasero_track_stream_cfg_parent_class)->set_boundaries (BRASERO_TRACK_STREAM (obj),
													    -1,
													    len,
													    -1);
		}
	}

	snapshot = g_file_info_get_attribute_object (info, BRASERO_IO_THUMBNAIL);
	if (snapshot) {
		GValue *value;

		value = g_new0 (GValue, 1);
		g_value_init (value, GDK_TYPE_PIXBUF);
		g_value_set_object (value, g_object_ref (snapshot));
		brasero_track_tag_add (BRASERO_TRACK (obj),
				       BRASERO_TRACK_STREAM_THUMBNAIL_TAG,
				       value);
	}

	if (g_file_info_get_content_type (info)) {
		const gchar *icon_string = "text-x-preview";
		GtkIconTheme *theme;
		GIcon *icon;

		theme = gtk_icon_theme_get_default ();

		/* NOTE: implemented in glib 2.15.6 (not for windows though) */
		icon = g_content_type_get_icon (g_file_info_get_content_type (info));
		if (G_IS_THEMED_ICON (icon)) {
			const gchar * const *names = NULL;

			names = g_themed_icon_get_names (G_THEMED_ICON (icon));
			if (names) {
				gint i;

				for (i = 0; names [i]; i++) {
					if (gtk_icon_theme_has_icon (theme, names [i])) {
						icon_string = names [i];
						break;
					}
				}
			}
		}

		brasero_track_tag_add_string (BRASERO_TRACK (obj),
					      BRASERO_TRACK_STREAM_MIME_TAG,
					      icon_string);
		g_object_unref (icon);
	}

	/* Get the song info */
	if (g_file_info_get_attribute_string (info, BRASERO_IO_TITLE)
	&& !brasero_track_tag_lookup_string (BRASERO_TRACK (obj), BRASERO_TRACK_STREAM_TITLE_TAG))
		brasero_track_tag_add_string (BRASERO_TRACK (obj),
					      BRASERO_TRACK_STREAM_TITLE_TAG,
					      g_file_info_get_attribute_string (info, BRASERO_IO_TITLE));
	if (g_file_info_get_attribute_string (info, BRASERO_IO_ARTIST)
	&& !brasero_track_tag_lookup_string (BRASERO_TRACK (obj), BRASERO_TRACK_STREAM_ARTIST_TAG))
		brasero_track_tag_add_string (BRASERO_TRACK (obj),
					      BRASERO_TRACK_STREAM_ARTIST_TAG,
					      g_file_info_get_attribute_string (info, BRASERO_IO_ARTIST));
	if (g_file_info_get_attribute_string (info, BRASERO_IO_ALBUM)
	&& !brasero_track_tag_lookup_string (BRASERO_TRACK (obj), BRASERO_TRACK_STREAM_ALBUM_TAG))
		brasero_track_tag_add_string (BRASERO_TRACK (obj),
					      BRASERO_TRACK_STREAM_ALBUM_TAG,
					      g_file_info_get_attribute_string (info, BRASERO_IO_ALBUM));
	if (g_file_info_get_attribute_string (info, BRASERO_IO_COMPOSER)
	&& !brasero_track_tag_lookup_string (BRASERO_TRACK (obj), BRASERO_TRACK_STREAM_COMPOSER_TAG))
		brasero_track_tag_add_string (BRASERO_TRACK (obj),
					      BRASERO_TRACK_STREAM_COMPOSER_TAG,
					      g_file_info_get_attribute_string (info, BRASERO_IO_COMPOSER));
	if (g_file_info_get_attribute_string (info, BRASERO_IO_ISRC)
	&& !brasero_track_tag_lookup_string (BRASERO_TRACK (obj), BRASERO_TRACK_STREAM_ISRC_TAG))
		brasero_track_tag_add_string (BRASERO_TRACK (obj),
					      BRASERO_TRACK_STREAM_ISRC_TAG,
					      g_file_info_get_attribute_string (info, BRASERO_IO_ISRC));

	/* Start monitoring it */
	file = g_file_new_for_uri (uri);
	priv->monitor = g_file_monitor_file (file,
	                                     G_FILE_MONITOR_NONE,
	                                     NULL,
	                                     NULL);
	g_object_unref (file);

	g_signal_connect (priv->monitor,
	                  "changed",
	                  G_CALLBACK (brasero_track_stream_cfg_file_changed),
	                  obj);

	brasero_track_changed (BRASERO_TRACK (obj));
}

static void
brasero_track_stream_cfg_get_info (BraseroTrackStreamCfg *track)
{
	BraseroTrackStreamCfgPrivate *priv;
	gchar *uri;

	priv = BRASERO_TRACK_STREAM_CFG_PRIVATE (track);

	if (priv->error) {
		g_error_free (priv->error);
		priv->error = NULL;
	}

	/* get info async for the file */
	if (!priv->load_uri) {
		if (!io_methods)
			io_methods = brasero_io_register_job_methods (brasero_track_stream_cfg_results_cb,
			                                              NULL,
			                                              NULL);

		priv->load_uri = brasero_io_register_with_methods (G_OBJECT (track), io_methods);
	}

	priv->loading = TRUE;
	uri = brasero_track_stream_get_source (BRASERO_TRACK_STREAM (track), TRUE);
	brasero_io_get_file_info (uri,
				  priv->load_uri,
				  BRASERO_IO_INFO_PERM|
				  BRASERO_IO_INFO_MIME|
				  BRASERO_IO_INFO_URGENT|
				  BRASERO_IO_INFO_METADATA|
				  BRASERO_IO_INFO_METADATA_MISSING_CODEC|
				  BRASERO_IO_INFO_METADATA_THUMBNAIL,
				  track);
	g_free (uri);
}

static BraseroBurnResult
brasero_track_stream_cfg_set_source (BraseroTrackStream *track,
				     const gchar *uri)
{
	BraseroTrackStreamCfgPrivate *priv;

	priv = BRASERO_TRACK_STREAM_CFG_PRIVATE (track);
	if (priv->monitor) {
		g_object_unref (priv->monitor);
		priv->monitor = NULL;
	}

	if (priv->load_uri)
		brasero_io_cancel_by_base (priv->load_uri);

	if (BRASERO_TRACK_STREAM_CLASS (brasero_track_stream_cfg_parent_class)->set_source)
		BRASERO_TRACK_STREAM_CLASS (brasero_track_stream_cfg_parent_class)->set_source (track, uri);

	brasero_track_stream_cfg_get_info (BRASERO_TRACK_STREAM_CFG (track));
	brasero_track_changed (BRASERO_TRACK (track));
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_track_stream_cfg_get_status (BraseroTrack *track,
				     BraseroStatus *status)
{
	BraseroTrackStreamCfgPrivate *priv;

	priv = BRASERO_TRACK_STREAM_CFG_PRIVATE (track);

	if (priv->error) {
		brasero_status_set_error (status, g_error_copy (priv->error));
		return BRASERO_BURN_ERR;
	}

	if (priv->loading) {
		if (status)
			brasero_status_set_not_ready (status,
						      -1.0,
						      _("Analysing video files"));

		return BRASERO_BURN_NOT_READY;
	}

	if (status)
		brasero_status_set_completed (status);

	return BRASERO_TRACK_CLASS (brasero_track_stream_cfg_parent_class)->get_status (track, status);
}

static void
brasero_track_stream_cfg_init (BraseroTrackStreamCfg *object)
{ }

static void
brasero_track_stream_cfg_finalize (GObject *object)
{
	BraseroTrackStreamCfgPrivate *priv;

	priv = BRASERO_TRACK_STREAM_CFG_PRIVATE (object);

	if (priv->load_uri) {
		brasero_io_cancel_by_base (priv->load_uri);

		if (io_methods->ref == 1)
			io_methods = NULL;

		brasero_io_job_base_free (priv->load_uri);
		priv->load_uri = NULL;
	}

	if (priv->monitor) {
		g_object_unref (priv->monitor);
		priv->monitor = NULL;
	}

	if (priv->error) {
		g_error_free (priv->error);
		priv->error = NULL;
	}

	G_OBJECT_CLASS (brasero_track_stream_cfg_parent_class)->finalize (object);
}

static void
brasero_track_stream_cfg_class_init (BraseroTrackStreamCfgClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	BraseroTrackClass* track_class = BRASERO_TRACK_CLASS (klass);
	BraseroTrackStreamClass *parent_class = BRASERO_TRACK_STREAM_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroTrackStreamCfgPrivate));

	object_class->finalize = brasero_track_stream_cfg_finalize;

	track_class->get_status = brasero_track_stream_cfg_get_status;

	parent_class->set_source = brasero_track_stream_cfg_set_source;
}

/**
 * brasero_track_stream_cfg_new:
 *
 *  Creates a new #BraseroTrackStreamCfg object.
 *
 * Return value: a #BraseroTrackStreamCfg object.
 **/

BraseroTrackStreamCfg *
brasero_track_stream_cfg_new (void)
{
	return g_object_new (BRASERO_TYPE_TRACK_STREAM_CFG, NULL);
}

