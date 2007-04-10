/***************************************************************************
 *            burn-libburn-common.c
 *
 *  mer aoû 30 16:35:40 2006
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

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <string.h>

#include "burn-basics.h"
#include "burn-job.h"
#include "burn-libburn-common.h"
#include "brasero-ncb.h"

#ifdef HAVE_LIBBURN

#include <libburn/libburn.h>

static void brasero_libburn_common_class_init (BraseroLibburnCommonClass *klass);
static void brasero_libburn_common_init (BraseroLibburnCommon *sp);
static void brasero_libburn_common_finalize (GObject *object);

static BraseroBurnResult
brasero_libburn_common_start (BraseroJob *job,
			      int in_fd,
			      int *out_fd,
			      GError **error);
static BraseroBurnResult
brasero_libburn_common_stop (BraseroJob *job,
			     BraseroBurnResult retval,
			     GError **error);

struct _BraseroLibburnCommonPrivate {
	struct burn_drive_info *drive_info;
	struct burn_drive *drive;
	struct burn_disc *disc;

	enum burn_drive_status status;

	gint clock_id;

	/* used detect track hops */
	gint track_num;

	/* used to report current written sector */
	gint64 sectors;
	gint64 cur_sector;
	gint64 track_sectors;

	gint has_leadin;
};

static BraseroJobClass *parent_class = NULL;

GType
brasero_libburn_common_get_type()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroLibburnCommonClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_libburn_common_class_init,
			NULL,
			NULL,
			sizeof (BraseroLibburnCommon),
			0,
			(GInstanceInitFunc)brasero_libburn_common_init,
		};

		type = g_type_register_static (BRASERO_TYPE_JOB, 
					       "BraseroLibburnCommon",
					       &our_info,
					       0);
	}

	return type;
}

static void
brasero_libburn_common_class_init (BraseroLibburnCommonClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroJobClass *job_class = BRASERO_JOB_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_libburn_common_finalize;

	job_class->start = brasero_libburn_common_start;
	job_class->stop = brasero_libburn_common_stop;
}

static void
brasero_libburn_common_init (BraseroLibburnCommon *obj)
{
	obj->priv = g_new0 (BraseroLibburnCommonPrivate, 1);
	//burn_set_verbosity (666);
}

static void
brasero_libburn_common_stop_real (BraseroLibburnCommon *self)
{
	if (self->priv->drive) {
		enum burn_drive_status status;

		status = burn_drive_get_status (self->priv->drive, NULL);
		if (status == BURN_DRIVE_READING || status == BURN_DRIVE_WRITING) {
			burn_drive_cancel (self->priv->drive);
			status = burn_drive_get_status (self->priv->drive, NULL);
		}

		/* wait for the drive to be idle */
		while (status != BURN_DRIVE_IDLE) {
			g_main_context_iteration (NULL, FALSE);
			status = burn_drive_get_status (self->priv->drive, NULL);
		}

		burn_drive_release (self->priv->drive, 0);
	}
}

static void
brasero_libburn_common_finalize (GObject *object)
{
	BraseroLibburnCommon *cobj;

	cobj = BRASERO_LIBBURN_COMMON (object);

	brasero_libburn_common_stop_real (cobj);

	if (cobj->priv->disc) {
		burn_disc_free (cobj->priv->disc);
		cobj->priv->disc = NULL;
	}

	if (cobj->priv->drive_info) {
		burn_drive_info_free (cobj->priv->drive_info);
		cobj->priv->drive_info = NULL;
		cobj->priv->drive = NULL;
	}	

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

BraseroLibburnCommon *
brasero_libburn_common_new ()
{
	BraseroLibburnCommon *obj;
	
	obj = BRASERO_LIBBURN_COMMON (g_object_new(BRASERO_TYPE_LIBBURN_COMMON, NULL));
	
	return obj;
}

BraseroBurnResult
brasero_libburn_common_set_drive (BraseroLibburnCommon *self,
				  NautilusBurnDrive *drive,
				  GError **error)
{
	gchar libburn_device [BURN_DRIVE_ADR_LEN];
	BraseroMediumInfo media;
	int res;

	if (self->priv->drive_info) {
		burn_drive_info_free (self->priv->drive_info);
		self->priv->drive_info = NULL;
		self->priv->drive = NULL;
	}

	if (!drive)
		return BRASERO_BURN_OK;

	/* libburn now supports DVD+/-R(W) but DL */
	media = NCB_MEDIA_GET_STATUS (drive);
	if (media & BRASERO_MEDIUM_DL)
		BRASERO_JOB_NOT_SUPPORTED (self);

	/* we just want to scan the drive proposed by NCB drive */
	res = burn_drive_convert_fs_adr ((gchar*) NCB_DRIVE_GET_DEVICE (drive), libburn_device);
	if (res <= 0)
		BRASERO_JOB_NOT_SUPPORTED (self);

	res = burn_drive_scan_and_grab (&self->priv->drive_info, libburn_device, 0);
	if (res <= 0) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the drive couldn't be initialized"));
		return BRASERO_BURN_ERR;
	}

	self->priv->drive = self->priv->drive_info->drive;
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_libburn_common_get_drive (BraseroLibburnCommon *self,
				  struct burn_drive **drive)
{
	g_return_val_if_fail (drive != NULL, BRASERO_BURN_ERR);

	*drive = self->priv->drive;
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_libburn_common_set_disc (BraseroLibburnCommon *self,
				 struct burn_disc *disc)
{
	if (self->priv->disc)
		burn_disc_free (self->priv->disc);

	self->priv->disc = disc;
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_libburn_common_get_disc (BraseroLibburnCommon *self,
				 struct burn_disc **disc)
{
	*disc = self->priv->disc;
	return BRASERO_BURN_OK;
}

static gboolean
brasero_libburn_common_process_message (BraseroLibburnCommon *self)
{
	int ret;
	GError *error;
	int err_code = 0;
	int err_errno = 0;
	char err_sev [80];
	char err_txt [BURN_MSGS_MESSAGE_LEN] = {0};

	ret = burn_msgs_obtain ("FATAL",
				&err_code,
				err_txt,
				&err_errno,
				err_sev);

	if (ret == 0)
	        return TRUE;

	if (ret < 0) {
		error = g_error_new (BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     err_txt);
		brasero_job_error (BRASERO_JOB (self), error);
		return FALSE;
	}

	BRASERO_JOB_LOG (self,
			 _("(%s) libburn tried to say something"),
		         err_txt);
	return TRUE;
}

static void
brasero_libburn_common_status_changed (BraseroLibburnCommon *self,
				       enum burn_drive_status status,
				       struct burn_progress *progress)
{
	BraseroBurnAction action = BRASERO_BURN_ACTION_NONE;

	switch (status) {
		case BURN_DRIVE_WRITING:
			/* we ignore it if it happens after leadout */
			if (self->priv->status == BURN_DRIVE_WRITING_LEADOUT
			||  self->priv->status == BURN_DRIVE_CLOSING_TRACK)
				return;

			if (self->priv->status == BURN_DRIVE_WRITING_LEADIN
			||  self->priv->status == BURN_DRIVE_WRITING_PREGAP) {
				self->priv->sectors += self->priv->track_sectors;
				self->priv->track_sectors = progress->sectors;
				self->priv->track_num = progress->track;
			}

			action = BRASERO_BURN_ACTION_WRITING;
			brasero_job_set_dangerous (BRASERO_JOB (self), TRUE);
			break;

		case BURN_DRIVE_WRITING_LEADIN:		/* DAO */
		case BURN_DRIVE_WRITING_PREGAP:		/* TAO */
			self->priv->has_leadin = 1;
			action = BRASERO_BURN_ACTION_PREPARING;
			brasero_job_set_dangerous (BRASERO_JOB (self), FALSE);
			break;

		case BURN_DRIVE_WRITING_LEADOUT: 	/* DAO */
		case BURN_DRIVE_CLOSING_TRACK:		/* TAO */
			self->priv->sectors += self->priv->track_sectors;
			self->priv->track_sectors = progress->sectors;

			action = BRASERO_BURN_ACTION_FIXATING;
			brasero_job_set_dangerous (BRASERO_JOB (self), FALSE);
			break;

		case BURN_DRIVE_ERASING:
		case BURN_DRIVE_FORMATTING:
			action = BRASERO_BURN_ACTION_ERASING;
			brasero_job_set_dangerous (BRASERO_JOB (self), TRUE);
			break;

		case BURN_DRIVE_IDLE:
			brasero_job_finished (BRASERO_JOB (self));
			brasero_job_set_dangerous (BRASERO_JOB (self), FALSE);
			break;

		case BURN_DRIVE_SPAWNING:
			if (self->priv->status == BURN_DRIVE_IDLE)
				action = BRASERO_BURN_ACTION_PREPARING;
			else
				action = BRASERO_BURN_ACTION_FIXATING;
			brasero_job_set_dangerous (BRASERO_JOB (self), FALSE);
			break;

		case BURN_DRIVE_READING:
			action = BRASERO_BURN_ACTION_DRIVE_COPY;
			brasero_job_set_dangerous (BRASERO_JOB (self), FALSE);
			break;

		default:
			return;
	}

	self->priv->status = status;
	BRASERO_JOB_TASK_SET_ACTION (self,
				     action,
				     NULL,
				     FALSE);
}

static gint64
brasero_libburn_common_get_session_size (BraseroLibburnCommon *self)
{
	struct burn_session **sessions;
	gint64 sectors = 0;
	int num_sessions;
	int i;

	sessions = burn_disc_get_sessions (self->priv->disc, &num_sessions);
	for (i = 0; i < num_sessions; i ++) {
		sectors += burn_session_get_sectors (sessions [i]);

		/* add the size for lead-out in case of raw */
//		if (self->priv->has_leadin)
//			sectors += (i == 0) ? 6750:2250;
	}

	/* add the size for lead-in in case of raw */
//	if (self->priv->has_leadin)
//		sectors += 11475;

	return sectors;
}

static void
brasero_libburn_common_clock_id (BraseroTask *task, BraseroLibburnCommon *self)
{
	gint64 cur_sector;
	gdouble fraction;
	gint64 sectors;

	enum burn_drive_status status;
	struct burn_progress progress;

	/* see if there is any pending message */
	if (!brasero_libburn_common_process_message (self))
		return;

	if (!self->priv->drive)
		return;

	status = burn_drive_get_status (self->priv->drive, &progress);

	/* FIXME! for some operations that libburn can't perform the drive stays
	 * idle and we've got no way to tell that kind of use case */
	if (self->priv->status != status)
		brasero_libburn_common_status_changed (self, status, &progress);

	if (status == BURN_DRIVE_IDLE
	||  status == BURN_DRIVE_SPAWNING
	||  !progress.sectors
	||  !progress.sector) {
		self->priv->sectors = 0;

		self->priv->track_num = progress.track;
		self->priv->track_sectors = progress.sectors;
		return;
	}

	if (status != BURN_DRIVE_ERASING
	&&  status != BURN_DRIVE_FORMATTING) {
		if (self->priv->track_num != progress.track) {
			gchar *string;

			self->priv->sectors += self->priv->track_sectors;
			self->priv->track_sectors = progress.sectors;
			self->priv->track_num = progress.track;

			string = g_strdup_printf (_("Writing track %02i"), progress.track);
			BRASERO_JOB_TASK_SET_ACTION (self,
						     BRASERO_BURN_ACTION_WRITING,
						     string,
						     TRUE);
			g_free (string);
		}

		cur_sector = progress.sector + self->priv->sectors;
		sectors = brasero_libburn_common_get_session_size (self);
		BRASERO_JOB_TASK_SET_WRITTEN (self, cur_sector * 2048);
		BRASERO_JOB_TASK_SET_TOTAL (self, sectors * 2048);
	}
	else {
		cur_sector = progress.sector;
		sectors = progress.sectors;
	}

	fraction = (gdouble) (cur_sector) /
		   (gdouble) (sectors);

	BRASERO_JOB_TASK_SET_PROGRESS (self, fraction);
	BRASERO_JOB_TASK_START_PROGRESS (self, FALSE);
}

static BraseroBurnResult
brasero_libburn_common_start (BraseroJob *job,
			      int in_fd,
			      int *out_fd,
			      GError **error)
{
	BraseroLibburnCommon *self;

	self = BRASERO_LIBBURN_COMMON (job);
	BRASERO_JOB_TASK_CONNECT_TO_CLOCK (self,
					   brasero_libburn_common_clock_id,
					   self->priv->clock_id);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_libburn_common_stop (BraseroJob *job,
			     BraseroBurnResult retval,
			     GError **error)
{
	BraseroLibburnCommon *self;

	self = BRASERO_LIBBURN_COMMON (job);

	BRASERO_JOB_TASK_DISCONNECT_FROM_CLOCK (self, self->priv->clock_id);

	return BRASERO_BURN_OK;
}

#endif /* HAVE_LIBBURN */
