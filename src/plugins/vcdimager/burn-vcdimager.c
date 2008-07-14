/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2008 <bonfire-app@wanadoo.fr>
 * 
 * brasero is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * brasero is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
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
#include "burn-vcdimager.h"

BRASERO_PLUGIN_BOILERPLATE (BraseroVcdImager, brasero_vcd_imager, BRASERO_TYPE_PROCESS, BraseroProcess);

typedef struct _BraseroVcdImagerPrivate BraseroVcdImagerPrivate;
struct _BraseroVcdImagerPrivate
{
	guint svcd:1;
};

#define BRASERO_VCD_IMAGER_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_VCD_IMAGER, BraseroVcdImagerPrivate))

static BraseroProcessClass *parent_class = NULL;

static BraseroBurnResult
brasero_vcd_imager_read_stdout (BraseroProcess *process,
				const gchar *line)
{
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_vcd_imager_read_stderr (BraseroProcess *process,
				const gchar *line)
{
	if (!strstr (line, ""))
		return BRASERO_BURN_OK;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_vcd_imager_generate_xml_file (BraseroProcess *process,
				      const gchar *path,
				      GError **error)
{
	BraseroVcdImagerPrivate *priv;
	GSList *tracks = NULL;
	xmlTextWriter *xml;
	gchar buffer [64];
	gint success;
	GSList *iter;
	gchar *name;
	gint i;

	BRASERO_JOB_LOG (process, "Creating DVD layout xml file(%s)", path);

	xml = xmlNewTextWriterFilename (path, 0);
	if (!xml)
		return BRASERO_BURN_ERR;

	priv = BRASERO_VCD_IMAGER_PRIVATE (process);

	xmlTextWriterSetIndent (xml, 1);
	xmlTextWriterSetIndentString (xml, (xmlChar *) "\t");

	success = xmlTextWriterStartDocument (xml,
					      NULL,
					      "UTF8",
					      NULL);
	if (success < 0)
		goto error;

	success = xmlTextWriterWriteDTD (xml,
					(xmlChar *) "videocd",
					(xmlChar *) "-//GNU//DTD VideoCD//EN",
					(xmlChar *) "http://www.gnu.org/software/vcdimager/videocd.dtd",
					(xmlChar *) NULL);
	if (success < 0)
		goto error;

	/* let's start */
	success = xmlTextWriterStartElement (xml, (xmlChar *) "videocd");
	if (success < 0)
		goto error;

	success = xmlTextWriterWriteAttribute (xml,
					       (xmlChar *) "xmlns",
					       (xmlChar *) "http://www.gnu.org/software/vcdimager/1.0/");
	if (success < 0)
		goto error;

	if (priv->svcd)
		success = xmlTextWriterWriteAttribute (xml,
						       (xmlChar *) "class",
						       (xmlChar *) "svcd");
	else
		success = xmlTextWriterWriteAttribute (xml,
						       (xmlChar *) "class",
						       (xmlChar *) "vcd");

	if (success < 0)
		goto error;

	if (priv->svcd)
		success = xmlTextWriterWriteAttribute (xml,
						       (xmlChar *) "version",
						       (xmlChar *) "1.0");
	else
		success = xmlTextWriterWriteAttribute (xml,
						       (xmlChar *) "version",
						       (xmlChar *) "2.0");
	if (success < 0)
		goto error;

	/* info part */
	success = xmlTextWriterStartElement (xml, (xmlChar *) "info");
	if (success < 0)
		goto error;

	/* name of the volume */
	name = NULL;
	brasero_job_get_audio_title (BRASERO_JOB (process), &name);
	success = xmlTextWriterWriteElement (xml,
					     (xmlChar *) "album-id",
					     (xmlChar *) name);
	g_free (name);
	if (success < 0)
		goto error;

	/* number of CDs */
	success = xmlTextWriterWriteElement (xml,
					     (xmlChar *) "volume-count",
					     (xmlChar *) "1");
	if (success < 0)
		goto error;

	/* CD number */
	success = xmlTextWriterWriteElement (xml,
					     (xmlChar *) "volume-number",
					     (xmlChar *) "1");
	if (success < 0)
		goto error;

	/* close info part */
	success = xmlTextWriterEndElement (xml);
	if (success < 0)
		goto error;

	/* Primary Volume descriptor */
	success = xmlTextWriterStartElement (xml, (xmlChar *) "pvd");
	if (success < 0)
		goto error;

	name = NULL;
	brasero_job_get_audio_title (BRASERO_JOB (process), &name);
	success = xmlTextWriterWriteElement (xml,
					     (xmlChar *) "volume-id",
					     (xmlChar *) name);
	g_free (name);
	if (success < 0)
		goto error;

	/* Makes it CD-i compatible */
	success = xmlTextWriterWriteElement (xml,
					     (xmlChar *) "system-id",
					     (xmlChar *) "CD-RTOS CD-BRIDGE");
	if (success < 0)
		goto error;

	/* Close Primary Volume descriptor */
	success = xmlTextWriterEndElement (xml);
	if (success < 0)
		goto error;

	/* the tracks */
	success = xmlTextWriterStartElement (xml, (xmlChar *) "sequence-items");
	if (success < 0)
		goto error;

	/* get all tracks */
	brasero_job_get_tracks (BRASERO_JOB (process), &tracks);
	for (i = 0, iter = tracks; iter; iter = iter->next, i++) {
		BraseroTrack *track;
		gchar *video;

		track = iter->data;
		success = xmlTextWriterStartElement (xml, (xmlChar *) "sequence-item");
		if (success < 0)
			goto error;

		video = brasero_track_get_audio_source (track, FALSE);
		success = xmlTextWriterWriteAttribute (xml,
						       (xmlChar *) "src",
						       (xmlChar *) video);
		g_free (video);

		if (success < 0)
			goto error;

		sprintf (buffer, "track-%i", i);
		success = xmlTextWriterWriteAttribute (xml,
						       (xmlChar *) "id",
						       (xmlChar *) buffer);
		if (success < 0)
			goto error;

		/* close sequence-item */
		success = xmlTextWriterEndElement (xml);
		if (success < 0)
			goto error;
	}

	/* sequence-items */
	success = xmlTextWriterEndElement (xml);
	if (success < 0)
		goto error;

	/* the navigation */
	success = xmlTextWriterStartElement (xml, (xmlChar *) "pbc");
	if (success < 0)
		goto error;

	/* get all tracks */
	brasero_job_get_tracks (BRASERO_JOB (process), &tracks);
	for (i = 0, iter = tracks; iter; iter = iter->next, i++) {
		BraseroTrack *track;

		track = iter->data;

		sprintf (buffer, "playlist-%i", i);
		success = xmlTextWriterStartElement (xml, (xmlChar *) "playlist");
		if (success < 0)
			goto error;

		success = xmlTextWriterWriteAttribute (xml,
						       (xmlChar *) "id",
						       (xmlChar *) buffer);
		if (success < 0)
			goto error;

		success = xmlTextWriterWriteElement (xml,
						     (xmlChar *) "wait",
						     (xmlChar *) "0");
		if (success < 0)
			goto error;

		success = xmlTextWriterStartElement (xml, (xmlChar *) "play-item");
		if (success < 0)
			goto error;

		sprintf (buffer, "track-%i", i);
		success = xmlTextWriterWriteAttribute (xml,
						       (xmlChar *) "ref",
						       (xmlChar *) buffer);
		if (success < 0)
			goto error;

		/* play-item */
		success = xmlTextWriterEndElement (xml);
		if (success < 0)
			goto error;

		/* playlist */
		success = xmlTextWriterEndElement (xml);
		if (success < 0)
			goto error;
	}

	/* pbc */
	success = xmlTextWriterEndElement (xml);
	if (success < 0)
		goto error;

	/* close videocd */
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
brasero_vcd_imager_set_argv (BraseroProcess *process,
			     GPtrArray *argv,
			     GError **error)
{
	BraseroVcdImagerPrivate *priv;
	BraseroBurnResult result;
	BraseroJobAction action;
	BraseroMedia medium;
	gchar *output;
	gchar *image;
	gchar *toc;

	priv = BRASERO_VCD_IMAGER_PRIVATE (process);

	brasero_job_get_action (BRASERO_JOB (process), &action);
	if (action != BRASERO_JOB_ACTION_IMAGE)
		BRASERO_JOB_NOT_SUPPORTED (process);

	g_ptr_array_add (argv, g_strdup ("vcdxbuild"));

	g_ptr_array_add (argv, g_strdup ("--progress"));
	g_ptr_array_add (argv, g_strdup ("-v"));

	/* specifies output */
	image = toc = NULL;
	brasero_job_get_image_output (BRASERO_JOB (process),
				      &image,
				      &toc);

	g_ptr_array_add (argv, g_strdup ("-c"));
	g_ptr_array_add (argv, toc);
	g_ptr_array_add (argv, g_strdup ("-b"));
	g_ptr_array_add (argv, image);

	/* get temporary file to write XML */
	result = brasero_job_get_tmp_file (BRASERO_JOB (process),
					   NULL,
					   &output,
					   error);
	if (result != BRASERO_BURN_OK)
		return result;

	g_ptr_array_add (argv, output);

	brasero_job_get_media (BRASERO_JOB (process), &medium);
	if (medium & BRASERO_MEDIUM_CD) {
		GValue *value = NULL;

		brasero_job_tag_lookup (BRASERO_JOB (process),
					BRASERO_VCD_TYPE,
					&value);
		if (value)
			priv->svcd = (g_value_get_int (value) == BRASERO_SVCD);
	}

	result = brasero_vcd_imager_generate_xml_file (process, output, error);
	if (result != BRASERO_BURN_OK)
		return result;
	
	brasero_job_set_current_action (BRASERO_JOB (process),
					BRASERO_BURN_ACTION_CREATING_IMAGE,
					_("Creating file layout"),
					FALSE);
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_vcd_imager_post (BraseroJob *job)
{
	BraseroVcdImagerPrivate *priv;

	priv = BRASERO_VCD_IMAGER_PRIVATE (job);
	return brasero_job_finished_session (job);
}

static void
brasero_vcd_imager_init (BraseroVcdImager *object)
{}

static void
brasero_vcd_imager_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_vcd_imager_class_init (BraseroVcdImagerClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	BraseroProcessClass* process_class = BRASERO_PROCESS_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroVcdImagerPrivate));

	object_class->finalize = brasero_vcd_imager_finalize;
	process_class->stdout_func = brasero_vcd_imager_read_stdout;
	process_class->stderr_func = brasero_vcd_imager_read_stderr;
	process_class->set_argv = brasero_vcd_imager_set_argv;
	process_class->post = brasero_vcd_imager_post;
}

static BraseroBurnResult
brasero_vcd_imager_export_caps (BraseroPlugin *plugin, gchar **error)
{
	BraseroBurnResult result;
	GSList *output;
	GSList *input;

	/* NOTE: it seems that cdrecord can burn cue files on the fly */
	brasero_plugin_define (plugin,
			       "vcdimager",
			       _("use vcdimager to convert a set of files to burn to SVCDs"),
			       "Philippe Rouquier",
			       1);

	/* First see if this plugin can be used */
	result = brasero_process_check_path ("vcdimager", error);
	if (result != BRASERO_BURN_OK)
		return result;

	input = brasero_caps_audio_new (BRASERO_PLUGIN_IO_ACCEPT_FILE,
					BRASERO_AUDIO_FORMAT_MP2|
					BRASERO_AUDIO_FORMAT_44100|
					BRASERO_VIDEO_FORMAT_VCD);
	output = brasero_caps_image_new (BRASERO_PLUGIN_IO_ACCEPT_FILE,
					 BRASERO_IMAGE_FORMAT_CUE);

	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (output);
	g_slist_free (input);

	/* we only support CDs they must be blank */
	brasero_plugin_set_flags (plugin,
				  BRASERO_MEDIUM_CDRW|
				  BRASERO_MEDIUM_BLANK|
				  BRASERO_MEDIUM_CLOSED|
				  BRASERO_MEDIUM_APPENDABLE|
				  BRASERO_MEDIUM_HAS_DATA|
				  BRASERO_MEDIUM_HAS_AUDIO,
				  BRASERO_BURN_FLAG_NONE,
				  BRASERO_BURN_FLAG_NONE);

	brasero_plugin_set_flags (plugin,
				  BRASERO_MEDIUM_FILE|
				  BRASERO_MEDIUM_CDR|
				  BRASERO_MEDIUM_BLANK|
				  BRASERO_MEDIUM_APPENDABLE|
				  BRASERO_MEDIUM_HAS_DATA|
				  BRASERO_MEDIUM_HAS_AUDIO,
				  BRASERO_BURN_FLAG_NONE,
				  BRASERO_BURN_FLAG_NONE);
	return BRASERO_BURN_OK;
}

