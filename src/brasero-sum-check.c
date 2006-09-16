/***************************************************************************
 *            brasero-sum-checks.c
 *
 *  lun sep 11 07:39:26 2006
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


#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/param.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>

#include "brasero-sum-check.h"
#include "burn-sum.h"
#include "burn-basics.h"
#include "burn-md5.h"
#include "brasero-ncb.h"

struct _BraseroSumCheckCtx {
	BraseroMD5Ctx *md5_ctx;
	GSList *wrong_sums;
	const gchar *root;

	gint files_num;
	gint file_nb;

	gboolean cancel;
	GMainLoop *loop;
	GError *error;
};

static void
brasero_sum_check_reset (BraseroSumCheckCtx *ctx)
{
	if (ctx->error) {
		g_error_free (ctx->error);
		ctx->error = NULL;
	}
			
	g_slist_foreach (ctx->wrong_sums, (GFunc) g_free, NULL);
	g_slist_free (ctx->wrong_sums);
	ctx->wrong_sums = NULL;

	ctx->cancel = FALSE;
}

static gboolean
brasero_sum_check_get_line_num (BraseroSumCheckCtx *ctx,
				FILE *file)
{
	gint c;
	gint num = 0;

	while ((c = getc (file)) != EOF) {
		if (c == '\n')
			num ++;
	}

	if (!feof (file)) {
		ctx->error = g_error_new (BRASERO_BURN_ERROR,
					  BRASERO_BURN_ERROR_GENERAL,
					  strerror (errno));
		return FALSE;
	}

	rewind (file);

	ctx->files_num = num;
	return TRUE;
}

static gpointer
brasero_sum_check_thread (gpointer data)
{
	gint root_len;
	FILE *file = NULL;
	BraseroSumCheckCtx *ctx = data;
	gchar filename [MAXPATHLEN + 1];

	root_len = strlen (ctx->root);
	memcpy (filename, ctx->root, root_len);
	filename [root_len ++] = G_DIR_SEPARATOR;

	strcpy (filename + root_len, BRASERO_CHECKSUM_FILE);
	file = fopen (filename, "r");
	if (!file) {
		ctx->error = g_error_new (BRASERO_BURN_ERROR,
					  BRASERO_BURN_ERROR_GENERAL,
					  strerror (errno));
		goto end;
	}

	/* we need to get the number of files at this time and rewind */
	ctx->file_nb = 0;
	if (!brasero_sum_check_get_line_num (ctx, file)) {
		ctx->error = g_error_new (BRASERO_BURN_ERROR,
					  BRASERO_BURN_ERROR_GENERAL,
					  strerror (errno));
		goto end;
	}

	ctx->md5_ctx = brasero_md5_new ();
	while (1) {
		BraseroBurnResult result;
		gchar checksum_file [33];
		gchar checksum_real [33];
		gint i;
		int c;

		if (ctx->cancel)
			break;

		if (fread (checksum_file, 1, 32, file) != 32) {
			if (!feof (file))
				ctx->error = g_error_new (BRASERO_BURN_ERROR,
							  BRASERO_BURN_ERROR_GENERAL,
							  strerror (errno));
			break;
		}

		checksum_file [32] = '\0';
		if (ctx->cancel)
			break;

		/* skip spaces in between */
		while (1) {
			c = fgetc (file);

			if (c == EOF) {
				if (feof (file))
					goto end;

				if (errno == EAGAIN || errno == EINTR)
					continue;

				ctx->error = g_error_new (BRASERO_BURN_ERROR,
							  BRASERO_BURN_ERROR_GENERAL,
							  strerror (errno));
				goto end;
			}

			if (!isspace (c)) {
				filename [root_len] = c;
				break;
			}
		}

		/* get the filename */
		i = root_len + 1;
		while (1) {
			c = fgetc (file);
			if (c == EOF) {
				if (feof (file))
					goto end;

				if (errno == EAGAIN || errno == EINTR)
					continue;

				ctx->error = g_error_new (BRASERO_BURN_ERROR,
							  BRASERO_BURN_ERROR_GENERAL,
							  strerror (errno));
				goto end;
			}

			if (c == '\n')
				break;

			if (i < MAXPATHLEN)
				filename [i ++] = c;
		}

		if (i > MAXPATHLEN) {
			/* we ignore paths that are too long */
			continue;
		}

		filename [i] = 0;
		result = brasero_md5_sum_to_string (ctx->md5_ctx,
						    filename,
						    checksum_real,
						    &ctx->error);

		if (result == BRASERO_BURN_RETRY)
			ctx->wrong_sums = g_slist_prepend (ctx->wrong_sums,
							   g_strdup (filename));
		else if (result != BRASERO_BURN_OK)
			break;

		ctx->file_nb++;
		if (strcmp (checksum_file, checksum_real))
			ctx->wrong_sums = g_slist_prepend (ctx->wrong_sums,
							   g_strdup (filename));

		if (ctx->cancel)
			break;
	}

end:
	if (file)
		fclose (file);

	if (ctx->md5_ctx) {
		brasero_md5_free (ctx->md5_ctx);
		ctx->md5_ctx = NULL;
	}

	if (ctx->loop
	&&  g_main_loop_is_running (ctx->loop))
		g_main_loop_quit (ctx->loop);

	g_thread_exit (NULL);
	return NULL;
}

BraseroBurnResult
brasero_sum_check (BraseroSumCheckCtx *ctx,
		   const gchar *root,
		   GSList **wrong_sums,
		   GError **error)
{
	GThread *thread;

	g_return_val_if_fail (wrong_sums != NULL, FALSE);

	brasero_sum_check_reset (ctx);
	ctx->root = root;

	thread = g_thread_create (brasero_sum_check_thread,
				  ctx,
				  TRUE,
				  error);
	if (!thread)
		return BRASERO_BURN_ERR;

	ctx->loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (ctx->loop);
	g_main_loop_unref (ctx->loop);
	ctx->loop = NULL;

	if (ctx->cancel)
		return BRASERO_BURN_CANCEL;

	if (ctx->error) {
		g_propagate_error (error, ctx->error);
		ctx->error = NULL;
		return BRASERO_BURN_ERR;
	}

	*wrong_sums = ctx->wrong_sums;
	ctx->wrong_sums = NULL;

	return BRASERO_BURN_OK;
}

void
brasero_sum_check_cancel (BraseroSumCheckCtx *ctx)
{
	ctx->cancel = TRUE;
	if (ctx->md5_ctx)
		brasero_md5_cancel (ctx->md5_ctx);
}

BraseroSumCheckCtx *
brasero_sum_check_new (void)
{
	return g_new0 (BraseroSumCheckCtx, 1);
}

void
brasero_sum_check_free (BraseroSumCheckCtx *ctx)
{
	brasero_sum_check_reset (ctx);
	g_free (ctx);
}

void
brasero_sum_check_progress (BraseroSumCheckCtx *ctx,
			    gint *checked,
			    gint *total)
{
	if (checked)
		*checked = ctx->file_nb;

	if (total)
		*total = ctx->files_num;
}
