/***************************************************************************
 *            burn-dvdcss.c
 *
 *  lun aoû 21 14:34:32 2006
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

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <gmodule.h>

#include <nautilus-burn-drive.h>

#include "burn-basics.h"
#include "burn-debug.h"
#include "burn-job.h"
#include "burn-plugin.h"
#include "burn-dvdcss.h"
#include "burn-dvdcss-private.h"
#include "burn-volume.h"
#include "brasero-ncb.h"

BRASERO_PLUGIN_BOILERPLATE (BraseroDvdcss, brasero_dvdcss, BRASERO_TYPE_JOB, BraseroJob);

struct _BraseroDvdcssPrivate {
	GError *error;
	GThread *thread;
	guint thread_id;

	guint cancel:1;
};
typedef struct _BraseroDvdcssPrivate BraseroDvdcssPrivate;

#define BRASERO_DVDCSS_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_DVDCSS, BraseroDvdcssPrivate))

#define BRASERO_DVDCSS_I_BLOCKS	16

static GObjectClass *parent_class = NULL;

static gboolean
brasero_dvdcss_library_init (GError **error)
{
	GModule *module;
	gpointer address;
	gchar *dvdcss_interface_2 = NULL;

	if (css_ready)
		return TRUE;

	/* load libdvdcss library and see the version (min is 1.2.0) */
	module = g_module_open ("libdvdcss.so", G_MODULE_BIND_LOCAL);
	if (!module)
		goto error_doesnt_exist;

	if (!g_module_symbol (module, "dvdcss_interface_2", &address))
		goto error_version;

	dvdcss_interface_2 = address;
	BRASERO_BURN_LOG ("libdvdcss version %c.%c.%c\n",
			  (guchar) dvdcss_interface_2 [0],
			  (guchar) dvdcss_interface_2 [1],
			  (guchar) dvdcss_interface_2 [2]);

	if (!g_module_symbol (module, "dvdcss_open", &address))
		goto error_loading;
	dvdcss_open = address;

	if (!g_module_symbol (module, "dvdcss_close", &address))
		goto error_loading;
	dvdcss_close = address;

	if (!g_module_symbol (module, "dvdcss_read", &address))
		goto error_loading;
	dvdcss_read = address;

	if (!g_module_symbol (module, "dvdcss_seek", &address))
		goto error_loading;
	dvdcss_seek = address;

	if (!g_module_symbol (module, "dvdcss_error", &address))
		goto error_loading;
	dvdcss_error = address;

	css_ready = TRUE;
	return TRUE;

error_doesnt_exist:
	g_set_error (error,
		     BRASERO_BURN_ERROR,
		     BRASERO_BURN_ERROR_GENERAL,
		     _("encrypted DVD: please, install libdvdcss version 1.2.x"));
	return FALSE;

error_version:
	g_set_error (error,
		     BRASERO_BURN_ERROR,
		     BRASERO_BURN_ERROR_GENERAL,
		     _("libdvdcss version %s is not supported.\nPlease, install libdvdcss version 1.2.x"),
		     dvdcss_interface_2);
	g_module_close (module);
	return FALSE;


error_loading:
	g_set_error (error,
		     BRASERO_BURN_ERROR,
		     BRASERO_BURN_ERROR_GENERAL,
		     _("libdvdcss couldn't be loaded properly"));
	g_module_close (module);
	return FALSE;
}

static gboolean
brasero_dvdcss_thread_finished (gpointer data)
{
	gchar *image = NULL;
	BraseroTrackType type;
	BraseroTrack *track = NULL;
	BraseroDvdcss *self = data;
	BraseroDvdcssPrivate *priv;

	priv = BRASERO_DVDCSS_PRIVATE (self);
	priv->thread_id = 0;

	if (priv->error) {
		GError *error;

		error = priv->error;
		priv->error = NULL;
		brasero_job_error (BRASERO_JOB (self), error);
		return FALSE;
	}

	brasero_job_get_output_type (BRASERO_JOB (self), &type);
	track = brasero_track_new (type.type);

	brasero_job_get_image_output (BRASERO_JOB (self),
				      &image,
				      NULL);
	brasero_track_set_image_source (track,
					image,
					NULL,
					BRASERO_IMAGE_FORMAT_BIN);

	brasero_job_add_track (BRASERO_JOB (self), track);
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
				 buffer + bytes_written,
				 bytes_remaining);

		if (priv->cancel)
			break;

		if (written != bytes_remaining) {
			if (errno != EINTR && errno != EAGAIN) {
				/* unrecoverable error */
				priv->error = g_error_new (BRASERO_BURN_ERROR,
							   BRASERO_BURN_ERROR_GENERAL,
							   _("the data couldn't be written to the pipe (%i: %s)"),
							   errno,
							   strerror (errno));
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
brasero_dvdcss_create_scrambled_sectors_map (GQueue *map,
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

				range = g_new0 (BraseroScrambledSectorRange, 1);
				range->start = file->specific.file.address_block;
				range->end = range->start + (file->specific.file.size_bytes / DVDCSS_BLOCK_SIZE);

				g_queue_push_head (map, range);

				if (dvdcss_seek (handle, range->start, DVDCSS_SEEK_KEY) != range->start) {
					g_set_error (error,
						     BRASERO_BURN_ERROR,
						     BRASERO_BURN_ERROR_GENERAL,
						     _("Error reading video DVD (%s)"),
						     dvdcss_error (handle));
					return FALSE;
				}
			}
		}
		else if (!brasero_dvdcss_create_scrambled_sectors_map (map, handle, file, error))
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
	NautilusBurnDrive *drive = NULL;
	BraseroVolFile *files = NULL;
	dvdcss_handle *handle = NULL;
	BraseroDvdcssPrivate *priv;
	gint64 written_sectors = 0;
	BraseroDvdcss *self = data;
	BraseroTrack *track = NULL;
	guint64 remaining_sectors;
	FILE *output_fd = NULL;
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
	drive = brasero_track_get_drive_source (track);
	files = brasero_volume_get_files (NCB_DRIVE_GET_DEVICE (drive),
					  0,
					  NULL,
					  NULL,
					  NULL,
					  &priv->error);
	if (!files)
		goto end;

	NCB_MEDIA_GET_DATA_SIZE (drive, NULL, &volume_size);
	if (volume_size == -1) {
		priv->error = g_error_new (BRASERO_BURN_ERROR,
					   BRASERO_BURN_ERROR_GENERAL,
					   _("the size of the volume couln't be retrieved"));
		goto end;
	}

	/* create a handle/open DVD */
	handle = dvdcss_open (NCB_DRIVE_GET_DEVICE (drive));
	if (!handle) {
		priv->error = g_error_new (BRASERO_BURN_ERROR,
					   BRASERO_BURN_ERROR_GENERAL,
					   _("DVD could not be opened"));
		goto end;
	}

	/* look through the files to get the ranges of encrypted sectors
	 * and cache the CSS keys while at it. */
	map = g_queue_new ();
	if (!brasero_dvdcss_create_scrambled_sectors_map (map, handle, files, &priv->error))
		goto end;

	g_queue_sort (map, brasero_dvdcss_sort_ranges, NULL);

	brasero_volume_file_free (files);
	files = NULL;

	if (dvdcss_seek (handle, 0, DVDCSS_NOFLAGS) != 0) {
		priv->error = g_error_new (BRASERO_BURN_ERROR,
					   BRASERO_BURN_ERROR_GENERAL,
					   _("Error reading video DVD (%s)"),
					   dvdcss_error (handle));
		goto end;
	}

	brasero_job_set_current_action (BRASERO_JOB (self),
					BRASERO_BURN_ACTION_DRIVE_COPY,
					_("Copying Video DVD"),
					FALSE);

	brasero_job_start_progress (BRASERO_JOB (self), TRUE);

	remaining_sectors = volume_size;
	range = g_queue_pop_head (map);

	if (brasero_job_get_fd_out (BRASERO_JOB (self), NULL) != BRASERO_BURN_OK) {
		gchar *output = NULL;

		brasero_job_get_image_output (BRASERO_JOB (self), &output, NULL);
		output_fd = fopen (output, "w");
		g_free (output);

		if (!output_fd) {
			priv->error = g_error_new (BRASERO_BURN_ERROR,
						   BRASERO_BURN_ERROR_GENERAL,
						   strerror (errno));
			goto end;
		}
	}

	while (remaining_sectors) {
		gint flag, num_blocks, data_size;

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
			if (written_sectors == range->start
			&&  dvdcss_seek (handle, written_sectors, DVDCSS_SEEK_KEY) != written_sectors) {
				priv->error = g_error_new (BRASERO_BURN_ERROR,
							   BRASERO_BURN_ERROR_GENERAL,
							   _("Error reading video DVD (%s)"),
							   dvdcss_error (handle));
				break;
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

		if (dvdcss_read (handle, buf, num_blocks, flag) != num_blocks) {
			priv->error = g_error_new (BRASERO_BURN_ERROR,
						   BRASERO_BURN_ERROR_GENERAL,
						   _("Error reading video DVD (%s)"),
						   dvdcss_error (handle));
			break;
		}

		data_size = num_blocks * DVDCSS_BLOCK_SIZE;
		if (output_fd) {
			if (fwrite (buf, 1, data_size, output_fd) != data_size) {
				priv->error = g_error_new (BRASERO_BURN_ERROR,
							   BRASERO_BURN_ERROR_GENERAL,
							   _("the data couldn't be written to the file (%i: %s)"),
							   errno,
							   strerror (errno));
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
		brasero_job_set_written (BRASERO_JOB (self), written_sectors * DVDCSS_BLOCK_SIZE);
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

	priv->thread = NULL;
	g_thread_exit (NULL);

	return NULL;
}

static BraseroBurnResult
brasero_dvdcss_start (BraseroJob *job,
		      GError **error)
{
	BraseroDvdcss *self;
	BraseroDvdcssPrivate *priv;

	self = BRASERO_DVDCSS (job);
	priv = BRASERO_DVDCSS_PRIVATE (self);

	if (priv->thread)
		return BRASERO_BURN_RUNNING;

	if (!brasero_dvdcss_library_init (error))
		return BRASERO_BURN_ERR;

	priv->thread = g_thread_create (brasero_dvdcss_write_image_thread,
					self,
					TRUE,
					error);

	if (!priv->thread)
		return BRASERO_BURN_ERR;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_dvdcss_init_real (BraseroJob *job,
			  GError **error)
{
	BraseroJobAction action;

	brasero_job_get_action (job, &action);
	if (action == BRASERO_JOB_ACTION_SIZE) {
		gint64 blocks = 0;
		BraseroTrack *track;

		brasero_job_get_current_track (job, &track);
		brasero_track_get_disc_data_size (track, &blocks, NULL);
		brasero_job_set_output_size_for_current_track (job,
							       blocks,
							       blocks * DVDCSS_BLOCK_SIZE);
		return BRASERO_BURN_NOT_RUNNING;
	}

	if (action != BRASERO_JOB_ACTION_IMAGE)
		return BRASERO_BURN_NOT_SUPPORTED;

	return BRASERO_BURN_OK;
}

static void
brasero_dvdcss_stop_real (BraseroDvdcss *self)
{
	BraseroDvdcssPrivate *priv;

	priv = BRASERO_DVDCSS_PRIVATE (self);
	if (priv->thread) {
		priv->cancel = 1;
		g_thread_join (priv->thread);
		priv->cancel = 0;
	}

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

	job_class->init = brasero_dvdcss_init_real;
	job_class->start = brasero_dvdcss_start;
	job_class->stop = brasero_dvdcss_stop;
}

static void
brasero_dvdcss_init (BraseroDvdcss *obj)
{ }

static void
brasero_dvdcss_finalize (GObject *object)
{
	brasero_dvdcss_stop_real (BRASERO_DVDCSS (object));
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static BraseroBurnResult
brasero_dvdcss_export_caps (BraseroPlugin *plugin, gchar **error)
{
	GError *gerror = NULL;
	GSList *output;
	GSList *input;

	brasero_plugin_define (plugin,
			       "dvdcss",
			       _("Dvdcss allows to read css encrypted video DVDs"),
			       "Philippe Rouquier",
			       0);

	/* see if libdvdcss can be initted */
	if (!brasero_dvdcss_library_init (&gerror)) {
		if (gerror) {
			*error = g_strdup (gerror->message);
			g_error_free (gerror);
		}
		return BRASERO_BURN_ERR;
	}

	/* to my knowledge, css can only be applied to pressed discs so no need
	 * to specify anything else but ROM */
	output = brasero_caps_image_new (BRASERO_PLUGIN_IO_ACCEPT_FILE|
					 BRASERO_PLUGIN_IO_ACCEPT_PIPE,
					 BRASERO_IMAGE_FORMAT_BIN);
	input = brasero_caps_disc_new (BRASERO_MEDIUM_DVD|
				       BRASERO_MEDIUM_ROM|
				       BRASERO_MEDIUM_CLOSED|
				       BRASERO_MEDIUM_HAS_DATA|
				       BRASERO_MEDIUM_PROTECTED);

	brasero_plugin_link_caps (plugin, output, input);

	g_slist_free (input);
	g_slist_free (output);

	return BRASERO_BURN_OK;
}
