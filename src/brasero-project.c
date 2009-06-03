/***************************************************************************
 *            project.c
 *
 *  mar nov 29 09:32:17 2005
 *  Copyright  2005  Rouquier Philippe
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include <gio/gio.h>

#include <gtk/gtk.h>

#include <libxml/xmlerror.h>
#include <libxml/xmlwriter.h>
#include <libxml/parser.h>
#include <libxml/xmlstring.h>
#include <libxml/uri.h>

#include <gconf/gconf-client.h>

#include <gst/gst.h>

#ifdef BUILD_PLAYLIST
#include <totem-pl-parser.h>
#endif

#include "brasero-misc.h"
#include "brasero-jacket-edit.h"

#include "brasero-tags.h"
#include "brasero-session.h"

#ifdef BUILD_PREVIEW
#include "brasero-player.h"
#endif

#include "brasero-track-data.h"
#include "brasero-session-cfg.h"
#include "brasero-burn-options.h"
#include "brasero-cover.h"

#include "brasero-medium-selection-priv.h"
#include "brasero-session-helper.h"
#include "brasero-session-cfg.h"
#include "brasero-dest-selection.h"

#include "brasero-project-type-chooser.h"
#include "brasero-app.h"
#include "brasero-project.h"
#include "brasero-disc.h"
#include "brasero-data-disc.h"
#include "brasero-audio-disc.h"
#include "brasero-video-disc.h"
#include "brasero-uri-container.h"
#include "brasero-layout-object.h"
#include "brasero-disc-message.h"
#include "brasero-file-chooser.h"
#include "brasero-notify.h"
#include "brasero-project-parse.h"
#include "brasero-burn-options.h"
#include "brasero-project-name.h"

static void brasero_project_class_init (BraseroProjectClass *klass);
static void brasero_project_init (BraseroProject *sp);
static void brasero_project_iface_uri_container_init (BraseroURIContainerIFace *iface);
static void brasero_project_iface_layout_object_init (BraseroLayoutObjectIFace *iface);
static void brasero_project_finalize (GObject *object);

static void
brasero_project_save_cb (GtkAction *action, BraseroProject *project);
static void
brasero_project_save_as_cb (GtkAction *action, BraseroProject *project);

static void
brasero_project_add_uris_cb (GtkAction *action, BraseroProject *project);
static void
brasero_project_remove_selected_uris_cb (GtkAction *action, BraseroProject *project);
static void
brasero_project_empty_cb (GtkAction *action, BraseroProject *project);

static void
brasero_project_burn_cb (GtkAction *action, BraseroProject *project);

static void
brasero_project_size_changed_cb (BraseroDisc *disc,
			         gint64 size,
			         BraseroProject *project);
static void
brasero_project_flags_changed_cb (BraseroDisc *disc,
				  BraseroBurnFlag flags,
				  BraseroProject *project);

static void
brasero_project_burn_clicked_cb (GtkButton *button, BraseroProject *project);

static void
brasero_project_contents_changed_cb (BraseroDisc *disc,
				     gint nb_files,
				     BraseroProject *project);
static void
brasero_project_selection_changed_cb (BraseroDisc *disc,
				      BraseroProject *project);

static gchar *
brasero_project_get_selected_uri (BraseroURIContainer *container);
static gboolean
brasero_project_get_boundaries (BraseroURIContainer *container,
				gint64 *start,
				gint64 *end);

static void
brasero_project_get_proportion (BraseroLayoutObject *object,
				gint *header,
				gint *center,
				gint *footer);

static void
brasero_project_get_proportion (BraseroLayoutObject *object,
				gint *header,
				gint *center,
				gint *footer);

typedef enum {
	BRASERO_PROJECT_SAVE_XML			= 0,
	BRASERO_PROJECT_SAVE_PLAIN			= 1,
	BRASERO_PROJECT_SAVE_PLAYLIST_PLS		= 2,
	BRASERO_PROJECT_SAVE_PLAYLIST_M3U		= 3,
	BRASERO_PROJECT_SAVE_PLAYLIST_XSPF		= 4,
	BRASERO_PROJECT_SAVE_PLAYLIST_IRIVER_PLA	= 5
} BraseroProjectSave;

struct BraseroProjectPrivate {
	BraseroSessionCfg *session;

	GtkWidget *selection;
	GtkWidget *name_display;
	GtkWidget *button_img;
	GtkWidget *icon_img;
	GtkWidget *discs;
	GtkWidget *audio;
	GtkWidget *data;
	GtkWidget *video;

	GtkWidget *message;

	GtkUIManager *manager;

	guint status_ctx;

	GtkWidget *project_status;

	/* header */
	GtkWidget *burn;

	GtkActionGroup *project_group;
	guint merge_id;

	gchar *project;

	gchar *cover;

	gint64 sectors;
	BraseroDisc *current;

	BraseroURIContainer *current_source;

	GtkWidget *chooser;
	gulong selected_id;
	gulong activated_id;

	guint is_burning:1;

    	guint burnt:1;

	guint empty:1;
	guint modified:1;
	guint has_focus:1;
	guint oversized:1;
	guint selected_uris:1;

	guint merging:1;
};

static GtkActionEntry entries [] = {
	{"Save", GTK_STOCK_SAVE, NULL, NULL,
	 N_("Save current project"), G_CALLBACK (brasero_project_save_cb)},
	{"SaveAs", GTK_STOCK_SAVE_AS, N_("Save _As..."), NULL,
	 N_("Save current project to a different location"), G_CALLBACK (brasero_project_save_as_cb)},
	{"Add", GTK_STOCK_ADD, N_("_Add Files"), NULL,
	 N_("Add files to the project"), G_CALLBACK (brasero_project_add_uris_cb)},
	{"DeleteProject", GTK_STOCK_REMOVE, N_("_Remove Files"), NULL,
	 N_("Remove the selected files from the project"), G_CALLBACK (brasero_project_remove_selected_uris_cb)},
	/* Translators: "empty" is a verb here */
	{"DeleteAll", GTK_STOCK_CLEAR, N_("E_mpty Project"), NULL,
	 N_("Remove all files from the project"), G_CALLBACK (brasero_project_empty_cb)},
	{"Burn", "media-optical-burn", N_("_Burn..."), NULL,
	 N_("Burn the disc"), G_CALLBACK (brasero_project_burn_cb)},
};

static const gchar *description = {
	"<ui>"
	    "<menubar name='menubar' >"
		"<menu action='ProjectMenu'>"
			"<placeholder name='ProjectPlaceholder'>"
			    "<menuitem action='Save'/>"
			    "<menuitem action='SaveAs'/>"
			    "<separator/>"
			"<menuitem action='Burn'/>"
			"</placeholder>"
		"</menu>"
		
		"<menu action='EditMenu'>"
			"<placeholder name='EditPlaceholder'>"
			    "<menuitem action='Add'/>"
			    "<menuitem action='DeleteProject'/>"
			    "<menuitem action='DeleteAll'/>"
			    "<separator/>"
			"</placeholder>"
		"</menu>"

		"<menu action='ViewMenu'>"
		"</menu>"

		"<menu action='ToolMenu'>"
			"<placeholder name='DiscPlaceholder'/>"
		"</menu>"
	    "</menubar>"
	    "<toolbar name='Toolbar'>"
		"<separator/>"
		"<toolitem action='Add'/>"
		"<toolitem action='DeleteProject'/>"
		"<toolitem action='DeleteAll'/>"
		"<placeholder name='DiscButtonPlaceholder'/>"
	     "</toolbar>"
	"</ui>"
};

static GObjectClass *parent_class = NULL;

#define BRASERO_PROJECT_SIZE_WIDGET_BORDER	1

#define BRASERO_KEY_SHOW_PREVIEW		"/apps/brasero/display/viewer"

#define BRASERO_PROJECT_VERSION "0.2"

#define BRASERO_RESPONSE_ADD			1976

GType
brasero_project_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroProjectClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_project_class_init,
			NULL,
			NULL,
			sizeof (BraseroProject),
			0,
			(GInstanceInitFunc) brasero_project_init,
		};

		static const GInterfaceInfo uri_container_info =
		{
			(GInterfaceInitFunc) brasero_project_iface_uri_container_init,
			NULL,
			NULL
		};
		static const GInterfaceInfo layout_object =
		{
			(GInterfaceInitFunc) brasero_project_iface_layout_object_init,
			NULL,
			NULL
		};

		type = g_type_register_static (GTK_TYPE_VBOX, 
					       "BraseroProject",
					       &our_info, 0);

		g_type_add_interface_static (type,
					     BRASERO_TYPE_URI_CONTAINER,
					     &uri_container_info);
		g_type_add_interface_static (type,
					     BRASERO_TYPE_LAYOUT_OBJECT,
					     &layout_object);
	}

	return type;
}

static void
brasero_project_class_init (BraseroProjectClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_project_finalize;
}

static void
brasero_project_iface_uri_container_init (BraseroURIContainerIFace *iface)
{
	iface->get_selected_uri = brasero_project_get_selected_uri;
	iface->get_boundaries = brasero_project_get_boundaries;
}

static void
brasero_project_iface_layout_object_init (BraseroLayoutObjectIFace *iface)
{
	iface->get_proportion = brasero_project_get_proportion;
}

static void
brasero_project_get_proportion (BraseroLayoutObject *object,
				gint *header,
				gint *center,
				gint *footer)
{
	if (!BRASERO_PROJECT (object)->priv->name_display)
		return;

	*footer = BRASERO_PROJECT (object)->priv->name_display->parent->allocation.height;
}

static void
brasero_project_set_remove_button_state (BraseroProject *project)
{
	GtkAction *action;
	gboolean sensitive;

	sensitive = (project->priv->has_focus &&
		    !project->priv->empty &&
		     project->priv->selected_uris);

	action = gtk_action_group_get_action (project->priv->project_group, "DeleteProject");
	gtk_action_set_sensitive (action, sensitive);
}

static void
brasero_project_set_add_button_state (BraseroProject *project)
{
	GtkAction *action;
	GtkWidget *widget;
	gboolean sensitive;
	GtkWidget *toplevel;

	sensitive = ((!project->priv->current_source || !project->priv->has_focus) &&
		      !project->priv->oversized);

	action = gtk_action_group_get_action (project->priv->project_group, "Add");
	gtk_action_set_sensitive (action, sensitive);

	/* set the Add button to be the default for the whole window. That fixes 
	 * #465175 – Location field not working. GtkFileChooser needs a default
	 * widget to be activated. */
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (project));
	if (!sensitive) {
		gtk_window_set_default (GTK_WINDOW (toplevel), NULL);
		return;
	}

	widget = gtk_ui_manager_get_widget (project->priv->manager, "/Toolbar/Add");
	widget = gtk_bin_get_child (GTK_BIN (widget));
	GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_DEFAULT);
	gtk_window_set_default (GTK_WINDOW (toplevel), widget);
}

static void
brasero_project_focus_changed_cb (GtkContainer *container,
				  GtkWidget *widget,
				  gpointer NULL_data)
{
	BraseroProject *project;

	project = BRASERO_PROJECT (container);
	project->priv->has_focus = (widget != NULL);

	brasero_project_set_remove_button_state (project);
	brasero_project_set_add_button_state (project);
}

static void
brasero_project_name_changed_cb (BraseroProjectName *name,
				 BraseroProject *project)
{
	GtkAction *action;

	project->priv->modified = TRUE;

	/* the state of the following depends on the existence of an opened project */
	action = gtk_action_group_get_action (project->priv->project_group, "Save");
	if (project->priv->modified)
		gtk_action_set_sensitive (action, TRUE);
	else
		gtk_action_set_sensitive (action, FALSE);
}

static void
brasero_project_data_icon_error (BraseroProject *project,
				 GError *error)
{
	if (error) {
		brasero_app_alert (brasero_app_get_default (),
				   /* Translators: this is a picture not
				    * a disc image */
				   C_("picture", "Please select another image."),
				   error->message,
				   GTK_MESSAGE_ERROR);
	}
	else {
		brasero_app_alert (brasero_app_get_default (),
				   /* Translators: this is a picture not
				    * a disc image */
				   C_("picture", "Please select another image."),
				   _("Unknown error"),
				   GTK_MESSAGE_ERROR);
	}
}

static void
brasero_project_icon_changed_cb (BraseroDisc *disc,
				 BraseroProject *project)
{
	GError *error = NULL;
	GdkPixbuf *pixbuf;
	gchar *icon; 

	icon = brasero_data_disc_get_scaled_icon_path (BRASERO_DATA_DISC (project->priv->current));
	if (!icon) {
		gtk_image_set_from_icon_name (GTK_IMAGE (project->priv->icon_img),
					      "media-optical",
					      GTK_ICON_SIZE_LARGE_TOOLBAR);
		return;
	}

	/* Load and convert (48x48) the image into a pixbuf */
	pixbuf = gdk_pixbuf_new_from_file_at_scale (icon,
						    48,
						    48,
						    FALSE,
						    &error);
	g_free (icon);

	if (!pixbuf) {
		gtk_image_set_from_icon_name (GTK_IMAGE (project->priv->icon_img),
					      "media-optical",
					      GTK_ICON_SIZE_LARGE_TOOLBAR);
		brasero_project_data_icon_error (project, error);
		g_error_free (error);
		return;
	}

	gtk_image_set_from_pixbuf (GTK_IMAGE (project->priv->icon_img), pixbuf);
	g_object_unref (pixbuf);
}

static void
brasero_project_icon_button_clicked (GtkWidget *button,
				     BraseroProject *project)
{
	GtkFileFilter *filter;
	GError *error = NULL;
	GtkWidget *chooser;
	gchar *path;
	gint res;

	if (!BRASERO_IS_DATA_DISC (project->priv->current))
		return;

	chooser = gtk_file_chooser_dialog_new (_("Medium Icon"),
					       GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (project))),
					       GTK_FILE_CHOOSER_ACTION_OPEN,
					       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					       GTK_STOCK_OK, GTK_RESPONSE_OK,
					       NULL);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("All files"));
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

	filter = gtk_file_filter_new ();
	/* Translators: this is an image, a picture, not a "Disc Image" */
	gtk_file_filter_set_name (filter, C_("picture", "Image files"));
	gtk_file_filter_add_mime_type (filter, "image/*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

	gtk_widget_show (chooser);
	res = gtk_dialog_run (GTK_DIALOG (chooser));
	if (res != GTK_RESPONSE_OK) {
		gtk_widget_destroy (chooser);
		return;
	}

	path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
	gtk_widget_destroy (chooser);

	if (!brasero_data_disc_set_icon_path (BRASERO_DATA_DISC (project->priv->current), path, &error)) {
		if (error) {
			brasero_project_data_icon_error (project, error);
			g_error_free (error);
		}
	}
	g_free (path);
}

static void
brasero_project_icon_button_size_allocate (GtkWidget *widget,
					   GtkAllocation *allocation,
					   gpointer NULL_data)
{
	allocation->width = MAX (allocation->width, allocation->height);
	allocation->height = MAX (allocation->width, allocation->height);
}

static void
brasero_project_icon_button_size_request (GtkWidget *widget,
					  GtkRequisition *requisition,
					  gpointer NULL_data)
{
	requisition->width = MAX (requisition->width, requisition->height);
	requisition->height = MAX (requisition->width, requisition->height);
}

static void
brasero_project_message_response_span_cb (BraseroDiscMessage *message,
					  GtkResponseType response,
					  BraseroProject *project)
{
	if (response == GTK_RESPONSE_OK)
		brasero_session_span_start (BRASERO_SESSION_SPAN (project->priv->session));
}

static void
brasero_project_message_response_overburn_cb (BraseroDiscMessage *message,
					      GtkResponseType response,
					      BraseroProject *project)
{
	if (response == GTK_RESPONSE_OK)
		brasero_session_cfg_add_flags (project->priv->session, BRASERO_BURN_FLAG_OVERBURN);
}

static void
brasero_project_is_valid (BraseroSessionCfg *session,
			  BraseroProject *project)
{
	BraseroSessionError valid;

	valid = brasero_session_cfg_get_error (project->priv->session);

	/* Update burn button state */
	gtk_widget_set_sensitive (project->priv->burn, BRASERO_SESSION_IS_VALID (valid));

	/* FIXME: update option button state as well */

	/* Clean any message */
	brasero_notify_message_remove (BRASERO_NOTIFY (project->priv->message),
				       BRASERO_NOTIFY_CONTEXT_SIZE);

	if (valid == BRASERO_SESSION_INSUFFICIENT_SPACE) {
		/* Here there is an alternative: we may be able to span the data
		 * across multiple media. So try that. */
		if (brasero_session_span_possible (BRASERO_SESSION_SPAN (project->priv->session)) == BRASERO_BURN_RETRY) {
			GtkWidget *message;

			message = brasero_notify_message_add (BRASERO_NOTIFY (project->priv->message),
							      _("Would you like to burn the selection of files across several media?"),
							      _("The size of the project is too large for the disc even with the overburn option."),
							      -1,
							      BRASERO_NOTIFY_CONTEXT_SIZE);
			brasero_notify_button_add (BRASERO_NOTIFY (project->priv->message),
						   BRASERO_DISC_MESSAGE (message),
						   _("_Burn Several Discs"),
						   _("Burn the selection of files across several media"),
						   GTK_RESPONSE_OK);

			g_signal_connect (message,
					  "response",
					  G_CALLBACK (brasero_project_message_response_span_cb),
					  project);
		}
		else
			brasero_notify_message_add (BRASERO_NOTIFY (project->priv->message),
						    _("Please choose another CD or DVD or insert a new one."),
						    _("The size of the project is too large for the disc even with the overburn option."),
						    -1,
						    BRASERO_NOTIFY_CONTEXT_SIZE);
	}
	else if (valid == BRASERO_SESSION_OVERBURN_NECESSARY) {
		GtkWidget *message;

		message = brasero_notify_message_add (BRASERO_NOTIFY (project->priv->message),
						      _("Would you like to burn beyond the disc reported capacity?"),
						      _("The size of the project is too large for the disc and you must remove files from the project otherwise."
							"\nYou may want to use this option if you're using 90 or 100 min CD-R(W) which cannot be properly recognised and therefore need overburn option."
							"\nNOTE: This option might cause failure."),
						      -1,
						      BRASERO_NOTIFY_CONTEXT_SIZE);
		brasero_notify_button_add (BRASERO_NOTIFY (project->priv->message),
					   BRASERO_DISC_MESSAGE (message),
					   _("_Overburn"),
					   _("Burn beyond the disc reported capacity"),
					   GTK_RESPONSE_OK);

		g_signal_connect (message,
				  "response",
				  G_CALLBACK (brasero_project_message_response_overburn_cb),
				  project);
	}
	else if (valid == BRASERO_SESSION_NO_OUTPUT) {
		brasero_notify_message_add (BRASERO_NOTIFY (project->priv->message),
					    _("Please insert a recordable CD or DVD."),
					    _("There is no recordable disc inserted."),
					    -1,
					    BRASERO_NOTIFY_CONTEXT_SIZE);
	}
	else if (valid == BRASERO_SESSION_NO_CD_TEXT) {
		brasero_notify_message_add (BRASERO_NOTIFY (project->priv->message),
					    _("No track information (artist, title, ...) will be written to the disc."),
					    _("This is not supported by the current active burning backend."),
					    -1,
					    BRASERO_NOTIFY_CONTEXT_SIZE);
	}
	else if (valid == BRASERO_SESSION_NOT_SUPPORTED) {
		brasero_notify_message_add (BRASERO_NOTIFY (project->priv->message),
					    _("Please replace the disc with a supported CD or DVD."),
					    _("It is not possible to write with the current set of plugins."),
					    -1,
					    BRASERO_NOTIFY_CONTEXT_SIZE);
	}
	else if (brasero_burn_session_is_dest_file (BRASERO_BURN_SESSION (project->priv->session))
	     &&  brasero_medium_selection_get_media_num (BRASERO_MEDIUM_SELECTION (project->priv->selection)) == 1) {
		/* The user may have forgotten to insert a disc so remind him of that if
		 * there aren't any other possibility in the selection */
		brasero_notify_message_add (BRASERO_NOTIFY (project->priv->message),
					    _("Please insert a recordable CD or DVD if you don't want to write to an image file."),
					    NULL,
					    -1,
					    BRASERO_NOTIFY_CONTEXT_SIZE);
	}


}

static void
brasero_project_init (BraseroProject *obj)
{
	GtkSizeGroup *size_group;
	GtkWidget *alignment;
	GtkWidget *selector;
	GtkWidget *button;
	GtkWidget *label;
	GtkWidget *table;
	GtkWidget *image;
	GtkWidget *box;

	obj->priv = g_new0 (BraseroProjectPrivate, 1);

	g_signal_connect (G_OBJECT (obj),
			  "set-focus-child",
			  G_CALLBACK (brasero_project_focus_changed_cb),
			  NULL);

	obj->priv->message = brasero_notify_new ();
	gtk_box_pack_start (GTK_BOX (obj), obj->priv->message, FALSE, TRUE, 0);
	gtk_widget_show (obj->priv->message);

	obj->priv->session = brasero_session_cfg_new ();
	g_signal_connect (obj->priv->session,
			  "is-valid",
			  G_CALLBACK (brasero_project_is_valid),
			  obj);

	/* bottom */
	box = gtk_hbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (box), 0);
	gtk_widget_show (box);
	gtk_box_pack_end (GTK_BOX (obj), box, FALSE, TRUE, 0);

	table = gtk_table_new (4, 2, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (table), 0);
	gtk_table_set_col_spacings (GTK_TABLE (table), 6);
	gtk_widget_show (table);
	gtk_box_pack_end (GTK_BOX (obj), table, FALSE, TRUE, 0);

	/* Media selection widget */
	label = gtk_label_new_with_mnemonic (_("_Disc:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label,
			  1, 2,
			  0, 1,
			  GTK_FILL,
			  GTK_EXPAND,
			  0, 0);

	selector = brasero_dest_selection_new (BRASERO_BURN_SESSION (obj->priv->session));
	gtk_widget_show (selector);
	obj->priv->selection = selector;

	gtk_table_attach (GTK_TABLE (table), selector,
			  2, 3,
			  0, 1,
			  GTK_FILL|GTK_EXPAND,
			  GTK_FILL|GTK_EXPAND,
			  0, 0);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), selector);

	size_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);

	/* Properties/options buttons */
	button = brasero_medium_properties_new (obj->priv->session);
	gtk_size_group_add_widget (GTK_SIZE_GROUP (size_group), button);
	gtk_widget_show (button);
	gtk_button_set_focus_on_click (GTK_BUTTON (button), FALSE);
	gtk_table_attach (GTK_TABLE (table), button,
			  3, 4,
			  0, 1,
			  GTK_FILL,
			  GTK_EXPAND,
			  0, 0);

	/* burn button set insensitive since there are no files in the selection */
	obj->priv->burn = brasero_utils_make_button (_("_Burn"),
						     NULL,
						     "media-optical-burn",
						     GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (obj->priv->burn);
	gtk_widget_set_sensitive (obj->priv->burn, FALSE);
	gtk_button_set_focus_on_click (GTK_BUTTON (obj->priv->burn), FALSE);
	g_signal_connect (obj->priv->burn,
			  "clicked",
			  G_CALLBACK (brasero_project_burn_clicked_cb),
			  obj);
	gtk_widget_set_tooltip_text (obj->priv->burn,
				     _("Start to burn the contents of the selection"));
	gtk_size_group_add_widget (GTK_SIZE_GROUP (size_group), obj->priv->burn);

	alignment = gtk_alignment_new (1.0, 0.5, 0.0, 0.0);
	gtk_widget_show (alignment);
	gtk_container_add (GTK_CONTAINER (alignment), obj->priv->burn);
//	gtk_box_pack_end (GTK_BOX (box), alignment, FALSE, TRUE, 0);
	gtk_table_attach (GTK_TABLE (table), alignment,
			  3, 4,
			  1, 2,
			  GTK_FILL,
			  GTK_EXPAND,
			  0, 0);

	/* icon */
/*	label = gtk_label_new_with_mnemonic (_("_Icon:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	obj->priv->icon_label = label;
	gtk_table_attach (GTK_TABLE (table), label,
			  0, 1,
			  1, 2,
			  GTK_FILL,
			  GTK_EXPAND,
			  0, 0);
*/
	image = gtk_image_new_from_icon_name ("media-optical", GTK_ICON_SIZE_LARGE_TOOLBAR);
	gtk_widget_show (image);
	obj->priv->icon_img = image;

	button = gtk_button_new ();
	g_signal_connect (button,
			  "size-request",
			  G_CALLBACK (brasero_project_icon_button_size_request),
			  NULL);
	g_signal_connect (button,
			  "size-allocate",
			  G_CALLBACK (brasero_project_icon_button_size_allocate),
			  NULL);
	gtk_widget_show (button);
	gtk_button_set_image (GTK_BUTTON (button), image);
	obj->priv->button_img = button;

	gtk_widget_set_tooltip_text (button, _("Select an icon for the disc that will appear in file managers"));

	g_signal_connect (button,
			  "clicked",
			  G_CALLBACK (brasero_project_icon_button_clicked),
			  obj);

	alignment = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);
	gtk_widget_show (alignment);
	gtk_container_add (GTK_CONTAINER (alignment), button);

	gtk_table_attach (GTK_TABLE (table), alignment,
			  0, 1,
			  0, 2,
			  GTK_FILL,
			  GTK_FILL|GTK_EXPAND,
			  0, 0);

	/* Name widget */
	label = gtk_label_new_with_mnemonic (_("_Name:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table), label,
			  1, 2,
			  1, 2,
			  GTK_FILL,
			  GTK_EXPAND,
			  0, 0);

	obj->priv->name_display = brasero_project_name_new ();
	gtk_widget_show (obj->priv->name_display);
	gtk_table_attach (GTK_TABLE (table), obj->priv->name_display,
			  2, 3,
			  1, 2,
			  GTK_EXPAND|GTK_FILL,
			  GTK_EXPAND,
			  0, 0);
	obj->priv->empty = 1;

	g_signal_connect (obj->priv->name_display,
			  "name-changed",
			  G_CALLBACK (brasero_project_name_changed_cb),
			  obj);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), obj->priv->name_display);

	/* The three panes to put into the notebook */
	obj->priv->audio = brasero_audio_disc_new ();
	gtk_widget_show (obj->priv->audio);
	g_signal_connect (G_OBJECT (obj->priv->audio),
			  "contents-changed",
			  G_CALLBACK (brasero_project_contents_changed_cb),
			  obj);
	g_signal_connect (G_OBJECT (obj->priv->audio),
			  "size-changed",
			  G_CALLBACK (brasero_project_size_changed_cb),
			  obj);
	g_signal_connect (G_OBJECT (obj->priv->audio),
			  "selection-changed",
			  G_CALLBACK (brasero_project_selection_changed_cb),
			  obj);

	obj->priv->data = brasero_data_disc_new ();
	gtk_widget_show (obj->priv->data);
	brasero_data_disc_set_right_button_group (BRASERO_DATA_DISC (obj->priv->data), size_group);
	g_signal_connect (G_OBJECT (obj->priv->data),
			  "contents-changed",
			  G_CALLBACK (brasero_project_contents_changed_cb),
			  obj);
	g_signal_connect (G_OBJECT (obj->priv->data),
			  "size-changed",
			  G_CALLBACK (brasero_project_size_changed_cb),
			  obj);
	g_signal_connect (G_OBJECT (obj->priv->data),
			  "flags-changed",
			  G_CALLBACK (brasero_project_flags_changed_cb),
			  obj);
	g_signal_connect (G_OBJECT (obj->priv->data),
			  "selection-changed",
			  G_CALLBACK (brasero_project_selection_changed_cb),
			  obj);
	g_signal_connect (obj->priv->data,
			  "icon-changed",
			  G_CALLBACK (brasero_project_icon_changed_cb),
			  obj);

	obj->priv->video = brasero_video_disc_new ();
	gtk_widget_show (obj->priv->video);
	g_signal_connect (G_OBJECT (obj->priv->video),
			  "contents-changed",
			  G_CALLBACK (brasero_project_contents_changed_cb),
			  obj);
	g_signal_connect (G_OBJECT (obj->priv->video),
			  "size-changed",
			  G_CALLBACK (brasero_project_size_changed_cb),
			  obj);
	g_signal_connect (G_OBJECT (obj->priv->video),
			  "selection-changed",
			  G_CALLBACK (brasero_project_selection_changed_cb),
			  obj);

	obj->priv->discs = gtk_notebook_new ();
	gtk_widget_show (obj->priv->discs);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (obj->priv->discs), FALSE);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (obj->priv->discs), FALSE);

	gtk_notebook_prepend_page (GTK_NOTEBOOK (obj->priv->discs),
				   obj->priv->video, NULL);
	gtk_notebook_prepend_page (GTK_NOTEBOOK (obj->priv->discs),
				   obj->priv->data, NULL);
	gtk_notebook_prepend_page (GTK_NOTEBOOK (obj->priv->discs),
				   obj->priv->audio, NULL);

	gtk_box_pack_start (GTK_BOX (obj),
			    obj->priv->discs,
			    TRUE,
			    TRUE,
			    0);

	g_object_unref (size_group);
}

static void
brasero_project_finalize (GObject *object)
{
	BraseroProject *cobj;
	cobj = BRASERO_PROJECT(object);

	if (cobj->priv->project)
		g_free (cobj->priv->project);

	if (cobj->priv->cover)
		g_free (cobj->priv->cover);

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkWidget *
brasero_project_new ()
{
	BraseroProject *obj;
	
	obj = BRASERO_PROJECT (g_object_new (BRASERO_TYPE_PROJECT, NULL));
	gtk_notebook_set_current_page (GTK_NOTEBOOK (obj->priv->discs), 0);

	return GTK_WIDGET (obj);
}

/********************************** size ***************************************/

gchar *
brasero_project_get_sectors_string (gint64 sectors,
				    gboolean time_format)
{
	gint64 size_bytes;

	if (time_format) {
		size_bytes = sectors * GST_SECOND / 75;
		return brasero_units_get_time_string (size_bytes, TRUE, FALSE);
	}
	else {
		size_bytes = sectors * 2048;
		return g_format_size_for_display (size_bytes);
	}
}

static void
brasero_project_update_project_size (BraseroProject *project,
				     guint64 sectors)
{
	GtkWidget *status;
	gchar *string;
	gchar *size;

	status = brasero_app_get_statusbar2 (brasero_app_get_default ());

	if (!project->priv->status_ctx)
		project->priv->status_ctx = gtk_statusbar_get_context_id (GTK_STATUSBAR (status),
									  "size_project");

	gtk_statusbar_pop (GTK_STATUSBAR (status), project->priv->status_ctx);

	string = brasero_project_get_sectors_string (sectors,
						     !BRASERO_IS_DATA_DISC (project->priv->current));
	if (project->priv->merging) {
		gchar *medium_string;
		BraseroMedium *medium;
		gint64 free_space = 0;

		medium = brasero_data_disc_get_loaded_medium (BRASERO_DATA_DISC (project->priv->current));
		brasero_medium_get_free_space (medium,
					       &free_space,
					       NULL);

		medium_string = g_format_size_for_display (free_space);
		/* Translators: first %s is the size of the project and the 
		 * second %s is the remaining free space on the disc that is
		 * used for multisession */
		size = g_strdup_printf (_("Project estimated size: %s/%s"),
					string,
					medium_string);
		g_free (medium_string);
	}
	else
		size = g_strdup_printf (_("Project estimated size: %s"), string);

	g_free (string);

	gtk_statusbar_push (GTK_STATUSBAR (status), project->priv->status_ctx, size);
	g_free (size);
}

static void
brasero_project_size_changed_cb (BraseroDisc *disc,
			         gint64 sectors,
			         BraseroProject *project)
{
	project->priv->sectors = sectors;
	brasero_project_update_project_size (project, sectors);
}

static void
brasero_project_flags_changed_cb (BraseroDisc *disc,
				  BraseroBurnFlag flags,
				  BraseroProject *project)
{
	gboolean merging;

	merging = (flags & BRASERO_BURN_FLAG_MERGE) != 0;

	/* see if the project name should be updated */
	brasero_project_name_set_multisession_medium (BRASERO_PROJECT_NAME (project->priv->name_display),
						      brasero_data_disc_get_loaded_medium (BRASERO_DATA_DISC (disc)));

	/* we just need to know if MERGE flag is on */
	project->priv->merging = merging;
	brasero_project_update_project_size (project, project->priv->sectors);
}

/***************************** URIContainer ************************************/
static void
brasero_project_selection_changed_cb (BraseroDisc *disc,
				      BraseroProject *project)
{
	project->priv->selected_uris = brasero_disc_get_selected_uri (project->priv->current, NULL);
	brasero_project_set_remove_button_state (project);
	brasero_uri_container_uri_selected (BRASERO_URI_CONTAINER (project));
}

static gchar *
brasero_project_get_selected_uri (BraseroURIContainer *container)
{
	BraseroProject *project;
	gchar *uri = NULL;

	project = BRASERO_PROJECT (container);

	/* if we are burning we better not return anything so as to stop 
	 * preview widget from carrying on to play */
	if (project->priv->is_burning)
		return NULL;

	if (brasero_disc_get_selected_uri (project->priv->current, &uri))
		return uri;

	return NULL;
}

static gboolean
brasero_project_get_boundaries (BraseroURIContainer *container,
				gint64 *start,
				gint64 *end)
{
	BraseroProject *project;

	project = BRASERO_PROJECT (container);

	/* if we are burning we better not return anything so as to stop 
	 * preview widget from carrying on to play */
	if (project->priv->is_burning)
		return FALSE;

	return brasero_disc_get_boundaries (project->priv->current,
					    start,
					    end);
}

/******************** useful function to wait when burning/saving **************/
static gboolean
_wait_for_ready_state (GtkWidget *dialog)
{
	gchar *current_task = NULL;
	GtkProgressBar *progress;
	BraseroDiscResult status;
	BraseroProject *project;
	gint remaining = 0;
	gint initial;

	project = g_object_get_data (G_OBJECT (dialog), "Project");
	if (project->priv->oversized
	|| !project->priv->current
	|| !project->priv->project_status) {
		gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
		return FALSE;
	}

	progress = g_object_get_data (G_OBJECT (dialog), "ProgressBar");
	initial = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (dialog), "Remaining"));
	status = brasero_disc_get_status (project->priv->current, &remaining, &current_task);
	if (status == BRASERO_DISC_NOT_READY || status == BRASERO_DISC_LOADING) {
		gchar *string;
		gchar *size_str;

		if (initial <= 0 || remaining <= 0)
			gtk_progress_bar_pulse (progress);
		else
			gtk_progress_bar_set_fraction (progress, (gdouble) ((gdouble) (initial - remaining) / (gdouble) initial));

		if (current_task) {
			GtkWidget *current_action;

			current_action = g_object_get_data (G_OBJECT (dialog), "CurrentAction");
			string = g_strdup_printf ("<i>%s</i>", current_task);
			g_free (current_task);

			gtk_label_set_markup (GTK_LABEL (current_action), string);
			g_free (string);
		}

		string = brasero_project_get_sectors_string (project->priv->sectors,
							     !BRASERO_IS_DATA_DISC (project->priv->current));

		size_str = g_strdup_printf (_("Project estimated size: %s"), string);
		g_free (string);

		gtk_progress_bar_set_text (progress, size_str);
		g_free (size_str);

		return TRUE;
	}

	gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	return FALSE;
}

static BraseroDiscResult
brasero_project_check_status (BraseroProject *project,
			      BraseroDisc *disc)
{
	int id;
	int answer;
	GtkWidget *box;
	GtkWidget *dialog;
	gchar *current_task;
	GtkWidget *progress;
	gint remaining = -1;
	BraseroDiscResult result;
	GtkWidget *current_action;

	current_task = NULL;
	result = brasero_disc_get_status (disc, &remaining, &current_task);
	if (result != BRASERO_DISC_NOT_READY && result != BRASERO_DISC_LOADING)
		return result;

	/* we are not ready to create tracks presumably because
	 * data or audio has not finished to explore a directory
	 * or get the metadata of a song or a film  */

	/* This dialog can run as a standalone window when run from nautilus
	 * to burn burn:// URI contents. */
	dialog = brasero_app_dialog (brasero_app_get_default (),
				     _("Please wait until the estimation of the project size is completed."),
				     GTK_BUTTONS_CANCEL,
				     GTK_MESSAGE_OTHER);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  _("All files from the project need to be analysed to complete this operation."));

	gtk_window_set_title (GTK_WINDOW (dialog), _("Project Size Estimation"));

	box = gtk_vbox_new (FALSE, 4);
	gtk_box_pack_end (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			  box,
			  TRUE,
			  TRUE,
			  0);

	progress = gtk_progress_bar_new ();
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (progress), " ");
	gtk_box_pack_start (GTK_BOX (box),
			    progress,
			    TRUE,
			    TRUE,
			    0);

	if (current_task) {
		gchar *string;

		string = g_strdup_printf ("<i>%s</i>", current_task);
		g_free (current_task);

		current_action = gtk_label_new (string);
		g_free (string);
	}
	else
		current_action = gtk_label_new ("");

	gtk_label_set_use_markup (GTK_LABEL (current_action), TRUE);
	gtk_misc_set_alignment (GTK_MISC (current_action), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (box),
			    current_action,
			    FALSE,
			    TRUE,
			    0);

	gtk_widget_show_all (dialog);
	gtk_progress_bar_pulse (GTK_PROGRESS_BAR (progress));

	g_object_set_data (G_OBJECT (dialog), "CurrentAction", current_action);
	g_object_set_data (G_OBJECT (dialog), "ProgressBar", progress);
	g_object_set_data (G_OBJECT (dialog), "Remaining", GINT_TO_POINTER (remaining));
	g_object_set_data (G_OBJECT (dialog), "Project", project);

	id = g_timeout_add (100,
			    (GSourceFunc) _wait_for_ready_state,
		            dialog);

	project->priv->project_status = dialog;
	answer = gtk_dialog_run (GTK_DIALOG (dialog));
	g_source_remove (id);

	gtk_widget_destroy (dialog);

	if (!project->priv->project_status)
		return BRASERO_DISC_CANCELLED;

	project->priv->project_status = NULL;

	if (answer != GTK_RESPONSE_OK)
		return BRASERO_DISC_CANCELLED;
	else if (project->priv->oversized)
		return BRASERO_DISC_ERROR_SIZE;

	return brasero_disc_get_status (disc, NULL, NULL);
}

/******************************** burning **************************************/
static void
brasero_project_no_song_dialog (BraseroProject *project)
{
	brasero_app_alert (brasero_app_get_default (),
			   _("Please add songs to the project."),
			   _("The project is empty"),
			   GTK_MESSAGE_WARNING);
}

static void
brasero_project_no_file_dialog (BraseroProject *project)
{
	brasero_app_alert (brasero_app_get_default (),
			   _("Please add files to the project."),
			   _("The project is empty"),
			   GTK_MESSAGE_WARNING);
}

static void
brasero_project_setup_session (BraseroProject *project,
			       BraseroBurnSession *session)
{
	const gchar *label;

	label = gtk_entry_get_text (GTK_ENTRY (project->priv->name_display));
	brasero_burn_session_set_label (session, label);

	if (project->priv->cover) {
		GValue *value;

		value = g_new0 (GValue, 1);
		g_value_init (value, G_TYPE_STRING);
		g_value_set_string (value, project->priv->cover);
		brasero_burn_session_tag_add (session,
					      BRASERO_COVER_URI,
					      value);
	}
}

void
brasero_project_burn (BraseroProject *project)
{
	BraseroSessionCfg *session;
	BraseroDiscResult result;
	GtkWidget *dialog;
	gboolean success;

	result = brasero_project_check_status (project, project->priv->current);
	if (result == BRASERO_DISC_CANCELLED)
		return;

	if (result == BRASERO_DISC_ERROR_SIZE)
		return;

	if (result == BRASERO_DISC_ERROR_EMPTY_SELECTION) {
		if (BRASERO_IS_AUDIO_DISC (project->priv->current))
			brasero_project_no_song_dialog (project);
		else
			brasero_project_no_file_dialog (project);

		return;
	}

	if (result != BRASERO_DISC_OK)
		return;

	project->priv->is_burning = 1;

	/* This is to stop the preview widget from playing */
	brasero_uri_container_uri_selected (BRASERO_URI_CONTAINER (project));

  	/* setup, show, and run options dialog */
 	session = brasero_session_cfg_new ();
 	brasero_disc_set_session_contents (project->priv->current, BRASERO_BURN_SESSION (session));
 	dialog = brasero_burn_options_new (session);

	brasero_app_set_toplevel (brasero_app_get_default (), GTK_WINDOW (dialog));

	result = gtk_dialog_run (GTK_DIALOG (dialog));
	if (result != GTK_RESPONSE_OK) {
		g_object_unref (session);
		gtk_widget_destroy (dialog);
		goto end;
	}

	gtk_widget_destroy (dialog);

	brasero_project_setup_session (project, BRASERO_BURN_SESSION (session));

	/* now setup the burn dialog */
	success = brasero_app_burn (brasero_app_get_default (), BRASERO_BURN_SESSION (session));

    	project->priv->burnt = success;
	g_object_unref (session);

end:

	project->priv->is_burning = 0;
}

/******************************** cover ****************************************/
void
brasero_project_create_audio_cover (BraseroProject *project,
				    BraseroJacketEdit *cover)
{
	BraseroBurnSession *session;
	GtkWidget *window;

	if (!BRASERO_IS_AUDIO_DISC (project->priv->current))
		return;

	session = BRASERO_BURN_SESSION (brasero_session_cfg_new ());
	brasero_disc_set_session_contents (BRASERO_DISC (project->priv->current), session);
	brasero_project_setup_session (project, session);

	window = brasero_session_edit_cover (session, gtk_widget_get_toplevel (GTK_WIDGET (project)));
	g_object_unref (session);

	gtk_dialog_run (GTK_DIALOG (window));
	gtk_widget_destroy (window);
}

/********************************     ******************************************/
static void
brasero_project_switch (BraseroProject *project, BraseroProjectType type)
{
	GtkAction *action;
	GConfClient *client;
	
	if (project->priv->project_status) {
		gtk_widget_hide (project->priv->project_status);
		gtk_dialog_response (GTK_DIALOG (project->priv->project_status),
				     GTK_RESPONSE_CANCEL);
		project->priv->project_status = NULL;
	}

	gtk_image_set_from_icon_name (GTK_IMAGE (project->priv->icon_img),
				      "media-optical",
				      GTK_ICON_SIZE_LARGE_TOOLBAR);

	if (project->priv->current)
		brasero_disc_reset (project->priv->current);

	if (project->priv->chooser) {
		gtk_widget_destroy (project->priv->chooser);
		project->priv->chooser = NULL;
	}

	project->priv->empty = 1;
    	project->priv->burnt = 0;
	project->priv->merging = 0;
	project->priv->modified = 0;

	if (project->priv->project) {
		g_free (project->priv->project);
		project->priv->project = NULL;
	}

	if (project->priv->cover) {
		g_free (project->priv->cover);
		project->priv->cover = NULL;
	}

	client = gconf_client_get_default ();

	/* remove the buttons from the "toolbar" */
	if (project->priv->merge_id)
		gtk_ui_manager_remove_ui (project->priv->manager,
					  project->priv->merge_id);

	if (type == BRASERO_PROJECT_TYPE_AUDIO) {
		gtk_widget_hide (project->priv->button_img);

		project->priv->current = BRASERO_DISC (project->priv->audio);
		project->priv->merge_id = brasero_disc_add_ui (project->priv->current,
							       project->priv->manager,
							       project->priv->message);

		gtk_notebook_set_current_page (GTK_NOTEBOOK (project->priv->discs), 0);
		brasero_project_update_project_size (project, 0);

		brasero_medium_selection_show_media_type (BRASERO_MEDIUM_SELECTION (project->priv->selection),
							  BRASERO_MEDIA_TYPE_WRITABLE);
		brasero_dest_selection_choose_best (BRASERO_DEST_SELECTION (project->priv->selection));
	}
	else if (type == BRASERO_PROJECT_TYPE_DATA) {
		gtk_widget_show (project->priv->button_img);

		project->priv->current = BRASERO_DISC (project->priv->data);
		project->priv->merge_id = brasero_disc_add_ui (project->priv->current,
							       project->priv->manager,
							       project->priv->message);

		gtk_notebook_set_current_page (GTK_NOTEBOOK (project->priv->discs), 1);
		brasero_project_update_project_size (project, 0);

		brasero_medium_selection_show_media_type (BRASERO_MEDIUM_SELECTION (project->priv->selection),
							  BRASERO_MEDIA_TYPE_WRITABLE|
							  BRASERO_MEDIA_TYPE_FILE);
		brasero_dest_selection_choose_best (BRASERO_DEST_SELECTION (project->priv->selection));
	}
	else if (type == BRASERO_PROJECT_TYPE_VIDEO) {
		gtk_widget_hide (project->priv->button_img);

		project->priv->current = BRASERO_DISC (project->priv->video);
		project->priv->merge_id = brasero_disc_add_ui (project->priv->current,
							       project->priv->manager,
							       project->priv->message);

		gtk_notebook_set_current_page (GTK_NOTEBOOK (project->priv->discs), 2);
		brasero_project_update_project_size (project, 0);

		brasero_medium_selection_show_media_type (BRASERO_MEDIUM_SELECTION (project->priv->selection),
							  BRASERO_MEDIA_TYPE_WRITABLE|
							  BRASERO_MEDIA_TYPE_FILE);
		brasero_dest_selection_choose_best (BRASERO_DEST_SELECTION (project->priv->selection));
	}

	if (project->priv->current)
		brasero_disc_set_session_contents (project->priv->current, BRASERO_BURN_SESSION (project->priv->session));

	brasero_notify_message_remove (BRASERO_NOTIFY (project->priv->message), BRASERO_NOTIFY_CONTEXT_SIZE);

	/* update the menus */
	action = gtk_action_group_get_action (project->priv->project_group, "Add");
	gtk_action_set_visible (action, TRUE);
	gtk_action_set_sensitive (action, TRUE);
	action = gtk_action_group_get_action (project->priv->project_group, "DeleteProject");
	gtk_action_set_visible (action, TRUE);
	gtk_action_set_sensitive (action, FALSE);
	action = gtk_action_group_get_action (project->priv->project_group, "DeleteAll");
	gtk_action_set_visible (action, TRUE);
	gtk_action_set_sensitive (action, FALSE);
	action = gtk_action_group_get_action (project->priv->project_group, "SaveAs");
	gtk_action_set_sensitive (action, TRUE);
	action = gtk_action_group_get_action (project->priv->project_group, "Save");
	gtk_action_set_sensitive (action, FALSE);

	brasero_project_name_set_type (BRASERO_PROJECT_NAME (project->priv->name_display), type);
}

void
brasero_project_set_audio (BraseroProject *project, GSList *uris)
{
	brasero_project_switch (project, BRASERO_PROJECT_TYPE_AUDIO);

	for (; uris; uris = uris->next) {
		gchar *uri;

	    	uri = uris->data;
		brasero_disc_add_uri (project->priv->current, uri);
	}
}

void
brasero_project_set_data (BraseroProject *project,
			  GSList *uris)
{
	brasero_project_switch (project, BRASERO_PROJECT_TYPE_DATA);

	for (; uris; uris = uris->next) {
		gchar *uri;

	    	uri = uris->data;
		brasero_disc_add_uri (project->priv->current, uri);
	}
}

void
brasero_project_set_video (BraseroProject *project, GSList *uris)
{
	brasero_project_switch (project, BRASERO_PROJECT_TYPE_VIDEO);

	for (; uris; uris = uris->next) {
		gchar *uri;

	    	uri = uris->data;
		brasero_disc_add_uri (project->priv->current, uri);
	}
}

gboolean
brasero_project_confirm_switch (BraseroProject *project)
{
	GtkWidget *dialog;
	GtkResponseType answer;

	if (project->priv->project) {
		if (!project->priv->modified)
			return TRUE;

		dialog = brasero_app_dialog (brasero_app_get_default (),
					     _("Do you really want to create a new project and discard the changes to current one?"),
					     GTK_BUTTONS_CANCEL,
					     GTK_MESSAGE_WARNING);

		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  _("If you choose to create a new project, all changes made will be lost."));
		gtk_dialog_add_button (GTK_DIALOG (dialog),
				       _("_Discard Changes"), GTK_RESPONSE_OK);

	}
	else {
		if (project->priv->empty)
			return TRUE;

		dialog = brasero_app_dialog (brasero_app_get_default (),
					     _("Do you really want to create a new project and discard the current one?"),
					     GTK_BUTTONS_CANCEL,
					     GTK_MESSAGE_WARNING);

		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  _("If you choose to create a new project, "
							    "all files already added will be discarded. "
							    "Note that files will not be deleted from their own location, "
							    "just no longer listed here."));
		gtk_dialog_add_button (GTK_DIALOG (dialog),
				       _("_Discard Project"), GTK_RESPONSE_OK);
	}

	answer = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	if (answer != GTK_RESPONSE_OK)
		return FALSE;

	return TRUE;
}

void
brasero_project_set_none (BraseroProject *project)
{
	GtkAction *action;
	GtkWidget *status;

	if (project->priv->project_status) {
		gtk_widget_hide (project->priv->project_status);
		gtk_dialog_response (GTK_DIALOG (project->priv->project_status),
				     GTK_RESPONSE_CANCEL);
		project->priv->project_status = NULL;
	}

	if (project->priv->chooser) {
		gtk_widget_destroy (project->priv->chooser);
		project->priv->chooser = NULL;
	}

	if (project->priv->project) {
		g_free (project->priv->project);
		project->priv->project = NULL;
	}

	if (project->priv->cover) {
		g_free (project->priv->cover);
		project->priv->cover = NULL;
	}

	if (project->priv->chooser) {
		gtk_widget_destroy (project->priv->chooser);
		project->priv->chooser = NULL;
	}

	if (project->priv->current)
		brasero_disc_reset (project->priv->current);

	project->priv->current = NULL;

	/* update buttons/menus */
	action = gtk_action_group_get_action (project->priv->project_group, "Add");
	gtk_action_set_visible (action, FALSE);
	action = gtk_action_group_get_action (project->priv->project_group, "DeleteProject");
	gtk_action_set_visible (action, FALSE);
	action = gtk_action_group_get_action (project->priv->project_group, "DeleteAll");
	gtk_action_set_visible (action, FALSE);
	action = gtk_action_group_get_action (project->priv->project_group, "SaveAs");
	gtk_action_set_sensitive (action, FALSE);
	action = gtk_action_group_get_action (project->priv->project_group, "Save");
	gtk_action_set_sensitive (action, FALSE);

	if (project->priv->merge_id)
		gtk_ui_manager_remove_ui (project->priv->manager,
					  project->priv->merge_id);

	status = brasero_app_get_statusbar2 (brasero_app_get_default ());

	if (project->priv->status_ctx)
		gtk_statusbar_pop (GTK_STATUSBAR (status), project->priv->status_ctx);
}

/********************* update the appearance of menus and buttons **************/
static void
brasero_project_contents_changed_cb (BraseroDisc *disc,
				     gint nb_files,
				     BraseroProject *project)
{
	GtkAction *action;
	gboolean sensitive;

	project->priv->empty = (nb_files == 0);

	if (brasero_disc_get_status (disc, NULL, NULL) != BRASERO_DISC_LOADING)
		project->priv->modified = 1;

	brasero_project_set_remove_button_state (project);
	brasero_project_set_add_button_state (project);

	action = gtk_action_group_get_action (project->priv->project_group, "DeleteAll");
	gtk_action_set_sensitive (action, (project->priv->empty == FALSE));

	/* the following button/action states depend on the project size too */
	sensitive = (project->priv->oversized == 0 &&
		     project->priv->empty == 0);

	action = gtk_action_group_get_action (project->priv->project_group, "Burn");
	gtk_action_set_sensitive (action, sensitive);
	gtk_widget_set_sensitive (project->priv->burn, sensitive);

	/* the state of the following depends on the existence of an opened project */
	action = gtk_action_group_get_action (project->priv->project_group, "Save");
	if (project->priv->modified)
		gtk_action_set_sensitive (action, TRUE);
	else
		gtk_action_set_sensitive (action, FALSE);
}

/**************************** manage the relations with the sources ************/
static void
brasero_project_transfer_uris_from_src (BraseroProject *project)
{
	gchar **uris;
	gchar **uri;

	if (!project->priv->current_source)
		return;

	uris = brasero_uri_container_get_selected_uris (project->priv->current_source);
	if (!uris)
		return;

	uri = uris;
	while (*uri) {
		brasero_disc_add_uri (project->priv->current, *uri);
		uri ++;
	}

	g_strfreev (uris);
}

static void
brasero_project_source_uri_activated_cb (BraseroURIContainer *container,
					 BraseroProject *project)
{
	brasero_project_transfer_uris_from_src (project);
}

static void
brasero_project_source_uri_selected_cb (BraseroURIContainer *container,
					BraseroProject *project)
{
	brasero_project_set_add_button_state (project);
}

void
brasero_project_set_source (BraseroProject *project,
			    BraseroURIContainer *source)
{
	if (project->priv->activated_id) {
		g_signal_handler_disconnect (project->priv->current_source,
					     project->priv->activated_id);
		project->priv->activated_id = 0;
	}

	if (project->priv->selected_id) {
		g_signal_handler_disconnect (project->priv->current_source,
					     project->priv->selected_id);
		project->priv->selected_id = 0;
	}

	project->priv->current_source = source;

	if (source) {
		project->priv->activated_id = g_signal_connect (source,
							        "uri-activated",
							        G_CALLBACK (brasero_project_source_uri_activated_cb),
							        project);
		project->priv->selected_id = g_signal_connect (source,
							       "uri-selected",
							       G_CALLBACK (brasero_project_source_uri_selected_cb),
							       project);
	}

	brasero_project_set_add_button_state (project);
}

/******************************* menus/buttons *********************************/
static void
brasero_project_save_cb (GtkAction *action, BraseroProject *project)
{
	brasero_project_save_project (project);
}

static void
brasero_project_save_as_cb (GtkAction *action, BraseroProject *project)
{
	brasero_project_save_project_as (project);
}

static void
brasero_project_file_chooser_activated_cb (GtkWidget *chooser,
					   BraseroProject *project)
{
	GSList *uris;
	GSList *iter;

	uris = gtk_file_chooser_get_uris (GTK_FILE_CHOOSER (chooser));
	for (iter = uris; iter; iter = iter->next) {
		gchar *uri;

		uri = iter->data;
		brasero_disc_add_uri (project->priv->current, uri);
	}
	g_slist_foreach (uris, (GFunc) g_free, NULL);
	g_slist_free (uris);

	gtk_widget_destroy (GTK_WIDGET (project->priv->chooser));
	project->priv->chooser = NULL;
}

static void
brasero_project_file_chooser_response_cb (GtkWidget *chooser,
					  GtkResponseType response,
					  BraseroProject *project)
{
	GSList *uris;
	GSList *iter;

	if (response != BRASERO_RESPONSE_ADD) {
		gtk_widget_destroy (chooser);
		project->priv->chooser = NULL;
		return;
	}

	uris = gtk_file_chooser_get_uris (GTK_FILE_CHOOSER (chooser));
	for (iter = uris; iter; iter = iter->next) {
		gchar *uri;

		uri = iter->data;
		brasero_disc_add_uri (project->priv->current, uri);
	}
	g_slist_foreach (uris, (GFunc) g_free, NULL);
	g_slist_free (uris);

	gtk_widget_destroy (GTK_WIDGET (project->priv->chooser));
	project->priv->chooser = NULL;
}

static void
brasero_project_preview_ready (BraseroPlayer *player,
			       GtkFileChooser *chooser)
{
	gtk_file_chooser_set_preview_widget_active (chooser, TRUE);
}

static void
brasero_project_update_preview (GtkFileChooser *chooser,
				BraseroPlayer *player)
{
	gchar *uri;

	gtk_file_chooser_set_preview_widget_active (chooser, FALSE);

	uri = gtk_file_chooser_get_preview_uri (chooser);
	brasero_player_set_uri (player, uri);
	g_free (uri);
}

static void
brasero_project_add_uris_cb (GtkAction *action,
			     BraseroProject *project)
{
	GtkWidget *toplevel;
	GtkFileFilter *filter;

	if (project->priv->current_source) {
		brasero_project_transfer_uris_from_src (project);
		return;
	}

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (project));
	project->priv->chooser = gtk_file_chooser_dialog_new (_("Select Files"),
							      GTK_WINDOW (toplevel),
							      GTK_FILE_CHOOSER_ACTION_OPEN,
							      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
							      NULL);
	gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (project->priv->chooser), TRUE);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (project->priv->chooser), TRUE);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (project->priv->chooser), g_get_home_dir ());
	brasero_file_chooser_customize (project->priv->chooser, NULL);
	gtk_widget_show (project->priv->chooser);

	/* This is to work around a bug in GTK+ which doesn't want to add "Add"
	 * button or anything that is not "Open" or "Cancel" buttons */
	/* Just for the record, file chooser creation uses all GtkResponseType
	 * that are already defined for internal use like GTK_RESPONSE_OK,
	 * *_APPLY and so on (usually to open directories, not add them). So we
	 * have to define on custom here. */
	gtk_dialog_add_button (GTK_DIALOG (project->priv->chooser),
			       GTK_STOCK_ADD,
			       BRASERO_RESPONSE_ADD);
	gtk_dialog_set_default_response (GTK_DIALOG (project->priv->chooser),
					 BRASERO_RESPONSE_ADD);

	g_signal_connect (project->priv->chooser,
			  "file-activated",
			  G_CALLBACK (brasero_project_file_chooser_activated_cb),
			  project);
	g_signal_connect (project->priv->chooser,
			  "response",
			  G_CALLBACK (brasero_project_file_chooser_response_cb),
			  project);
	g_signal_connect (project->priv->chooser,
			  "close",
			  G_CALLBACK (brasero_project_file_chooser_activated_cb),
			  project);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("All files"));
	gtk_file_filter_add_pattern (filter, "*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (project->priv->chooser), filter);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("Audio files"));
	gtk_file_filter_add_mime_type (filter, "audio/*");
	gtk_file_filter_add_mime_type (filter, "application/ogg");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (project->priv->chooser), filter);

	if (BRASERO_IS_AUDIO_DISC (project->priv->current))
		gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (project->priv->chooser), filter);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("Movies"));
	gtk_file_filter_add_mime_type (filter, "video/*");
	gtk_file_filter_add_mime_type (filter, "application/ogg");
	gtk_file_filter_add_mime_type (filter, "application/x-flash-video");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (project->priv->chooser), filter);

	filter = gtk_file_filter_new ();
	/* Translators: this is an image, a picture, not a "Disc Image" */
	gtk_file_filter_set_name (filter, C_("picture", "Image files"));
	gtk_file_filter_add_mime_type (filter, "image/*");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (project->priv->chooser), filter);

#ifdef BUILD_PREVIEW

	GConfClient *client;
	GtkWidget *player;
	gboolean res;

	client = gconf_client_get_default ();
	res = gconf_client_get_bool (client, BRASERO_KEY_SHOW_PREVIEW, NULL);
	g_object_unref (client);

	if (!res)
		return;

	/* if preview is activated add it */
	player = brasero_player_new ();

	gtk_widget_show (player);
	gtk_file_chooser_set_preview_widget_active (GTK_FILE_CHOOSER (project->priv->chooser), FALSE);
	gtk_file_chooser_set_use_preview_label (GTK_FILE_CHOOSER (project->priv->chooser), FALSE);
	gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (project->priv->chooser), player);

	g_signal_connect (project->priv->chooser,
			  "update-preview",
			  G_CALLBACK (brasero_project_update_preview),
			  player);

	g_signal_connect (player,
			  "ready",
			  G_CALLBACK (brasero_project_preview_ready),
			  project->priv->chooser);
#endif

}

static void
brasero_project_remove_selected_uris_cb (GtkAction *action, BraseroProject *project)
{
	brasero_disc_delete_selected (BRASERO_DISC (project->priv->current));
}

static void
brasero_project_empty_cb (GtkAction *action, BraseroProject *project)
{
	if (!project->priv->empty) {
		GtkWidget *dialog;
		GtkResponseType answer;

		dialog = brasero_app_dialog (brasero_app_get_default (),
					      _("Do you really want to empty the current project?"),
					     GTK_BUTTONS_CANCEL,
					     GTK_MESSAGE_WARNING);

		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  _("Emptying a project will remove all files already added. "
							    "All the work will be lost. "
							    "Note that files will not be deleted from their own location, "
							    "just no longer listed here."));
		gtk_dialog_add_button (GTK_DIALOG (dialog),
					/* Translators: "empty" is a verb here */
				       _("E_mpty Project"),
				       GTK_RESPONSE_OK);

		answer = gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		if (answer != GTK_RESPONSE_OK)
			return;
	}

	brasero_disc_clear (BRASERO_DISC (project->priv->current));
}

static void
brasero_project_burn_cb (GtkAction *action, BraseroProject *project)
{
	brasero_project_burn (project);
}

static void
brasero_project_burn_clicked_cb (GtkButton *button, BraseroProject *project)
{
	brasero_project_burn (project);
}

void
brasero_project_register_ui (BraseroProject *project, GtkUIManager *manager)
{
	GError *error = NULL;
	GtkAction *action;

	/* menus */
	project->priv->project_group = gtk_action_group_new ("ProjectActions1");
	gtk_action_group_set_translation_domain (project->priv->project_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (project->priv->project_group,
				      entries,
				      G_N_ELEMENTS (entries),
				      project);

	gtk_ui_manager_insert_action_group (manager, project->priv->project_group, 0);
	if (!gtk_ui_manager_add_ui_from_string (manager,
						description,
						-1,
						&error)) {
		g_message ("building menus/toolbar failed: %s", error->message);
		g_error_free (error);
	}
	
	action = gtk_action_group_get_action (project->priv->project_group, "Save");
	g_object_set (action,
		      "short-label", _("_Save"), /* for toolbar buttons */
		      NULL);
	gtk_action_set_sensitive (action, FALSE);
	action = gtk_action_group_get_action (project->priv->project_group, "SaveAs");
	gtk_action_set_sensitive (action, FALSE);

	action = gtk_action_group_get_action (project->priv->project_group, "Burn");
	gtk_action_set_sensitive (action, FALSE);
	action = gtk_action_group_get_action (project->priv->project_group, "Add");
	gtk_action_set_sensitive (action, FALSE);
	g_object_set (action,
		      "short-label", _("_Add"), /* for toolbar buttons */
		      NULL);
	action = gtk_action_group_get_action (project->priv->project_group, "DeleteProject");
	gtk_action_set_sensitive (action, FALSE);
	g_object_set (action,
		      "short-label", _("_Remove"), /* for toolbar buttons */
		      NULL);
	action = gtk_action_group_get_action (project->priv->project_group, "DeleteAll");
	gtk_action_set_sensitive (action, FALSE);

	project->priv->manager = manager;
}

/******************************* common to save/open ***************************/
static void
brasero_project_add_to_recents (BraseroProject *project,
				const gchar *uri,
				gboolean is_project)
{
   	GtkRecentManager *recent;
	gchar *groups [] = { "brasero", NULL };
	gchar *open_playlist = "brasero -l %u";
	GtkRecentData recent_data = { NULL,
				      NULL,
				      "application/x-brasero",
				      "brasero",
				      "brasero -p %u",
				      groups,
				      FALSE };

    	recent = gtk_recent_manager_get_default ();

	if (is_project)
		recent_data.app_exec = open_playlist;

    	gtk_recent_manager_add_full (recent, uri, &recent_data);
}

static void
brasero_project_set_uri (BraseroProject *project,
			 const gchar *uri,
			 BraseroProjectType type)
{
     	gchar *name;
	gchar *title;
	GtkAction *action;
	GtkWidget *toplevel;

	/* possibly reset the name of the project */
	if (uri) {
		if (project->priv->project)
			g_free (project->priv->project);

		project->priv->project = g_strdup (uri);
	}

	uri = uri ? uri : project->priv->project;

	/* add it to recent manager */
	if (brasero_app_is_running (brasero_app_get_default ()))
		brasero_project_add_to_recents (project, uri, TRUE);

	/* update the name of the main window */
    	BRASERO_GET_BASENAME_FOR_DISPLAY (uri, name);
	if (type == BRASERO_PROJECT_TYPE_DATA)
		/* Translators: %s is the name of the project */
		title = g_strdup_printf (_("Brasero - %s (Data Disc)"), name);
	else if (type == BRASERO_PROJECT_TYPE_AUDIO)
		/* Translators: %s is the name of the project */
		title = g_strdup_printf (_("Brasero - %s (Audio Disc)"), name);
	else if (type == BRASERO_PROJECT_TYPE_VIDEO)
		/* Translators: %s is the name of the project */
		title = g_strdup_printf (_("Brasero - %s (Video Disc)"), name);
	else
		title = NULL;
 
	g_free (name);

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (project));
	if (toplevel)
		gtk_window_set_title (GTK_WINDOW (toplevel), title);
	g_free (title);

	/* update the menus */
	action = gtk_action_group_get_action (project->priv->project_group, "Save");
	gtk_action_set_sensitive (action, FALSE);
}

/******************************* Projects **************************************/
BraseroProjectType
brasero_project_open_project (BraseroProject *project,
			      BraseroDiscTrack *track,
			      const gchar *uri)	/* escaped */
{
	BraseroProjectType type;

	if (!track)
		return BRASERO_PROJECT_TYPE_INVALID;

	brasero_project_update_project_size (project, 0);

	if (track->type == BRASERO_PROJECT_TYPE_AUDIO)
		type = BRASERO_PROJECT_TYPE_AUDIO;
	else if (track->type == BRASERO_PROJECT_TYPE_DATA)
		type = BRASERO_PROJECT_TYPE_DATA;
	else if (track->type == BRASERO_PROJECT_TYPE_VIDEO)
		type = BRASERO_PROJECT_TYPE_VIDEO;
	else
		return BRASERO_PROJECT_TYPE_INVALID;

	brasero_project_switch (project, type);

	if (track->label) {
		g_signal_handlers_block_by_func (project->priv->name_display,
						 brasero_project_name_changed_cb,
						 project);
		gtk_entry_set_text (GTK_ENTRY (project->priv->name_display), track->label);
		g_signal_handlers_unblock_by_func (project->priv->name_display,
						   brasero_project_name_changed_cb,
						   project);
	}

	if (track->cover) {
		if (project->priv->cover)
			g_free (project->priv->cover);

		project->priv->cover = g_strdup (track->cover);
	}

	brasero_disc_load_track (project->priv->current, track);
	project->priv->modified = 0;

	if (uri)
		brasero_project_set_uri (project, uri, type);

	return type;
}

BraseroProjectType
brasero_project_load_session (BraseroProject *project, const gchar *uri)
{
	BraseroDiscTrack *track = NULL;
	BraseroProjectType type;

	if (!brasero_project_open_project_xml (uri, &track, FALSE))
		return BRASERO_PROJECT_TYPE_INVALID;

	if (track->type == BRASERO_PROJECT_TYPE_AUDIO)
		type = BRASERO_PROJECT_TYPE_AUDIO;
	else if (track->type == BRASERO_PROJECT_TYPE_DATA)
		type = BRASERO_PROJECT_TYPE_DATA;
	else if (track->type == BRASERO_PROJECT_TYPE_VIDEO)
		type = BRASERO_PROJECT_TYPE_VIDEO;
	else {
	    	brasero_track_free (track);
		return BRASERO_PROJECT_TYPE_INVALID;
	}

	brasero_project_switch (project, type);

	g_signal_handlers_block_by_func (project->priv->name_display,
					 brasero_project_name_changed_cb,
					 project);
	gtk_entry_set_text (GTK_ENTRY (project->priv->name_display), (gchar *) track->label);
	g_signal_handlers_unblock_by_func (project->priv->name_display,
					   brasero_project_name_changed_cb,
					   project);

	brasero_disc_load_track (project->priv->current, track);
	brasero_track_free (track);

	project->priv->modified = 0;

    	return type;
}

/******************************** save project *********************************/
static void
brasero_project_not_saved_dialog (BraseroProject *project)
{
	xmlError *error;

	error = xmlGetLastError ();
	brasero_app_alert (brasero_app_get_default (),
			   _("Your project has not been saved."),
			   error? error->message:_("An unknown error occured"),
			   GTK_MESSAGE_ERROR);
	xmlResetLastError ();
}

static GtkResponseType
brasero_project_save_project_dialog (BraseroProject *project,
				     gboolean show_cancel)
{
	GtkWidget *dialog;
	GtkResponseType result;

	dialog = brasero_app_dialog (brasero_app_get_default (),
				     _("Save the changes of current project before closing?"),
				     GTK_BUTTONS_NONE,
				     GTK_MESSAGE_WARNING);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  _("If you don't save, changes will be permanently lost."));

	if (show_cancel)
		gtk_dialog_add_buttons (GTK_DIALOG (dialog),
					_("Cl_ose Without Saving"), GTK_RESPONSE_NO,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_SAVE, GTK_RESPONSE_YES,
					NULL);
	else
		gtk_dialog_add_buttons (GTK_DIALOG (dialog),
					_("Cl_ose Without Saving"), GTK_RESPONSE_NO,
					GTK_STOCK_SAVE, GTK_RESPONSE_YES,
					NULL);

	result = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	if (result == GTK_RESPONSE_CANCEL)
		return GTK_RESPONSE_CANCEL;

	if (show_cancel && result == GTK_RESPONSE_DELETE_EVENT)
		return GTK_RESPONSE_CANCEL;

	if (result != GTK_RESPONSE_YES)
		return GTK_RESPONSE_NO;

	return GTK_RESPONSE_YES;
}

static gboolean
_save_audio_track_xml (xmlTextWriter *project,
		       BraseroDiscTrack *track)
{
	GSList *iter;
	gint success;

	for (iter = track->contents.tracks; iter; iter = iter->next) {
		BraseroDiscSong *song;
		BraseroStreamInfo *info;
		xmlChar *escaped;
		gchar *start;
		gchar *isrc;
		gchar *end;

		song = iter->data;
		info = song->info;

		escaped = (unsigned char *) g_uri_escape_string (song->uri, NULL, FALSE);
		success = xmlTextWriterWriteElement (project,
						    (xmlChar *) "uri",
						     escaped);
		g_free (escaped);

		if (success == -1)
			return FALSE;

		if (song->gap) {
			gchar *silence;

			silence = g_strdup_printf ("%"G_GINT64_FORMAT, song->gap);
			success = xmlTextWriterWriteElement (project,
							     (xmlChar *) "silence",
							     (xmlChar *) silence);

			g_free (silence);
			if (success == -1)
				return FALSE;
		}

		if (song->end > 0) {
			/* start of the song */
			start = g_strdup_printf ("%"G_GINT64_FORMAT, song->start);
			success = xmlTextWriterWriteElement (project,
							     (xmlChar *) "start",
							     (xmlChar *) start);

			g_free (start);
			if (success == -1)
				return FALSE;

			/* end of the song */
			end = g_strdup_printf ("%"G_GINT64_FORMAT, song->end);
			success = xmlTextWriterWriteElement (project,
							     (xmlChar *) "end",
							     (xmlChar *) end);

			g_free (end);
			if (success == -1)
				return FALSE;
		}

		if (!info)
			continue;

		if (info->title) {
			escaped = (unsigned char *) g_uri_escape_string (info->title, NULL, FALSE);
			success = xmlTextWriterWriteElement (project,
							    (xmlChar *) "title",
							     escaped);
			g_free (escaped);

			if (success == -1)
				return FALSE;
		}

		if (info->artist) {
			escaped = (unsigned char *) g_uri_escape_string (info->artist, NULL, FALSE);
			success = xmlTextWriterWriteElement (project,
							    (xmlChar *) "artist",
							     escaped);
			g_free (escaped);

			if (success == -1)
				return FALSE;
		}

		if (info->composer) {
			escaped = (unsigned char *) g_uri_escape_string (info->composer, NULL, FALSE);
			success = xmlTextWriterWriteElement (project,
							    (xmlChar *) "composer",
							     escaped);
			g_free (escaped);

			if (success == -1)
				return FALSE;
		}

		if (info->isrc) {
			isrc = g_strdup_printf ("%d", info->isrc);
			success = xmlTextWriterWriteElement (project,
							     (xmlChar *) "isrc",
							     (xmlChar *) isrc);

			g_free (isrc);
			if (success == -1)
				return FALSE;
		}
	}

	return TRUE;
}

static gboolean
_save_data_track_xml (xmlTextWriter *project,
		      BraseroDiscTrack *track)
{
	gchar *uri;
	gint success;
	GSList *iter;
	GSList *grafts;
	BraseroGraftPt *graft;

	if (track->contents.data.icon) {
		/* Write the icon if any */
		success = xmlTextWriterWriteElement (project, (xmlChar *) "icon", (xmlChar *) track->contents.data.icon);
		if (success < 0)
			return FALSE;
	}

	for (grafts = track->contents.data.grafts; grafts; grafts = grafts->next) {
		graft = grafts->data;

		success = xmlTextWriterStartElement (project, (xmlChar *) "graft");
		if (success < 0)
			return FALSE;

		success = xmlTextWriterWriteElement (project, (xmlChar *) "path", (xmlChar *) graft->path);
		if (success < 0)
			return FALSE;

		if (graft->uri) {
			xmlChar *escaped;

			escaped = (unsigned char *) g_uri_escape_string (graft->uri, NULL, FALSE);
			success = xmlTextWriterWriteElement (project, (xmlChar *) "uri", escaped);
			g_free (escaped);
			if (success < 0)
				return FALSE;
		}

		success = xmlTextWriterEndElement (project); /* graft */
		if (success < 0)
			return FALSE;
	}

	/* save excluded uris */
	for (iter = track->contents.data.excluded; iter; iter = iter->next) {
		xmlChar *escaped;

		escaped = xmlURIEscapeStr ((xmlChar *) iter->data, NULL);
		success = xmlTextWriterWriteElement (project, (xmlChar *) "excluded", (xmlChar *) escaped);
		g_free (escaped);
		if (success < 0)
			return FALSE;
	}

	/* save restored uris */
	for (iter = track->contents.data.restored; iter; iter = iter->next) {
		uri = iter->data;
		success = xmlTextWriterWriteElement (project, (xmlChar *) "restored", (xmlChar *) uri);
		if (success < 0)
			return FALSE;
	}

	/* NOTE: we don't write symlinks and unreadable they are useless */
	return TRUE;
}

static gboolean 
brasero_project_save_project_xml (BraseroProject *proj,
				  const gchar *uri,
				  BraseroDiscTrack *track,
				  gboolean use_dialog)
{
	xmlTextWriter *project;
	gboolean retval;
	gint success;
    	gchar *path;

	path = g_filename_from_uri (uri, NULL, NULL);
	if (!path)
		return FALSE;

	project = xmlNewTextWriterFilename (path, 0);
	if (!project) {
		g_free (path);

	    	if (use_dialog)
			brasero_project_not_saved_dialog (proj);

		return FALSE;
	}

	xmlTextWriterSetIndent (project, 1);
	xmlTextWriterSetIndentString (project, (xmlChar *) "\t");

	success = xmlTextWriterStartDocument (project,
					      NULL,
					      "UTF8",
					      NULL);
	if (success < 0)
		goto error;

	success = xmlTextWriterStartElement (project, (xmlChar *) "braseroproject");
	if (success < 0)
		goto error;

	/* write the name of the version */
	success = xmlTextWriterWriteElement (project,
					     (xmlChar *) "version",
					     (xmlChar *) BRASERO_PROJECT_VERSION);
	if (success < 0)
		goto error;

	success = xmlTextWriterWriteElement (project,
					     (xmlChar *) "label",
					     (xmlChar *) gtk_entry_get_text (GTK_ENTRY (proj->priv->name_display)));
	if (success < 0)
		goto error;

	if (proj->priv->cover) {
		gchar *escaped;

		escaped = g_uri_escape_string (proj->priv->cover, NULL, FALSE);
		success = xmlTextWriterWriteElement (project,
						     (xmlChar *) "cover",
						     (xmlChar *) escaped);
		g_free (escaped);

		if (success < 0)
			goto error;
	}

	success = xmlTextWriterStartElement (project, (xmlChar *) "track");
	if (success < 0)
		goto error;

	if (track->type == BRASERO_PROJECT_TYPE_AUDIO) {
		success = xmlTextWriterStartElement (project, (xmlChar *) "audio");
		if (success < 0)
			goto error;

		retval = _save_audio_track_xml (project, track);
		if (!retval)
			goto error;

		success = xmlTextWriterEndElement (project); /* audio */
		if (success < 0)
			goto error;
	}
	else if (track->type == BRASERO_PROJECT_TYPE_DATA) {
		success = xmlTextWriterStartElement (project, (xmlChar *) "data");
		if (success < 0)
			goto error;

		retval = _save_data_track_xml (project, track);
		if (!retval)
			goto error;

		success = xmlTextWriterEndElement (project); /* data */
		if (success < 0)
			goto error;
	}
	else  if (track->type == BRASERO_PROJECT_TYPE_VIDEO) {
		success = xmlTextWriterStartElement (project, (xmlChar *) "video");
		if (success < 0)
			goto error;

		retval = _save_audio_track_xml (project, track);
		if (!retval)
			goto error;

		success = xmlTextWriterEndElement (project); /* audio */
		if (success < 0)
			goto error;
	}
	else
		retval = FALSE;

	success = xmlTextWriterEndElement (project); /* track */
	if (success < 0)
		goto error;

	success = xmlTextWriterEndElement (project); /* braseroproject */
	if (success < 0)
		goto error;

	xmlTextWriterEndDocument (project);
	xmlFreeTextWriter (project);
	g_free (path);
	return TRUE;

error:

	xmlTextWriterEndDocument (project);
	xmlFreeTextWriter (project);

	g_remove (path);
	g_free (path);

    	if (use_dialog)
		brasero_project_not_saved_dialog (proj);

	return FALSE;
}

static gboolean
brasero_project_save_audio_project_plain_text (BraseroProject *proj,
					       const gchar *uri,
					       BraseroDiscTrack *track,
					       gboolean use_dialog)
{
	const gchar *title;
	guint written;
	GSList *iter;
	gchar *path;
	FILE *file;

    	path = g_filename_from_uri (uri, NULL, NULL);
    	if (!path)
		return FALSE;

	file = fopen (path, "w+");
	g_free (path);
	if (!file) {
		if (use_dialog)
			brasero_project_not_saved_dialog (proj);

		return FALSE;
	}

	/* write title */
	title = gtk_entry_get_text (GTK_ENTRY (proj->priv->name_display));
	written = fwrite (title, strlen (title), 1, file);
	if (written != 1)
		goto error;

	written = fwrite ("\n", 1, 1, file);
	if (written != 1)
		goto error;

	for (iter = track->contents.tracks; iter; iter = iter->next) {
		BraseroDiscSong *song;
		BraseroStreamInfo *info;
		gchar *time;

		song = iter->data;
		info = song->info;

		written = fwrite (info->title, 1, strlen (info->title), file);
		if (written != strlen (info->title))
			goto error;

		time = brasero_units_get_time_string (song->end - song->start, TRUE, FALSE);
		if (time) {
			written = fwrite ("\t", 1, 1, file);
			if (written != 1)
				goto error;

			written = fwrite (time, 1, strlen (time), file);
			if (written != strlen (time)) {
				g_free (time);
				goto error;
			}
			g_free (time);
		}

		if (info->artist) {
			gchar *string;

			written = fwrite ("\t", 1, 1, file);
			if (written != 1)
				goto error;

			/* Translators: %s is an artist */
			string = g_strdup_printf (" by %s", info->artist);
			written = fwrite (string, 1, strlen (string), file);
			if (written != strlen (string)) {
				g_free (string);
				goto error;
			}
			g_free (string);
		}

		written = fwrite ("\n(", 1, 2, file);
		if (written != 2)
			goto error;

		written = fwrite (song->uri, 1, strlen (song->uri), file);
		if (written != strlen (song->uri))
			goto error;

		written = fwrite (")", 1, 1, file);
		if (written != 1)
			goto error;

		written = fwrite ("\n\n", 1, 2, file);
		if (written != 2)
			goto error;
	}

	fclose (file);
	return TRUE;
	
error:
	fclose (file);

    	if (use_dialog)
		brasero_project_not_saved_dialog (proj);

	return FALSE;
}

#ifdef BUILD_PLAYLIST

static void
brasero_project_save_audio_playlist_entry (GtkTreeModel *model,
					   GtkTreeIter *iter,
					   gchar **uri,
					   gchar **title,
					   gboolean *custom_title,
					   gpointer user_data)
{
	gtk_tree_model_get (model, iter,
			    0, uri,
			    1, title,
			    2, custom_title,
			    -1);
}

static gboolean
brasero_project_save_audio_project_playlist (BraseroProject *proj,
					     const gchar *uri,
					     BraseroProjectSave type,
					     BraseroDiscTrack *track,
					     gboolean use_dialog)
{
	TotemPlParserType pl_type;
	TotemPlParser *parser;
	GtkListStore *model;
	GtkTreeIter t_iter;
	gboolean result;
	GSList *iter;
	gchar *path;

    	path = g_filename_from_uri (uri, NULL, NULL);
    	if (!path)
		return FALSE;

	parser = totem_pl_parser_new ();

	/* create and populate treemodel */
	model = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
	for (iter = track->contents.tracks; iter; iter = iter->next) {
		BraseroDiscSong *song;
		BraseroStreamInfo *info;

		song = iter->data;
		info = song->info;

		gtk_list_store_append (model, &t_iter);
		gtk_list_store_set (model, &t_iter,
				    0, song->uri,
				    1, info->title,
				    2, TRUE,
				    -1);
	}

	switch (type) {
		case BRASERO_PROJECT_SAVE_PLAYLIST_M3U:
			pl_type = TOTEM_PL_PARSER_M3U;
			break;
		case BRASERO_PROJECT_SAVE_PLAYLIST_XSPF:
			pl_type = TOTEM_PL_PARSER_XSPF;
			break;
		case BRASERO_PROJECT_SAVE_PLAYLIST_IRIVER_PLA:
			pl_type = TOTEM_PL_PARSER_IRIVER_PLA;
			break;

		case BRASERO_PROJECT_SAVE_PLAYLIST_PLS:
		default:
			pl_type = TOTEM_PL_PARSER_PLS;
			break;
	}

	result = totem_pl_parser_write_with_title (parser,
						   GTK_TREE_MODEL (model),
						   brasero_project_save_audio_playlist_entry,
						   path,
						   gtk_entry_get_text (GTK_ENTRY (proj->priv->name_display)),
						   pl_type,
						   NULL,
						   NULL);
	if (!result && use_dialog)
		brasero_project_not_saved_dialog (proj);

	if (result)
		brasero_project_add_to_recents (proj, uri, FALSE);

	g_object_unref (model);
	g_object_unref (parser);
	g_free (path);

	return result;
}

#endif

static gboolean
brasero_project_save_project_real (BraseroProject *project,
				   const gchar *uri,
				   BraseroProjectSave save_type)
{
	BraseroDiscResult result;
	BraseroProjectType type;
	BraseroDiscTrack track;

	g_return_val_if_fail (uri != NULL || project->priv->project != NULL, FALSE);

	result = brasero_project_check_status (project, project->priv->current);
	if (result != BRASERO_DISC_OK)
		return FALSE;

	bzero (&track, sizeof (track));

	result = brasero_disc_get_track (project->priv->current, &track);
	if (result == BRASERO_DISC_ERROR_EMPTY_SELECTION) {
		if (BRASERO_IS_AUDIO_DISC (project->priv->current))
			brasero_project_no_song_dialog (project);
		else if (BRASERO_IS_DATA_DISC (project->priv->current))
			brasero_project_no_file_dialog (project);

		return FALSE;
	}
	else if (result != BRASERO_DISC_OK) {
		brasero_project_not_saved_dialog (project);
		return FALSE;
	}

	if (track.type == BRASERO_PROJECT_TYPE_AUDIO)
		type = BRASERO_PROJECT_TYPE_AUDIO;
	else if (track.type == BRASERO_PROJECT_TYPE_DATA)
		type = BRASERO_PROJECT_TYPE_DATA;
	else if (track.type == BRASERO_PROJECT_TYPE_VIDEO)
		type = BRASERO_PROJECT_TYPE_VIDEO;
	else {
		brasero_track_clear (&track);
		return BRASERO_PROJECT_TYPE_INVALID;
	}

	if (save_type == BRASERO_PROJECT_SAVE_XML
	||  track.type == BRASERO_PROJECT_TYPE_DATA) {
		brasero_project_set_uri (project, uri, type);
		if (!brasero_project_save_project_xml (project,
						       uri ? uri : project->priv->project,
						       &track,
						       TRUE))
			return FALSE;

		project->priv->modified = 0;
	}
	else if (save_type == BRASERO_PROJECT_SAVE_PLAIN) {
		if (!brasero_project_save_audio_project_plain_text (project,
								    uri,
								    &track,
								    TRUE))
			return FALSE;
	}

#ifdef BUILD_PLAYLIST

	else {
		if (!brasero_project_save_audio_project_playlist (project,
								  uri,
								  save_type,
								  &track,
								  TRUE))
			return FALSE;
	}

#endif

	brasero_track_clear (&track);
	return TRUE;
}

static gchar *
brasero_project_save_project_ask_for_path (BraseroProject *project,
					   BraseroProjectSave *type)
{
	GtkWidget *combo = NULL;
	GtkWidget *toplevel;
	GtkWidget *chooser;
	gchar *uri = NULL;
	gint answer;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (project));
	chooser = gtk_file_chooser_dialog_new (_("Save Current Project"),
					       GTK_WINDOW (toplevel),
					       GTK_FILE_CHOOSER_ACTION_SAVE,
					       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					       GTK_STOCK_SAVE, GTK_RESPONSE_OK,
					       NULL);

	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), TRUE);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser),
					     g_get_home_dir ());

	/* if the file chooser is an audio project offer the possibility to save
	 * in plain text a list of the current displayed songs (only in save as
	 * mode) */
	if (type && BRASERO_IS_AUDIO_DISC (project->priv->current)) {
		combo = gtk_combo_box_new_text ();
		gtk_widget_show (combo);

		gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Save project as Brasero audio project"));
		gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Save project as a plain text list"));

#ifdef BUILD_PLAYLIST

		gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Save project as a PLS playlist"));
		gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Save project as an M3U playlist"));
		gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Save project as a XSPF playlist"));
		gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _("Save project as an IRIVER playlist"));

#endif

		gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
		gtk_file_chooser_set_extra_widget (GTK_FILE_CHOOSER (chooser), combo);
	}

	gtk_widget_show (chooser);
	answer = gtk_dialog_run (GTK_DIALOG (chooser));
	if (answer == GTK_RESPONSE_OK) {
		if (combo)
			*type = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));

		uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (chooser));
		if (*uri == '\0') {
			g_free (uri);
			uri = NULL;
		}
	}

	gtk_widget_destroy (chooser);
	return uri;
}

gboolean
brasero_project_save_project (BraseroProject *project)
{
	gchar *uri = NULL;
	gboolean result;

	if (!project->priv->project && !(uri = brasero_project_save_project_ask_for_path (project, NULL)))
		return FALSE;

	result = brasero_project_save_project_real (project, uri, BRASERO_PROJECT_SAVE_XML);
	g_free (uri);

	return result;
}

gboolean
brasero_project_save_project_as (BraseroProject *project)
{
	BraseroProjectSave type = BRASERO_PROJECT_SAVE_XML;
	gboolean result;
	gchar *uri;

	uri = brasero_project_save_project_ask_for_path (project, &type);
	if (!uri)
		return FALSE;

	result = brasero_project_save_project_real (project, uri, type);
	g_free (uri);

	return result;
}

/**
 * NOTE: this function returns FALSE if it succeeds and TRUE otherwise.
 * this value is mainly used by the session object to cancel or not the app
 * closing
 */

gboolean
brasero_project_save_session (BraseroProject *project,
			      const gchar *uri,
			      gchar **saved_uri,
			      gboolean show_cancel)
{
    	BraseroDiscTrack track;

	if (!project->priv->current) {
		if (saved_uri)
			*saved_uri = NULL;

		return FALSE;
	}

	if (project->priv->project) {
		GtkResponseType answer;

		if (!project->priv->modified) {
			/* there is a saved project but unmodified.
			 * No need to ask anything */
			if (saved_uri)
				*saved_uri = g_strdup (project->priv->project);

			return FALSE;
		}

		/* ask the user if he wants to save the changes */
		answer = brasero_project_save_project_dialog (project, show_cancel);
		if (answer == GTK_RESPONSE_CANCEL)
			return TRUE;

		if (answer != GTK_RESPONSE_YES) {
			if (saved_uri)
				*saved_uri = NULL;

			return FALSE;
		}

		if (!brasero_project_save_project_real (project, NULL, BRASERO_PROJECT_SAVE_XML))
			return TRUE;

		if (saved_uri)
			*saved_uri = g_strdup (project->priv->project);

		return FALSE;
	}

	if (project->priv->empty) {
		/* the project is empty anyway. No need to ask anything.
		 * return FALSE since this is not a tmp project */
		if (saved_uri)
			*saved_uri = NULL;

		return FALSE;
	}

    	if (project->priv->burnt) {
		GtkResponseType answer;

		/* the project wasn't saved but burnt ask if the user wants to
		 * keep it for another time by saving it */
		answer = brasero_project_save_project_dialog (project, show_cancel);
		if (answer == GTK_RESPONSE_CANCEL)
			return TRUE;

		if (answer != GTK_RESPONSE_YES) {
			if (saved_uri)
				*saved_uri = NULL;

			return FALSE;
		}

		if (!brasero_project_save_project_as (project))
			return TRUE;

		if (saved_uri)
			*saved_uri = g_strdup (project->priv->project);

		return FALSE;
	}

    	if (!uri) {
		if (saved_uri)
			*saved_uri = NULL;

		return FALSE;
	}

    	bzero (&track, sizeof (track));
	if (brasero_disc_get_track (project->priv->current, &track) == BRASERO_DISC_OK) {
		/* NOTE: is this right? because brasero could not shut itself
		 * down if an error occurs. */
		if (!brasero_project_save_project_xml (project,
						       uri,
						       &track,
						       FALSE))
			return TRUE;
	}

	brasero_track_clear (&track);

	if (saved_uri)
		*saved_uri = g_strdup (uri);

    	return FALSE;
}
