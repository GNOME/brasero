/***************************************************************************
 *            readcd.c
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

#include <nautilus-burn-drive.h>

#include "burn-basics.h"
#include "burn-common.h"
#include "burn-readcd.h"
#include "burn-imager.h"
#include "burn-process.h"
#include "burn-job.h"
#include "burn-caps.h"
#include "brasero-ncb.h"

static void brasero_readcd_class_init (BraseroReadcdClass *klass);
static void brasero_readcd_init (BraseroReadcd *sp);
static void brasero_readcd_finalize (GObject *object);
static void brasero_readcd_iface_init_image (BraseroImagerIFace *iface);

static BraseroBurnResult
brasero_readcd_read_stderr (BraseroProcess *process, const char *line);
static BraseroBurnResult
brasero_readcd_set_argv (BraseroProcess *process,
			 GPtrArray *argv,
			 gboolean has_master,
			 GError **error);

static BraseroBurnResult
brasero_readcd_set_source (BraseroJob *job,
			   const BraseroTrackSource *source,
			   GError **error);
static BraseroBurnResult
brasero_readcd_set_output (BraseroImager *imager,
			   const char *output,
			   gboolean overwrite,
			   gboolean clean,
			   GError **error);
static BraseroBurnResult
brasero_readcd_set_output_type (BraseroImager *imager,
				BraseroTrackSourceType type,
				BraseroImageFormat format,
				GError **error);
static BraseroBurnResult
brasero_readcd_get_track (BraseroImager *imager,
			  BraseroTrackSource **track,
			  GError **error);
static BraseroBurnResult
brasero_readcd_get_size (BraseroImager *imager,
			 gint64 *size,
			 gboolean sectors,
			 GError **error);

static BraseroBurnResult
brasero_readcd_get_track_type (BraseroImager *imager,
			       BraseroTrackSourceType *track_type,
			       BraseroImageFormat *format);

typedef enum {
	BRASERO_READCD_ACTION_IMAGING,
	BRASERO_READCD_ACTION_GETTING_SIZE,
	BRASERO_READCD_ACTION_NONE
} BraseroReadcdAction;

struct BraseroReadcdPrivate {
	BraseroReadcdAction action;
	BraseroBurnCaps *caps;

	BraseroImageFormat image_format;
	BraseroTrackSource *source;

	gint64 sectors_num;

	gchar *toc;
	gchar *output;

	gint overwrite:1;
	gint clean:1;

	gint track_ready:1;
	gint is_DVD:1;
};

static GObjectClass *parent_class = NULL;

GType
brasero_readcd_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroReadcdClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_readcd_class_init,
			NULL,
			NULL,
			sizeof (BraseroReadcd),
			0,
			(GInstanceInitFunc)brasero_readcd_init,
		};

		static const GInterfaceInfo imager_info =
		{
			(GInterfaceInitFunc) brasero_readcd_iface_init_image,
			NULL,
			NULL
		};

		type = g_type_register_static (BRASERO_TYPE_PROCESS,
					       "BraseroReadcd",
					       &our_info,
					       0);

		g_type_add_interface_static (type,
					     BRASERO_TYPE_IMAGER,
					     &imager_info);
	}

	return type;
}

static void
brasero_readcd_class_init (BraseroReadcdClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	BraseroJobClass *job_class = BRASERO_JOB_CLASS (klass);
	BraseroProcessClass *process_class = BRASERO_PROCESS_CLASS (klass);

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_readcd_finalize;

	job_class->set_source = brasero_readcd_set_source;

	process_class->stderr_func = brasero_readcd_read_stderr;
	process_class->set_argv = brasero_readcd_set_argv;
}

static void
brasero_readcd_iface_init_image (BraseroImagerIFace *iface)
{
	iface->set_output_type = brasero_readcd_set_output_type;
	iface->set_output = brasero_readcd_set_output;

	iface->get_track_type = brasero_readcd_get_track_type;
	iface->get_track = brasero_readcd_get_track;
	iface->get_size = brasero_readcd_get_size;
}

static void
brasero_readcd_init (BraseroReadcd *obj)
{
	obj->priv = g_new0 (BraseroReadcdPrivate, 1);
	obj->priv->caps = brasero_burn_caps_get_default ();
}

static void
brasero_readcd_finalize (GObject *object)
{
	BraseroReadcd *cobj;
	cobj = BRASERO_READCD(object);

	g_object_unref (cobj->priv->caps);
	cobj->priv->caps = NULL;

	if (cobj->priv->toc) {
		if (cobj->priv->clean)
			g_remove (cobj->priv->toc);

		g_free (cobj->priv->toc);
		cobj->priv->toc= NULL;
	}

	if (cobj->priv->output) {
		if (cobj->priv->clean)
			g_remove (cobj->priv->output);

		g_free (cobj->priv->output);
		cobj->priv->output= NULL;
	}

	if (cobj->priv->source) {
		brasero_track_source_free (cobj->priv->source);
		cobj->priv->source = NULL;
	}

	g_free(cobj->priv);
	G_OBJECT_CLASS(parent_class)->finalize(object);
}

static BraseroBurnResult
brasero_readcd_get_track (BraseroImager *imager,
			  BraseroTrackSource **track,
			  GError **error)
{
	BraseroReadcd *readcd;
	BraseroTrackSource *retval;
	BraseroImageFormat format;

	readcd = BRASERO_READCD (imager);

	if (!readcd->priv->source)
		BRASERO_JOB_NOT_READY (readcd);

	if (readcd->priv->image_format == BRASERO_IMAGE_FORMAT_NONE)
		BRASERO_JOB_NOT_READY (readcd);

	if (!readcd->priv->track_ready) {
		NautilusBurnDrive *drive;
		BraseroBurnResult result;
		NautilusBurnMediaType media;

		drive = readcd->priv->source->contents.drive.disc;
		media = nautilus_burn_drive_get_media_type (drive);
		if (media > NAUTILUS_BURN_MEDIA_TYPE_CDRW)
			readcd->priv->is_DVD = TRUE;
		else
			readcd->priv->is_DVD = FALSE;

		readcd->priv->action = BRASERO_READCD_ACTION_IMAGING;
		result = brasero_job_run (BRASERO_JOB (readcd), error);
		readcd->priv->action = BRASERO_READCD_ACTION_NONE;

		if (result != BRASERO_BURN_OK)
			return result;

		readcd->priv->track_ready = 1;
	}

	/* see if we are ready */
	retval = g_new0 (BraseroTrackSource, 1);

	/* the disc is the same as when we called set_argv so is the target */
	format = readcd->priv->image_format;
	if (format == BRASERO_IMAGE_FORMAT_ANY)
		format = brasero_burn_caps_get_imager_default_format (readcd->priv->caps,
								      readcd->priv->source);

	retval->type = BRASERO_TRACK_SOURCE_IMAGE;
	retval->format = format;
	if (format & BRASERO_IMAGE_FORMAT_ISO) {
		retval->contents.image.image = g_strdup_printf ("file://%s", readcd->priv->output);
	}
	else if (format & BRASERO_IMAGE_FORMAT_CLONE) {
		retval->contents.image.toc = g_strdup_printf ("file://%s", readcd->priv->toc);
		retval->contents.image.image = g_strdup_printf ("file://%s", readcd->priv->output);
	}

	*track = retval;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_readcd_get_size (BraseroImager *imager,
			 gint64 *size,
			 gboolean sectors,
			 GError **error)
{
	BraseroReadcd *readcd;
	BraseroImageFormat format;
	BraseroBurnResult result = BRASERO_BURN_OK;

	readcd = BRASERO_READCD (imager);

	if (!readcd->priv->source)
		BRASERO_JOB_NOT_READY (readcd);

	if (readcd->priv->sectors_num == 0) {
		if (brasero_job_is_running (BRASERO_JOB (imager)))
			return BRASERO_BURN_RUNNING;

		readcd->priv->action = BRASERO_READCD_ACTION_GETTING_SIZE;
		result = brasero_job_run (BRASERO_JOB (readcd), error);
		readcd->priv->action = BRASERO_READCD_ACTION_NONE;

		if (result != BRASERO_BURN_OK)
			return result;
	}

	format = readcd->priv->image_format;
	if (format == BRASERO_IMAGE_FORMAT_ANY)
		format = brasero_burn_caps_get_imager_default_format (readcd->priv->caps,
								      readcd->priv->source);
	if (sectors)
		*size = readcd->priv->sectors_num;
	else if (format & BRASERO_IMAGE_FORMAT_ISO)
		*size = readcd->priv->sectors_num * 2048;
	else if (format & BRASERO_IMAGE_FORMAT_CLONE)
		*size = readcd->priv->sectors_num * 2448;
	else
		BRASERO_JOB_NOT_SUPPORTED (readcd);

	return result;
}

static BraseroBurnResult
brasero_readcd_get_track_type (BraseroImager *imager,
			       BraseroTrackSourceType *type,
			       BraseroImageFormat *format)
{
	BraseroReadcd *readcd;

	readcd = BRASERO_READCD (imager);

	if (type)
		*type = BRASERO_TRACK_SOURCE_IMAGE;

	if (format) {
		if (readcd->priv->image_format == BRASERO_IMAGE_FORMAT_ANY)
			*format = brasero_burn_caps_get_imager_default_format (readcd->priv->caps,
									       readcd->priv->source);
		else
			*format = readcd->priv->image_format;
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_readcd_set_source (BraseroJob *job,
			   const BraseroTrackSource *source,
			   GError **error)
{
	BraseroReadcd *readcd;

	readcd = BRASERO_READCD (job);

	if (readcd->priv->source) {
		brasero_track_source_free (readcd->priv->source);
		readcd->priv->source = NULL;
	}

	if (readcd->priv->clean && readcd->priv->track_ready) {
		if (readcd->priv->output)
			g_remove (readcd->priv->output);

		if (readcd->priv->toc)
			g_remove (readcd->priv->toc);
	}

	readcd->priv->sectors_num = 0;
	readcd->priv->track_ready = 0;

	if (source->type != BRASERO_TRACK_SOURCE_DISC)
		BRASERO_JOB_NOT_SUPPORTED (readcd);

	readcd->priv->source = brasero_track_source_copy (source);
	return BRASERO_BURN_OK;	
}

static BraseroBurnResult
brasero_readcd_set_output_type (BraseroImager *imager,
				BraseroTrackSourceType type,
				BraseroImageFormat format,
				GError **error)
{
	BraseroReadcd *readcd;

	readcd = BRASERO_READCD (imager);

	if (type != BRASERO_TRACK_SOURCE_IMAGE
	&&  type != BRASERO_TRACK_SOURCE_DEFAULT)
		BRASERO_JOB_NOT_SUPPORTED (readcd);

	if (!(format & (BRASERO_IMAGE_FORMAT_CLONE | BRASERO_IMAGE_FORMAT_ISO)))
		BRASERO_JOB_NOT_SUPPORTED (readcd);

	if (readcd->priv->clean && readcd->priv->track_ready) {
		if (readcd->priv->output)
			g_remove (readcd->priv->output);

		if (readcd->priv->toc)
			g_remove (readcd->priv->toc);
	}

	readcd->priv->sectors_num = 0;
	readcd->priv->track_ready = 0;

	/* NOTE: it's not good to call burn_caps here to get the default
	 * target for the output since the disc might change before we 
	 * call get track */
	readcd->priv->image_format = format;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_readcd_set_output (BraseroImager *imager,
			   const gchar *output,
			   gboolean overwrite,
			   gboolean clean,
			   GError **error)
{
	BraseroReadcd *readcd;

	readcd = BRASERO_READCD (imager);

	if (readcd->priv->output) {
		if (readcd->priv->clean && readcd->priv->track_ready)
			g_remove (readcd->priv->output);

		g_free (readcd->priv->output);
		readcd->priv->output = NULL;
	}

	if (readcd->priv->toc) {
		if (readcd->priv->clean && readcd->priv->track_ready)
			g_remove (readcd->priv->toc);

		g_free (readcd->priv->toc);
		readcd->priv->toc = NULL;
	}
	readcd->priv->track_ready = 0;

	if (output)
		readcd->priv->output = g_strdup (output);

	readcd->priv->overwrite = overwrite;
	readcd->priv->clean = clean;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_readcd_read_stderr (BraseroProcess *process, const gchar *line)
{
	BraseroReadcd *readcd;
	gchar *pos;

	readcd = BRASERO_READCD (process);
	if ((pos = strstr (line, "addr:"))) {
		gint sector;
		gint64 written;
		BraseroImageFormat format;

		pos += strlen ("addr:");
		sector = strtoll (pos, NULL, 10);

		format = readcd->priv->image_format;
		if (format == BRASERO_IMAGE_FORMAT_ANY)
			format = brasero_burn_caps_get_imager_default_format (readcd->priv->caps,
									      readcd->priv->source);

		if (format & BRASERO_IMAGE_FORMAT_ISO)
			written = sector * 2048;
		else if (format & BRASERO_IMAGE_FORMAT_CLONE)
			written = sector * 2448;
		else
			written = sector * 2048;

		BRASERO_JOB_TASK_SET_WRITTEN (readcd, written);

		if (sector > 10)
			BRASERO_JOB_TASK_START_PROGRESS (process, FALSE);
	}
	else if ((pos = strstr (line, "Capacity:"))) {
		gint64 total;
		BraseroImageFormat format;

		pos += strlen ("Capacity:");
		readcd->priv->sectors_num = strtoll (pos, NULL, 10);

		format = readcd->priv->image_format;
		if (format == BRASERO_IMAGE_FORMAT_ANY)
			format = brasero_burn_caps_get_imager_default_format (readcd->priv->caps,
									      readcd->priv->source);

		if (format & BRASERO_IMAGE_FORMAT_ISO)
			total = readcd->priv->sectors_num * 2048;
		else if (format & BRASERO_IMAGE_FORMAT_CLONE)
			total = readcd->priv->sectors_num * 2448;
		else
			total = readcd->priv->sectors_num * 2048;

		BRASERO_JOB_TASK_SET_TOTAL (readcd, total);

		if (readcd->priv->action != BRASERO_READCD_ACTION_GETTING_SIZE) {
			BRASERO_JOB_TASK_SET_ACTION (readcd,
						     BRASERO_BURN_ACTION_DRIVE_COPY,
						     NULL,
						     FALSE);
		}
		else
			brasero_job_finished (BRASERO_JOB (readcd));
	}
	else if (strstr (line, "Device not ready.")) {
		brasero_job_error (BRASERO_JOB (readcd),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_BUSY_DRIVE,
						_("the drive is not ready")));
	}
	else if (strstr (line, "Device or resource busy")) {
		brasero_job_error (BRASERO_JOB (readcd),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_BUSY_DRIVE,
						_("you don't seem to have the required permissions to access the drive")));
	}
	else if (strstr (line, "Cannot open SCSI driver.")) {
		brasero_job_error (BRASERO_JOB (readcd),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_BUSY_DRIVE,
						_("you don't seem to have the required permissions to access the drive")));		
	}
	else if (strstr (line, "Cannot send SCSI cmd via ioctl")) {
		brasero_job_error (BRASERO_JOB (readcd),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_SCSI_IOCTL,
						_("you don't seem to have the required permissions to access the drive")));
	}
	else if (strstr (line, "Time total:")) {
		brasero_job_finished (BRASERO_JOB (process));
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_readcd_set_argv (BraseroProcess *process,
			 GPtrArray *argv,
			 gboolean has_master,
			 GError **error)
{
	BraseroBurnResult result = FALSE;
	BraseroImageFormat format;
	NautilusBurnMediaType type;
	NautilusBurnDrive *drive;
	BraseroReadcd *readcd;
	char *outfile_arg;
	char *dev_str;

	readcd = BRASERO_READCD (process);
	brasero_job_set_run_slave (BRASERO_JOB (readcd), FALSE);

	if (!readcd->priv->source)
		BRASERO_JOB_NOT_READY (readcd);

	g_ptr_array_add (argv, g_strdup ("readcd"));

	drive = readcd->priv->source->contents.drive.disc;
	if (NCB_DRIVE_GET_DEVICE (drive))
		dev_str = g_strdup_printf ("dev=%s", NCB_DRIVE_GET_DEVICE (drive));
	else
		return BRASERO_BURN_ERR;

	g_ptr_array_add (argv, dev_str);

	g_ptr_array_add (argv, g_strdup ("-nocorr"));

	type = nautilus_burn_drive_get_media_type (drive);
	format = readcd->priv->image_format;

	if (format == BRASERO_IMAGE_FORMAT_ANY)
		format = brasero_burn_caps_get_imager_default_format (readcd->priv->caps,
								      readcd->priv->source);
	if (type > NAUTILUS_BURN_MEDIA_TYPE_CDRW 
	&& !(format & BRASERO_IMAGE_FORMAT_ISO)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("raw images cannot be created with DVDs"));
		return BRASERO_BURN_ERR;
	}

	if (format & BRASERO_IMAGE_FORMAT_CLONE) {
		/* NOTE: with this option the sector size is 2448 
		 * because it is raw96 (2352+96) otherwise it is 2048  */
		g_ptr_array_add (argv, g_strdup ("-clone"));
	}
	else if (format & BRASERO_IMAGE_FORMAT_ISO)
		g_ptr_array_add (argv, g_strdup ("-noerror"));
	else
		BRASERO_JOB_NOT_SUPPORTED (readcd);

	if (readcd->priv->action == BRASERO_READCD_ACTION_GETTING_SIZE) {
		g_ptr_array_add (argv, g_strdup ("-sectors=0-0"));

		BRASERO_JOB_TASK_SET_ACTION (readcd,
					     BRASERO_BURN_ACTION_GETTING_SIZE,
					     NULL,
					     FALSE);
		BRASERO_JOB_TASK_START_PROGRESS (readcd, FALSE);
		return BRASERO_BURN_OK;
	}

	if (!has_master) {
		if (format & BRASERO_IMAGE_FORMAT_ISO) {
			result = brasero_burn_common_check_output (&readcd->priv->output,
								   BRASERO_IMAGE_FORMAT_ISO,
								   TRUE,
								   readcd->priv->overwrite,
								   NULL,
								   error);
		}
		else if (format & BRASERO_IMAGE_FORMAT_CLONE) {	
			result = brasero_burn_common_check_output (&readcd->priv->output,
								   BRASERO_IMAGE_FORMAT_CLONE,
								   TRUE,
								   readcd->priv->overwrite,
								   &readcd->priv->toc,
								   error);
		}
		else
			BRASERO_JOB_NOT_SUPPORTED (readcd);

		if (result != BRASERO_BURN_OK)
			return result;

		outfile_arg = g_strdup_printf ("-f=%s", readcd->priv->output);
		g_ptr_array_add (argv, outfile_arg);
	}
	else if (format & BRASERO_IMAGE_FORMAT_ISO) {
		outfile_arg = g_strdup ("-f=-");
		g_ptr_array_add (argv, outfile_arg);
	}
	else 	/* unfortunately raw images can't be piped out */
		BRASERO_JOB_NOT_SUPPORTED (readcd);

	return BRASERO_BURN_OK;
}
