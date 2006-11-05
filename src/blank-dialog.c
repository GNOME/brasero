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

#include <nautilus-burn-drive.h>

#include "burn-basics.h"
#include "burn.h"
#include "utils.h"
#include "brasero-tool-dialog.h"
#include "blank-dialog.h"

extern gint debug;

static void brasero_blank_dialog_class_init (BraseroBlankDialogClass *klass);
static void brasero_blank_dialog_init (BraseroBlankDialog *sp);
static void brasero_blank_dialog_finalize (GObject *object);

static gboolean brasero_blank_dialog_cancel (BraseroToolDialog *dialog);
static gboolean brasero_blank_dialog_activate (BraseroToolDialog *dialog,
					         NautilusBurnDrive *drive);
static void brasero_blank_dialog_media_changed (BraseroToolDialog *dialog,
						 NautilusBurnMediaType media);

static void brasero_blank_dialog_device_opts_setup (BraseroBlankDialog *dialog,
						    NautilusBurnMediaType type,
						    gboolean messages);

struct BraseroBlankDialogPrivate {
	BraseroBurn *burn;
	BraseroBurnCaps *caps;

	GtkWidget *fast_enabled;
	GtkWidget *dummy_toggle;
};

static GObjectClass *parent_class = NULL;

GType
brasero_blank_dialog_get_type ()
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroBlankDialogClass),
			NULL,
			NULL,
			(GClassInitFunc) brasero_blank_dialog_class_init,
			NULL,
			NULL,
			sizeof (BraseroBlankDialog),
			0,
			(GInstanceInitFunc) brasero_blank_dialog_init,
		};

		type = g_type_register_static (BRASERO_TYPE_TOOL_DIALOG,
					       "BraseroBlankDialog",
					       &our_info,
					       0);
	}

	return type;
}

static void
brasero_blank_dialog_class_init (BraseroBlankDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroToolDialogClass *tool_dialog_class = BRASERO_TOOL_DIALOG_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = brasero_blank_dialog_finalize;

	tool_dialog_class->activate = brasero_blank_dialog_activate;
	tool_dialog_class->media_changed = brasero_blank_dialog_media_changed;
	tool_dialog_class->cancel = brasero_blank_dialog_cancel;
}

static void
brasero_blank_dialog_init (BraseroBlankDialog *obj)
{
	obj->priv = g_new0 (BraseroBlankDialogPrivate, 1);

	obj->priv->caps = brasero_burn_caps_get_default ();

	obj->priv->fast_enabled = gtk_check_button_new_with_label (_("fast blanking"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (obj->priv->fast_enabled), TRUE);
	obj->priv->dummy_toggle = gtk_check_button_new_with_label (_("simulation"));

	brasero_tool_dialog_pack_options (BRASERO_TOOL_DIALOG (obj),
					  obj->priv->dummy_toggle,
					  obj->priv->fast_enabled,
					  NULL);

	brasero_tool_dialog_set_button (BRASERO_TOOL_DIALOG (obj),
					_("Blank"),
					GTK_STOCK_CDROM);
}

static void
brasero_blank_dialog_finalize (GObject *object)
{
	BraseroBlankDialog *cobj;

	cobj = BRASERO_BLANK_DIALOG (object);

	if (cobj->priv->caps) {
		g_object_unref (cobj->priv->caps);
		cobj->priv->caps = NULL;
	}

	if (cobj->priv->burn) {
		brasero_burn_cancel (cobj->priv->burn, FALSE);
		g_object_unref (cobj->priv->burn);
		cobj->priv->burn = NULL;
	}

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkWidget *
brasero_blank_dialog_new ()
{
	BraseroBlankDialog *obj;
	NautilusBurnMediaType media;

	obj = BRASERO_BLANK_DIALOG (g_object_new (BRASERO_TYPE_BLANK_DIALOG,
						  "title", _("Disc blanking"),
						  NULL));

	media = brasero_tool_dialog_get_media (BRASERO_TOOL_DIALOG (obj));
	brasero_blank_dialog_device_opts_setup (obj, media, FALSE);

	return GTK_WIDGET (obj);
}

static void
brasero_blank_dialog_device_opts_setup (BraseroBlankDialog *dialog,
					NautilusBurnMediaType type,
					gboolean messages)
{
	BraseroBurnResult result;
	gboolean fast_enabled = FALSE;
	gboolean fast_supported = FALSE;
	BraseroBurnFlag flags = BRASERO_BURN_FLAG_NONE;

	/* set the options */
	brasero_burn_caps_blanking_get_default_flags (dialog->priv->caps,
						      type,
						      &flags,
						      &fast_enabled);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->priv->dummy_toggle),
				      (flags & BRASERO_BURN_FLAG_DUMMY));

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->priv->fast_enabled),
				      fast_enabled);

	result = brasero_burn_caps_blanking_get_supported_flags (dialog->priv->caps,
								 type,
								 &flags,
								 &fast_supported);

	gtk_widget_set_sensitive (dialog->priv->dummy_toggle,
				  (flags & BRASERO_BURN_FLAG_DUMMY));

	gtk_widget_set_sensitive (dialog->priv->fast_enabled, fast_supported);

	if (result == BRASERO_BURN_NOT_SUPPORTED) {
		GtkWidget *message;

		if (!messages)
			return;

		/* we don't need / can't blank(ing) so tell the user */
		message = gtk_message_dialog_new_with_markup (GTK_WINDOW (dialog),
							      GTK_DIALOG_MODAL |
							      GTK_DIALOG_DESTROY_WITH_PARENT,
							      GTK_MESSAGE_INFO,
							      GTK_BUTTONS_CLOSE,
							      "<big><b>This type of disc can't or doesn't require to be blanked.</b></big>");
		gtk_window_set_title (GTK_WINDOW (message), _("Unneeded operation"));

		gtk_dialog_run (GTK_DIALOG (message));
		gtk_widget_destroy (message);
	}
	else if (result == BRASERO_BURN_ERR)
		return;

	if (!messages)
		return;

	/* FIXME: do we really need this following messages ? */
	if (type == NAUTILUS_BURN_MEDIA_TYPE_DVD_PLUS_RW) {
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->fast_enabled))) {
			GtkWidget *message;

			/* simulation doesn't work with DVDs */
			/* Tell the user fast blanking is not required with this kind of drive */
			message = gtk_message_dialog_new (GTK_WINDOW (dialog),
							  GTK_DIALOG_MODAL |
							  GTK_DIALOG_DESTROY_WITH_PARENT,
							  GTK_MESSAGE_INFO,
							  GTK_BUTTONS_CLOSE,
							  _("You can use this type of DVD without prior blanking.\n"
							    "NOTE: it doesn't support simulation."));

			gtk_window_set_title (GTK_WINDOW (message), _("Unneeded operation"));

			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
								  _("You can nevertheless blank it with the slow option if you want to."));
			gtk_dialog_run (GTK_DIALOG (message));
			gtk_widget_destroy (message);
		}
	}
}

static void
brasero_blank_dialog_media_changed (BraseroToolDialog *dialog,
				    NautilusBurnMediaType media)
{
	BraseroBlankDialog *self;

	self = BRASERO_BLANK_DIALOG (dialog);

	brasero_blank_dialog_device_opts_setup (self, media, TRUE);
}

static gboolean
brasero_blank_dialog_cancel_dialog (GtkWidget *toplevel)
{
	gint result;
	GtkWidget *button;
	GtkWidget *message;

	message = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					  GTK_DIALOG_DESTROY_WITH_PARENT|
					  GTK_DIALOG_MODAL,
					  GTK_MESSAGE_QUESTION,
					  GTK_BUTTONS_NONE,
					  _("Do you really want to quit?"));

	gtk_window_set_title (GTK_WINDOW (message), _("Confirm"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
						  _("Interrupting the process may make disc unusable."));
	gtk_dialog_add_buttons (GTK_DIALOG (message),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				NULL);

	button = brasero_utils_make_button (_("Continue"), GTK_STOCK_OK, NULL);
	gtk_widget_show_all (button);
	gtk_dialog_add_action_widget (GTK_DIALOG (message),
				      button, GTK_RESPONSE_OK);

	result = gtk_dialog_run (GTK_DIALOG (message));
	gtk_widget_destroy (message);

	if (result != GTK_RESPONSE_OK)
		return TRUE;

	return FALSE;
}

static gboolean
brasero_blank_dialog_cancel (BraseroToolDialog *dialog)
{
	BraseroBlankDialog *self;
	BraseroBurnResult result = BRASERO_BURN_OK;

	self = BRASERO_BLANK_DIALOG (dialog);

	if (self->priv->burn)
		result = brasero_burn_cancel (self->priv->burn, TRUE);

	if (result == BRASERO_BURN_DANGEROUS) {
		if (brasero_blank_dialog_cancel_dialog (GTK_WIDGET (self))) {
			if (self->priv->burn)
				brasero_burn_cancel (self->priv->burn, FALSE);
		}
		else
			return FALSE;
	}

	return TRUE;
}

static void
brasero_blank_dialog_progress_changed_cb (BraseroBurn *burn,
					  gdouble overall_progress,
					  gdouble task_progress,
					  glong remaining,
					  BraseroBlankDialog *dialog)
{
	brasero_tool_dialog_set_progress (BRASERO_TOOL_DIALOG (dialog),
					  overall_progress,
					  task_progress,
					  remaining,
					  -1,
					  -1);
}

static void
brasero_blank_dialog_action_changed_cb (BraseroBurn *burn,
					BraseroBurnAction action,
					BraseroBlankDialog *dialog)
{
	gchar *string = NULL;

	brasero_burn_get_action_string (dialog->priv->burn, action, &string);
	brasero_tool_dialog_set_action (BRASERO_TOOL_DIALOG (dialog),
					action,
					string);
	g_free (string);
}

static BraseroBurnResult
brasero_blank_dialog_blank_insert_media_cb (BraseroBurn *burn,
					    BraseroBurnError error,
					    BraseroMediaType type,
					    BraseroBlankDialog *dialog)
{
	return BRASERO_BURN_CANCEL;
}

static gboolean
brasero_blank_dialog_activate (BraseroToolDialog *dialog,
			       NautilusBurnDrive *drive)
{
	BraseroBlankDialog *self;
	BraseroBurnResult result;
	BraseroBurnFlag flags;
	GError *error = NULL;
	gboolean fast_blank;

	self = BRASERO_BLANK_DIALOG (dialog);

	/* set the flags */
	flags = BRASERO_BURN_FLAG_EJECT | BRASERO_BURN_FLAG_NOGRACE;
	if (debug)
		flags |= BRASERO_BURN_FLAG_DEBUG;

	if (GTK_WIDGET_SENSITIVE (self->priv->dummy_toggle)
	&&  gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->priv->dummy_toggle)))
		flags |= BRASERO_BURN_FLAG_DUMMY;

	if (GTK_WIDGET_SENSITIVE (self->priv->fast_enabled)
	&&  gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->priv->fast_enabled)))
		fast_blank = TRUE;
	else
		fast_blank = FALSE;

	self->priv->burn = brasero_burn_new ();
	g_signal_connect (self->priv->burn,
			  "progress-changed",
			  G_CALLBACK (brasero_blank_dialog_progress_changed_cb),
			  self);
	g_signal_connect (self->priv->burn,
			  "action-changed",
			  G_CALLBACK (brasero_blank_dialog_action_changed_cb),
			  self);
	g_signal_connect (G_OBJECT (self->priv->burn),
			  "insert_media",
			  G_CALLBACK (brasero_blank_dialog_blank_insert_media_cb),
			  self);

	result = brasero_burn_blank (self->priv->burn,
				     flags,
				     drive,
				     fast_blank,
				     &error);

	/* Tell the user the result of the operation */
	if (result == BRASERO_BURN_ERR || error) {
		GtkWidget *message;

		message =  gtk_message_dialog_new (GTK_WINDOW (self),
						   GTK_DIALOG_DESTROY_WITH_PARENT|
						   GTK_DIALOG_MODAL,
						   GTK_MESSAGE_ERROR,
						   GTK_BUTTONS_CLOSE,
						   _("Error Blanking:"));

		gtk_window_set_title (GTK_WINDOW (self), _("Blanking finished"));

		if (error) {
			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
								  "%s.",
								  error->message);
			g_error_free (error);
		}
		else
			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
								  _("Unexpected error"));

		gtk_dialog_run (GTK_DIALOG (message));
		gtk_widget_destroy (message);
	}
	else if (result == BRASERO_BURN_OK) {
		GtkWidget *message;

		message = gtk_message_dialog_new (GTK_WINDOW (self),
						  GTK_DIALOG_DESTROY_WITH_PARENT|
						  GTK_DIALOG_MODAL,
						  GTK_MESSAGE_INFO,
						  GTK_BUTTONS_CLOSE,
						  _("The disc was successfully blanked:"));

		gtk_window_set_title (GTK_WINDOW (self), _("Blanking finished"));

		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
							  _("the disc is ready for use."));
		gtk_dialog_run (GTK_DIALOG (message));
		gtk_widget_destroy (message);
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

	nautilus_burn_drive_unref (drive);
	g_object_unref (self->priv->burn);
	self->priv->burn = NULL;

	return FALSE;
}
