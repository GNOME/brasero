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
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gmodule.h>

#include "burn-cdrkit.h"
#include "burn-process.h"
#include "burn-job.h"
#include "brasero-plugin-registration.h"
#include "brasero-tags.h"
#include "brasero-track-disc.h"

#include "burn-volume.h"
#include "brasero-drive.h"


#define BRASERO_TYPE_READOM         (brasero_readom_get_type ())
#define BRASERO_READOM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_READOM, BraseroReadom))
#define BRASERO_READOM_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_READOM, BraseroReadomClass))
#define BRASERO_IS_READOM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_READOM))
#define BRASERO_IS_READOM_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_READOM))
#define BRASERO_READOM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_READOM, BraseroReadomClass))

BRASERO_PLUGIN_BOILERPLATE (BraseroReadom, brasero_readom, BRASERO_TYPE_PROCESS, BraseroProcess);

static GObjectClass *parent_class = NULL;

static BraseroBurnResult
brasero_readom_read_stderr (BraseroProcess *process, const gchar *line)
{
	BraseroReadom *readom;
	gint dummy1;
	gint dummy2;
	gchar *pos;

	readom = BRASERO_READOM (process);

	if ((pos = strstr (line, "addr:"))) {
		gint sector;
		gint64 written;
		BraseroTrackType *output = NULL;

		pos += strlen ("addr:");
		sector = strtoll (pos, NULL, 10);

		output = brasero_track_type_new ();
		brasero_job_get_output_type (BRASERO_JOB (readom), output);

		if (brasero_track_type_get_image_format (output) == BRASERO_IMAGE_FORMAT_BIN)
			written = (gint64) ((gint64) sector * 2048ULL);
		else if (brasero_track_type_get_image_format (output) == BRASERO_IMAGE_FORMAT_CLONE)
			written = (gint64) ((gint64) sector * 2448ULL);
		else
			written = (gint64) ((gint64) sector * 2048ULL);

		brasero_job_set_written_track (BRASERO_JOB (readom), written);

		if (sector > 10)
			brasero_job_start_progress (BRASERO_JOB (readom), FALSE);

		brasero_track_type_free (output);
	}
	else if ((pos = strstr (line, "Capacity:"))) {
		brasero_job_set_current_action (BRASERO_JOB (readom),
						BRASERO_BURN_ACTION_DRIVE_COPY,
						NULL,
						FALSE);
	}
	else if (strstr (line, "Device not ready.")) {
		brasero_job_error (BRASERO_JOB (readom),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_DRIVE_BUSY,
						_("The drive is busy")));
	}
	else if (strstr (line, "Cannot open SCSI driver.")) {
		brasero_job_error (BRASERO_JOB (readom),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_PERMISSION,
						_("You do not have the required permissions to use this drive")));		
	}
	else if (strstr (line, "Cannot send SCSI cmd via ioctl")) {
		brasero_job_error (BRASERO_JOB (readom),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_PERMISSION,
						_("You do not have the required permissions to use this drive")));
	}
	/* we scan for this error as in this case readcd returns success */
	else if (sscanf (line, "Input/output error. Error on sector %d not corrected. Total of %d error", &dummy1, &dummy2) == 2) {
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_GENERAL,
						_("An internal error occurred")));
	}
	else if (strstr (line, "No space left on device")) {
		/* This is necessary as readcd won't return an error code on exit */
		brasero_job_error (BRASERO_JOB (readom),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_DISK_SPACE,
						_("The location you chose to store the image on does not have enough free space for the disc image")));
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_readom_argv_set_iso_boundary (BraseroReadom *readom,
				      GPtrArray *argv,
				      GError **error)
{
	goffset nb_blocks;
	BraseroTrack *track;
	GValue *value = NULL;
	BraseroTrackType *output = NULL;

	brasero_job_get_current_track (BRASERO_JOB (readom), &track);

	output = brasero_track_type_new ();
	brasero_job_get_output_type (BRASERO_JOB (readom), output);

	brasero_track_tag_lookup (track,
				  BRASERO_TRACK_MEDIUM_ADDRESS_START_TAG,
				  &value);
	if (value) {
		guint64 start, end;

		/* we were given an address to start */
		start = g_value_get_uint64 (value);

		/* get the length now */
		value = NULL;
		brasero_track_tag_lookup (track,
					  BRASERO_TRACK_MEDIUM_ADDRESS_END_TAG,
					  &value);

		end = g_value_get_uint64 (value);

		BRASERO_JOB_LOG (readom,
				 "reading from sector %" G_GUINT64_FORMAT " to %" G_GUINT64_FORMAT,
				 start,
				 end);
		g_ptr_array_add (argv, g_strdup_printf ("-sectors=%"G_GINT64_FORMAT"-%"G_GINT64_FORMAT,
							start,
							end));
	}
	/* 0 means all disc, -1 problem */
	else if (brasero_track_disc_get_track_num (BRASERO_TRACK_DISC (track)) > 0) {
		goffset start;
		BraseroDrive *drive;
		BraseroMedium *medium;

		drive = brasero_track_disc_get_drive (BRASERO_TRACK_DISC (track));
		medium = brasero_drive_get_medium (drive);
		brasero_medium_get_track_space (medium,
						brasero_track_disc_get_track_num (BRASERO_TRACK_DISC (track)),
						NULL,
						&nb_blocks);
		brasero_medium_get_track_address (medium,
						  brasero_track_disc_get_track_num (BRASERO_TRACK_DISC (track)),
						  NULL,
						  &start);

		BRASERO_JOB_LOG (readom,
				 "reading %i from sector %" G_GOFFSET_FORMAT " to %" G_GOFFSET_FORMAT,
				 brasero_track_disc_get_track_num (BRASERO_TRACK_DISC (track)),
				 start,
				 start + nb_blocks);
		g_ptr_array_add (argv, g_strdup_printf ("-sectors=%"G_GINT64_FORMAT"-%"G_GINT64_FORMAT,
							start,
							start + nb_blocks));
	}
	/* if it's BIN output just read the last track */
	else if (brasero_track_type_get_image_format (output) == BRASERO_IMAGE_FORMAT_BIN) {
		goffset start;
		BraseroDrive *drive;
		BraseroMedium *medium;

		drive = brasero_track_disc_get_drive (BRASERO_TRACK_DISC (track));
		medium = brasero_drive_get_medium (drive);
		brasero_medium_get_last_data_track_space (medium,
							  NULL,
							  &nb_blocks);
		brasero_medium_get_last_data_track_address (medium,
							    NULL,
							    &start);
		BRASERO_JOB_LOG (readom,
				 "reading last track from sector %"G_GINT64_FORMAT" to %"G_GINT64_FORMAT,
				 start,
				 start + nb_blocks);
		g_ptr_array_add (argv, g_strdup_printf ("-sectors=%"G_GINT64_FORMAT"-%"G_GINT64_FORMAT,
							start,
							start + nb_blocks));
	}
	else {
		brasero_track_get_size (track, &nb_blocks, NULL);
		g_ptr_array_add (argv, g_strdup_printf ("-sectors=0-%"G_GINT64_FORMAT, nb_blocks));
	}

	brasero_track_type_free (output);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_readom_get_size (BraseroReadom *self,
			 GError **error)
{
	goffset blocks;
	GValue *value = NULL;
	BraseroTrack *track = NULL;
	BraseroTrackType *output = NULL;

	output = brasero_track_type_new ();
	brasero_job_get_output_type (BRASERO_JOB (self), output);

	if (!brasero_track_type_get_has_image (output)) {
		brasero_track_type_free (output);
		return BRASERO_BURN_ERR;
	}

	brasero_job_get_current_track (BRASERO_JOB (self), &track);
	brasero_track_tag_lookup (track,
				  BRASERO_TRACK_MEDIUM_ADDRESS_START_TAG,
				  &value);
	if (value) {
		guint64 start, end;

		/* we were given an address to start */
		start = g_value_get_uint64 (value);

		/* get the length now */
		value = NULL;
		brasero_track_tag_lookup (track,
					  BRASERO_TRACK_MEDIUM_ADDRESS_END_TAG,
					  &value);

		end = g_value_get_uint64 (value);
		blocks = end - start;
	}
	else if (brasero_track_disc_get_track_num (BRASERO_TRACK_DISC (track)) > 0) {
		BraseroDrive *drive;
		BraseroMedium *medium;

		drive = brasero_track_disc_get_drive (BRASERO_TRACK_DISC (track));
		medium = brasero_drive_get_medium (drive);
		brasero_medium_get_track_space (medium,
						brasero_track_disc_get_track_num (BRASERO_TRACK_DISC (track)),
						NULL,
						&blocks);
	}
	else if (brasero_track_type_get_image_format (output) == BRASERO_IMAGE_FORMAT_BIN) {
		BraseroDrive *drive;
		BraseroMedium *medium;

		drive = brasero_track_disc_get_drive (BRASERO_TRACK_DISC (track));
		medium = brasero_drive_get_medium (drive);
		brasero_medium_get_last_data_track_space (medium,
							  NULL,
							  &blocks);
	}
	else
		brasero_track_get_size (track, &blocks, NULL);

	if (brasero_track_type_get_image_format (output) == BRASERO_IMAGE_FORMAT_BIN) {
		brasero_job_set_output_size_for_current_track (BRASERO_JOB (self),
							       blocks,
							       blocks * 2048ULL);
	}
	else if (brasero_track_type_get_image_format (output) == BRASERO_IMAGE_FORMAT_CLONE) {
		brasero_job_set_output_size_for_current_track (BRASERO_JOB (self),
							       blocks,
							       blocks * 2448ULL);
	}
	else {
		brasero_track_type_free (output);
		return BRASERO_BURN_NOT_SUPPORTED;
	}

	brasero_track_type_free (output);

	/* no need to go any further */
	return BRASERO_BURN_NOT_RUNNING;
}

static BraseroBurnResult
brasero_readom_set_argv (BraseroProcess *process,
			 GPtrArray *argv,
			 GError **error)
{
	BraseroBurnResult result = FALSE;
	BraseroTrackType *output = NULL;
	BraseroImageFormat format;
	BraseroJobAction action;
	BraseroReadom *readom;
	BraseroMedium *medium;
	BraseroDrive *drive;
	BraseroTrack *track;
	BraseroMedia media;
	gchar *outfile_arg;
	gchar *dev_str;

	readom = BRASERO_READOM (process);

	/* This is a kind of shortcut */
	brasero_job_get_action (BRASERO_JOB (process), &action);
	if (action == BRASERO_JOB_ACTION_SIZE)
		return brasero_readom_get_size (readom, error);

	g_ptr_array_add (argv, g_strdup ("readom"));

	brasero_job_get_current_track (BRASERO_JOB (readom), &track);
	drive = brasero_track_disc_get_drive (BRASERO_TRACK_DISC (track));
	if (!brasero_drive_get_device (drive))
		return BRASERO_BURN_ERR;

	dev_str = g_strdup_printf ("dev=%s", brasero_drive_get_device (drive));
	g_ptr_array_add (argv, dev_str);

	g_ptr_array_add (argv, g_strdup ("-nocorr"));

	medium = brasero_drive_get_medium (drive);
	media = brasero_medium_get_status (medium);

	output = brasero_track_type_new ();
	brasero_job_get_output_type (BRASERO_JOB (readom), output);
	format = brasero_track_type_get_image_format (output);
	brasero_track_type_free (output);

	if ((media & BRASERO_MEDIUM_DVD)
	&&   format != BRASERO_IMAGE_FORMAT_BIN) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("An internal error occurred"));
		return BRASERO_BURN_ERR;
	}

	if (format == BRASERO_IMAGE_FORMAT_CLONE) {
		/* NOTE: with this option the sector size is 2448 
		 * because it is raw96 (2352+96) otherwise it is 2048  */
		g_ptr_array_add (argv, g_strdup ("-clone"));
	}
	else if (format == BRASERO_IMAGE_FORMAT_BIN) {
		g_ptr_array_add (argv, g_strdup ("-noerror"));

		/* don't do it for clone since we need the entire disc */
		result = brasero_readom_argv_set_iso_boundary (readom, argv, error);
		if (result != BRASERO_BURN_OK)
			return result;
	}
	else
		BRASERO_JOB_NOT_SUPPORTED (readom);

	if (brasero_job_get_fd_out (BRASERO_JOB (readom), NULL) != BRASERO_BURN_OK) {
		gchar *image;

		if (format != BRASERO_IMAGE_FORMAT_CLONE
		&&  format != BRASERO_IMAGE_FORMAT_BIN)
			BRASERO_JOB_NOT_SUPPORTED (readom);

		result = brasero_job_get_image_output (BRASERO_JOB (readom),
						       &image,
						       NULL);
		if (result != BRASERO_BURN_OK)
			return result;

		outfile_arg = g_strdup_printf ("-f=%s", image);
		g_ptr_array_add (argv, outfile_arg);
		g_free (image);
	}
	else if (format == BRASERO_IMAGE_FORMAT_BIN) {
		outfile_arg = g_strdup ("-f=-");
		g_ptr_array_add (argv, outfile_arg);
	}
	else 	/* unfortunately raw images can't be piped out */
		BRASERO_JOB_NOT_SUPPORTED (readom);

	brasero_job_set_use_average_rate (BRASERO_JOB (process), TRUE);
	return BRASERO_BURN_OK;
}

static void
brasero_readom_class_init (BraseroReadomClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	BraseroProcessClass *process_class = BRASERO_PROCESS_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_readom_finalize;

	process_class->stderr_func = brasero_readom_read_stderr;
	process_class->set_argv = brasero_readom_set_argv;
}

static void
brasero_readom_init (BraseroReadom *obj)
{ }

static void
brasero_readom_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_readom_export_caps (BraseroPlugin *plugin)
{
	GSList *output;
	GSList *input;

	brasero_plugin_define (plugin,
			       "readom",
	                       NULL,
			       _("Copies any disc to a disc image"),
			       "Philippe Rouquier",
			       1);

	/* that's for clone mode only The only one to copy audio */
	output = brasero_caps_image_new (BRASERO_PLUGIN_IO_ACCEPT_FILE,
					 BRASERO_IMAGE_FORMAT_CLONE);

	input = brasero_caps_disc_new (BRASERO_MEDIUM_CD|
				       BRASERO_MEDIUM_ROM|
				       BRASERO_MEDIUM_WRITABLE|
				       BRASERO_MEDIUM_REWRITABLE|
				       BRASERO_MEDIUM_APPENDABLE|
				       BRASERO_MEDIUM_CLOSED|
				       BRASERO_MEDIUM_HAS_AUDIO|
				       BRASERO_MEDIUM_HAS_DATA);

	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (output);
	g_slist_free (input);

	/* that's for regular mode: it accepts the previous type of discs 
	 * plus the DVDs types as well */
	output = brasero_caps_image_new (BRASERO_PLUGIN_IO_ACCEPT_FILE|
					 BRASERO_PLUGIN_IO_ACCEPT_PIPE,
					 BRASERO_IMAGE_FORMAT_BIN);

	input = brasero_caps_disc_new (BRASERO_MEDIUM_CD|
				       BRASERO_MEDIUM_DVD|
				       BRASERO_MEDIUM_BD|
				       BRASERO_MEDIUM_DUAL_L|
				       BRASERO_MEDIUM_PLUS|
				       BRASERO_MEDIUM_SEQUENTIAL|
				       BRASERO_MEDIUM_RESTRICTED|
				       BRASERO_MEDIUM_ROM|
				       BRASERO_MEDIUM_WRITABLE|
				       BRASERO_MEDIUM_REWRITABLE|
				       BRASERO_MEDIUM_CLOSED|
				       BRASERO_MEDIUM_APPENDABLE|
				       BRASERO_MEDIUM_HAS_DATA);

	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (output);
	g_slist_free (input);

	brasero_plugin_register_group (plugin, _(CDRKIT_DESCRIPTION));
}

G_MODULE_EXPORT void
brasero_plugin_check_config (BraseroPlugin *plugin)
{
	gint version [3] = { 1, 1, 0};
	brasero_plugin_test_app (plugin,
	                         "readom",
	                         "--version",
	                         "readcd %*s is not what you see here. This line is only a fake for too clever\nGUIs and other frontend applications. In fact, this program is:\nreadom %d.%d.%d",
	                         version);
}
