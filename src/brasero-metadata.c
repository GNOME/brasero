/***************************************************************************
 *            metadata.c
 *
 *  jeu jui 28 12:49:41 2005
 *  Copyright  2005  Philippe Rouquier
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

#include <string.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib-object.h>

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>
#include <gst/tag/tag.h>

#include "brasero-metadata.h"
#include "brasero-utils.h"
#include "burn-debug.h"


G_DEFINE_TYPE(BraseroMetadata, brasero_metadata, G_TYPE_OBJECT)

#define BRASERO_METADATA_SILENCE_INTERVAL		100000000

struct BraseroMetadataPrivate {
	GstElement *pipeline;
	GstElement *source;
	GstElement *decode;
	GstElement *convert;
	GstElement *level;
	GstElement *sink;
	GstElement *first;

	GMainLoop *loop;
	GError *error;
	guint watch;

	guint progress_id;

	BraseroMetadataSilence *silence;

	BraseroMetadataFlag flags;
	BraseroMetadataInfo *info;

	GMutex *mutex;
	GCond *cond;

	guint started:1;
	guint moved_forward:1;
	guint prev_level_mes:1;
};
typedef struct BraseroMetadataPrivate BraseroMetadataPrivate;
#define BRASERO_METADATA_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), BRASERO_TYPE_METADATA, BraseroMetadataPrivate))

enum {
	PROP_NONE,
	PROP_URI
};

typedef enum {
	COMPLETED_SIGNAL,
	PROGRESS_CHANGED_SIGNAL,
	LAST_SIGNAL
} BraseroMetadataSignalType;

static guint brasero_metadata_signals [LAST_SIGNAL] = { 0 };

#define BRASERO_METADATA_IS_FAST(flags) 					\
	(!((flags) & BRASERO_METADATA_FLAG_SILENCES) &&				\
	((flags) & BRASERO_METADATA_FLAG_FAST))

static GObjectClass *parent_class = NULL;

void
brasero_metadata_info_clear (BraseroMetadataInfo *info)
{
	if (!info)
		return;

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

static void
brasero_metadata_stop (BraseroMetadata *self)
{
	BraseroMetadataPrivate *priv;

	priv = BRASERO_METADATA_PRIVATE (self);

	if (priv->pipeline)
		gst_element_set_state (priv->pipeline, GST_STATE_NULL);

	if (priv->progress_id) {
		g_source_remove (priv->progress_id);
		priv->progress_id = 0;
	}

	if (priv->watch) {
		g_source_remove (priv->watch);
		priv->watch = 0;
	}

	/* stop the pipeline */
	priv->started = 0;

	/* that's for sync_wait */
	g_mutex_lock (priv->mutex);
	g_cond_signal (priv->cond);
	g_mutex_unlock (priv->mutex);

	/* stop loop */
	if (priv->loop && g_main_loop_is_running (priv->loop))
		g_main_loop_quit (priv->loop);
}

void
brasero_metadata_cancel (BraseroMetadata *self)
{
	BraseroMetadataPrivate *priv;

	priv = BRASERO_METADATA_PRIVATE (self);

	brasero_metadata_stop (self);

	if (priv->error) {
		g_error_free (priv->error);
		priv->error = NULL;
	}
}

static gint
brasero_metadata_report_progress (BraseroMetadata *self)
{
	gdouble progress;
	gint64 position = -1;
	gint64 duration = -1;
	BraseroMetadataPrivate *priv;
	GstFormat format = GST_FORMAT_BYTES;

	priv = BRASERO_METADATA_PRIVATE (self);
	if (!gst_element_query_duration (priv->pipeline, &format, &duration))
		return TRUE;

	if (!gst_element_query_position (priv->pipeline, &format, &position))
		return TRUE;

	progress = position / duration;
	g_signal_emit (self,
		       brasero_metadata_signals [PROGRESS_CHANGED_SIGNAL],
		       0,
		       progress);

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
	query = gst_query_new_seeking (GST_FORMAT_DEFAULT);
	if (!gst_element_query (priv->source, query))
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

static gboolean
brasero_metadata_get_mime_type (BraseroMetadata *self)
{
	BraseroMetadataPrivate *priv;
	GstElement *typefind;
	GstCaps *caps = NULL;
	GstElement *decode;
	const gchar *mime;

	priv = BRASERO_METADATA_PRIVATE (self);

	if (priv->info->type) {
		g_free (priv->info->type);
		priv->info->type = NULL;
	}

	/* find the type of the file */
	decode = gst_bin_get_by_name (GST_BIN (priv->pipeline),
				      "decode");
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

	if (!strcmp (mime, "application/x-id3"))
		priv->info->type = g_strdup ("audio/mpeg");
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

static gboolean
brasero_metadata_completed (BraseroMetadata *self)
{
	BraseroMetadataPrivate *priv;

	priv = BRASERO_METADATA_PRIVATE (self);

	if ((!priv->loop || !g_main_loop_is_running (priv->loop)) && !priv->cond) {
		/* we send a message only if we haven't got a loop (= async mode) */
		g_object_ref (self);
		g_signal_emit (G_OBJECT (self),
			       brasero_metadata_signals [COMPLETED_SIGNAL],
			       0,
			       priv->error);
		g_object_unref (self);
	}

	brasero_metadata_stop (self);
	return TRUE;
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
		gst_tag_list_get_int (list, tag, &(priv->info->isrc));
	}
	else if (!strcmp (tag, GST_TAG_MUSICBRAINZ_TRACKID)) {
		gst_tag_list_get_string (list, tag, &(priv->info->musicbrainz_id));
	}
}

static void
brasero_metadata_process_pending_tag_messages (BraseroMetadata *self)
{
	GstBus *bus;
	GstMessage *msg;
	BraseroMetadataPrivate *priv;

	priv = BRASERO_METADATA_PRIVATE (self);

	bus = gst_pipeline_get_bus (GST_PIPELINE (priv->pipeline));
	while ((msg = gst_bus_pop_filtered (bus, GST_MESSAGE_TAG))) {
		GstTagList *tags = NULL;

		gst_message_parse_tag (msg, &tags);
		gst_tag_list_foreach (tags, (GstTagForeachFunc) foreach_tag, self);
		gst_tag_list_free (tags);

		gst_message_unref (msg);
	}

	g_object_unref (bus);
}

static gboolean
brasero_metadata_success (BraseroMetadata *self)
{
	GstFormat format = GST_FORMAT_TIME;
	BraseroMetadataPrivate *priv;
	gint64 duration = -1;

	priv = BRASERO_METADATA_PRIVATE (self);

	/* find the type of the file */
	brasero_metadata_get_mime_type (self);

	/* get the size */
	if (!BRASERO_METADATA_IS_FAST (priv->flags)
	&&   brasero_metadata_is_mp3 (self))
		gst_element_query_position (GST_ELEMENT (priv->sink),
					    &format,
					    &duration);

	if (duration == -1)
		gst_element_query_duration (GST_ELEMENT (priv->pipeline),
					    &format,
					    &duration);

	if (duration == -1) {
		if (!priv->error) {
			priv->error = g_error_new (BRASERO_ERROR,
						   BRASERO_ERROR_GENERAL,
						   _("this format is not supported by gstreamer"));
		}

		return brasero_metadata_completed (self);
	}

	BRASERO_BURN_LOG ("found duration %lli", duration);

	priv->info->len = duration;

	/* check if that's a seekable one */
	brasero_metadata_is_seekable (self);

	if (priv->silence) {
		priv->silence->end = duration;
		priv->info->silences = g_slist_append (priv->info->silences, priv->silence);
		priv->silence = NULL;
	}

	/* empty the bus of any pending message */
	brasero_metadata_process_pending_tag_messages (self);

	return brasero_metadata_completed (self);
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
		if (!strcmp (gst_structure_get_name (msg->structure), "level")
		&&   gst_structure_has_field (msg->structure, "peak")) {
			const GValue *value;
			const GValue *list;
			gdouble peak;

			list = gst_structure_get_value (msg->structure, "peak");
			value = gst_value_list_get_value (list, 0);
			peak = g_value_get_double (value);

			/* detection of silence */
			if (peak < -50.0) {
				gint64 pos = -1;
				GstFormat format = GST_FORMAT_TIME;
	
				/* was there a silence last time we check ?
				 * NOTE: if that's the first signal we receive
				 * then consider that silence started from 0 */
				gst_element_query_position (priv->pipeline, &format, &pos);
				if (pos == -1) {
					BRASERO_BURN_LOG ("impossible to retrieve position");
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

				BRASERO_BURN_LOG ("silence detected at %lli", pos);
			}
			else if (priv->silence) {
				BRASERO_BURN_LOG ("silence finished");

				priv->info->silences = g_slist_append (priv->info->silences,
								       priv->silence);
				priv->silence = NULL;
			}
			priv->prev_level_mes = 1;
		}
		break;

	case GST_MESSAGE_ERROR:
		gst_message_parse_error (msg, &error, &debug_string);
		BRASERO_BURN_LOG (debug_string);
		g_free (debug_string);

		priv->error = error;
		brasero_metadata_completed (self);
		break;

	case GST_MESSAGE_EOS:
		brasero_metadata_success (self);
		break;

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

		if ((priv->flags & BRASERO_METADATA_FLAG_SILENCES)
		||   brasero_metadata_is_mp3 (self)) {
			gst_element_set_state (priv->pipeline, GST_STATE_PLAYING);

			if (BRASERO_METADATA_IS_FAST (priv->flags)
			&& !priv->moved_forward) {
				/* we try to go forward. This allows us to avoid
				 * reading the whole file till the end. The byte
				 * format is really important here as time
				 * format needs calculation and is dead slow. 
				 * The number of bytes is important to reach 
				 * the end. */
				gst_element_seek (priv->pipeline,
						  1.0,
						  GST_FORMAT_BYTES,
						  GST_SEEK_FLAG_FLUSH,
						  GST_SEEK_TYPE_SET,
						  52428800,
						  GST_SEEK_TYPE_NONE,
						  GST_CLOCK_TIME_NONE);
				priv->moved_forward = 1;
			}

			if (!priv->progress_id)
				priv->progress_id = g_timeout_add (500,
								   (GSourceFunc) brasero_metadata_report_progress,
								   self);

			break;
		}

		brasero_metadata_success (self);
		break;

	default:
		break;
	}

	return TRUE;
}

static void
brasero_metadata_new_decoded_pad_cb (GstElement *decode,
				     GstPad *pad,
				     gboolean arg2,
				     BraseroMetadata *self)
{
	GstPad *sink;
	GstCaps *caps;
	GstPadLinkReturn res;
	GstStructure *structure;
	BraseroMetadataPrivate *priv;

	priv = BRASERO_METADATA_PRIVATE (self);

	res = GST_PAD_LINK_REFUSED;
	BRASERO_BURN_LOG ("new pad");
	sink = gst_element_get_pad (priv->first, "sink");
	if (GST_PAD_IS_LINKED (sink))
		return;

	/* make sure that this is audio / video */
	caps = gst_pad_get_caps (pad);
	structure = gst_caps_get_structure (caps, 0);
	if (structure) {
		const gchar *name;

		name = gst_structure_get_name (structure);
		priv->info->has_audio = (g_strrstr (name, "audio") != NULL);
		priv->info->has_video = (g_strrstr (name, "video") != NULL);

		if (priv->info->has_audio || priv->info->has_video)
			res = gst_pad_link (pad, sink);
	}

	gst_object_unref (sink);
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
		priv->error = g_error_new (BRASERO_ERROR,
					   BRASERO_ERROR_GENERAL,
					   "decode can't be created");
		return FALSE;
	}
	g_signal_connect (G_OBJECT (priv->decode), "new-decoded-pad",
			  G_CALLBACK (brasero_metadata_new_decoded_pad_cb),
			  self);

	gst_bin_add (GST_BIN (priv->pipeline), priv->decode);

	/* the two following objects don't always run */
	priv->convert = gst_element_factory_make ("audioconvert", NULL);
	if (!priv->convert) {
		priv->error = g_error_new (BRASERO_ERROR,
					   BRASERO_ERROR_GENERAL,
					   "Can't create audioconvert");
		return FALSE;
	}

	priv->level = gst_element_factory_make ("level", NULL);
	if (!priv->level) {
		priv->error = g_error_new (BRASERO_ERROR,
					   BRASERO_ERROR_GENERAL,
					   "Can't create level");
		return FALSE;
	}
	g_object_set (priv->level,
		      "message", TRUE,
		      "interval", (guint64) BRASERO_METADATA_SILENCE_INTERVAL,
		      NULL);

	priv->sink = gst_element_factory_make ("fakesink", NULL);
	if (priv->sink == NULL) {
		priv->error = g_error_new (BRASERO_ERROR,
					   BRASERO_ERROR_GENERAL,
					   "Can't create fake sink");
		return FALSE;
	}
	gst_bin_add (GST_BIN (priv->pipeline), priv->sink);

	return TRUE;
}

static gboolean
brasero_metadata_set_new_uri (BraseroMetadata *self,
			      const gchar *uri)
{
	BraseroMetadataPrivate *priv;
	GstBus *bus;

	priv = BRASERO_METADATA_PRIVATE (self);

	brasero_metadata_info_free (priv->info);
	priv->info = NULL;

	if (priv->silence) {
		g_free (priv->silence);
		priv->silence = NULL;
	}

	if (priv->progress_id) {
		g_source_remove (priv->progress_id);
		priv->progress_id = 0;
	}

	priv->info = g_new0 (BraseroMetadataInfo, 1);
	priv->info->uri = g_strdup (uri);

	if (priv->pipeline){
		gst_element_set_state (priv->pipeline, GST_STATE_NULL);
		if (priv->source) {
			gst_bin_remove (GST_BIN (priv->pipeline), priv->source);
			priv->source = NULL;
		}
	}
	else if (!brasero_metadata_create_pipeline (self))
		return FALSE;

	if (!gst_uri_is_valid (uri))
		return FALSE;

	/* set up the pipeline according to flags */
	if (priv->flags & BRASERO_METADATA_FLAG_SILENCES) {
		priv->prev_level_mes = 0;
		if (priv->first != priv->convert) {
			gst_bin_add_many (GST_BIN (priv->pipeline),
					  priv->convert,
					  priv->level,
					  NULL);
			gst_element_link_many (priv->convert,
					       priv->level,
					       priv->sink,
					       NULL);

			priv->first = priv->convert;
		}
	}
	else {
		if (priv->first == priv->convert) {
			gst_object_ref (priv->convert);
			gst_object_ref (priv->level);

			gst_bin_remove_many (GST_BIN (priv->pipeline),
					     priv->convert,
					     priv->level,
					     NULL);
		}

		if (priv->first != priv->sink)
			priv->first = priv->sink;
	}

	/* create a necessary source */
	priv->source = gst_element_make_from_uri (GST_URI_SRC,
						  uri,
						  NULL);
	if (!priv->source) {
		priv->error = g_error_new (BRASERO_ERROR,
					   BRASERO_ERROR_GENERAL,
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

static void
brasero_metadata_cancelled_cb (GCancellable *cancel,
			       BraseroMetadata *self)
{
	brasero_metadata_cancel (self);
}

gboolean
brasero_metadata_get_info_wait (BraseroMetadata *self,
				GCancellable *cancel,
				const gchar *uri,
				BraseroMetadataFlag flags,
				GError **error)
{
	BraseroMetadataPrivate *priv;
	gulong cancel_signal = 0;

	priv = BRASERO_METADATA_PRIVATE (self);

	priv->flags = flags;
	if (!brasero_metadata_set_new_uri (self, uri)) {
		g_propagate_error (error, priv->error);
		priv->error = NULL;

		brasero_metadata_info_free (priv->info);
		priv->info = NULL;

		return FALSE;
	}

	g_mutex_lock (priv->mutex);

	cancel_signal = g_signal_connect (cancel,
					  "cancelled",
					  G_CALLBACK (brasero_metadata_cancelled_cb),
					  self);

	/* Now wait ... but check a last time otherwise we wouldn't get the
	 * any notice of cancellation if it had been cancelled before we 
	 * connected to the signal */
	if (g_cancellable_is_cancelled (cancel)) {
		g_signal_handler_disconnect (cancel, cancel_signal);
		g_mutex_unlock (priv->mutex);

		brasero_metadata_stop (self);
		brasero_metadata_info_free (priv->info);
		priv->info = NULL;

		return FALSE;
	}

	priv->started = 1;
	gst_element_set_state (GST_ELEMENT (priv->pipeline), GST_STATE_PLAYING);
	g_cond_wait (priv->cond, priv->mutex);
	g_mutex_unlock (priv->mutex);

	g_signal_handler_disconnect (cancel, cancel_signal);

	if (priv->error) {
		if (error) {
			g_propagate_error (error, priv->error);
			priv->error = NULL;
		} 
		else {
			BRASERO_BURN_LOG ("ERROR getting metadata : %s\n", priv->error->message);
			g_error_free (priv->error);
			priv->error = NULL;
		}

		return FALSE;
	}

	return TRUE;
}

gboolean
brasero_metadata_get_info_sync (BraseroMetadata *self,
				const gchar *uri,
				BraseroMetadataFlag flags,
				GError **error)
{
	BraseroMetadataPrivate *priv;

	priv = BRASERO_METADATA_PRIVATE (self);

	priv->flags = flags;

	if (!brasero_metadata_set_new_uri (self, uri)) {
		g_propagate_error (error, priv->error);
		priv->error = NULL;

		brasero_metadata_info_free (priv->info);
		priv->info = NULL;

		return FALSE;
	}

	priv->started = 1;
	gst_element_set_state (GST_ELEMENT (priv->pipeline), GST_STATE_PLAYING);

	/* run the loop until it's finished */
	priv->loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (priv->loop);
	g_main_loop_unref (priv->loop);
	priv->loop = NULL;

	if (priv->error) {
		if (error) {
			g_propagate_error (error, priv->error);
			priv->error = NULL;
		} 
		else {
			BRASERO_BURN_LOG ("ERROR getting metadata : %s\n", priv->error->message);
			g_error_free (priv->error);
			priv->error = NULL;
		}

		return FALSE;
	}

	return TRUE;
}

gboolean
brasero_metadata_get_info_async (BraseroMetadata *self,
				 const gchar *uri,
				 BraseroMetadataFlag flags)
{
	BraseroMetadataPrivate *priv;

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
			g_error_free (priv->error);
			priv->error = NULL;
		}
		return FALSE;
	}

	priv->started = 1;
	gst_element_set_state (GST_ELEMENT (priv->pipeline), GST_STATE_PLAYING);

	return TRUE;
}

void
brasero_metadata_info_copy (BraseroMetadataInfo *dest,
			    BraseroMetadataInfo *src)
{
	GSList *iter;

	if (!dest || !src)
		return;

	dest->isrc = src->isrc;
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

	for (iter = src->silences; iter; iter = iter->next) {
		BraseroMetadataSilence *silence, *copy;

		silence = iter->data;

		copy = g_new0 (BraseroMetadataSilence, 1);
		copy->start = silence->start;
		copy->end = silence->end;

		dest->silences = g_slist_append (dest->silences, copy);
	}

}

gboolean
brasero_metadata_set_info (BraseroMetadata *self,
			   BraseroMetadataInfo *info)
{
	BraseroMetadataPrivate *priv;

	priv = BRASERO_METADATA_PRIVATE (self);

	if (!priv->info)
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

	priv->cond = g_cond_new ();
	priv->mutex = g_mutex_new ();
}

static void
brasero_metadata_destroy_pipeline (BraseroMetadata *self)
{
	GstStateChangeReturn change;
	BraseroMetadataPrivate *priv;

	priv = BRASERO_METADATA_PRIVATE (self);

	priv->started = 0;

	if (!priv->pipeline)
		return;

	/* better to wait for the state change to be completed */
	change = gst_element_set_state (GST_ELEMENT (priv->pipeline),
					GST_STATE_READY);

	while (change == GST_STATE_CHANGE_ASYNC) {
		GstState state;
		GstState pending;

		change = gst_element_get_state (priv->pipeline,
						&state,
						&pending,
						GST_MSECOND);
	};
	
	change = gst_element_set_state (GST_ELEMENT (priv->pipeline),
					GST_STATE_NULL);

	while (change == GST_STATE_CHANGE_ASYNC) {
		GstState state;
		GstState pending;

		change = gst_element_get_state (priv->pipeline,
						&state,
						&pending,
						GST_MSECOND);
	}

	if (change == GST_STATE_CHANGE_FAILURE)
		g_warning ("State change failure\n");

	gst_object_unref (GST_OBJECT (priv->pipeline));
	priv->pipeline = NULL;

	if (priv->first == priv->sink) {
		gst_object_unref (GST_OBJECT (priv->convert));
		priv->convert = NULL;

		gst_object_unref (GST_OBJECT (priv->level));
		priv->level = NULL;
	}
}

static void
brasero_metadata_finalize (GObject *object)
{
	BraseroMetadataPrivate *priv;

	priv = BRASERO_METADATA_PRIVATE (object);

	brasero_metadata_destroy_pipeline (BRASERO_METADATA (object));

	if (priv->silence) {
		g_free (priv->silence);
		priv->silence = NULL;
	}

	if (priv->progress_id) {
		g_source_remove (priv->progress_id);
		priv->progress_id = 0;
	}

	if (priv->error) {
		g_error_free (priv->error);
		priv->error = NULL;
	}

	if (priv->watch) {
		g_source_remove (priv->watch);
		priv->watch = 0;
	}

	if (priv->loop) {
		if (g_main_loop_is_running (priv->loop))
			g_main_loop_quit (priv->loop);

		g_main_loop_unref (priv->loop);
		priv->loop = NULL;
	}

	if (priv->info) {
		brasero_metadata_info_free (priv->info);
		priv->info = NULL;
	}

	if (priv->mutex) {
		g_mutex_free (priv->mutex);
		priv->mutex = NULL;
	}

	if (priv->cond) {
		g_cond_free (priv->cond);
		priv->cond = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
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

	parent_class = g_type_class_peek_parent (klass);

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
	brasero_metadata_signals[PROGRESS_CHANGED_SIGNAL] =
	    g_signal_new ("progress",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (BraseroMetadataClass,
					   progress),
			  NULL, NULL,
			  g_cclosure_marshal_VOID__DOUBLE,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_DOUBLE);
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
