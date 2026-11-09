/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-burn
 * Copyright (C) The Brasero Developers 2026
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

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gmodule.h>

#include <libburn/libburn.h>

#include "burn-libburnia.h"
#include "burn-libburn-common.h"
#include "burn-job.h"
#include "brasero-plugin-registration.h"
#include "brasero-tags.h"
#include "brasero-track-disc.h"
#include "brasero-track-image.h"

#define BRASERO_TYPE_LIBBURN_READ         (brasero_libburn_read_get_type ())
#define BRASERO_LIBBURN_READ(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_LIBBURN_READ, BraseroLibburnRead))
#define BRASERO_LIBBURN_READ_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_LIBBURN_READ, BraseroLibburnReadClass))
#define BRASERO_IS_LIBBURN_READ(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_LIBBURN_READ))
#define BRASERO_IS_LIBBURN_READ_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_LIBBURN_READ))
#define BRASERO_LIBBURN_READ_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_LIBBURN_READ, BraseroLibburnReadClass))

#define BRASERO_LIBBURN_READ_CHUNK_SECTORS 32
#define BRASERO_LIBBURN_SECTOR_SIZE 2048

struct _BraseroLibburnReadPrivate {
	GThread *thread;
	GMutex mutex;
	GCond cond;

	GError *error;
	guint thread_id;

	guint cancel:1;
};

typedef struct _BraseroLibburnReadPrivate BraseroLibburnReadPrivate;

BRASERO_PLUGIN_BOILERPLATE (BraseroLibburnRead, brasero_libburn_read, BraseroJob);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (BraseroLibburnRead,
				brasero_libburn_read,
				BRASERO_TYPE_JOB,
				0,
				G_ADD_PRIVATE_DYNAMIC (BraseroLibburnRead));

static BraseroBurnResult
brasero_libburn_read_get_boundaries (BraseroJob *job,
                                     goffset *start_sector,
                                     goffset *nb_blocks,
                                     GError **error)
{
	BraseroTrack *track = NULL;
	GValue *value = NULL;
	BraseroTrackType *output = NULL;

	brasero_job_get_current_track (job, &track);
	if (!track) {
		g_set_error (error,
		             BRASERO_BURN_ERROR,
		             BRASERO_BURN_ERROR_GENERAL,
		             _("An internal error occurred"));
		return BRASERO_BURN_ERR;
	}

	output = brasero_track_type_new ();
	brasero_job_get_output_type (job, output);

	brasero_track_tag_lookup (track,
	                          BRASERO_TRACK_MEDIUM_ADDRESS_START_TAG,
	                          &value);
	if (value) {
		guint64 start, end;

		start = g_value_get_uint64 (value);

		value = NULL;
		brasero_track_tag_lookup (track,
		                          BRASERO_TRACK_MEDIUM_ADDRESS_END_TAG,
		                          &value);
		if (!value) {
			brasero_track_type_free (output);
			g_set_error (error,
			             BRASERO_BURN_ERROR,
			             BRASERO_BURN_ERROR_GENERAL,
			             _("An internal error occurred"));
			return BRASERO_BURN_ERR;
		}

		end = g_value_get_uint64 (value);
		*start_sector = start;
		*nb_blocks = end - start;
	}
	else if (BRASERO_IS_TRACK_DISC (track) &&
	         brasero_track_disc_get_track_num (BRASERO_TRACK_DISC (track)) > 0) {
		BraseroDrive *drive;
		BraseroMedium *medium;

		drive = brasero_track_disc_get_drive (BRASERO_TRACK_DISC (track));
		medium = brasero_drive_get_medium (drive);
		brasero_medium_get_track_space (medium,
		                                brasero_track_disc_get_track_num (BRASERO_TRACK_DISC (track)),
		                                NULL,
		                                nb_blocks);
		brasero_medium_get_track_address (medium,
		                                  brasero_track_disc_get_track_num (BRASERO_TRACK_DISC (track)),
		                                  NULL,
		                                  start_sector);
	}
	else if (brasero_track_type_get_image_format (output) == BRASERO_IMAGE_FORMAT_BIN) {
		BraseroDrive *drive = NULL;
		BraseroMedium *medium;

		if (BRASERO_IS_TRACK_DISC (track))
			drive = brasero_track_disc_get_drive (BRASERO_TRACK_DISC (track));

		if (drive) {
			medium = brasero_drive_get_medium (drive);
			brasero_medium_get_last_data_track_space (medium,
			                                          NULL,
			                                          nb_blocks);
			brasero_medium_get_last_data_track_address (medium,
			                                            NULL,
			                                            start_sector);
		} else {
			brasero_track_get_size (track, nb_blocks, NULL);
			*start_sector = 0;
		}
	}
	else {
		brasero_track_get_size (track, nb_blocks, NULL);
		*start_sector = 0;
	}

	brasero_track_type_free (output);
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_libburn_read_get_size (BraseroJob *job,
                               GError **error)
{
	goffset start = 0;
	goffset blocks = 0;
	BraseroBurnResult res;

	res = brasero_libburn_read_get_boundaries (job, &start, &blocks, error);
	if (res != BRASERO_BURN_OK)
		return res;

	brasero_job_set_output_size_for_current_track (job,
	                                               blocks,
	                                               blocks * (goffset) BRASERO_LIBBURN_SECTOR_SIZE);
	return BRASERO_BURN_NOT_RUNNING;
}

static BraseroLibburnCtx *
brasero_libburn_read_ctx_new (BraseroLibburnRead *self,
                              gchar *device,
                              GError **error)
{
	gchar libburn_device [BURN_DRIVE_ADR_LEN];
	BraseroLibburnCtx *ctx = NULL;
	int res;

	if (!device) {
		g_set_error (error,
		             BRASERO_BURN_ERROR,
		             BRASERO_BURN_ERROR_GENERAL,
		             _("The drive address could not be retrieved"));
		return NULL;
	}

	if (!burn_initialize ()) {
		g_set_error (error,
		             BRASERO_BURN_ERROR,
		             BRASERO_BURN_ERROR_GENERAL,
		             _("libburn library could not be initialized"));
		return NULL;
	}

	burn_msgs_set_severities ("ALL", "NEVER", "");

	res = burn_drive_convert_fs_adr (device, libburn_device);
	if (res <= 0) {
		g_set_error (error,
		             BRASERO_BURN_ERROR,
		             BRASERO_BURN_ERROR_GENERAL,
		             _("The drive address could not be retrieved"));
		return NULL;
	}

	ctx = g_new0 (BraseroLibburnCtx, 1);
	ctx->is_burning = FALSE;
	res = burn_drive_scan_and_grab (&ctx->drive_info, libburn_device, 0);
	BRASERO_JOB_LOG (self, "Reader Drive (%s) init result = %d", libburn_device, res);
	if (res <= 0) {
		g_free (ctx);
		g_set_error (error,
		             BRASERO_BURN_ERROR,
		             BRASERO_BURN_ERROR_DRIVE_BUSY,
		             _("The drive is busy"));
		return NULL;
	}

	ctx->drive = ctx->drive_info->drive;
	return ctx;
}

static BraseroBurnResult
brasero_libburn_read_write_to_fd (BraseroLibburnRead *self,
                                  int fd,
                                  gpointer buffer,
                                  gint bytes_remaining)
{
	gint bytes_written = 0;
	BraseroLibburnReadPrivate *priv;

	priv = brasero_libburn_read_get_instance_private (self);

	while (bytes_remaining > 0) {
		gint written;

		written = write (fd,
		                 ((gchar *) buffer) + bytes_written,
		                 bytes_remaining);

		if (priv->cancel)
			break;

		if (written < 0) {
			if (errno == EPIPE) {
				BRASERO_JOB_LOG (self, "Downstream closed pipe (EPIPE)");
				return BRASERO_BURN_ERR;
			}

			if (errno != EINTR && errno != EAGAIN) {
				int errsv = errno;

				priv->error = g_error_new (BRASERO_BURN_ERROR,
				                           BRASERO_BURN_ERROR_GENERAL,
				                           _("Data could not be written (%s)"),
				                           g_strerror (errsv));
				return BRASERO_BURN_ERR;
			}

			g_thread_yield ();
		} else if (written > 0) {
			bytes_remaining -= written;
			bytes_written += written;
		}
	}

	return BRASERO_BURN_OK;
}

static gboolean
brasero_libburn_read_thread_finished (gpointer data)
{
	BraseroLibburnRead *self = data;
	BraseroLibburnReadPrivate *priv;

	priv = brasero_libburn_read_get_instance_private (self);
	priv->thread_id = 0;

	if (priv->error) {
		GError *error;

		error = priv->error;
		priv->error = NULL;
		brasero_job_error (BRASERO_JOB (self), error);
		return FALSE;
	}

	if (brasero_job_get_fd_out (BRASERO_JOB (self), NULL) != BRASERO_BURN_OK) {
		BraseroTrackImage *track = NULL;
		gchar *output = NULL;
		goffset blocks = 0;

		track = brasero_track_image_new ();
		brasero_job_get_image_output (BRASERO_JOB (self),
		                              &output,
		                              NULL);
		brasero_track_image_set_source (track,
		                                output,
		                                NULL,
		                                BRASERO_IMAGE_FORMAT_BIN);

		brasero_job_get_session_output_size (BRASERO_JOB (self), &blocks, NULL);
		brasero_track_image_set_block_num (track, blocks);

		brasero_job_add_track (BRASERO_JOB (self), BRASERO_TRACK (track));
		g_object_unref (track);
		g_free (output);
	}

	brasero_job_finished_track (BRASERO_JOB (self));
	return FALSE;
}

static gpointer
brasero_libburn_read_thread (gpointer data)
{
	BraseroLibburnRead *self = BRASERO_LIBBURN_READ (data);
	BraseroLibburnReadPrivate *priv = brasero_libburn_read_get_instance_private (self);
	BraseroLibburnCtx *ctx = NULL;
	BraseroTrack *track = NULL;
	BraseroDrive *drive = NULL;
	gchar *device = NULL;
	goffset start_sector = 0;
	goffset nb_blocks = 0;
	goffset cur_sector = 0;
	goffset end_sector = 0;
	goffset read_blocks = 0;
	FILE *output_file = NULL;
	int fd = -1;
	gboolean is_pipe = FALSE;
	char *image_path = NULL;

	BRASERO_JOB_LOG (self, "Starting libburn read thread");

	brasero_job_get_current_track (BRASERO_JOB (self), &track);
	if (track && BRASERO_IS_TRACK_DISC (track))
		drive = brasero_track_disc_get_drive (BRASERO_TRACK_DISC (track));

	if (drive && brasero_drive_get_device (drive)) {
		device = g_strdup (brasero_drive_get_device (drive));
	} else {
		brasero_job_get_device (BRASERO_JOB (self), &device);
	}

	ctx = brasero_libburn_read_ctx_new (self, device, &priv->error);
	g_free (device);

	if (!ctx) {
		BRASERO_JOB_LOG (self, "Failed to initialize libburn drive context");
		goto end;
	}

	if (brasero_libburn_read_get_boundaries (BRASERO_JOB (self), &start_sector, &nb_blocks, &priv->error) != BRASERO_BURN_OK) {
		BRASERO_JOB_LOG (self, "Failed to determine disc read boundaries");
		goto end;
	}

	BRASERO_JOB_LOG (self, "Reading from sector %" G_GOFFSET_FORMAT " for %" G_GOFFSET_FORMAT " sectors",
	                 start_sector, nb_blocks);

	if (brasero_job_get_fd_out (BRASERO_JOB (self), &fd) == BRASERO_BURN_OK) {
		is_pipe = TRUE;
		brasero_job_set_nonblocking (BRASERO_JOB (self), NULL);
	} else {
		if (brasero_job_get_image_output (BRASERO_JOB (self), &image_path, NULL) != BRASERO_BURN_OK) {
			priv->error = g_error_new (BRASERO_BURN_ERROR,
			                           BRASERO_BURN_ERROR_GENERAL,
			                           _("An internal error occurred"));
			goto end;
		}
		output_file = g_fopen (image_path, "wb");
		if (!output_file) {
			priv->error = g_error_new (BRASERO_BURN_ERROR,
			                           BRASERO_BURN_ERROR_GENERAL,
			                           _("Data could not be written (%s)"),
			                           g_strerror (errno));
			goto end;
		}
	}

	brasero_job_set_use_average_rate (BRASERO_JOB (self), TRUE);
	brasero_job_set_current_action (BRASERO_JOB (self),
	                                BRASERO_BURN_ACTION_DRIVE_COPY,
	                                NULL,
	                                FALSE);
	brasero_job_start_progress (BRASERO_JOB (self), FALSE);

	cur_sector = start_sector;
	end_sector = start_sector + nb_blocks;

	while (cur_sector < end_sector) {
		char buf [BRASERO_LIBBURN_READ_CHUNK_SECTORS * BRASERO_LIBBURN_SECTOR_SIZE];
		off_t to_read_sectors;
		off_t bytes_to_read;
		off_t bytes_read = 0;
		int res;

		if (priv->cancel)
			break;

		to_read_sectors = end_sector - cur_sector;
		if (to_read_sectors > BRASERO_LIBBURN_READ_CHUNK_SECTORS)
			to_read_sectors = BRASERO_LIBBURN_READ_CHUNK_SECTORS;

		bytes_to_read = to_read_sectors * BRASERO_LIBBURN_SECTOR_SIZE;
		res = burn_read_data (ctx->drive,
		                      (off_t) cur_sector * (off_t) BRASERO_LIBBURN_SECTOR_SIZE,
		                      buf,
		                      bytes_to_read,
		                      &bytes_read,
		                      0);

		if (res <= 0 || bytes_read < bytes_to_read) {
			int err_code = 0, err_errno = 0;
			char err_sev [80] = {0};
			char err_txt [BURN_MSGS_MESSAGE_LEN] = {0};

			burn_msgs_obtain ("ALL", &err_code, err_txt, &err_errno, err_sev);
			if (err_txt[0] != '\0')
				BRASERO_JOB_LOG (self, "libburn error: %s", err_txt);

			BRASERO_JOB_LOG (self, "burn_read_data error at sector %" G_GOFFSET_FORMAT " (res=%d, bytes_read=%ld)",
			                 cur_sector, res, (long) bytes_read);

			priv->error = g_error_new (BRASERO_BURN_ERROR,
			                           BRASERO_BURN_ERROR_GENERAL,
			                           _("An error occurred while reading the disc"));
			break;
		}

		if (is_pipe) {
			if (brasero_libburn_read_write_to_fd (self, fd, buf, bytes_read) != BRASERO_BURN_OK)
				break;
		} else {
			if (fwrite (buf, 1, bytes_read, output_file) != (size_t) bytes_read) {
				priv->error = g_error_new (BRASERO_BURN_ERROR,
				                           BRASERO_BURN_ERROR_GENERAL,
				                           _("Data could not be written (%s)"),
				                           g_strerror (errno));
				break;
			}
		}

		cur_sector += to_read_sectors;
		read_blocks += to_read_sectors;
		brasero_job_set_written_track (BRASERO_JOB (self), (gint64) read_blocks * (gint64) BRASERO_LIBBURN_SECTOR_SIZE);
		if (nb_blocks > 0)
			brasero_job_set_progress (BRASERO_JOB (self), (gdouble) read_blocks / (gdouble) nb_blocks);
	}

end:
	if (output_file)
		fclose (output_file);
	g_free (image_path);

	if (ctx)
		brasero_libburn_common_ctx_free (ctx);

	BRASERO_JOB_LOG (self, "Exiting libburn read thread");

	g_mutex_lock (&priv->mutex);
	if (!priv->cancel)
		priv->thread_id = g_idle_add (brasero_libburn_read_thread_finished, self);

	priv->thread = NULL;
	g_cond_signal (&priv->cond);
	g_mutex_unlock (&priv->mutex);

	return NULL;
}

static BraseroBurnResult
brasero_libburn_read_start (BraseroJob *job,
                            GError **error)
{
	BraseroLibburnRead *self = BRASERO_LIBBURN_READ (job);
	BraseroLibburnReadPrivate *priv = brasero_libburn_read_get_instance_private (self);
	BraseroJobAction action;

	brasero_job_get_action (job, &action);
	if (action == BRASERO_JOB_ACTION_SIZE)
		return brasero_libburn_read_get_size (job, error);

	if (priv->error) {
		g_error_free (priv->error);
		priv->error = NULL;
	}

	g_mutex_lock (&priv->mutex);
	if (priv->thread) {
		g_mutex_unlock (&priv->mutex);
		return BRASERO_BURN_RUNNING;
	}

	priv->cancel = 0;
	priv->thread = g_thread_new ("libburn-read",
	                             brasero_libburn_read_thread,
	                             self);
	g_mutex_unlock (&priv->mutex);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_libburn_read_stop (BraseroJob *job,
                           GError **error)
{
	BraseroLibburnRead *self = BRASERO_LIBBURN_READ (job);
	BraseroLibburnReadPrivate *priv = brasero_libburn_read_get_instance_private (self);

	g_mutex_lock (&priv->mutex);
	priv->cancel = 1;
	while (priv->thread)
		g_cond_wait (&priv->cond, &priv->mutex);
	priv->cancel = 0;
	g_mutex_unlock (&priv->mutex);

	if (priv->thread_id) {
		g_source_remove (priv->thread_id);
		priv->thread_id = 0;
	}

	return BRASERO_BURN_OK;
}

static void
brasero_libburn_read_finalize (GObject *object)
{
	BraseroLibburnRead *self = BRASERO_LIBBURN_READ (object);
	BraseroLibburnReadPrivate *priv = brasero_libburn_read_get_instance_private (self);

	brasero_libburn_read_stop (BRASERO_JOB (self), NULL);

	g_mutex_clear (&priv->mutex);
	g_cond_clear (&priv->cond);

	if (priv->error) {
		g_error_free (priv->error);
		priv->error = NULL;
	}

	G_OBJECT_CLASS (brasero_libburn_read_parent_class)->finalize (object);
}

static void
brasero_libburn_read_class_init (BraseroLibburnReadClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroJobClass *job_class = BRASERO_JOB_CLASS (klass);

	object_class->finalize = brasero_libburn_read_finalize;

	job_class->start = brasero_libburn_read_start;
	job_class->stop = brasero_libburn_read_stop;
}

static void
brasero_libburn_read_class_finalize (BraseroLibburnReadClass *klass)
{
}

static void
brasero_libburn_read_init (BraseroLibburnRead *obj)
{
	BraseroLibburnReadPrivate *priv = brasero_libburn_read_get_instance_private (obj);

	g_mutex_init (&priv->mutex);
	g_cond_init (&priv->cond);
}

static void
brasero_libburn_read_export_caps (BraseroPlugin *plugin)
{
	GSList *output;
	GSList *input;

	brasero_plugin_define (plugin,
	                       "libburn-read",
	                       _("Libburn Disc Reader"),
	                       _("Copies any disc to a disc image or stream"),
	                       "Philippe Rouquier",
	                       15);

	output = brasero_caps_image_new (BRASERO_PLUGIN_IO_ACCEPT_FILE|
	                                 BRASERO_PLUGIN_IO_ACCEPT_PIPE,
	                                 BRASERO_IMAGE_FORMAT_BIN);

	input = brasero_caps_disc_new (BRASERO_MEDIUM_CD|
	                               BRASERO_MEDIUM_DVD|
	                               BRASERO_MEDIUM_BD|
	                               BRASERO_MEDIUM_DUAL_L|
	                               BRASERO_MEDIUM_PLUS|
	                               BRASERO_MEDIUM_SEQUENTIAL|
	                               BRASERO_MEDIUM_RESTRICTED|
	                               BRASERO_MEDIUM_RAM|
	                               BRASERO_MEDIUM_ROM|
	                               BRASERO_MEDIUM_WRITABLE|
	                               BRASERO_MEDIUM_REWRITABLE|
	                               BRASERO_MEDIUM_CLOSED|
	                               BRASERO_MEDIUM_APPENDABLE|
	                               BRASERO_MEDIUM_HAS_DATA);

	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (output);
	g_slist_free (input);

	brasero_plugin_register_group (plugin, _(LIBBURNIA_DESCRIPTION));
}
