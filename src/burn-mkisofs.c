/***************************************************************************
 *            mkisofs.c
 *
 *  dim jan 22 15:20:57 2006
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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <nautilus-burn-drive.h>

#include "burn-basics.h"
#include "burn-caps.h"
#include "burn-common.h"
#include "burn-process.h"
#include "burn-mkisofs.h"
#include "burn-imager.h"
#include "brasero-ncb.h"

static void brasero_mkisofs_class_init (BraseroMkisofsClass *klass);
static void brasero_mkisofs_init (BraseroMkisofs *sp);
static void brasero_mkisofs_finalize (GObject *object);
static void brasero_mkisofs_iface_init_image (BraseroImagerIFace *iface);

static BraseroBurnResult
brasero_mkisofs_read_stdout (BraseroProcess *process, const char *line);
static BraseroBurnResult
brasero_mkisofs_read_stderr (BraseroProcess *process, const char *line);
static BraseroBurnResult
brasero_mkisofs_set_argv (BraseroProcess *process,
			  GPtrArray *argv,
			  gboolean has_master,
			  GError **error);

static BraseroBurnResult
brasero_mkisofs_set_source (BraseroJob *job,
			    const BraseroTrackSource *source,
			    GError **error);
static BraseroBurnResult
brasero_mkisofs_set_append (BraseroImager *imager,
			    NautilusBurnDrive *drive,
			    gboolean merge,
			    GError **error);
static BraseroBurnResult
brasero_mkisofs_set_output (BraseroImager *imager,
			    const char *output,
			    gboolean overwrite,
			    gboolean clean,
			    GError **error);
static BraseroBurnResult
brasero_mkisofs_set_output_type (BraseroImager *imager,
				 BraseroTrackSourceType type,
				 BraseroImageFormat format,
				 GError **error);
static BraseroBurnResult
brasero_mkisofs_get_track (BraseroImager *imager,
			   BraseroTrackSource **track,
			   GError **error);
static BraseroBurnResult
brasero_mkisofs_get_track_type (BraseroImager *imager,
				BraseroTrackSourceType *type,
				BraseroImageFormat *format);
static BraseroBurnResult
brasero_mkisofs_get_size (BraseroImager *imager,
			  gint64 *size,
			  gboolean sectors,
			  GError **error);

typedef enum {
	BRASERO_MKISOFS_ACTION_NONE,
	BRASERO_MKISOFS_ACTION_GET_SIZE,
	BRASERO_MKISOFS_ACTION_GET_IMAGE
} BraseroMkisofsAction;

struct BraseroMkisofsPrivate {
	BraseroBurnCaps *caps;
	BraseroMkisofsAction action;
	BraseroImageFormat image_format;

	gint64 sectors_num;

        NautilusBurnDrive *drive;
	gchar *output;

	BraseroTrackSource *source;

	int overwrite:1;
	int use_utf8:1;
	int merge:1;
	int clean:1;

	int iso_ready:1;
	int iso_joliet_ready:1;
};

static GObjectClass *parent_class = NULL;

GType
brasero_mkisofs_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroMkisofsClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_mkisofs_class_init,
			NULL,
			NULL,
			sizeof (BraseroMkisofs),
			0,
			(GInstanceInitFunc)brasero_mkisofs_init,
		};
		static const GInterfaceInfo imager_info =
		{
			(GInterfaceInitFunc) brasero_mkisofs_iface_init_image,
			NULL,
			NULL
		};
		type = g_type_register_static (BRASERO_TYPE_PROCESS, 
					       "BraseroMkisofs",
					       &our_info,
					       0);
		g_type_add_interface_static (type,
					     BRASERO_TYPE_IMAGER,
					     &imager_info);
	}

	return type;
}

static void
brasero_mkisofs_class_init (BraseroMkisofsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroJobClass *job_class = BRASERO_JOB_CLASS (klass);
	BraseroProcessClass *process_class = BRASERO_PROCESS_CLASS (klass);

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_mkisofs_finalize;

	job_class->set_source = brasero_mkisofs_set_source;

	process_class->stdout_func = brasero_mkisofs_read_stdout;
	process_class->stderr_func = brasero_mkisofs_read_stderr;
	process_class->set_argv = brasero_mkisofs_set_argv;
}

static void
brasero_mkisofs_iface_init_image (BraseroImagerIFace *iface)
{
	iface->get_size = brasero_mkisofs_get_size;
	iface->get_track = brasero_mkisofs_get_track;
	iface->get_track_type = brasero_mkisofs_get_track_type;
	iface->set_output = brasero_mkisofs_set_output;
	iface->set_append = brasero_mkisofs_set_append;
	iface->set_output_type = brasero_mkisofs_set_output_type;
}

static void
brasero_mkisofs_init (BraseroMkisofs *obj)
{
	gchar *standard_error;
	gboolean res;

	obj->priv = g_new0(BraseroMkisofsPrivate, 1);

	obj->priv->caps = brasero_burn_caps_get_default ();

	/* this code used to be ncb_mkisofs_supports_utf8 */
	res = g_spawn_command_line_sync ("mkisofs -input-charset utf8",
					 NULL,
					 &standard_error,
					 NULL,
					 NULL);

	if (res && !g_strrstr (standard_error, "Unknown charset"))
		obj->priv->use_utf8 = TRUE;
	else
		obj->priv->use_utf8 = FALSE;

	obj->priv->image_format = BRASERO_IMAGE_FORMAT_NONE;

	g_free (standard_error);
}

static void
brasero_mkisofs_clear (BraseroMkisofs *mkisofs)
{
	if (mkisofs->priv->source) {
		brasero_track_source_free (mkisofs->priv->source);
		mkisofs->priv->source = NULL;
	}

	if (mkisofs->priv->drive) {
		nautilus_burn_drive_unref (mkisofs->priv->drive);
		mkisofs->priv->drive = NULL;
	}

	if (mkisofs->priv->output) {
		if (mkisofs->priv->clean)
			g_remove (mkisofs->priv->output);

		g_free (mkisofs->priv->output),
		mkisofs->priv->output = NULL;
	}

	mkisofs->priv->use_utf8 = FALSE;
}
	
static void
brasero_mkisofs_finalize (GObject *object)
{
	BraseroMkisofs *cobj;
	cobj = BRASERO_MKISOFS(object);

	g_object_unref (cobj->priv->caps);
	cobj->priv->caps = NULL;

	brasero_mkisofs_clear (cobj);

	g_free(cobj->priv);
	G_OBJECT_CLASS(parent_class)->finalize(object);
}

BraseroMkisofs *
brasero_mkisofs_new ()
{
	BraseroMkisofs *obj;
	
	obj = BRASERO_MKISOFS(g_object_new(BRASERO_TYPE_MKISOFS, NULL));
	return obj;
}

static BraseroBurnResult
brasero_mkisofs_read_isosize (BraseroMkisofs *mkisofs, const gchar *line)
{
	/* mkisofs reports blocks of 2048 bytes */
	mkisofs->priv->sectors_num = strtoll (line, NULL, 10);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_mkisofs_read_stdout (BraseroProcess *process, const gchar *line)
{
	BraseroMkisofs *mkisofs;

	mkisofs = BRASERO_MKISOFS (process);

	if (mkisofs->priv->action == BRASERO_MKISOFS_ACTION_GET_SIZE)
		return brasero_mkisofs_read_isosize (mkisofs, line);

	return TRUE;
}

static BraseroBurnResult
brasero_mkisofs_read_stderr (BraseroProcess *process, const gchar *line)
{
	gchar fraction_str [7];
	BraseroMkisofs *mkisofs;

	mkisofs = BRASERO_MKISOFS (process);

	if (strstr (line, "estimate finish")
	&&  sscanf (line, "%6c%% done, estimate finish", fraction_str) == 1) {
		gdouble fraction;
		gint64 written;
	
		fraction = g_strtod (fraction_str, NULL) / (gdouble) 100.0;
		written = mkisofs->priv->sectors_num * 2048 * fraction;

		BRASERO_JOB_TASK_SET_PROGRESS (mkisofs, fraction);
		BRASERO_JOB_TASK_SET_WRITTEN (mkisofs, written);
		BRASERO_JOB_TASK_START_PROGRESS (process, FALSE);
	}
	else if (strstr (line, "Input/output error. Read error on old image")) {
		brasero_job_error (BRASERO_JOB (process), 
				   g_error_new_literal (BRASERO_BURN_ERROR,
							BRASERO_BURN_ERROR_GENERAL,
							_("the old image couldn't be read")));
	}
	else if (strstr (line, "Unable to sort directory")) {
		brasero_job_error (BRASERO_JOB (process), 
				   g_error_new_literal (BRASERO_BURN_ERROR,
							BRASERO_BURN_ERROR_GENERAL,
							_("the image can't be created")));
	}
	else if (strstr (line, "have the same joliet name")) {
		/* we keep the name of the files in case we need to rerun */
	}
	else if (strstr (line, "Joliet tree sort failed.")) {
		brasero_job_error (BRASERO_JOB (process), 
				   g_error_new_literal (BRASERO_BURN_ERROR,
							BRASERO_BURN_ERROR_JOLIET_TREE,
							_("the image can't be created")));
	}
	else if (strstr (line, "Use mkisofs -help")) {
		brasero_job_error (BRASERO_JOB (process), 
				   g_error_new_literal (BRASERO_BURN_ERROR,
							BRASERO_BURN_ERROR_JOLIET_TREE,
							_("this version of mkisofs doesn't seem to be supported")));
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

		res = brasero_mkisofs_base_ask_unreadable_file (BRASERO_MKISOFS_BASE (process),
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
							BRASERO_BURN_ERROR_JOLIET_TREE,
							_("Some files have invalid filenames")));
	}
	else if (strstr (line, "Unknown charset")) {
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new_literal (BRASERO_BURN_ERROR,
							BRASERO_BURN_ERROR_GENERAL,
							_("Unknown character encoding")));
	}
	else if (strstr (line, "No space left on device")) {
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new_literal (BRASERO_BURN_ERROR,
							BRASERO_BURN_ERROR_DISC_SPACE,
							_("There is no space left on the device")));
	}
	else if (strstr (line, "Value too large for defined data type")) {
		/* TODO: get filename from error message */
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new_literal (BRASERO_BURN_ERROR,
							BRASERO_BURN_ERROR_GENERAL,
							_("The file is too large for a CD")));
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_mkisofs_set_append (BraseroImager *imager,
			    NautilusBurnDrive *drive,
			    gboolean merge,
			    GError **error)
{
	BraseroMkisofs *mkisofs;

	mkisofs = BRASERO_MKISOFS (imager);

	if (mkisofs->priv->output
	&& (mkisofs->priv->iso_ready || mkisofs->priv->iso_joliet_ready)
	&&  mkisofs->priv->clean)
		g_remove (mkisofs->priv->output);

	mkisofs->priv->iso_ready = 0;
	mkisofs->priv->iso_joliet_ready = 0;

	if (mkisofs->priv->drive) {
		nautilus_burn_drive_unref (mkisofs->priv->drive);
		mkisofs->priv->drive = NULL;
	}

        nautilus_burn_drive_ref (drive);
	mkisofs->priv->drive = drive;
	mkisofs->priv->merge = merge;

	/* we'll get the start point later */
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_mkisofs_set_output (BraseroImager *imager,
			    const char *output,
			    gboolean overwrite,
			    gboolean clean,
			    GError **error)
{
	BraseroMkisofs *mkisofs;

	mkisofs = BRASERO_MKISOFS (imager);

	if (mkisofs->priv->output) {
		if ((mkisofs->priv->iso_ready || mkisofs->priv->iso_joliet_ready)
		&&  mkisofs->priv->clean)
			g_remove (mkisofs->priv->output);

		g_free (mkisofs->priv->output);
		mkisofs->priv->output = NULL;

		mkisofs->priv->iso_ready = 0;
		mkisofs->priv->iso_joliet_ready = 0;
	}

	if (output)
		mkisofs->priv->output = g_strdup (output);

	mkisofs->priv->overwrite = overwrite;
	mkisofs->priv->clean = clean;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_mkisofs_set_output_type (BraseroImager *imager,
				 BraseroTrackSourceType type,
				 BraseroImageFormat format,
				 GError **error)
{
	BraseroMkisofs *mkisofs;

	mkisofs = BRASERO_MKISOFS (imager);

	if (type != BRASERO_TRACK_SOURCE_DEFAULT
	&&  type != BRASERO_TRACK_SOURCE_IMAGE)
		BRASERO_JOB_NOT_SUPPORTED (mkisofs);

	if (!(format & (BRASERO_IMAGE_FORMAT_ISO | BRASERO_IMAGE_FORMAT_VIDEO)))
		BRASERO_JOB_NOT_SUPPORTED (mkisofs)
	 
	if (mkisofs->priv->image_format == format)
		return BRASERO_BURN_OK;

	mkisofs->priv->image_format = format;
	if (mkisofs->priv->output) {
		if ((mkisofs->priv->iso_ready || mkisofs->priv->iso_joliet_ready)
		&&  mkisofs->priv->clean)
			g_remove (mkisofs->priv->output);

		mkisofs->priv->iso_ready = 0;
		mkisofs->priv->iso_joliet_ready = 0;
	}
	
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_mkisofs_get_track_type (BraseroImager *imager,
				BraseroTrackSourceType *type,
				BraseroImageFormat *format)
{
	BraseroMkisofs *mkisofs;
	BraseroImageFormat retval;

	mkisofs = BRASERO_MKISOFS (imager);

	if (!mkisofs->priv->source)
		BRASERO_JOB_NOT_READY (mkisofs);

	if (type)
		*type = BRASERO_TRACK_SOURCE_IMAGE;

	if (mkisofs->priv->image_format == BRASERO_IMAGE_FORMAT_ANY)
		retval = brasero_burn_caps_get_imager_default_format (mkisofs->priv->caps,
								      mkisofs->priv->source);
	else
		retval = mkisofs->priv->image_format;

	if (format)
		*format = retval;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_mkisofs_get_grafts (BraseroMkisofs *mkisofs,
			    const BraseroTrackSource *source,
			    GError **error)
{
	BraseroBurnResult result;
	BraseroImager *imager = NULL;

	/* we need to download all non local files first
	* and make a list of excluded and graft points */

	/* ask BurnCaps to create an object to get GRAFTS */
	result = brasero_burn_caps_create_imager (mkisofs->priv->caps,
						  &imager,
						  source,
						  BRASERO_TRACK_SOURCE_GRAFTS,
						  NAUTILUS_BURN_MEDIA_TYPE_UNKNOWN,
						  NAUTILUS_BURN_MEDIA_TYPE_UNKNOWN,
						  error);

	/* that way the slave will be unref at the same
	 * time as us or if we set another slave */
	brasero_job_set_slave (BRASERO_JOB (mkisofs), BRASERO_JOB (imager));
	g_object_unref (imager);

	result = brasero_job_set_source (BRASERO_JOB (imager),
					 source,
					 error);
	if (result != BRASERO_BURN_OK)
		return result;

	result = brasero_imager_set_output (imager,
					    NULL,
					    FALSE,
					    TRUE,
					    error);
	if (result != BRASERO_BURN_OK)
		return result;

	result = brasero_imager_set_output_type (imager,
						 BRASERO_TRACK_SOURCE_GRAFTS,
						 source->format,
						 error);
	if (result != BRASERO_BURN_OK)
		return result;

	brasero_job_set_relay_slave_signals (BRASERO_JOB (mkisofs), TRUE);
	result = brasero_imager_get_track (imager,
					   &mkisofs->priv->source,
					   error);
	brasero_job_set_relay_slave_signals (BRASERO_JOB (mkisofs), FALSE);
	return result;
}

static BraseroBurnResult
brasero_mkisofs_set_source (BraseroJob *job,
			    const BraseroTrackSource *source,
			    GError **error)
{
	BraseroMkisofs *mkisofs;
	BraseroBurnResult result = BRASERO_BURN_OK;

	mkisofs = BRASERO_MKISOFS (job);

	if (mkisofs->priv->source) {
		brasero_track_source_free (mkisofs->priv->source);
		mkisofs->priv->source = NULL;
	}
	mkisofs->priv->sectors_num = 0;

	if (mkisofs->priv->output
	&& (mkisofs->priv->iso_ready || mkisofs->priv->iso_joliet_ready)
	&&  mkisofs->priv->clean)
		g_remove (mkisofs->priv->output);

	mkisofs->priv->iso_ready = 0;
	mkisofs->priv->iso_joliet_ready = 0;

	if (source->type != BRASERO_TRACK_SOURCE_GRAFTS
	&&  source->type != BRASERO_TRACK_SOURCE_DATA)
		BRASERO_JOB_NOT_SUPPORTED (mkisofs);

	if (source->type == BRASERO_TRACK_SOURCE_DATA)
		result = brasero_mkisofs_get_grafts (mkisofs, source, error);
	else
		mkisofs->priv->source = brasero_track_source_copy (source);

	return result;
}

static BraseroBurnResult
brasero_mkisofs_set_argv_image (BraseroMkisofs *mkisofs,
				GPtrArray *argv,
				gboolean has_master,
				GError **error)
{
	BraseroBurnResult result;
	BraseroImageFormat format;

	if (!mkisofs->priv->source
	||   mkisofs->priv->source->type != BRASERO_TRACK_SOURCE_GRAFTS)
		BRASERO_JOB_NOT_READY (mkisofs);

	/* set argv */
	g_ptr_array_add (argv, g_strdup ("-r"));

	if (mkisofs->priv->image_format == BRASERO_IMAGE_FORMAT_ANY)
		format = brasero_burn_caps_get_imager_default_format (mkisofs->priv->caps,
								      mkisofs->priv->source);
	else
		format = mkisofs->priv->image_format;

	if ((format & BRASERO_IMAGE_FORMAT_JOLIET))
		g_ptr_array_add (argv, g_strdup ("-J"));

	if ((format & BRASERO_IMAGE_FORMAT_VIDEO))
		g_ptr_array_add (argv, g_strdup ("-dvd-video"));

	g_ptr_array_add (argv, g_strdup ("-graft-points"));
	g_ptr_array_add (argv, g_strdup ("-D"));	// This is dangerous the manual says but apparently it works well

	g_ptr_array_add (argv, g_strdup ("-path-list"));
	g_ptr_array_add (argv, g_strdup (mkisofs->priv->source->contents.grafts.grafts_path));

	if (mkisofs->priv->source->contents.grafts.excluded_path) {
		g_ptr_array_add (argv, g_strdup ("-exclude-list"));
		g_ptr_array_add (argv, g_strdup (mkisofs->priv->source->contents.grafts.excluded_path));
	}

	if (mkisofs->priv->action == BRASERO_MKISOFS_ACTION_GET_SIZE) {
		g_ptr_array_add (argv, g_strdup ("-quiet"));
		g_ptr_array_add (argv, g_strdup ("-print-size"));

		BRASERO_JOB_TASK_SET_ACTION (mkisofs,
					     BRASERO_BURN_ACTION_GETTING_SIZE,
					     NULL,
					     FALSE);
		BRASERO_JOB_TASK_START_PROGRESS (mkisofs, FALSE);
		return BRASERO_BURN_OK;
	}

	if (mkisofs->priv->source->contents.grafts.label) {
		g_ptr_array_add (argv, g_strdup ("-V"));
		g_ptr_array_add (argv, g_strdup (mkisofs->priv->source->contents.grafts.label));
	}

	g_ptr_array_add (argv, g_strdup ("-A"));
	g_ptr_array_add (argv, g_strdup_printf ("Brasero-%i.%i.%i",
						BRASERO_MAJOR_VERSION,
						BRASERO_MINOR_VERSION,
						BRASERO_SUB));
	
	g_ptr_array_add (argv, g_strdup ("-sysid"));
	g_ptr_array_add (argv, g_strdup ("LINUX"));
	
	/* FIXME! -sort is an interesting option allowing to decide where the 
	* files are written on the disc and therefore to optimize later reading */
	/* FIXME: -hidden --hidden-list -hide-jolie -hide-joliet-list will allow to hide
	* some files when we will display the contents of a disc we will want to merge */
	/* FIXME: support preparer publisher options */

	if (!has_master) {
		/* see if an output was given otherwise create a temp one */
		result = brasero_burn_common_check_output (&mkisofs->priv->output,
							   BRASERO_IMAGE_FORMAT_ISO,
							   TRUE,
							   mkisofs->priv->overwrite,
							   NULL,
							   error);
		if (result != BRASERO_BURN_OK)
			return result;

		g_ptr_array_add (argv, g_strdup ("-o"));
		g_ptr_array_add (argv, g_strdup (mkisofs->priv->output));
	}

	if (mkisofs->priv->drive) {
		gint64 last_session, next_wr_add;
		gchar *startpoint = NULL;

		last_session = NCB_MEDIA_GET_LAST_DATA_TRACK_ADDRESS (mkisofs->priv->drive);
		next_wr_add = NCB_MEDIA_GET_NEXT_WRITABLE_ADDRESS (mkisofs->priv->drive);
		if (last_session == -1 || next_wr_add == -1) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("failed to get the start point of the track. Make sure the media allow to add files (it is not closed)"));
			return BRASERO_BURN_ERR;
		}

		startpoint = g_strdup_printf ("%"G_GINT64_FORMAT",%"G_GINT64_FORMAT,
					      last_session,
					      next_wr_add);

		g_ptr_array_add (argv, g_strdup ("-C"));
		g_ptr_array_add (argv, startpoint);

		if (mkisofs->priv->merge) {
		        gchar *dev_str = NULL;

			g_ptr_array_add (argv, g_strdup ("-M"));

			dev_str = g_strdup (NCB_DRIVE_GET_DEVICE (mkisofs->priv->drive));
			g_ptr_array_add (argv, dev_str);
		}
	}

	BRASERO_JOB_TASK_SET_ACTION (mkisofs, 
				     BRASERO_BURN_ACTION_CREATING_IMAGE,
				     NULL,
				     FALSE);

	if (mkisofs->priv->sectors_num)
		BRASERO_JOB_TASK_SET_TOTAL (mkisofs, mkisofs->priv->sectors_num * 2048);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_mkisofs_set_argv (BraseroProcess *process,
			  GPtrArray *argv,
			  gboolean has_master,
			  GError **error)
{
	BraseroMkisofs *mkisofs;
	BraseroBurnResult result;

	mkisofs = BRASERO_MKISOFS (process);

	g_ptr_array_add (argv, g_strdup ("mkisofs"));

	if (mkisofs->priv->use_utf8) {
		g_ptr_array_add (argv, g_strdup ("-input-charset"));
		g_ptr_array_add (argv, g_strdup ("utf8"));
	}

	if (mkisofs->priv->action == BRASERO_MKISOFS_ACTION_GET_SIZE)
		result = brasero_mkisofs_set_argv_image (mkisofs, argv, has_master, error);
	else if (mkisofs->priv->action == BRASERO_MKISOFS_ACTION_GET_IMAGE)
		result = brasero_mkisofs_set_argv_image (mkisofs, argv, has_master, error);
	else if (has_master)
		result = brasero_mkisofs_set_argv_image (mkisofs, argv, has_master, error);
	else
		BRASERO_JOB_NOT_READY (mkisofs);

	if (result != BRASERO_BURN_OK)
		return result;

	/* if we have a slave (ex: BraseroMkisofsBase)
	 * we don't want it to be started */
	brasero_job_set_run_slave (BRASERO_JOB (mkisofs), FALSE);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_mkisofs_get_track (BraseroImager *imager,
			   BraseroTrackSource **track,
			   GError **error)
{
	BraseroMkisofs *mkisofs;
	BraseroImageFormat format;
	BraseroTrackSource *retval;

	mkisofs = BRASERO_MKISOFS (imager);

	if (mkisofs->priv->image_format == BRASERO_IMAGE_FORMAT_NONE)
		BRASERO_JOB_NOT_READY (mkisofs);

	if (mkisofs->priv->image_format == BRASERO_IMAGE_FORMAT_ANY)
		format = brasero_burn_caps_get_imager_default_format (mkisofs->priv->caps,
								      mkisofs->priv->source);
	else
		format = mkisofs->priv->image_format;

	if ((!mkisofs->priv->iso_ready && !(format & BRASERO_IMAGE_FORMAT_JOLIET))
	||  (!mkisofs->priv->iso_joliet_ready && (format & BRASERO_IMAGE_FORMAT_JOLIET))){
		BraseroBurnResult result;

		mkisofs->priv->action = BRASERO_MKISOFS_ACTION_GET_IMAGE;
		result = brasero_job_run (BRASERO_JOB (mkisofs), error);
		mkisofs->priv->action = BRASERO_MKISOFS_ACTION_NONE;
	
		if (result != BRASERO_BURN_OK)
			return result;

		if ((format & BRASERO_IMAGE_FORMAT_JOLIET))
			mkisofs->priv->iso_joliet_ready = 1;
		else
			mkisofs->priv->iso_ready = 1;
	}

	retval = g_new0 (BraseroTrackSource, 1);
	retval->type = BRASERO_TRACK_SOURCE_IMAGE;
	retval->format = format;
	retval->contents.image.image = g_strdup_printf ("file://%s", mkisofs->priv->output);

	*track = retval;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_mkisofs_get_size (BraseroImager *imager,
			  gint64 *size,
			  gboolean sectors,
			  GError **error)
{
	BraseroMkisofs *mkisofs;
	BraseroBurnResult result = BRASERO_BURN_OK;

	mkisofs = BRASERO_MKISOFS (imager);

	if (!mkisofs->priv->source) {
		BraseroJob *slave;

		slave = brasero_job_get_slave (BRASERO_JOB (mkisofs));
		if (!slave)
			BRASERO_JOB_NOT_READY (mkisofs);

		return brasero_imager_get_size (BRASERO_IMAGER (slave), size, sectors, error);
	}

	if (!mkisofs->priv->sectors_num) {
		if (brasero_job_is_running (BRASERO_JOB (imager)))
			return BRASERO_BURN_RUNNING;

		mkisofs->priv->action = BRASERO_MKISOFS_ACTION_GET_SIZE;
		result = brasero_job_run (BRASERO_JOB (mkisofs), error);
		mkisofs->priv->action = BRASERO_MKISOFS_ACTION_NONE;

		if (result != BRASERO_BURN_OK)
			return result;
	}

	if (sectors)
		*size = mkisofs->priv->sectors_num;
	else
		*size = mkisofs->priv->sectors_num * 2048;

	return result;
}
