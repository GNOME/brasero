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

#ifndef BURN_CAPS_H
#define BURN_CAPS_H

#include <glib.h>
#include <glib-object.h>

#include "burn-basics.h"
#include "brasero-medium.h"
#include "brasero-session.h"
#include "burn-plugin.h"
#include "burn-task.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_BURNCAPS         (brasero_burn_caps_get_type ())
#define BRASERO_BURNCAPS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_BURNCAPS, BraseroBurnCaps))
#define BRASERO_BURNCAPS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_BURNCAPS, BraseroBurnCapsClass))
#define BRASERO_IS_BURNCAPS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_BURNCAPS))
#define BRASERO_IS_BURNCAPS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_BURNCAPS))
#define BRASERO_BURNCAPS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_BURNCAPS, BraseroBurnCapsClass))

typedef struct BraseroBurnCapsPrivate BraseroBurnCapsPrivate;

typedef struct {
	GObject parent;
	BraseroBurnCapsPrivate *priv;
} BraseroBurnCaps;

typedef struct {
	GObjectClass parent_class;
} BraseroBurnCapsClass;

GType brasero_burn_caps_get_type();

BraseroBurnCaps *brasero_burn_caps_get_default ();

/**
 * Returns a GSList * of BraseroTask * for a given session
 */

GSList *
brasero_burn_caps_new_task (BraseroBurnCaps *caps,
			    BraseroBurnSession *session,
			    GError **error);
BraseroTask *
brasero_burn_caps_new_blanking_task (BraseroBurnCaps *caps,
				     BraseroBurnSession *session,
				     GError **error);
BraseroTask *
brasero_burn_caps_new_checksuming_task (BraseroBurnCaps *caps,
					BraseroBurnSession *session,
					GError **error);


#endif /* BURN_CAPS_H */
