/***************************************************************************
 *            disc-copy-dialog.c
 *
 *  ven jui 15 16:02:10 2005
 *  Copyright  2005  Philippe Rouquier
 *  brasero-app@wanadoo.fr
 ***************************************************************************/

/*
 *  Brasero is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Brasero is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */


#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <gtk/gtkstock.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkwindow.h>

#include "burn-basics.h"
#include "burn-session.h"
#include "burn-caps.h"
#include "burn-medium.h"
#include "brasero-utils.h"
#include "burn-drive.h"
#include "brasero-session-cfg.h"
#include "brasero-disc-copy-dialog.h"
#include "brasero-dest-selection.h"
#include "brasero-src-selection.h"

G_DEFINE_TYPE (BraseroDiscCopyDialog, brasero_disc_copy_dialog, GTK_TYPE_DIALOG);

struct BraseroDiscCopyDialogPrivate {
	GtkWidget *selection;
	GtkWidget *source;

	GtkWidget *button;

	BraseroBurnSession *session;
};
typedef struct BraseroDiscCopyDialogPrivate BraseroDiscCopyDialogPrivate;

#define BRASERO_DISC_COPY_DIALOG_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_DISC_COPY_DIALOG, BraseroDiscCopyDialogPrivate))

static GObjectClass *parent_class = NULL;

BraseroBurnSession *
brasero_disc_copy_dialog_get_session (BraseroDiscCopyDialog *self)
{
	BraseroDiscCopyDialogPrivate *priv;

	priv = BRASERO_DISC_COPY_DIALOG_PRIVATE (self);
	g_object_ref (priv->session);
	return priv->session;
}

gboolean
brasero_disc_copy_dialog_set_drive (BraseroDiscCopyDialog *self,
				    BraseroDrive *drive)
{
	BraseroDiscCopyDialogPrivate *priv;

	priv = BRASERO_DISC_COPY_DIALOG_PRIVATE (self);
	return brasero_drive_selection_set_drive (BRASERO_DRIVE_SELECTION (priv->source), drive);
}

static void
brasero_disc_copy_dialog_set_burn_button_state (BraseroDiscCopyDialog *self,
						gboolean valid)
{
	BraseroDiscCopyDialogPrivate *priv;

	priv = BRASERO_DISC_COPY_DIALOG_PRIVATE (self);
	gtk_widget_set_sensitive (priv->button, valid);
}

static void
brasero_disc_copy_dialog_valid_media_cb (BraseroBurnSession *session,
					 gboolean valid,
					 BraseroDiscCopyDialog *self)
{
	brasero_disc_copy_dialog_set_burn_button_state (self, valid);
}

static void
brasero_disc_copy_dialog_init (BraseroDiscCopyDialog *obj)
{
	gchar *title_str;
	GtkWidget *button;
	BraseroDiscCopyDialogPrivate *priv;

	priv = BRASERO_DISC_COPY_DIALOG_PRIVATE (obj);

	gtk_dialog_set_has_separator (GTK_DIALOG (obj), FALSE);
	gtk_window_set_title (GTK_WINDOW (obj), _("CD/DVD Copy Options"));

	button = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
	gtk_widget_show (button);
	gtk_dialog_add_action_widget (GTK_DIALOG (obj),
				      button, 
				      GTK_RESPONSE_CANCEL);

	priv->button = brasero_utils_make_button (_("_Copy"),
						  NULL,
						  "media-optical-burn",
						  GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (priv->button);
	gtk_dialog_add_action_widget (GTK_DIALOG (obj),
				      priv->button,
				      GTK_RESPONSE_OK);

	/* create a session and add some default sane flags */
	priv->session = BRASERO_BURN_SESSION (brasero_session_cfg_new ());
	g_signal_connect (priv->session,
			  "is_valid",
			  G_CALLBACK (brasero_disc_copy_dialog_valid_media_cb),
			  obj);

	brasero_burn_session_add_flag (priv->session,
				       BRASERO_BURN_FLAG_EJECT|
				       BRASERO_BURN_FLAG_NOGRACE|
				       BRASERO_BURN_FLAG_BURNPROOF|
				       BRASERO_BURN_FLAG_CHECK_SIZE|
				       BRASERO_BURN_FLAG_DONT_CLEAN_OUTPUT|
				       BRASERO_BURN_FLAG_FAST_BLANK);

	/* take care of source media */
	priv->source = brasero_src_selection_new (priv->session);
	title_str = g_strdup_printf ("<b>%s</b>", _("Select disc to copy"));
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (obj)->vbox),
			    brasero_utils_pack_properties (title_str,
							   priv->source,
							   NULL),
			    FALSE,
			    FALSE,
			    0);
	g_free (title_str);

	/* destination drive */
	priv->selection = brasero_dest_selection_new (priv->session);

	title_str = g_strdup_printf ("<b>%s</b>", _("Select a disc to write to"));
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (obj)->vbox),
			    brasero_utils_pack_properties (title_str,
							   priv->selection,
							   NULL),
			    FALSE,
			    FALSE,
			    0);
	g_free (title_str);

	/* only show media with something to be read on them */
	brasero_drive_selection_set_type_shown (BRASERO_DRIVE_SELECTION (priv->source),
						BRASERO_MEDIA_TYPE_READABLE);

	/* This is a special case. When we're copying, someone may want to read
	 * and burn to the same drive so provided that the drive is a burner
	 * then show its contents. */
	brasero_drive_selection_set_type_shown (BRASERO_DRIVE_SELECTION (priv->selection),
						BRASERO_MEDIA_TYPE_ANY_IN_BURNER|
						BRASERO_MEDIA_TYPE_FILE);
}

static void
brasero_disc_copy_dialog_finalize (GObject *object)
{
	BraseroDiscCopyDialogPrivate *priv;

	priv = BRASERO_DISC_COPY_DIALOG_PRIVATE (object);
	if (priv->session) {
		g_object_unref (priv->session);
		priv->session = NULL;
	}

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

GtkWidget *
brasero_disc_copy_dialog_new ()
{
	BraseroDiscCopyDialog *obj;

	obj = BRASERO_DISC_COPY_DIALOG (g_object_new (BRASERO_TYPE_DISC_COPY_DIALOG,
						      NULL));

	return GTK_WIDGET (obj);
}
