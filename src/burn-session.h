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

G_BEGIN_DECLS

#define BRASERO_TYPE_BURN_SESSION         (brasero_burn_session_get_type ())
#define BRASERO_BURN_SESSION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_BURN_SESSION, BraseroBurnSession))
#define BRASERO_BURN_SESSION_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_BURN_SESSION, BraseroBurnSessionClass))
#define BRASERO_IS_BURN_SESSION(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_BURN_SESSION))
#define BRASERO_IS_BURN_SESSION_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_BURN_SESSION))
#define BRASERO_BURN_SESSION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_BURN_SESSION, BraseroBurnSessionClass))

typedef struct _BraseroBurnSession BraseroBurnSession;
typedef struct _BraseroBurnSessionPrivate BraseroBurnSessionPrivate;
typedef struct _BraseroBurnSessionClass BraseroBurnSessionClass;

struct _BraseroBurnSession {
	GObject parent;
	BraseroBurnSessionPrivate *priv;
};

struct _BraseroBurnSessionClass {
	GObjectClass parent_class;
};

GType brasero_burn_session_get_type ();
BraseroBurnSession *brasero_burn_session_new ();

void
brasero_burn_session_logv (BraseroBurnSession *session,
			   const gchar *format,
			   va_list arg_list);

void
brasero_burn_session_set_log_path (BraseroBurnSession *session,
				   const gchar *session_path);

const gchar *
brasero_burn_session_get_log_path (BraseroBurnSession *session);

gboolean
brasero_burn_session_start (BraseroBurnSession *session);

void
brasero_burn_session_stop (BraseroBurnSession *session);

G_END_DECLS

#endif /* BURN_SESSION_H */
