/***************************************************************************
 *            burn-libburn.c
 *
 *  lun aoû 21 14:33:24 2006
 *  Copyright  2006  Rouquier Philippe
 *  bonfire-app@wanadoo.fr
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include "burn-basics.h"
#include "burn-imager.h"
#include "burn-recorder.h"
#include "burn-job.h"
#include "burn-libburn-common.h"
#include "burn-libburn.h"

#include "scsi-get-configuration.h"

#ifdef HAVE_LIBBURN

#include <libburn/libburn.h>

static void brasero_libburn_class_init (BraseroLibburnClass *klass);
static void brasero_libburn_init (BraseroLibburn *sp);
static void brasero_libburn_finalize (GObject *object);

static void brasero_libburn_iface_init_record (BraseroRecorderIFace *iface);

static BraseroBurnResult
brasero_libburn_set_source (BraseroJob *job,
			    const BraseroTrackSource *source,
			    GError **error);
static BraseroBurnResult
brasero_libburn_set_drive (BraseroRecorder *recorder,
			   NautilusBurnDrive *drive,
			   GError **error);
static BraseroBurnResult
brasero_libburn_set_flags (BraseroRecorder *recorder,
			   BraseroRecorderFlag flags,
			   GError **error);
static BraseroBurnResult
brasero_libburn_set_rate (BraseroJob *job,
			  gint64 rate);

static BraseroBurnResult
brasero_libburn_pre_init (BraseroJob *job,
			  gboolean has_master,
			  GError **error);
static BraseroBurnResult
brasero_libburn_start (BraseroJob *job,
		       int in_fd,
		       int *out_fd,
		       GError **error);
static BraseroBurnResult
brasero_libburn_stop (BraseroJob *job,
		      BraseroBurnResult retval,
		      GError **error);

static BraseroBurnResult
brasero_libburn_record (BraseroRecorder *recorder,
			GError **error);
static BraseroBurnResult
brasero_libburn_blank (BraseroRecorder *recorder,
		       GError **error);

typedef enum {
	BRASERO_LIBBURN_ACTION_NONE,
	BRASERO_LIBBURN_ACTION_RECORD,
	BRASERO_LIBBURN_ACTION_ERASE
} BraseroLibburnAction;

struct _BraseroLibburnPrivate {
	BraseroLibburnAction action;

	BraseroTrackSource *source;
	BraseroTrackSource *infs;
	gint64 size;

	gint64 rate;

	GSList *tracks;

	gint burnproof:1;
	gint overburn:1;
	gint dummy:1;
	gint multi:1;
	gint dao:1;

	gint blank_fast:1;
};

static GObjectClass *parent_class = NULL;

GType
brasero_libburn_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroLibburnClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_libburn_class_init,
			NULL,
			NULL,
			sizeof (BraseroLibburn),
			0,
			(GInstanceInitFunc)brasero_libburn_init,
		};

		static const GInterfaceInfo recorder_info =
		{
			(GInterfaceInitFunc) brasero_libburn_iface_init_record,
			NULL,
			NULL
		};

		type = g_type_register_static (BRASERO_TYPE_LIBBURN_COMMON, 
					       "BraseroLibburn",
					       &our_info,
					       0);

		g_type_add_interface_static (type,
					     BRASERO_TYPE_RECORDER,
					     &recorder_info);
	}

	return type;
}

static void
brasero_libburn_class_init (BraseroLibburnClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroJobClass *job_class = BRASERO_JOB_CLASS (klass);

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_libburn_finalize;

	job_class->set_source = brasero_libburn_set_source;
	job_class->set_rate = brasero_libburn_set_rate;

	job_class->start_init = brasero_libburn_pre_init;
	job_class->start = brasero_libburn_start;
	job_class->stop = brasero_libburn_stop;
}

static void
brasero_libburn_iface_init_record (BraseroRecorderIFace *iface)
{
	iface->blank = brasero_libburn_blank;
	iface->record = brasero_libburn_record;
	iface->set_drive = brasero_libburn_set_drive;
	iface->set_flags = brasero_libburn_set_flags;
}

static void
brasero_libburn_init (BraseroLibburn *obj)
{
	obj->priv = g_new0 (BraseroLibburnPrivate, 1);
}

static void
brasero_libburn_stop_real (BraseroLibburn *self)
{
	brasero_job_set_slave (BRASERO_JOB (self), NULL);

	if (self->priv->infs) {
		brasero_track_source_free (self->priv->infs);
		self->priv->infs = NULL;
	}
}

static void
brasero_libburn_finalize (GObject *object)
{
	BraseroLibburn *cobj;

	cobj = BRASERO_LIBBURN(object);

	brasero_libburn_stop_real (cobj);

	if (cobj->priv->source) {
		brasero_track_source_free (cobj->priv->source);
		cobj->priv->source = NULL;
	}

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

BraseroLibburn *
brasero_libburn_new ()
{
	BraseroLibburn *obj;
	
	obj = BRASERO_LIBBURN (g_object_new (BRASERO_TYPE_LIBBURN, NULL));
	
	return obj;
}

static BraseroBurnResult
brasero_libburn_set_source (BraseroJob *job,
			    const BraseroTrackSource *source,
			    GError **error)
{
	BraseroLibburn *self;

	self = BRASERO_LIBBURN (job);

	if (source->type == BRASERO_TRACK_SOURCE_IMAGE) {
		if ((source->format & (BRASERO_IMAGE_FORMAT_ISO|BRASERO_IMAGE_FORMAT_CLONE)) == 0
		&&   source->format != BRASERO_IMAGE_FORMAT_NONE)
			BRASERO_JOB_NOT_SUPPORTED (self);
	}
	else if (source->type != BRASERO_TRACK_SOURCE_IMAGER
	     &&  source->type != BRASERO_TRACK_SOURCE_AUDIO)
		BRASERO_JOB_NOT_SUPPORTED (self);

	if (self->priv->source)
		brasero_track_source_free (self->priv->source);

	self->priv->source = brasero_track_source_copy (source);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_libburn_set_drive (BraseroRecorder *recorder,
			   NautilusBurnDrive *drive,
			   GError **error)
{
	BraseroLibburn *self;

	self = BRASERO_LIBBURN (recorder);
	brasero_libburn_common_set_drive (BRASERO_LIBBURN_COMMON (self),
					  drive,
					  error);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_libburn_set_flags (BraseroRecorder *recorder,
			   BraseroRecorderFlag flags,
			   GError **error)
{
	BraseroLibburn *self;

	self = BRASERO_LIBBURN (recorder);

	self->priv->dummy = (flags & BRASERO_RECORDER_FLAG_DUMMY) != 0;
	self->priv->dao = (flags & BRASERO_RECORDER_FLAG_DAO) != 0;
	self->priv->burnproof = (flags & BRASERO_RECORDER_FLAG_BURNPROOF) != 0;
	self->priv->overburn = (flags & BRASERO_RECORDER_FLAG_OVERBURN) != 0;
	self->priv->blank_fast = (flags & BRASERO_RECORDER_FLAG_FAST_BLANK) != 0;
	self->priv->multi = (flags & BRASERO_RECORDER_FLAG_MULTI) != 0;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_libburn_set_rate (BraseroJob *job,
			  gint64 rate)
{
	BraseroLibburn *self;

	self = BRASERO_LIBBURN (job);

	if (brasero_job_is_running (job))
		return BRASERO_BURN_RUNNING;

	self->priv->rate = rate;
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_libburn_add_track (struct burn_session *session,
			   struct burn_track *track,
			   struct burn_source *src,
			   gint mode,
			   GError **error)
{
	if (burn_track_set_source (track, src) != BURN_SOURCE_OK) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("unable to set the source"));
		return BRASERO_BURN_ERR;
	}

	if (!burn_session_add_track (session, track, BURN_POS_END)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("unable to add the track to the session"));
		return BRASERO_BURN_ERR;
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_libburn_add_file_track (struct burn_session *session,
				const gchar *path,
				gint mode,
				off_t size,
				GError **error)
{
	struct burn_source *src;
	struct burn_track *track;
	BraseroBurnResult result;

	track = burn_track_create ();
	burn_track_define_data (track, 0, 0, 0, mode);

	src = burn_file_source_new (path, NULL);
	result = brasero_libburn_add_track (session, track, src, mode, error);

	if (size > 0)
		burn_track_set_default_size (track, size);

	burn_source_free (src);
	burn_track_free (track);

	return result;
}

static BraseroBurnResult
brasero_libburn_add_fd_track (struct burn_session *session,
			      int fd,
			      gint mode,
			      gint64 size,
			      GError **error)
{
	struct burn_source *src;
	struct burn_track *track;
	BraseroBurnResult result;

	track = burn_track_create ();
	burn_track_define_data (track, 0, 0, 0, mode);

	src = burn_fd_source_new (fd, -1, size);
	result = brasero_libburn_add_track (session, track, src, mode, error);

	burn_source_free (src);
	burn_track_free (track);

	return result;
}

#define BRASERO_ASSERT_NO_SLAVE_RUNNING(in_fd, error)			\
	if (in_fd != -1) {						\
		g_set_error (error,					\
			     BRASERO_BURN_ERROR,			\
			     BRASERO_BURN_ERROR_GENERAL,		\
			     _("a slave tried to get connected"));	\
		return BRASERO_BURN_ERR;				\
	}

static BraseroBurnResult
brasero_libburn_pre_init (BraseroJob *job,
			  gboolean has_master,
			  GError **error)
{
	BraseroTrackSourceType output_type;
	BraseroImageFormat format;
	BraseroBurnResult result;
	BraseroImager *imager;
	BraseroLibburn *self;

	self = BRASERO_LIBBURN (job);

	/* this function is only implemented for the cases when the source is
	 * an imager from which we need to get some information before running
	 * which basically only happens in one case: audio on the fly */
	if (!self->priv->source
	||   self->priv->source->type != BRASERO_TRACK_SOURCE_IMAGER)
		return BRASERO_BURN_OK;

	imager = self->priv->source->contents.imager.obj;
	result = brasero_imager_get_track_type (imager, &output_type, &format);

	if (result != BRASERO_BURN_OK)
		return result;

	/* we have to do this here because in start method the slave will be in
	 * running state and so if the size hasn't been asked already it will 
	 * error out */
	if (output_type == BRASERO_TRACK_SOURCE_AUDIO) {
		/* we need to get the inf first */
		result = brasero_imager_set_output_type (imager,
							 BRASERO_TRACK_SOURCE_INF,
							 BRASERO_IMAGE_FORMAT_NONE,
							 error);
		
		if (result != BRASERO_BURN_OK)
			return result;

		brasero_job_set_slave (BRASERO_JOB (self), BRASERO_JOB (imager));
		brasero_job_set_relay_slave_signals (BRASERO_JOB (self), TRUE);

		result = brasero_imager_get_track (imager,
						   &self->priv->infs,
						   error);

		brasero_job_set_relay_slave_signals (BRASERO_JOB (self), FALSE);
		if (result != BRASERO_BURN_OK)
			return result;

		result = brasero_imager_set_output_type (imager,
							 BRASERO_TRACK_SOURCE_AUDIO,
							 BRASERO_IMAGE_FORMAT_NONE,
							 error);
	}
	else if (output_type == BRASERO_TRACK_SOURCE_IMAGE) {
		gint64 size;

		if ((format & (BRASERO_IMAGE_FORMAT_ISO|BRASERO_IMAGE_FORMAT_CLONE)) == 0)
			BRASERO_JOB_NOT_SUPPORTED (self);

		result = brasero_imager_get_size (imager,
						  &size,
						  FALSE,
						  error);
		self->priv->size = size;
	}
	else
		BRASERO_JOB_NOT_SUPPORTED (self);

	return result;
}

static BraseroBurnResult
brasero_libburn_setup_disc (BraseroLibburn *self, 
			    struct burn_disc **retval,
			    int in_fd,
			    GError **error)
{
	BraseroTrackSource *source;
	struct burn_session *session;
	struct burn_disc *disc;
	BraseroBurnResult result;

	if (!self->priv->source)
		BRASERO_JOB_NOT_READY (self);

	source = self->priv->source;

	/* set the source image */
	disc = burn_disc_create ();

	/* create the session */
	session = burn_session_create ();
	burn_disc_add_session (disc, session, BURN_POS_END);
	burn_session_free (session);

	/* create the track(s) */
	result = BRASERO_BURN_OK;
	if (source->type == BRASERO_TRACK_SOURCE_AUDIO) {
		GSList *iter;

		BRASERO_ASSERT_NO_SLAVE_RUNNING (in_fd, error);

		for (iter = source->contents.audio.infos; iter; iter = iter->next) {
			BraseroSongInfo *info;

			info = iter->data;

			result = brasero_libburn_add_file_track (session,
								 info->path,
								 BURN_AUDIO,
								 -1,
								 error);
			if (result != BRASERO_BURN_OK)
				break;
		}
	}
	else if (source->type == BRASERO_TRACK_SOURCE_IMAGE) {
		gchar *imagepath;
		gint mode;

		if ((source->format & (BRASERO_IMAGE_FORMAT_ISO)) == 0
		&&   source->format != BRASERO_IMAGE_FORMAT_NONE)
				       /*|BRASERO_IMAGE_FORMAT_CLONE) == 0)*/
			BRASERO_JOB_NOT_SUPPORTED (self);

		BRASERO_ASSERT_NO_SLAVE_RUNNING (in_fd, error);

		if (self->priv->source->format & BRASERO_IMAGE_FORMAT_ISO) {
			mode = BURN_MODE1;
			imagepath = brasero_track_source_get_image_localpath (source);
		}
		else if (self->priv->source->format == BRASERO_IMAGE_FORMAT_NONE) {
			mode = BURN_MODE1;
			imagepath = brasero_track_source_get_image_localpath (source);
		}
		else {
			mode = BURN_MODE1|BURN_MODE_RAW|BURN_SUBCODE_R96,
			imagepath = brasero_track_source_get_raw_localpath (source);
		}

		if (!imagepath)
			return BRASERO_BURN_ERR;

		result = brasero_libburn_add_file_track (session,
							 imagepath,
							 mode,
							 -1,
							 error);
	}
	else if (source->type == BRASERO_TRACK_SOURCE_IMAGER) {
		BraseroTrackSourceType output_type;
		BraseroImageFormat format;
		BraseroImager *imager;

		if (in_fd < 0) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("unable to connect to imager"));
			return BRASERO_BURN_ERR;
		}

		imager = source->contents.imager.obj;
		brasero_job_set_slave (BRASERO_JOB (self), BRASERO_JOB (imager));
		brasero_job_set_relay_slave_signals (BRASERO_JOB (self), FALSE);
		brasero_job_set_run_slave (BRASERO_JOB (self), TRUE);
	
		/* need to find out what type of track the imager will output */
		result = brasero_imager_get_track_type (imager,
							&output_type,
							&format);
		if (result != BRASERO_BURN_OK)
			return result;

		switch (output_type) {
			case BRASERO_TRACK_SOURCE_IMAGE:
			{
				gint mode;

				if ((format & (BRASERO_IMAGE_FORMAT_ISO)) == 0)
/*					       BRASERO_IMAGE_FORMAT_CLONE)) == 0)*/
					BRASERO_JOB_NOT_SUPPORTED (self);

				if (format & BRASERO_IMAGE_FORMAT_ISO)
					mode = BURN_MODE1;
				else
					mode = BURN_MODE1|BURN_MODE_RAW|BURN_SUBCODE_R96;

				result = brasero_libburn_add_fd_track (session,
								       in_fd,
								       mode,
								       self->priv->size,
								       error);
				break;
			}

			case BRASERO_TRACK_SOURCE_AUDIO:
			{
				GSList *iter;

				if (!self->priv->infs)
					BRASERO_JOB_NOT_READY (self);

				for (iter = self->priv->infs->contents.audio.infos; iter; iter = iter->next) {
					BraseroSongInfo *info;

					info = iter->data;

					/* we dup the descriptor so the same 
					 * will be shared by all tracks */
					result = brasero_libburn_add_fd_track (session,
									       dup (in_fd),
									       BURN_AUDIO,
									       info->sectors * 2352,
									       error);
					if (result != BRASERO_BURN_OK)
						return result;
				}

				/* since all the track have a copy of the descriptor
				 * we can safely close in_fd now */
				close (in_fd);
				break;
			}

			default:
				return BRASERO_BURN_NOT_SUPPORTED;
		}
	}
	else
		BRASERO_JOB_NOT_SUPPORTED (self);

	*retval = disc;
	return result;
}

static BraseroBurnResult
brasero_libburn_start_record (BraseroLibburn *self,
			      struct burn_drive *drive,
			      int in_fd,
			      GError **error)
{
	int profile;
	char prof_name [80];
	BraseroBurnResult result;
	struct burn_write_opts *opts;
	struct burn_disc *disc = NULL;

	if (burn_disc_get_profile (drive, &profile, prof_name) < 0) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("no profile available for the medium"));
		return BRASERO_BURN_ERR;
	}

	result = brasero_libburn_setup_disc (self, &disc, in_fd, error);
	if (result != BRASERO_BURN_OK)
		return result;

	brasero_libburn_common_set_disc (BRASERO_LIBBURN_COMMON (self),
					 disc);

	/* Note: we don't need to call burn_drive_get_status nor
	 * burn_disc_get_status since we took care of the disc
	 * checking thing earlier ourselves. Now there is a proper
	 * disc and tray is locked. */
	opts = burn_write_opts_new (drive);
	burn_write_opts_set_perform_opc (opts, 0);

	if (profile != BRASERO_SCSI_PROF_DVD_RW_RESTRICTED
	&&  profile != BRASERO_SCSI_PROF_DVD_RW_PLUS
	&&  self->priv->dao)
		burn_write_opts_set_write_type (opts,
						BURN_WRITE_SAO,
						BURN_BLOCK_SAO);
	else
		burn_write_opts_set_write_type (opts,
						BURN_WRITE_TAO,
						BURN_BLOCK_MODE1);

	burn_write_opts_set_multi (opts, self->priv->multi);
	burn_write_opts_set_underrun_proof (opts, self->priv->burnproof);
	burn_write_opts_set_simulate (opts, self->priv->dummy);

	burn_drive_set_speed (drive, self->priv->rate, 0);
	burn_disc_write (opts, disc);
	burn_write_opts_free (opts);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_libburn_start_erase (BraseroLibburn *self,
			     struct burn_drive *drive,
			     GError **error)
{
	char reasons [BURN_REASONS_LEN];
	struct burn_session *session;
	struct burn_write_opts *opts;
	BraseroBurnResult result;
	struct burn_disc *disc;
	char prof_name [80];
	int profile;
	int fd;

	if (burn_disc_get_profile (drive, &profile, prof_name) < 0) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("no profile available for the medium"));
		return BRASERO_BURN_ERR;
	}

	/* here we try to respect the current formatting of DVD-RW. For 
	 * overwritable media fast option means erase the first 64 Kib
	 * and long a forced reformatting */
	if (profile == BRASERO_SCSI_PROF_DVD_RW_RESTRICTED) {
		if (!self->priv->blank_fast) {
			/* leave libburn choose the best format */
			burn_disc_format (drive,
					  (off_t) 0,
					  (1 << 4));
			return BRASERO_BURN_OK;
		}
	}
	else if (profile == BRASERO_SCSI_PROF_DVD_RW_PLUS) {
		if (!self->priv->blank_fast) {
			/* Bit 2 is for format max available size
			 * Bit 4 is enforce (re)-format if needed
			 * 0x26 is DVD+RW format is to be set from bit 8
			 * in the latter case bit 7 needs to be set as 
			 * well.
			 */
			burn_disc_format (drive,
					  (off_t) 0,
					  (1 << 2)|(1 << 4));
			return BRASERO_BURN_OK;
		}
	}
	else if (burn_disc_erasable (drive)) {
		/* This is mainly for CDRW and sequential DVD-RW */
		burn_disc_erase (drive, self->priv->blank_fast);
		return BRASERO_BURN_OK;
	}
	else
		BRASERO_JOB_NOT_SUPPORTED (self);

	/* This is the "fast option": basically we only write 64 Kib of 0 from
	 * /dev/null */
	fd = open ("/dev/null", O_RDONLY);
	if (fd == -1) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("/dev/null can't be opened"));
		return BRASERO_BURN_ERR;
	}

	disc = burn_disc_create ();
	brasero_libburn_common_set_disc (BRASERO_LIBBURN_COMMON (self), disc);

	/* create the session */
	session = burn_session_create ();
	burn_disc_add_session (disc, session, BURN_POS_END);
	burn_session_free (session);

	result = brasero_libburn_add_fd_track (session,
					       fd,
					       BURN_MODE1,
					       65536,		/* 32 blocks */
					       error);
	opts = burn_write_opts_new (drive);
	burn_write_opts_set_perform_opc (opts, 0);
	burn_write_opts_set_underrun_proof (opts, 1);
	burn_write_opts_set_simulate (opts, self->priv->dummy);
	burn_drive_set_speed (drive, burn_drive_get_write_speed (drive), 0);
	burn_write_opts_set_write_type (opts,
					BURN_WRITE_TAO,
					BURN_BLOCK_MODE1);

	if (burn_precheck_write (opts, disc, reasons, 0) <= 0) {
		burn_write_opts_free (opts);
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("libburn can't burn: %s"), reasons);
		return BRASERO_BURN_ERR;
	}

	burn_disc_write (opts, disc);
	burn_write_opts_free (opts);

	return result;
}

static BraseroBurnResult
brasero_libburn_start (BraseroJob *job,
		       int in_fd,
		       int *out_fd,
		       GError **error)
{
	BraseroLibburn *self;
	BraseroBurnResult result;
	struct burn_drive *drive = NULL;

	self = BRASERO_LIBBURN (job);

	brasero_libburn_common_get_drive (BRASERO_LIBBURN_COMMON (self),
					  &drive);
	if (!drive)
		BRASERO_JOB_NOT_READY (self);

	if (self->priv->action == BRASERO_LIBBURN_ACTION_RECORD) {
		result = brasero_libburn_start_record (self,
						       drive,
						       in_fd,
						       error);
		if (result != BRASERO_BURN_OK)
			return result;

		BRASERO_JOB_TASK_SET_ACTION (self,
					     BRASERO_BURN_ACTION_PREPARING,
					     NULL,
					     FALSE);
	}
	else if (self->priv->action == BRASERO_LIBBURN_ACTION_ERASE) {
		result = brasero_libburn_start_erase (self,
						      drive,
						      error);
		if (result != BRASERO_BURN_OK)
			return result;

		BRASERO_JOB_TASK_SET_ACTION (self,
					     BRASERO_BURN_ACTION_ERASING,
					     NULL,
					     FALSE);
	}
	else
		BRASERO_JOB_NOT_READY (self);

	if (BRASERO_JOB_CLASS (parent_class)->start)
		BRASERO_JOB_CLASS (parent_class)->start (job,
							 in_fd,
							 out_fd,
							 error);
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_libburn_stop (BraseroJob *job,
		      BraseroBurnResult retval,
		      GError **error)
{
	BraseroLibburn *self;

	self = BRASERO_LIBBURN (job);

	brasero_libburn_stop_real (self);

	if (BRASERO_JOB_CLASS (parent_class)->stop)
		BRASERO_JOB_CLASS (parent_class)->stop (job,
							retval,
							error);
	return retval;
}

static BraseroBurnResult
brasero_libburn_record (BraseroRecorder *recorder,
			GError **error)
{
	BraseroLibburn *self;
	BraseroBurnResult result;

	self = BRASERO_LIBBURN (recorder);

	if (!self->priv->source)
		BRASERO_JOB_NOT_READY (self);

	/* set as slave if track is an imager (on the fly burning) */
	if (self->priv->source->type == BRASERO_TRACK_SOURCE_IMAGER) {
		BraseroJob *slave;

		slave = BRASERO_JOB (self->priv->source->contents.imager.obj);
		brasero_job_set_slave (BRASERO_JOB (self), slave);
		brasero_job_set_relay_slave_signals (BRASERO_JOB (self), FALSE);
		brasero_job_set_run_slave (BRASERO_JOB (self), TRUE);
	}
	else
		brasero_job_set_run_slave (BRASERO_JOB (self), FALSE);

	self->priv->action = BRASERO_LIBBURN_ACTION_RECORD;
	result = brasero_job_run (BRASERO_JOB (self), error);
	self->priv->action = BRASERO_LIBBURN_ACTION_NONE;

	return result;
}

static BraseroBurnResult
brasero_libburn_blank (BraseroRecorder *recorder,
		       GError **error)
{
	BraseroLibburn *self;
	BraseroBurnResult result;

	self = BRASERO_LIBBURN (recorder);

	self->priv->action = BRASERO_LIBBURN_ACTION_ERASE;
	result = brasero_job_run (BRASERO_JOB (self), error);
	self->priv->action = BRASERO_LIBBURN_ACTION_NONE;

	return result;
}

#endif /* HAVE_LIBBURN */
