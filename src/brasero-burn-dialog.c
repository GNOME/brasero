/***************************************************************************
 *            burn-dialog.c
 *
 *  mer jun 29 13:05:45 2005
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

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <gdk/gdk.h>

#include <gtk/gtkwidget.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkbox.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkstock.h>
#include <gtk/gtklabel.h>

#include <nautilus-burn-drive-monitor.h>
#include <nautilus-burn-drive.h>

#include "utils.h"
#include "brasero-disc.h"
#include "brasero-tray.h"
#include "brasero-burn-dialog.h"
#include "burn-basics.h"
#include "burn-session.h"
#include "burn-medium.h"
#include "brasero-drive-selection.h"
#include "brasero-progress.h"
#include "brasero-ncb.h"

static void brasero_burn_dialog_class_init (BraseroBurnDialogClass *klass);
static void brasero_burn_dialog_init (BraseroBurnDialog *obj);
static void brasero_burn_dialog_finalize (GObject *object);
static void brasero_burn_dialog_destroy (GtkObject *object);

static gboolean
brasero_burn_dialog_delete (GtkWidget *widget,
			    GdkEventAny *event);

static void
brasero_burn_dialog_cancel_clicked_cb (GtkWidget *button,
				       BraseroBurnDialog *dialog);

static void
brasero_burn_dialog_tray_cancel_cb (BraseroTrayIcon *tray,
				    BraseroBurnDialog *dialog);
static void
brasero_burn_dialog_tray_show_dialog_cb (BraseroTrayIcon *tray,
					 gboolean show,
					 GtkWidget *dialog);
static void
brasero_burn_dialog_tray_close_after_cb (BraseroTrayIcon *tray,
					 gboolean close,
					 BraseroBurnDialog *dialog);

struct BraseroBurnDialogPrivate {
	BraseroBurn *burn;
	BraseroBurnSession *session;

	GtkWidget *close_check;
	GtkWidget *progress;
	GtkWidget *header;
	GtkWidget *cancel;
	GtkWidget *image;
	BraseroTrayIcon *tray;

	gint close_timeout;
};

#define TIMEOUT	10000
#define WAITED_FOR_DRIVE	"WaitedForDriveKey"

static GObjectClass *parent_class = NULL;

GType
brasero_burn_dialog_get_type ()
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroBurnDialogClass),
			NULL,
			NULL,
			(GClassInitFunc) brasero_burn_dialog_class_init,
			NULL,
			NULL,
			sizeof (BraseroBurnDialog),
			0,
			(GInstanceInitFunc) brasero_burn_dialog_init,
		};

		type = g_type_register_static (GTK_TYPE_DIALOG,
					       "BraseroBurnDialog",
					       &our_info, 0);
	}

	return type;
}

static void
brasero_burn_dialog_class_init (BraseroBurnDialogClass * klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_burn_dialog_finalize;
	gtk_object_class->destroy = brasero_burn_dialog_destroy;
	widget_class->delete_event = brasero_burn_dialog_delete;
}

static gchar *
brasero_burn_dialog_get_media_type_string (BraseroBurn *burn,
					   BraseroMedia type,
					   gboolean insert)
{
	gchar *message = NULL;

	if (type & BRASERO_MEDIUM_HAS_DATA) {
		if (!insert) {
			if (type & BRASERO_MEDIUM_WRITABLE) 
				message = g_strdup (_("replace the disc with the burnt media to perform an integrity check"));
			else if (type & BRASERO_MEDIUM_REWRITABLE)
				message = g_strdup (_("replace the disc with a rewritable disc holding data."));
			else
				message = g_strdup (_("replace the disc with a disc holding data."));
		}
		else {
			if (type & BRASERO_MEDIUM_WRITABLE) 
				message = g_strdup (_("insert the burnt media to perform an integrity check"));
			else if (type & BRASERO_MEDIUM_REWRITABLE)
				message = g_strdup (_("insert a rewritable disc holding data."));
			else
				message = g_strdup (_("insert a disc holding data."));
		}
	}
	else if (type & BRASERO_MEDIUM_WRITABLE) {
		gint64 isosize = 0;
	
		brasero_burn_status (burn,
				     NULL,
				     &isosize,
				     NULL,
				     NULL);

		if ((type & BRASERO_MEDIUM_CD) && !(type & BRASERO_MEDIUM_DVD)) {
			if (!insert) {
				if (isosize)
					message = g_strdup_printf (_("replace the disc with a recordable CD with a least %i MiB free."), 
								   (int) (isosize / 1048576));
				else
					message = g_strdup_printf (_("replace the disc with a recordable CD."));
			}
			else {
				if (isosize)
					message = g_strdup_printf (_("insert a recordable CD with a least %i MiB free."), 
								   (int) (isosize / 1048576));
				else
					message = g_strdup_printf (_("insert a recordable CD."));
			}
		}
		else if (!(type & BRASERO_MEDIUM_CD) && (type & BRASERO_MEDIUM_DVD)) {
			if (!insert) {
				if (isosize)
					message = g_strdup_printf (_("replace the disc with a recordable DVD with a least %i MiB free."), 
								   (int) (isosize / 1048576));
				else
					message = g_strdup_printf (_("replace the disc with a recordable DVD."));
			}
			else {
				if (isosize)
					message = g_strdup_printf (_("insert a recordable DVD with a least %i MiB free."), 
								   (int) (isosize / 1048576));
				else
					message = g_strdup_printf (_("insert a recordable DVD."));
			}
		}
		else if (!insert) {
			if (isosize)
				message = g_strdup_printf (_("replace the disc with a recordable CD or DVD with a least %i MiB free."), 
							   (int) (isosize / 1048576));
			else
				message = g_strdup_printf (_("replace the disc with a recordable CD or DVD."));
		}
		else {
			if (isosize)
				message = g_strdup_printf (_("insert a recordable CD or DVD with a least %i MiB free."), 
							   (int) (isosize / 1048576));
			else
				message = g_strdup_printf (_("insert a recordable CD or DVD."));
		}
	}

	return message;
}

static void
brasero_burn_dialog_wait_for_insertion (NautilusBurnDriveMonitor *monitor,
					NautilusBurnDrive *drive,
					GtkDialog *message)
{
	NautilusBurnDrive *waited_drive;

	waited_drive = g_object_get_data (G_OBJECT (message), WAITED_FOR_DRIVE);

	/* we must make sure that the change was triggered
	 * by the current selected drive */
	if (!nautilus_burn_drive_equal (drive, waited_drive))
		return;

	/* we might have a dialog waiting for the 
	 * insertion of a disc if so close it */
	gtk_dialog_response (GTK_DIALOG (message), GTK_RESPONSE_OK);
}

static BraseroBurnResult
brasero_burn_dialog_insert_disc_cb (BraseroBurn *burn,
				    NautilusBurnDrive *drive,
				    BraseroBurnError error,
				    BraseroMedia type,
				    BraseroBurnDialog *dialog)
{
	gint result;
	gint added_id;
	gchar *drive_name;
	GtkWindow *window;
	GtkWidget *message;
	gboolean hide = FALSE;
	NautilusBurnDriveMonitor *monitor;
	gchar *main_message = NULL, *secondary_message = NULL;

	if (!GTK_WIDGET_VISIBLE (dialog)) {
		gtk_widget_show (GTK_WIDGET (dialog));
		hide = TRUE;
	}

	/* FIXME: we should specify the name of the drive where to put the disc */
	if (drive)
		drive_name = nautilus_burn_drive_get_name_for_display (drive);
	else
		drive_name = NULL;

	if (error == BRASERO_BURN_ERROR_MEDIA_BUSY) {
		main_message = g_strdup_printf (_("The disc in \"%s\" is busy:"),
						drive_name);
		secondary_message = g_strdup (_("make sure another application is not using it."));
	} 
	else if (error == BRASERO_BURN_ERROR_MEDIA_NONE) {
		main_message = g_strdup_printf (_("There is no disc in \"%s\":"),
						drive_name);
		secondary_message = brasero_burn_dialog_get_media_type_string (burn, type, TRUE);
	}
	else if (error == BRASERO_BURN_ERROR_MEDIA_UNSUPPORTED) {
		main_message = g_strdup_printf (_("The disc in \"%s\" is not supported:"),
						drive_name);
		secondary_message = brasero_burn_dialog_get_media_type_string (burn, type, TRUE);
	}
	else if (error == BRASERO_BURN_ERROR_MEDIA_NOT_REWRITABLE) {
		main_message = g_strdup_printf (_("The disc in \"%s\" is not rewritable:"),
						drive_name);
		secondary_message = brasero_burn_dialog_get_media_type_string (burn, type, FALSE);
	}
	else if (error == BRASERO_BURN_ERROR_MEDIA_BLANK) {
		main_message = g_strdup_printf (_("The disc in \"%s\" is empty:"),
						drive_name);
		secondary_message = brasero_burn_dialog_get_media_type_string (burn, type, FALSE);
	}
	else if (error == BRASERO_BURN_ERROR_MEDIA_NOT_WRITABLE) {
		main_message = g_strdup_printf (_("The disc in \"%s\" is not writable:"),
						drive_name);
		secondary_message = brasero_burn_dialog_get_media_type_string (burn, type, FALSE);
	}
	else if (error == BRASERO_BURN_ERROR_DVD_NOT_SUPPORTED) {
		main_message = g_strdup_printf (_("The disc in \"%s\" is a DVD:"),
						drive_name);
		secondary_message = brasero_burn_dialog_get_media_type_string (burn, type, FALSE);
	}
	else if (error == BRASERO_BURN_ERROR_CD_NOT_SUPPORTED) {
		main_message = g_strdup_printf (_("The disc in \"%s\" is a CD:"),
						drive_name);
		secondary_message = brasero_burn_dialog_get_media_type_string (burn, type, FALSE);
	}
	else if (error == BRASERO_BURN_ERROR_MEDIA_SPACE) {
		main_message = g_strdup_printf (_("The disc in \"%s\" is not big enough:"),
						drive_name);
		secondary_message = brasero_burn_dialog_get_media_type_string (burn, type, FALSE);
	}
	else if (error == BRASERO_BURN_ERROR_NONE) {
		secondary_message = brasero_burn_dialog_get_media_type_string (burn, type, FALSE);
		main_message = g_strdup_printf ("<b><big>%s</big></b>", secondary_message);
		g_free (secondary_message);
		secondary_message = NULL;
	}
	else if (error == BRASERO_BURN_ERROR_RELOAD_MEDIA) {
		main_message = g_strdup_printf (_("The disc in \"%s\" needs to be reloaded:"),
						drive_name);
		secondary_message = g_strdup (_("eject the disc and reload it."));
	}

	g_free (drive_name);

	window = GTK_WINDOW (dialog);

	if (secondary_message) {
		message = gtk_message_dialog_new (window,
						  GTK_DIALOG_DESTROY_WITH_PARENT|
						  GTK_DIALOG_MODAL,
						  GTK_MESSAGE_WARNING,
						  GTK_BUTTONS_CANCEL,
						  main_message);

		if (secondary_message) {
			gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
								  secondary_message);
			g_free (secondary_message);
		}
	}
	else
		message = gtk_message_dialog_new_with_markup (window,
							      GTK_DIALOG_DESTROY_WITH_PARENT|
							      GTK_DIALOG_MODAL,
							      GTK_MESSAGE_WARNING,
							      GTK_BUTTONS_CANCEL,
							      main_message);

	g_free (main_message);

	if (error == BRASERO_BURN_ERROR_MEDIA_NONE)
		gtk_window_set_title (GTK_WINDOW (message), _("Waiting for disc insertion"));
	else
		gtk_window_set_title (GTK_WINDOW (message), _("Waiting for disc replacement"));

	/* connect to signals to be warned when media is inserted */
	g_object_set_data (G_OBJECT (message), WAITED_FOR_DRIVE, drive);

	monitor = nautilus_burn_get_drive_monitor ();
	added_id = g_signal_connect_after (monitor,
					   "media-added",
					   G_CALLBACK (brasero_burn_dialog_wait_for_insertion),
					   message);

	result = gtk_dialog_run (GTK_DIALOG (message));

	g_signal_handler_disconnect (monitor, added_id);
	gtk_widget_destroy (message);

	if (hide)
		gtk_widget_hide (GTK_WIDGET (dialog));

	if (result != GTK_RESPONSE_OK)
		return BRASERO_BURN_CANCEL;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_dialog_loss_warnings_cb (GtkDialog *dialog, 
				      const gchar *title,
				      const gchar *main_message,
				      const gchar *secondary_message,
				      const gchar *button_text,
				      const gchar *button_icon)
{
	gint result;
	GtkWindow *window;
	GtkWidget *button;
	GtkWidget *message;
	gboolean hide = FALSE;

	if (!GTK_WIDGET_VISIBLE (dialog)) {
		gtk_widget_show (GTK_WIDGET (dialog));
		hide = TRUE;
	}

	window = GTK_WINDOW (dialog);
	message = gtk_message_dialog_new (window,
					  GTK_DIALOG_DESTROY_WITH_PARENT|
					  GTK_DIALOG_MODAL,
					  GTK_MESSAGE_WARNING,
					  GTK_BUTTONS_NONE,
					  main_message);

	gtk_window_set_title (GTK_WINDOW (message), title);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
						 secondary_message);

	gtk_dialog_add_buttons (GTK_DIALOG (message),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				NULL);

	button = brasero_utils_make_button (_("Replace disc"),
					    GTK_STOCK_REFRESH,
					    NULL,
					    GTK_ICON_SIZE_BUTTON);
	gtk_widget_show_all (button);
	gtk_dialog_add_action_widget (GTK_DIALOG (message),
				      button, GTK_RESPONSE_ACCEPT);

	button = brasero_utils_make_button (button_text,
					    NULL,
					    button_icon,
					    GTK_ICON_SIZE_BUTTON);
	gtk_widget_show_all (button);
	gtk_dialog_add_action_widget (GTK_DIALOG (message),
				      button, GTK_RESPONSE_OK);

	result = gtk_dialog_run (GTK_DIALOG (message));
	gtk_widget_destroy (message);

	if (hide)
		gtk_widget_hide (GTK_WIDGET (dialog));

	if (result == GTK_RESPONSE_ACCEPT)
		return BRASERO_BURN_NEED_RELOAD;

	if (result != GTK_RESPONSE_OK)
		return BRASERO_BURN_CANCEL;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_dialog_data_loss_cb (BraseroBurn *burn,
				  GtkDialog *dialog)
{
	return brasero_burn_dialog_loss_warnings_cb (dialog,
						     _("Possible loss of data"),
						     _("The disc in the drive holds data:"),
						     _("Do you want to erase the current disc?\nOr replace the current disc with a new disc?"),
						     _("Erase disc"),
						     "media-optical-blank");
}

static BraseroBurnResult
brasero_burn_dialog_previous_session_loss_cb (BraseroBurn *burn,
					      GtkDialog *dialog)
{
	return brasero_burn_dialog_loss_warnings_cb (dialog,
						     _("Multisession disc"),
						     _("Appending new files to a multisession disc is not advised:"),
						     _("already burnt files will be invisible (though still readable).\nDo you want to continue anyway?"),
						     _("Continue"),
						     "media-optical-burn");
}

static BraseroBurnResult
brasero_burn_dialog_audio_to_appendable_cb (BraseroBurn *burn,
					    GtkDialog *dialog)
{
	return brasero_burn_dialog_loss_warnings_cb (dialog,
						     _("Multisession disc"),
						     _("Appending audio tracks to a CD is not advised:"),
						     _("you might not be able to listen to them with stereos and CD-TEXT won't be written.\nDo you want to continue anyway?"),
						     _("Continue"),
						     "media-optical-burn");
}

static BraseroBurnResult
brasero_burn_dialog_rewritable_cb (BraseroBurn *burn,
				   GtkDialog *dialog)
{
	return brasero_burn_dialog_loss_warnings_cb (dialog,
						     _("Rewritable disc"),
						     _("Recording audio tracks on a rewritable disc is not advised:"),
						     _("you might not be able to listen to it with stereos.\nDo you want to continue anyway?"),
						     _("Continue"),
						     "media-optical-burn");
}

static BraseroBurnResult
brasero_burn_dialog_disable_joliet_cb (BraseroBurn *burn,
				       GtkDialog *dialog)
{
	gint result;
	GtkWindow *window;
	GtkWidget *button;
	GtkWidget *message;
	gboolean hide = FALSE;

	if (!GTK_WIDGET_VISIBLE (dialog)) {
		gtk_widget_show (GTK_WIDGET (dialog));
		hide = TRUE;
	}

	window = GTK_WINDOW (dialog);
	message = gtk_message_dialog_new (window,
					  GTK_DIALOG_DESTROY_WITH_PARENT|
					  GTK_DIALOG_MODAL,
					  GTK_MESSAGE_WARNING,
					  GTK_BUTTONS_NONE,
					  _("Some files don't have a suitable name for a Windows-compatible CD:"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
						  _("Do you want to continue with Windows compatibility disabled?"));

	gtk_window_set_title (GTK_WINDOW (message), _("Windows compatibility"));
	gtk_dialog_add_buttons (GTK_DIALOG (message),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				NULL);

	button = brasero_utils_make_button (_("Continue"),
					    GTK_STOCK_OK,
					    NULL,
					    GTK_ICON_SIZE_BUTTON);
	gtk_widget_show_all (button);
	gtk_dialog_add_action_widget (GTK_DIALOG (message),
				      button, GTK_RESPONSE_OK);

	result = gtk_dialog_run (GTK_DIALOG (message));
	gtk_widget_destroy (message);

	if (hide)
		gtk_widget_hide (GTK_WIDGET (dialog));

	if (result != GTK_RESPONSE_OK)
		return BRASERO_BURN_CANCEL;

	return BRASERO_BURN_OK;
}

static void
brasero_burn_dialog_progress_changed_real (BraseroBurnDialog *dialog,
					   gint64 written,
					   gint64 isosize,
					   gint64 rate,
					   gdouble overall_progress,
					   gdouble task_progress,
					   glong remaining,
					   gboolean is_DVD)
{
	gint mb_isosize = -1;
	gint mb_written = -1;

	if (written >= 0)
		mb_written = (gint) (written / 1048576);
	
	if (isosize > 0)
		mb_isosize = (gint) (isosize / 1048576);

	brasero_burn_progress_set_status (BRASERO_BURN_PROGRESS (dialog->priv->progress),
					  is_DVD,
					  overall_progress,
					  task_progress,
					  remaining,
					  mb_isosize,
					  mb_written,
					  rate);

	brasero_tray_icon_set_progress (BRASERO_TRAYICON (dialog->priv->tray),
					overall_progress,
					remaining);
}

static void
brasero_burn_dialog_progress_changed_cb (BraseroBurn *burn, 
					 gdouble overall_progress,
					 gdouble task_progress,
					 glong remaining,
					 BraseroBurnDialog *dialog)
{
	BraseroMedia media = BRASERO_MEDIUM_NONE;
	gint64 isosize = -1;
	gint64 written = -1;
	gint64 rate = -1;

	brasero_burn_status (dialog->priv->burn,
			     &media,
			     &isosize,
			     &written,
			     &rate);

	brasero_burn_dialog_progress_changed_real (dialog,
						   written,
						   isosize,
						   rate,
						   overall_progress,
						   task_progress,
						   remaining,
						   (media & BRASERO_MEDIUM_DVD));
}

static void
brasero_burn_dialog_action_changed_real (BraseroBurnDialog *dialog,
					 BraseroBurnAction action,
					 const gchar *string)
{
	brasero_burn_progress_set_action (BRASERO_BURN_PROGRESS (dialog->priv->progress),
					  action,
					  string);
	brasero_tray_icon_set_action (BRASERO_TRAYICON (dialog->priv->tray),
				      action);
}

static void
brasero_burn_dialog_action_changed_cb (BraseroBurn *burn, 
				       BraseroBurnAction action,
				       BraseroBurnDialog *dialog)
{
	gchar *string = NULL;

	brasero_burn_get_action_string (dialog->priv->burn, action, &string);
	brasero_burn_dialog_action_changed_real (dialog, action, string);
	g_free (string);
}

static void
brasero_burn_dialog_init (BraseroBurnDialog * obj)
{
	GtkWidget *box;
	GtkWidget *vbox;
	GtkWidget *alignment;

	obj->priv = g_new0 (BraseroBurnDialogPrivate, 1);
	gtk_window_set_default_size (GTK_WINDOW (obj), 500, 0);

	gtk_dialog_set_has_separator (GTK_DIALOG (obj), FALSE);
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW (obj), FALSE);
	gtk_window_set_skip_pager_hint (GTK_WINDOW (obj), FALSE);
	gtk_window_set_type_hint (GTK_WINDOW (obj), GDK_WINDOW_TYPE_HINT_NORMAL);
	gtk_window_set_position (GTK_WINDOW (obj),GTK_WIN_POS_CENTER);

	obj->priv->tray = brasero_tray_icon_new ();
	g_signal_connect (obj->priv->tray,
			  "cancel",
			  G_CALLBACK (brasero_burn_dialog_tray_cancel_cb),
			  obj);
	g_signal_connect (obj->priv->tray,
			  "show-dialog",
			  G_CALLBACK (brasero_burn_dialog_tray_show_dialog_cb),
			  obj);
	g_signal_connect (obj->priv->tray,
			  "close-after",
			  G_CALLBACK (brasero_burn_dialog_tray_close_after_cb),
			  obj);

	alignment = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment), 6, 8, 6, 6);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (obj)->vbox),
			    alignment,
			    TRUE,
			    TRUE,
			    0);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (alignment), vbox);

	box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);

	obj->priv->header = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (obj->priv->header), 0.0, 0.5);
	gtk_misc_set_padding (GTK_MISC (obj->priv->header), 0, 18);
	gtk_box_pack_start (GTK_BOX (box), obj->priv->header, TRUE, TRUE, 0);

	obj->priv->image = gtk_image_new ();
	gtk_box_pack_start (GTK_BOX (box), obj->priv->image, FALSE, FALSE, 0);

	obj->priv->progress = brasero_burn_progress_new ();
	gtk_box_pack_start (GTK_BOX (vbox),
			    obj->priv->progress,
			    FALSE,
			    FALSE,
			    0);

	obj->priv->close_check = gtk_check_button_new_with_label (_("Close the application if the burn process is successful"));
	gtk_box_pack_end (GTK_BOX (obj->priv->progress),
			  obj->priv->close_check,
			  FALSE,
			  FALSE,
			  0);

	/* buttons */
	obj->priv->cancel = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
	gtk_dialog_add_action_widget (GTK_DIALOG (obj),
				      obj->priv->cancel,
				      GTK_RESPONSE_CANCEL);
}

static void
brasero_burn_dialog_destroy (GtkObject * object)
{
	BraseroBurnDialog *cobj;

	cobj = BRASERO_BURN_DIALOG (object);

	if (cobj->priv->burn) {
		g_object_unref (cobj->priv->burn);
		cobj->priv->burn = NULL;
	}

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void
brasero_burn_dialog_finalize (GObject * object)
{
	BraseroBurnDialog *cobj;

	cobj = BRASERO_BURN_DIALOG (object);

	if (cobj->priv->burn) {
		brasero_burn_cancel (cobj->priv->burn, TRUE);
		g_object_unref (cobj->priv->burn);
		cobj->priv->burn = NULL;
	}

	if (cobj->priv->tray) {
		g_object_unref (cobj->priv->tray);
		cobj->priv->tray = NULL;
	}

	if (cobj->priv->close_timeout) {
		g_source_remove (cobj->priv->close_timeout);
		cobj->priv->close_timeout = 0;
	}

	if (cobj->priv->session) {
		g_object_unref (cobj->priv->session);
		cobj->priv->session = NULL;
	}

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkWidget *
brasero_burn_dialog_new (void)
{
	BraseroBurnDialog *obj;

	obj = BRASERO_BURN_DIALOG (g_object_new (BRASERO_TYPE_BURN_DIALOG, NULL));

	return GTK_WIDGET (obj);
}

static void
brasero_burn_dialog_activity_start (BraseroBurnDialog *dialog)
{
	GdkCursor *cursor;

	cursor = gdk_cursor_new (GDK_WATCH);
	gdk_window_set_cursor (GTK_WIDGET (dialog)->window, NULL);
	gdk_cursor_unref (cursor);

	gtk_button_set_use_stock (GTK_BUTTON (dialog->priv->cancel), TRUE);
	gtk_button_set_label (GTK_BUTTON (dialog->priv->cancel), GTK_STOCK_CANCEL);
	gtk_window_set_urgency_hint (GTK_WINDOW (dialog), FALSE);

	g_signal_connect (dialog->priv->cancel,
			  "clicked",
			  G_CALLBACK (brasero_burn_dialog_cancel_clicked_cb),
			  dialog);

	brasero_burn_progress_set_status (BRASERO_BURN_PROGRESS (dialog->priv->progress),
					  FALSE,
					  0.0,
					  0.0,
					  -1,
					  -1,
					  -1,
					  -1);
}

static void
brasero_burn_dialog_activity_stop (BraseroBurnDialog *dialog,
				   const gchar *message)
{
	gchar *markup;

	gdk_window_set_cursor (GTK_WIDGET (dialog)->window, NULL);

	markup = g_strdup_printf ("<b><big>%s</big></b>", message);
	gtk_label_set_text (GTK_LABEL (dialog->priv->header), markup);
	gtk_label_set_use_markup (GTK_LABEL (dialog->priv->header), TRUE);
	g_free (markup);

	gtk_button_set_use_stock (GTK_BUTTON (dialog->priv->cancel), TRUE);
	gtk_button_set_label (GTK_BUTTON (dialog->priv->cancel), GTK_STOCK_CLOSE);

	g_signal_handlers_disconnect_by_func (dialog->priv->cancel,
					      brasero_burn_dialog_cancel_clicked_cb,
					      dialog);

	brasero_burn_progress_set_status (BRASERO_BURN_PROGRESS (dialog->priv->progress),
					  FALSE,
					  1.0,
					  1.0,
					  -1,
					  -1,
					  -1,
					  -1);

	gtk_widget_show (GTK_WIDGET (dialog));
	gtk_window_set_urgency_hint (GTK_WINDOW (dialog), TRUE);
}

static void
brasero_burn_dialog_update_info (BraseroBurnDialog *dialog)
{
	gchar *title = NULL;
	gchar *header = NULL;
	BraseroMedia media;
	BraseroTrackType source;
	NautilusBurnDrive *drive;

	/* check what drive we should display */
	brasero_burn_session_get_input_type (dialog->priv->session, &source);
	if (!BRASERO_BURN_SESSION_NO_TMP_FILE (dialog->priv->session)
	||   source.type != BRASERO_TRACK_TYPE_DISC) {
		drive = brasero_burn_session_get_burner (dialog->priv->session);

		if (NCB_DRIVE_GET_TYPE (drive) == NAUTILUS_BURN_DRIVE_TYPE_FILE) {
			/* we are creating an image to the hard drive */
			gtk_image_set_from_icon_name (GTK_IMAGE (dialog->priv->image),
						      "iso-image-new",
						      GTK_ICON_SIZE_DIALOG);

			header = g_strdup_printf ("<big><b>Creating image</b></big>");
			title = g_strdup (_("Creating image"));
			goto end;
		}
	}
	else
		drive = brasero_burn_session_get_src_drive (dialog->priv->session);

	media = NCB_MEDIA_GET_STATUS (drive);
	if (media & BRASERO_MEDIUM_DVD) {
		if (source.type == BRASERO_TRACK_TYPE_DATA) {
			title = g_strdup (_("Burning DVD"));
			header = g_strdup (_("<big><b>Burning data DVD</b></big>"));

			gtk_image_set_from_icon_name (GTK_IMAGE (dialog->priv->image),
						      "media-optical-data-new",
						      GTK_ICON_SIZE_DIALOG);
		}
		else if (source.type == BRASERO_TRACK_TYPE_IMAGE) {
			title = g_strdup (_("Burning DVD"));
			header = g_strdup (_("<big><b>Burning image to DVD</b></big>"));

			gtk_image_set_from_icon_name (GTK_IMAGE (dialog->priv->image),
						      NCB_MEDIA_GET_ICON (drive),
						      GTK_ICON_SIZE_DIALOG);
		}
		else if (source.type == BRASERO_TRACK_TYPE_DISC) {
			title = g_strdup (_("Copying DVD"));
			header = g_strdup (_("<big><b>Copying data DVD</b></big>"));

			gtk_image_set_from_icon_name (GTK_IMAGE (dialog->priv->image),
						"media-optical-copy",
						GTK_ICON_SIZE_DIALOG);
		}
	}
	else if (media & BRASERO_MEDIUM_CD) {
		if (source.type == BRASERO_TRACK_TYPE_AUDIO) {
			title = g_strdup (_("Burning CD"));
			header = g_strdup_printf (_("<big><b>Burning audio CD</b></big>"));

			gtk_image_set_from_icon_name (GTK_IMAGE (dialog->priv->image),
						      "media-optical-audio-new",
						      GTK_ICON_SIZE_DIALOG);
		}
		else if (source.type == BRASERO_TRACK_TYPE_DATA) {
			title = g_strdup (_("Burning CD"));
			header = g_strdup_printf (_("<big><b>Burning data CD</b></big>"));

			gtk_image_set_from_icon_name (GTK_IMAGE (dialog->priv->image),
						      "media-optical-data-new",
						      GTK_ICON_SIZE_DIALOG);
		}
		else if (source.type == BRASERO_TRACK_TYPE_DISC) {
			title = g_strdup (_("Copying CD"));
			header = g_strdup(_("<big><b>Copying CD</b></big>"));

			gtk_image_set_from_icon_name (GTK_IMAGE (dialog->priv->image),
						      "media-optical-copy",
						      GTK_ICON_SIZE_DIALOG);
		}
		else if (source.type == BRASERO_TRACK_TYPE_IMAGE) {
			title = g_strdup (_("Burning CD"));
			header = g_strdup (_("<big><b>Burning image to CD</b></big>"));
		
			gtk_image_set_from_icon_name (GTK_IMAGE (dialog->priv->image),
						      NCB_MEDIA_GET_ICON (drive),
						      GTK_ICON_SIZE_DIALOG);
		}
	}
	else if (source.type == BRASERO_TRACK_TYPE_AUDIO) {
		title = g_strdup (_("Burning CD"));
		header = g_strdup_printf (_("<big><b>Burning audio CD</b></big>"));
		gtk_image_set_from_icon_name (GTK_IMAGE (dialog->priv->image),
					      "gnome-dev-removable",
					      GTK_ICON_SIZE_DIALOG);
	}
	else if (source.type == BRASERO_TRACK_TYPE_DATA) {
		title = g_strdup (_("Burning disc"));
		header = g_strdup_printf (_("<big><b>Burning data disc</b></big>"));
		gtk_image_set_from_icon_name (GTK_IMAGE (dialog->priv->image),
					      "gnome-dev-removable",
					      GTK_ICON_SIZE_DIALOG);
	}
	else if (source.type == BRASERO_TRACK_TYPE_DISC) {
		title = g_strdup (_("Copying disc"));
		header = g_strdup(_("<big><b>Copying disc</b></big>"));
		gtk_image_set_from_icon_name (GTK_IMAGE (dialog->priv->image),
					      "gnome-dev-removable",
					      GTK_ICON_SIZE_DIALOG);
	}
	else if (source.type == BRASERO_TRACK_TYPE_IMAGE) {
		title = g_strdup (_("Burning disc"));
		header = g_strdup (_("<big><b>Burning image to disc</b></big>"));
		gtk_image_set_from_icon_name (GTK_IMAGE (dialog->priv->image),
					      "gnome-dev-removable",
					      GTK_ICON_SIZE_DIALOG);
	}

end:

	nautilus_burn_drive_unref (drive);

	gtk_window_set_title (GTK_WINDOW (dialog), title);
	g_free (title);

	gtk_label_set_text (GTK_LABEL (dialog->priv->header), header);
	gtk_label_set_use_markup (GTK_LABEL (dialog->priv->header), TRUE);
	g_free (header);
}

static void
brasero_burn_dialog_input_changed_cb (BraseroBurnSession *session,
				      BraseroBurnDialog *dialog)
{
	brasero_burn_dialog_update_info (dialog);
}

static void
brasero_burn_dialog_output_changed_cb (BraseroBurnSession *session,
				       BraseroBurnDialog *dialog)
{
	brasero_burn_dialog_update_info (dialog);
}

static BraseroBurnResult
brasero_burn_dialog_setup_session (BraseroBurnDialog *dialog,
				   GError **error)
{
	brasero_burn_session_start (dialog->priv->session);

	dialog->priv->burn = brasero_burn_new ();
	g_signal_connect (dialog->priv->burn,
			  "insert-media",
			  G_CALLBACK (brasero_burn_dialog_insert_disc_cb),
			  dialog);
	g_signal_connect (dialog->priv->burn,
			  "warn-data-loss",
			  G_CALLBACK (brasero_burn_dialog_data_loss_cb),
			  dialog);
	g_signal_connect (dialog->priv->burn,
			  "warn-previous-session-loss",
			  G_CALLBACK (brasero_burn_dialog_previous_session_loss_cb),
			  dialog);
	g_signal_connect (dialog->priv->burn,
			  "warn-audio-to-appendable",
			  G_CALLBACK (brasero_burn_dialog_audio_to_appendable_cb),
			  dialog);
	g_signal_connect (dialog->priv->burn,
			  "warn-rewritable",
			  G_CALLBACK (brasero_burn_dialog_rewritable_cb),
			  dialog);
	g_signal_connect (dialog->priv->burn,
			  "disable-joliet",
			  G_CALLBACK (brasero_burn_dialog_disable_joliet_cb),
			  dialog);
	g_signal_connect (dialog->priv->burn,
			  "progress-changed",
			  G_CALLBACK (brasero_burn_dialog_progress_changed_cb),
			  dialog);
	g_signal_connect (dialog->priv->burn,
			  "action-changed",
			  G_CALLBACK (brasero_burn_dialog_action_changed_cb),
			  dialog);

	brasero_burn_progress_set_status (BRASERO_BURN_PROGRESS (dialog->priv->progress),
					  FALSE,
					  0.0,
					  -1.0,
					  -1,
					  -1,
					  -1,
					  -1);

	brasero_tray_icon_set_progress (BRASERO_TRAYICON (dialog->priv->tray),
					0.0,
					-1);

	brasero_burn_progress_set_action (BRASERO_BURN_PROGRESS (dialog->priv->progress),
					  BRASERO_BURN_ACTION_NONE,
					  NULL);

	brasero_tray_icon_set_action (BRASERO_TRAYICON (dialog->priv->tray),
				      BRASERO_BURN_ACTION_NONE);
	return BRASERO_BURN_OK;
}

static void
brasero_burn_dialog_save_log (BraseroBurnDialog *dialog)
{
	gchar *contents;
	gchar *path = NULL;
	GtkWidget *chooser;
	GtkResponseType answer;

	chooser = gtk_file_chooser_dialog_new (_("Save current session"),
					       GTK_WINDOW (dialog),
					       GTK_FILE_CHOOSER_ACTION_SAVE,
					       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					       GTK_STOCK_SAVE, GTK_RESPONSE_OK,
					       NULL);

	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), TRUE);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser),
					     g_get_home_dir ());
	gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (chooser),
					   "brasero-session.log");

	gtk_widget_show (chooser);
	answer = gtk_dialog_run (GTK_DIALOG (chooser));
	if (answer != GTK_RESPONSE_OK) {
		gtk_widget_destroy (chooser);
		return;
	}

	path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
	gtk_widget_destroy (chooser);

	if (!path)
		return;

	if (path && *path == '\0') {
		g_free (path);
		return;
	}

	g_file_get_contents (brasero_burn_session_get_log_path (dialog->priv->session),
			     &contents,
			     NULL,
			     NULL);
	g_file_set_contents (path, contents, -1, NULL);

	g_free (contents);
	g_free (path);
}

static void
brasero_burn_dialog_message (BraseroBurnDialog *dialog,
			     const gchar *title,
			     const gchar *primary_message,
			     const gchar *secondary_message,
			     GtkMessageType type)
{
	GtkWidget *message;

	message = gtk_message_dialog_new (GTK_WINDOW (dialog),
					  GTK_DIALOG_MODAL |
					  GTK_DIALOG_DESTROY_WITH_PARENT,
					  type,
					  GTK_BUTTONS_CLOSE,
					  primary_message);

	gtk_window_set_title (GTK_WINDOW (message), title);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
						  secondary_message);
	gtk_dialog_run (GTK_DIALOG (message));
	gtk_widget_destroy (message);
}

static void
brasero_burn_dialog_show_log (BraseroBurnDialog *dialog)
{
	gint words_num;
	GtkWidget *view;
	GtkTextIter iter;
	struct stat stats;
	GtkWidget *message;
	GtkWidget *scrolled;
	GtkTextBuffer *text;
	const gchar *logfile;
	GtkTextTag *object_tag;
	GtkTextTag *domain_tag;

	message = gtk_dialog_new_with_buttons (_("Session log"),
					       GTK_WINDOW (dialog),
					       GTK_DIALOG_DESTROY_WITH_PARENT |
					       GTK_DIALOG_MODAL,
					       GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
					       NULL);
	gtk_window_set_default_size (GTK_WINDOW (message), 500, 375);
	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_set_border_width (GTK_CONTAINER (scrolled), 6);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled),
					     GTK_SHADOW_ETCHED_IN);
	gtk_box_pack_end (GTK_BOX (GTK_DIALOG (message)->vbox),
			  scrolled,
			  TRUE,
			  TRUE,
			  0);

	view = gtk_text_view_new ();
	gtk_text_view_set_editable (GTK_TEXT_VIEW (view), FALSE);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled), view);

	/* we better make sure the session log is not too big < 10 MB otherwise
	 * everything will freeze and will take a huge part of memory. If it is
	 * bigger then only show the end which is the most relevant. */
	logfile = brasero_burn_session_get_log_path (dialog->priv->session);
	if (g_stat (logfile, &stats) == -1) {
		brasero_burn_dialog_message (dialog,
					     _("Session log error"),
					     _("The session log cannot be displayed:"),
					     _("the log file could not be found."),
					     GTK_MESSAGE_ERROR);
		gtk_widget_destroy (message);
		return;
	}

	text = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	if (stats.st_size > 1 * 1024 * 1024) {
		gchar contents [1 * 1024 * 1024];
		GtkTextIter iter;
		FILE *file;

		gtk_text_buffer_get_start_iter (text, &iter);
		gtk_text_buffer_insert (text,
					&iter,
					_("This is a excerpt from the session log (the last 10 MiB):\n\n"),
					-1);

		file = g_fopen (logfile, "r");
		if (!file) {
			brasero_burn_dialog_message (dialog,
						     _("Session log error"),
						     _("The session log cannot be displayed:"),
						     strerror (errno),
						     GTK_MESSAGE_ERROR);
			gtk_widget_destroy (message);
			return;
		}

		if (fread (contents, 1, sizeof (contents), file) != sizeof (contents)) {
			brasero_burn_dialog_message (dialog,
						     _("Session log error"),
						     _("The session log cannot be displayed:"),
						     strerror (errno),
						     GTK_MESSAGE_ERROR);
			gtk_widget_destroy (message);
			return;
		}

		gtk_text_buffer_insert (text, &iter, contents, sizeof (contents));
	}
	else {
		gchar *contents;

		/* fill the buffer */
		g_file_get_contents (brasero_burn_session_get_log_path (dialog->priv->session),
				     &contents,
				     NULL,
				     NULL);
		gtk_text_buffer_set_text (text, contents, -1);
		g_free (contents);
	}

	/* create tags and apply them */
	object_tag = gtk_text_buffer_create_tag (text,
						 NULL,
						 "foreground", "red",
						 "weight", PANGO_WEIGHT_BOLD,
						 NULL);
	domain_tag = gtk_text_buffer_create_tag (text,
						 NULL,
						 "foreground", "blue",
						 NULL);
	gtk_text_buffer_get_start_iter (text, &iter);
	words_num = 0;
	while (gtk_text_iter_forward_word_end (&iter)) {
		GtkTextIter start = iter;

		gtk_text_iter_backward_word_start (&start);

		if (words_num == 2)
			gtk_text_buffer_apply_tag (text, object_tag, &start, &iter);
		else if (gtk_text_iter_starts_line (&start)) {
			words_num = 1;
			gtk_text_buffer_apply_tag (text, domain_tag, &start, &iter);
		}

		words_num ++;
	}

	/* run everything */
	gtk_widget_show_all (scrolled);
	gtk_dialog_run (GTK_DIALOG (message));

	gtk_widget_destroy (message);
}

static void
brasero_burn_dialog_notify_error (BraseroBurnDialog *dialog,
				  GError *error)
{
	gchar *secondary;
	GtkWidget *button;
	GtkWidget *message;
	GtkResponseType response;

	if (error) {
		secondary =  g_strdup (error->message);
		g_error_free (error);
	}
	else
		secondary = g_strdup (_("An unknown error occured. Check your disc"));

	if (!GTK_WIDGET_VISIBLE (dialog))
		gtk_widget_show (GTK_WIDGET (dialog));

	message = gtk_message_dialog_new (GTK_WINDOW (dialog),
					  GTK_DIALOG_DESTROY_WITH_PARENT |
					  GTK_DIALOG_MODAL,
					  GTK_MESSAGE_ERROR,
					  GTK_BUTTONS_NONE,
					  _("Error while burning:"));

	gtk_window_set_title (GTK_WINDOW (message), _("Burning error"));
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
						  "%s.",
						  secondary);
	g_free (secondary);

	button = brasero_utils_make_button (_("Save log"),
					    GTK_STOCK_SAVE_AS,
					    NULL,
					    GTK_ICON_SIZE_BUTTON);
	gtk_widget_show_all (button);
	gtk_dialog_add_action_widget (GTK_DIALOG (message), button, GTK_RESPONSE_APPLY);

	button = brasero_utils_make_button (_("View log"),
					    GTK_STOCK_EDIT,
					    NULL,
					    GTK_ICON_SIZE_BUTTON);
	gtk_widget_show_all (button);
	gtk_dialog_add_action_widget (GTK_DIALOG (message), button, GTK_RESPONSE_OK);

	button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
	gtk_widget_show (button);
	gtk_dialog_add_action_widget (GTK_DIALOG (message), button, GTK_RESPONSE_CLOSE);

	response = gtk_dialog_run (GTK_DIALOG (message));
	while (1) {
		if (response == GTK_RESPONSE_APPLY)
			brasero_burn_dialog_save_log (dialog);
		else if (response == GTK_RESPONSE_OK)
			brasero_burn_dialog_show_log (dialog);
		else
			break;

		response = gtk_dialog_run (GTK_DIALOG (message));
	}

	gtk_widget_destroy (message);
}

static gboolean
brasero_burn_dialog_success_timeout (BraseroBurnDialog *dialog)
{
	gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	dialog->priv->close_timeout = 0;

	return FALSE;
}

static void
brasero_burn_dialog_success_run (BraseroBurnDialog *dialog)
{
	gint answer;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->close_check))) {
		dialog->priv->close_timeout = g_timeout_add (TIMEOUT,
							     (GSourceFunc) brasero_burn_dialog_success_timeout,
							     dialog);

	}

	answer = gtk_dialog_run (GTK_DIALOG (dialog));

	/* remove the timeout if need be */
	if (dialog->priv->close_timeout) {
		g_source_remove (dialog->priv->close_timeout);
		dialog->priv->close_timeout = 0;
	}
}

static void
brasero_burn_dialog_notify_success (BraseroBurnDialog *dialog)
{
	gchar *primary = NULL;
	gchar *secondary = NULL;
	BraseroTrackType source;
	BraseroMedia media;
	NautilusBurnDrive *drive;

	drive = brasero_burn_session_get_burner (dialog->priv->session);
	brasero_burn_session_get_input_type (dialog->priv->session, &source);
	if (source.type != BRASERO_TRACK_TYPE_DISC)
		media = brasero_burn_session_get_dest_media (dialog->priv->session);
	else
		media = source.subtype.media;

	switch (source.type) {
	case BRASERO_TRACK_TYPE_AUDIO:
		primary = g_strdup (_("Audio CD successfully burnt"));
		secondary = g_strdup_printf (_("\"%s\" is now ready for use"), 
					     brasero_burn_session_get_label (dialog->priv->session));
		break;
	case BRASERO_TRACK_TYPE_DISC:
		if (NCB_DRIVE_GET_TYPE (drive) != NAUTILUS_BURN_DRIVE_TYPE_FILE) {
			if (media & BRASERO_MEDIUM_DVD) {
				primary = g_strdup (_("DVD successfully copied"));
				secondary = g_strdup_printf (_("DVD is now ready for use"));
			}
			else {
				primary = g_strdup (_("CD successfully copied"));
				secondary = g_strdup_printf (_("CD is now ready for use"));
			}
		}
		else {
			if (media & BRASERO_MEDIUM_DVD) {
				primary = g_strdup (_("Image of DVD successfully created"));
				secondary = g_strdup_printf (_("DVD is now ready for use"));
			}
			else {
				primary = g_strdup (_("Image of CD successfully created"));
				secondary = g_strdup_printf (_("CD is now ready for use"));
			}
		}
		break;
	case BRASERO_TRACK_TYPE_IMAGE:
		if (NCB_DRIVE_GET_TYPE (drive) != NAUTILUS_BURN_DRIVE_TYPE_FILE) {
			if (media & BRASERO_MEDIUM_DVD) {
				primary = g_strdup (_("Image successfully burnt to DVD"));
				secondary = g_strdup_printf (_("DVD is now ready for use"));
			}
			else {
				primary = g_strdup (_("Image successfully burnt to CD"));
				secondary = g_strdup_printf (_("CD is now ready for use"));
			}
		}
		break;
	default:
		if (NCB_DRIVE_GET_TYPE (drive) != NAUTILUS_BURN_DRIVE_TYPE_FILE) {
			if (media & BRASERO_MEDIUM_DVD) {
				primary = g_strdup (_("Data DVD successfully burnt"));
				secondary = g_strdup_printf (_("\"%s\" is now ready for use"),
							     brasero_burn_session_get_label (dialog->priv->session));
			}
			else {
				primary = g_strdup (_("Data CD successfully burnt"));
				secondary = g_strdup_printf (_("\"%s\" is now ready for use"),
							     brasero_burn_session_get_label (dialog->priv->session));
			}
		}
		else {
			primary = g_strdup (_("Image successfully created"));
			secondary = g_strdup_printf (_("\"%s\" is now ready for use"),
						     brasero_burn_session_get_label (dialog->priv->session));
		}
		break;
	}

	brasero_burn_dialog_activity_stop (dialog, primary);
	brasero_burn_dialog_success_run (dialog);

	g_free (primary);
	g_free (secondary);
}

static gboolean
brasero_burn_dialog_end_session (BraseroBurnDialog *dialog,
				 BraseroBurnResult result,
				 GError *error)
{
	gboolean close_dialog;

	if (dialog->priv->burn) {
		g_object_unref (dialog->priv->burn);
		dialog->priv->burn = NULL;
	}

	brasero_burn_session_stop (dialog->priv->session);

	if (result == BRASERO_BURN_CANCEL) {
		/* nothing to do */
		close_dialog = FALSE;
	}
	else if (error || result != BRASERO_BURN_OK) {
		close_dialog = FALSE;
		brasero_burn_dialog_notify_error (dialog, error);
	}
	else {
		brasero_burn_dialog_notify_success (dialog);
		close_dialog = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->close_check));
	}

	return close_dialog;
}

gboolean
brasero_burn_dialog_run (BraseroBurnDialog *dialog,
			 BraseroBurnSession *session)
{
	gint input_sig;
	gint output_sig;
	GError *error = NULL;
	gboolean close_dialog;
	BraseroBurnResult result;

	dialog->priv->session = session;
	g_object_ref (session);

	/* Leave the time to all sub systems and libs to get notified */
	brasero_burn_dialog_update_info (dialog);		
	input_sig = g_signal_connect_after (session,
					     "input-changed",
					     G_CALLBACK (brasero_burn_dialog_input_changed_cb),
					     dialog);
	output_sig = g_signal_connect_after (session,
					     "output-changed",
					     G_CALLBACK (brasero_burn_dialog_output_changed_cb),
					     dialog);

	brasero_burn_dialog_activity_start (dialog);

	/* start the recording session */
	result = brasero_burn_dialog_setup_session (dialog, &error);
	if (result == BRASERO_BURN_OK) {
		NautilusBurnDrive *drive;
		BraseroBurnFlag flags;

		drive = brasero_burn_session_get_burner (session);
		flags = brasero_burn_session_get_flags (session);

		/* basically we don't use DAO when:
		 * - we're appending to a CD/DVD
		 * - starting a multisession DVD-/+ R
		 * - we're writing to a file */
		if (!(flags & (BRASERO_BURN_FLAG_APPEND|BRASERO_BURN_FLAG_MERGE))
		&& (!NCB_MEDIA_IS (drive, BRASERO_MEDIUM_DVD|BRASERO_MEDIUM_WRITABLE)
		||  !(flags & BRASERO_BURN_FLAG_MULTI))  /* that targets DVD+/-R */
		&&   NCB_DRIVE_GET_TYPE (drive) != NAUTILUS_BURN_DRIVE_TYPE_FILE)
			brasero_burn_session_add_flag (session, BRASERO_BURN_FLAG_DAO);

		brasero_burn_session_set_num_copies (session, 1);
		result = brasero_burn_record (dialog->priv->burn,
					      session,
					      &error);
	}

	if (input_sig) {
		g_signal_handler_disconnect (session, input_sig);
		input_sig = 0;
	}

	if (output_sig) {
		g_signal_handler_disconnect (session, output_sig);
		output_sig = 0;
	}

	close_dialog = brasero_burn_dialog_end_session (dialog,
							result,
							error);

	return close_dialog;
}

static gboolean
brasero_burn_dialog_cancel_dialog (GtkWidget *toplevel)
{
	gint result;
	GtkWidget *button;
	GtkWidget *message;

	message = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					  GTK_DIALOG_DESTROY_WITH_PARENT |
					  GTK_DIALOG_MODAL,
					  GTK_MESSAGE_WARNING,
					  GTK_BUTTONS_NONE,
					  _("Do you really want to quit?"));

	gtk_window_set_title (GTK_WINDOW (message), _("Confirm"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG
						  (message),
						  _("Interrupting the process may make disc unusable."));

	gtk_dialog_add_buttons (GTK_DIALOG (message),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				NULL);

	button = brasero_utils_make_button (_("Continue"),
					    GTK_STOCK_OK,
					    NULL,
					    GTK_ICON_SIZE_BUTTON);
	gtk_widget_show_all (button);
	gtk_dialog_add_action_widget (GTK_DIALOG (message),
				      button, GTK_RESPONSE_OK);

	result = gtk_dialog_run (GTK_DIALOG (message));
	gtk_widget_destroy (message);

	return (result != GTK_RESPONSE_OK);
}

static gboolean
brasero_burn_dialog_cancel (BraseroBurnDialog *dialog)
{
	if (dialog->priv->burn) {
		BraseroBurnResult result;

		result = brasero_burn_cancel (dialog->priv->burn, TRUE);

		if (result == BRASERO_BURN_DANGEROUS) {
			if (brasero_burn_dialog_cancel_dialog (GTK_WIDGET (dialog)))
				brasero_burn_cancel (dialog->priv->burn, FALSE);
			else
				return FALSE;
		}
	}

	return TRUE;
}

static gboolean
brasero_burn_dialog_delete (GtkWidget *widget, 
			    GdkEventAny *event)
{
	BraseroBurnDialog *dialog;

	dialog = BRASERO_BURN_DIALOG (widget);

	brasero_tray_icon_set_show_dialog (BRASERO_TRAYICON (dialog->priv->tray), FALSE);
 	return TRUE;
}

static void
brasero_burn_dialog_cancel_clicked_cb (GtkWidget *button,
				       BraseroBurnDialog *dialog)
{
	/* a burning is ongoing cancel it */
	brasero_burn_dialog_cancel (dialog);
}

static void
brasero_burn_dialog_tray_cancel_cb (BraseroTrayIcon *tray,
				    BraseroBurnDialog *dialog)
{
	brasero_burn_dialog_cancel (dialog);
}

static void
brasero_burn_dialog_tray_show_dialog_cb (BraseroTrayIcon *tray,
					 gboolean show,
					 GtkWidget *dialog)
{
	/* we prevent to show the burn dialog once the success dialog has been 
	 * shown to avoid the following strange behavior:
	 * Steps:
	 * - start burning
	 * - move to another workspace (ie, virtual desktop)
	 * - when the burning finishes, double-click the notification icon
	 * - you'll be unable to dismiss the dialogues normally and their behaviour will
	 *   be generally strange */
	if (!BRASERO_BURN_DIALOG (dialog)->priv->burn)
		return;

	if (show)
		gtk_widget_show (dialog);
	else
		gtk_widget_hide (dialog);
}

static void
brasero_burn_dialog_tray_close_after_cb (BraseroTrayIcon *tray,
					 gboolean close,
					 BraseroBurnDialog *dialog)
{
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->priv->close_check), close);
}
