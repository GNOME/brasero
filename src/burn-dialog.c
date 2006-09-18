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

#ifdef HAVE_LIBNOTIFY
#include <libnotify/notify.h>
#endif

#include "utils.h"
#include "disc.h"
#include "tray.h"
#include "burn-basics.h"
#include "burn-session.h"
#include "burn-job.h"
#include "burn-imager.h"
#include "burn-dialog.h"
#include "burn-sum.h"
#include "burn-common.h"
#include "burn-md5.h"
#include "burn-local-image.h"
#include "recorder-selection.h"
#include "progress.h"
#include "brasero-sum-check.h"
#include "brasero-ncb.h"
#include "burn-task.h"

extern gint debug;

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
brasero_burn_dialog_close_clicked_cb (GtkButton *button,
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
	BraseroJob *checksum;
	BraseroJob *local_image;

	BraseroBurnSession *session;

	BraseroSumCheckCtx *file_ctx;

	gint64 isosize;
	NautilusBurnDrive *drive;
	NautilusBurnMediaType media_type;
	BraseroTrackSourceType track_type;

	GtkWidget *waiting_disc_dialog;

	GtkWidget *close_check;
	GtkWidget *progress;
	GtkWidget *header;
	GtkWidget *cancel;
	GtkWidget *image;
	BraseroTrayIcon *tray;

	GMainLoop *loop;
	gint close_timeout;

	gint mount_timeout;
};

#define TIMEOUT	10000
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

static char *
brasero_burn_dialog_get_media_type_string (BraseroBurn *burn,
					   BraseroMediaType type,
					   gboolean insert)
{
	char *message = NULL;

	if (type & BRASERO_MEDIA_WITH_DATA) {
		if (!insert) {
			if (type & BRASERO_MEDIA_REWRITABLE)
				message = g_strdup (_("replace the disc with a rewritable disc holding data."));
			else
				message = g_strdup (_("replace the disc with a disc holding data."));
		}
		else {
			if (type & BRASERO_MEDIA_REWRITABLE)
				message = g_strdup (_("insert a rewritable disc holding data."));
			else
				message = g_strdup (_("insert a disc holding data."));
		}
	}
	else if (type & BRASERO_MEDIA_WRITABLE) {
		gint64 isosize = 0;
	
		brasero_burn_status (burn,
				     NULL,
				     &isosize,
				     NULL,
				     NULL);

		if ((type & BRASERO_MEDIA_TYPE_CD) && !(type & BRASERO_MEDIA_TYPE_DVD)) {
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
		else if (!(type & BRASERO_MEDIA_TYPE_CD) && (type & BRASERO_MEDIA_TYPE_DVD)) {
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

static BraseroBurnResult
brasero_burn_dialog_insert_disc_cb (BraseroBurn *burn,
				    BraseroBurnError error,
				    BraseroMediaType type,
				    BraseroBurnDialog *dialog)
{
	gint result;
	GtkWindow *window;
	GtkWidget *message;
	gboolean hide = FALSE;
	char *main_message = NULL, *secondary_message = NULL;

	if (!GTK_WIDGET_VISIBLE (dialog)) {
		gtk_widget_show (GTK_WIDGET (dialog));
		hide = TRUE;
	}

	if (error == BRASERO_BURN_ERROR_MEDIA_BUSY) {
		main_message = g_strdup (_("The disc in the drive is busy:"));
		secondary_message = g_strdup (_("make sure another application is not using it."));
	} 
	else if (error == BRASERO_BURN_ERROR_MEDIA_NONE) {
		main_message = g_strdup (_("There is no disc in the drive:"));
		secondary_message = brasero_burn_dialog_get_media_type_string (burn, type, TRUE);
	}
	else if (error == BRASERO_BURN_ERROR_MEDIA_NOT_REWRITABLE) {
		main_message = g_strdup (_("The disc in the drive is not rewritable:"));
		secondary_message = brasero_burn_dialog_get_media_type_string (burn, type, FALSE);
	}
	else if (error == BRASERO_BURN_ERROR_MEDIA_BLANK) {
		main_message = g_strdup (_("The disc in the drive is empty:"));
		secondary_message = brasero_burn_dialog_get_media_type_string (burn, type, FALSE);
	}
	else if (error == BRASERO_BURN_ERROR_MEDIA_NOT_WRITABLE) {
		main_message = g_strdup (_("The disc in the drive is not writable:"));
		secondary_message = brasero_burn_dialog_get_media_type_string (burn, type, FALSE);
	}
	else if (error == BRASERO_BURN_ERROR_DVD_NOT_SUPPORTED) {
		main_message = g_strdup (_("The disc in the drive is a DVD:"));
		secondary_message = brasero_burn_dialog_get_media_type_string (burn, type, FALSE);
	}
	else if (error == BRASERO_BURN_ERROR_CD_NOT_SUPPORTED) {
		main_message = g_strdup (_("The disc in the drive is a CD:"));
		secondary_message = brasero_burn_dialog_get_media_type_string (burn, type, FALSE);
	}
	else if (error == BRASERO_BURN_ERROR_MEDIA_SPACE) {
		main_message = g_strdup (_("The disc in the drive is not big enough:"));
		secondary_message = brasero_burn_dialog_get_media_type_string (burn, type, FALSE);
	}
	else if (error == BRASERO_BURN_ERROR_NONE) {
		main_message = g_strdup_printf ("<b><big>%s</big></b>",
						brasero_burn_dialog_get_media_type_string (burn, type, FALSE));
	}
	else if (error == BRASERO_BURN_ERROR_RELOAD_MEDIA) {
		main_message = g_strdup (_("The disc in the drive needs to be reloaded:"));
		secondary_message = g_strdup (_("eject the disc and reload it."));
	}

	window = GTK_WINDOW (dialog);

	if (secondary_message) {
		message = gtk_message_dialog_new (window,
						  GTK_DIALOG_DESTROY_WITH_PARENT|
						  GTK_DIALOG_MODAL,
						  GTK_MESSAGE_QUESTION,
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
							      GTK_MESSAGE_QUESTION,
							      GTK_BUTTONS_CANCEL,
							      main_message);

	g_free (main_message);

	if (error == BRASERO_BURN_ERROR_MEDIA_NONE)
		gtk_window_set_title (GTK_WINDOW (message), _("Waiting for disc insertion"));
	else
		gtk_window_set_title (GTK_WINDOW (message), _("Waiting for disc replacement"));

	dialog->priv->waiting_disc_dialog = message;

	result = gtk_dialog_run (GTK_DIALOG (message));
	gtk_widget_destroy (message);

	dialog->priv->waiting_disc_dialog = NULL;

	if (hide)
		gtk_widget_hide (GTK_WIDGET (dialog));

	if (result != GTK_RESPONSE_OK)
		return BRASERO_BURN_CANCEL;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_dialog_data_loss_cb (BraseroBurn *burn,
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
					  GTK_MESSAGE_QUESTION,
					  GTK_BUTTONS_NONE,
					  _("The disc in the drive holds data:"));

	gtk_window_set_title (GTK_WINDOW (message), _("Possible loss of data"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
						 _("Do you want to erase the current disc?\nOr replace the current disc with a new disc?"));

	gtk_dialog_add_buttons (GTK_DIALOG (message),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				NULL);

	button = brasero_utils_make_button (_("Replace disc"), GTK_STOCK_REFRESH);
	gtk_widget_show_all (button);
	gtk_dialog_add_action_widget (GTK_DIALOG (message),
				      button, GTK_RESPONSE_ACCEPT);

	button = brasero_utils_make_button (_("Erase disc"), GTK_STOCK_CLEAR);
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
brasero_burn_dialog_rewritable_cb (BraseroBurn *burn,
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
					  GTK_MESSAGE_QUESTION,
					  GTK_BUTTONS_NONE,
					  _("Recording audio tracks on a rewritable disc is not advised:"));

	gtk_window_set_title (GTK_WINDOW (message), _("Rewritable disc"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
						  _("you might not be able to listen to it with stereos.\nDo you want to continue anyway?"));

	button = brasero_utils_make_button (_("Replace the disc"), GTK_STOCK_REFRESH);
	gtk_widget_show_all (button);
	gtk_dialog_add_action_widget (GTK_DIALOG (message),
				      button, GTK_RESPONSE_ACCEPT);

	gtk_dialog_add_buttons (GTK_DIALOG (message),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				NULL);

	button = brasero_utils_make_button (_("Continue"), GTK_STOCK_OK);
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
					  GTK_MESSAGE_QUESTION,
					  GTK_BUTTONS_NONE,
					  _("Some files don't have a suitable name for a Windows-compatible CD:"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
						  _("Do you want to continue with Windows compatibility disabled?"));

	gtk_window_set_title (GTK_WINDOW (message), _("Windows compatibility"));
	gtk_dialog_add_buttons (GTK_DIALOG (message),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				NULL);

	button = brasero_utils_make_button (_("Continue"), GTK_STOCK_OK);
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
brasero_burn_dialog_job_progress_changed (BraseroTask *task,
					  BraseroBurnDialog *dialog)
{
	gint64 total = 0;
	gint64 written = 0;
	glong remaining = -1;
	gdouble progress = -1.0;

	if (brasero_task_get_progress (task, &progress) != BRASERO_BURN_OK)
		return;

	brasero_task_get_total (task, &total);
	brasero_task_get_written (task, &written);
	brasero_task_get_remaining_time (task, &remaining);

	brasero_burn_dialog_progress_changed_real (dialog,
						   written,
						   total,
						   -1,
						   progress,
						   progress,
						   remaining,
						   FALSE);
}

static void
brasero_burn_dialog_progress_changed_cb (BraseroBurn *burn, 
					 gdouble overall_progress,
					 gdouble task_progress,
					 glong remaining,
					 BraseroBurnDialog *dialog)
{
	NautilusBurnMediaType type = NAUTILUS_BURN_MEDIA_TYPE_UNKNOWN;
	gint64 isosize = -1;
	gint64 written = -1;
	gint64 rate = -1;

	brasero_burn_status (dialog->priv->burn,
			     &type,
			     &isosize,
			     &written,
			     &rate);

	if (type != NAUTILUS_BURN_MEDIA_TYPE_UNKNOWN)
		dialog->priv->media_type = type;

	brasero_burn_dialog_progress_changed_real (dialog,
						   written,
						   isosize,
						   rate,
						   overall_progress,
						   task_progress,
						   remaining,
						   dialog->priv->media_type > NAUTILUS_BURN_MEDIA_TYPE_CDRW);
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
brasero_burn_dialog_job_action_changed (BraseroTask *task,
					BraseroBurnAction action,
					BraseroBurnDialog *dialog)
{
	gchar *string;

	if (brasero_task_get_action_string (task, action, &string) != BRASERO_BURN_OK)
		string = g_strdup (brasero_burn_action_to_string (action));

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
	gtk_dialog_add_action_widget (GTK_DIALOG (obj), obj->priv->cancel, GTK_RESPONSE_CANCEL);
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

	if (cobj->priv->local_image) {
		brasero_job_cancel (cobj->priv->local_image, TRUE);
		g_object_unref (cobj->priv->local_image);
		cobj->priv->local_image = NULL;
	}

	if (cobj->priv->checksum) {
		brasero_job_cancel (cobj->priv->checksum, TRUE);
		g_object_unref (cobj->priv->checksum);
		cobj->priv->checksum = NULL;
	}

	if (cobj->priv->tray) {
		g_object_unref (cobj->priv->tray);
		cobj->priv->tray = NULL;
	}

	if (cobj->priv->close_timeout) {
		g_source_remove (cobj->priv->close_timeout);
		cobj->priv->close_timeout = 0;
	}

	if (cobj->priv->mount_timeout) {
		g_source_remove (cobj->priv->mount_timeout);
		cobj->priv->mount_timeout = 0;
	}

	if (cobj->priv->loop) {
		g_main_loop_quit (cobj->priv->loop);
		g_main_loop_unref (cobj->priv->loop);
		cobj->priv->loop = NULL;
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

	g_signal_handlers_disconnect_by_func (dialog->priv->cancel,
					      brasero_burn_dialog_close_clicked_cb,
					      dialog);

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
	g_signal_connect (dialog->priv->cancel,
			  "clicked",
			  G_CALLBACK (brasero_burn_dialog_close_clicked_cb),
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
	gtk_window_set_urgency_hint (GTK_WINDOW (dialog), FALSE);
}

static void
brasero_burn_dialog_update_info (BraseroBurnDialog *dialog)
{
	char *title = NULL;
	char *header = NULL;
	GdkPixbuf *pixbuf = NULL;
	NautilusBurnDrive *drive;
	NautilusBurnMediaType media_type;

	char *types [] = { 	NULL,
				NULL,
				NULL,
				"gnome-dev-cdrom",
				"gnome-dev-disc-cdr",
				"gnome-dev-disc-cdrw",
				"gnome-dev-disc-dvdrom",
				"gnome-dev-disc-dvdr",
				"gnome-dev-disc-dvdrw",
				"gnome-dev-disc-dvdram",
				"gnome-dev-disc-dvdr-plus",
				"gnome-dev-disc-dvdrw", /* FIXME */
				"gnome-dev-disc-dvdr-plus" /* FIXME */,
				NULL };

	drive = dialog->priv->drive;
	if (NCB_DRIVE_GET_TYPE (drive) == NAUTILUS_BURN_DRIVE_TYPE_FILE) {
		/* we are creating an image to the hard drive */
		pixbuf = brasero_utils_get_icon_for_mime ("application/x-cd-image", 48);

		header = g_strdup_printf ("<big><b>Creating image</b></big>");
		title = g_strdup (_("Creating image"));
		goto end;
	}

	media_type = nautilus_burn_drive_get_media_type (drive);

	if (media_type > NAUTILUS_BURN_MEDIA_TYPE_CDRW) {
		if (dialog->priv->track_type == BRASERO_TRACK_SOURCE_DATA
		||  dialog->priv->track_type == BRASERO_TRACK_SOURCE_IMAGE
		||  dialog->priv->track_type == BRASERO_TRACK_SOURCE_GRAFTS) {
			title = g_strdup (_("Burning DVD"));
			header = g_strdup (_("<big><b>Burning data DVD</b></big>"));
		}
		else if (dialog->priv->track_type == BRASERO_TRACK_SOURCE_IMAGE) {
			title = g_strdup (_("Burning DVD"));
			header = g_strdup (_("<big><b>Burning image to DVD</b></big>"));
		}
		else if (dialog->priv->track_type == BRASERO_TRACK_SOURCE_DISC) {
			title = g_strdup (_("Copying DVD"));
			header = g_strdup (_("<big><b>Copying data DVD</b></big>"));
		}
	}
	else if (dialog->priv->track_type == BRASERO_TRACK_SOURCE_AUDIO
	      ||  dialog->priv->track_type == BRASERO_TRACK_SOURCE_SONG) {
		title = g_strdup (_("Burning CD"));
		header = g_strdup_printf (_("<big><b>Burning audio CD</b></big>"));
		pixbuf = brasero_utils_get_icon ("gnome-dev-cdrom-audio", 48);
	}
	else if (dialog->priv->track_type == BRASERO_TRACK_SOURCE_DATA
	      ||  dialog->priv->track_type == BRASERO_TRACK_SOURCE_GRAFTS) {
		title = g_strdup (_("Burning CD"));
		header = g_strdup_printf (_("<big><b>Burning data CD</b></big>"));
	}
	else if (dialog->priv->track_type == BRASERO_TRACK_SOURCE_DISC) {
		title = g_strdup (_("Copying CD"));
		header = g_strdup(_("<big><b>Copying CD</b></big>"));
	}
	else if (dialog->priv->track_type == BRASERO_TRACK_SOURCE_IMAGE) {
		title = g_strdup (_("Burning CD"));
		header = g_strdup (_("<big><b>Burning image to CD</b></big>"));
	}

	if (!pixbuf) {
		pixbuf = brasero_utils_get_icon (types [media_type], 48);
		if (!pixbuf)
			pixbuf = brasero_utils_get_icon ("gnome-dev-removable", 48);
	}

end:

	gtk_window_set_title (GTK_WINDOW (dialog), title);
	g_free (title);

	gtk_image_set_from_pixbuf (GTK_IMAGE (dialog->priv->image), pixbuf);
	g_object_unref (pixbuf);

	gtk_label_set_text (GTK_LABEL (dialog->priv->header), header);
	gtk_label_set_use_markup (GTK_LABEL (dialog->priv->header), TRUE);
	g_free (header);
}

static void
brasero_burn_dialog_media_added_cb (NautilusBurnDriveMonitor *monitor,
				    NautilusBurnDrive *drive,
				    BraseroBurnDialog *dialog)
{
	brasero_burn_dialog_update_info (dialog);

	/* we might have a dialog waiting for the 
	 * insertion of a disc if so close it */
	if (dialog->priv->waiting_disc_dialog) {
		gtk_dialog_response (GTK_DIALOG (dialog->priv->waiting_disc_dialog),
				     GTK_RESPONSE_OK);
	}
}

static void
brasero_burn_dialog_media_removed_cb (NautilusBurnDriveMonitor *monitor,
				      NautilusBurnDrive *drive,
				      BraseroBurnDialog *dialog)
{
	GdkPixbuf *pixbuf;

	pixbuf = brasero_utils_get_icon ("gnome-dev-removable", 48);
	gtk_image_set_from_pixbuf (GTK_IMAGE (dialog->priv->image), pixbuf);
	g_object_unref (pixbuf);
}

static BraseroBurnResult
brasero_burn_dialog_job_get_track (BraseroBurnDialog *dialog,
				   BraseroJob *job,
				   const BraseroTrackSource *source,
				   BraseroTrackSource **retval,
				   GError **error)
{
	BraseroBurnResult result;
	BraseroTask *task;

	result = brasero_job_set_source (job,
					 source,
					 error);

	if (result != BRASERO_BURN_OK)
		return result;

	brasero_job_set_debug (job, debug);
	brasero_job_set_session (job, dialog->priv->session);

	/* NOTE: here we don't obey the flags as that data is ours and not
	 * part of the resulting track. */
	result = brasero_imager_set_output (BRASERO_IMAGER (job),
					    NULL,
					    TRUE, /* we don't overwrite */
					    TRUE, /* we clean everything */
					    error);

	if (result != BRASERO_BURN_OK)
		return result;

	task = brasero_task_new ();
	brasero_task_set_total (task, dialog->priv->isosize);

	g_signal_connect (task,
			  "progress_changed",
			  G_CALLBACK (brasero_burn_dialog_job_progress_changed),
			  dialog);
	g_signal_connect (task,
			  "action_changed",
			  G_CALLBACK (brasero_burn_dialog_job_action_changed),
			  dialog);

	brasero_job_set_task (job, task);
	result = brasero_imager_get_track (BRASERO_IMAGER (job),
					   retval,
					   error);
	brasero_job_set_task (job, NULL);
	g_object_unref (task);

	return result;
}

static BraseroBurnResult
brasero_burn_dialog_get_local_source (BraseroBurnDialog *dialog,
				      const BraseroTrackSource *source,
				      BraseroTrackSource **local,
				      GError **error)
{
	if (source->type == BRASERO_TRACK_SOURCE_DISC
	||  source->type == BRASERO_TRACK_SOURCE_INF) {
		*local = brasero_track_source_copy (source);
		return BRASERO_BURN_OK;
	}

	if (dialog->priv->local_image)
		g_object_unref (dialog->priv->local_image);

	dialog->priv->local_image = g_object_new (BRASERO_TYPE_LOCAL_IMAGE, NULL);
	return brasero_burn_dialog_job_get_track (dialog,
						   dialog->priv->local_image,
						   source,
						   local,
						   error);
}

static BraseroBurnResult
brasero_burn_dialog_get_checksumed_source (BraseroBurnDialog *dialog,
					   const BraseroTrackSource *source,
					   BraseroTrackSource **checksum,
					   GError **error)
{
	if (source->type != BRASERO_TRACK_SOURCE_DATA) {
		*checksum = brasero_track_source_copy (source);
		return BRASERO_BURN_OK;
	}

	if (dialog->priv->checksum)
		g_object_unref (dialog->priv->checksum);

	dialog->priv->checksum = g_object_new (BRASERO_TYPE_BURN_SUM, NULL);
	if (!dialog->priv->checksum)
		return BRASERO_BURN_ERR;

	return brasero_burn_dialog_job_get_track (dialog,
						   dialog->priv->checksum,
						   source,
						   checksum,
						   error);
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
brasero_burn_dialog_integrity_ok (BraseroBurnDialog *dialog)
{
	brasero_burn_dialog_message (dialog,
				     _("Data integrity check"),
				     _("The file integrity check succeeded:"),
				     _("there seems to be no corrupted file on the disc."),
				     GTK_MESSAGE_INFO);
}

static void
brasero_burn_dialog_integrity_error (BraseroBurnDialog *dialog,
				     GError *error)
{
	brasero_burn_dialog_message (dialog,
				     _("Data integrity check"),
				     _("The data integrity check could not be performed:"),
				     error ? error->message:_("unknown error."),
				     GTK_MESSAGE_ERROR);
}

static void
brasero_burn_dialog_integrity_wrong_sums (BraseroBurnDialog *dialog)
{
	brasero_burn_dialog_message (dialog,
				     _("Data integrity check"),
				     _("The file integrity check failed:"),
				     _("some files may be corrupted on the disc."),
				     GTK_MESSAGE_ERROR);
}

static void
brasero_burn_dialog_close_reload_disc_dlg (NautilusBurnDriveMonitor *monitor,
					   NautilusBurnDrive *drive,
					   BraseroBurnDialog *dialog)
{
	/* we might have a dialog waiting for the 
	 * insertion of a disc if so close it */
	if (dialog->priv->waiting_disc_dialog) {
		gtk_dialog_response (GTK_DIALOG (dialog->priv->waiting_disc_dialog),
				     GTK_RESPONSE_OK);
	}
}

static BraseroBurnResult
brasero_burn_dialog_reload_disc_dlg (BraseroBurnDialog *dialog,
				     const gchar *primary,
				     const gchar *secondary)
{
	gint answer;
	gint added_id;
	GtkWidget *message;
	NautilusBurnDriveMonitor *monitor;

	/* display a dialog to the user explaining what we're
	 * going to do, that is reload the disc before checking */
	message = gtk_message_dialog_new (GTK_WINDOW (dialog),
					  GTK_DIALOG_MODAL |
					  GTK_DIALOG_DESTROY_WITH_PARENT,
					  GTK_MESSAGE_INFO,
					  GTK_BUTTONS_CANCEL,
					  primary);

	gtk_window_set_title (GTK_WINDOW (message), _("Data integrity check"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
						  secondary);

	monitor = nautilus_burn_get_drive_monitor ();
	added_id = g_signal_connect_after (monitor,
					   "media-added",
					   G_CALLBACK (brasero_burn_dialog_close_reload_disc_dlg),
					   dialog);

	dialog->priv->waiting_disc_dialog = message;
	answer = gtk_dialog_run (GTK_DIALOG (message));
	dialog->priv->waiting_disc_dialog = NULL;

	gtk_widget_destroy (message);

	g_signal_handler_disconnect (monitor, added_id);

	if (answer == GTK_RESPONSE_CANCEL)
		return BRASERO_BURN_CANCEL;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_dialog_check_image_integrity (BraseroBurnDialog *dialog,
					   NautilusBurnDrive *drive,
					   const BraseroTrackSource *source,
					   GError **error)
{
	gboolean success;
	BraseroBurnResult result;
	BraseroTrackSource track;
	BraseroTrackSource *checksum_disc, *checksum_track;

	/* NOTE: since we don't want to destroy that track we don't ref drive */
	track.type = BRASERO_TRACK_SOURCE_DISC;
	track.format = BRASERO_IMAGE_FORMAT_NONE;
	track.contents.drive.disc = drive;

	if (dialog->priv->checksum)
		g_object_unref (dialog->priv->checksum);

	dialog->priv->checksum = g_object_new (BRASERO_TYPE_BURN_SUM, NULL);
	if (!dialog->priv->checksum)
		return BRASERO_BURN_ERR;

	result = brasero_burn_dialog_job_get_track (dialog,
						    dialog->priv->checksum,
						    &track,
						    &checksum_disc,
						    error);
	if (result != BRASERO_BURN_OK)
		return result;

	if (source->type == BRASERO_TRACK_SOURCE_DISC
	&&  nautilus_burn_drive_equal (drive, source->contents.drive.disc)) {
		nautilus_burn_drive_eject (drive);
		result = brasero_burn_dialog_reload_disc_dlg (dialog,
							      _("The source media needs to be reloaded:"),
							      _("Please insert it again."));
		if (result == BRASERO_BURN_CANCEL) {
			brasero_track_source_free (checksum_disc);
			return result;
		}
	}

	/* now that we have a checksum for the drive we compare
	 * it to the one of the image */
	result = brasero_burn_dialog_job_get_track (dialog,
						    dialog->priv->checksum,
						    source,
						    &checksum_track,
						    error);
	if (result != BRASERO_BURN_OK) {
		brasero_track_source_free (checksum_disc);
		return result;
	}

	/* compare the checksums */
	success = brasero_md5_equal (&checksum_track->contents.sum.md5,
				     &checksum_disc->contents.sum.md5);

	brasero_track_source_free (checksum_disc);
	brasero_track_source_free (checksum_track);

	if (success)
		return BRASERO_BURN_OK;

	return BRASERO_BURN_ERR;
}

static gboolean
brasero_burn_dialog_check_integrity_report (gpointer user_data)
{
	BraseroBurnDialog *dialog = BRASERO_BURN_DIALOG (user_data);

	if (dialog->priv->file_ctx) {
		gint checked, total;
		gdouble progress;

		brasero_sum_check_progress (dialog->priv->file_ctx,
					    &checked,
					    &total);

		progress = (gdouble) checked / (gdouble) total;
		brasero_burn_progress_set_status (BRASERO_BURN_PROGRESS (dialog->priv->progress),
						  FALSE,
						  progress,
						  progress,
						  -1,
						  -1,
						  -1,
						  -1);
	}

	return TRUE;
}

static BraseroBurnResult
brasero_burn_dialog_check_files_integrity (BraseroBurnDialog *dialog,
					   NautilusBurnDrive *drive,
					   GError **error)
{
	gint id;
	BraseroBurnResult result;
	gchar *mount_point;
	gboolean mounted_by_us;
	GSList *wrong_sums = NULL;

	brasero_burn_progress_set_action (BRASERO_BURN_PROGRESS (dialog->priv->progress),
					  BRASERO_BURN_ACTION_CHECKSUM,
					  _("Checking files integrity"));

	/* mount and fetch the mount point for the media */
	mount_point = NCB_DRIVE_GET_MOUNT_POINT (drive,
						 &mounted_by_us,
						 error);
	if (!mount_point)
		return BRASERO_BURN_ERR;

	id = g_timeout_add (500,
			    brasero_burn_dialog_check_integrity_report,
			    dialog);

	/* check the sum of every file */
	dialog->priv->file_ctx = brasero_sum_check_new ();
	result = brasero_sum_check (dialog->priv->file_ctx,
				    mount_point,
				    &wrong_sums,
				    error);

	brasero_sum_check_free (dialog->priv->file_ctx);
	g_free (mount_point);

	g_source_remove (id);

	if (wrong_sums) {
		g_slist_foreach (wrong_sums, (GFunc) g_free, NULL);
		g_slist_free (wrong_sums);

		result = BRASERO_BURN_ERR;
	}

	if (mounted_by_us)
		NCB_DRIVE_UNMOUNT (drive, NULL);

	return result;
}

typedef struct {
	GtkWidget *button;
	BraseroBurnDialog *dialog;
	NautilusBurnDrive *burner;
	const BraseroTrackSource *source;
} IntegrityCheckData;

static gboolean
brasero_burn_dialog_integrity_start (gpointer func_data)
{
	gchar *header;
	gboolean success;
	GError *error = NULL;
	BraseroBurnDialog *dialog;
	IntegrityCheckData *data = func_data;

	dialog = data->dialog;

	header = g_strdup (gtk_label_get_text (GTK_LABEL (dialog->priv->header)));

	brasero_burn_dialog_activity_start (dialog);
	gtk_label_set_text (GTK_LABEL (dialog->priv->header), 
			    "<b><big>Performing integrity check</big></b>");
	gtk_label_set_use_markup (GTK_LABEL (dialog->priv->header), TRUE);

	if (data->source->type == BRASERO_TRACK_SOURCE_DATA)
		success = brasero_burn_dialog_check_files_integrity (dialog,
								     data->burner,
								     &error);
	else
		success = brasero_burn_dialog_check_image_integrity (dialog, 
								     data->burner,
								     data->source,
								     &error);

	if (error) {
		brasero_burn_dialog_integrity_error (dialog, error);
		g_error_free (error);
	}
	else if (success != BRASERO_BURN_OK)
		brasero_burn_dialog_integrity_wrong_sums (dialog);
	else
		brasero_burn_dialog_integrity_ok (dialog);

	brasero_burn_progress_set_action (BRASERO_BURN_PROGRESS (dialog->priv->progress),
					  BRASERO_BURN_ACTION_FINISHED,
					  _("Success"));

	gtk_widget_set_sensitive (GTK_WIDGET (data->button), TRUE);
	brasero_burn_dialog_activity_stop (dialog, header);
	g_free (header);

	return FALSE;
}

static void
brasero_burn_dialog_integrity_button_pressed (GtkButton *button,
					      IntegrityCheckData *data)
{
	BraseroBurnResult result;
	BraseroBurnDialog *dialog;

	dialog = data->dialog;

	/* we remove the close timeout */
	if (dialog->priv->close_timeout) {
		g_source_remove (dialog->priv->close_timeout);
		dialog->priv->close_timeout = 0;
	}

	/* display a dialog to the user explaining what we're
	 * going to do, that is reload the disc before checking */
	result = brasero_burn_dialog_reload_disc_dlg (dialog,
						      _("The burnt media needs to be reloaded to perform integrity check:"),
						      _("please, insert it again."));
	if (result == BRASERO_BURN_CANCEL)
		return;

	/* this to leave the time to gnome-vfs to see a new drive was inserted */
	data->button = GTK_WIDGET (button);
	gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);

	/* we give the disc 2 seconds to settle down before trying to mount */
	dialog->priv->mount_timeout = g_timeout_add (2000,
						     brasero_burn_dialog_integrity_start,
						     data);
}

static BraseroBurnResult
brasero_burn_dialog_setup_session (BraseroBurnDialog *dialog,
				   const BraseroTrackSource *source,
				   BraseroTrackSource **track,
				   gboolean checksum,
				   GError **error)
{
	BraseroTrackSource *local_track;
	BraseroBurnResult result;

	dialog->priv->session = brasero_burn_session_new ();
	brasero_burn_session_start (dialog->priv->session);

	/* make sure all files in track are local if not download */
	result = brasero_burn_dialog_get_local_source (dialog,
						       source,
						       &local_track,
						       error);

	if (result != BRASERO_BURN_OK)
		return result;

	/* generates a checksum if the user wants it */
	if (checksum && local_track->type == BRASERO_TRACK_SOURCE_DATA) {
		result = brasero_burn_dialog_get_checksumed_source (dialog,
								    local_track,
								    track,
								    error);
		brasero_track_source_free (local_track);
		if (result != BRASERO_BURN_OK)
			return result;
	}
	else
		*track = local_track;

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

	brasero_burn_set_session (dialog->priv->burn,
				  dialog->priv->session);

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

#ifdef HAVE_LIBNOTIFY

static void
brasero_burn_dialog_notify_daemon_close (NotifyNotification *notification,
					 BraseroBurnDialog *dialog)
{
	if (dialog->priv->loop
	&&  g_main_loop_is_running (dialog->priv->loop))
		g_main_loop_quit (dialog->priv->loop);
}

gboolean
brasero_burn_dialog_notify_daemon (BraseroBurnDialog *dialog,
				   const gchar *primary,
				   const gchar *secondary)
{
	NotifyNotification *notification;
	gboolean result;

	notification = notify_notification_new (primary,
						secondary,
						GTK_STOCK_CDROM,
						GTK_STATUS_ICON (dialog->priv->tray));
	if (!notification)
		return FALSE;

	g_signal_connect (notification,
			  "closed",
			  G_CALLBACK (brasero_burn_dialog_notify_daemon_close),
			  dialog);

	notify_notification_set_timeout (notification, TIMEOUT);
	notify_notification_set_urgency (notification, NOTIFY_URGENCY_NORMAL);
	result = notify_notification_show (notification, NULL);

	/* now we wait for the notification to disappear or for the user to
	 * click on the icon in the tray */
	dialog->priv->loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (dialog->priv->loop);

	if (dialog->priv->loop) {
		g_main_loop_unref (dialog->priv->loop);
		dialog->priv->loop = NULL;
	}

	return result;
}

#endif

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
brasero_burn_dialog_show_log (BraseroBurnDialog *dialog)
{
	gint words_num;
	gchar *contents;
	GtkWidget *view;
	GtkTextIter iter;
	GtkWidget *message;
	GtkWidget *scrolled;
	GtkTextBuffer *text;
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

	/* fill the buffer */
	text = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	g_file_get_contents (brasero_burn_session_get_log_path (dialog->priv->session),
			     &contents,
			     NULL,
			     NULL);
	gtk_text_buffer_set_text (text, contents, -1);
	g_free (contents);

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

	button = brasero_utils_make_button (_("Save log"), GTK_STOCK_SAVE_AS);
	gtk_widget_show_all (button);
	gtk_dialog_add_action_widget (GTK_DIALOG (message), button, GTK_RESPONSE_APPLY);

	button = brasero_utils_make_button (_("View log"), GTK_STOCK_EDIT);
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
	if (dialog->priv->loop
	&&  g_main_loop_is_running (dialog->priv->loop))
		g_main_loop_quit (dialog->priv->loop);

	return FALSE;
}

static void
brasero_burn_dialog_close_clicked_cb (GtkButton *button,
				      BraseroBurnDialog *dialog)
{
	if (dialog->priv->loop
	&&  g_main_loop_is_running (dialog->priv->loop))
		g_main_loop_quit (dialog->priv->loop);
}

static void
brasero_burn_dialog_success_run (BraseroBurnDialog *dialog)
{
	dialog->priv->loop = g_main_loop_new (NULL, FALSE);
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->close_check))) {
		dialog->priv->close_timeout = g_timeout_add (TIMEOUT,
							     (GSourceFunc) brasero_burn_dialog_success_timeout,
							     dialog);

	}

	g_main_loop_run (dialog->priv->loop);
	g_main_loop_unref (dialog->priv->loop);
	dialog->priv->close_timeout = 0;
	dialog->priv->loop = NULL;
}

static void
brasero_burn_dialog_notify_success (BraseroBurnDialog *dialog,
				    NautilusBurnDrive *burner,
				    const BraseroTrackSource *track,
				    gboolean checksum)
{
	gchar *primary = NULL;
	gchar *secondary = NULL;
	NautilusBurnDrive *drive;
	IntegrityCheckData check_data = {0,};

	drive = dialog->priv->drive;

	switch (track->type) {
	case BRASERO_TRACK_SOURCE_SONG:
		primary = g_strdup (_("Audio CD successfully burnt"));
		secondary = g_strdup_printf (_("\"%s\" is now ready for use"), track->contents.songs.album);
		break;
	case BRASERO_TRACK_SOURCE_DISC:
		if (NCB_DRIVE_GET_TYPE (drive) != NAUTILUS_BURN_DRIVE_TYPE_FILE) {
			if (dialog->priv->media_type > NAUTILUS_BURN_MEDIA_TYPE_CDRW) {
				primary = g_strdup (_("DVD successfully copied"));
				secondary = g_strdup_printf (_("DVD is now ready for use"));
			}
			else {
				primary = g_strdup (_("CD successfully copied"));
				secondary = g_strdup_printf (_("CD is now ready for use"));
			}
		}
		else {
			if (dialog->priv->media_type > NAUTILUS_BURN_MEDIA_TYPE_CDRW) {
				primary = g_strdup (_("Image of DVD successfully created"));
				secondary = g_strdup_printf (_("DVD is now ready for use"));
			}
			else {
				primary = g_strdup (_("Image of CD successfully created"));
				secondary = g_strdup_printf (_("CD is now ready for use"));
			}
		}
		break;
	case BRASERO_TRACK_SOURCE_IMAGE:
		if (NCB_DRIVE_GET_TYPE (drive) != NAUTILUS_BURN_DRIVE_TYPE_FILE) {
			if (dialog->priv->media_type > NAUTILUS_BURN_MEDIA_TYPE_CDRW) {
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
			if (dialog->priv->media_type > NAUTILUS_BURN_MEDIA_TYPE_CDRW) {
				primary = g_strdup (_("Data DVD successfully burnt"));
				secondary = g_strdup_printf (_("\"%s\" is now ready for use"), track->contents.data.label);
			}
			else {
				primary = g_strdup (_("Data CD successfully burnt"));
				secondary = g_strdup_printf (_("\"%s\" is now ready for use"), track->contents.data.label);
			}
		}
		else {
			primary = g_strdup (_("Image successfully created"));
			secondary = g_strdup_printf (_("\"%s\" is now ready for use"), track->contents.data.label);
		}
		break;
	}

	brasero_burn_dialog_activity_stop (dialog, primary);

	if (checksum) {
		GtkWidget *button;

		button = brasero_utils_make_button (_("Check integrity"),
						    GTK_STOCK_FIND);
		gtk_widget_show_all (button);

		check_data.dialog = dialog;
		check_data.burner = burner;
		check_data.source = track;
		g_signal_connect (button,
				  "clicked",
				  G_CALLBACK (brasero_burn_dialog_integrity_button_pressed),
				  &check_data);
		gtk_dialog_add_action_widget (GTK_DIALOG (dialog),
					      button,
					      GTK_RESPONSE_OK);
	}

#ifdef HAVE_LIBNOTIFY

	if (!GTK_WIDGET_VISIBLE (GTK_WIDGET (dialog))) {
		gchar *message;

		message = g_strdup_printf ("%s.", primary);
		brasero_burn_dialog_notify_daemon (dialog,
						   primary,
						   secondary);
		g_free (message);
	}
	else
		brasero_burn_dialog_success_run (dialog);

#else

	if (!GTK_WIDGET_VISIBLE (GTK_WIDGET (dialog)))
		brasero_burn_dialog_success_run (dialog);

#endif

	g_free (primary);
	g_free (secondary);
}

static gboolean
brasero_burn_dialog_end_session (BraseroBurnDialog *dialog,
				 NautilusBurnDrive *burner,
				 const BraseroTrackSource *track,
				 gboolean checksum,
				 BraseroBurnResult result,
				 GError *error)
{
	gboolean close_dialog;

	if (dialog->priv->local_image) {
		g_object_unref (dialog->priv->local_image);
		dialog->priv->local_image = NULL;
	}

	if (dialog->priv->checksum) {
		g_object_unref (dialog->priv->checksum);
		dialog->priv->checksum = NULL;
	}

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
		brasero_burn_dialog_notify_error (dialog,
						  error);
	}
	else {
		brasero_burn_dialog_notify_success (dialog,
						    burner,
						    track,
						    checksum);

		close_dialog = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->priv->close_check));
	}

	return close_dialog;
}

gboolean
brasero_burn_dialog_run (BraseroBurnDialog *dialog,
			 NautilusBurnDrive *drive,
			 gint speed,
			 const gchar *output,
			 const BraseroTrackSource *source,
			 BraseroBurnFlag flags,
			 gint64 sectors,
			 gboolean checksum)
{
	gint added_id;
	gint removed_id;
	GError *error = NULL;
	gboolean close_dialog;
	BraseroBurnResult result;
	BraseroTrackSource *track = NULL;
	NautilusBurnDriveMonitor *monitor;

	dialog->priv->track_type = source->type;
	dialog->priv->isosize = sectors * 2048;

	if (debug)
		flags |= BRASERO_BURN_FLAG_DEBUG;

	/* try to get the media type for the title */
	if (NCB_DRIVE_GET_TYPE (drive) == NAUTILUS_BURN_DRIVE_TYPE_FILE
	&&  source->type == BRASERO_TRACK_SOURCE_DISC)
		dialog->priv->drive = source->contents.drive.disc;
	else
		dialog->priv->drive = drive;

	nautilus_burn_drive_ref (dialog->priv->drive);
	nautilus_burn_drive_ref (drive);

	/* Leave the time to all sub systems and libs to get notified */
	monitor = nautilus_burn_get_drive_monitor ();
	added_id = g_signal_connect_after (monitor,
					   "media-added",
					   G_CALLBACK (brasero_burn_dialog_media_added_cb),
					   dialog);
	removed_id = g_signal_connect_after (monitor,
					     "media-removed",
					     G_CALLBACK (brasero_burn_dialog_media_removed_cb),
					     dialog);

	brasero_burn_dialog_update_info (dialog);		
	brasero_burn_dialog_activity_start (dialog);

	/* start the recording session */
	result = brasero_burn_dialog_setup_session (dialog,
						    source,
						    &track,
						    checksum,
						    &error);

	if (result == BRASERO_BURN_OK) {
		result = brasero_burn_record (dialog->priv->burn,
					      flags,
					      drive,
					      speed,
					      track,
					      output,
					      &error);
	}

	if (added_id) {
		g_signal_handler_disconnect (monitor, added_id);
		added_id = 0;
	}

	if (removed_id) {
		g_signal_handler_disconnect (monitor, removed_id);
		removed_id = 0;
	}

	checksum = NCB_DRIVE_GET_TYPE (drive) != NAUTILUS_BURN_DRIVE_TYPE_FILE &&
		   (checksum ||
		   (source->type == BRASERO_TRACK_SOURCE_IMAGE && (source->format & BRASERO_IMAGE_FORMAT_ISO)) ||
		   (source->type == BRASERO_TRACK_SOURCE_DISC));
	
	close_dialog = brasero_burn_dialog_end_session (dialog,
							drive,
							track,
							checksum,
							result,
							error);

	nautilus_burn_drive_unref (drive);
	nautilus_burn_drive_unref (dialog->priv->drive);
	dialog->priv->drive = NULL;

	if (track)
		brasero_track_source_free (track);

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
					  GTK_MESSAGE_QUESTION,
					  GTK_BUTTONS_NONE,
					  _("Do you really want to quit?"));

	gtk_window_set_title (GTK_WINDOW (message), _("Confirm"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG
						  (message),
						  _("Interrupting the process may make disc unusable."));

	gtk_dialog_add_buttons (GTK_DIALOG (message),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				NULL);

	button = brasero_utils_make_button (_("Continue"), GTK_STOCK_OK);
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
	if (dialog->priv->file_ctx)
		brasero_sum_check_cancel (dialog->priv->file_ctx);
	else if (dialog->priv->local_image
	      &&  brasero_job_is_running (dialog->priv->local_image))
		brasero_job_cancel (dialog->priv->local_image, TRUE);
	else if (dialog->priv->checksum
	      &&  brasero_job_is_running (dialog->priv->checksum))
		brasero_job_cancel (dialog->priv->checksum, TRUE);
	else if (dialog->priv->burn) {
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
