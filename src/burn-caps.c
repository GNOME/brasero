/***************************************************************************
 *            burn-caps.c
 *
 *  mar avr 18 20:58:42 2006
 *  Copyright  2006  Rouquier Philippe
 *  brasero-app@wanadoo.fr
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
#include <glib/gi18n.h>

#include <nautilus-burn-drive.h>

/* NOTE: the day we want to make a library out of this we might want to avoid
 * such a dependency. One idea could be to define an interface BraseroBurnCaps
 * and pass an object that implements such an interface to BraseroBurn every time.
 * That way we could have various implementations of the "autoconfigure" could be
 * possible and the GConf dep would remain in brasero. */
#include <gconf/gconf-client.h>

#include "burn-basics.h"
#include "burn-caps.h"
#include "burn-imager.h"
#include "burn-recorder.h"
#include "burn-readcd.h"
#include "burn-transcode.h"
#include "burn-mkisofs-base.h"
#include "burn-mkisofs.h"
#include "burn-cdrdao.h"
#include "burn-cdrecord.h"
#include "burn-growisofs.h"
#include "burn-dvd-rw-format.h"
#include "burn-libburn.h"
#include "burn-libisofs.h"
#include "burn-libread-disc.h"
#include "burn-dvdcss.h"
#include "brasero-ncb.h"

#ifdef HAVE_LIBBURN

#include <libburn/libburn.h>
#include "burn-libburn.h"

#else

#define BRASERO_TYPE_LIBBURN G_TYPE_NONE

#endif

/* This object is intended to autoconfigure the burn-engine. It should provide
 * supported and safest default flags according to system config and arguments
 * given, and also creates the most appropriate recorder and imager objects */

/* FIXME: extension
	  gboolean cdrecord_02  => no on the fly
	  gboolean cdrecord_02_1 => no the fly for audio
*/
	 

static void brasero_burn_caps_class_init (BraseroBurnCapsClass *klass);
static void brasero_burn_caps_init (BraseroBurnCaps *sp);
static void brasero_burn_caps_finalize (GObject *object);

struct BraseroBurnCapsPrivate {
	/* if FALSE: we can't create *.cue files and can't copy on the fly */
	gint minbuf;

	gboolean use_libburn;
	gboolean use_libiso;
	gboolean use_libread;

	gboolean cdrdao_disabled;
	gboolean immediate;
};

#define GCONF_KEY_CDRDAO_DISABLED	"/apps/brasero/config/cdrdao_disabled"
#define GCONF_KEY_IMMEDIATE_FLAG	"/apps/brasero/config/immed_flag"
#define GCONF_KEY_MINBUF_VALUE		"/apps/brasero/config/minbuf_value"

#define GCONF_KEY_USE_LIBBURN_BURN	"/apps/brasero/config/libburn_burn"
#define GCONF_KEY_USE_LIBBURN_ISO	"/apps/brasero/config/libburn_iso"
#define GCONF_KEY_USE_LIBBURN_READ	"/apps/brasero/config/libburn_read"

static GObjectClass *parent_class = NULL;
static BraseroBurnCaps *default_caps = NULL;

#define BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG(caps, error)			\
	{									\
		g_set_error (error,						\
			     BRASERO_BURN_ERROR,				\
			     BRASERO_BURN_ERROR_GENERAL,			\
			     _("unsupported operation (at %s)"),			\
			     G_STRLOC);						\
		return BRASERO_BURN_NOT_SUPPORTED;				\
	}

GType
brasero_burn_caps_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroBurnCapsClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_burn_caps_class_init,
			NULL,
			NULL,
			sizeof (BraseroBurnCaps),
			0,
			(GInstanceInitFunc)brasero_burn_caps_init,
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "BraseroBurnCaps",
					       &our_info,
					       0);
	}

	return type;
}

static void
brasero_burn_caps_class_init (BraseroBurnCapsClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_burn_caps_finalize;
}

static void
brasero_burn_caps_init (BraseroBurnCaps *obj)
{
	GConfClient *client;

	obj->priv = g_new0 (BraseroBurnCapsPrivate, 1);

	/* load our "configuration" */
	client = gconf_client_get_default ();

#ifdef HAVE_LIBBURN
	gboolean use_libburn, use_libisofs, use_libreaddisc;

	use_libburn = gconf_client_get_bool (client,
					     GCONF_KEY_USE_LIBBURN_BURN,
					     NULL);
	use_libisofs = gconf_client_get_bool (client,
					      GCONF_KEY_USE_LIBBURN_ISO,
					      NULL);
	use_libreaddisc = gconf_client_get_bool (client,
						 GCONF_KEY_USE_LIBBURN_READ,
						 NULL);

	if (use_libisofs || use_libreaddisc || use_libburn) {
		if (burn_initialize () == 1) {
			obj->priv->use_libburn = use_libburn;
			obj->priv->use_libiso = use_libisofs;
			obj->priv->use_libread = use_libreaddisc;
		}
		else
			g_warning ("Failed to initialize libburn\n");
	}

#endif

	obj->priv->cdrdao_disabled = gconf_client_get_bool (client,
							    GCONF_KEY_CDRDAO_DISABLED,
							    NULL);
	obj->priv->immediate = gconf_client_get_bool (client,
						      GCONF_KEY_IMMEDIATE_FLAG,
						      NULL);
	obj->priv->minbuf = gconf_client_get_int (client,
						  GCONF_KEY_MINBUF_VALUE,
						  NULL);
	g_object_unref (client);
}

static void
brasero_burn_caps_finalize (GObject *object)
{
	BraseroBurnCaps *cobj;

	cobj = BRASERO_BURNCAPS(object);
	
	default_caps = NULL;

#ifdef HAVE_LIBBURN

	if (cobj->priv->use_libburn
	||  cobj->priv->use_libiso
	||  cobj->priv->use_libread)
		burn_finish ();

#endif
		
	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

BraseroBurnCaps *
brasero_burn_caps_get_default ()
{
	if (!default_caps) 
		default_caps = BRASERO_BURNCAPS (g_object_new (BRASERO_TYPE_BURNCAPS, NULL));
	else
		g_object_ref (default_caps);

	return default_caps;
}

/* that function receives all errors returned by the object and 'learns' from 
 * these errors what are the safest defaults for a particular system. It should 
 * also offer fallbacks if an error occurs through a signal */
static BraseroBurnResult
brasero_burn_caps_job_error_cb (BraseroJob *job,
				BraseroBurnError error,
				BraseroBurnCaps *caps)
{
	if (BRASERO_IS_CDRDAO (job) && error == BRASERO_BURN_ERROR_SCSI_IOCTL) {
		GError *error = NULL;
		GConfClient *client;

		/* This is for a bug in fedora 5 that prevents from sending
		 * SCSI commands as a normal user through cdrdao. There is a
		 * fallback fortunately with cdrecord and raw images but no 
		 * on_the_fly burning */
		caps->priv->cdrdao_disabled = 1;

		/* set it in GConf to remember that next time */
		client = gconf_client_get_default ();
		gconf_client_set_bool (client, GCONF_KEY_CDRDAO_DISABLED, TRUE, &error);
		if (error) {
			g_warning ("Can't write with GConf: %s\n", error->message);
			g_error_free (error);
		}
		g_object_unref (client);

		return BRASERO_BURN_RETRY;
	}

	return BRASERO_BURN_ERR;
}

/* sets the best (safest) appropriate flags given the information
 * and the system configuration we have */

BraseroBurnResult
brasero_burn_caps_blanking_get_default_flags (BraseroBurnCaps *caps,
					      NautilusBurnMediaType media_type,
					      BraseroBurnFlag *flags,
					      gboolean *fast_default)
{
	BraseroBurnFlag default_flags = BRASERO_BURN_FLAG_NOGRACE|
					BRASERO_BURN_FLAG_EJECT;

	if (media_type == NAUTILUS_BURN_MEDIA_TYPE_UNKNOWN
	||  media_type == NAUTILUS_BURN_MEDIA_TYPE_ERROR
	||  media_type == NAUTILUS_BURN_MEDIA_TYPE_BUSY)
		return BRASERO_BURN_ERR;

	if (media_type != NAUTILUS_BURN_MEDIA_TYPE_CDRW
	&&  media_type != NAUTILUS_BURN_MEDIA_TYPE_DVDRW
	&&  media_type != NAUTILUS_BURN_MEDIA_TYPE_DVD_PLUS_RW)
		return BRASERO_BURN_NOT_SUPPORTED;

	if (!NAUTILUS_BURN_DRIVE_MEDIA_TYPE_IS_DVD (media_type) && fast_default)
		*fast_default = TRUE;

	if (flags)
		*flags = default_flags;

	return BRASERO_BURN_OK;
}

/**
 * returns the flags that must be used (compulsory), the safest flags (default),
 * and the flags that can be used (supported).
 */
BraseroBurnResult
brasero_burn_caps_get_flags (BraseroBurnCaps *caps,
			     const BraseroTrackSource *source,
			     NautilusBurnDrive *drive,
			     BraseroBurnFlag *default_retval,
			     BraseroBurnFlag *compulsory_retval,
			     BraseroBurnFlag *supported_retval)
{
	BraseroBurnFlag compulsory_flags = BRASERO_BURN_FLAG_NONE;
	BraseroBurnFlag supported_flags = BRASERO_BURN_FLAG_DONT_OVERWRITE|
					  BRASERO_BURN_FLAG_DONT_CLEAN_OUTPUT|
					  BRASERO_BURN_FLAG_CHECK_SIZE|
					  BRASERO_BURN_FLAG_NOGRACE|
					  BRASERO_BURN_FLAG_DEBUG; /* always supported */
	BraseroBurnFlag default_flags = BRASERO_BURN_FLAG_CHECK_SIZE|
					BRASERO_BURN_FLAG_NOGRACE;

	g_return_val_if_fail (BRASERO_IS_BURNCAPS (caps), BRASERO_BURN_ERR);
	g_return_val_if_fail (source != NULL, BRASERO_BURN_ERR);
	g_return_val_if_fail (drive != NULL, BRASERO_BURN_ERR);

	if (NCB_DRIVE_GET_TYPE (drive) != NAUTILUS_BURN_DRIVE_TYPE_FILE) {
		NautilusBurnMediaType media_type;
		gboolean is_appendable, is_blank, is_rewritable, has_audio;

		supported_flags |= BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE;
		supported_flags |= BRASERO_BURN_FLAG_BURNPROOF;
		supported_flags |= BRASERO_BURN_FLAG_OVERBURN;
		supported_flags |= BRASERO_BURN_FLAG_EJECT;
		supported_flags |= BRASERO_BURN_FLAG_DAO;

		default_flags |= BRASERO_BURN_FLAG_BURNPROOF;
		default_flags |= BRASERO_BURN_FLAG_EJECT;

		media_type = NCB_DRIVE_MEDIA_GET_TYPE (drive,
						       &is_blank,
						       &is_rewritable,
						       NULL,
						       &has_audio);

		is_appendable = NCB_MEDIA_IS_APPENDABLE (drive);

		/* we don't support this for DVD+-RW. Growisofs doesn't
		 * honour the option */
		if (media_type != NAUTILUS_BURN_MEDIA_TYPE_DVD_PLUS_RW)
			supported_flags |= BRASERO_BURN_FLAG_DUMMY;

		if (source->type == BRASERO_TRACK_SOURCE_DISC) {
			NautilusBurnMediaType source_media;

			/* check that the source and dest drive are not the same
			 * since then on the fly is impossible */

			/* in cd-to-cd copy we can:
			 * - make an accurate copy (on the fly) burning with cdrdao
			 * - make an accurate copy with an image with readcd -clone (RAW)
			 * - make a copy of single session CD (on the fly) with readcd (ISO).
			 *   that's what we do with DVD for example.
			 * so if no cdrdao => no on the fly with CDs */
			source_media = nautilus_burn_drive_get_media_type (source->contents.drive.disc);

			/* enable on the fly for CDs only that's the safest */
			if (!nautilus_burn_drive_equal (drive, source->contents.drive.disc)) {
				supported_flags |= BRASERO_BURN_FLAG_ON_THE_FLY;

				if (!NAUTILUS_BURN_DRIVE_MEDIA_TYPE_IS_DVD (source_media)
				&&  !caps->priv->cdrdao_disabled)
					default_flags |= BRASERO_BURN_FLAG_ON_THE_FLY;
			}
		}
		else if ((source->format & BRASERO_IMAGE_FORMAT_ISO) == 0
		     &&   source->format & (BRASERO_IMAGE_FORMAT_CUE|
					    BRASERO_IMAGE_FORMAT_CLONE|
					    BRASERO_IMAGE_FORMAT_CDRDAO)) {
			/* *.cue file and *.raw file only work with CDs */
			if (NAUTILUS_BURN_DRIVE_MEDIA_TYPE_IS_DVD (media_type))
				return BRASERO_BURN_NOT_SUPPORTED;

			/* NOTE: no need for ON_THE_FLY with _ISO or _ISO_JOLIET
			 * since image is already done */
		}
		else if (source->type == BRASERO_TRACK_SOURCE_SONG
		     ||  source->type == BRASERO_TRACK_SOURCE_AUDIO) {
			/* for audio burning our capabilities are limited to CDs */
			if (NAUTILUS_BURN_DRIVE_MEDIA_TYPE_IS_DVD (media_type))
				return BRASERO_BURN_NOT_SUPPORTED;

			supported_flags |= BRASERO_BURN_FLAG_ON_THE_FLY;

			if (!caps->priv->use_libburn)
				supported_flags |= BRASERO_BURN_FLAG_DONT_CLOSE;

			/* for the time being don't force on the fly */
			/* default_flags |= BRASERO_BURN_FLAG_ON_THE_FLY; */
		}
		else if (source->type == BRASERO_TRACK_SOURCE_GRAFTS
		     ||  source->type == BRASERO_TRACK_SOURCE_DATA) {
			supported_flags |= BRASERO_BURN_FLAG_ON_THE_FLY;
			default_flags |= BRASERO_BURN_FLAG_ON_THE_FLY;

			/* with growisofs as our sole DVD backend, the burning
			 * is always performed on the fly */
			if (NAUTILUS_BURN_DRIVE_MEDIA_TYPE_IS_DVD (media_type)) {
				supported_flags |= BRASERO_BURN_FLAG_DONT_CLOSE;
				compulsory_flags |= BRASERO_BURN_FLAG_ON_THE_FLY;

				/* FIXME: check for restricted overwrite DVD-RW */
				if (media_type == NAUTILUS_BURN_MEDIA_TYPE_DVD_PLUS_RW) {
					/* that's to increase DVD compatibility */
					if ((source->format & BRASERO_IMAGE_FORMAT_VIDEO) == 0) {
						default_flags |= BRASERO_BURN_FLAG_DONT_CLOSE;
						compulsory_flags |= BRASERO_BURN_FLAG_DONT_CLOSE;
					}
				}

				if (is_appendable)
					supported_flags |= BRASERO_BURN_FLAG_APPEND|
							   BRASERO_BURN_FLAG_MERGE;

				if (is_appendable && !is_blank && !is_rewritable) {
					compulsory_flags |= BRASERO_BURN_FLAG_APPEND;

					default_flags |= BRASERO_BURN_FLAG_APPEND|
							 BRASERO_BURN_FLAG_MERGE;
				}
			}
			else if (!caps->priv->use_libburn || !caps->priv->use_libiso) {
				supported_flags |= BRASERO_BURN_FLAG_DONT_CLOSE;

				/* when we don't know the media type we allow
				 * the following options nevertheless */
				if (media_type < NAUTILUS_BURN_MEDIA_TYPE_CD) {
					supported_flags |=  BRASERO_BURN_FLAG_APPEND|
							    BRASERO_BURN_FLAG_MERGE;

				}
				else if (is_appendable) {
					supported_flags |=  BRASERO_BURN_FLAG_APPEND;

					if (!has_audio)
						supported_flags |= BRASERO_BURN_FLAG_MERGE;

					if (is_appendable
					&& !is_blank
					&& !is_rewritable) {
						compulsory_flags |=  BRASERO_BURN_FLAG_APPEND;

						default_flags |= BRASERO_BURN_FLAG_APPEND|
								 BRASERO_BURN_FLAG_DONT_CLOSE;

						if (!has_audio)
							default_flags |= BRASERO_BURN_FLAG_MERGE;
					}
				}
			}
			/* libburn doesn't support this (yet) */
			/*else
				supported_flags |=  BRASERO_BURN_FLAG_DONT_CLOSE|
						    BRASERO_BURN_FLAG_APPEND|
						    BRASERO_BURN_FLAG_MERGE;
			*/
		}
		else if (source->type == BRASERO_TRACK_SOURCE_IMAGER) {
			supported_flags |= BRASERO_BURN_FLAG_ON_THE_FLY;
			default_flags |= BRASERO_BURN_FLAG_ON_THE_FLY;
		}

		/* NOTE: with _ISO or _ISO_JOLIET no need for ON_THE_FLY since
		 * image is already done */
	}
	else {
		if (source->type == BRASERO_TRACK_SOURCE_DISC) {
			supported_flags |= BRASERO_BURN_FLAG_EJECT;
			default_flags |= BRASERO_BURN_FLAG_EJECT;
		}

		compulsory_flags |= BRASERO_BURN_FLAG_DONT_CLEAN_OUTPUT;
		default_flags |= BRASERO_BURN_FLAG_DONT_CLEAN_OUTPUT;
	}

	if (default_retval)
		*default_retval = default_flags;
	if (supported_retval)
		*supported_retval = supported_flags;
	if (compulsory_retval)
		*compulsory_retval = compulsory_flags;
	
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_burn_caps_blanking_get_supported_flags (BraseroBurnCaps *caps,
						NautilusBurnMediaType media_type,
						BraseroBurnFlag *flags,
						gboolean *fast_supported)
{
	BraseroBurnFlag supported_flags = BRASERO_BURN_FLAG_NOGRACE|
					  BRASERO_BURN_FLAG_EJECT|
					  BRASERO_BURN_FLAG_DEBUG;

	if (media_type == NAUTILUS_BURN_MEDIA_TYPE_UNKNOWN
	||  media_type == NAUTILUS_BURN_MEDIA_TYPE_ERROR
	||  media_type == NAUTILUS_BURN_MEDIA_TYPE_BUSY)
		return BRASERO_BURN_ERR;
    
	if (media_type != NAUTILUS_BURN_MEDIA_TYPE_CDRW
	&&  media_type != NAUTILUS_BURN_MEDIA_TYPE_DVDRW
	&&  media_type != NAUTILUS_BURN_MEDIA_TYPE_DVD_PLUS_RW)
		return BRASERO_BURN_NOT_SUPPORTED;

	if (!NAUTILUS_BURN_DRIVE_MEDIA_TYPE_IS_DVD (media_type))
		supported_flags |= BRASERO_BURN_FLAG_DUMMY;

	if (fast_supported)
		*fast_supported = TRUE;

	if (flags)
		*flags = supported_flags;

	return BRASERO_BURN_OK;
}

BraseroBurnFlag
brasero_burn_caps_check_flags_consistency (BraseroBurnCaps *caps,
					   const BraseroTrackSource *source,
					   NautilusBurnDrive *drive,
					   BraseroBurnFlag flags)
{
	BraseroBurnFlag retval;
	BraseroBurnResult result;
	BraseroBurnFlag supported = BRASERO_BURN_FLAG_NONE;
	BraseroBurnFlag compulsory = BRASERO_BURN_FLAG_NONE;

	g_return_val_if_fail (BRASERO_IS_BURNCAPS (caps), BRASERO_BURN_FLAG_NONE);
	g_return_val_if_fail (source != NULL, BRASERO_BURN_FLAG_NONE);

	/* we make sure first that all the flags given are supported */
	result = brasero_burn_caps_get_flags (caps,
					      source,
					      drive,
					      NULL,
					      &compulsory,
					      &supported);
	if (result != BRASERO_BURN_OK)
		return result;

	retval = flags & supported;
	if (retval != flags)
		g_warning ("Some flags were not supported (%i => %i). Corrected\n",
			   flags,
			   retval);

	 if (retval != (retval | compulsory)) {
		g_warning ("Some compulsory flags were forgotten (%i => %i). Corrected\n",
			   (retval & compulsory),
			   compulsory);

		retval |= compulsory;
	 }

	/* we check flags consistency 
	 * NOTE: should we return an error if they are not consistent? */
	if ((source->type != BRASERO_TRACK_SOURCE_SONG
	&&   source->type != BRASERO_TRACK_SOURCE_DATA
	&&   source->type != BRASERO_TRACK_SOURCE_GRAFTS
	&&   source->type != BRASERO_TRACK_SOURCE_DISC)
	||   NCB_DRIVE_GET_TYPE (drive) == NAUTILUS_BURN_DRIVE_TYPE_FILE) {
		if (retval & BRASERO_BURN_FLAG_MERGE) {
			g_warning ("Inconsistent flag: you can't use flag merge\n");
			retval &= ~BRASERO_BURN_FLAG_MERGE;
		}
			
		if (retval & BRASERO_BURN_FLAG_APPEND) {
			g_warning ("Inconsistent flags: you can't use flag append\n");
			retval &= ~BRASERO_BURN_FLAG_APPEND;
		}

		if (retval & BRASERO_BURN_FLAG_ON_THE_FLY) {
			g_warning ("Inconsistent flag: you can't use flag on_the_fly\n");
			retval &= ~BRASERO_BURN_FLAG_ON_THE_FLY;
		}
	}

	if ((retval & (BRASERO_BURN_FLAG_MERGE|BRASERO_BURN_FLAG_APPEND)) != 0
	&&  (retval & BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE) != 0) {
		g_warning ("Inconsistent flag: you can't use flag blank_before_write\n");
		retval &= ~BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE;
	}

	if (NCB_DRIVE_GET_TYPE (drive) == NAUTILUS_BURN_DRIVE_TYPE_FILE
	&&  (retval & BRASERO_BURN_FLAG_DONT_CLEAN_OUTPUT) == 0) {
		g_warning ("Forgotten flag: you must use flag dont_clean_output\n");
		retval |= BRASERO_BURN_FLAG_DONT_CLEAN_OUTPUT;
	}

	/* since we use growisofs we have to check that */
	if ((source->type == BRASERO_TRACK_SOURCE_GRAFTS
	||   source->type == BRASERO_TRACK_SOURCE_DATA)
	&&  NCB_DRIVE_GET_TYPE (drive) != NAUTILUS_BURN_DRIVE_TYPE_FILE
	&&  nautilus_burn_drive_get_media_type (drive) > NAUTILUS_BURN_MEDIA_TYPE_CDRW
	&&  (flags & (BRASERO_BURN_FLAG_MERGE|BRASERO_BURN_FLAG_APPEND))
	&&  !(retval & BRASERO_BURN_FLAG_ON_THE_FLY)) {
		g_warning ("Merging data on a DVD: adding on_the_fly flag\n");
		retval |= BRASERO_BURN_FLAG_ON_THE_FLY;
	}

	return retval;
}

BraseroBurnResult
brasero_burn_caps_create_recorder (BraseroBurnCaps *caps,
				   BraseroRecorder **recorder,
				   const BraseroTrackSource *source,
				   NautilusBurnMediaType media_type,
				   GError **error)
{
	BraseroRecorder *obj = NULL;
	BraseroTrackSourceType type;
	BraseroImageFormat format;

	if (source->type == BRASERO_TRACK_SOURCE_IMAGER) {
		BraseroBurnResult result;
		BraseroImager *imager;

		imager = source->contents.imager.obj;
		if (BRASERO_IS_RECORDER (imager)) {
			/* if it is both recorder/imager, logically it should
			 * burn the image it creates (cdrdao/growisofs) */
			obj = BRASERO_RECORDER (imager);
			g_object_ref (obj);
			goto end;
		}

		result = brasero_imager_get_track_type (imager, &type, &format);
		if (result != BRASERO_BURN_OK)
			return result;
	}
	else {
		format = source->format;
		type = source->type;
	}

	switch (type) {
	case BRASERO_TRACK_SOURCE_AUDIO:
	case BRASERO_TRACK_SOURCE_INF:
		if (NAUTILUS_BURN_DRIVE_MEDIA_TYPE_IS_DVD (media_type))
			BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG (caps, error);

		if (caps->priv->use_libburn)
			obj = BRASERO_RECORDER (g_object_new (BRASERO_TYPE_LIBBURN, NULL));
		else
			obj = BRASERO_RECORDER (g_object_new (BRASERO_TYPE_CD_RECORD, NULL));
		break;

	case BRASERO_TRACK_SOURCE_IMAGE:
		if (NAUTILUS_BURN_DRIVE_MEDIA_TYPE_IS_DVD (media_type)) {
			/* for the time being we can only burn ISO images on DVDs */
			if (format != BRASERO_IMAGE_FORMAT_NONE
			&&!(format & BRASERO_IMAGE_FORMAT_ISO))
				BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG (caps, error);

			obj = BRASERO_RECORDER (g_object_new (BRASERO_TYPE_GROWISOFS, NULL));
		}
		else if (format & BRASERO_IMAGE_FORMAT_CDRDAO) {
			if (caps->priv->cdrdao_disabled)
				BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG (caps, error);

			obj = BRASERO_RECORDER (g_object_new (BRASERO_TYPE_CDRDAO, NULL));
		}
		else if (format & BRASERO_IMAGE_FORMAT_CUE) {
			if (!caps->priv->cdrdao_disabled) {
				/* here we have two solution either cdrdao or cdrecord
				 * but we'll use cdrdao only as a fallback except for 
				 * on the fly burning see above in IMAGERS */
				obj = BRASERO_RECORDER (g_object_new (BRASERO_TYPE_CDRDAO, NULL));
			}
			else
				obj = BRASERO_RECORDER (g_object_new (BRASERO_TYPE_CD_RECORD, NULL));
		}
		/* both cdrecord and libburn are supposed to be able to burn raw and iso images */
		else if (caps->priv->use_libburn)
			obj = BRASERO_RECORDER (g_object_new (BRASERO_TYPE_LIBBURN, NULL));
		else
			obj = BRASERO_RECORDER (g_object_new (BRASERO_TYPE_CD_RECORD, NULL));
		
		break;

	default:
		BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG (caps, error);
	}

end:
	/* This is for certain type of screwed up setup */
	if (BRASERO_IS_CD_RECORD (obj) && caps->priv->immediate)
		brasero_cdrecord_set_immediate (BRASERO_CD_RECORD (obj),
						caps->priv->minbuf);

	*recorder = obj;

	/* connect to the error signal to detect error and autoconfigure */
	g_signal_connect (obj,
			  "error",
			  G_CALLBACK (brasero_burn_caps_job_error_cb),
			  caps);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_burn_caps_create_recorder_for_blanking (BraseroBurnCaps *caps,
						BraseroRecorder **recorder,
						NautilusBurnMediaType type,
						gboolean fast,
						GError **error)
{
	BraseroRecorder *obj;

	if (type == NAUTILUS_BURN_MEDIA_TYPE_DVD_PLUS_RW) {
		/* FIXME: the problem here is that DVD-RW in restricted overwrite
		 * behaves in the same way as +RW. They have only one session and
		 * don't need to be formatted. We need to make a difference between
		 * the two modes for DVD-RW. */
		if (type == NAUTILUS_BURN_MEDIA_TYPE_DVD_PLUS_RW && !fast)
			obj = BRASERO_RECORDER (g_object_new (BRASERO_TYPE_GROWISOFS, NULL));
		else
			obj = BRASERO_RECORDER (g_object_new (BRASERO_TYPE_DVD_RW_FORMAT, NULL));
	}
	else if (type == NAUTILUS_BURN_MEDIA_TYPE_DVDRW
	     ||  type == NAUTILUS_BURN_MEDIA_TYPE_DVD_RAM) {
		obj = BRASERO_RECORDER (g_object_new (BRASERO_TYPE_DVD_RW_FORMAT, NULL));
	}
	else if (type == NAUTILUS_BURN_MEDIA_TYPE_CDRW) {
		if (caps->priv->use_libburn)
			obj = BRASERO_RECORDER (g_object_new (BRASERO_TYPE_LIBBURN, NULL));
		else
			obj = BRASERO_RECORDER (g_object_new (BRASERO_TYPE_CD_RECORD, NULL));
	}
	else		
		BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG (caps, error);

	*recorder = obj;
	return BRASERO_BURN_OK;
}

/* returns the available targets for the given settings and system configuration.
 * NOTE: the first type in the array is the default one */
BraseroBurnResult
brasero_burn_caps_get_imager_available_formats (BraseroBurnCaps *caps,
						NautilusBurnDrive *drive,
						BraseroTrackSourceType type,
						BraseroImageFormat **formats)
{
	BraseroImageFormat *retval;

	switch (type) {
	case BRASERO_TRACK_SOURCE_SONG:
		retval = g_new0 (BraseroImageFormat, 1);
		retval [0] = BRASERO_IMAGE_FORMAT_NONE;
		break;

	case BRASERO_TRACK_SOURCE_GRAFTS:
	case BRASERO_TRACK_SOURCE_DATA:
		retval = g_new0 (BraseroImageFormat, 3);
		retval [0] = BRASERO_IMAGE_FORMAT_ISO | BRASERO_IMAGE_FORMAT_JOLIET;
		retval [1] = BRASERO_IMAGE_FORMAT_ISO;
		retval [2] = BRASERO_IMAGE_FORMAT_NONE;
		break;
	
	case BRASERO_TRACK_SOURCE_IMAGE: {
		NautilusBurnMediaType media;

		media = nautilus_burn_drive_get_media_type (drive);
		if (!NAUTILUS_BURN_DRIVE_MEDIA_TYPE_IS_DVD (media)) {
			retval = g_new0 (BraseroImageFormat, 5);
			retval [0] = BRASERO_IMAGE_FORMAT_ISO;
			retval [1] = BRASERO_IMAGE_FORMAT_CLONE;
			retval [2] = BRASERO_IMAGE_FORMAT_CUE;
			retval [3] = BRASERO_IMAGE_FORMAT_CDRDAO;
			retval [4] = BRASERO_IMAGE_FORMAT_NONE;
		}
		else {
			retval = g_new0 (BraseroImageFormat, 2);
			retval [0] = BRASERO_IMAGE_FORMAT_ISO;
			retval [1] = BRASERO_IMAGE_FORMAT_NONE;
		}
		break;
	}

	case BRASERO_TRACK_SOURCE_DISC: {
		NautilusBurnMediaType type;

		type = nautilus_burn_drive_get_media_type (drive);
		if (!NAUTILUS_BURN_DRIVE_MEDIA_TYPE_IS_DVD (type)) {
			/* with CDs there are three possibilities:
			 * - if cdrdao is working => *.cue
			 * - readcd -clone => *.raw
			 * - simply readcd => *.iso 
			 */
			if (!caps->priv->cdrdao_disabled) {
				retval = g_new0 (BraseroImageFormat, 5);
				retval [0] = BRASERO_IMAGE_FORMAT_CUE;
				retval [1] = BRASERO_IMAGE_FORMAT_CLONE;
				retval [2] = BRASERO_IMAGE_FORMAT_ISO;
				retval [3] = BRASERO_IMAGE_FORMAT_CDRDAO;
				retval [4] = BRASERO_IMAGE_FORMAT_NONE;
			}
			else {
				retval = g_new0 (BraseroImageFormat, 3);
				retval [0] = BRASERO_IMAGE_FORMAT_CLONE;
				retval [1] = BRASERO_IMAGE_FORMAT_ISO;
				retval [2] = BRASERO_IMAGE_FORMAT_NONE;
			}
		}
		else {
			/* with DVDs only one type of track is possible: *.iso */
			retval = g_new0 (BraseroImageFormat, 2);
			retval [0] = BRASERO_IMAGE_FORMAT_ISO;
			retval [1] = BRASERO_IMAGE_FORMAT_NONE;
		}
		break;
	}

	default:
		return BRASERO_BURN_NOT_SUPPORTED;
	}

	*formats = retval;
	return BRASERO_BURN_OK;
}

/* returns what kind of BraseroTrackSourceType will be returned
 * by default for the given settings and system configuration */
BraseroImageFormat
brasero_burn_caps_get_imager_default_format (BraseroBurnCaps *caps,
					     const BraseroTrackSource *source)
{
	g_return_val_if_fail (BRASERO_IS_BURNCAPS (caps), BRASERO_TRACK_SOURCE_UNKNOWN);
	g_return_val_if_fail (source != NULL, BRASERO_TRACK_SOURCE_UNKNOWN);

	switch (source->type) {
	case BRASERO_TRACK_SOURCE_SONG:
		return BRASERO_IMAGE_FORMAT_NONE;

	case BRASERO_TRACK_SOURCE_GRAFTS:
	case BRASERO_TRACK_SOURCE_DATA:
		return BRASERO_IMAGE_FORMAT_ISO | BRASERO_IMAGE_FORMAT_JOLIET;

	case BRASERO_TRACK_SOURCE_IMAGE:
		return BRASERO_IMAGE_FORMAT_ISO;

	case BRASERO_TRACK_SOURCE_DISC: {
		NautilusBurnMediaType type;

		type = nautilus_burn_drive_get_media_type (source->contents.drive.disc);
		if (!NAUTILUS_BURN_DRIVE_MEDIA_TYPE_IS_DVD (type)) {
			/* with CDs there are two possible default:
			 * - if cdrdao is working => copy on the fly
			 * - readcd -clone => image => cdrecord */
			if (!caps->priv->cdrdao_disabled)
				return BRASERO_IMAGE_FORMAT_CDRDAO;
			else
				return BRASERO_IMAGE_FORMAT_CLONE;
		}
		else if (type == NAUTILUS_BURN_MEDIA_TYPE_DVD)
			return BRASERO_IMAGE_FORMAT_NONE;

		return BRASERO_IMAGE_FORMAT_ISO;
	}

	default:
		break;
	}

	return BRASERO_TRACK_SOURCE_UNKNOWN;
}

BraseroTrackSourceType
brasero_burn_caps_get_imager_default_target (BraseroBurnCaps *caps,
					     const BraseroTrackSource *source)
{
	g_return_val_if_fail (BRASERO_IS_BURNCAPS (caps), BRASERO_TRACK_SOURCE_UNKNOWN);
	g_return_val_if_fail (source != NULL, BRASERO_TRACK_SOURCE_UNKNOWN);

	switch (source->type) {
	case BRASERO_TRACK_SOURCE_SONG:
		return BRASERO_TRACK_SOURCE_AUDIO;

	case BRASERO_TRACK_SOURCE_GRAFTS:
	case BRASERO_TRACK_SOURCE_DATA:
	case BRASERO_TRACK_SOURCE_DISC:
		return BRASERO_TRACK_SOURCE_IMAGE;

	case BRASERO_TRACK_SOURCE_IMAGE:
		return BRASERO_TRACK_SOURCE_UNKNOWN;

	default:
		break;
	}

	return BRASERO_TRACK_SOURCE_UNKNOWN;
}

BraseroBurnResult
brasero_burn_caps_create_imager (BraseroBurnCaps *caps,
				 BraseroImager **imager,
				 const BraseroTrackSource *source,
				 BraseroTrackSourceType target,
				 NautilusBurnMediaType src_media_type,
				 NautilusBurnMediaType dest_media_type,
				 GError **error)
{
	BraseroImager *obj = NULL;
	BraseroImageFormat format;

	format = source->format;
	if (format == BRASERO_IMAGE_FORMAT_ANY)
		format = brasero_burn_caps_get_imager_default_format (caps, source);

	if (target == BRASERO_TRACK_SOURCE_DEFAULT)
		target = brasero_burn_caps_get_imager_default_target (caps, source);

	switch (source->type) {
	case BRASERO_TRACK_SOURCE_SONG:
		/* works only with CDs */
		if (dest_media_type > NAUTILUS_BURN_MEDIA_TYPE_CDRW)
			BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG (caps, error);

		/* we can only output one of these two types */
		if (target != BRASERO_TRACK_SOURCE_INF
		&&  target != BRASERO_TRACK_SOURCE_AUDIO)
			BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG (caps, error);

		obj = BRASERO_IMAGER (g_object_new (BRASERO_TYPE_TRANSCODE, NULL));
		break;

	case BRASERO_TRACK_SOURCE_DATA:
		/* we can only output an iso image or grafts files from data */
		if (target == BRASERO_TRACK_SOURCE_GRAFTS) {
			obj = BRASERO_IMAGER (g_object_new (BRASERO_TYPE_MKISOFS_BASE, NULL));
			break;
		}

		if (target != BRASERO_TRACK_SOURCE_IMAGE)
			BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG (caps, error);
		
		if (format & BRASERO_IMAGE_FORMAT_ISO) {
			if (dest_media_type > NAUTILUS_BURN_MEDIA_TYPE_CDRW) 
				obj = BRASERO_IMAGER (g_object_new (BRASERO_TYPE_GROWISOFS, NULL));
			else if (caps->priv->use_libiso)
				obj = BRASERO_IMAGER (g_object_new (BRASERO_TYPE_LIBISOFS, NULL));
			 else
				obj = BRASERO_IMAGER (g_object_new (BRASERO_TYPE_MKISOFS, NULL));
		}
		else /* still can't produce raw and cue images from grafts */
			BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG (caps, error);

		break;

	case BRASERO_TRACK_SOURCE_GRAFTS:
		/* we can only output an iso image from graft */
		if (target != BRASERO_TRACK_SOURCE_IMAGE)
			BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG (caps, error);

		if (!(format & BRASERO_IMAGE_FORMAT_ISO))
			BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG (caps, error);

		if (dest_media_type > NAUTILUS_BURN_MEDIA_TYPE_CDRW) 
			obj = BRASERO_IMAGER (g_object_new (BRASERO_TYPE_GROWISOFS, NULL));
		else if (caps->priv->use_libiso)
			obj = BRASERO_IMAGER (g_object_new (BRASERO_TYPE_LIBISOFS, NULL));
		else
			obj = BRASERO_IMAGER (g_object_new (BRASERO_TYPE_MKISOFS, NULL));

		break;

	case BRASERO_TRACK_SOURCE_DISC:
		if (target != BRASERO_TRACK_SOURCE_IMAGE)
			BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG (caps, error);

		if (NAUTILUS_BURN_DRIVE_MEDIA_TYPE_IS_DVD (src_media_type)) {
			if (src_media_type == NAUTILUS_BURN_MEDIA_TYPE_DVD) {
				obj = BRASERO_IMAGER (g_object_new (BRASERO_TYPE_DVDCSS, NULL));
			}
			else if (format & BRASERO_IMAGE_FORMAT_ISO) {
				/* FIXME: not sure if libread disc could do anything here */
				if (caps->priv->use_libread)
					obj = BRASERO_IMAGER (g_object_new (BRASERO_TYPE_LIBREAD_DISC, NULL));
				else
					obj = BRASERO_IMAGER (g_object_new (BRASERO_TYPE_READCD, NULL));
			}
			else
				BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG (caps, error);
		}
		else if (format & BRASERO_IMAGE_FORMAT_ISO) {
			if (caps->priv->use_libread)
				obj = BRASERO_IMAGER (g_object_new (BRASERO_TYPE_LIBREAD_DISC, NULL));
			else
				obj = BRASERO_IMAGER (g_object_new (BRASERO_TYPE_READCD, NULL));
		}
		else if (format == BRASERO_IMAGE_FORMAT_CUE) {
			/* only cdrdao can create cues */
			if (!caps->priv->cdrdao_disabled)
				obj = BRASERO_IMAGER (g_object_new (BRASERO_TYPE_CDRDAO, NULL));
			else
				BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG (caps, error);
		}
		else if (format == BRASERO_IMAGE_FORMAT_CDRDAO) {
			/* only cdrdao can create cues */
			if (!caps->priv->cdrdao_disabled)
				obj = BRASERO_IMAGER (g_object_new (BRASERO_TYPE_CDRDAO, NULL));
			else
				BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG (caps, error);
		}
		else
			BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG (caps, error);

		break;

	default:
			BRASERO_BURN_CAPS_NOT_SUPPORTED_LOG (caps, error);
	}

	*imager = obj;

	/* connect to the error signal to detect error and autoconfigure */
	g_signal_connect (obj,
			  "error",
			  G_CALLBACK (brasero_burn_caps_job_error_cb),
			  caps);

	return BRASERO_BURN_OK;
}

BraseroMediaType
brasero_burn_caps_get_required_media_type (BraseroBurnCaps *caps,
					   const BraseroTrackSource *source)
{
	BraseroMediaType required_media;

	/* all the following type can only be burnt to a CD not a DVD */
	required_media = BRASERO_MEDIA_WRITABLE;
	if (source->type == BRASERO_TRACK_SOURCE_SONG
	||  source->type == BRASERO_TRACK_SOURCE_AUDIO
	||  source->type == BRASERO_TRACK_SOURCE_INF)
		required_media |= BRASERO_MEDIA_TYPE_CD;
	else if (source->type == BRASERO_TRACK_SOURCE_IMAGE
	      ||  source->type == BRASERO_TRACK_SOURCE_GRAFTS
	      ||  source->type == BRASERO_TRACK_SOURCE_DATA) {
		if (source->format == BRASERO_IMAGE_FORMAT_CUE
		||  source->format == BRASERO_IMAGE_FORMAT_CDRDAO
		||  source->format == BRASERO_IMAGE_FORMAT_CLONE)
			required_media |= BRASERO_MEDIA_TYPE_CD;
	}
	else if (source->type == BRASERO_TRACK_SOURCE_DISC) {
		NautilusBurnMediaType media;

		media = nautilus_burn_drive_get_media_type (source->contents.drive.disc);
		if (NAUTILUS_BURN_DRIVE_MEDIA_TYPE_IS_DVD (media))
			required_media |= BRASERO_MEDIA_TYPE_DVD;
		else
			required_media |= BRASERO_MEDIA_TYPE_CD;
	}

	return required_media;
}
