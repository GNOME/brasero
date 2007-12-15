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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <libgnomevfs/gnome-vfs.h>

#include "burn-basics.h"
#include "burn-debug.h"
#include "burn-track.h"
#include "burn-mkisofs-base.h"

struct _BraseroMkisofsBase {
	const gchar *emptydir;

	gint grafts_fd;
	gint excluded_fd;

	GHashTable *grafts;
};
typedef struct _BraseroMkisofsBase BraseroMkisofsBase;

struct _BraseroWriteGraftData {
	BraseroMkisofsBase *base;
	GError **error;
};
typedef struct _BraseroWriteGraftData BraseroWriteGraftData;

static void
brasero_mkisofs_base_clean (BraseroMkisofsBase *base)
{
	/* now we clean base we have the most important :
	 * graft and excluded list, flags and that's what
	 * we're going to use when we'll start the image 
	 * creation */
	if (base->grafts_fd)
		close (base->grafts_fd);
	if (base->excluded_fd)
		close (base->excluded_fd);

	if (base->grafts) {
		g_hash_table_destroy (base->grafts);
		base->grafts = NULL;
	}
}

static BraseroBurnResult
_write_line (int fd, const gchar *filepath, GError **error)
{
	gint len;
	gint w_len;

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
brasero_mkisofs_base_write_excluded (BraseroMkisofsBase *base,
				     const gchar *uri,
				     GError **error)
{
	gint num;
	gint forbidden;
	gchar *character;
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

	/* we need to escape some characters like []\? since in this file we
	 * can use glob like expressions. */
	character = localpath;
	forbidden = 0;
	num = 0;

	while (character [0]) {
		if (character [0] == '['
		||  character [0] == ']'
		||  character [0] == '?'
		||  character [0] == '\\')
			forbidden++;

		num++;
		character++;
	}

	if (forbidden) {
		gchar *tmp;
		gint i;

		tmp = g_new0 (gchar, num + forbidden + 1);
		character = tmp;

		for (i = 0; i < num; i++) {
			if (localpath [i] == '['
			||  localpath [i] == ']'
			||  localpath [i] == '?'
			||  localpath [i] == '\\') {
				character [i] = '\\';
				character++;
			}
			character [i] = localpath [i];
		}

		BRASERO_BURN_LOG ("Escaped path %s into %s", localpath, tmp);
		g_free (localpath);
		localpath = tmp;
	}

	/* we just ignore if localpath is NULL:
	 * - it could be a non local whose graft point couldn't be downloaded */
	if (localpath)
		result = _write_line (base->excluded_fd, localpath, error);
	
	return result;
}

static gchar *
_escape_path (const gchar *str)
{
	gchar *escaped, *d;
	const gchar *s;
	gint len;

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

		/* There is a graft because either it's not at the root of the 
		 * disc or because its name has changed. */
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
brasero_mkisofs_base_write_graft (BraseroMkisofsBase *base,
				  const gchar *uri,
				  const gchar *disc_path,
				  GError **error)
{
	gchar *graft_point;
	BraseroBurnResult result;

	/* build up graft and write it */
	graft_point = _build_graft_point (uri, disc_path);

	if (!graft_point) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("null graft point"));
		return BRASERO_BURN_ERR;
	}

	result = _write_line (base->grafts_fd, graft_point, error);
	g_free (graft_point);
	if(result != BRASERO_BURN_OK)
		return result;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_mkisofs_base_write_excluded_valid_paths (BraseroMkisofsBase *base,
						 const gchar *uri,
						 GError **error)
{
	gint size;
	gchar *tmp;
	gchar *path;
	gchar *parent;
	GSList *iter;
	gchar *excluded;
	GSList *grafts;
	gboolean found;
	BraseroGraftPt *graft;
	BraseroBurnResult result;

	/* we go straight to its parent so that if the excluded uri
	 * is a graft point it won't be taken into account (it has 
	 * already with all other graft points) */
	parent = g_path_get_dirname (uri);
	while (parent [1] != '\0') {
		grafts = g_hash_table_lookup (base->grafts, parent);

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

			result = brasero_mkisofs_base_write_graft (base,
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
_foreach_write_grafts (const gchar *uri,
		       GSList *grafts,
		       BraseroWriteGraftData *data)
{
	BraseroBurnResult result;
	BraseroGraftPt *graft;

	for (; grafts; grafts = grafts->next) {
		graft = grafts->data;
		result = brasero_mkisofs_base_write_graft (data->base,
							   graft->uri,
							   graft->path,
							   data->error);
		if (result != BRASERO_BURN_OK)
			return TRUE;
	}

	return FALSE;
}

static gboolean
brasero_mkisofs_base_write_grafts (BraseroMkisofsBase *base,
				   GError **error)
{
	BraseroWriteGraftData callback_data;
	gpointer result;

	callback_data.error = error;
	callback_data.base = base;

	result = g_hash_table_find (base->grafts,
				    (GHRFunc) _foreach_write_grafts,
				    &callback_data);

	if (result)
		return BRASERO_BURN_ERR;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_mkisofs_base_empty_directory (BraseroMkisofsBase *base,
				      const gchar *disc_path,
				      GError **error)
{
	BraseroBurnResult result;
	gchar *graft_point;

	/* This a special case for uri = NULL; that
	 * is treated as if it were a directory */
	graft_point = _build_graft_point (base->emptydir, disc_path);
	result = _write_line (base->grafts_fd, graft_point, error);
	g_free (graft_point);

	return result;
}

static BraseroBurnResult
brasero_mkisofs_base_add_graft (BraseroMkisofsBase *base,
				BraseroGraftPt *graft,
				GError **error)
{
	GSList *list;

	/* check the file is local */
	if (!g_str_has_prefix (graft->uri, "file://")) {
		/* Error out, files must be local */
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the file is not stored locally"));
		return BRASERO_BURN_ERR;
	}

	/* make up the graft point */
	list = g_hash_table_lookup (base->grafts, graft->uri);
	if (list)
		g_hash_table_steal (base->grafts, graft->uri);

	list = g_slist_prepend (list, graft);
	g_hash_table_insert (base->grafts, graft->uri, list);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_mkisofs_base_write_to_files (GSList *grafts,
				     GSList *excluded,
				     const gchar *emptydir,
				     const gchar *grafts_path,
				     const gchar *excluded_path,
				     GError **error)
{
	gchar *uri;
	GSList *list;
	GSList *grafts_excluded;
	BraseroMkisofsBase base;
	BraseroBurnResult result;

	/* initialize base */
	bzero (&base, sizeof (base));

	base.grafts_fd = open (grafts_path, O_WRONLY|O_TRUNC|O_EXCL);
	if (base.grafts_fd == -1) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     strerror (errno));
		return BRASERO_BURN_ERR;
	}

	base.excluded_fd = open (excluded_path, O_WRONLY|O_TRUNC|O_EXCL);
	if (base.excluded_fd == -1) {
		close (base.excluded_fd);
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     strerror (errno));
		return BRASERO_BURN_ERR;
	}

	base.emptydir = emptydir;

	base.grafts = g_hash_table_new_full (g_str_hash,
					     g_str_equal,
					     NULL,
					    (GDestroyNotify) g_slist_free);

	/* we analyse the graft points:
	 * first add graft points and excluded. At the same time create a hash 
	 * table in which key = uri and value = graft points, a list of all
	 * the uris that have been excluded and a hash for non local files.
	 * once finished, for each excluded use the hash to see if there are not
	 * other paths at which the excluded uri must appear. If so, create an
	 * explicit graft point. */
	grafts_excluded = NULL;
	for (; grafts; grafts = grafts->next) {
		BraseroGraftPt *graft;

		graft = grafts->data;

		if (!graft->uri) {
			result = brasero_mkisofs_base_empty_directory (&base,
								       graft->path,
								       error);
			if (result != BRASERO_BURN_OK)
				goto cleanup;

			continue;
		}

		result = brasero_mkisofs_base_add_graft (&base,
							 graft,
							 error);
		if (result != BRASERO_BURN_OK)
			goto cleanup;

		for (list = graft->excluded; list; list = list->next) {
			gchar *uri;

			uri = list->data;
			grafts_excluded = g_slist_prepend (grafts_excluded, uri);
		}
	}

	/* write the grafts list */
	result = brasero_mkisofs_base_write_grafts (&base, error);
	if (result != BRASERO_BURN_OK)
		goto cleanup;

	/* now add the excluded and the paths where they still exist */
	for (; grafts_excluded; grafts_excluded = g_slist_remove (grafts_excluded, uri)) {
		uri = grafts_excluded->data;

		result = brasero_mkisofs_base_write_excluded (&base,
							      uri,
							      error);
		if (result != BRASERO_BURN_OK)
			goto cleanup;

		/* One thing is a bit tricky here. If we exclude one file mkisofs considers
		 * it to be excluded for all paths. So if one file appears multiple times
		 * and is excluded just once, it will always be excluded that's why we create
		 * explicit graft points where it is not excluded.*/
		result = brasero_mkisofs_base_write_excluded_valid_paths (&base,
									  uri,
									  error);
		if (result != BRASERO_BURN_OK)
			goto cleanup;
	}

	/* write the global excluded files list */
	for (; excluded; excluded = excluded->next) {
		uri = excluded->data;

		result = brasero_mkisofs_base_write_excluded (&base,
							      uri,
							      error);
		if (result != BRASERO_BURN_OK)
			goto cleanup;
	}

	brasero_mkisofs_base_clean (&base);
	return BRASERO_BURN_OK;


cleanup:

	brasero_mkisofs_base_clean (&base);
	return result;
}
