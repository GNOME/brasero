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

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <gmodule.h>

#include "brasero-units.h"

#include "burn-job.h"
#include "brasero-plugin-registration.h"
#include "burn-dvdcss-private.h"
#include "burn-volume.h"
#include "brasero-medium.h"
#include "brasero-track-image.h"
#include "brasero-track-disc.h"


#define BRASERO_TYPE_DVDCSS         (brasero_dvdcss_get_type ())
#define BRASERO_DVDCSS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_DVDCSS, BraseroDvdcss))
#define BRASERO_DVDCSS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_DVDCSS, BraseroDvdcssClass))
#define BRASERO_IS_DVDCSS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_DVDCSS))
#define BRASERO_IS_DVDCSS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_DVDCSS))
#define BRASERO_DVDCSS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_DVDCSS, BraseroDvdcssClass))

BRASERO_PLUGIN_BOILERPLATE (BraseroDvdcss, brasero_dvdcss, BRASERO_TYPE_JOB, BraseroJob);

struct _BraseroDvdcssPrivate {
	GError *error;
	GThread *thread;
	GMutex *mutex;
	GCond *cond;
	guint thread_id;

	guint cancel:1;
};
typedef struct _BraseroDvdcssPrivate BraseroDvdcssPrivate;

#define BRASERO_DVDCSS_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_DVDCSS, BraseroDvdcssPrivate))

#define BRASERO_DVDCSS_I_BLOCKS	16ULL

static GObjectClass *parent_class = NULL;

static gboolean
brasero_dvdcss_library_init (BraseroPlugin *plugin)
{
	gpointer address;
	GModule *module;

	if (css_ready)
		return TRUE;

	/* load libdvdcss library and see the version (mine is 1.2.0) */
	module = g_module_open ("libdvdcss.so.2", G_MODULE_BIND_LOCAL);
	if (!module)
		goto error_doesnt_exist;

	if (!g_module_symbol (module, "dvdcss_open", &address))
		goto error_version;
	dvdcss_open = address;

	if (!g_module_symbol (module, "dvdcss_close", &address))
		goto error_version;
	dvdcss_close = address;

	if (!g_module_symbol (module, "dvdcss_read", &address))
		goto error_version;
	dvdcss_read = address;

	if (!g_module_symbol (module, "dvdcss_seek", &address))
		goto error_version;
	dvdcss_seek = address;

	if (!g_module_symbol (module, "dvdcss_error", &address))
		goto error_version;
	dvdcss_error = address;

	if (plugin) {
		g_module_close (module);
		return TRUE;
	}

	css_ready = TRUE;
	return TRUE;

error_doesnt_exist:
	brasero_plugin_add_error (plugin,
	                          BRASERO_PLUGIN_ERROR_MISSING_LIBRARY,
	                          "libdvdcss.so.2");
	return FALSE;

error_version:
	brasero_plugin_add_error (plugin,
	                          BRASERO_PLUGIN_ERROR_LIBRARY_VERSION,
	                          "libdvdcss.so.2");
	g_module_close (module);
	return FALSE;
}

static gboolean
brasero_dvdcss_thread_finished (gpointer data)
{
	goffset blocks = 0;
	gchar *image = NULL;
	BraseroDvdcss *self = data;
	BraseroDvdcssPrivate *priv;
	BraseroTrackImage *track = NULL;

	priv = BRASERO_DVDCSS_PRIVATE (self);
	priv->thread_id = 0;

	if (priv->error) {
		GError *error;

		error = priv->error;
		priv->error = NULL;
		brasero_job_error (BRASERO_JOB (self), error);
		return FALSE;
	}

	track = brasero_track_image_new ();
	brasero_job_get_image_output (BRASERO_JOB (self),
				      &image,
				      NULL);
	brasero_track_image_set_source (track,
					image,
					NULL,
					BRASERO_IMAGE_FORMAT_BIN);

	brasero_job_get_session_output_size (BRASERO_JOB (self), &blocks, NULL);
	brasero_track_image_set_block_num (track, blocks);

	brasero_job_add_track (BRASERO_JOB (self), BRASERO_TRACK (track));

	/* It's good practice to unref the track afterwards as we don't need it
	 * anymore. BraseroTaskCtx refs it. */
	g_object_unref (track);

	brasero_job_finished_track (BRASERO_JOB (self));

	return FALSE;
}

static BraseroBurnResult
brasero_dvdcss_write_sector_to_fd (BraseroDvdcss *self,
				   gpointer buffer,
				   gint bytes_remaining)
{
	int fd;
	gint bytes_written = 0;
	BraseroDvdcssPrivate *priv;

	priv = BRASERO_DVDCSS_PRIVATE (self);

	brasero_job_get_fd_out (BRASERO_JOB (self), &fd);
	while (bytes_remaining) {
		gint written;

		written = write (fd,
				 ((gchar *) buffer)  + bytes_written,
				 bytes_remaining);

		if (priv->cancel)
			break;

		if (written != bytes_remaining) {
			if (errno != EINTR && errno != EAGAIN) {
                                int errsv = errno;

				/* unrecoverable error */
				priv->error = g_error_new (BRASERO_BURN_ERROR,
							   BRASERO_BURN_ERROR_GENERAL,
							   _("Data could not be written (%s)"),
							   g_strerror (errsv));
				return BRASERO_BURN_ERR;
			}

			g_thread_yield ();
		}

		if (written > 0) {
			bytes_remaining -= written;
			bytes_written += written;
		}
	}

	return BRASERO_BURN_OK;
}

struct _BraseroScrambledSectorRange {
	gint start;
	gint end;
};
typedef struct _BraseroScrambledSectorRange BraseroScrambledSectorRange;

static gboolean
brasero_dvdcss_create_scrambled_sectors_map (BraseroDvdcss *self,
					     BraseroDrive *drive,
                                             GQueue *map,
					     dvdcss_handle *handle,
					     BraseroVolFile *parent,
					     GError **error)
{
	GList *iter;

	/* this allows to cache keys for encrypted files */
	for (iter = parent->specific.dir.children; iter; iter = iter->next) {
		BraseroVolFile *file;

		file = iter->data;
		if (!file->isdir) {
			if (!strncmp (file->name + strlen (file->name) - 6, ".VOB", 4)) {
				BraseroScrambledSectorRange *range;
				gsize current_extent;
				GSList *extents;

				BRASERO_JOB_LOG (self, "Retrieving keys for %s", file->name);

				/* take the first address for each extent of the file */
				if (!file->specific.file.extents) {
					BRASERO_JOB_LOG (self, "Problem: file has no extents");
					return FALSE;
				}

				range = g_new0 (BraseroScrambledSectorRange, 1);
				for (extents = file->specific.file.extents; extents; extents = extents->next) {
					BraseroVolFileExtent *extent;

					extent = extents->data;

					range->start = extent->block;
					range->end = extent->block + BRASERO_BYTES_TO_SECTORS (extent->size, DVDCSS_BLOCK_SIZE);

					BRASERO_JOB_LOG (self, "From 0x%llx to 0x%llx", range->start, range->end);
					g_queue_push_head (map, range);

					if (extent->size == 0) {
						BRASERO_JOB_LOG (self, "0 size extent");
						continue;
					}

					current_extent = dvdcss_seek (handle, range->start, DVDCSS_SEEK_KEY);
					if (current_extent != range->start) {
						BRASERO_JOB_LOG (self, "Problem: could not retrieve key");
						g_set_error (error,
							     BRASERO_BURN_ERROR,
							     BRASERO_BURN_ERROR_GENERAL,
							     /* Translators: %s is the path to a drive. "regionset %s"
							      * should be left as is just like "DVDCSS_METHOD=title
							      * brasero --no-existing-session" */
							     _("Error while retrieving a key used for encryption. You may solve such a problem with one of the following methods: in a terminal either set the proper DVD region code for your CD/DVD player with the \"regionset %s\" command or run the \"DVDCSS_METHOD=title brasero --no-existing-session\" command"),
							     brasero_drive_get_device (drive));
						return FALSE;
					}
				}
			}
		}
		else if (!brasero_dvdcss_create_scrambled_sectors_map (self, drive, map, handle, file, error))
			return FALSE;
	}

	return TRUE;
}

static gint
brasero_dvdcss_sort_ranges (gconstpointer a, gconstpointer b, gpointer user_data)
{
	const BraseroScrambledSectorRange *range_a = a;
	const BraseroScrambledSectorRange *range_b = b;

	return range_a->start - range_b->start;
}

static gpointer
brasero_dvdcss_write_image_thread (gpointer data)
{
	guchar buf [DVDCSS_BLOCK_SIZE * BRASERO_DVDCSS_I_BLOCKS];
	BraseroScrambledSectorRange *range = NULL;
	BraseroMedium *medium = NULL;
	BraseroVolFile *files = NULL;
	dvdcss_handle *handle = NULL;
	BraseroDrive *drive = NULL;
	BraseroDvdcssPrivate *priv;
	gint64 written_sectors = 0;
	BraseroDvdcss *self = data;
	BraseroTrack *track = NULL;
	guint64 remaining_sectors;
	FILE *output_fd = NULL;
	BraseroVolSrc *vol;
	gint64 volume_size;
	GQueue *map = NULL;

	brasero_job_set_use_average_rate (BRASERO_JOB (self), TRUE);
	brasero_job_set_current_action (BRASERO_JOB (self),
					BRASERO_BURN_ACTION_ANALYSING,
					_("Retrieving DVD keys"),
					FALSE);
	brasero_job_start_progress (BRASERO_JOB (self), FALSE);

	priv = BRASERO_DVDCSS_PRIVATE (self);

	/* get the contents of the DVD */
	brasero_job_get_current_track (BRASERO_JOB (self), &track);
	drive = brasero_track_disc_get_drive (BRASERO_TRACK_DISC (track));

	vol = brasero_volume_source_open_file (brasero_drive_get_device (drive), &priv->error);
	files = brasero_volume_get_files (vol,
					  0,
					  NULL,
					  NULL,
					  NULL,
					  &priv->error);
	brasero_volume_source_close (vol);
	if (!files)
		goto end;

	medium = brasero_drive_get_medium (drive);
	brasero_medium_get_data_size (medium, NULL, &volume_size);
	if (volume_size == -1) {
		priv->error = g_error_new (BRASERO_BURN_ERROR,
					   BRASERO_BURN_ERROR_GENERAL,
					   _("The size of the volume could not be retrieved"));
		goto end;
	}

	/* create a handle/open DVD */
	handle = dvdcss_open (brasero_drive_get_device (drive));
	if (!handle) {
		priv->error = g_error_new (BRASERO_BURN_ERROR,
					   BRASERO_BURN_ERROR_GENERAL,
					   _("Video DVD could not be opened"));
		goto end;
	}

	/* look through the files to get the ranges of encrypted sectors
	 * and cache the CSS keys while at it. */
	map = g_queue_new ();
	if (!brasero_dvdcss_create_scrambled_sectors_map (self, drive, map, handle, files, &priv->error))
		goto end;

	BRASERO_JOB_LOG (self, "DVD map created (keys retrieved)");

	g_queue_sort (map, brasero_dvdcss_sort_ranges, NULL);

	brasero_volume_file_free (files);
	files = NULL;

	if (dvdcss_seek (handle, 0, DVDCSS_NOFLAGS) < 0) {
		BRASERO_JOB_LOG (self, "Error initial seeking");
		priv->error = g_error_new (BRASERO_BURN_ERROR,
					   BRASERO_BURN_ERROR_GENERAL,
					   _("Error while reading video DVD (%s)"),
					   dvdcss_error (handle));
		goto end;
	}

	brasero_job_set_current_action (BRASERO_JOB (self),
					BRASERO_BURN_ACTION_DRIVE_COPY,
					_("Copying video DVD"),
					FALSE);

	brasero_job_start_progress (BRASERO_JOB (self), TRUE);

	remaining_sectors = volume_size;
	range = g_queue_pop_head (map);

	if (brasero_job_get_fd_out (BRASERO_JOB (self), NULL) != BRASERO_BURN_OK) {
		gchar *output = NULL;

		brasero_job_get_image_output (BRASERO_JOB (self), &output, NULL);
		output_fd = fopen (output, "w");
		if (!output_fd) {
			priv->error = g_error_new_literal (BRASERO_BURN_ERROR,
							   BRASERO_BURN_ERROR_GENERAL,
							   g_strerror (errno));
			g_free (output);
			goto end;
		}
		g_free (output);
	}

	while (remaining_sectors) {
		gint flag;
		guint64 num_blocks, data_size;

		if (priv->cancel)
			break;

		num_blocks = BRASERO_DVDCSS_I_BLOCKS;

		/* see if we are approaching the end of the dvd */
		if (num_blocks > remaining_sectors)
			num_blocks = remaining_sectors;

		/* see if we need to update the key */
		if (!range || written_sectors < range->start) {
			/* this is in a non scrambled sectors range */
			flag = DVDCSS_NOFLAGS;
	
			/* we don't want to mix scrambled and non scrambled sectors */
			if (range && written_sectors + num_blocks > range->start)
				num_blocks = range->start - written_sectors;
		}
		else {
			/* this is in a scrambled sectors range */
			flag = DVDCSS_READ_DECRYPT;

			/* see if we need to update the key */
			if (written_sectors == range->start) {
				int pos;

				pos = dvdcss_seek (handle, written_sectors, DVDCSS_SEEK_KEY);
				if (pos < 0) {
					BRASERO_JOB_LOG (self, "Error seeking");
					priv->error = g_error_new (BRASERO_BURN_ERROR,
								   BRASERO_BURN_ERROR_GENERAL,
								   _("Error while reading video DVD (%s)"),
								   dvdcss_error (handle));
					break;
				}
			}

			/* we don't want to mix scrambled and non scrambled sectors
			 * NOTE: range->end address is the next non scrambled sector */
			if (written_sectors + num_blocks > range->end)
				num_blocks = range->end - written_sectors;

			if (written_sectors + num_blocks == range->end) {
				/* update to get the next range of scrambled sectors */
				g_free (range);
				range = g_queue_pop_head (map);
			}
		}

		num_blocks = dvdcss_read (handle, buf, num_blocks, flag);
		if (num_blocks < 0) {
			BRASERO_JOB_LOG (self, "Error reading");
			priv->error = g_error_new (BRASERO_BURN_ERROR,
						   BRASERO_BURN_ERROR_GENERAL,
						   _("Error while reading video DVD (%s)"),
						   dvdcss_error (handle));
			break;
		}

		data_size = num_blocks * DVDCSS_BLOCK_SIZE;
		if (output_fd) {
			if (fwrite (buf, 1, data_size, output_fd) != data_size) {
                                int errsv = errno;

				priv->error = g_error_new (BRASERO_BURN_ERROR,
							   BRASERO_BURN_ERROR_GENERAL,
							   _("Data could not be written (%s)"),
							   g_strerror (errsv));
				break;
			}
		}
		else {
			BraseroBurnResult result;

			result = brasero_dvdcss_write_sector_to_fd (self,
								    buf,
								    data_size);
			if (result != BRASERO_BURN_OK)
				break;
		}

		written_sectors += num_blocks;
		remaining_sectors -= num_blocks;
		brasero_job_set_written_track (BRASERO_JOB (self), written_sectors * DVDCSS_BLOCK_SIZE);
	}

end:

	if (range)
		g_free (range);

	if (handle)
		dvdcss_close (handle);

	if (files)
		brasero_volume_file_free (files);

	if (output_fd)
		fclose (output_fd);

	if (map) {
		g_queue_foreach (map, (GFunc) g_free, NULL);
		g_queue_free (map);
	}

	if (!priv->cancel)
		priv->thread_id = g_idle_add (brasero_dvdcss_thread_finished, self);

	/* End thread */
	g_mutex_lock (priv->mutex);
	priv->thread = NULL;
	g_cond_signal (priv->cond);
	g_mutex_unlock (priv->mutex);

	g_thread_exit (NULL);

	return NULL;
}

static BraseroBurnResult
brasero_dvdcss_start (BraseroJob *job,
		      GError **error)
{
	BraseroDvdcss *self;
	BraseroJobAction action;
	BraseroDvdcssPrivate *priv;
	GError *thread_error = NULL;

	self = BRASERO_DVDCSS (job);
	priv = BRASERO_DVDCSS_PRIVATE (self);

	brasero_job_get_action (job, &action);
	if (action == BRASERO_JOB_ACTION_SIZE) {
		goffset blocks = 0;
		BraseroTrack *track;

		brasero_job_get_current_track (job, &track);
		brasero_track_get_size (track, &blocks, NULL);
		brasero_job_set_output_size_for_current_track (job,
							       blocks,
							       blocks * DVDCSS_BLOCK_SIZE);
		return BRASERO_BURN_NOT_RUNNING;
	}

	if (action != BRASERO_JOB_ACTION_IMAGE)
		return BRASERO_BURN_NOT_SUPPORTED;

	if (priv->thread)
		return BRASERO_BURN_RUNNING;

	if (!brasero_dvdcss_library_init (NULL))
		return BRASERO_BURN_ERR;

	g_mutex_lock (priv->mutex);
	priv->thread = g_thread_create (brasero_dvdcss_write_image_thread,
					self,
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

static void
brasero_dvdcss_stop_real (BraseroDvdcss *self)
{
	BraseroDvdcssPrivate *priv;

	priv = BRASERO_DVDCSS_PRIVATE (self);

	g_mutex_lock (priv->mutex);
	if (priv->thread) {
		priv->cancel = 1;
		g_cond_wait (priv->cond, priv->mutex);
		priv->cancel = 0;
	}
	g_mutex_unlock (priv->mutex);

	if (priv->thread_id) {
		g_source_remove (priv->thread_id);
		priv->thread_id = 0;
	}

	if (priv->error) {
		g_error_free (priv->error);
		priv->error = NULL;
	}
}

static BraseroBurnResult
brasero_dvdcss_stop (BraseroJob *job,
		     GError **error)
{
	BraseroDvdcss *self;

	self = BRASERO_DVDCSS (job);

	brasero_dvdcss_stop_real (self);
	return BRASERO_BURN_OK;
}

static void
brasero_dvdcss_class_init (BraseroDvdcssClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroJobClass *job_class = BRASERO_JOB_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroDvdcssPrivate));

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_dvdcss_finalize;

	job_class->start = brasero_dvdcss_start;
	job_class->stop = brasero_dvdcss_stop;
}

static void
brasero_dvdcss_init (BraseroDvdcss *obj)
{
	BraseroDvdcssPrivate *priv;

	priv = BRASERO_DVDCSS_PRIVATE (obj);

	priv->mutex = g_mutex_new ();
	priv->cond = g_cond_new ();
}

static void
brasero_dvdcss_finalize (GObject *object)
{
	BraseroDvdcssPrivate *priv;

	priv = BRASERO_DVDCSS_PRIVATE (object);

	brasero_dvdcss_stop_real (BRASERO_DVDCSS (object));

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
brasero_dvdcss_export_caps (BraseroPlugin *plugin)
{
	GSList *output;
	GSList *input;

	brasero_plugin_define (plugin,
			       "dvdcss",
	                       NULL,
			       _("Copies CSS encrypted video DVDs to a disc image"),
			       "Philippe Rouquier",
			       0);

	/* to my knowledge, css can only be applied to pressed discs so no need
	 * to specify anything else but ROM */
	output = brasero_caps_image_new (BRASERO_PLUGIN_IO_ACCEPT_FILE|
					 BRASERO_PLUGIN_IO_ACCEPT_PIPE,
					 BRASERO_IMAGE_FORMAT_BIN);
	input = brasero_caps_disc_new (BRASERO_MEDIUM_DVD|
				       BRASERO_MEDIUM_DUAL_L|
				       BRASERO_MEDIUM_ROM|
				       BRASERO_MEDIUM_CLOSED|
				       BRASERO_MEDIUM_HAS_DATA|
				       BRASERO_MEDIUM_PROTECTED);

	brasero_plugin_link_caps (plugin, output, input);

	g_slist_free (input);
	g_slist_free (output);
}

G_MODULE_EXPORT void
brasero_plugin_check_config (BraseroPlugin *plugin)
{
	brasero_dvdcss_library_init (plugin);
}
