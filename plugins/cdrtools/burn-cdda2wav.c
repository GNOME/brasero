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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gmodule.h>

#include "brasero-units.h"

#include "burn-debug.h"
#include "burn-job.h"
#include "burn-process.h"
#include "brasero-plugin-registration.h"
#include "burn-cdrtools.h"
#include "brasero-track-disc.h"
#include "brasero-track-stream.h"

#define BRASERO_TYPE_CDDA2WAV         (brasero_cdda2wav_get_type ())
#define BRASERO_CDDA2WAV(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_CDDA2WAV, BraseroCdda2wav))
#define BRASERO_CDDA2WAV_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_CDDA2WAV, BraseroCdda2wavClass))
#define BRASERO_IS_CDDA2WAV(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_CDDA2WAV))
#define BRASERO_IS_CDDA2WAV_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_CDDA2WAV))
#define BRASERO_CDDA2WAV_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_CDDA2WAV, BraseroCdda2wavClass))

BRASERO_PLUGIN_BOILERPLATE (BraseroCdda2wav, brasero_cdda2wav, BRASERO_TYPE_PROCESS, BraseroProcess);

struct _BraseroCdda2wavPrivate {
	gchar *file_pattern;

	guint track_num;
	guint track_nb;

	guint is_inf	:1;
};
typedef struct _BraseroCdda2wavPrivate BraseroCdda2wavPrivate;

#define BRASERO_CDDA2WAV_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_CDDA2WAV, BraseroCdda2wavPrivate))
static GObjectClass *parent_class = NULL;


static BraseroBurnResult
brasero_cdda2wav_post (BraseroJob *job)
{
	BraseroCdda2wavPrivate *priv;
	BraseroMedium *medium;
	BraseroJobAction action;
	BraseroDrive *drive;
	BraseroTrack *track;
	int track_num;
	int i;

	priv = BRASERO_CDDA2WAV_PRIVATE (job);

	brasero_job_get_action (job, &action);
	if (action == BRASERO_JOB_ACTION_SIZE)
		return BRASERO_BURN_OK;

	/* we add the tracks */
	track = NULL;
	brasero_job_get_current_track (job, &track);

	drive = brasero_track_disc_get_drive (BRASERO_TRACK_DISC (track));
	medium = brasero_drive_get_medium (drive);

	track_num = brasero_medium_get_track_num (medium);
	for (i = 0; i < track_num; i ++) {
		BraseroTrackStream *track_stream;
		goffset block_num = 0;

		brasero_medium_get_track_space (medium, i + 1, NULL, &block_num);
		track_stream = brasero_track_stream_new ();

		brasero_track_stream_set_format (track_stream,
		                                 BRASERO_AUDIO_FORMAT_RAW|
		                                 BRASERO_METADATA_INFO);

		BRASERO_JOB_LOG (job, "Adding new audio track of size %" G_GOFFSET_FORMAT, BRASERO_BYTES_TO_DURATION (block_num * 2352));

		/* either add .inf or .cdr files */
		if (!priv->is_inf) {
			gchar *uri;
			gchar *filename;

			if (track_num == 1)
				filename = g_strdup_printf ("%s.cdr", priv->file_pattern);
			else
				filename = g_strdup_printf ("%s_%02i.cdr", priv->file_pattern, i + 1);

			uri = g_filename_to_uri (filename, NULL, NULL);
			g_free (filename);

			brasero_track_stream_set_source (track_stream, uri);
			g_free (uri);

			/* signal to cdrecord that we have an .inf file */
			if (i != 0)
				filename = g_strdup_printf ("%s_%02i.inf", priv->file_pattern, i);
			else
				filename = g_strdup_printf ("%s.inf", priv->file_pattern);

			brasero_track_tag_add_string (BRASERO_TRACK (track_stream),
			                              BRASERO_CDRTOOLS_TRACK_INF_FILE,
			                              filename);
			g_free (filename);
		}

		/* Always set the boundaries after the source as
		 * brasero_track_stream_set_source () resets the length */
		brasero_track_stream_set_boundaries (track_stream,
		                                     0,
		                                     BRASERO_BYTES_TO_DURATION (block_num * 2352),
		                                     0);
		brasero_job_add_track (job, BRASERO_TRACK (track_stream));
		g_object_unref (track_stream);
	}

	return brasero_job_finished_session (job);
}

static gboolean
brasero_cdda2wav_get_output_filename_pattern (BraseroCdda2wav *cdda2wav,
                                              GError **error)
{
	gchar *path;
	gchar *file_pattern;
	BraseroCdda2wavPrivate *priv;

	priv = BRASERO_CDDA2WAV_PRIVATE (cdda2wav);

	if (priv->file_pattern) {
		g_free (priv->file_pattern);
		priv->file_pattern = NULL;
	}

	/* Create a tmp directory so cdda2wav can
	 * put all its stuff in there */
	path = NULL;
	brasero_job_get_tmp_dir (BRASERO_JOB (cdda2wav), &path, error);
	if (!path)
		return FALSE;

	file_pattern = g_strdup_printf ("%s/cd_file", path);
	g_free (path);

	/* NOTE: this file pattern is used to
	 * name all wav and inf files. It is followed
	 * by underscore/number of the track/extension */

	priv->file_pattern = file_pattern;
	return TRUE;
}

static BraseroBurnResult
brasero_cdda2wav_read_stderr (BraseroProcess *process, const gchar *line)
{
	int num;
	BraseroCdda2wav *cdda2wav;
	BraseroCdda2wavPrivate *priv;

	cdda2wav = BRASERO_CDDA2WAV (process);
	priv = BRASERO_CDDA2WAV_PRIVATE (process);

	if (sscanf (line, "100%%  track %d '%*s' recorded successfully", &num) == 1) {
		gchar *string;

		priv->track_nb = num;
		string = g_strdup_printf (_("Copying audio track %02d"), priv->track_nb + 1);
		brasero_job_set_current_action (BRASERO_JOB (process),
		                                BRASERO_BURN_ACTION_DRIVE_COPY,
		                                string,
		                                TRUE);
		g_free (string);
	}
	else if (strstr (line, "percent_done:")) {
		gchar *string;

		string = g_strdup_printf (_("Copying audio track %02d"), 1);
		brasero_job_set_current_action (BRASERO_JOB (process),
		                                BRASERO_BURN_ACTION_DRIVE_COPY,
		                                string,
		                                TRUE);
		g_free (string);
	}
	/* we have to do this otherwise with sscanf it will 
	 * match every time it begins with a number */
	else if (strchr (line, '%') && sscanf (line, " %d%%", &num) == 1) {
		gdouble fraction;

		fraction = (gdouble) num / (gdouble) 100.0;
		fraction = ((gdouble) priv->track_nb + fraction) / (gdouble) priv->track_num;
		brasero_job_set_progress (BRASERO_JOB (cdda2wav), fraction);
		brasero_job_start_progress (BRASERO_JOB (process), FALSE);
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_cdda2wav_set_argv_image (BraseroCdda2wav *cdda2wav,
				GPtrArray *argv,
				GError **error)
{
	BraseroCdda2wavPrivate *priv;
	int fd_out;

	priv = BRASERO_CDDA2WAV_PRIVATE (cdda2wav);

	/* We want raw output */
	g_ptr_array_add (argv, g_strdup ("output-format=cdr"));

	/* we want all tracks */
	g_ptr_array_add (argv, g_strdup ("-B"));

	priv->is_inf = FALSE;

	if (brasero_job_get_fd_out (BRASERO_JOB (cdda2wav), &fd_out) == BRASERO_BURN_OK) {
		/* On the fly copying */
		g_ptr_array_add (argv, g_strdup ("-"));
	}
	else {
		if (!brasero_cdda2wav_get_output_filename_pattern (cdda2wav, error))
			return BRASERO_BURN_ERR;

		g_ptr_array_add (argv, g_strdup (priv->file_pattern));

		brasero_job_set_current_action (BRASERO_JOB (cdda2wav),
		                                BRASERO_BURN_ACTION_DRIVE_COPY,
		                                _("Preparing to copy audio disc"),
		                                FALSE);
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_cdda2wav_set_argv_size (BraseroCdda2wav *cdda2wav,
                                GPtrArray *argv,
                                GError **error)
{
	BraseroCdda2wavPrivate *priv;
	BraseroMedium *medium;
	BraseroTrack *track;
	BraseroDrive *drive;
	goffset medium_len;
	int i;

	priv = BRASERO_CDDA2WAV_PRIVATE (cdda2wav);

	/* we add the tracks */
	medium_len = 0;
	track = NULL;
	brasero_job_get_current_track (BRASERO_JOB (cdda2wav), &track);

	drive = brasero_track_disc_get_drive (BRASERO_TRACK_DISC (track));
	medium = brasero_drive_get_medium (drive);

	priv->track_num = brasero_medium_get_track_num (medium);
	for (i = 0; i < priv->track_num; i ++) {
		goffset len = 0;

		brasero_medium_get_track_space (medium, i, NULL, &len);
		medium_len += len;
	}
	brasero_job_set_output_size_for_current_track (BRASERO_JOB (cdda2wav), medium_len, medium_len * 2352);

	/* if there isn't any output file then that
	 * means we have to generate all the
	 * .inf files for cdrecord. */
	if (brasero_job_get_audio_output (BRASERO_JOB (cdda2wav), NULL) != BRASERO_BURN_OK)
		return BRASERO_BURN_NOT_RUNNING;

	/* we want all tracks */
	g_ptr_array_add (argv, g_strdup ("-B"));

	/* since we're running for an on-the-fly burning
	 * we only want the .inf files */
	g_ptr_array_add (argv, g_strdup ("-J"));

	if (!brasero_cdda2wav_get_output_filename_pattern (cdda2wav, error))
		return BRASERO_BURN_ERR;

	g_ptr_array_add (argv, g_strdup (priv->file_pattern));

	priv->is_inf = TRUE;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_cdda2wav_set_argv (BraseroProcess *process,
			  GPtrArray *argv,
			  GError **error)
{
	BraseroDrive *drive;
	const gchar *device;
	BraseroTrack *track;
	BraseroJobAction action;
	BraseroBurnResult result;
	BraseroCdda2wav *cdda2wav;

	cdda2wav = BRASERO_CDDA2WAV (process);

	g_ptr_array_add (argv, g_strdup ("cdda2wav"));

	/* Add the device path */
	track = NULL;
	result = brasero_job_get_current_track (BRASERO_JOB (process), &track);
	if (!track)
		return result;

	drive = brasero_track_disc_get_drive (BRASERO_TRACK_DISC (track));
	device = brasero_drive_get_device (drive);
	g_ptr_array_add (argv, g_strdup_printf ("dev=%s", device));

	/* Have it talking */
	g_ptr_array_add (argv, g_strdup ("-v255"));

	brasero_job_get_action (BRASERO_JOB (cdda2wav), &action);
	if (action == BRASERO_JOB_ACTION_SIZE)
		result = brasero_cdda2wav_set_argv_size (cdda2wav, argv, error);
	else if (action == BRASERO_JOB_ACTION_IMAGE)
		result = brasero_cdda2wav_set_argv_image (cdda2wav, argv, error);
	else
		BRASERO_JOB_NOT_SUPPORTED (cdda2wav);

	return result;
}

static void
brasero_cdda2wav_class_init (BraseroCdda2wavClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroProcessClass *process_class = BRASERO_PROCESS_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroCdda2wavPrivate));

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_cdda2wav_finalize;

	process_class->stderr_func = brasero_cdda2wav_read_stderr;
	process_class->set_argv = brasero_cdda2wav_set_argv;
	process_class->post = brasero_cdda2wav_post;
}

static void
brasero_cdda2wav_init (BraseroCdda2wav *obj)
{ }

static void
brasero_cdda2wav_finalize (GObject *object)
{
	BraseroCdda2wavPrivate *priv;

	priv = BRASERO_CDDA2WAV_PRIVATE (object);

	if (priv->file_pattern) {
		g_free (priv->file_pattern);
		priv->file_pattern = NULL;
	}
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_cdda2wav_export_caps (BraseroPlugin *plugin)
{
	GSList *output;
	GSList *input;

	brasero_plugin_define (plugin,
			       "cdda2wav",
	                       NULL,
			       _("Copy tracks from an audio CD with all associated information"),
			       "Philippe Rouquier",
			       1);

	output = brasero_caps_audio_new (BRASERO_PLUGIN_IO_ACCEPT_FILE /*|BRASERO_PLUGIN_IO_ACCEPT_PIPE*/, /* Keep on the fly on hold until it gets proper testing */
					 BRASERO_AUDIO_FORMAT_RAW|
	                                 BRASERO_METADATA_INFO);

	input = brasero_caps_disc_new (BRASERO_MEDIUM_CDR|
	                               BRASERO_MEDIUM_CDRW|
	                               BRASERO_MEDIUM_CDROM|
	                               BRASERO_MEDIUM_CLOSED|
	                               BRASERO_MEDIUM_APPENDABLE|
	                               BRASERO_MEDIUM_HAS_AUDIO);

	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (output);
	g_slist_free (input);

	brasero_plugin_register_group (plugin, _(CDRTOOLS_DESCRIPTION));
}

G_MODULE_EXPORT void
brasero_plugin_check_config (BraseroPlugin *plugin)
{
	gchar *prog_path;

	/* Just check that the program is in the path and executable. */
	prog_path = g_find_program_in_path ("cdda2wav");
	if (!prog_path) {
		brasero_plugin_add_error (plugin,
		                          BRASERO_PLUGIN_ERROR_MISSING_APP,
		                          "cdda2wav");
		return;
	}

	if (!g_file_test (prog_path, G_FILE_TEST_IS_EXECUTABLE)) {
		g_free (prog_path);
		brasero_plugin_add_error (plugin,
		                          BRASERO_PLUGIN_ERROR_MISSING_APP,
		                          "cdda2wav");
		return;
	}
	g_free (prog_path);

	/* This is what it should be. Now, as I did not write and icedax plugin
	 * the above is enough so that cdda2wav can use a symlink to icedax.
	 * As for the version checking, it becomes impossible given that with
	 * icedax the string would not start with cdda2wav. So ... */
	/*
	gint version [3] = { 2, 0, -1};
	brasero_plugin_test_app (plugin,
	                         "cdda2wav",
	                         "--version",
	                         "cdda2wav %d.%d",
	                         version);
	*/
}
