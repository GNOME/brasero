/***************************************************************************
 *            growisofs.c
 *
 *  dim jan  15:8:51 6
 *  Copyright  6  Rouquier Philippe
 *  brasero-app@wanadoo.fr
 ***************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version  of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite , Boston, MA 111-17, USA.
 */


#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include "burn-basics.h"
#include "burn-common.h"
#include "burn-caps.h"
#include "burn-mkisofs-base.h"
#include "burn-growisofs.h"
#include "burn-process.h"
#include "burn-recorder.h"
#include "burn-imager.h"
#include "brasero-ncb.h"

static void brasero_growisofs_class_init (BraseroGrowisofsClass *klass);
static void brasero_growisofs_init (BraseroGrowisofs *sp);
static void brasero_growisofs_finalize (GObject *object);
static void brasero_growisofs_iface_init_image (BraseroImagerIFace *iface);
static void brasero_growisofs_iface_init_record (BraseroRecorderIFace *iface);

/* Imaging part */
static BraseroBurnResult
brasero_growisofs_set_source (BraseroJob *job,
			      const BraseroTrackSource *source,
			      GError **error);
static BraseroBurnResult
brasero_growisofs_set_output_type (BraseroImager *imager,
				   BraseroTrackSourceType type,
				   BraseroImageFormat format,
				   GError **error);
static BraseroBurnResult
brasero_growisofs_set_append (BraseroImager *imager,
			      NautilusBurnDrive *drive,
			      gboolean merge,
			      GError **error);
static BraseroBurnResult
brasero_growisofs_get_size (BraseroImager *imager,
			    gint64 *size,
			    gboolean sectors,
			    GError **error);

/* Process functions */
static BraseroBurnResult
brasero_growisofs_read_stdout (BraseroProcess *process, 
			       const char *line);
static BraseroBurnResult
brasero_growisofs_read_stderr (BraseroProcess *process,
			       const char *line);
static BraseroBurnResult
brasero_growisofs_set_argv (BraseroProcess *process,
			    GPtrArray *array,
			    gboolean has_master,
			    GError **error);
			
/* Recording part */
static BraseroBurnResult
brasero_growisofs_set_drive (BraseroRecorder *recorder,
			     NautilusBurnDrive *drive,
			     GError **error);
static BraseroBurnResult
brasero_growisofs_set_flags (BraseroRecorder *recorder,
			     BraseroRecorderFlag flags,
			     GError **error);
static BraseroBurnResult
brasero_growisofs_set_rate (BraseroJob *job,
			     gint64 rate);

static BraseroBurnResult
brasero_growisofs_record (BraseroRecorder *recorder,
			  GError **error);
static BraseroBurnResult
brasero_growisofs_blank (BraseroRecorder *recorder,
			 GError **error);

typedef enum {
	BRASERO_GROWISOFS_ACTION_NONE,
	BRASERO_GROWISOFS_ACTION_RECORD,
	BRASERO_GROWISOFS_ACTION_BLANK,
	BRASERO_GROWISOFS_ACTION_GET_SIZE
} BraseroGrowisofsAction;

struct BraseroGrowisofsPrivate {
	BraseroBurnCaps *caps;
	BraseroImageFormat image_format;
	BraseroGrowisofsAction action;

	NautilusBurnDrive *drive;
	gint rate;

	BraseroTrackSource *source;

	gint64 sectors_num;

	int fast_blank:1;
	int use_utf8:1;
	int append:1;
	int merge:1;
	int dummy:1;
	int multi:1;
	int dao:1;
};

static GObjectClass *parent_class = NULL;

GType
brasero_growisofs_get_type()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroGrowisofsClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_growisofs_class_init,
			NULL,
			NULL,
			sizeof (BraseroGrowisofs),
			0,
			(GInstanceInitFunc)brasero_growisofs_init,
		};
		static const GInterfaceInfo imager_info =
		{
			(GInterfaceInitFunc) brasero_growisofs_iface_init_image,
			NULL,
			NULL
		};
		static const GInterfaceInfo recorder_info =
		{
			(GInterfaceInitFunc) brasero_growisofs_iface_init_record,
			NULL,
			NULL
		};

		type = g_type_register_static (BRASERO_TYPE_PROCESS, 
					       "BraseroGrowisofs",
					       &our_info,
					       0);
		g_type_add_interface_static (type,
					     BRASERO_TYPE_IMAGER,
					     &imager_info);
		g_type_add_interface_static (type,
					     BRASERO_TYPE_RECORDER,
					     &recorder_info);
	}

	return type;
}

static void
brasero_growisofs_class_init (BraseroGrowisofsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroJobClass *job_class = BRASERO_JOB_CLASS (klass);
	BraseroProcessClass *process_class = BRASERO_PROCESS_CLASS (klass);

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_growisofs_finalize;

	job_class->set_source = brasero_growisofs_set_source;
	job_class->set_rate = brasero_growisofs_set_rate;

	process_class->stdout_func = brasero_growisofs_read_stdout;
	process_class->stderr_func = brasero_growisofs_read_stderr;
	process_class->set_argv = brasero_growisofs_set_argv;
}

static void
brasero_growisofs_iface_init_image (BraseroImagerIFace *iface)
{
	iface->set_output_type = brasero_growisofs_set_output_type;
	iface->set_append = brasero_growisofs_set_append;
	iface->get_size = brasero_growisofs_get_size;
}

static void
brasero_growisofs_iface_init_record (BraseroRecorderIFace *iface)
{
	iface->set_drive = brasero_growisofs_set_drive;
	iface->set_flags = brasero_growisofs_set_flags;
	iface->record = brasero_growisofs_record;
	iface->blank = brasero_growisofs_blank;
}

static void
brasero_growisofs_init (BraseroGrowisofs *obj)
{
	gchar *standard_error;
	gboolean res;

	obj->priv = g_new0 (BraseroGrowisofsPrivate, 1);
	obj->priv->caps = brasero_burn_caps_get_default ();

	/* this code comes from ncb_mkisofs_supports_utf8 */
	res = g_spawn_command_line_sync ("mkisofs -input-charset utf8", 
					 NULL,
					 &standard_error,
					 NULL, 
					 NULL);
	if (res && !g_strrstr (standard_error, "Unknown charset"))
		obj->priv->use_utf8 = TRUE;
	else
		obj->priv->use_utf8 = FALSE;

	g_free (standard_error);
}

static void
brasero_growisofs_finalize (GObject *object)
{
	BraseroGrowisofs *cobj;
	cobj = BRASERO_GROWISOFS(object);

	if (cobj->priv->caps) {
		g_object_unref (cobj->priv->caps);
		cobj->priv->caps = NULL;
	}

	if (cobj->priv->source) {
		brasero_track_source_free (cobj->priv->source);
		cobj->priv->source = NULL;
	}

	if (cobj->priv->drive) {
		nautilus_burn_drive_unref (cobj->priv->drive);
		cobj->priv->drive = NULL;
	}

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

BraseroGrowisofs *
brasero_growisofs_new ()
{
	BraseroGrowisofs *obj;
	
	obj = BRASERO_GROWISOFS (g_object_new (BRASERO_TYPE_GROWISOFS, NULL));
	
	return obj;
}

static BraseroBurnResult
brasero_growisofs_set_output_type (BraseroImager *imager,
				   BraseroTrackSourceType type,
				   BraseroImageFormat format,
				   GError **error)
{
	BraseroGrowisofs *growisofs;

	growisofs = BRASERO_GROWISOFS (imager);

	if (type != BRASERO_TRACK_SOURCE_DEFAULT
	&&  type != BRASERO_TRACK_SOURCE_IMAGE)
		BRASERO_JOB_NOT_SUPPORTED (growisofs);

	if (!(format & BRASERO_IMAGE_FORMAT_ISO))
		BRASERO_JOB_NOT_SUPPORTED (growisofs);

	growisofs->priv->image_format = format;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_growisofs_get_track (BraseroGrowisofs *growisofs,
			     const BraseroTrackSource *source,
			     GError **error)
{
	BraseroBurnResult result;
	BraseroBurnCaps *caps;
	BraseroImager *imager;

	/* ask BurnCaps to create an object to get GRAFTS */
	caps = brasero_burn_caps_get_default ();
	result = brasero_burn_caps_create_imager (caps,
						  &imager,
						  source,
						  BRASERO_TRACK_SOURCE_GRAFTS,
						  NAUTILUS_BURN_MEDIA_TYPE_UNKNOWN,
						  NAUTILUS_BURN_MEDIA_TYPE_UNKNOWN,
						  error);
	g_object_unref (caps);

	/* that way the slave will be unref at the same
	 * time as us or if we set another slave */
	brasero_job_set_slave (BRASERO_JOB (growisofs), BRASERO_JOB (imager));
	g_object_unref (imager);

	result = brasero_job_set_source (BRASERO_JOB (imager), source, error);
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

	brasero_job_set_relay_slave_signals (BRASERO_JOB (growisofs), TRUE);
	result = brasero_imager_get_track (imager,
					   &growisofs->priv->source,
					   error);
	brasero_job_set_relay_slave_signals (BRASERO_JOB (growisofs), FALSE);
	return result;
}

static BraseroBurnResult
brasero_growisofs_set_source (BraseroJob *job,
			      const BraseroTrackSource *source,
			      GError **error)
{
	BraseroGrowisofs *growisofs;
	BraseroBurnResult result = BRASERO_BURN_OK;

	growisofs = BRASERO_GROWISOFS (job);

	/* we accept ourselves as source and in this case we don't change 
	 * anything: growisofs is both imager and recorder. In this case
	 * we don't delete the previous source */
	if (source->type == BRASERO_TRACK_SOURCE_IMAGER
	&&  source->contents.imager.obj == BRASERO_IMAGER (growisofs))
		return BRASERO_BURN_OK;

	if (growisofs->priv->source) {
		brasero_track_source_free (growisofs->priv->source);
		growisofs->priv->source = NULL;
	}
	growisofs->priv->sectors_num = 0;

	if (source->type != BRASERO_TRACK_SOURCE_DATA
	&&  source->type != BRASERO_TRACK_SOURCE_GRAFTS
	&&  source->type != BRASERO_TRACK_SOURCE_IMAGER
	&&  source->type != BRASERO_TRACK_SOURCE_IMAGE)
		BRASERO_JOB_NOT_SUPPORTED (growisofs);

	if (source->type == BRASERO_TRACK_SOURCE_IMAGE) {
		if (source->format != BRASERO_IMAGE_FORMAT_NONE
		&& (source->format & BRASERO_IMAGE_FORMAT_ISO) == 0)
			BRASERO_JOB_NOT_SUPPORTED (growisofs);
	}
	else if (source->type != BRASERO_TRACK_SOURCE_IMAGER
	     && !(source->format & BRASERO_IMAGE_FORMAT_ISO))
		BRASERO_JOB_NOT_SUPPORTED (growisofs);

	if (source->type == BRASERO_TRACK_SOURCE_DATA)
		result = brasero_growisofs_get_track (growisofs, source, error);
	else
		growisofs->priv->source = brasero_track_source_copy (source);

	return result;
}

static BraseroBurnResult
brasero_growisofs_set_append (BraseroImager *imager,
			      NautilusBurnDrive *drive,
			      gboolean merge,
			      GError **error)
{
	BraseroGrowisofs *growisofs;

	growisofs = BRASERO_GROWISOFS (imager);

	if (drive) {
		if (growisofs->priv->drive) {
			nautilus_burn_drive_unref (growisofs->priv->drive);
			growisofs->priv->drive = NULL;
		}
	
		nautilus_burn_drive_ref (drive);
		growisofs->priv->drive = drive;
	}

	/* growisofs doesn't give the choice it merges */
	growisofs->priv->append = 1;
	growisofs->priv->merge = 1;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_growisofs_get_size (BraseroImager *imager,
			    gint64 *size,
			    gboolean sectors,
			    GError **error)
{
	BraseroBurnResult result = BRASERO_BURN_OK;
	BraseroGrowisofs *growisofs;

	growisofs = BRASERO_GROWISOFS (imager);

	if (!growisofs->priv->source)
		BRASERO_JOB_NOT_READY (growisofs);

	if (growisofs->priv->source->type != BRASERO_TRACK_SOURCE_GRAFTS)
		BRASERO_JOB_NOT_SUPPORTED (growisofs);

	if (!growisofs->priv->sectors_num) {
		if (brasero_job_is_running (BRASERO_JOB (imager)))
			return BRASERO_BURN_RUNNING;

		growisofs->priv->action = BRASERO_GROWISOFS_ACTION_GET_SIZE;
		result = brasero_job_run (BRASERO_JOB (growisofs), error);
		growisofs->priv->action = BRASERO_GROWISOFS_ACTION_NONE;

		if (result != BRASERO_BURN_OK)
			return result;
	}

	/* NOTE: the size in bytes doesn't mean anything since growisofs doesn't
	 * write to the disc the size in sectors is more relevant to check if it
	 * will fit on the disc */
	if (sectors)
		*size = growisofs->priv->sectors_num;
	else 
		*size = growisofs->priv->sectors_num * 2048;

	return result;
}

/* Recording part */
static BraseroBurnResult
brasero_growisofs_record (BraseroRecorder *recorder,
			  GError **error)
{
	BraseroGrowisofs *growisofs;
	BraseroBurnResult result;

	growisofs = BRASERO_GROWISOFS (recorder);

	if (!growisofs->priv->drive)
		BRASERO_JOB_NOT_READY (growisofs);

	if (!growisofs->priv->source)
		BRASERO_JOB_NOT_READY (growisofs);

	growisofs->priv->action = BRASERO_GROWISOFS_ACTION_RECORD;
	result = brasero_job_run (BRASERO_JOB (growisofs), error);
	growisofs->priv->action = BRASERO_GROWISOFS_ACTION_NONE;

	return result;
}

static BraseroBurnResult
brasero_growisofs_blank (BraseroRecorder *recorder,
			 GError **error)
{
	BraseroBurnResult result;
	BraseroGrowisofs *growisofs;
	NautilusBurnMediaType media;

	growisofs = BRASERO_GROWISOFS (recorder);

	if (!growisofs->priv->drive)
		BRASERO_JOB_NOT_READY (growisofs);

	media = nautilus_burn_drive_get_media_type (growisofs->priv->drive);

	if (media != NAUTILUS_BURN_MEDIA_TYPE_DVD_PLUS_RW)
		BRASERO_JOB_NOT_SUPPORTED (growisofs);

	/* There is no way to format RW+ or RW- in restricted overwrite mode in a fast way */
	/* FIXME: actually there is: just reformat them (with dvd+rw-format -force) */
	/* FIXME: we should use the same thing for DVD+RW and DVD-RW in restricted
	 * overwrite mode */
        if (media != NAUTILUS_BURN_MEDIA_TYPE_DVD_PLUS_RW
	||  growisofs->priv->fast_blank)
		BRASERO_JOB_NOT_SUPPORTED (growisofs);

	/* if we have a slave we don't want it to run */
	brasero_job_set_run_slave (BRASERO_JOB (recorder), FALSE);

	growisofs->priv->action = BRASERO_GROWISOFS_ACTION_BLANK;
	result = brasero_job_run (BRASERO_JOB (recorder), error);
	growisofs->priv->action = BRASERO_GROWISOFS_ACTION_NONE;

	return result;
}

static BraseroBurnResult
brasero_growisofs_set_drive (BraseroRecorder *recorder,
			     NautilusBurnDrive *drive,
			     GError **error)
{
	BraseroGrowisofs *growisofs;

	growisofs = BRASERO_GROWISOFS (recorder);

	if (growisofs->priv->drive) {
		nautilus_burn_drive_unref (growisofs->priv->drive);
		growisofs->priv->drive = NULL;
	}
	
	nautilus_burn_drive_ref (drive);
	growisofs->priv->drive = drive;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_growisofs_set_flags (BraseroRecorder *recorder,
			     BraseroRecorderFlag flags,
			     GError **error)
{
	BraseroGrowisofs *growisofs;

	growisofs = BRASERO_GROWISOFS (recorder);

	growisofs->priv->dao = (flags & BRASERO_RECORDER_FLAG_DAO) != 0;
	growisofs->priv->fast_blank = (flags & BRASERO_RECORDER_FLAG_FAST_BLANK) != 0;
	growisofs->priv->dummy = (flags & BRASERO_RECORDER_FLAG_DUMMY) != 0;
	growisofs->priv->multi = (flags & BRASERO_RECORDER_FLAG_MULTI) != 0;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_growisofs_set_rate (BraseroJob *job,
			    gint64 rate)
{
	BraseroGrowisofs *growisofs;

	if (brasero_job_is_running (job))
		return BRASERO_BURN_RUNNING;

	growisofs = BRASERO_GROWISOFS (job);
	growisofs->priv->rate = rate / DVD_SPEED;

	return BRASERO_BURN_OK;
}

/* Process start */
static BraseroBurnResult
brasero_growisofs_read_stdout (BraseroProcess *process, const char *line)
{
	int perc_1, perc_2;
	int speed_1, speed_2;
	long long b_written, b_total;
	BraseroGrowisofs *growisofs;

	growisofs = BRASERO_GROWISOFS (process);

	if (sscanf (line, "%10lld/%lld ( %2d.%1d%%) @%2d.%1dx, remaining %*d:%*d",
		    &b_written, &b_total, &perc_1, &perc_2, &speed_1, &speed_2) == 6) {
		BRASERO_JOB_TASK_SET_WRITTEN (growisofs, b_written);
		BRASERO_JOB_TASK_SET_TOTAL (growisofs, b_total);
		BRASERO_JOB_TASK_SET_RATE (growisofs, (gdouble) (speed_1 * 10 + speed_2) / 10.0 * (gdouble) DVD_SPEED);

		if (growisofs->priv->action == BRASERO_GROWISOFS_ACTION_BLANK) {
			BRASERO_JOB_TASK_SET_ACTION (growisofs,
						     BRASERO_BURN_ACTION_ERASING,
						     NULL,
						     FALSE);
		}
		else {
			BRASERO_JOB_TASK_SET_ACTION (growisofs,
						     BRASERO_BURN_ACTION_WRITING,
						     NULL,
						     FALSE);
		}

		BRASERO_JOB_TASK_START_PROGRESS (growisofs, FALSE);
	}
	else if (strstr (line, "About to execute") || strstr (line, "Executing"))
		brasero_job_set_dangerous (BRASERO_JOB (process), TRUE);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_growisofs_read_stderr (BraseroProcess *process, const char *line)
{
	int perc_1, perc_2;
	BraseroGrowisofs *growisofs;

	growisofs = BRASERO_GROWISOFS (process);

	if (sscanf (line, " %2d.%1d%% done, estimate finish", &perc_1, &perc_2) == 2) {
		gdouble fraction;
		gint64 written, total;

		fraction = (gdouble) ((gdouble) perc_1 +
			   ((gdouble) perc_2 / (gdouble) 10.0)) /
			   (gdouble) 100.0;

		total = growisofs->priv->sectors_num * 2048;
		written = total * fraction;

		BRASERO_JOB_TASK_SET_TOTAL (growisofs, total);
		BRASERO_JOB_TASK_SET_WRITTEN (growisofs, written);
		BRASERO_JOB_TASK_SET_PROGRESS (growisofs, fraction);
		BRASERO_JOB_TASK_SET_ACTION (growisofs,
					     BRASERO_BURN_ACTION_WRITING,
					     NULL,
					     FALSE);
		BRASERO_JOB_TASK_START_PROGRESS (growisofs, FALSE);
	}
	else if (strstr (line, "Total extents scheduled to be written = ")) {
		BraseroGrowisofs *growisofs;

		growisofs = BRASERO_GROWISOFS (process);

		line += strlen ("Total extents scheduled to be written = ");
		growisofs->priv->sectors_num = strtoll (line, NULL, 10);
		BRASERO_JOB_TASK_SET_TOTAL (growisofs, growisofs->priv->sectors_num * 2048);
	}
	else if (strstr (line, "flushing cache") != NULL) {
		BRASERO_JOB_TASK_SET_PROGRESS (growisofs, 1.0);
		BRASERO_JOB_TASK_SET_WRITTEN (growisofs, growisofs->priv->sectors_num * 2048);
		BRASERO_JOB_TASK_SET_ACTION (growisofs,
					     BRASERO_BURN_ACTION_FIXATING,
					     NULL,
					     FALSE);
	}
	else if (strstr (line, "already carries isofs") && strstr (line, "FATAL:")) {
		brasero_job_error (BRASERO_JOB (process), 
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_MEDIA_NOT_WRITABLE,
						_("The disc is already burnt")));
	}
	else if (strstr (line, "unable to open")
	     ||  strstr (line, "unable to stat")) {
		brasero_job_error (BRASERO_JOB (process), 
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_BUSY_DRIVE,
						_("The recorder could not be accessed")));
	}
	else if (strstr (line, "not enough space available") != NULL) {
		brasero_job_error (BRASERO_JOB (process), 
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_GENERAL,
						_("Not enough space available on the disc")));
	}
	else if (strstr (line, "end of user area encountered on this track") != NULL) {
		brasero_job_error (BRASERO_JOB (process), 
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_GENERAL,
						_("The files selected did not fit on the CD")));
	}
	else if (strstr (line, "blocks are free") != NULL) {
		brasero_job_error (BRASERO_JOB (process), 
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_GENERAL,
						_("The files selected did not fit on the CD")));
	}
	else if (strstr (line, "unable to proceed with recording: unable to unmount")) {
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new_literal (BRASERO_BURN_ERROR,
							BRASERO_BURN_ERROR_JOLIET_TREE,
							_("the drive seems to be busy")));
	}
	else if (strstr (line, ":-(") != NULL || strstr (line, "FATAL")) {
		brasero_job_error (BRASERO_JOB (process), 
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_GENERAL,
						_("Unhandled error, aborting")));
	}
	else if (strstr (line, "Incorrectly encoded string")) {
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new_literal (BRASERO_BURN_ERROR,
							BRASERO_BURN_ERROR_JOLIET_TREE,
							_("Some files have invalid filenames")));
	}
	else if (strstr (line, "Joliet tree sort failed.")) {
		brasero_job_error (BRASERO_JOB (process), 
				   g_error_new_literal (BRASERO_BURN_ERROR,
							BRASERO_BURN_ERROR_JOLIET_TREE,
							_("the image can't be created")));
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_growisofs_set_mkisofs_argv (BraseroGrowisofs *growisofs,
				    GPtrArray *argv,
				    GError **error)
{
	BraseroImageFormat format;
	BraseroTrackSource *source;

	g_ptr_array_add (argv, g_strdup ("-r"));
	source = growisofs->priv->source;

	if (growisofs->priv->image_format == BRASERO_IMAGE_FORMAT_ANY)
		format = brasero_burn_caps_get_imager_default_format (growisofs->priv->caps,
								      source);
	else
		format = growisofs->priv->image_format;

	if (format & BRASERO_IMAGE_FORMAT_JOLIET)
		g_ptr_array_add (argv, g_strdup ("-J"));

	if (format & BRASERO_IMAGE_FORMAT_VIDEO)
		g_ptr_array_add (argv, g_strdup ("-dvd-video"));

	if (growisofs->priv->use_utf8) {
		g_ptr_array_add (argv, g_strdup ("-input-charset"));
		g_ptr_array_add (argv, g_strdup ("utf8"));
	}

	g_ptr_array_add (argv, g_strdup ("-graft-points"));
	g_ptr_array_add (argv, g_strdup ("-D"));	// This is dangerous the manual says but apparently it works well

	g_ptr_array_add (argv, g_strdup ("-path-list"));
	g_ptr_array_add (argv, g_strdup (growisofs->priv->source->contents.grafts.grafts_path));

	if (source->contents.grafts.excluded_path) {
		g_ptr_array_add (argv, g_strdup ("-exclude-list"));
		g_ptr_array_add (argv, g_strdup (growisofs->priv->source->contents.grafts.excluded_path));
	}

	if (growisofs->priv->action != BRASERO_GROWISOFS_ACTION_GET_SIZE) {
		if (growisofs->priv->source->contents.grafts.label) {
			g_ptr_array_add (argv, g_strdup ("-V"));
			g_ptr_array_add (argv, g_strdup (growisofs->priv->source->contents.grafts.label));
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

		g_ptr_array_add (argv, g_strdup ("-v"));
	}
	else {
		/* we don't specify -q as there wouldn't be anything */
		g_ptr_array_add (argv, g_strdup ("-print-size"));
	}

	return BRASERO_BURN_OK;
}

/**
 * Some info about use-the-force-luke options
 * dry-run => stops after invoking mkisofs
 * no_tty => avoids fatal error if an isofs exists and an image is piped
 *  	  => skip the five seconds waiting
 * 
 */
static BraseroBurnResult
brasero_growisofs_set_argv_record (BraseroGrowisofs *growisofs,
				   GPtrArray *argv,
				   GError **error)
{
	BraseroBurnResult result;
	BraseroTrackSource *source;

	if (!growisofs->priv->drive)
		BRASERO_JOB_NOT_READY (growisofs);

	if (!growisofs->priv->source)
		BRASERO_JOB_NOT_READY (growisofs);

	source = growisofs->priv->source;

	/* This seems to help to eject tray after burning (at least with mine) */
	g_ptr_array_add (argv, g_strdup ("-use-the-force-luke=notray"));

	if (growisofs->priv->dummy)
		g_ptr_array_add (argv, g_strdup ("-use-the-force-luke=dummy"));

	/* NOTE: dao is supported for DL DVD after 6.0 (think about that for BurnCaps) */
	if (growisofs->priv->dao)
		g_ptr_array_add (argv, g_strdup ("-use-the-force-luke=dao"));

	if (growisofs->priv->rate > 0)
		g_ptr_array_add (argv, g_strdup_printf ("-speed=%d", growisofs->priv->rate));

	/* dvd-compat closes the discs and could cause problems if multi session
	* is required. NOTE: it doesn't work with DVD+RW and DVD-RW in restricted
	* overwrite mode. */
	if (!growisofs->priv->multi)
		g_ptr_array_add (argv, g_strdup ("-dvd-compat"));

	/* see if we're asked to merge some new data: in this case we MUST have
	 * a list of grafts. The image can't come through stdin or an already 
	 * made image */
	if (growisofs->priv->merge) {
		if (source->type != BRASERO_TRACK_SOURCE_GRAFTS)
			BRASERO_JOB_NOT_SUPPORTED (growisofs);

		if (growisofs->priv->sectors_num)
			g_ptr_array_add (argv, g_strdup_printf ("-use-the-force-luke=tracksize:%"G_GINT64_FORMAT, growisofs->priv->sectors_num));

		g_ptr_array_add (argv, g_strdup ("-M"));
		if (NCB_DRIVE_GET_DEVICE (growisofs->priv->drive))
			g_ptr_array_add (argv, g_strdup (NCB_DRIVE_GET_DEVICE (growisofs->priv->drive)));
		else
			return BRASERO_BURN_ERR;
		
		/* this can only happen if source->type == BRASERO_TRACK_SOURCE_GRAFTS */
		if (growisofs->priv->action == BRASERO_GROWISOFS_ACTION_GET_SIZE)
			g_ptr_array_add (argv, g_strdup ("-dry-run"));

		result = brasero_growisofs_set_mkisofs_argv (growisofs, 
							     argv,
							     error);
		if (result != BRASERO_BURN_OK)
			return result;

		brasero_job_set_run_slave (BRASERO_JOB (growisofs), FALSE);
	}
	else {
		if (!growisofs->priv->multi)
			g_ptr_array_add (argv, g_strdup ("-use-the-force-luke=tty"));

		if (source->type == BRASERO_TRACK_SOURCE_IMAGER) {
			BraseroTrackSourceType track_type;
			BraseroImageFormat format;
			BraseroImager *imager;
			gint64 sectors;

			imager = growisofs->priv->source->contents.imager.obj;

			/* we need to know what is the type of the track */
			result = brasero_imager_get_track_type (imager,
								&track_type,
								&format);

			if (result != BRASERO_BURN_OK) {
				if (!error)
					g_set_error (error,
						     BRASERO_BURN_ERROR,
						     BRASERO_BURN_ERROR_GENERAL,
						     _("imager doesn't seem to be ready"));
				return BRASERO_BURN_ERR;
			}

			if (track_type != BRASERO_TRACK_SOURCE_IMAGE
			|| !(format & BRASERO_IMAGE_FORMAT_ISO)) {
				g_set_error (error,
					     BRASERO_BURN_ERROR,
					     BRASERO_BURN_ERROR_GENERAL,
					     _("imager can't create iso9660 images"));
				return BRASERO_BURN_ERR;
			}

			/* ask the size */
			result = brasero_imager_get_size (imager, &sectors, TRUE, error);
			if (result != BRASERO_BURN_OK) {
				if (!error)
					g_set_error (error,
						     BRASERO_BURN_ERROR,
						     BRASERO_BURN_ERROR_GENERAL,
						     _("imager doesn't seem to be ready"));
				return BRASERO_BURN_ERR;
			}

			/* set the buffer. NOTE: apparently this needs to be a power of 2 */
			/* FIXME: is it right to mess with it ? 
			   g_ptr_array_add (argv, g_strdup_printf ("-use-the-force-luke=bufsize:%im", 32)); */

			/* NOTE: tracksize is in block number (2048 bytes) */
			g_ptr_array_add (argv, g_strdup_printf ("-use-the-force-luke=tracksize:%"G_GINT64_FORMAT, sectors));
			if (!g_file_test ("/proc/self/fd/0", G_FILE_TEST_EXISTS)) {
				g_set_error (error,
					     BRASERO_BURN_ERROR,
					     BRASERO_BURN_ERROR_GENERAL,
					     _("the file /proc/self/fd/0 is missing"));
				return BRASERO_BURN_ERR;
			}

			/* FIXME: should we use DAO ? */
			g_ptr_array_add (argv, g_strdup ("-Z"));
			g_ptr_array_add (argv, g_strdup_printf ("%s=/proc/self/fd/0", NCB_DRIVE_GET_DEVICE (growisofs->priv->drive)));

			/* we set the imager as slave */
			brasero_job_set_slave (BRASERO_JOB (growisofs), BRASERO_JOB (imager));
			brasero_job_set_relay_slave_signals (BRASERO_JOB (growisofs), FALSE);
			brasero_job_set_run_slave (BRASERO_JOB (growisofs), TRUE);
		}
		else if (source->type == BRASERO_TRACK_SOURCE_IMAGE) {
			gchar *localpath;

			if (source->format != BRASERO_IMAGE_FORMAT_NONE
			&& (source->format & BRASERO_IMAGE_FORMAT_ISO) == 0)
				BRASERO_JOB_NOT_SUPPORTED (growisofs);

			if (growisofs->priv->sectors_num)
				g_ptr_array_add (argv, g_strdup_printf ("-use-the-force-luke=tracksize:%"G_GINT64_FORMAT, growisofs->priv->sectors_num));

			localpath = brasero_track_source_get_image_localpath (source);
			if (!localpath) {
				g_set_error (error,
					     BRASERO_BURN_ERROR,
					     BRASERO_BURN_ERROR_GENERAL,
					     _("the image is not stored locally"));
				return BRASERO_BURN_ERR;
			}

			g_ptr_array_add (argv, g_strdup ("-Z"));
			g_ptr_array_add (argv, g_strdup_printf ("%s=%s",
								NCB_DRIVE_GET_DEVICE (growisofs->priv->drive),
								localpath));
			g_free (localpath);
			brasero_job_set_run_slave (BRASERO_JOB (growisofs), FALSE);
		}
		else if (source->type == BRASERO_TRACK_SOURCE_GRAFTS) {
			if (growisofs->priv->sectors_num)
				g_ptr_array_add (argv, g_strdup_printf ("-use-the-force-luke=tracksize:%"G_GINT64_FORMAT, growisofs->priv->sectors_num));

			g_ptr_array_add (argv, g_strdup ("-Z"));
			g_ptr_array_add (argv, g_strdup (NCB_DRIVE_GET_DEVICE (growisofs->priv->drive)));

			/* this can only happen if source->type == BRASERO_TRACK_SOURCE_GRAFTS */
			if (growisofs->priv->action == BRASERO_GROWISOFS_ACTION_GET_SIZE)
				g_ptr_array_add (argv, g_strdup ("-dry-run"));

			result = brasero_growisofs_set_mkisofs_argv (growisofs, 
								     argv,
								     error);
			if (result != BRASERO_BURN_OK)
				return result;

			brasero_job_set_run_slave (BRASERO_JOB (growisofs), FALSE);
		}
		else
			BRASERO_JOB_NOT_SUPPORTED (growisofs);
	}

	if (growisofs->priv->action == BRASERO_GROWISOFS_ACTION_GET_SIZE) {
		BRASERO_JOB_TASK_SET_ACTION (growisofs,
					     BRASERO_BURN_ACTION_GETTING_SIZE,
					     NULL,
					     FALSE);
	}
	else {
		BRASERO_JOB_TASK_SET_ACTION (growisofs,
					     BRASERO_BURN_ACTION_PREPARING,
					     NULL,
					     FALSE);
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_growisofs_set_argv_blank (BraseroGrowisofs *growisofs,
				  GPtrArray *argv)
{
	if (!growisofs->priv->fast_blank) {
		g_ptr_array_add (argv, g_strdup ("-Z"));
		g_ptr_array_add (argv, g_strdup_printf ("%s=%s", 
							NCB_DRIVE_GET_DEVICE (growisofs->priv->drive),
							"/dev/zero"));

		/* That should fix a problem where when the DVD had an isofs
		 * growisofs warned that it had an isofs already on the disc */
		g_ptr_array_add (argv, g_strdup ("-use-the-force-luke=tty"));

		/* set a decent speed since from growisofs point of view this
		 * is still writing. Set 4x and growisofs will adapt the speed
		 * anyway if disc or drive can't be used at this speed. */
		g_ptr_array_add (argv, g_strdup_printf ("-speed=%d", 4));

		if (growisofs->priv->dummy)
			g_ptr_array_add (argv, g_strdup ("-use-the-force-luke=dummy"));

		BRASERO_JOB_TASK_SET_ACTION (growisofs,
					     BRASERO_BURN_ACTION_ERASING,
					     NULL,
					     FALSE);
		BRASERO_JOB_TASK_START_PROGRESS (growisofs, FALSE);
	}
	else
		BRASERO_JOB_LOG (growisofs,"skipping fast blank for already formatted DVD+RW media");

	/* we never want any slave to be started */
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_growisofs_set_argv (BraseroProcess *process,
			    GPtrArray *argv,
			    gboolean has_master,
			    GError **error)
{
	BraseroGrowisofs *growisofs;
	BraseroBurnResult result;

	growisofs = BRASERO_GROWISOFS (process);

	if (has_master)
		BRASERO_JOB_NOT_SUPPORTED (growisofs);

	g_ptr_array_add (argv, g_strdup ("growisofs"));

	if (growisofs->priv->action == BRASERO_GROWISOFS_ACTION_RECORD)
		result = brasero_growisofs_set_argv_record (growisofs,
							    argv,
							    error);
	else if (growisofs->priv->action == BRASERO_GROWISOFS_ACTION_GET_SIZE)
		result = brasero_growisofs_set_argv_record (growisofs,
							    argv,
							    error);
	else if (growisofs->priv->action == BRASERO_GROWISOFS_ACTION_BLANK)
		result = brasero_growisofs_set_argv_blank (growisofs, argv);
	else
		BRASERO_JOB_NOT_READY (growisofs);

	return result;
}
