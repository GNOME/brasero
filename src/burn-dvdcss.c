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

/* This for large file support */
#define _GNU_SOURCE 1

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

#include <nautilus-burn-drive.h>

#include "burn-basics.h"
#include "burn-common.h"
#include "burn-caps.h"
#include "burn-imager.h"
#include "burn-dvdcss.h"
#include "burn-dvdcss-private.h"
#include "burn-volume.h"
#include "brasero-ncb.h"

static void brasero_dvdcss_class_init (BraseroDvdcssClass *klass);
static void brasero_dvdcss_init (BraseroDvdcss *sp);
static void brasero_dvdcss_finalize (GObject *object);

static void brasero_dvdcss_iface_init_image (BraseroImagerIFace *iface);

static BraseroBurnResult
brasero_dvdcss_set_source (BraseroJob *job,
			   const BraseroTrackSource *source,
			   GError **error);
static BraseroBurnResult
brasero_dvdcss_set_output_type (BraseroImager *imager,
				BraseroTrackSourceType type,
				BraseroImageFormat format,
				GError **error);
static BraseroBurnResult
brasero_dvdcss_set_output (BraseroImager *imager,
			   const gchar *path,
			   gboolean overwrite,
			   gboolean clean,
			   GError **error);

static BraseroBurnResult
brasero_dvdcss_start (BraseroJob *job,
			int in_fd,
			int *out_fd,
			GError **error);
static BraseroBurnResult
brasero_dvdcss_stop (BraseroJob *job,
		     BraseroBurnResult retval,
		     GError **error);
static BraseroBurnResult
brasero_dvdcss_get_track (BraseroImager *imager,
			  BraseroTrackSource **track,
			  GError **error);

static BraseroBurnResult
brasero_dvdcss_get_track_type (BraseroImager *imager,
				BraseroTrackSourceType *type,
				BraseroImageFormat *format);
static BraseroBurnResult
brasero_dvdcss_get_size (BraseroImager *imager,
			 gint64 *size,
			 gboolean sectors,
			 GError **error);

typedef enum {
	BRASERO_DVDCSS_ACTION_NONE				= 0,
	BRASERO_DVDCSS_ACTION_WRITE_IMAGE			= 1 << 0,
} BraseroDvdcssAction;

struct _BraseroDvdcssPrivate {
	BraseroBurnCaps *caps;

	BraseroDvdcssAction action;

	BraseroTrackSource *source;
	BraseroImageFormat image_format;

	union {
	FILE *file;
	int pipe;
	} out;

	gchar *output;

	GError *error;
	GThread *thread;
	gint thread_id;
	gint cancel:1;

	gint clean:1;
	gint to_file:1;
	gint overwrite:1;

	gint tmp_output:1;

	gint image_ready:1;
};

#define BRASERO_DVDCSS_I_BLOCKS	16

static GObjectClass *parent_class = NULL;

GType
brasero_dvdcss_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroDvdcssClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_dvdcss_class_init,
			NULL,
			NULL,
			sizeof (BraseroDvdcss),
			0,
			(GInstanceInitFunc)brasero_dvdcss_init,
		};

		static const GInterfaceInfo imager_info =
		{
			(GInterfaceInitFunc) brasero_dvdcss_iface_init_image,
			NULL,
			NULL
		};

		type = g_type_register_static (BRASERO_TYPE_JOB, 
					       "BraseroDvdcss",
					       &our_info,
					       0);

		g_type_add_interface_static (type,
					     BRASERO_TYPE_IMAGER,
					     &imager_info);
	}

	return type;
}

static gboolean
brasero_dvdcss_library_init (BraseroDvdcss *self, GError **error)
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
	BRASERO_JOB_LOG (self, 
			 "libdvdcss version %c.%c.%c\n",
			 (guchar) dvdcss_interface_2 [0],
			 (guchar) dvdcss_interface_2 [1],
			 (guchar) dvdcss_interface_2 [3]);

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

static void
brasero_dvdcss_iface_init_image (BraseroImagerIFace *iface)
{
	iface->get_size = brasero_dvdcss_get_size;
	iface->get_track = brasero_dvdcss_get_track;
	iface->get_track_type = brasero_dvdcss_get_track_type;
	iface->set_output = brasero_dvdcss_set_output;
	iface->set_output_type = brasero_dvdcss_set_output_type;
}

static void
brasero_dvdcss_class_init (BraseroDvdcssClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroJobClass *job_class = BRASERO_JOB_CLASS (klass);

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_dvdcss_finalize;

	job_class->set_source = brasero_dvdcss_set_source;
	job_class->start = brasero_dvdcss_start;
	job_class->stop = brasero_dvdcss_stop;
}

static void
brasero_dvdcss_init (BraseroDvdcss *obj)
{
	obj->priv = g_new0 (BraseroDvdcssPrivate, 1);
	obj->priv->caps = brasero_burn_caps_get_default ();
}

static void
brasero_dvdcss_stop_real (BraseroDvdcss *self)
{
	if (self->priv->thread) {
		self->priv->cancel = 1;
		g_thread_join (self->priv->thread);
		self->priv->cancel = 0;
	}

	if (self->priv->thread_id) {
		g_source_remove (self->priv->thread_id);
		self->priv->thread_id = 0;
	}

	if (self->priv->to_file) {
		if (self->priv->out.file) {
			fclose (self->priv->out.file);
			self->priv->out.file = NULL;
		}
	}
	else if (self->priv->out.pipe > 0) {
		close (self->priv->out.pipe) ;
		self->priv->out.pipe = 0;
	}

	if (self->priv->error) {
		g_error_free (self->priv->error);
		self->priv->error = NULL;
	}

	self->priv->action = BRASERO_DVDCSS_ACTION_NONE;
}

static void
brasero_dvdcss_clean_output (BraseroDvdcss *self)
{
	if (self->priv->image_ready
	&& (self->priv->clean || self->priv->tmp_output))
		g_remove (self->priv->output);
	
	self->priv->image_ready = 0;
}

static void
brasero_dvdcss_finalize (GObject *object)
{
	BraseroDvdcss *cobj;

	cobj = BRASERO_DVDCSS (object);

	brasero_dvdcss_stop_real (cobj);
	brasero_dvdcss_clean_output (cobj);

	if (cobj->priv->source) {
		brasero_track_source_free (cobj->priv->source);
		cobj->priv->source = NULL;
	}

	if (cobj->priv->output) {
		g_free (cobj->priv->output),
		cobj->priv->output = NULL;
	}

	if (cobj->priv->caps) {
		g_object_unref (cobj->priv->caps);
		cobj->priv->caps = NULL;
	}

	g_free (cobj->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

BraseroDvdcss *
brasero_dvdcss_new ()
{
	BraseroDvdcss *obj;
	
	obj = BRASERO_DVDCSS (g_object_new (BRASERO_TYPE_DVDCSS, NULL));
	
	return obj;
}

static BraseroBurnResult
brasero_dvdcss_set_source (BraseroJob *job,
			     const BraseroTrackSource *source,
			     GError **error)
{
	BraseroDvdcss *self;

	self = BRASERO_DVDCSS (job);

	if (self->priv->source) {
		brasero_track_source_free (self->priv->source);
		self->priv->source = NULL;
	}

	brasero_dvdcss_clean_output (self);

	if (source->type != BRASERO_TRACK_SOURCE_DISC)
		BRASERO_JOB_NOT_SUPPORTED (self);

	self->priv->source = brasero_track_source_copy (source);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_dvdcss_set_output_type (BraseroImager *imager,
				  BraseroTrackSourceType type,
				  BraseroImageFormat format,
				  GError **error)
{
	BraseroDvdcss *self;

	self = BRASERO_DVDCSS (imager);

	if (type != BRASERO_TRACK_SOURCE_IMAGE
	&&  type != BRASERO_TRACK_SOURCE_DEFAULT)
		BRASERO_JOB_NOT_SUPPORTED (self);

	if (format != BRASERO_IMAGE_FORMAT_NONE
	&&  format != BRASERO_IMAGE_FORMAT_ANY)
		BRASERO_JOB_NOT_SUPPORTED (self);

	if (self->priv->image_format == format)
		return BRASERO_BURN_OK;

	brasero_dvdcss_clean_output (self);
	self->priv->image_format = format;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_dvdcss_set_output (BraseroImager *imager,
			   const gchar *output,
			   gboolean overwrite,
			   gboolean clean,
			   GError **error)
{
	BraseroDvdcss *self;

	self = BRASERO_DVDCSS (imager);

	if (self->priv->output) {
		brasero_dvdcss_clean_output (self);

		g_free (self->priv->output);
		self->priv->output = NULL;
	}

	if (output) {
		self->priv->output = g_strdup (output);
		self->priv->tmp_output = 0;
	}

	self->priv->overwrite = overwrite;
	self->priv->clean = clean;

	return BRASERO_BURN_OK;
}

static gboolean
brasero_dvdcss_thread_finished (gpointer data)
{
	BraseroDvdcss *self = data;

	self->priv->thread_id = 0;

	if (self->priv->error) {
		GError *error;

		error = self->priv->error;
		self->priv->error = NULL;
		brasero_job_error (BRASERO_JOB (self), error);
	}
	else
		brasero_job_finished (BRASERO_JOB (self));

	return FALSE;
}

static BraseroBurnResult
brasero_dvdcss_write_sector_to_fd (BraseroDvdcss *self,
				   gpointer buffer,
				   gint bytes_remaining)
{
	gint bytes_written = 0;

	while (bytes_remaining) {
		gint written;

		written = write (self->priv->out.pipe,
				 buffer + bytes_written,
				 bytes_remaining);

		if (self->priv->cancel)
			break;

		if (written != bytes_remaining) {
			if (errno != EINTR && errno != EAGAIN) {
				/* unrecoverable error */
				self->priv->error = g_error_new (BRASERO_BURN_ERROR,
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
	gint64 written_sectors = 0;
	BraseroDvdcss *self = data;
	guint64 remaining_sectors;
	gint64 volume_size;
	GQueue *map = NULL;

	BRASERO_JOB_TASK_SET_USE_AVERAGE_RATE (self, TRUE);
	BRASERO_JOB_TASK_SET_ACTION (self,
				     BRASERO_BURN_ACTION_ANALYSING,
				     _("Retrieving DVD keys"),
				     FALSE);
	BRASERO_JOB_TASK_START_PROGRESS (self, FALSE);

	/* get the contents of the DVD */
	drive = self->priv->source->contents.drive.disc;
	files = brasero_volume_get_files (NCB_DRIVE_GET_DEVICE (drive),
					  0,
					  NULL,
					  NULL,
					  NULL,
					  &self->priv->error);
	if (!files)
		goto end;

	NCB_MEDIA_GET_DATA_SIZE (self->priv->source->contents.drive.disc, NULL, &volume_size);
	if (volume_size == -1) {
		self->priv->error = g_error_new (BRASERO_BURN_ERROR,
						 BRASERO_BURN_ERROR_GENERAL,
						 _("the size of the volume couln't be retrieved"));
		goto end;
	}

	/* create a handle/open DVD */
	handle = dvdcss_open (g_strdup (NCB_DRIVE_GET_DEVICE (drive)));
	if (!handle) {
		self->priv->error = g_error_new (BRASERO_BURN_ERROR,
						 BRASERO_BURN_ERROR_GENERAL,
						 _("DVD could not be opened"));
		goto end;
	}

	/* look through the files to get the ranges of encrypted sectors
	 * and cache the CSS keys while at it. */
	map = g_queue_new ();
	if (!brasero_dvdcss_create_scrambled_sectors_map (map, handle, files, &self->priv->error))
		goto end;

	g_queue_sort (map, brasero_dvdcss_sort_ranges, NULL);

	brasero_volume_file_free (files);
	files = NULL;

	if (dvdcss_seek (handle, 0, DVDCSS_NOFLAGS) != 0) {
		self->priv->error = g_error_new (BRASERO_BURN_ERROR,
						 BRASERO_BURN_ERROR_GENERAL,
						 _("Error reading video DVD (%s)"),
						 dvdcss_error (handle));
		goto end;
	}

	BRASERO_JOB_TASK_SET_ACTION (self,
				     BRASERO_BURN_ACTION_DRIVE_COPY,
				     _("Copying Video DVD"),
				     FALSE);
	BRASERO_JOB_TASK_SET_TOTAL (self, volume_size * DVDCSS_BLOCK_SIZE);
	BRASERO_JOB_TASK_START_PROGRESS (self, TRUE);

	remaining_sectors = volume_size;
	range = g_queue_pop_head (map);

	while (remaining_sectors) {
		gint flag, num_blocks, data_size;

		if (self->priv->cancel)
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
				self->priv->error = g_error_new (BRASERO_BURN_ERROR,
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
			self->priv->error = g_error_new (BRASERO_BURN_ERROR,
							 BRASERO_BURN_ERROR_GENERAL,
							 _("Error reading video DVD (%s)"),
							 dvdcss_error (handle));
			break;
		}

		data_size = num_blocks * DVDCSS_BLOCK_SIZE;
		if (self->priv->to_file) {
			if (fwrite (buf, 1, data_size, self->priv->out.file) != data_size) {
				self->priv->error = g_error_new (BRASERO_BURN_ERROR,
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
		BRASERO_JOB_TASK_SET_WRITTEN (self, written_sectors * DVDCSS_BLOCK_SIZE);
	}

end:

	if (range)
		g_free (range);

	if (handle)
		dvdcss_close (handle);

	if (files)
		brasero_volume_file_free (files);

	if (map) {
		g_queue_foreach (map, (GFunc) g_free, NULL);
		g_queue_free (map);
	}

	if (self->priv->to_file){
		fclose (self->priv->out.file);
		self->priv->out.file = NULL;
	}
	else {
		close (self->priv->out.pipe);
		self->priv->out.pipe = 0;
	}

	if (!self->priv->cancel)
		self->priv->thread_id = g_idle_add (brasero_dvdcss_thread_finished, self);

	self->priv->thread = NULL;
	g_thread_exit (NULL);

	return NULL;
}

static BraseroBurnResult
brasero_dvdcss_write_image_to_fd (BraseroDvdcss *self,
				  int *out_fd,
				  GError **error)
{
	int pipe_out [2];
	BraseroBurnResult result;

	result = brasero_common_create_pipe (pipe_out, error);
	if (result != BRASERO_BURN_OK)
		return result;

	self->priv->to_file= 0;

	*out_fd = pipe_out [0];
	self->priv->out.pipe = pipe_out [1];
	self->priv->thread = g_thread_create (brasero_dvdcss_write_image_thread,
					      self,
					      TRUE,
					      error);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_dvdcss_write_image_to_file (BraseroDvdcss *self,
				    GError **error)
{
	BraseroBurnResult result;

	self->priv->to_file = 1;

	if (self->priv->tmp_output) {
		brasero_dvdcss_clean_output (self);
		g_free (self->priv->output);
		self->priv->output = NULL;
	}

	if (!self->priv->output)
		self->priv->tmp_output = 1;

	result = brasero_burn_common_check_output (&self->priv->output,
						   BRASERO_IMAGE_FORMAT_NONE,
						   TRUE,
						   self->priv->overwrite,
						   NULL,
						   error);
	if (result != BRASERO_BURN_OK)
		return result;

	self->priv->out.file = fopen64 (self->priv->output, "w");
	if (!self->priv->out.file) {
		g_set_error (error, 
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     strerror (errno));
		return BRASERO_BURN_ERR;
	}

	self->priv->thread = g_thread_create (brasero_dvdcss_write_image_thread,
					      self,
					      TRUE,
					      error);
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_dvdcss_start (BraseroJob *job,
			int in_fd,
			int *out_fd,
			GError **error)
{
	BraseroDvdcss *self;
	BraseroBurnResult result;

	self = BRASERO_DVDCSS (job);
	if (in_fd > -1)
		BRASERO_JOB_NOT_SUPPORTED (self);

	if (self->priv->thread)
		return BRASERO_BURN_RUNNING;

	if (!brasero_dvdcss_library_init (self, error))
		return BRASERO_BURN_ERR;

	self->priv->action |= BRASERO_DVDCSS_ACTION_WRITE_IMAGE;

	if (!out_fd)
		result = brasero_dvdcss_write_image_to_file (self, error);
	else
		result = brasero_dvdcss_write_image_to_fd (self, out_fd, error);

	if (result != BRASERO_BURN_OK)
		return result;

	if (!self->priv->thread)
		return BRASERO_BURN_ERR;

	return result;
}

static BraseroBurnResult
brasero_dvdcss_stop (BraseroJob *job,
		     BraseroBurnResult retval,
		     GError **error)
{
	BraseroDvdcss *self;

	self = BRASERO_DVDCSS (job);

	brasero_dvdcss_stop_real (self);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_dvdcss_get_track (BraseroImager *imager,
			    BraseroTrackSource **track,
			    GError **error)
{
	BraseroDvdcss *self;
	BraseroTrackSource *retval;

	self = BRASERO_DVDCSS (imager);

	if (!self->priv->image_ready) {
		BraseroBurnResult result;

		result = brasero_job_run (BRASERO_JOB (imager), error);
		if (result != BRASERO_BURN_OK)
			return result;

		self->priv->image_ready = 1;
	}

	retval = g_new0 (BraseroTrackSource, 1);
	retval->type = BRASERO_TRACK_SOURCE_IMAGE;

	retval->format = BRASERO_IMAGE_FORMAT_NONE;
	if (self->priv->image_format & BRASERO_IMAGE_FORMAT_JOLIET)
		retval->format |= BRASERO_IMAGE_FORMAT_JOLIET;

	retval->contents.image.image = g_strdup_printf ("file://%s", self->priv->output);

	*track = retval;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_dvdcss_get_size (BraseroImager *imager,
			 gint64 *size_return,
			 gboolean sectors,
			 GError **error)
{
	gint64 blocks;
	BraseroDvdcss *self;

	self = BRASERO_DVDCSS (imager);

	NCB_MEDIA_GET_DATA_SIZE (self->priv->source->contents.drive.disc, NULL, &blocks);
	if (blocks == -1) {
		g_set_error (error,
			    BRASERO_BURN_ERROR,
			    BRASERO_BURN_ERROR_GENERAL,
			    _("the size of the volume couln't be retrieved"));
		return BRASERO_BURN_ERR;
	}

	if (sectors)
		*size_return = blocks;
	else
		*size_return = blocks * DVDCSS_BLOCK_SIZE;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_dvdcss_get_track_type (BraseroImager *imager,
			       BraseroTrackSourceType *type,
			       BraseroImageFormat *format)
{
	BraseroDvdcss *self;

	self = BRASERO_DVDCSS (imager);

	if (!self->priv->source)
		BRASERO_JOB_NOT_READY (self);

	*format = BRASERO_IMAGE_FORMAT_NONE;
	*type = BRASERO_TRACK_SOURCE_IMAGE;

	return BRASERO_BURN_OK;
}

