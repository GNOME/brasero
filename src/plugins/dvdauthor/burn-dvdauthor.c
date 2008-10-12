/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * burn-dvdauthor.c
 * Copyright (C) Philippe Rouquier 2005-2008 <bonfire-app@wanadoo.fr>
 * 
 * burn-dvdauthor.c is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * burn-dvdauthor.c is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with burn-dvdauthor.c.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
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

#include <libxml/xmlerror.h>
#include <libxml/xmlwriter.h>
#include <libxml/parser.h>
#include <libxml/xmlstring.h>
#include <libxml/uri.h>

#include "burn-basics.h"
#include "burn-plugin.h"
#include "burn-job.h"
#include "burn-process.h"
#include "burn-dvdauthor.h"

BRASERO_PLUGIN_BOILERPLATE (BraseroDvdAuthor, brasero_dvd_author, BRASERO_TYPE_PROCESS, BraseroProcess);

typedef struct _BraseroDvdAuthorPrivate BraseroDvdAuthorPrivate;
struct _BraseroDvdAuthorPrivate
{
	gchar *output;
};

#define BRASERO_DVD_AUTHOR_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_DVD_AUTHOR, BraseroDvdAuthorPrivate))

static BraseroProcessClass *parent_class = NULL;

static BraseroBurnResult
brasero_dvd_author_add_track (BraseroJob *job)
{
	gchar *path;
	BraseroTrack *track;
	GSList *grafts = NULL;
	BraseroGraftPt *graft;
	BraseroDvdAuthorPrivate *priv;

	priv = BRASERO_DVD_AUTHOR_PRIVATE (job);

	/* create the track */
	track = brasero_track_new (BRASERO_TRACK_TYPE_DATA);

	/* audio */
	graft = g_new (BraseroGraftPt, 1);
	path = g_build_path (G_DIR_SEPARATOR_S,
			     priv->output,
			     "AUDIO_TS",
			     NULL);
	graft->uri = g_filename_to_uri (path, NULL, NULL);
	g_free (path);

	graft->path = g_strdup ("/AUDIO_TS");
	grafts = g_slist_prepend (grafts, graft);

	BRASERO_JOB_LOG (job, "Adding graft point for %s", graft->uri);

	/* video */
	graft = g_new (BraseroGraftPt, 1);
	path = g_build_path (G_DIR_SEPARATOR_S,
			     priv->output,
			     "VIDEO_TS",
			     NULL);
	graft->uri = g_filename_to_uri (path, NULL, NULL);
	g_free (path);

	graft->path = g_strdup ("/VIDEO_TS");
	grafts = g_slist_prepend (grafts, graft);

	BRASERO_JOB_LOG (job, "Adding graft point for %s", graft->uri);

	brasero_track_add_data_fs (track,
				   BRASERO_IMAGE_FS_ISO|
				   BRASERO_IMAGE_FS_UDF|
				   BRASERO_IMAGE_FS_VIDEO);
	brasero_track_set_data_source (track,
				       grafts,
				       NULL);
	brasero_job_add_track (job, track);
	brasero_track_unref (track);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_dvd_author_read_stdout (BraseroProcess *process,
				const gchar *line)
{
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_dvd_author_read_stderr (BraseroProcess *process,
				const gchar *line)
{
	gint percent = 0;

	if (sscanf (line, "STAT: fixing VOBU at %*s (%*d/%*d, %d%%)", &percent) == 1) {
		brasero_job_start_progress (BRASERO_JOB (process), FALSE);
		brasero_job_set_progress (BRASERO_JOB (process),
					  (gdouble) ((gdouble) percent) / 100.0);
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_dvd_author_generate_xml_file (BraseroProcess *process,
				      const gchar *path,
				      GError **error)
{
	BraseroDvdAuthorPrivate *priv;
	BraseroBurnResult result;
	GSList *tracks = NULL;
	xmlTextWriter *xml;
	gint success;
	GSList *iter;

	BRASERO_JOB_LOG (process, "Creating DVD layout xml file(%s)", path);

	xml = xmlNewTextWriterFilename (path, 0);
	if (!xml)
		return BRASERO_BURN_ERR;

	priv = BRASERO_DVD_AUTHOR_PRIVATE (process);

	xmlTextWriterSetIndent (xml, 1);
	xmlTextWriterSetIndentString (xml, (xmlChar *) "\t");

	success = xmlTextWriterStartDocument (xml,
					      NULL,
					      "UTF8",
					      NULL);
	if (success < 0)
		goto error;

	result = brasero_job_get_tmp_dir (BRASERO_JOB (process),
					  &priv->output,
					  error);
	if (result != BRASERO_BURN_OK)
		return result;

	/* let's start */
	success = xmlTextWriterStartElement (xml, (xmlChar *) "dvdauthor");
	if (success < 0)
		goto error;

	success = xmlTextWriterWriteAttribute (xml,
					       (xmlChar *) "dest",
					       (xmlChar *) priv->output);
	if (success < 0)
		goto error;

	/* This is needed to finalize */
	success = xmlTextWriterWriteElement (xml, (xmlChar *) "vmgm", (xmlChar *) "");
	if (success < 0)
		goto error;

	/* the tracks */
	success = xmlTextWriterStartElement (xml, (xmlChar *) "titleset");
	if (success < 0)
		goto error;

	success = xmlTextWriterStartElement (xml, (xmlChar *) "titles");
	if (success < 0)
		goto error;

	/* get all tracks */
	brasero_job_get_tracks (BRASERO_JOB (process), &tracks);
	for (iter = tracks; iter; iter = iter->next) {
		BraseroTrack *track;
		gchar *video;

		track = iter->data;
		success = xmlTextWriterStartElement (xml, (xmlChar *) "pgc");
		if (success < 0)
			goto error;

		success = xmlTextWriterStartElement (xml, (xmlChar *) "vob");
		if (success < 0)
			goto error;

		video = brasero_track_get_audio_source (track, FALSE);
		success = xmlTextWriterWriteAttribute (xml,
						       (xmlChar *) "file",
						       (xmlChar *) video);
		g_free (video);

		if (success < 0)
			goto error;

		/* vob */
		success = xmlTextWriterEndElement (xml);
		if (success < 0)
			goto error;

		/* pgc */
		success = xmlTextWriterEndElement (xml);
		if (success < 0)
			goto error;
	}

	/* titles */
	success = xmlTextWriterEndElement (xml);
	if (success < 0)
		goto error;

	/* titleset */
	success = xmlTextWriterEndElement (xml);
	if (success < 0)
		goto error;

	/* close dvdauthor */
	success = xmlTextWriterEndElement (xml);
	if (success < 0)
		goto error;

	xmlTextWriterEndDocument (xml);
	xmlFreeTextWriter (xml);

	return BRASERO_BURN_OK;

error:

	BRASERO_JOB_LOG (process, "Error");

	/* close everything */
	xmlTextWriterEndDocument (xml);
	xmlFreeTextWriter (xml);

	/* FIXME: get the error */

	return BRASERO_BURN_ERR;
}

static BraseroBurnResult
brasero_dvd_author_set_argv (BraseroProcess *process,
			     GPtrArray *argv,
			     GError **error)
{
	BraseroDvdAuthorPrivate *priv;
	BraseroBurnResult result;
	BraseroJobAction action;
	gchar *output;

	priv = BRASERO_DVD_AUTHOR_PRIVATE (process);

	brasero_job_get_action (BRASERO_JOB (process), &action);
	if (action != BRASERO_JOB_ACTION_IMAGE)
		BRASERO_JOB_NOT_SUPPORTED (process);

	g_ptr_array_add (argv, g_strdup ("dvdauthor"));
	
	/* get all arguments to write XML file */
	result = brasero_job_get_tmp_file (BRASERO_JOB (process),
					   NULL,
					   &output,
					   error);
	if (result != BRASERO_BURN_OK)
		return result;

	g_ptr_array_add (argv, g_strdup ("-x"));
	g_ptr_array_add (argv, output);

	result = brasero_dvd_author_generate_xml_file (process, output, error);
	if (result != BRASERO_BURN_OK)
		return result;

	brasero_job_set_current_action (BRASERO_JOB (process),
					BRASERO_BURN_ACTION_CREATING_IMAGE,
					_("Creating file layout"),
					FALSE);
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_dvd_author_post (BraseroJob *job)
{
	BraseroDvdAuthorPrivate *priv;

	priv = BRASERO_DVD_AUTHOR_PRIVATE (job);

	brasero_dvd_author_add_track (job);

	if (priv->output) {
		g_free (priv->output);
		priv->output = NULL;
	}

	return brasero_job_finished_session (job);
}

static void
brasero_dvd_author_init (BraseroDvdAuthor *object)
{}

static void
brasero_dvd_author_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_dvd_author_class_init (BraseroDvdAuthorClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	BraseroProcessClass* process_class = BRASERO_PROCESS_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroDvdAuthorPrivate));

	object_class->finalize = brasero_dvd_author_finalize;

	process_class->stdout_func = brasero_dvd_author_read_stdout;
	process_class->stderr_func = brasero_dvd_author_read_stderr;
	process_class->set_argv = brasero_dvd_author_set_argv;
	process_class->post = brasero_dvd_author_post;
}

static BraseroBurnResult
brasero_dvd_author_export_caps (BraseroPlugin *plugin, gchar **error)
{
	BraseroBurnResult result;
	GSList *output;
	GSList *input;

	/* NOTE: it seems that cdrecord can burn cue files on the fly */
	brasero_plugin_define (plugin,
			       "dvdauthor",
			       _("use dvdauthor to convert a set of files to burn to Video DVDs"),
			       "Philippe Rouquier",
			       1);

	/* First see if this plugin can be used */
	result = brasero_process_check_path ("dvdauthor", error);
	if (result != BRASERO_BURN_OK)
		return result;

	input = brasero_caps_audio_new (BRASERO_PLUGIN_IO_ACCEPT_FILE,
					BRASERO_AUDIO_FORMAT_AC3|
					BRASERO_AUDIO_FORMAT_MP2|
					BRASERO_AUDIO_FORMAT_RAW|
					BRASERO_AUDIO_FORMAT_44100|
					BRASERO_AUDIO_FORMAT_48000|
					BRASERO_VIDEO_FORMAT_VIDEO_DVD);
	output = brasero_caps_data_new (BRASERO_IMAGE_FS_ISO|
					BRASERO_IMAGE_FS_UDF|
					BRASERO_IMAGE_FS_VIDEO);

	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (output);
	g_slist_free (input);

	/* we only support DVDs */
	brasero_plugin_set_flags (plugin,
  				  BRASERO_MEDIUM_FILE|
				  BRASERO_MEDIUM_DVDR|
				  BRASERO_MEDIUM_DVDR_PLUS|
				  BRASERO_MEDIUM_DUAL_L|
				  BRASERO_MEDIUM_BLANK|
				  BRASERO_MEDIUM_APPENDABLE|
				  BRASERO_MEDIUM_HAS_DATA,
				  BRASERO_BURN_FLAG_NONE,
				  BRASERO_BURN_FLAG_NONE);

	brasero_plugin_set_flags (plugin,
				  BRASERO_MEDIUM_DVDRW|
				  BRASERO_MEDIUM_DVDRW_PLUS|
				  BRASERO_MEDIUM_DVDRW_RESTRICTED|
				  BRASERO_MEDIUM_DUAL_L|
				  BRASERO_MEDIUM_BLANK|
				  BRASERO_MEDIUM_CLOSED|
				  BRASERO_MEDIUM_APPENDABLE|
				  BRASERO_MEDIUM_HAS_DATA,
				  BRASERO_BURN_FLAG_NONE,
				  BRASERO_BURN_FLAG_NONE);

	return BRASERO_BURN_OK;
}
