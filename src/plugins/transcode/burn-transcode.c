/***************************************************************************
 *            transcode.c
 *
 *  ven jui  8 16:15:04 2005
 *  Copyright  2005  Philippe Rouquier
 *  Brasero-app@wanadoo.fr
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
#include <math.h>
#include <unistd.h>
#include <fcntl.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gmodule.h>

#include <gst/gst.h>

#include <nautilus-burn-drive.h>

#include "burn-basics.h"
#include "burn-job.h"
#include "burn-plugin.h"
#include "burn-transcode.h"

BRASERO_PLUGIN_BOILERPLATE (BraseroTranscode, brasero_transcode, BRASERO_TYPE_JOB, BraseroJob);

static gboolean brasero_transcode_bus_messages (GstBus *bus,
						GstMessage *msg,
						BraseroTranscode *transcode);
static void brasero_transcode_new_decoded_pad_cb (GstElement *decode,
						  GstPad *pad,
						  gboolean arg2,
						  GstElement *convert);

struct BraseroTranscodePrivate {
	GstElement *pipeline;
	GstElement *convert;
	GstElement *decode;
	GstElement *source;
	GstElement *sink;

	gint pad_size;
	gint pad_fd;
	gint pad_id;

	gint64 size;
	gint64 pos;

	gulong probe;
	gint64 segment_start;
	gint64 segment_end;

	guint set_active_state:1;
};
typedef struct BraseroTranscodePrivate BraseroTranscodePrivate;

#define BRASERO_TRANSCODE_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_TRANSCODE, BraseroTranscodePrivate))

static GObjectClass *parent_class = NULL;

static gboolean
brasero_transcode_buffer_handler (GstPad *pad, GstBuffer *buffer, BraseroTranscode *self)
{
	BraseroTranscodePrivate *priv;
	GstPad *peer;
	gint64 size;

	priv = BRASERO_TRANSCODE_PRIVATE (self);

	size = GST_BUFFER_SIZE (buffer);

	if (priv->segment_start <= 0 && priv->segment_end <= 0)
		return TRUE;

	/* what we do here is more or less what gstreamer does when seeking:
	 * it reads and process from 0 to the seek position (I tried).
	 * It even forwards the data before the seek position to the sink (which
	 * is a problem in our case as it would be written) */
	if (priv->size > priv->segment_end) {
		priv->size += size;
		return FALSE;
	}

	if (priv->size + size > priv->segment_end) {
		GstBuffer *new_buffer;
		int data_size;

		/* the entire the buffer is not interesting for us */
		/* create a new buffer and push it on the pad:
		 * NOTE: we're going to receive it ... */
		data_size = priv->segment_end - priv->size;
		new_buffer = gst_buffer_new_and_alloc (data_size);
		memcpy (GST_BUFFER_DATA (new_buffer), GST_BUFFER_DATA (buffer), data_size);

		/* Recursive: the following calls ourselves BEFORE we finish */
		peer = gst_pad_get_peer (pad);
		gst_pad_push (peer, new_buffer);

		/* post an EOS event to stop pipeline */
		gst_pad_push_event (peer, gst_event_new_eos ());
		
		gst_object_unref (peer);

		priv->size += size - data_size;
		return FALSE;
	}

	/* see if the buffer is in the segment */
	if (priv->size < priv->segment_start) {
		GstBuffer *new_buffer;
		gint data_size;

		/* see if all the buffer is interesting for us */
		if (priv->size + size < priv->segment_start) {
			priv->size += size;
			return FALSE;
		}

		/* create a new buffer and push it on the pad:
		 * NOTE: we're going to receive it ... */
		data_size = priv->size + size - priv->segment_start;
		new_buffer = gst_buffer_new_and_alloc (data_size);
		memcpy (GST_BUFFER_DATA (new_buffer),
			GST_BUFFER_DATA (buffer) +
			GST_BUFFER_SIZE (buffer) -
			data_size,
			data_size);
		GST_BUFFER_TIMESTAMP (new_buffer) = GST_BUFFER_TIMESTAMP (buffer) + data_size;

		/* this is recursive the following calls ourselves 
		 * BEFORE we finish */
		peer = gst_pad_get_peer (pad);
		gst_pad_push (peer, new_buffer);
		gst_object_unref (peer);

		priv->size += size - data_size;
		return FALSE;
	}

	priv->size += size;
	priv->pos += size;

	return TRUE;
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
	start = brasero_track_get_audio_start (track);
	end = brasero_track_get_audio_end (track);

	priv->segment_start = BRASERO_DURATION_TO_BYTES (start);
	priv->segment_end = BRASERO_DURATION_TO_BYTES (end);

	BRASERO_JOB_LOG (transcode, "settings track boundaries time = %lli %lli / bytes = %lli %lli",
			 start, end,
			 priv->segment_start, priv->segment_end);

	return BRASERO_BURN_OK;
}

static gboolean
brasero_transcode_create_pipeline (BraseroTranscode *transcode, GError **error)
{
	gchar *uri;
	GstPad *sinkpad;
	GstElement *decode;
	GstElement *source;
	GstBus *bus = NULL;
	GstCaps *filtercaps;
	GstElement *pipeline;
	GstElement *sink = NULL;
	BraseroJobAction action;
	GstElement *filter = NULL;
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
		priv->pipeline = NULL;
		priv->sink = NULL;
		priv->source = NULL;
		priv->convert = NULL;
		priv->pipeline = NULL;
	}

	/* create three types of pipeline according to the needs:
	 * - filesrc ! decodebin ! audioconvert ! fakesink (find size)
	 * - filesrc ! decodebin ! audioresample ! audioconvert ! audio/x-raw-int,rate=44100,width=16,depth=16,endianness=4321,signed ! filesink
	 * - filesrc ! decodebin ! audioresample ! audioconvert ! audio/x-raw-int,rate=44100,width=16,depth=16,endianness=4321,signed ! fdsink
	 */
	pipeline = gst_pipeline_new (NULL);

	bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
	gst_bus_add_watch (bus,
			   (GstBusFunc) brasero_transcode_bus_messages,
			   transcode);
	gst_object_unref (bus);

	/* source */
	brasero_job_get_current_track (BRASERO_JOB (transcode), &track);
	uri = brasero_track_get_audio_source (track, TRUE);
	source = gst_element_make_from_uri (GST_URI_SRC, uri, NULL);
	if (source == NULL) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("source can't be created"));
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
		sink = gst_element_factory_make ("fakesink", NULL);
		break;

	case BRASERO_JOB_ACTION_IMAGE:
		if (brasero_job_get_fd_out (BRASERO_JOB (transcode), NULL) != BRASERO_BURN_OK) {
			gchar *output;

			brasero_job_get_image_output (BRASERO_JOB (transcode),
						      &output,
						      NULL);
			sink = gst_element_factory_make ("filesink", NULL);
			g_object_set (sink,
				      "location", output,
				      NULL);
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
			     _("sink can't be created"));
		goto error;
	}

	gst_bin_add (GST_BIN (pipeline), sink);
	g_object_set (sink,
		      "sync", FALSE,
		      NULL);
		
	/* audioconvert */
	convert = gst_element_factory_make ("audioconvert", NULL);
	if (convert == NULL) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("audioconvert can't be created"));
		goto error;
	}
	gst_bin_add (GST_BIN (pipeline), convert);

	if (action == BRASERO_JOB_ACTION_IMAGE) {
		/* audioresample */
		resample = gst_element_factory_make ("audioresample", NULL);
		if (resample == NULL) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("audioresample can't be created"));
			goto error;
		}
		gst_bin_add (GST_BIN (pipeline), resample);

		/* filter */
		filter = gst_element_factory_make ("capsfilter", NULL);
		if (!filter) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("filter can't be created"));
			goto error;
		}
		gst_bin_add (GST_BIN (pipeline), filter);
		filtercaps = gst_caps_new_full (gst_structure_new ("audio/x-raw-int",
								   "channels", G_TYPE_INT, 2,
								   "width", G_TYPE_INT, 16,
								   "depth", G_TYPE_INT, 16,
								   "endianness", G_TYPE_INT, 1234,
								   "rate", G_TYPE_INT, 44100,
								   "signed", G_TYPE_BOOLEAN, TRUE,
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
			     _("decode can't be created"));
		goto error;
	}
	gst_bin_add (GST_BIN (pipeline), decode);
	priv->decode = decode;

	if (action == BRASERO_JOB_ACTION_IMAGE) {
		gst_element_link_many (source, decode, NULL);
		g_signal_connect (G_OBJECT (decode),
				  "new-decoded-pad",
				  G_CALLBACK (brasero_transcode_new_decoded_pad_cb),
				  resample);
		gst_element_link_many (resample,
				       convert,
				       filter,
				       sink,
				       NULL);
	}
	else {
		gst_element_link (source, decode);
		gst_element_link (convert, sink);

		g_signal_connect (G_OBJECT (decode),
				  "new-decoded-pad",
				  G_CALLBACK (brasero_transcode_new_decoded_pad_cb),
				  convert);
	}

	priv->sink = sink;
	priv->source = source;
	priv->convert = convert;
	priv->pipeline = pipeline;

	/* This is an ugly workaround for the lack of accuracy with gstreamer.
	 * Yet this is unfortunately a necessary evil. */
	priv->pos = 0;
	priv->size = 0;
	sinkpad = gst_element_get_pad (priv->sink, "sink");
	priv->probe = gst_pad_add_buffer_probe (sinkpad,
						G_CALLBACK (brasero_transcode_buffer_handler),
						transcode);
	gst_object_unref (sinkpad);

	gst_element_set_state (priv->pipeline, GST_STATE_PLAYING);
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
	brasero_track_set_audio_boundaries (track, -1, duration, -1);
	duration += brasero_track_get_audio_gap (track);

	/* if transcoding on the fly we should add some length just to make
	 * sure we won't be too short (gstreamer duration discrepancy) */
	brasero_job_set_output_size_for_current_track (BRASERO_JOB (transcode),
						       BRASERO_DURATION_TO_SECTORS (duration),
						       BRASERO_DURATION_TO_BYTES (duration));

	uri = brasero_track_get_audio_source (track, FALSE);
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
	BraseroSongInfo *src_info, *dest_info;
	BraseroTrack *dest;
	gint64 duration;

	/* it means the same file uri is in the selection and was already
	 * checked. Simply get the values for the length and other information
	 * and copy them. */
	/* NOTE: no need to copy the length since if they are sibling that means
	 * that they have the same length */
	brasero_track_get_audio_length (src, &duration);
	brasero_job_set_output_size_for_current_track (BRASERO_JOB (transcode),
						       BRASERO_DURATION_TO_SECTORS (duration),
						       BRASERO_DURATION_TO_BYTES (duration));

	/* copy the info we are missing */
	src_info = brasero_track_get_audio_info (src);
	brasero_job_get_current_track (BRASERO_JOB (transcode), &dest);
	dest_info = brasero_track_get_audio_info (dest);

	if (!dest_info->artist)
		dest_info->artist = g_strdup (src_info->artist);
	if (!dest_info->composer)
		dest_info->composer = g_strdup (src_info->composer);
	if (!dest_info->title)
		dest_info->title = g_strdup (src_info->title);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_transcode_create_sibling_image (BraseroTranscode *transcode,
					BraseroTrack *src,
					GError **error)
{
	BraseroSongInfo *info;
	BraseroTrack *track;
	BraseroTrack *dest;
	gint64 length = 0;
	gchar *path_dest;
	gchar *path_src;

	/* it means the file is already in the selection. Simply create a 
	 * symlink pointing to first file in the selection with the same uri */
	path_src = brasero_track_get_audio_source (src, FALSE);
	brasero_job_get_audio_output (BRASERO_JOB (transcode), &path_dest);

	if (symlink (path_src, path_dest) == -1) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("a symlink could not be created (%s)"),
			     strerror (errno));

		goto error;
	}

	dest = brasero_track_new (BRASERO_TRACK_TYPE_AUDIO);
	brasero_track_set_audio_source (dest, path_dest, BRASERO_AUDIO_FORMAT_RAW);

	/* NOTE: there is no gap and start = 0 since these tracks are the result
	 * of the transformation of previous ones */
	brasero_track_get_audio_length (src, &length);
	brasero_track_set_audio_boundaries (track, 0, length, 0);

	/* copy all infos but from the current track */
	brasero_job_get_current_track (BRASERO_JOB (transcode), &track);
	info = brasero_track_get_audio_info (track);
	info = brasero_song_info_copy (info);
	brasero_track_set_audio_info (track, info);

	brasero_job_add_track (BRASERO_JOB (transcode), dest);

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
	start = brasero_track_get_audio_start (track);
	end = brasero_track_get_audio_end (track);
	uri = brasero_track_get_audio_source (track, TRUE);

	brasero_job_get_done_tracks (BRASERO_JOB (transcode), &songs);

	for (iter = songs; iter; iter = iter->next) {
		gchar *iter_uri;
		gint64 iter_end;
		gint64 iter_start;
		BraseroTrack *iter_track;

		iter_track = iter->data;
		iter_uri = brasero_track_get_audio_source (iter_track, TRUE);

		if (strcmp (iter_uri, uri))
			continue;

		iter_end = brasero_track_get_audio_end (iter_track);
		if (!iter_end)
			continue;

		if (iter_end != end)
			continue;

		iter_start = brasero_track_get_audio_start (track);
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

		if (brasero_track_get_audio_end (track) > 0)
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
	else if (action == BRASERO_JOB_ACTION_IMAGE) {
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

	sinkpad = gst_element_get_pad (priv->sink, "sink");
	gst_pad_remove_buffer_probe (sinkpad, priv->probe);
	gst_object_unref (sinkpad);

	gst_element_set_state (priv->pipeline, GST_STATE_NULL);
	gst_object_unref (GST_OBJECT (priv->pipeline));
	priv->pipeline = NULL;
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
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("error padding (%s)"),
				     strerror (errno));
			return -1;
		}
	}

	return 0;
}

static void
brasero_transcode_push_track (BraseroTranscode *transcode)
{
	gint64 length = 0;
	BraseroTrack *track;
	gchar *output = NULL;
	BraseroSongInfo *info;
	BraseroTrackType type;

	brasero_job_get_audio_output (BRASERO_JOB (transcode), &output);
	brasero_job_get_current_track (BRASERO_JOB (transcode), &track);

	brasero_track_get_audio_length (track, &length);

	info = brasero_track_get_audio_info (track);
	info = brasero_song_info_copy (info);

	brasero_job_get_output_type (BRASERO_JOB (transcode), &type);
	track = brasero_track_new (BRASERO_TRACK_TYPE_AUDIO);

	brasero_track_set_audio_source (track, output, BRASERO_AUDIO_FORMAT_RAW);
	brasero_track_set_audio_boundaries (track, 0, length, 0);
	brasero_track_set_audio_info (track, info);

	brasero_job_add_track (BRASERO_JOB (transcode), track);
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
	gint64 length = 0;
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
	brasero_track_get_audio_length (track, &length);

	if (priv->pos < BRASERO_DURATION_TO_BYTES (length)) {
		gint64 b_written = 0;

		/* Check bytes boundary for length */
		b_written = BRASERO_DURATION_TO_BYTES (length);
		b_written += (b_written % 2352) ? 2352 - (b_written % 2352):0;
		bytes2write = b_written - priv->pos;

		BRASERO_JOB_LOG (transcode,
				 "wrote %lli bytes (= %lli ns) out of %lli (= %lli ns)"
				 "\n=> padding %lli bytes",
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
				 "wrote %lli bytes (= %lli ns)"
				 "\n=> padding %lli bytes",
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
	fd = open (output, O_WRONLY | O_CREAT | O_APPEND);
	g_free (output);

	if (fd == -1) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("error opening file for padding : %s"),
			     strerror (errno));
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
	typefind = gst_bin_get_by_name (GST_BIN (priv->decode),
					"typefind");

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
brasero_transcode_get_position (BraseroTranscode *transcode)
{
	gint64 position;
	GstElement *element;
	BraseroTranscodePrivate *priv;
	GstFormat format = GST_FORMAT_TIME;

	priv = BRASERO_TRANSCODE_PRIVATE (transcode);
	if (priv->convert)
		element = priv->convert;
	else
		element = priv->pipeline;

	gst_element_query_position (GST_ELEMENT (element),
				    &format,
				    &position);

	return position;
}

static gint64
brasero_transcode_get_duration (BraseroTranscode *transcode)
{
	gint64 duration = -1;
	BraseroJobAction action;
	BraseroTranscodePrivate *priv;
	GstFormat format = GST_FORMAT_TIME;

	priv = BRASERO_TRANSCODE_PRIVATE (transcode);

	/* this is the most reliable way to get the duration for mp3 read them
	 * till the end and get the position. Convert is then needed. */
	brasero_job_get_action (BRASERO_JOB (transcode), &action);
	if (action == BRASERO_JOB_ACTION_IMAGE
	&&  brasero_transcode_is_mp3 (transcode))
		duration = brasero_transcode_get_position (transcode);

	if (duration == -1)
		gst_element_query_duration (GST_ELEMENT (priv->pipeline),
					    &format,
					    &duration);

	BRASERO_JOB_LOG (transcode, "got duration %"G_GINT64_FORMAT, duration);

	if (duration == -1)	
	    brasero_job_error (BRASERO_JOB (transcode),
			       g_error_new (BRASERO_BURN_ERROR,
					    BRASERO_BURN_ERROR_GENERAL,
					    _("error getting duration")));
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
	BraseroSongInfo *info;
	BraseroJobAction action;

	brasero_job_get_action (BRASERO_JOB (transcode), &action);
	brasero_job_get_current_track (BRASERO_JOB (transcode), &track);
	info = brasero_track_get_audio_info (track);

	if (!strcmp (tag, GST_TAG_TITLE)) {
		if (!info->title)
			gst_tag_list_get_string (list, tag, &(info->title));
	}
	else if (!strcmp (tag, GST_TAG_ARTIST)) {
		if (!info->artist)
			gst_tag_list_get_string (list, tag, &(info->artist));
	}
	else if (!strcmp (tag, GST_TAG_ISRC)) {
		gst_tag_list_get_int (list, tag, &(info->isrc));
	}
	else if (!strcmp (tag, GST_TAG_PERFORMER)) {
		if (!info->artist)
			gst_tag_list_get_string (list, tag, &(info->artist));
	}
	else if (action == BRASERO_JOB_ACTION_SIZE
	     &&  !strcmp (tag, GST_TAG_DURATION)) {
		guint64 duration;

		/* this is only useful when we try to have the size */
		gst_tag_list_get_uint64 (list, tag, &duration);
		brasero_track_set_audio_boundaries (track, 0, duration, -1);
	}
}

static BraseroBurnResult
brasero_transcode_active_state (BraseroTranscode *transcode)
{
	BraseroTranscodePrivate *priv;
	gchar *name, *string, *uri;
	BraseroJobAction action;
	BraseroTrack *track;

	priv = BRASERO_TRANSCODE_PRIVATE (transcode);

	if (priv->set_active_state)
		return BRASERO_BURN_OK;

	brasero_job_get_current_track (BRASERO_JOB (transcode), &track);
	uri = brasero_track_get_audio_source (track, FALSE);

	priv->set_active_state = 1;
	brasero_job_get_action (BRASERO_JOB (transcode), &action);
	if (action == BRASERO_JOB_ACTION_SIZE) {
		BRASERO_GET_BASENAME_FOR_DISPLAY (uri, name);
		string = g_strdup_printf (_("Analysing \"%s\""), name);
		g_free (name);
	
		brasero_job_set_current_action (BRASERO_JOB (transcode),
						BRASERO_BURN_ACTION_ANALYSING,
						string,
						TRUE);
		g_free (string);

		BRASERO_JOB_LOG (transcode,
				 "Analysing Track %s",
				 uri);

		brasero_job_start_progress (BRASERO_JOB (transcode), FALSE);
		if (!brasero_transcode_is_mp3 (transcode))
			return brasero_transcode_song_end_reached (transcode);
	}
	else {
		BRASERO_GET_BASENAME_FOR_DISPLAY (uri, name);
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
		}
		else
			BRASERO_JOB_LOG (transcode,
					 "start piping %s",
					 uri)
	}

	g_free (uri);
	return BRASERO_BURN_OK;
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
		BRASERO_JOB_LOG (transcode, debug);
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

		if (state == GST_STATE_PLAYING) {
			BraseroJobAction action;

			brasero_job_get_action (BRASERO_JOB (transcode), &action);
			if (action == BRASERO_JOB_ACTION_SIZE) {
				if (!brasero_transcode_is_mp3 (transcode)) {
					brasero_transcode_song_end_reached (transcode);
					return TRUE;
				}
			}

			brasero_transcode_active_state (transcode);
		}

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
				      gboolean arg2,
				      GstElement *convert)
{
	GstPad *sink;
	GstCaps *caps;
	GstStructure *structure;

	sink = gst_element_get_pad (convert, "sink");
	if (GST_PAD_IS_LINKED (sink))
		return;

	/* make sure we only have audio */
	caps = gst_pad_get_caps (pad);
	if (!caps)
		return;

	structure = gst_caps_get_structure (caps, 0);
	if (structure
	&&  g_strrstr (gst_structure_get_name (structure), "audio"))
		gst_pad_link (pad, sink);

	gst_object_unref (sink);
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

static BraseroBurnResult
brasero_transcode_export_caps (BraseroPlugin *plugin, gchar **error)
{
	GSList *input;
	GSList *output;

	brasero_plugin_define (plugin,
			       "transcode",
			       _("Transcode converts song files into a format proper to burn them on CDs"),
			       "Philippe Rouquier",
			       0);

	output = brasero_caps_audio_new (BRASERO_PLUGIN_IO_ACCEPT_FILE|
					 BRASERO_PLUGIN_IO_ACCEPT_PIPE,
					 BRASERO_AUDIO_FORMAT_RAW);

	input = brasero_caps_audio_new (BRASERO_PLUGIN_IO_ACCEPT_FILE,
					BRASERO_AUDIO_FORMAT_UNDEFINED);

	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (output);
	g_slist_free (input);

	return BRASERO_BURN_OK;
}
