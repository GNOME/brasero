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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/param.h>
#include <unistd.h>
#include <fcntl.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <gmodule.h>

#include "brasero-plugin-registration.h"
#include "burn-job.h"
#include "burn-volume.h"
#include "brasero-drive.h"
#include "brasero-track-disc.h"
#include "brasero-track-image.h"
#include "brasero-tags.h"


#define BRASERO_TYPE_CHECKSUM_IMAGE		(brasero_checksum_image_get_type ())
#define BRASERO_CHECKSUM_IMAGE(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_CHECKSUM_IMAGE, BraseroChecksumImage))
#define BRASERO_CHECKSUM_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_CHECKSUM_IMAGE, BraseroChecksumImageClass))
#define BRASERO_IS_CHECKSUM_IMAGE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_CHECKSUM_IMAGE))
#define BRASERO_IS_CHECKSUM_IMAGE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_CHECKSUM_IMAGE))
#define BRASERO_CHECKSUM_GET_CLASS(o)		(G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_CHECKSUM_IMAGE, BraseroChecksumImageClass))

BRASERO_PLUGIN_BOILERPLATE (BraseroChecksumImage, brasero_checksum_image, BRASERO_TYPE_JOB, BraseroJob);

struct _BraseroChecksumImagePrivate {
	GChecksum *checksum;
	BraseroChecksumType checksum_type;

	/* That's for progress reporting */
	goffset total;
	goffset bytes;

	/* this is for the thread and the end of it */
	GThread *thread;
	GMutex *mutex;
	GCond *cond;
	gint end_id;

	guint cancel;
};
typedef struct _BraseroChecksumImagePrivate BraseroChecksumImagePrivate;

#define BRASERO_CHECKSUM_IMAGE_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_CHECKSUM_IMAGE, BraseroChecksumImagePrivate))

#define BRASERO_SCHEMA_CONFIG		"org.gnome.brasero.config"
#define BRASERO_PROPS_CHECKSUM_IMAGE	"checksum-image"

static BraseroJobClass *parent_class = NULL;

static gint
brasero_checksum_image_read (BraseroChecksumImage *self,
			     int fd,
			     guchar *buffer,
			     gint bytes,
			     GError **error)
{
	gint total = 0;
	gint read_bytes;
	BraseroChecksumImagePrivate *priv;

	priv = BRASERO_CHECKSUM_IMAGE_PRIVATE (self);

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
                                int errsv = errno;

				g_set_error (error,
					     BRASERO_BURN_ERROR,
					     BRASERO_BURN_ERROR_GENERAL,
					     _("Data could not be read (%s)"),
					     g_strerror (errsv));
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
brasero_checksum_image_write (BraseroChecksumImage *self,
			      int fd,
			      guchar *buffer,
			      gint bytes,
			      GError **error)
{
	gint bytes_remaining;
	gint bytes_written = 0;
	BraseroChecksumImagePrivate *priv;

	priv = BRASERO_CHECKSUM_IMAGE_PRIVATE (self);

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
                                int errsv = errno;

				/* unrecoverable error */
				g_set_error (error,
					     BRASERO_BURN_ERROR,
					     BRASERO_BURN_ERROR_GENERAL,
					     _("Data could not be written (%s)"),
					     g_strerror (errsv));
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
brasero_checksum_image_checksum (BraseroChecksumImage *self,
				 GChecksumType checksum_type,
				 int fd_in,
				 int fd_out,
				 GError **error)
{
	gint read_bytes;
	guchar buffer [2048];
	BraseroBurnResult result;
	BraseroChecksumImagePrivate *priv;

	priv = BRASERO_CHECKSUM_IMAGE_PRIVATE (self);

	priv->checksum = g_checksum_new (checksum_type);
	result = BRASERO_BURN_OK;
	while (1) {
		read_bytes = brasero_checksum_image_read (self,
							  fd_in,
							  buffer,
							  sizeof (buffer),
							  error);
		if (read_bytes == -2)
			return BRASERO_BURN_CANCEL;

		if (read_bytes == -1)
			return BRASERO_BURN_ERR;

		if (!read_bytes)
			break;

		/* it can happen when we're just asked to generate a checksum
		 * that we don't need to output the received data */
		if (fd_out > 0) {
			result = brasero_checksum_image_write (self,
							       fd_out,
							       buffer,
							       read_bytes, error);
			if (result != BRASERO_BURN_OK)
				break;
		}

		g_checksum_update (priv->checksum,
				   buffer,
				   read_bytes);

		priv->bytes += read_bytes;
	}

	return result;
}

static BraseroBurnResult
brasero_checksum_image_checksum_fd_input (BraseroChecksumImage *self,
					  GChecksumType checksum_type,
					  GError **error)
{
	int fd_in = -1;
	int fd_out = -1;
	BraseroBurnResult result;
	BraseroChecksumImagePrivate *priv;

	priv = BRASERO_CHECKSUM_IMAGE_PRIVATE (self);

	BRASERO_JOB_LOG (self, "Starting checksum generation live (size = %" G_GOFFSET_FORMAT ")", priv->total);
	result = brasero_job_set_nonblocking (BRASERO_JOB (self), error);
	if (result != BRASERO_BURN_OK)
		return result;

	brasero_job_get_fd_in (BRASERO_JOB (self), &fd_in);
	brasero_job_get_fd_out (BRASERO_JOB (self), &fd_out);

	return brasero_checksum_image_checksum (self, checksum_type, fd_in, fd_out, error);
}

static BraseroBurnResult
brasero_checksum_image_checksum_file_input (BraseroChecksumImage *self,
					    GChecksumType checksum_type,
					    GError **error)
{
	BraseroChecksumImagePrivate *priv;
	BraseroBurnResult result;
	BraseroTrack *track;
	int fd_out = -1;
	int fd_in = -1;
	gchar *path;

	priv = BRASERO_CHECKSUM_IMAGE_PRIVATE (self);

	/* get all information */
	brasero_job_get_current_track (BRASERO_JOB (self), &track);
	path = brasero_track_image_get_source (BRASERO_TRACK_IMAGE (track), FALSE);
	if (!path) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_FILE_NOT_LOCAL,
			     _("The file is not stored locally"));
		return BRASERO_BURN_ERR;
	}

	BRASERO_JOB_LOG (self,
			 "Starting checksuming file %s (size = %"G_GOFFSET_FORMAT")",
			 path,
			 priv->total);

	fd_in = open (path, O_RDONLY);
	if (!fd_in) {
                int errsv;
		gchar *name = NULL;

		if (errno == ENOENT)
			return BRASERO_BURN_RETRY;

		name = g_path_get_basename (path);

                errsv = errno;

		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     /* Translators: first %s is the filename, second %s
			      * is the error generated from errno */
			     _("\"%s\" could not be opened (%s)"),
			     name,
			     g_strerror (errsv));
		g_free (name);
		g_free (path);

		return BRASERO_BURN_ERR;
	}

	/* and here we go */
	brasero_job_get_fd_out (BRASERO_JOB (self), &fd_out);
	result = brasero_checksum_image_checksum (self, checksum_type, fd_in, fd_out, error);
	g_free (path);
	close (fd_in);

	return result;
}

static BraseroBurnResult
brasero_checksum_image_create_checksum (BraseroChecksumImage *self,
					GError **error)
{
	BraseroBurnResult result;
	BraseroTrack *track = NULL;
	GChecksumType checksum_type;
	BraseroChecksumImagePrivate *priv;

	priv = BRASERO_CHECKSUM_IMAGE_PRIVATE (self);

	/* get the checksum type */
	switch (priv->checksum_type) {
		case BRASERO_CHECKSUM_MD5:
			checksum_type = G_CHECKSUM_MD5;
			break;
		case BRASERO_CHECKSUM_SHA1:
			checksum_type = G_CHECKSUM_SHA1;
			break;
		case BRASERO_CHECKSUM_SHA256:
			checksum_type = G_CHECKSUM_SHA256;
			break;
		default:
			return BRASERO_BURN_ERR;
	}

	brasero_job_set_current_action (BRASERO_JOB (self),
					BRASERO_BURN_ACTION_CHECKSUM,
					_("Creating image checksum"),
					FALSE);
	brasero_job_start_progress (BRASERO_JOB (self), FALSE);
	brasero_job_get_current_track (BRASERO_JOB (self), &track);

	/* see if another plugin is sending us data to checksum
	 * or if we do it ourself (and then that must be from an
	 * image file only). */
	if (brasero_job_get_fd_in (BRASERO_JOB (self), NULL) == BRASERO_BURN_OK) {
		BraseroMedium *medium;
		GValue *value = NULL;
		BraseroDrive *drive;
		guint64 start, end;
		goffset sectors;
		goffset bytes;

		brasero_track_tag_lookup (track,
					  BRASERO_TRACK_MEDIUM_ADDRESS_START_TAG,
					  &value);

		/* we were given an address to start */
		start = g_value_get_uint64 (value);

		/* get the length now */
		value = NULL;
		brasero_track_tag_lookup (track,
					  BRASERO_TRACK_MEDIUM_ADDRESS_END_TAG,
					  &value);

		end = g_value_get_uint64 (value);

		priv->total = end - start;

		/* we're only able to checksum ISO format at the moment so that
		 * means we can only handle last session */
		drive = brasero_track_disc_get_drive (BRASERO_TRACK_DISC (track));
		medium = brasero_drive_get_medium (drive);
		brasero_medium_get_last_data_track_space (medium,
							  &bytes,
							  &sectors);

		/* That's the only way to get the sector size */
		priv->total *= bytes / sectors;

		return brasero_checksum_image_checksum_fd_input (self, checksum_type, error);
	}
	else {
		result = brasero_track_get_size (track,
						 NULL,
						 &priv->total);
		if (result != BRASERO_BURN_OK)
			return result;

		return brasero_checksum_image_checksum_file_input (self, checksum_type, error);
	}

	return BRASERO_BURN_OK;
}

static BraseroChecksumType
brasero_checksum_get_checksum_type (void)
{
	GSettings *settings;
	GChecksumType checksum_type;

	settings = g_settings_new (BRASERO_SCHEMA_CONFIG);
	checksum_type = g_settings_get_int (settings, BRASERO_PROPS_CHECKSUM_IMAGE);
	g_object_unref (settings);

	return checksum_type;
}

static BraseroBurnResult
brasero_checksum_image_image_and_checksum (BraseroChecksumImage *self,
					   GError **error)
{
	BraseroBurnResult result;
	GChecksumType checksum_type;
	BraseroChecksumImagePrivate *priv;

	priv = BRASERO_CHECKSUM_IMAGE_PRIVATE (self);

	priv->checksum_type = brasero_checksum_get_checksum_type ();

	if (priv->checksum_type & BRASERO_CHECKSUM_MD5)
		checksum_type = G_CHECKSUM_MD5;
	else if (priv->checksum_type & BRASERO_CHECKSUM_SHA1)
		checksum_type = G_CHECKSUM_SHA1;
	else if (priv->checksum_type & BRASERO_CHECKSUM_SHA256)
		checksum_type = G_CHECKSUM_SHA256;
	else {
		checksum_type = G_CHECKSUM_MD5;
		priv->checksum_type = BRASERO_CHECKSUM_MD5;
	}

	brasero_job_set_current_action (BRASERO_JOB (self),
					BRASERO_BURN_ACTION_CHECKSUM,
					_("Creating image checksum"),
					FALSE);
	brasero_job_start_progress (BRASERO_JOB (self), FALSE);

	if (brasero_job_get_fd_in (BRASERO_JOB (self), NULL) != BRASERO_BURN_OK) {
		BraseroTrack *track;

		brasero_job_get_current_track (BRASERO_JOB (self), &track);
		result = brasero_track_get_size (track,
						 NULL,
						 &priv->total);
		if (result != BRASERO_BURN_OK)
			return result;

		result = brasero_checksum_image_checksum_file_input (self,
								     checksum_type,
								     error);
	}
	else
		result = brasero_checksum_image_checksum_fd_input (self,
								   checksum_type,
								   error);

	return result;
}

struct _BraseroChecksumImageThreadCtx {
	BraseroChecksumImage *sum;
	BraseroBurnResult result;
	GError *error;
};
typedef struct _BraseroChecksumImageThreadCtx BraseroChecksumImageThreadCtx;

static gboolean
brasero_checksum_image_end (gpointer data)
{
	BraseroChecksumImage *self;
	BraseroTrack *track;
	const gchar *checksum;
	BraseroBurnResult result;
	BraseroChecksumImagePrivate *priv;
	BraseroChecksumImageThreadCtx *ctx;

	ctx = data;
	self = ctx->sum;
	priv = BRASERO_CHECKSUM_IMAGE_PRIVATE (self);

	/* NOTE ctx/data is destroyed in its own callback */
	priv->end_id = 0;

	if (ctx->result != BRASERO_BURN_OK) {
		GError *error;

		error = ctx->error;
		ctx->error = NULL;

		g_checksum_free (priv->checksum);
		priv->checksum = NULL;

		brasero_job_error (BRASERO_JOB (self), error);
		return FALSE;
	}

	/* we were asked to check the sum of the track so get the type
	 * of the checksum first to see what to do */
	track = NULL;
	brasero_job_get_current_track (BRASERO_JOB (self), &track);

	/* Set the checksum for the track and at the same time compare it to a
	 * potential previous one. */
	checksum = g_checksum_get_string (priv->checksum);
	BRASERO_JOB_LOG (self,
			 "Setting new checksum (type = %i) %s (%s before)",
			 priv->checksum_type,
			 checksum,
			 brasero_track_get_checksum (track));
	result = brasero_track_set_checksum (track,
					     priv->checksum_type,
					     checksum);
	g_checksum_free (priv->checksum);
	priv->checksum = NULL;

	if (result != BRASERO_BURN_OK)
		goto error;

	brasero_job_finished_track (BRASERO_JOB (self));
	return FALSE;

error:
{
	GError *error = NULL;

	error = g_error_new (BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_BAD_CHECKSUM,
			     _("Some files may be corrupted on the disc"));
	brasero_job_error (BRASERO_JOB (self), error);
	return FALSE;
}
}

static void
brasero_checksum_image_destroy (gpointer data)
{
	BraseroChecksumImageThreadCtx *ctx;

	ctx = data;
	if (ctx->error) {
		g_error_free (ctx->error);
		ctx->error = NULL;
	}

	g_free (ctx);
}

static gpointer
brasero_checksum_image_thread (gpointer data)
{
	GError *error = NULL;
	BraseroJobAction action;
	BraseroTrack *track = NULL;
	BraseroChecksumImage *self;
	BraseroChecksumImagePrivate *priv;
	BraseroChecksumImageThreadCtx *ctx;
	BraseroBurnResult result = BRASERO_BURN_NOT_SUPPORTED;

	self = BRASERO_CHECKSUM_IMAGE (data);
	priv = BRASERO_CHECKSUM_IMAGE_PRIVATE (self);

	/* check DISC types and add checksums for DATA and IMAGE-bin types */
	brasero_job_get_action (BRASERO_JOB (self), &action);
	brasero_job_get_current_track (BRASERO_JOB (self), &track);

	if (action == BRASERO_JOB_ACTION_CHECKSUM) {
		priv->checksum_type = brasero_track_get_checksum_type (track);
		if (priv->checksum_type & (BRASERO_CHECKSUM_MD5|BRASERO_CHECKSUM_SHA1|BRASERO_CHECKSUM_SHA256))
			result = brasero_checksum_image_create_checksum (self, &error);
		else
			result = BRASERO_BURN_ERR;
	}
	else if (action == BRASERO_JOB_ACTION_IMAGE) {
		BraseroTrackType *input;

		input = brasero_track_type_new ();
		brasero_job_get_input_type (BRASERO_JOB (self), input);

		if (brasero_track_type_get_has_image (input))
			result = brasero_checksum_image_image_and_checksum (self, &error);
		else
			result = BRASERO_BURN_ERR;

		brasero_track_type_free (input);
	}

	if (result != BRASERO_BURN_CANCEL) {
		ctx = g_new0 (BraseroChecksumImageThreadCtx, 1);
		ctx->sum = self;
		ctx->error = error;
		ctx->result = result;
		priv->end_id = g_idle_add_full (G_PRIORITY_HIGH_IDLE,
						brasero_checksum_image_end,
						ctx,
						brasero_checksum_image_destroy);
	}

	/* End thread */
	g_mutex_lock (priv->mutex);
	priv->thread = NULL;
	g_cond_signal (priv->cond);
	g_mutex_unlock (priv->mutex);

	g_thread_exit (NULL);
	return NULL;
}

static BraseroBurnResult
brasero_checksum_image_start (BraseroJob *job,
			      GError **error)
{
	BraseroChecksumImagePrivate *priv;
	GError *thread_error = NULL;
	BraseroJobAction action;

	brasero_job_get_action (job, &action);
	if (action == BRASERO_JOB_ACTION_SIZE) {
		/* say we won't write to disc if we're just checksuming "live" */
		if (brasero_job_get_fd_in (job, NULL) == BRASERO_BURN_OK)
			return BRASERO_BURN_NOT_SUPPORTED;

		/* otherwise return an output of 0 since we're not actually 
		 * writing anything to the disc. That will prevent a disc space
		 * failure. */
		brasero_job_set_output_size_for_current_track (job, 0, 0);
		return BRASERO_BURN_NOT_RUNNING;
	}

	/* we start a thread for the exploration of the graft points */
	priv = BRASERO_CHECKSUM_IMAGE_PRIVATE (job);
	g_mutex_lock (priv->mutex);
	priv->thread = g_thread_create (brasero_checksum_image_thread,
					BRASERO_CHECKSUM_IMAGE (job),
					FALSE,
					&thread_error);
	g_mutex_unlock (priv->mutex);

	/* Reminder: this is not necessarily an error as the thread may have finished */
	//if (!priv->thread)
	//	return BRASERO_BURN_ERR;
	if (thread_error) {
		g_propagate_error (error, thread_error);
		return BRASERO_BURN_ERR;
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_checksum_image_activate (BraseroJob *job,
				 GError **error)
{
	BraseroBurnFlag flags = BRASERO_BURN_FLAG_NONE;
	BraseroTrack *track = NULL;
	BraseroJobAction action;

	brasero_job_get_current_track (job, &track);
	brasero_job_get_action (job, &action);

	if (action == BRASERO_JOB_ACTION_IMAGE
	&&  brasero_track_get_checksum_type (track) != BRASERO_CHECKSUM_NONE
	&&  brasero_track_get_checksum_type (track) == brasero_checksum_get_checksum_type ()) {
		BRASERO_JOB_LOG (job,
				 "There is a checksum already %d",
				 brasero_track_get_checksum_type (track));
		/* if there is a checksum already, if so no need to redo one */
		return BRASERO_BURN_NOT_RUNNING;
	}

	flags = BRASERO_BURN_FLAG_NONE;
	brasero_job_get_flags (job, &flags);
	if (flags & BRASERO_BURN_FLAG_DUMMY) {
		BRASERO_JOB_LOG (job, "Dummy operation, skipping");
		return BRASERO_BURN_NOT_RUNNING;
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_checksum_image_clock_tick (BraseroJob *job)
{
	BraseroChecksumImagePrivate *priv;

	priv = BRASERO_CHECKSUM_IMAGE_PRIVATE (job);

	if (!priv->checksum)
		return BRASERO_BURN_OK;

	if (!priv->total)
		return BRASERO_BURN_OK;

	brasero_job_start_progress (job, FALSE);
	brasero_job_set_progress (job,
				  (gdouble) priv->bytes /
				  (gdouble) priv->total);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_checksum_image_stop (BraseroJob *job,
			     GError **error)
{
	BraseroChecksumImagePrivate *priv;

	priv = BRASERO_CHECKSUM_IMAGE_PRIVATE (job);

	g_mutex_lock (priv->mutex);
	if (priv->thread) {
		priv->cancel = 1;
		g_cond_wait (priv->cond, priv->mutex);
		priv->cancel = 0;
		priv->thread = NULL;
	}
	g_mutex_unlock (priv->mutex);

	if (priv->end_id) {
		g_source_remove (priv->end_id);
		priv->end_id = 0;
	}

	if (priv->checksum) {
		g_checksum_free (priv->checksum);
		priv->checksum = NULL;
	}

	return BRASERO_BURN_OK;
}

static void
brasero_checksum_image_init (BraseroChecksumImage *obj)
{
	BraseroChecksumImagePrivate *priv;

	priv = BRASERO_CHECKSUM_IMAGE_PRIVATE (obj);

	priv->mutex = g_mutex_new ();
	priv->cond = g_cond_new ();
}

static void
brasero_checksum_image_finalize (GObject *object)
{
	BraseroChecksumImagePrivate *priv;
	
	priv = BRASERO_CHECKSUM_IMAGE_PRIVATE (object);

	g_mutex_lock (priv->mutex);
	if (priv->thread) {
		priv->cancel = 1;
		g_cond_wait (priv->cond, priv->mutex);
		priv->cancel = 0;
		priv->thread = NULL;
	}
	g_mutex_unlock (priv->mutex);

	if (priv->end_id) {
		g_source_remove (priv->end_id);
		priv->end_id = 0;
	}

	if (priv->checksum) {
		g_checksum_free (priv->checksum);
		priv->checksum = NULL;
	}

	if (priv->mutex) {
		g_mutex_free (priv->mutex);
		priv->mutex = NULL;
	}

	if (priv->cond) {
		g_cond_free (priv->cond);
		priv->cond = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_checksum_image_class_init (BraseroChecksumImageClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroJobClass *job_class = BRASERO_JOB_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroChecksumImagePrivate));

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_checksum_image_finalize;

	job_class->activate = brasero_checksum_image_activate;
	job_class->start = brasero_checksum_image_start;
	job_class->stop = brasero_checksum_image_stop;
	job_class->clock_tick = brasero_checksum_image_clock_tick;
}

static void
brasero_checksum_image_export_caps (BraseroPlugin *plugin)
{
	GSList *input;
	BraseroPluginConfOption *checksum_type;

	brasero_plugin_define (plugin,
	                       "image-checksum",
			       /* Translators: this is the name of the plugin
				* which will be translated only when it needs
				* displaying. */
			       N_("Image Checksum"),
			       _("Checks disc integrity after it is burnt"),
			       "Philippe Rouquier",
			       0);

	/* For images we can process (thus generating a sum on the fly or simply
	 * test images). */
	input = brasero_caps_image_new (BRASERO_PLUGIN_IO_ACCEPT_FILE|
					BRASERO_PLUGIN_IO_ACCEPT_PIPE,
					BRASERO_IMAGE_FORMAT_BIN);
	brasero_plugin_process_caps (plugin, input);

	brasero_plugin_set_process_flags (plugin,
					  BRASERO_PLUGIN_RUN_PREPROCESSING|
					  BRASERO_PLUGIN_RUN_BEFORE_TARGET);

	brasero_plugin_check_caps (plugin,
				   BRASERO_CHECKSUM_MD5|
				   BRASERO_CHECKSUM_SHA1|
				   BRASERO_CHECKSUM_SHA256,
				   input);
	g_slist_free (input);

	/* add some configure options */
	checksum_type = brasero_plugin_conf_option_new (BRASERO_PROPS_CHECKSUM_IMAGE,
							_("Hashing algorithm to be used:"),
							BRASERO_PLUGIN_OPTION_CHOICE);
	brasero_plugin_conf_option_choice_add (checksum_type,
					       _("MD5"), BRASERO_CHECKSUM_MD5);
	brasero_plugin_conf_option_choice_add (checksum_type,
					       _("SHA1"), BRASERO_CHECKSUM_SHA1);
	brasero_plugin_conf_option_choice_add (checksum_type,
					       _("SHA256"), BRASERO_CHECKSUM_SHA256);

	brasero_plugin_add_conf_option (plugin, checksum_type);

	brasero_plugin_set_compulsory (plugin, FALSE);
}
