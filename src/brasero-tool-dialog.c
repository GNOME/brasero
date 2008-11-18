/***************************************************************************
 *            brasero-tool-dialog.c
 *
 *  ven sep  1 19:45:01 2006
 *  Copyright  2006  Rouquier Philippe
 *  bonfire-app@wanadoo.fr
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtklabel.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtkmessagedialog.h>

#include "brasero-utils.h"
#include "brasero-progress.h"
#include "brasero-medium-selection.h"
#include "brasero-tool-dialog.h"
#include "burn-session.h"
#include "burn.h"
#include "burn-medium.h"
#include "burn-drive.h"

G_DEFINE_TYPE (BraseroToolDialog, brasero_tool_dialog, GTK_TYPE_DIALOG);

struct _BraseroToolDialogPrivate {
	GtkWidget *upper_box;
	GtkWidget *lower_box;
	GtkWidget *selector;
	GtkWidget *progress;
	GtkWidget *button;
	GtkWidget *options;
	GtkWidget *cancel;

	BraseroBurn *burn;

	gboolean running;
	gboolean close;
};

static GtkDialogClass *parent_class = NULL;

static void
brasero_tool_dialog_media_error (BraseroToolDialog *self)
{
	brasero_utils_message_dialog (GTK_WIDGET (self),
				     _("The operation cannot be performed."),
				     _("The disc is not supported"),
				     GTK_MESSAGE_ERROR);
}

static void
brasero_tool_dialog_media_busy (BraseroToolDialog *self)
{
	gchar *string;

	string = g_strdup_printf ("%s. %s",
				  _("The drive is busy"),
				  _("Make sure another application is not using it"));
	brasero_utils_message_dialog (GTK_WIDGET (self),
				     _("The operation cannot be performed."),
				     string,
				     GTK_MESSAGE_ERROR);
	g_free (string);
}

static void
brasero_tool_dialog_no_media (BraseroToolDialog *self)
{
	brasero_utils_message_dialog (GTK_WIDGET (self),
				     _("The operation cannot be performed."),
				     _("The drive is empty"),
				     GTK_MESSAGE_ERROR);
}

void
brasero_tool_dialog_set_progress (BraseroToolDialog *self,
				  gdouble overall_progress,
				  gdouble task_progress,
				  glong remaining,
				  gint size_mb,
				  gint written_mb)
{
	brasero_burn_progress_set_status (BRASERO_BURN_PROGRESS (self->priv->progress),
					  FALSE, /* no need for the media here since speed is not specified */
					  overall_progress,
					  task_progress,
					  remaining,
					  -1,
					  -1,
					  -1);
}

void
brasero_tool_dialog_set_action (BraseroToolDialog *self,
				BraseroBurnAction action,
				const gchar *string)
{
	brasero_burn_progress_set_action (BRASERO_BURN_PROGRESS (self->priv->progress),
					  action,
					  string);
}

static void
brasero_tool_dialog_progress_changed (BraseroBurn *burn,
				      gdouble overall_progress,
				      gdouble progress,
				      glong time_remaining,
				      BraseroToolDialog *self)
{
	brasero_tool_dialog_set_progress (self,
					  -1.0,
					  progress,
					  -1,
					  -1,
					  -1);
}

static void
brasero_tool_dialog_action_changed (BraseroBurn *burn,
				    BraseroBurnAction action,
				    BraseroToolDialog *self)
{
	gchar *string;

	brasero_burn_get_action_string (burn, action, &string);
	brasero_tool_dialog_set_action (self,
					action,
					string);
	g_free (string);
}

BraseroBurn *
brasero_tool_dialog_get_burn (BraseroToolDialog *self)
{
	if (self->priv->burn) {
		brasero_burn_cancel (self->priv->burn, FALSE);
		g_object_unref (self->priv->burn);
	}

	self->priv->burn = brasero_burn_new ();
	g_signal_connect (self->priv->burn,
			  "progress_changed",
			  G_CALLBACK (brasero_tool_dialog_progress_changed),
			  self);
	g_signal_connect (self->priv->burn,
			  "action_changed",
			  G_CALLBACK (brasero_tool_dialog_action_changed),
			  self);

	return self->priv->burn;
}

static void
brasero_tool_dialog_run (BraseroToolDialog *self)
{
	BraseroToolDialogClass *klass;
	gboolean close = FALSE;
	BraseroMedium *medium;
	BraseroMedia media;
	GdkCursor *cursor;

	medium = brasero_medium_selection_get_active (BRASERO_MEDIUM_SELECTION (self->priv->selector));

	/* set up */
	gtk_widget_set_sensitive (self->priv->upper_box, FALSE);
	gtk_widget_set_sensitive (self->priv->lower_box, TRUE);
	gtk_widget_set_sensitive (GTK_WIDGET (self->priv->button), FALSE);

	cursor = gdk_cursor_new (GDK_WATCH);
	gdk_window_set_cursor (GTK_WIDGET (self)->window, cursor);
	gdk_cursor_unref (cursor);

	gtk_button_set_label (GTK_BUTTON (self->priv->cancel), GTK_STOCK_CANCEL);

	/* check the contents of the drive */
	media = brasero_medium_get_status (medium);
	if (media == BRASERO_MEDIUM_NONE) {
		brasero_tool_dialog_no_media (self);
		gtk_widget_set_sensitive (GTK_WIDGET (self->priv->button), TRUE);
		goto end;
	}
	else if (media == BRASERO_MEDIUM_UNSUPPORTED) {
		/* error out */
		gtk_widget_set_sensitive (GTK_WIDGET (self->priv->button), TRUE);
		brasero_tool_dialog_media_error (self);
		goto end;
	}
	else if (media == BRASERO_MEDIUM_BUSY) {
		gtk_widget_set_sensitive (GTK_WIDGET (self->priv->button), TRUE);
		brasero_tool_dialog_media_busy (self);
		goto end;
	}

	self->priv->running = TRUE;
	klass = BRASERO_TOOL_DIALOG_GET_CLASS (self);
	if (klass->activate)
		close = klass->activate (self, medium);
	self->priv->running = FALSE;

	if (close || self->priv->close) {
		gtk_widget_destroy (GTK_WIDGET (self));

		if (medium)
			g_object_unref (medium);
		return;
	}

end:

	gdk_window_set_cursor (GTK_WIDGET (self)->window, NULL);
	gtk_button_set_label (GTK_BUTTON (self->priv->cancel), GTK_STOCK_CLOSE);

	gtk_widget_set_sensitive (self->priv->upper_box, TRUE);
	gtk_widget_set_sensitive (self->priv->lower_box, FALSE);

	brasero_burn_progress_reset (BRASERO_BURN_PROGRESS (self->priv->progress));

	if (medium)
		g_object_unref (medium);

	g_signal_stop_emission_by_name (self, "response");
}

static void
brasero_tool_dialog_button_clicked (GtkButton *button,
				    BraseroToolDialog *self)
{
	brasero_tool_dialog_run (self);
}

void
brasero_tool_dialog_pack_options (BraseroToolDialog *self,
				  ...)
{
	gchar *title;
	va_list vlist;
	GtkWidget *child;
	GSList *list = NULL;

	va_start (vlist, self);
	while ((child = va_arg (vlist, GtkWidget *)))
		list = g_slist_prepend (list, child);
	va_end (vlist);

	title = g_strdup_printf ("<b>%s</b>", _("Options"));
	self->priv->options = brasero_utils_pack_properties_list (title, list);
	g_free (title);

	g_slist_free (list);

	gtk_widget_show_all (self->priv->options);
	gtk_box_pack_start (GTK_BOX (self->priv->upper_box),
			    self->priv->options,
			    FALSE,
			    FALSE,
			    0);
}

void
brasero_tool_dialog_set_button (BraseroToolDialog *self,
				const gchar *text,
				const gchar *image,
				const gchar *theme)
{
	GtkWidget *button;

	if (self->priv->button)
		g_object_unref (self->priv->button);

	button = brasero_utils_make_button (text,
					    image,
					    theme,
					    GTK_ICON_SIZE_BUTTON);
	gtk_widget_show_all (button);
	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (brasero_tool_dialog_button_clicked),
			  self);

	gtk_dialog_add_action_widget (GTK_DIALOG (self),
				      button,
				      GTK_RESPONSE_OK);

	self->priv->button = button;
}

void
brasero_tool_dialog_set_valid (BraseroToolDialog *self,
			       gboolean valid)
{
	gtk_widget_set_sensitive (self->priv->button, valid);
}

void
brasero_tool_dialog_set_medium_type_shown (BraseroToolDialog *self,
					   BraseroMediaType media_type)
{
	brasero_medium_selection_show_type (BRASERO_MEDIUM_SELECTION (self->priv->selector),
					    media_type);
}

BraseroMedium *
brasero_tool_dialog_get_medium (BraseroToolDialog *self)
{
	return brasero_medium_selection_get_active (BRASERO_MEDIUM_SELECTION (self->priv->selector));
}

void
brasero_tool_dialog_set_medium (BraseroToolDialog *self,
				BraseroMedium *medium)
{
	if (!medium)
		return;

	brasero_medium_selection_set_active (BRASERO_MEDIUM_SELECTION (self->priv->selector), medium);
}

static void
brasero_tool_dialog_drive_changed_cb (GtkComboBox *combo_box,
				      BraseroToolDialog *self)
{
	BraseroToolDialogClass *klass;
	BraseroMedium *medium;

	medium = brasero_medium_selection_get_active (BRASERO_MEDIUM_SELECTION (combo_box));

	klass = BRASERO_TOOL_DIALOG_GET_CLASS (self);
	if (klass->drive_changed)
		klass->drive_changed (self, medium);

	if (medium)
		g_object_unref (medium);
}

static gboolean
brasero_tool_dialog_cancel_dialog (GtkWidget *toplevel)
{
	gint result;
	GtkWidget *button;
	GtkWidget *message;

	message = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					  GTK_DIALOG_DESTROY_WITH_PARENT|
					  GTK_DIALOG_MODAL,
					  GTK_MESSAGE_WARNING,
					  GTK_BUTTONS_NONE,
					  _("Do you really want to quit?"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
						  _("Interrupting the process may make disc unusable."));
	gtk_dialog_add_buttons (GTK_DIALOG (message),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				NULL);

	button = brasero_utils_make_button (_("_Continue"),
					    GTK_STOCK_OK,
					    NULL,
					    GTK_ICON_SIZE_BUTTON);
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
brasero_tool_dialog_cancel (BraseroToolDialog *self)
{
	BraseroBurnResult result = BRASERO_BURN_OK;
	BraseroToolDialogClass *klass;

	klass = BRASERO_TOOL_DIALOG_GET_CLASS (self);
	if (klass->cancel)
		klass->cancel (self);

	if (self->priv->burn)
		result = brasero_burn_cancel (self->priv->burn, TRUE);

	if (result == BRASERO_BURN_DANGEROUS) {
		if (brasero_tool_dialog_cancel_dialog (GTK_WIDGET (self))) {
			if (self->priv->burn)
				brasero_burn_cancel (self->priv->burn, FALSE);
		}
		else
			return FALSE;
	}

	self->priv->close = TRUE;

	if (!self->priv->running)
		gtk_widget_destroy (GTK_WIDGET (self));

	return TRUE;
}

static void
brasero_tool_dialog_cancel_clicked_cb (GtkWidget *button,
				       BraseroToolDialog *dialog)
{
	brasero_tool_dialog_cancel (dialog);
}

static gboolean
brasero_tool_dialog_delete (GtkWidget *widget, GdkEventAny *event)
{
	BraseroToolDialog *self;

	self = BRASERO_TOOL_DIALOG (widget);

	brasero_tool_dialog_cancel (self);
	return FALSE;
}

static void
brasero_tool_dialog_finalize (GObject *object)
{
	BraseroToolDialog *cobj;

	cobj = BRASERO_TOOL_DIALOG (object);

	g_free (cobj->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_tool_dialog_class_init (BraseroToolDialogClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_tool_dialog_finalize;

	widget_class->delete_event = brasero_tool_dialog_delete;
}

static void
brasero_tool_dialog_init (BraseroToolDialog *obj)
{
	GtkWidget *title;
	gchar *title_str;

	obj->priv = g_new0 (BraseroToolDialogPrivate, 1);
	gtk_dialog_set_has_separator (GTK_DIALOG (obj), FALSE);

	/* upper part */
	obj->priv->upper_box = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (GTK_WIDGET (obj->priv->upper_box));

	obj->priv->selector = brasero_medium_selection_new ();
	gtk_widget_show (GTK_WIDGET (obj->priv->selector));

	title_str = g_strdup_printf ("<b>%s</b>", _("Select a disc"));
	gtk_box_pack_start (GTK_BOX (obj->priv->upper_box),
			    brasero_utils_pack_properties (title_str,
							   obj->priv->selector,
							   NULL),
			    FALSE, FALSE, 0);
	g_free (title_str);

	brasero_medium_selection_show_type (BRASERO_MEDIUM_SELECTION (obj->priv->selector),
					    BRASERO_MEDIA_TYPE_REWRITABLE|
					    BRASERO_MEDIA_TYPE_WRITABLE|
					    BRASERO_MEDIA_TYPE_READABLE);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (obj)->vbox),
			    obj->priv->upper_box,
			    FALSE,
			    FALSE,
			    0);

	/* lower part */
	obj->priv->lower_box = gtk_vbox_new (FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (obj->priv->lower_box), 12);
	gtk_widget_set_sensitive (obj->priv->lower_box, FALSE);
	gtk_widget_show (obj->priv->lower_box);

	title_str = g_strdup_printf ("<b>%s</b>", _("Progress"));
	title = gtk_label_new (title_str);
	g_free (title_str);

	gtk_label_set_use_markup (GTK_LABEL (title), TRUE);
	gtk_misc_set_alignment (GTK_MISC (title), 0.0, 0.5);
	gtk_misc_set_padding(GTK_MISC (title), 0, 6);
	gtk_widget_show (title);
	gtk_box_pack_start (GTK_BOX (obj->priv->lower_box),
			    title,
			    FALSE,
			    FALSE,
			    0);

	obj->priv->progress = brasero_burn_progress_new ();
	gtk_widget_show (obj->priv->progress);
	g_object_set (G_OBJECT (obj->priv->progress),
		      "show-info", FALSE,
		      NULL);

	gtk_box_pack_start (GTK_BOX (obj->priv->lower_box),
			    obj->priv->progress,
			    FALSE,
			    FALSE,
			    0);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (obj)->vbox),
			    obj->priv->lower_box,
			    FALSE,
			    FALSE,
			    0);

	/* buttons */
	obj->priv->cancel = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
	gtk_widget_show (obj->priv->cancel);
	g_signal_connect (G_OBJECT (obj->priv->cancel), "clicked",
			  G_CALLBACK (brasero_tool_dialog_cancel_clicked_cb),
			  obj);
	gtk_dialog_add_action_widget (GTK_DIALOG (obj),
				      obj->priv->cancel,
				      GTK_RESPONSE_CANCEL);

	g_signal_connect (G_OBJECT (obj->priv->selector),
			  "changed",
			  G_CALLBACK (brasero_tool_dialog_drive_changed_cb),
			  obj);

	gtk_window_resize (GTK_WINDOW (obj), 10, 10);
}
