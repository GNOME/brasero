/***************************************************************************
 *            burn-session.c
 *
 *  mer aoû  9 22:22:16 2006
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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>

#include "burn-session.h"
#include "burn-basics.h"
#include "burn-debug.h"
#include "burn-track.h"
#include "burn-medium.h"
#include "brasero-ncb.h"

G_DEFINE_TYPE (BraseroBurnSession, brasero_burn_session, G_TYPE_OBJECT);
#define BRASERO_BURN_SESSION_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_BURN_SESSION, BraseroBurnSessionPrivate))

struct _BraseroSessionSetting {
	NautilusBurnDrive *burner;

	guint num_copies;

	/**
	 * Used when outputting an image instead of burning
	 */
	BraseroImageFormat format;
	gchar *image;
	gchar *toc;

	/**
	 * Used when burning
	 */
	gchar *label;
	guint64 rate;

	gchar *tmpdir;

	BraseroBurnFlag flags;
};
typedef struct _BraseroSessionSetting BraseroSessionSetting;

struct _BraseroBurnSessionPrivate {
	FILE *session;
	gchar *session_path;

	GSList *wrong_checksums;

	GSList *tmpfiles;

	BraseroSessionSetting settings [1];
	GSList *pile_settings;

	BraseroTrackType input;

	guint src_added_sig;
	guint src_removed_sig;
	guint dest_added_sig;
	guint dest_removed_sig;

	GSList *tracks;
	GSList *pile_tracks;
};
typedef struct _BraseroBurnSessionPrivate BraseroBurnSessionPrivate;

#define BRASERO_BURN_SESSION_WRITE_TO_DISC(priv)	(priv->settings->burner			\
							&& NCB_DRIVE_GET_TYPE (priv->settings->burner) \
							!= NAUTILUS_BURN_DRIVE_TYPE_FILE)
#define BRASERO_BURN_SESSION_WRITE_TO_FILE(priv)	(priv->settings->burner			\
							&& NCB_DRIVE_GET_TYPE (priv->settings->burner) \
							== NAUTILUS_BURN_DRIVE_TYPE_FILE)
#define BRASERO_STR_EQUAL(a, b)	((!(a) && !(b)) || ((a) && (b) && !strcmp ((a), (b))))

typedef enum {
	INPUT_CHANGED_SIGNAL,
	OUTPUT_CHANGED_SIGNAL,
	LAST_SIGNAL
} BraseroBurnSessionSignalType;

static guint brasero_burn_session_signals [LAST_SIGNAL] = { 0 };
static GObjectClass *parent_class = NULL;

static void
brasero_session_settings_clean (BraseroSessionSetting *settings)
{
	if (settings->image) {
		g_free (settings->image);
		settings->image = NULL;
	}

	if (settings->toc) {
		g_free (settings->toc);
		settings->toc = NULL;
	}

	if (settings->tmpdir) {
		g_free (settings->tmpdir);
		settings->tmpdir = NULL;
	}

	if (settings->label) {
		g_free (settings->label);
		settings->label = NULL;
	}

	if (settings->burner) {
		nautilus_burn_drive_unref (settings->burner);
		settings->burner = NULL;
	}
}

void
brasero_session_settings_copy (BraseroSessionSetting *dest,
			       BraseroSessionSetting *original)
{
	brasero_session_settings_clean (dest);

	memcpy (dest, original, sizeof (BraseroSessionSetting));

	nautilus_burn_drive_ref (dest->burner);
	dest->image = g_strdup (original->image);
	dest->toc = g_strdup (original->toc);
	dest->label = g_strdup (original->label);
	dest->tmpdir = g_strdup (original->tmpdir);
}

static void
brasero_session_settings_free (BraseroSessionSetting *settings)
{
	brasero_session_settings_clean (settings);
	g_free (settings);
}

static void
brasero_burn_session_src_media_added (NautilusBurnDrive *drive,
				      BraseroBurnSession *self)
{
	g_signal_emit (self,
		       brasero_burn_session_signals [INPUT_CHANGED_SIGNAL],
		       0);
}

static void
brasero_burn_session_src_media_removed (NautilusBurnDrive *drive,
					BraseroBurnSession *self)
{
	g_signal_emit (self,
		       brasero_burn_session_signals [INPUT_CHANGED_SIGNAL],
		       0);
}

static void
brasero_burn_session_start_src_drive_monitoring (BraseroBurnSession *self)
{
	NautilusBurnDrive *drive;
	BraseroBurnSessionPrivate *priv;

	if (brasero_burn_session_get_input_type (self, NULL) != BRASERO_TRACK_TYPE_DISC)
		return;

	drive = brasero_burn_session_get_src_drive (self);
	if (!drive)
		return;

	priv = BRASERO_BURN_SESSION_PRIVATE (self);
	priv->src_added_sig = g_signal_connect (drive,
						"media-added",
						G_CALLBACK (brasero_burn_session_src_media_added),
						self);
	priv->src_removed_sig = g_signal_connect (drive,
						  "media-removed",
						  G_CALLBACK (brasero_burn_session_src_media_removed),
						  self);
}

static void
brasero_burn_session_stop_src_drive_monitoring (BraseroBurnSession *self)
{
	NautilusBurnDrive *drive;
	BraseroBurnSessionPrivate *priv;

	if (brasero_burn_session_get_input_type (self, NULL) != BRASERO_TRACK_TYPE_DISC)
		return;

	drive = brasero_burn_session_get_src_drive (self);
	if (!drive)
		return;

	priv = BRASERO_BURN_SESSION_PRIVATE (self);

	if (priv->src_added_sig) {
		g_signal_handler_disconnect (drive, priv->src_added_sig);
		priv->src_added_sig = 0;
	}

	if (priv->src_removed_sig) {
		g_signal_handler_disconnect (drive, priv->src_removed_sig);
		priv->src_removed_sig = 0;
	}
}

void
brasero_burn_session_free_tracks (BraseroBurnSession *self)
{
	BraseroBurnSessionPrivate *priv;

	g_return_if_fail (BRASERO_IS_BURN_SESSION (self));

	priv = BRASERO_BURN_SESSION_PRIVATE (self);

	brasero_burn_session_stop_src_drive_monitoring (self);

	g_slist_foreach (priv->tracks, (GFunc) brasero_track_unref, NULL);
	g_slist_free (priv->tracks);
	priv->tracks = NULL;

	g_signal_emit (self,
		       brasero_burn_session_signals [INPUT_CHANGED_SIGNAL],
		       0);
}

BraseroBurnResult
brasero_burn_session_add_track (BraseroBurnSession *self,
				BraseroTrack *new_track)
{
	BraseroBurnSessionPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (self), BRASERO_BURN_ERR);

	priv = BRASERO_BURN_SESSION_PRIVATE (self);

	brasero_track_ref (new_track);
	if (!priv->tracks) {
		BraseroTrackType new_type;

		brasero_track_get_type (new_track, &new_type);

		/* we only need to emit the signal here since if there are
		 * multiple tracks they must be exactly of the same time */
		priv->tracks = g_slist_prepend (NULL, new_track);
		brasero_burn_session_start_src_drive_monitoring (self);

		/* if (!brasero_track_type_equal (priv->input, &new_type)) */
			g_signal_emit (self,
				       brasero_burn_session_signals [INPUT_CHANGED_SIGNAL],
				       0);

		return BRASERO_BURN_OK;
	}

	brasero_burn_session_stop_src_drive_monitoring (self);

	/* if there is already a track, then we replace it on condition that it
	 * has the same type */
	if (brasero_track_get_type (new_track, NULL) != BRASERO_TRACK_TYPE_AUDIO
	||  brasero_burn_session_get_input_type (self, NULL) != BRASERO_TRACK_TYPE_AUDIO) {
		g_slist_foreach (priv->tracks, (GFunc) brasero_track_unref, NULL);
		g_slist_free (priv->tracks);

		priv->tracks = g_slist_prepend (NULL, new_track);
		brasero_burn_session_start_src_drive_monitoring (self);

		g_signal_emit (self,
			       brasero_burn_session_signals [INPUT_CHANGED_SIGNAL],
			       0);
	}
	else
		priv->tracks = g_slist_append (priv->tracks, new_track);

	return BRASERO_BURN_OK;
}

GSList *
brasero_burn_session_get_tracks (BraseroBurnSession *self)
{
	BraseroBurnSessionPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (self), NULL);

	priv = BRASERO_BURN_SESSION_PRIVATE (self);

	return priv->tracks;
}

void
brasero_burn_session_set_input_type (BraseroBurnSession *self,
				     BraseroTrackType *type)
{
	BraseroBurnSessionPrivate *priv;

	g_return_if_fail (BRASERO_IS_BURN_SESSION (self));
	g_return_if_fail (type != NULL);

	priv = BRASERO_BURN_SESSION_PRIVATE (self);

	memcpy (&priv->input, type, sizeof (BraseroTrackType));

	if (!priv->tracks)
		g_signal_emit (self,
			       brasero_burn_session_signals [INPUT_CHANGED_SIGNAL],
			       0);
}

BraseroTrackDataType
brasero_burn_session_get_input_type (BraseroBurnSession *self,
				     BraseroTrackType *type)
{
	BraseroTrack *track;
	BraseroBurnSessionPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (self), BRASERO_TRACK_TYPE_NONE);

	priv = BRASERO_BURN_SESSION_PRIVATE (self);

	if (!priv->tracks) {
		if (type)
			memcpy (type, &priv->input, sizeof (BraseroTrackType));

		return priv->input.type;
	}

	/* there can be many tracks (in case of audio) but they must be
	 * all of the same kind for the moment */
	track = priv->tracks->data;
	return brasero_track_get_type (track, type);
}

/**
 *
 */

static void
brasero_burn_session_dest_media_added (NautilusBurnDrive *drive,
				       BraseroBurnSession *self)
{
	g_signal_emit (self,
		       brasero_burn_session_signals [OUTPUT_CHANGED_SIGNAL],
		       0);
}

static void
brasero_burn_session_dest_media_removed (NautilusBurnDrive *drive,
					 BraseroBurnSession *self)
{
	g_signal_emit (self,
		       brasero_burn_session_signals [OUTPUT_CHANGED_SIGNAL],
		       0);
}

void
brasero_burn_session_set_burner (BraseroBurnSession *self,
				 NautilusBurnDrive *drive)
{
	BraseroBurnSessionPrivate *priv;

	g_return_if_fail (BRASERO_IS_BURN_SESSION (self));

	priv = BRASERO_BURN_SESSION_PRIVATE (self);

	if (nautilus_burn_drive_equal (drive, priv->settings->burner))
		return;

	if (priv->settings->burner) {
		if (priv->dest_added_sig) {
			g_signal_handler_disconnect (priv->settings->burner,
						     priv->dest_added_sig);
			priv->dest_added_sig = 0;
		}

		if (priv->dest_removed_sig) {
			g_signal_handler_disconnect (priv->settings->burner,
						     priv->dest_removed_sig);
			priv->dest_removed_sig = 0;	
		}

		nautilus_burn_drive_unref (priv->settings->burner);
	}

	if (drive) {
		priv->dest_added_sig = g_signal_connect (drive,
							 "media-added",
							 G_CALLBACK (brasero_burn_session_dest_media_added),
							 self);
		priv->dest_removed_sig = g_signal_connect (drive,
							   "media-removed",
							   G_CALLBACK (brasero_burn_session_dest_media_removed),
							   self);
		nautilus_burn_drive_ref (drive);
	}

	priv->settings->burner = drive;

	g_signal_emit (self,
		       brasero_burn_session_signals [OUTPUT_CHANGED_SIGNAL],
		       0);
}

NautilusBurnDrive *
brasero_burn_session_get_burner (BraseroBurnSession *self)
{
	BraseroBurnSessionPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (self), NULL);

	priv = BRASERO_BURN_SESSION_PRIVATE (self);
	return priv->settings->burner;
}

BraseroBurnResult
brasero_burn_session_set_rate (BraseroBurnSession *self, guint64 rate)
{
	BraseroBurnSessionPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (self), BRASERO_BURN_ERR);

	priv = BRASERO_BURN_SESSION_PRIVATE (self);

	if (!BRASERO_BURN_SESSION_WRITE_TO_DISC (priv))
		return BRASERO_BURN_ERR;

	priv->settings->rate = rate;
	return BRASERO_BURN_OK;
}

guint64
brasero_burn_session_get_rate (BraseroBurnSession *self)
{
	BraseroBurnSessionPrivate *priv;
	gint64 max_rate;

	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (self), 0);

	priv = BRASERO_BURN_SESSION_PRIVATE (self);

	if (!BRASERO_BURN_SESSION_WRITE_TO_DISC (priv))
		return 0;

	max_rate = NCB_MEDIA_GET_MAX_WRITE_RATE (priv->settings->burner);
	if (priv->settings->rate <= 0)
		return max_rate;
	else
		return MIN (max_rate, priv->settings->rate);
}

void
brasero_burn_session_set_num_copies (BraseroBurnSession *self,
				     guint copies)
{
	BraseroBurnSessionPrivate *priv;

	g_return_if_fail (BRASERO_IS_BURN_SESSION (self));

	priv = BRASERO_BURN_SESSION_PRIVATE (self);

	if (!BRASERO_BURN_SESSION_WRITE_TO_DISC (priv))
		return;

	priv->settings->num_copies = copies;
}

guint
brasero_burn_session_get_num_copies (BraseroBurnSession *self)
{
	BraseroBurnSessionPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (self), 0);

	priv = BRASERO_BURN_SESSION_PRIVATE (self);

	if (!BRASERO_BURN_SESSION_WRITE_TO_DISC (priv))
		return 1;

	return priv->settings->num_copies;
}

static gchar *
brasero_burn_session_get_file_complement (BraseroBurnSession *self,
					  BraseroImageFormat format,
					  const gchar *path)
{
	gchar *retval = NULL;
	BraseroBurnSessionPrivate *priv;

	priv = BRASERO_BURN_SESSION_PRIVATE (self);

	if (format == BRASERO_IMAGE_FORMAT_CLONE) {
		if (g_str_has_suffix (path, ".toc"))
			retval = g_strdup_printf ("%.*sraw",
						  strlen (path) - 3,
						  path);
		else
			retval = g_strdup_printf ("%s.raw", path);
	}
	else if (format == BRASERO_IMAGE_FORMAT_CUE) {
		if (g_str_has_suffix (path, ".cue"))
			retval = g_strdup_printf ("%.*sbin",
						  strlen (path) - 3,
						  path);
		else
			retval = g_strdup_printf ("%s.bin", path);
	}
	else if (format == BRASERO_IMAGE_FORMAT_CDRDAO) {
		if (g_str_has_suffix (path, ".toc"))
			retval = g_strdup_printf ("%.*sbin",
						  strlen (path) - 3,
						  path);
		else
			retval = g_strdup_printf ("%s.bin", path);
	}
	else
		retval = NULL;

	return retval;
}

static BraseroBurnResult
brasero_burn_session_file_test (BraseroBurnSession *self,
				const gchar *path,
				GError **error)
{
	BraseroBurnSessionPrivate *priv;

	priv = BRASERO_BURN_SESSION_PRIVATE (self);

	if (!path) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("no path"));
		return BRASERO_BURN_ERR;
	}

	if (!g_file_test (path, G_FILE_TEST_EXISTS))
		return BRASERO_BURN_OK;
	
	if (priv->settings->flags & BRASERO_BURN_FLAG_DONT_OVERWRITE) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("%s already exists"),
			     path);
		return BRASERO_BURN_ERR;
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_session_set_image_output_retval (BraseroBurnSession *self,
					      BraseroImageFormat format,
					      gchar **image,
					      gchar **toc,
					      gchar *output,
					      gchar *complement)
{
	BraseroBurnSessionPrivate *priv;

	priv = BRASERO_BURN_SESSION_PRIVATE (self);

	switch (format) {
	case BRASERO_IMAGE_FORMAT_BIN:
	case BRASERO_IMAGE_FORMAT_NONE:
		if (image)
			*image = output;
		else
			g_free (output);

		if (toc)
			*toc = NULL;

		g_free (complement);
		break;

	case BRASERO_IMAGE_FORMAT_CLONE:
		if (image)
			*image = output;
		else
			g_free (output);

		if (toc)
			*toc = complement;
		else
			g_free (complement);
		break;

	case BRASERO_IMAGE_FORMAT_CUE:
	case BRASERO_IMAGE_FORMAT_CDRDAO:
		if (image)
			*image = complement;
		else
			g_free (complement);

		if (toc)
			*toc = output;
		else
			g_free (output);
		break;

	default:
		return BRASERO_BURN_ERR;
	}

	return BRASERO_BURN_OK;
}

/**
 * This function returns a path only if we should output to a file image
 * and not burn.
 */

BraseroBurnResult
brasero_burn_session_get_output (BraseroBurnSession *self,
				 gchar **image,
				 gchar **toc,
				 GError **error)
{
	BraseroBurnResult result;
	BraseroBurnSessionPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (self), BRASERO_BURN_ERR);

	priv = BRASERO_BURN_SESSION_PRIVATE (self);

	if (!BRASERO_BURN_SESSION_WRITE_TO_FILE (priv)) {
		BRASERO_BURN_LOG ("no file disc");
		return BRASERO_BURN_ERR;
	}

	/* output paths were set so test them and returns them if OK */
	if (priv->settings->image) {
		result = brasero_burn_session_file_test (self,
							 priv->settings->image,
							 error);
		if (result != BRASERO_BURN_OK) {
			BRASERO_BURN_LOG ("Problem with image existence");
			return result;
		}
	}
	else {
		BRASERO_BURN_LOG ("no output specified");

		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("no output specified"));
		return BRASERO_BURN_ERR;
	}

	if (priv->settings->toc) {
		result = brasero_burn_session_file_test (self,
							 priv->settings->toc,
							 error);
		if (result != BRASERO_BURN_OK) {
			BRASERO_BURN_LOG ("Problem with toc existence");
			return result;
		}
	}

	if (image)
		*image = g_strdup (priv->settings->image);
	if (toc)
		*toc = g_strdup (priv->settings->toc);

	return BRASERO_BURN_OK;
}

BraseroImageFormat
brasero_burn_session_get_output_format (BraseroBurnSession *self)
{
	BraseroBurnSessionPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (self), BRASERO_IMAGE_FORMAT_NONE);

	priv = BRASERO_BURN_SESSION_PRIVATE (self);

	if (!BRASERO_BURN_SESSION_WRITE_TO_FILE (priv))
		return BRASERO_IMAGE_FORMAT_NONE;

	return priv->settings->format;
}

/**
 * This function allows to tell where we should write the image. Depending on
 * the type of image it can be a toc (cue) or the path of the image (all others)
 */

BraseroBurnResult
brasero_burn_session_set_image_output_full (BraseroBurnSession *self,
					    BraseroImageFormat format,
					    const gchar *image,
					    const gchar *toc)
{
	BraseroBurnSessionPrivate *priv;
	NautilusBurnDriveMonitor *monitor;

	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (self), BRASERO_BURN_ERR);

	priv = BRASERO_BURN_SESSION_PRIVATE (self);
	monitor = nautilus_burn_get_drive_monitor ();

	if (!BRASERO_BURN_SESSION_WRITE_TO_FILE (priv))
		brasero_burn_session_set_burner (self, nautilus_burn_drive_monitor_get_drive_for_image (monitor));

	if (priv->settings->format == format
	&&  BRASERO_STR_EQUAL (image, priv->settings->image)
	&&  BRASERO_STR_EQUAL (toc, priv->settings->toc))
		return BRASERO_BURN_OK;

	if (priv->settings->image)
		g_free (priv->settings->image);

	if (image)
		priv->settings->image = g_strdup (image);
	else
		priv->settings->image = NULL;

	if (priv->settings->toc)
		g_free (priv->settings->toc);

	if (toc)
		priv->settings->toc = g_strdup (toc);
	else
		priv->settings->toc = NULL;

	priv->settings->format = format;

	g_signal_emit (self,
		       brasero_burn_session_signals [OUTPUT_CHANGED_SIGNAL],
		       0);
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_burn_session_set_image_output (BraseroBurnSession *self,
				       BraseroImageFormat format,
				       const gchar *path)
{
	BraseroBurnSessionPrivate *priv;
	BraseroBurnResult result;
	gchar *complement;
	gchar *image = NULL;
	gchar *toc = NULL;

	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (self), BRASERO_BURN_ERR);

	priv = BRASERO_BURN_SESSION_PRIVATE (self);

	/* find the file complement */
	complement = brasero_burn_session_get_file_complement (self,
							       format,
							       path);

	brasero_burn_session_set_image_output_retval (self,
						      format,
						      &image,
						      &toc,
						      g_strdup (path),
						      complement);

	result = brasero_burn_session_set_image_output_full (self,
							     format,
							     image,
							     toc);
	g_free (image);
	g_free (toc);

	return result;
}

/**
 *
 */

BraseroBurnResult
brasero_burn_session_set_tmpdir (BraseroBurnSession *self,
				 const gchar *path)
{
	BraseroBurnSessionPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (self), BRASERO_BURN_ERR);

	priv = BRASERO_BURN_SESSION_PRIVATE (self);

	if (priv->settings->tmpdir)
		g_free (priv->settings->tmpdir);

	if (path)
		priv->settings->tmpdir = g_strdup (path);
	else
		priv->settings->tmpdir = NULL;

	return BRASERO_BURN_OK;
}

const gchar *
brasero_burn_session_get_tmpdir (BraseroBurnSession *self)
{
	BraseroBurnSessionPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (self), NULL);

	priv = BRASERO_BURN_SESSION_PRIVATE (self);
	return priv->settings->tmpdir? priv->settings->tmpdir:g_get_tmp_dir ();
}

BraseroBurnResult
brasero_burn_session_get_tmp_dir (BraseroBurnSession *self,
				  gchar **path,
				  GError **error)
{
	gchar *tmp;
	const gchar *tmpdir;
	BraseroBurnSessionPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (self), BRASERO_BURN_ERR);

	priv = BRASERO_BURN_SESSION_PRIVATE (self);

	/* create a working directory in tmp */
	tmpdir = priv->settings->tmpdir ?
		 priv->settings->tmpdir :
		 g_get_tmp_dir ();

	tmp = g_build_path (G_DIR_SEPARATOR_S,
			    tmpdir,
			    BRASERO_BURN_TMP_FILE_NAME,
			    NULL);

	*path = mkdtemp (tmp);
	if (*path == NULL) {
		g_free (tmp);
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("a temporary directory could not be created (%s)"),
			     strerror (errno));
		return BRASERO_BURN_ERR;
	}

	/* this must be removed when session is completly unreffed */
	priv->tmpfiles = g_slist_prepend (priv->tmpfiles, g_strdup (tmp));

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_burn_session_get_tmp_file (BraseroBurnSession *self,
				   const gchar *suffix,
				   gchar **path,
				   GError **error)
{
	BraseroBurnSessionPrivate *priv;
	const gchar *tmpdir;
	gchar *name;
	gchar *tmp;
	int fd;

	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (self), BRASERO_BURN_ERR);

	priv = BRASERO_BURN_SESSION_PRIVATE (self);

	if (!path)
		return BRASERO_BURN_OK;

	/* takes care of the output file */
	tmpdir = priv->settings->tmpdir ?
		 priv->settings->tmpdir :
		 g_get_tmp_dir ();

	name = g_strconcat (BRASERO_BURN_TMP_FILE_NAME, suffix, NULL);
	tmp = g_build_path (G_DIR_SEPARATOR_S,
			    tmpdir,
			    name,
			    NULL);
	g_free (name);

	fd = g_mkstemp (tmp);
	if (fd == -1) {
		g_free (tmp);
		g_set_error (error, 
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("a temporary file can't be created: %s"),
			     strerror (errno));
		return BRASERO_BURN_ERR;
	}

	/* this must be removed when session is completly unreffed */
	priv->tmpfiles = g_slist_prepend (priv->tmpfiles,
					  g_strdup (tmp));

	close (fd);
	*path = tmp;
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_burn_session_get_tmp_image (BraseroBurnSession *self,
				    BraseroImageFormat format,
				    gchar **image,
				    gchar **toc,
				    GError **error)
{
	BraseroBurnSessionPrivate *priv;
	BraseroBurnResult result;
	gchar *complement = NULL;
	gchar *path = NULL;

	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (self), BRASERO_BURN_ERR);

	priv = BRASERO_BURN_SESSION_PRIVATE (self);

	/* Image tmp file */
	result = brasero_burn_session_get_tmp_file (self, NULL, &path, error);
	if (result != BRASERO_BURN_OK)
		return result;

	if (format != BRASERO_IMAGE_FORMAT_BIN) {
		/* toc tmp file */
		complement = brasero_burn_session_get_file_complement (self, format, path);
		if (complement) {
			result = brasero_burn_session_file_test (self,
								 complement,
								 error);
			if (result != BRASERO_BURN_OK) {
				g_free (complement);
				return result;
			}
		}
	}

	if (complement)
		priv->tmpfiles = g_slist_prepend (priv->tmpfiles,
						  g_strdup (complement));

	brasero_burn_session_set_image_output_retval (self,
						      format,
						      image,
						      toc,
						      path,
						      complement);

	return BRASERO_BURN_OK;
}

/**
 * used to modify session flags.
 */

void
brasero_burn_session_set_flags (BraseroBurnSession *self,
			        BraseroBurnFlag flags)
{
	BraseroBurnSessionPrivate *priv;

	g_return_if_fail (BRASERO_IS_BURN_SESSION (self));

	priv = BRASERO_BURN_SESSION_PRIVATE (self);
	priv->settings->flags = flags;
}

void
brasero_burn_session_add_flag (BraseroBurnSession *self,
			       BraseroBurnFlag flag)
{
	BraseroBurnSessionPrivate *priv;

	g_return_if_fail (BRASERO_IS_BURN_SESSION (self));

	priv = BRASERO_BURN_SESSION_PRIVATE (self);
	priv->settings->flags |= flag;
}

void
brasero_burn_session_remove_flag (BraseroBurnSession *self,
				  BraseroBurnFlag flag)
{
	BraseroBurnSessionPrivate *priv;

	g_return_if_fail (BRASERO_IS_BURN_SESSION (self));

	priv = BRASERO_BURN_SESSION_PRIVATE (self);
	priv->settings->flags &= ~flag;
}

BraseroBurnFlag
brasero_burn_session_get_flags (BraseroBurnSession *self)
{
	BraseroBurnSessionPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (self), BRASERO_BURN_ERR);

	priv = BRASERO_BURN_SESSION_PRIVATE (self);
	return priv->settings->flags;
}

/**
 * Used to set the label or the title of an album. 
 */
 
void
brasero_burn_session_set_label (BraseroBurnSession *self,
				const gchar *label)
{
	BraseroBurnSessionPrivate *priv;

	g_return_if_fail (BRASERO_IS_BURN_SESSION (self));

	priv = BRASERO_BURN_SESSION_PRIVATE (self);
	if (priv->settings->label)
		g_free (priv->settings->label);

	priv->settings->label = NULL;

	if (label)
		priv->settings->label = g_strdup (label);
}

const gchar *
brasero_burn_session_get_label (BraseroBurnSession *self)
{
	BraseroBurnSessionPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (self), NULL);

	priv = BRASERO_BURN_SESSION_PRIVATE (self);
	return priv->settings->label;
}

/**
 * Used to save and restore settings/sources
 */

void
brasero_burn_session_push_settings (BraseroBurnSession *self)
{
	BraseroSessionSetting *settings;
	BraseroBurnSessionPrivate *priv;

	g_return_if_fail (BRASERO_IS_BURN_SESSION (self));

	priv = BRASERO_BURN_SESSION_PRIVATE (self);

	/* NOTE: don't clean the settings so no need to issue a signal */
	settings = g_new0 (BraseroSessionSetting, 1);
	brasero_session_settings_copy (settings, priv->settings);
	priv->pile_settings = g_slist_prepend (priv->pile_settings, settings);
}

void
brasero_burn_session_pop_settings (BraseroBurnSession *self)
{
	BraseroSessionSetting *settings;
	BraseroBurnSessionPrivate *priv;

	g_return_if_fail (BRASERO_IS_BURN_SESSION (self));

	priv = BRASERO_BURN_SESSION_PRIVATE (self);

	if (priv->dest_added_sig) {
		g_signal_handler_disconnect (priv->settings->burner,
					     priv->dest_added_sig);
		priv->dest_added_sig = 0;
	}

	if (priv->dest_removed_sig) {
		g_signal_handler_disconnect (priv->settings->burner,
					     priv->dest_removed_sig);
		priv->dest_removed_sig = 0;	
	}

	brasero_session_settings_clean (priv->settings);

	if (!priv->pile_settings)
		return;

	settings = priv->pile_settings->data;
	priv->pile_settings = g_slist_remove (priv->pile_settings, settings);
	brasero_session_settings_copy (priv->settings, settings);

	brasero_session_settings_free (settings);

	if (priv->settings->burner) {
		priv->dest_added_sig = g_signal_connect (priv->settings->burner,
							 "media-added",
							 G_CALLBACK (brasero_burn_session_dest_media_added),
							 self);
		priv->dest_removed_sig = g_signal_connect (priv->settings->burner,
							   "media-removed",
							   G_CALLBACK (brasero_burn_session_dest_media_removed),
							   self);
	}

	g_signal_emit (self,
		       brasero_burn_session_signals [OUTPUT_CHANGED_SIGNAL],
		       0);
}

void
brasero_burn_session_push_tracks (BraseroBurnSession *self)
{
	BraseroBurnSessionPrivate *priv;

	g_return_if_fail (BRASERO_IS_BURN_SESSION (self));

	priv = BRASERO_BURN_SESSION_PRIVATE (self);

	brasero_burn_session_stop_src_drive_monitoring (self);

	priv->pile_tracks = g_slist_prepend (priv->pile_tracks,
					     priv->tracks);
	priv->tracks = NULL;

	g_signal_emit (self,
		       brasero_burn_session_signals [INPUT_CHANGED_SIGNAL],
		       0);
}

void
brasero_burn_session_pop_tracks (BraseroBurnSession *self)
{
	GSList *sources;
	BraseroBurnSessionPrivate *priv;

	g_return_if_fail (BRASERO_IS_BURN_SESSION (self));

	priv = BRASERO_BURN_SESSION_PRIVATE (self);

	if (priv->tracks) {
		brasero_burn_session_stop_src_drive_monitoring (self);

		g_slist_foreach (priv->tracks, (GFunc) brasero_track_unref, NULL);
		g_slist_free (priv->tracks);
		priv->tracks = NULL;

		if (!priv->pile_tracks) {
			g_signal_emit (self,
				       brasero_burn_session_signals [INPUT_CHANGED_SIGNAL],
				       0);
			return;
		}
	}

	if (!priv->pile_tracks)
		return;

	sources = priv->pile_tracks->data;
	priv->pile_tracks = g_slist_remove (priv->pile_tracks, sources);
	priv->tracks = sources;

	brasero_burn_session_start_src_drive_monitoring (self);

	g_signal_emit (self,
		       brasero_burn_session_signals [INPUT_CHANGED_SIGNAL],
		       0);
}

/**
 *
 */

gboolean
brasero_burn_session_is_dest_file (BraseroBurnSession *self)
{
	BraseroBurnSessionPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (self), FALSE);

	priv = BRASERO_BURN_SESSION_PRIVATE (self);
	return BRASERO_BURN_SESSION_WRITE_TO_FILE (priv);
}

BraseroMedia
brasero_burn_session_get_dest_media (BraseroBurnSession *self)
{
	BraseroBurnSessionPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (self), BRASERO_MEDIUM_NONE);

	priv = BRASERO_BURN_SESSION_PRIVATE (self);
	if (BRASERO_BURN_SESSION_WRITE_TO_FILE (priv))
		return BRASERO_MEDIUM_FILE;

	return NCB_MEDIA_GET_STATUS (priv->settings->burner);
}

NautilusBurnDrive *
brasero_burn_session_get_src_drive (BraseroBurnSession *self)
{
	BraseroTrack *track;
	BraseroBurnSessionPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (self), NULL);

	priv = BRASERO_BURN_SESSION_PRIVATE (self);

	/* to be able to burn to a DVD we must:
	 * - have only one track
	 * - not have any audio track */

	if (!priv->tracks)
		return NULL;

	if (g_slist_length (priv->tracks) != 1)
		return NULL;

	track = priv->tracks->data;
	if (brasero_track_get_type (track, NULL) != BRASERO_TRACK_TYPE_DISC)
		return NULL;

	return brasero_track_get_drive_source (track);
}

gboolean
brasero_burn_session_same_src_dest_drive (BraseroBurnSession *self)
{
	BraseroTrack *track;
	NautilusBurnDrive *drive;
	BraseroBurnSessionPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (self), FALSE);

	priv = BRASERO_BURN_SESSION_PRIVATE (self);

	/* to be able to burn to a DVD we must:
	 * - have only one track
	 * - not have any audio track 
	 */

	if (!priv->tracks)
		return FALSE;

	if (g_slist_length (priv->tracks) > 1)
		return FALSE;

	track = priv->tracks->data;
	if (brasero_track_get_type (track, NULL) != BRASERO_TRACK_TYPE_DISC)
		return FALSE;

	drive = brasero_track_get_drive_source (track);
	if (!drive)
		return FALSE;

	return nautilus_burn_drive_equal (priv->settings->burner, drive);
}


/**
 *
 */

void
brasero_burn_session_add_wrong_checksum (BraseroBurnSession *self,
					 const gchar *path)
{
	BraseroBurnSessionPrivate *priv;

	priv = BRASERO_BURN_SESSION_PRIVATE (self);
	priv->wrong_checksums = g_slist_prepend (priv->wrong_checksums, g_strdup (path));
}

GSList *
brasero_burn_session_get_wrong_checksums (BraseroBurnSession *self)
{
	BraseroBurnSessionPrivate *priv;
	GSList *retval;

	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (self), NULL);

	priv = BRASERO_BURN_SESSION_PRIVATE (self);

	/* reset our list so it will return only the new ones next time */
	retval = priv->wrong_checksums;
	priv->wrong_checksums = NULL;

	return retval;
}

/****************************** this part is for log ***************************/
void
brasero_burn_session_logv (BraseroBurnSession *self,
			   const gchar *format,
			   va_list arg_list)
{
	gchar *message;
	gchar *offending;
	BraseroBurnSessionPrivate *priv;

	g_return_if_fail (BRASERO_IS_BURN_SESSION (self));

	priv = BRASERO_BURN_SESSION_PRIVATE (self);

	if (!format)
		return;

	if (!priv->session)
		return;

	if (arg_list)
		message = g_strdup_vprintf (format, arg_list);
	else
		message = g_strdup (format);

	/* we also need to validate the messages to be in UTF-8 */
	if (!g_utf8_validate (message, -1, (const gchar**) &offending))
		*offending = '\0';

	if (fwrite (message, strlen (message), 1, priv->session) != 1)
		g_warning ("Some log data couldn't be written: %s\n", message);

	g_free (message);

	fwrite ("\n", 1, 1, priv->session);
}

void
brasero_burn_session_log (BraseroBurnSession *self,
			  const gchar *format,
			  ...)
{
	va_list args;
	BraseroBurnSessionPrivate *priv;

	g_return_if_fail (BRASERO_IS_BURN_SESSION (self));

	priv = BRASERO_BURN_SESSION_PRIVATE (self);

	va_start (args, format);
	brasero_burn_session_logv (self, format, args);
	va_end (args);
}

void
brasero_burn_session_set_log_path (BraseroBurnSession *self,
				   const gchar *session_path)
{
	BraseroBurnSessionPrivate *priv;

	g_return_if_fail (BRASERO_IS_BURN_SESSION (self));

	priv = BRASERO_BURN_SESSION_PRIVATE (self);
	if (priv->session_path) {
		g_free (priv->session_path);
		priv->session_path = NULL;
	}

	if (session_path)
		priv->session_path = g_strdup (session_path);
}

const gchar *
brasero_burn_session_get_log_path (BraseroBurnSession *self)
{
	BraseroBurnSessionPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (self), NULL);

	priv = BRASERO_BURN_SESSION_PRIVATE (self);
	return priv->session_path;
}

gboolean
brasero_burn_session_start (BraseroBurnSession *self)
{
	BraseroTrackType type;
	BraseroBurnSessionPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (self), FALSE);

	priv = BRASERO_BURN_SESSION_PRIVATE (self);

	if (!priv->session_path) {
		int fd;

		priv->session_path = g_build_path (G_DIR_SEPARATOR_S,
						   g_get_tmp_dir (),
						   BRASERO_BURN_TMP_FILE_NAME,
						   NULL);

		fd = g_mkstemp (priv->session_path);
		priv->session = fdopen (fd, "w");
	}
	else
		priv->session = fopen (priv->session_path, "w");

	if (!priv->session) {
		g_warning ("Impossible to open a session file\n");
		return FALSE;
	}


	BRASERO_BURN_LOG ("Session starting:");

	brasero_burn_session_get_input_type (self, &type);
	BRASERO_BURN_LOG_TYPE (&type, "Input\t=");

	BRASERO_BURN_LOG_FLAGS (priv->settings->flags, "flags\t=");

	if (!brasero_burn_session_is_dest_file (self)) {
		BRASERO_BURN_LOG_DISC_TYPE (NCB_MEDIA_GET_STATUS (priv->settings->burner), "media type\t=");
		BRASERO_BURN_LOG ("speed\t= %i", priv->settings->rate);
		BRASERO_BURN_LOG ("number of copies\t= %i", priv->settings->num_copies);
	}
	else {
		type.type = BRASERO_TRACK_TYPE_IMAGE;
		type.subtype.img_format = brasero_burn_session_get_output_format (self);
		BRASERO_BURN_LOG_TYPE (&type, "output format\t=");
	}

	return TRUE;
}

void
brasero_burn_session_stop (BraseroBurnSession *self)
{
	BraseroBurnSessionPrivate *priv;

	g_return_if_fail (BRASERO_IS_BURN_SESSION (self));

	priv = BRASERO_BURN_SESSION_PRIVATE (self);
	if (priv->session) {
		fclose (priv->session);
		priv->session = NULL;
	}
}

/**
 * Misc
 */

gchar *
brasero_burn_session_get_config_key (BraseroBurnSession *self,
				     const gchar *property)
{
	NautilusBurnDrive *drive;
	gchar *display_name;
	gchar *key = NULL;
	gchar *disc_type;

	drive = brasero_burn_session_get_burner (self);
	if (!drive)
		return NULL;

	if (NCB_MEDIA_GET_STATUS (drive) == BRASERO_MEDIUM_NONE)
		return NULL;
	
	/* make sure display_name doesn't contain any forbidden characters */
	display_name = nautilus_burn_drive_get_name_for_display (drive);
	g_strdelimit (display_name, " +()", '_');

	disc_type = g_strdup (NCB_MEDIA_GET_TYPE_STRING (drive));
	if (!disc_type) {
		g_free (display_name);
		return NULL;
	}

	g_strdelimit (disc_type, " +()", '_');

	switch (brasero_burn_session_get_input_type (self, NULL)) {
	case BRASERO_TRACK_TYPE_NONE:
		key = g_strdup_printf ("%s/%s/none_%s/%s",
				       BRASERO_DRIVE_PROPERTIES_KEY,
				       display_name,
				       disc_type,
				       property);
		break;
	case BRASERO_TRACK_TYPE_DISC:
		key = g_strdup_printf ("%s/%s/disc_%s/%s",
				       BRASERO_DRIVE_PROPERTIES_KEY,
				       display_name,
				       disc_type,
				       property);
		break;

	case BRASERO_TRACK_TYPE_DATA:
		key = g_strdup_printf ("%s/%s/data_%s/%s",
				       BRASERO_DRIVE_PROPERTIES_KEY,
				       display_name,
				       disc_type,
				       property);
		break;

	case BRASERO_TRACK_TYPE_IMAGE:
		key = g_strdup_printf ("%s/%s/image_%s/%s",
				       BRASERO_DRIVE_PROPERTIES_KEY,
				       display_name,
				       disc_type,
				       property);
		break;

	case BRASERO_TRACK_TYPE_AUDIO:
		key = g_strdup_printf ("%s/%s/audio_%s/%s",
				       BRASERO_DRIVE_PROPERTIES_KEY,
				       display_name,
				       disc_type,
				       property);
		break;
	default:
		break;
	}

	g_free (display_name);
	g_free (disc_type);
	return key;
}

/**
 *
 */

static void
brasero_burn_session_track_list_free (GSList *list)
{
	g_slist_foreach (list, (GFunc) brasero_track_unref, NULL);
	g_slist_free (list);
}

static void
brasero_burn_session_finalize (GObject *object)
{
	BraseroBurnSessionPrivate *priv;
	GSList *iter;

	priv = BRASERO_BURN_SESSION_PRIVATE (object);

	if (priv->dest_added_sig) {
		g_signal_handler_disconnect (priv->settings->burner,
					     priv->dest_added_sig);
		priv->dest_added_sig = 0;
	}

	if (priv->dest_removed_sig) {
		g_signal_handler_disconnect (priv->settings->burner,
					     priv->dest_removed_sig);
		priv->dest_removed_sig = 0;	
	}

	brasero_burn_session_stop_src_drive_monitoring (BRASERO_BURN_SESSION (object));

	if (priv->pile_tracks) {
		g_slist_foreach (priv->pile_tracks,
				(GFunc) brasero_burn_session_track_list_free,
				NULL);

		g_slist_free (priv->pile_tracks);
		priv->pile_tracks = NULL;
	}

	if (priv->tracks) {
		g_slist_foreach (priv->tracks,
				 (GFunc) brasero_track_unref,
				 NULL);
		g_slist_free (priv->tracks);
		priv->tracks = NULL;
	}

	if (priv->pile_settings) {
		g_slist_foreach (priv->pile_settings,
				(GFunc) brasero_session_settings_free,
				NULL);
		g_slist_free (priv->pile_settings);
		priv->pile_settings = NULL;
	}

	/* clean tmpfiles */
	for (iter = priv->tmpfiles; iter; iter = iter->next) {
		gchar *tmpfile;

		tmpfile = iter->data;

		g_remove (tmpfile);
		g_free (tmpfile);
	}
	g_slist_free (priv->tmpfiles);

	if (priv->session) {
		fclose (priv->session);
		priv->session = NULL;
	}

	if (priv->session_path) {
		g_remove (priv->session_path);
		g_free (priv->session_path);
		priv->session_path = NULL;
	}

	if (priv->wrong_checksums) {
		g_slist_foreach (priv->wrong_checksums, (GFunc) g_free, NULL);
		g_slist_free (priv->wrong_checksums);
		priv->wrong_checksums = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_burn_session_init (BraseroBurnSession *obj)
{ }

static void
brasero_burn_session_class_init (BraseroBurnSessionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroBurnSessionPrivate));

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_burn_session_finalize;

	/* This is to delay the setting of track source until we know all settings */
	brasero_burn_session_signals [OUTPUT_CHANGED_SIGNAL] =
	    g_signal_new ("output_changed",
			  BRASERO_TYPE_BURN_SESSION,
			  G_SIGNAL_RUN_LAST|G_SIGNAL_ACTION|G_SIGNAL_NO_RECURSE,
			  G_STRUCT_OFFSET (BraseroBurnSessionClass, output_changed),
			  NULL,
			  NULL,
			  g_cclosure_marshal_VOID__VOID,
			  G_TYPE_NONE,
			  0);

	brasero_burn_session_signals [INPUT_CHANGED_SIGNAL] =
	    g_signal_new ("input_changed",
			  BRASERO_TYPE_BURN_SESSION,
			  G_SIGNAL_RUN_LAST|G_SIGNAL_ACTION|G_SIGNAL_NO_RECURSE,
			  G_STRUCT_OFFSET (BraseroBurnSessionClass, input_changed),
			  NULL,
			  NULL,
			  g_cclosure_marshal_VOID__VOID,
			  G_TYPE_NONE,
			  0);
}

BraseroBurnSession *
brasero_burn_session_new ()
{
	BraseroBurnSession *obj;
	
	obj = BRASERO_BURN_SESSION (g_object_new (BRASERO_TYPE_BURN_SESSION, NULL));
	
	return obj;
}
