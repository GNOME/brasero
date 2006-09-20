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

#include <string.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib-object.h>

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>
#include <gst/tag/tag.h>

#include "metadata.h"
#include "utils.h"

extern gint debug;

static void brasero_metadata_class_init (BraseroMetadataClass *klass);
static void brasero_metadata_init (BraseroMetadata *sp);
static void brasero_metadata_finalize (GObject *object);

static void brasero_metadata_get_property (GObject *obj,
					   guint prop_id,
					   GValue *value,
					   GParamSpec *pspec);
static void brasero_metadata_set_property (GObject *obj,
					   guint prop_id,
					   const GValue *value,
					   GParamSpec *pspec);

struct BraseroMetadataPrivate {
	GstElement *pipeline;
	GstElement *source;
	GstElement *decode;
	GstElement *sink;

	GMainLoop *loop;
	GError *error;
	guint watch;
	guint stop_id;

	gint fast:1;
	gint complete_at_playing:1;
};

typedef enum {
	COMPLETED_SIGNAL,
	LAST_SIGNAL
} BraseroMetadataSignalType;

static guint brasero_metadata_signals[LAST_SIGNAL] = { 0 };
static GObjectClass *parent_class = NULL;

static gboolean brasero_metadata_bus_messages (GstBus *bus,
					       GstMessage *msg,
					       BraseroMetadata *meta);

static void brasero_metadata_new_decoded_pad_cb (GstElement *decode,
						 GstPad *pad,
						 gboolean arg2,
						 BraseroMetadata *meta);

enum {
	PROP_NONE,
	PROP_URI
};

GType
brasero_metadata_get_type ()
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroMetadataClass),
			NULL,
			NULL,
			(GClassInitFunc) brasero_metadata_class_init,
			NULL,
			NULL,
			sizeof (BraseroMetadata),
			0,
			(GInstanceInitFunc) brasero_metadata_init,
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "BraseroMetadata",
					       &our_info, 0);
	}

	return type;
}

static void
brasero_metadata_class_init (BraseroMetadataClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
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

static void
brasero_metadata_get_property (GObject *obj,
			       guint prop_id,
			       GValue *value,
			       GParamSpec *pspec)
{
	char *uri;
	BraseroMetadata *meta;

	meta = BRASERO_METADATA (obj);
	switch (prop_id) {
	case PROP_URI:
		g_object_get (G_OBJECT (meta->priv->source), "location",
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
	const char *uri;
	BraseroMetadata *meta;

	meta = BRASERO_METADATA (obj);
	switch (prop_id) {
	case PROP_URI:
		uri = g_value_get_string (value);
		gst_element_set_state (GST_ELEMENT (meta->priv->pipeline), GST_STATE_NULL);
		if (meta->priv->source)
			g_object_set (G_OBJECT (meta->priv->source),
				      "location", uri,
				      NULL);
		gst_element_set_state (GST_ELEMENT (meta->priv->pipeline),
				       GST_STATE_PAUSED);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
brasero_metadata_init (BraseroMetadata *obj)
{
	obj->priv = g_new0 (BraseroMetadataPrivate, 1);
}

static void
brasero_metadata_finalize (GObject *object)
{
	BraseroMetadata *cobj;

	cobj = BRASERO_METADATA (object);

	if (cobj->priv->stop_id) {
		g_source_remove (cobj->priv->stop_id);
		cobj->priv->stop_id = 0;
	}

	if (cobj->priv->watch) {
		g_source_remove (cobj->priv->watch);
		cobj->priv->watch = 0;
	}

	if (cobj->priv->pipeline) {
		gst_element_set_state (GST_ELEMENT (cobj->priv->pipeline), GST_STATE_NULL);
		gst_object_unref (cobj->priv->pipeline);
		cobj->priv->pipeline = NULL;
	}

	if (cobj->uri) {
		g_free (cobj->uri);
		cobj->uri = NULL;
	}

	if (cobj->type)
		g_free (cobj->type);

	if (cobj->title)
		g_free (cobj->title);

	if (cobj->artist)
		g_free (cobj->artist);

	if (cobj->album)
		g_free (cobj->album);

	if (cobj->musicbrainz_id)
		g_free (cobj->musicbrainz_id);

	if (cobj->priv->error) {
		g_error_free (cobj->priv->error);
		cobj->priv->error = NULL;
	}

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
brasero_metadata_create_pipeline (BraseroMetadata *metadata)
{
	GstBus *bus;

	metadata->priv->pipeline = gst_pipeline_new (NULL);

	bus = gst_pipeline_get_bus (GST_PIPELINE (metadata->priv->pipeline));
	metadata->priv->watch = gst_bus_add_watch (bus,
						   (GstBusFunc) brasero_metadata_bus_messages,
						   metadata);
	gst_object_unref (bus);

	metadata->priv->source = gst_element_make_from_uri (GST_URI_SRC,
							    metadata->uri,
							    NULL);

	if (metadata->priv->source == NULL) {
		metadata->priv->error = g_error_new (BRASERO_ERROR,
						     BRASERO_ERROR_GENERAL,
						     "Can't create file source");
		return FALSE;
	}
	gst_bin_add (GST_BIN (metadata->priv->pipeline), metadata->priv->source);

	metadata->priv->decode = gst_element_factory_make ("decodebin", NULL);
	if (metadata->priv->decode == NULL) {
		metadata->priv->error = g_error_new (BRASERO_ERROR,
						     BRASERO_ERROR_GENERAL,
						     "Can't create decode");
		return FALSE;
	}
	g_signal_connect (G_OBJECT (metadata->priv->decode), "new-decoded-pad",
			  G_CALLBACK (brasero_metadata_new_decoded_pad_cb),
			  metadata);

	gst_bin_add (GST_BIN (metadata->priv->pipeline), metadata->priv->decode);

	gst_element_link_many (metadata->priv->source, metadata->priv->decode, NULL);

	metadata->priv->sink = gst_element_factory_make ("fakesink", NULL);
	if (metadata->priv->sink == NULL) {
		metadata->priv->error = g_error_new (BRASERO_ERROR,
				                BRASERO_ERROR_GENERAL,
						"Can't create fake sink");
		return FALSE;
	}
	gst_bin_add (GST_BIN (metadata->priv->pipeline), metadata->priv->sink);

	return TRUE;
}

BraseroMetadata *
brasero_metadata_new (const gchar *uri)
{
	BraseroMetadata *obj = NULL;

    	/* escaped URIS are only used by gnome vfs */
	if (gst_uri_is_valid (uri)) {
		obj = BRASERO_METADATA (g_object_new (BRASERO_TYPE_METADATA, NULL));
		obj->uri = g_strdup (uri);
	}

	return obj;
}

static void
brasero_metadata_is_seekable (BraseroMetadata *meta)
{
	GstQuery *query;
	GstFormat format;
	gboolean seekable;

	meta->is_seekable = FALSE;
	query = gst_query_new_seeking (GST_FORMAT_DEFAULT);
	if (!gst_element_query (meta->priv->pipeline, query))
		goto end;

	gst_query_parse_seeking (query,
				 &format,
				 &seekable,
				 NULL,
				 NULL);

	meta->is_seekable = seekable;

end:
	gst_query_unref (query);
}

static gboolean
brasero_metadata_completed (BraseroMetadata *meta, gboolean eos)
{
	GstFormat format = GST_FORMAT_TIME;
	GstElement *typefind;
	GstCaps *caps = NULL;
	GstQuery *query;
	gint64 duration;

	/* find the type of the file */
	typefind = gst_bin_get_by_name (GST_BIN (meta->priv->decode), "typefind");
	if (typefind == NULL) {
		meta->priv->error = g_error_new (BRASERO_ERROR,
						 BRASERO_ERROR_GENERAL,
						 "can't get typefind");
		goto signal;
	}

	g_object_get (typefind, "caps", &caps, NULL);
	if (!caps) {
		meta->priv->error = g_error_new (BRASERO_ERROR,
						 BRASERO_ERROR_GENERAL,
						_("unknown type of file"));
		gst_object_unref (typefind);
		goto signal;
	}

	if (caps && gst_caps_get_size (caps) > 0) {
		if (meta->type)
			g_free (meta->type);

		meta->type = g_strdup (gst_structure_get_name (gst_caps_get_structure (caps, 0)));
		gst_object_unref (typefind);

		if (meta->type && !strcmp (meta->type, "application/x-id3")) {
			/* if slow was chosen we return and read the mp3 till 
			 * the end in case it's VBR. In the latter case the 
			 * following only gives an approximation. */
			g_free (meta->type);
			meta->type = g_strdup ("audio/mpeg");
		}

		if (!eos
		&&  !meta->priv->fast
		&&  !strcmp (meta->type, "audio/mpeg")) {
			/* we try to go forward. This allows us to avoid reading
			 * the whole file till the end. The byte format is 
			 * really important here as time format needs calculation
			 * and is dead slow. The number of bytes is important to
			 * reach the end. */
			if (!gst_element_seek (meta->priv->pipeline,
					       1.0,
					       GST_FORMAT_BYTES,
					       GST_SEEK_FLAG_FLUSH,
					       GST_SEEK_TYPE_SET,
					       52428800,
					       GST_FORMAT_UNDEFINED,
					       GST_CLOCK_TIME_NONE))
				g_warning ("Forward move was impossible.\n");
			return FALSE;
		}
	}
	else
		gst_object_unref (typefind);

	query = gst_query_new_duration (format);
	if (!gst_element_query (GST_ELEMENT (meta->priv->pipeline), query)) {
		if (meta->priv->error == NULL) {
			meta->priv->error = g_error_new (BRASERO_ERROR,
							 BRASERO_ERROR_GENERAL,
							 _("this format is not supported by gstreamer"));
		}
		goto signal;
	}
	gst_query_parse_duration (query, NULL, &duration);

	/* we use this value only if there is not a tag for duration or if this 
	 * value = +/- 3% of the tag value */
	if (!meta->len
	||  (duration <= meta->len * 103 / 100 && duration >= meta->len * 97 / 100))
		meta->len = duration;

	gst_query_unref (query);

	brasero_metadata_is_seekable (meta);

signal:
	gst_element_set_state (GST_ELEMENT (meta->priv->pipeline), GST_STATE_NULL);

	if (!meta->priv->loop
	||  !g_main_loop_is_running (meta->priv->loop)) {
		/* we send a message only if we haven't got a loop (= async mode) */
		g_object_ref (meta);
		g_signal_emit (G_OBJECT (meta),
			       brasero_metadata_signals [COMPLETED_SIGNAL],
			       0,
			       meta->priv->error);
		g_object_unref (meta);
	}
	else
		g_main_loop_quit (meta->priv->loop);

	return TRUE;
}

static void
foreach_tag (const GstTagList *list,
	     const char *tag,
	     BraseroMetadata *meta)
{
	if (!strcmp (tag, GST_TAG_TITLE)) {
		if (meta->title)
			g_free (meta->title);

		gst_tag_list_get_string (list, tag, &(meta->title));
	} else if (!strcmp (tag, GST_TAG_ARTIST)
	       ||  !strcmp (tag, GST_TAG_PERFORMER)) {
		if (meta->artist)
			g_free (meta->artist);

		gst_tag_list_get_string (list, tag, &(meta->artist));
	}
	else if (!strcmp (tag, GST_TAG_ALBUM)) {
		if (meta->album)
			g_free (meta->album);

		gst_tag_list_get_string (list, tag, &(meta->album));
	}
/*	else if (!strcmp (tag, GST_TAG_COMPOSER)) {
		if (meta->composer)
			g_free (meta->composer);

		gst_tag_list_get_string (list, tag, &(meta->composer));
	}
*/	else if (!strcmp (tag, GST_TAG_ISRC)) {
		gst_tag_list_get_int (list, tag, &(meta->isrc));
	}
	else if (!strcmp (tag, GST_TAG_MUSICBRAINZ_TRACKID)) {
		gst_tag_list_get_string (list, tag, &(meta->musicbrainz_id));
	}
	else if (!strcmp (tag, GST_TAG_DURATION)) {
		gst_tag_list_get_uint64 (list, tag, &(meta->len));
	}
}

static gboolean
brasero_metadata_stop_pipeline_timeout (BraseroMetadata *meta)
{
	GstBus *bus;

	meta->priv->stop_id = 0;
	gst_element_set_state (GST_ELEMENT (meta->priv->pipeline),
			       GST_STATE_PAUSED);

	/* read all messages from the queue (might be tags) */
	bus = gst_pipeline_get_bus (GST_PIPELINE (meta->priv->pipeline));
	while (gst_bus_have_pending (bus)) {
		GstMessage *msg;
		GstTagList *tags = NULL;

		msg = gst_bus_pop (bus);
		if (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_TAG) {
			gst_message_parse_tag (msg, &tags);
			gst_tag_list_foreach (tags, (GstTagForeachFunc) foreach_tag, meta);
			gst_tag_list_free (tags);
		}
		gst_message_unref (msg);
	}
	gst_object_unref (bus);

	if (!brasero_metadata_completed (meta, FALSE)) {
		meta->priv->complete_at_playing = 1;
		gst_element_set_state (GST_ELEMENT (meta->priv->pipeline),
				       GST_STATE_PLAYING);
	}

	return FALSE;
}

static void
brasero_metadata_stop_pipeline (BraseroMetadata *meta)
{
	if (meta->priv->stop_id)
		return;

	meta->priv->stop_id = g_timeout_add (1000,
					     (GSourceFunc) brasero_metadata_stop_pipeline_timeout,
					     meta);
}

static gboolean
brasero_metadata_bus_messages (GstBus *bus,
			       GstMessage *msg,
			       BraseroMetadata *meta)
{
	GstStateChangeReturn result;
	gchar *debug_string = NULL;
	GstTagList *tags = NULL;
	GError *error = NULL;
	GstState newstate;

	switch (GST_MESSAGE_TYPE (msg)) {
	case GST_MESSAGE_ERROR:
		/* when stopping the pipeline we are only interested in TAGS */
		gst_message_parse_error (msg, &error, &debug_string);
		if (debug && debug_string)
			g_warning ("DEBUG: %s\n", debug_string);

		g_free (debug_string);

		meta->priv->error = error;
		brasero_metadata_completed (meta, TRUE);
		return FALSE;

	case GST_MESSAGE_EOS:
		/* when stopping the pipeline we are only interested in TAGS */
		brasero_metadata_completed (meta, TRUE);
		return FALSE;

	case GST_MESSAGE_TAG:
		gst_message_parse_tag (msg, &tags);
		gst_tag_list_foreach (tags, (GstTagForeachFunc) foreach_tag, meta);
		gst_tag_list_free (tags);
		break;

	case GST_MESSAGE_STATE_CHANGED:
		/* when stopping the pipeline we are only interested in TAGS */
		result = gst_element_get_state (GST_ELEMENT (meta->priv->pipeline),
						&newstate,
						NULL,
						0);

		if (result != GST_STATE_CHANGE_SUCCESS)
			break;

		if (newstate == GST_STATE_PAUSED
		&& !meta->priv->complete_at_playing)
			brasero_metadata_stop_pipeline (meta);

		if (newstate == GST_STATE_PLAYING
		&& !meta->priv->complete_at_playing)
			brasero_metadata_stop_pipeline (meta);

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
				     BraseroMetadata *meta)
{
	GstPad *sink;
	GstCaps *caps;
	GstStructure *structure;

	sink = gst_element_get_pad (meta->priv->sink, "sink");
	if (GST_PAD_IS_LINKED (sink))
		return;

	/* make sure that this is audio / video */
	caps = gst_pad_get_caps (pad);
	structure = gst_caps_get_structure (caps, 0);
	if (structure) {
		const gchar *name;

		name = gst_structure_get_name (structure);
		meta->has_audio = (g_strrstr (name, "audio") != NULL);
		meta->has_video = (g_strrstr (name, "video") != NULL);

		if (meta->has_audio || meta->has_video)
			gst_pad_link (pad, sink);
	}

	gst_object_unref (sink);
	gst_caps_unref (caps);
}

gboolean
brasero_metadata_get_sync (BraseroMetadata *meta, gboolean fast, GError **error)
{
	if (!meta->priv->pipeline
	&&  !brasero_metadata_create_pipeline (meta)) {
		g_propagate_error (error, meta->priv->error);
		meta->priv->error = NULL;
		return FALSE;
	}

	meta->priv->fast = (fast == TRUE);

	gst_element_set_state (GST_ELEMENT (meta->priv->pipeline), GST_STATE_PLAYING);

	meta->priv->loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (meta->priv->loop);
	g_main_loop_unref (meta->priv->loop);
	meta->priv->loop = NULL;

	gst_element_set_state (GST_ELEMENT (meta->priv->pipeline), GST_STATE_NULL);
	if (meta->priv->error) {
		if (error) {
			g_propagate_error (error, meta->priv->error);
			meta->priv->error = NULL;
		} 
		else {
			g_warning ("ERROR getting metadata : %s\n",
				   meta->priv->error->message);
			g_error_free (meta->priv->error);
			meta->priv->error = NULL;
		}
		return FALSE;
	}

	return TRUE;
}

gboolean
brasero_metadata_get_async (BraseroMetadata *meta, gboolean fast)
{
	meta->priv->fast = (fast == TRUE);

	if (!meta->priv->pipeline
	&&  !brasero_metadata_create_pipeline (meta)) {
		g_object_ref (meta);
		g_signal_emit (G_OBJECT (meta),
			       brasero_metadata_signals [COMPLETED_SIGNAL],
			       0,
			       meta->priv->error);
		g_object_unref (meta);

		if (meta->priv->error) {
			g_error_free (meta->priv->error);
			meta->priv->error = NULL;
		}
		return FALSE;
	}

	gst_element_set_state (GST_ELEMENT (meta->priv->pipeline), GST_STATE_PLAYING);
	return TRUE;
}

void
brasero_metadata_cancel (BraseroMetadata *meta)
{
	if (meta->priv->stop_id)
		return;

	if (meta->priv->pipeline) {
		if (meta->priv->watch) {
			g_source_remove (meta->priv->watch);
			meta->priv->watch = 0;
		}

		gst_element_set_state (GST_ELEMENT (meta->priv->pipeline), GST_STATE_NULL);
		gst_object_unref (GST_OBJECT (meta->priv->pipeline));
		meta->priv->pipeline = NULL;
	}
}
