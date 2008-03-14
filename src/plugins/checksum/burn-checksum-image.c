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
#include <fcntl.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <gmodule.h>

#include <gconf/gconf-client.h>

#include "burn-plugin.h"
#include "burn-job.h"
#include "burn-checksum-image.h"
#include "burn-volume.h"
#include "burn-drive.h"

BRASERO_PLUGIN_BOILERPLATE (BraseroMd5sum, brasero_md5sum, BRASERO_TYPE_JOB, BraseroJob);

struct _BraseroMd5sumPrivate {
	GChecksum *checksum;
	BraseroChecksumType checksum_type;

	/* That's for progress reporting */
	gint64 total;
	gint64 bytes;

	/* this is for the thread and the end of it */
	GThread *thread;
	gint end_id;

	guint cancel;
};
typedef struct _BraseroMd5sumPrivate BraseroMd5sumPrivate;

#define BRASERO_MD5SUM_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_MD5SUM, BraseroMd5sumPrivate))

#define GCONF_KEY_CHECKSUM_TYPE		"/apps/brasero/config/checksum_image"

static BraseroJobClass *parent_class = NULL;

static gint
brasero_md5sum_read (BraseroMd5sum *self,
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
brasero_md5sum_write (BraseroMd5sum *self,
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
brasero_md5sum_checksum (BraseroMd5sum *self,
			 GChecksumType checksum_type,
			 int fd_in,
			 int fd_out,
			 GError **error)
{
	gint read_bytes;
	guchar buffer [2048];
	BraseroBurnResult result;
	BraseroMd5sumPrivate *priv;

	priv = BRASERO_MD5SUM_PRIVATE (self);

	priv->checksum = g_checksum_new (checksum_type);
	result = BRASERO_BURN_OK;
	while (1) {
		read_bytes = brasero_md5sum_read (self,
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
			result = brasero_md5sum_write (self,
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
brasero_md5sum_checksum_fd_input (BraseroMd5sum *self,
				  GChecksumType checksum_type,
				  GError **error)
{
	int fd_in = -1;
	int fd_out = -1;
	BraseroBurnResult result;
	BraseroMd5sumPrivate *priv;

	priv = BRASERO_MD5SUM_PRIVATE (self);

	BRASERO_JOB_LOG (self, "Starting md5 generation live (size = %i)", priv->total);
	result = brasero_job_set_nonblocking (BRASERO_JOB (self), error);
	if (result != BRASERO_BURN_OK)
		return result;

	brasero_job_get_fd_in (BRASERO_JOB (self), &fd_in);
	brasero_job_get_fd_out (BRASERO_JOB (self), &fd_out);

	return brasero_md5sum_checksum (self, checksum_type, fd_in, fd_out, error);
}

static BraseroBurnResult
brasero_md5sum_checksum_file_input (BraseroMd5sum *self,
				    GChecksumType checksum_type,
				    GError **error)
{
	BraseroMd5sumPrivate *priv;
	BraseroBurnResult result;
	BraseroTrack *track;
	int fd_out = -1;
	int fd_in = -1;
	gchar *path;

	priv = BRASERO_MD5SUM_PRIVATE (self);

	BRASERO_JOB_LOG (self,
			 "Starting checksuming file %s (size = %i)",
			 path,
			 priv->total);

	/* get all information */
	brasero_job_get_current_track (BRASERO_JOB (self), &track);
	path = brasero_track_get_image_source (track, FALSE);
	if (!path) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the image is not local"));
		return BRASERO_BURN_ERR;
	}

	fd_in = open (path, O_RDONLY);
	if (!fd_in) {
		gchar *name = NULL;

		if (errno == ENOENT)
			return BRASERO_BURN_RETRY;

		name = g_path_get_basename (path);

		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the file %s couldn't be read (%s)"),
			     name,
			     strerror (errno));
		g_free (name);
		g_free (path);

		return BRASERO_BURN_ERR;
	}

	/* and here we go */
	brasero_job_get_fd_out (BRASERO_JOB (self), &fd_out);
	result = brasero_md5sum_checksum (self, checksum_type, fd_in, fd_out, error);
	g_free (path);
	close (fd_in);

	return result;
}

static BraseroBurnResult
brasero_md5sum_create_checksum (BraseroMd5sum *self,
				GError **error)
{
	BraseroBurnResult result;
	BraseroMd5sumPrivate *priv;
	BraseroTrack *track = NULL;
	GChecksumType checksum_type;

	priv = BRASERO_MD5SUM_PRIVATE (self);

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

	/* see if another plugin is sending us data to checksum */
	if (brasero_job_get_fd_in (BRASERO_JOB (self), NULL) == BRASERO_BURN_OK) {
		BraseroMedium *medium;

		/* we're only able to checksum ISO format at the moment so that
		 * means we can only handle last session */
		medium = brasero_track_get_medium_source (track);
		brasero_medium_get_last_data_track_space (medium,
							  &priv->total,
							  NULL);

		return brasero_md5sum_checksum_fd_input (self, checksum_type, error);
	}
	else {
		result = brasero_track_get_image_size (track,
						       NULL,
						       NULL,
						       &priv->total,
						       error);
		if (result != BRASERO_BURN_OK)
			return result;

		return brasero_md5sum_checksum_file_input (self, checksum_type, error);
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_md5sum_image_and_checksum (BraseroMd5sum *self,
				   GError **error)
{
	GConfClient *client;
	BraseroBurnResult result;
	GChecksumType checksum_type;
	BraseroMd5sumPrivate *priv;

	priv = BRASERO_MD5SUM_PRIVATE (self);
	client = gconf_client_get_default ();
	priv->checksum_type = gconf_client_get_int (client, GCONF_KEY_CHECKSUM_TYPE, NULL);
	g_object_unref (client);

	if (priv->checksum_type == BRASERO_CHECKSUM_NONE)
		checksum_type = G_CHECKSUM_MD5;
	else if (priv->checksum_type & BRASERO_CHECKSUM_MD5)
		checksum_type = G_CHECKSUM_MD5;
	else if (priv->checksum_type & BRASERO_CHECKSUM_SHA1)
		checksum_type = G_CHECKSUM_SHA1;
	else if (priv->checksum_type & BRASERO_CHECKSUM_SHA256)
		checksum_type = G_CHECKSUM_SHA256;
	else
		checksum_type = G_CHECKSUM_MD5;

	if (brasero_job_get_fd_in (BRASERO_JOB (self), NULL) == BRASERO_BURN_OK)
		result = brasero_md5sum_checksum_fd_input (self, checksum_type, error);
	else
		result = brasero_md5sum_checksum_file_input (self, checksum_type, error);

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
	const gchar *checksum;
	BraseroBurnResult result;
	BraseroMd5sumPrivate *priv;
	BraseroMd5sumThreadCtx *ctx;

	ctx = data;
	self = ctx->sum;
	priv = BRASERO_MD5SUM_PRIVATE (self);

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
			 "setting new checksum (type = %i) %s (%s before)",
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
		BraseroTrack *track;

		brasero_job_get_current_track (BRASERO_JOB (self), &track);
		priv->checksum_type = brasero_track_get_checksum_type (track);
		if (priv->checksum_type & (BRASERO_CHECKSUM_MD5|BRASERO_CHECKSUM_SHA1|BRASERO_CHECKSUM_SHA256))
			result = brasero_md5sum_create_checksum (self, &error);
		else
			result = BRASERO_BURN_ERR;
	}
	else if (action == BRASERO_JOB_ACTION_IMAGE) {
		BraseroTrackType type;

		brasero_job_get_input_type (BRASERO_JOB (self), &type);
		if (type.type == BRASERO_TRACK_TYPE_IMAGE)
			result = brasero_md5sum_image_and_checksum (self, &error);
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
	if (!priv->checksum)
		return BRASERO_BURN_OK;

	if (!priv->total)
		return BRASERO_BURN_OK;

	brasero_job_set_progress (job,
				  (gdouble) priv->bytes /
				  (gdouble) priv->total);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_md5sum_stop (BraseroJob *job,
		     GError **error)
{
	BraseroMd5sumPrivate *priv;

	priv = BRASERO_MD5SUM_PRIVATE (job);

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

	if (priv->checksum) {
		g_checksum_free (priv->checksum);
		priv->checksum = NULL;
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

	if (priv->checksum) {
		g_checksum_free (priv->checksum);
		priv->checksum = NULL;
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
	BraseroPluginConfOption *checksum_type;

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
	brasero_plugin_check_caps (plugin,
				   BRASERO_CHECKSUM_MD5|
				   BRASERO_CHECKSUM_SHA1|
				   BRASERO_CHECKSUM_SHA256,
				   input);
	g_slist_free (input);

	brasero_plugin_set_process_flags (plugin,
					  BRASERO_PLUGIN_RUN_FIRST|
					  BRASERO_PLUGIN_RUN_LAST);

	/* add some configure options */
	checksum_type = brasero_plugin_conf_option_new (GCONF_KEY_CHECKSUM_TYPE,
							_("Hashing algorithm to be used:"),
							BRASERO_PLUGIN_OPTION_CHOICE);
	brasero_plugin_conf_option_choice_add (checksum_type,
					       _("MD5"), BRASERO_CHECKSUM_MD5);
	brasero_plugin_conf_option_choice_add (checksum_type,
					       _("SHA1"), BRASERO_CHECKSUM_SHA1);
	brasero_plugin_conf_option_choice_add (checksum_type,
					       _("SHA256"), BRASERO_CHECKSUM_SHA256);

	brasero_plugin_add_conf_option (plugin, checksum_type);

	return BRASERO_BURN_OK;
}
