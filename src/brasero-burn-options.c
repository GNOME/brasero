/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2008 <bonfire-app@wanadoo.fr>
 * 
 * brasero is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * brasero is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtk.h>

#include "burn-basics.h"
#include "burn-session.h"
#include "burn-caps.h"
#include "burn-medium.h"

#include "brasero-burn-options.h"
#include "brasero-session-cfg.h"
#include "brasero-notify.h"
#include "brasero-dest-selection.h"
#include "brasero-utils.h"
#include "brasero-medium-properties.h"

typedef struct _BraseroBurnOptionsPrivate BraseroBurnOptionsPrivate;
struct _BraseroBurnOptionsPrivate
{
	BraseroSessionCfg *session;

	gulong valid_sig;

	GtkWidget *source;
	GtkWidget *message_input;
	GtkWidget *selection;
	GtkWidget *properties;
	GtkWidget *warning;
	GtkWidget *copies_box;
	GtkWidget *copies_spin;
	GtkWidget *message_output;
	GtkWidget *options;
	GtkWidget *button;

	guint is_valid:1;
};

#define BRASERO_BURN_OPTIONS_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_BURN_OPTIONS, BraseroBurnOptionsPrivate))



G_DEFINE_TYPE (BraseroBurnOptions, brasero_burn_options, GTK_TYPE_DIALOG);

void
brasero_burn_options_add_source (BraseroBurnOptions *self,
				 const gchar *title,
				 ...)
{
	va_list vlist;
	GtkWidget *child;
	GtkWidget *source;
	GSList *list = NULL;
	BraseroBurnOptionsPrivate *priv;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (self);

	list = g_slist_prepend (list, priv->message_input);

	va_start (vlist, title);
	while ((child = va_arg (vlist, GtkWidget *)))
		list = g_slist_prepend (list, child);
	va_end (vlist);

	source = brasero_utils_pack_properties_list (title, list);
	g_slist_free (list);

	gtk_container_add (GTK_CONTAINER (priv->source), source);
	gtk_widget_show (priv->source);

	brasero_dest_selection_choose_best (BRASERO_DEST_SELECTION (priv->selection));
}

void
brasero_burn_options_add_options (BraseroBurnOptions *self,
				  GtkWidget *options)
{
	BraseroBurnOptionsPrivate *priv;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (self);

	gtk_container_add (GTK_CONTAINER (priv->options), options);
	gtk_widget_show (priv->options);
}

GtkWidget *
brasero_burn_options_add_burn_button (BraseroBurnOptions *self,
				      const gchar *text,
				      const gchar *icon)
{
	BraseroBurnOptionsPrivate *priv;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (self);

	if (priv->button) {
		gtk_widget_destroy (priv->button);
		priv->button = NULL;
	}

	priv->button = brasero_utils_make_button (text,
						  NULL,
						  icon,
						  GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (priv->button);
	gtk_dialog_add_action_widget (GTK_DIALOG (self),
				      priv->button,
				      GTK_RESPONSE_OK);
	return priv->button;
}

void
brasero_burn_options_lock_selection (BraseroBurnOptions *self)
{
	BraseroBurnOptionsPrivate *priv;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (self);
	brasero_medium_selection_set_active (BRASERO_MEDIUM_SELECTION (priv->selection),
					     brasero_drive_get_medium (brasero_burn_session_get_burner (BRASERO_BURN_SESSION (priv->session))));
	brasero_dest_selection_lock (BRASERO_DEST_SELECTION (priv->selection), TRUE);
}

void
brasero_burn_options_set_type_shown (BraseroBurnOptions *self,
				     BraseroMediaType type)
{
	BraseroBurnOptionsPrivate *priv;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (self);
	brasero_medium_selection_show_type (BRASERO_MEDIUM_SELECTION (priv->selection), type);
}

BraseroBurnSession *
brasero_burn_options_get_session (BraseroBurnOptions *self)
{
	BraseroBurnOptionsPrivate *priv;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (self);
	g_object_ref (priv->session);

	return BRASERO_BURN_SESSION (priv->session);
}

static void
brasero_burn_options_copies_num_changed_cb (GtkSpinButton *button,
					    BraseroBurnOptions *self)
{
	gint numcopies;
	BraseroBurnOptionsPrivate *priv;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (self);
	numcopies = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->copies_spin));
	brasero_burn_session_set_num_copies (BRASERO_BURN_SESSION (priv->session), numcopies);
}

static void
brasero_burn_options_message_response_cb (BraseroDiscMessage *message,
					  GtkResponseType response,
					  BraseroBurnOptions *self)
{
	if (response == GTK_RESPONSE_OK) {
		BraseroBurnOptionsPrivate *priv;

		priv = BRASERO_BURN_OPTIONS_PRIVATE (self);
		brasero_session_cfg_add_flags (priv->session, BRASERO_BURN_FLAG_OVERBURN);
	}
}

#define BRASERO_BURN_OPTIONS_NO_MEDIUM_WARNING	1000

static void
brasero_burn_options_update_no_medium_warning (BraseroBurnOptions *self)
{
	BraseroBurnOptionsPrivate *priv;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (self);

	if (!priv->is_valid) {
		brasero_notify_message_remove (BRASERO_NOTIFY (priv->message_output),
					       BRASERO_BURN_OPTIONS_NO_MEDIUM_WARNING);
		return;
	}

	if (!brasero_burn_session_is_dest_file (BRASERO_BURN_SESSION (priv->session))) {
		brasero_notify_message_remove (BRASERO_NOTIFY (priv->message_output),
					       BRASERO_BURN_OPTIONS_NO_MEDIUM_WARNING);
		return;
	}

	if (brasero_medium_selection_get_drive_num (BRASERO_MEDIUM_SELECTION (priv->selection)) != 1) {
		brasero_notify_message_remove (BRASERO_NOTIFY (priv->message_output),
					       BRASERO_BURN_OPTIONS_NO_MEDIUM_WARNING);
		return;
	}

	/* The user may have forgotten to insert a disc so remind him of that if
	 * there aren't any other possibility in the selection */
	brasero_notify_message_add (BRASERO_NOTIFY (priv->message_output),
				    _("Please, insert a recordable CD or DVD if you don't want to write to an image file."),
				    NULL,
				    -1,
				    BRASERO_BURN_OPTIONS_NO_MEDIUM_WARNING);
}

static void
brasero_burn_options_valid_media_cb (BraseroSessionCfg *session,
				     BraseroBurnOptions *self)
{
	BraseroBurnOptionsPrivate *priv;
	BraseroSessionError valid;
	gint numcopies;

	valid = brasero_session_cfg_get_error (session);

	priv = BRASERO_BURN_OPTIONS_PRIVATE (self);

	gtk_widget_set_sensitive (priv->button, valid == BRASERO_SESSION_VALID);
	gtk_widget_set_sensitive (priv->options, valid == BRASERO_SESSION_VALID);
	gtk_widget_set_sensitive (priv->properties, valid == BRASERO_SESSION_VALID);

	if (valid != BRASERO_SESSION_VALID) {
		gtk_widget_hide (priv->warning);
		gtk_widget_hide (priv->copies_box);
	}
	else if (brasero_burn_session_is_dest_file (BRASERO_BURN_SESSION (priv->session))) {
		gtk_widget_hide (priv->warning);
		gtk_widget_hide (priv->copies_box);
	}
	else {
		numcopies = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->copies_spin));
		brasero_burn_session_set_num_copies (BRASERO_BURN_SESSION (priv->session), numcopies);
		gtk_widget_set_sensitive (priv->copies_box, TRUE);

		if (brasero_burn_session_same_src_dest_drive (BRASERO_BURN_SESSION (priv->session)))
			gtk_widget_show (priv->warning);
		else
			gtk_widget_hide (priv->warning);

		gtk_widget_show (priv->copies_box);
	}

	if (priv->message_input) {
		gtk_widget_hide (priv->message_input);
		brasero_notify_message_remove (BRASERO_NOTIFY (priv->message_input),
					       BRASERO_NOTIFY_CONTEXT_SIZE);
	}

	brasero_notify_message_remove (BRASERO_NOTIFY (priv->message_output),
				       BRASERO_NOTIFY_CONTEXT_SIZE);

	priv->is_valid = FALSE;
	if (valid == BRASERO_SESSION_INSUFFICIENT_SPACE) {
		brasero_notify_message_add (BRASERO_NOTIFY (priv->message_output),
					    _("Please, choose another CD or DVD or insert a new one."),
					    _("The size of the project is too large for the disc even with the overburn option."),
					    -1,
					    BRASERO_NOTIFY_CONTEXT_SIZE);
	}
	else if (valid == BRASERO_SESSION_NO_OUTPUT) {
		brasero_notify_message_add (BRASERO_NOTIFY (priv->message_output),
					    _("Please, insert a recordable CD or DVD."),
					    _("There is no recordable medium inserted."),
					    -1,
					    BRASERO_NOTIFY_CONTEXT_SIZE);
	}
	else if (valid == BRASERO_SESSION_NO_INPUT_MEDIUM) {
		GtkWidget *message;

		if (priv->message_input) {
			gtk_widget_show (priv->message_input);
			message = brasero_notify_message_add (BRASERO_NOTIFY (priv->message_input),
							      _("Please, insert a disc holding data."),
							      _("There is no inserted medium to copy."),
							      -1,
							      BRASERO_NOTIFY_CONTEXT_SIZE);
		}
	}
	else if (valid == BRASERO_SESSION_NO_INPUT_IMAGE) {
		GtkWidget *message;

		if (priv->message_input) {
			gtk_widget_show (priv->message_input);
			message = brasero_notify_message_add (BRASERO_NOTIFY (priv->message_input),
							      _("Please, select an image."),
							      _("There is no selected image."),
							      -1,
							      BRASERO_NOTIFY_CONTEXT_SIZE);
		}
	}
	else if (valid == BRASERO_SESSION_UNKNOWN_IMAGE) {
		GtkWidget *message;

		if (priv->message_input) {
			gtk_widget_show (priv->message_input);
			message = brasero_notify_message_add (BRASERO_NOTIFY (priv->message_input),
							      _("Please, select another image."),
							      _("It doesn't appear to be a valid image or a valid cue file."),
							      -1,
							      BRASERO_NOTIFY_CONTEXT_SIZE);
		}
	}
	else if (valid == BRASERO_SESSION_DISC_PROTECTED) {
		GtkWidget *message;

		if (priv->message_input) {
			gtk_widget_show (priv->message_input);
			message = brasero_notify_message_add (BRASERO_NOTIFY (priv->message_input),
							      _("Please, insert a disc that is not copy protected."),
							      _("Such a medium can't be copied without the proper plugins."),
							      -1,
							      BRASERO_NOTIFY_CONTEXT_SIZE);
		}
	}
	else if (valid == BRASERO_SESSION_NOT_SUPPORTED) {
		brasero_notify_message_add (BRASERO_NOTIFY (priv->message_output),
					    _("Please, replace the disc with a supported CD or DVD."),
					    _("It is not possible to write with the current set of plugins."),
					    -1,
					    BRASERO_NOTIFY_CONTEXT_SIZE);
	}
	else if (valid == BRASERO_SESSION_OVERBURN_NECESSARY) {
		GtkWidget *message;

		message = brasero_notify_message_add (BRASERO_NOTIFY (priv->message_output),
						      _("Would you like to burn beyond the disc reported capacity?"),
						      _("The size of the project is too large for the disc."
							"\nYou may want to use this option if you're using 90 or 100 min CD-R(W) which can't be properly recognised and therefore need overburn option."
							"\nNOTE: This option might cause failure."),
						      -1,
						      BRASERO_NOTIFY_CONTEXT_SIZE);
		brasero_notify_button_add (BRASERO_NOTIFY (priv->message_output),
					   BRASERO_DISC_MESSAGE (message),
					   _("_Overburn"),
					   _("Burn beyond the disc reported capacity"),
					   GTK_RESPONSE_OK);

		g_signal_connect (message,
				  "response",
				  G_CALLBACK (brasero_burn_options_message_response_cb),
				  self);
	}
	else
		priv->is_valid = TRUE;

	brasero_burn_options_update_no_medium_warning (self);
	gtk_window_resize (GTK_WINDOW (self), 10, 10);
}

static void
brasero_burn_options_medium_num_changed (BraseroMediumSelection *selection,
					 BraseroBurnOptions *self)
{
	brasero_burn_options_update_no_medium_warning (self);
	gtk_window_resize (GTK_WINDOW (self), 10, 10);
}

static void
brasero_burn_options_init (BraseroBurnOptions *object)
{
	BraseroBurnOptionsPrivate *priv;
	GtkWidget *selection;
	GtkWidget *button;
	GtkWidget *label;
	gchar *string;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (object);

	gtk_dialog_set_has_separator (GTK_DIALOG (object), FALSE);

	/* Create the session */
	priv->session = brasero_session_cfg_new ();
	brasero_burn_session_add_flag (BRASERO_BURN_SESSION (priv->session),
				       BRASERO_BURN_FLAG_NOGRACE|
				       BRASERO_BURN_FLAG_CHECK_SIZE);

	/* Create a cancel button */
	button = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
	gtk_widget_show (button);
	gtk_dialog_add_action_widget (GTK_DIALOG (object),
				      button, 
				      GTK_RESPONSE_CANCEL);

	/* Create a default Burn button */
	priv->button = brasero_utils_make_button (_("_Burn"),
						  NULL,
						  "media-optical-burn",
						  GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (priv->button);
	gtk_dialog_add_action_widget (GTK_DIALOG (object),
				      priv->button,
				      GTK_RESPONSE_OK);

	/* Create an upper box for sources */
	priv->source = gtk_alignment_new (0.0, 0.5, 1.0, 1.0);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (object)->vbox),
			    priv->source,
			    FALSE,
			    TRUE,
			    0);

	/* create message queue for input */
	priv->message_input = brasero_notify_new ();

	/* Medium selection box */
	selection = gtk_hbox_new (FALSE, 12);
	gtk_widget_show (selection);

	priv->selection = brasero_dest_selection_new (BRASERO_BURN_SESSION (priv->session));
	gtk_widget_show (priv->selection);
	gtk_box_pack_start (GTK_BOX (selection),
			    priv->selection,
			    TRUE,
			    TRUE,
			    0);

	priv->properties = brasero_medium_properties_new (BRASERO_BURN_SESSION (priv->session));
	gtk_widget_show (priv->properties);
	gtk_box_pack_start (GTK_BOX (selection),
			    priv->properties,
			    FALSE,
			    FALSE,
			    0);

	/* Medium info */
	string = g_strdup_printf ("<b><i>%s</i></b>\n<i>%s</i>",
				  _("The drive that holds the source media will also be the one used to record."),
				  _("A new recordable media will be required once the one currently loaded has been copied."));
	priv->warning = gtk_label_new (string);
	g_free (string);

	gtk_misc_set_alignment (GTK_MISC (priv->warning), 0.0, 0.5);
	gtk_label_set_line_wrap_mode (GTK_LABEL (priv->warning), PANGO_WRAP_WORD);
	gtk_label_set_line_wrap (GTK_LABEL (priv->warning), TRUE);
	gtk_label_set_use_markup (GTK_LABEL (priv->warning), TRUE);

	gtk_widget_show (priv->warning);

	/* Number of copies */
	priv->copies_box = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (priv->copies_box);

	label = gtk_label_new (_("Number of copies"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (priv->copies_box), label, FALSE, FALSE, 0);

	priv->copies_spin = gtk_spin_button_new_with_range (1.0, 99.0, 1.0);
	gtk_widget_show (priv->copies_spin);
	gtk_box_pack_start (GTK_BOX (priv->copies_box), priv->copies_spin, FALSE, FALSE, 0);
	g_signal_connect (priv->copies_spin,
			  "value-changed",
			  G_CALLBACK (brasero_burn_options_copies_num_changed_cb),
			  object);

	/* Box to display warning messages */
	priv->message_output = brasero_notify_new ();
	gtk_widget_show (priv->message_output);

	string = g_strdup_printf ("<b>%s</b>", _("Select a disc to write to"));
	selection = brasero_utils_pack_properties (string,
						   priv->message_output,
						   priv->copies_box,
						   priv->warning,
						   selection,
						   NULL);
	g_free (string);
	gtk_widget_show (selection);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (object)->vbox),
			    selection,
			    FALSE,
			    TRUE,
			    0);

	/* Create a lower box for options */
	priv->options = gtk_alignment_new (0.0, 0.5, 1.0, 1.0);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (object)->vbox),
			    priv->options,
			    FALSE,
			    TRUE,
			    0);

	priv->valid_sig = g_signal_connect (priv->session,
					    "is-valid",
					    G_CALLBACK (brasero_burn_options_valid_media_cb),
					    object);

	g_signal_connect (priv->selection,
			  "medium-added",
			  G_CALLBACK (brasero_burn_options_medium_num_changed),
			  object);
	g_signal_connect (priv->selection,
			  "medium-removed",
			  G_CALLBACK (brasero_burn_options_medium_num_changed),
			  object);
}

static void
brasero_burn_options_finalize (GObject *object)
{
	BraseroBurnOptionsPrivate *priv;

	priv = BRASERO_BURN_OPTIONS_PRIVATE (object);

	if (priv->valid_sig) {
		g_signal_handler_disconnect (priv->session,
					     priv->valid_sig);
		priv->valid_sig = 0;
	}

	if (priv->session) {
		g_object_unref (priv->session);
		priv->session = NULL;
	}

	G_OBJECT_CLASS (brasero_burn_options_parent_class)->finalize (object);
}

static void
brasero_burn_options_class_init (BraseroBurnOptionsClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroBurnOptionsPrivate));

	object_class->finalize = brasero_burn_options_finalize;
}


