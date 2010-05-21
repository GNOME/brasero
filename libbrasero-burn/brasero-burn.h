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

#ifndef BURN_H
#define BURN_H

#include <glib.h>
#include <glib-object.h>

#include <brasero-error.h>
#include <brasero-track.h>
#include <brasero-session.h>

#include <brasero-medium.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_BURN         (brasero_burn_get_type ())
#define BRASERO_BURN(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_BURN, BraseroBurn))
#define BRASERO_BURN_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_BURN, BraseroBurnClass))
#define BRASERO_IS_BURN(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_BURN))
#define BRASERO_IS_BURN_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_BURN))
#define BRASERO_BURN_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_BURN, BraseroBurnClass))

typedef struct {
	GObject parent;
} BraseroBurn;

typedef struct {
	GObjectClass parent_class;

	/* signals */
	BraseroBurnResult		(*insert_media_request)		(BraseroBurn *obj,
									 BraseroDrive *drive,
									 BraseroBurnError error,
									 BraseroMedia required_media);

	BraseroBurnResult		(*eject_failure)		(BraseroBurn *obj,
							                 BraseroDrive *drive);

	BraseroBurnResult		(*blank_failure)		(BraseroBurn *obj);

	BraseroBurnResult		(*location_request)		(BraseroBurn *obj,
									 GError *error,
									 gboolean is_temporary);

	BraseroBurnResult		(*ask_disable_joliet)		(BraseroBurn *obj);

	BraseroBurnResult		(*warn_data_loss)		(BraseroBurn *obj);
	BraseroBurnResult		(*warn_previous_session_loss)	(BraseroBurn *obj);
	BraseroBurnResult		(*warn_audio_to_appendable)	(BraseroBurn *obj);
	BraseroBurnResult		(*warn_rewritable)		(BraseroBurn *obj);

	BraseroBurnResult		(*dummy_success)		(BraseroBurn *obj);

	void				(*progress_changed)		(BraseroBurn *obj,
									 gdouble overall_progress,
									 gdouble action_progress,
									 glong time_remaining);
	void				(*action_changed)		(BraseroBurn *obj,
									 BraseroBurnAction action);

	BraseroBurnResult		(*install_missing)		(BraseroBurn *obj,
									 BraseroPluginErrorType error,
									 const gchar *detail);
} BraseroBurnClass;

GType brasero_burn_get_type (void);
BraseroBurn *brasero_burn_new (void);

BraseroBurnResult 
brasero_burn_record (BraseroBurn *burn,
		     BraseroBurnSession *session,
		     GError **error);

BraseroBurnResult
brasero_burn_check (BraseroBurn *burn,
		    BraseroBurnSession *session,
		    GError **error);

BraseroBurnResult
brasero_burn_blank (BraseroBurn *burn,
		    BraseroBurnSession *session,
		    GError **error);

BraseroBurnResult
brasero_burn_cancel (BraseroBurn *burn,
		     gboolean protect);

BraseroBurnResult
brasero_burn_status (BraseroBurn *burn,
		     BraseroMedia *media,
		     goffset *isosize,
		     goffset *written,
		     guint64 *rate);

void
brasero_burn_get_action_string (BraseroBurn *burn,
				BraseroBurnAction action,
				gchar **string);

G_END_DECLS

#endif /* BURN_H */
