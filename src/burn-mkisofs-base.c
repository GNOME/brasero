/***************************************************************************
 *            mkisofs-case.c
 *
 *  mar jan 24 16:41:02 2006
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

#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <libgnomevfs/gnome-vfs.h>

#include <nautilus-burn-drive.h>

#include "brasero-marshal.h"
#include "burn-basics.h"
#include "burn-common.h"
#include "burn-mkisofs-base.h"
#include "burn-process.h"
#include "burn-imager.h"
#include "utils.h"

static void brasero_mkisofs_base_class_init (BraseroMkisofsBaseClass *klass);
static void brasero_mkisofs_base_init (BraseroMkisofsBase *sp);
static void brasero_mkisofs_base_finalize (GObject *object);
static void brasero_mkisofs_base_iface_init_image (BraseroImagerIFace *iface);

static BraseroBurnResult
brasero_mkisofs_base_set_source (BraseroJob *job,
				 const BraseroTrackSource *source,
				 GError **error);
static BraseroBurnResult
brasero_mkisofs_base_set_output_type (BraseroImager *imager,
				      BraseroTrackSourceType type,
				      BraseroImageFormat format,
				      GError **error);
static BraseroBurnResult
brasero_mkisofs_base_set_output (BraseroImager *imager,
				 const char *output,
				 gboolean overwrite,
				 gboolean clean,
				 GError **error);
static BraseroBurnResult
brasero_mkisofs_base_get_track (BraseroImager *imager,
				BraseroTrackSource **source,
				GError **error);
static BraseroBurnResult
brasero_mkisofs_base_get_size (BraseroImager *imager,
			       gint64 *size,
			       gboolean sectors,
			       GError **error);

struct _BraseroMkisofsData {
	int grafts_fd;
	int excluded_fd;

	GHashTable *grafts;
};
typedef struct _BraseroMkisofsData BraseroMkisofsData;

struct _BraseroDownloadNonlocalData {
	BraseroMkisofsData *data;
	GSList *list;
};
typedef struct _BraseroDownloadNonlocalData BraseroDownloadNonlocalData;

struct _BraseroWriteGraftData {
	BraseroMkisofsData *data;
	GError **error;
};
typedef struct _BraseroWriteGraftData BraseroWriteGraftData;

struct BraseroMkisofsBasePrivate {
	BraseroMkisofsData *data;
	gchar *tmpdir;

	BraseroTrackSource *source;

	gchar *emptydir;
	gchar *grafts_path;
	gchar *excluded_path;

	gint overwrite:1;
	gint clean:1;
};

static GObjectClass *parent_class = NULL;

GType
brasero_mkisofs_base_get_type()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroMkisofsBaseClass),
			NULL,
			NULL,
			(GClassInitFunc) brasero_mkisofs_base_class_init,
			NULL,
			NULL,
			sizeof (BraseroMkisofsBase),
			0,
			(GInstanceInitFunc) brasero_mkisofs_base_init,
		};

		static const GInterfaceInfo imager_info = {
			(GInterfaceInitFunc) brasero_mkisofs_base_iface_init_image,
			NULL,
			NULL
		};

		type = g_type_register_static (BRASERO_TYPE_JOB, 
					       "BraseroMkisofsBase",
					       &our_info,
					       0);

		g_type_add_interface_static (type,
					     BRASERO_TYPE_IMAGER,
					     &imager_info);
	}

	return type;
}

static void
brasero_mkisofs_base_class_init (BraseroMkisofsBaseClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroJobClass *job_class = BRASERO_JOB_CLASS (klass);

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_mkisofs_base_finalize;

	/* this is only used when we are copying files 
	 * otherwise we pass the call on to process class */
	job_class->set_source = brasero_mkisofs_base_set_source;

	/* NOTE: though it's derived from BraseroJob, this object is not
	 * to run hence, the absence of start and stop methods. It just
	 * runs a slave it it needs to download non local files. */
}

static void
brasero_mkisofs_base_iface_init_image (BraseroImagerIFace *iface)
{
	iface->get_size = brasero_mkisofs_base_get_size;
	iface->get_track = brasero_mkisofs_base_get_track;
	iface->set_output = brasero_mkisofs_base_set_output;
	iface->set_output_type = brasero_mkisofs_base_set_output_type;
}

static void
brasero_mkisofs_base_init (BraseroMkisofsBase *obj)
{
	obj->priv = g_new0 (BraseroMkisofsBasePrivate, 1);
}

static void
brasero_mkisofs_base_clean_track (BraseroMkisofsBase *base)
{
	if (base->priv->clean) {
		if (base->priv->grafts_path)
			g_remove (base->priv->grafts_path);

		if (base->priv->excluded_path) 
			g_remove (base->priv->excluded_path);

		if (base->priv->emptydir)
			g_remove (base->priv->emptydir);
	}

	if (base->priv->emptydir) {
		g_free (base->priv->emptydir);
		base->priv->emptydir = NULL;
	}

	if (base->priv->grafts_path) {
		g_free (base->priv->grafts_path);
		base->priv->grafts_path = NULL;
	}

	if (base->priv->excluded_path) {
		g_free (base->priv->excluded_path);
		base->priv->excluded_path = NULL;
	}
}

static void
brasero_mkisofs_base_clean_tmpdir (BraseroMkisofsBase *base)
{
	brasero_mkisofs_base_clean_track (base);

	if (base->priv->clean) {
		/* since we remove only our files if a file created
		 * by user remains directory won't be removed */
		if (base->priv->tmpdir)
			g_remove (base->priv->tmpdir);
	}

	if (base->priv->tmpdir) {
		g_free (base->priv->tmpdir);
		base->priv->tmpdir = NULL;
	}
}

static void
brasero_mkisofs_base_finalize (GObject *object)
{
	BraseroMkisofsBase *cobj;

	cobj = BRASERO_MKISOFS_BASE (object);

	brasero_mkisofs_base_clean_tmpdir (cobj);

	if (cobj->priv->source) {
		brasero_track_source_free (cobj->priv->source);
		cobj->priv->source = NULL;
	}

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static BraseroBurnResult
_write_line (int fd, const char *filepath, GError **error)
{
	int len;
	int w_len;

	if (lseek (fd, 0, SEEK_CUR)
	&&  write (fd, "\n", 1) != 1) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     strerror (errno));
		return BRASERO_BURN_ERR;
	}

	len = strlen (filepath);
	w_len = write (fd, filepath, len);

	if (w_len != len) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     strerror (errno));
		return BRASERO_BURN_ERR;
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_mkisofs_base_write_excluded (BraseroMkisofsData *data,
				     const gchar *uri,
				     GError **error)
{
	gchar *localpath;
	BraseroBurnResult result = BRASERO_BURN_OK;

	/* make sure uri is local: otherwise error out */
	if (!g_str_has_prefix (uri, "file://")) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the file is not stored locally"));
		return BRASERO_BURN_ERR;
	}

	localpath = gnome_vfs_get_local_path_from_uri (uri);

	/* we just ignore if localpath is NULL:
	 * - it could be a non local whose graft point couldn't be downloaded */
	if (localpath)
		result = _write_line (data->excluded_fd, localpath, error);
	
	return result;
}

static char *
_escape_path (const char *str)
{
	char *escaped, *d;
	const char *s;
	int len;

	s = str;
	len = 1;
	while (*s != 0) {
		if (*s == '\\' || *s == '=') {
			len++;
		}

		len++;
		s++;
	}

	escaped = g_malloc (len);

	s = str;
	d = escaped;
	while (*s != 0) {
		if (*s == '\\' || *s == '=') {
			*d++ = '\\';
		}

		*d++ = *s++;
	}
	*d = 0;

	return escaped;
}

static gchar *
_build_graft_point (const gchar *uri, const gchar *discpath) {
	gchar *escaped_discpath;
	gchar *graft_point;
	gchar *path;

	if (uri == NULL || discpath == NULL)
		return NULL;

	/* make up the graft point */
	if (*uri != '/')
		path = gnome_vfs_get_local_path_from_uri (uri);
	else
		path = g_strdup (uri);

	if (discpath) {
		gchar *escaped_path;

		escaped_path = _escape_path (path);
		g_free (path);

		escaped_discpath = _escape_path (discpath);
		graft_point = g_strconcat (escaped_discpath,
					   "=",
					   escaped_path,
					   NULL);
		g_free (escaped_path);
		g_free (escaped_discpath);
	}
	else
		graft_point = path;

	return graft_point;
}

static BraseroBurnResult
brasero_mkisofs_base_write_graft (BraseroMkisofsData *data,
				  const char *uri,
				  const char *disc_path,
				  GError **error)
{
	gchar *graft_point;
	gchar *localpath = NULL;
	BraseroBurnResult result;

	localpath = gnome_vfs_get_local_path_from_uri (uri);

	/* build up graft and write it */
	graft_point = _build_graft_point (localpath, disc_path);
	g_free (localpath);

	if (!graft_point) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("null graft point"));
		return BRASERO_BURN_ERR;
	}

	result = _write_line (data->grafts_fd, graft_point, error);
	g_free (graft_point);
	if(result != BRASERO_BURN_OK) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     strerror (errno));
		return result;
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_mkisofs_base_write_excluded_valid_paths (BraseroMkisofsData *data,
						 const char *uri,
						 GError **error)
{
	int size;
	char *tmp;
	char *path;
	char *parent;
	GSList *iter;
	char *excluded;
	GSList *grafts;
	gboolean found;
	BraseroGraftPt *graft;
	BraseroBurnResult result;

	/* we go straight to its parent so that if the excluded uri
	 * is a graft point it won't be taken into account (it has 
	 * already with all other graft points) */
	parent = g_path_get_dirname (uri);
	while (parent [1] != '\0') {
		grafts = g_hash_table_lookup (data->grafts, parent);

		for (; grafts; grafts = grafts->next) {
			graft = grafts->data;

			/* see if the uri or one of its parent is excluded */
			found = FALSE;
			for (iter = graft->excluded; iter; iter = iter->next) {
				excluded = iter->data;
				size = strlen (excluded);

				if (!strncmp (excluded, uri, size)
				&& (*(uri + size) == '\0' ||  *(uri + size) == '/')) {
					found = TRUE;
					break;
				}
			}

			if (found)
				continue;

			/* there is at least one path which is not excluded */
			path = g_strconcat (graft->path,
					    uri + strlen (graft->uri),
					    NULL);

			result = brasero_mkisofs_base_write_graft (data,
								   uri,
								   path,
								   error);
			g_free (path);

			if (result != BRASERO_BURN_OK) {
				g_free (parent);
				return result;
			}
		}

		tmp = parent;
		parent = g_path_get_dirname (parent);
		g_free (tmp);
	}
	g_free (parent);

	return BRASERO_BURN_OK;
}

static gboolean
_foreach_write_grafts (const char *uri,
		       GSList *grafts,
		       BraseroWriteGraftData *data)
{
	BraseroBurnResult result;
	BraseroGraftPt *graft;

	for (; grafts; grafts = grafts->next) {
		graft = grafts->data;
		result = brasero_mkisofs_base_write_graft (data->data,
							   graft->uri,
							   graft->path,
							   data->error);
		if (result != BRASERO_BURN_OK)
			return TRUE;
	}

	return FALSE;
}

static gboolean
brasero_mkisofs_base_write_grafts (BraseroMkisofsData *data,
				   GError **error)
{
	BraseroWriteGraftData callback_data;
	gpointer result;

	callback_data.error = error;
	callback_data.data = data;

	result = g_hash_table_find (data->grafts,
				    (GHRFunc) _foreach_write_grafts,
				    &callback_data);

	if (result)
		return BRASERO_BURN_ERR;

	return BRASERO_BURN_OK;
}


static BraseroBurnResult
brasero_mkisofs_base_empty_directory (BraseroMkisofsBase *base,
				      BraseroMkisofsData *data,
				      const char *disc_path,
				      GError **error)
{
	BraseroBurnResult result;
	char *graft_point;

	/* This a special case for uri = NULL; that
	 * is treated as if it were a directory */
        if (!base->priv->emptydir) {
		char *dirpath;

		dirpath = g_strconcat (base->priv->tmpdir,
				       "/"BRASERO_BURN_TMP_FILE_NAME,
				       NULL);
		dirpath = mkdtemp (dirpath);
		if (dirpath == NULL) {
			g_free (dirpath);
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_TMP_DIR,
				     _("a temporary directory couldn't be created : %s"),
				     strerror (errno));
			return BRASERO_BURN_ERR;
		}

		base->priv->emptydir = dirpath;
	}

	graft_point = _build_graft_point (base->priv->emptydir, disc_path);
	result = _write_line (data->grafts_fd, graft_point, error);
	g_free (graft_point);

	return result;
}

static BraseroBurnResult
brasero_mkisofs_base_add_graft (BraseroMkisofsBase *base,
				BraseroGraftPt *graft,
				GError **error)
{
	GSList *list;
	BraseroMkisofsData *data;

	data = base->priv->data;

	/* check the file is local */
	if (g_str_has_prefix (graft->uri, "file://")) {
		BraseroBurnResult result;

		result = brasero_burn_common_check_local_file (graft->uri, error);
		if (result != BRASERO_BURN_OK)
			return result;
	}
	else {
		/* Error out, files must be local */
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the file is not stored locally"));
		return BRASERO_BURN_ERR;
	}

	/* make up the graft point */
	list = g_hash_table_lookup (data->grafts, graft->uri);
	if (list)
		g_hash_table_steal (data->grafts, graft->uri);

	list = g_slist_prepend (list, graft);
	g_hash_table_insert (data->grafts, graft->uri, list);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_mkisofs_base_init_data (BraseroMkisofsBase *base,
				GError **error)
{
	BraseroMkisofsData *data;
	BraseroBurnResult result;

	data = base->priv->data;
	if (!data)
		return BRASERO_BURN_ERR;

	result = brasero_burn_common_create_tmp_directory (&base->priv->tmpdir,
							   base->priv->overwrite,
							   error);
	if (result != BRASERO_BURN_OK)
		return result;

	/* open a new file list */
	if (base->priv->grafts_path) {
		if (base->priv->clean)
			g_remove (base->priv->grafts_path);

		g_free (base->priv->grafts_path);
	}

	base->priv->grafts_path = g_strconcat (base->priv->tmpdir,
					       "/"BRASERO_BURN_TMP_FILE_NAME,
					       NULL);
	data->grafts_fd = g_mkstemp (base->priv->grafts_path);
	if (data->grafts_fd < 0) {
		g_free (base->priv->grafts_path);
		base->priv->grafts_path = NULL;

		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_TMP_DIR,
			     _("a temporary file couldn't be created: %s"),
			     strerror (errno));
		return BRASERO_BURN_ERR;
	}

	/* open a new excluded list */
	if (base->priv->excluded_path) {
		if (base->priv->clean)
			g_remove (base->priv->excluded_path);

		g_free (base->priv->excluded_path);
	}

	base->priv->excluded_path = g_strconcat (base->priv->tmpdir, 
						 "/"BRASERO_BURN_TMP_FILE_NAME,
						 NULL);
	data->excluded_fd = g_mkstemp (base->priv->excluded_path);
	if (data->excluded_fd < 0) {
		g_free (base->priv->excluded_path);
		base->priv->excluded_path = NULL;

		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_TMP_DIR,
			     _("a temporary file couldn't be created: %s"),
			     strerror (errno));
		return BRASERO_BURN_ERR;
	}

	data->grafts = g_hash_table_new_full (g_str_hash,
					      g_str_equal,
					      NULL,
					      (GDestroyNotify) g_slist_free);
	return BRASERO_BURN_OK;
}

static BraseroBurnResult 
brasero_mkisofs_base_run (BraseroMkisofsBase *base,
			  GError **error)
{
	char *uri;
	GSList *list;
	GSList *grafts;
	GSList *excluded;
	BraseroMkisofsData data;
	BraseroBurnResult result;
	BraseroTrackSource *track = NULL;

	/* initialize data */
	bzero (&data, sizeof (data));
	base->priv->data = &data;
	result = brasero_mkisofs_base_init_data (base, error);
	if (result != BRASERO_BURN_OK)
		goto cleanup;

	/* we analyse the graft points:
	 * first add graft points and excluded. At the same time create a hash 
	 * table in which key = uri and value = graft points, a list of all
	 * the uris that have been excluded and a hash for non local files.
	 * once finished, for each excluded use the hash to see if there are not
	 * other paths at which the excluded uri must appear. If so, create an
	 * explicit graft point. */
	track = base->priv->source;

	grafts = track->contents.data.grafts;
	excluded = NULL;
	for (; grafts; grafts = grafts->next) {
		BraseroGraftPt *graft;

		graft = grafts->data;

		if (!graft->uri) {
			result = brasero_mkisofs_base_empty_directory (base,
								       &data,
								       graft->path,
								       error);
			if (result != BRASERO_BURN_OK)
				goto cleanup;

			continue;
		}

		result = brasero_mkisofs_base_add_graft (base,
							 graft,
							 error);
		if (result != BRASERO_BURN_OK)
			goto cleanup;

		for (list = graft->excluded; list; list = list->next) {
			char *uri;

			uri = list->data;
			excluded = g_slist_prepend (excluded, uri);
		}
	}

	/* write the grafts list */
	result = brasero_mkisofs_base_write_grafts (&data, error);
	if (result != BRASERO_BURN_OK)
		goto cleanup;

	/* now add the excluded and the paths where they still exist */
	for (; excluded; excluded = g_slist_remove (excluded, uri)) {
		uri = excluded->data;

		result = brasero_mkisofs_base_write_excluded (&data,
							      uri,
							      error);
		if (result != BRASERO_BURN_OK)
			goto cleanup;

		/* One thing is a bit tricky here. If we exclude one file mkisofs considers
		 * it to be excluded for all paths. So if one file appears multiple times
		 * and is excluded just once, it will always be excluded that's why we create
		 * explicit graft points where it is not excluded.*/
		result = brasero_mkisofs_base_write_excluded_valid_paths (&data,
									  uri,
									  error);
		if (result != BRASERO_BURN_OK)
			goto cleanup;
	}

	/* write the global excluded files list */
	for (excluded = track->contents.data.excluded; excluded; excluded = excluded->next) {
		uri = excluded->data;

		result = brasero_mkisofs_base_write_excluded (&data,
							      uri,
							      error);
		if (result != BRASERO_BURN_OK)
			goto cleanup;
	}

     cleanup:

	base->priv->data = NULL;

	/* now we clean data we have the most important :
	 * graft and excluded list, flags and that's what
	 * we're going to use when we'll start the image 
	 * creation */
	if (data.grafts_fd)
		close (data.grafts_fd);
	if (data.excluded_fd)
		close (data.excluded_fd);

	if (data.grafts) {
		g_hash_table_destroy (data.grafts);
		data.grafts = NULL;
	}

	return result;
}

static BraseroBurnResult
brasero_mkisofs_base_set_source (BraseroJob *job,
				 const BraseroTrackSource *source,
				 GError **error)
{
	char *label;
	BraseroMkisofsBase *base;

	g_return_val_if_fail (source != NULL, BRASERO_BURN_ERR);

	base = BRASERO_MKISOFS_BASE (job);

	if (source->type != BRASERO_TRACK_SOURCE_DATA)
		BRASERO_JOB_NOT_SUPPORTED (base);

	brasero_mkisofs_base_clean_track (base);

	if (base->priv->source) {
		brasero_track_source_free (base->priv->source);
		base->priv->source = NULL;
	}

	label = source->contents.data.label;
	if (label && (strlen (label) > 32)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_INVALID_FILE,
			     _("The label for the image is too long."));
		return BRASERO_BURN_ERR;
	}

	base->priv->source = brasero_track_source_copy (source);
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_mkisofs_base_set_output (BraseroImager *imager,
				 const gchar *output,
				 gboolean overwrite,
				 gboolean clean,
				 GError **error)
{
	BraseroMkisofsBase *base;

	base = BRASERO_MKISOFS_BASE (imager);

	brasero_mkisofs_base_clean_tmpdir (base);

	/* NOTE: we are supposed to create it so we own this directory */
	if (output)
		base->priv->tmpdir = g_strdup (output);

	base->priv->clean = clean;
	base->priv->overwrite = overwrite;
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_mkisofs_base_set_output_type (BraseroImager *imager,
				      BraseroTrackSourceType type,
				      BraseroImageFormat format,
				      GError **error)
{
	BraseroMkisofsBase *base;

	base = BRASERO_MKISOFS_BASE (imager);

	if (type != BRASERO_TRACK_SOURCE_GRAFTS
	&&  type != BRASERO_TRACK_SOURCE_DEFAULT)
		BRASERO_JOB_NOT_SUPPORTED (base);

	if (!(format & BRASERO_IMAGE_FORMAT_ISO))
		BRASERO_JOB_NOT_SUPPORTED (base);

	/* NOTE : no need to keep this value since it can only output grafts */
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_mkisofs_base_get_track (BraseroImager *imager,
				BraseroTrackSource **source,
				GError **error)
{
	BraseroMkisofsBase *base;
	BraseroTrackSource *retval;

	g_return_val_if_fail (source != NULL, BRASERO_BURN_ERR);

	base = BRASERO_MKISOFS_BASE (imager);

	if (!base->priv->grafts_path) {
		BraseroBurnResult result;

		result = brasero_mkisofs_base_run (base, error);

		if (result != BRASERO_BURN_OK)
			return result;
	}

	retval = g_new0 (BraseroTrackSource, 1);

	retval->type = BRASERO_TRACK_SOURCE_GRAFTS;
	retval->contents.grafts.grafts_path = g_strdup (base->priv->grafts_path);
	retval->contents.grafts.excluded_path = g_strdup (base->priv->excluded_path);
	retval->contents.grafts.label = g_strdup (base->priv->source->contents.data.label);

	retval->format = base->priv->source->format;

	*source = retval;
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_mkisofs_base_get_size (BraseroImager *imager,
			       gint64 *size,
			       gboolean sectors,
			       GError **error)
{
	BraseroJob *slave;
	BraseroMkisofsBase *base;

	/* BraseroMkisofsBase has no size to return except when it's running
	 * its slave to download all non local files */
	base = BRASERO_MKISOFS_BASE (imager);

	slave = brasero_job_get_slave (BRASERO_JOB (imager));
	if (!slave)
		BRASERO_JOB_NOT_READY (base);

	return brasero_imager_get_size (BRASERO_IMAGER (slave), size, sectors, error);
}

