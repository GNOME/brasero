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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gmodule.h>

#include "brasero-plugin-registration.h"
#include "burn-job.h"
#include "burn-process.h"
#include "burn-cdrdao-common.h"
#include "brasero-track-image.h"


#define BRASERO_TYPE_TOC2CUE         (brasero_toc2cue_get_type ())
#define BRASERO_TOC2CUE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_TOC2CUE, BraseroToc2Cue))
#define BRASERO_TOC2CUE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_TOC2CUE, BraseroToc2CueClass))
#define BRASERO_IS_TOC2CUE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_TOC2CUE))
#define BRASERO_IS_TOC2CUE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_TOC2CUE))
#define BRASERO_TOC2CUE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_TOC2CUE, BraseroToc2CueClass))

BRASERO_PLUGIN_BOILERPLATE (BraseroToc2Cue, brasero_toc2cue, BRASERO_TYPE_PROCESS, BraseroProcess);

struct _BraseroToc2CuePrivate {
	gchar *output;
};
typedef struct _BraseroToc2CuePrivate BraseroToc2CuePrivate;

#define BRASERO_TOC2CUE_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_TOC2CUE, BraseroToc2CuePrivate))

static BraseroProcessClass *parent_class = NULL;

static BraseroBurnResult
brasero_toc2cue_read_stdout (BraseroProcess *process,
			     const gchar *line)
{
	BraseroToc2Cue *self;

	self = BRASERO_TOC2CUE (process);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_toc2cue_read_stderr (BraseroProcess *process,
			     const gchar *line)
{
	BraseroTrack *current = NULL;
	BraseroToc2CuePrivate *priv;
	BraseroTrackImage *track;
	BraseroToc2Cue *self;
	GError *error = NULL;
	gchar *tmp_img_path;
	GIOChannel *source;
	guint tmp_path_len;
	goffset blocks = 0;
	GIOStatus status;
	gchar *img_path;
	gchar *toc_path;
	gchar *buffer;
	FILE *output;

	self = BRASERO_TOC2CUE (process);
	priv = BRASERO_TOC2CUE_PRIVATE (self);

	if (!strstr (line, "Converted toc-file"))
		return BRASERO_BURN_OK;

	/* Now we also need to replace all the occurences of tmp file name by
	 * the real output file name in the created cue */
	source = g_io_channel_new_file (priv->output, "r", &error);
	if (!source) {
		brasero_job_error (BRASERO_JOB (process), error);
		return BRASERO_BURN_OK;
	}

	brasero_job_get_image_output (BRASERO_JOB (self),
				      &img_path,
				      &toc_path);
	
	output = fopen (toc_path, "w");
	if (!output) {
                int errsv = errno;

		g_io_channel_unref (source);

		g_free (img_path);
		g_free (toc_path);

		brasero_job_error (BRASERO_JOB (process),
				   g_error_new_literal (BRASERO_BURN_ERROR,
							BRASERO_BURN_ERROR_GENERAL,
							g_strerror (errsv)));
		return BRASERO_BURN_OK;
	}

	/* get the path of the image that should remain unchanged */
	brasero_job_get_current_track (BRASERO_JOB (self), &current);
	tmp_img_path = brasero_track_image_get_source (BRASERO_TRACK_IMAGE (current), FALSE);
	tmp_path_len = strlen (tmp_img_path);

	status = g_io_channel_read_line (source, &buffer, NULL, NULL, &error);
	while (status == G_IO_STATUS_NORMAL) {
		gchar *location;

		location = strstr (buffer, tmp_img_path);
		if (location) {
			gchar *tmp;

			tmp = buffer;
			buffer = g_strdup_printf ("%.*s%s%s",
						  (int) (location - buffer),
						  buffer,
						  img_path,
						  location + tmp_path_len);
			g_free (tmp);
		}

		if (!fwrite (buffer, strlen (buffer), 1, output)) {
                        int errsv = errno;

			g_free (buffer);

			fclose (output);
			g_io_channel_unref (source);

			g_free (tmp_img_path);
			
			g_free (img_path);
			g_free (toc_path);

			brasero_job_error (BRASERO_JOB (process),
					   g_error_new_literal (BRASERO_BURN_ERROR,
								BRASERO_BURN_ERROR_GENERAL,
								g_strerror (errsv)));
			return BRASERO_BURN_OK;
		}

		g_free (buffer);
		status = g_io_channel_read_line (source, &buffer, NULL, NULL, &error);
	}

	fclose (output);
	g_io_channel_unref (source);

	if (status == G_IO_STATUS_ERROR) {
		g_free (tmp_img_path);
		g_free (img_path);
		g_free (toc_path);
		brasero_job_error (BRASERO_JOB (process), error);
		return BRASERO_BURN_OK;
	}

	/* the previous track image path will now be a link pointing to the
	 * image path of the new track just created */
	if (g_rename (tmp_img_path, img_path)) {
                int errsv = errno;

		brasero_job_error (BRASERO_JOB (self),
				   g_error_new_literal (BRASERO_BURN_ERROR,
							BRASERO_BURN_ERROR_GENERAL,
							g_strerror (errsv)));
		return BRASERO_BURN_OK;
	}

	if (link (img_path, tmp_img_path)) {
                int errsv = errno;

		brasero_job_error (BRASERO_JOB (self),
				   g_error_new_literal (BRASERO_BURN_ERROR,
							BRASERO_BURN_ERROR_GENERAL,
							g_strerror (errsv)));
		return BRASERO_BURN_OK;
	} /* symlink () could also be used */

	track = brasero_track_image_new ();
	brasero_track_image_set_source (track,
					img_path,
					toc_path,
					BRASERO_IMAGE_FORMAT_CUE);

	g_free (tmp_img_path);
	g_free (img_path);
	g_free (toc_path);

	brasero_job_get_session_output_size (BRASERO_JOB (self), &blocks, NULL);
	brasero_track_image_set_block_num (track, blocks);

	brasero_job_add_track (BRASERO_JOB (process), BRASERO_TRACK (track));

	/* It's good practice to unref the track afterwards as we don't need it
	 * anymore. BraseroTaskCtx refs it. */
	g_object_unref (track);

	brasero_job_finished_track (BRASERO_JOB (process));
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_toc2cue_set_argv (BraseroProcess *process,
			  GPtrArray *argv,
			  GError **error)
{
	BraseroToc2CuePrivate *priv;
	BraseroBurnResult result;
	BraseroJobAction action;
	BraseroToc2Cue *self;
	BraseroTrack *track;
	gchar *tocpath;
	gchar *output;

	self = BRASERO_TOC2CUE (process);
	priv = BRASERO_TOC2CUE_PRIVATE (self);

	brasero_job_get_action (BRASERO_JOB (self), &action);
	if (action != BRASERO_JOB_ACTION_IMAGE)
		BRASERO_JOB_NOT_SUPPORTED (process);

	result = brasero_job_get_tmp_file (BRASERO_JOB (process),
					   NULL,
					   &output,
					   error);
	if (result != BRASERO_BURN_OK)
		return result;

	brasero_job_get_current_track (BRASERO_JOB (self), &track);
	tocpath = brasero_track_image_get_toc_source (BRASERO_TRACK_IMAGE (track), FALSE);

	priv->output = g_strdup (output);
	g_remove (priv->output);

	g_ptr_array_add (argv, g_strdup ("toc2cue"));
	g_ptr_array_add (argv, tocpath);
	g_ptr_array_add (argv, output);

	brasero_job_set_current_action (BRASERO_JOB (self),
					BRASERO_BURN_ACTION_CREATING_IMAGE,
					_("Converting toc file"),
					FALSE);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_toc2cue_post (BraseroJob *job)
{
	BraseroToc2CuePrivate *priv;

	priv = BRASERO_TOC2CUE_PRIVATE (job);
	if (priv->output) {
		g_free (priv->output);
		priv->output = NULL;
	}

	return brasero_job_finished_session (job);
}

static void
brasero_toc2cue_class_init (BraseroToc2CueClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroProcessClass *process_class = BRASERO_PROCESS_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroToc2CuePrivate));

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_toc2cue_finalize;

	process_class->stdout_func = brasero_toc2cue_read_stdout;
	process_class->stderr_func = brasero_toc2cue_read_stderr;
	process_class->set_argv = brasero_toc2cue_set_argv;
	process_class->post = brasero_toc2cue_post;
}

static void
brasero_toc2cue_init (BraseroToc2Cue *obj)
{ }

static void
brasero_toc2cue_finalize (GObject *object)
{
	brasero_toc2cue_post (BRASERO_JOB (object));
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_toc2cue_export_caps (BraseroPlugin *plugin)
{
	GSList *output;
	GSList *input;

	brasero_plugin_define (plugin,
			       "toc2cue",
			       _("Converts .toc files into .cue files"),
			       "Philippe Rouquier",
			       0);

	input = brasero_caps_image_new (BRASERO_PLUGIN_IO_ACCEPT_FILE,
					BRASERO_IMAGE_FORMAT_CDRDAO);

	output = brasero_caps_image_new (BRASERO_PLUGIN_IO_ACCEPT_FILE,
					 BRASERO_IMAGE_FORMAT_CUE);

	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (output);
	g_slist_free (input);

	brasero_plugin_register_group (plugin, _(CDRDAO_DESCRIPTION));
}

G_MODULE_EXPORT void
brasero_plugin_check_config (BraseroPlugin *plugin)
{
	brasero_plugin_test_app (plugin, "toc2cue");
}
