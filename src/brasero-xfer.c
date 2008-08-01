/***************************************************************************
 *            burn-xfer.c
 *
 *  Sun Sep 10 08:53:22 2006
 *  Copyright  2006  philippe
 *  <philippe@Rouquier Philippe.localdomain>
 ****************************************************************************/

/*
 * Brasero is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * Brasero is distributed in the hope that it will be useful,
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
#include <gio/gio.h>

#include "brasero-xfer.h"
#include "brasero-io.h"
#include "burn-basics.h"
#include "brasero-utils.h"

struct _BraseroXferCtx {
	BraseroIOJobBase *base;

	gint64 current_copy_size;
	gint64 current_bytes_copied;

	GMainLoop *loop;
	GError *error;

	gboolean cancel;
};

/* This one is for error reporting */
static void
brasero_xfer_result_cb (GObject *object,
			GError *error,
			const gchar *uri,
			GFileInfo *info,
			gpointer callback_data)
{
	BraseroXferCtx *ctx = callback_data;

	if (error) {
		ctx->error = g_error_copy (error);
		if (g_main_loop_is_running (ctx->loop))
			g_main_loop_quit (ctx->loop);
		return;
	}

	/* we've been cancelled */
	if (ctx->cancel) {
		if (g_main_loop_is_running (ctx->loop))
			g_main_loop_quit (ctx->loop);
		return;
	}

	if (g_main_loop_is_running (ctx->loop))
		g_main_loop_quit (ctx->loop);
}

/* This one is for progress reporting */
static void
brasero_xfer_progress_cb (GObject *object,
			  BraseroIOJobProgress *progress,
			  gpointer user_data)
{
	BraseroXferCtx *ctx = user_data;

	if (ctx->cancel)
		return;

	ctx->current_copy_size = brasero_io_job_progress_get_total (progress);
	ctx->current_bytes_copied = brasero_io_job_progress_get_read (progress);;
}

BraseroBurnResult
brasero_xfer (BraseroXferCtx *ctx,
	      const gchar *src_uri,
	      const gchar *dest_path,
	      GError **error)
{
	BraseroIO *io;

	ctx->base = brasero_io_register (NULL,
					 brasero_xfer_result_cb,
					 NULL,
					 brasero_xfer_progress_cb);

	/* download */
	bzero (ctx, sizeof (BraseroXferCtx));

	io = brasero_io_get_default ();
	brasero_io_xfer (io, src_uri, dest_path, ctx->base, BRASERO_IO_INFO_NONE, ctx);

	ctx->loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (ctx->loop);
	g_main_loop_unref (ctx->loop);
	ctx->loop = NULL;

	if (!ctx->cancel)
		return BRASERO_BURN_CANCEL;

	if (ctx->error) {
		g_propagate_error (error, ctx->error);
		return BRASERO_BURN_ERR;
	}

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_xfer_cancel (BraseroXferCtx *ctx)
{
	BraseroIO *io;

	ctx->cancel = 1;

	io = brasero_io_get_default ();
	brasero_io_cancel_by_data (io, ctx);
	g_object_unref (io);

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
