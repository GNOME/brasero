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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <gmodule.h>

#include <libburn/libburn.h>

#include "brasero-units.h"
#include "burn-job.h"
#include "burn-debug.h"
#include "brasero-plugin-registration.h"
#include "burn-libburn-common.h"
#include "burn-libburnia.h"
#include "brasero-track-image.h"
#include "brasero-track-stream.h"


#define BRASERO_TYPE_LIBBURN         (brasero_libburn_get_type ())
#define BRASERO_LIBBURN(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_LIBBURN, BraseroLibburn))
#define BRASERO_LIBBURN_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_LIBBURN, BraseroLibburnClass))
#define BRASERO_IS_LIBBURN(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_LIBBURN))
#define BRASERO_IS_LIBBURN_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_LIBBURN))
#define BRASERO_LIBBURN_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_LIBBURN, BraseroLibburnClass))

BRASERO_PLUGIN_BOILERPLATE (BraseroLibburn, brasero_libburn, BRASERO_TYPE_JOB, BraseroJob);

#define BRASERO_PVD_SIZE	32ULL * 2048ULL

struct _BraseroLibburnPrivate {
	BraseroLibburnCtx *ctx;

	/* This buffer is used to capture Primary Volume Descriptor for
	 * for overwrite media so as to "grow" the latter. */
	unsigned char *pvd;

	guint sig_handler:1;
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

struct _BraseroLibburnSrcData {
	int fd;
	off_t size;

	/* That's for the primary volume descriptor used for overwrite media */
	int pvd_size;						/* in blocks */
	unsigned char *pvd;

	int read_pvd:1;
};
typedef struct _BraseroLibburnSrcData BraseroLibburnSrcData;

static void
brasero_libburn_src_free_data (struct burn_source *src)
{
	BraseroLibburnSrcData *data;

	data = src->data;
	close (data->fd);
	g_free (data);
}

static off_t
brasero_libburn_src_get_size (struct burn_source *src)
{
	BraseroLibburnSrcData *data;

	data = src->data;
	return data->size;
}

static int
brasero_libburn_src_set_size (struct burn_source *src,
			      off_t size)
{
	BraseroLibburnSrcData *data;

	data = src->data;
	data->size = size;
	return 1;
}

/**
 * This is a copy from burn-volume.c
 */

struct _BraseroVolDesc {
	guchar type;
	gchar id			[5];
	guchar version;
};
typedef struct _BraseroVolDesc BraseroVolDesc;

static int
brasero_libburn_src_read_xt (struct burn_source *src,
			     unsigned char *buffer,
			     int size)
{
	int total;
	BraseroLibburnSrcData *data;

	data = src->data;

	total = 0;
	while (total < size) {
		int bytes;

		bytes = read (data->fd, buffer + total, size - total);
		if (bytes < 0)
			return -1;

		if (!bytes)
			break;

		total += bytes;
	}

	/* copy the primary volume descriptor if a buffer is provided */
	if (data->pvd
	&& !data->read_pvd
	&&  data->pvd_size < BRASERO_PVD_SIZE) {
		unsigned char *current_pvd;
		int i;

		current_pvd = data->pvd + data->pvd_size;

		/* read volume descriptors until we reach the end of the
		 * buffer or find a volume descriptor set end. */
		for (i = 0; (i << 11) < size && data->pvd_size + (i << 11) < BRASERO_PVD_SIZE; i ++) {
			BraseroVolDesc *desc;

			/* No need to check the first 16 blocks */
			if ((data->pvd_size >> 11) + i < 16)
				continue;

			desc = (BraseroVolDesc *) (buffer + sizeof (BraseroVolDesc) * i);
			if (desc->type == 255) {
				data->read_pvd = 1;
				BRASERO_BURN_LOG ("found volume descriptor set end");
				break;
			}
		}

		memcpy (current_pvd, buffer, i << 11);
		data->pvd_size += i << 11;
	}

	return total;
}

static struct burn_source *
brasero_libburn_create_fd_source (int fd,
				  gint64 size,
				  unsigned char *pvd)
{
	struct burn_source *src;
	BraseroLibburnSrcData *data;

	data = g_new0 (BraseroLibburnSrcData, 1);
	data->fd = fd;
	data->size = size;
	data->pvd = pvd;

	/* FIXME: this could be wrapped into a fifo source to get a smoother
	 * data delivery. But that means another thread ... */
	src = g_new0 (struct burn_source, 1);
	src->version = 1;
	src->refcount = 1;
	src->read_xt = brasero_libburn_src_read_xt;
	src->get_size = brasero_libburn_src_get_size;
	src->set_size = brasero_libburn_src_set_size;
	src->free_data = brasero_libburn_src_free_data;
	src->data = data;

	return src;
}

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
			     _("libburn track could not be created"));
		return BRASERO_BURN_ERR;
	}

	if (!burn_session_add_track (session, track, BURN_POS_END)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("libburn track could not be created"));
		return BRASERO_BURN_ERR;
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_libburn_add_fd_track (struct burn_session *session,
			      int fd,
			      gint mode,
			      gint64 size,
			      unsigned char *pvd,
			      GError **error)
{
	struct burn_source *src;
	struct burn_track *track;
	BraseroBurnResult result;

	track = burn_track_create ();
	burn_track_define_data (track, 0, 0, 0, mode);

	src = brasero_libburn_create_fd_source (fd, size, pvd);
	result = brasero_libburn_add_track (session, track, src, mode, error);

	burn_source_free (src);
	burn_track_free (track);

	return result;
}

static BraseroBurnResult
brasero_libburn_add_file_track (struct burn_session *session,
				const gchar *path,
				gint mode,
				off_t size,
				unsigned char *pvd,
				GError **error)
{
	int fd;

	fd = open (path, O_RDONLY);
	if (fd == -1) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     "%s",
			     g_strerror (errno));
		return BRASERO_BURN_ERR;
	}

	return brasero_libburn_add_fd_track (session, fd, mode, size, pvd, error);
}

static BraseroBurnResult
brasero_libburn_setup_session_fd (BraseroLibburn *self,
			          struct burn_session *session,
			          GError **error)
{
	int fd;
	goffset bytes = 0;
	BraseroLibburnPrivate *priv;
	BraseroTrackType *type = NULL;
	BraseroBurnResult result = BRASERO_BURN_OK;

	priv = BRASERO_LIBBURN_PRIVATE (self);

	brasero_job_get_fd_in (BRASERO_JOB (self), &fd);

	/* need to find out what type of track the imager will output */
	type = brasero_track_type_new ();
	brasero_job_get_input_type (BRASERO_JOB (self), type);

	if (brasero_track_type_get_has_image (type)) {
		gint mode;

		/* FIXME: implement other IMAGE types */
		if (brasero_track_type_get_image_format (type) == BRASERO_IMAGE_FORMAT_BIN)
			mode = BURN_MODE1;
		else
			mode = BURN_MODE1|BURN_MODE_RAW|BURN_SUBCODE_R96;

		brasero_track_type_free (type);

		brasero_job_get_session_output_size (BRASERO_JOB (self),
						     NULL,
						     &bytes);

		result = brasero_libburn_add_fd_track (session,
						       fd,
						       mode,
						       bytes,
						       priv->pvd,
						       error);
	}
	else if (brasero_track_type_get_has_stream (type)) {
		GSList *tracks;
		guint64 length = 0;

		brasero_track_type_free (type);

		brasero_job_get_tracks (BRASERO_JOB (self), &tracks);
		for (; tracks; tracks = tracks->next) {
			BraseroTrack *track;

			track = tracks->data;
			brasero_track_stream_get_length (BRASERO_TRACK_STREAM (track), &length);
			bytes = BRASERO_DURATION_TO_BYTES (length);

			/* we dup the descriptor so the same 
			 * will be shared by all tracks */
			result = brasero_libburn_add_fd_track (session,
							       dup (fd),
							       BURN_AUDIO,
							       bytes,
							       NULL,
							       error);
			if (result != BRASERO_BURN_OK)
				return result;
		}
	}
	else
		BRASERO_JOB_NOT_SUPPORTED (self);

	return result;
}

static BraseroBurnResult
brasero_libburn_setup_session_file (BraseroLibburn *self, 
				    struct burn_session *session,
				    GError **error)
{
	BraseroLibburnPrivate *priv;
	BraseroBurnResult result;
	GSList *tracks = NULL;

	priv = BRASERO_LIBBURN_PRIVATE (self);

	/* create the track(s) */
	result = BRASERO_BURN_OK;
	brasero_job_get_tracks (BRASERO_JOB (self), &tracks);
	for (; tracks; tracks = tracks->next) {
		BraseroTrack *track;

		track = tracks->data;
		if (BRASERO_IS_TRACK_STREAM (track)) {
			gchar *audiopath;
			guint64 size;

			audiopath = brasero_track_stream_get_source (BRASERO_TRACK_STREAM (track), FALSE);
			brasero_track_stream_get_length (BRASERO_TRACK_STREAM (track), &size);
			size = BRASERO_DURATION_TO_BYTES (size);

			result = brasero_libburn_add_file_track (session,
								 audiopath,
								 BURN_AUDIO,
								 size,
								 NULL,
								 error);
			if (result != BRASERO_BURN_OK)
				break;
		}
		else if (BRASERO_IS_TRACK_IMAGE (track)) {
			BraseroImageFormat format;
			gchar *imagepath;
			goffset bytes;
			gint mode;

			format = brasero_track_image_get_format (BRASERO_TRACK_IMAGE (track));
			if (format == BRASERO_IMAGE_FORMAT_BIN) {
				mode = BURN_MODE1;
				imagepath = brasero_track_image_get_source (BRASERO_TRACK_IMAGE (track), FALSE);
			}
			else if (format == BRASERO_IMAGE_FORMAT_NONE) {
				mode = BURN_MODE1;
				imagepath = brasero_track_image_get_source (BRASERO_TRACK_IMAGE (track), FALSE);
			}
			else
				BRASERO_JOB_NOT_SUPPORTED (self);

			if (!imagepath)
				return BRASERO_BURN_ERR;

			result = brasero_track_get_size (track,
							 NULL,
							 &bytes);
			if (result != BRASERO_BURN_OK)
				return BRASERO_BURN_ERR;

			result = brasero_libburn_add_file_track (session,
								 imagepath,
								 mode,
								 bytes,
								 priv->pvd,
								 error);
			g_free (imagepath);
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
	guint64 rate;
	goffset blocks = 0;
	BraseroMedia media;
	BraseroBurnFlag flags;
	BraseroBurnResult result;
	BraseroLibburnPrivate *priv;
	struct burn_write_opts *opts;
	gchar reason [BURN_REASONS_LEN];

	priv = BRASERO_LIBBURN_PRIVATE (self);

	/* if appending a DVD+-RW get PVD */
	brasero_job_get_flags (BRASERO_JOB (self), &flags);
	brasero_job_get_media (BRASERO_JOB (self), &media);

	if (flags & (BRASERO_BURN_FLAG_MERGE|BRASERO_BURN_FLAG_APPEND)
	&&  BRASERO_MEDIUM_RANDOM_WRITABLE (media)
	&& (media & BRASERO_MEDIUM_HAS_DATA))
		priv->pvd = g_new0 (unsigned char, BRASERO_PVD_SIZE);

	result = brasero_libburn_create_disc (self, &priv->ctx->disc, error);
	if (result != BRASERO_BURN_OK)
		return result;

	/* Note: we don't need to call burn_drive_get_status nor
	 * burn_disc_get_status since we took care of the disc
	 * checking thing earlier ourselves. Now there is a proper
	 * disc and tray is locked. */
	opts = burn_write_opts_new (priv->ctx->drive);

	/* only turn this on for CDs */
	if ((media & BRASERO_MEDIUM_CD) != 0)
		burn_write_opts_set_perform_opc (opts, 0);
	else
		burn_write_opts_set_perform_opc (opts, 0);

	if (flags & BRASERO_BURN_FLAG_DAO)
		burn_write_opts_set_write_type (opts,
						BURN_WRITE_SAO,
						BURN_BLOCK_SAO);
	else {
		burn_write_opts_set_write_type (opts,
						BURN_WRITE_TAO,
						BURN_BLOCK_MODE1);

		/* we also set the start block to write from if MERGE is set.
		 * That only for random writable media; for other media libburn
		 * handles all by himself where to start writing. */
		if (BRASERO_MEDIUM_RANDOM_WRITABLE (media)
		&& (flags & BRASERO_BURN_FLAG_MERGE)) {
			goffset address = 0;

			brasero_job_get_next_writable_address (BRASERO_JOB (self), &address);

			BRASERO_JOB_LOG (self, "Starting to write at block = %lli and byte %lli", address, address * 2048);
			burn_write_opts_set_start_byte (opts, address * 2048);
		}
	}

	if (!BRASERO_MEDIUM_RANDOM_WRITABLE (media)) {
		BRASERO_JOB_LOG (BRASERO_JOB (self), "Setting multi %i", (flags & BRASERO_BURN_FLAG_MULTI) != 0);
		burn_write_opts_set_multi (opts, (flags & BRASERO_BURN_FLAG_MULTI) != 0);
	}

	burn_write_opts_set_underrun_proof (opts, (flags & BRASERO_BURN_FLAG_BURNPROOF) != 0);
	BRASERO_JOB_LOG (BRASERO_JOB (self), "Setting burnproof %i", (flags & BRASERO_BURN_FLAG_BURNPROOF) != 0);

	burn_write_opts_set_simulate (opts, (flags & BRASERO_BURN_FLAG_DUMMY) != 0);
	BRASERO_JOB_LOG (BRASERO_JOB (self), "Setting dummy %i", (flags & BRASERO_BURN_FLAG_DUMMY) != 0);

	brasero_job_get_rate (BRASERO_JOB (self), &rate);
	burn_drive_set_speed (priv->ctx->drive, rate, 0);

	if (burn_precheck_write (opts, priv->ctx->disc, reason, 0) < 1) {
		BRASERO_JOB_LOG (BRASERO_JOB (self), "Precheck failed %s", reason);
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     "%s",
			     reason);
		return BRASERO_BURN_ERR;
	}

	/* If we're writing to a disc remember that the session can't be under
	 * 300 sectors (= 614400 bytes) */
	brasero_job_get_session_output_size (BRASERO_JOB (self), &blocks, NULL);
	if (blocks < 300)
		brasero_job_set_output_size_for_current_track (BRASERO_JOB (self),
							       300L - blocks,
							       614400L - blocks * 2048);

	if (!priv->sig_handler) {
		burn_set_signal_handling ("brasero", NULL, 0);
		priv->sig_handler = 1;
	}

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
	if (burn_disc_get_profile (priv->ctx->drive, &profile, prof_name) <= 0) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_MEDIUM_INVALID,
			     _("The disc is not supported"));
		return BRASERO_BURN_ERR;
	}

	/* here we try to respect the current formatting of DVD-RW. For 
	 * overwritable media fast option means erase the first 64 Kib
	 * and long a forced reformatting */
	brasero_job_get_flags (BRASERO_JOB (self), &flags);
	if (profile == BRASERO_SCSI_PROF_DVD_RW_RESTRICTED) {
		if (!(flags & BRASERO_BURN_FLAG_FAST_BLANK)) {
			/* leave libburn choose the best format */
			if (!priv->sig_handler) {
				burn_set_signal_handling ("brasero", NULL, 0);
				priv->sig_handler = 1;
			}

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
			 * well. */
			if (!priv->sig_handler) {
				burn_set_signal_handling ("brasero", NULL, 0);
				priv->sig_handler = 1;
			}

			burn_disc_format (priv->ctx->drive,
					  (off_t) 0,
					  (1 << 2)|(1 << 4));
			return BRASERO_BURN_OK;
		}
	}
	else if (burn_disc_erasable (priv->ctx->drive)) {
		/* This is mainly for CDRW and sequential DVD-RW */
		if (!priv->sig_handler) {
			burn_set_signal_handling ("brasero", NULL, 0);
			priv->sig_handler = 1;
		}

		/* NOTE: for an unknown reason (to me)
		 * libburn when minimally blanking a DVD-RW
		 * will only allow to write to it with DAO 
		 * afterwards... */
		burn_disc_erase (priv->ctx->drive, (flags & BRASERO_BURN_FLAG_FAST_BLANK) != 0);
		return BRASERO_BURN_OK;
	}
	else
		BRASERO_JOB_NOT_SUPPORTED (self);

	/* This is the "fast option": basically we only write 64 Kib of 0 from
	 * /dev/null. If we reached that part it means we're dealing with
	 * overwrite media. */
	fd = open ("/dev/null", O_RDONLY);
	if (fd == -1) {
		int errnum = errno;

		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     /* Translators: first %s is the filename, second %s is the error
			      * generated from errno */
			     _("\"%s\" could not be opened (%s)"),
			     "/dev/null",
			     g_strerror (errnum));
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
					       priv->pvd,
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
			     /* Translators: %s is the error returned by libburn */
			     _("An internal error occurred (%s)"),
			     reasons);
		return BRASERO_BURN_ERR;
	}

	if (!priv->sig_handler) {
		burn_set_signal_handling ("brasero", NULL, 0);
		priv->sig_handler = 1;
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

	brasero_job_get_action (job, &action);
	if (action == BRASERO_JOB_ACTION_RECORD) {
		GError *ret_error = NULL;

		/* TRUE is a context that helps to adapt action
		 * messages like for DVD+RW which need a
		 * pre-formatting before actually writing
		 * and without this we would not know if
		 * we are actually formatting or just pre-
		 * formatting == starting to record */
		priv->ctx = brasero_libburn_common_ctx_new (job, TRUE, &ret_error);
		if (!priv->ctx) {
			if (ret_error && ret_error->code == BRASERO_BURN_ERROR_DRIVE_BUSY) {
				g_propagate_error (error, ret_error);
				return BRASERO_BURN_RETRY;
			}

			if (error)
				g_propagate_error (error, ret_error);
			return BRASERO_BURN_ERR;
		}

		result = brasero_libburn_start_record (self, error);
		if (result != BRASERO_BURN_OK)
			return result;

		brasero_job_set_current_action (job,
						BRASERO_BURN_ACTION_START_RECORDING,
						NULL,
						FALSE);
	}
	else if (action == BRASERO_JOB_ACTION_ERASE) {
		GError *ret_error = NULL;

		priv->ctx = brasero_libburn_common_ctx_new (job, FALSE, &ret_error);
		if (!priv->ctx) {
			if (ret_error && ret_error->code == BRASERO_BURN_ERROR_DRIVE_BUSY) {
				g_propagate_error (error, ret_error);
				return BRASERO_BURN_RETRY;
			}

			if (error)
				g_propagate_error (error, ret_error);
			return BRASERO_BURN_ERR;
		}

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

	if (priv->sig_handler) {
		priv->sig_handler = 0;
		burn_set_signal_handling (NULL, NULL, 1);
	}

	if (priv->ctx) {
		brasero_libburn_common_ctx_free (priv->ctx);
		priv->ctx = NULL;
	}

	if (priv->pvd) {
		g_free (priv->pvd);
		priv->pvd = NULL;
	}

	if (BRASERO_JOB_CLASS (parent_class)->stop)
		BRASERO_JOB_CLASS (parent_class)->stop (job, error);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_libburn_clock_tick (BraseroJob *job)
{
	BraseroLibburnPrivate *priv;
	BraseroBurnResult result;
	int ret;

	priv = BRASERO_LIBBURN_PRIVATE (job);
	result = brasero_libburn_common_status (job, priv->ctx);

	if (result != BRASERO_BURN_OK)
		return BRASERO_BURN_OK;

	/* Double check that everything went well */
	if (!burn_drive_wrote_well (priv->ctx->drive)) {
		BRASERO_JOB_LOG (job, "Something went wrong");
		brasero_job_error (job,
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_WRITE_MEDIUM,
						_("An error occurred while writing to disc")));
		return BRASERO_BURN_OK;
	}

	/* That's finished */
	if (!priv->pvd) {
		brasero_job_set_dangerous (job, FALSE);
		brasero_job_finished_session (job);
		return BRASERO_BURN_OK;
	}

	/* In case we append data to a DVD+RW or DVD-RW
	 * (restricted overwrite) medium , we're not
	 * done since we need to overwrite the primary
	 * volume descriptor at sector 0.
	 * NOTE: This is a synchronous call but given the size of the buffer
	 * that shouldn't block.
	 * NOTE 2: in source we read the volume descriptors until we reached
	 * either the end of the buffer or the volume descriptor set end. That's
	 * kind of useless since for a DVD 16 blocks are written at a time. */
	BRASERO_JOB_LOG (job, "Starting to overwrite primary volume descriptor");
	ret = burn_random_access_write (priv->ctx->drive,
					0,
					(char*)priv->pvd,
					BRASERO_PVD_SIZE,
					0);
	if (ret != 1) {
		BRASERO_JOB_LOG (job, "Random write failed");
		brasero_job_error (job,
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_WRITE_MEDIUM,
						_("An error occurred while writing to disc")));
		return BRASERO_BURN_OK;
	}

	brasero_job_set_dangerous (job, FALSE);
	brasero_job_finished_session (job);

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
	job_class->clock_tick = brasero_libburn_clock_tick;
}

static void
brasero_libburn_init (BraseroLibburn *obj)
{

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

static void
brasero_libburn_export_caps (BraseroPlugin *plugin)
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
					       BRASERO_MEDIUM_DUAL_L|
					       BRASERO_MEDIUM_PLUS|
					       BRASERO_MEDIUM_RESTRICTED|
					       BRASERO_MEDIUM_REWRITABLE|
					       BRASERO_MEDIUM_UNFORMATTED|
					       BRASERO_MEDIUM_BLANK|
					       BRASERO_MEDIUM_APPENDABLE|
					       BRASERO_MEDIUM_CLOSED|
					       BRASERO_MEDIUM_HAS_DATA;
	const BraseroMedia media_bd_r = BRASERO_MEDIUM_BD|
					BRASERO_MEDIUM_REWRITABLE|
					BRASERO_MEDIUM_UNFORMATTED|
					BRASERO_MEDIUM_BLANK|
					BRASERO_MEDIUM_APPENDABLE|
					BRASERO_MEDIUM_CLOSED|
					BRASERO_MEDIUM_HAS_DATA;
	GSList *output;
	GSList *input;

	brasero_plugin_define (plugin,
			       "libburn",
	                       NULL,
			       _("Burns, blanks and formats CDs, DVDs and BDs"),
			       "Philippe Rouquier",
			       15);

	/* libburn has no OVERBURN capabilities */

	/* CD(R)W */
	/* Use DAO for first session since AUDIO need it to write CD-TEXT
	 * Though libburn is unable to write CD-TEXT.... */
	/* Note: when burning multiple tracks to a CD (like audio for example)
	 * in dummy mode with TAO libburn will fail probably because it does
	 * not use a correct next writable address for the second track (it uses
	 * the same as for track #1). So remove dummy mode.
	 * This is probably the same reason why it fails at merging another
	 * session to a data CD in dummy mode. */
	BRASERO_PLUGIN_ADD_STANDARD_CDR_FLAGS (plugin,
	                                       BRASERO_BURN_FLAG_OVERBURN|
	                                       BRASERO_BURN_FLAG_DUMMY);
	BRASERO_PLUGIN_ADD_STANDARD_CDRW_FLAGS (plugin,
	                                        BRASERO_BURN_FLAG_OVERBURN|
	                                        BRASERO_BURN_FLAG_DUMMY);

	/* audio support for CDs only */
	input = brasero_caps_audio_new (BRASERO_PLUGIN_IO_ACCEPT_PIPE|
					BRASERO_PLUGIN_IO_ACCEPT_FILE,
					BRASERO_AUDIO_FORMAT_RAW_LITTLE_ENDIAN);
	
	output = brasero_caps_disc_new (media_cd);
	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (input);

	/* Image support for CDs ... */
	input = brasero_caps_image_new (BRASERO_PLUGIN_IO_ACCEPT_PIPE|
					BRASERO_PLUGIN_IO_ACCEPT_FILE,
					BRASERO_IMAGE_FORMAT_BIN);

	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (output);

	/* ... and DVD-R and DVD+R ... */
	output = brasero_caps_disc_new (media_dvd_w);
	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (output);

	BRASERO_PLUGIN_ADD_STANDARD_DVDR_PLUS_FLAGS (plugin, BRASERO_BURN_FLAG_NONE);
	BRASERO_PLUGIN_ADD_STANDARD_DVDR_FLAGS (plugin, BRASERO_BURN_FLAG_NONE);

	/* ... and DVD-RW (sequential) */
	output = brasero_caps_disc_new (media_dvd_rw);
	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (output);

	BRASERO_PLUGIN_ADD_STANDARD_DVDRW_FLAGS (plugin, BRASERO_BURN_FLAG_NONE);

	/* for DVD+/-RW restricted */
	output = brasero_caps_disc_new (media_dvd_rw_plus);
	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (output);
	g_slist_free (input);

	BRASERO_PLUGIN_ADD_STANDARD_DVDRW_RESTRICTED_FLAGS (plugin, BRASERO_BURN_FLAG_NONE);
	BRASERO_PLUGIN_ADD_STANDARD_DVDRW_PLUS_FLAGS (plugin, BRASERO_BURN_FLAG_NONE);

	/* for BD-R and BD-RE */
	output = brasero_caps_disc_new (media_bd_r);
	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (output);
	g_slist_free (input);

	BRASERO_PLUGIN_ADD_STANDARD_BD_R_FLAGS (plugin, BRASERO_BURN_FLAG_NONE);
	BRASERO_PLUGIN_ADD_STANDARD_BD_RE_FLAGS (plugin, BRASERO_BURN_FLAG_NONE);

	/* add blank caps */
	output = brasero_caps_disc_new (BRASERO_MEDIUM_CD|
					BRASERO_MEDIUM_REWRITABLE|
					BRASERO_MEDIUM_APPENDABLE|
					BRASERO_MEDIUM_CLOSED|
					BRASERO_MEDIUM_HAS_DATA|
					BRASERO_MEDIUM_HAS_AUDIO|
					BRASERO_MEDIUM_BLANK);
	brasero_plugin_blank_caps (plugin, output);
	g_slist_free (output);

	output = brasero_caps_disc_new (BRASERO_MEDIUM_DVD|
					BRASERO_MEDIUM_PLUS|
					BRASERO_MEDIUM_SEQUENTIAL|
					BRASERO_MEDIUM_RESTRICTED|
					BRASERO_MEDIUM_REWRITABLE|
					BRASERO_MEDIUM_APPENDABLE|
					BRASERO_MEDIUM_CLOSED|
					BRASERO_MEDIUM_HAS_DATA|
					BRASERO_MEDIUM_UNFORMATTED|
				        BRASERO_MEDIUM_BLANK);
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
					BRASERO_MEDIUM_HAS_AUDIO|
					BRASERO_MEDIUM_UNFORMATTED|
				        BRASERO_MEDIUM_BLANK,
					BRASERO_BURN_FLAG_NOGRACE|
					BRASERO_BURN_FLAG_FAST_BLANK,
					BRASERO_BURN_FLAG_NONE);

	/* no dummy mode for DVD+RW */
	brasero_plugin_set_blank_flags (plugin,
					BRASERO_MEDIUM_DVDRW_PLUS|
					BRASERO_MEDIUM_APPENDABLE|
					BRASERO_MEDIUM_CLOSED|
					BRASERO_MEDIUM_HAS_DATA|
					BRASERO_MEDIUM_UNFORMATTED|
				        BRASERO_MEDIUM_BLANK,
					BRASERO_BURN_FLAG_NOGRACE|
					BRASERO_BURN_FLAG_FAST_BLANK,
					BRASERO_BURN_FLAG_NONE);

	brasero_plugin_register_group (plugin, _(LIBBURNIA_DESCRIPTION));
}
