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


#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <libgnomevfs/gnome-vfs.h>

#include "burn-job.h"
#include "burn-imager.h"
#include "burn-sum.h"
#include "burn-md5.h"
#include "brasero-ncb.h"

static void brasero_burn_sum_class_init (BraseroBurnSumClass *klass);
static void brasero_burn_sum_init (BraseroBurnSum *sp);
static void brasero_burn_sum_finalize (GObject *object);
static void brasero_burn_sum_iface_init_image (BraseroImagerIFace *iface);

static BraseroBurnResult
brasero_burn_sum_start (BraseroJob *job,
			int in_fd,
			int *out_fd,
			GError **error);
static BraseroBurnResult
brasero_burn_sum_stop (BraseroJob *job,
		       BraseroBurnResult retval,
		       GError **error);

static BraseroBurnResult
brasero_burn_sum_get_size (BraseroImager *imager,
			   gint64 *size,
			   gboolean sectors,
			   GError **error);

static BraseroBurnResult
brasero_burn_sum_set_source (BraseroJob *job,
			     const BraseroTrackSource *source,
			     GError **error);

static BraseroBurnResult
brasero_burn_sum_set_output (BraseroImager *imager,
			     const gchar *output,
			     gboolean overwrite,
			     gboolean clean,
			     GError **error);
static BraseroBurnResult
brasero_burn_sum_set_output_type (BraseroImager *imager,
				  BraseroTrackSourceType type,
				  BraseroImageFormat format,
				  GError **error);
static BraseroBurnResult
brasero_burn_sum_get_track (BraseroImager *imager,
			    BraseroTrackSource **source,
			    GError **error);

struct _BraseroBurnSumPrivate {
	BraseroTrackSource *source;

	BraseroMD5Ctx *ctx;

	BraseroMD5 md5;

	gint clock_id;
	off_t size;

	/* the path and fd for the file containing the md5 of files */
	gchar *sums_path;
	FILE *file;

	/* this is for the thread and the end of it */
	GThread *thread;
	gint end_id;

	gint sums_ready:1;
	gint overwrite:1;
	gint cancel:1;
	gint clean:1;
};

static BraseroJobClass *parent_class = NULL;

GType
brasero_burn_sum_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroBurnSumClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_burn_sum_class_init,
			NULL,
			NULL,
			sizeof (BraseroBurnSum),
			0,
			(GInstanceInitFunc)brasero_burn_sum_init,
		};
		static const GInterfaceInfo imager_info =
		{
			(GInterfaceInitFunc) brasero_burn_sum_iface_init_image,
			NULL,
			NULL
		};
		type = g_type_register_static (BRASERO_TYPE_JOB, 
					       "BraseroBurnSum",
					       &our_info,
					       0);
		g_type_add_interface_static (type,
					     BRASERO_TYPE_IMAGER,
					     &imager_info);
	}

	return type;
}

static void
brasero_burn_sum_class_init (BraseroBurnSumClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroJobClass *job_class = BRASERO_JOB_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_burn_sum_finalize;

	job_class->start = brasero_burn_sum_start;
	job_class->stop = brasero_burn_sum_stop;
	job_class->set_source = brasero_burn_sum_set_source;
}

static void
brasero_burn_sum_iface_init_image (BraseroImagerIFace *iface)
{
	iface->get_size = brasero_burn_sum_get_size;
	iface->get_track = brasero_burn_sum_get_track;
//	iface->get_track_type = brasero_burn_sum_get_track_type;
	iface->set_output = brasero_burn_sum_set_output;
//	iface->set_append = brasero_burn_sum_set_append;
	iface->set_output_type = brasero_burn_sum_set_output_type;
}

static void
brasero_burn_sum_init (BraseroBurnSum *obj)
{
	obj->priv = g_new0 (BraseroBurnSumPrivate, 1);
}

static void
brasero_burn_sum_finalize (GObject *object)
{
	BraseroBurnSum *cobj;
	
	cobj = BRASERO_BURN_SUM(object);

	if (cobj->priv->thread) {
		cobj->priv->cancel = 1;
		g_thread_join (cobj->priv->thread);
		cobj->priv->cancel = 0;
		cobj->priv->thread = NULL;
	}

	if (cobj->priv->end_id) {
		g_source_remove (cobj->priv->end_id);
		cobj->priv->end_id = 0;
	}

	if (cobj->priv->file) {
		fclose (cobj->priv->file);
		cobj->priv->file = NULL;
	}

	if (cobj->priv->sums_path) {
		if (cobj->priv->clean)
			g_remove (cobj->priv->sums_path);

		g_free (cobj->priv->sums_path);
		cobj->priv->sums_path = NULL;
	}

	if (cobj->priv->source) {
		brasero_track_source_free (cobj->priv->source);
		cobj->priv->source = NULL;
	}
		
	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

BraseroBurnSum *
brasero_burn_sum_new ()
{
	BraseroBurnSum *obj;
	
	obj = BRASERO_BURN_SUM (g_object_new (BRASERO_TYPE_BURN_SUM, NULL));
	
	return obj;
}

static BraseroBurnResult
brasero_burn_sum_start_md5 (BraseroBurnSum *sum,
			    const gchar *path,
			    const gchar *graft_path,
			    GError **error)
{
	BraseroBurnResult result = BRASERO_BURN_OK;
	gchar md5_checksum [MD5_STRING_LEN + 1];
	gint written;

	/* write to the file */
	result = brasero_md5_sum_to_string (sum->priv->ctx,
					    path,
					    md5_checksum,
					    error);
	if (result != BRASERO_BURN_OK)
		return result;

	written = fwrite (md5_checksum,
			  strlen (md5_checksum),
			  1,
			  sum->priv->file);

	if (written != 1) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the md5 file couldn't be written to (%s)"),
			     strerror (errno));
			
		return BRASERO_BURN_ERR;
	}

	written = fwrite ("  ",
			  2,
			  1,
			  sum->priv->file);

	/* NOTE: we remove the first "/" from path so the file can be
	 * used with md5sum at the root of the disc once mounted */
	written = fwrite (graft_path + 1,
			  strlen (graft_path + 1),
			  1,
			  sum->priv->file);

	if (written != 1) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the md5 file couldn't be written to (%s)"),
			     strerror (errno));

		return BRASERO_BURN_ERR;
	}

	written = fwrite ("\n",
			  1,
			  1,
			  sum->priv->file);

	return result;
}

static BraseroBurnResult
brasero_burn_sum_explore_directory (BraseroBurnSum *sum,
				    const gchar *directory,
				    const gchar *disc_path,
				    GHashTable *excludedH,
				    GError **error)
{
	BraseroBurnResult result = BRASERO_BURN_OK;
	const gchar *name;
	GDir *dir;

	dir = g_dir_open (directory, 0, error);
	if (!dir || *error)
		return BRASERO_BURN_ERR;

	while ((name = g_dir_read_name (dir))) {
		gchar *path;
		gchar *graft_path;

		if (sum->priv->cancel) {
			result = BRASERO_BURN_CANCEL;
			break;
		}

		path = g_build_path (G_DIR_SEPARATOR_S, directory, name, NULL);
		if (g_hash_table_lookup (excludedH, path)) {
			g_free (path);
			continue;
		}

		graft_path = g_build_path (G_DIR_SEPARATOR_S, disc_path, name, NULL);
		if (g_file_test (path, G_FILE_TEST_IS_DIR)) {
			result = brasero_burn_sum_explore_directory (sum,
								     path,
								     graft_path,
								     excludedH,
								     error);
			g_free (path);
			g_free (graft_path);

			if (result != BRASERO_BURN_OK)
				break;

			continue;
		}
			
		result = brasero_burn_sum_start_md5 (sum,
						     path,
						     graft_path,
						     error);
		g_free (graft_path);
		g_free (path);

		if (result != BRASERO_BURN_OK)
			break;
	}
	g_dir_close (dir);

	/* NOTE: we don't care if the file is twice or more on the disc,
	 * that would be too much overhead/memory consumption for something
	 * that scarcely happens and that way each file can be checked later*/

	return result;
}

static gboolean
brasero_burn_sum_clean_excluded_table_cb (gpointer key,
					  gpointer data,
					  gpointer user_data)
{
	if (GPOINTER_TO_INT (data) == 1)
		return TRUE;

	return FALSE;
}

static BraseroBurnResult
brasero_burn_sum_explore_grafts (BraseroBurnSum *sum, GError **error)
{
	GSList *iter;
	GHashTable *excludedH;
	BraseroBurnResult result = BRASERO_BURN_OK;

	/* opens a file for the sums */
	if (!sum->priv->sums_path) {
		int fd;

		sum->priv->sums_path = g_strdup_printf ("%s/"BRASERO_BURN_TMP_FILE_NAME,
							g_get_tmp_dir ());
		fd = g_mkstemp (sum->priv->sums_path);
		sum->priv->file = fdopen (fd, "w");
	}
	else {
		/* we check that this file doesn't already exists */
		if (!sum->priv->overwrite
		&&  g_file_test (sum->priv->sums_path, G_FILE_TEST_EXISTS)) {
			gchar *name;
	
			BRASERO_GET_BASENAME_FOR_DISPLAY (sum->priv->sums_path, name);
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("\"%s\" already exists"),
				     name);
			g_free (name);

			return BRASERO_BURN_ERR;
		}

		sum->priv->file = fopen (sum->priv->sums_path, "w");
	}

	if (!sum->priv->file) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("md5 file couldn't be opened (%s)"),
			     strerror (errno));

		return BRASERO_BURN_ERR;
	}

	/* we fill a hash table with all the files that are excluded globally */
	excludedH = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	for (iter = sum->priv->source->contents.data.excluded; iter; iter = iter->next) {
		gchar *uri;
		gchar *path;

		/* get the path */
		uri = iter->data;
		path = gnome_vfs_get_local_path_from_uri (uri);

		if (path)
			g_hash_table_insert (excludedH, path, path);
	}

	sum->priv->ctx = brasero_md5_new ();

	/* it's now time to start reporting our progress */
	BRASERO_JOB_TASK_SET_ACTION (sum,
				     BRASERO_BURN_ACTION_CHECKSUM,
				     _("Creating checksum for image files"),
				     TRUE);

	BRASERO_JOB_TASK_START_PROGRESS (sum, TRUE);

	for (iter = sum->priv->source->contents.data.grafts; iter; iter = iter->next) {
		BraseroGraftPt *graft;
		gchar *graft_path;
		GSList *excluded;
		gchar *path;
		gchar *uri;

		if (sum->priv->cancel) {
			result = BRASERO_BURN_CANCEL;
			break;
		}

		graft = iter->data;
		if (!graft->uri)
			continue;

		/* add all excluded in the excluded graft hash */
		for (excluded = graft->excluded; excluded; excluded = excluded->next) {
			uri = excluded->data;
			path = gnome_vfs_get_local_path_from_uri (uri);
			g_hash_table_insert (excludedH, path, GINT_TO_POINTER (1));
		}

		/* get the current and futures paths */
		uri = graft->uri;
		path = gnome_vfs_get_local_path_from_uri (uri);
		graft_path = graft->path;

		if (g_file_test (path, G_FILE_TEST_IS_DIR))
			result = brasero_burn_sum_explore_directory (sum,
								     path,
								     graft_path,
								     excludedH,
								     error);
		else
			result = brasero_burn_sum_start_md5 (sum,
							     path,
							     graft_path,
							     error);

		g_free (path);

		/* clean excluded hash table of all graft point excluded */
		g_hash_table_foreach_remove (excludedH,
					     brasero_burn_sum_clean_excluded_table_cb,
					     NULL);

		if (result != BRASERO_BURN_OK)
			break;
	}

	brasero_md5_free (sum->priv->ctx);
	sum->priv->ctx = NULL;

	g_hash_table_destroy (excludedH);

	/* that's finished we close the file */
	fclose (sum->priv->file);
	sum->priv->file = NULL;

	return result;
}

static BraseroBurnResult
brasero_burn_sum_image (BraseroBurnSum *self, GError **error)
{
	BraseroBurnResult result;
	struct stat stats;
	gchar *path;

	path = brasero_track_source_get_image_localpath (self->priv->source);
	if (!path) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the image is not local"));
		return BRASERO_BURN_ERR;
	}

	if (stat (path, &stats) < 0) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     strerror (errno));
		return BRASERO_BURN_ERR;
	}

	self->priv->size = stats.st_size;

	BRASERO_JOB_TASK_SET_ACTION (self,
				     BRASERO_BURN_ACTION_CHECKSUM,
				     _("Creating image checksum"),
				     FALSE);
	BRASERO_JOB_TASK_START_PROGRESS (self, FALSE);

	self->priv->ctx = brasero_md5_new ();
	result = brasero_md5_sum (self->priv->ctx,
				  path,
				  &self->priv->md5,
				  -1,
				  error);
	g_free (path);
	brasero_md5_free (self->priv->ctx);
	self->priv->ctx = NULL;

	return result;
}

static BraseroBurnResult
brasero_burn_sum_disc (BraseroBurnSum *self, GError **error)
{
	gint64 size;
	gint64 limit = -1;
	const gchar *device;
	BraseroBurnResult result;
	NautilusBurnDrive *drive;
	NautilusBurnMediaType media;

	drive = self->priv->source->contents.drive.disc;
	
	/* we get the size of the image */
	size = nautilus_burn_drive_get_media_size (drive);
	BRASERO_JOB_TASK_SET_TOTAL (self, size);

	media = nautilus_burn_drive_get_media_type (drive);
	if (media < NAUTILUS_BURN_MEDIA_TYPE_CD) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the disc is empty or busy."));
		return BRASERO_BURN_ERR;
	}

	if (NAUTILUS_BURN_DRIVE_MEDIA_TYPE_IS_DVD (media)) {
		/* This is to avoid reading till the end of the DVD */
		limit = NCB_MEDIA_GET_SIZE (drive);
	}

	device = NCB_DRIVE_GET_DEVICE (drive);

	BRASERO_JOB_TASK_SET_ACTION (self,
				     BRASERO_BURN_ACTION_CHECKSUM,
				     _("Creating disc checksum"),
				     FALSE);
	BRASERO_JOB_TASK_START_PROGRESS (self, FALSE);

	self->priv->ctx = brasero_md5_new ();
	result = brasero_md5_sum (self->priv->ctx,
				  device,
				  &self->priv->md5,
				  limit,
				  error);
	brasero_md5_free (self->priv->ctx);
	self->priv->ctx = NULL;

	return result;
}

struct _BraseroBurnSumThreadCtx {
	BraseroBurnSum *sum;
	BraseroBurnResult result;
	GError *error;
};
typedef struct _BraseroBurnSumThreadCtx BraseroBurnSumThreadCtx;

static gboolean
brasero_burn_sum_end (gpointer data)
{
	GError *error;
	BraseroBurnSum *sum;
	BraseroBurnSumThreadCtx *ctx;

	ctx = data;
	sum = ctx->sum;
	error = ctx->error;

	if (ctx->result == BRASERO_BURN_NOT_READY) {
		BRASERO_JOB_NOT_READY (sum);
	}
	else if (ctx->result == BRASERO_BURN_NOT_SUPPORTED) {
		BRASERO_JOB_NOT_SUPPORTED (sum);
	}
	else if (ctx->result == BRASERO_BURN_ERR)		
		brasero_job_error (BRASERO_JOB (sum), error);
	else
		brasero_job_finished (BRASERO_JOB (sum));

	sum->priv->end_id = 0;
	return FALSE;
}

static void
brasero_burn_sum_destroy (gpointer data)
{
	BraseroBurnSumThreadCtx *ctx;

	ctx = data;
	if (ctx->error)
		g_error_free (ctx->error);
	g_free (ctx);
}

static gpointer
brasero_burn_sum_thread (gpointer data)
{
	GError *error = NULL;
	BraseroBurnSum *sum;
	BraseroBurnResult result;
	BraseroTrackSourceType type;
	BraseroBurnSumThreadCtx *ctx;

	sum = BRASERO_BURN_SUM (data);

	type = sum->priv->source->type;
	if (type == BRASERO_TRACK_SOURCE_DATA)
		result = brasero_burn_sum_explore_grafts (sum, &error);
	else if (type == BRASERO_TRACK_SOURCE_IMAGE)
		result = brasero_burn_sum_image (sum, &error);
	else if (type == BRASERO_TRACK_SOURCE_DISC)
		result = brasero_burn_sum_disc (sum, &error);
	else
		result = BRASERO_BURN_CANCEL;

	if (result == BRASERO_BURN_CANCEL) {
		g_thread_exit (NULL);
		return NULL;
	}

	ctx = g_new0 (BraseroBurnSumThreadCtx, 1);
	ctx->sum = sum;
	ctx->error = error;
	ctx->result = result;
	sum->priv->end_id = g_idle_add_full (G_PRIORITY_HIGH_IDLE,
					     brasero_burn_sum_end,
					     ctx,
					     brasero_burn_sum_destroy);

	g_thread_exit (NULL);
	return NULL;
}

static void
brasero_burn_sum_clock_tick (BraseroTask *task, BraseroBurnSum *sum)
{
	if (!sum->priv->ctx)
		return;

	BRASERO_JOB_TASK_SET_WRITTEN (sum, brasero_md5_get_written (sum->priv->ctx));
}

static BraseroBurnResult
brasero_burn_sum_start (BraseroJob *job,
			int in_fd,
			int *out_fd,
			GError **error)
{
	BraseroBurnSum *sum;

	sum = BRASERO_BURN_SUM (job);
	if (in_fd > 0 || out_fd)
		BRASERO_JOB_NOT_SUPPORTED (sum);

	if (!sum->priv->source)
		BRASERO_JOB_NOT_READY (sum);

	/* we start a thread for the exploration of the graft points */
	sum->priv->thread = g_thread_create (brasero_burn_sum_thread,
					     sum,
					     TRUE,
					     error);

	if (!sum->priv->thread)
		return BRASERO_BURN_ERR;

	BRASERO_JOB_TASK_CONNECT_TO_CLOCK (sum,
					   brasero_burn_sum_clock_tick,
					   sum->priv->clock_id);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_sum_stop (BraseroJob *job,
		       BraseroBurnResult retval, 
		       GError **error)
{
	BraseroBurnSum *sum;

	sum = BRASERO_BURN_SUM (job);

	BRASERO_JOB_TASK_DISCONNECT_FROM_CLOCK (sum, sum->priv->clock_id);

	if (sum->priv->ctx)
		brasero_md5_cancel (sum->priv->ctx);

	if (sum->priv->thread) {
		sum->priv->cancel = 1;
		g_thread_join (sum->priv->thread);
		sum->priv->cancel = 0;
		sum->priv->thread = NULL;
	}

	if (sum->priv->file) {
		fclose (sum->priv->file);
		sum->priv->file = NULL;
	}

	if (retval == BRASERO_BURN_CANCEL)
		g_remove (sum->priv->sums_path);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_sum_set_source (BraseroJob *job,
			     const BraseroTrackSource *source,
			     GError **error)
{
	BraseroBurnSum *self;

	self = BRASERO_BURN_SUM (job);

	if (self->priv->sums_path) {
		if (self->priv->clean)
			g_remove (self->priv->sums_path);

		g_free (self->priv->sums_path);
		self->priv->sums_path = NULL;
	}

	self->priv->sums_ready = 0;

	/* FIXME: we could add disc as well ? */
	if (source->type != BRASERO_TRACK_SOURCE_DATA
	&&  source->type != BRASERO_TRACK_SOURCE_IMAGE
	&&  source->type != BRASERO_TRACK_SOURCE_DISC)
		BRASERO_JOB_NOT_SUPPORTED (job);

	if (self->priv->source)
		brasero_track_source_free (self->priv->source);

	self->priv->source = brasero_track_source_copy (source);
	self->priv->sums_ready = 0;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_sum_set_output (BraseroImager *imager,
			     const gchar *output,
			     gboolean overwrite,
			     gboolean clean,
			     GError **error)
{
	BraseroBurnSum *sum;

	sum = BRASERO_BURN_SUM (imager);

	if (sum->priv->sums_path) {
		if (sum->priv->clean)
			g_remove (sum->priv->sums_path);

		g_free (sum->priv->sums_path);
		sum->priv->sums_path = NULL;
	}

	sum->priv->sums_ready = 0;

	if (output)
		sum->priv->sums_path = g_strdup (output);

	sum->priv->sums_ready = 0;
	sum->priv->clean = (clean == TRUE);
	sum->priv->overwrite = (overwrite == TRUE);

	return BRASERO_BURN_OK;
}
    
static BraseroBurnResult
brasero_burn_sum_set_output_type (BraseroImager *imager,
				  BraseroTrackSourceType type,
				  BraseroImageFormat format,
				  GError **error)
{
	BraseroBurnSum *sum;

	sum = BRASERO_BURN_SUM (imager);

	if (type == BRASERO_TRACK_SOURCE_DATA) {
		if (sum->priv->source
		&&  sum->priv->source->type != BRASERO_TRACK_SOURCE_DATA)
			BRASERO_JOB_NOT_SUPPORTED (sum);
	}
	else if (type == BRASERO_TRACK_SOURCE_SUM) {
		if (sum->priv->source
		&&  sum->priv->source->type == BRASERO_TRACK_SOURCE_DATA)
			BRASERO_JOB_NOT_SUPPORTED (sum);
	}
	else if (type != BRASERO_TRACK_SOURCE_DEFAULT)
		BRASERO_JOB_NOT_SUPPORTED (sum);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_sum_get_track (BraseroImager *imager,
			    BraseroTrackSource **source,
			    GError **error)
{
	BraseroBurnSum *self;
	BraseroGraftPt *graft;
	BraseroTrackSource *track;
	BraseroTrackSourceType target;

	self = BRASERO_BURN_SUM (imager);

	if (!self->priv->source)
		BRASERO_JOB_NOT_READY (self);

	/* we check the target */
	if (self->priv->source->type == BRASERO_TRACK_SOURCE_DATA)
		target = BRASERO_TRACK_SOURCE_DATA;
	else
		target = BRASERO_TRACK_SOURCE_SUM;

	if (!self->priv->sums_ready) {
		BraseroBurnResult result;

		result = brasero_job_run (BRASERO_JOB (self), error);
		if (result != BRASERO_BURN_OK)
			return result;
	}
	self->priv->sums_ready = 1;

	if (target == BRASERO_TRACK_SOURCE_DATA) {
		track = brasero_track_source_copy (self->priv->source);
		graft = g_new0 (BraseroGraftPt, 1);
		graft->uri = g_strconcat ("file://", self->priv->sums_path, NULL);
		graft->path = g_strdup ("/"BRASERO_CHECKSUM_FILE);
		track->contents.data.grafts = g_slist_prepend (track->contents.data.grafts, graft);
	}
	else if (target == BRASERO_TRACK_SOURCE_SUM) {
		track = g_new0 (BraseroTrackSource, 1);
		track->type = BRASERO_TRACK_SOURCE_SUM;
		track->format = BRASERO_IMAGE_FORMAT_NONE;
		memcpy (&track->contents.sum.md5,
			&self->priv->md5,
			sizeof (BraseroMD5));
	}
	else
		track = NULL;

	*source = track;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_sum_get_size (BraseroImager *imager,
			   gint64 *size,
			   gboolean sectors,
			   GError **error)
{
	BraseroBurnSum *self;

	self = BRASERO_BURN_SUM (imager);

	if (self->priv->size <= 0)
		BRASERO_JOB_NOT_READY (self);

	if (sectors)
		*size = self->priv->size % 2048 ? self->priv->size / 2048 + 1:
						  self->priv->size / 2048;
	else
		*size = self->priv->size;

	return BRASERO_BURN_OK;
}
