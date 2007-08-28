/***************************************************************************
 *            burn-toc2cue.c
 *
 *  mar oct  3 18:30:51 2006
 *  Copyright  2006  Philippe Rouquier
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

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gmodule.h>

#include "burn-basics.h"
#include "burn-plugin.h"
#include "burn-job.h"
#include "burn-process.h"
#include "burn-toc2cue.h"
 
BRASERO_PLUGIN_BOILERPLATE (BraseroToc2Cue, brasero_toc2cue, BRASERO_TYPE_PROCESS, BraseroProcess);

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
	gchar *image = NULL, *toc = NULL;
	BraseroToc2Cue *self;
	BraseroTrack *track;
	gchar *img_path;

	self = BRASERO_TOC2CUE (process);

	if (!strstr (line, "Converted toc-file"))
		return BRASERO_BURN_OK;

	brasero_job_get_image_output (BRASERO_JOB (self),
				      &image,
				      &toc);

	/* get the path of the image that should remain unchanged */
	brasero_job_get_current_track (BRASERO_JOB (self), &track);
	img_path = brasero_track_get_image_source (track, FALSE);

	/* the previous track image path will now be a link pointing to the
	 * image path of the new track just created */
	g_rename (img_path, image);
	link (img_path, image); /* symlink () could also be used */

	track = brasero_track_new (BRASERO_TRACK_TYPE_IMAGE);
	brasero_track_set_image_source (track,
					image,
					toc,
					BRASERO_IMAGE_FORMAT_CUE);

	g_free (image);
	g_free (toc);

	brasero_job_add_track (BRASERO_JOB (process), track);
	brasero_job_finished_track (BRASERO_JOB (process));
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_toc2cue_set_argv (BraseroProcess *process,
			  GPtrArray *argv,
			  GError **error)
{
	BraseroJobAction action;
	BraseroToc2Cue *self;
	BraseroTrack *track;
	gchar *tocpath;
	gchar *output;

	self = BRASERO_TOC2CUE (process);

	brasero_job_get_action (BRASERO_JOB (self), &action);
	if (action == BRASERO_JOB_ACTION_SIZE)
		return BRASERO_BURN_NOT_RUNNING;

	brasero_job_get_image_output (BRASERO_JOB (self),
				      NULL,
				      &output);

	brasero_job_get_current_track (BRASERO_JOB (self), &track);
	tocpath = brasero_track_get_toc_source (track, TRUE);

	g_ptr_array_add (argv, g_strdup ("toc2cue"));
	g_ptr_array_add (argv, tocpath);
	g_ptr_array_add (argv, output);

	brasero_job_set_current_action (BRASERO_JOB (self),
					BRASERO_BURN_ACTION_CREATING_IMAGE,
					_("Converting toc file"),
					FALSE);

	return BRASERO_BURN_OK;
}

static void
brasero_toc2cue_class_init (BraseroToc2CueClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroProcessClass *process_class = BRASERO_PROCESS_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_toc2cue_finalize;

	process_class->stdout_func = brasero_toc2cue_read_stdout;
	process_class->stderr_func = brasero_toc2cue_read_stderr;
	process_class->set_argv = brasero_toc2cue_set_argv;
}

static void
brasero_toc2cue_init (BraseroToc2Cue *obj)
{ }

static void
brasero_toc2cue_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static BraseroBurnResult
brasero_toc2cue_export_caps (BraseroPlugin *plugin, gchar **error)
{
	gchar *prog_name;
	GSList *output;
	GSList *input;

	brasero_plugin_define (plugin,
			       "toc2cue",
			       _("toc2cue converts .toc files into .cue files"),
			       "Philippe Rouquier",
			       0);

	/* First see if this plugin can be used, i.e. if readcd is in
	 * the path */
	prog_name = g_find_program_in_path ("toc2cue");
	if (!prog_name) {
		*error = g_strdup (_("toc2cue could not be found in the path"));
		return BRASERO_BURN_ERR;
	}
	g_free (prog_name);

	input = brasero_caps_image_new (BRASERO_PLUGIN_IO_ACCEPT_FILE,
					BRASERO_IMAGE_FORMAT_CDRDAO);

	output = brasero_caps_image_new (BRASERO_PLUGIN_IO_ACCEPT_FILE,
					 BRASERO_IMAGE_FORMAT_CUE);

	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (output);
	g_slist_free (input);

	return BRASERO_BURN_OK;
}
