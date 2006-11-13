/***************************************************************************
 *            burn-libisofs.c
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

#include <libgnomevfs/gnome-vfs.h>

#include "burn-basics.h"
#include "burn-common.h"
#include "burn-caps.h"
#include "burn-imager.h"
#include "burn-libisofs.h"

#ifdef HAVE_LIBBURN

#include <libburn/libisofs.h>
#include <libburn/libburn.h>

static void brasero_libisofs_class_init (BraseroLibisofsClass *klass);
static void brasero_libisofs_init (BraseroLibisofs *sp);
static void brasero_libisofs_finalize (GObject *object);

static void brasero_libisofs_iface_init_image (BraseroImagerIFace *iface);

static BraseroBurnResult
brasero_libisofs_set_source (BraseroJob *job,
			     const BraseroTrackSource *source,
			     GError **error);
static BraseroBurnResult
brasero_libisofs_set_output_type (BraseroImager *imager,
				  BraseroTrackSourceType type,
				  BraseroImageFormat format,
				  GError **error);
static BraseroBurnResult
brasero_libisofs_set_append (BraseroImager *imager,
			     NautilusBurnDrive *drive,
			     gboolean merge,
			     GError **error);
static BraseroBurnResult
brasero_libisofs_set_output (BraseroImager *imager,
			     const gchar *path,
			     gboolean overwrite,
			     gboolean clean,
			     GError **error);

static BraseroBurnResult
brasero_libisofs_start (BraseroJob *job,
			int in_fd,
			int *out_fd,
			GError **error);
static BraseroBurnResult
brasero_libisofs_stop (BraseroJob *job,
		       BraseroBurnResult retval,
		       GError **error);
static BraseroBurnResult
brasero_libisofs_get_track (BraseroImager *imager,
			    BraseroTrackSource **track,
			    GError **error);

static BraseroBurnResult
brasero_libisofs_get_track_type (BraseroImager *imager,
				 BraseroTrackSourceType *type,
				 BraseroImageFormat *format);
static BraseroBurnResult
brasero_libisofs_get_size (BraseroImager *imager,
			   gint64 *size,
			   gboolean sectors,
			   GError **error);

typedef enum {
	BRASERO_LIBISOFS_ACTION_NONE,
	BRASERO_LIBISOFS_ACTION_CREATE_VOLUME,
	BRASERO_LIBISOFS_ACTION_CREATE_IMAGE,
} BraseroLibisofsAction;

struct _BraseroLibisofsPrivate {
	BraseroBurnCaps *caps;

	BraseroLibisofsAction action;

	BraseroTrackSource *source;
	BraseroImageFormat image_format;

	struct burn_source *libburn_src;

	FILE *file;
	int pipe_out;

	gchar *output;

	GError *error;
	GThread *thread;
	gint thread_id;
	gint cancel:1;

	gint clean:1;
	gint overwrite:1;

	gint iso_ready:1;
	gint iso_joliet_ready:1;
};

static GObjectClass *parent_class = NULL;

GType
brasero_libisofs_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroLibisofsClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_libisofs_class_init,
			NULL,
			NULL,
			sizeof (BraseroLibisofs),
			0,
			(GInstanceInitFunc)brasero_libisofs_init,
		};

		static const GInterfaceInfo imager_info =
		{
			(GInterfaceInitFunc) brasero_libisofs_iface_init_image,
			NULL,
			NULL
		};

		type = g_type_register_static (BRASERO_TYPE_JOB, 
					       "BraseroLibisofs",
					       &our_info,
					       0);

		g_type_add_interface_static (type,
					     BRASERO_TYPE_IMAGER,
					     &imager_info);
	}

	return type;
}

static void
brasero_libisofs_iface_init_image (BraseroImagerIFace *iface)
{
	iface->get_size = brasero_libisofs_get_size;
	iface->get_track = brasero_libisofs_get_track;
	iface->get_track_type = brasero_libisofs_get_track_type;
	iface->set_output = brasero_libisofs_set_output;
	iface->set_append = brasero_libisofs_set_append;
	iface->set_output_type = brasero_libisofs_set_output_type;
}

static void
brasero_libisofs_class_init (BraseroLibisofsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroJobClass *job_class = BRASERO_JOB_CLASS (klass);

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_libisofs_finalize;

	job_class->set_source = brasero_libisofs_set_source;
	job_class->start = brasero_libisofs_start;
	job_class->stop = brasero_libisofs_stop;
}

static void
brasero_libisofs_init (BraseroLibisofs *obj)
{
	obj->priv = g_new0(BraseroLibisofsPrivate, 1);
	obj->priv->caps = brasero_burn_caps_get_default ();
}

static void
brasero_libisofs_stop_real (BraseroLibisofs *self)
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

	if (self->priv->pipe_out > 0) {
		close (self->priv->pipe_out) ;
		self->priv->pipe_out = 0;
	}

	if (self->priv->file) {
		fclose (self->priv->file);
		self->priv->file = NULL;
	}

	if (self->priv->error) {
		g_error_free (self->priv->error);
		self->priv->error = NULL;
	}
}

static void
brasero_libisofs_clean_output (BraseroLibisofs *self)
{
	if (self->priv->iso_ready && self->priv->clean)
		g_remove (self->priv->output);
	
	self->priv->iso_ready = 0;

	if (self->priv->libburn_src) {
		burn_source_free (self->priv->libburn_src);
		self->priv->libburn_src = NULL;
	}
}

static void
brasero_libisofs_finalize (GObject *object)
{
	BraseroLibisofs *cobj;

	cobj = BRASERO_LIBISOFS (object);

	brasero_libisofs_stop_real (cobj);
	brasero_libisofs_clean_output (cobj);

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

BraseroLibisofs *
brasero_libisofs_new ()
{
	BraseroLibisofs *obj;
	
	obj = BRASERO_LIBISOFS (g_object_new (BRASERO_TYPE_LIBISOFS, NULL));
	
	return obj;
}

static gint
brasero_libisofs_sort_graft_points (gconstpointer a, gconstpointer b)
{
	const BraseroGraftPt *graft_a, *graft_b;
	gint len_a, len_b;

	graft_a = a;
	graft_b = b;

	/* we only want to know if:
	 * - a is a parent of b (a > b, retval < 0) 
	 * - b is a parent of a (b > a, retval > 0). */

	len_a = strlen (graft_a->path);
	len_b = strlen (graft_b->path);

	if (len_a < len_b
	&&  graft_b->path [len_a] == G_DIR_SEPARATOR
	&&  !strncmp (graft_a->path, graft_b->path, len_a))
		return -1;
		
	if (len_a > len_b
	&&  graft_a->path [len_b] == G_DIR_SEPARATOR
	&&  !strncmp (graft_b->path, graft_a->path, len_b))
		return 1;

	return 0;
}

static BraseroBurnResult
brasero_libisofs_set_source (BraseroJob *job,
			     const BraseroTrackSource *source,
			     GError **error)
{
	BraseroLibisofs *self;

	self = BRASERO_LIBISOFS (job);

	if (self->priv->source) {
		brasero_track_source_free (self->priv->source);
		self->priv->source = NULL;
	}

	brasero_libisofs_clean_output (self);

	if (source->type != BRASERO_TRACK_SOURCE_DATA)
		BRASERO_JOB_NOT_SUPPORTED (self);

	self->priv->source = brasero_track_source_copy (source);

	/* sort the graft points according to their paths. To add a graft a
	 * parent must have been inserted before */
	self->priv->source->contents.data.grafts = g_slist_sort (self->priv->source->contents.data.grafts,
								 brasero_libisofs_sort_graft_points);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_libisofs_set_output_type (BraseroImager *imager,
				  BraseroTrackSourceType type,
				  BraseroImageFormat format,
				  GError **error)
{
	BraseroLibisofs *self;

	self = BRASERO_LIBISOFS (imager);

	if (type != BRASERO_TRACK_SOURCE_IMAGE
	&&  type != BRASERO_TRACK_SOURCE_DEFAULT)
		BRASERO_JOB_NOT_SUPPORTED (self);

	if ((format & BRASERO_IMAGE_FORMAT_ISO) == 0)
		BRASERO_JOB_NOT_SUPPORTED (self);

	if (self->priv->image_format == format)
		return BRASERO_BURN_OK;

	brasero_libisofs_clean_output (self);
	self->priv->image_format = format;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_libisofs_set_append (BraseroImager *imager,
			     NautilusBurnDrive *drive,
			     gboolean merge,
			     GError **error)
{
	BraseroLibisofs *self;

	self = BRASERO_LIBISOFS (imager);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_libisofs_set_output (BraseroImager *imager,
			     const gchar *output,
			     gboolean overwrite,
			     gboolean clean,
			     GError **error)
{
	BraseroLibisofs *self;

	self = BRASERO_LIBISOFS (imager);

	if (self->priv->output) {
		brasero_libisofs_clean_output (self);

		g_free (self->priv->output);
		self->priv->output = NULL;
	}

	if (output)
		self->priv->output = g_strdup (output);

	self->priv->overwrite = overwrite;
	self->priv->clean = clean;

	return BRASERO_BURN_OK;
}

static gboolean
brasero_libisofs_thread_finished (gpointer data)
{
	BraseroLibisofs *self = data;

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

static gpointer
brasero_libisofs_create_volume_thread (gpointer data)
{
	BraseroLibisofs *self = data;
	BraseroTrackSource *source;
	struct iso_volume *volume;
	struct iso_volset *volset;
	gchar *publisher;
	GSList *excluded;
	GSList *iter;
	gint flags;

	/* FIXME!
	if (self->priv->volset)
		iso_volset_free (self->priv->volume); */

	publisher = g_strdup_printf ("Brasero-%i.%i.%i",
				     BRASERO_MAJOR_VERSION,
				     BRASERO_MINOR_VERSION,
				     BRASERO_SUB);

	volume = iso_volume_new (self->priv->source->contents.grafts.label,
				 publisher,
				 g_get_real_name ());
	g_free (publisher);

	source = self->priv->source;

	/* we add the globally excluded */
	for (excluded = source->contents.data.excluded; excluded; excluded = excluded->next) {
		gchar *uri;
		gchar *path;

		uri = excluded->data;
		path = gnome_vfs_get_local_path_from_uri (uri);
		iso_exclude_add_path (path);
		g_free (path);
	}

	BRASERO_JOB_TASK_START_PROGRESS (self, FALSE);

	for (iter = source->contents.data.grafts; iter; iter = iter->next) {
		GSList *excluded_path;
		BraseroGraftPt *graft;
		struct iso_tree_node *node;

		if (self->priv->cancel)
			goto end;

		graft = iter->data;

		/* now let's take care of the excluded files */
		excluded_path = NULL;
		for (excluded = graft->excluded; excluded; excluded = excluded->next) {
			gchar *uri;
			gchar *path;

			uri = excluded->data;
			path = gnome_vfs_get_local_path_from_uri (uri);
			iso_exclude_add_path (path);

			/* keep the path for later since we'll remove it */
			excluded_path = g_slist_prepend (excluded_path, path);
		}

		/* add the file/directory to the volume */
		if (graft->uri) {
			gchar *local_path;

			local_path = gnome_vfs_get_local_path_from_uri (graft->uri);
			node = iso_tree_volume_add_path (volume,
							 graft->path,
							 local_path);
			g_free (local_path);
		}
		else
			node = iso_tree_volume_add_new_dir (volume, graft->path);

		if (!node) {
			/* an error has occured, possibly libisofs hasn't been
			 * able to find a parent for this node */
			self->priv->error = g_error_new (BRASERO_BURN_ERROR,
							 BRASERO_BURN_ERROR_GENERAL,
							 _("a parent for the path (%s) could not be found in the tree"),
							 graft->path);
			goto end;
		}

		/* remove all path from exclusion */
		for (excluded = excluded_path; excluded; excluded = excluded->next) {
			gchar *path;

			path = excluded->data;
			iso_exclude_remove_path (path);
			g_free (path);
		}
		g_slist_free (excluded_path);
	}

end:

	/* clean the exclusion */
	iso_exclude_empty ();

	volset = iso_volset_new (volume, "VOLSETID");
	iso_volume_free (volume);

	flags = ((self->priv->image_format & BRASERO_IMAGE_FORMAT_JOLIET) ? ECMA119_JOLIET : 0);
	flags |= ECMA119_ROCKRIDGE;

	self->priv->libburn_src = iso_source_new_ecma119 (volset,
							  0,
							  2,
							  flags);

	BRASERO_JOB_TASK_SET_TOTAL (self, self->priv->libburn_src->get_size (self->priv->libburn_src));
	self->priv->thread_id = g_idle_add (brasero_libisofs_thread_finished, self);
	self->priv->thread = NULL;

	return NULL;
}

static BraseroBurnResult
brasero_libisofs_create_volume (BraseroLibisofs *self, GError **error)
{
	if (self->priv->thread)
		return BRASERO_BURN_RUNNING;

	self->priv->thread = g_thread_create (brasero_libisofs_create_volume_thread,
					      self,
					      TRUE,
					      error);
	if (!self->priv->thread)
		return BRASERO_BURN_ERR;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_libisofs_write_sector_to_fd (BraseroLibisofs *self,
				     gpointer buffer,
				     gint bytes_remaining)
{
	gint bytes_written = 0;

	while (bytes_remaining) {
		gint written;

		written = write (self->priv->pipe_out,
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

static gpointer
brasero_libisofs_write_image_to_fd_thread (gpointer data)
{
	const gint sector_size = 2048;
	BraseroLibisofs *self = data;
	BraseroBurnResult result;
	guchar buf [sector_size];
	gint64 written_sectors = 0;

	BRASERO_JOB_TASK_SET_TOTAL (self, self->priv->libburn_src->get_size (self->priv->libburn_src));
	BRASERO_JOB_TASK_START_PROGRESS (self, FALSE);

	while (self->priv->libburn_src->read (self->priv->libburn_src, buf, sector_size) == sector_size) {
		if (self->priv->cancel)
			break;

		result = brasero_libisofs_write_sector_to_fd (self,
							      buf,
							      sector_size);
		if (result != BRASERO_BURN_OK)
			break;

		written_sectors ++;
		BRASERO_JOB_TASK_SET_WRITTEN (self, written_sectors << 11);
	}

	close (self->priv->pipe_out);
	self->priv->pipe_out = -1;

	if (!self->priv->cancel)
		self->priv->thread_id = g_idle_add (brasero_libisofs_thread_finished, self);

	self->priv->thread = NULL;
	g_thread_exit (NULL);

	return NULL;
}

static BraseroBurnResult
brasero_libisofs_write_image_to_fd (BraseroLibisofs *self,
				    int *out_fd,
				    GError **error)
{
	int pipe_out [2];
	BraseroBurnResult result;

	result = brasero_common_create_pipe (pipe_out, error);

	if (self->priv->thread)
		return BRASERO_BURN_RUNNING;

	if (result != BRASERO_BURN_OK)
		return result;

	*out_fd = pipe_out [0];
	self->priv->pipe_out = pipe_out [1];
	self->priv->thread = g_thread_create (brasero_libisofs_write_image_to_fd_thread,
					      self,
					      TRUE,
					      error);
	if (!self->priv->thread)
		return BRASERO_BURN_ERR;

	BRASERO_JOB_TASK_SET_ACTION (self,
				     BRASERO_BURN_ACTION_CREATING_IMAGE,
				     NULL,
				     FALSE);

	return BRASERO_BURN_OK;
}

static gpointer
brasero_libisofs_write_image_to_file_thread (gpointer data)
{
	const gint sector_size = 2048;
	BraseroLibisofs *self = data;
	gint64 written_sectors = 0;
	guchar buf [sector_size];

	BRASERO_JOB_TASK_SET_TOTAL (self, self->priv->libburn_src->get_size (self->priv->libburn_src));
	BRASERO_JOB_TASK_START_PROGRESS (self, FALSE);

	while (self->priv->libburn_src->read (self->priv->libburn_src, buf, sector_size) == sector_size) {
		if (self->priv->cancel)
			break;

		if (fwrite (buf, 1, sector_size, self->priv->file) != sector_size) {
			self->priv->error = g_error_new (BRASERO_BURN_ERROR,
							 BRASERO_BURN_ERROR_GENERAL,
							 _("the data couldn't be written to the file (%i: %s)"),
							 errno,
							 strerror (errno));
			break;
		}

		if (self->priv->cancel)
			break;

		written_sectors ++;
		BRASERO_JOB_TASK_SET_WRITTEN (self, written_sectors << 11);
	}

	fclose (self->priv->file);
	self->priv->file = NULL;

	if (!self->priv->cancel)
		self->priv->thread_id = g_idle_add (brasero_libisofs_thread_finished, self);

	self->priv->thread = NULL;
	g_thread_exit (NULL);

	return NULL;
}

static BraseroBurnResult
brasero_libisofs_write_image_to_file (BraseroLibisofs *self,
				      GError **error)
{
	if (!self->priv->output) {
		int fd;

		self->priv->output = g_strdup_printf ("%s/"BRASERO_BURN_TMP_FILE_NAME,
						      g_get_tmp_dir ());
		fd = mkstemp (self->priv->output);
		if (fd < 0) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     strerror (errno));
			return BRASERO_BURN_ERR;
		}

		self->priv->file = fdopen (fd, "w");
	}
	else
		self->priv->file = fopen (self->priv->output, "w");

	if (!self->priv->file) {
		g_set_error (error, 
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     strerror (errno));
		return BRASERO_BURN_ERR;
	}

	if (self->priv->thread)
		return BRASERO_BURN_RUNNING;

	self->priv->thread = g_thread_create (brasero_libisofs_write_image_to_file_thread,
					      self,
					      TRUE,
					      error);
	if (!self->priv->thread)
		return BRASERO_BURN_ERR;

	BRASERO_JOB_TASK_SET_ACTION (self,
				     BRASERO_BURN_ACTION_CREATING_IMAGE,
				     NULL,
				     FALSE);
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_libisofs_start (BraseroJob *job,
			int in_fd,
			int *out_fd,
			GError **error)
{
	BraseroLibisofs *self;

	self = BRASERO_LIBISOFS (job);
	if (in_fd > -1)
		BRASERO_JOB_NOT_SUPPORTED (self);

	if (self->priv->action == BRASERO_LIBISOFS_ACTION_CREATE_VOLUME) {
		BRASERO_JOB_TASK_SET_ACTION (self,
					     BRASERO_BURN_ACTION_GETTING_SIZE,
					     NULL,
					     FALSE);
		return brasero_libisofs_create_volume (self, error);
	}

	if (!out_fd)
		return brasero_libisofs_write_image_to_file (self, error);

	return brasero_libisofs_write_image_to_fd (self, out_fd, error);
}

static BraseroBurnResult
brasero_libisofs_stop (BraseroJob *job,
		       BraseroBurnResult retval,
		       GError **error)
{
	BraseroLibisofs *self;

	self = BRASERO_LIBISOFS (job);
	
	brasero_libisofs_stop_real (self);
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_libisofs_get_track (BraseroImager *imager,
			    BraseroTrackSource **track,
			    GError **error)
{
	BraseroLibisofs *self;
	BraseroTrackSource *retval;

	self = BRASERO_LIBISOFS (imager);

	if (!self->priv->libburn_src) {
		BraseroBurnResult result;

		self->priv->action = BRASERO_LIBISOFS_ACTION_CREATE_VOLUME;
		result = brasero_job_run (BRASERO_JOB (imager), error);
		self->priv->action = BRASERO_LIBISOFS_ACTION_NONE;

		if (result != BRASERO_BURN_OK)
			return result;
	}

	if (!self->priv->iso_ready) {
		BraseroBurnResult result;

		self->priv->action = BRASERO_LIBISOFS_ACTION_CREATE_IMAGE;
		result = brasero_job_run (BRASERO_JOB (imager), error);
		self->priv->action = BRASERO_LIBISOFS_ACTION_NONE;

		if (result != BRASERO_BURN_OK)
			return result;

		self->priv->iso_ready = 1;
	}

	retval = g_new0 (BraseroTrackSource, 1);
	retval->type = BRASERO_TRACK_SOURCE_IMAGE;
	retval->format = BRASERO_IMAGE_FORMAT_ISO;
	if (self->priv->image_format & BRASERO_IMAGE_FORMAT_JOLIET)
		retval->format |= BRASERO_IMAGE_FORMAT_JOLIET;

	retval->contents.image.image = g_strdup_printf ("file://%s", self->priv->output);

	*track = retval;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_libisofs_get_size (BraseroImager *imager,
			   gint64 *size,
			   gboolean sectors,
			   GError **error)
{
	BraseroLibisofs *self;
	BraseroBurnResult result;

	self = BRASERO_LIBISOFS (imager);

	result = BRASERO_BURN_OK;

	if (!self->priv->libburn_src) {
		if (brasero_job_is_running (BRASERO_JOB (self)))
			return BRASERO_BURN_RUNNING;

		self->priv->action = BRASERO_LIBISOFS_ACTION_CREATE_VOLUME;
		result = brasero_job_run (BRASERO_JOB (imager), error);
		self->priv->action = BRASERO_LIBISOFS_ACTION_NONE;

		if (result != BRASERO_BURN_OK)
			return result;
	}

	if (self->priv->libburn_src) {
		*size = (gint64) self->priv->libburn_src->get_size (self->priv->libburn_src);
		if (sectors)
			*size /= 2048;
	}

	return result;
}

static BraseroBurnResult
brasero_libisofs_get_track_type (BraseroImager *imager,
				 BraseroTrackSourceType *type,
				 BraseroImageFormat *format)
{
	BraseroLibisofs *self;
	BraseroImageFormat retval;

	self = BRASERO_LIBISOFS (imager);

	if (!self->priv->source)
		BRASERO_JOB_NOT_READY (self);

	if (self->priv->image_format == BRASERO_IMAGE_FORMAT_ANY)
		retval = brasero_burn_caps_get_imager_default_format (self->priv->caps,
								      self->priv->source);
	else
		retval = self->priv->image_format;

	*format = retval;
	*type = BRASERO_TRACK_SOURCE_IMAGE;

	return BRASERO_BURN_OK;
}

#endif /* HAVE_LIBBURN */
