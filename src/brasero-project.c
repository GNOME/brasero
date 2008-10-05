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

#include <string.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include <gio/gio.h>

#include <gtk/gtknotebook.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkfilechooser.h>
#include <gtk/gtkfilechooserdialog.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkfilechooser.h>
#include <gtk/gtkfilechooserdialog.h>

#include <libxml/xmlerror.h>
#include <libxml/xmlwriter.h>
#include <libxml/parser.h>
#include <libxml/xmlstring.h>
#include <libxml/uri.h>

#include <gconf/gconf-client.h>

#ifdef BUILD_PLAYLIST
#include <totem-pl-parser.h>
#endif

#include "burn-debug.h"
#include "burn-session.h"

#ifdef BUILD_PREVIEW
#include "brasero-player.h"
#endif

#include "brasero-app.h"
#include "brasero-project.h"
#include "brasero-session-cfg.h"
#include "brasero-jacket-edit.h"
#include "brasero-project-type-chooser.h"
#include "brasero-disc.h"
#include "brasero-data-disc.h"
#include "brasero-audio-disc.h"
#include "brasero-video-disc.h"
#include "brasero-disc-option-dialog.h"
#include "brasero-burn-dialog.h"
#include "brasero-utils.h"
#include "brasero-uri-container.h"
#include "brasero-layout-object.h"
#include "brasero-disc-message.h"
#include "brasero-file-chooser.h"
#include "brasero-notify.h"
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
	GtkWidget *name_display;
	GtkWidget *discs;
	GtkWidget *audio;
	GtkWidget *data;
	GtkWidget *video;

	GtkWidget *message;

	GtkUIManager *manager;

	guint status_ctx;

	/* header */
	GtkWidget *burn;

	GtkActionGroup *project_group;
	guint merge_id;

	gchar *project;

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
brasero_project_init (BraseroProject *obj)
{
	GtkSizeGroup *size_group;
	GtkWidget *alignment;
	GtkWidget *label;
	GtkWidget *box;

	obj->priv = g_new0 (BraseroProjectPrivate, 1);

	g_signal_connect (G_OBJECT (obj),
			  "set-focus-child",
			  G_CALLBACK (brasero_project_focus_changed_cb),
			  NULL);

	obj->priv->message = brasero_notify_new ();
	gtk_box_pack_start (GTK_BOX (obj), obj->priv->message, FALSE, TRUE, 0);
	gtk_widget_show (obj->priv->message);

	/* bottom */
	box = gtk_hbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (box), 4);
	gtk_widget_show (box);
	gtk_box_pack_end (GTK_BOX (obj), box, FALSE, TRUE, 0);

	/* Name widget */
	label = gtk_label_new_with_mnemonic (_("_Name:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_misc_set_padding (GTK_MISC (label), 6, 0);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (box), label, FALSE, TRUE, 0);

	obj->priv->name_display = brasero_project_name_new ();
	gtk_widget_show (obj->priv->name_display);
	gtk_box_pack_start (GTK_BOX (box), obj->priv->name_display, TRUE, TRUE, 0);
	obj->priv->empty = 1;

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), obj->priv->name_display);

	/* burn button set insensitive since there are no files in the selection */
	size_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);
	obj->priv->burn = brasero_utils_make_button (_("_Burn..."),
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
	gtk_box_pack_end (GTK_BOX (box), alignment, FALSE, TRUE, 0);

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

	g_free(cobj->priv);
	G_OBJECT_CLASS(parent_class)->finalize(object);
}

GtkWidget *
brasero_project_new ()
{
	BraseroProject *obj;
	
	obj = BRASERO_PROJECT(g_object_new(BRASERO_TYPE_PROJECT, NULL));
	gtk_notebook_set_current_page (GTK_NOTEBOOK (obj->priv->discs), 0);

	return GTK_WIDGET (obj);
}

/********************************** size ***************************************/
static void
brasero_project_update_project_size (BraseroProject *project,
				     guint64 sectors)
{
	GtkWidget *toplevel;
	GtkWidget *status;
	gchar *string;
	gchar *size;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (project));
	status = brasero_app_get_statusbar2 (BRASERO_APP (toplevel));

	if (!project->priv->status_ctx)
		project->priv->status_ctx = gtk_statusbar_get_context_id (GTK_STATUSBAR (status),
									  "size_project");


	gtk_statusbar_pop (GTK_STATUSBAR (status), project->priv->status_ctx);

	string = brasero_utils_get_sectors_string (sectors,
						   !BRASERO_IS_DATA_DISC (project->priv->current),
						   TRUE,
						   FALSE);
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
{ }

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
	GtkProgressBar *progress;
	BraseroProject *project;

	project = g_object_get_data (G_OBJECT (dialog), "Project");
	if (project->priv->oversized) {
		gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
		return FALSE;
	}

	progress = g_object_get_data (G_OBJECT (dialog), "ProgressBar");

	if (brasero_disc_get_status (project->priv->current) == BRASERO_DISC_NOT_READY) {
		gtk_progress_bar_pulse (progress);
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
	GtkWidget *dialog;
	GtkWidget *progress;
	GtkWidget *toplevel;
	BraseroDiscResult result;

	result = brasero_disc_get_status (disc);
	if (result != BRASERO_DISC_NOT_READY)
		return result;

	/* we are not ready to create tracks presumably because
	 * data or audio has not finished to explore a directory
	 * or get the metadata of a song or a film */
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (project));
	dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					 GTK_DIALOG_DESTROY_WITH_PARENT |
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_INFO,
					 GTK_BUTTONS_CLOSE,
					 _("Please wait:"));

	gtk_window_set_title (GTK_WINDOW (dialog), _("Please Wait"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  _("some tasks are not completed yet."));

	gtk_window_set_title (GTK_WINDOW (dialog), _("Ongoing Tasks"));

	progress = gtk_progress_bar_new ();
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (progress), " ");
	gtk_box_pack_end (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			  progress,
			  TRUE,
			  TRUE,
			  10);

	gtk_widget_show_all (dialog);
	gtk_progress_bar_pulse (GTK_PROGRESS_BAR (progress));

	g_object_set_data (G_OBJECT (dialog), "ProgressBar", progress);
	g_object_set_data (G_OBJECT (dialog), "Project", project);

	id = g_timeout_add (100,
			    (GSourceFunc) _wait_for_ready_state,
		            dialog);

	answer = gtk_dialog_run (GTK_DIALOG (dialog));
	g_source_remove (id);

	gtk_widget_destroy (dialog);

	if (answer != GTK_RESPONSE_OK)
		return BRASERO_DISC_CANCELLED;
	else if (project->priv->oversized)
		return BRASERO_DISC_ERROR_SIZE;

	return brasero_disc_get_status (disc);
}

/******************************** cover ****************************************/

void
brasero_project_set_cover_specifics (BraseroProject *self,
				     BraseroJacketEdit *cover)
{
	BraseroBurnSession *session;

	if (!BRASERO_IS_AUDIO_DISC (self->priv->current))
		return;

	session = BRASERO_BURN_SESSION (brasero_session_cfg_new ());
	brasero_disc_set_session_param (BRASERO_DISC (self->priv->current), session);
	brasero_disc_set_session_contents (BRASERO_DISC (self->priv->current), session);
	brasero_jacket_edit_set_audio_tracks (BRASERO_JACKET_EDIT (cover),
					      brasero_burn_session_get_label (session),
					      brasero_burn_session_get_tracks (session));
	g_object_unref (session);
}

/******************************** burning **************************************/
static void
brasero_project_no_song_dialog (BraseroProject *project)
{
	GtkWidget *message;
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (project));
	message = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					  GTK_DIALOG_DESTROY_WITH_PARENT|
					  GTK_DIALOG_MODAL,
					  GTK_MESSAGE_WARNING,
					  GTK_BUTTONS_CLOSE,
					  _("Please add songs to the project."));

	gtk_window_set_title (GTK_WINDOW (message), _("Empty Project"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
						  _("The project is empty."));

	gtk_dialog_run (GTK_DIALOG (message));
	gtk_widget_destroy (message);
}

static void
brasero_project_no_file_dialog (BraseroProject *project)
{
	GtkWidget *message;
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (project));
	message = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					  GTK_DIALOG_DESTROY_WITH_PARENT|
					  GTK_DIALOG_MODAL,
					  GTK_MESSAGE_WARNING,
					  GTK_BUTTONS_CLOSE,
					  _("Please add files to the project."));

	gtk_window_set_title (GTK_WINDOW (message), _("Empty Project"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
						  _("The project is empty."));

	gtk_dialog_run (GTK_DIALOG (message));
	gtk_widget_destroy (message);
}

void
brasero_project_burn (BraseroProject *project)
{
	BraseroBurnSession *session;
	BraseroDiscResult result;
	GtkWidget *toplevel;
	GtkWidget *dialog;
	gboolean destroy;
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
	destroy = FALSE;

	/* This is to stop the preview widget from playing */
	brasero_uri_container_uri_selected (BRASERO_URI_CONTAINER (project));

	/* setup, show, and run options dialog */
	dialog = brasero_disc_option_dialog_new ();
	brasero_disc_option_dialog_set_disc (BRASERO_DISC_OPTION_DIALOG (dialog),
					     project->priv->current);

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (project));
	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_widget_show (dialog);

	result = gtk_dialog_run (GTK_DIALOG (dialog));
	if (result != GTK_RESPONSE_OK)
		goto end;

	session = brasero_disc_option_dialog_get_session (BRASERO_DISC_OPTION_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	/* set the label for the session */
	brasero_burn_session_set_label (session, gtk_entry_get_text (GTK_ENTRY (project->priv->name_display)));

	/* now setup the burn dialog */
	dialog = brasero_burn_dialog_new ();
	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

	gtk_widget_hide (toplevel);
	gtk_widget_show (dialog);

	success = brasero_burn_dialog_run (BRASERO_BURN_DIALOG (dialog),
					   session,
					   &destroy);
	g_object_unref (session);

    	project->priv->burnt = success;

end:

	project->priv->is_burning = 0;
	gtk_widget_destroy (dialog);

	if (destroy)
		gtk_widget_destroy (toplevel);
	else
		gtk_widget_show (toplevel);
}

/********************************     ******************************************/
static void
brasero_project_switch (BraseroProject *project, BraseroProjectType type)
{
	GtkAction *action;
	GConfClient *client;

	if (project->priv->chooser) {
		gtk_widget_destroy (project->priv->chooser);
		project->priv->chooser = NULL;
	}

	project->priv->empty = 1;
    	project->priv->burnt = 0;
	project->priv->modified = 0;

	if (project->priv->current)
		brasero_disc_reset (project->priv->current);

	if (project->priv->project) {
		g_free (project->priv->project);
		project->priv->project = NULL;
	}

	client = gconf_client_get_default ();

	/* remove the buttons from the "toolbar" */
	if (project->priv->merge_id)
		gtk_ui_manager_remove_ui (project->priv->manager,
					  project->priv->merge_id);

	if (type == BRASERO_PROJECT_TYPE_AUDIO) {
		project->priv->current = BRASERO_DISC (project->priv->audio);
		project->priv->merge_id = brasero_disc_add_ui (project->priv->current,
							       project->priv->manager,
							       project->priv->message);

		gtk_notebook_set_current_page (GTK_NOTEBOOK (project->priv->discs), 0);
		brasero_project_update_project_size (project, 0);
	}
	else if (type == BRASERO_PROJECT_TYPE_DATA) {
		project->priv->current = BRASERO_DISC (project->priv->data);
		project->priv->merge_id = brasero_disc_add_ui (project->priv->current,
							       project->priv->manager,
							       project->priv->message);

		gtk_notebook_set_current_page (GTK_NOTEBOOK (project->priv->discs), 1);
		brasero_project_update_project_size (project, 0);
	}
	else if (type == BRASERO_PROJECT_TYPE_VIDEO) {
		project->priv->current = BRASERO_DISC (project->priv->video);
		project->priv->merge_id = brasero_disc_add_ui (project->priv->current,
							       project->priv->manager,
							       project->priv->message);

		gtk_notebook_set_current_page (GTK_NOTEBOOK (project->priv->discs), 2);
		brasero_project_update_project_size (project, 0);
	}

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
brasero_project_set_data (BraseroProject *project, GSList *uris)
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
	GtkWidget *toplevel;
	GtkResponseType answer;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (project));

	if (project->priv->project) {
		if (!project->priv->modified)
			return TRUE;

		dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
						 GTK_DIALOG_DESTROY_WITH_PARENT |
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_WARNING,
						 GTK_BUTTONS_CANCEL,
						 _("Do you really want to create a new project and discard the changes to current one?"));

		
		gtk_window_set_title (GTK_WINDOW (dialog), _("Unsaved Project"));

		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  _("If you choose to create a new project, all changes made will be lost."));
		gtk_dialog_add_button (GTK_DIALOG (dialog),
				       _("_Discard Changes"), GTK_RESPONSE_OK);

	}
	else {
		if (project->priv->empty)
			return TRUE;

		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (project));
		dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
						 GTK_DIALOG_DESTROY_WITH_PARENT |
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_WARNING,
						 GTK_BUTTONS_CANCEL,
						 _("Do you really want to create a new project and discard the current one?"));

		
		gtk_window_set_title (GTK_WINDOW (dialog), _("New Project"));

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
	GtkWidget *toplevel;

	if (project->priv->project) {
		g_free (project->priv->project);
		project->priv->project = NULL;
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

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (project));
	status = brasero_app_get_statusbar2 (BRASERO_APP (toplevel));

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

	if (brasero_disc_get_status (disc) != BRASERO_DISC_LOADING)
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
	gtk_file_filter_set_name (filter, _("Audio files only"));
	gtk_file_filter_add_mime_type (filter, "audio/*");
	gtk_file_filter_add_mime_type (filter, "application/ogg");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (project->priv->chooser), filter);

	if (BRASERO_IS_AUDIO_DISC (project->priv->current))
		gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (project->priv->chooser), filter);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("Movies only"));
	gtk_file_filter_add_mime_type (filter, "video/*");
	gtk_file_filter_add_mime_type (filter, "application/ogg");
	gtk_file_filter_add_mime_type (filter, "application/x-flash-video");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (project->priv->chooser), filter);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("Image files only"));
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
		GtkWidget *toplevel;
		GtkResponseType answer;

		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (project));
		dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
						 GTK_DIALOG_DESTROY_WITH_PARENT |
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_WARNING,
						 GTK_BUTTONS_CANCEL,
						 _("Do you really want to empty the current project?"));

		
		gtk_window_set_title (GTK_WINDOW (dialog), _("Empty Project"));

		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  _("Emptying a project will remove all files already added. "
							    "All the work will be lost. "
							    "Note that files will not be deleted from their own location, "
							    "just no longer listed here."));
		gtk_dialog_add_button (GTK_DIALOG (dialog),
				       _("_Empty Project"), GTK_RESPONSE_OK);

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
		      "short-label", _("Save"), /* for toolbar buttons */
		      NULL);
	gtk_action_set_sensitive (action, FALSE);
	action = gtk_action_group_get_action (project->priv->project_group, "SaveAs");
	gtk_action_set_sensitive (action, FALSE);

	action = gtk_action_group_get_action (project->priv->project_group, "Burn");
	gtk_action_set_sensitive (action, FALSE);
	action = gtk_action_group_get_action (project->priv->project_group, "Add");
	gtk_action_set_sensitive (action, FALSE);
	g_object_set (action,
		      "short-label", _("Add"), /* for toolbar buttons */
		      NULL);
	action = gtk_action_group_get_action (project->priv->project_group, "DeleteProject");
	gtk_action_set_sensitive (action, FALSE);
	g_object_set (action,
		      "short-label", _("Remove"), /* for toolbar buttons */
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
    	brasero_project_add_to_recents (project, uri, TRUE);

	/* update the name of the main window */
    	BRASERO_GET_BASENAME_FOR_DISPLAY (uri, name);
	if (type == BRASERO_PROJECT_TYPE_DATA)
		title = g_strdup_printf (_("Brasero - %s (Data Disc)"), name);
	else if (type == BRASERO_PROJECT_TYPE_AUDIO)
		title = g_strdup_printf (_("Brasero - %s (Audio Disc)"), name);
	else if (type == BRASERO_PROJECT_TYPE_AUDIO)
		title = g_strdup_printf (_("Brasero - %s (Video Disc)"), name);
	else
		title = NULL;
 
	g_free (name);

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (project));
	gtk_window_set_title (GTK_WINDOW (toplevel), title);
	g_free (title);

	/* update the menus */
	action = gtk_action_group_get_action (project->priv->project_group, "Save");
	gtk_action_set_sensitive (action, FALSE);
}

/******************************* Projects **************************************/
static void
brasero_project_invalid_project_dialog (BraseroProject *project,
					const char *reason)
{
	GtkWidget *dialog;
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (project));
	dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					 GTK_DIALOG_DESTROY_WITH_PARENT |
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_CLOSE,
					 _("Error while loading the project:"));

	gtk_window_set_title (GTK_WINDOW (dialog), _("Project Loading Error"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  reason);

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static gboolean
_read_graft_point (xmlDocPtr project,
		   xmlNodePtr graft,
		   BraseroDiscTrack *track)
{
	BraseroGraftPt *retval;

	retval = g_new0 (BraseroGraftPt, 1);
	while (graft) {
		if (!xmlStrcmp (graft->name, (const xmlChar *) "uri")) {
			xmlChar *uri;

			if (retval->uri)
				goto error;

			uri = xmlNodeListGetString (project,
						    graft->xmlChildrenNode,
						    1);
			retval->uri = g_uri_unescape_string ((char *)uri, NULL);
			g_free (uri);
			if (!retval->uri)
				goto error;
		}
		else if (!xmlStrcmp (graft->name, (const xmlChar *) "path")) {
			if (retval->path)
				goto error;

			retval->path = (char *) xmlNodeListGetString (project,
								      graft->xmlChildrenNode,
								      1);
			if (!retval->path)
				goto error;
		}
		else if (!xmlStrcmp (graft->name, (const xmlChar *) "excluded")) {
			xmlChar *excluded;

			excluded = xmlNodeListGetString (project,
							 graft->xmlChildrenNode,
							 1);
			if (!excluded)
				goto error;

			track->contents.data.excluded = g_slist_prepend (track->contents.data.excluded,
									 xmlURIUnescapeString ((char*) excluded, 0, NULL));
			g_free (excluded);
		}
		else if (graft->type == XML_ELEMENT_NODE)
			goto error;

		graft = graft->next;
	}

	track->contents.data.grafts = g_slist_prepend (track->contents.data.grafts, retval);
	return TRUE;

error:
	brasero_graft_point_free (retval);
	return FALSE;
}

static BraseroDiscTrack *
_read_data_track (xmlDocPtr project,
		  xmlNodePtr item)
{
	BraseroDiscTrack *track;

	track = g_new0 (BraseroDiscTrack, 1);
	track->type = BRASERO_DISC_TRACK_DATA;

	while (item) {
		if (!xmlStrcmp (item->name, (const xmlChar *) "graft")) {
			if (!_read_graft_point (project, item->xmlChildrenNode, track))
				goto error;
		}
		else if (!xmlStrcmp (item->name, (const xmlChar *) "restored")) {
			xmlChar *restored;

			restored = xmlNodeListGetString (project,
							 item->xmlChildrenNode,
							 1);
			if (!restored)
				goto error;

			track->contents.data.restored = g_slist_prepend (track->contents.data.restored, restored);
		}
		else if (!xmlStrcmp (item->name, (const xmlChar *) "excluded")) {
			xmlChar *excluded;

			excluded = xmlNodeListGetString (project,
							 item->xmlChildrenNode,
							 1);
			if (!excluded)
				goto error;

			track->contents.data.excluded = g_slist_prepend (track->contents.data.excluded,
									 xmlURIUnescapeString ((char*) excluded, 0, NULL));
			g_free (excluded);
		}
		else if (item->type == XML_ELEMENT_NODE)
			goto error;

		item = item->next;
	}

	track->contents.data.excluded = g_slist_reverse (track->contents.data.excluded);
	track->contents.data.grafts = g_slist_reverse (track->contents.data.grafts);
	return track;

error:
	brasero_track_free (track);
	return NULL;
}

static BraseroDiscTrack *
_read_audio_track (xmlDocPtr project,
		   xmlNodePtr uris)
{
	BraseroDiscTrack *track;
	BraseroDiscSong *song;

	track = g_new0 (BraseroDiscTrack, 1);
	song = NULL;

	while (uris) {
		if (!xmlStrcmp (uris->name, (const xmlChar *) "uri")) {
			xmlChar *uri;

			uri = xmlNodeListGetString (project,
						    uris->xmlChildrenNode,
						    1);
			if (!uri)
				goto error;

			song = g_new0 (BraseroDiscSong, 1);
			song->uri = g_uri_unescape_string ((char *) uri, NULL);

			/* to know if this info was set or not */
			song->start = -1;
			song->end = -1;
			g_free (uri);
			track->contents.tracks = g_slist_prepend (track->contents.tracks, song);
		}
		else if (!xmlStrcmp (uris->name, (const xmlChar *) "silence")) {
			gchar *silence;

			if (!song)
				goto error;

			/* impossible to have two gaps in a row */
			if (song->gap)
				goto error;

			silence = (gchar *) xmlNodeListGetString (project,
								  uris->xmlChildrenNode,
								  1);
			if (!silence)
				goto error;

			song->gap = (gint64) g_ascii_strtoull (silence, NULL, 10);
			g_free (silence);
		}
		else if (!xmlStrcmp (uris->name, (const xmlChar *) "start")) {
			gchar *start;

			if (!song)
				goto error;

			start = (gchar *) xmlNodeListGetString (project,
								uris->xmlChildrenNode,
								1);
			if (!start)
				goto error;

			song->start = (gint64) g_ascii_strtoull (start, NULL, 10);
			g_free (start);
		}
		else if (!xmlStrcmp (uris->name, (const xmlChar *) "end")) {
			gchar *end;

			if (!song)
				goto error;

			end = (gchar *) xmlNodeListGetString (project,
							      uris->xmlChildrenNode,
							      1);
			if (!end)
				goto error;

			song->end = (gint64) g_ascii_strtoull (end, NULL, 10);
			g_free (end);
		}
		else if (uris->type == XML_ELEMENT_NODE)
			goto error;

		uris = uris->next;
	}

	track->contents.tracks = g_slist_reverse (track->contents.tracks);
	return (BraseroDiscTrack*) track;

error:
	brasero_track_free ((BraseroDiscTrack *) track);
	return NULL;
}

static gboolean
_get_tracks (xmlDocPtr project,
	     xmlNodePtr track_node,
	     BraseroDiscTrack **track)
{
	BraseroDiscTrack *newtrack;

	track_node = track_node->xmlChildrenNode;

	newtrack = NULL;
	while (track_node) {
		if (!xmlStrcmp (track_node->name, (const xmlChar *) "audio")) {
			if (newtrack)
				goto error;

			newtrack = _read_audio_track (project,
						      track_node->xmlChildrenNode);
			if (!newtrack)
				goto error;

			newtrack->type = BRASERO_DISC_TRACK_AUDIO;
		}
		else if (!xmlStrcmp (track_node->name, (const xmlChar *) "data")) {
			if (newtrack)
				goto error;

			newtrack = _read_data_track (project,
						     track_node->xmlChildrenNode);

			if (!newtrack)
				goto error;
		}
		else if (!xmlStrcmp (track_node->name, (const xmlChar *) "video")) {
			if (newtrack)
				goto error;

			newtrack = _read_audio_track (project,
						      track_node->xmlChildrenNode);

			if (!newtrack)
				goto error;

			newtrack->type = BRASERO_DISC_TRACK_VIDEO;
		}
		else if (track_node->type == XML_ELEMENT_NODE)
			goto error;

		track_node = track_node->next;
	}

	if (!newtrack)
		goto error;

	*track = newtrack;
	return TRUE;

error :
	if (newtrack)
		brasero_track_free (newtrack);

	brasero_track_free (newtrack);
	return FALSE;
}

static gboolean
brasero_project_open_project_xml (BraseroProject *proj,
				  const gchar *uri,
				  BraseroDiscTrack **track,
				  gboolean warn_user)
{
	xmlNodePtr track_node = NULL;
	xmlDocPtr project;
	xmlNodePtr item;
	gboolean retval;
    	gchar *path;

    	path = g_filename_from_uri (uri, NULL, NULL);
    	if (!path)
		return FALSE;

	/* start parsing xml doc */
	project = xmlParseFile (path);
    	g_free (path);

	if (!project) {
	    	if (warn_user)
			brasero_project_invalid_project_dialog (proj, _("the project could not be opened."));

		return FALSE;
	}

	/* parses the "header" */
	item = xmlDocGetRootElement (project);
	if (!item) {
	    	if (warn_user)
			brasero_project_invalid_project_dialog (proj, _("the file is empty."));

		xmlFreeDoc (project);
		return FALSE;
	}

	if (xmlStrcmp (item->name, (const xmlChar *) "braseroproject")
	||  item->next)
		goto error;

	item = item->children;
	while (item) {
		if (!xmlStrcmp (item->name, (const xmlChar *) "version")) {
			/* simply ignore it */
		}
		else if (!xmlStrcmp (item->name, (const xmlChar *) "track")) {
			if (track_node)
				goto error;

			track_node = item;
		}
		else if (item->type == XML_ELEMENT_NODE)
			goto error;

		item = item->next;
	}

	retval = _get_tracks (project, track_node, track);
	xmlFreeDoc (project);

	if (!retval && warn_user)
		brasero_project_invalid_project_dialog (proj, _("it doesn't seem to be a valid Brasero project."));

	return retval;

error:

	xmlFreeDoc (project);
    	if (warn_user)
		brasero_project_invalid_project_dialog (proj, _("it doesn't seem to be a valid Brasero project."));

	return FALSE;
}

BraseroProjectType
brasero_project_open_project (BraseroProject *project,
			      const gchar *uri)	/* escaped */
{
	BraseroDiscTrack *track = NULL;
	BraseroProjectType type;

	if (!uri || *uri =='\0')
		return BRASERO_PROJECT_TYPE_INVALID;

	if (!brasero_project_open_project_xml (project, uri, &track, TRUE))
		return BRASERO_PROJECT_TYPE_INVALID;

	brasero_project_update_project_size (project, 0);

	if (track->type == BRASERO_DISC_TRACK_AUDIO)
		type = BRASERO_PROJECT_TYPE_AUDIO;
	else if (track->type == BRASERO_DISC_TRACK_DATA)
		type = BRASERO_PROJECT_TYPE_DATA;
	else if (track->type == BRASERO_DISC_TRACK_VIDEO)
		type = BRASERO_PROJECT_TYPE_VIDEO;
	else {
		brasero_track_free (track);
		return BRASERO_PROJECT_TYPE_INVALID;
	}

	brasero_project_switch (project, type);

	brasero_disc_load_track (project->priv->current, track);
	brasero_track_free (track);

	project->priv->modified = 0;

	brasero_project_set_uri (project, uri, type);
	return type;
}

#ifdef BUILD_PLAYLIST

static void
brasero_project_playlist_entry_parsed (TotemPlParser *parser,
				       const gchar *uri,
				       GHashTable *metadata,
				       gpointer user_data)
{
	BraseroDiscTrack *track = user_data;
	BraseroDiscSong *song;

	song = g_new0 (BraseroDiscSong, 1);
	song->uri = g_strdup (uri);

	/* to know if this info was set or not */
	song->start = -1;
	song->end = -1;
	track->contents.tracks = g_slist_prepend (track->contents.tracks, song);
}

static gboolean
brasero_project_open_audio_playlist_project (BraseroProject *proj,
					     const gchar *uri,
					     BraseroDiscTrack **track,
					     gboolean warn_user)
{
	TotemPlParser *parser;
	TotemPlParserResult result;
	BraseroDiscTrack *new_track;

	new_track = g_new0 (BraseroDiscTrack, 1);
	new_track->type = BRASERO_DISC_TRACK_AUDIO;

	parser = totem_pl_parser_new ();
	g_object_set (parser,
		      "recurse", FALSE,
		      "disable-unsafe", TRUE,
		      NULL);

	g_signal_connect (parser,
			  "entry-parsed",
			  G_CALLBACK (brasero_project_playlist_entry_parsed),
			  new_track);

	result = totem_pl_parser_parse (parser, uri, FALSE);
	if (result != TOTEM_PL_PARSER_RESULT_SUCCESS) {
		if (warn_user)
			brasero_project_invalid_project_dialog (proj, _("it doesn't seem to be a valid Brasero project."));

		brasero_track_free (new_track);
	}
	else
		*track = new_track;

	g_object_unref (parser);

	return (result == TOTEM_PL_PARSER_RESULT_SUCCESS);
}

BraseroProjectType
brasero_project_open_playlist (BraseroProject *project,
			       const gchar *uri) /* escaped */
{
	BraseroDiscTrack *track = NULL;
	BraseroProjectType type;

	if (!uri || *uri =='\0')
		return BRASERO_PROJECT_TYPE_INVALID;

	if (!brasero_project_open_audio_playlist_project (project, uri, &track, TRUE))
		return BRASERO_PROJECT_TYPE_INVALID;


	brasero_project_update_project_size (project, 0);
	brasero_project_switch (project, TRUE);
	type = BRASERO_PROJECT_TYPE_AUDIO;

	brasero_disc_load_track (project->priv->current, track);
	brasero_track_free (track);

	brasero_project_add_to_recents (project, uri, FALSE);
	project->priv->modified = 0;

	return type;
}

#endif

BraseroProjectType
brasero_project_load_session (BraseroProject *project, const gchar *uri)
{
	BraseroDiscTrack *track = NULL;
	BraseroProjectType type;

	if (!brasero_project_open_project_xml (project, uri, &track, FALSE))
		return BRASERO_PROJECT_TYPE_INVALID;

	if (track->type == BRASERO_DISC_TRACK_AUDIO)
		type = BRASERO_PROJECT_TYPE_AUDIO;
	else if (track->type == BRASERO_DISC_TRACK_DATA)
		type = BRASERO_PROJECT_TYPE_DATA;
	else if (track->type == BRASERO_DISC_TRACK_VIDEO)
		type = BRASERO_PROJECT_TYPE_VIDEO;
	else {
	    	brasero_track_free (track);
		return BRASERO_PROJECT_TYPE_INVALID;
	}

	brasero_project_switch (project, type);

	brasero_disc_load_track (project->priv->current, track);
	brasero_track_free (track);

	project->priv->modified = 0;

    	return type;
}

/******************************** save project *********************************/
static void
brasero_project_not_saved_dialog (BraseroProject *project)
{
	GtkWidget *dialog;
	xmlErrorPtr error;
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (project));
	dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					 GTK_DIALOG_DESTROY_WITH_PARENT|
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_WARNING,
					 GTK_BUTTONS_CLOSE,
					 _("Your project has not been saved:"));

	gtk_window_set_title (GTK_WINDOW (dialog), _("Unsaved Project"));

	error = xmlGetLastError ();
	if (error)
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  error->message);
	else
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  _("Unknown error."));	
	xmlResetLastError ();

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static GtkResponseType
brasero_project_save_project_dialog (BraseroProject *project,
				     gboolean show_cancel)
{
	GtkWidget *dialog;
	GtkWidget *toplevel;
	GtkResponseType result;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (project));
	dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					 GTK_DIALOG_DESTROY_WITH_PARENT|
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_WARNING,
					 GTK_BUTTONS_NONE,
					 _("Save the changes of current project before closing?"));

	gtk_window_set_title (GTK_WINDOW (dialog), _("Modified Project"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  _("If you don't save, changes will be permanently lost."));

	if (show_cancel)
		gtk_dialog_add_buttons (GTK_DIALOG (dialog),
					_("Cl_ose without saving"), GTK_RESPONSE_NO,
					GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					GTK_STOCK_SAVE, GTK_RESPONSE_YES,
					NULL);
	else
		gtk_dialog_add_buttons (GTK_DIALOG (dialog),
					_("Cl_ose without saving"), GTK_RESPONSE_NO,
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
		xmlChar *escaped;
		gchar *start;
		gchar *end;

		song = iter->data;
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

	success = xmlTextWriterStartElement (project, (xmlChar *) "track");
	if (success < 0)
		goto error;

	if (track->type == BRASERO_DISC_TRACK_AUDIO) {
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
	else if (track->type == BRASERO_DISC_TRACK_DATA) {
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
	else  if (track->type == BRASERO_DISC_TRACK_VIDEO) {
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

	for (iter = track->contents.tracks; iter; iter = iter->next) {
		BraseroDiscSong *song;
		BraseroSongInfo *info;
		guint written;
		gchar *time;

		song = iter->data;
		info = song->info;

		written = fwrite (info->title, 1, strlen (info->title), file);
		if (written != strlen (info->title))
			goto error;

		time = brasero_utils_get_time_string (song->end - song->start, TRUE, FALSE);
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
			string = g_strdup_printf (_(" by %s"), info->artist);
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
		BraseroSongInfo *info;

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

	result = totem_pl_parser_write (parser,
					GTK_TREE_MODEL (model),
					brasero_project_save_audio_playlist_entry,
					path,
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

	if (save_type == BRASERO_PROJECT_SAVE_XML) {
		brasero_project_set_uri (project, uri, track.type);
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
	BraseroProjectSave type;
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
			      gboolean show_cancel)
{
    	BraseroDiscTrack track;

	if (project->priv->project) {
		GtkResponseType answer;

		if (!project->priv->modified) {
			/* there is a saved project but unmodified.
			 * No need to ask anything */
			return FALSE;
		}

		/* ask the user if he wants to save the changes */
		answer = brasero_project_save_project_dialog (project, show_cancel);
		if (answer == GTK_RESPONSE_CANCEL)
			return TRUE;

		if (answer != GTK_RESPONSE_YES)
			return FALSE;

		brasero_project_save_project_real (project, NULL, BRASERO_PROJECT_SAVE_XML);

		/* return FALSE since this is not a tmp project */
		return FALSE;
	}

	if (project->priv->empty) {
		/* the project is empty anyway. No need to ask anything.
		 * return FALSE since this is not a tmp project */
		return FALSE;
	}

    	if (!project->priv->current)
		return FALSE;

    	if (project->priv->burnt) {
		GtkResponseType answer;

		/* the project wasn't saved but burnt ask if the user wants to
		 * keep it for another time by saving it */
		answer = brasero_project_save_project_dialog (project, show_cancel);
		if (answer == GTK_RESPONSE_CANCEL)
			return TRUE;

		if (answer != GTK_RESPONSE_YES)
			return FALSE;

		brasero_project_save_project_as (project);

		/* return FALSE since this is not a tmp project */
		return FALSE;
	}

    	if (!uri)
		return FALSE;

    	bzero (&track, sizeof (track));
	if (brasero_disc_get_track (project->priv->current, &track) == BRASERO_DISC_OK)
		brasero_project_save_project_xml (project,
						  uri,
						  &track,
						  FALSE);

	brasero_track_clear (&track);

	/* let the application close itself anyway. It wasn't asked by the user
	 * and it may get into some critical shutdown */
    	return FALSE;
}
