/***************************************************************************
 *            burn-libburn.c
 *
 *  lun aoû 21 14:33:24 2006
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <gmodule.h>

#include <libburn/libburn.h>

#include "burn-basics.h"
#include "burn-job.h"
#include "burn-plugin.h"
#include "burn-libburn-common.h"
#include "burn-libburn.h"

BRASERO_PLUGIN_BOILERPLATE (BraseroLibburn, brasero_libburn, BRASERO_TYPE_JOB, BraseroJob);

struct _BraseroLibburnPrivate {
	BraseroLibburnCtx *ctx;
};
typedef struct _BraseroLibburnPrivate BraseroLibburnPrivate;

#define BRASERO_LIBBURN_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_LIBBURN, BraseroLibburnPrivate))

/**
 * taken from scsi-get-configuration.h
 */

typedef enum {
BRASERO_SCSI_PROF_DVD_RW_RESTRICTED	= 0x0013,
BRASERO_SCSI_PROF_DVD_RW_PLUS		= 0x001A,
} BraseroScsiProfile;

static GObjectClass *parent_class = NULL;

static BraseroBurnResult
brasero_libburn_add_track (struct burn_session *session,
			   struct burn_track *track,
			   struct burn_source *src,
			   gint mode,
			   GError **error)
{
	if (burn_track_set_source (track, src) != BURN_SOURCE_OK) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("unable to set the source"));
		return BRASERO_BURN_ERR;
	}

	if (!burn_session_add_track (session, track, BURN_POS_END)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("unable to add the track to the session"));
		return BRASERO_BURN_ERR;
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_libburn_add_fd_track (struct burn_session *session,
			      int fd,
			      gint mode,
			      gint64 size,
			      GError **error)
{
	struct burn_source *src;
	struct burn_track *track;
	BraseroBurnResult result;

	track = burn_track_create ();
	burn_track_define_data (track, 0, 0, 0, mode);

	src = burn_fd_source_new (fd, -1, size);
	result = brasero_libburn_add_track (session, track, src, mode, error);

	burn_source_free (src);
	burn_track_free (track);

	return result;
}

static BraseroBurnResult
brasero_libburn_setup_session_fd (BraseroLibburn *self,
			          struct burn_session *session,
			          GError **error)
{
	int fd;
	gint64 size;
	BraseroTrackType type;
	BraseroBurnResult result = BRASERO_BURN_OK;

	brasero_job_get_fd_in (BRASERO_JOB (self), &fd);

	/* need to find out what type of track the imager will output */
	brasero_job_get_input_type (BRASERO_JOB (self), &type);
	switch (type.type) {
		case BRASERO_TRACK_TYPE_IMAGE:
		{
			gint mode;

			if (type.subtype.img_format == BRASERO_IMAGE_FORMAT_BIN)
				mode = BURN_MODE1;
			else
				mode = BURN_MODE1|BURN_MODE_RAW|BURN_SUBCODE_R96;

			brasero_job_get_session_output_size (BRASERO_JOB (self),
							     NULL,
							     &size);
			result = brasero_libburn_add_fd_track (session,
							       fd,
							       mode,
							       size,
							       error);
			break;
		}

		case BRASERO_TRACK_TYPE_AUDIO:
		{
			GSList *tracks;

			brasero_job_get_tracks (BRASERO_JOB (self), &tracks);
			for (; tracks; tracks = tracks->next) {
				BraseroTrack *track;

				track = tracks->data;
				brasero_track_get_audio_length (track, &size);
				size = BRASERO_DURATION_TO_BYTES (size);

				/* we dup the descriptor so the same 
				 * will be shared by all tracks */
				result = brasero_libburn_add_fd_track (session,
								       dup (fd),
								       BURN_AUDIO,
								       size,
								       error);
				if (result != BRASERO_BURN_OK)
					return result;
			}
			break;
		}

		default:
			BRASERO_JOB_NOT_SUPPORTED (self);
	}

	return result;
}

static BraseroBurnResult
brasero_libburn_add_file_track (struct burn_session *session,
				const gchar *path,
				gint mode,
				off_t size,
				GError **error)
{
	struct burn_source *src;
	struct burn_track *track;
	BraseroBurnResult result;

	track = burn_track_create ();
	burn_track_define_data (track, 0, 0, 0, mode);

	src = burn_file_source_new (path, NULL);
	result = brasero_libburn_add_track (session, track, src, mode, error);

	if (size > 0)
		burn_track_set_default_size (track, size);

	burn_source_free (src);
	burn_track_free (track);

	return result;
}

static BraseroBurnResult
brasero_libburn_setup_session_file (BraseroLibburn *self, 
				    struct burn_session *session,
				    GError **error)
{
	BraseroBurnResult result;
	GSList *tracks = NULL;

	/* create the track(s) */
	result = BRASERO_BURN_OK;
	brasero_job_get_tracks (BRASERO_JOB (self), &tracks);
	for (; tracks; tracks = tracks->next) {
		BraseroTrack *track;
		BraseroTrackType type;

		track = tracks->data;
		brasero_track_get_type (track, &type);
		if (type.type == BRASERO_TRACK_TYPE_AUDIO) {
			gchar *audiopath;

			audiopath = brasero_track_get_audio_source (track, FALSE);
			result = brasero_libburn_add_file_track (session,
								 audiopath,
								 BURN_AUDIO,
								 -1,
								 error);
			if (result != BRASERO_BURN_OK)
				break;
		}
		else if (type.type == BRASERO_TRACK_TYPE_IMAGE) {
			gchar *imagepath;
			gint mode;

			if (type.subtype.img_format == BRASERO_IMAGE_FORMAT_BIN) {
				mode = BURN_MODE1;
				imagepath = brasero_track_get_image_source (track, FALSE);
			}
			else if (type.subtype.img_format == BRASERO_IMAGE_FORMAT_NONE) {
				mode = BURN_MODE1;
				imagepath = brasero_track_get_image_source (track, FALSE);
			}
			else
				BRASERO_JOB_NOT_SUPPORTED (self);

			if (!imagepath)
				return BRASERO_BURN_ERR;

			result = brasero_libburn_add_file_track (session,
								 imagepath,
								 mode,
								 -1,
								 error);
		}
		else
			BRASERO_JOB_NOT_SUPPORTED (self);
	}

	return result;
}

static BraseroBurnResult
brasero_libburn_create_disc (BraseroLibburn *self,
			     struct burn_disc **retval,
			     GError **error)
{
	struct burn_disc *disc;
	BraseroBurnResult result;
	struct burn_session *session;

	/* set the source image */
	disc = burn_disc_create ();

	/* create the session */
	session = burn_session_create ();
	burn_disc_add_session (disc, session, BURN_POS_END);
	burn_session_free (session);

	if (brasero_job_get_fd_in (BRASERO_JOB (self), NULL) == BRASERO_BURN_OK)
		result = brasero_libburn_setup_session_fd (self, session, error);
	else
		result = brasero_libburn_setup_session_file (self, session, error);

	if (result != BRASERO_BURN_OK) {
		burn_disc_free (disc);
		return result;
	}

	*retval = disc;
	return result;
}

static BraseroBurnResult
brasero_libburn_start_record (BraseroLibburn *self,
			      GError **error)
{
	int profile;
	guint64 rate;
	char prof_name [80];
	BraseroBurnFlag flags;
	BraseroBurnResult result;
	BraseroLibburnPrivate *priv;
	struct burn_write_opts *opts;

	priv = BRASERO_LIBBURN_PRIVATE (self);
	if (burn_disc_get_profile (priv->ctx->drive, &profile, prof_name) < 0) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("no profile available for the medium"));
		return BRASERO_BURN_ERR;
	}

	result = brasero_libburn_create_disc (self, &priv->ctx->disc, error);
	if (result != BRASERO_BURN_OK)
		return result;

	/* Note: we don't need to call burn_drive_get_status nor
	 * burn_disc_get_status since we took care of the disc
	 * checking thing earlier ourselves. Now there is a proper
	 * disc and tray is locked. */
	opts = burn_write_opts_new (priv->ctx->drive);
	burn_write_opts_set_perform_opc (opts, 0);

	brasero_job_get_flags (BRASERO_JOB (self), &flags);
	if (profile != BRASERO_SCSI_PROF_DVD_RW_RESTRICTED
	&&  profile != BRASERO_SCSI_PROF_DVD_RW_PLUS
	&& (flags & BRASERO_BURN_FLAG_DAO))
		burn_write_opts_set_write_type (opts,
						BURN_WRITE_SAO,
						BURN_BLOCK_SAO);
	else
		burn_write_opts_set_write_type (opts,
						BURN_WRITE_TAO,
						BURN_BLOCK_MODE1);

	burn_write_opts_set_underrun_proof (opts, (flags & BRASERO_BURN_FLAG_BURNPROOF) != 0);
	burn_write_opts_set_multi (opts, (flags & BRASERO_BURN_FLAG_MULTI) != 0);
	burn_write_opts_set_simulate (opts, (flags & BRASERO_BURN_FLAG_DUMMY) != 0);

	brasero_job_get_rate (BRASERO_JOB (self), &rate);
	burn_drive_set_speed (priv->ctx->drive, rate, 0);
	burn_disc_write (opts, priv->ctx->disc);
	burn_write_opts_free (opts);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_libburn_start_erase (BraseroLibburn *self,
			     GError **error)
{
	char reasons [BURN_REASONS_LEN];
	struct burn_session *session;
	struct burn_write_opts *opts;
	BraseroLibburnPrivate *priv;
	BraseroBurnResult result;
	BraseroBurnFlag flags;
	char prof_name [80];
	int profile;
	int fd;

	priv = BRASERO_LIBBURN_PRIVATE (self);
	if (burn_disc_get_profile (priv->ctx->drive, &profile, prof_name) < 0) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("no profile available for the medium"));
		return BRASERO_BURN_ERR;
	}

	/* here we try to respect the current formatting of DVD-RW. For 
	 * overwritable media fast option means erase the first 64 Kib
	 * and long a forced reformatting */
	brasero_job_get_flags (BRASERO_JOB (self), &flags);
	if (profile == BRASERO_SCSI_PROF_DVD_RW_RESTRICTED) {
		if (!(flags & BRASERO_BURN_FLAG_FAST_BLANK)) {
			/* leave libburn choose the best format */
			burn_disc_format (priv->ctx->drive,
					  (off_t) 0,
					  (1 << 4));
			return BRASERO_BURN_OK;
		}
	}
	else if (profile == BRASERO_SCSI_PROF_DVD_RW_PLUS) {
		if (!(flags & BRASERO_BURN_FLAG_FAST_BLANK)) {
			/* Bit 2 is for format max available size
			 * Bit 4 is enforce (re)-format if needed
			 * 0x26 is DVD+RW format is to be set from bit 8
			 * in the latter case bit 7 needs to be set as 
			 * well.
			 */
			burn_disc_format (priv->ctx->drive,
					  (off_t) 0,
					  (1 << 2)|(1 << 4));
			return BRASERO_BURN_OK;
		}
	}
	else if (burn_disc_erasable (priv->ctx->drive)) {
		/* This is mainly for CDRW and sequential DVD-RW */
		burn_disc_erase (priv->ctx->drive, (flags & BRASERO_BURN_FLAG_FAST_BLANK) != 0);
		return BRASERO_BURN_OK;
	}
	else
		BRASERO_JOB_NOT_SUPPORTED (self);

	/* This is the "fast option": basically we only write 64 Kib of 0 from
	 * /dev/null */
	fd = open ("/dev/null", O_RDONLY);
	if (fd == -1) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("/dev/null can't be opened"));
		return BRASERO_BURN_ERR;
	}

	priv->ctx->disc = burn_disc_create ();

	/* create the session */
	session = burn_session_create ();
	burn_disc_add_session (priv->ctx->disc, session, BURN_POS_END);
	burn_session_free (session);

	result = brasero_libburn_add_fd_track (session,
					       fd,
					       BURN_MODE1,
					       65536,		/* 32 blocks */
					       error);
	close (fd);

	opts = burn_write_opts_new (priv->ctx->drive);
	burn_write_opts_set_perform_opc (opts, 0);
	burn_write_opts_set_underrun_proof (opts, 1);
	burn_write_opts_set_simulate (opts, (flags & BRASERO_BURN_FLAG_DUMMY));

	burn_drive_set_speed (priv->ctx->drive, burn_drive_get_write_speed (priv->ctx->drive), 0);
	burn_write_opts_set_write_type (opts,
					BURN_WRITE_TAO,
					BURN_BLOCK_MODE1);

	if (burn_precheck_write (opts, priv->ctx->disc, reasons, 0) <= 0) {
		burn_write_opts_free (opts);
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("libburn can't burn: %s"), reasons);
		return BRASERO_BURN_ERR;
	}

	burn_disc_write (opts, priv->ctx->disc);
	burn_write_opts_free (opts);

	return result;
}

static BraseroBurnResult
brasero_libburn_start (BraseroJob *job,
		       GError **error)
{
	BraseroLibburn *self;
	BraseroJobAction action;
	BraseroBurnResult result;
	BraseroLibburnPrivate *priv;

	self = BRASERO_LIBBURN (job);
	priv = BRASERO_LIBBURN_PRIVATE (self);

	priv->ctx = brasero_libburn_common_ctx_new (job, error);
	if (!priv->ctx)
		return BRASERO_BURN_ERR;

	brasero_job_get_action (job, &action);
	if (action == BRASERO_JOB_ACTION_RECORD) {
		result = brasero_libburn_start_record (self, error);
		if (result != BRASERO_BURN_OK)
			return result;

		brasero_job_set_current_action (job,
						BRASERO_BURN_ACTION_START_RECORDING,
						NULL,
						FALSE);
	}
	else if (action == BRASERO_JOB_ACTION_ERASE) {
		result = brasero_libburn_start_erase (self, error);
		if (result != BRASERO_BURN_OK)
			return result;

		brasero_job_set_current_action (job,
						BRASERO_BURN_ACTION_BLANKING,
						NULL,
						FALSE);
	}
	else
		BRASERO_JOB_NOT_SUPPORTED (self);

	burn_msgs_set_severities ("ALL", "ALL", "brasero (libburn):");

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_libburn_stop (BraseroJob *job,
		      GError **error)
{
	BraseroLibburn *self;
	BraseroLibburnPrivate *priv;

	self = BRASERO_LIBBURN (job);
	priv = BRASERO_LIBBURN_PRIVATE (self);

	if (priv->ctx) {
		brasero_libburn_common_ctx_free (priv->ctx);
		priv->ctx = NULL;
	}

	if (BRASERO_JOB_CLASS (parent_class)->stop)
		BRASERO_JOB_CLASS (parent_class)->stop (job, error);

	return BRASERO_BURN_OK;
}

static void
brasero_libburn_class_init (BraseroLibburnClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroJobClass *job_class = BRASERO_JOB_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroLibburnPrivate));

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_libburn_finalize;

	job_class->start = brasero_libburn_start;
	job_class->stop = brasero_libburn_stop;
}

static void
brasero_libburn_init (BraseroLibburn *obj)
{
	/* that's for debugging */
	burn_set_verbosity (666);
}

static void
brasero_libburn_finalize (GObject *object)
{
	BraseroLibburn *cobj;
	BraseroLibburnPrivate *priv;

	cobj = BRASERO_LIBBURN (object);
	priv = BRASERO_LIBBURN_PRIVATE (cobj);

	if (priv->ctx) {
		brasero_libburn_common_ctx_free (priv->ctx);
		priv->ctx = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static BraseroBurnResult
brasero_libburn_export_caps (BraseroPlugin *plugin, gchar **error)
{
	const BraseroMedia media_cd = BRASERO_MEDIUM_CD|
				      BRASERO_MEDIUM_REWRITABLE|
				      BRASERO_MEDIUM_WRITABLE|
				      BRASERO_MEDIUM_BLANK|
				      BRASERO_MEDIUM_APPENDABLE|
				      BRASERO_MEDIUM_HAS_AUDIO|
				      BRASERO_MEDIUM_HAS_DATA;
	const BraseroMedia media_dvd_w = BRASERO_MEDIUM_DVD|
					 BRASERO_MEDIUM_PLUS|
					 BRASERO_MEDIUM_SEQUENTIAL|
					 BRASERO_MEDIUM_WRITABLE|
					 BRASERO_MEDIUM_APPENDABLE|
					 BRASERO_MEDIUM_HAS_DATA|
					 BRASERO_MEDIUM_BLANK;
	const BraseroMedia media_dvd_rw = BRASERO_MEDIUM_DVD|
					  BRASERO_MEDIUM_SEQUENTIAL|
					  BRASERO_MEDIUM_REWRITABLE|
					  BRASERO_MEDIUM_APPENDABLE|
					  BRASERO_MEDIUM_HAS_DATA|
					  BRASERO_MEDIUM_BLANK;
	const BraseroMedia media_dvd_rw_plus = BRASERO_MEDIUM_DVD|
					       BRASERO_MEDIUM_PLUS|
					       BRASERO_MEDIUM_RESTRICTED|
					       BRASERO_MEDIUM_REWRITABLE|
					       BRASERO_MEDIUM_BLANK;
	GSList *output;
	GSList *input;

	brasero_plugin_define (plugin,
			       "libburn",
			       _("libburn burns CD(RW), DVD+/-(RW)"),
			       "Philippe Rouquier",
			       15);

	brasero_plugin_set_flags (plugin,
				  media_cd,
				  BRASERO_BURN_FLAG_DAO|
				  BRASERO_BURN_FLAG_BURNPROOF|
				  BRASERO_BURN_FLAG_OVERBURN|
				  BRASERO_BURN_FLAG_MULTI|
				  BRASERO_BURN_FLAG_DUMMY|
				  BRASERO_BURN_FLAG_NOGRACE,
				  BRASERO_BURN_FLAG_NONE);

	/* audio support for CDs only*/
	input = brasero_caps_audio_new (BRASERO_PLUGIN_IO_ACCEPT_PIPE|
					BRASERO_PLUGIN_IO_ACCEPT_FILE,
					BRASERO_AUDIO_FORMAT_RAW);
	
	output = brasero_caps_disc_new (media_cd);
	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (input);

	/* Image support for CDs ... */
	input = brasero_caps_image_new (BRASERO_PLUGIN_IO_ACCEPT_PIPE|
					BRASERO_PLUGIN_IO_ACCEPT_FILE,
					BRASERO_IMAGE_FORMAT_BIN);

	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (output);

	/* ... and DVDs +-R ... */
	brasero_plugin_set_flags (plugin,
				  BRASERO_MEDIUM_DVDR|
				  BRASERO_MEDIUM_APPENDABLE|
				  BRASERO_MEDIUM_HAS_DATA|
				  BRASERO_MEDIUM_BLANK,
				  BRASERO_BURN_FLAG_DAO|
				  BRASERO_BURN_FLAG_BURNPROOF|
				  BRASERO_BURN_FLAG_OVERBURN|
				  BRASERO_BURN_FLAG_MULTI|
				  BRASERO_BURN_FLAG_DUMMY|
				  BRASERO_BURN_FLAG_NOGRACE,
				  BRASERO_BURN_FLAG_NONE);

	/* NOTE: DVD+R don't have a dummy mode */
	brasero_plugin_set_flags (plugin,
				  BRASERO_MEDIUM_DVDR_PLUS|
				  BRASERO_MEDIUM_APPENDABLE|
				  BRASERO_MEDIUM_HAS_DATA|
				  BRASERO_MEDIUM_BLANK,
				  BRASERO_BURN_FLAG_DAO|
				  BRASERO_BURN_FLAG_BURNPROOF|
				  BRASERO_BURN_FLAG_OVERBURN|
				  BRASERO_BURN_FLAG_MULTI|
				  BRASERO_BURN_FLAG_NOGRACE,
				  BRASERO_BURN_FLAG_NONE);

	output = brasero_caps_disc_new (media_dvd_w);
	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (output);

	/* ... and finally DVDs +-RW */
	brasero_plugin_set_flags (plugin,
				  BRASERO_MEDIUM_DVDRW|
				  BRASERO_MEDIUM_APPENDABLE|
				  BRASERO_MEDIUM_HAS_DATA|
				  BRASERO_MEDIUM_BLANK,
				  BRASERO_BURN_FLAG_DAO|
				  BRASERO_BURN_FLAG_BURNPROOF|
				  BRASERO_BURN_FLAG_OVERBURN|
				  BRASERO_BURN_FLAG_MULTI|
				  BRASERO_BURN_FLAG_DUMMY|
				  BRASERO_BURN_FLAG_NOGRACE,
				  BRASERO_BURN_FLAG_NONE);

	output = brasero_caps_disc_new (media_dvd_rw);
	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (output);

	/* NOTE: libburn can't append anything to DVDRW+ and restricted
	 * moreover DVD+ R/RW don't have a dummy mode. */
	brasero_plugin_set_flags (plugin,
				  BRASERO_MEDIUM_DVDRW_RESTRICTED|
				  BRASERO_MEDIUM_BLANK,
				  BRASERO_BURN_FLAG_DAO|
				  BRASERO_BURN_FLAG_BURNPROOF|
				  BRASERO_BURN_FLAG_OVERBURN|
				  BRASERO_BURN_FLAG_DUMMY|
				  BRASERO_BURN_FLAG_NOGRACE,
				  BRASERO_BURN_FLAG_NONE);

	brasero_plugin_set_flags (plugin,
				  BRASERO_MEDIUM_DVDRW_PLUS|
				  BRASERO_MEDIUM_BLANK,
				  BRASERO_BURN_FLAG_DAO|
				  BRASERO_BURN_FLAG_BURNPROOF|
				  BRASERO_BURN_FLAG_OVERBURN|
				  BRASERO_BURN_FLAG_NOGRACE,
				  BRASERO_BURN_FLAG_NONE);

	output = brasero_caps_disc_new (media_dvd_rw_plus);
	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (output);
	g_slist_free (input);

	/* add blank caps */
	output = brasero_caps_disc_new (BRASERO_MEDIUM_CD|
					BRASERO_MEDIUM_REWRITABLE|
					BRASERO_MEDIUM_APPENDABLE|
					BRASERO_MEDIUM_CLOSED|
					BRASERO_MEDIUM_HAS_DATA|
					BRASERO_MEDIUM_HAS_AUDIO);
	brasero_plugin_blank_caps (plugin, output);
	g_slist_free (output);

	output = brasero_caps_disc_new (BRASERO_MEDIUM_DVD|
					BRASERO_MEDIUM_PLUS|
					BRASERO_MEDIUM_SEQUENTIAL|
					BRASERO_MEDIUM_RESTRICTED|
					BRASERO_MEDIUM_REWRITABLE|
					BRASERO_MEDIUM_APPENDABLE|
					BRASERO_MEDIUM_CLOSED|
					BRASERO_MEDIUM_HAS_DATA);
	brasero_plugin_blank_caps (plugin, output);
	g_slist_free (output);

	brasero_plugin_set_blank_flags (plugin,
					BRASERO_MEDIUM_CD|
					BRASERO_MEDIUM_DVD|
					BRASERO_MEDIUM_SEQUENTIAL|
					BRASERO_MEDIUM_RESTRICTED|
					BRASERO_MEDIUM_REWRITABLE|
					BRASERO_MEDIUM_APPENDABLE|
					BRASERO_MEDIUM_CLOSED|
					BRASERO_MEDIUM_HAS_DATA|
					BRASERO_MEDIUM_HAS_AUDIO,
					BRASERO_BURN_FLAG_NOGRACE|
					BRASERO_BURN_FLAG_FAST_BLANK,
					BRASERO_BURN_FLAG_NONE);

	/* no dummy mode for DVD+RW */
	brasero_plugin_set_blank_flags (plugin,
					BRASERO_MEDIUM_DVDRW_PLUS|
					BRASERO_MEDIUM_APPENDABLE|
					BRASERO_MEDIUM_CLOSED|
					BRASERO_MEDIUM_HAS_DATA,
					BRASERO_BURN_FLAG_NOGRACE|
					BRASERO_BURN_FLAG_FAST_BLANK,
					BRASERO_BURN_FLAG_NONE);

	return BRASERO_BURN_OK;
}
