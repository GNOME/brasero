/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Brasero
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Brasero is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Brasero authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Brasero. This permission is above and beyond the permissions granted
 * by the GPL license by which Brasero is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Brasero is distributed in the hope that it will be useful,
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

#ifndef _BRASERO_MEDIUM_SELECTION_PRIV_H_
#define _BRASERO_MEDIUM_SELECTION_PRIV_H_

#include "brasero-medium-selection.h"

G_BEGIN_DECLS

typedef gboolean (*BraseroMediumSelectionFunc) (BraseroMedium *medium, gpointer callback_data);

guint
brasero_medium_selection_get_media_num (BraseroMediumSelection *selection);

void
brasero_medium_selection_foreach (BraseroMediumSelection *selection,
				  BraseroMediumSelectionFunc function,
				  gpointer callback_data);

void
brasero_medium_selection_update_media_string (BraseroMediumSelection *selection);

G_END_DECLS

#endif /* _BRASERO_MEDIUM_SELECTION_PRIV_H_ */
