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

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gmodule.h>

#include <gst/gst.h>

#include "brasero-tags.h"

#include "burn-job.h"
#include "burn-normalize.h"
#include "brasero-plugin-registration.h"


#define BRASERO_TYPE_NORMALIZE             (brasero_normalize_get_type ())
#define BRASERO_NORMALIZE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_NORMALIZE, BraseroNormalize))
#define BRASERO_NORMALIZE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_NORMALIZE, BraseroNormalizeClass))
#define BRASERO_IS_NORMALIZE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_NORMALIZE))
#define BRASERO_IS_NORMALIZE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_NORMALIZE))
#define BRASERO_NORMALIZE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_NORMALIZE, BraseroNormalizeClass))

BRASERO_PLUGIN_BOILERPLATE (BraseroNormalize, brasero_normalize, BRASERO_TYPE_JOB, BraseroJob);

typedef struct _BraseroNormalizePrivate BraseroNormalizePrivate;
struct _BraseroNormalizePrivate
{
	GstElement *pipeline;
	GstElement *analysis;
	GstElement *decode;
	GstElement *resample;

	GSList *tracks;
	BraseroTrack *track;

	gdouble album_peak;
	gdouble album_gain;
	gdouble track_peak;
	gdouble track_gain;
};

#define BRASERO_NORMALIZE_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_NORMALIZE, BraseroNormalizePrivate))

static GObjectClass *parent_class = NULL;


static gboolean
brasero_normalize_bus_messages (GstBus *bus,
				GstMessage *msg,
				BraseroNormalize *normalize);

static void
brasero_normalize_stop_pipeline (BraseroNormalize *normalize)
{
	BraseroNormalizePrivate *priv;

	priv = BRASERO_NORMALIZE_PRIVATE (normalize);
	if (!priv->pipeline)
		return;

	gst_element_set_state (priv->pipeline, GST_STATE_NULL);
	gst_object_unref (GST_OBJECT (priv->pipeline));
	priv->pipeline = NULL;
	priv->resample = NULL;
	priv->analysis = NULL;
	priv->decode = NULL;
}

static void
brasero_normalize_new_decoded_pad_cb (GstElement *decode,
				      GstPad *pad,
				      BraseroNormalize *normalize)
{
	GstPad *sink;
	GstCaps *caps;
	GstStructure *structure;
	BraseroNormalizePrivate *priv;

	priv = BRASERO_NORMALIZE_PRIVATE (normalize);

	sink = gst_element_get_static_pad (priv->resample, "sink");
	if (GST_PAD_IS_LINKED (sink)) {
		BRASERO_JOB_LOG (normalize, "New decoded pad already linked");
		return;
	}

	/* make sure we only have audio */
	/* FIXME: get_current_caps() doesn't always seem to work yet here */
	caps = gst_pad_query_caps (pad, NULL);
	if (!caps)
		return;

	structure = gst_caps_get_structure (caps, 0);
	if (structure && g_strrstr (gst_structure_get_name (structure), "audio")) {
		if (gst_pad_link (pad, sink) != GST_PAD_LINK_OK) {
			BRASERO_JOB_LOG (normalize, "New decoded pad can't be linked");
			brasero_job_error (BRASERO_JOB (normalize), NULL);
		}
		else
			BRASERO_JOB_LOG (normalize, "New decoded pad linked");
	}
	else
		BRASERO_JOB_LOG (normalize, "New decoded pad with unsupported stream time");

	gst_object_unref (sink);
	gst_caps_unref (caps);
}

  static gboolean
brasero_normalize_build_pipeline (BraseroNormalize *normalize,
                                  const gchar *uri,
                                  GstElement *analysis,
                                  GError **error)
{
	GstBus *bus = NULL;
	GstElement *source;
	GstElement *decode;
	GstElement *pipeline;
	GstElement *sink = NULL;
	GstElement *convert = NULL;
	GstElement *resample = NULL;
	BraseroNormalizePrivate *priv;

	priv = BRASERO_NORMALIZE_PRIVATE (normalize);

	BRASERO_JOB_LOG (normalize, "Creating new pipeline");

	/* create filesrc ! decodebin ! audioresample ! audioconvert ! rganalysis ! fakesink */
	pipeline = gst_pipeline_new (NULL);
	priv->pipeline = pipeline;

	/* a new source is created */
	source = gst_element_make_from_uri (GST_URI_SRC, uri, NULL, NULL);
	if (source == NULL) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Source\"");
		goto error;
	}
	gst_bin_add (GST_BIN (priv->pipeline), source);
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
	priv->decode = decode;

	if (!gst_element_link (source, decode)) {
		BRASERO_JOB_LOG (normalize, "Elements could not be linked");
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
		             _("Impossible to link plugin pads"));
		goto error;
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
	priv->resample = resample;

	/* rganalysis: set the number of tracks to be expected */
	priv->analysis = analysis;
	gst_bin_add (GST_BIN (pipeline), analysis);

	/* sink */
	sink = gst_element_factory_make ("fakesink", NULL);
	if (!sink) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("%s element could not be created"),
			     "\"Fakesink\"");
		goto error;
	}
	gst_bin_add (GST_BIN (pipeline), sink);
	g_object_set (sink,
		      "sync", FALSE,
		      NULL);

	/* link everything */
	g_signal_connect (G_OBJECT (decode),
	                  "pad-added",
	                  G_CALLBACK (brasero_normalize_new_decoded_pad_cb),
	                  normalize);
	if (!gst_element_link_many (resample,
	                            convert,
	                            analysis,
	                            sink,
	                            NULL)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
		             _("Impossible to link plugin pads"));
	}

	/* connect to the bus */	
	bus = gst_pipeline_get_bus (GST_PIPELINE (priv->pipeline));
	gst_bus_add_watch (bus,
			   (GstBusFunc) brasero_normalize_bus_messages,
			   normalize);
	gst_object_unref (bus);

	gst_element_set_state (priv->pipeline, GST_STATE_PLAYING);

	return TRUE;

error:

	if (error && (*error))
		BRASERO_JOB_LOG (normalize,
				 "can't create object : %s \n",
				 (*error)->message);

	gst_object_unref (GST_OBJECT (pipeline));
	return FALSE;
}

static BraseroBurnResult
brasero_normalize_set_next_track (BraseroJob *job,
                                  GError **error)
{
	gchar *uri;
	GValue *value;
	GstElement *analysis;
	BraseroTrackType *type;
	BraseroTrack *track = NULL;
	gboolean dts_allowed = FALSE;
	BraseroNormalizePrivate *priv;

	priv = BRASERO_NORMALIZE_PRIVATE (job);

	/* See if dts is allowed */
	value = NULL;
	brasero_job_tag_lookup (job, BRASERO_SESSION_STREAM_AUDIO_FORMAT, &value);
	if (value)
		dts_allowed = (g_value_get_int (value) & BRASERO_AUDIO_FORMAT_DTS) != 0;

	type = brasero_track_type_new ();
	while (priv->tracks && priv->tracks->data) {
		track = priv->tracks->data;
		priv->tracks = g_slist_remove (priv->tracks, track);

		brasero_track_get_track_type (track, type);
		if (brasero_track_type_get_has_stream (type)) {
			if (!dts_allowed)
				break;

			/* skip DTS tracks as we won't modify them */
			if ((brasero_track_type_get_stream_format (type) & BRASERO_AUDIO_FORMAT_DTS) == 0) 
				break;

			BRASERO_JOB_LOG (job, "Skipped DTS track");
		}

		track = NULL;
	}
	brasero_track_type_free (type);

	if (!track)
		return BRASERO_BURN_OK;

	if (!priv->analysis) {
		analysis = gst_element_factory_make ("rganalysis", NULL);
		if (analysis == NULL) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("%s element could not be created"),
				     "\"Rganalysis\"");
			return BRASERO_BURN_ERR;
		}

		g_object_set (analysis,
			      "num-tracks", g_slist_length (priv->tracks),
			      NULL);
	}
	else {
		/* destroy previous pipeline but save our plugin */
		analysis = g_object_ref (priv->analysis);

		/* NOTE: why lock state? because otherwise analysis would lose all 
		 * information about tracks already analysed by going into the NULL
		 * state. */
		gst_element_set_locked_state (analysis, TRUE);
		gst_bin_remove (GST_BIN (priv->pipeline), analysis);
		brasero_normalize_stop_pipeline (BRASERO_NORMALIZE (job));
		gst_element_set_locked_state (analysis, FALSE);
	}

	/* create a new one */
	priv->track = track;
	uri = brasero_track_stream_get_source (BRASERO_TRACK_STREAM (track), TRUE);
	BRASERO_JOB_LOG (job, "Analysing track %s", uri);

	if (!brasero_normalize_build_pipeline (BRASERO_NORMALIZE (job), uri, analysis, error)) {
		g_free (uri);
		return BRASERO_BURN_ERR;
	}

	g_free (uri);
	return BRASERO_BURN_RETRY;
}

static BraseroBurnResult
brasero_normalize_stop (BraseroJob *job,
			GError **error)
{
	BraseroNormalizePrivate *priv;

	priv = BRASERO_NORMALIZE_PRIVATE (job);

	brasero_normalize_stop_pipeline (BRASERO_NORMALIZE (job));
	if (priv->tracks) {
		g_slist_free (priv->tracks);
		priv->tracks = NULL;
	}

	priv->track = NULL;

	return BRASERO_BURN_OK;
}

static void
foreach_tag (const GstTagList *list,
	     const gchar *tag,
	     BraseroNormalize *normalize)
{
	gdouble value = 0.0;
	BraseroNormalizePrivate *priv;

	priv = BRASERO_NORMALIZE_PRIVATE (normalize);

	/* Those next two are generated at the end only */
	if (!strcmp (tag, GST_TAG_ALBUM_GAIN)) {
		gst_tag_list_get_double (list, tag, &value);
		priv->album_gain = value;
	}
	else if (!strcmp (tag, GST_TAG_ALBUM_PEAK)) {
		gst_tag_list_get_double (list, tag, &value);
		priv->album_peak = value;
	}
	else if (!strcmp (tag, GST_TAG_TRACK_PEAK)) {
		gst_tag_list_get_double (list, tag, &value);
		priv->track_peak = value;
	}
	else if (!strcmp (tag, GST_TAG_TRACK_GAIN)) {
		gst_tag_list_get_double (list, tag, &value);
		priv->track_gain = value;
	}
}

static void
brasero_normalize_song_end_reached (BraseroNormalize *normalize)
{
	GValue *value;
	GError *error = NULL;
	BraseroBurnResult result;
	BraseroNormalizePrivate *priv;

	priv = BRASERO_NORMALIZE_PRIVATE (normalize);
	
	/* finished track: set tags */
	BRASERO_JOB_LOG (normalize,
			 "Setting track peak (%lf) and gain (%lf)",
			 priv->track_peak,
			 priv->track_gain);

	value = g_new0 (GValue, 1);
	g_value_init (value, G_TYPE_DOUBLE);
	g_value_set_double (value, priv->track_peak);
	brasero_track_tag_add (priv->track,
			       BRASERO_TRACK_PEAK_VALUE,
			       value);

	value = g_new0 (GValue, 1);
	g_value_init (value, G_TYPE_DOUBLE);
	g_value_set_double (value, priv->track_gain);
	brasero_track_tag_add (priv->track,
			       BRASERO_TRACK_GAIN_VALUE,
			       value);

	priv->track_peak = 0.0;
	priv->track_gain = 0.0;

	result = brasero_normalize_set_next_track (BRASERO_JOB (normalize), &error);
	if (result == BRASERO_BURN_OK) {
		BRASERO_JOB_LOG (normalize,
				 "Setting album peak (%lf) and gain (%lf)",
				 priv->album_peak,
				 priv->album_gain);

		/* finished: set tags */
		value = g_new0 (GValue, 1);
		g_value_init (value, G_TYPE_DOUBLE);
		g_value_set_double (value, priv->album_peak);
		brasero_job_tag_add (BRASERO_JOB (normalize),
				     BRASERO_ALBUM_PEAK_VALUE,
				     value);

		value = g_new0 (GValue, 1);
		g_value_init (value, G_TYPE_DOUBLE);
		g_value_set_double (value, priv->album_gain);
		brasero_job_tag_add (BRASERO_JOB (normalize),
				     BRASERO_ALBUM_GAIN_VALUE,
				     value);

		brasero_job_finished_session (BRASERO_JOB (normalize));
		return;
	}

	/* jump to next track */
	if (result == BRASERO_BURN_ERR) {
		brasero_job_error (BRASERO_JOB (normalize), error);
		return;
	}
}

static gboolean
brasero_normalize_bus_messages (GstBus *bus,
				GstMessage *msg,
				BraseroNormalize *normalize)
{
	GstTagList *tags = NULL;
	GError *error = NULL;
	gchar *debug;

	switch (GST_MESSAGE_TYPE (msg)) {
	case GST_MESSAGE_TAG:
		/* This is the information we've been waiting for.
		 * NOTE: levels for whole album is delivered at the end */
		gst_message_parse_tag (msg, &tags);
		gst_tag_list_foreach (tags, (GstTagForeachFunc) foreach_tag, normalize);
		gst_tag_list_free (tags);
		return TRUE;

	case GST_MESSAGE_ERROR:
		gst_message_parse_error (msg, &error, &debug);
		BRASERO_JOB_LOG (normalize, "%s", debug);
		g_free (debug);

	        brasero_job_error (BRASERO_JOB (normalize), error);
		return FALSE;

	case GST_MESSAGE_EOS:
		brasero_normalize_song_end_reached (normalize);
		return FALSE;

	case GST_MESSAGE_STATE_CHANGED:
		break;

	default:
		return TRUE;
	}

	return TRUE;
}

static BraseroBurnResult
brasero_normalize_start (BraseroJob *job,
			 GError **error)
{
	BraseroNormalizePrivate *priv;
	BraseroBurnResult result;

	priv = BRASERO_NORMALIZE_PRIVATE (job);

	priv->album_gain = -1.0;
	priv->album_peak = -1.0;

	/* get tracks */
	brasero_job_get_tracks (job, &priv->tracks);
	if (!priv->tracks)
		return BRASERO_BURN_ERR;

	priv->tracks = g_slist_copy (priv->tracks);

	result = brasero_normalize_set_next_track (job, error);
	if (result == BRASERO_BURN_ERR)
		return BRASERO_BURN_ERR;

	if (result == BRASERO_BURN_OK)
		return BRASERO_BURN_NOT_RUNNING;

	/* ready to go */
	brasero_job_set_current_action (job,
					BRASERO_BURN_ACTION_ANALYSING,
					_("Normalizing tracks"),
					FALSE);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_normalize_activate (BraseroJob *job,
			    GError **error)
{
	GSList *tracks;
	BraseroJobAction action;

	brasero_job_get_action (job, &action);
	if (action != BRASERO_JOB_ACTION_IMAGE)
		return BRASERO_BURN_NOT_RUNNING;

	/* check we have more than one track */
	brasero_job_get_tracks (job, &tracks);
	if (g_slist_length (tracks) < 2)
		return BRASERO_BURN_NOT_RUNNING;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_normalize_clock_tick (BraseroJob *job)
{
	gint64 position = 0.0;
	gint64 duration = 0.0;
	BraseroNormalizePrivate *priv;

	priv = BRASERO_NORMALIZE_PRIVATE (job);

	gst_element_query_duration (priv->pipeline, GST_FORMAT_TIME, &duration);
	gst_element_query_position (priv->pipeline, GST_FORMAT_TIME, &position);

	if (duration > 0) {
		GSList *tracks;
		gdouble progress;

		brasero_job_get_tracks (job, &tracks);
		progress = (gdouble) position / (gdouble) duration;

		if (tracks) {
			gdouble num_tracks;

			num_tracks = g_slist_length (tracks);
			progress = (gdouble) (num_tracks - 1.0 - (gdouble) g_slist_length (priv->tracks) + progress) / (gdouble) num_tracks;
			brasero_job_set_progress (job, progress);
		}
	}

	return BRASERO_BURN_OK;
}

static void
brasero_normalize_init (BraseroNormalize *object)
{}

static void
brasero_normalize_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_normalize_class_init (BraseroNormalizeClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	BraseroJobClass *job_class = BRASERO_JOB_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroNormalizePrivate));

	object_class->finalize = brasero_normalize_finalize;

	job_class->activate = brasero_normalize_activate;
	job_class->start = brasero_normalize_start;
	job_class->clock_tick = brasero_normalize_clock_tick;
	job_class->stop = brasero_normalize_stop;
}

static void
brasero_normalize_export_caps (BraseroPlugin *plugin)
{
	GSList *input;

	brasero_plugin_define (plugin,
	                       "normalize",
			       N_("Normalization"),
			       _("Sets consistent sound levels between tracks"),
			       "Philippe Rouquier",
			       0);

	/* Add dts to make sure that when they are mixed with regular songs
	 * this plugin will be called for the regular tracks */
	input = brasero_caps_audio_new (BRASERO_PLUGIN_IO_ACCEPT_FILE,
					BRASERO_AUDIO_FORMAT_UNDEFINED|
	                                BRASERO_AUDIO_FORMAT_DTS|
					BRASERO_METADATA_INFO);
	brasero_plugin_process_caps (plugin, input);
	g_slist_free (input);

	/* Add dts to make sure that when they are mixed with regular songs
	 * this plugin will be called for the regular tracks */
	input = brasero_caps_audio_new (BRASERO_PLUGIN_IO_ACCEPT_FILE,
					BRASERO_AUDIO_FORMAT_UNDEFINED|
	                                BRASERO_AUDIO_FORMAT_DTS);
	brasero_plugin_process_caps (plugin, input);
	g_slist_free (input);

	/* We should run first... unfortunately since the gstreamer-1 port
	 * we're unable to process more than a single track with rganalysis
	 * and the GStreamer pipeline becomes stopped indefinitely.
	 * Disable normalisation until this is resolved.
	 * See https://bugzilla.gnome.org/show_bug.cgi?id=699599 */
	brasero_plugin_set_process_flags (plugin, BRASERO_PLUGIN_RUN_NEVER);

	brasero_plugin_set_compulsory (plugin, FALSE);
}

G_MODULE_EXPORT void
brasero_plugin_check_config (BraseroPlugin *plugin)
{
	brasero_plugin_test_gstreamer_plugin (plugin, "rgvolume");
	brasero_plugin_test_gstreamer_plugin (plugin, "rganalysis");
}
