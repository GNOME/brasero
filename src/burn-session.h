/***************************************************************************
 *            burn-session.h
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

#ifndef BURN_SESSION_H
#define BURN_SESSION_H

#include <glib.h>
#include <glib-object.h>

#include "burn-basics.h"
#include "burn-track.h"
#include "burn-drive.h"

G_BEGIN_DECLS

#define BRASERO_DRIVE_PROPERTIES_KEY		"/apps/brasero/drives"

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
	void	(*input_changed)		(BraseroBurnSession *session);

	void	(*output_changed)		(BraseroBurnSession *session);
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

void
brasero_burn_session_set_input_type (BraseroBurnSession *session,
				     BraseroTrackType *type);

BraseroTrackDataType
brasero_burn_session_get_input_type (BraseroBurnSession *session,
				     BraseroTrackType *type);

const gchar *
brasero_burn_session_get_label (BraseroBurnSession *session);

void
brasero_burn_session_set_label (BraseroBurnSession *session,
				const gchar *label);

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
 * 
 */

BraseroBurnResult
brasero_burn_session_set_rate (BraseroBurnSession *session,
			       guint64 rate);

guint64
brasero_burn_session_get_rate (BraseroBurnSession *session);

void
brasero_burn_session_set_num_copies (BraseroBurnSession *session,
				     guint copies);

guint
brasero_burn_session_get_num_copies (BraseroBurnSession *session);

BraseroDrive *
brasero_burn_session_get_burner (BraseroBurnSession *session);


/**
 * When outputting to an image file burner needs to be set to a drive with FILE type
 */

void
brasero_burn_session_set_burner (BraseroBurnSession *session,
				 BraseroDrive *burner);

BraseroBurnResult
brasero_burn_session_set_image_output (BraseroBurnSession *session,
				       BraseroImageFormat format,
				       const gchar *path);

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
 * Used to deal with the temporary files
 */

BraseroBurnResult
brasero_burn_session_set_tmpdir (BraseroBurnSession *session,
				 const gchar *path);
const gchar *
brasero_burn_session_get_tmpdir (BraseroBurnSession *session);

BraseroBurnResult
brasero_burn_session_get_tmp_image (BraseroBurnSession *session,
				    BraseroImageFormat format,
				    gchar **image,
				    gchar **toc,
				    GError **error);

BraseroBurnResult
brasero_burn_session_get_tmp_file (BraseroBurnSession *session,
				   const gchar *suffix,
				   gchar **path,
				   GError **error);

BraseroBurnResult
brasero_burn_session_get_tmp_dir (BraseroBurnSession *session,
				  gchar **path,
				  GError **error);

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
 * Allow to save a whole session settings/source and restore it later.
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
 * Some convenient functions
 */

gchar *
brasero_burn_session_get_config_key (BraseroBurnSession *session,
				     const gchar *property);

gboolean
brasero_burn_session_same_src_dest_drive (BraseroBurnSession *session);

BraseroMedia
brasero_burn_session_get_dest_media (BraseroBurnSession *session);

BraseroDrive *
brasero_burn_session_get_src_drive (BraseroBurnSession *session);

BraseroMedium *
brasero_burn_session_get_src_medium (BraseroBurnSession *session);

gboolean
brasero_burn_session_is_dest_file (BraseroBurnSession *session);


#define BRASERO_BURN_SESSION_EJECT(session)					\
(brasero_burn_session_get_flags ((session)) & BRASERO_BURN_FLAG_EJECT)

#define BRASERO_BURN_SESSION_CHECK_SIZE(session)				\
(brasero_burn_session_get_flags ((session)) & BRASERO_BURN_FLAG_CHECK_SIZE)

#define BRASERO_BURN_SESSION_NO_TMP_FILE(session)				\
(brasero_burn_session_get_flags ((session)) & BRASERO_BURN_FLAG_NO_TMP_FILES)

#define BRASERO_BURN_SESSION_OVERBURN(session)					\
(brasero_burn_session_get_flags ((session)) & BRASERO_BURN_FLAG_OVERBURN)

#define BRASERO_BURN_SESSION_APPEND(session)					\
(brasero_burn_session_get_flags ((session)) & (BRASERO_BURN_FLAG_APPEND|BRASERO_BURN_FLAG_MERGE))


/**
 * Used to report wrong checksum
 */

void
brasero_burn_session_add_wrong_checksum (BraseroBurnSession *session,
					 const gchar *path);

GSList *
brasero_burn_session_get_wrong_checksums (BraseroBurnSession *session);


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

G_END_DECLS

#endif /* BURN_SESSION_H */
