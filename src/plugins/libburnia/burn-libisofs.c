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
#include <gmodule.h>

#include <libisofs/libisofs.h>
#include <libburn/libburn.h>

#include <libgnomevfs/gnome-vfs.h>

#include "burn-basics.h"
#include "burn-libisofs.h"
#include "burn-job.h"
#include "burn-plugin.h"

BRASERO_PLUGIN_BOILERPLATE (BraseroLibisofs, brasero_libisofs, BRASERO_TYPE_JOB, BraseroJob);

struct _BraseroLibisofsPrivate {
	struct burn_source *libburn_src;

	FILE *file;
	int pipe_out;

	GError *error;
	GThread *thread;
	guint thread_id;
	guint cancel:1;
};
typedef struct _BraseroLibisofsPrivate BraseroLibisofsPrivate;

#define BRASERO_LIBISOFS_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_LIBISOFS, BraseroLibisofsPrivate))

static GObjectClass *parent_class = NULL;

static gboolean
brasero_libisofs_thread_finished (gpointer data)
{
	gchar *output = NULL;
	BraseroTrackType type;
	BraseroTrack *track = NULL;
	BraseroLibisofs *self = data;
	BraseroLibisofsPrivate *priv;

	priv = BRASERO_LIBISOFS_PRIVATE (self);

	priv->thread_id = 0;
	if (priv->error) {
		GError *error;

		error = priv->error;
		priv->error = NULL;
		brasero_job_error (BRASERO_JOB (self), error);
		return FALSE;
	}

	/* Let's make a track */
	brasero_job_get_output_type (BRASERO_JOB (self), &type);
	track = brasero_track_new (type.type);

	brasero_job_get_image_output (BRASERO_JOB (self),
				      &output,
				      NULL);
	brasero_track_set_image_source (track,
					output,
					NULL,
					BRASERO_IMAGE_FORMAT_BIN);

	brasero_job_finished (BRASERO_JOB (self), track);
	return FALSE;
}

static BraseroBurnResult
brasero_libisofs_write_sector_to_fd (BraseroLibisofs *self,
				     gpointer buffer,
				     gint bytes_remaining)
{
	int fd;
	gint bytes_written = 0;
	BraseroLibisofsPrivate *priv;

	priv = BRASERO_LIBISOFS_PRIVATE (self);

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

static void
brasero_libisofs_write_image_to_fd_thread (BraseroLibisofs *self)
{
	const gint sector_size = 2048;
	BraseroLibisofsPrivate *priv;
	gint64 written_sectors = 0;
	BraseroBurnResult result;
	guchar buf [sector_size];

	priv = BRASERO_LIBISOFS_PRIVATE (self);

	brasero_job_set_current_action (BRASERO_JOB (self),
					BRASERO_BURN_ACTION_CREATING_IMAGE,
					NULL,
					FALSE);

	brasero_job_set_current_track_size (BRASERO_JOB (self),
					    2048,
					    -1,
					    priv->libburn_src->get_size (priv->libburn_src));
	brasero_job_start_progress (BRASERO_JOB (self), FALSE);

	while (priv->libburn_src->read (priv->libburn_src, buf, sector_size) == sector_size) {
		if (priv->cancel)
			break;

		result = brasero_libisofs_write_sector_to_fd (self,
							      buf,
							      sector_size);
		if (result != BRASERO_BURN_OK)
			break;

		written_sectors ++;
		brasero_job_set_written (BRASERO_JOB (self), written_sectors << 11);
	}
}

static void
brasero_libisofs_write_image_to_file_thread (BraseroLibisofs *self)
{
	const gint sector_size = 2048;
	BraseroLibisofsPrivate *priv;
	gint64 written_sectors = 0;
	guchar buf [sector_size];
	gchar *output;
	FILE *file;

	priv = BRASERO_LIBISOFS_PRIVATE (self);

	brasero_job_get_image_output (BRASERO_JOB (self), &output, NULL);
	file = fopen (output, "w");
	if (!file) {
		priv->error = g_error_new (BRASERO_BURN_ERROR,
					   BRASERO_BURN_ERROR_GENERAL,
					   strerror (errno));
		return;
	}

	brasero_job_set_current_action (BRASERO_JOB (self),
					BRASERO_BURN_ACTION_CREATING_IMAGE,
					NULL,
					FALSE);

	priv = BRASERO_LIBISOFS_PRIVATE (self);
	brasero_job_set_current_track_size (BRASERO_JOB (self),
					    2048,
					    -1,
					    priv->libburn_src->get_size (priv->libburn_src));
	brasero_job_start_progress (BRASERO_JOB (self), FALSE);

	while (priv->libburn_src->read (priv->libburn_src, buf, sector_size) == sector_size) {
		if (priv->cancel)
			break;

		if (fwrite (buf, 1, sector_size, file) != sector_size) {
			priv->error = g_error_new (BRASERO_BURN_ERROR,
						   BRASERO_BURN_ERROR_GENERAL,
						   _("the data couldn't be written to the file (%i: %s)"),
						   errno,
						   strerror (errno));
			break;
		}

		if (priv->cancel)
			break;

		written_sectors ++;
		brasero_job_set_written (BRASERO_JOB (self), written_sectors << 11);
	}

	fclose (file);
	file = NULL;
}

static gpointer
brasero_libisofs_thread_started (gpointer data)
{
	BraseroLibisofsPrivate *priv;
	BraseroLibisofs *self;

	self = BRASERO_LIBISOFS (data);
	priv = BRASERO_LIBISOFS_PRIVATE (self);

	if (brasero_job_get_fd_out (BRASERO_JOB (self), NULL) == BRASERO_BURN_OK)
		brasero_libisofs_write_image_to_fd_thread (self);
	else
		brasero_libisofs_write_image_to_file_thread (self);

	if (!priv->cancel)
		priv->thread_id = g_idle_add (brasero_libisofs_thread_finished, self);

	priv->thread = NULL;
	g_thread_exit (NULL);
	return NULL;
}

static BraseroBurnResult
brasero_libisofs_create_image (BraseroLibisofs *self,
			       GError **error)
{
	BraseroLibisofsPrivate *priv;

	priv = BRASERO_LIBISOFS_PRIVATE (self);

	if (priv->thread)
		return BRASERO_BURN_RUNNING;

	priv->thread = g_thread_create (brasero_libisofs_thread_started,
					self,
					TRUE,
					error);
	if (!priv->thread)
		return BRASERO_BURN_ERR;

	return BRASERO_BURN_OK;
}

static gboolean
brasero_libisofs_create_volume_thread_finished (gpointer data)
{
	BraseroLibisofs *self = data;
	BraseroLibisofsPrivate *priv;
	BraseroJobAction action;

	priv = BRASERO_LIBISOFS_PRIVATE (self);

	priv->thread_id = 0;
	if (priv->error) {
		GError *error;

		error = priv->error;
		priv->error = NULL;
		brasero_job_error (BRASERO_JOB (self), error);
		return FALSE;
	}

	brasero_job_set_current_track_size (BRASERO_JOB (self),
					    2048,
					    -1,
					    priv->libburn_src->get_size (priv->libburn_src));

	brasero_job_get_action (BRASERO_JOB (self), &action);
	if (action == BRASERO_JOB_ACTION_IMAGE) {
		BraseroBurnResult result;
		GError *error = NULL;

		result = brasero_libisofs_create_image (self, &error);
		if (error)
			priv->error = error;

		return FALSE;
	}

	brasero_job_finished (BRASERO_JOB (self), NULL);
	return FALSE;
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

	return len_a - len_b;
}

static gpointer
brasero_libisofs_create_volume_thread (gpointer data)
{
	BraseroLibisofsPrivate *priv;
	BraseroLibisofs *self = data;
	BraseroTrack *track = NULL;
	struct iso_volume *volume;
	struct iso_volset *volset;
	GSList *excluded = NULL;
	GSList *grafts = NULL;
	BraseroTrackType type;
	gchar *label = NULL;
	gchar *publisher;
	GSList *iter;
	gint flags;

	priv = BRASERO_LIBISOFS_PRIVATE (self);
	publisher = g_strdup_printf ("Brasero-%i.%i.%i",
				     BRASERO_MAJOR_VERSION,
				     BRASERO_MINOR_VERSION,
				     BRASERO_SUB);

	brasero_job_get_data_label (BRASERO_JOB (self), &label);
	volume = iso_volume_new (label,
				 publisher,
				 g_get_real_name ());
	g_free (publisher);
	g_free (label);

	/* we add the globally excluded */
	brasero_job_get_current_track (BRASERO_JOB (self), &track);
	excluded = brasero_track_get_data_excluded_source (track);
	for (; excluded; excluded = excluded->next) {
		gchar *uri;
		gchar *path;

		uri = excluded->data;
		path = gnome_vfs_get_local_path_from_uri (uri);
		iso_exclude_add_path (path);
		g_free (path);
	}

	brasero_job_start_progress (BRASERO_JOB (self), FALSE);

	/* we need to copy the list as we're going to reorder it */
	grafts = brasero_track_get_data_grafts_source (track);
	grafts = g_slist_copy (grafts);
	grafts = g_slist_sort (grafts, brasero_libisofs_sort_graft_points);
	for (iter = grafts; iter; iter = iter->next) {
		GSList *excluded_path;
		BraseroGraftPt *graft;
		struct iso_tree_node *node;

		if (priv->cancel)
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
			priv->error = g_error_new (BRASERO_BURN_ERROR,
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

	if (iter)
		g_slist_free (iter);

	/* clean the exclusion */
	iso_exclude_empty ();

	volset = iso_volset_new (volume, "VOLSETID");
	iso_volume_free (volume);

	brasero_track_get_type (track, &type);
	flags = ((type.subtype.fs_type & BRASERO_IMAGE_FS_JOLIET) ? ECMA119_JOLIET : 0);
	flags |= ECMA119_ROCKRIDGE;

	priv->libburn_src = iso_source_new_ecma119 (volset,
						    flags);
	iso_volset_free (volset);

	brasero_job_set_current_track_size (BRASERO_JOB (self), 
					    2048,
					    -1,
					    priv->libburn_src->get_size (priv->libburn_src));
	priv->thread_id = g_idle_add (brasero_libisofs_create_volume_thread_finished, self);
	priv->thread = NULL;

	return NULL;
}

static BraseroBurnResult
brasero_libisofs_create_volume (BraseroLibisofs *self, GError **error)
{
	BraseroLibisofsPrivate *priv;

	priv = BRASERO_LIBISOFS_PRIVATE (self);
	if (priv->thread)
		return BRASERO_BURN_RUNNING;

	priv->thread = g_thread_create (brasero_libisofs_create_volume_thread,
					self,
					TRUE,
					error);
	if (!priv->thread)
		return BRASERO_BURN_ERR;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_libisofs_start (BraseroJob *job,
			GError **error)
{
	BraseroLibisofs *self;
	BraseroJobAction action;
	BraseroLibisofsPrivate *priv;

	self = BRASERO_LIBISOFS (job);
	priv = BRASERO_LIBISOFS_PRIVATE (self);

	brasero_job_get_action (job, &action);
	if (action == BRASERO_JOB_ACTION_SIZE
	|| !priv->libburn_src) {
		brasero_job_set_current_action (BRASERO_JOB (self),
						BRASERO_BURN_ACTION_GETTING_SIZE,
						NULL,
						FALSE);
		return brasero_libisofs_create_volume (self, error);
	}


	return brasero_libisofs_create_image (self, error);
}

static void
brasero_libisofs_stop_real (BraseroLibisofs *self)
{
	BraseroLibisofsPrivate *priv;

	priv = BRASERO_LIBISOFS_PRIVATE (self);
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
brasero_libisofs_stop (BraseroJob *job,
		       GError **error)
{
	BraseroLibisofs *self;

	self = BRASERO_LIBISOFS (job);
	brasero_libisofs_stop_real (self);
	return BRASERO_BURN_OK;
}

static void
brasero_libisofs_class_init (BraseroLibisofsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroJobClass *job_class = BRASERO_JOB_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroLibisofsPrivate));

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_libisofs_finalize;

	job_class->start = brasero_libisofs_start;
	job_class->stop = brasero_libisofs_stop;
}

static void
brasero_libisofs_init (BraseroLibisofs *obj)
{ }

static void
brasero_libisofs_clean_output (BraseroLibisofs *self)
{
	BraseroLibisofsPrivate *priv;

	priv = BRASERO_LIBISOFS_PRIVATE (self);
	if (priv->libburn_src) {
		burn_source_free (priv->libburn_src);
		priv->libburn_src = NULL;
	}
}

static void
brasero_libisofs_finalize (GObject *object)
{
	BraseroLibisofs *cobj;

	cobj = BRASERO_LIBISOFS (object);

	brasero_libisofs_stop_real (cobj);
	brasero_libisofs_clean_output (cobj);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static BraseroBurnResult
brasero_libisofs_export_caps (BraseroPlugin *plugin, gchar **error)
{
	GSList *output;
	GSList *input;

	brasero_plugin_define (plugin,
			       "libisofs",
			       _("libisofs creates disc images from files"),
			       "Philippe Rouquier",
			       0);

	output = brasero_caps_image_new (BRASERO_PLUGIN_IO_ACCEPT_FILE|
					 BRASERO_PLUGIN_IO_ACCEPT_PIPE,
					 BRASERO_IMAGE_FORMAT_BIN);
	input = brasero_caps_data_new (BRASERO_IMAGE_FS_ISO|
				       BRASERO_IMAGE_FS_JOLIET);

	brasero_plugin_link_caps (plugin, output, input);

	g_slist_free (input);
	g_slist_free (output);

	return BRASERO_BURN_OK;
}
