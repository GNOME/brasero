/***************************************************************************
 *            readom.c
 *
 *  dim jan 22 18:06:10 2006
 *  Copyright  2006  Rouquier Philippe
 *  brasero-app@wanadoo.fr
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
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gmodule.h>

#include <nautilus-burn-drive.h>

#include "burn-basics.h"
#include "burn-readom.h"
#include "burn-process.h"
#include "burn-job.h"
#include "burn-plugin.h"
#include "burn-volume.h"
#include "brasero-ncb.h"

BRASERO_PLUGIN_BOILERPLATE (BraseroReadom, brasero_readom, BRASERO_TYPE_PROCESS, BraseroProcess);
static GObjectClass *parent_class = NULL;

static BraseroBurnResult
brasero_readom_read_stderr (BraseroProcess *process, const gchar *line)
{
	BraseroReadom *readom;
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

		brasero_job_set_written (BRASERO_JOB (readom), written);

		if (sector > 10)
			brasero_job_start_progress (BRASERO_JOB (readom), FALSE);
	}
	else if ((pos = strstr (line, "Capacity:"))) {
		gint64 total;
		gint64 sectors;
		BraseroTrackType output;
		BraseroJobAction action;

		pos += strlen ("Capacity:");
		sectors = strtoll (pos, NULL, 10);

		brasero_job_get_output_type (BRASERO_JOB (readom), &output);
		if (output.subtype.img_format == BRASERO_IMAGE_FORMAT_BIN) {
			total = sectors * 2048;
			brasero_job_set_current_track_size (BRASERO_JOB (readom),
							    2048,
							    sectors,
							    -1);
		}
		else if (output.subtype.img_format == BRASERO_IMAGE_FORMAT_CLONE) {
			total = sectors * 2448;
			brasero_job_set_current_track_size (BRASERO_JOB (readom),
							    2448,
							    sectors,
							    -1);
		}
		else {
			total = sectors * 2048;
			brasero_job_set_current_track_size (BRASERO_JOB (readom),
							    2048,
							    sectors,
							    -1);
		}

		brasero_job_get_action (BRASERO_JOB (readom), &action);
		if (action != BRASERO_JOB_ACTION_SIZE)
			brasero_job_set_current_action (BRASERO_JOB (readom),
							BRASERO_BURN_ACTION_DRIVE_COPY,
							NULL,
							FALSE);
		else
			brasero_job_finished (BRASERO_JOB (readom), NULL);
	}
	else if (strstr (line, "Device not ready.")) {
		brasero_job_error (BRASERO_JOB (readom),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_BUSY_DRIVE,
						_("the drive is not ready")));
	}
	else if (strstr (line, "Device or resource busy")) {
		brasero_job_error (BRASERO_JOB (readom),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_BUSY_DRIVE,
						_("you don't seem to have the required permissions to access the drive")));
	}
	else if (strstr (line, "Cannot open SCSI driver.")) {
		brasero_job_error (BRASERO_JOB (readom),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_BUSY_DRIVE,
						_("you don't seem to have the required permissions to access the drive")));		
	}
	else if (strstr (line, "Cannot send SCSI cmd via ioctl")) {
		brasero_job_error (BRASERO_JOB (readom),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_SCSI_IOCTL,
						_("you don't seem to have the required permissions to access the drive")));
	}
	else if (strstr (line, "Time total:")) {
		BraseroTrackType output;
		BraseroTrack *track;
		gchar *image;
		gchar *toc;

		brasero_job_get_output_type (BRASERO_JOB (readom), &output);
		brasero_job_get_image_output (BRASERO_JOB (readom),
					      &image,
					      &toc);

		track = brasero_track_new (output.type);
		brasero_track_set_image_source (track,
						image,
						toc,
						output.subtype.img_format);
		g_free (image);
		g_free (toc);

		brasero_job_finished (BRASERO_JOB (process), track);
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
	BraseroMedia media;
	NautilusBurnDrive *drive;

	brasero_job_get_current_track (BRASERO_JOB (readom), &track);
	drive = brasero_track_get_drive_source (track);
	
	media = NCB_MEDIA_GET_STATUS (drive);
	if (!BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDRW_PLUS)
	&&  !BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDRW_RESTRICTED))
		return BRASERO_BURN_OK;

	NCB_MEDIA_GET_DATA_SIZE (drive, NULL, &nb_blocks);

	g_ptr_array_add (argv, g_strdup_printf ("-sectors=0-%lli", nb_blocks));
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_readom_set_argv (BraseroProcess *process,
			 GPtrArray *argv,
			 GError **error)
{
	BraseroBurnResult result = FALSE;
	NautilusBurnDrive *drive;
	BraseroJobAction action;
	BraseroTrackType output;
	BraseroReadom *readom;
	BraseroTrack *track;
	BraseroMedia media;
	gchar *outfile_arg;
	gchar *dev_str;

	readom = BRASERO_READOM (process);

	g_ptr_array_add (argv, g_strdup ("readom"));

	brasero_job_get_current_track (BRASERO_JOB (readom), &track);
	drive = brasero_track_get_drive_source (track);
	if (!NCB_DRIVE_GET_DEVICE (drive))
		return BRASERO_BURN_ERR;

	dev_str = g_strdup_printf ("dev=%s", NCB_DRIVE_GET_DEVICE (drive));
	g_ptr_array_add (argv, dev_str);

	g_ptr_array_add (argv, g_strdup ("-nocorr"));

	media = NCB_MEDIA_GET_STATUS (drive);
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
	else if (output.subtype.img_format == BRASERO_IMAGE_FORMAT_BIN)
		g_ptr_array_add (argv, g_strdup ("-noerror"));
	else
		BRASERO_JOB_NOT_SUPPORTED (readom);

	brasero_job_get_action (BRASERO_JOB (readom), &action);
	if (action == BRASERO_JOB_ACTION_SIZE) {
		g_ptr_array_add (argv, g_strdup ("-sectors=0-0"));

		brasero_job_set_current_action (BRASERO_JOB(readom),
						BRASERO_BURN_ACTION_GETTING_SIZE,
						NULL,
						FALSE);
		brasero_job_start_progress (BRASERO_JOB (readom), FALSE);
		return BRASERO_BURN_OK;
	}

	if (!brasero_job_get_fd_out (BRASERO_JOB (readom), NULL)) {
		gchar *image;

		if (output.subtype.img_format != BRASERO_IMAGE_FORMAT_CLONE
		&&  output.subtype.img_format != BRASERO_IMAGE_FORMAT_BIN)
			BRASERO_JOB_NOT_SUPPORTED (readom);

		result = brasero_readom_argv_set_iso_boundary (readom, argv, error);
		if (result != BRASERO_BURN_OK)
			return result;

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
		result = brasero_readom_argv_set_iso_boundary (readom, argv, error);
		if (result != BRASERO_BURN_OK)
			return result;

		outfile_arg = g_strdup ("-f=-");
		g_ptr_array_add (argv, outfile_arg);
	}
	else 	/* unfortunately raw images can't be piped out */
		BRASERO_JOB_NOT_SUPPORTED (readom);

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

G_MODULE_EXPORT GType
brasero_plugin_register (BraseroPlugin *plugin, gchar **error)
{
	gchar *prog_name;
	GSList *output;
	GSList *input;

	/* First see if this plugin can be used, i.e. if readom is in
	 * the path */
	prog_name = g_find_program_in_path ("readom");
	if (!prog_name) {
		*error = g_strdup (_("readom could not be found in the path"));
		return G_TYPE_NONE;
	}

	g_free (prog_name);

	brasero_plugin_define (plugin,
			       "readom",
			       _("use readom to image CDs"),
			       "Philippe Rouquier",
			       1);

	/* that's for clone mode only */
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

	/* that's for regular mode: it accepts the previous type of discs 
	 * plus the DVDs types as well */
	output = brasero_caps_image_new (BRASERO_PLUGIN_IO_ACCEPT_FILE|
					 BRASERO_PLUGIN_IO_ACCEPT_PIPE,
					 BRASERO_IMAGE_FORMAT_BIN);

	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (input);

	input = brasero_caps_disc_new (BRASERO_MEDIUM_DVD|
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

	return brasero_readom_get_type (plugin);
}
