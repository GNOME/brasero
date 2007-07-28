/***************************************************************************
 *            job.c
 *
 *  dim jan 22 10:40:26 2006
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
#include <errno.h>
#include <fcntl.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include "burn-basics.h"
#include "burn-debug.h"
#include "burn-session.h"
#include "burn-plugin-private.h"
#include "burn-mkisofs-base.h"
#include "burn-job.h"
#include "burn-task-ctx.h"
#include "burn-task-item.h"
#include "brasero-marshal.h"
#include "brasero-ncb.h"
#include "burn-medium.h"
 
static void brasero_job_iface_init_task_item (BraseroTaskItemIFace *iface);
G_DEFINE_TYPE_WITH_CODE (BraseroJob, brasero_job, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (BRASERO_TYPE_TASK_ITEM,
						brasero_job_iface_init_task_item));

typedef struct BraseroJobPrivate BraseroJobPrivate;
struct BraseroJobPrivate {
	BraseroJob *next;
	BraseroJob *previous;

	BraseroTaskCtx *ctx;
	BraseroTrackType type;

	union {
		struct {
			int in;
			int out;
		} fd;
		struct {
			gchar *image;
			gchar *toc;
		} file;
	} output;
};

#define BRASERO_JOB_DEBUG(job)	brasero_job_log_message (job, G_STRLOC,	\
				"%s called %s", 			\
				G_OBJECT_TYPE_NAME (job),		\
				G_STRFUNC);

#define BRASERO_JOB_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_JOB, BraseroJobPrivate))

enum {
	PROP_NONE,
	PROP_OUTPUT,
};

typedef enum {
	ERROR_SIGNAL,
	LAST_SIGNAL
} BraseroJobSignalType;

static guint brasero_job_signals [LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;


/**
 * Task item virtual functions implementation
 */

static BraseroTaskItem *
brasero_job_item_next (BraseroTaskItem *item)
{
	BraseroJob *self;
	BraseroJobPrivate *priv;

	self = BRASERO_JOB (item);
	priv = BRASERO_JOB_PRIVATE (self);

	if (!priv->next)
		return NULL;

	return BRASERO_TASK_ITEM (priv->next);
}

static BraseroTaskItem *
brasero_job_item_previous (BraseroTaskItem *item)
{
	BraseroJob *self;
	BraseroJobPrivate *priv;

	self = BRASERO_JOB (item);
	priv = BRASERO_JOB_PRIVATE (self);

	if (!priv->previous)
		return NULL;

	return BRASERO_TASK_ITEM (priv->previous);
}

static BraseroBurnResult
brasero_job_item_connect (BraseroTaskItem *input,
			  BraseroTaskItem *output)
{
	BraseroJobPrivate *priv_input;
	BraseroJobPrivate *priv_output;

	priv_input = BRASERO_JOB_PRIVATE (input);
	priv_output = BRASERO_JOB_PRIVATE (output);

	priv_input->next = BRASERO_JOB (output);
	priv_output->previous = BRASERO_JOB (input);

	g_object_ref (input);
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_job_set_output (BraseroJob *self,
			BraseroBurnSession *session,
			GError **error)
{
	BraseroBurnResult result;
	BraseroJobPrivate *priv;

	priv = BRASERO_JOB_PRIVATE (self);

	if (priv->next) {
		BraseroJobPrivate *next_priv;
		long flags = 0;
		int fd [2];

		BRASERO_BURN_LOG ("Creating pipe between %s and %s",
				  G_OBJECT_TYPE_NAME (self),
				  G_OBJECT_TYPE_NAME (priv->next));

		if (priv->output.fd.out) {
			/* This one's already connected */
			return BRASERO_BURN_OK;
		}

		/* There is another job set up pipes */
		if (pipe (fd)) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("the pipe couldn't be created (%s)"),
				     strerror (errno));
			return BRASERO_BURN_ERR;
		}

		if (fcntl (fd [0], F_GETFL, &flags) != -1) {
			flags |= O_NONBLOCK;
			if (fcntl (fd [0], F_SETFL, flags) == -1) {
				g_set_error (error,
					     BRASERO_BURN_ERROR,
					     BRASERO_BURN_ERROR_GENERAL,
					     _("couldn't set non blocking mode"));
				return BRASERO_BURN_ERR;
			}
		}
		else {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("couldn't get pipe flags"));
			return BRASERO_BURN_ERR;
		}

		flags = 0;
		if (fcntl (fd [1], F_GETFL, &flags) != -1) {
			flags |= O_NONBLOCK;
			if (fcntl (fd [1], F_SETFL, flags) == -1) {
				g_set_error (error,
					     BRASERO_BURN_ERROR,
					     BRASERO_BURN_ERROR_GENERAL,
					     _("couldn't set non blocking mode"));
				return BRASERO_BURN_ERR;
			}
		}
		else {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("couldn't get pipe flags"));
			return BRASERO_BURN_ERR;
		}

		priv->output.fd.out = fd [1];
		next_priv = BRASERO_JOB_PRIVATE (priv->next);
		next_priv->output.fd.in = fd [0];

		BRASERO_JOB_LOG (self, "Connected");
		return BRASERO_BURN_OK;
	}

	if (priv->type.type == BRASERO_TRACK_TYPE_DISC)
		return BRASERO_BURN_OK;

	/* clean */
	if (priv->output.file.image) {
		g_free (priv->output.file.image);
		priv->output.file.image = NULL;
	}

	if (priv->output.file.toc) {
		g_free (priv->output.file.toc);
		priv->output.file.toc = NULL;
	}

	/* no next job so we need a file */
	if (priv->type.type == BRASERO_TRACK_TYPE_IMAGE) {
		BraseroImageFormat format;

		format = brasero_burn_session_get_output_format (session);
		if (priv->type.subtype.img_format == format)
			result = brasero_burn_session_get_output (session,
								  &priv->output.file.image,
								  &priv->output.file.toc,
								  error);
		else
			result = brasero_burn_session_get_tmp_image (session,
								     priv->type.subtype.img_format,
								     &priv->output.file.image,
								     &priv->output.file.toc,
								     error);
		BRASERO_JOB_LOG (self, "Ouput set (IMAGE) image = %s toc = %s",
				 priv->output.file.image,
				 priv->output.file.toc);
	}
	else if (priv->type.type == BRASERO_TRACK_TYPE_AUDIO) {
		/* NOTE: this one can only a temporary file */
		result = brasero_burn_session_get_tmp_file (session,
							    &priv->output.file.image,
							    error);
		BRASERO_JOB_LOG (self, "Ouput set (AUDIO) image = %s",
				 priv->output.file.image);
	}
	else
		BRASERO_JOB_NOT_SUPPORTED (self);

	return result;
}

static BraseroBurnResult
brasero_job_item_init (BraseroTaskItem *item,
		       BraseroTaskCtx *ctx,
		       GError **error)
{
	BraseroJob *self;
	BraseroJobClass *klass;
	BraseroJobPrivate *priv;
	BraseroTaskAction action;
	BraseroBurnSession *session;
	BraseroBurnResult result = BRASERO_BURN_OK;

	self = BRASERO_JOB (item);
	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (ctx);

	action = brasero_task_ctx_get_action (ctx);

	if (!priv->next
	&&   priv->previous
	&&   action == BRASERO_TASK_ACTION_NONE
	&&   priv->type.type == BRASERO_TRACK_TYPE_DISC)
		return BRASERO_BURN_NOT_RUNNING;

	g_object_ref (ctx);
	priv->ctx = ctx;

	/* set the output for the job */
	if (action == BRASERO_JOB_ACTION_IMAGE
	&&  priv->type.type == BRASERO_TRACK_TYPE_IMAGE) {
		result = brasero_job_set_output (self, session, error);
		if (result != BRASERO_BURN_OK)
			return result;
	}

	klass = BRASERO_JOB_GET_CLASS (self);
	if (klass->init)
		result = klass->init (self, error);

	return result;
}

static BraseroBurnResult
brasero_job_item_start (BraseroTaskItem *item,
		        BraseroTaskCtx *ctx,
		        GError **error)
{
	BraseroJob *self;
	BraseroJobClass *klass;
	BraseroJobPrivate *priv;

	/* This function is compulsory */
	self = BRASERO_JOB (item);
	priv = BRASERO_JOB_PRIVATE (self);
	if (!priv->ctx)
		return BRASERO_BURN_OK;

	klass = BRASERO_JOB_GET_CLASS (self);
	if (!klass->start) {
		BRASERO_JOB_LOG (self, "no start method");
		BRASERO_JOB_NOT_SUPPORTED (self);
	}
	
	return klass->start (self, error);
}

static BraseroBurnResult
brasero_job_item_clock_tick (BraseroTaskItem *item,
			     BraseroTaskCtx *ctx,
			     GError **error)
{
	BraseroJob *self;
	BraseroJobClass *klass;
	BraseroJobPrivate *priv;
	BraseroBurnResult result = BRASERO_BURN_OK;

	self = BRASERO_JOB (item);
	priv = BRASERO_JOB_PRIVATE (self);
	if (!priv->ctx)
		return BRASERO_BURN_OK;

	klass = BRASERO_JOB_GET_CLASS (self);
	if (klass->clock_tick)
		result = klass->clock_tick (self);

	return result;
}

static BraseroBurnResult
brasero_job_disconnect (BraseroJob *self,
			GError **error)
{
	BraseroJobPrivate *priv;
	BraseroBurnResult result = BRASERO_BURN_OK;

	priv = BRASERO_JOB_PRIVATE (self);

	if (priv->next) {
		if (priv->output.fd.out > 0) {
			close (priv->output.fd.out);
			priv->output.fd.out = 0;
		}
	}
	else if (priv->type.type != BRASERO_TRACK_TYPE_DISC) {
		g_free (priv->output.file.image);
		priv->output.file.image = NULL;

		g_free (priv->output.file.toc);
		priv->output.file.toc = NULL;
	}

	if (priv->previous) {
		if (priv->output.fd.in) {
			close (priv->output.fd.in > 0);
			priv->output.fd.in = 0;
		}
	}

	return result;
}

static BraseroBurnResult
brasero_job_item_stop (BraseroTaskItem *item,
		       BraseroTaskCtx *ctx,
		       GError **error)
{
	BraseroJob *self;
	BraseroJobClass *klass;
	BraseroJobPrivate *priv;
	BraseroBurnResult result = BRASERO_BURN_OK;

	self = BRASERO_JOB (item);
	priv = BRASERO_JOB_PRIVATE (self);

	brasero_job_disconnect (self, error);

	if (!priv->ctx)
		return BRASERO_BURN_OK;

	klass = BRASERO_JOB_GET_CLASS (self);
	if (klass->stop)
		result = klass->stop (self, error);

	g_object_unref (priv->ctx);
	priv->ctx = NULL;

	return BRASERO_BURN_OK;
}

static void
brasero_job_iface_init_task_item (BraseroTaskItemIFace *iface)
{
	iface->next = brasero_job_item_next;
	iface->previous = brasero_job_item_previous;
	iface->connect = brasero_job_item_connect;
	iface->init = brasero_job_item_init;
	iface->start = brasero_job_item_start;
	iface->stop = brasero_job_item_stop;
	iface->clock_tick = brasero_job_item_clock_tick;
}

/**
 * Means a job successfully completed its task.
 * track can be NULL, depending on whether or not the job created a track.
 */

static gboolean
brasero_job_is_last_running (BraseroJob *self)
{
	BraseroJobPrivate *priv, *priv_next;

	priv = BRASERO_JOB_PRIVATE (self);
	if (!priv->next)
		return TRUE;

	priv_next = BRASERO_JOB_PRIVATE (priv->next);
	if (!priv_next->ctx)
		return TRUE;

	return FALSE;
}

static gboolean
brasero_job_is_first_running (BraseroJob *self)
{
	BraseroJobPrivate *priv, *priv_prev;

	priv = BRASERO_JOB_PRIVATE (self);
	if (!priv->previous)
		return TRUE;

	priv_prev = BRASERO_JOB_PRIVATE (priv->previous);
	if (!priv_prev->ctx)
		return TRUE;

	return FALSE;
}

BraseroBurnResult
brasero_job_finished (BraseroJob *self, BraseroTrack *track)
{
	GError *error = NULL;
	BraseroJobPrivate *priv;
	BraseroBurnResult result;

	priv = BRASERO_JOB_PRIVATE (self);

	BRASERO_JOB_LOG (self, "finished successfully %s", track? "(adding track)":"");

	if (brasero_job_is_first_running (self)) {
		BraseroJobClass *klass;

		/* call the stop method of the job since it's finished */ 
		klass = BRASERO_JOB_GET_CLASS (self);
		if (klass->stop) {
			result = klass->stop (self, &error);

			if (result != BRASERO_BURN_OK)
				return brasero_task_ctx_error (priv->ctx,
							       result,
							       error);
		}

		/* go to next track and see if there is one. If so, restart job */
		result = brasero_task_ctx_next_track (priv->ctx, track);
		if (result == BRASERO_BURN_RETRY)
			return BRASERO_BURN_OK;

		if (!brasero_job_is_last_running (self)) {
			/* this job is finished but it's not the leader so the
			 * task is not finished. Close the pipe on one side to
			 * let the next job know that there isn't any more data
			 * to be expected */
			result = brasero_job_disconnect (self, &error);
			if (result != BRASERO_BURN_OK)
				return brasero_task_ctx_error (priv->ctx,
							       result,
							       error);

			return BRASERO_BURN_OK;
		}
	}
	else if (!brasero_job_is_last_running (self)) {
		/* This job is apparently a go between job. It should only call
		 * for a stop on an error. */
		BRASERO_JOB_LOG (self, "is not a leader");
		error = g_error_new (BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("a plugin did not behave properly"));
		return brasero_task_ctx_error (priv->ctx, BRASERO_BURN_ERR, error);
	}

	return brasero_task_ctx_finished (priv->ctx, track);
}

/**
 * means a job didn't successfully completed its task
 */

BraseroBurnResult
brasero_job_error (BraseroJob *self, GError *error)
{
	GValue instance_and_params [2];
	BraseroJobPrivate *priv;
	GValue return_value;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);

	instance_and_params [0].g_type = 0;
	g_value_init (instance_and_params, G_TYPE_FROM_INSTANCE (self));
	g_value_set_instance (instance_and_params, self);

	instance_and_params [1].g_type = 0;
	g_value_init (instance_and_params + 1, G_TYPE_INT);
	g_value_set_int (instance_and_params + 1, error->code);

	return_value.g_type = 0;
	g_value_init (&return_value, G_TYPE_INT);
	g_value_set_int (&return_value, BRASERO_BURN_ERR);

	/* There was an error: signal it. That's mainly done
	 * for BraseroBurnCaps to override the result value */
	g_signal_emitv (instance_and_params,
			brasero_job_signals [ERROR_SIGNAL],
			0,
			&return_value);

	g_value_unset (instance_and_params);

	BRASERO_JOB_LOG (self,
			 "asked to stop because of an error\n"
			 "\terror\t\t= %i\n"
			 "\tmessage\t= \"%s\"",
			 error ? error->code:0,
			 error ? error->message:"no message");

	return brasero_task_ctx_error (priv->ctx, g_value_get_int (&return_value), error);
}

/**
 * Used to retrieve config for a job
 * If the parameter is missing for the next 4 functions
 * it allows to test if they could be used
 */

BraseroBurnResult
brasero_job_get_fd_in (BraseroJob *self, int *fd_in)
{
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);

	if (!priv->previous)
		return BRASERO_BURN_ERR;

	if (!fd_in)
		return BRASERO_BURN_OK;

	*fd_in = priv->output.fd.in;
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_current_track (BraseroJob *self,
			       BraseroTrack **track)
{
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);
	if (priv->previous)
		return BRASERO_BURN_ERR;

	if (!track)
		return BRASERO_BURN_OK;

	return brasero_task_ctx_get_current_track (priv->ctx, track);
}

BraseroBurnResult
brasero_job_get_done_tracks (BraseroJob *self, GSList **tracks)
{
	BraseroJobPrivate *priv;
	BraseroBurnSession *session;

	BRASERO_JOB_DEBUG (self);

	g_return_val_if_fail (tracks != NULL, BRASERO_BURN_ERR);

	/* tracks already done are those that are in session */
	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (priv->ctx);
	*tracks = brasero_burn_session_get_tracks (session);
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_tracks (BraseroJob *self, GSList **tracks)
{
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	g_return_val_if_fail (tracks != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_JOB_PRIVATE (self);
	return brasero_task_ctx_get_tracks (priv->ctx, tracks);
}

BraseroBurnResult
brasero_job_get_fd_out (BraseroJob *self, int *fd_out)
{
	BraseroJobPrivate *priv;
	BraseroJobPrivate *priv_next;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);

	/* To have a pipe we need another job ... */
	if (!priv->next)
		return BRASERO_BURN_ERR;

	/* ... and it must be running as well */
	priv_next = BRASERO_JOB_PRIVATE (priv->next);
	if (!priv_next->ctx)
		return BRASERO_BURN_ERR;

	if (!fd_out)
		return BRASERO_BURN_OK;

	*fd_out = priv->output.fd.out;
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_image_output (BraseroJob *self,
			      gchar **image,
			      gchar **toc)
{
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);
	if (priv->next)
		return BRASERO_BURN_ERR;

	if (image)
		*image = g_strdup (priv->output.file.image);

	if (toc)
		*toc = g_strdup (priv->output.file.toc);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_audio_output (BraseroJob *self,
			      gchar **path)
{
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);
	if (priv->next)
		return BRASERO_BURN_ERR;

	if (path)
		*path = g_strdup (priv->output.file.image);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_flags (BraseroJob *self, BraseroBurnFlag *flags)
{
	BraseroBurnSession *session;
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	g_return_val_if_fail (flags != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (priv->ctx);
	*flags = brasero_burn_session_get_flags (session);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_input_type (BraseroJob *self, BraseroTrackType *type)
{
	BraseroBurnResult result;
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);
	if (!priv->previous) {
		BraseroBurnSession *session;

		session = brasero_task_ctx_get_session (priv->ctx);
		result = brasero_burn_session_get_input_type (session, type);
	}
	else {
		BraseroJobPrivate *prev_priv;

		prev_priv = BRASERO_JOB_PRIVATE (priv->previous);
		memcpy (type, &prev_priv->type, sizeof (BraseroTrackType));
		result = BRASERO_BURN_OK;
	}

	return result;
}

BraseroBurnResult
brasero_job_get_output_type (BraseroJob *self, BraseroTrackType *type)
{
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);

	memcpy (type, &priv->type, sizeof (BraseroTrackType));
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_action (BraseroJob *self, BraseroJobAction *action)
{
	BraseroJobPrivate *priv;
	BraseroTaskAction task_action;

	BRASERO_JOB_DEBUG (self);

	g_return_val_if_fail (action != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_JOB_PRIVATE (self);

	if (!brasero_job_is_last_running (self)) {
		*action = BRASERO_JOB_ACTION_IMAGE;
		return BRASERO_BURN_OK;
	}

	task_action = brasero_task_ctx_get_action (priv->ctx);
	switch (task_action) {
	case BRASERO_TASK_ACTION_NONE:
		*action = BRASERO_JOB_ACTION_SIZE;
		break;

	case BRASERO_TASK_ACTION_ERASE:
		*action = BRASERO_JOB_ACTION_ERASE;
		break;

	case BRASERO_TASK_ACTION_NORMAL:
		if (priv->type.type == BRASERO_TRACK_TYPE_IMAGE)
			*action = BRASERO_JOB_ACTION_IMAGE;
		else
			*action = BRASERO_JOB_ACTION_RECORD;
		break;

	case BRASERO_TASK_ACTION_CHECKSUM:
		*action = BRASERO_JOB_ACTION_CHECKSUM;
		break;

	default:
		*action = BRASERO_JOB_ACTION_NONE;
		break;
	}

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_device (BraseroJob *self, gchar **device)
{
	BraseroBurnSession *session;
	NautilusBurnDrive *drive;
	BraseroJobPrivate *priv;
	const gchar *path;

	BRASERO_JOB_DEBUG (self);

	g_return_val_if_fail (device != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (priv->ctx);

	drive = brasero_burn_session_get_burner (session);
	path = NCB_DRIVE_GET_DEVICE (drive);
	*device = g_strdup (path);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_media (BraseroJob *self, BraseroMedia *media)
{
	BraseroBurnSession *session;
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	g_return_val_if_fail (media != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (priv->ctx);
	*media = brasero_burn_session_get_dest_media (session);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_last_session_address (BraseroJob *self, guint64 *address)
{
	BraseroBurnSession *session;
	NautilusBurnDrive *drive;
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	g_return_val_if_fail (address != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (priv->ctx);
	drive = brasero_burn_session_get_burner (session);
	*address = NCB_MEDIA_GET_LAST_DATA_TRACK_ADDRESS (drive);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_next_writable_address (BraseroJob *self, guint64 *address)
{
	BraseroBurnSession *session;
	NautilusBurnDrive *drive;
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	g_return_val_if_fail (address != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (priv->ctx);
	drive = brasero_burn_session_get_burner (session);
	*address = NCB_MEDIA_GET_NEXT_WRITABLE_ADDRESS (drive);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_rate (BraseroJob *self, guint64 *rate)
{
	BraseroBurnSession *session;
	BraseroJobPrivate *priv;

	g_return_val_if_fail (rate != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (priv->ctx);
	*rate = brasero_burn_session_get_rate (session);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_speed (BraseroJob *self, guint *speed)
{
	BraseroBurnSession *session;
	BraseroJobPrivate *priv;
	BraseroMedia media;
	guint64 rate;

	BRASERO_JOB_DEBUG (self);

	g_return_val_if_fail (speed != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (priv->ctx);
	rate = brasero_burn_session_get_rate (session);

	media = brasero_burn_session_get_dest_media (session);
	if (media & BRASERO_MEDIUM_DVD)
		*speed = NAUTILUS_BURN_DRIVE_DVD_SPEED (rate);
	else 
		*speed = NAUTILUS_BURN_DRIVE_CD_SPEED (rate);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_max_rate (BraseroJob *self, guint64 *rate)
{
	BraseroBurnSession *session;
	NautilusBurnDrive *drive;
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	g_return_val_if_fail (rate != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (priv->ctx);

	drive = brasero_burn_session_get_burner (session);
	*rate = NCB_MEDIA_GET_MAX_WRITE_RATE  (drive);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_max_speed (BraseroJob *self, guint *speed)
{
	BraseroBurnSession *session;
	NautilusBurnDrive *drive;
	BraseroJobPrivate *priv;
	BraseroMedia media;
	guint64 rate;

	BRASERO_JOB_DEBUG (self);

	g_return_val_if_fail (speed != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (priv->ctx);

	drive = brasero_burn_session_get_burner (session);
	rate = NCB_MEDIA_GET_MAX_WRITE_RATE  (drive);
	media = NCB_MEDIA_GET_STATUS (drive);
	if (media & BRASERO_MEDIUM_DVD)
		*speed = NAUTILUS_BURN_DRIVE_DVD_SPEED (rate);
	else 
		*speed = NAUTILUS_BURN_DRIVE_CD_SPEED (rate);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_tmp_file (BraseroJob *self,
			  gchar **output,
			  GError **error)
{
	BraseroBurnSession *session;
	BraseroJobPrivate *priv;

	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (priv->ctx);
	brasero_burn_session_get_tmp_file (session,
					   output,
					   error);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_tmp_dir (BraseroJob *self,
			 gchar **output,
			 GError **error)
{
	BraseroBurnSession *session;
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (priv->ctx);
	brasero_burn_session_get_tmp_dir (session,
					  output,
					  error);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_audio_title (BraseroJob *self, gchar **album)
{
	BraseroBurnSession *session;
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	g_return_val_if_fail (album != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (priv->ctx);

	*album = g_strdup (brasero_burn_session_get_label (session));
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_data_label (BraseroJob *self, gchar **label)
{
	BraseroBurnSession *session;
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	g_return_val_if_fail (label != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (priv->ctx);

	*label = g_strdup (brasero_burn_session_get_label (session));
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_session_size (BraseroJob *self,
			      guint64 *blocks,
			      guint64 *size)
{
	BraseroBurnSession *session;
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (priv->ctx);
	return brasero_burn_session_get_size (session, blocks, size);
}

/**
 * Starts task internal timer 
 */

BraseroBurnResult
brasero_job_start_progress (BraseroJob *self,
			    gboolean force)
{
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);
	if (priv->next)
		return BRASERO_BURN_NOT_RUNNING;

	return brasero_task_ctx_start_progress (priv->ctx, force);
}

/**
 * these should be used to set the different values of the task by the jobs
 */

BraseroBurnResult
brasero_job_add_wrong_checksum (BraseroJob *self,
				const gchar *path)
{
	BraseroJobPrivate *priv;
	BraseroBurnSession *session;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (priv->ctx);

	brasero_burn_session_add_wrong_checksum (session, path);
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_set_progress (BraseroJob *self,
			  gdouble progress)
{
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);
	if (priv->next)
		return BRASERO_BURN_NOT_RUNNING;

	return brasero_task_ctx_set_progress (priv->ctx, progress);
}

BraseroBurnResult
brasero_job_set_current_action (BraseroJob *self,
				BraseroBurnAction action,
				const gchar *string,
				gboolean force)
{
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);
	if (!brasero_job_is_last_running (self))
		return BRASERO_BURN_NOT_RUNNING;

	return brasero_task_ctx_set_current_action (priv->ctx,
						    action,
						    string,
						    force);
}

BraseroBurnResult
brasero_job_get_current_action (BraseroJob *self,
				BraseroBurnAction *action)
{
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	g_return_val_if_fail (action != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_JOB_PRIVATE (self);
	return brasero_task_ctx_get_current_action (priv->ctx, action);
}

BraseroBurnResult
brasero_job_set_rate (BraseroJob *self,
		      gint64 rate)
{
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);
	if (priv->next)
		return BRASERO_BURN_NOT_RUNNING;

	return brasero_task_ctx_set_rate (priv->ctx, rate);
}

BraseroBurnResult
brasero_job_set_current_track_size (BraseroJob *self,
				    guint64 block_size,
				    guint64 sectors,
				    gint64 size)
{
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);
	if (!brasero_job_is_last_running (self))
		return BRASERO_BURN_ERR;

	return brasero_task_ctx_set_track_size (priv->ctx,
						block_size,
						sectors,
						size);
}

BraseroBurnResult
brasero_job_set_written (BraseroJob *self,
			 gint64 written)
{
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);
	if (priv->next)
		return BRASERO_BURN_NOT_RUNNING;

	return brasero_task_ctx_set_written (priv->ctx, written);
}

BraseroBurnResult
brasero_job_set_use_average_rate (BraseroJob *self, gboolean value)
{
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);
	if (priv->next)
		return BRASERO_BURN_NOT_RUNNING;

	return brasero_task_ctx_set_use_average (priv->ctx, value);
}

void
brasero_job_set_dangerous (BraseroJob *self, gboolean value)
{
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);
	brasero_task_ctx_set_dangerous (priv->ctx, value);
}

/**
 * used for debugging
 */

void
brasero_job_log_message (BraseroJob *self,
			 const gchar *location,
			 const gchar *format,
			 ...)
{
	va_list arg_list;
	BraseroJobPrivate *priv;
	BraseroBurnSession *session;

	g_return_if_fail (BRASERO_IS_JOB (self));
	g_return_if_fail (format != NULL);

	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (priv->ctx);

	va_start (arg_list, format);
	brasero_burn_session_logv (session, format, arg_list);
	brasero_burn_debug_messagev (location, format, arg_list);
	va_end (arg_list);
}

/**
 * Object creation stuff
 */

static void
brasero_job_get_property (GObject *object,
			  guint prop_id,
			  GValue *value,
			  GParamSpec *pspec)
{
	BraseroJobPrivate *priv;
	BraseroTrackType *ptr;

	priv = BRASERO_JOB_PRIVATE (object);

	switch (prop_id) {
	case PROP_OUTPUT:
		ptr = g_value_get_pointer (value);
		memcpy (ptr, &priv->type, sizeof (BraseroTrackType));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
brasero_job_set_property (GObject *object,
			  guint prop_id,
			  const GValue *value,
			  GParamSpec *pspec)
{
	BraseroJobPrivate *priv;
	BraseroTrackType *ptr;

	priv = BRASERO_JOB_PRIVATE (object);

	switch (prop_id) {
	case PROP_OUTPUT:
		ptr = g_value_get_pointer (value);
		if (!ptr) {
			priv->type.type = BRASERO_TRACK_TYPE_NONE;
			priv->type.subtype.media = BRASERO_MEDIUM_NONE;
		}
		else
			memcpy (&priv->type, ptr, sizeof (BraseroTrackType));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
brasero_job_finalize (GObject *object)
{
	BraseroJobPrivate *priv;

	priv = BRASERO_JOB_PRIVATE (object);

	if (priv->ctx) {
		g_object_unref (priv->ctx);
		priv->ctx = NULL;
	}

	if (priv->previous) {
		g_object_unref (priv->previous);
		priv->previous = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_job_class_init (BraseroJobClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroJobPrivate));

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_job_finalize;
	object_class->set_property = brasero_job_set_property;
	object_class->get_property = brasero_job_get_property;

	brasero_job_signals [ERROR_SIGNAL] =
	    g_signal_new ("error",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  G_STRUCT_OFFSET (BraseroJobClass, error),
			  NULL, NULL,
			  brasero_marshal_INT__INT,
			  G_TYPE_INT, 1, G_TYPE_INT);

	g_object_class_install_property (object_class,
					 PROP_OUTPUT,
					 g_param_spec_pointer ("output",
							       "The type the job must output",
							       "The type the job must output",
							       G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY));
}

static void
brasero_job_init (BraseroJob *obj)
{ }
