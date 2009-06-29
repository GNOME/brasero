/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero-normalize.c
 * Copyright (C) Rouquier Philippe 2008 <bonfire-app@wanadoo.fr>
 * 
 * brasero-normalize.c is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * brasero-normalize.c is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with brasero-normalize.c.  If not, write to:
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

#include "burn-job.h"
#include "brasero-plugin-registration.h"
#include "burn-normalize.h"

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
				      gboolean arg2,
				      BraseroNormalize *normalize)
{
	GstPad *sink;
	GstCaps *caps;
	GstStructure *structure;
	BraseroNormalizePrivate *priv;

	priv = BRASERO_NORMALIZE_PRIVATE (normalize);

	sink = gst_element_get_pad (priv->resample, "sink");
	if (GST_PAD_IS_LINKED (sink)) {
		BRASERO_JOB_LOG (normalize, "New decoded pad already linked");
		return;
	}

	/* make sure we only have audio */
	caps = gst_pad_get_caps (pad);
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
	source = gst_element_make_from_uri (GST_URI_SRC, uri, NULL);
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
	                  "new-decoded-pad",
	                  G_CALLBACK (brasero_normalize_new_decoded_pad_cb),
	                  normalize);
	gst_element_link_many (resample,
			       convert,
			       analysis,
			       sink,
			       NULL);

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

static gboolean
brasero_normalize_set_next_track (BraseroJob *job,
				  BraseroTrack *track,
				  GError **error)
{
	gchar *uri;
	GstElement *analysis;
	BraseroNormalizePrivate *priv;

	priv = BRASERO_NORMALIZE_PRIVATE (job);

	if (!priv->analysis) {
		analysis = gst_element_factory_make ("rganalysis", NULL);
		if (analysis == NULL) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("%s element could not be created"),
				     "\"Rganalysis\"");
			return FALSE;
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
		return FALSE;
	}

	g_free (uri);
	return TRUE;
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
	BraseroTrack *track;
	GError *error = NULL;
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

	if (!priv->tracks) {
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


	track = priv->tracks->data;
	priv->tracks = g_slist_remove (priv->tracks, track);
	if (!brasero_normalize_set_next_track (BRASERO_JOB (normalize), track, &error)) {
		brasero_job_error (BRASERO_JOB (normalize), error);
		return;
	}
}

static gboolean
brasero_normalize_bus_messages (GstBus *bus,
				GstMessage *msg,
				BraseroNormalize *normalize)
{
	BraseroNormalizePrivate *priv;
	GstTagList *tags = NULL;
	GError *error = NULL;
	gchar *debug;

	priv = BRASERO_NORMALIZE_PRIVATE (normalize);
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
		BRASERO_JOB_LOG (normalize, debug);
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
	BraseroTrack *track;

	priv = BRASERO_NORMALIZE_PRIVATE (job);

	priv->album_gain = -1.0;
	priv->album_peak = -1.0;

	/* get tracks */
	brasero_job_get_tracks (job, &priv->tracks);
	if (!priv->tracks)
		return BRASERO_BURN_ERR;

	priv->tracks = g_slist_copy (priv->tracks);
	track = priv->tracks->data;
	priv->tracks = g_slist_remove (priv->tracks, track);

	if (!brasero_normalize_set_next_track (job, track, error))
		return BRASERO_BURN_ERR;

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
	GstFormat format = GST_FORMAT_TIME;

	priv = BRASERO_NORMALIZE_PRIVATE (job);

	gst_element_query_duration (priv->pipeline, &format, &duration);
	gst_element_query_position (priv->pipeline, &format, &position);

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

static BraseroBurnResult
brasero_normalize_export_caps (BraseroPlugin *plugin, gchar **error)
{
	GSList *input;
	GstElement *element;

	brasero_plugin_define (plugin,
			       N_("Normalize"),
			       _("Normalize allows to set consistent sound levels between tracks"),
			       "Philippe Rouquier",
			       0);

	/* Let's see if we've got the plugins we need */
	element = gst_element_factory_make ("rgvolume", NULL);
	if (!element) {
		*error = g_strdup_printf (_("%s element could not be created"),
					  "\"Rgvolume\"");
		return BRASERO_BURN_ERR;
	}
	gst_object_unref (element);

	element = gst_element_factory_make ("rganalysis", NULL);
	if (!element) {
		*error = g_strdup_printf (_("%s element could not be created"),
					  "\"Rganalysis\"");
		return BRASERO_BURN_ERR;
	}
	gst_object_unref (element);

	input = brasero_caps_audio_new (BRASERO_PLUGIN_IO_ACCEPT_FILE,
					BRASERO_AUDIO_FORMAT_UNDEFINED|
					BRASERO_METADATA_INFO);
	brasero_plugin_process_caps (plugin, input);
	g_slist_free (input);

	input = brasero_caps_audio_new (BRASERO_PLUGIN_IO_ACCEPT_FILE,
					BRASERO_AUDIO_FORMAT_UNDEFINED);
	brasero_plugin_process_caps (plugin, input);
	g_slist_free (input);

	/* We should run first */
	brasero_plugin_set_process_flags (plugin, BRASERO_PLUGIN_RUN_PREPROCESSING);

	brasero_plugin_set_compulsory (plugin, FALSE);

	return BRASERO_BURN_OK;
}
