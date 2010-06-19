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

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gmodule.h>

#include "brasero-error.h"
#include "brasero-plugin-registration.h"
#include "burn-job.h"
#include "burn-process.h"
#include "brasero-track-disc.h"
#include "brasero-track-image.h"
#include "brasero-drive.h"
#include "brasero-medium.h"

#define CDRDAO_DESCRIPTION		N_("cdrdao burning suite")

#define BRASERO_TYPE_CDRDAO         (brasero_cdrdao_get_type ())
#define BRASERO_CDRDAO(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_CDRDAO, BraseroCdrdao))
#define BRASERO_CDRDAO_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_CDRDAO, BraseroCdrdaoClass))
#define BRASERO_IS_CDRDAO(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_CDRDAO))
#define BRASERO_IS_CDRDAO_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_CDRDAO))
#define BRASERO_CDRDAO_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_CDRDAO, BraseroCdrdaoClass))

BRASERO_PLUGIN_BOILERPLATE (BraseroCdrdao, brasero_cdrdao, BRASERO_TYPE_PROCESS, BraseroProcess);

struct _BraseroCdrdaoPrivate {
 	gchar *tmp_toc_path;
	guint use_raw:1;
};
typedef struct _BraseroCdrdaoPrivate BraseroCdrdaoPrivate;
#define BRASERO_CDRDAO_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_CDRDAO, BraseroCdrdaoPrivate)) 

static GObjectClass *parent_class = NULL;

#define BRASERO_SCHEMA_CONFIG		"org.gnome.brasero.config"
#define BRASERO_KEY_RAW_FLAG		"raw-flag"

static gboolean
brasero_cdrdao_read_stderr_image (BraseroCdrdao *cdrdao, const gchar *line)
{
	int min, sec, sub, s1;

	if (sscanf (line, "%d:%d:%d", &min, &sec, &sub) == 3) {
		guint64 secs = min * 60 + sec;

		brasero_job_set_written_track (BRASERO_JOB (cdrdao), secs * 75 * 2352);
		if (secs > 2)
			brasero_job_start_progress (BRASERO_JOB (cdrdao), FALSE);
	}
	else if (sscanf (line, "Leadout %*s %*d %d:%d:%*d(%i)", &min, &sec, &s1) == 3) {
		BraseroJobAction action;

		brasero_job_get_action (BRASERO_JOB (cdrdao), &action);
		if (action == BRASERO_JOB_ACTION_SIZE) {
			/* get the number of sectors. As we added -raw sector = 2352 bytes */
			brasero_job_set_output_size_for_current_track (BRASERO_JOB (cdrdao), s1, (gint64) s1 * 2352ULL);
			brasero_job_finished_session (BRASERO_JOB (cdrdao));
		}
	}
	else if (strstr (line, "Copying audio tracks")) {
		brasero_job_set_current_action (BRASERO_JOB (cdrdao),
						BRASERO_BURN_ACTION_DRIVE_COPY,
						_("Copying audio track"),
						FALSE);
	}
	else if (strstr (line, "Copying data track")) {
		brasero_job_set_current_action (BRASERO_JOB (cdrdao),
						BRASERO_BURN_ACTION_DRIVE_COPY,
						_("Copying data track"),
						FALSE);
	}
	else
		return FALSE;

	return TRUE;
}

static gboolean
brasero_cdrdao_read_stderr_record (BraseroCdrdao *cdrdao, const gchar *line)
{
	int fifo, track, min, sec;
	guint written, total;

	if (sscanf (line, "Wrote %u of %u (Buffers %d%%  %*s", &written, &total, &fifo) >= 2) {
		brasero_job_set_dangerous (BRASERO_JOB (cdrdao), TRUE);

		brasero_job_set_written_session (BRASERO_JOB (cdrdao), written * 1048576);
		brasero_job_set_current_action (BRASERO_JOB (cdrdao),
						BRASERO_BURN_ACTION_RECORDING,
						NULL,
						FALSE);

		brasero_job_start_progress (BRASERO_JOB (cdrdao), FALSE);
	}
	else if (sscanf (line, "Wrote %*s blocks. Buffer fill min") == 1) {
		/* this is for fixating phase */
		brasero_job_set_current_action (BRASERO_JOB (cdrdao),
						BRASERO_BURN_ACTION_FIXATING,
						NULL,
						FALSE);
	}
	else if (sscanf (line, "Analyzing track %d %*s start %d:%d:%*d, length %*d:%*d:%*d", &track, &min, &sec) == 3) {
		gchar *string;

		string = g_strdup_printf (_("Analysing track %02i"), track);
		brasero_job_set_current_action (BRASERO_JOB (cdrdao),
						BRASERO_BURN_ACTION_ANALYSING,
						string,
						TRUE);
		g_free (string);
	}
	else if (sscanf (line, "%d:%d:%*d", &min, &sec) == 2) {
		gint64 written;
		guint64 secs = min * 60 + sec;

		if (secs > 2)
			brasero_job_start_progress (BRASERO_JOB (cdrdao), FALSE);

		written = secs * 75 * 2352;
		brasero_job_set_written_session (BRASERO_JOB (cdrdao), written);
	}
	else if (strstr (line, "Writing track")) {
		brasero_job_set_dangerous (BRASERO_JOB (cdrdao), TRUE);
	}
	else if (strstr (line, "Writing finished successfully")
	     ||  strstr (line, "On-the-fly CD copying finished successfully")) {
		brasero_job_set_dangerous (BRASERO_JOB (cdrdao), FALSE);
	}
	else if (strstr (line, "Blanking disk...")) {
		brasero_job_set_current_action (BRASERO_JOB (cdrdao),
						BRASERO_BURN_ACTION_BLANKING,
						NULL,
						FALSE);
		brasero_job_start_progress (BRASERO_JOB (cdrdao), FALSE);
		brasero_job_set_dangerous (BRASERO_JOB (cdrdao), TRUE);
	}
	else {
		gchar *name = NULL;
		gchar *cuepath = NULL;
		BraseroTrack *track = NULL;
		BraseroJobAction action;

		/* Try to catch error could not find cue file */

		/* Track could be NULL here if we're simply blanking a medium */
		brasero_job_get_action (BRASERO_JOB (cdrdao), &action);
		if (action == BRASERO_JOB_ACTION_ERASE)
			return TRUE;

		brasero_job_get_current_track (BRASERO_JOB (cdrdao), &track);
		if (!track)
			return FALSE;

		cuepath = brasero_track_image_get_toc_source (BRASERO_TRACK_IMAGE (track), FALSE);

		if (!cuepath)
			return FALSE;

		if (!strstr (line, "ERROR: Could not find input file")) {
			g_free (cuepath);
			return FALSE;
		}

		name = g_path_get_basename (cuepath);
		g_free (cuepath);

		brasero_job_error (BRASERO_JOB (cdrdao),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_FILE_NOT_FOUND,
						/* Translators: %s is a filename */
						_("\"%s\" could not be found"),
						name));
		g_free (name);
	}

	return FALSE;
}

static BraseroBurnResult
brasero_cdrdao_read_stderr (BraseroProcess *process, const gchar *line)
{
	BraseroCdrdao *cdrdao;
	gboolean result = FALSE;
	BraseroJobAction action;

	cdrdao = BRASERO_CDRDAO (process);

	brasero_job_get_action (BRASERO_JOB (cdrdao), &action);
	if (action == BRASERO_JOB_ACTION_RECORD
	||  action == BRASERO_JOB_ACTION_ERASE)
		result = brasero_cdrdao_read_stderr_record (cdrdao, line);
	else if (action == BRASERO_JOB_ACTION_IMAGE
	     ||  action == BRASERO_JOB_ACTION_SIZE)
		result = brasero_cdrdao_read_stderr_image (cdrdao, line);

	if (result)
		return BRASERO_BURN_OK;

	if (strstr (line, "Cannot setup device")) {
		brasero_job_error (BRASERO_JOB (cdrdao),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_DRIVE_BUSY,
						_("The drive is busy")));
	}
	else if (strstr (line, "Operation not permitted. Cannot send SCSI")) {
		brasero_job_error (BRASERO_JOB (cdrdao),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_PERMISSION,
						_("You do not have the required permissions to use this drive")));
	}

	return BRASERO_BURN_OK;
}

static void
brasero_cdrdao_set_argv_device (BraseroCdrdao *cdrdao,
				GPtrArray *argv)
{
	gchar *device = NULL;

	g_ptr_array_add (argv, g_strdup ("--device"));

	/* NOTE: that function returns either bus_target_lun or the device path
	 * according to OSes. Basically it returns bus/target/lun only for FreeBSD
	 * which is the only OS in need for that. For all others it returns the device
	 * path. */
	brasero_job_get_bus_target_lun (BRASERO_JOB (cdrdao), &device);
	g_ptr_array_add (argv, device);
}

static void
brasero_cdrdao_set_argv_common_rec (BraseroCdrdao *cdrdao,
				    GPtrArray *argv)
{
	BraseroBurnFlag flags;
	gchar *speed_str;
	guint speed;

	brasero_job_get_flags (BRASERO_JOB (cdrdao), &flags);
	if (flags & BRASERO_BURN_FLAG_DUMMY)
		g_ptr_array_add (argv, g_strdup ("--simulate"));

	g_ptr_array_add (argv, g_strdup ("--speed"));

	brasero_job_get_speed (BRASERO_JOB (cdrdao), &speed);
	speed_str = g_strdup_printf ("%d", speed);
	g_ptr_array_add (argv, speed_str);

	if (flags & BRASERO_BURN_FLAG_OVERBURN)
		g_ptr_array_add (argv, g_strdup ("--overburn"));
	if (flags & BRASERO_BURN_FLAG_MULTI)
		g_ptr_array_add (argv, g_strdup ("--multi"));
}

static void
brasero_cdrdao_set_argv_common (BraseroCdrdao *cdrdao,
				GPtrArray *argv)
{
	BraseroBurnFlag flags;

	brasero_job_get_flags (BRASERO_JOB (cdrdao), &flags);

	/* cdrdao manual says it is a similar option to gracetime */
	if (flags & BRASERO_BURN_FLAG_NOGRACE)
		g_ptr_array_add (argv, g_strdup ("-n"));

	g_ptr_array_add (argv, g_strdup ("-v"));
	g_ptr_array_add (argv, g_strdup ("2"));
}

static BraseroBurnResult
brasero_cdrdao_set_argv_record (BraseroCdrdao *cdrdao,
				GPtrArray *argv)
{
	BraseroTrackType *type = NULL;
	BraseroCdrdaoPrivate *priv;

	priv = BRASERO_CDRDAO_PRIVATE (cdrdao); 

	g_ptr_array_add (argv, g_strdup ("cdrdao"));

	type = brasero_track_type_new ();
	brasero_job_get_input_type (BRASERO_JOB (cdrdao), type);

        if (brasero_track_type_get_has_medium (type)) {
		BraseroDrive *drive;
		BraseroTrack *track;
		BraseroBurnFlag flags;

		g_ptr_array_add (argv, g_strdup ("copy"));
		brasero_cdrdao_set_argv_device (cdrdao, argv);
		brasero_cdrdao_set_argv_common (cdrdao, argv);
		brasero_cdrdao_set_argv_common_rec (cdrdao, argv);

		brasero_job_get_flags (BRASERO_JOB (cdrdao), &flags);
		if (flags & BRASERO_BURN_FLAG_NO_TMP_FILES)
			g_ptr_array_add (argv, g_strdup ("--on-the-fly"));

		if (priv->use_raw)
		  	g_ptr_array_add (argv, g_strdup ("--driver generic-mmc-raw")); 

		g_ptr_array_add (argv, g_strdup ("--source-device"));

		brasero_job_get_current_track (BRASERO_JOB (cdrdao), &track);
		drive = brasero_track_disc_get_drive (BRASERO_TRACK_DISC (track));

		/* NOTE: that function returns either bus_target_lun or the device path
		 * according to OSes. Basically it returns bus/target/lun only for FreeBSD
		 * which is the only OS in need for that. For all others it returns the device
		 * path. */
		g_ptr_array_add (argv, brasero_drive_get_bus_target_lun_string (drive));
	}
	else if (brasero_track_type_get_has_image (type)) {
		gchar *cuepath;
		BraseroTrack *track;

		g_ptr_array_add (argv, g_strdup ("write"));
		
		brasero_job_get_current_track (BRASERO_JOB (cdrdao), &track);

		if (brasero_track_type_get_image_format (type) == BRASERO_IMAGE_FORMAT_CUE) {
			gchar *parent;

			cuepath = brasero_track_image_get_toc_source (BRASERO_TRACK_IMAGE (track), FALSE);
			parent = g_path_get_dirname (cuepath);
			brasero_process_set_working_directory (BRASERO_PROCESS (cdrdao), parent);
			g_free (parent);

			/* This does not work as toc2cue will use BINARY even if
			 * if endianness is big endian */
			/* we need to check endianness */
			/* if (brasero_track_image_need_byte_swap (BRASERO_TRACK_IMAGE (track)))
				g_ptr_array_add (argv, g_strdup ("--swap")); */
		}
		else if (brasero_track_type_get_image_format (type) == BRASERO_IMAGE_FORMAT_CDRDAO) {
			/* CDRDAO files are always BIG ENDIAN */
			cuepath = brasero_track_image_get_toc_source (BRASERO_TRACK_IMAGE (track), FALSE);
		}
		else {
			brasero_track_type_free (type);
			BRASERO_JOB_NOT_SUPPORTED (cdrdao);
		}

		if (!cuepath) {
			brasero_track_type_free (type);
			BRASERO_JOB_NOT_READY (cdrdao);
		}

		brasero_cdrdao_set_argv_device (cdrdao, argv);
		brasero_cdrdao_set_argv_common (cdrdao, argv);
		brasero_cdrdao_set_argv_common_rec (cdrdao, argv);

		g_ptr_array_add (argv, cuepath);
	}
	else {
		brasero_track_type_free (type);
		BRASERO_JOB_NOT_SUPPORTED (cdrdao);
	}

	brasero_track_type_free (type);
	brasero_job_set_use_average_rate (BRASERO_JOB (cdrdao), TRUE);
	brasero_job_set_current_action (BRASERO_JOB (cdrdao),
					BRASERO_BURN_ACTION_START_RECORDING,
					NULL,
					FALSE);
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_cdrdao_set_argv_blank (BraseroCdrdao *cdrdao,
			       GPtrArray *argv)
{
	BraseroBurnFlag flags;

	g_ptr_array_add (argv, g_strdup ("cdrdao"));
	g_ptr_array_add (argv, g_strdup ("blank"));

	brasero_cdrdao_set_argv_device (cdrdao, argv);
	brasero_cdrdao_set_argv_common (cdrdao, argv);

	g_ptr_array_add (argv, g_strdup ("--blank-mode"));
	brasero_job_get_flags (BRASERO_JOB (cdrdao), &flags);
	if (!(flags & BRASERO_BURN_FLAG_FAST_BLANK))
		g_ptr_array_add (argv, g_strdup ("full"));
	else
		g_ptr_array_add (argv, g_strdup ("minimal"));

	brasero_job_set_current_action (BRASERO_JOB (cdrdao),
					BRASERO_BURN_ACTION_BLANKING,
					NULL,
					FALSE);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_cdrdao_post (BraseroJob *job)
{
	BraseroCdrdaoPrivate *priv;

	priv = BRASERO_CDRDAO_PRIVATE (job);
	if (!priv->tmp_toc_path) {
		brasero_job_finished_session (job);
		return BRASERO_BURN_OK;
	}

	/* we have to run toc2cue now to convert the toc file into a cue file */
	return BRASERO_BURN_RETRY;
}

static BraseroBurnResult
brasero_cdrdao_start_toc2cue (BraseroCdrdao *cdrdao,
			      GPtrArray *argv,
			      GError **error)
{
	gchar *cue_output;
	BraseroBurnResult result;
	BraseroCdrdaoPrivate *priv;

	priv = BRASERO_CDRDAO_PRIVATE (cdrdao);

	g_ptr_array_add (argv, g_strdup ("toc2cue"));

	g_ptr_array_add (argv, priv->tmp_toc_path);
	priv->tmp_toc_path = NULL;

	result = brasero_job_get_image_output (BRASERO_JOB (cdrdao),
					       NULL,
					       &cue_output);
	if (result != BRASERO_BURN_OK)
		return result;

	g_ptr_array_add (argv, cue_output);

	/* if there is a file toc2cue will fail */
	g_remove (cue_output);

	brasero_job_set_current_action (BRASERO_JOB (cdrdao),
					BRASERO_BURN_ACTION_CREATING_IMAGE,
					_("Converting toc file"),
					TRUE);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_cdrdao_set_argv_image (BraseroCdrdao *cdrdao,
			       GPtrArray *argv,
			       GError **error)
{
	gchar *image = NULL, *toc = NULL;
	BraseroTrackType *output = NULL;
	BraseroCdrdaoPrivate *priv;
	BraseroBurnResult result;
	BraseroJobAction action;
	BraseroDrive *drive;
	BraseroTrack *track;

	priv = BRASERO_CDRDAO_PRIVATE (cdrdao);
	if (priv->tmp_toc_path)
		return brasero_cdrdao_start_toc2cue (cdrdao, argv, error);

	g_ptr_array_add (argv, g_strdup ("cdrdao"));
	g_ptr_array_add (argv, g_strdup ("read-cd"));
	g_ptr_array_add (argv, g_strdup ("--device"));

	brasero_job_get_current_track (BRASERO_JOB (cdrdao), &track);
	drive = brasero_track_disc_get_drive (BRASERO_TRACK_DISC (track));

	/* NOTE: that function returns either bus_target_lun or the device path
	 * according to OSes. Basically it returns bus/target/lun only for FreeBSD
	 * which is the only OS in need for that. For all others it returns the device
	 * path. */
	g_ptr_array_add (argv, brasero_drive_get_bus_target_lun_string (drive));
	g_ptr_array_add (argv, g_strdup ("--read-raw"));

	/* This is done so that if a cue file is required we first generate
	 * a temporary toc file that will be later converted to a cue file.
	 * The datafile is written where it should be from the start. */
	output = brasero_track_type_new ();
	brasero_job_get_output_type (BRASERO_JOB (cdrdao), output);

	if (brasero_track_type_get_image_format (output) == BRASERO_IMAGE_FORMAT_CDRDAO) {
		result = brasero_job_get_image_output (BRASERO_JOB (cdrdao),
						       &image,
						       &toc);
		if (result != BRASERO_BURN_OK) {
			brasero_track_type_free (output);
			return result;
		}
	}
	else if (brasero_track_type_get_image_format (output) == BRASERO_IMAGE_FORMAT_CUE) {
		/* NOTE: we don't generate the .cue file right away; we'll call
		 * toc2cue right after we finish */
		result = brasero_job_get_image_output (BRASERO_JOB (cdrdao),
						       &image,
						       NULL);
		if (result != BRASERO_BURN_OK) {
			brasero_track_type_free (output);
			return result;
		}

		result = brasero_job_get_tmp_file (BRASERO_JOB (cdrdao),
						   NULL,
						   &toc,
						   error);
		if (result != BRASERO_BURN_OK) {
			g_free (image);
			brasero_track_type_free (output);
			return result;
		}

		/* save the temporary toc path to resuse it later. */
		priv->tmp_toc_path = g_strdup (toc);
	}

	brasero_track_type_free (output);

	/* it's safe to remove them: session/task make sure they don't exist 
	 * when there is the proper flag whether it be tmp or real output. */ 
	if (toc)
		g_remove (toc);
	if (image)
		g_remove (image);

	brasero_job_get_action (BRASERO_JOB (cdrdao), &action);
	if (action == BRASERO_JOB_ACTION_SIZE) {
		brasero_job_set_current_action (BRASERO_JOB (cdrdao),
						BRASERO_BURN_ACTION_GETTING_SIZE,
						NULL,
						FALSE);
		brasero_job_start_progress (BRASERO_JOB (cdrdao), FALSE);
	}

	g_ptr_array_add (argv, g_strdup ("--datafile"));
	g_ptr_array_add (argv, image);

	g_ptr_array_add (argv, g_strdup ("-v"));
	g_ptr_array_add (argv, g_strdup ("2"));

	g_ptr_array_add (argv, toc);

	brasero_job_set_use_average_rate (BRASERO_JOB (cdrdao), TRUE);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_cdrdao_set_argv (BraseroProcess *process,
			 GPtrArray *argv,
			 GError **error)
{
	BraseroCdrdao *cdrdao;
	BraseroJobAction action;

	cdrdao = BRASERO_CDRDAO (process);

	/* sets the first argv */
	brasero_job_get_action (BRASERO_JOB (cdrdao), &action);
	if (action == BRASERO_JOB_ACTION_RECORD)
		return brasero_cdrdao_set_argv_record (cdrdao, argv);
	else if (action == BRASERO_JOB_ACTION_ERASE)
		return brasero_cdrdao_set_argv_blank (cdrdao, argv);
	else if (action == BRASERO_JOB_ACTION_IMAGE)
		return brasero_cdrdao_set_argv_image (cdrdao, argv, error);
	else if (action == BRASERO_JOB_ACTION_SIZE) {
		BraseroTrack *track;

		brasero_job_get_current_track (BRASERO_JOB (cdrdao), &track);
		if (BRASERO_IS_TRACK_DISC (track)) {
			goffset sectors = 0;

			brasero_track_get_size (track, &sectors, NULL);

			/* cdrdao won't get a track size under 300 sectors */
			if (sectors < 300)
				sectors = 300;

			brasero_job_set_output_size_for_current_track (BRASERO_JOB (cdrdao),
								       sectors,
								       sectors * 2352ULL);
		}
		else
			return BRASERO_BURN_NOT_SUPPORTED;

		return BRASERO_BURN_NOT_RUNNING;
	}

	BRASERO_JOB_NOT_SUPPORTED (cdrdao);
}

static void
brasero_cdrdao_class_init (BraseroCdrdaoClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroProcessClass *process_class = BRASERO_PROCESS_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroCdrdaoPrivate));

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_cdrdao_finalize;

	process_class->stderr_func = brasero_cdrdao_read_stderr;
	process_class->set_argv = brasero_cdrdao_set_argv;
	process_class->post = brasero_cdrdao_post;
}

static void
brasero_cdrdao_init (BraseroCdrdao *obj)
{  
	GSettings *settings;
 	BraseroCdrdaoPrivate *priv;
 	
	/* load our "configuration" */
 	priv = BRASERO_CDRDAO_PRIVATE (obj);

	settings = g_settings_new (BRASERO_SCHEMA_CONFIG);
	priv->use_raw = g_settings_get_boolean (settings, BRASERO_KEY_RAW_FLAG);
	g_object_unref (settings);
}

static void
brasero_cdrdao_finalize (GObject *object)
{
	BraseroCdrdaoPrivate *priv;

	priv = BRASERO_CDRDAO_PRIVATE (object);
	if (priv->tmp_toc_path) {
		g_free (priv->tmp_toc_path);
		priv->tmp_toc_path = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_cdrdao_export_caps (BraseroPlugin *plugin)
{
	GSList *input;
	GSList *output;
	BraseroPluginConfOption *use_raw; 
	const BraseroMedia media_w = BRASERO_MEDIUM_CD|
				     BRASERO_MEDIUM_WRITABLE|
				     BRASERO_MEDIUM_REWRITABLE|
				     BRASERO_MEDIUM_BLANK;
	const BraseroMedia media_rw = BRASERO_MEDIUM_CD|
				      BRASERO_MEDIUM_REWRITABLE|
				      BRASERO_MEDIUM_APPENDABLE|
				      BRASERO_MEDIUM_CLOSED|
				      BRASERO_MEDIUM_HAS_DATA|
				      BRASERO_MEDIUM_HAS_AUDIO|
				      BRASERO_MEDIUM_BLANK;

	brasero_plugin_define (plugin,
			       "cdrdao",
	                       NULL,
			       _("Copies, burns and blanks CDs"),
			       "Philippe Rouquier",
			       0);

	/* that's for cdrdao images: CDs only as input */
	input = brasero_caps_disc_new (BRASERO_MEDIUM_CD|
				       BRASERO_MEDIUM_ROM|
				       BRASERO_MEDIUM_WRITABLE|
				       BRASERO_MEDIUM_REWRITABLE|
				       BRASERO_MEDIUM_APPENDABLE|
				       BRASERO_MEDIUM_CLOSED|
				       BRASERO_MEDIUM_HAS_AUDIO|
				       BRASERO_MEDIUM_HAS_DATA);

	/* an image can be created ... */
	output = brasero_caps_image_new (BRASERO_PLUGIN_IO_ACCEPT_FILE,
					 BRASERO_IMAGE_FORMAT_CDRDAO);
	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (output);

	output = brasero_caps_image_new (BRASERO_PLUGIN_IO_ACCEPT_FILE,
					 BRASERO_IMAGE_FORMAT_CUE);
	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (output);

	/* ... or a disc */
	output = brasero_caps_disc_new (media_w);
	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (input);

	/* cdrdao can also record these types of images to a disc */
	input = brasero_caps_image_new (BRASERO_PLUGIN_IO_ACCEPT_FILE,
					BRASERO_IMAGE_FORMAT_CDRDAO|
					BRASERO_IMAGE_FORMAT_CUE);
	
	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (output);
	g_slist_free (input);

	/* cdrdao is used to burn images so it can't APPEND and the disc must
	 * have been blanked before (it can't overwrite)
	 * NOTE: BRASERO_MEDIUM_FILE is needed here because of restriction API
	 * when we output an image. */
	brasero_plugin_set_flags (plugin,
				  media_w|
				  BRASERO_MEDIUM_FILE,
				  BRASERO_BURN_FLAG_DAO|
				  BRASERO_BURN_FLAG_BURNPROOF|
				  BRASERO_BURN_FLAG_OVERBURN|
				  BRASERO_BURN_FLAG_DUMMY|
				  BRASERO_BURN_FLAG_NOGRACE,
				  BRASERO_BURN_FLAG_NONE);

	/* cdrdao can also blank */
	output = brasero_caps_disc_new (media_rw);
	brasero_plugin_blank_caps (plugin, output);
	g_slist_free (output);

	brasero_plugin_set_blank_flags (plugin,
					media_rw,
					BRASERO_BURN_FLAG_NOGRACE|
					BRASERO_BURN_FLAG_FAST_BLANK,
					BRASERO_BURN_FLAG_NONE);

	use_raw = brasero_plugin_conf_option_new (BRASERO_KEY_RAW_FLAG,
						  _("Enable the \"--driver generic-mmc-raw\" flag (see cdrdao manual)"),
						  BRASERO_PLUGIN_OPTION_BOOL);

	brasero_plugin_add_conf_option (plugin, use_raw);

	brasero_plugin_register_group (plugin, _(CDRDAO_DESCRIPTION));
}

G_MODULE_EXPORT void
brasero_plugin_check_config (BraseroPlugin *plugin)
{
	gint version [3] = { 1, 2, 0};
	brasero_plugin_test_app (plugin,
	                         "cdrdao",
	                         "version",
	                         "Cdrdao version %d.%d.%d - (C) Andreas Mueller <andreas@daneb.de>",
	                         version);

	brasero_plugin_test_app (plugin,
	                         "toc2cue",
	                         "-V",
	                         "%d.%d.%d",
	                         version);
}
