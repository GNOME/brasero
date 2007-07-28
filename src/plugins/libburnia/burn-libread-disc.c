/***************************************************************************
 *            burn-libread-disc.c
 *
 *  ven aoû 25 22:15:11 2006
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>

#include "burn-basics.h"
#include "burn-caps.h"
#include "burn-imager.h"
#include "burn-libburn-common.h"
#include "burn-libread-disc.h"

#ifdef HAVE_LIBBURN

#include <libburn/libburn.h>

static void brasero_libread_disc_class_init (BraseroLibreadDiscClass *klass);
static void brasero_libread_disc_init (BraseroLibreadDisc *sp);
static void brasero_libread_disc_finalize (GObject *object);

static void brasero_libread_disc_iface_init_image (BraseroImagerIFace *iface);

static BraseroBurnResult
brasero_libread_disc_set_source (BraseroJob *job,
				 const BraseroTrackSource *source,
				 GError **error);
static BraseroBurnResult
brasero_libread_disc_set_output_type (BraseroImager *imager,
				      BraseroTrackSourceType type,
				      BraseroImageFormat format,
				      GError **error);

static BraseroBurnResult
brasero_libread_disc_set_output (BraseroImager *imager,
				 const gchar *path,
				 gboolean overwrite,
				 gboolean clean,
				 GError **error);

static BraseroBurnResult
brasero_libread_disc_start (BraseroJob *job,
			    int in_fd,
			    int *out_fd,
			    GError **error);
static BraseroBurnResult
brasero_libread_disc_stop (BraseroJob *job,
			   BraseroBurnResult retval,
			   GError **error);
static BraseroBurnResult
brasero_libread_disc_get_track (BraseroImager *imager,
				BraseroTrackSource **track,
				GError **error);

static BraseroBurnResult
brasero_libread_disc_get_track_type (BraseroImager *imager,
				     BraseroTrackSourceType *type,
				     BraseroImageFormat *format);
static BraseroBurnResult
brasero_libread_disc_get_size (BraseroImager *imager,
			       gint64 *size,
			       gboolean sectors,
			       GError **error);

typedef enum {
	BRASERO_LIBREAD_DISC_ACTION_NONE,
	BRASERO_LIBREAD_DISC_ACTION_GETTING_SIZE,
	BRASERO_LIBREAD_DISC_ACTION_IMAGING
} BraseroLibreadDiscAction;

struct _BraseroLibreadDiscPrivate {
	BraseroLibreadDiscAction action;

	BraseroBurnCaps *caps;

	BraseroTrackSource *source;
	BraseroImageFormat format;

	struct burn_disc *disc;

	gchar *output;

	gint clean:1;
	gint overwrite:1;

	gint track_ready:1;
};

static BraseroJobClass *parent_class = NULL;

GType
brasero_libread_disc_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroLibreadDiscClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_libread_disc_class_init,
			NULL,
			NULL,
			sizeof (BraseroLibreadDisc),
			0,
			(GInstanceInitFunc)brasero_libread_disc_init,
		};

		static const GInterfaceInfo imager_info =
		{
			(GInterfaceInitFunc) brasero_libread_disc_iface_init_image,
			NULL,
			NULL
		};

		type = g_type_register_static (BRASERO_TYPE_LIBBURN_COMMON, 
					       "BraseroLibreadDisc",
					       &our_info, 0);

		g_type_add_interface_static (type,
					     BRASERO_TYPE_IMAGER,
					     &imager_info);
	}

	return type;
}

static void
brasero_libread_disc_class_init (BraseroLibreadDiscClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroJobClass *job_class = BRASERO_JOB_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_libread_disc_finalize;

	job_class->set_source = brasero_libread_disc_set_source;
	job_class->start = brasero_libread_disc_start;
	job_class->stop = brasero_libread_disc_stop;
}

static void
brasero_libread_disc_iface_init_image (BraseroImagerIFace *iface)
{
	iface->get_size = brasero_libread_disc_get_size;
	iface->get_track = brasero_libread_disc_get_track;
	iface->get_track_type = brasero_libread_disc_get_track_type;
	iface->set_output = brasero_libread_disc_set_output;
	iface->set_output_type = brasero_libread_disc_set_output_type;
}

static void
brasero_libread_disc_init (BraseroLibreadDisc *obj)
{
	obj->priv = g_new0 (BraseroLibreadDiscPrivate, 1);
	obj->priv->caps = brasero_burn_caps_get_default ();
}

static void
brasero_libread_disc_stop_real (BraseroLibreadDisc *self)
{

}

static void
brasero_libread_disc_finalize (GObject *object)
{
	BraseroLibreadDisc *cobj;

	cobj = BRASERO_LIBREAD_DISC (object);

	brasero_libread_disc_stop_real (cobj);

	if (cobj->priv->source) {
		brasero_track_source_free (cobj->priv->source);
		cobj->priv->source = NULL;
	}

	if (cobj->priv->disc) {
		burn_disc_free (cobj->priv->disc);
		cobj->priv->disc = NULL;
	}

	if (cobj->priv->output) {
		if (cobj->priv->track_ready && cobj->priv->clean)
			g_remove (cobj->priv->output);

		g_free (cobj->priv->output);
		cobj->priv->output = NULL;

		cobj->priv->track_ready = 0;
	}

	if (cobj->priv->caps) {
		g_object_unref (cobj->priv->caps);
		cobj->priv->caps = NULL;
	}

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

BraseroLibreadDisc *
brasero_libread_disc_new()
{
	BraseroLibreadDisc *obj;
	
	obj = BRASERO_LIBREAD_DISC (g_object_new (BRASERO_TYPE_LIBREAD_DISC, NULL));
	
	return obj;
}

static BraseroBurnResult
brasero_libread_disc_set_source (BraseroJob *job,
				 const BraseroTrackSource *source,
				 GError **error)
{
	NautilusBurnDrive *drive;
	BraseroLibreadDisc *self;
	struct burn_drive *libburn_drive;

	self = BRASERO_LIBREAD_DISC (job);

	if (source->type != BRASERO_TRACK_SOURCE_DISC)
		BRASERO_JOB_NOT_SUPPORTED (self);

	if (self->priv->source) {
		brasero_track_source_free (self->priv->source);
		self->priv->source = NULL;
	}

	if (self->priv->disc) {
		burn_disc_free (self->priv->disc);
		self->priv->disc = NULL;
	}

	if (self->priv->output) {
		if (self->priv->track_ready && self->priv->clean)
			g_remove (self->priv->output);

		g_free (self->priv->output);
		self->priv->output = NULL;

		self->priv->track_ready = 0;
	}

	drive = source->contents.drive.disc;
	self->priv->source = brasero_track_source_copy (source);

	brasero_libburn_common_set_drive (BRASERO_LIBBURN_COMMON (job),
					  drive,
					  error);

	brasero_libburn_common_get_drive (BRASERO_LIBBURN_COMMON (self),
					  &libburn_drive);
	if (libburn_drive)
		return BRASERO_BURN_OK;

	self->priv->disc = burn_drive_get_disc (libburn_drive);
	if (!self->priv->disc) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("no disc could be found"));

		return BRASERO_BURN_ERR;
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_libread_disc_set_output (BraseroImager *imager,
				 const gchar *output,
				 gboolean overwrite,
				 gboolean clean,
				 GError **error)
{
	BraseroLibreadDisc *self;

	self = BRASERO_LIBREAD_DISC (imager);

	if (self->priv->output) {
		if (self->priv->track_ready && self->priv->clean)
			g_remove (self->priv->output);

		g_free (self->priv->output);
		self->priv->output = NULL;

		self->priv->track_ready = 0;
	}

	if (output)
		self->priv->output = g_strdup (output);

	self->priv->overwrite = overwrite;
	self->priv->clean = clean;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_libread_disc_set_output_type (BraseroImager *imager,
				      BraseroTrackSourceType type,
				      BraseroImageFormat format,
				      GError **error)
{
	BraseroLibreadDisc *self;

	self = BRASERO_LIBREAD_DISC (imager);

	if (type != BRASERO_TRACK_SOURCE_DEFAULT
	&&  type != BRASERO_TRACK_SOURCE_IMAGE)
		BRASERO_JOB_NOT_SUPPORTED (self);

	if ((format & (BRASERO_IMAGE_FORMAT_ISO|BRASERO_IMAGE_FORMAT_CLONE)) == 0)
		BRASERO_JOB_NOT_SUPPORTED (self);

	if (self->priv->format == format)
		return BRASERO_BURN_OK;

	if (self->priv->output) {
		if (self->priv->track_ready && self->priv->clean)
			g_remove (self->priv->output);

		self->priv->track_ready = 0;
	}

	self->priv->format = format;

	return BRASERO_BURN_OK;
}

static gint64
brasero_libread_disc_get_disc_sectors_real (BraseroLibreadDisc *self)
{
	int i;
	gint64 sectors;
	int nb_sessions = 0;
	struct burn_session **sessions;

	sessions = burn_disc_get_sessions (self->priv->disc, &nb_sessions);

	sectors = 0;
	for (i = 0; i < nb_sessions; i ++)
		sectors += burn_session_get_sectors (sessions[i]);

	return sectors;
}

static BraseroBurnResult
brasero_libread_disc_start (BraseroJob *job,
			    int in_fd,
			    int *out_fd,
			    GError **error)
{
	BraseroLibreadDisc *self;
	struct burn_read_opts *opts;
	struct burn_drive *drive = NULL;

	self = BRASERO_LIBREAD_DISC (job);

	if (in_fd != -1)
		BRASERO_JOB_NOT_SUPPORTED (self);

	brasero_libburn_common_get_drive (BRASERO_LIBBURN_COMMON (job),
					  &drive);
	if (!drive)
		return BRASERO_BURN_ERR;

	if (self->priv->action == BRASERO_LIBREAD_DISC_ACTION_IMAGING) {
		BraseroImageFormat format;

		if (self->priv->format == BRASERO_IMAGE_FORMAT_ANY)
			format = brasero_burn_caps_get_imager_default_format (self->priv->caps,
									      self->priv->source);
		else
			format = self->priv->format;

		opts = burn_read_opts_new (drive);

		if (format & BRASERO_IMAGE_FORMAT_CLONE)
			burn_read_opts_set_raw (opts, 1);

		burn_read_opts_set_hardware_error_recovery (opts, 1);
		burn_read_opts_set_hardware_error_retries (opts, 5);
		burn_read_opts_transfer_damaged_blocks (opts, 1);

		/* FIXME: libburn API doesn't allow to go any further */
		burn_disc_read (drive, opts);
		burn_read_opts_free (opts);

		BRASERO_JOB_TASK_SET_ACTION (job,
					     BRASERO_BURN_ACTION_CREATING_IMAGE,
					     NULL,
					     FALSE);
	}

	if (BRASERO_JOB_CLASS (parent_class)->start)
		BRASERO_JOB_CLASS (parent_class)->start (job,
							 in_fd,
							 out_fd,
							 error);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_libread_disc_stop (BraseroJob *job,
			   BraseroBurnResult retval,
			   GError **error)
{
	BraseroLibreadDisc *self;

	self = BRASERO_LIBREAD_DISC (job);

	brasero_libread_disc_stop_real (self);

	if (BRASERO_JOB_CLASS (parent_class)->stop)
		BRASERO_JOB_CLASS (parent_class)->stop (job,
							retval,
							error);
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_libread_disc_get_track (BraseroImager *imager,
				BraseroTrackSource **track,
				GError **error)
{
	BraseroLibreadDisc *self;
	BraseroImageFormat format;
	BraseroTrackSource *retval;

	self = BRASERO_LIBREAD_DISC (imager);

	if (!self->priv->track_ready) {
		BraseroBurnResult result;

		if (brasero_job_is_running (BRASERO_JOB (self)))
			return BRASERO_BURN_RUNNING;

		self->priv->action = BRASERO_LIBREAD_DISC_ACTION_IMAGING;
		result = brasero_job_run (BRASERO_JOB (self), error);
		self->priv->action = BRASERO_LIBREAD_DISC_ACTION_NONE;

		if (result != BRASERO_BURN_OK)
			return result;

		self->priv->track_ready = 1;
	}

	if (self->priv->format == BRASERO_IMAGE_FORMAT_ANY)
		format = brasero_burn_caps_get_imager_default_format (self->priv->caps,
								      self->priv->source);
	else
		format = self->priv->format;

	/* see if we are ready */
	retval = g_new0 (BraseroTrackSource, 1);
	retval->type = BRASERO_TRACK_SOURCE_IMAGE;

	if (format & BRASERO_IMAGE_FORMAT_ISO)
		retval->contents.image.image = g_strdup_printf ("file://%s", self->priv->output);
	else if (format & BRASERO_IMAGE_FORMAT_CLONE) {
		retval->contents.image.toc = NULL;
		retval->contents.image.image = g_strdup_printf ("file://%s", self->priv->output);
	}

	*track = retval;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_libread_disc_get_size (BraseroImager *imager,
			       gint64 *size,
			       gboolean in_sectors,
			       GError **error)
{
	BraseroLibreadDisc *self;
	gint64 sectors;

	self = BRASERO_LIBREAD_DISC (imager);

	if (!self->priv->disc)
		BRASERO_JOB_NOT_READY (self);

	sectors = brasero_libread_disc_get_disc_sectors_real (self);
	if (sectors <= 0)
		BRASERO_JOB_NOT_READY (self);

	if (in_sectors)
		*size = sectors;
	else
		*size = sectors * 2048;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_libread_disc_get_track_type (BraseroImager *imager,
				     BraseroTrackSourceType *type,
				     BraseroImageFormat *format)
{
	BraseroLibreadDisc *self;
	BraseroImageFormat retval;

	self = BRASERO_LIBREAD_DISC (imager);

	if (!self->priv->source)
		BRASERO_JOB_NOT_READY (self);

	if (self->priv->format == BRASERO_IMAGE_FORMAT_ANY)
		retval = brasero_burn_caps_get_imager_default_format (self->priv->caps,
								      self->priv->source);
	else
		retval = self->priv->format;

	*type = BRASERO_TRACK_SOURCE_IMAGE;
	*format = retval;

	return BRASERO_BURN_OK;
}

#endif /* HAVE_LIBBURN */
