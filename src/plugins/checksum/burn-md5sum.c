/***************************************************************************
 *            burn-sum.c
 *
 *  ven aoû  4 19:46:34 2006
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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/param.h>
#include <unistd.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <gmodule.h>

#include "burn-plugin.h"
#include "burn-job.h"
#include "burn-md5.h"
#include "burn-md5sum.h"
#include "burn-volume.h"
#include "brasero-ncb.h"

BRASERO_PLUGIN_BOILERPLATE (BraseroMd5sum, brasero_md5sum, BRASERO_TYPE_JOB, BraseroJob);

struct _BraseroMd5sumPrivate {
	BraseroMD5Ctx *ctx;
	BraseroMD5 md5;

	/* the path and fd for the file containing the md5 of files */
	gchar *sums_path;
	gint64 total;

	/* this is for the thread and the end of it */
	GThread *thread;
	gint end_id;

	guint cancel;
};
typedef struct _BraseroMd5sumPrivate BraseroMd5sumPrivate;

#define BRASERO_MD5SUM_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_MD5SUM, BraseroMd5sumPrivate))

static BraseroJobClass *parent_class = NULL;

static gint
brasero_md5sum_live_read (BraseroMd5sum *self,
			  int fd,
			  guchar *buffer,
			  gint bytes,
			  GError **error)
{
	gint total = 0;
	gint read_bytes;
	BraseroMd5sumPrivate *priv;

	priv = BRASERO_MD5SUM_PRIVATE (self);

	while (1) {
		read_bytes = read (fd, buffer + total, (bytes - total));

		/* maybe that's the end of the stream ... */
		if (!read_bytes)
			return total;

		if (priv->cancel)
			return -2;

		/* ... or an error =( */
		if (read_bytes == -1) {
			if (errno != EAGAIN && errno != EINTR) {
				g_set_error (error,
					     BRASERO_BURN_ERROR,
					     BRASERO_BURN_ERROR_GENERAL,
					     _("data could not be read from the pipe (%i: %s)"),
					     errno,
					     strerror (errno));
				return -1;
			}
		}
		else {
			total += read_bytes;

			if (total == bytes)
				return total;
		}

		g_usleep (500);
	}

	return total;
}

static BraseroBurnResult
brasero_md5sum_live_write (BraseroMd5sum *self,
			   int fd,
			   guchar *buffer,
			   gint bytes,
			   GError **error)
{
	gint bytes_remaining;
	gint bytes_written = 0;
	BraseroMd5sumPrivate *priv;

	priv = BRASERO_MD5SUM_PRIVATE (self);

	bytes_remaining = bytes;
	while (bytes_remaining) {
		gint written;

		written = write (fd,
				 buffer + bytes_written,
				 bytes_remaining);

		if (priv->cancel)
			return BRASERO_BURN_CANCEL;

		if (written != bytes_remaining) {
			if (errno != EINTR && errno != EAGAIN) {
				/* unrecoverable error */
				g_set_error (error,
					     BRASERO_BURN_ERROR,
					     BRASERO_BURN_ERROR_GENERAL,
					     _("the data couldn't be written to the pipe (%i: %s)"),
					     errno,
					     strerror (errno));
				return BRASERO_BURN_ERR;
			}
		}

		g_usleep (500);

		if (written > 0) {
			bytes_remaining -= written;
			bytes_written += written;
		}
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_md5sum_live (BraseroMd5sum *self,
		     GError **error)
{
	int fd_in = -1;
	int fd_out = -1;
	guint sum_bytes;
	gint read_bytes;
	guchar buffer [2048];
	BraseroBurnResult result;
	BraseroMd5sumPrivate *priv;

	priv = BRASERO_MD5SUM_PRIVATE (self);

	BRASERO_JOB_LOG (self, "starting md5 generation live");
	result = brasero_job_set_nonblocking (BRASERO_JOB (self), error);
	if (result != BRASERO_BURN_OK)
		return result;

	brasero_job_get_fd_in (BRASERO_JOB (self), &fd_in);
	brasero_job_get_fd_out (BRASERO_JOB (self), &fd_out);

	priv->ctx = brasero_md5_new ();
	brasero_md5_init (priv->ctx, &priv->md5);

	result = BRASERO_BURN_OK;
	while (1) {
		sum_bytes = 0;

		read_bytes = brasero_md5sum_live_read (self,
						       fd_in,
						       buffer,
						       sizeof (buffer),
						       error);
		if (read_bytes == -2) {
			result = BRASERO_BURN_CANCEL;
			goto end;
		}

		if (read_bytes == -1) {
			result = BRASERO_BURN_ERR;
			goto end;
		}

		if (!read_bytes)
			break;

		/* it can happen when we're just asked to generate a checksum
		 * that we don't need to output the received data */
		if (fd_out > 0
		&&  brasero_md5sum_live_write (self, fd_out, buffer, read_bytes, error) != BRASERO_BURN_OK)
			goto end;

		sum_bytes = brasero_md5_sum (priv->ctx,
					     &priv->md5,
					     buffer,
					     read_bytes);
		if (sum_bytes == -1) {
			result = BRASERO_BURN_CANCEL;
			goto end;
		}

		/* this could be a problem, disc recording is more important */
		if (sum_bytes)
			break;
	}

	brasero_md5_end (priv->ctx, &priv->md5, buffer + (read_bytes - sum_bytes), sum_bytes);

end:

	brasero_md5_free (priv->ctx);
	priv->ctx = NULL;

	return result;
}

static BraseroBurnResult
brasero_md5sum_image_live (BraseroMd5sum *self, GError **error)
{
	BraseroMd5sumPrivate *priv;
	BraseroBurnResult result;
	BraseroTrack *track;
	gchar *path;

	priv = BRASERO_MD5SUM_PRIVATE (self);

	if (brasero_job_get_fd_in (BRASERO_JOB (self), NULL) == BRASERO_BURN_OK)
		return brasero_md5sum_live (self, error);

	brasero_job_get_current_track (BRASERO_JOB (self), &track);
	path = brasero_track_get_image_source (track, FALSE);
	if (!path) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the image is not local"));
		return BRASERO_BURN_ERR;
	}

	result = brasero_track_get_image_size (track, NULL, NULL, &priv->total, error);
	if (result != BRASERO_BURN_OK)
		return result;

	brasero_job_set_current_action (BRASERO_JOB (self),
					BRASERO_BURN_ACTION_CHECKSUM,
					_("Creating image checksum"),
					FALSE);

	brasero_job_start_progress (BRASERO_JOB (self), FALSE);

	priv->ctx = brasero_md5_new ();
	result = brasero_md5_file (priv->ctx,
				   path,
				   &priv->md5,
				   0,
				   -1,
				   error);
	g_free (path);
	brasero_md5_free (priv->ctx);
	priv->ctx = NULL;

	return result;
}

static BraseroBurnResult
brasero_md5sum_image (BraseroMd5sum *self, GError **error)
{
	gchar *path;
	BraseroTrack *track;
	BraseroBurnResult result;
	BraseroMd5sumPrivate *priv;

	priv = BRASERO_MD5SUM_PRIVATE (self);
	brasero_job_get_current_track (BRASERO_JOB (self), &track);

	/* see if another plugin is sending us data to checksum */
	if (brasero_job_get_fd_in (BRASERO_JOB (self), NULL) == BRASERO_BURN_OK) {
		NautilusBurnDrive *drive;

		/* we're only able to checksum ISO format at the moment so that
		 * means we can only handle last session */
		drive = brasero_track_get_drive_source (track);
		NCB_MEDIA_GET_LAST_DATA_TRACK_SPACE (drive, &priv->total, NULL);

		BRASERO_JOB_LOG (self,
				 "Starting checksuming (live) (size = %i)",
				 priv->total);

		brasero_job_set_current_action (BRASERO_JOB (self),
						BRASERO_BURN_ACTION_CHECKSUM,
						_("Creating local image checksum"),
						FALSE);
		brasero_job_start_progress (BRASERO_JOB (self), FALSE);

		return brasero_md5sum_live (self, error);
	}

	/* get all needed information about the image */
	result = brasero_track_get_image_size (track,
					       NULL,
					       NULL,
					       &priv->total,
					       error);
	if (result != BRASERO_BURN_OK)
		return result;

	path = brasero_track_get_image_source (track, FALSE);

	/* simply open the file, read and checksum */
	BRASERO_JOB_LOG (self,
			 "Starting checksuming %s (size = %i)",
			 path,
			 priv->total);

	brasero_job_set_current_action (BRASERO_JOB (self),
					BRASERO_BURN_ACTION_CHECKSUM,
					_("Creating local image checksum"),
					FALSE);
	brasero_job_start_progress (BRASERO_JOB (self), FALSE);

	priv->ctx = brasero_md5_new ();
	result = brasero_md5_file (priv->ctx,
				   path,
				   &priv->md5,
				   0,
				   priv->total,
				   error);
	brasero_md5_free (priv->ctx);
	priv->ctx = NULL;
	g_free (path);

	return result;
}

struct _BraseroMd5sumThreadCtx {
	BraseroMd5sum *sum;
	BraseroBurnResult result;
	GError *error;
};
typedef struct _BraseroMd5sumThreadCtx BraseroMd5sumThreadCtx;

static gboolean
brasero_md5sum_end (gpointer data)
{
	BraseroMd5sum *self;
	BraseroTrack *track;
	BraseroTrackType input;
	BraseroJobAction action;
	BraseroBurnResult result;
	BraseroMd5sumPrivate *priv;
	BraseroMd5sumThreadCtx *ctx;
	gchar checksum [MD5_STRING_LEN + 1];

	ctx = data;
	self = ctx->sum;
	priv = BRASERO_MD5SUM_PRIVATE (self);

	/* NOTE ctx/data is destroyed in its own callback */
	priv->end_id = 0;

	if (ctx->result != BRASERO_BURN_OK) {
		GError *error;

		error = ctx->error;
		ctx->error = NULL;

		brasero_job_error (BRASERO_JOB (self), error);
		return FALSE;
	}

	brasero_job_get_action (BRASERO_JOB (self), &action);
	if (action == BRASERO_JOB_ACTION_CHECKSUM) {
		BraseroChecksumType type;

		/* we were asked to check the sum of the track so get the type
		 * of the checksum first to see what to do */
		track = NULL;
		brasero_job_get_current_track (BRASERO_JOB (self), &track);
		type = brasero_track_get_checksum_type (track);

		if (type == BRASERO_CHECKSUM_MD5_FILE) {
			/* in this case all was already set in session */
			brasero_job_finished_track (BRASERO_JOB (self));
			return FALSE;
		}

		/* DISC checking. Set the checksum for the track and at the same
		 * time compare it to a potential one */
		brasero_md5_string (&priv->md5, checksum);
		checksum [MD5_STRING_LEN] = '\0';

		BRASERO_JOB_LOG (self,
				 "setting new checksum (type = %i) %s (%s before)",
				 type,
				 checksum,
				 brasero_track_get_checksum (track));

		result = brasero_track_set_checksum (track,
						     BRASERO_CHECKSUM_MD5,
						     checksum);
		if (result != BRASERO_BURN_OK)
			goto error;

		brasero_job_finished_track (BRASERO_JOB (self));
		return FALSE;
	}

	/* we were asked to create a checksum. Its type depends on the input */
	brasero_job_get_input_type (BRASERO_JOB (self), &input);

	/* let's create a new DATA track with the md5 file created */
	if (input.type == BRASERO_TRACK_TYPE_DATA) {
		GSList *grafts;
		GSList *excluded;
		BraseroGraftPt *graft;
		BraseroTrackType type;
		GSList *new_grafts = NULL;

		/* for DATA track we add the file to the track */
		brasero_job_get_current_track (BRASERO_JOB (self), &track);
		brasero_track_get_type (track, &type);
		grafts = brasero_track_get_data_grafts_source (track);

		for (; grafts; grafts = grafts->next) {
			graft = grafts->data;
			graft = brasero_graft_point_copy (graft);
			new_grafts = g_slist_prepend (new_grafts, graft);
		}

		graft = g_new0 (BraseroGraftPt, 1);
		graft->uri = g_strconcat ("file://", priv->sums_path, NULL);
		graft->path = g_strdup ("/"BRASERO_CHECKSUM_FILE);
		new_grafts = g_slist_prepend (new_grafts, graft);

		track = brasero_track_new (BRASERO_TRACK_TYPE_DATA);
		brasero_track_add_data_fs (track, type.subtype.fs_type);

		excluded = brasero_track_get_data_excluded_source (track, TRUE);
		brasero_track_set_data_source (track, new_grafts, excluded);

		brasero_track_set_checksum (track,
					    BRASERO_CHECKSUM_MD5_FILE,
					    graft->uri);

		brasero_job_add_track (BRASERO_JOB (self), track);

		/* It's good practice to unref the track afterwards as we don't 
		 * need it anymore. BraseroTaskCtx refs it. */
		brasero_track_unref (track);

		brasero_job_finished_track (BRASERO_JOB (self));
		return FALSE;
	}
	else if (input.type == BRASERO_TRACK_TYPE_IMAGE) {
		track = NULL;
		brasero_job_get_current_track (BRASERO_JOB (self), &track);

		brasero_md5_string (&priv->md5, checksum);
		checksum [MD5_STRING_LEN] = '\0';

		BRASERO_JOB_LOG (self,
				 "setting new checksum %s (%s before)",
				 checksum,
				 brasero_track_get_checksum (track));

		result = brasero_track_set_checksum (track,
						     BRASERO_CHECKSUM_MD5,
						     checksum);
		if (result != BRASERO_BURN_OK)
			goto error;

		brasero_job_finished_track (BRASERO_JOB (self));
	}
	else
		goto error;

	return FALSE;

error:
{
	GError *error = NULL;

	error = g_error_new (BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_BAD_CHECKSUM,
			     _("some files may be corrupted on the disc"));
	brasero_job_error (BRASERO_JOB (self), error);
	return FALSE;
}
}

static void
brasero_md5sum_destroy (gpointer data)
{
	BraseroMd5sumThreadCtx *ctx;

	ctx = data;
	if (ctx->error) {
		g_error_free (ctx->error);
		ctx->error = NULL;
	}

	g_free (ctx);
}

static gpointer
brasero_md5sum_thread (gpointer data)
{
	BraseroMd5sum *self;
	GError *error = NULL;
	BraseroJobAction action;
	BraseroMd5sumPrivate *priv;
	BraseroMd5sumThreadCtx *ctx;
	BraseroBurnResult result = BRASERO_BURN_NOT_SUPPORTED;

	self = BRASERO_MD5SUM (data);
	priv = BRASERO_MD5SUM_PRIVATE (self);

	/* check DISC types and add checksums for DATA and IMAGE-bin types */
	brasero_job_get_action (BRASERO_JOB (self), &action);

	if (action == BRASERO_JOB_ACTION_CHECKSUM) {
		BraseroChecksumType type;
		BraseroTrack *track;

		brasero_job_get_current_track (BRASERO_JOB (self), &track);
		type = brasero_track_get_checksum_type (track);
		if (type == BRASERO_CHECKSUM_MD5)
			result = brasero_md5sum_image (self, &error);
		else
			result = BRASERO_BURN_ERR;
	}
	else if (action == BRASERO_JOB_ACTION_IMAGE) {
		BraseroTrackType type;

		brasero_job_get_input_type (BRASERO_JOB (self), &type);
		if (type.type == BRASERO_TRACK_TYPE_IMAGE)
			result = brasero_md5sum_image_live (self, &error);
		else
			result = BRASERO_BURN_ERR;
	}

	if (result != BRASERO_BURN_CANCEL) {
		ctx = g_new0 (BraseroMd5sumThreadCtx, 1);
		ctx->sum = self;
		ctx->error = error;
		ctx->result = result;
		priv->end_id = g_idle_add_full (G_PRIORITY_HIGH_IDLE,
						brasero_md5sum_end,
						ctx,
						brasero_md5sum_destroy);
	}

	priv->thread = NULL;
	g_thread_exit (NULL);
	return NULL;
}

static BraseroBurnResult
brasero_md5sum_start (BraseroJob *job,
		      GError **error)
{
	BraseroMd5sumPrivate *priv;
	BraseroJobAction action;

	brasero_job_get_action (job, &action);
	if (action == BRASERO_JOB_ACTION_SIZE)
		return BRASERO_BURN_NOT_SUPPORTED;

	/* we start a thread for the exploration of the graft points */
	priv = BRASERO_MD5SUM_PRIVATE (job);
	priv->thread = g_thread_create (brasero_md5sum_thread,
					BRASERO_MD5SUM (job),
					TRUE,
					error);

	if (!priv->thread)
		return BRASERO_BURN_ERR;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_md5sum_clock_tick (BraseroJob *job)
{
	BraseroMd5sumPrivate *priv;

	priv = BRASERO_MD5SUM_PRIVATE (job);
	if (!priv->ctx)
		return BRASERO_BURN_OK;

	if (priv->total) {
		gint64 written = 0;

		written = brasero_md5_get_written (priv->ctx);
		brasero_job_set_progress (job,
					  (gdouble) written /
					  (gdouble) priv->total);
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_md5sum_stop (BraseroJob *job,
		     GError **error)
{
	BraseroMd5sumPrivate *priv;

	priv = BRASERO_MD5SUM_PRIVATE (job);

	if (priv->ctx)
		brasero_md5_cancel (priv->ctx);

	if (priv->thread) {
		priv->cancel = 1;
		g_thread_join (priv->thread);
		priv->cancel = 0;
		priv->thread = NULL;
	}

	if (priv->end_id) {
		g_source_remove (priv->end_id);
		priv->end_id = 0;
	}

	if (priv->sums_path) {
		g_free (priv->sums_path);
		priv->sums_path = NULL;
	}

	return BRASERO_BURN_OK;
}

static void
brasero_md5sum_init (BraseroMd5sum *obj)
{ }

static void
brasero_md5sum_finalize (GObject *object)
{
	BraseroMd5sumPrivate *priv;
	
	priv = BRASERO_MD5SUM_PRIVATE (object);

	if (priv->thread) {
		priv->cancel = 1;
		g_thread_join (priv->thread);
		priv->cancel = 0;
		priv->thread = NULL;
	}

	if (priv->end_id) {
		g_source_remove (priv->end_id);
		priv->end_id = 0;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_md5sum_class_init (BraseroMd5sumClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroJobClass *job_class = BRASERO_JOB_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroMd5sumPrivate));

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_md5sum_finalize;

	job_class->start = brasero_md5sum_start;
	job_class->stop = brasero_md5sum_stop;
	job_class->clock_tick = brasero_md5sum_clock_tick;
}

static BraseroBurnResult
brasero_md5sum_export_caps (BraseroPlugin *plugin, gchar **error)
{
	GSList *input;

	brasero_plugin_define (plugin,
			       "md5sum",
			       _("allows to check data integrity on disc after it is burnt"),
			       "Philippe Rouquier",
			       0);

	/* For images we can process (thus generating a sum on the fly or simply
	 * test images. */
	input = brasero_caps_image_new (BRASERO_PLUGIN_IO_ACCEPT_FILE|
					BRASERO_PLUGIN_IO_ACCEPT_PIPE,
					BRASERO_IMAGE_FORMAT_BIN);
	brasero_plugin_process_caps (plugin, input);
	brasero_plugin_check_caps (plugin, BRASERO_CHECKSUM_MD5, input);
	g_slist_free (input);

	brasero_plugin_set_process_flags (plugin,
					  BRASERO_PLUGIN_RUN_FIRST|
					  BRASERO_PLUGIN_RUN_LAST);

	return BRASERO_BURN_OK;
}
