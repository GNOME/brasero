/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2007-2008 <bonfire-app@wanadoo.fr>
 * 
 * brasero is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * brasero is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with brasero.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifndef _BRASERO_DATA_SESSION_H_
#define _BRASERO_DATA_SESSION_H_

#include <glib-object.h>

#include "burn-drive.h"
#include "brasero-data-project.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_DATA_SESSION             (brasero_data_session_get_type ())
#define BRASERO_DATA_SESSION(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_DATA_SESSION, BraseroDataSession))
#define BRASERO_DATA_SESSION_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_DATA_SESSION, BraseroDataSessionClass))
#define BRASERO_IS_DATA_SESSION(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_DATA_SESSION))
#define BRASERO_IS_DATA_SESSION_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_DATA_SESSION))
#define BRASERO_DATA_SESSION_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_DATA_SESSION, BraseroDataSessionClass))

typedef struct _BraseroDataSessionClass BraseroDataSessionClass;
typedef struct _BraseroDataSession BraseroDataSession;

struct _BraseroDataSessionClass
{
	BraseroDataProjectClass parent_class;
};

struct _BraseroDataSession
{
	BraseroDataProject parent_instance;
};

GType brasero_data_session_get_type (void) G_GNUC_CONST;

gboolean
brasero_data_session_add_last (BraseroDataSession *session,
			       GError **error);
BraseroDrive *
brasero_data_session_get_loaded_medium (BraseroDataSession *session);

void
brasero_data_session_set_drive (BraseroDataSession *session,
				BraseroDrive *drive);
void
brasero_data_session_remove_last (BraseroDataSession *session);

G_END_DECLS

#endif /* _BRASERO_DATA_SESSION_H_ */
