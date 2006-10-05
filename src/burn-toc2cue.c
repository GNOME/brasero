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

#include "burn-job.h"
#include "burn-process.h"
#include "burn-imager.h"
#include "burn-toc2cue.h"
#include "burn-common.h"
 
static void brasero_toc2cue_class_init (BraseroToc2CueClass *klass);
static void brasero_toc2cue_init (BraseroToc2Cue *sp);
static void brasero_toc2cue_finalize (GObject *object);

static void brasero_toc2cue_iface_init_image (BraseroImagerIFace *iface);

struct _BraseroToc2CuePrivate {
	BraseroTrackSource *source;

	gchar *output;

	gint overwrite:1;
	gint success:1;
	gint clean:1;
};

static BraseroProcessClass *parent_class = NULL;

GType
brasero_toc2cue_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroToc2CueClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_toc2cue_class_init,
			NULL,
			NULL,
			sizeof (BraseroToc2Cue),
			0,
			(GInstanceInitFunc)brasero_toc2cue_init,
		};

		static const GInterfaceInfo imager_info =
		{
			(GInterfaceInitFunc) brasero_toc2cue_iface_init_image,
			NULL,
			NULL
		};

		type = g_type_register_static (BRASERO_TYPE_PROCESS,
					       "BraseroToc2Cue",
					       &our_info,
					       0);
		g_type_add_interface_static (type,
					     BRASERO_TYPE_IMAGER,
					     &imager_info);
	}

	return type;
}

static void
brasero_toc2cue_init (BraseroToc2Cue *obj)
{
	obj->priv = g_new0 (BraseroToc2CuePrivate, 1);
}

static void
brasero_toc2cue_finalize (GObject *object)
{
	BraseroToc2Cue *cobj;

	cobj = BRASERO_TOC2CUE (object);

	if (cobj->priv->source) {
		brasero_track_source_free (cobj->priv->source);
		cobj->priv->source = NULL;
	}

	if (cobj->priv->output) {
		if (cobj->priv->clean)
			g_remove (cobj->priv->output);

		g_free (cobj->priv->output);
		cobj->priv->output = NULL;
	}

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

BraseroToc2Cue *
brasero_toc2cue_new ()
{
	BraseroToc2Cue *obj;
	
	obj = BRASERO_TOC2CUE (g_object_new (BRASERO_TYPE_TOC2CUE, NULL));
	
	return obj;
}

static BraseroBurnResult
brasero_toc2cue_set_source (BraseroJob *job,
			    const BraseroTrackSource *source,
			    GError **error)
{
	BraseroToc2Cue *self;

	self = BRASERO_TOC2CUE (job);

	if (self->priv->output && self->priv->clean)
		g_remove (self->priv->output);

	if (self->priv->source) {
		brasero_track_source_free (self->priv->source);
		self->priv->source = NULL;
	}

	if (source->type != BRASERO_TRACK_SOURCE_IMAGE
	&& !(source->format & BRASERO_IMAGE_FORMAT_CDRDAO))
		BRASERO_JOB_NOT_SUPPORTED (job);

	self->priv->source = brasero_track_source_copy (source);

	return BRASERO_BURN_OK;
}

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
	BraseroToc2Cue *self;

	self = BRASERO_TOC2CUE (process);

	if (strstr (line, "Converted toc-file"))
		self->priv->success = 1;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_toc2cue_set_argv (BraseroProcess *process,
			  GPtrArray *argv,
			  gboolean has_master,
			  GError **error)
{
	BraseroBurnResult result;
	BraseroToc2Cue *self;
	gchar *tocpath;
	gchar *output;

	self = BRASERO_TOC2CUE (process);
	if (!self->priv->source)
		BRASERO_JOB_NOT_READY (process);

	output = g_strdup (self->priv->output);
	result = brasero_burn_common_check_output (&output,
						   BRASERO_IMAGE_FORMAT_CDRDAO,
						   FALSE,
						   self->priv->overwrite,
						   NULL,
						   error);
	if (result != BRASERO_BURN_OK) {
		g_free (output);
		return result;
	}

	BRASERO_JOB_TASK_SET_ACTION (self,
				     BRASERO_BURN_ACTION_CREATING_IMAGE,
				     _("Converting toc file"),
				     FALSE);
	tocpath = brasero_track_source_get_cdrdao_localpath (self->priv->source);

	g_ptr_array_add (argv, g_strdup ("toc2cue"));
	g_ptr_array_add (argv, tocpath);
	g_ptr_array_add (argv, output);

	self->priv->success = 0;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_toc2cue_get_track (BraseroImager *imager,
			   BraseroTrackSource **source,
			   GError **error)
{
	BraseroToc2Cue *self;
	BraseroBurnResult result;
	BraseroTrackSource *track;

	self = BRASERO_TOC2CUE (imager);

	result = brasero_job_run (BRASERO_JOB (self), error);
	if (result != BRASERO_BURN_OK)
		return result;

	if (!self->priv->success) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the conversion of the toc file failed"));
		return BRASERO_BURN_ERR;
	}

	track = g_new0 (BraseroTrackSource, 1);
	track->type = BRASERO_TRACK_SOURCE_IMAGE;
	track->format = BRASERO_IMAGE_FORMAT_CUE;
	track->contents.image.toc = gnome_vfs_get_uri_from_local_path (self->priv->output);
	track->contents.image.image = g_strdup (self->priv->source->contents.image.image);

	*source = track;
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_toc2cue_set_output (BraseroImager *imager,
			    const char *output,
			    gboolean overwrite,
			    gboolean clean,
			    GError **error)
{
	BraseroToc2Cue *self;

	self = BRASERO_TOC2CUE (imager);

	if (self->priv->output) {
		if (self->priv->clean)
			g_remove (self->priv->output);

		g_free (self->priv->output);
		self->priv->output = NULL;
	}

	self->priv->output = g_strdup (output);
	self->priv->overwrite = overwrite;
	self->priv->clean = clean;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_toc2cue_set_output_type (BraseroImager *imager,
				 BraseroTrackSourceType type,
				 BraseroImageFormat format,
				 GError **error)
{
	BraseroToc2Cue *self;

	self = BRASERO_TOC2CUE (imager);
	if (type != BRASERO_TRACK_SOURCE_IMAGE
	|| !(format & BRASERO_IMAGE_FORMAT_CUE))
		BRASERO_JOB_NOT_SUPPORTED (imager);

	if (self->priv->output && self->priv->clean)
		g_remove (self->priv->output);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_toc2cue_get_track_type (BraseroImager *imager,
				BraseroTrackSourceType *type,
				BraseroImageFormat *format)
{
	if (type)
		*type = BRASERO_TRACK_SOURCE_IMAGE;

	if (format)
		*format = BRASERO_IMAGE_FORMAT_CUE;

	return BRASERO_BURN_OK;
}

static void
brasero_toc2cue_class_init (BraseroToc2CueClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroJobClass *job_class = BRASERO_JOB_CLASS (klass);
	BraseroProcessClass *process_class = BRASERO_PROCESS_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_toc2cue_finalize;

	job_class->set_source = brasero_toc2cue_set_source;

	process_class->stdout_func = brasero_toc2cue_read_stdout;
	process_class->stderr_func = brasero_toc2cue_read_stderr;
	process_class->set_argv = brasero_toc2cue_set_argv;
}

static void
brasero_toc2cue_iface_init_image (BraseroImagerIFace *iface)
{
/*	iface->get_size = brasero_toc2cue_get_size; */
	iface->get_track = brasero_toc2cue_get_track;
	iface->get_track_type = brasero_toc2cue_get_track_type;
	iface->set_output = brasero_toc2cue_set_output;
	iface->set_output_type = brasero_toc2cue_set_output_type;
}
