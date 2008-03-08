/***************************************************************************
 *            blank-dialog.c
 *
 *  mar jui 26 12:23:01 2005
 *  Copyright  2005  Philippe Rouquier
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

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtklabel.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkcheckbutton.h>

#include "burn-basics.h"
#include "burn-session.h"
#include "burn.h"
#include "burn-plugin-manager.h"
#include "brasero-utils.h"
#include "brasero-tool-dialog.h"
#include "brasero-blank-dialog.h"

G_DEFINE_TYPE (BraseroBlankDialog, brasero_blank_dialog, BRASERO_TYPE_TOOL_DIALOG);

struct BraseroBlankDialogPrivate {
	BraseroBurnSession *session;
	BraseroBurnCaps *caps;

	GtkWidget *fast;

	guint caps_sig;
	guint output_sig;

	guint fast_saved;
	guint dummy_saved;
};
typedef struct BraseroBlankDialogPrivate BraseroBlankDialogPrivate;

#define BRASERO_BLANK_DIALOG_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_BLANK_DIALOG, BraseroBlankDialogPrivate))

static BraseroToolDialogClass *parent_class = NULL;

static guint
brasero_blank_dialog_set_button (BraseroBurnSession *session,
				 guint saved,
				 GtkWidget *button,
				 BraseroBurnFlag flag,
				 BraseroBurnFlag supported,
				 BraseroBurnFlag compulsory)
{
	if (flag & supported) {
		if (compulsory & flag) {
			if (GTK_WIDGET_SENSITIVE (button))
				saved = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

			gtk_widget_set_sensitive (button, FALSE);
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

			brasero_burn_session_add_flag (session, flag);
		}
		else {
			if (!GTK_WIDGET_SENSITIVE (button)) {
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), saved);

				if (saved)
					brasero_burn_session_add_flag (session, flag);
				else
					brasero_burn_session_remove_flag (session, flag);
			}

			gtk_widget_set_sensitive (button, TRUE);
		}
	}
	else {
		if (GTK_WIDGET_SENSITIVE (button))
			saved = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

		gtk_widget_set_sensitive (button, FALSE);
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), FALSE);

		brasero_burn_session_remove_flag (session, flag);
	}

	return saved;
}

static void
brasero_blank_dialog_device_opts_setup (BraseroBlankDialog *self)
{
	BraseroBurnFlag supported;
	BraseroBurnFlag compulsory;
	BraseroBlankDialogPrivate *priv;

	priv = BRASERO_BLANK_DIALOG_PRIVATE (self);

	/* set the options */
	brasero_burn_caps_get_blanking_flags (priv->caps,
					      priv->session,
					      &supported,
					      &compulsory);

	priv->fast_saved = brasero_blank_dialog_set_button (priv->session,
							    priv->fast_saved,
							    priv->fast,
							    BRASERO_BURN_FLAG_FAST_BLANK,
							    supported,
							    compulsory);

	/* This must be done afterwards, once the session flags were updated */
	brasero_tool_dialog_set_valid (BRASERO_TOOL_DIALOG (self),
				       (brasero_burn_caps_can_blank (priv->caps, priv->session) == BRASERO_BURN_OK));
}

static void
brasero_blank_dialog_caps_changed (BraseroPluginManager *manager,
				   BraseroBlankDialog *dialog)
{
	brasero_blank_dialog_device_opts_setup (dialog);
}

static void
brasero_blank_dialog_output_changed (BraseroBurnSession *session,
				     BraseroBlankDialog *dialog)
{
	brasero_blank_dialog_device_opts_setup (dialog);
}

static void
brasero_blank_dialog_fast_toggled (GtkToggleButton *toggle,
				   BraseroBlankDialog *self)
{
	BraseroBlankDialogPrivate *priv;

	priv = BRASERO_BLANK_DIALOG_PRIVATE (self);
	if (gtk_toggle_button_get_active (toggle))
		brasero_burn_session_add_flag (priv->session, BRASERO_BURN_FLAG_FAST_BLANK);
	else
		brasero_burn_session_remove_flag (priv->session, BRASERO_BURN_FLAG_FAST_BLANK);
}

static void
brasero_blank_dialog_drive_changed (BraseroToolDialog *dialog,
				    BraseroMedium *medium)
{
	BraseroBlankDialogPrivate *priv;
	BraseroDrive *drive;

	priv = BRASERO_BLANK_DIALOG_PRIVATE (dialog);

	if (medium)
		drive = brasero_medium_get_drive (medium);
	else
		drive = NULL;

	brasero_burn_session_set_burner (priv->session, drive);
}

static gboolean
brasero_blank_dialog_activate (BraseroToolDialog *dialog,
			       BraseroMedium *medium)
{
	BraseroBlankDialogPrivate *priv;
	BraseroBlankDialog *self;
	BraseroBurnResult result;
	GError *error = NULL;
	BraseroBurn *burn;

	self = BRASERO_BLANK_DIALOG (dialog);
	priv = BRASERO_BLANK_DIALOG_PRIVATE (self);

	burn = brasero_tool_dialog_get_burn (dialog);
	result = brasero_burn_blank (burn,
				     priv->session,
				     &error);

	/* Tell the user the result of the operation */
	if (result == BRASERO_BURN_ERR || error) {
		GtkResponseType answer;
		GtkWidget *message;
		GtkWidget *button;

		message =  gtk_message_dialog_new (GTK_WINDOW (self),
						   GTK_DIALOG_DESTROY_WITH_PARENT|
						   GTK_DIALOG_MODAL,
						   GTK_MESSAGE_ERROR,
						   GTK_BUTTONS_CLOSE,
						   _("Error Blanking:"));

		gtk_window_set_title (GTK_WINDOW (self), _("Blanking finished"));

		button = brasero_utils_make_button (_("Blank _Again"),
						    NULL,
						    "media-optical-blank",
						    GTK_ICON_SIZE_BUTTON);
		gtk_widget_show (button);
		gtk_dialog_add_action_widget (GTK_DIALOG (message),
					      button,
					      GTK_RESPONSE_OK);

		if (error) {
			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
								  "%s.",
								  error->message);
			g_error_free (error);
		}
		else
			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
								  _("Unexpected error"));

		answer = gtk_dialog_run (GTK_DIALOG (message));
		gtk_widget_destroy (message);

		if (answer == GTK_RESPONSE_OK) {
			brasero_blank_dialog_device_opts_setup (self);
			return FALSE;
		}
	}
	else if (result == BRASERO_BURN_OK) {
		GtkResponseType answer;
		GtkWidget *message;
		GtkWidget *button;

		message = gtk_message_dialog_new (GTK_WINDOW (self),
						  GTK_DIALOG_DESTROY_WITH_PARENT|
						  GTK_DIALOG_MODAL,
						  GTK_MESSAGE_INFO,
						  GTK_BUTTONS_NONE,
						  _("The disc was successfully blanked:"));

		gtk_window_set_title (GTK_WINDOW (self), _("Blanking finished"));

		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
							  _("the disc is ready for use."));

		button = brasero_utils_make_button (_("Blank _Again"),
						    NULL,
						    "media-optical-blank",
						    GTK_ICON_SIZE_BUTTON);
		gtk_widget_show (button);
		gtk_dialog_add_action_widget (GTK_DIALOG (message),
					      button,
					      GTK_RESPONSE_OK);

		gtk_dialog_add_button (GTK_DIALOG (message),
				       GTK_STOCK_CLOSE,
				       GTK_RESPONSE_CLOSE);

		answer = gtk_dialog_run (GTK_DIALOG (message));
		gtk_widget_destroy (message);

		if (answer == GTK_RESPONSE_OK) {
			brasero_blank_dialog_device_opts_setup (self);
			return FALSE;
		}
	}
	else if (result == BRASERO_BURN_NOT_SUPPORTED) {
		g_warning ("operation not supported");
	}
	else if (result == BRASERO_BURN_NOT_READY) {
		g_warning ("operation not ready");
	}
	else if (result == BRASERO_BURN_NOT_RUNNING) {
		g_warning ("job not running");
	}
	else if (result == BRASERO_BURN_RUNNING) {
		g_warning ("job running");
	}

	return TRUE;
}

static void
brasero_blank_dialog_finalize (GObject *object)
{
	BraseroBlankDialogPrivate *priv;

	priv = BRASERO_BLANK_DIALOG_PRIVATE (object);

	if (priv->caps_sig) {
		BraseroPluginManager *manager;

		manager = brasero_plugin_manager_get_default ();
		g_signal_handler_disconnect (manager, priv->caps_sig);
		priv->caps_sig = 0;
	}

	if (priv->caps) {
		g_object_unref (priv->caps);
		priv->caps = NULL;
	}

	if (priv->output_sig) {
		g_signal_handler_disconnect (priv->session, priv->output_sig);
		priv->output_sig = 0;
	}

	if (priv->session) {
		g_object_unref (priv->session);
		priv->session = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_blank_dialog_class_init (BraseroBlankDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroToolDialogClass *tool_dialog_class = BRASERO_TOOL_DIALOG_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroBlankDialogPrivate));

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = brasero_blank_dialog_finalize;

	tool_dialog_class->activate = brasero_blank_dialog_activate;
	tool_dialog_class->drive_changed = brasero_blank_dialog_drive_changed;
}

static void
brasero_blank_dialog_init (BraseroBlankDialog *obj)
{
	BraseroBlankDialogPrivate *priv;
	BraseroPluginManager *manager;
	BraseroMedium *medium;
	BraseroDrive *drive;

	priv = BRASERO_BLANK_DIALOG_PRIVATE (obj);

	brasero_tool_dialog_set_button (BRASERO_TOOL_DIALOG (obj),
					_("_Blank"),
					NULL,
					"media-optical-blank");

	medium = brasero_tool_dialog_get_medium (BRASERO_TOOL_DIALOG (obj));
	drive = brasero_medium_get_drive (medium);

	priv->session = brasero_burn_session_new ();
	brasero_burn_session_set_flags (priv->session,
				        BRASERO_BURN_FLAG_EJECT|
				        BRASERO_BURN_FLAG_NOGRACE);
	brasero_burn_session_set_burner (priv->session, drive);

	if (medium)
		g_object_unref (medium);

	priv->output_sig = g_signal_connect (priv->session,
					     "output-changed",
					     G_CALLBACK (brasero_blank_dialog_output_changed),
					     obj);

	priv->caps = brasero_burn_caps_get_default ();
	manager = brasero_plugin_manager_get_default ();
	priv->caps_sig = g_signal_connect (manager,
					   "caps-changed",
					   G_CALLBACK (brasero_blank_dialog_caps_changed),
					   obj);

	priv->fast = gtk_check_button_new_with_mnemonic (_("_fast blanking"));
	gtk_widget_set_tooltip_text (priv->fast, _("Activate fast blanking by opposition to a longer thorough blanking"));
	g_signal_connect (priv->fast,
			  "clicked",
			  G_CALLBACK (brasero_blank_dialog_fast_toggled),
			  obj);

	brasero_tool_dialog_pack_options (BRASERO_TOOL_DIALOG (obj),
					  priv->fast,
					  NULL);

	brasero_blank_dialog_device_opts_setup (obj);

	/* if fast blank is supported check it by default */
	if (GTK_WIDGET_IS_SENSITIVE (priv->fast))
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (priv->fast), TRUE);
}

GtkWidget *
brasero_blank_dialog_new ()
{
	BraseroBlankDialog *obj;

	obj = BRASERO_BLANK_DIALOG (g_object_new (BRASERO_TYPE_BLANK_DIALOG,
						  "title", _("Disc blanking"),
						  NULL));
	return GTK_WIDGET (obj);
}
