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

#include "burn-debug.h"
#include "burn-job.h"
#include "burn-process.h"
#include "brasero-plugin-registration.h"
#include "burn-cdrtools.h"
#include "brasero-track-data.h"


#define BRASERO_TYPE_MKISOFS         (brasero_mkisofs_get_type ())
#define BRASERO_MKISOFS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_MKISOFS, BraseroMkisofs))
#define BRASERO_MKISOFS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_MKISOFS, BraseroMkisofsClass))
#define BRASERO_IS_MKISOFS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_MKISOFS))
#define BRASERO_IS_MKISOFS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_MKISOFS))
#define BRASERO_MKISOFS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_MKISOFS, BraseroMkisofsClass))

BRASERO_PLUGIN_BOILERPLATE (BraseroMkisofs, brasero_mkisofs, BRASERO_TYPE_PROCESS, BraseroProcess);

struct _BraseroMkisofsPrivate {
	guint use_utf8:1;
};
typedef struct _BraseroMkisofsPrivate BraseroMkisofsPrivate;

#define BRASERO_MKISOFS_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_MKISOFS, BraseroMkisofsPrivate))
static GObjectClass *parent_class = NULL;

static BraseroBurnResult
brasero_mkisofs_read_isosize (BraseroProcess *process, const gchar *line)
{
	gint64 sectors;

	sectors = strtoll (line, NULL, 10);
	if (!sectors)
		return BRASERO_BURN_OK;

	/* mkisofs reports blocks of 2048 bytes */
	brasero_job_set_output_size_for_current_track (BRASERO_JOB (process),
						       sectors,
						       sectors * 2048ULL);
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_mkisofs_read_stdout (BraseroProcess *process, const gchar *line)
{
	BraseroJobAction action;

	brasero_job_get_action (BRASERO_JOB (process), &action);
	if (action == BRASERO_JOB_ACTION_SIZE)
		return brasero_mkisofs_read_isosize (process, line);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_mkisofs_read_stderr (BraseroProcess *process, const gchar *line)
{
	gchar fraction_str [7] = { 0, };
	BraseroMkisofs *mkisofs;

	mkisofs = BRASERO_MKISOFS (process);
	if (strstr (line, "estimate finish")
	&&  sscanf (line, "%6c%% done, estimate finish", fraction_str) == 1) {
		gdouble fraction;
	
		fraction = g_strtod (fraction_str, NULL) / (gdouble) 100.0;
		brasero_job_set_progress (BRASERO_JOB (mkisofs), fraction);
		brasero_job_start_progress (BRASERO_JOB (process), FALSE);
	}
	else if (strstr (line, "Input/output error. Read error on old image")) {
		brasero_job_error (BRASERO_JOB (process), 
				   g_error_new_literal (BRASERO_BURN_ERROR,
							BRASERO_BURN_ERROR_IMAGE_LAST_SESSION,
							_("Last session import failed")));
	}
	else if (strstr (line, "Unable to sort directory")) {
		brasero_job_error (BRASERO_JOB (process), 
				   g_error_new_literal (BRASERO_BURN_ERROR,
							BRASERO_BURN_ERROR_WRITE_IMAGE,
							_("An image could not be created")));
	}
	else if (strstr (line, "have the same joliet name")
	     ||  strstr (line, "Joliet tree sort failed.")) {
		brasero_job_error (BRASERO_JOB (process), 
				   g_error_new_literal (BRASERO_BURN_ERROR,
							BRASERO_BURN_ERROR_IMAGE_JOLIET,
							_("An image could not be created")));
	}
	else if (strstr (line, "Use mkisofs -help")) {
		brasero_job_error (BRASERO_JOB (process), 
				   g_error_new_literal (BRASERO_BURN_ERROR,
							BRASERO_BURN_ERROR_GENERAL,
							_("This version of mkisofs is not supported")));
	}
/*	else if ((pos =  strstr (line,"mkisofs: Permission denied. "))) {
		int res = FALSE;
		gboolean isdir = FALSE;
		char *path = NULL;

		pos += strlen ("mkisofs: Permission denied. ");
		if (!strncmp (pos, "Unable to open directory ", 24)) {
			isdir = TRUE;

			pos += strlen ("Unable to open directory ");
			path = g_strdup (pos);
			path[strlen (path) - 1] = 0;
		}
		else if (!strncmp (pos, "File ", 5)) {
			char *end;

			isdir = FALSE;
			pos += strlen ("File ");
			end = strstr (pos, " is not readable - ignoring");
			if (end)
				path = g_strndup (pos, end - pos);
		}
		else
			return TRUE;

		res = brasero_mkisofs_base_ask_unreadable_file (BRASERO_GENISOIMAGE_BASE (process),
								path,
								isdir);
		if (!res) {
			g_free (path);

			brasero_job_progress_changed (BRASERO_JOB (process), 1.0, -1);
			brasero_job_cancel (BRASERO_JOB (process), FALSE);
			return FALSE;
		}
	}*/
	else if (strstr (line, "Incorrectly encoded string")) {
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new_literal (BRASERO_BURN_ERROR,
							BRASERO_BURN_ERROR_INPUT_INVALID,
							_("Some files have invalid filenames")));
	}
	else if (strstr (line, "Unknown charset")) {
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new_literal (BRASERO_BURN_ERROR,
							BRASERO_BURN_ERROR_INPUT_INVALID,
							_("Unknown character encoding")));
	}
	else if (strstr (line, "No space left on device")) {
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new_literal (BRASERO_BURN_ERROR,
							BRASERO_BURN_ERROR_DISK_SPACE,
							_("There is no space left on the device")));

	}
	else if (strstr (line, "Unable to open disc image file")) {
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new_literal (BRASERO_BURN_ERROR,
							BRASERO_BURN_ERROR_PERMISSION,
							_("You do not have the required permission to write at this location")));

	}
	else if (strstr (line, "Value too large for defined data type")) {
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new_literal (BRASERO_BURN_ERROR,
							BRASERO_BURN_ERROR_MEDIUM_SPACE,
							_("Not enough space available on the disc")));
	}

	/** REMINDER: these should not be necessary

	else if (strstr (line, "Resource temporarily unavailable")) {
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new_literal (BRASERO_BURN_ERROR,
							BRASERO_BURN_ERROR_INPUT,
							_("Data could not be written")));
	}
	else if (strstr (line, "Bad file descriptor.")) {
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new_literal (BRASERO_BURN_ERROR,
							BRASERO_BURN_ERROR_INPUT,
							_("Internal error: bad file descriptor")));
	}

	**/

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_mkisofs_set_argv_image (BraseroMkisofs *mkisofs,
				GPtrArray *argv,
				GError **error)
{
	gchar *label = NULL;
	BraseroTrack *track;
	BraseroBurnFlag flags;
	gchar *videodir = NULL;
	gchar *emptydir = NULL;
	BraseroJobAction action;
	BraseroImageFS image_fs;
	BraseroBurnResult result;
	gchar *grafts_path = NULL;
	gchar *excluded_path = NULL;

	/* set argv */
	g_ptr_array_add (argv, g_strdup ("-r"));

	result = brasero_job_get_current_track (BRASERO_JOB (mkisofs), &track);
	if (result != BRASERO_BURN_OK)
		BRASERO_JOB_NOT_READY (mkisofs);

	image_fs = brasero_track_data_get_fs (BRASERO_TRACK_DATA (track));
	if (image_fs & BRASERO_IMAGE_FS_JOLIET)
		g_ptr_array_add (argv, g_strdup ("-J"));

	if ((image_fs & BRASERO_IMAGE_FS_ISO)
	&&  (image_fs & BRASERO_IMAGE_ISO_FS_LEVEL_3)) {
		g_ptr_array_add (argv, g_strdup ("-iso-level"));
		g_ptr_array_add (argv, g_strdup ("3"));
	}

	if (image_fs & BRASERO_IMAGE_FS_UDF)
		g_ptr_array_add (argv, g_strdup ("-udf"));

	if (image_fs & BRASERO_IMAGE_FS_VIDEO) {
		g_ptr_array_add (argv, g_strdup ("-dvd-video"));

		result = brasero_job_get_tmp_dir (BRASERO_JOB (mkisofs),
						  &videodir,
						  error);
		if (result != BRASERO_BURN_OK)
			return result;
	}

	g_ptr_array_add (argv, g_strdup ("-graft-points"));

	if (image_fs & BRASERO_IMAGE_ISO_FS_DEEP_DIRECTORY)
		g_ptr_array_add (argv, g_strdup ("-D"));	// This is dangerous the manual says but apparently it works well

	result = brasero_job_get_tmp_file (BRASERO_JOB (mkisofs),
					   NULL,
					   &grafts_path,
					   error);
	if (result != BRASERO_BURN_OK) {
		g_free (videodir);
		return result;
	}

	result = brasero_job_get_tmp_file (BRASERO_JOB (mkisofs),
					   NULL,
					   &excluded_path,
					   error);
	if (result != BRASERO_BURN_OK) {
		g_free (grafts_path);
		g_free (videodir);
		return result;
	}

	result = brasero_job_get_tmp_dir (BRASERO_JOB (mkisofs),
					  &emptydir,
					  error);
	if (result != BRASERO_BURN_OK) {
		g_free (videodir);
		g_free (grafts_path);
		g_free (excluded_path);
		return result;
	}

	result = brasero_track_data_write_to_paths (BRASERO_TRACK_DATA (track),
	                                            grafts_path,
	                                            excluded_path,
	                                            emptydir,
	                                            videodir,
	                                            error);
	g_free (emptydir);

	if (result != BRASERO_BURN_OK) {
		g_free (videodir);
		g_free (grafts_path);
		g_free (excluded_path);
		return result;
	}

	g_ptr_array_add (argv, g_strdup ("-path-list"));
	g_ptr_array_add (argv, grafts_path);

	g_ptr_array_add (argv, g_strdup ("-exclude-list"));
	g_ptr_array_add (argv, excluded_path);

	brasero_job_get_data_label (BRASERO_JOB (mkisofs), &label);
	if (label) {
		g_ptr_array_add (argv, g_strdup ("-V"));
		g_ptr_array_add (argv, label);
	}

	g_ptr_array_add (argv, g_strdup ("-A"));
	g_ptr_array_add (argv, g_strdup_printf ("Brasero-%i.%i.%i",
						BRASERO_MAJOR_VERSION,
						BRASERO_MINOR_VERSION,
						BRASERO_SUB));
	
	g_ptr_array_add (argv, g_strdup ("-sysid"));
#if defined(HAVE_STRUCT_USCSI_CMD)
	g_ptr_array_add (argv, g_strdup ("SOLARIS"));
#else
	g_ptr_array_add (argv, g_strdup ("LINUX"));
#endif
	
	/* FIXME! -sort is an interesting option allowing to decide where the 
	* files are written on the disc and therefore to optimize later reading */
	/* FIXME: -hidden --hidden-list -hide-jolie -hide-joliet-list will allow to hide
	* some files when we will display the contents of a disc we will want to merge */
	/* FIXME: support preparer publisher options */

	brasero_job_get_flags (BRASERO_JOB (mkisofs), &flags);
	if (flags & (BRASERO_BURN_FLAG_APPEND|BRASERO_BURN_FLAG_MERGE)) {
		goffset last_session = 0, next_wr_add = 0;
		gchar *startpoint = NULL;

		brasero_job_get_last_session_address (BRASERO_JOB (mkisofs), &last_session);
		brasero_job_get_next_writable_address (BRASERO_JOB (mkisofs), &next_wr_add);
		if (last_session == -1 || next_wr_add == -1) {
			g_free (videodir);
			BRASERO_JOB_LOG (mkisofs, "Failed to get the start point of the track. Make sure the media allow to add files (it is not closed)"); 
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("An internal error occurred"));
			return BRASERO_BURN_ERR;
		}

		startpoint = g_strdup_printf ("%"G_GINT64_FORMAT",%"G_GINT64_FORMAT,
					      last_session,
					      next_wr_add);

		g_ptr_array_add (argv, g_strdup ("-C"));
		g_ptr_array_add (argv, startpoint);

		if (flags & BRASERO_BURN_FLAG_MERGE) {
		        gchar *device = NULL;

			g_ptr_array_add (argv, g_strdup ("-M"));

			/* NOTE: that function returns either bus_target_lun or the device path
			 * according to OSes. Basically it returns bus/target/lun only for FreeBSD
			 * which is the only OS in need for that. For all others it returns the device
			 * path. */
			brasero_job_get_bus_target_lun (BRASERO_JOB (mkisofs), &device);
			g_ptr_array_add (argv, device);
		}
	}

	brasero_job_get_action (BRASERO_JOB (mkisofs), &action);
	if (action == BRASERO_JOB_ACTION_SIZE) {
		g_ptr_array_add (argv, g_strdup ("-quiet"));
		g_ptr_array_add (argv, g_strdup ("-print-size"));

		brasero_job_set_current_action (BRASERO_JOB (mkisofs),
						BRASERO_BURN_ACTION_GETTING_SIZE,
						NULL,
						FALSE);
		brasero_job_start_progress (BRASERO_JOB (mkisofs), FALSE);

		if (videodir) {
			g_ptr_array_add (argv, g_strdup ("-f"));
			g_ptr_array_add (argv, videodir);
		}

		return BRASERO_BURN_OK;
	}

	if (brasero_job_get_fd_out (BRASERO_JOB (mkisofs), NULL) != BRASERO_BURN_OK) {
		gchar *output = NULL;

		result = brasero_job_get_image_output (BRASERO_JOB (mkisofs),
						      &output,
						       NULL);
		if (result != BRASERO_BURN_OK) {
			g_free (videodir);
			return result;
		}

		g_ptr_array_add (argv, g_strdup ("-o"));
		g_ptr_array_add (argv, output);
	}

	if (videodir) {
		g_ptr_array_add (argv, g_strdup ("-f"));
		g_ptr_array_add (argv, videodir);
	}

	brasero_job_set_current_action (BRASERO_JOB (mkisofs),
					BRASERO_BURN_ACTION_CREATING_IMAGE,
					NULL,
					FALSE);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_mkisofs_set_argv (BraseroProcess *process,
			  GPtrArray *argv,
			  GError **error)
{
	gchar *prog_name;
	BraseroJobAction action;
	BraseroMkisofs *mkisofs;
	BraseroBurnResult result;
	BraseroMkisofsPrivate *priv;

	mkisofs = BRASERO_MKISOFS (process);
	priv = BRASERO_MKISOFS_PRIVATE (process);

	prog_name = g_find_program_in_path ("mkisofs");
	if (prog_name && g_file_test (prog_name, G_FILE_TEST_IS_EXECUTABLE))
		g_ptr_array_add (argv, prog_name);
	else
		g_ptr_array_add (argv, g_strdup ("mkisofs"));

	if (priv->use_utf8) {
		g_ptr_array_add (argv, g_strdup ("-input-charset"));
		g_ptr_array_add (argv, g_strdup ("utf8"));
	}

	brasero_job_get_action (BRASERO_JOB (mkisofs), &action);
	if (action == BRASERO_JOB_ACTION_SIZE)
		result = brasero_mkisofs_set_argv_image (mkisofs, argv, error);
	else if (action == BRASERO_JOB_ACTION_IMAGE)
		result = brasero_mkisofs_set_argv_image (mkisofs, argv, error);
	else
		BRASERO_JOB_NOT_SUPPORTED (mkisofs);

	return result;
}

static void
brasero_mkisofs_class_init (BraseroMkisofsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroProcessClass *process_class = BRASERO_PROCESS_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroMkisofsPrivate));

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_mkisofs_finalize;

	process_class->stdout_func = brasero_mkisofs_read_stdout;
	process_class->stderr_func = brasero_mkisofs_read_stderr;
	process_class->set_argv = brasero_mkisofs_set_argv;
}

static void
brasero_mkisofs_init (BraseroMkisofs *obj)
{
	BraseroMkisofsPrivate *priv;
	gchar *standard_error;
	gboolean res;

	priv = BRASERO_MKISOFS_PRIVATE (obj);

	/* this code used to be ncb_mkisofs_supports_utf8 */
	res = g_spawn_command_line_sync ("mkisofs -input-charset utf8",
					 NULL,
					 &standard_error,
					 NULL,
					 NULL);

	if (res && !g_strrstr (standard_error, "Unknown charset"))
		priv->use_utf8 = TRUE;
	else
		priv->use_utf8 = FALSE;

	g_free (standard_error);
}

static void
brasero_mkisofs_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_mkisofs_export_caps (BraseroPlugin *plugin)
{
	GSList *output;
	GSList *input;

	brasero_plugin_define (plugin,
			       "mkisofs",
	                       NULL,
			       _("Creates disc images from a file selection"),
			       "Philippe Rouquier",
			       2);

	brasero_plugin_set_flags (plugin,
				  BRASERO_MEDIUM_CDR|
				  BRASERO_MEDIUM_CDRW|
				  BRASERO_MEDIUM_DVDR|
				  BRASERO_MEDIUM_DVDRW|
				  BRASERO_MEDIUM_DVDR_PLUS|
				  BRASERO_MEDIUM_DUAL_L|
				  BRASERO_MEDIUM_APPENDABLE|
				  BRASERO_MEDIUM_HAS_AUDIO|
				  BRASERO_MEDIUM_HAS_DATA,
				  BRASERO_BURN_FLAG_APPEND|
				  BRASERO_BURN_FLAG_MERGE,
				  BRASERO_BURN_FLAG_NONE);

	brasero_plugin_set_flags (plugin,
				  BRASERO_MEDIUM_DUAL_L|
				  BRASERO_MEDIUM_DVDRW_PLUS|
				  BRASERO_MEDIUM_RESTRICTED|
				  BRASERO_MEDIUM_APPENDABLE|
				  BRASERO_MEDIUM_CLOSED|
				  BRASERO_MEDIUM_HAS_DATA,
				  BRASERO_BURN_FLAG_APPEND|
				  BRASERO_BURN_FLAG_MERGE,
				  BRASERO_BURN_FLAG_NONE);

	/* Caps */
	output = brasero_caps_image_new (BRASERO_PLUGIN_IO_ACCEPT_FILE|
					 BRASERO_PLUGIN_IO_ACCEPT_PIPE,
					 BRASERO_IMAGE_FORMAT_BIN);

	input = brasero_caps_data_new (BRASERO_IMAGE_FS_ISO|
				       BRASERO_IMAGE_FS_UDF|
				       BRASERO_IMAGE_ISO_FS_LEVEL_3|
				       BRASERO_IMAGE_ISO_FS_DEEP_DIRECTORY|
				       BRASERO_IMAGE_FS_JOLIET|
				       BRASERO_IMAGE_FS_VIDEO);
	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (input);

	input = brasero_caps_data_new (BRASERO_IMAGE_FS_ISO|
				       BRASERO_IMAGE_ISO_FS_LEVEL_3|
				       BRASERO_IMAGE_ISO_FS_DEEP_DIRECTORY|
				       BRASERO_IMAGE_FS_SYMLINK);
	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (input);

	g_slist_free (output);

	brasero_plugin_register_group (plugin, _(CDRTOOLS_DESCRIPTION));
}

G_MODULE_EXPORT void
brasero_plugin_check_config (BraseroPlugin *plugin)
{
	gint version [3] = { 2, 0, -1};
	brasero_plugin_test_app (plugin,
	                         "mkisofs",
	                         "--version",
	                         "mkisofs %d.%d",
	                         version);
}
