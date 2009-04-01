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


#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <gtk/gtk.h>

#include "burn-basics.h"
#include "brasero-session.h"
#include "brasero-medium.h"
#include "brasero-drive.h"
#include "brasero-session-cfg.h"
#include "brasero-disc-copy-dialog.h"
#include "brasero-dest-selection.h"
#include "brasero-src-selection.h"
#include "brasero-burn-options.h"

G_DEFINE_TYPE (BraseroDiscCopyDialog, brasero_disc_copy_dialog, BRASERO_TYPE_BURN_OPTIONS);

struct BraseroDiscCopyDialogPrivate {
	GtkWidget *source;
};
typedef struct BraseroDiscCopyDialogPrivate BraseroDiscCopyDialogPrivate;

#define BRASERO_DISC_COPY_DIALOG_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_DISC_COPY_DIALOG, BraseroDiscCopyDialogPrivate))

static GObjectClass *parent_class = NULL;

static void
brasero_disc_copy_dialog_set_session (BraseroDiscCopyDialog *obj)
{
	gchar *title_str;
	BraseroBurnSession *session;
	BraseroDiscCopyDialogPrivate *priv;

	priv = BRASERO_DISC_COPY_DIALOG_PRIVATE (obj);

	brasero_burn_options_add_burn_button (BRASERO_BURN_OPTIONS (obj),
					      _("_Copy"),
					      "media-optical-burn");

	/* take care of source media */
	session = brasero_burn_options_get_session (BRASERO_BURN_OPTIONS (obj));
	priv->source = brasero_src_selection_new (session);
	gtk_widget_show (priv->source);
	g_object_unref (session);

	title_str = g_strdup_printf ("<b>%s</b>", _("Select disc to copy"));
	brasero_burn_options_add_source (BRASERO_BURN_OPTIONS (obj),
					 title_str,
					 priv->source,
					 NULL);
	g_free (title_str);

	/* only show media with something to be read on them */
	brasero_medium_selection_show_media_type (BRASERO_MEDIUM_SELECTION (priv->source),
						  BRASERO_MEDIA_TYPE_AUDIO|
						  BRASERO_MEDIA_TYPE_DATA);

	/* This is a special case. When we're copying, someone may want to read
	 * and burn to the same drive so provided that the drive is a burner
	 * then show its contents. */
	brasero_burn_options_set_type_shown (BRASERO_BURN_OPTIONS (obj),
					     BRASERO_MEDIA_TYPE_ANY_IN_BURNER|
					     BRASERO_MEDIA_TYPE_FILE);
}

static void
brasero_disc_copy_dialog_init (BraseroDiscCopyDialog *obj)
{
	gtk_window_set_title (GTK_WINDOW (obj), _("CD/DVD Copy Options"));
	g_signal_connect (obj,
			  "notify::session",
			  G_CALLBACK (brasero_disc_copy_dialog_set_session),
			  NULL);
}

static void
brasero_disc_copy_dialog_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_disc_copy_dialog_class_init (BraseroDiscCopyDialogClass * klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroDiscCopyDialogPrivate));

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_disc_copy_dialog_finalize;
}

