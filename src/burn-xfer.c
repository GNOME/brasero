/***************************************************************************
 *            burn-xfer.c
 *
 *  Sun Sep 10 08:53:22 2006
 *  Copyright  2006  philippe
 *  <philippe@algernon.localdomain>
 ****************************************************************************/

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */

#include <string.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <libgnomevfs/gnome-vfs.h>

#include "burn-xfer.h"
#include "burn-basics.h"
#include "utils.h"

struct _BraseroXferCtx {
	gint64 current_copy_size;
	gint64 current_bytes_copied;

	GnomeVFSAsyncHandle *handle;
	GMainLoop *loop;
	GError *error;
};

/* This one is for error reporting */
static int
brasero_xfer_async_cb (GnomeVFSAsyncHandle *handle,
		       GnomeVFSXferProgressInfo *info,
		       gpointer user_data)
{
	BraseroXferCtx *ctx = user_data;

	/* we've been cancelled */
	if (!ctx->handle)
		return FALSE;

	if (info->phase == GNOME_VFS_XFER_PHASE_COMPLETED) {
		if (g_main_loop_is_running (ctx->loop))
			g_main_loop_quit (ctx->loop);

		return FALSE;
	}
	else if (info->status != GNOME_VFS_XFER_PROGRESS_STATUS_OK) {
		ctx->error = g_error_new (BRASERO_BURN_ERROR,
					  BRASERO_BURN_ERROR_GENERAL,
					  gnome_vfs_result_to_string (info->vfs_status));
		return GNOME_VFS_XFER_ERROR_ACTION_ABORT;
	}

	return TRUE;
}

/* This one is for progress reporting */
static gint
brasero_xfer_cb (GnomeVFSXferProgressInfo *info,
		 gpointer user_data)
{
	BraseroXferCtx *ctx = user_data;

	if (!ctx->handle)
		return FALSE;

	if (info->file_size)
		ctx->current_copy_size = info->bytes_total;

	if (info->bytes_copied)
		ctx->current_bytes_copied = info->total_bytes_copied;

	return TRUE;
}

BraseroBurnResult
brasero_xfer (BraseroXferCtx *ctx,
	      GnomeVFSURI *vfsuri,
	      GnomeVFSURI *tmpuri,
	      GError **error)
{
	GList *dest_list, *src_list;
	GnomeVFSResult res;

	/* download */
	dest_list = g_list_append (NULL, tmpuri);
	src_list = g_list_append (NULL, vfsuri);

	bzero (ctx, sizeof (BraseroXferCtx));
	res = gnome_vfs_async_xfer (&ctx->handle,
				    src_list,
				    dest_list,
				    GNOME_VFS_XFER_DEFAULT |
				    GNOME_VFS_XFER_USE_UNIQUE_NAMES |
				    GNOME_VFS_XFER_RECURSIVE,
				    GNOME_VFS_XFER_ERROR_MODE_ABORT,
				    GNOME_VFS_XFER_OVERWRITE_MODE_ABORT,
				    GNOME_VFS_PRIORITY_DEFAULT,
				    brasero_xfer_async_cb,
				    ctx,
				    brasero_xfer_cb,
				    ctx);

	g_list_free (src_list);
	g_list_free (dest_list);

	if (res != GNOME_VFS_OK) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     gnome_vfs_result_to_string (res));

		return BRASERO_BURN_ERR;
	}

	ctx->loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (ctx->loop);
	g_main_loop_unref (ctx->loop);
	ctx->loop = NULL;

	if (!ctx->handle)
		return BRASERO_BURN_CANCEL;

	ctx->handle = NULL;

	if (ctx->error) {
		g_propagate_error (error, ctx->error);
		return BRASERO_BURN_ERR;
	}

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_xfer_cancel (BraseroXferCtx *ctx)
{
	if (ctx->handle) {
		gnome_vfs_async_cancel (ctx->handle);
		ctx->handle = NULL;
	}

	if (ctx->loop && g_main_loop_is_running (ctx->loop))
		g_main_loop_quit (ctx->loop);

	return BRASERO_BURN_OK;
}

BraseroXferCtx *
brasero_xfer_new (void)
{
	return g_new0 (BraseroXferCtx, 1);
}

void
brasero_xfer_free (BraseroXferCtx *ctx)
{
	g_free (ctx);
}

BraseroBurnResult
brasero_xfer_get_progress (BraseroXferCtx *ctx,
			   gint64 *written,
			   gint64 *total)
{
	if (written)
		*written = ctx->current_bytes_copied;

	if (total)
		*total = ctx->current_copy_size;

	return BRASERO_BURN_OK;
}
