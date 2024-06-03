/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-misc
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-misc is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-misc authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-misc. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-misc is distributed in the hope that it will be useful,
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

#include <string.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib-object.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>
#include <gst/pbutils/install-plugins.h>
#include <gst/pbutils/missing-plugins.h>
#include <gst/tag/tag.h>

#include "brasero-misc.h"
#include "brasero-metadata.h"

#define BRASERO_METADATA_SILENCE_INTERVAL		100000000LL
#define BRASERO_METADATA_INITIAL_STATE			GST_STATE_PAUSED

struct BraseroMetadataPrivate {
	GstElement *pipeline;
	GstElement *source;
	GstElement *decode;
	GstElement *convert;
	GstElement *level;
	GstElement *sink;

	GstElement *pipeline_mp3;

	GstElement *audio;
	GstElement *video;

	GstElement *snapshot;

	GError *error;
	guint watch;
	guint watch_mp3;

	BraseroMetadataSilence *silence;

	BraseroMetadataFlag flags;
	BraseroMetadataInfo *info;

	/* This is for automatic missing plugin install */
	GSList *missing_plugins;
	GSList *downloads;

	GMutex *mutex;
	GSList *conditions;

	gint listeners;

	BraseroMetadataGetXidCb xid_callback;
	gpointer xid_user_data;

	guint started:1;
	guint moved_forward:1;
	guint prev_level_mes:1;
	guint video_linked:1;
	guint audio_linked:1;
	guint snapshot_started:1;
};
typedef struct BraseroMetadataPrivate BraseroMetadataPrivate;
#define BRASERO_METADATA_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), BRASERO_TYPE_METADATA, BraseroMetadataPrivate))

enum {
	PROP_NONE,
	PROP_URI
};

typedef enum {
	COMPLETED_SIGNAL,
	LAST_SIGNAL
} BraseroMetadataSignalType;

static guint brasero_metadata_signals [LAST_SIGNAL] = { 0 };

#define BRASERO_METADATA_IS_FAST(flags) 					\
	(!((flags) & BRASERO_METADATA_FLAG_SILENCES) &&				\
	((flags) & BRASERO_METADATA_FLAG_FAST))

G_DEFINE_TYPE (BraseroMetadata, brasero_metadata, G_TYPE_OBJECT)

static GSList *downloading = NULL;
static GSList *downloaded = NULL;

static gboolean
brasero_metadata_completed (BraseroMetadata *self);

static int
brasero_metadata_get_xid (BraseroMetadata *metadata)
{
	BraseroMetadataPrivate *priv;

	priv = BRASERO_METADATA_PRIVATE (metadata);
	if (!priv->xid_callback)
		return 0;

	return priv->xid_callback (priv->xid_user_data);
}

void
brasero_metadata_set_get_xid_callback (BraseroMetadata *metadata,
                                       BraseroMetadataGetXidCb callback,
                                       gpointer user_data)
{
	BraseroMetadataPrivate *priv;

	priv = BRASERO_METADATA_PRIVATE (metadata);
	priv->xid_callback = callback;
	priv->xid_user_data = user_data;
}

struct _BraseroMetadataGstDownload {
	gchar *detail;

	/* These are all metadata objects waiting */
	GSList *objects;
};
typedef struct _BraseroMetadataGstDownload BraseroMetadataGstDownload;

void
brasero_metadata_info_clear (BraseroMetadataInfo *info)
{
	if (!info)
		return;

	if (info->snapshot) {
		g_object_unref (info->snapshot);
		info->snapshot = NULL;
	}

	if (info->uri)
		g_free (info->uri);

	if (info->type)
		g_free (info->type);

	if (info->title)
		g_free (info->title);

	if (info->artist)
		g_free (info->artist);

	if (info->album)
		g_free (info->album);

	if (info->genre)
		g_free (info->genre);

	if (info->musicbrainz_id)
		g_free (info->musicbrainz_id);

	if (info->isrc)
		g_free (info->isrc);

	if (info->silences) {
		g_slist_foreach (info->silences, (GFunc) g_free, NULL);
		g_slist_free (info->silences);
		info->silences = NULL;
	}
}

void
brasero_metadata_info_free (BraseroMetadataInfo *info)
{
	if (!info)
		return;

	brasero_metadata_info_clear (info);

	g_free (info);
}

void
brasero_metadata_info_copy (BraseroMetadataInfo *dest,
			    BraseroMetadataInfo *src)
{
	GSList *iter;

	if (!dest || !src)
		return;

	dest->has_dts = src->has_dts;
	dest->rate = src->rate;
	dest->channels = src->channels;
	dest->len = src->len;
	dest->is_seekable = src->is_seekable;
	dest->has_audio = src->has_audio;
	dest->has_video = src->has_video;

	if (src->uri)
		dest->uri = g_strdup (src->uri);

	if (src->type)
		dest->type = g_strdup (src->type);

	if (src->title)
		dest->title = g_strdup (src->title);

	if (src->artist)
		dest->artist = g_strdup (src->artist);

	if (src->album)
		dest->album = g_strdup (src->album);

	if (src->genre)
		dest->genre = g_strdup (src->genre);

	if (src->musicbrainz_id)
		dest->musicbrainz_id = g_strdup (src->musicbrainz_id);

	if (src->isrc)
		dest->isrc = g_strdup (src->isrc);

	if (src->snapshot) {
		dest->snapshot = src->snapshot;
		g_object_ref (dest->snapshot);
	}

	for (iter = src->silences; iter; iter = iter->next) {
		BraseroMetadataSilence *silence, *copy;

		silence = iter->data;

		copy = g_new0 (BraseroMetadataSilence, 1);
		copy->start = silence->start;
		copy->end = silence->end;

		dest->silences = g_slist_append (dest->silences, copy);
	}
}

static void
brasero_metadata_stop_pipeline (GstElement *pipeline)
{
	GstState state;
	GstStateChangeReturn change;

	change = gst_element_set_state (GST_ELEMENT (pipeline),
					GST_STATE_NULL);

	change = gst_element_get_state (pipeline,
					&state,
					NULL,
					GST_MSECOND);

	/* better wait for the state change to be completed */
	while (change == GST_STATE_CHANGE_ASYNC && state != GST_STATE_NULL) {
		GstState pending;

		change = gst_element_get_state (pipeline,
						&state,
						&pending,
						GST_MSECOND);
		BRASERO_UTILS_LOG ("Get state (current = %i pending = %i) returned %i",
				   state, pending, change);
	}

	if (change == GST_STATE_CHANGE_FAILURE)
		g_warning ("State change failure");

}

static void
brasero_metadata_destroy_pipeline (BraseroMetadata *self)
{
	BraseroMetadataPrivate *priv;

	priv = BRASERO_METADATA_PRIVATE (self);

	priv->started = 0;

	if (priv->pipeline_mp3) {
		brasero_metadata_stop_pipeline (priv->pipeline_mp3);
		gst_object_unref (GST_OBJECT (priv->pipeline_mp3));
		priv->pipeline_mp3 = NULL;
	}

	if (priv->watch_mp3) {
		g_source_remove (priv->watch_mp3);
		priv->watch_mp3 = 0;
	}

	if (!priv->pipeline)
		return;

	brasero_metadata_stop_pipeline (priv->pipeline);

	if (priv->audio) {
		gst_bin_remove (GST_BIN (priv->pipeline), priv->audio);
		priv->audio = NULL;
	}

	if (priv->video) {
		gst_bin_remove (GST_BIN (priv->pipeline), priv->video);
		priv->snapshot = NULL;
		priv->video = NULL;
	}

	gst_object_unref (GST_OBJECT (priv->pipeline));
	priv->pipeline = NULL;

	if (priv->level) {
		gst_object_unref (GST_OBJECT (priv->level));
		priv->level = NULL;
	}

	if (priv->sink) {
		gst_object_unref (GST_OBJECT (priv->sink));
		priv->sink = NULL;
	}

	if (priv->convert) {
		gst_object_unref (GST_OBJECT (priv->convert));
		priv->convert = NULL;
	}
}

static void
brasero_metadata_stop (BraseroMetadata *self)
{
	BraseroMetadataPrivate *priv;
	GSList *iter;

	priv = BRASERO_METADATA_PRIVATE (self);

	BRASERO_UTILS_LOG ("Retrieval ended for %s %p",
  			   priv->info ? priv->info->uri:"Unknown",
			   self);

	g_mutex_lock (priv->mutex);

	/* Destroy the pipeline as it has become un-re-usable */
	if (priv->watch) {
		g_source_remove (priv->watch);
		priv->watch = 0;
	}

	if (priv->pipeline)
		brasero_metadata_destroy_pipeline (self);

	/* That's automatic missing plugin installation */
	if (priv->missing_plugins) {
		g_slist_foreach (priv->missing_plugins,
				 (GFunc) gst_mini_object_unref,
				 NULL);
		g_slist_free (priv->missing_plugins);
		priv->missing_plugins = NULL;
	}

	if (priv->downloads) {
		GSList *iter;

		for (iter = priv->downloads; iter; iter = iter->next) {
			BraseroMetadataGstDownload *download;

			download = iter->data;
			download->objects = g_slist_remove (download->objects, self);
		}

		g_slist_free (priv->downloads);
		priv->downloads = NULL;
	}

	/* stop the pipeline */
	priv->started = 0;

	/* Tell all the waiting threads that we're done */
	for (iter = priv->conditions; iter; iter = iter->next) {
		GCond *condition;

		condition = iter->data;
		g_cond_broadcast (condition);
	}

	g_mutex_unlock (priv->mutex);
}

void
brasero_metadata_cancel (BraseroMetadata *self)
{
	BraseroMetadataPrivate *priv;

	priv = BRASERO_METADATA_PRIVATE (self);

	BRASERO_UTILS_LOG ("Metadata retrieval cancelled for %s %p",
			   priv->info ? priv->info->uri:"Unknown",
			   self);
	
	brasero_metadata_stop (self);
	if (priv->error) {
		g_error_free (priv->error);
		priv->error = NULL;
	}
}
static void
brasero_metadata_install_plugins_add_downloaded (GSList *downloads)
{
	GSList *iter;

	for (iter = downloads; iter; iter = iter->next) {
		BraseroMetadataGstDownload *download;

		download = iter->data;
		downloaded = g_slist_prepend (downloaded, download->detail);
		download->detail = NULL;
	}
}

static void
brasero_metadata_install_plugins_free_data (GSList *downloads)
{
	GSList *iter;

	for (iter = downloads; iter; iter = iter->next) {
		BraseroMetadataGstDownload *download;
		GSList *meta;

		download = iter->data;
		if (download->detail)
			g_free (download->detail);

		for (meta = download->objects; meta; meta = meta->next) {
			BraseroMetadataPrivate *priv;

			priv = BRASERO_METADATA_PRIVATE (meta->data);
			priv->downloads = g_slist_remove (priv->downloads, download);
		}
		g_slist_free (download->objects);

		downloading = g_slist_remove (downloading, download);
		g_free (download);
	}

	g_slist_free (downloads);
}

static void
brasero_metadata_install_plugins_success (BraseroMetadataGstDownload *download)
{
	GSList *iter;

	for (iter = download->objects; iter; iter = iter->next) {
		BraseroMetadataPrivate *priv;

		priv = BRASERO_METADATA_PRIVATE (iter->data);

		if (priv->error) {
			/* free previously saved error message */
			g_error_free (priv->error);
			priv->error = NULL;
		}

		gst_element_set_state (GST_ELEMENT (priv->pipeline), GST_STATE_NULL);
		gst_element_set_state (GST_ELEMENT (priv->pipeline), GST_STATE_PLAYING);
	}
}

static void
brasero_metadata_install_plugins_abort (BraseroMetadataGstDownload *download)
{
	GSList *iter;
	GSList *next;

	for (iter = download->objects; iter; iter = next) {
		BraseroMetadataPrivate *priv;

		next = iter->next;

		priv = BRASERO_METADATA_PRIVATE (iter->data);

		if (priv->error) {
			g_error_free (priv->error);
			priv->error = NULL;
		}

		brasero_metadata_completed (BRASERO_METADATA (iter->data));
	}
}

static void
brasero_metadata_install_plugins_completed (BraseroMetadataGstDownload *download)
{
	GSList *iter;
	GSList *next;

	for (iter = download->objects; iter; iter = next) {
		next = iter->next;
		brasero_metadata_completed (BRASERO_METADATA (iter->data));
	}
}

static void
brasero_metadata_install_plugins_result (GstInstallPluginsReturn res,
					 gpointer data)
{
	GSList *downloads = data;
	GSList *iter;

	switch (res) {
	case GST_INSTALL_PLUGINS_PARTIAL_SUCCESS:
	case GST_INSTALL_PLUGINS_SUCCESS:
		brasero_metadata_install_plugins_add_downloaded (downloads);

		/* force gst to update plugin list */
		gst_update_registry ();

		/* restart metadata search */
		for (iter = downloads; iter; iter = iter->next) {
			BraseroMetadataGstDownload *download;

			download = iter->data;
			brasero_metadata_install_plugins_success (download);
		}
		break;

	case GST_INSTALL_PLUGINS_NOT_FOUND:
		brasero_metadata_install_plugins_add_downloaded (downloads);

		/* stop everything */
		for (iter = downloads; iter; iter = iter->next)
			brasero_metadata_install_plugins_completed (iter->data);
		break;

	case GST_INSTALL_PLUGINS_USER_ABORT:
		brasero_metadata_install_plugins_add_downloaded (downloads);

		/* free previously saved error message */
		for (iter = downloads; iter; iter = iter->next) {
			BraseroMetadataGstDownload *download;

			download = iter->data;
			brasero_metadata_install_plugins_abort (download);
		}
		break;

	case GST_INSTALL_PLUGINS_ERROR:
	case GST_INSTALL_PLUGINS_CRASHED:
	default:
		for (iter = downloads; iter; iter = iter->next)
			brasero_metadata_install_plugins_completed (iter->data);

		break;
	}

	brasero_metadata_install_plugins_free_data (downloads);
}

static BraseroMetadataGstDownload *
brasero_metadata_is_downloading (const gchar *detail)
{
	GSList *iter;

	for (iter = downloading; iter; iter = iter->next) {
		BraseroMetadataGstDownload *download;

		download = iter->data;
		if (!strcmp (download->detail, detail))
			return download;
	}

	return NULL;
}

static gboolean
brasero_metadata_install_missing_plugins (BraseroMetadata *self)
{
	GstInstallPluginsContext *context;
	GstInstallPluginsReturn status;
	BraseroMetadataPrivate *priv;
	GSList *downloads = NULL;
	GPtrArray *details;
	GSList *iter;

	priv = BRASERO_METADATA_PRIVATE (self);

	BRASERO_UTILS_LOG ("Starting to download missing plugins");

	details = g_ptr_array_new ();
	for (iter = priv->missing_plugins; iter; iter = iter->next) {
		gchar *detail;
		BraseroMetadataGstDownload *download;

		/* Check if this plugin:
		 * - has already been downloaded (whether it was successful or not)
		 * - is being downloaded
		 * If so don't do anything. */
		detail = gst_missing_plugin_message_get_installer_detail (iter->data);
		gst_mini_object_unref (iter->data);

		download = brasero_metadata_is_downloading (detail);
		if (download) {
			download->objects = g_slist_prepend (download->objects, self);
			g_free (detail);
			continue;
		}

		if (g_slist_find_custom (downloaded, detail, (GCompareFunc) strcmp)) {
			g_free (detail);
			continue;
		}

		download = g_new0 (BraseroMetadataGstDownload, 1);
		download->detail = detail;
		download->objects = g_slist_prepend (download->objects, self);
		priv->downloads = g_slist_prepend (priv->downloads, download);

		downloads = g_slist_prepend (downloads, download);
		downloading = g_slist_prepend (downloading, download);

		g_ptr_array_add (details, detail);
	}

	g_slist_free (priv->missing_plugins);
	priv->missing_plugins = NULL;

	if (!details->len) {
		/* either these plugins were downloaded or are being downloaded */
		g_ptr_array_free (details, TRUE);
		if (!priv->downloads)
			return FALSE;

		return TRUE;
	}

	g_ptr_array_add (details, NULL);

	/* FIXME: we'd need the main window here to set it modal */

	context = gst_install_plugins_context_new ();
	gst_install_plugins_context_set_xid (context, brasero_metadata_get_xid (self));
	status = gst_install_plugins_async ((const gchar* const*) details->pdata,
					    context,
					    brasero_metadata_install_plugins_result,
					    downloads);

	gst_install_plugins_context_free (context);
	g_ptr_array_free (details, TRUE);

	BRASERO_UTILS_LOG ("Download status %i", status);

	if (status != GST_INSTALL_PLUGINS_STARTED_OK) {
		brasero_metadata_install_plugins_free_data (downloads);
		return FALSE;
	}

	return TRUE;
}

static gboolean
brasero_metadata_completed (BraseroMetadata *self)
{
	BraseroMetadataPrivate *priv;

	priv = BRASERO_METADATA_PRIVATE (self);

	if (priv->error) {
		BRASERO_UTILS_LOG ("Operation completed with an error %s", priv->error->message);
	}

	/* See if we have missing plugins */
	if (priv->missing_plugins) {
		if (brasero_metadata_install_missing_plugins (self))
			return TRUE;
	}

	/* we send a message only if we haven't got a loop (= async mode) */
	g_object_ref (self);
	g_signal_emit (G_OBJECT (self),
		       brasero_metadata_signals [COMPLETED_SIGNAL],
		       0,
		       priv->error);

	brasero_metadata_stop (self);

	g_object_unref (self);

	/* Return FALSE on purpose here as it will stop the bus callback 
	 * It's not whether we succeeded or not. */
	return FALSE;
}

static gboolean
brasero_metadata_thumbnail (BraseroMetadata *self)
{
	BraseroMetadataPrivate *priv;
	gint64 position;
	gboolean res;

	priv = BRASERO_METADATA_PRIVATE (self);

	/* find the right position and move forward */
	position = 15 * GST_SECOND;
	while (position > 0 && position >= priv->info->len)
		position -= 5 * GST_SECOND;

	if (position <= 0)
		return FALSE;

	gst_element_set_state (priv->pipeline, GST_STATE_PAUSED);

	priv->snapshot_started = 1;
	if (position < GST_SECOND)
		position = GST_SECOND;

	res = gst_element_seek_simple (priv->pipeline,
				       GST_FORMAT_TIME,
				       GST_SEEK_FLAG_FLUSH,
				       position);

	BRASERO_UTILS_LOG ("Seeking forward %i for %s", res, priv->info->uri);
	if (!res)
		return brasero_metadata_completed (self);

	g_object_set (priv->snapshot,
		      "send-messages", TRUE,
		      NULL);

	return TRUE;
}

static void
brasero_metadata_is_seekable (BraseroMetadata *self)
{
	GstQuery *query;
	GstFormat format;
	gboolean seekable;
	BraseroMetadataPrivate *priv;

	priv = BRASERO_METADATA_PRIVATE (self);

	priv->info->is_seekable = FALSE;

	/* NOTE: apparently GST_FORMAT_DEFAULT does not work here */
	query = gst_query_new_seeking (GST_FORMAT_TIME);

	/* NOTE: it works better now on the pipeline than on the source as we
	 * used to do */
	if (!gst_element_query (priv->pipeline, query))
		goto end;

	gst_query_parse_seeking (query,
				 &format,
				 &seekable,
				 NULL,
				 NULL);

	priv->info->is_seekable = seekable;

end:

	gst_query_unref (query);
}

/* FIXME: use GstDiscoverer ? */
static gboolean
brasero_metadata_get_mime_type (BraseroMetadata *self)
{
	BraseroMetadataPrivate *priv;
	GstElement *typefind;
	GstCaps *caps = NULL;
	const gchar *mime;

	priv = BRASERO_METADATA_PRIVATE (self);

	if (priv->info->type) {
		g_free (priv->info->type);
		priv->info->type = NULL;
	}

	/* find the type of the file */
	typefind = gst_bin_get_by_name (GST_BIN (priv->decode),
					"typefind");

	g_object_get (typefind, "caps", &caps, NULL);
	if (!caps) {
		gst_object_unref (typefind);
		return FALSE;
	}

	if (gst_caps_get_size (caps) <= 0) {
		gst_object_unref (typefind);
		return FALSE;
	}

	mime = gst_structure_get_name (gst_caps_get_structure (caps, 0));
	gst_object_unref (typefind);

	BRASERO_UTILS_LOG ("Mime type %s", mime);

	if (!mime)
		return FALSE;

	if (!strcmp (mime, "application/x-id3"))
		priv->info->type = g_strdup ("audio/mpeg");
	else if (!strcmp (mime, "audio/x-wav")) {
		GstElement *wavparse = NULL;
		GstIteratorResult res;
		GstIterator *iter;
		GValue value = { 0, };

		priv->info->type = g_strdup (mime);

		/* make sure it doesn't have dts inside */
		iter = gst_bin_iterate_recurse (GST_BIN (priv->decode));

		res = gst_iterator_next (iter, &value);
		while (res == GST_ITERATOR_OK) {
			GstElement *element;
			gchar *name;

			element = GST_ELEMENT (g_value_get_object (&value));
			name = gst_object_get_name (GST_OBJECT (element));
			if (name) {
				if (!strncmp (name, "wavparse", 8)) {
					wavparse = gst_object_ref (element);
					g_value_unset (&value);
					g_free (name);
					break;
				}
				g_free (name);
			}

			g_value_unset (&value);
			element = NULL;

			res = gst_iterator_next (iter, &value);
		}
		gst_iterator_free (iter);

		if (wavparse) {
			GstCaps *src_caps;
			GstPad *src_pad;

			src_pad = gst_element_get_static_pad (wavparse, "src");
			src_caps = gst_pad_get_current_caps (src_pad);
			gst_object_unref (src_pad);
			src_pad = NULL;

			if (src_caps) {
				GstStructure *structure;

				/* negotiated caps will always have one structure */
				structure = gst_caps_get_structure (src_caps, 0);
				priv->info->has_dts = gst_structure_has_name (structure, "audio/x-dts");
				gst_caps_unref (src_caps);
			}
			gst_object_unref (wavparse);
		}

		BRASERO_UTILS_LOG ("Wav file has dts: %s", priv->info->has_dts? "yes":"no");
	}
	else
		priv->info->type = g_strdup (mime);

	return TRUE;
}

static gboolean
brasero_metadata_is_mp3 (BraseroMetadata *self)
{
	BraseroMetadataPrivate *priv;

	priv = BRASERO_METADATA_PRIVATE (self);

	if (!priv->info->type
	&&  !brasero_metadata_get_mime_type (self))
		return FALSE;

	if (!strcmp (priv->info->type, "audio/mpeg"))
		return TRUE;

	return FALSE;
}

static void
foreach_tag (const GstTagList *list,
	     const gchar *tag,
	     BraseroMetadata *self)
{
	BraseroMetadataPrivate *priv;
	priv = BRASERO_METADATA_PRIVATE (self);

	if (!strcmp (tag, GST_TAG_TITLE)) {
		if (priv->info->title)
			g_free (priv->info->title);

		gst_tag_list_get_string (list, tag, &(priv->info->title));
	} else if (!strcmp (tag, GST_TAG_ARTIST)
	       ||  !strcmp (tag, GST_TAG_PERFORMER)) {
		if (priv->info->artist)
			g_free (priv->info->artist);

		gst_tag_list_get_string (list, tag, &(priv->info->artist));
	}
	else if (!strcmp (tag, GST_TAG_ALBUM)) {
		if (priv->info->album)
			g_free (priv->info->album);

		gst_tag_list_get_string (list, tag, &(priv->info->album));
	}
	else if (!strcmp (tag, GST_TAG_GENRE)) {
		if (priv->info->genre)
			g_free (priv->info->genre);

		gst_tag_list_get_string (list, tag, &(priv->info->genre));
	}
/*	else if (!strcmp (tag, GST_TAG_COMPOSER)) {
		if (self->composer)
			g_free (self->composer);

		gst_tag_list_get_string (list, tag, &(self->composer));
	}
*/	else if (!strcmp (tag, GST_TAG_ISRC)) {
		if (priv->info->isrc)
			g_free (priv->info->isrc);

		gst_tag_list_get_string (list, tag, &(priv->info->isrc));
	}
	else if (!strcmp (tag, GST_TAG_MUSICBRAINZ_TRACKID)) {
		gst_tag_list_get_string (list, tag, &(priv->info->musicbrainz_id));
	}
}

static gboolean
brasero_metadata_process_element_messages (BraseroMetadata *self,
					   GstMessage *msg)
{
	BraseroMetadataPrivate *priv;
	const GstStructure *s;

	priv = BRASERO_METADATA_PRIVATE (self);

	s = gst_message_get_structure (msg);

	/* This is for snapshot function */
	if (gst_message_has_name (msg, "preroll-pixbuf")
	||  gst_message_has_name (msg, "pixbuf")) {
		const GValue *value;

		value = gst_structure_get_value (s, "pixbuf");
		priv->info->snapshot = g_value_get_object (value);
		g_object_ref (priv->info->snapshot);

		BRASERO_UTILS_LOG ("Received pixbuf snapshot sink (%p) for %s", priv->info->snapshot, priv->info->uri);

		/* Now we can stop */
		return brasero_metadata_completed (self);
	}

	/* here we just want to check if that's a missing codec */
	if ((priv->flags & BRASERO_METADATA_FLAG_MISSING)
	&&   gst_is_missing_plugin_message (msg)) {
		priv->missing_plugins = g_slist_prepend (priv->missing_plugins, gst_message_ref (msg));
	}
	else if (gst_message_has_name (msg, "level")
	&&   gst_structure_has_field (s, "peak")) {
		const GValue *value;
		const GValue *list;
		gdouble peak;

		/* FIXME: this might still be changed to GValueArray before 1.0 release */
		list = gst_structure_get_value (s, "peak");
		value = gst_value_list_get_value (list, 0);
		peak = g_value_get_double (value);

		/* detection of silence */
		if (peak < -50.0) {
			gint64 pos = -1;

			/* was there a silence last time we check ?
			 * NOTE: if that's the first signal we receive
			 * then consider that silence started from 0 */
			gst_element_query_position (priv->pipeline, GST_FORMAT_TIME, &pos);
			if (pos == -1) {
				BRASERO_UTILS_LOG ("impossible to retrieve position");
				return TRUE;
			}

			if (!priv->silence) {
				priv->silence = g_new0 (BraseroMetadataSilence, 1);
				if (priv->prev_level_mes) {
					priv->silence->start = pos;
					priv->silence->end = pos;
				}
				else {
					priv->silence->start = 0;
					priv->silence->end = pos;
				}
			}				
			else
				priv->silence->end = pos;

			BRASERO_UTILS_LOG ("silence detected at %lli", pos);
		}
		else if (priv->silence) {
			BRASERO_UTILS_LOG ("silence finished");

			priv->info->silences = g_slist_append (priv->info->silences,
							       priv->silence);
			priv->silence = NULL;
		}
		priv->prev_level_mes = 1;
	}

	return TRUE;
}

static void
brasero_metadata_process_pending_messages (BraseroMetadata *self)
{
	GstBus *bus;
	GstMessage *msg;
	BraseroMetadataPrivate *priv;

	priv = BRASERO_METADATA_PRIVATE (self);

	bus = gst_pipeline_get_bus (GST_PIPELINE (priv->pipeline));
	while ((msg = gst_bus_pop (bus))) {
		GstTagList *tags = NULL;

		if (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_TAG) {
			gst_message_parse_tag (msg, &tags);
			gst_tag_list_foreach (tags, (GstTagForeachFunc) foreach_tag, self);
			gst_tag_list_free (tags);
		}
		else if (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_ELEMENT)
			brasero_metadata_process_element_messages (self, msg);

		gst_message_unref (msg);
	}

	g_object_unref (bus);
}

static gboolean
brasero_metadata_success (BraseroMetadata *self)
{
	BraseroMetadataPrivate *priv;

	priv = BRASERO_METADATA_PRIVATE (self);

	BRASERO_UTILS_LOG ("Metadata retrieval completed for %s", priv->info->uri);

	/* check if that's a seekable one */
	brasero_metadata_is_seekable (self);

	if (priv->silence) {
		priv->silence->end = priv->info->len;
		priv->info->silences = g_slist_append (priv->info->silences, priv->silence);
		priv->silence = NULL;
	}

	/* before leaving, check if we need a snapshot */
	if (priv->info->len > 0
	&&  priv->snapshot
	&&  priv->video_linked
	&& !priv->snapshot_started)
		return brasero_metadata_thumbnail (self);

	return brasero_metadata_completed (self);
}

static gboolean
brasero_metadata_get_duration (BraseroMetadata *self,
			       GstElement *pipeline,
			       gboolean use_duration)
{
	BraseroMetadataPrivate *priv;
	gint64 duration = -1;

	priv = BRASERO_METADATA_PRIVATE (self);

	if (!use_duration)
		gst_element_query_position (GST_ELEMENT (pipeline),
					    GST_FORMAT_TIME,
					    &duration);
	else
		gst_element_query_duration (GST_ELEMENT (pipeline),
					    GST_FORMAT_TIME,
					    &duration);

	if (duration == -1) {
		if (!priv->error) {
			gchar *name;

			BRASERO_GET_BASENAME_FOR_DISPLAY (priv->info->uri, name);
			priv->error = g_error_new (BRASERO_UTILS_ERROR,
						   BRASERO_UTILS_ERROR_GENERAL,
						   _("\"%s\" could not be handled by GStreamer."),
						   name);
			g_free (name);
		}

		return brasero_metadata_completed (self);
	}

	BRASERO_UTILS_LOG ("Found duration %lli for %s", duration, priv->info->uri);

	priv->info->len = duration;
	return brasero_metadata_success (self);
}

/**
 * This is to deal with mp3 more particularly the vbrs
 **/

static gboolean
brasero_metadata_mp3_bus_messages (GstBus *bus,
				   GstMessage *msg,
				   BraseroMetadata *self)
{
	BraseroMetadataPrivate *priv;
	gchar *debug_string = NULL;
	GError *error = NULL;

	priv = BRASERO_METADATA_PRIVATE (self);

	switch (GST_MESSAGE_TYPE (msg)) {
	case GST_MESSAGE_ERROR:
		/* save the error message */
		gst_message_parse_error (msg, &error, &debug_string);
		BRASERO_UTILS_LOG ("GStreamer error - mp3 - (%s)", debug_string);
		g_free (debug_string);
		if (!priv->error && error)
			priv->error = error;

		brasero_metadata_completed (self);
		return FALSE;

	case GST_MESSAGE_EOS:
		BRASERO_UTILS_LOG ("End of stream reached - mp3 - for %s", priv->info->uri);
		brasero_metadata_get_duration (self, priv->pipeline_mp3, FALSE);
		return FALSE;

	default:
		break;
	}

	return TRUE;
}

static gboolean
brasero_metadata_create_mp3_pipeline (BraseroMetadata *self)
{
	BraseroMetadataPrivate *priv;
	GstElement *source;
	GstElement *parse;
	GstElement *sink;
	GstBus *bus;

	priv = BRASERO_METADATA_PRIVATE (self);

	priv->pipeline_mp3 = gst_pipeline_new (NULL);

	source = gst_element_make_from_uri (GST_URI_SRC,
					    priv->info->uri,
					    NULL, NULL);
	if (!source) {
		priv->error = g_error_new (BRASERO_UTILS_ERROR,
					   BRASERO_UTILS_ERROR_GENERAL,
					   _("%s element could not be created"),
					   "\"Source\"");

		g_object_unref (priv->pipeline_mp3);
		priv->pipeline_mp3 = NULL;
		return FALSE;
	}
	gst_bin_add (GST_BIN (priv->pipeline_mp3), source);

	parse = gst_element_factory_make ("mpegaudioparse", NULL);
	if (!parse) {
		priv->error = g_error_new (BRASERO_UTILS_ERROR,
					   BRASERO_UTILS_ERROR_GENERAL,
					   _("%s element could not be created"),
					   "\"mpegaudioparse\"");

		g_object_unref (priv->pipeline_mp3);
		priv->pipeline_mp3 = NULL;
		return FALSE;
	}
	gst_bin_add (GST_BIN (priv->pipeline_mp3), parse);

	sink = gst_element_factory_make ("fakesink", NULL);
	if (!sink) {
		priv->error = g_error_new (BRASERO_UTILS_ERROR,
					   BRASERO_UTILS_ERROR_GENERAL,
					   _("%s element could not be created"),
					   "\"Fakesink\"");

		g_object_unref (priv->pipeline_mp3);
		priv->pipeline_mp3 = NULL;
		return FALSE;
	}
	gst_bin_add (GST_BIN (priv->pipeline_mp3), sink);

	/* Link */
	if (!gst_element_link_many (source, parse, sink, NULL)) {
		g_object_unref (priv->pipeline_mp3);
		priv->pipeline_mp3 = NULL;
		return FALSE;
	}

	/* Bus */
	bus = gst_pipeline_get_bus (GST_PIPELINE (priv->pipeline_mp3));
	priv->watch_mp3 = gst_bus_add_watch (bus,
					     (GstBusFunc) brasero_metadata_mp3_bus_messages,
					     self);
	gst_object_unref (bus);

	gst_element_set_state (priv->pipeline_mp3, GST_STATE_PLAYING);
	return TRUE;
}

static gboolean
brasero_metadata_success_main (BraseroMetadata *self)
{
	BraseroMetadataPrivate *priv;

	priv = BRASERO_METADATA_PRIVATE (self);

	BRASERO_UTILS_LOG ("Metadata retrieval successfully completed for %s", priv->info->uri);

	/* find the type of the file */
	brasero_metadata_get_mime_type (self);

	/* empty the bus of any pending message */
	brasero_metadata_process_pending_messages (self);

	/* get the size */
	if (brasero_metadata_is_mp3 (self)) {
		if (!brasero_metadata_create_mp3_pipeline (self)) {
			BRASERO_UTILS_LOG ("Impossible to run mp3 specific pipeline");
			return brasero_metadata_completed (self);
		}

		/* Return FALSE here not because we failed but to stop the Bus callback */
		return FALSE;
	}

	return brasero_metadata_get_duration (self, priv->pipeline, TRUE);
}

static gboolean
brasero_metadata_bus_messages (GstBus *bus,
			       GstMessage *msg,
			       BraseroMetadata *self)
{
	BraseroMetadataPrivate *priv;
	GstStateChangeReturn result;
	gchar *debug_string = NULL;
	GstTagList *tags = NULL;
	GError *error = NULL;
	GstState newstate;

	priv = BRASERO_METADATA_PRIVATE (self);

	switch (GST_MESSAGE_TYPE (msg)) {
	case GST_MESSAGE_ELEMENT:
		return brasero_metadata_process_element_messages (self, msg);

	case GST_MESSAGE_ERROR:
		/* save the error message */
		gst_message_parse_error (msg, &error, &debug_string);
		BRASERO_UTILS_LOG ("GStreamer error (%s)", debug_string);
		g_free (debug_string);
		if (!priv->error && error)
			priv->error = error;

		return brasero_metadata_completed (self);

	case GST_MESSAGE_EOS:
		BRASERO_UTILS_LOG ("End of stream reached for %s", priv->info->uri);
		return brasero_metadata_success_main (self);

	case GST_MESSAGE_TAG:
		gst_message_parse_tag (msg, &tags);
		gst_tag_list_foreach (tags, (GstTagForeachFunc) foreach_tag, self);
		gst_tag_list_free (tags);
		break;

	case GST_MESSAGE_STATE_CHANGED:
		/* when stopping the pipeline we are only interested in TAGS */
		result = gst_element_get_state (GST_ELEMENT (priv->pipeline),
						&newstate,
						NULL,
						0);

		if (result != GST_STATE_CHANGE_SUCCESS)
			break;

		if (newstate != GST_STATE_PAUSED && newstate != GST_STATE_PLAYING)
			break;

		if (!priv->snapshot_started)
			return brasero_metadata_success_main (self);

		break;

	default:
		break;
	}

	return TRUE;
}

static gboolean
brasero_metadata_create_audio_pipeline (BraseroMetadata *self)
{
	BraseroMetadataPrivate *priv;
	GstPad *audio_pad;

	priv = BRASERO_METADATA_PRIVATE (self);

	priv->audio = gst_bin_new (NULL);

	/* set up the pipeline according to flags */
	if (priv->flags & BRASERO_METADATA_FLAG_SILENCES) {
		priv->prev_level_mes = 0;

		/* Add a reference to these objects as we want to keep them
		 * around after the bin they've been added to is destroyed
		 * NOTE: now we destroy the pipeline every time which means
		 * that it doesn't really matter. */
		if (!priv->level) {
			priv->level = gst_element_factory_make ("level", NULL);
			if (!priv->level) {
				priv->error = g_error_new (BRASERO_UTILS_ERROR,
							   BRASERO_UTILS_ERROR_GENERAL,
							   _("%s element could not be created"),
							   "\"Level\"");
				gst_object_unref (priv->audio);
				priv->audio = NULL;
				return FALSE;
			}
			g_object_set (priv->level,
				      "message", TRUE,
				      "interval", (guint64) BRASERO_METADATA_SILENCE_INTERVAL,
				      NULL);
		}

		gst_object_ref (priv->convert);
		gst_object_ref (priv->level);
		gst_object_ref (priv->sink);

		gst_bin_add_many (GST_BIN (priv->audio),
				  priv->convert,
				  priv->level,
				  priv->sink,
				  NULL);

		if (!gst_element_link_many (priv->convert,
		                            priv->level,
		                            priv->sink,
		                            NULL)) {
			BRASERO_UTILS_LOG ("Impossible to link elements");
			gst_object_unref (priv->audio);
			priv->audio = NULL;
			return FALSE;
		}

		audio_pad = gst_element_get_static_pad (priv->convert, "sink");
	}
	else if (priv->flags & BRASERO_METADATA_FLAG_THUMBNAIL) {
		GstElement *queue;

		queue = gst_element_factory_make ("queue", NULL);
		gst_object_ref (priv->convert);
		gst_object_ref (priv->sink);

		gst_bin_add_many (GST_BIN (priv->audio),
				  queue,
				  priv->convert,
				  priv->sink,
				  NULL);
		if (!gst_element_link_many (queue,
		                            priv->convert,
		                            priv->sink,
		                            NULL)) {
			BRASERO_UTILS_LOG ("Impossible to link elements");
			gst_object_unref (priv->audio);
			priv->audio = NULL;
			return FALSE;
		}

		audio_pad = gst_element_get_static_pad (queue, "sink");
	}
	else {
		GstElement *queue;

		queue = gst_element_factory_make ("queue", NULL);
		gst_bin_add (GST_BIN (priv->audio), queue);

		gst_object_ref (priv->sink);
		gst_bin_add (GST_BIN (priv->audio), priv->sink);

		if (!gst_element_link (queue, priv->sink)) {
			BRASERO_UTILS_LOG ("Impossible to link elements");
			gst_object_unref (priv->audio);
			priv->audio = NULL;
			return FALSE;
		}

		audio_pad = gst_element_get_static_pad (queue, "sink");
	}

	gst_element_add_pad (priv->audio, gst_ghost_pad_new ("sink", audio_pad));
	gst_object_unref (audio_pad);

	gst_bin_add (GST_BIN (priv->pipeline), priv->audio);
	BRASERO_UTILS_LOG ("Adding audio pipeline for %s", priv->info->uri);

	return TRUE;
}

static gboolean
brasero_metadata_create_video_pipeline (BraseroMetadata *self)
{
	BraseroMetadataPrivate *priv;
	GstElement *colorspace;
	GstPad *video_pad;
	GstElement *queue;

	priv = BRASERO_METADATA_PRIVATE (self);
	priv->video = gst_bin_new (NULL);

	priv->snapshot = gst_element_factory_make ("gdkpixbufsink", NULL);
	if (!priv->snapshot) {
		gst_object_unref (priv->video);
		priv->video = NULL;

		BRASERO_UTILS_LOG ("gdkpixbufsink is not installed");
		return FALSE;
	}
	gst_bin_add (GST_BIN (priv->video), priv->snapshot);

	g_object_set (priv->snapshot,
		      "qos", FALSE,
		      "send-messages", FALSE,
		      "max-lateness", (gint64) - 1,
		      NULL);

	colorspace = gst_element_factory_make ("videoconvert", NULL);
	if (!colorspace) {
		gst_object_unref (priv->video);
		priv->video = NULL;

		BRASERO_UTILS_LOG ("videoconvert is not installed");
		return FALSE;
	}
	gst_bin_add (GST_BIN (priv->video), colorspace);

	queue = gst_element_factory_make ("queue", NULL);
	gst_bin_add (GST_BIN (priv->video), queue);

	/* link elements */
	if (!gst_element_link_many (queue,
	                            colorspace,
	                            priv->snapshot,
	                            NULL)) {
		gst_object_unref (priv->video);
		priv->video = NULL;

		BRASERO_UTILS_LOG ("Impossible to link elements");
		return FALSE;
	}

	video_pad = gst_element_get_static_pad (queue, "sink");
	gst_element_add_pad (priv->video, gst_ghost_pad_new ("sink", video_pad));
	gst_object_unref (video_pad);

	gst_bin_add (GST_BIN (priv->pipeline), priv->video);
	BRASERO_UTILS_LOG ("Adding pixbuf snapshot sink for %s", priv->info->uri);

	return TRUE;
}

static void
brasero_metadata_error_on_pad_linking (BraseroMetadata *self)
{
	BraseroMetadataPrivate *priv;
	GstMessage *message;
	GstBus *bus;

	priv = BRASERO_METADATA_PRIVATE (self);

	message = gst_message_new_error (GST_OBJECT (priv->pipeline),
					 priv->error,
					 "Sent by brasero_metadata_error_on_pad_linking");

	bus = gst_pipeline_get_bus (GST_PIPELINE (priv->pipeline));
	gst_bus_post (bus, message);
	g_object_unref (bus);
}

static gboolean
brasero_metadata_link_dummy_pad (BraseroMetadata *self,
				 GstPad *pad)
{
	BraseroMetadataPrivate *priv;
	GstElement *fakesink;
	GstPadLinkReturn res;
	GstPad *sink;

	priv = BRASERO_METADATA_PRIVATE (self);

	BRASERO_UTILS_LOG ("Linking to a fake sink");

	/* It doesn't hurt to link to a fakesink and can avoid some deadlocks.
	 * I don't know why but some demuxers in particular will lock (probably
	 * because they can't output video) if only their audio streams are
	 * linked and not their video streams (one example is dv demuxer).
	 * NOTE: there must also be a queue for audio streams. */
	fakesink = gst_element_factory_make ("fakesink", NULL);
	if (!fakesink)
		return FALSE;

	gst_bin_add (GST_BIN (priv->pipeline), fakesink);
	sink = gst_element_get_static_pad (fakesink, "sink");
	if (!sink)
		return FALSE;

	res = gst_pad_link (pad, sink);

	if (res == GST_PAD_LINK_OK) {
		gst_element_set_state (fakesink, BRASERO_METADATA_INITIAL_STATE);
		return TRUE;
	}

	return FALSE;
}

static void
brasero_metadata_audio_caps (BraseroMetadata *self,
                             GstCaps *caps)
{
	int i;
	int num_caps;
	BraseroMetadataPrivate *priv;

	priv = BRASERO_METADATA_PRIVATE (self);

	num_caps = gst_caps_get_size (caps);
	for (i = 0; i < num_caps; i++) {
		const GstStructure *structure;

		structure = gst_caps_get_structure (caps, i);
		if (!structure)
			continue;

		if (gst_structure_has_field (structure, "channels")) {
			if (gst_structure_get_field_type (structure, "channels") == G_TYPE_INT) {
				priv->info->channels = 0;
				gst_structure_get_int (structure, "channels", &priv->info->channels);

				BRASERO_UTILS_LOG ("Number of channels %i", priv->info->channels);
			}
			else if (gst_structure_get_field_type (structure, "channels") == GST_TYPE_INT_RANGE) {
				const GValue *value;

				value = gst_structure_get_value (structure, "channels");
				if (value) {
					priv->info->channels = gst_value_get_int_range_max (value);
					BRASERO_UTILS_LOG ("Number of channels %i", priv->info->channels);
				}
			}
			else if (gst_structure_get_field_type (structure, "channels") != G_TYPE_INVALID) {
				BRASERO_UTILS_LOG ("Unhandled type for channel prop %s",
				                   g_type_name (gst_structure_get_field_type (structure, "channels")));
			}
		}

		if (gst_structure_has_field (structure, "rate")) {
			if (gst_structure_get_field_type (structure, "rate") == G_TYPE_INT) {
				priv->info->rate = 0;
				gst_structure_get_int (structure, "rate", &priv->info->rate);

				BRASERO_UTILS_LOG ("Rate %i", priv->info->rate);
			}
			else if (gst_structure_get_field_type (structure, "rate") == GST_TYPE_INT_RANGE) {
				const GValue *value;

				value = gst_structure_get_value (structure, "rate");
				if (value) {
					priv->info->rate = gst_value_get_int_range_max (value);
					BRASERO_UTILS_LOG ("Rate %i", priv->info->rate);
				}
			}
			else if (gst_structure_get_field_type (structure, "rate") != G_TYPE_INVALID) {
				BRASERO_UTILS_LOG ("Unhandled type for rate prop %s",
				                   g_type_name (gst_structure_get_field_type (structure, "rate")));
			}
		}
	}
}

static void
brasero_metadata_new_decoded_pad_cb (GstElement *decode,
				     GstPad *pad,
				     BraseroMetadata *self)
{
	GstPad *sink;
	GstCaps *caps;
	const gchar *name;
	gboolean has_audio;
	gboolean has_video;
	GstPadLinkReturn res;
	GstStructure *structure;
	BraseroMetadataPrivate *priv;

	priv = BRASERO_METADATA_PRIVATE (self);

	res = GST_PAD_LINK_REFUSED;

	BRASERO_UTILS_LOG ("New pad for %s", priv->info->uri);

	/* make sure that this is audio / video */
	/* FIXME: get_current_caps() doesn't always seem to work yet here */
	caps = gst_pad_query_caps (pad, NULL);
	if (!caps) {
		g_warning ("Expected caps on decodebin pad %s", GST_PAD_NAME (pad));
		return;
	}
	structure = gst_caps_get_structure (caps, 0);
	name = gst_structure_get_name (structure);

	has_audio = (g_strrstr (name, "audio") != NULL);
	has_video = (g_strrstr (name, "video") != NULL);
	priv->info->has_audio |= has_audio;
	priv->info->has_video |= has_video;

	if (has_audio && !priv->audio_linked) {
		brasero_metadata_audio_caps (self, caps);
		brasero_metadata_create_audio_pipeline (self);
		sink = gst_element_get_static_pad (priv->audio, "sink");
		if (sink && !GST_PAD_IS_LINKED (sink)) {
			res = gst_pad_link (pad, sink);
			BRASERO_UTILS_LOG ("Audio stream link %i for %s", res, priv->info->uri);
			gst_object_unref (sink);

			priv->audio_linked = (res == GST_PAD_LINK_OK);
			gst_element_set_state (priv->audio, BRASERO_METADATA_INITIAL_STATE);
		}
	}

	if (!strcmp (name, "video/x-raw") && !priv->video_linked) {
		BRASERO_UTILS_LOG ("RAW video stream found");

		if (!priv->video && (priv->flags & BRASERO_METADATA_FLAG_THUMBNAIL)) {
			/* we shouldn't error out if we can't create a video
			 * pipeline (mostly used for snapshots) */
			/* FIXME: we should nevertheless tell the user what
			 * plugin he is missing. */
			if (!brasero_metadata_create_video_pipeline (self)) {
				BRASERO_UTILS_LOG ("Impossible to create video pipeline");

				gst_caps_unref (caps);

				if (!brasero_metadata_link_dummy_pad (self, pad))
					brasero_metadata_error_on_pad_linking (self);

				return;
			}

			sink = gst_element_get_static_pad (priv->video, "sink");
			if (!sink || GST_PAD_IS_LINKED (sink)) {
				gst_object_unref (sink);
				gst_caps_unref (caps);
				return;
			}

			res = gst_pad_link (pad, sink);
			priv->video_linked = (res == GST_PAD_LINK_OK);
			gst_object_unref (sink);

			gst_element_set_state (priv->video, BRASERO_METADATA_INITIAL_STATE);

			BRASERO_UTILS_LOG ("Video stream link %i for %s", res, priv->info->uri);
		}
		else if (!brasero_metadata_link_dummy_pad (self, pad))
			brasero_metadata_error_on_pad_linking (self);
	}
	else if (has_video && !brasero_metadata_link_dummy_pad (self, pad))
		brasero_metadata_error_on_pad_linking (self);

	gst_caps_unref (caps);
}

static gboolean
brasero_metadata_create_pipeline (BraseroMetadata *self)
{
	BraseroMetadataPrivate *priv;

	priv = BRASERO_METADATA_PRIVATE (self);

	priv->pipeline = gst_pipeline_new (NULL);

	priv->decode = gst_element_factory_make ("decodebin", NULL);
	if (priv->decode == NULL) {
		priv->error = g_error_new (BRASERO_UTILS_ERROR,
					   BRASERO_UTILS_ERROR_GENERAL,
					   _("%s element could not be created"),
					   "\"Decodebin\"");
		return FALSE;
	}
	g_signal_connect (G_OBJECT (priv->decode), "pad-added",
			  G_CALLBACK (brasero_metadata_new_decoded_pad_cb),
			  self);

	gst_bin_add (GST_BIN (priv->pipeline), priv->decode);

	/* the two following objects don't always run */
	priv->convert = gst_element_factory_make ("audioconvert", NULL);
	if (!priv->convert) {
		priv->error = g_error_new (BRASERO_UTILS_ERROR,
					   BRASERO_UTILS_ERROR_GENERAL,
					   _("%s element could not be created"),
					   "\"Audioconvert\"");
		return FALSE;
	}

	priv->sink = gst_element_factory_make ("fakesink", NULL);
	if (priv->sink == NULL) {
		priv->error = g_error_new (BRASERO_UTILS_ERROR,
					   BRASERO_UTILS_ERROR_GENERAL,
					   _("%s element could not be created"),
					   "\"Fakesink\"");
		return FALSE;
	}

	return TRUE;
}

static gboolean
brasero_metadata_set_new_uri (BraseroMetadata *self,
			      const gchar *uri)
{
	BraseroMetadataPrivate *priv;
	GstBus *bus;

	priv = BRASERO_METADATA_PRIVATE (self);

	BRASERO_UTILS_LOG ("New retrieval for %s %p", uri, self);

	if (priv->error) {
		g_error_free (priv->error);
		priv->error = NULL;
	}

	brasero_metadata_info_free (priv->info);
	priv->info = NULL;

	if (priv->silence) {
		g_free (priv->silence);
		priv->silence = NULL;
	}

	priv->info = g_new0 (BraseroMetadataInfo, 1);
	priv->info->uri = g_strdup (uri);

	if (priv->pipeline){
		gst_element_set_state (priv->pipeline, GST_STATE_NULL);
		if (priv->source) {
			gst_bin_remove (GST_BIN (priv->pipeline), priv->source);
			priv->source = NULL;
		}

		if (priv->audio) {
			gst_bin_remove (GST_BIN (priv->pipeline), priv->audio);
			priv->audio = NULL;
		}

		if (priv->video) {
			gst_bin_remove (GST_BIN (priv->pipeline), priv->video);
			priv->snapshot = NULL;
			priv->video = NULL;
		}
	}
	else if (!brasero_metadata_create_pipeline (self))
		return FALSE;

	if (!gst_uri_is_valid (uri))
		return FALSE;

	priv->video_linked = 0;
	priv->audio_linked = 0;
	priv->snapshot_started = 0;

	/* create a necessary source */
	priv->source = gst_element_make_from_uri (GST_URI_SRC,
						  uri,
						  NULL, NULL);
	if (!priv->source) {
		priv->error = g_error_new (BRASERO_UTILS_ERROR,
					   BRASERO_UTILS_ERROR_GENERAL,
					   "Can't create file source");
		return FALSE;
	}

	gst_bin_add (GST_BIN (priv->pipeline), priv->source);
	gst_element_link (priv->source, priv->decode);

	/* apparently we need to reconnect to the bus every time */
	if (priv->watch)
		g_source_remove (priv->watch);

	bus = gst_pipeline_get_bus (GST_PIPELINE (priv->pipeline));
	priv->watch = gst_bus_add_watch (bus,
					 (GstBusFunc) brasero_metadata_bus_messages,
					 self);
	gst_object_unref (bus);

	return TRUE;
}

gboolean
brasero_metadata_set_uri (BraseroMetadata *self,
			  BraseroMetadataFlag flags,
			  const gchar *uri,
			  GError **error)
{
	GstStateChangeReturn state_change;
	BraseroMetadataPrivate *priv;

	priv = BRASERO_METADATA_PRIVATE (self);

	g_mutex_lock (priv->mutex);

	priv->flags = flags;
	if (!brasero_metadata_set_new_uri (self, uri)) {
		if (priv->error) {
			BRASERO_UTILS_LOG ("Failed to set new URI %s", priv->error->message);
			g_propagate_error (error, priv->error);
			priv->error = NULL;
		}

		brasero_metadata_info_free (priv->info);
		priv->info = NULL;

		g_mutex_unlock (priv->mutex);
		return FALSE;
	}

	priv->started = 1;
	state_change = gst_element_set_state (GST_ELEMENT (priv->pipeline),
					      BRASERO_METADATA_INITIAL_STATE);

	g_mutex_unlock (priv->mutex);

	if (state_change == GST_STATE_CHANGE_FAILURE)
		brasero_metadata_stop (self);

	return (state_change != GST_STATE_CHANGE_FAILURE);
}

gboolean
brasero_metadata_get_info_async (BraseroMetadata *self,
				 const gchar *uri,
				 BraseroMetadataFlag flags)
{
	BraseroMetadataPrivate *priv;
	GstStateChangeReturn state_change;

	priv = BRASERO_METADATA_PRIVATE (self);

	priv->flags = flags;

	if (!brasero_metadata_set_new_uri (self, uri)) {
		g_object_ref (self);
		g_signal_emit (G_OBJECT (self),
			       brasero_metadata_signals [COMPLETED_SIGNAL],
			       0,
			       priv->error);
		g_object_unref (self);

		if (priv->error) {
			BRASERO_UTILS_LOG ("Failed to set new URI %s", priv->error->message);
			g_error_free (priv->error);
			priv->error = NULL;
		}
		return FALSE;
	}

	state_change = gst_element_set_state (GST_ELEMENT (priv->pipeline),
					      BRASERO_METADATA_INITIAL_STATE);

	priv->started = (state_change != GST_STATE_CHANGE_FAILURE);
	return priv->started;
}

static void
brasero_metadata_wait_cancelled (GCancellable *cancel,
				 GCond *condition)
{
	BRASERO_UTILS_LOG ("Thread waiting for retrieval end cancelled");
	g_cond_broadcast (condition);
}

void
brasero_metadata_wait (BraseroMetadata *self,
		       GCancellable *cancel)
{
	BraseroMetadataPrivate *priv;
	GCond *condition;
	gulong sig;

	priv = BRASERO_METADATA_PRIVATE (self);

	BRASERO_UTILS_LOG ("Metadata lock and wait %p", self);

	g_mutex_lock (priv->mutex);

	if (!priv->started) {
		/* Maybe we were waiting for the lock which was held by the 
		 * finish function. That's why we check if it didn't finish in
		 * the mean time. */
		g_mutex_unlock (priv->mutex);
		return;
	}

	condition = g_cond_new ();
	priv->conditions = g_slist_prepend (priv->conditions, condition);

	sig = g_signal_connect (cancel,
				"cancelled",
				G_CALLBACK (brasero_metadata_wait_cancelled),
				condition);

	if (!g_cancellable_is_cancelled (cancel))
		g_cond_wait (condition, priv->mutex);

	priv->conditions = g_slist_remove (priv->conditions, condition);
	g_cond_free (condition);

	g_mutex_unlock (priv->mutex);

	g_signal_handler_disconnect (cancel, sig);
}

void
brasero_metadata_increase_listener_number (BraseroMetadata *self)
{
	BraseroMetadataPrivate *priv;

	priv = BRASERO_METADATA_PRIVATE (self);
	g_atomic_int_inc (&priv->listeners);
}

gboolean
brasero_metadata_decrease_listener_number (BraseroMetadata *self)
{
	BraseroMetadataPrivate *priv;

	priv = BRASERO_METADATA_PRIVATE (self);
	return g_atomic_int_dec_and_test (&priv->listeners);
}

const gchar *
brasero_metadata_get_uri (BraseroMetadata *self)
{
	BraseroMetadataPrivate *priv;

	priv = BRASERO_METADATA_PRIVATE (self);
	return priv->info?priv->info->uri:NULL;
}

BraseroMetadataFlag
brasero_metadata_get_flags (BraseroMetadata *self)
{
	BraseroMetadataPrivate *priv;

	priv = BRASERO_METADATA_PRIVATE (self);
	return priv->flags;
}

gboolean
brasero_metadata_get_result (BraseroMetadata *self,
			     BraseroMetadataInfo *info,
			     GError **error)
{
	BraseroMetadataPrivate *priv;

	priv = BRASERO_METADATA_PRIVATE (self);

	if (priv->error) {
		if (error)
			*error = g_error_copy (priv->error);

		return FALSE;
	}

	if (!priv->info)
		return FALSE;

	if (priv->started)
		return FALSE;

	memset (info, 0, sizeof (BraseroMetadataInfo));
	brasero_metadata_info_copy (info, priv->info);
	return TRUE;
}

static void
brasero_metadata_init (BraseroMetadata *obj)
{
	BraseroMetadataPrivate *priv;

	priv = BRASERO_METADATA_PRIVATE (obj);

	priv->mutex = g_mutex_new ();
}

static void
brasero_metadata_finalize (GObject *object)
{
	BraseroMetadataPrivate *priv;
	GSList *iter;

	priv = BRASERO_METADATA_PRIVATE (object);

	brasero_metadata_destroy_pipeline (BRASERO_METADATA (object));

	if (priv->silence) {
		g_free (priv->silence);
		priv->silence = NULL;
	}

	if (priv->error) {
		g_error_free (priv->error);
		priv->error = NULL;
	}

	if (priv->watch) {
		g_source_remove (priv->watch);
		priv->watch = 0;
	}

	if (priv->info) {
		brasero_metadata_info_free (priv->info);
		priv->info = NULL;
	}

	for (iter = priv->conditions; iter; iter = iter->next) {
		GCond *condition;

		condition = iter->data;
		g_cond_broadcast (condition);
		g_cond_free (condition);
	}
	g_slist_free (priv->conditions);
	priv->conditions = NULL;

	if (priv->mutex) {
		g_mutex_free (priv->mutex);
		priv->mutex = NULL;
	}

	G_OBJECT_CLASS (brasero_metadata_parent_class)->finalize (object);
}

static void
brasero_metadata_get_property (GObject *obj,
			       guint prop_id,
			       GValue *value,
			       GParamSpec *pspec)
{
	gchar *uri;
	BraseroMetadata *self;
	BraseroMetadataPrivate *priv;

	self = BRASERO_METADATA (obj);
	priv = BRASERO_METADATA_PRIVATE (self);

	switch (prop_id) {
	case PROP_URI:
		g_object_get (G_OBJECT (priv->source), "location",
			      &uri, NULL);
		g_value_set_string (value, uri);
		g_free (uri);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
brasero_metadata_set_property (GObject *obj,
			       guint prop_id,
			       const GValue *value,
			       GParamSpec *pspec)
{
	const gchar *uri;
	BraseroMetadata *self;
	BraseroMetadataPrivate *priv;

	self = BRASERO_METADATA (obj);
	priv = BRASERO_METADATA_PRIVATE (self);

	switch (prop_id) {
	case PROP_URI:
		uri = g_value_get_string (value);
		gst_element_set_state (GST_ELEMENT (priv->pipeline), GST_STATE_NULL);
		if (priv->source)
			g_object_set (G_OBJECT (priv->source),
				      "location", uri,
				      NULL);
		gst_element_set_state (GST_ELEMENT (priv->pipeline), GST_STATE_PAUSED);
		priv->started = 1;
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
brasero_metadata_class_init (BraseroMetadataClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroMetadataPrivate));

	object_class->finalize = brasero_metadata_finalize;
	object_class->set_property = brasero_metadata_set_property;
	object_class->get_property = brasero_metadata_get_property;

	brasero_metadata_signals[COMPLETED_SIGNAL] =
	    g_signal_new ("completed",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (BraseroMetadataClass,
					   completed),
			  NULL, NULL,
			  g_cclosure_marshal_VOID__POINTER,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_POINTER);
	g_object_class_install_property (object_class,
					 PROP_URI,
					 g_param_spec_string ("uri",
							      "The uri of the song",
							      "The uri of the song",
							      NULL,
							      G_PARAM_READWRITE));
}

BraseroMetadata *
brasero_metadata_new (void)
{
	return BRASERO_METADATA (g_object_new (BRASERO_TYPE_METADATA, NULL));
}
