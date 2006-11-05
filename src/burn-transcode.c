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

#include <gst/gst.h>

#include <nautilus-burn-drive.h>

#include "burn-basics.h"
#include "brasero-marshal.h"
#include "burn-caps.h"
#include "burn-common.h"
#include "burn-job.h"
#include "burn-imager.h"
#include "burn-transcode.h"

static void brasero_transcode_class_init (BraseroTranscodeClass *klass);
static void brasero_transcode_init (BraseroTranscode *sp);
static void brasero_transcode_finalize (GObject *object);
static void brasero_transcode_iface_init_image (BraseroImagerIFace *iface);

static BraseroBurnResult
brasero_transcode_set_source (BraseroJob *job,
			      const BraseroTrackSource *source,
			      GError **error);
static BraseroBurnResult
brasero_transcode_set_output (BraseroImager *imager,
			      const char *output,
			      gboolean overwrite,
			      gboolean clean,
			      GError **error);
static BraseroBurnResult
brasero_transcode_set_append (BraseroImager *imager,
			      NautilusBurnDrive *drive,
			      gboolean merge,
			      GError **error);
static BraseroBurnResult
brasero_transcode_set_output_type (BraseroImager *imager, 
				   BraseroTrackSourceType type,
				   BraseroImageFormat format,
				   GError **error);
static BraseroBurnResult
brasero_transcode_get_track (BraseroImager *imager,
			     BraseroTrackSource **track,
			     GError **error);
static BraseroBurnResult
brasero_transcode_get_track_type (BraseroImager *imager,
				  BraseroTrackSourceType *type,
				  BraseroImageFormat *format);
static BraseroBurnResult
brasero_transcode_get_size (BraseroImager *imager,
			    gint64 *size,
			    gboolean sectors,
			    GError **error);

static void
brasero_transcode_clock_tick (BraseroTask *task,
			      BraseroTranscode *transcode);

static BraseroBurnResult
brasero_transcode_start (BraseroJob *job,
			 int fd_in,
			 int *fd_out,
			 GError **error);
static BraseroBurnResult
brasero_transcode_stop (BraseroJob *job,
			BraseroBurnResult retval,
			GError **error);
static BraseroBurnResult
brasero_transcode_set_rate (BraseroJob *job,
			    gint64 rate);

static gboolean brasero_transcode_bus_messages (GstBus *bus,
						  GstMessage *msg,
						  BraseroTranscode *transcode);
static void brasero_transcode_new_decoded_pad_cb (GstElement *decode,
						   GstPad *pad,
						   gboolean arg2,
						   GstElement *convert);
static BraseroBurnResult
brasero_transcode_next_song (BraseroTranscode *transcode,
			     GError **error);

typedef struct _BraseroSong BraseroSong;
struct _BraseroSong {
	BraseroSong *next;
	gchar *uri;
	gchar *dest;

	guint64 duration;
	gint sectors;
	gint64 gap;
	gint start;
	gint index;

	/* for CD-Text */
	gchar *artist;
	gchar *title;
	gchar *composer;
	gint isrc;
};

typedef enum {
	BRASERO_TRANSCODE_ACTION_NONE,
	BRASERO_TRANSCODE_ACTION_INF,
	BRASERO_TRANSCODE_ACTION_TRANSCODING,
	BRASERO_TRANSCODE_ACTION_GETTING_SIZE
} BraseroTranscodeAction;

struct BraseroTranscodePrivate {
	BraseroBurnCaps *caps;

	BraseroTranscodeAction action;

	GstElement *pipeline;
	GstElement *identity;
	GstElement *convert;
	GstElement *decode;
	GstElement *source;
	GstElement *sink;

	gint64 rate;

	gchar *output;
	gint pipe_out;

	/* used to report progress and how many sectors were written */
	gint64 global_pos;

	BraseroTrackSourceType track_type;
	BraseroTrackSource *source_track;
	BraseroSong *current;
	BraseroSong *songs;
	gchar *album;

	/* global size in sectors for the session */
	gint64 sectors_num;

	gint pad_size;
	gint pad_id;

	gint clock_id;

	gint audio_ready:1;
	gint inf_ready:1;

	gint own_output:1;
	gint overwrite:1;
	gint clean:1;

	gint on_the_fly:1;

	gint new_track:1;
};

#define SECTORS_TO_DURATION(sectors)	((gint64) sectors * GST_SECOND / 75)
#define DURATION_TO_SECTORS(duration)	((gint64) duration * 75 / GST_SECOND)

static GObjectClass *parent_class = NULL;

GType
brasero_transcode_get_type ()
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroTranscodeClass),
			NULL,
			NULL,
			(GClassInitFunc) brasero_transcode_class_init,
			NULL,
			NULL,
			sizeof (BraseroTranscode),
			0,
			(GInstanceInitFunc) brasero_transcode_init,
		};

		static const GInterfaceInfo imager_info =
		{
			(GInterfaceInitFunc) brasero_transcode_iface_init_image,
			NULL,
			NULL
		};
		type = g_type_register_static (BRASERO_TYPE_JOB,
					       "BraseroTranscode",
					       &our_info, 0);

		g_type_add_interface_static (type,
					     BRASERO_TYPE_IMAGER,
					     &imager_info);
	}

	return type;
}

static void
brasero_transcode_iface_init_image (BraseroImagerIFace *iface)
{
	iface->get_size = brasero_transcode_get_size;
	iface->get_track = brasero_transcode_get_track;

	iface->set_output = brasero_transcode_set_output;
	iface->get_track_type = brasero_transcode_get_track_type;
	iface->set_output_type = brasero_transcode_set_output_type;
	iface->set_append = brasero_transcode_set_append;
}

static void
brasero_transcode_class_init (BraseroTranscodeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroJobClass *job_class = BRASERO_JOB_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_transcode_finalize;

	job_class->set_source = brasero_transcode_set_source;
	job_class->set_rate = brasero_transcode_set_rate;
	job_class->start = brasero_transcode_start;
	job_class->stop = brasero_transcode_stop;
}

static void
brasero_transcode_init (BraseroTranscode *obj)
{
	obj->priv = g_new0 (BraseroTranscodePrivate, 1);

	obj->priv->clean = TRUE;
	obj->priv->pipe_out = -1;
	obj->priv->caps = brasero_burn_caps_get_default ();
}

static void
brasero_transcode_free_songs (BraseroTranscode *transcode)
{
	BraseroSong *song, *next;

	if (transcode->priv->album) {
		g_free (transcode->priv->album);
		transcode->priv->album = NULL;
	}

	for (song = transcode->priv->songs; song; song = next) {
		next = song->next;
	
		if (song->uri)
			g_free (song->uri);
		if (song->title)
			g_free (song->title);
		if (song->artist)
			g_free (song->artist);
		if (song->composer)
			g_free (song->composer);

		if (song->dest) {
			if (transcode->priv->clean)
				g_remove (song->dest);

			g_free (song->dest);
		}

		g_free (song);
	}

	if (transcode->priv->own_output) {
		g_remove (transcode->priv->output);
		transcode->priv->own_output = 0;
	}

	transcode->priv->songs = NULL;
	transcode->priv->current = NULL;
	transcode->priv->sectors_num = 0;
	transcode->priv->global_pos = 0;

	transcode->priv->inf_ready = 0;
	transcode->priv->audio_ready = 0;
}

static void
brasero_transcode_finalize (GObject *object)
{
	BraseroTranscode *cobj;

	cobj = BRASERO_TRANSCODE (object);

	if (cobj->priv->pad_id) {
		g_source_remove (cobj->priv->pad_id);
		cobj->priv->pad_id = 0;
	}

	if (cobj->priv->pipe_out != -1) {
		close (cobj->priv->pipe_out);
		cobj->priv->pipe_out = -1;
	}

	if (cobj->priv->pipeline) {
		gst_element_set_state (cobj->priv->pipeline, GST_STATE_NULL);
		gst_object_unref (GST_OBJECT (cobj->priv->pipeline));
	}

	if (cobj->priv->caps) {
		g_object_unref (cobj->priv->caps);
		cobj->priv->caps = NULL;
	}

	if (cobj->priv->source_track) {
		brasero_track_source_free (cobj->priv->source_track);
		cobj->priv->source_track = NULL;
	}

	brasero_transcode_free_songs (cobj);

	if (cobj->priv->output) {
		g_free (cobj->priv->output);
		cobj->priv->output = NULL;
	}

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

BraseroTranscode *
brasero_transcode_new (void)
{
	BraseroTranscode *obj;

	obj = BRASERO_TRANSCODE (g_object_new (BRASERO_TYPE_TRANSCODE,NULL));
	return obj;
}

static BraseroBurnResult
brasero_transcode_set_rate (BraseroJob *job, gint64 rate)
{
	BraseroTranscode *transcode;

	transcode = BRASERO_TRANSCODE (job);

	if (transcode->priv->rate == rate)
		return BRASERO_BURN_OK;

	transcode->priv->rate = rate;
	if (transcode->priv->identity)
		g_object_set (transcode->priv->identity,
			      "datarate", rate,
			      NULL);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_transcode_set_source (BraseroJob *job,
			      const BraseroTrackSource *source,
			      GError **error)
{
	BraseroTranscode *transcode;
	BraseroSong *song = NULL;
	GSList *iter;
	int num = 0;

	transcode = BRASERO_TRANSCODE (job);

	brasero_transcode_free_songs (transcode);

	g_return_val_if_fail (source != NULL, BRASERO_BURN_ERR);

	if (source->type != BRASERO_TRACK_SOURCE_SONG)
		BRASERO_JOB_NOT_SUPPORTED (transcode);

	transcode->priv->album = g_strdup (source->contents.songs.album);

	transcode->priv->sectors_num = 0;
	transcode->priv->global_pos = 0;
	for (iter = source->contents.songs.files; iter; iter = iter->next) {
		BraseroSongFile *file;

		file = iter->data;
		if (song) {
			song->next = g_new0 (BraseroSong, 1);
			song = song->next;
		}
		else {
			song = g_new0 (BraseroSong, 1);
			transcode->priv->songs = song;
		}

		if (file->gap)
			song->gap = file->gap;
		if (file->title)
			song->title = g_strdup (file->title);
		if (file->artist)
			song->artist = g_strdup (file->artist);
		if (file->composer)
			song->composer = g_strdup (file->composer);
		if (file->isrc > 0)
			song->isrc = file->isrc;

		song->uri = g_strdup (file->uri);
		song->index = num;
		num ++;
	}

	if (transcode->priv->source_track)
		brasero_track_source_free (transcode->priv->source_track);

	transcode->priv->source_track = brasero_track_source_copy (source);

	return BRASERO_BURN_OK;
}

static void
brasero_transcode_rm_songs_from_disc (BraseroTranscode *transcode)
{
	BraseroSong *song;

	if (!transcode->priv->clean || !transcode->priv->songs)
		return;

	for (song = transcode->priv->songs; song; song =song->next) {
		if (song->dest) {
			if (transcode->priv->audio_ready)
				g_remove (song->dest);

			g_free (song->dest);
			song->dest = NULL;
		}
	}

	transcode->priv->global_pos = 0;
	transcode->priv->inf_ready = 0;
	transcode->priv->audio_ready = 0;
}

static BraseroBurnResult
brasero_transcode_set_append (BraseroImager *imager,
			      NautilusBurnDrive *drive,
			      gboolean merge,
			      GError **error)
{
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_transcode_set_output (BraseroImager *imager,
			      const char *output,
			      gboolean overwrite,
			      gboolean clean,
			      GError **error)
{
	BraseroTranscode *transcode;

	transcode = BRASERO_TRANSCODE (imager);

	if (transcode->priv->clean) {
		brasero_transcode_rm_songs_from_disc (transcode);

		if (transcode->priv->own_output) {
			g_remove (transcode->priv->output);
			transcode->priv->own_output = 0;
		}
	}

	if (transcode->priv->output) {
		g_free (transcode->priv->output);
		transcode->priv->output = NULL;
	}

	/* we check that this is a directory */
	if (output) {
		if (!g_file_test (output, G_FILE_TEST_IS_DIR)) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("%s is not a directory"),
				     output);
			return BRASERO_BURN_ERR;
		}

		transcode->priv->output = g_strdup (output);
		transcode->priv->own_output = 0;
	}
	else
		transcode->priv->own_output = 1;

	transcode->priv->overwrite = overwrite;
	transcode->priv->clean = clean;
	
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_transcode_set_output_type (BraseroImager *imager, 
				   BraseroTrackSourceType type,
				   BraseroImageFormat format,
				   GError **error)
{
	BraseroTranscode *transcode;

	transcode = BRASERO_TRANSCODE (imager);

	if (format != BRASERO_IMAGE_FORMAT_NONE)
		BRASERO_JOB_NOT_SUPPORTED (transcode);

	if (type != BRASERO_TRACK_SOURCE_INF
	&&  type != BRASERO_TRACK_SOURCE_AUDIO
	&&  type != BRASERO_TRACK_SOURCE_DEFAULT)
		BRASERO_JOB_NOT_SUPPORTED (transcode);

	transcode->priv->track_type = type;
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_transcode_get_track (BraseroImager *imager,
			     BraseroTrackSource **track,
			     GError **error)
{
	BraseroTrackSourceType target;
	BraseroTranscode *transcode;
	BraseroTrackSource *retval;
	BraseroBurnResult result;
	BraseroSong *iter;

	transcode = BRASERO_TRANSCODE (imager);

	if (transcode->priv->track_type == BRASERO_TRACK_SOURCE_DEFAULT)
		target = brasero_burn_caps_get_imager_default_target (transcode->priv->caps,
								      transcode->priv->source_track);
	else
		target = transcode->priv->track_type;

	if (target == BRASERO_TRACK_SOURCE_INF && !transcode->priv->inf_ready) {
		transcode->priv->global_pos = 0;
	
		transcode->priv->action = BRASERO_TRANSCODE_ACTION_INF;
		result = brasero_job_run (BRASERO_JOB (imager), error);
		transcode->priv->action = BRASERO_TRANSCODE_ACTION_NONE;
	
		if (result != BRASERO_BURN_OK)
			return result;

		transcode->priv->inf_ready = 1;
	}
	else if (target == BRASERO_TRACK_SOURCE_AUDIO) {
		transcode->priv->global_pos = 0;
	
		transcode->priv->action = BRASERO_TRANSCODE_ACTION_TRANSCODING;
		result = brasero_job_run (BRASERO_JOB (imager), error);
		transcode->priv->action = BRASERO_TRANSCODE_ACTION_NONE;
	
		if (result != BRASERO_BURN_OK)
			return result;

		transcode->priv->audio_ready = 1;
		transcode->priv->inf_ready = 1;
	}

	retval = g_new0 (BraseroTrackSource, 1);
	retval->type = target;

	if (transcode->priv->album)
		retval->contents.audio.album = g_strdup (transcode->priv->album);

	for (iter = transcode->priv->songs; iter; iter = iter->next) {
		BraseroSongInfo *info;

		info = g_new0 (BraseroSongInfo, 1);
		info->title = g_strdup (iter->title);
		info->artist = g_strdup (iter->artist);
		info->composer = g_strdup (iter->composer);
		info->path = g_strdup (iter->dest);
		info->isrc = iter->isrc;
		info->duration = iter->duration;
		info->sectors = iter->sectors;
			
		retval->contents.audio.infos = g_slist_append (retval->contents.audio.infos,
							       info);
	}

	*track = retval;
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_transcode_get_size (BraseroImager *imager,
			    gint64 *size,
			    gboolean sectors,
			    GError **error)
{
	BraseroTranscode *transcode;
	BraseroBurnResult result = BRASERO_BURN_OK;

	transcode = BRASERO_TRANSCODE (imager);

	if (!transcode->priv->songs)
		BRASERO_JOB_NOT_READY (transcode);

	if (transcode->priv->sectors_num <= 0) {
		if (brasero_job_is_running (BRASERO_JOB (transcode)))
			return BRASERO_BURN_RUNNING;

		/* now we need to be able to return the global size */
		transcode->priv->action = BRASERO_TRANSCODE_ACTION_GETTING_SIZE;
		result = brasero_job_run (BRASERO_JOB (transcode), error);
		transcode->priv->action = BRASERO_TRANSCODE_ACTION_NONE;

		if (result != BRASERO_BURN_OK)
			return result;
	}

	if (sectors)
		*size = transcode->priv->sectors_num;
	else
		*size = transcode->priv->sectors_num * 2352;

	return result;
}

static gboolean
brasero_transcode_create_pipeline (BraseroTranscode *transcode, GError **error)
{
	GstElement *pipeline;
	GstCaps *filtercaps;
	GstBus *bus = NULL;
	GstElement *convert = NULL;
	GstElement *resample = NULL;
	GstElement *source;
	GstElement *identity = NULL;
	GstElement *decode;
	GstElement *filter = NULL;
	GstElement *sink = NULL;
	BraseroSong *song;

	transcode->priv->new_track = 1;

	song = transcode->priv->current;
	if (!song)
		return FALSE;

	/* free the possible current pipeline and create a new one */
	if (transcode->priv->pipeline) {
		gst_element_set_state (transcode->priv->pipeline, GST_STATE_NULL);
		gst_object_unref (G_OBJECT (transcode->priv->pipeline));
		transcode->priv->pipeline = NULL;
		transcode->priv->sink = NULL;
		transcode->priv->source = NULL;
		transcode->priv->convert = NULL;
		transcode->priv->identity = NULL;
		transcode->priv->pipeline = NULL;
	}

	/* create three types of pipeline according to the needs:
	 * - filesrc ! decodebin ! audioconvert ! fakesink (find size/write infs)
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
	source = gst_element_make_from_uri (GST_URI_SRC,
					    song->uri,
					    NULL);
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
	switch (transcode->priv->action) {
	case BRASERO_TRANSCODE_ACTION_GETTING_SIZE:
		sink = gst_element_factory_make ("fakesink", NULL);
		break;

	case BRASERO_TRANSCODE_ACTION_INF:
		sink = gst_element_factory_make ("fakesink", NULL);
		break;

	case BRASERO_TRANSCODE_ACTION_TRANSCODING:
		if (!transcode->priv->on_the_fly) {
			char *path;

			path = g_strdup_printf ("%s/Track%02i.cdr",
						transcode->priv->output,
						song->index);
		
			if (!transcode->priv->overwrite && g_file_test (path, G_FILE_TEST_EXISTS)) {
				g_set_error (error,
					     BRASERO_BURN_ERROR,
					     BRASERO_BURN_ERROR_FILE_EXIST,
					     _("%s already exist (can't overwrite)"),
					     path);
		
				g_free (path);
				return BRASERO_BURN_ERR;
			}

			sink = gst_element_factory_make ("filesink", NULL);
			g_object_set (sink,
				      "location", path,
				      NULL);
			song->dest = path;
		}
		else {
			sink = gst_element_factory_make ("fdsink", NULL);
			g_object_set (sink,
				      "fd", transcode->priv->pipe_out,
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

	if (transcode->priv->action == BRASERO_TRANSCODE_ACTION_TRANSCODING) {
		/* identity to control rate of data */
/*		identity = gst_element_factory_make ("identity", NULL);
		if (identity == NULL) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("identity can't be created"));
			goto error;
		}
		gst_bin_add (GST_BIN (pipeline), identity);
		if (transcode->priv->rate)
			g_object_set (identity,
				      "datarate", transcode->priv->rate,
				      "sync", TRUE,
				      NULL);
*/
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
	transcode->priv->decode = decode;

	if (transcode->priv->action == BRASERO_TRANSCODE_ACTION_TRANSCODING) {
		if (transcode->priv->on_the_fly)
			transcode->priv->sink = sink;

		gst_element_link_many (source, /* identity, */ decode, NULL);
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

	transcode->priv->source = source;
	transcode->priv->convert = convert;
	transcode->priv->identity = identity;
	transcode->priv->pipeline = pipeline;

	gst_element_set_state (transcode->priv->pipeline, GST_STATE_PLAYING);
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
brasero_transcode_set_inf (BraseroTranscode *transcode,
			   BraseroSong *song,
			   gint64 duration)
{
	gint64 sectors;

	/* 1 sec = 75 sectors = 2352 bytes */
	sectors = duration * 75;
	if (sectors % 1000000000)
		sectors = sectors / 1000000000 + 1;
	else
		sectors /= 1000000000;

	/* if it's on the fly we add 2 sector for security since gstreamer is
	 * not really reliable when it comes to getting an accurate duration */
	/*if (transcode->priv->action == BRASERO_TRANSCODE_ACTION_INF)
		sectors += 2;*/

	/* gap > 0 means a gap is required after the song */
	if (song->gap >= 0)
		sectors += song->gap;

	/* if transcoding on the fly we should add some length just to make
	 * sure we won't be too short (gstreamer duration discrepancy) */
	transcode->priv->sectors_num -= song->sectors;
	transcode->priv->sectors_num += sectors;
	BRASERO_JOB_TASK_SET_TOTAL (transcode, transcode->priv->sectors_num * 2352);

	song->sectors = sectors;
	song->duration = duration;

	BRASERO_JOB_LOG (transcode,
			 "Song %s"
			 "\nsectors %" G_GINT64_FORMAT
			 "\ntime %" G_GINT64_FORMAT 
			 "\nTOTAL %i sectors\n",
			 song->uri,
			 sectors,
			 duration,
			 transcode->priv->sectors_num);
}

static BraseroBurnResult
brasero_transcode_create_inf_siblings (BraseroTranscode *transcode,
				       BraseroSong *src,
				       BraseroSong *dest,
				       GError **error)
{
	/* it means the same file uri is in the selection and
	 * was already created. Simply get the values for the 
	 * inf file from the already created one and write the
	 * inf file */
	brasero_transcode_set_inf (transcode, dest, src->duration);

	/* let's not forget the tags */
	if (!dest->artist)
		dest->artist = g_strdup (src->artist);
	if (!dest->composer)
		dest->composer = g_strdup (src->composer);
	if (!dest->title)
		dest->title = g_strdup (src->title);

	/* since we skip this song we update the position */
	transcode->priv->global_pos += src->duration;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_transcode_create_song_siblings (BraseroTranscode *transcode,
					BraseroSong *src,
					BraseroSong *dest,
					GError **error)
{
	gchar *path_dest;
	gchar *path_src;

	/* it means the file is already in the selection.
	 * Simply create a symlink pointing to the first
	 * file in the selection with the same uri */
	path_src = g_strdup_printf ("%s/Track%02i.cdr",
				    transcode->priv->output,
				    src->index);

	path_dest = g_strdup_printf ("%s/Track%02i.cdr",
				     transcode->priv->output,
				     dest->index);

	/* check that path_dest doesn't exist */
	if (g_file_test (path_dest, G_FILE_TEST_EXISTS)) {
		if (!transcode->priv->overwrite) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_FILE_EXIST,
				     _("%s already exist"),
				     path_dest);
			goto error;			
		}
		else
			g_remove (path_dest);
	}

	if (symlink (path_src, path_dest) == -1) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("a symlink could not be created (%s)"),
			     strerror (errno));

		goto error;
	}

	g_free (path_src);
	dest->dest = path_dest;

	/* now we generate the associated inf path */
	return brasero_transcode_create_inf_siblings (transcode, src, dest, error);

error:
	g_free (path_src);
	g_free (path_dest);

	return BRASERO_BURN_ERR;
}

static BraseroSong *
brasero_transcode_find_sibling (BraseroTranscode *transcode,
				BraseroSong *song)
{
	BraseroSong *sibling;

	sibling = transcode->priv->songs;
	for (; sibling && sibling != song; sibling = sibling->next) {
		if (!strcmp (sibling->uri, song->uri))
			return sibling;
	}

	return NULL;
}

static gboolean
brasero_transcode_check_tmpdir (BraseroTranscode *transcode, GError **error)
{
	char *folder;

	if (transcode->priv->output)
		return TRUE;

	folder = g_strdup_printf ("%s/"BRASERO_BURN_TMP_FILE_NAME, g_get_tmp_dir ());
	transcode->priv->output = mkdtemp (folder);

	if (!transcode->priv->output) {
		g_free (folder);
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_TMP_DIR,
			     _("a temporary directory could not be created"));
		return FALSE;
	}

	BRASERO_JOB_LOG (transcode,
			 "created temporary directory %s",
			 transcode->priv->output);

	transcode->priv->own_output = 1;

	return TRUE;
}

static BraseroBurnResult
brasero_transcode_start (BraseroJob *job,
			 int in_fd,
			 int *out_fd,
			 GError **error)
{
	BraseroTranscode *transcode;

	transcode = BRASERO_TRANSCODE (job);

	if (in_fd > 0)
		BRASERO_JOB_NOT_SUPPORTED (transcode);

	if (!transcode->priv->songs)
		BRASERO_JOB_NOT_READY (transcode);

	transcode->priv->global_pos = 0;
	transcode->priv->current = transcode->priv->songs;

	if (transcode->priv->action == BRASERO_TRANSCODE_ACTION_GETTING_SIZE) {
		transcode->priv->on_the_fly = 0;

		if (!brasero_transcode_create_pipeline (transcode, error))
			return BRASERO_BURN_ERR;

		BRASERO_JOB_TASK_SET_ACTION (transcode,
					     BRASERO_BURN_ACTION_GETTING_SIZE,
					     NULL,
					     TRUE);
		BRASERO_JOB_TASK_START_PROGRESS (transcode, FALSE);
		return BRASERO_BURN_OK;
	}

	if (out_fd) {
		int pipe_out [2];
		BraseroBurnResult result;

		transcode->priv->action = BRASERO_TRANSCODE_ACTION_TRANSCODING;

		/* now we generate the data, piping it to cdrecord presumably */
		result = brasero_common_create_pipe (pipe_out, error);
		if (result != BRASERO_BURN_OK)
			return result;

		transcode->priv->pipe_out = pipe_out [1];
		transcode->priv->on_the_fly = 1;
		if (!brasero_transcode_create_pipeline (transcode, error)) {
			close (pipe_out [0]);
			close (pipe_out [1]);
			return BRASERO_BURN_ERR;
		}

		*out_fd = pipe_out [0];
	}
	else if (transcode->priv->action == BRASERO_TRANSCODE_ACTION_TRANSCODING) {
		if (!brasero_transcode_check_tmpdir (transcode, error))
			return BRASERO_BURN_ERR;

		transcode->priv->on_the_fly = 0;
		if (!brasero_transcode_create_pipeline (transcode, error))
			return BRASERO_BURN_ERR;
	}
	else if (transcode->priv->action == BRASERO_TRANSCODE_ACTION_INF) {
		if (!brasero_transcode_check_tmpdir (transcode, error))
			return BRASERO_BURN_ERR;

		if (!brasero_transcode_create_pipeline (transcode, error))
			return BRASERO_BURN_ERR;
	}
	else
		BRASERO_JOB_NOT_SUPPORTED (transcode);

	BRASERO_JOB_TASK_SET_TOTAL (transcode, transcode->priv->sectors_num * 2352);
	BRASERO_JOB_TASK_CONNECT_TO_CLOCK (transcode,
					   brasero_transcode_clock_tick,
					   transcode->priv->clock_id);

	return BRASERO_BURN_OK;
}

static void
brasero_transcode_stop_pipeline (BraseroTranscode *transcode)
{
	if (!transcode->priv->pipeline)
		return;

	gst_element_set_state (transcode->priv->pipeline, GST_STATE_NULL);
	gst_object_unref (GST_OBJECT (transcode->priv->pipeline));
	transcode->priv->pipeline = NULL;
	transcode->priv->sink = NULL;
	transcode->priv->source = NULL;
	transcode->priv->convert = NULL;
	transcode->priv->identity = NULL;
	transcode->priv->pipeline = NULL;
}

static BraseroBurnResult
brasero_transcode_stop (BraseroJob *job,
			BraseroBurnResult retval,
			GError **error)
{
	BraseroTranscode *transcode;

	transcode = BRASERO_TRANSCODE (job);

	transcode->priv->current = NULL;

	BRASERO_JOB_TASK_DISCONNECT_FROM_CLOCK (transcode, transcode->priv->clock_id);
	
	if (transcode->priv->pad_id) {
		g_source_remove (transcode->priv->pad_id);
		transcode->priv->pad_id = 0;
	}

	brasero_transcode_stop_pipeline (transcode);

	if (transcode->priv->pipe_out != -1) {
		close (transcode->priv->pipe_out);
		transcode->priv->pipe_out = -1;
	}

	return retval;
}

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

static gboolean
brasero_transcode_pad_idle (BraseroTranscode *transcode)
{
	gint64 bytes2write;
	GError *error = NULL;
	BraseroBurnResult result;

	bytes2write = brasero_transcode_pad_real (transcode,
						  transcode->priv->pipe_out,
						  transcode->priv->pad_size,
						  &error);

	if (bytes2write == -1) {
		transcode->priv->pad_id = 0;
		brasero_job_error (BRASERO_JOB (transcode), error);
		return FALSE;
	}

	if (bytes2write) {
		transcode->priv->pad_size = bytes2write;
		return TRUE;
	}

	/* we are finished with padding */
	transcode->priv->pad_id = 0;
	if (!transcode->priv->on_the_fly) {
		close (transcode->priv->pipe_out);
		transcode->priv->pipe_out = -1;
	}

	/* set the next song or finish */
	result = brasero_transcode_next_song (transcode, &error);
	if (result != BRASERO_BURN_OK) {
		brasero_job_error (BRASERO_JOB (transcode), error);
		return FALSE;
	}

	if (!transcode->priv->current) {
		brasero_job_finished (BRASERO_JOB (transcode));
		return FALSE;
	}

	return FALSE;
}

static gboolean
brasero_transcode_is_mp3 (BraseroTranscode *transcode)
{
	GstElement *typefind;
	GstCaps *caps = NULL;
	const gchar *mime;

	/* find the type of the file */
	typefind = gst_bin_get_by_name (GST_BIN (transcode->priv->decode),
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
brasero_transcode_get_duration (BraseroTranscode *transcode)
{
	GstElement *element;
	gint64 duration = -1;
	GstFormat format = GST_FORMAT_TIME;

	/* this is the most reliable way to get the duration for mp3 read them
	 * till the end and get the position. Convert is then needed. */
	if (transcode->priv->action != BRASERO_TRANSCODE_ACTION_GETTING_SIZE
	&&  brasero_transcode_is_mp3 (transcode)) {
		if (transcode->priv->convert)
			element = transcode->priv->convert;
		else
			element = transcode->priv->pipeline;

		gst_element_query_position (GST_ELEMENT (element),
					    &format,
					    &duration);
	}

	if (duration == -1)
		gst_element_query_duration (GST_ELEMENT (transcode->priv->pipeline),
					    &format,
					    &duration);

	BRASERO_JOB_LOG (transcode, "got duration %"G_GINT64_FORMAT"\n", duration);

	if (duration == -1)	
	    brasero_job_error (BRASERO_JOB (transcode),
			       g_error_new (BRASERO_BURN_ERROR,
					    BRASERO_BURN_ERROR_GENERAL,
					    _("error getting duration")));
	return duration;
}

/* we must make sure that the track size is a multiple
 * of 2352 to be burnt by cdrecord with on the fly */
static gboolean
brasero_transcode_pad (BraseroTranscode *transcode, int fd, GError **error)
{
	gint64 duration;
	gint64 b_written;
	gint64 bytes2write;

	duration = brasero_transcode_get_duration (transcode);
	if (duration == -1)
		return FALSE;

	/* we need to comply more or less to what we told
	 * cdrecord about the size of the track in sectors */
	b_written = duration * 75 * 2352;
	b_written = b_written % 1000000000 ? b_written / 1000000000 + 1 : b_written / 1000000000;
	bytes2write = transcode->priv->current->sectors * 2352 - b_written;

	BRASERO_JOB_LOG (transcode,
			 "Padding\t= %" G_GINT64_FORMAT 
			 "\n\t\t\t\t%i"
			 "\n\t\t\t\t%" G_GINT64_FORMAT,
			 bytes2write,
			 transcode->priv->current->sectors,
			 b_written);

	bytes2write = brasero_transcode_pad_real (transcode,
						  fd,
						  bytes2write,
						  error);
	if (bytes2write == -1)
		return FALSE;

	if (bytes2write) {
		/* when writing to a pipe it can happen that its buffer is full
		 * because cdrecord is not fast enough. Therefore we couldn't
		 * write/pad it and we'll have to wait for the pipe to become
		 * available again */
		transcode->priv->pipe_out = fd;
		transcode->priv->pad_size = bytes2write;
		transcode->priv->pad_id = g_timeout_add (50,
							 (GSourceFunc) brasero_transcode_pad_idle,
							 transcode);
		return FALSE;		
	}

	return TRUE;
}

static gboolean
brasero_transcode_pad_pipe (BraseroTranscode *transcode, GError **error)
{
	return brasero_transcode_pad (transcode, transcode->priv->pipe_out, error);
}

static gboolean
brasero_transcode_pad_file (BraseroTranscode *transcode, GError **error)
{
	int fd;
	gboolean result;

	fd = open (transcode->priv->current->dest, O_WRONLY | O_CREAT | O_APPEND);
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
brasero_transcode_set_current_song_start (BraseroTranscode *transcode, 
					  GError **error)
{
	gint64 duration;
	BraseroSong *current;
	GstFormat format = GST_FORMAT_TIME;

	/* NOTE: this function is only called when getting the sizes of files */
	/* as in metadata we first get the size through the tags if there is one
	 * if there is see that the following duration is not too far fetched. */
	/* now we set the length in sectors */
	current = transcode->priv->current;

	if (gst_element_query_duration (GST_ELEMENT (transcode->priv->pipeline),
					&format,
					&duration) == FALSE) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("error getting duration"));
		return FALSE;
	}

	if (current->duration) {
		if (duration > current->duration * 103 / 100
		||  duration < current->duration * 97 / 100)
			duration = current->duration;

		/* it was only temporarily based on GST_TAG_DURATION */
		current->duration = 0;
	}

	brasero_transcode_set_inf (transcode, transcode->priv->current, duration);
	return TRUE;
}

static BraseroBurnResult
brasero_transcode_next_song (BraseroTranscode *transcode, GError **error)
{
	BraseroSong *song;
	BraseroSong *sibling = NULL;

	/* make sure the pipeline is really stopped */
	brasero_transcode_stop_pipeline (transcode);
	if (!transcode->priv->current->next) {
		transcode->priv->current = NULL;
		return BRASERO_BURN_OK;
	}

	song = transcode->priv->current->next;
	transcode->priv->current = song;
	if (!song)
		return BRASERO_BURN_OK;

	if (!transcode->priv->on_the_fly)
		sibling = brasero_transcode_find_sibling (transcode, song);

	if (sibling) {
		BraseroBurnResult result;

		BRASERO_JOB_LOG (transcode,
				 "found sibling for %s : skipping\n",
				 song->uri);
		if (transcode->priv->action == BRASERO_TRANSCODE_ACTION_TRANSCODING
		&&  !transcode->priv->on_the_fly) {
			result = brasero_transcode_create_song_siblings (transcode,
									 sibling,
									 song,
									 error);
			if (result != BRASERO_BURN_OK)
				return result;

			return brasero_transcode_next_song (transcode, error);
		}
		else if (transcode->priv->action == BRASERO_TRANSCODE_ACTION_INF) {
			result = brasero_transcode_create_inf_siblings (transcode,
									sibling,
									song,
									error);
			if (result != BRASERO_BURN_OK)
				return result;

			return brasero_transcode_next_song (transcode, error);
		}
		else if (transcode->priv->action == BRASERO_TRANSCODE_ACTION_GETTING_SIZE) {
			brasero_transcode_set_inf (transcode, song, sibling->duration);
			return brasero_transcode_next_song (transcode, error);
		}	
	}

	if (!brasero_transcode_create_pipeline (transcode, error))
		return BRASERO_BURN_ERR;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_transcode_song_end_reached (BraseroTranscode *transcode)
{
	GError *error = NULL;
	BraseroBurnResult result;

	transcode->priv->global_pos += transcode->priv->current->duration;
	if (!transcode->priv->on_the_fly) {
		gint64 duration;

		/* this is when we need to write infs:
		 * - when asked to create infs
		 * - when decoding to a file */
		duration = brasero_transcode_get_duration (transcode);
		if (duration == -1)
			return FALSE;

		brasero_transcode_set_inf (transcode,
					   transcode->priv->current,
					   duration);
	}

	if (transcode->priv->action == BRASERO_TRANSCODE_ACTION_TRANSCODING) {
		gboolean result;

		/* pad file so it is a multiple of 2352 (= 1 sector) */
		if (transcode->priv->pipe_out != -1)
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

	/* set the next song */
	result = brasero_transcode_next_song (transcode, &error);
	if (result != BRASERO_BURN_OK) {
		brasero_job_error (BRASERO_JOB (transcode), error);
		return FALSE;
	}

	if (!transcode->priv->current) {
		brasero_job_finished (BRASERO_JOB (transcode));
		return FALSE;
	}

	return TRUE;
}

static gboolean 
brasero_transcode_get_size_end (BraseroTranscode *transcode)
{
	BraseroBurnResult result;
	GError *error = NULL;

	if (!brasero_transcode_set_current_song_start (transcode, &error)) {
		brasero_job_error (BRASERO_JOB (transcode), error);
		return FALSE;
	}

	result = brasero_transcode_next_song (transcode, &error);
	if (result != BRASERO_BURN_OK)
		brasero_job_error (BRASERO_JOB (transcode), error);
	else if (!transcode->priv->current)
		brasero_job_finished (BRASERO_JOB (transcode));

	return FALSE;
}

static void
foreach_tag (const GstTagList *list,
	     const gchar *tag,
	     BraseroTranscode *transcode)
{
	if (!strcmp (tag, GST_TAG_TITLE)) {
		if (!transcode->priv->current->title)
			gst_tag_list_get_string (list, tag, &(transcode->priv->current->title));
	}
	else if (!strcmp (tag, GST_TAG_ARTIST)) {
		if (!transcode->priv->current->artist)
			gst_tag_list_get_string (list, tag, &(transcode->priv->current->artist));
	}
	else if (!strcmp (tag, GST_TAG_ISRC)) {
		gst_tag_list_get_int (list, tag, &(transcode->priv->current->isrc));
	}
	else if (!strcmp (tag, GST_TAG_PERFORMER)) {
		if (!transcode->priv->current->artist)
			gst_tag_list_get_string (list, tag, &(transcode->priv->current->artist));
	}
	else if (transcode->priv->action == BRASERO_TRANSCODE_ACTION_GETTING_SIZE
	     &&  !strcmp (tag, GST_TAG_DURATION)) {
		/* this is only useful when we try to have the size */
		gst_tag_list_get_uint64 (list, tag, &(transcode->priv->current->duration));
	}
}

static gboolean
brasero_transcode_bus_messages (GstBus *bus,
				GstMessage *msg,
				BraseroTranscode *transcode)
{
	GstTagList *tags = NULL;
	GError *error = NULL;
	GstState state;
	char *debug;

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
		if (transcode->priv->action == BRASERO_TRANSCODE_ACTION_GETTING_SIZE)
			return brasero_transcode_get_size_end (transcode);
		else
			return brasero_transcode_song_end_reached (transcode);

	case GST_MESSAGE_STATE_CHANGED: {
		GstStateChangeReturn result;

		result = gst_element_get_state (transcode->priv->pipeline,
						&state,
						NULL,
						1);

		if (result != GST_STATE_CHANGE_SUCCESS)
			return TRUE;

		if (state != GST_STATE_PAUSED && state != GST_STATE_PLAYING)
			return TRUE;

		if (transcode->priv->action == BRASERO_TRANSCODE_ACTION_GETTING_SIZE) {
			/* try to do the same as in metadata to get a pretty
			 * good accurate size */
			if (brasero_transcode_is_mp3 (transcode)) {
				if (!gst_element_seek (transcode->priv->pipeline,
						       1.0,
						       GST_FORMAT_BYTES,
						       GST_SEEK_FLAG_FLUSH,
						       GST_SEEK_TYPE_SET,
						       52428800,
						       GST_FORMAT_UNDEFINED,
						       GST_CLOCK_TIME_NONE))
					g_warning ("Seeking forward was impossible.\n");
				return TRUE;
			}

			return brasero_transcode_get_size_end (transcode);
		}

		if (state == GST_STATE_PLAYING && transcode->priv->new_track) {
			transcode->priv->new_track = 0;

			if (transcode->priv->action == BRASERO_TRANSCODE_ACTION_INF) {
				gchar *name, *string;

				BRASERO_JOB_LOG (transcode,
						 "start generating inf %s/Track%02i.inf for %s",
						 transcode->priv->output,
						 transcode->priv->current->index,
						 transcode->priv->current->uri);

				BRASERO_GET_BASENAME_FOR_DISPLAY (transcode->priv->current->uri, name);
				string = g_strdup_printf (_("Analysing \"%s\""), name);
				g_free (name);

				BRASERO_JOB_TASK_SET_ACTION (transcode,
							     BRASERO_BURN_ACTION_ANALYSING,
							     string,
							     TRUE);
				g_free (string);

				BRASERO_JOB_TASK_START_PROGRESS (transcode, FALSE);

				if (!brasero_transcode_is_mp3 (transcode))
					return brasero_transcode_song_end_reached (transcode);
			}
			else {
				gchar *name, *string;

				BRASERO_GET_BASENAME_FOR_DISPLAY (transcode->priv->current->uri, name);
				string = g_strdup_printf (_("Transcoding \"%s\""), name);
			    	g_free (name);

				BRASERO_JOB_TASK_SET_ACTION (transcode,
							     BRASERO_BURN_ACTION_TRANSCODING,
							     string,
							     TRUE);
				g_free (string);

				BRASERO_JOB_TASK_START_PROGRESS (transcode, FALSE);

				if (transcode->priv->on_the_fly)
					BRASERO_JOB_LOG (transcode,
							 "start piping %s",
							 transcode->priv->current->uri)
				else
					BRASERO_JOB_LOG (transcode,
							 "start decoding %s to %s",
							 transcode->priv->current->uri,
							 transcode->priv->current->dest);
			}
		}

		return TRUE;
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
brasero_transcode_get_track_type (BraseroImager *imager,
				  BraseroTrackSourceType *type,
				  BraseroImageFormat *format)
{
	BraseroTranscode *transcode;
	BraseroTrackSourceType target;

	g_return_val_if_fail (type != NULL, BRASERO_BURN_ERR);

	transcode = BRASERO_TRANSCODE (imager);

	if (transcode->priv->track_type == BRASERO_TRACK_SOURCE_DEFAULT)
		target = brasero_burn_caps_get_imager_default_target (transcode->priv->caps,
								      transcode->priv->source_track);
	else
		target = transcode->priv->track_type;
	
	if (target == BRASERO_TRACK_SOURCE_UNKNOWN)
		BRASERO_JOB_NOT_READY (transcode);

	if (type)
		*type = target;

	if (format)
		format = BRASERO_IMAGE_FORMAT_NONE;

	return BRASERO_BURN_OK;
}

static void
brasero_transcode_clock_tick (BraseroTask *task,
			      BraseroTranscode *transcode)
{
	gint64 pos = -1;
	gpointer element;
	GstIterator *iterator;
	GstFormat format = GST_FORMAT_TIME;

	if (!transcode->priv->pipeline)
		return;

	/* this is a workaround : when asking the pipeline especially
	 * a pipeline such as filesrc ! decodebin ! fakesink we can't
	 * get the position in time or the results are simply crazy.
	 * so we iterate through the elements in decodebin until we
	 * find an element that can tell us the position. */
	iterator = gst_bin_iterate_sorted (GST_BIN (transcode->priv->decode));
	while (gst_iterator_next (iterator, &element) == GST_ITERATOR_OK) {
		if (gst_element_query_position (GST_ELEMENT (element),
						 &format,
						 &pos)) {
			gst_object_unref (element);
			break;
		}
		gst_object_unref (element);
	}
	gst_iterator_free (iterator);

	if (pos == -1) {
		BRASERO_JOB_LOG (transcode, "can't get position in the stream");
		return;
	}

	BRASERO_JOB_LOG (transcode,
			 "got position (%" G_GINT64_FORMAT ") and global (%"G_GINT64_FORMAT")",
			 pos,
			 transcode->priv->global_pos + pos);

	pos = transcode->priv->global_pos + pos;

	BRASERO_JOB_TASK_SET_WRITTEN (transcode, DURATION_TO_SECTORS (pos) * 2352);
}
