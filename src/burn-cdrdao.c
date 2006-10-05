/***************************************************************************
 *            cdrdao.c
 *
 *  dim jan 22 15:38:18 2006
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

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <libgnomevfs/gnome-vfs-utils.h>

#include <nautilus-burn-drive.h>

#include "burn-cdrdao.h"
#include "burn-common.h"
#include "burn-basics.h"
#include "burn-process.h"
#include "burn-recorder.h"
#include "burn-imager.h"
#include "burn-toc2cue.h"
#include "burn-caps.h"
#include "brasero-ncb.h"

static void brasero_cdrdao_class_init (BraseroCdrdaoClass *klass);
static void brasero_cdrdao_init (BraseroCdrdao *sp);
static void brasero_cdrdao_finalize (GObject *object);
static void brasero_cdrdao_iface_init_record (BraseroRecorderIFace *iface);
static void brasero_cdrdao_iface_init_image (BraseroImagerIFace *iface);

static BraseroBurnResult
brasero_cdrdao_read_stderr (BraseroProcess *process,
			    const char *line);
static BraseroBurnResult
brasero_cdrdao_set_argv (BraseroProcess *process,
			 GPtrArray *argv,
			 gboolean has_master, 
			 GError **error);
static BraseroBurnResult
brasero_cdrdao_post (BraseroProcess *process,
		     BraseroBurnResult retval);
				  
static BraseroBurnResult
brasero_cdrdao_set_drive (BraseroRecorder *recorder,
			  NautilusBurnDrive *drive,
			  GError **error);

static BraseroBurnResult
brasero_cdrdao_set_flags (BraseroRecorder *recorder,
			  BraseroRecorderFlag flags,
			  GError **error);
static BraseroBurnResult
brasero_cdrdao_set_rate (BraseroJob *job,
			  gint64 rate);

static BraseroBurnResult
brasero_cdrdao_record (BraseroRecorder *recorder,
		       GError **error);
static BraseroBurnResult
brasero_cdrdao_blank (BraseroRecorder *recorder,
		      GError **error);

static BraseroBurnResult
brasero_cdrdao_get_track_type (BraseroImager *imager,
			       BraseroTrackSourceType *type,
			       BraseroImageFormat *format);

static BraseroBurnResult
brasero_cdrdao_get_size_image (BraseroImager *imager,
			       gint64 *size,
			       gboolean sectors,
			       GError **error);
static BraseroBurnResult
brasero_cdrdao_get_track (BraseroImager *imager,
			  BraseroTrackSource **track,
			  GError **error);

static BraseroBurnResult
brasero_cdrdao_set_source (BraseroJob *job,
			   const BraseroTrackSource *source,
			   GError **error);
static BraseroBurnResult
brasero_cdrdao_set_output (BraseroImager *imager,
			   const char *ouput,
			   gboolean overwrite,
			   gboolean clean,
			   GError **error);
static BraseroBurnResult
brasero_cdrdao_set_output_type (BraseroImager *imager,
				BraseroTrackSourceType type,
				BraseroImageFormat format,
				GError **error);

typedef enum {
	BRASERO_CDRDAO_ACTION_NONE,
	BRASERO_CDRDAO_ACTION_IMAGE,
	BRASERO_CDRDAO_ACTION_GET_SIZE,
	BRASERO_CDRDAO_ACTION_RECORD,
	BRASERO_CDRDAO_ACTION_BLANK,
} BraseroCdrdaoAction;

struct BraseroCdrdaoPrivate {
	BraseroCdrdaoAction action;
	BraseroTrackSource *track;
	NautilusBurnDrive *drive;

	BraseroImageFormat format;

	gchar *src_output;

	gchar *output;
	gchar *datafile;

	gint rate;               /* speed at which we should write */

	gint64 isosize;
	gint sectors_num;

	int dao:1;
	int dummy:1;
	int multi:1;
	int nograce:1;
	int overburn:1;
	int burnproof:1;

	int overwrite:1;
	int clean:1;

	int blank_fast:1;

	int track_ready:1;
};

static GObjectClass *parent_class = NULL;

GType
brasero_cdrdao_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroCdrdaoClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_cdrdao_class_init,
			NULL,
			NULL,
			sizeof (BraseroCdrdao),
			0,
			(GInstanceInitFunc)brasero_cdrdao_init,
		};

		static const GInterfaceInfo imager_info =
		{
			(GInterfaceInitFunc) brasero_cdrdao_iface_init_image,
			NULL,
			NULL
		};
		static const GInterfaceInfo recorder_info =
		{
			(GInterfaceInitFunc) brasero_cdrdao_iface_init_record,
			NULL,
			NULL
		};

		type = g_type_register_static (BRASERO_TYPE_PROCESS, 
					       "BraseroCdrdao", 
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
brasero_cdrdao_iface_init_record (BraseroRecorderIFace *iface)
{
	iface->set_drive = brasero_cdrdao_set_drive;
	iface->set_flags = brasero_cdrdao_set_flags;
	iface->record = brasero_cdrdao_record;
	iface->blank = brasero_cdrdao_blank;
}

static void
brasero_cdrdao_iface_init_image (BraseroImagerIFace *iface)
{
	iface->get_size = brasero_cdrdao_get_size_image;
	iface->get_track = brasero_cdrdao_get_track;
	iface->get_track_type = brasero_cdrdao_get_track_type;

	iface->set_output = brasero_cdrdao_set_output;
	iface->set_output_type = brasero_cdrdao_set_output_type;
}

static void
brasero_cdrdao_class_init (BraseroCdrdaoClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroJobClass *job_class = BRASERO_JOB_CLASS (klass);
	BraseroProcessClass *process_class = BRASERO_PROCESS_CLASS (klass);

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_cdrdao_finalize;

	job_class->set_rate = brasero_cdrdao_set_rate;
	job_class->set_source = brasero_cdrdao_set_source;

	process_class->stderr_func = brasero_cdrdao_read_stderr;
	process_class->set_argv = brasero_cdrdao_set_argv;
	process_class->post = brasero_cdrdao_post;
}

static void
brasero_cdrdao_init (BraseroCdrdao *obj)
{
	obj->priv = g_new0(BraseroCdrdaoPrivate, 1);
	obj->priv->isosize = -1;
	obj->priv->clean = TRUE;
}

static void
brasero_cdrdao_clean_output (BraseroCdrdao *cdrdao)
{
	if (cdrdao->priv->output) {
		BraseroImageFormat format = BRASERO_IMAGE_FORMAT_NONE;

		if (cdrdao->priv->format == BRASERO_IMAGE_FORMAT_ANY) {
			BraseroBurnCaps *caps;

			caps = brasero_burn_caps_get_default ();
			format = brasero_burn_caps_get_imager_default_format (caps,
									      cdrdao->priv->track);
			g_object_unref (caps);
		}
		else
			format = cdrdao->priv->format;

		if (cdrdao->priv->clean
		|| !(format & BRASERO_IMAGE_FORMAT_CDRDAO))
			g_remove (cdrdao->priv->output);

		g_free (cdrdao->priv->output);
		cdrdao->priv->output = NULL;
	}

	if (cdrdao->priv->datafile) {
		if (cdrdao->priv->clean && cdrdao->priv->track_ready)
			g_remove (cdrdao->priv->datafile);

		g_free (cdrdao->priv->datafile);
		cdrdao->priv->datafile = NULL;
	}

	cdrdao->priv->track_ready = 0;
}

static void
brasero_cdrdao_finalize (GObject *object)
{
	BraseroCdrdao *cobj;
	cobj = BRASERO_CDRDAO (object);

	if (cobj->priv->src_output) {
		g_free (cobj->priv->src_output);
		cobj->priv->src_output = NULL;
	}

	brasero_cdrdao_clean_output (cobj);

	if (cobj->priv->drive) {
		nautilus_burn_drive_unref (cobj->priv->drive);
		cobj->priv->drive = NULL;
	}
	
	if (cobj->priv->track) {
		brasero_track_source_free (cobj->priv->track);
		cobj->priv->track = NULL;
	}

	g_free(cobj->priv);
	G_OBJECT_CLASS(parent_class)->finalize(object);
}

BraseroCdrdao *
brasero_cdrdao_new ()
{
	BraseroCdrdao *obj;
	
	obj = BRASERO_CDRDAO (g_object_new (BRASERO_TYPE_CDRDAO, NULL));
	
	return obj;
}

static gboolean
brasero_cdrdao_read_stderr_image (BraseroCdrdao *cdrdao, const char *line)
{
	int min, sec, sub, s1;

	if (sscanf (line, "%d:%d:%d", &min, &sec, &sub) == 3) {
		gdouble fraction;
		guint64 secs = min * 60 + sec;

		fraction = (gdouble) secs / cdrdao->priv->isosize;
		BRASERO_JOB_TASK_SET_WRITTEN (cdrdao, secs * 75 * 2352);
		BRASERO_JOB_TASK_SET_PROGRESS (cdrdao, fraction);

		if (secs > 2)
			BRASERO_JOB_TASK_START_PROGRESS (cdrdao, FALSE);
	}
	else if (sscanf (line, "Leadout %*s %*d %d:%d:%*d(%i)", &min, &sec, &s1) == 3) {
		/* we get the number of sectors. As we added -raw each sector = 2352 bytes */
		cdrdao->priv->sectors_num = s1;
		BRASERO_JOB_TASK_SET_TOTAL (cdrdao, s1 * 2352);

		cdrdao->priv->isosize = min * 60 + sec;
		if (cdrdao->priv->action == BRASERO_CDRDAO_ACTION_GET_SIZE)
			brasero_job_finished (BRASERO_JOB (cdrdao));
	}
	else if (strstr (line, "Copying audio tracks")) {
		BRASERO_JOB_TASK_SET_ACTION (cdrdao,
					     BRASERO_BURN_ACTION_DRIVE_COPY,
					     _("Copying audio track"),
					     FALSE);
	}
	else if (strstr (line, "Copying data track")) {
		BRASERO_JOB_TASK_SET_ACTION (cdrdao,
					     BRASERO_BURN_ACTION_DRIVE_COPY,
					     _("Copying data track"),
					     FALSE);
	}
	else
		return FALSE;

	return TRUE;
}

static gboolean
brasero_cdrdao_read_stderr_record (BraseroCdrdao *cdrdao, const char *line)
{
	int fifo, track, min, sec;
	guint written, total;

	if (sscanf (line, "Wrote %u of %u (Buffers %d%%  %*s", &written, &total, &fifo) >= 2) {
		brasero_job_set_dangerous (BRASERO_JOB (cdrdao), TRUE);

		BRASERO_JOB_TASK_SET_WRITTEN (cdrdao, written * 1048576);
		BRASERO_JOB_TASK_SET_TOTAL (cdrdao, total * 1048576);
		BRASERO_JOB_TASK_SET_ACTION (cdrdao,
					     BRASERO_BURN_ACTION_WRITING,
					     NULL,
					     FALSE);

		BRASERO_JOB_TASK_START_PROGRESS (cdrdao, FALSE);
	}
	else if (sscanf (line, "Wrote %*s blocks. Buffer fill min") == 1) {
		/* this is for fixating phase */
		BRASERO_JOB_TASK_SET_ACTION (cdrdao,
					     BRASERO_BURN_ACTION_FIXATING,
					     NULL,
					     FALSE);
	}
	else if (sscanf (line, "Analyzing track %d %*s start %d:%d:%*d, length %*d:%*d:%*d", &track, &min, &sec) == 3) {
		gchar *string;

		string = g_strdup_printf (_("Analysing track %02i"), track);
		BRASERO_JOB_TASK_SET_ACTION (cdrdao,
					     BRASERO_BURN_ACTION_ANALYSING,
					     string,
					     TRUE);
		g_free (string);
	}
	else if (sscanf (line, "%d:%d:%*d", &min, &sec) == 2) {
		gint64 total;
		gint64 written;
		guint64 secs = min * 60 + sec;

		if (secs > 2)
			BRASERO_JOB_TASK_START_PROGRESS (cdrdao, FALSE);

		written = secs * 75 * 2352;
		BRASERO_JOB_TASK_SET_WRITTEN (cdrdao, written);

		total = cdrdao->priv->isosize * 75 * 2352;
		BRASERO_JOB_TASK_SET_TOTAL (cdrdao, total);
	}
	else if (strstr (line, "Writing track")) {
		brasero_job_set_dangerous (BRASERO_JOB (cdrdao), TRUE);
	}
	else if (strstr (line, "Writing finished successfully")
	     ||  strstr (line, "On-the-fly CD copying finished successfully")) {
		brasero_job_set_dangerous (BRASERO_JOB (cdrdao), FALSE);
	}
	else {
		gchar *cuepath, *name;

		if (!cdrdao->priv->track)
			return FALSE;

		cuepath = brasero_track_source_get_cue_localpath (cdrdao->priv->track);
		if (!cuepath)
			return FALSE;

		if (!strstr (line, cuepath)) {
			g_free (cuepath);
			return FALSE;
		}

		name = g_path_get_basename (cuepath);
		g_free (cuepath);

		brasero_job_error (BRASERO_JOB (cdrdao),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_GENERAL,
						_("the cue file (%s) seems to be invalid"),
						name));
		g_free (name);
	}

	return TRUE;
}

static BraseroBurnResult
brasero_cdrdao_read_stderr (BraseroProcess *process, const char *line)
{
	BraseroCdrdao *cdrdao;
	gboolean result = FALSE;

	cdrdao = BRASERO_CDRDAO (process);

	if (cdrdao->priv->action == BRASERO_CDRDAO_ACTION_RECORD
	||  cdrdao->priv->action == BRASERO_CDRDAO_ACTION_BLANK)
		result = brasero_cdrdao_read_stderr_record (cdrdao, line);
	else if (cdrdao->priv->action == BRASERO_CDRDAO_ACTION_IMAGE
	      ||  cdrdao->priv->action == BRASERO_CDRDAO_ACTION_GET_SIZE)
		result = brasero_cdrdao_read_stderr_image (cdrdao, line);

	if (result)
		return BRASERO_BURN_OK;

	if (strstr (line, "Cannot setup device")) {
		brasero_job_error (BRASERO_JOB (cdrdao),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_BUSY_DRIVE,
						_("the drive seems to be busy")));
	}
	else if (strstr (line, "Illegal command")) {
		brasero_job_error (BRASERO_JOB (cdrdao),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_GENERAL,
						_("your version of cdrdao doesn't seem to be supported by libbrasero")));
	}
	else if (strstr (line, "Operation not permitted. Cannot send SCSI")) {
		brasero_job_error (BRASERO_JOB (cdrdao),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_SCSI_IOCTL,
						_("You don't seem to have the required permission to use this drive")));
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_cdrdao_get_track_type (BraseroImager *imager,
			       BraseroTrackSourceType *type,
			       BraseroImageFormat *format)
{
	BraseroCdrdao *cdrdao;

	cdrdao = BRASERO_CDRDAO (imager);

	if (!cdrdao->priv->track)
		BRASERO_JOB_NOT_READY (cdrdao);

	if (type)
		*type = BRASERO_TRACK_SOURCE_IMAGE;

	if (format) {
		BraseroImageFormat obj_format;
		BraseroBurnCaps *caps;

		caps = brasero_burn_caps_get_default ();
		if (cdrdao->priv->format == BRASERO_IMAGE_FORMAT_ANY)
			obj_format = brasero_burn_caps_get_imager_default_format (caps,
										  cdrdao->priv->track);
		else
			obj_format = cdrdao->priv->format;
		g_object_unref (caps);

		*format = obj_format;
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_cdrdao_set_source (BraseroJob *job,
			   const BraseroTrackSource *source,
			   GError **error)
{
	BraseroCdrdao *cdrdao;

	cdrdao = BRASERO_CDRDAO (job);

	/* Remove any current output */
	brasero_cdrdao_clean_output (cdrdao);

	cdrdao->priv->track_ready = 0;

	/* NOTE: we can accept ourselves as our source (cdrdao is both imager
	 * and recorder). In this case we don't delete the previous source */
	if (source->type == BRASERO_TRACK_SOURCE_IMAGER
	&&  BRASERO_IMAGER (cdrdao) == source->contents.imager.obj)
		return BRASERO_BURN_OK;

	if (cdrdao->priv->track) {
		brasero_track_source_free (cdrdao->priv->track);
		cdrdao->priv->track = NULL;
	}

	/* NOTE: we don't check the type of source precisely since this check partly
	 * depends on what we do with cdrdao afterward (it's both imager/recorder) */
        if (source->type != BRASERO_TRACK_SOURCE_DISC
	&& !(source->format & (BRASERO_IMAGE_FORMAT_CUE|BRASERO_IMAGE_FORMAT_CDRDAO)))
		BRASERO_JOB_NOT_SUPPORTED (cdrdao);

	cdrdao->priv->track = brasero_track_source_copy (source);
	cdrdao->priv->isosize = -1;
	cdrdao->priv->sectors_num = 0;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_cdrdao_set_output (BraseroImager *imager,
			   const char *output,
			   gboolean overwrite,
			   gboolean clean,
			   GError **error)
{
	BraseroCdrdao *cdrdao;

	cdrdao = BRASERO_CDRDAO (imager);

	brasero_cdrdao_clean_output (cdrdao);

	if (cdrdao->priv->src_output) {
		g_free (cdrdao->priv->src_output);
		cdrdao->priv->src_output = NULL;
	}

	if (output)
		cdrdao->priv->src_output = g_strdup (output);

	cdrdao->priv->overwrite = overwrite;
	cdrdao->priv->clean = clean;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_cdrdao_set_output_type (BraseroImager *imager,
				BraseroTrackSourceType type,
				BraseroImageFormat format,
				GError **error)
{
	BraseroCdrdao *cdrdao;

	cdrdao = BRASERO_CDRDAO (imager);

	if (type != BRASERO_TRACK_SOURCE_DEFAULT
	&&  type != BRASERO_TRACK_SOURCE_IMAGE)
		BRASERO_JOB_NOT_SUPPORTED (cdrdao);

	if (!(format & (BRASERO_IMAGE_FORMAT_CDRDAO|BRASERO_IMAGE_FORMAT_CUE)))
		BRASERO_JOB_NOT_SUPPORTED (cdrdao);

	/* no need to keep the type since it can only be an image */
	cdrdao->priv->format = format;
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_cdrdao_toc2cue (BraseroCdrdao *cdrdao,
			const BraseroTrackSource *toc_track,
			BraseroTrackSource **cue_track,
			GError **error)
{
	BraseroImager *imager;
	BraseroBurnResult result;

	imager = BRASERO_IMAGER (brasero_toc2cue_new ());
	brasero_job_set_slave (BRASERO_JOB (cdrdao), BRASERO_JOB (imager));
	g_object_unref (imager);

	result = brasero_job_set_source (BRASERO_JOB (imager),
					 toc_track,
					 error);
	if (result != BRASERO_BURN_OK)
		return result;

	result = brasero_imager_set_output (imager,
					    cdrdao->priv->src_output,
					    cdrdao->priv->overwrite,
					    cdrdao->priv->clean,
					    error);
	if (result != BRASERO_BURN_OK)
		return result;

	brasero_job_set_relay_slave_signals (BRASERO_JOB (cdrdao), TRUE);
	result = brasero_imager_get_track (imager,
					   cue_track,
					   error);
	brasero_job_set_relay_slave_signals (BRASERO_JOB (cdrdao), FALSE);

	if (result != BRASERO_BURN_OK)
		return result;

	brasero_job_set_slave (BRASERO_JOB (cdrdao), NULL);
	return result;
}

static BraseroBurnResult
brasero_cdrdao_get_track (BraseroImager *imager,
			  BraseroTrackSource **track,
			  GError **error)
{
	BraseroCdrdao *cdrdao;
	BraseroImageFormat format;
	BraseroTrackSource *retval;

	cdrdao = BRASERO_CDRDAO (imager);

	if (!cdrdao->priv->track)
		BRASERO_JOB_NOT_READY (cdrdao);

	if (cdrdao->priv->track->type != BRASERO_TRACK_SOURCE_DISC)
		BRASERO_JOB_NOT_SUPPORTED (cdrdao);

	if (!cdrdao->priv->track_ready) {
		BraseroBurnResult result;

		cdrdao->priv->action = BRASERO_CDRDAO_ACTION_IMAGE;
		result = brasero_job_run (BRASERO_JOB (imager), error);
		cdrdao->priv->action = BRASERO_CDRDAO_ACTION_NONE;

		if (result != BRASERO_BURN_OK)
			return result;

		cdrdao->priv->track_ready = 1;
	}

	retval = g_new0 (BraseroTrackSource, 1);

	retval->type = BRASERO_TRACK_SOURCE_IMAGE;
	retval->format = BRASERO_IMAGE_FORMAT_CDRDAO;

	/* the output given is the .cue file */
	retval->contents.image.toc = gnome_vfs_get_uri_from_local_path (cdrdao->priv->output);
	retval->contents.image.image = gnome_vfs_get_uri_from_local_path (cdrdao->priv->datafile);

	if (cdrdao->priv->format == BRASERO_IMAGE_FORMAT_ANY) {
		BraseroBurnCaps *caps;

		caps = brasero_burn_caps_get_default ();
		format = brasero_burn_caps_get_imager_default_format (caps,
								      cdrdao->priv->track);
		g_object_unref (caps);
	}
	else
		format = cdrdao->priv->format;

	if (format == BRASERO_IMAGE_FORMAT_CUE) {
		BraseroBurnResult result;

		result = brasero_cdrdao_toc2cue (cdrdao, retval, track, error);

		g_remove (retval->contents.image.toc);
		brasero_track_source_free (retval);
		return result;
	}

	*track = retval;
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_cdrdao_get_size_image (BraseroImager *imager,
			       gint64 *size,
			       gboolean sectors,
			       GError **error)
{
	BraseroCdrdao *cdrdao;

	cdrdao = BRASERO_CDRDAO (imager);

	if (cdrdao->priv->sectors_num < 1) {
		BraseroBurnResult result;

		if (brasero_job_is_running (BRASERO_JOB (imager)))
			return BRASERO_BURN_RUNNING;

		/* unfortunately cdrdao doesn't allow it beforehand 
		 * so we cheat here : we start cdrdao and stop it as
		 * soon as we get the size */
		cdrdao->priv->action = BRASERO_CDRDAO_ACTION_GET_SIZE;
		result = brasero_job_run (BRASERO_JOB (imager), error);
		cdrdao->priv->action = BRASERO_CDRDAO_ACTION_NONE;

		if (result != BRASERO_BURN_OK)
			return result;

		/* now we must remove the .toc and .bin in case they were created */
		brasero_cdrdao_clean_output (cdrdao);
	}

	if (sectors) {
		/* NOTE: 1 sec = 75 sectors, 1 sector = 2352 bytes */
		*size = cdrdao->priv->sectors_num;
	}
	else
		*size = cdrdao->priv->sectors_num * 2352;
	
	return BRASERO_BURN_OK;
}

static void
brasero_cdrdao_set_argv_device (BraseroCdrdao *cdrdao,
				GPtrArray *argv)
{
	g_ptr_array_add (argv, g_strdup ("--device"));
	if (NCB_DRIVE_GET_DEVICE (cdrdao->priv->drive))
		g_ptr_array_add (argv, g_strdup (NCB_DRIVE_GET_DEVICE (cdrdao->priv->drive)));
}

static void
brasero_cdrdao_set_argv_common_rec (BraseroCdrdao *cdrdao,
				    GPtrArray *argv)
{
	gchar *speed_str;

	g_ptr_array_add (argv, g_strdup ("--speed"));
	speed_str = g_strdup_printf ("%d", cdrdao->priv->rate);
	g_ptr_array_add (argv, speed_str);

	if (cdrdao->priv->overburn)
		g_ptr_array_add (argv, g_strdup ("--overburn"));
	if (cdrdao->priv->multi)
		g_ptr_array_add (argv, g_strdup ("--multi"));
}

static void
brasero_cdrdao_set_argv_common (BraseroCdrdao *cdrdao,
				GPtrArray *argv)
{
	if (cdrdao->priv->dummy)
		g_ptr_array_add (argv, g_strdup ("--simulate"));

	/* cdrdao manual says it is a similar option to gracetime */
	if (cdrdao->priv->nograce)
		g_ptr_array_add (argv, g_strdup ("-n"));

	g_ptr_array_add (argv, g_strdup ("-v"));
	g_ptr_array_add (argv, g_strdup ("2"));
}

static BraseroBurnResult
brasero_cdrdao_set_argv_record (BraseroCdrdao *cdrdao,
				GPtrArray *argv)
{
	BraseroTrackSource *track;

	if (!cdrdao->priv->drive)
		BRASERO_JOB_NOT_READY (cdrdao);

	track = cdrdao->priv->track;
	if (!track)
		BRASERO_JOB_NOT_READY (cdrdao);

        if (track->type == BRASERO_TRACK_SOURCE_DISC) {
		NautilusBurnDrive *source;

		g_ptr_array_add (argv, g_strdup ("copy"));
		brasero_cdrdao_set_argv_device (cdrdao, argv);
		brasero_cdrdao_set_argv_common (cdrdao, argv);
		brasero_cdrdao_set_argv_common_rec (cdrdao, argv);

		source = cdrdao->priv->track->contents.drive.disc;
		if (!nautilus_burn_drive_equal (source, cdrdao->priv->drive))
			g_ptr_array_add (argv, g_strdup ("--on-the-fly"));

		g_ptr_array_add (argv, g_strdup ("--source-device"));
		g_ptr_array_add (argv, g_strdup (NCB_DRIVE_GET_DEVICE (source)));
	}
	else if (track->type == BRASERO_TRACK_SOURCE_IMAGE) {
		gchar *cuepath;

		if (track->format & BRASERO_IMAGE_FORMAT_CUE)
			cuepath = brasero_track_source_get_cue_localpath (track);
		else if (track->format & BRASERO_IMAGE_FORMAT_CDRDAO)
			cuepath = brasero_track_source_get_cdrdao_localpath (track);
		else
			BRASERO_JOB_NOT_SUPPORTED (cdrdao);

		if (!cuepath)
			return BRASERO_BURN_ERR;

		g_ptr_array_add (argv, g_strdup ("write"));

		brasero_cdrdao_set_argv_device (cdrdao, argv);
		brasero_cdrdao_set_argv_common (cdrdao, argv);
		brasero_cdrdao_set_argv_common_rec (cdrdao, argv);

		g_ptr_array_add (argv, cuepath);
	}
	else
		BRASERO_JOB_NOT_SUPPORTED (cdrdao);

	BRASERO_JOB_TASK_SET_USE_AVERAGE_RATE (cdrdao, TRUE);
	BRASERO_JOB_TASK_SET_ACTION (cdrdao,
				     BRASERO_BURN_ACTION_PREPARING,
				     NULL,
				     FALSE);
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_cdrdao_set_argv_blank (BraseroCdrdao *cdrdao,
			       GPtrArray *argv)
{
	if (!cdrdao->priv->drive)
		BRASERO_JOB_NOT_READY (cdrdao);

	g_ptr_array_add (argv, g_strdup ("blank"));

	brasero_cdrdao_set_argv_device (cdrdao, argv);
	brasero_cdrdao_set_argv_common (cdrdao, argv);

	if (!cdrdao->priv->blank_fast) {
		g_ptr_array_add (argv, g_strdup ("--blank-mode"));
		g_ptr_array_add (argv, g_strdup ("full"));
	}

	BRASERO_JOB_TASK_SET_ACTION (cdrdao,
				     BRASERO_BURN_ACTION_ERASING,
				     NULL,
				     FALSE);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_cdrdao_set_argv_image (BraseroCdrdao *cdrdao,
			       GPtrArray *argv,
			       GError **error)
{
	gchar *output, *datafile;
	BraseroBurnResult result;
	NautilusBurnDrive *source;
	BraseroImageFormat format;

	if (!cdrdao->priv->track)
		BRASERO_JOB_NOT_READY (cdrdao);

	g_ptr_array_add (argv, g_strdup ("read-cd"));
	g_ptr_array_add (argv, g_strdup ("--device"));

	source = cdrdao->priv->track->contents.drive.disc;
	g_ptr_array_add (argv, g_strdup (NCB_DRIVE_GET_DEVICE (source)));

	g_ptr_array_add (argv, g_strdup ("--read-raw"));

	/* This is done so that if a cue file is required we first generate
	 * a temporary toc file that will be later converted to a cue file.
	 * The datafile is written where it should be from the start. */
	if (cdrdao->priv->format == BRASERO_IMAGE_FORMAT_ANY) {
		BraseroBurnCaps *caps;

		caps = brasero_burn_caps_get_default ();
		format = brasero_burn_caps_get_imager_default_format (caps,
								      cdrdao->priv->track);
		g_object_unref (caps);
	}
	else
		format = cdrdao->priv->format;

	if (cdrdao->priv->src_output
	&& (format & BRASERO_IMAGE_FORMAT_CDRDAO))
		output = g_strdup (cdrdao->priv->src_output);
	else
		output = NULL;

	if (cdrdao->priv->src_output)
		datafile = brasero_get_file_complement (format,
							FALSE,
							cdrdao->priv->src_output);
	else
		datafile = NULL;

	result = brasero_burn_common_check_output (&output,
						   BRASERO_IMAGE_FORMAT_CDRDAO,
						   FALSE,
						   cdrdao->priv->overwrite,
						   &datafile,
						   error);
	if (result != BRASERO_BURN_OK)
		return result;

	brasero_cdrdao_clean_output (cdrdao);
	cdrdao->priv->datafile = g_strdup (datafile);
	cdrdao->priv->output = g_strdup (output);

	if (cdrdao->priv->action == BRASERO_CDRDAO_ACTION_GET_SIZE) {
		BRASERO_JOB_TASK_SET_ACTION (cdrdao,
					     BRASERO_BURN_ACTION_GETTING_SIZE,
					     NULL,
					     FALSE);
		BRASERO_JOB_TASK_START_PROGRESS (cdrdao, FALSE);
	}

	g_ptr_array_add (argv, g_strdup ("--datafile"));
	g_ptr_array_add (argv, datafile);

	g_ptr_array_add (argv, g_strdup ("-v"));
	g_ptr_array_add (argv, g_strdup ("2"));

	g_ptr_array_add (argv, output);

	BRASERO_JOB_TASK_SET_USE_AVERAGE_RATE (cdrdao, TRUE);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_cdrdao_set_argv (BraseroProcess *process,
			 GPtrArray *argv,
			 gboolean has_master,
			 GError **error)
{
	BraseroCdrdao *cdrdao;

	cdrdao = BRASERO_CDRDAO (process);

	if (has_master)
		BRASERO_JOB_NOT_SUPPORTED (cdrdao);

	brasero_job_set_run_slave (BRASERO_JOB (cdrdao), FALSE);

	/* sets the first argv */
	g_ptr_array_add (argv, g_strdup ("cdrdao"));

	if (cdrdao->priv->action == BRASERO_CDRDAO_ACTION_RECORD)
		return brasero_cdrdao_set_argv_record (cdrdao, argv);
	else if (cdrdao->priv->action == BRASERO_CDRDAO_ACTION_BLANK)
		return brasero_cdrdao_set_argv_blank (cdrdao, argv);
	else if (cdrdao->priv->action == BRASERO_CDRDAO_ACTION_IMAGE)
		return brasero_cdrdao_set_argv_image (cdrdao, argv, error);
	else if (cdrdao->priv->action == BRASERO_CDRDAO_ACTION_GET_SIZE)
		return brasero_cdrdao_set_argv_image (cdrdao, argv, error);


	BRASERO_JOB_NOT_READY (cdrdao);
}

static BraseroBurnResult
brasero_cdrdao_post (BraseroProcess *process,
		     BraseroBurnResult retval)
{
	BraseroCdrdao *cdrdao;

	cdrdao = BRASERO_CDRDAO (process);

	if (retval == BRASERO_BURN_CANCEL)
		brasero_cdrdao_clean_output (cdrdao);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_cdrdao_set_drive (BraseroRecorder *recorder,
			  NautilusBurnDrive *drive,
			  GError **error)
{
	NautilusBurnMediaType media;
	BraseroCdrdao *cdrdao;

	cdrdao = BRASERO_CDRDAO (recorder);

	media = nautilus_burn_drive_get_media_type (drive);
	if (media > NAUTILUS_BURN_MEDIA_TYPE_CDRW)
		BRASERO_JOB_NOT_SUPPORTED (cdrdao);

	if (cdrdao->priv->drive) {
		nautilus_burn_drive_unref (cdrdao->priv->drive);
		cdrdao->priv->drive = NULL;
	}

	cdrdao->priv->drive = drive;
	nautilus_burn_drive_ref (drive);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_cdrdao_set_flags (BraseroRecorder *recorder,
			  BraseroRecorderFlag flags,
			  GError **error)
{
	BraseroCdrdao *cdrdao;

	cdrdao = BRASERO_CDRDAO (recorder);

	cdrdao->priv->multi = (flags & BRASERO_RECORDER_FLAG_MULTI);
	cdrdao->priv->dummy = (flags & BRASERO_RECORDER_FLAG_DUMMY);
	cdrdao->priv->dao = (flags & BRASERO_RECORDER_FLAG_DAO);
	cdrdao->priv->nograce = (flags & BRASERO_RECORDER_FLAG_NOGRACE);
	cdrdao->priv->burnproof = (flags & BRASERO_RECORDER_FLAG_BURNPROOF);
	cdrdao->priv->overburn = (flags & BRASERO_RECORDER_FLAG_OVERBURN);
	cdrdao->priv->blank_fast = (flags & BRASERO_RECORDER_FLAG_FAST_BLANK);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_cdrdao_set_rate (BraseroJob *job,
			 gint64 speed)
{
	BraseroCdrdao *cdrdao;

	if (brasero_job_is_running (job))
		return BRASERO_BURN_RUNNING;
	
	cdrdao = BRASERO_CDRDAO (job);
	cdrdao->priv->rate = speed / CDR_SPEED;

	return BRASERO_BURN_OK;

}

static BraseroBurnResult
brasero_cdrdao_record (BraseroRecorder *recorder,
		       GError **error)
{
	BraseroCdrdao *cdrdao;
	BraseroBurnResult result;

	cdrdao = BRASERO_CDRDAO (recorder);

	cdrdao->priv->action = BRASERO_CDRDAO_ACTION_RECORD;
	result = brasero_job_run (BRASERO_JOB (cdrdao), error);
	cdrdao->priv->action = BRASERO_CDRDAO_ACTION_NONE;

	return result;
}

static BraseroBurnResult
brasero_cdrdao_blank (BraseroRecorder *recorder,
		      GError **error)
{
	BraseroCdrdao *cdrdao;
	BraseroBurnResult result;

	cdrdao = BRASERO_CDRDAO (recorder);

	if (!nautilus_burn_drive_can_write (cdrdao->priv->drive)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the drive cannot rewrite CDs or DVDs"));
		return BRASERO_BURN_ERR;
	}

	cdrdao->priv->action = BRASERO_CDRDAO_ACTION_BLANK;
	result = brasero_job_run (BRASERO_JOB (cdrdao), error);
	cdrdao->priv->action = BRASERO_CDRDAO_ACTION_NONE;

	return result;
}

