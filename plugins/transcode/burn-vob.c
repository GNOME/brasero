/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero-vob.c
 * Copyright (C) Rouquier Philippe 2008 <bonfire-app@wanadoo.fr>
 * 
 * brasero-vob.c is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * brasero-vob.c is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with brasero-vob.c.  If not, write to:
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
#include <gmodule.h>

#include <gst/gst.h>

#include "brasero-tags.h"
#include "burn-job.h"
#include "brasero-plugin-registration.h"
#include "burn-vob.h"

BRASERO_PLUGIN_BOILERPLATE (BraseroVob, brasero_vob, BRASERO_TYPE_JOB, BraseroJob);

typedef struct _BraseroVobPrivate BraseroVobPrivate;
struct _BraseroVobPrivate
{
	GstElement *pipeline;

	GstElement *audio;
	GstElement *video;

	BraseroStreamFormat format;

	guint svcd:1;
	guint is_video_dvd:1;
};

#define BRASERO_VOB_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_VOB, BraseroVobPrivate))

static GObjectClass *parent_class = NULL;


static void
brasero_vob_stop_pipeline (BraseroVob *vob)
{
	BraseroVobPrivate *priv;

	priv = BRASERO_VOB_PRIVATE (vob);
	if (!priv->pipeline)
		return;

	gst_element_set_state (priv->pipeline, GST_STATE_NULL);
	gst_object_unref (GST_OBJECT (priv->pipeline));
	priv->pipeline = NULL;
}

static BraseroBurnResult
brasero_vob_stop (BraseroJob *job,
		  GError **error)
{
	BraseroVobPrivate *priv;

	priv = BRASERO_VOB_PRIVATE (job);

	brasero_vob_stop_pipeline (BRASERO_VOB (job));
	return BRASERO_BURN_OK;
}

static void
brasero_vob_finished (BraseroVob *vob)
{
	BraseroTrackType *type = NULL;
	BraseroTrackStream *track;
	BraseroVobPrivate *priv;
	gchar *output = NULL;

	priv = BRASERO_VOB_PRIVATE (vob);

	type = brasero_track_type_new ();
	brasero_job_get_output_type (BRASERO_JOB (vob), type);
	brasero_job_get_audio_output (BRASERO_JOB (vob), &output);

	track = brasero_track_stream_new ();
	brasero_track_stream_set_source (track, output);
	brasero_track_stream_set_format (track, brasero_track_type_get_stream_format (type));

	brasero_job_add_track (BRASERO_JOB (vob), BRASERO_TRACK (track));
	g_object_unref (track);

	brasero_track_type_free (type);
	g_free (output);

	brasero_job_finished_track (BRASERO_JOB (vob));
}

static gboolean
brasero_vob_bus_messages (GstBus *bus,
			  GstMessage *msg,
			  BraseroVob *vob)
{
	BraseroVobPrivate *priv;
	GError *error = NULL;
	gchar *debug;

	priv = BRASERO_VOB_PRIVATE (vob);
	switch (GST_MESSAGE_TYPE (msg)) {
	case GST_MESSAGE_TAG:
		return TRUE;

	case GST_MESSAGE_ERROR:
		gst_message_parse_error (msg, &error, &debug);
		BRASERO_JOB_LOG (vob, debug);
		g_free (debug);

	        brasero_job_error (BRASERO_JOB (vob), error);
		return FALSE;

	case GST_MESSAGE_EOS:
		BRASERO_JOB_LOG (vob, "Transcoding finished");

		/* add a new track and terminate */
		brasero_vob_finished (vob);
		return FALSE;

	case GST_MESSAGE_STATE_CHANGED:
		break;

	default:
		return TRUE;
	}

	return TRUE;
}

static void
brasero_vob_new_decoded_pad_cb (GstElement *decode,
				GstPad *pad,
				gboolean arg2,
				BraseroVob *vob)
{
	GstPad *sink;
	GstCaps *caps;
	GstStructure *structure;
	BraseroVobPrivate *priv;

	priv = BRASERO_VOB_PRIVATE (vob);

	/* make sure we only have audio */
	caps = gst_pad_get_caps (pad);
	if (!caps)
		return;

	structure = gst_caps_get_structure (caps, 0);
	if (structure) {
		if (g_strrstr (gst_structure_get_name (structure), "video")) {
			sink = gst_element_get_pad (priv->video, "sink");
			gst_pad_link (pad, sink);
			gst_object_unref (sink);

			gst_element_set_state (priv->video, GST_STATE_PLAYING);
		}

		if (g_strrstr (gst_structure_get_name (structure), "audio")) {
			sink = gst_element_get_pad (priv->audio, "sink");
			gst_pad_link (pad, sink);
			gst_object_unref (sink);

			gst_element_set_state (priv->audio, GST_STATE_PLAYING);
		}
	}

	gst_caps_unref (caps);
}

static gboolean
brasero_vob_link_audio (BraseroVob *vob,
			GstElement *start,
			GstElement *end,
			GstElement *tee,
			GstElement *muxer)
{
	GstPad *srcpad;
	GstPad *sinkpad;
	GstPadLinkReturn res;

	srcpad = gst_element_get_request_pad (tee, "src%d");
	sinkpad = gst_element_get_static_pad (start, "sink");
	res = gst_pad_link (srcpad, sinkpad);
	gst_object_unref (sinkpad);
	gst_object_unref (srcpad);

	BRASERO_JOB_LOG (vob, "Linked audio bin to tee == %d", res);
	if (res != GST_PAD_LINK_OK)
		return FALSE;

	sinkpad = gst_element_get_request_pad (muxer, "audio_%d");
	srcpad = gst_element_get_static_pad (end, "src");
	res = gst_pad_link (srcpad, sinkpad);
	gst_object_unref (sinkpad);
	gst_object_unref (srcpad);

	BRASERO_JOB_LOG (vob, "Linked audio bin to muxer == %d", res);
	if (res != GST_PAD_LINK_OK)
		return FALSE;

	return TRUE;
}

static gboolean
brasero_vob_build_audio_pcm (BraseroVob *vob,
			     GstElement *tee,
			     GstElement *muxer,
			     GError **error)
{
	GstElement *queue;
	GstElement *queue1;
	GstElement *filter;
	GstElement *convert;
	GstCaps *filtercaps;
	GstElement *resample;
	BraseroVobPrivate *priv;

	priv = BRASERO_VOB_PRIVATE (vob);

	/* NOTE: this can only be used for Video DVDs as PCM cannot be used for
	 * (S)VCDs. */

	/* queue */
	queue = gst_element_factory_make ("queue", NULL);
	if (queue == NULL) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     /* Translators: %s is the name of the GstElement that 
			      * could not be created */
			     _("%s element could not be created"),
			     "\"Queue\"");
		goto error;
	}
	gst_bin_add (GST_BIN (priv->pipeline), queue);

	/* audioresample */
	resample = gst_element_factory_make ("audioresample", NULL);
	if (resample == NULL) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     /* Translators: %s is the name of the GstElement that 
			      * could not be created */
			     _("%s element could not be created"),
			     "\"Audioresample\"");
		goto error;
	}
	gst_bin_add (GST_BIN (priv->pipeline), resample);

	/* audioconvert */
	convert = gst_element_factory_make ("audioconvert", NULL);
	if (convert == NULL) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     /* Translators: %s is the name of the GstElement that 
			      * element could not be created */
			     _("%s element could not be created"),
			     "\"Audioconvert\"");
		goto error;
	}
	gst_bin_add (GST_BIN (priv->pipeline), convert);

	/* another queue */
	queue1 = gst_element_factory_make ("queue", NULL);
	if (queue1 == NULL) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Queue1\"");
		goto error;
	}
	gst_bin_add (GST_BIN (priv->pipeline), queue1);

	/* create a filter */
	filter = gst_element_factory_make ("capsfilter", NULL);
	if (filter == NULL) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Filter\"");
		goto error;
	}
	gst_bin_add (GST_BIN (priv->pipeline), filter);

	/* NOTE: what about the number of channels (up to 6) ? */
	filtercaps = gst_caps_new_full (gst_structure_new ("audio/x-raw-int",
							   "width", G_TYPE_INT, 16,
							   "depth", G_TYPE_INT, 16,
							   "rate", G_TYPE_INT, 48000,
							   NULL),
					NULL);

	g_object_set (GST_OBJECT (filter), "caps", filtercaps, NULL);
	gst_caps_unref (filtercaps);

	gst_element_link_many (queue, resample, convert, filter, queue1, NULL);
	brasero_vob_link_audio (vob, queue, queue1, tee, muxer);

	return TRUE;

error:

	return FALSE;
}

static gboolean
brasero_vob_build_audio_mp2 (BraseroVob *vob,
			     GstElement *tee,
			     GstElement *muxer,
			     GError **error)
{
	GstElement *queue;
	GstElement *queue1;
	GstElement *encode;
	GstElement *filter;
	GstElement *convert;
	GstCaps *filtercaps;
	GstElement *resample;
	BraseroVobPrivate *priv;

	priv = BRASERO_VOB_PRIVATE (vob);

	/* queue */
	queue = gst_element_factory_make ("queue", NULL);
	if (queue == NULL) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Queue\"");
		goto error;
	}
	gst_bin_add (GST_BIN (priv->pipeline), queue);

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
	gst_bin_add (GST_BIN (priv->pipeline), convert);

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
	gst_bin_add (GST_BIN (priv->pipeline), resample);

	/* encoder */
	encode = gst_element_factory_make ("ffenc_mp2", NULL);
	if (encode == NULL) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"ffenc_mp2\"");
		goto error;
	}
	gst_bin_add (GST_BIN (priv->pipeline), encode);

	/* another queue */
	queue1 = gst_element_factory_make ("queue", NULL);
	if (queue1 == NULL) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Queue1\"");
		goto error;
	}
	gst_bin_add (GST_BIN (priv->pipeline), queue1);

	/* create a filter */
	filter = gst_element_factory_make ("capsfilter", NULL);
	if (filter == NULL) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Filter\"");
		goto error;
	}
	gst_bin_add (GST_BIN (priv->pipeline), filter);

	if (priv->is_video_dvd) {
		BRASERO_JOB_LOG (vob, "Setting mp2 bitrate to 448000, 48000 khz");
		g_object_set (encode,
			      "bitrate", 448000, /* it could go up to 912k */
			      NULL);

		/* NOTE: what about the number of channels (up to 7.1) ? */
		filtercaps = gst_caps_new_full (gst_structure_new ("audio/x-raw-int",
								   "rate", G_TYPE_INT, 48000,
								   NULL),
						NULL);
	}
	else if (!priv->svcd) {
		/* VCD */
		BRASERO_JOB_LOG (vob, "Setting mp2 bitrate to 224000, 44100 khz");
		g_object_set (encode,
			      "bitrate", 224000,
			      NULL);

		/* 2 channels tops */
		filtercaps = gst_caps_new_full (gst_structure_new ("audio/x-raw-int",
								   "channels", G_TYPE_INT, 2,
								   "rate", G_TYPE_INT, 44100,
								   NULL),
						NULL);
	}
	else {
		/* SVCDs */
		BRASERO_JOB_LOG (vob, "Setting mp2 bitrate to 384000, 44100 khz");
		g_object_set (encode,
			      "bitrate", 384000,
			      NULL);

		/* NOTE: channels up to 5.1 or dual */
		filtercaps = gst_caps_new_full (gst_structure_new ("audio/x-raw-int",
								   "rate", G_TYPE_INT, 44100,
								   NULL),
						NULL);
	}

	g_object_set (GST_OBJECT (filter), "caps", filtercaps, NULL);
	gst_caps_unref (filtercaps);

	gst_element_link_many (queue, convert, resample, filter, encode, queue1, NULL);

	brasero_vob_link_audio (vob, queue, queue1, tee, muxer);
	return TRUE;

error:

	return FALSE;
}

static gboolean
brasero_vob_build_audio_ac3 (BraseroVob *vob,
			     GstElement *tee,
			     GstElement *muxer,
			     GError **error)
{
	GstElement *queue;
	GstElement *queue1;
	GstElement *filter;
	GstElement *encode;
	GstElement *convert;
	GstCaps *filtercaps;
	GstElement *resample;
	BraseroVobPrivate *priv;

	priv = BRASERO_VOB_PRIVATE (vob);

	/* NOTE: This has to be a DVD since AC3 is not supported by (S)VCD */

	/* queue */
	queue = gst_element_factory_make ("queue", NULL);
	if (queue == NULL) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Queue\"");
		goto error;
	}
	gst_bin_add (GST_BIN (priv->pipeline), queue);

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
	gst_bin_add (GST_BIN (priv->pipeline), convert);

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
	gst_bin_add (GST_BIN (priv->pipeline), resample);

	/* create a filter */
	filter = gst_element_factory_make ("capsfilter", NULL);
	if (filter == NULL) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Filter\"");
		goto error;
	}
	gst_bin_add (GST_BIN (priv->pipeline), filter);

	BRASERO_JOB_LOG (vob, "Setting AC3 rate to 48000");
	/* NOTE: we may want to limit the number of channels. */
	filtercaps = gst_caps_new_full (gst_structure_new ("audio/x-raw-int",
							   "rate", G_TYPE_INT, 48000,
							   NULL),
					NULL);

	g_object_set (GST_OBJECT (filter), "caps", filtercaps, NULL);
	gst_caps_unref (filtercaps);

	/* encoder */
	encode = gst_element_factory_make ("ffenc_ac3", NULL);
	if (encode == NULL) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Ffenc_ac3\"");
		goto error;
	}
	gst_bin_add (GST_BIN (priv->pipeline), encode);

	BRASERO_JOB_LOG (vob, "Setting AC3 bitrate to 448000");
	g_object_set (encode,
		      "bitrate", 448000, /* Maximum allowed, is it useful ? */
		      NULL);

	/* another queue */
	queue1 = gst_element_factory_make ("queue", NULL);
	if (queue1 == NULL) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Queue1\"");
		goto error;
	}
	gst_bin_add (GST_BIN (priv->pipeline), queue1);

	gst_element_link_many (queue, convert, resample, filter, encode, queue1, NULL);
	brasero_vob_link_audio (vob, queue, queue1, tee, muxer);

	return TRUE;

error:

	return FALSE;
}

static GstElement *
brasero_vob_build_audio_bins (BraseroVob *vob,
			      GstElement *muxer,
			      GError **error)
{
	GValue *value;
	GstElement *tee;
	BraseroVobPrivate *priv;

	priv = BRASERO_VOB_PRIVATE (vob);

	/* tee */
	tee = gst_element_factory_make ("tee", NULL);
	if (tee == NULL) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Tee\"");
		goto error;
	}
	gst_bin_add (GST_BIN (priv->pipeline), tee);

	if (priv->is_video_dvd) {
		/* Get output format */
		value = NULL;
		brasero_job_tag_lookup (BRASERO_JOB (vob),
					BRASERO_DVD_STREAM_FORMAT,
					&value);

		if (value)
			priv->format = g_value_get_int (value);

		if (priv->format == BRASERO_AUDIO_FORMAT_NONE)
			priv->format = BRASERO_AUDIO_FORMAT_RAW;

		/* FIXME: for the moment even if we use tee, we can't actually
		 * encode and use more than one audio stream */
		if (priv->format & BRASERO_AUDIO_FORMAT_RAW) {
			/* PCM : on demand */
			BRASERO_JOB_LOG (vob, "Adding PCM audio stream");
			if (!brasero_vob_build_audio_pcm (vob, tee, muxer, error))
				goto error;
		}

		if (priv->format & BRASERO_AUDIO_FORMAT_AC3) {
			/* AC3 : on demand */
			BRASERO_JOB_LOG (vob, "Adding AC3 audio stream");
			if (!brasero_vob_build_audio_ac3 (vob, tee, muxer, error))
				goto error;
		}

		if (priv->format & BRASERO_AUDIO_FORMAT_MP2) {
			/* MP2 : on demand */
			BRASERO_JOB_LOG (vob, "Adding MP2 audio stream");
			if (!brasero_vob_build_audio_mp2 (vob, tee, muxer, error))
				goto error;
		}
	}
	else if (!brasero_vob_build_audio_mp2 (vob, tee, muxer, error))
		goto error;

	return tee;

error:
	return NULL;
}

static GstElement *
brasero_vob_build_video_bin (BraseroVob *vob,
			     GstElement *muxer,
			     GError **error)
{
	GValue *value;
	GstPad *srcpad;
	GstPad *sinkpad;
	GstElement *scale;
	GstElement *queue;
	GstElement *queue1;
	GstElement *filter;
	GstElement *encode;
	GstPadLinkReturn res;
	GstElement *framerate;
	GstElement *colorspace;
	BraseroVobPrivate *priv;
	BraseroBurnResult result;

	priv = BRASERO_VOB_PRIVATE (vob);

	queue = gst_element_factory_make ("queue", NULL);
	if (queue == NULL) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Queue\"");
		goto error;
	}
	gst_bin_add (GST_BIN (priv->pipeline), queue);

	/* framerate and video type control */
	framerate = gst_element_factory_make ("videorate", NULL);
	if (framerate == NULL) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Framerate\"");
		goto error;
	}
	gst_bin_add (GST_BIN (priv->pipeline), framerate);
	g_object_set (framerate,
		      "silent", TRUE,
		      NULL);

	/* size scaling */
	scale = gst_element_factory_make ("videoscale", NULL);
	if (scale == NULL) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Videoscale\"");
		goto error;
	}
	gst_bin_add (GST_BIN (priv->pipeline), scale);

	/* create a filter */
	filter = gst_element_factory_make ("capsfilter", NULL);
	if (filter == NULL) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Filter\"");
		goto error;
	}
	gst_bin_add (GST_BIN (priv->pipeline), filter);

	colorspace = gst_element_factory_make ("ffmpegcolorspace", NULL);
	if (colorspace == NULL) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Ffmepgcolorspace\"");
		goto error;
	}
	gst_bin_add (GST_BIN (priv->pipeline), colorspace);

	encode = gst_element_factory_make ("mpeg2enc", NULL);
	if (encode == NULL) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Mpeg2enc\"");
		goto error;
	}
	gst_bin_add (GST_BIN (priv->pipeline), encode);

	if (priv->is_video_dvd)
		g_object_set (encode,
			      "format", 8,
			      NULL);
	else if (priv->svcd) {
		/* Option to improve compatibility with vcdimager */
		g_object_set (encode,
			      "dummy-svcd-sof", TRUE,
			      NULL);
		g_object_set (encode,
			      "format", 4,
			      NULL);
	}
	else
		g_object_set (encode,
			      "format", 1,
			      NULL);

	/* settings */
	value = NULL;
	result = brasero_job_tag_lookup (BRASERO_JOB (vob),
					 BRASERO_VIDEO_OUTPUT_FRAMERATE,
					 &value);

	if (result == BRASERO_BURN_OK && value) {
		gint rate;
		GstCaps *filtercaps = NULL;

		rate = g_value_get_int (value);

		if (rate == BRASERO_VIDEO_FRAMERATE_NTSC) {
			g_object_set (encode,
				      "norm", 110,
				      "framerate", 4,
				      NULL);

			if (priv->is_video_dvd)
				filtercaps = gst_caps_new_full (gst_structure_new ("video/x-raw-yuv",
										   "framerate", GST_TYPE_FRACTION, 30000, 1001,
										   "width", G_TYPE_INT, 720,
										   "height", G_TYPE_INT, 480,
										   NULL),
								gst_structure_new ("video/x-raw-rgb",
										   "framerate", GST_TYPE_FRACTION, 30000, 1001,
										   "width", G_TYPE_INT, 720,
										   "height", G_TYPE_INT, 480,
										   NULL),
								NULL);
			else if (priv->svcd)
				filtercaps = gst_caps_new_full (gst_structure_new ("video/x-raw-yuv",
										   "framerate", GST_TYPE_FRACTION, 30000, 1001,
										   "width", G_TYPE_INT, 480,
										   "height", G_TYPE_INT, 480,
										   NULL),
								gst_structure_new ("video/x-raw-rgb",
										   "framerate", GST_TYPE_FRACTION, 30000, 1001,
										   "width", G_TYPE_INT, 480,
										   "height", G_TYPE_INT, 480,
										   NULL),
								NULL);
			else
				filtercaps = gst_caps_new_full (gst_structure_new ("video/x-raw-yuv",
										   "framerate", GST_TYPE_FRACTION, 30000, 1001,
										   "width", G_TYPE_INT, 352,
										   "height", G_TYPE_INT, 240,
										   NULL),
								gst_structure_new ("video/x-raw-rgb",
										   "framerate", GST_TYPE_FRACTION, 30000, 1001,
										   "width", G_TYPE_INT, 352,
										   "height", G_TYPE_INT, 240,
										   NULL),
								NULL);
		}
		else if (rate == BRASERO_VIDEO_FRAMERATE_PAL_SECAM) {
			g_object_set (encode,
				      "norm", 112,
				      "framerate", 3,
				      NULL);

			if (priv->is_video_dvd)
				filtercaps = gst_caps_new_full (gst_structure_new ("video/x-raw-yuv",
										   "framerate", GST_TYPE_FRACTION, 25, 1,
										   "width", G_TYPE_INT, 720,
										   "height", G_TYPE_INT, 576,
										   NULL),
								gst_structure_new ("video/x-raw-rgb",
										   "framerate", GST_TYPE_FRACTION, 25, 1,
										   "width", G_TYPE_INT, 720,
										   "height", G_TYPE_INT, 576,
										   NULL),
								NULL);
			else if (priv->svcd)
				filtercaps = gst_caps_new_full (gst_structure_new ("video/x-raw-yuv",
										   "framerate", GST_TYPE_FRACTION, 25, 1,
										   "width", G_TYPE_INT, 480,
										   "height", G_TYPE_INT, 576,
										   NULL),
								gst_structure_new ("video/x-raw-rgb",
										   "framerate", GST_TYPE_FRACTION, 25, 1,
										   "width", G_TYPE_INT, 480,
										   "height", G_TYPE_INT, 576,
										   NULL),
								NULL);
			else
				filtercaps = gst_caps_new_full (gst_structure_new ("video/x-raw-yuv",
										   "framerate", GST_TYPE_FRACTION, 25, 1,
										   "width", G_TYPE_INT, 352,
										   "height", G_TYPE_INT, 288,
										   NULL),
								gst_structure_new ("video/x-raw-rgb",
										   "framerate", GST_TYPE_FRACTION, 25, 1,
										   "width", G_TYPE_INT, 352,
										   "height", G_TYPE_INT, 288,
										   NULL),
								NULL);
		}

		if (filtercaps) {
			g_object_set (GST_OBJECT (filter), "caps", filtercaps, NULL);
			gst_caps_unref (filtercaps);
		}
	}

	if (priv->is_video_dvd || priv->svcd) {
		value = NULL;
		result = brasero_job_tag_lookup (BRASERO_JOB (vob),
						 BRASERO_VIDEO_OUTPUT_ASPECT,
						 &value);
		if (result == BRASERO_BURN_OK && value) {
			gint aspect;

			aspect = g_value_get_int (value);
			if (aspect == BRASERO_VIDEO_ASPECT_4_3) {
				BRASERO_JOB_LOG (vob, "Setting ration 4:3");
				g_object_set (encode,
					      "aspect", 2,
					      NULL);
			}
			else if (aspect == BRASERO_VIDEO_ASPECT_16_9) {
				BRASERO_JOB_LOG (vob, "Setting ration 16:9");
				g_object_set (encode,
					      "aspect", 3,
					      NULL);	
			}
		}
	}
	else {
		/* VCDs only support 4:3 */
		BRASERO_JOB_LOG (vob, "Setting ration 4:3");
		g_object_set (encode,
			      "aspect", 2,
			      NULL);
	}

	/* another queue */
	queue1 = gst_element_factory_make ("queue", NULL);
	if (queue1 == NULL) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Queue1\"");
		goto error;
	}
	gst_bin_add (GST_BIN (priv->pipeline), queue1);

	gst_element_link_many (queue, framerate, scale, colorspace, filter, encode, queue1, NULL);

	srcpad = gst_element_get_static_pad (queue1, "src");
	sinkpad = gst_element_get_request_pad (muxer, "video_%d");
	res = gst_pad_link (srcpad, sinkpad);
	BRASERO_JOB_LOG (vob, "Linked video bin to muxer == %d", res)
	gst_object_unref (sinkpad);
	gst_object_unref (srcpad);

	return queue;

error:

	return NULL;
}

static gboolean
brasero_vob_build_pipeline (BraseroVob *vob,
			    GError **error)
{
	gchar *uri;
	GstBus *bus;
	gchar *output;
	GstElement *sink;
	GstElement *muxer;
	GstElement *source;
	GstElement *decode;
	BraseroTrack *track;
	GstElement *pipeline;
	BraseroVobPrivate *priv;

	priv = BRASERO_VOB_PRIVATE (vob);

	BRASERO_JOB_LOG (vob, "Creating new pipeline");

	pipeline = gst_pipeline_new (NULL);
	priv->pipeline = pipeline;

	/* source */
	brasero_job_get_current_track (BRASERO_JOB (vob), &track);
	uri = brasero_track_stream_get_source (BRASERO_TRACK_STREAM (track), TRUE);
	source = gst_element_make_from_uri (GST_URI_SRC, uri, NULL);
	if (source == NULL) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Source\"");
		return FALSE;
	}
	gst_bin_add (GST_BIN (pipeline), source);
	g_object_set (source,
		      "typefind", FALSE,
		      NULL);

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
	gst_element_link_many (source, decode, NULL);

	/* muxer: "mplex" */
	muxer = gst_element_factory_make ("mplex", NULL);
	if (muxer == NULL) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Mplex\"");
		goto error;
	}
	gst_bin_add (GST_BIN (pipeline), muxer);

	if (priv->is_video_dvd)
		g_object_set (muxer,
			      "format", 8,
			      NULL);
	else if (priv->svcd)
		g_object_set (muxer,
			      "format", 4,
			      NULL);
	else	/* This should be 1 but it causes frame drops */
		g_object_set (muxer,
			      "format", 4,
			      NULL);

	/* create sink */
	output = NULL;
	brasero_job_get_audio_output (BRASERO_JOB (vob), &output);
	sink = gst_element_factory_make ("filesink", NULL);
	if (sink == NULL) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Sink\"");
		return FALSE;
	}
	g_object_set (sink,
		      "location", output,
		      NULL);

	gst_bin_add (GST_BIN (pipeline), sink);
	gst_element_link (muxer, sink);

	/* video encoding */
	priv->video = brasero_vob_build_video_bin (vob, muxer, error);
	if (!priv->video)
		goto error;

	/* audio encoding */
	priv->audio = brasero_vob_build_audio_bins (vob, muxer, error);
	if (!priv->audio)
		goto error;

	/* to be able to link everything */
	g_signal_connect (G_OBJECT (decode),
			  "new-decoded-pad",
			  G_CALLBACK (brasero_vob_new_decoded_pad_cb),
			  vob);

	/* connect to the bus */	
	bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
	gst_bus_add_watch (bus,
			   (GstBusFunc) brasero_vob_bus_messages,
			   vob);
	gst_object_unref (bus);

	return TRUE;

error:

	if (error && (*error))
		BRASERO_JOB_LOG (vob,
				 "can't create object : %s \n",
				 (*error)->message);

	gst_object_unref (GST_OBJECT (pipeline));
	return FALSE;
}

static BraseroBurnResult
brasero_vob_start (BraseroJob *job,
		   GError **error)
{
	BraseroVobPrivate *priv;
	BraseroJobAction action;
	BraseroTrackType *output = NULL;

	brasero_job_get_action (job, &action);
	if (action != BRASERO_JOB_ACTION_IMAGE)
		return BRASERO_BURN_NOT_SUPPORTED;

	priv = BRASERO_VOB_PRIVATE (job);

	/* get destination medium type */
	output = brasero_track_type_new ();
	brasero_job_get_output_type (job, output);

	if (brasero_track_type_get_stream_format (output) & BRASERO_VIDEO_FORMAT_VCD) {
		GValue *value = NULL;

		priv->is_video_dvd = FALSE;
		brasero_job_tag_lookup (job,
					BRASERO_VCD_TYPE,
					&value);
		if (value)
			priv->svcd = (g_value_get_int (value) == BRASERO_SVCD);
	}
	else
		priv->is_video_dvd = TRUE;

	BRASERO_JOB_LOG (job,
			 "Got output type (is DVD %i, is SVCD %i)",
			 priv->is_video_dvd,
			 priv->svcd);

	brasero_track_type_free (output);

	if (!brasero_vob_build_pipeline (BRASERO_VOB (job), error))
		return BRASERO_BURN_ERR;

	/* ready to go */
	brasero_job_set_current_action (job,
					BRASERO_BURN_ACTION_ANALYSING,
					_("Converting video file to MPEG2"),
					FALSE);
	brasero_job_start_progress (job, FALSE);

	gst_element_set_state (priv->pipeline, GST_STATE_PLAYING);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_vob_clock_tick (BraseroJob *job)
{
	gint64 position = 0.0;
	gint64 duration = 0.0;
	BraseroVobPrivate *priv;
	GstFormat format = GST_FORMAT_TIME;

	priv = BRASERO_VOB_PRIVATE (job);

	gst_element_query_duration (priv->pipeline, &format, &duration);
	gst_element_query_position (priv->pipeline, &format, &position);

	if (duration > 0.0) {
		gdouble progress;

		progress = (gdouble) position / (gdouble) duration;
		brasero_job_set_progress (job, progress);
	}

	return BRASERO_BURN_OK;
}

static void
brasero_vob_init (BraseroVob *object)
{}

static void
brasero_vob_finalize (GObject *object)
{
	BraseroVobPrivate *priv;

	priv = BRASERO_VOB_PRIVATE (object);

	if (priv->pipeline) {
		gst_object_unref (priv->pipeline);
		priv->pipeline = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_vob_class_init (BraseroVobClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	BraseroJobClass* job_class = BRASERO_JOB_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroVobPrivate));

	object_class->finalize = brasero_vob_finalize;

	job_class->start = brasero_vob_start;
	job_class->clock_tick = brasero_vob_clock_tick;
	job_class->stop = brasero_vob_stop;
}

static BraseroBurnResult
brasero_vob_export_caps (BraseroPlugin *plugin, gchar **error)
{
	GSList *input;
	GSList *output;
	GstElement *element;

	/* Let's see if we've got the plugins we need */
	element = gst_element_factory_make ("ffenc_mpeg2video", NULL);
	if (!element) {
		*error = g_strdup_printf (_("%s element could not be created"),
					  "\"ffenc_mpeg2video\"");
		return BRASERO_BURN_ERR;
	}
	gst_object_unref (element);

	element = gst_element_factory_make ("ffenc_ac3", NULL);
	if (!element) {
		*error = g_strdup_printf (_("%s element could not be created"),
					  "\"ffenc_ac3\"");
		return BRASERO_BURN_ERR;
	}
	gst_object_unref (element);

	element = gst_element_factory_make ("ffenc_mp2", NULL);
	if (!element) {
		*error = g_strdup_printf (_("%s element could not be created"),
					  "\"ffenc_mp2\"");
		return BRASERO_BURN_ERR;
	}
	gst_object_unref (element);

	element = gst_element_factory_make ("mplex", NULL);
	if (!element) {
		*error = g_strdup_printf (_("%s element could not be created"),
					  "\"mplex\"");
		return BRASERO_BURN_ERR;
	}
	gst_object_unref (element);

	brasero_plugin_define (plugin,
			       "transcode2vob",
			       _("Vob allows to transcode any video file to a format suitable for video DVDs"),
			       "Philippe Rouquier",
			       0);

	input = brasero_caps_audio_new (BRASERO_PLUGIN_IO_ACCEPT_FILE,
					BRASERO_AUDIO_FORMAT_UNDEFINED|
					BRASERO_VIDEO_FORMAT_UNDEFINED|
					BRASERO_METADATA_INFO);
	output = brasero_caps_audio_new (BRASERO_PLUGIN_IO_ACCEPT_FILE,
					 BRASERO_AUDIO_FORMAT_MP2|
					 BRASERO_AUDIO_FORMAT_44100|
					 BRASERO_METADATA_INFO|
					 BRASERO_VIDEO_FORMAT_VCD);
	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (output);

	output = brasero_caps_audio_new (BRASERO_PLUGIN_IO_ACCEPT_FILE,
					 BRASERO_AUDIO_FORMAT_AC3|
					 BRASERO_AUDIO_FORMAT_MP2|
					 BRASERO_AUDIO_FORMAT_RAW|
					 BRASERO_AUDIO_FORMAT_44100|
					 BRASERO_AUDIO_FORMAT_48000|
					 BRASERO_METADATA_INFO|
					 BRASERO_VIDEO_FORMAT_VIDEO_DVD);
	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (output);
	g_slist_free (input);

	input = brasero_caps_audio_new (BRASERO_PLUGIN_IO_ACCEPT_FILE,
					BRASERO_AUDIO_FORMAT_UNDEFINED|
					BRASERO_VIDEO_FORMAT_UNDEFINED);
	output = brasero_caps_audio_new (BRASERO_PLUGIN_IO_ACCEPT_FILE,
					 BRASERO_AUDIO_FORMAT_MP2|
					 BRASERO_AUDIO_FORMAT_44100|
					 BRASERO_VIDEO_FORMAT_VCD);
	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (output);

	output = brasero_caps_audio_new (BRASERO_PLUGIN_IO_ACCEPT_FILE,
					 BRASERO_AUDIO_FORMAT_AC3|
					 BRASERO_AUDIO_FORMAT_MP2|
					 BRASERO_AUDIO_FORMAT_RAW|
					 BRASERO_AUDIO_FORMAT_44100|
					 BRASERO_AUDIO_FORMAT_48000|
					 BRASERO_VIDEO_FORMAT_VIDEO_DVD);
	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (output);
	g_slist_free (input);
	return BRASERO_BURN_OK;
}
