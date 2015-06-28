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

#include <string.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gmodule.h>

#include <gst/gst.h>

#include "burn-basics.h"
#include "brasero-medium.h"
#include "brasero-tags.h"
#include "burn-job.h"
#include "brasero-plugin-registration.h"
#include "burn-normalize.h"


#define BRASERO_TYPE_TRANSCODE         (brasero_transcode_get_type ())
#define BRASERO_TRANSCODE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_TRANSCODE, BraseroTranscode))
#define BRASERO_TRANSCODE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_TRANSCODE, BraseroTranscodeClass))
#define BRASERO_IS_TRANSCODE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_TRANSCODE))
#define BRASERO_IS_TRANSCODE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_TRANSCODE))
#define BRASERO_TRANSCODE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_TRANSCODE, BraseroTranscodeClass))

BRASERO_PLUGIN_BOILERPLATE (BraseroTranscode, brasero_transcode, BRASERO_TYPE_JOB, BraseroJob);

static gboolean brasero_transcode_bus_messages (GstBus *bus,
						GstMessage *msg,
						BraseroTranscode *transcode);
static void brasero_transcode_new_decoded_pad_cb (GstElement *decode,
						  GstPad *pad,
						  BraseroTranscode *transcode);

struct BraseroTranscodePrivate {
	GstElement *pipeline;
	GstElement *convert;
	GstElement *source;
	GstElement *decode;
	GstElement *sink;

	/* element to link decode to */
	GstElement *link;

	gint pad_size;
	gint pad_fd;
	gint pad_id;

	gint64 size;
	gint64 pos;

	gulong probe;
	gint64 segment_start;
	gint64 segment_end;

	guint set_active_state:1;
	guint mp3_size_pipeline:1;
};
typedef struct BraseroTranscodePrivate BraseroTranscodePrivate;

#define BRASERO_TRANSCODE_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_TRANSCODE, BraseroTranscodePrivate))

static GObjectClass *parent_class = NULL;

/* FIXME: this entire function looks completely wrong, if there is or
 * was a bug in GStreamer it should be fixed there (tpm) */
static GstPadProbeReturn
brasero_transcode_buffer_handler (GstPad *pad,
                                  GstPadProbeInfo *info,
                                  gpointer user_data)
{
	BraseroTranscodePrivate *priv;
	BraseroTranscode *self = user_data;
	GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER (info);
	GstPad *peer;
	gint64 size;

	priv = BRASERO_TRANSCODE_PRIVATE (self);

	size = gst_buffer_get_size (buffer);

	if (priv->segment_start <= 0 && priv->segment_end <= 0)
		return GST_PAD_PROBE_OK;

	/* what we do here is more or less what gstreamer does when seeking:
	 * it reads and process from 0 to the seek position (I tried).
	 * It even forwards the data before the seek position to the sink (which
	 * is a problem in our case as it would be written) */
	if (priv->size > priv->segment_end) {
		priv->size += size;
		return GST_PAD_PROBE_DROP;
	}

	if (priv->size + size > priv->segment_end) {
		GstBuffer *new_buffer;
		int data_size;

		/* the entire the buffer is not interesting for us */
		/* create a new buffer and push it on the pad:
		 * NOTE: we're going to receive it ... */
		data_size = priv->segment_end - priv->size;
		new_buffer = gst_buffer_copy_region (buffer, GST_BUFFER_COPY_METADATA, 0, data_size);

		/* FIXME: we can now modify the probe buffer in 0.11 */
		/* Recursive: the following calls ourselves BEFORE we finish */
		peer = gst_pad_get_peer (pad);
		gst_pad_push (peer, new_buffer);

		priv->size += size - data_size;

		/* post an EOS event to stop pipeline */
		gst_pad_push_event (peer, gst_event_new_eos ());
		gst_object_unref (peer);
		return GST_PAD_PROBE_DROP;
	}

	/* see if the buffer is in the segment */
	if (priv->size < priv->segment_start) {
		GstBuffer *new_buffer;
		gint data_size;

		/* see if all the buffer is interesting for us */
		if (priv->size + size < priv->segment_start) {
			priv->size += size;
			return GST_PAD_PROBE_DROP;
		}

		/* create a new buffer and push it on the pad:
		 * NOTE: we're going to receive it ... */
		data_size = priv->size + size - priv->segment_start;
		new_buffer = gst_buffer_copy_region (buffer, GST_BUFFER_COPY_METADATA, size - data_size, data_size);
		/* FIXME: this looks dodgy (tpm) */
		GST_BUFFER_TIMESTAMP (new_buffer) = GST_BUFFER_TIMESTAMP (buffer) + data_size;

		/* move forward by the size of bytes we dropped */
		priv->size += size - data_size;

		/* FIXME: we can now modify the probe buffer in 0.11 */
		/* this is recursive the following calls ourselves 
		 * BEFORE we finish */
		peer = gst_pad_get_peer (pad);
		gst_pad_push (peer, new_buffer);
		gst_object_unref (peer);

		return GST_PAD_PROBE_DROP;
	}

	priv->size += size;
	priv->pos += size;

	return GST_PAD_PROBE_OK;
}

static BraseroBurnResult
brasero_transcode_set_boundaries (BraseroTranscode *transcode)
{
	BraseroTranscodePrivate *priv;
	BraseroTrack *track;
	gint64 start;
	gint64 end;

	priv = BRASERO_TRANSCODE_PRIVATE (transcode);

	/* we need to reach the song start and set a possible end; this is only
	 * needed when it is decoding a song. Otherwise*/
	brasero_job_get_current_track (BRASERO_JOB (transcode), &track);
	start = brasero_track_stream_get_start (BRASERO_TRACK_STREAM (track));
	end = brasero_track_stream_get_end (BRASERO_TRACK_STREAM (track));

	priv->segment_start = BRASERO_DURATION_TO_BYTES (start);
	priv->segment_end = BRASERO_DURATION_TO_BYTES (end);

	BRASERO_JOB_LOG (transcode, "settings track boundaries time = %" G_GINT64_FORMAT " %" G_GINT64_FORMAT " / bytes = %" G_GINT64_FORMAT " %" G_GINT64_FORMAT,
			 start, end,
			 priv->segment_start, priv->segment_end);

	return BRASERO_BURN_OK;
}

static void
brasero_transcode_send_volume_event (BraseroTranscode *transcode)
{
	BraseroTranscodePrivate *priv;
	gdouble track_peak = 0.0;
	gdouble track_gain = 0.0;
	GstTagList *tag_list;
	BraseroTrack *track;
	GstEvent *event;
	GValue *value;

	priv = BRASERO_TRANSCODE_PRIVATE (transcode);

	brasero_job_get_current_track (BRASERO_JOB (transcode), &track);

	BRASERO_JOB_LOG (transcode, "Sending audio levels tags");
	if (brasero_track_tag_lookup (track, BRASERO_TRACK_PEAK_VALUE, &value) == BRASERO_BURN_OK)
		track_peak = g_value_get_double (value);

	if (brasero_track_tag_lookup (track, BRASERO_TRACK_GAIN_VALUE, &value) == BRASERO_BURN_OK)
		track_gain = g_value_get_double (value);

	/* it's possible we fail */
	tag_list = gst_tag_list_new (GST_TAG_TRACK_GAIN, track_gain,
				     GST_TAG_TRACK_PEAK, track_peak,
				     NULL);

	/* NOTE: that event is goind downstream */
	event = gst_event_new_tag (tag_list);
	if (!gst_element_send_event (priv->convert, event))
		BRASERO_JOB_LOG (transcode, "Couldn't send tags to rgvolume");

	BRASERO_JOB_LOG (transcode, "Set volume level %lf %lf", track_gain, track_peak);
}

static GstElement *
brasero_transcode_create_volume (BraseroTranscode *transcode,
				 BraseroTrack *track)
{
	GstElement *volume = NULL;

	/* see if we need a volume object */
	if (brasero_track_tag_lookup (track, BRASERO_TRACK_PEAK_VALUE, NULL) == BRASERO_BURN_OK
	||  brasero_track_tag_lookup (track, BRASERO_TRACK_GAIN_VALUE, NULL) == BRASERO_BURN_OK) {
		BRASERO_JOB_LOG (transcode, "Found audio levels tags");
		volume = gst_element_factory_make ("rgvolume", NULL);
		if (volume)
			g_object_set (volume,
				      "album-mode", FALSE,
				      NULL);
		else
			BRASERO_JOB_LOG (transcode, "rgvolume object couldn't be created");
	}

	return volume;
}

static gboolean
brasero_transcode_create_pipeline_size_mp3 (BraseroTranscode *transcode,
					    GstElement *pipeline,
					    GstElement *source,
                                            GError **error)
{
	BraseroTranscodePrivate *priv;
	GstElement *parse;
	GstElement *sink;

	priv = BRASERO_TRANSCODE_PRIVATE (transcode);

	BRASERO_JOB_LOG (transcode, "Creating specific pipeline for MP3s");

	parse = gst_element_factory_make ("mpegaudioparse", NULL);
	if (!parse) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"mpegaudioparse\"");
		g_object_unref (pipeline);
		return FALSE;
	}
	gst_bin_add (GST_BIN (pipeline), parse);

	sink = gst_element_factory_make ("fakesink", NULL);
	if (!sink) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Fakesink\"");
		g_object_unref (pipeline);
		return FALSE;
	}
	gst_bin_add (GST_BIN (pipeline), sink);

	/* Link */
	if (!gst_element_link_many (source, parse, sink, NULL)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("Impossible to link plugin pads"));
		BRASERO_JOB_LOG (transcode, "Impossible to link elements");
		g_object_unref (pipeline);
		return FALSE;
	}

	priv->convert = NULL;

	priv->sink = sink;
	priv->source = source;
	priv->pipeline = pipeline;

	/* Get it going */
	gst_element_set_state (priv->pipeline, GST_STATE_PLAYING);
	return TRUE;
}

static void
brasero_transcode_error_on_pad_linking (BraseroTranscode *self,
                                        const gchar *function_name)
{
	BraseroTranscodePrivate *priv;
	GstMessage *message;
	GstBus *bus;

	priv = BRASERO_TRANSCODE_PRIVATE (self);

	BRASERO_JOB_LOG (self, "Error on pad linking");
	message = gst_message_new_error (GST_OBJECT (priv->pipeline),
					 g_error_new (BRASERO_BURN_ERROR,
						      BRASERO_BURN_ERROR_GENERAL,
						      /* Translators: This message is sent
						       * when brasero could not link together
						       * two gstreamer plugins so that one
						       * sends its data to the second for further
						       * processing. This data transmission is
						       * done through a pad. Maybe this is a bit
						       * too technical and should be removed? */
						      _("Impossible to link plugin pads")),
					 function_name);

	bus = gst_pipeline_get_bus (GST_PIPELINE (priv->pipeline));
	gst_bus_post (bus, message);
	g_object_unref (bus);
}

static gboolean
brasero_transcode_create_pipeline (BraseroTranscode *transcode,
				   GError **error)
{
	gchar *uri;
	gboolean keep_dts;
	GstElement *decode;
	GstElement *source;
	GstBus *bus = NULL;
	GstCaps *filtercaps;
	GValue *value = NULL;
	GstElement *pipeline;
	GstElement *sink = NULL;
	BraseroJobAction action;
	GstElement *filter = NULL;
	GstElement *volume = NULL;
	GstElement *convert = NULL;
	BraseroTrack *track = NULL;
	GstElement *resample = NULL;
	BraseroTranscodePrivate *priv;

	priv = BRASERO_TRANSCODE_PRIVATE (transcode);

	BRASERO_JOB_LOG (transcode, "Creating new pipeline");

	priv->set_active_state = 0;

	/* free the possible current pipeline and create a new one */
	if (priv->pipeline) {
		gst_element_set_state (priv->pipeline, GST_STATE_NULL);
		gst_object_unref (G_OBJECT (priv->pipeline));
		priv->link = NULL;
		priv->sink = NULL;
		priv->source = NULL;
		priv->convert = NULL;
		priv->pipeline = NULL;
	}

	/* create three types of pipeline according to the needs: (possibly adding grvolume)
	 * - filesrc ! decodebin ! audioconvert ! fakesink (find size) and filesrc!mp3parse!fakesink for mp3s
	 * - filesrc ! decodebin ! audioresample ! audioconvert ! audio/x-raw,format=S16BE,rate=44100 ! filesink
	 * - filesrc ! decodebin ! audioresample ! audioconvert ! audio/x-raw,format=S16BE,rate=44100 ! fdsink
	 */
	pipeline = gst_pipeline_new (NULL);

	bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
	gst_bus_add_watch (bus,
			   (GstBusFunc) brasero_transcode_bus_messages,
			   transcode);
	gst_object_unref (bus);

	/* source */
	brasero_job_get_current_track (BRASERO_JOB (transcode), &track);
	uri = brasero_track_stream_get_source (BRASERO_TRACK_STREAM (track), TRUE);
	source = gst_element_make_from_uri (GST_URI_SRC, uri, NULL, NULL);
	g_free (uri);

	if (source == NULL) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     /* Translators: %s is the name of the object (as in
			      * GObject) from the Gstreamer library that could
			      * not be created */
			     _("%s element could not be created"),
			     "\"Source\"");
		goto error;
	}
	gst_bin_add (GST_BIN (pipeline), source);
	g_object_set (source,
		      "typefind", FALSE,
		      NULL);

	/* sink */
	brasero_job_get_action (BRASERO_JOB (transcode), &action);
	switch (action) {
	case BRASERO_JOB_ACTION_SIZE:
		if (priv->mp3_size_pipeline)
			return brasero_transcode_create_pipeline_size_mp3 (transcode, pipeline, source, error);

		sink = gst_element_factory_make ("fakesink", NULL);
		break;

	case BRASERO_JOB_ACTION_IMAGE:
		volume = brasero_transcode_create_volume (transcode, track);

		if (brasero_job_get_fd_out (BRASERO_JOB (transcode), NULL) != BRASERO_BURN_OK) {
			gchar *output;

			brasero_job_get_image_output (BRASERO_JOB (transcode),
						      &output,
						      NULL);
			sink = gst_element_factory_make ("filesink", NULL);
			g_object_set (sink,
				      "location", output,
				      NULL);
			g_free (output);
		}
		else {
			int fd;

			brasero_job_get_fd_out (BRASERO_JOB (transcode), &fd);
			sink = gst_element_factory_make ("fdsink", NULL);
			g_object_set (sink,
				      "fd", fd,
				      NULL);
		}
		break;

	default:
		goto error;
	}

	if (!sink) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Sink\"");
		goto error;
	}

	gst_bin_add (GST_BIN (pipeline), sink);
	g_object_set (sink,
		      "sync", FALSE,
		      NULL);

	brasero_job_tag_lookup (BRASERO_JOB (transcode),
				BRASERO_SESSION_STREAM_AUDIO_FORMAT,
				&value);
	if (value)
		keep_dts = (g_value_get_int (value) & BRASERO_AUDIO_FORMAT_DTS) != 0;
	else
		keep_dts = FALSE;

	if (keep_dts
	&&  action == BRASERO_JOB_ACTION_IMAGE
	&& (brasero_track_stream_get_format (BRASERO_TRACK_STREAM (track)) & BRASERO_AUDIO_FORMAT_DTS) != 0) {
		GstElement *wavparse;
		GstPad *sinkpad;

		BRASERO_JOB_LOG (transcode, "DTS wav pipeline");

		/* FIXME: volume normalization won't work here. We'd need to 
		 * reencode it afterwards otherwise. */
		/* This is a special case. This is DTS wav. So we only decode wav. */
		wavparse = gst_element_factory_make ("wavparse", NULL);
		if (wavparse == NULL) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("%s element could not be created"),
				     "\"Wavparse\"");
			goto error;
		}
		gst_bin_add (GST_BIN (pipeline), wavparse);

		if (!gst_element_link_many (source, wavparse, sink, NULL)) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
			             _("Impossible to link plugin pads"));
			goto error;
		}

		/* This is an ugly workaround for the lack of accuracy with
		 * gstreamer. Yet this is unfortunately a necessary evil. */
		/* FIXME: this does not look like it makes sense... (tpm) */
		priv->pos = 0;
		priv->size = 0;
		sinkpad = gst_element_get_static_pad (sink, "sink");
		priv->probe = gst_pad_add_probe (sinkpad, GST_PAD_PROBE_TYPE_BUFFER,
		                                 brasero_transcode_buffer_handler,
		                                 transcode, NULL);
		gst_object_unref (sinkpad);


		priv->link = NULL;
		priv->sink = sink;
		priv->decode = NULL;
		priv->source = source;
		priv->convert = NULL;
		priv->pipeline = pipeline;

		gst_element_set_state (pipeline, GST_STATE_PLAYING);
		return TRUE;
	}

	/* audioconvert */
	convert = gst_element_factory_make ("audioconvert", NULL);
	if (convert == NULL) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Audioconvert\"");
		goto error;
	}
	gst_bin_add (GST_BIN (pipeline), convert);

	if (action == BRASERO_JOB_ACTION_IMAGE) {
		BraseroStreamFormat session_format;
		BraseroTrackType *output_type;

		output_type = brasero_track_type_new ();
		brasero_job_get_output_type (BRASERO_JOB (transcode), output_type);
		session_format = brasero_track_type_get_stream_format (output_type);
		brasero_track_type_free (output_type);

		/* audioresample */
		resample = gst_element_factory_make ("audioresample", NULL);
		if (resample == NULL) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("%s element could not be created"),
				     "\"Audioresample\"");
			goto error;
		}
		gst_bin_add (GST_BIN (pipeline), resample);

		/* filter */
		filter = gst_element_factory_make ("capsfilter", NULL);
		if (!filter) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("%s element could not be created"),
				     "\"Filter\"");
			goto error;
		}
		gst_bin_add (GST_BIN (pipeline), filter);
		filtercaps = gst_caps_new_full (gst_structure_new ("audio/x-raw",
								   /* NOTE: we use little endianness only for libburn which requires little */
								   "format", G_TYPE_STRING, (session_format & BRASERO_AUDIO_FORMAT_RAW_LITTLE_ENDIAN) != 0 ? "S16LE" : "S16BE",
								   "channels", G_TYPE_INT, 2,
								   "rate", G_TYPE_INT, 44100,
								   NULL),
						NULL);
		g_object_set (GST_OBJECT (filter), "caps", filtercaps, NULL);
		gst_caps_unref (filtercaps);
	}

	/* decode */
	decode = gst_element_factory_make ("decodebin", NULL);
	if (decode == NULL) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Decodebin\"");
		goto error;
	}
	gst_bin_add (GST_BIN (pipeline), decode);

	if (action == BRASERO_JOB_ACTION_IMAGE) {
		GstPad *sinkpad;
		gboolean res;

		if (!gst_element_link (source, decode)) {
			BRASERO_JOB_LOG (transcode, "Impossible to link plugin pads");
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
			             _("Impossible to link plugin pads"));
			goto error;
		}

		priv->link = resample;
		g_signal_connect (G_OBJECT (decode),
				  "pad-added",
				  G_CALLBACK (brasero_transcode_new_decoded_pad_cb),
				  transcode);

		if (volume) {
			gst_bin_add (GST_BIN (pipeline), volume);
			res = gst_element_link_many (resample,
			                             volume,
						     convert,
			                             filter,
			                             sink,
			                             NULL);
		}
		else
			res = gst_element_link_many (resample,
			                             convert,
			                             filter,
			                             sink,
			                             NULL);

		if (!res) {
			BRASERO_JOB_LOG (transcode, "Impossible to link plugin pads");
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("Impossible to link plugin pads"));
			goto error;
		}

		/* This is an ugly workaround for the lack of accuracy with
		 * gstreamer. Yet this is unfortunately a necessary evil. */
		/* FIXME: this does not look like it makes sense... (tpm) */
		priv->pos = 0;
		priv->size = 0;
		sinkpad = gst_element_get_static_pad (sink, "sink");
		priv->probe = gst_pad_add_probe (sinkpad, GST_PAD_PROBE_TYPE_BUFFER,
		                                 brasero_transcode_buffer_handler,
		                                 transcode, NULL);
		gst_object_unref (sinkpad);
	}
	else {
		if (!gst_element_link (source, decode)
		||  !gst_element_link (convert, sink)) {
			BRASERO_JOB_LOG (transcode, "Impossible to link plugin pads");
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("Impossible to link plugin pads"));
			goto error;
		}

		priv->link = convert;
		g_signal_connect (G_OBJECT (decode),
				  "pad-added",
				  G_CALLBACK (brasero_transcode_new_decoded_pad_cb),
				  transcode);
	}

	priv->sink = sink;
	priv->decode = decode;
	priv->source = source;
	priv->convert = convert;
	priv->pipeline = pipeline;

	gst_element_set_state (pipeline, GST_STATE_PLAYING);
	return TRUE;

error:

	if (error && (*error))
		BRASERO_JOB_LOG (transcode,
				 "can't create object : %s \n",
				 (*error)->message);

	gst_object_unref (GST_OBJECT (pipeline));
	return FALSE;
}

static void
brasero_transcode_set_track_size (BraseroTranscode *transcode,
				  gint64 duration)
{
	gchar *uri;
	BraseroTrack *track;

	brasero_job_get_current_track (BRASERO_JOB (transcode), &track);
	brasero_track_stream_set_boundaries (BRASERO_TRACK_STREAM (track), -1, duration, -1);
	duration += brasero_track_stream_get_gap (BRASERO_TRACK_STREAM (track));

	/* if transcoding on the fly we should add some length just to make
	 * sure we won't be too short (gstreamer duration discrepancy) */
	brasero_job_set_output_size_for_current_track (BRASERO_JOB (transcode),
						       BRASERO_DURATION_TO_SECTORS (duration),
						       BRASERO_DURATION_TO_BYTES (duration));

	uri = brasero_track_stream_get_source (BRASERO_TRACK_STREAM (track), FALSE);
	BRASERO_JOB_LOG (transcode,
			 "Song %s"
			 "\nsectors %" G_GINT64_FORMAT
			 "\ntime %" G_GINT64_FORMAT, 
			 uri,
			 BRASERO_DURATION_TO_SECTORS (duration),
			 duration);
	g_free (uri);
}

/**
 * These functions are to deal with siblings
 */

static BraseroBurnResult
brasero_transcode_create_sibling_size (BraseroTranscode *transcode,
				        BraseroTrack *src,
				        GError **error)
{
	BraseroTrack *dest;
	guint64 duration;

	/* it means the same file uri is in the selection and was already
	 * checked. Simply get the values for the length and other information
	 * and copy them. */
	/* NOTE: no need to copy the length since if they are sibling that means
	 * that they have the same length */
	brasero_track_stream_get_length (BRASERO_TRACK_STREAM (src), &duration);
	brasero_job_set_output_size_for_current_track (BRASERO_JOB (transcode),
						       BRASERO_DURATION_TO_SECTORS (duration),
						       BRASERO_DURATION_TO_BYTES (duration));

	/* copy the info we are missing */
	brasero_job_get_current_track (BRASERO_JOB (transcode), &dest);
	brasero_track_tag_copy_missing (dest, src);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_transcode_create_sibling_image (BraseroTranscode *transcode,
					BraseroTrack *src,
					GError **error)
{
	BraseroTrackStream *dest;
	BraseroTrack *track;
	guint64 length = 0;
	gchar *path_dest;
	gchar *path_src;

	/* it means the file is already in the selection. Simply create a 
	 * symlink pointing to first file in the selection with the same uri */
	path_src = brasero_track_stream_get_source (BRASERO_TRACK_STREAM (src), FALSE);
	brasero_job_get_audio_output (BRASERO_JOB (transcode), &path_dest);

	if (symlink (path_src, path_dest) == -1) {
                int errsv = errno;

		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     /* Translators: the %s is the error message from errno */
			     _("An internal error occurred (%s)"),
			     g_strerror (errsv));

		goto error;
	}

	dest = brasero_track_stream_new ();
	brasero_track_stream_set_source (dest, path_dest);

	/* FIXME: what if input had metadata ?*/
	brasero_track_stream_set_format (dest, BRASERO_AUDIO_FORMAT_RAW);

	/* NOTE: there is no gap and start = 0 since these tracks are the result
	 * of the transformation of previous ones */
	brasero_track_stream_get_length (BRASERO_TRACK_STREAM (src), &length);
	brasero_track_stream_set_boundaries (dest, 0, length, 0);

	/* copy all infos but from the current track */
	brasero_job_get_current_track (BRASERO_JOB (transcode), &track);
	brasero_track_tag_copy_missing (BRASERO_TRACK (dest), track);
	brasero_job_add_track (BRASERO_JOB (transcode), BRASERO_TRACK (dest));

	/* It's good practice to unref the track afterwards as we don't need it
	 * anymore. BraseroTaskCtx refs it. */
	g_object_unref (dest);

	g_free (path_src);
	g_free (path_dest);

	return BRASERO_BURN_NOT_RUNNING;

error:
	g_free (path_src);
	g_free (path_dest);

	return BRASERO_BURN_ERR;
}

static BraseroTrack *
brasero_transcode_search_for_sibling (BraseroTranscode *transcode)
{
	BraseroJobAction action;
	GSList *iter, *songs;
	BraseroTrack *track;
	gint64 start;
	gint64 end;
	gchar *uri;

	brasero_job_get_action (BRASERO_JOB (transcode), &action);

	brasero_job_get_current_track (BRASERO_JOB (transcode), &track);
	start = brasero_track_stream_get_start (BRASERO_TRACK_STREAM (track));
	end = brasero_track_stream_get_end (BRASERO_TRACK_STREAM (track));
	uri = brasero_track_stream_get_source (BRASERO_TRACK_STREAM (track), TRUE);

	brasero_job_get_done_tracks (BRASERO_JOB (transcode), &songs);

	for (iter = songs; iter; iter = iter->next) {
		gchar *iter_uri;
		gint64 iter_end;
		gint64 iter_start;
		BraseroTrack *iter_track;

		iter_track = iter->data;
		iter_uri = brasero_track_stream_get_source (BRASERO_TRACK_STREAM (iter_track), TRUE);

		if (strcmp (iter_uri, uri))
			continue;

		iter_end = brasero_track_stream_get_end (BRASERO_TRACK_STREAM (iter_track));
		if (!iter_end)
			continue;

		if (iter_end != end)
			continue;

		iter_start = brasero_track_stream_get_start (BRASERO_TRACK_STREAM (track));
		if (iter_start == start) {
			g_free (uri);
			return iter_track;
		}
	}

	g_free (uri);
	return NULL;
}

static BraseroBurnResult
brasero_transcode_has_track_sibling (BraseroTranscode *transcode,
				     GError **error)
{
	BraseroJobAction action;
	BraseroTrack *sibling = NULL;
	BraseroBurnResult result = BRASERO_BURN_OK;

	if (brasero_job_get_fd_out (BRASERO_JOB (transcode), NULL) == BRASERO_BURN_OK)
		return BRASERO_BURN_OK;

	sibling = brasero_transcode_search_for_sibling (transcode);
	if (!sibling)
		return BRASERO_BURN_OK;

	BRASERO_JOB_LOG (transcode, "found sibling: skipping");
	brasero_job_get_action (BRASERO_JOB (transcode), &action);
	if (action == BRASERO_JOB_ACTION_IMAGE)
		result = brasero_transcode_create_sibling_image (transcode,
								 sibling,
								 error);
	else if (action == BRASERO_JOB_ACTION_SIZE)
		result = brasero_transcode_create_sibling_size (transcode,
								sibling,
								error);

	return result;
}

static BraseroBurnResult
brasero_transcode_start (BraseroJob *job,
			 GError **error)
{
	BraseroTranscode *transcode;
	BraseroBurnResult result;
	BraseroJobAction action;

	transcode = BRASERO_TRANSCODE (job);

	brasero_job_get_action (job, &action);
	brasero_job_set_use_average_rate (job, TRUE);

	if (action == BRASERO_JOB_ACTION_SIZE) {
		BraseroTrack *track;

		/* see if the track size was already set since then no need to 
		 * carry on with a lengthy get size and the library will do it
		 * itself. */
		brasero_job_get_current_track (job, &track);
		if (brasero_track_stream_get_end (BRASERO_TRACK_STREAM (track)) > 0)
			return BRASERO_BURN_NOT_SUPPORTED;

		if (!brasero_transcode_create_pipeline (transcode, error))
			return BRASERO_BURN_ERR;

		brasero_job_set_current_action (job,
						BRASERO_BURN_ACTION_GETTING_SIZE,
						NULL,
						TRUE);

		brasero_job_start_progress (job, FALSE);
		return BRASERO_BURN_OK;
	}

	if (action == BRASERO_JOB_ACTION_IMAGE) {
		/* Look for a sibling to avoid transcoding twice. In this case
		 * though start and end of this track must be inside start and
		 * end of the previous track. Of course if we are piping that
		 * operation is simply impossible. */
		if (brasero_job_get_fd_out (job, NULL) != BRASERO_BURN_OK) {
			result = brasero_transcode_has_track_sibling (BRASERO_TRANSCODE (job), error);
			if (result != BRASERO_BURN_OK)
				return result;
		}

		brasero_transcode_set_boundaries (transcode);
		if (!brasero_transcode_create_pipeline (transcode, error))
			return BRASERO_BURN_ERR;
	}
	else
		BRASERO_JOB_NOT_SUPPORTED (transcode);

	return BRASERO_BURN_OK;
}

static void
brasero_transcode_stop_pipeline (BraseroTranscode *transcode)
{
	BraseroTranscodePrivate *priv;
	GstPad *sinkpad;

	priv = BRASERO_TRANSCODE_PRIVATE (transcode);
	if (!priv->pipeline)
		return;

	sinkpad = gst_element_get_static_pad (priv->sink, "sink");
	if (priv->probe)
		gst_pad_remove_probe (sinkpad, priv->probe);

	gst_object_unref (sinkpad);

	gst_element_set_state (priv->pipeline, GST_STATE_NULL);
	gst_object_unref (GST_OBJECT (priv->pipeline));

	priv->link = NULL;
	priv->sink = NULL;
	priv->source = NULL;
	priv->convert = NULL;
	priv->pipeline = NULL;

	priv->set_active_state = 0;
}

static BraseroBurnResult
brasero_transcode_stop (BraseroJob *job,
			GError **error)
{
	BraseroTranscodePrivate *priv;

	priv = BRASERO_TRANSCODE_PRIVATE (job);

	priv->mp3_size_pipeline = 0;

	if (priv->pad_id) {
		g_source_remove (priv->pad_id);
		priv->pad_id = 0;
	}

	brasero_transcode_stop_pipeline (BRASERO_TRANSCODE (job));
	return BRASERO_BURN_OK;
}

/* we must make sure that the track size is a multiple
 * of 2352 to be burnt by cdrecord with on the fly */

static gint64
brasero_transcode_pad_real (BraseroTranscode *transcode,
			    int fd,
			    gint64 bytes2write,
			    GError **error)
{
	const int buffer_size = 512;
	char buffer [buffer_size];
	gint64 b_written;
	gint64 size;

	b_written = 0;
	bzero (buffer, sizeof (buffer));
	for (; bytes2write; bytes2write -= b_written) {
		size = bytes2write > buffer_size ? buffer_size : bytes2write;
		b_written = write (fd, buffer, (int) size);

		BRASERO_JOB_LOG (transcode,
				 "written %" G_GINT64_FORMAT " bytes for padding",
				 b_written);

		/* we should not handle EINTR and EAGAIN as errors */
		if (b_written < 0) {
			if (errno == EINTR || errno == EAGAIN) {
				BRASERO_JOB_LOG (transcode, "got EINTR / EAGAIN, retrying");
	
				/* we'll try later again */
				return bytes2write;
			}
		}

		if (size != b_written) {
                        int errsv = errno;

			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     /* Translators: %s is the string error from errno */
				     _("Error while padding file (%s)"),
				     g_strerror (errsv));
			return -1;
		}
	}

	return 0;
}

static void
brasero_transcode_push_track (BraseroTranscode *transcode)
{
	guint64 length = 0;
	gchar *output = NULL;
	BraseroTrack *src = NULL;
	BraseroTrackStream *track;

	brasero_job_get_audio_output (BRASERO_JOB (transcode), &output);
	brasero_job_get_current_track (BRASERO_JOB (transcode), &src);

	brasero_track_stream_get_length (BRASERO_TRACK_STREAM (src), &length);

	track = brasero_track_stream_new ();
	brasero_track_stream_set_source (track, output);
	g_free (output);

	/* FIXME: what if input had metadata ?*/
	brasero_track_stream_set_format (track, BRASERO_AUDIO_FORMAT_RAW);
	brasero_track_stream_set_boundaries (track, 0, length, 0);
	brasero_track_tag_copy_missing (BRASERO_TRACK (track), src);

	brasero_job_add_track (BRASERO_JOB (transcode), BRASERO_TRACK (track));
	
	/* It's good practice to unref the track afterwards as we don't need it
	 * anymore. BraseroTaskCtx refs it. */
	g_object_unref (track);

	brasero_job_finished_track (BRASERO_JOB (transcode));
}

static gboolean
brasero_transcode_pad_idle (BraseroTranscode *transcode)
{
	gint64 bytes2write;
	GError *error = NULL;
	BraseroTranscodePrivate *priv;

	priv = BRASERO_TRANSCODE_PRIVATE (transcode);
	bytes2write = brasero_transcode_pad_real (transcode,
						  priv->pad_fd,
						  priv->pad_size,
						  &error);

	if (bytes2write == -1) {
		priv->pad_id = 0;
		brasero_job_error (BRASERO_JOB (transcode), error);
		return FALSE;
	}

	if (bytes2write) {
		priv->pad_size = bytes2write;
		return TRUE;
	}

	/* we are finished with padding */
	priv->pad_id = 0;
	close (priv->pad_fd);
	priv->pad_fd = -1;

	/* set the next song or finish */
	brasero_transcode_push_track (transcode);
	return FALSE;
}

static gboolean
brasero_transcode_pad (BraseroTranscode *transcode, int fd, GError **error)
{
	guint64 length = 0;
	gint64 bytes2write = 0;
	BraseroTrack *track = NULL;
	BraseroTranscodePrivate *priv;

	priv = BRASERO_TRANSCODE_PRIVATE (transcode);
	if (priv->pos < 0)
		return TRUE;

	/* Padding is important for two reasons:
	 * - first if didn't output enough bytes compared to what we should have
	 * - second we must output a multiple of 2352 to respect sector
	 *   boundaries */
	brasero_job_get_current_track (BRASERO_JOB (transcode), &track);
	brasero_track_stream_get_length (BRASERO_TRACK_STREAM (track), &length);

	if (priv->pos < BRASERO_DURATION_TO_BYTES (length)) {
		gint64 b_written = 0;

		/* Check bytes boundary for length */
		b_written = BRASERO_DURATION_TO_BYTES (length);
		b_written += (b_written % 2352) ? 2352 - (b_written % 2352):0;
		bytes2write = b_written - priv->pos;

		BRASERO_JOB_LOG (transcode,
				 "wrote %" G_GINT64_FORMAT " bytes (= %" G_GUINT64_FORMAT " ns) out of %" G_GINT64_FORMAT " (= %" G_GUINT64_FORMAT " ns)"
				 "\n=> padding %" G_GINT64_FORMAT " bytes",
				 priv->pos,
				 BRASERO_BYTES_TO_DURATION (priv->pos),
				 BRASERO_DURATION_TO_BYTES (length),
				 length,
				 bytes2write);
	}
	else {
		gint64 b_written = 0;

		/* wrote more or the exact amount of bytes. Check bytes boundary */
		b_written = priv->pos;
		bytes2write = (b_written % 2352) ? 2352 - (b_written % 2352):0;
		BRASERO_JOB_LOG (transcode,
				 "wrote %" G_GINT64_FORMAT " bytes (= %" G_GINT64_FORMAT " ns)"
				 "\n=> padding %" G_GINT64_FORMAT " bytes",
				 b_written,
				 priv->pos,
				 bytes2write);
	}

	if (!bytes2write)
		return TRUE;

	bytes2write = brasero_transcode_pad_real (transcode,
						  fd,
						  bytes2write,
						  error);
	if (bytes2write == -1)
		return TRUE;

	if (bytes2write) {
		BraseroTranscodePrivate *priv;

		priv = BRASERO_TRANSCODE_PRIVATE (transcode);
		/* when writing to a pipe it can happen that its buffer is full
		 * because cdrecord is not fast enough. Therefore we couldn't
		 * write/pad it and we'll have to wait for the pipe to become
		 * available again */
		priv->pad_fd = fd;
		priv->pad_size = bytes2write;
		priv->pad_id = g_timeout_add (50,
					     (GSourceFunc) brasero_transcode_pad_idle,
					      transcode);
		return FALSE;		
	}

	return TRUE;
}

static gboolean
brasero_transcode_pad_pipe (BraseroTranscode *transcode, GError **error)
{
	int fd;
	gboolean result;

	brasero_job_get_fd_out (BRASERO_JOB (transcode), &fd);
	fd = dup (fd);

	result = brasero_transcode_pad (transcode, fd, error);
	if (result)
		close (fd);

	return result;
}

static gboolean
brasero_transcode_pad_file (BraseroTranscode *transcode, GError **error)
{
	int fd;
	gchar *output;
	gboolean result;

	output = NULL;
	brasero_job_get_audio_output (BRASERO_JOB (transcode), &output);
	fd = open (output, O_WRONLY | O_CREAT | O_APPEND, S_IRWXU | S_IRGRP | S_IROTH);
	g_free (output);

	if (fd == -1) {
                int errsv = errno;

		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     /* Translators: %s is the string error from errno */
			     _("Error while padding file (%s)"),
			     g_strerror (errsv));
		return FALSE;
	}

	result = brasero_transcode_pad (transcode, fd, error);
	if (result)
		close (fd);

	return result;
}

static gboolean
brasero_transcode_is_mp3 (BraseroTranscode *transcode)
{
	BraseroTranscodePrivate *priv;
	GstElement *typefind;
	GstCaps *caps = NULL;
	const gchar *mime;

	priv = BRASERO_TRANSCODE_PRIVATE (transcode);

	/* find the type of the file */
	typefind = gst_bin_get_by_name (GST_BIN (priv->decode), "typefind");
	g_object_get (typefind, "caps", &caps, NULL);
	if (!caps) {
		gst_object_unref (typefind);
		return TRUE;
	}

	if (caps && gst_caps_get_size (caps) > 0) {
		mime = gst_structure_get_name (gst_caps_get_structure (caps, 0));
		gst_object_unref (typefind);

		if (mime && !strcmp (mime, "application/x-id3"))
			return TRUE;

		if (!strcmp (mime, "audio/mpeg"))
			return TRUE;
	}
	else
		gst_object_unref (typefind);

	return FALSE;
}

static gint64
brasero_transcode_get_duration (BraseroTranscode *transcode)
{
	gint64 duration = -1;
	BraseroTranscodePrivate *priv;

	priv = BRASERO_TRANSCODE_PRIVATE (transcode);

	/* This part is specific to MP3s */
	if (priv->mp3_size_pipeline) {
		/* This is the most reliable way to get the duration for mp3
		 * read them till the end and get the position. */
		gst_element_query_position (priv->pipeline,
					    GST_FORMAT_TIME,
					    &duration);
	}

	/* This is for any sort of files */
	if (duration == -1 || duration == 0)
		gst_element_query_duration (priv->pipeline,
					    GST_FORMAT_TIME,
					    &duration);

	BRASERO_JOB_LOG (transcode, "got duration %" GST_TIME_FORMAT, GST_TIME_ARGS (duration));

	if (duration == -1 || duration == 0)	
	    brasero_job_error (BRASERO_JOB (transcode),
			       g_error_new (BRASERO_BURN_ERROR,
					    BRASERO_BURN_ERROR_GENERAL, "%s",
					    _("Error while getting duration")));
	return duration;
}

static gboolean
brasero_transcode_song_end_reached (BraseroTranscode *transcode)
{
	GError *error = NULL;
	BraseroJobAction action;

	brasero_job_get_action (BRASERO_JOB (transcode), &action);
	if (action == BRASERO_JOB_ACTION_SIZE) {
		gint64 duration;

		/* this is when we need to write infs:
		 * - when asked to create infs
		 * - when decoding to a file */
		duration = brasero_transcode_get_duration (transcode);
		if (duration == -1)
			return FALSE;

		brasero_transcode_set_track_size (transcode, duration);
		brasero_job_finished_track (BRASERO_JOB (transcode));
		return TRUE;
	}

	if (action == BRASERO_JOB_ACTION_IMAGE) {
		gboolean result;

		/* pad file so it is a multiple of 2352 (= 1 sector) */
		if (brasero_job_get_fd_out (BRASERO_JOB (transcode), NULL) == BRASERO_BURN_OK)
			result = brasero_transcode_pad_pipe (transcode, &error);
		else
			result = brasero_transcode_pad_file (transcode, &error);
	
		if (error) {
			brasero_job_error (BRASERO_JOB (transcode), error);
			return FALSE;
		}

		if (!result) {
			brasero_transcode_stop_pipeline (transcode);
			return FALSE;
		}
	}

	brasero_transcode_push_track (transcode);
	return TRUE;
}

static void
foreach_tag (const GstTagList *list,
	     const gchar *tag,
	     BraseroTranscode *transcode)
{
	BraseroTrack *track;
	BraseroJobAction action;

	brasero_job_get_action (BRASERO_JOB (transcode), &action);
	brasero_job_get_current_track (BRASERO_JOB (transcode), &track);

	BRASERO_JOB_LOG (transcode, "Retrieving tags");

	if (!strcmp (tag, GST_TAG_TITLE)) {
		if (!brasero_track_tag_lookup_string (track, BRASERO_TRACK_STREAM_TITLE_TAG)) {
			gchar *title = NULL;

			gst_tag_list_get_string (list, tag, &title);
			brasero_track_tag_add_string (track,
						      BRASERO_TRACK_STREAM_TITLE_TAG,
						      title);
			g_free (title);
		}
	}
	else if (!strcmp (tag, GST_TAG_ARTIST)) {
		if (!brasero_track_tag_lookup_string (track, BRASERO_TRACK_STREAM_ARTIST_TAG)) {
			gchar *artist = NULL;

			gst_tag_list_get_string (list, tag, &artist);
			brasero_track_tag_add_string (track,
						      BRASERO_TRACK_STREAM_ARTIST_TAG,
						      artist);
			g_free (artist);
		}
	}
	else if (!strcmp (tag, GST_TAG_ISRC)) {
		if (!brasero_track_tag_lookup_string (track, BRASERO_TRACK_STREAM_ISRC_TAG)) {
			gchar *isrc = NULL;

			gst_tag_list_get_string (list, tag, &isrc);
			brasero_track_tag_add_string (track,
						      BRASERO_TRACK_STREAM_ISRC_TAG,
						      isrc);
		}
	}
	else if (!strcmp (tag, GST_TAG_PERFORMER)) {
		if (!brasero_track_tag_lookup_string (track, BRASERO_TRACK_STREAM_ARTIST_TAG)) {
			gchar *artist = NULL;

			gst_tag_list_get_string (list, tag, &artist);
			brasero_track_tag_add_string (track,
						      BRASERO_TRACK_STREAM_ARTIST_TAG,
						      artist);
			g_free (artist);
		}
	}
	else if (action == BRASERO_JOB_ACTION_SIZE
	     &&  !strcmp (tag, GST_TAG_DURATION)) {
		guint64 duration;

		/* this is only useful when we try to have the size */
		gst_tag_list_get_uint64 (list, tag, &duration);
		brasero_track_stream_set_boundaries (BRASERO_TRACK_STREAM (track), 0, duration, -1);
	}
}

/* NOTE: the return value is whether or not we should stop the bus callback */
static gboolean
brasero_transcode_active_state (BraseroTranscode *transcode)
{
	BraseroTranscodePrivate *priv;
	gchar *name, *string, *uri;
	BraseroJobAction action;
	BraseroTrack *track;

	priv = BRASERO_TRANSCODE_PRIVATE (transcode);

	if (priv->set_active_state)
		return TRUE;

	priv->set_active_state = TRUE;

	brasero_job_get_current_track (BRASERO_JOB (transcode), &track);
	uri = brasero_track_stream_get_source (BRASERO_TRACK_STREAM (track), FALSE);

	brasero_job_get_action (BRASERO_JOB (transcode), &action);
	if (action == BRASERO_JOB_ACTION_SIZE) {
		BRASERO_JOB_LOG (transcode,
				 "Analysing Track %s",
				 uri);

		if (priv->mp3_size_pipeline) {
			gchar *escaped_basename;

			/* Run the pipeline till the end */
			escaped_basename = g_path_get_basename (uri);
			name = g_uri_unescape_string (escaped_basename, NULL);
			g_free (escaped_basename);

			string = g_strdup_printf (_("Analysing \"%s\""), name);
			g_free (name);
		
			brasero_job_set_current_action (BRASERO_JOB (transcode),
							BRASERO_BURN_ACTION_ANALYSING,
							string,
							TRUE);
			g_free (string);

			brasero_job_start_progress (BRASERO_JOB (transcode), FALSE);
			g_free (uri);
			return TRUE;
		}

		if (brasero_transcode_is_mp3 (transcode)) {
			GError *error = NULL;

			/* Rebuild another pipeline which is specific to MP3s. */
			priv->mp3_size_pipeline = TRUE;
			brasero_transcode_stop_pipeline (transcode);

			if (!brasero_transcode_create_pipeline (transcode, &error))
				brasero_job_error (BRASERO_JOB (transcode), error);
		}
		else
			brasero_transcode_song_end_reached (transcode);

		g_free (uri);
		return FALSE;
	}
	else {
		gchar *escaped_basename;

		escaped_basename = g_path_get_basename (uri);
		name = g_uri_unescape_string (escaped_basename, NULL);
		g_free (escaped_basename);

		string = g_strdup_printf (_("Transcoding \"%s\""), name);
		g_free (name);

		brasero_job_set_current_action (BRASERO_JOB (transcode),
						BRASERO_BURN_ACTION_TRANSCODING,
						string,
						TRUE);
		g_free (string);
		brasero_job_start_progress (BRASERO_JOB (transcode), FALSE);

		if (brasero_job_get_fd_out (BRASERO_JOB (transcode), NULL) != BRASERO_BURN_OK) {
			gchar *dest = NULL;

			brasero_job_get_audio_output (BRASERO_JOB (transcode), &dest);
			BRASERO_JOB_LOG (transcode,
					 "start decoding %s to %s",
					 uri,
					 dest);
			g_free (dest);
		}
		else
			BRASERO_JOB_LOG (transcode,
					 "start piping %s",
					 uri)
	}

	g_free (uri);
	return TRUE;
}

static gboolean
brasero_transcode_bus_messages (GstBus *bus,
				GstMessage *msg,
				BraseroTranscode *transcode)
{
	BraseroTranscodePrivate *priv;
	GstTagList *tags = NULL;
	GError *error = NULL;
	GstState state;
	gchar *debug;

	priv = BRASERO_TRANSCODE_PRIVATE (transcode);
	switch (GST_MESSAGE_TYPE (msg)) {
	case GST_MESSAGE_TAG:
		/* we use the information to write an .inf file 
		 * for the time being just store the information */
		gst_message_parse_tag (msg, &tags);
		gst_tag_list_foreach (tags, (GstTagForeachFunc) foreach_tag, transcode);
		gst_tag_list_free (tags);
		return TRUE;

	case GST_MESSAGE_ERROR:
		gst_message_parse_error (msg, &error, &debug);
		BRASERO_JOB_LOG (transcode, "%s", debug);
		g_free (debug);

	        brasero_job_error (BRASERO_JOB (transcode), error);
		return FALSE;

	case GST_MESSAGE_EOS:
		brasero_transcode_song_end_reached (transcode);
		return FALSE;

	case GST_MESSAGE_STATE_CHANGED: {
		GstStateChangeReturn result;

		result = gst_element_get_state (priv->pipeline,
						&state,
						NULL,
						1);

		if (result != GST_STATE_CHANGE_SUCCESS)
			return TRUE;

		if (state == GST_STATE_PLAYING)
			return brasero_transcode_active_state (transcode);

		break;
	}

	default:
		return TRUE;
	}

	return TRUE;
}

static void
brasero_transcode_new_decoded_pad_cb (GstElement *decode,
				      GstPad *pad,
				      BraseroTranscode *transcode)
{
	GstCaps *caps;
	GstStructure *structure;
	BraseroTranscodePrivate *priv;

	priv = BRASERO_TRANSCODE_PRIVATE (transcode);

	BRASERO_JOB_LOG (transcode, "New pad");

	/* make sure we only have audio */
	/* FIXME: get_current_caps() doesn't always seem to work yet here */
	caps = gst_pad_query_caps (pad, NULL);
	if (!caps)
		return;

	structure = gst_caps_get_structure (caps, 0);
	if (structure) {
		if (g_strrstr (gst_structure_get_name (structure), "audio")) {
			GstPad *sink;
			GstElement *queue;
			GstPadLinkReturn res;

			/* before linking pads (before any data reach grvolume), send tags */
			brasero_transcode_send_volume_event (transcode);

			/* This is necessary in case there is a video stream
			 * (see brasero-metadata.c). we need to queue to avoid
			 * a deadlock. */
			queue = gst_element_factory_make ("queue", NULL);
			gst_bin_add (GST_BIN (priv->pipeline), queue);
			if (!gst_element_link (queue, priv->link)) {
				brasero_transcode_error_on_pad_linking (transcode, "Sent by brasero_transcode_new_decoded_pad_cb");
				goto end;
			}

			sink = gst_element_get_static_pad (queue, "sink");
			if (GST_PAD_IS_LINKED (sink)) {
				brasero_transcode_error_on_pad_linking (transcode, "Sent by brasero_transcode_new_decoded_pad_cb");
				goto end;
			}

			res = gst_pad_link (pad, sink);
			if (res == GST_PAD_LINK_OK)
				gst_element_set_state (queue, GST_STATE_PLAYING);
			else
				brasero_transcode_error_on_pad_linking (transcode, "Sent by brasero_transcode_new_decoded_pad_cb");

			gst_object_unref (sink);
		}
		/* For video streams add a fakesink (see brasero-metadata.c) */
		else if (g_strrstr (gst_structure_get_name (structure), "video")) {
			GstPad *sink;
			GstElement *fakesink;
			GstPadLinkReturn res;

			BRASERO_JOB_LOG (transcode, "Adding a fakesink for video stream");

			fakesink = gst_element_factory_make ("fakesink", NULL);
			if (!fakesink) {
				brasero_transcode_error_on_pad_linking (transcode, "Sent by brasero_transcode_new_decoded_pad_cb");
				goto end;
			}

			sink = gst_element_get_static_pad (fakesink, "sink");
			if (!sink) {
				brasero_transcode_error_on_pad_linking (transcode, "Sent by brasero_transcode_new_decoded_pad_cb");
				gst_object_unref (fakesink);
				goto end;
			}

			gst_bin_add (GST_BIN (priv->pipeline), fakesink);
			res = gst_pad_link (pad, sink);

			if (res == GST_PAD_LINK_OK)
				gst_element_set_state (fakesink, GST_STATE_PLAYING);
			else
				brasero_transcode_error_on_pad_linking (transcode, "Sent by brasero_transcode_new_decoded_pad_cb");

			gst_object_unref (sink);
		}
	}

end:
	gst_caps_unref (caps);
}

static BraseroBurnResult
brasero_transcode_clock_tick (BraseroJob *job)
{
	BraseroTranscodePrivate *priv;

	priv = BRASERO_TRANSCODE_PRIVATE (job);

	if (!priv->pipeline)
		return BRASERO_BURN_ERR;

	brasero_job_set_written_track (job, priv->pos);
	return BRASERO_BURN_OK;
}

static void
brasero_transcode_class_init (BraseroTranscodeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroJobClass *job_class = BRASERO_JOB_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroTranscodePrivate));

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_transcode_finalize;

	job_class->start = brasero_transcode_start;
	job_class->clock_tick = brasero_transcode_clock_tick;
	job_class->stop = brasero_transcode_stop;
}

static void
brasero_transcode_init (BraseroTranscode *obj)
{ }

static void
brasero_transcode_finalize (GObject *object)
{
	BraseroTranscodePrivate *priv;

	priv = BRASERO_TRANSCODE_PRIVATE (object);

	if (priv->pad_id) {
		g_source_remove (priv->pad_id);
		priv->pad_id = 0;
	}

	brasero_transcode_stop_pipeline (BRASERO_TRANSCODE (object));

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_transcode_export_caps (BraseroPlugin *plugin)
{
	GSList *input;
	GSList *output;

	brasero_plugin_define (plugin,
			       "transcode",
	                       NULL,
			       _("Converts any song file into a format suitable for audio CDs"),
			       "Philippe Rouquier",
			       1);

	output = brasero_caps_audio_new (BRASERO_PLUGIN_IO_ACCEPT_FILE|
					 BRASERO_PLUGIN_IO_ACCEPT_PIPE,
					 BRASERO_AUDIO_FORMAT_RAW|
					 BRASERO_AUDIO_FORMAT_RAW_LITTLE_ENDIAN|
					 BRASERO_METADATA_INFO);

	input = brasero_caps_audio_new (BRASERO_PLUGIN_IO_ACCEPT_FILE,
					BRASERO_AUDIO_FORMAT_UNDEFINED|
					BRASERO_METADATA_INFO);

	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (input);

	input = brasero_caps_audio_new (BRASERO_PLUGIN_IO_ACCEPT_FILE,
					BRASERO_AUDIO_FORMAT_DTS|
					BRASERO_METADATA_INFO);
	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (output);
	g_slist_free (input);

	output = brasero_caps_audio_new (BRASERO_PLUGIN_IO_ACCEPT_FILE|
					 BRASERO_PLUGIN_IO_ACCEPT_PIPE,
					 BRASERO_AUDIO_FORMAT_RAW|
					 BRASERO_AUDIO_FORMAT_RAW_LITTLE_ENDIAN);

	input = brasero_caps_audio_new (BRASERO_PLUGIN_IO_ACCEPT_FILE,
					BRASERO_AUDIO_FORMAT_UNDEFINED);

	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (input);

	input = brasero_caps_audio_new (BRASERO_PLUGIN_IO_ACCEPT_FILE,
					BRASERO_AUDIO_FORMAT_DTS);

	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (output);
	g_slist_free (input);
}
