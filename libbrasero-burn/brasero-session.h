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

	/** Virtual functions **/
	BraseroBurnResult	(*set_output_image)	(BraseroBurnSession *session,
							 BraseroImageFormat format,
							 const gchar *image,
							 const gchar *toc);
	BraseroBurnResult	(*get_output_path)	(BraseroBurnSession *session,
							 gchar **image,
							 gchar **toc);
	BraseroImageFormat	(*get_output_format)	(BraseroBurnSession *session);

	/** Signals **/
	void			(*tag_changed)		(BraseroBurnSession *session,
					                 const gchar *tag);
	void			(*track_added)		(BraseroBurnSession *session,
							 BraseroTrack *track);
	void			(*track_removed)	(BraseroBurnSession *session,
							 BraseroTrack *track,
							 guint former_position);
	void			(*track_changed)	(BraseroBurnSession *session,
							 BraseroTrack *track);
	void			(*output_changed)	(BraseroBurnSession *session,
							 BraseroMedium *former_medium);
};

GType brasero_burn_session_get_type (void);

BraseroBurnSession *brasero_burn_session_new (void);


/**
 * Used to manage tracks for input
 */

BraseroBurnResult
brasero_burn_session_add_track (BraseroBurnSession *session,
				BraseroTrack *new_track,
				BraseroTrack *sibling);

BraseroBurnResult
brasero_burn_session_move_track (BraseroBurnSession *session,
				 BraseroTrack *track,
				 BraseroTrack *sibling);

BraseroBurnResult
brasero_burn_session_remove_track (BraseroBurnSession *session,
				   BraseroTrack *track);

GSList *
brasero_burn_session_get_tracks (BraseroBurnSession *session);

/**
 * Get some information about the session
 */

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

BraseroBurnResult
brasero_burn_session_tag_add_int (BraseroBurnSession *self,
                                  const gchar *tag,
                                  gint value);
gint
brasero_burn_session_tag_lookup_int (BraseroBurnSession *self,
                                     const gchar *tag);

/**
 * Destination 
 */
BraseroBurnResult
brasero_burn_session_get_output_type (BraseroBurnSession *self,
                                      BraseroTrackType *output);

BraseroDrive *
brasero_burn_session_get_burner (BraseroBurnSession *session);

void
brasero_burn_session_set_burner (BraseroBurnSession *session,
				 BraseroDrive *drive);

BraseroBurnResult
brasero_burn_session_set_image_output_full (BraseroBurnSession *session,
					    BraseroImageFormat format,
					    const gchar *image,
					    const gchar *toc);

BraseroBurnResult
brasero_burn_session_get_output (BraseroBurnSession *session,
				 gchar **image,
				 gchar **toc);

BraseroBurnResult
brasero_burn_session_set_image_output_format (BraseroBurnSession *self,
					    BraseroImageFormat format);

BraseroImageFormat
brasero_burn_session_get_output_format (BraseroBurnSession *session);

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

/**
 * Session flags
 */

void
brasero_burn_session_set_flags (BraseroBurnSession *session,
			        BraseroBurnFlag flags);

void
brasero_burn_session_add_flag (BraseroBurnSession *session,
			       BraseroBurnFlag flags);

void
brasero_burn_session_remove_flag (BraseroBurnSession *session,
				  BraseroBurnFlag flags);

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

void
brasero_burn_session_set_strict_support (BraseroBurnSession *session,
                                         gboolean strict_check);

gboolean
brasero_burn_session_get_strict_support (BraseroBurnSession *session);

BraseroBurnResult
brasero_burn_session_can_blank (BraseroBurnSession *session);

BraseroBurnResult
brasero_burn_session_can_burn (BraseroBurnSession *session,
                               gboolean check_flags);

typedef BraseroBurnResult	(* BraseroForeachPluginErrorCb)	(BraseroPluginErrorType type,
		                                                 const gchar *detail,
		                                                 gpointer user_data);

BraseroBurnResult
brasero_session_foreach_plugin_error (BraseroBurnSession *session,
                                      BraseroForeachPluginErrorCb callback,
                                      gpointer user_data);

BraseroBurnResult
brasero_burn_session_input_supported (BraseroBurnSession *session,
				      BraseroTrackType *input,
                                      gboolean check_flags);

BraseroBurnResult
brasero_burn_session_output_supported (BraseroBurnSession *session,
				       BraseroTrackType *output);

BraseroMedia
brasero_burn_session_get_required_media_type (BraseroBurnSession *session);

guint
brasero_burn_session_get_possible_output_formats (BraseroBurnSession *session,
						  BraseroImageFormat *formats);

BraseroImageFormat
brasero_burn_session_get_default_output_format (BraseroBurnSession *session);


G_END_DECLS

#endif /* BURN_SESSION_H */
