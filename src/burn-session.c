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

#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gstdio.h>

#include "burn-basics.h"
#include "burn-session.h"
 
static void brasero_burn_session_class_init (BraseroBurnSessionClass *klass);
static void brasero_burn_session_init (BraseroBurnSession *sp);
static void brasero_burn_session_finalize (GObject *object);

struct _BraseroBurnSessionPrivate {
	FILE *session;
	gchar *session_path;
};

static GObjectClass *parent_class = NULL;

GType
brasero_burn_session_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroBurnSessionClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_burn_session_class_init,
			NULL,
			NULL,
			sizeof (BraseroBurnSession),
			0,
			(GInstanceInitFunc)brasero_burn_session_init,
		};

		type = g_type_register_static (G_TYPE_OBJECT, 
					       "BraseroBurnSession",
						&our_info,
					       0);
	}

	return type;
}

static void
brasero_burn_session_class_init (BraseroBurnSessionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_burn_session_finalize;
}

static void
brasero_burn_session_init (BraseroBurnSession *obj)
{
	obj->priv = g_new0 (BraseroBurnSessionPrivate, 1);
}

static void
brasero_burn_session_finalize (GObject *object)
{
	BraseroBurnSession *cobj;

	cobj = BRASERO_BURN_SESSION (object);

	if (cobj->priv->session) {
		fclose (cobj->priv->session);
		cobj->priv->session = NULL;
	}

	if (cobj->priv->session_path) {
		g_remove (cobj->priv->session_path);
		g_free (cobj->priv->session_path);
		cobj->priv->session_path = NULL;
	}

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

BraseroBurnSession *
brasero_burn_session_new ()
{
	BraseroBurnSession *obj;
	
	obj = BRASERO_BURN_SESSION (g_object_new (BRASERO_TYPE_BURN_SESSION, NULL));
	
	return obj;
}

void
brasero_burn_session_logv (BraseroBurnSession *session,
			   const gchar *format,
			   va_list arg_list)
{
	gchar *message;
	gchar *offending;

	if (!format)
		return;

	if (!session->priv->session)
		return;

	message = g_strdup_vprintf (format, arg_list);

	/* we also need to validate the messages to be in UTF-8 */
	if (!g_utf8_validate (message, -1, (const gchar**) &offending))
		*offending = '\0';

	if (fwrite (message, strlen (message), 1, session->priv->session) != 1)
		g_warning ("Some log data couldn't be written: %s\n", message);

	g_free (message);

	fwrite ("\n", 1, 1, session->priv->session);
}

void
brasero_burn_session_set_log_path (BraseroBurnSession *session,
				   const gchar *session_path)
{
	if (session->priv->session_path) {
		g_free (session->priv->session_path);
		session->priv->session_path = NULL;
	}

	if (session_path)
		session->priv->session_path = g_strdup (session_path);
}

const gchar *
brasero_burn_session_get_log_path (BraseroBurnSession *session)
{
	return session->priv->session_path;
}

gboolean
brasero_burn_session_start (BraseroBurnSession *session)
{
	if (!session->priv->session_path) {
		int fd;

		session->priv->session_path = g_build_path (G_DIR_SEPARATOR_S,
							    g_get_tmp_dir (),
							    BRASERO_BURN_TMP_FILE_NAME,
							    NULL);

		fd = g_mkstemp (session->priv->session_path);
		session->priv->session = fdopen (fd, "w");
	}
	else
		session->priv->session = fopen (session->priv->session_path, "w");

	if (!session->priv->session) {
		g_warning ("Impossible to open a session file\n");
		return FALSE;
	}

	return TRUE;
}

void
brasero_burn_session_stop (BraseroBurnSession *session)
{
	if (session->priv->session) {
		fclose (session->priv->session);
		session->priv->session = NULL;
	}
}
