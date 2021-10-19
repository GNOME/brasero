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

#include <string.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <errno.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include "brasero-xfer.h"
#include "burn-debug.h"

/* FIXME! one way to improve this would be to add auto mounting */
struct _BraseroXferCtx {
	goffset total_size;

	goffset bytes_copied;
	goffset current_bytes_copied;
};

static void
brasero_xfer_progress_cb (goffset current_num_bytes,
			  goffset total_num_bytes,
			  gpointer callback_data)
{
	BraseroXferCtx *ctx = callback_data;
	ctx->current_bytes_copied = current_num_bytes;
}

static gboolean
brasero_xfer_file_transfer (BraseroXferCtx *ctx,
			    GFile *src,
			    GFile *dest,
			    GCancellable *cancel,
			    GError **error)
{
	gboolean result;
	gchar *name;

	name = g_file_get_basename (src);
	BRASERO_BURN_LOG ("Downloading %s", name);
	g_free (name);

	result = g_file_copy (src,
			      dest,
			      G_FILE_COPY_ALL_METADATA,
			      cancel,
			      brasero_xfer_progress_cb,
			      ctx,
			      error);

	return result;
}

static gboolean
brasero_xfer_recursive_transfer (BraseroXferCtx *ctx,
				 GFile *src,
				 GFile *dest,
				 GCancellable *cancel,
				 GError **error)
{
	GFileInfo *info;
	gboolean result = TRUE;
	GFileEnumerator *enumerator;

	BRASERO_BURN_LOG ("Downloading directory contents");
	enumerator = g_file_enumerate_children (src,
						G_FILE_ATTRIBUTE_STANDARD_TYPE ","
						G_FILE_ATTRIBUTE_STANDARD_NAME ","
						G_FILE_ATTRIBUTE_STANDARD_SIZE,
						G_FILE_QUERY_INFO_NONE,	/* follow symlinks */
						cancel,
						error);
	if (!enumerator)
		return FALSE;

	while ((info = g_file_enumerator_next_file (enumerator, cancel, error))) {
		GFile *dest_child;
		GFile *src_child;

		src_child = g_file_get_child (src, g_file_info_get_name (info));
		dest_child = g_file_get_child (dest, g_file_info_get_name (info));

		if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
			gchar *path;

			path = g_file_get_path (dest_child);
			BRASERO_BURN_LOG ("Creating directory %s", path);

			/* create a directory with the same name and explore it */
			if (g_mkdir (path, S_IRWXU)) {
                                int errsv = errno;
				g_set_error (error,
					     BRASERO_BURN_ERROR,
					     BRASERO_BURN_ERROR_GENERAL,
					     _("Directory could not be created (%s)"),
					     g_strerror (errsv));
				result = FALSE;
			}
			else {
				result = brasero_xfer_recursive_transfer (ctx,
									  src_child,
									  dest_child,
									  cancel,
									  error);
			}

			g_free (path);
		}
		else {
			result = brasero_xfer_file_transfer (ctx,
							     src_child,
							     dest_child,
							     cancel,
							     error);
			ctx->bytes_copied += g_file_info_get_size (info);
		}

		g_object_unref (info);
		g_object_unref (src_child);
		g_object_unref (dest_child);

		if (!result)
			break;

		if (g_cancellable_is_cancelled (cancel))
			break;
	}

	g_file_enumerator_close (enumerator, cancel, NULL);
	g_object_unref (enumerator);

	return result;
}

static gboolean
brasero_xfer_get_download_size (BraseroXferCtx *ctx,
				GFile *src,
				GCancellable *cancel,
				GError **error)
{
	GFileEnumerator *enumerator;
	GFileInfo *info;

	enumerator = g_file_enumerate_children (src,
						G_FILE_ATTRIBUTE_STANDARD_TYPE ","
						G_FILE_ATTRIBUTE_STANDARD_NAME ","
						G_FILE_ATTRIBUTE_STANDARD_SIZE,
						G_FILE_QUERY_INFO_NONE,	/* follow symlinks */
						cancel,
						error);
	if (!enumerator)
		return FALSE;

	while ((info = g_file_enumerator_next_file (enumerator, cancel, error))) {
		if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
			GFile *child;
	
			child = g_file_get_child (src, g_file_info_get_name (info));
			brasero_xfer_get_download_size (ctx, child, cancel, error);
			g_object_unref (child);
		}
		else
			ctx->total_size += g_file_info_get_size (info);

		g_object_unref (info);

		if (g_cancellable_is_cancelled (cancel))
			break;
	}

	g_file_enumerator_close (enumerator, cancel, NULL);
	g_object_unref (enumerator);

	return TRUE;
}

gboolean
brasero_xfer_start (BraseroXferCtx *ctx,
		    GFile *src,
		    GFile *dest,
		    GCancellable *cancel,
		    GError **error)
{
	gboolean result;
	GFileInfo *info;

	bzero (ctx, sizeof (BraseroXferCtx));

	/* First step: get all the total size of what we have to move */
	info = g_file_query_info (src,
				  G_FILE_ATTRIBUTE_STANDARD_TYPE ","
				  G_FILE_ATTRIBUTE_STANDARD_SIZE,
				  G_FILE_QUERY_INFO_NONE, /* follow symlinks */
				  cancel,
				  error);
	if (!info)
		return FALSE;

	if (g_cancellable_is_cancelled (cancel))
		return FALSE;

	/* Retrieve the size of all the data. */
	if (g_file_info_get_file_type (info) != G_FILE_TYPE_DIRECTORY) {
		BRASERO_BURN_LOG ("Downloading file (size = %lli)", g_file_info_get_size (info));
		ctx->total_size = g_file_info_get_size (info);
	}
	else {
		brasero_xfer_get_download_size (ctx, src, cancel, error);
		BRASERO_BURN_LOG ("Downloading directory (size = %lli)", ctx->total_size);
	}

	ctx->bytes_copied = 0;

	/* Step 2: start downloading */
	if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
		gchar *dest_path;

		dest_path = g_file_get_path (dest);

		/* remove the temporary file that was created */
		g_remove (dest_path);
		if (g_mkdir_with_parents (dest_path, S_IRWXU)) {
                        int errsv = errno;

			g_free (dest_path);
			g_object_unref (info);

			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("Directory could not be created (%s)"),
				     g_strerror (errsv));
			return FALSE;
		}

		BRASERO_BURN_LOG ("Created directory %s", dest_path);
		g_free (dest_path);

		result = brasero_xfer_recursive_transfer (ctx, src, dest, cancel, error);
	}
	else {
		g_file_delete (dest, cancel, NULL);
		result = brasero_xfer_file_transfer (ctx, src, dest, cancel, error);
		ctx->bytes_copied += g_file_info_get_size (info);
	}

	g_object_unref (info);

	return result;
}

typedef struct _BraseroXferThreadData BraseroXferThreadData;
struct _BraseroXferThreadData
{
	BraseroXferCtx *ctx;

	GFile *dest;
	GFile *source;

	GCancellable *cancel;

	GMainLoop *loop;

	/* These are set in the thread */
	gboolean result;
	GError *error;
};

static gpointer
brasero_xfer_thread (gpointer callback_data)
{
	BraseroXferThreadData *data = callback_data;
	GError *error = NULL;

	data->result = brasero_xfer_start (data->ctx,
					   data->source,
					   data->dest,
					   data->cancel,
					   &error);
	data->error = error;

	/* Stop a loop which would be waiting for us */
	if (g_main_loop_is_running (data->loop))
		g_main_loop_quit (data->loop);

	g_thread_exit (NULL);

	return NULL;
}

static void
brasero_xfer_wait_cancelled_cb (GCancellable *cancel,
				BraseroXferThreadData *data)
{
	/* Now we can safely stop the main loop */
	if (g_main_loop_is_running (data->loop))
		g_main_loop_quit (data->loop);
}

gboolean
brasero_xfer_wait (BraseroXferCtx *ctx,
		   const gchar *src_uri,
		   const gchar *dest_path,
		   GCancellable *cancel,
		   GError **error)
{
	BraseroXferThreadData data = { NULL, };
	gulong cancel_sig;
	GThread *thread;

	bzero (ctx, sizeof (BraseroXferCtx));

	cancel_sig = g_signal_connect (cancel,
				       "cancelled",
				       G_CALLBACK (brasero_xfer_wait_cancelled_cb),
				       &data);

	data.ctx = ctx;
	data.cancel = g_object_ref (cancel);
	data.source = g_file_new_for_uri (src_uri);
	data.dest = g_file_new_for_path (dest_path);
	data.loop = g_main_loop_new (NULL, FALSE);

	thread = g_thread_create (brasero_xfer_thread,
				  &data,
				  TRUE,
				  error);
	if (!thread) {
		g_signal_handler_disconnect (cancel, cancel_sig);
		g_object_unref (cancel);

		g_main_loop_unref (data.loop);
		g_object_unref (data.source);
		g_object_unref (data.dest);
		return FALSE;
	}

	g_main_loop_run (data.loop);

	/* Join the thread and wait for its end.
	 * NOTE: necessary to free thread data. */
	g_thread_join (thread);

	/* clean up */
	g_main_loop_unref (data.loop);
	data.loop = NULL;

	g_object_unref (data.source);
	data.source = NULL;

	g_object_unref (data.dest);
	data.dest = NULL;

	/* Check results */
	g_signal_handler_disconnect (cancel, cancel_sig);
	if (g_cancellable_is_cancelled (cancel)) {
		g_object_unref (cancel);

		if (data.error) {
			g_error_free (data.error);
			data.error = NULL;
		}

		return FALSE;
	}

	g_object_unref (cancel);

	if (data.error) {
		BRASERO_BURN_LOG ("Error %s", data.error->message);
		g_propagate_error (error, data.error);
		return FALSE;
	}

	BRASERO_BURN_LOG ("File successfully downloaded to %s", dest_path);

	return data.result;
}

BraseroXferCtx *
brasero_xfer_new (void)
{
	BraseroXferCtx *ctx;

	ctx = g_new0 (BraseroXferCtx, 1);

	return ctx;
}

void
brasero_xfer_free (BraseroXferCtx *ctx)
{
	g_free (ctx);
}

gboolean
brasero_xfer_get_progress (BraseroXferCtx *ctx,
			   goffset *written,
			   goffset *total)
{
	if (written)
		*written = ctx->current_bytes_copied + ctx->bytes_copied;

	if (total)
		*total = ctx->total_size;

	return TRUE;
}
