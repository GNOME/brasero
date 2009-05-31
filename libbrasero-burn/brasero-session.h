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

#ifndef BURN_SESSION_H
#define BURN_SESSION_H

#include <glib.h>
#include <glib-object.h>

#include <brasero-drive.h>

#include <brasero-error.h>
#include <brasero-status.h>
#include <brasero-track.h>


G_BEGIN_DECLS

#define BRASERO_TYPE_BURN_SESSION         (brasero_burn_session_get_type ())
#define BRASERO_BURN_SESSION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_BURN_SESSION, BraseroBurnSession))
#define BRASERO_BURN_SESSION_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_BURN_SESSION, BraseroBurnSessionClass))
#define BRASERO_IS_BURN_SESSION(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_BURN_SESSION))
#define BRASERO_IS_BURN_SESSION_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_BURN_SESSION))
#define BRASERO_BURN_SESSION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_BURN_SESSION, BraseroBurnSessionClass))

typedef struct _BraseroBurnSession BraseroBurnSession;
typedef struct _BraseroBurnSessionClass BraseroBurnSessionClass;

struct _BraseroBurnSession {
	GObject parent;
};

struct _BraseroBurnSessionClass {
	GObjectClass parent_class;

	/**
	 * GObject signals could be used to warned of individual property
	 * changes but since changing one property could change others
	 * it's better to have one global signal and dialogs asking for
	 * the session properties they are interested in.
	 */
	void			(*input_changed)	(BraseroBurnSession *session);
	void			(*output_changed)	(BraseroBurnSession *session,
							 BraseroMedium *medium);
};

GType brasero_burn_session_get_type ();

BraseroBurnSession *brasero_burn_session_new ();


/**
 * Used to manage tracks for input
 */

BraseroBurnResult
brasero_burn_session_add_track (BraseroBurnSession *session,
				BraseroTrack *track);

GSList *
brasero_burn_session_get_tracks (BraseroBurnSession *session);

BraseroBurnResult
brasero_burn_session_get_status (BraseroBurnSession *session,
				 BraseroStatus *status);

BraseroBurnResult
brasero_burn_session_get_size (BraseroBurnSession *session,
			       goffset *blocks,
			       goffset *bytes);

BraseroBurnResult
brasero_burn_session_get_input_type (BraseroBurnSession *session,
				     BraseroTrackType *type);

/**
 * This is to set additional arbitrary information
 */

BraseroBurnResult
brasero_burn_session_tag_lookup (BraseroBurnSession *session,
				 const gchar *tag,
				 GValue **value);

BraseroBurnResult
brasero_burn_session_tag_add (BraseroBurnSession *session,
			      const gchar *tag,
			      GValue *value);

BraseroBurnResult
brasero_burn_session_tag_remove (BraseroBurnSession *session,
				 const gchar *tag);

/**
 * Destination 
 */

BraseroDrive *
brasero_burn_session_get_burner (BraseroBurnSession *session);

void
brasero_burn_session_set_burner (BraseroBurnSession *session,
				 BraseroDrive *burner);

BraseroBurnResult
brasero_burn_session_set_image_output_full (BraseroBurnSession *session,
					    BraseroImageFormat format,
					    const gchar *image,
					    const gchar *toc);

BraseroBurnResult
brasero_burn_session_get_output (BraseroBurnSession *session,
				 gchar **image,
				 gchar **toc,
				 GError **error);

BraseroImageFormat
brasero_burn_session_get_output_format (BraseroBurnSession *session);


/**
 * Session flags
 */

void
brasero_burn_session_set_flags (BraseroBurnSession *session,
			        BraseroBurnFlag flag);

void
brasero_burn_session_add_flag (BraseroBurnSession *session,
			       BraseroBurnFlag flag);

void
brasero_burn_session_remove_flag (BraseroBurnSession *session,
				  BraseroBurnFlag flag);

BraseroBurnFlag
brasero_burn_session_get_flags (BraseroBurnSession *session);


/**
 * Used to deal with the temporary files (mostly used by plugins)
 */

BraseroBurnResult
brasero_burn_session_set_tmpdir (BraseroBurnSession *session,
				 const gchar *path);
const gchar *
brasero_burn_session_get_tmpdir (BraseroBurnSession *session);

/**
 * Allow to save a whole session settings/source and restore it later.
 * (mostly used internally)
 */

void
brasero_burn_session_push_settings (BraseroBurnSession *session);
void
brasero_burn_session_pop_settings (BraseroBurnSession *session);

void
brasero_burn_session_push_tracks (BraseroBurnSession *session);
void
brasero_burn_session_pop_tracks (BraseroBurnSession *session);

/**
 * Test the supported or compulsory flags for a given session
 */

BraseroBurnResult
brasero_burn_session_get_burn_flags (BraseroBurnSession *session,
				     BraseroBurnFlag *supported,
				     BraseroBurnFlag *compulsory);

BraseroBurnResult
brasero_burn_session_get_blank_flags (BraseroBurnSession *session,
				      BraseroBurnFlag *supported,
				      BraseroBurnFlag *compulsory);

/**
 * Used to test the possibilities offered for a given session
 */

BraseroBurnResult
brasero_burn_session_can_blank (BraseroBurnSession *session);

BraseroBurnResult
brasero_burn_session_can_burn (BraseroBurnSession *session,
			       gboolean use_flags);

BraseroBurnResult
brasero_burn_session_input_supported (BraseroBurnSession *session,
				      BraseroTrackType *input,
				      gboolean use_flags);

BraseroBurnResult
brasero_burn_session_output_supported (BraseroBurnSession *session,
				       BraseroTrackType *output);

BraseroMedia
brasero_burn_session_get_required_media_type (BraseroBurnSession *session);

BraseroImageFormat
brasero_burn_session_get_default_output_format (BraseroBurnSession *session);

/**
 * This is to log a session
 */

const gchar *
brasero_burn_session_get_log_path (BraseroBurnSession *session);

void
brasero_burn_session_set_log_path (BraseroBurnSession *session,
				   const gchar *session_path);
gboolean
brasero_burn_session_start (BraseroBurnSession *session);

void
brasero_burn_session_stop (BraseroBurnSession *session);

void
brasero_burn_session_logv (BraseroBurnSession *session,
			   const gchar *format,
			   va_list arg_list);
void
brasero_burn_session_log (BraseroBurnSession *session,
			  const gchar *format,
			  ...);


const gchar *
brasero_burn_session_get_label (BraseroBurnSession *session);

void
brasero_burn_session_set_label (BraseroBurnSession *session,
				const gchar *label);

BraseroBurnResult
brasero_burn_session_set_rate (BraseroBurnSession *session,
			       guint64 rate);

guint64
brasero_burn_session_get_rate (BraseroBurnSession *session);

G_END_DECLS

#endif /* BURN_SESSION_H */
