/***************************************************************************
 *            readom.c
 *
 *  dim jan 22 18:06:10 2006
 *  Copyright  2006  Rouquier Philippe
 *  brasero-app@wanadoo.fr
 ***************************************************************************/

/*
 *  Brasero is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Brasero is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to:
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

#include "burn-basics.h"
#include "burn-cdrkit.h"
#include "burn-readom.h"
#include "burn-process.h"
#include "burn-job.h"
#include "burn-plugin.h"
#include "burn-volume.h"
#include "burn-drive.h"

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
		BraseroTrackType output;

		pos += strlen ("addr:");
		sector = strtoll (pos, NULL, 10);

		brasero_job_get_output_type (BRASERO_JOB (readom), &output);
		if (output.subtype.img_format == BRASERO_IMAGE_FORMAT_BIN)
			written = sector * 2048;
		else if (output.subtype.img_format == BRASERO_IMAGE_FORMAT_CLONE)
			written = sector * 2448;
		else
			written = sector * 2048;

		brasero_job_set_written_track (BRASERO_JOB (readom), written);

		if (sector > 10)
			brasero_job_start_progress (BRASERO_JOB (readom), FALSE);
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
						BRASERO_BURN_ERROR_BUSY_DRIVE,
						_("The drive is busy")));
	}
	else if (strstr (line, "Cannot open SCSI driver.")) {
		brasero_job_error (BRASERO_JOB (readom),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_BUSY_DRIVE,
						_("You do not seem to have the required permissions to access the drive")));		
	}
	else if (strstr (line, "Cannot send SCSI cmd via ioctl")) {
		brasero_job_error (BRASERO_JOB (readom),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_SCSI_IOCTL,
						_("You do not seem to have the required permissions to access the drive")));
	}
	/* we scan for this error as in this case readcd returns success */
	else if (sscanf (line, "Input/output error. Error on sector %d not corrected. Total of %d error", &dummy1, &dummy2) == 2) {
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_GENERAL,
						_("Internal error")));
	}


	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_readom_argv_set_iso_boundary (BraseroReadom *readom,
				      GPtrArray *argv,
				      GError **error)
{
	gint64 nb_blocks;
	BraseroTrack *track;
	GValue *value = NULL;
	BraseroTrackType output;

	brasero_job_get_current_track (BRASERO_JOB (readom), &track);
	brasero_job_get_output_type (BRASERO_JOB (readom), &output);

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
				 "reading from sector %lli to %lli",
				 start,
				 end);
		g_ptr_array_add (argv, g_strdup_printf ("-sectors=%lli-%lli",
							start,
							end));
	}
	/* 0 means all disc, -1 problem */
	else if (brasero_track_get_drive_track (track) > 0) {
		gint64 start;
		BraseroMedium *medium;

		medium = brasero_track_get_medium_source (track);
		brasero_medium_get_track_space (medium,
						brasero_track_get_drive_track (track),
						NULL,
						&nb_blocks);
		brasero_medium_get_track_address (medium,
						  brasero_track_get_drive_track (track),
						  NULL,
						  &start);

		BRASERO_JOB_LOG (readom,
				 "reading %i from sector %lli to %lli",
				 brasero_track_get_drive_track (track),
				 start,
				 start + nb_blocks);
		g_ptr_array_add (argv, g_strdup_printf ("-sectors=%lli-%lli",
							start,
							start + nb_blocks));
	}
	/* if it's BIN output just read the last track */
	else if (output.subtype.img_format == BRASERO_IMAGE_FORMAT_BIN) {
		gint64 start;
		BraseroMedium *medium;

		medium = brasero_track_get_medium_source (track);
		brasero_medium_get_last_data_track_space (medium,
							  NULL,
							  &nb_blocks);
		brasero_medium_get_last_data_track_address (medium,
							    NULL,
							    &start);
		BRASERO_JOB_LOG (readom,
				 "reading last track from sector %lli to %lli",
				 start,
				 start + nb_blocks);
		g_ptr_array_add (argv, g_strdup_printf ("-sectors=%lli-%lli",
							start,
							start + nb_blocks));
	}
	else {
		brasero_track_get_disc_data_size (track, &nb_blocks, NULL);
		g_ptr_array_add (argv, g_strdup_printf ("-sectors=0-%lli", nb_blocks));
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_readom_get_size (BraseroReadom *self,
			 GError **error)
{
	gint64 blocks;
	GValue *value = NULL;
	BraseroTrackType output;
	BraseroTrack *track = NULL;

	brasero_job_get_current_track (BRASERO_JOB (self), &track);
	brasero_job_get_output_type (BRASERO_JOB (self), &output);

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
	else if (brasero_track_get_drive_track (track) > 0) {
		BraseroMedium *medium;

		medium = brasero_track_get_medium_source (track);
		brasero_medium_get_track_space (medium,
						brasero_track_get_drive_track (track),
						NULL,
						&blocks);
	}
	else if (output.subtype.img_format == BRASERO_IMAGE_FORMAT_BIN) {
		BraseroMedium *medium;

		medium = brasero_track_get_medium_source (track);
		brasero_medium_get_last_data_track_space (medium,
							  NULL,
							  &blocks);
	}
	else
		brasero_track_get_disc_data_size (track, &blocks, NULL);

	if (output.type != BRASERO_TRACK_TYPE_IMAGE)
		return BRASERO_BURN_ERR;
	
	if (output.subtype.img_format == BRASERO_IMAGE_FORMAT_BIN) {
		brasero_job_set_output_size_for_current_track (BRASERO_JOB (self),
							       blocks,
							       blocks * 2048ULL);
	}
	else if (output.subtype.img_format == BRASERO_IMAGE_FORMAT_CLONE) {
		brasero_job_set_output_size_for_current_track (BRASERO_JOB (self),
							       blocks,
							       blocks * 2448ULL);
	}
	else
		return BRASERO_BURN_NOT_SUPPORTED;

	/* no need to go any further */
	return BRASERO_BURN_NOT_RUNNING;
}

static BraseroBurnResult
brasero_readom_set_argv (BraseroProcess *process,
			 GPtrArray *argv,
			 GError **error)
{
	BraseroBurnResult result = FALSE;
	BraseroJobAction action;
	BraseroTrackType output;
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
	drive = brasero_track_get_drive_source (track);
	if (!brasero_drive_get_device (drive))
		return BRASERO_BURN_ERR;

	dev_str = g_strdup_printf ("dev=%s", brasero_drive_get_device (drive));
	g_ptr_array_add (argv, dev_str);

	g_ptr_array_add (argv, g_strdup ("-nocorr"));

	medium = brasero_drive_get_medium (drive);
	media = brasero_medium_get_status (medium);
	brasero_job_get_output_type (BRASERO_JOB (readom), &output);

	if ((media & BRASERO_MEDIUM_DVD)
	&&  output.subtype.img_format != BRASERO_IMAGE_FORMAT_BIN) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("raw images cannot be created with DVDs"));
		return BRASERO_BURN_ERR;
	}

	if (output.subtype.img_format == BRASERO_IMAGE_FORMAT_CLONE) {
		/* NOTE: with this option the sector size is 2448 
		 * because it is raw96 (2352+96) otherwise it is 2048  */
		g_ptr_array_add (argv, g_strdup ("-clone"));
	}
	else if (output.subtype.img_format == BRASERO_IMAGE_FORMAT_BIN) {
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

		if (output.subtype.img_format != BRASERO_IMAGE_FORMAT_CLONE
		&&  output.subtype.img_format != BRASERO_IMAGE_FORMAT_BIN)
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
	else if (output.subtype.img_format == BRASERO_IMAGE_FORMAT_BIN) {
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

static BraseroBurnResult
brasero_readom_export_caps (BraseroPlugin *plugin, gchar **error)
{
	gchar *prog_name;
	GSList *output;
	GSList *input;

	brasero_plugin_define (plugin,
			       "readom",
			       _("use readom to image CDs"),
			       "Philippe Rouquier",
			       1);

	/* First see if this plugin can be used, i.e. if readom is in
	 * the path */
	prog_name = g_find_program_in_path ("readom");
	if (!prog_name) {
		*error = g_strdup (_("readom could not be found in the path"));
		return BRASERO_BURN_ERR;
	}
	g_free (prog_name);

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

	return BRASERO_BURN_OK;
}
