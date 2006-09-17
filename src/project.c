/***************************************************************************
 *            project.c
 *
 *  mar nov 29 09:32:17 2005
 *  Copyright  2005  Rouquier Philippe
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



#include <string.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>

#include <gtk/gtknotebook.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkfilechooser.h>
#include <gtk/gtkfilechooserdialog.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkimage.h>

#include <libxml/xmlerror.h>
#include <libxml/xmlwriter.h>
#include <libxml/parser.h>
#include <libxml/xmlstring.h>

#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-file-info.h>

#include <gconf/gconf-client.h>

#include "project.h"
#include "brasero-project-size.h"
#include "project-type-chooser.h"
#include "disc.h"
#include "data-disc.h"
#include "audio-disc.h"
#include "burn-options-dialog.h"
#include "burn-dialog.h"
#include "utils.h"
#include "brasero-uri-container.h"

static void brasero_project_class_init (BraseroProjectClass *klass);
static void brasero_project_init (BraseroProject *sp);
static void brasero_project_iface_uri_container_init (BraseroURIContainerIFace *iface);
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
brasero_project_disc_changed_cb (BraseroProjectSize *size,
				 BraseroProject *project);
static void
brasero_project_size_changed_cb (BraseroDisc *disc,
			         gint64 size,
			         BraseroProject *project);

static void
brasero_project_add_clicked_cb (GtkButton *button, BraseroProject *project);
static void
brasero_project_remove_clicked_cb (GtkButton *button, BraseroProject *project);
static void
brasero_project_burn_clicked_cb (GtkButton *button, BraseroProject *project);

static void
brasero_project_contents_changed_cb (BraseroDisc *disc,
				     int nb_files,
				     BraseroProject *project);
static void
brasero_project_selection_changed_cb (BraseroDisc *disc,
				      BraseroProject *project);
static char *
brasero_project_get_selected_uri (BraseroURIContainer *container);

struct BraseroProjectPrivate {
	GtkWidget *size_display;
	GtkWidget *discs;
	GtkWidget *audio;
	GtkWidget *data;

	/* header */
	GtkWidget *image;
	GtkWidget *label;
	GtkWidget *subtitle;
	GtkWidget *add;
	GtkWidget *remove;
	GtkWidget *burn;

	GtkTooltips *tooltip;
	GtkActionGroup *project_group;
	GtkActionGroup *action_group;

	gchar *project;
	gint64 sectors;
	BraseroDisc *current;
	BraseroURIContainer *current_source;

	gint is_burning:1;

	gint empty:1;
	gint oversized:1;
	gint ask_overburn:1;
};

static GtkActionEntry entries_project [] = {
	{"Save", GTK_STOCK_SAVE, N_("_Save"), NULL,
	 N_("Save current project"), G_CALLBACK (brasero_project_save_cb)},
	{"SaveAs", GTK_STOCK_SAVE_AS, N_("_Save as"), NULL,
	 N_("Save current project to a different location"), G_CALLBACK (brasero_project_save_as_cb)},
};

static const char *description_project = {
	"<ui>"
	    "<menubar name='menubar' >"
		"<menu action='ProjectMenu'>"
			"<placeholder name='ProjectPlaceholder'/>"
			    "<menuitem action='Save'/>"
			    "<menuitem action='SaveAs'/>"
			    "<separator/>"
		"</menu>"
	    "</menubar>"
	"</ui>"
};

static GtkActionEntry entries_actions [] = {
	{"Add", GTK_STOCK_ADD, N_("_Add files"), NULL,
	 N_("Add files to the project"), G_CALLBACK (brasero_project_add_uris_cb)},
	{"Delete", GTK_STOCK_REMOVE, N_("_Remove files"), NULL,
	 N_("Remove the selected files from the project"), G_CALLBACK (brasero_project_remove_selected_uris_cb)},
	{"DeleteAll", GTK_STOCK_DELETE, N_("E_mpty Project"), NULL,
	 N_("Delete all files from the project"), G_CALLBACK (brasero_project_empty_cb)},
	{"Burn", GTK_STOCK_CDROM, N_("_Burn"), NULL,
	 N_("Burn the disc"), G_CALLBACK (brasero_project_burn_cb)},
};

static const char *description_actions = {
	"<ui>"
	    "<menubar name='menubar' >"
		"<menu action='EditMenu'>"
			"<placeholder name='EditPlaceholder'/>"
			    "<menuitem action='Add'/>"
			    "<menuitem action='Delete'/>"
			    "<menuitem action='DeleteAll'/>"
			    "<separator/>"
		"</menu>"
		"<menu action='ViewMenu'>"
		"</menu>"
		"<menu action='DiscMenu'>"
			"<placeholder name='DiscPlaceholder'/>"
			"<menuitem action='Burn'/>"
		"</menu>"
		"</menubar>"
	"</ui>"
};

static GObjectClass *parent_class = NULL;

#define KEY_DEFAULT_DATA_BURNING_APP		"/desktop/gnome/volume_manager/autoburn_data_cd_command"
#define KEY_DEFAULT_AUDIO_BURNING_APP		"/desktop/gnome/volume_manager/autoburn_audio_cd_command"
#define KEY_ASK_DEFAULT_BURNING_APP		"/apps/brasero/ask_default_app"

enum {
	ADD_PRESSED_SIGNAL,
	LAST_SIGNAL
};
static guint brasero_project_signals[LAST_SIGNAL] = { 0 };

#define BRASERO_PROJECT_VERSION "0.2"

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

		type = g_type_register_static (GTK_TYPE_VBOX, 
					       "BraseroProject",
					       &our_info, 0);

		g_type_add_interface_static (type,
					     BRASERO_TYPE_URI_CONTAINER,
					     &uri_container_info);
	}

	return type;
}

static void
brasero_project_class_init (BraseroProjectClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_project_finalize;

	brasero_project_signals [ADD_PRESSED_SIGNAL] =
	    g_signal_new ("add_pressed",
			  BRASERO_TYPE_PROJECT,
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  G_STRUCT_OFFSET (BraseroProjectClass, add_pressed),
			  NULL,
			  NULL,
			  g_cclosure_marshal_VOID__VOID,
			  G_TYPE_NONE,
			  0);
}

static void
brasero_project_iface_uri_container_init (BraseroURIContainerIFace *iface)
{
	iface->get_selected_uri = brasero_project_get_selected_uri;
}

static void
brasero_project_init (BraseroProject *obj)
{
	GtkWidget *alignment;
	GtkWidget *separator;
	GtkWidget *vbox;
	GtkWidget *box;

	obj->priv = g_new0 (BraseroProjectPrivate, 1);
	gtk_box_set_spacing (GTK_BOX (obj), 6);

	obj->priv->tooltip = gtk_tooltips_new ();

	/* header */
	box = gtk_hbox_new (FALSE, 8);

	obj->priv->image = gtk_image_new ();
	gtk_misc_set_alignment (GTK_MISC (obj->priv->image), 0.0, 0.0);
	gtk_box_pack_start (GTK_BOX (box), obj->priv->image, FALSE, FALSE, 0);

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), vbox, FALSE, FALSE, 0);

	obj->priv->label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (obj->priv->label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox), obj->priv->label, FALSE, FALSE, 0);

	obj->priv->subtitle = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (obj->priv->subtitle), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox), obj->priv->subtitle, FALSE, FALSE, 0);

	/* add button set insensitive since there are no files in the selection */
	obj->priv->add = brasero_utils_make_button (NULL, GTK_STOCK_ADD);
	gtk_widget_set_sensitive (obj->priv->add, FALSE);

	g_signal_connect (obj->priv->add,
			  "clicked",
			  G_CALLBACK (brasero_project_add_clicked_cb),
			  obj);
	gtk_tooltips_set_tip (obj->priv->tooltip,
			      obj->priv->add,
			      _("Add selected files"),
			      NULL);
	alignment = gtk_alignment_new (1.0, 0.5, 0.0, 0.0);
	gtk_container_add (GTK_CONTAINER (alignment), obj->priv->add);
	gtk_box_pack_start (GTK_BOX (box), alignment, TRUE, TRUE, 0);

	obj->priv->remove = brasero_utils_make_button (NULL, GTK_STOCK_REMOVE);
	gtk_widget_set_sensitive (obj->priv->remove, FALSE);
	g_signal_connect (obj->priv->remove,
			  "clicked",
			  G_CALLBACK (brasero_project_remove_clicked_cb),
			  obj);
	gtk_tooltips_set_tip (obj->priv->tooltip,
			      obj->priv->remove,
			      _("Remove files selected in project"),
			      NULL);
	alignment = gtk_alignment_new (1.0, 0.5, 0.0, 0.0);
	gtk_container_add (GTK_CONTAINER (alignment), obj->priv->remove);
	gtk_box_pack_start (GTK_BOX (box), alignment, FALSE, FALSE, 0);

	separator = gtk_vseparator_new ();
	gtk_box_pack_start (GTK_BOX (box), separator, FALSE, FALSE, 0);

	/* burn button set insensitive since there are no files in the selection */
	obj->priv->burn = brasero_utils_make_button (_("Burn"), GTK_STOCK_CDROM);
	gtk_widget_set_sensitive (obj->priv->burn, FALSE);
	g_signal_connect (obj->priv->burn,
			  "clicked",
			  G_CALLBACK (brasero_project_burn_clicked_cb),
			  obj);
	gtk_tooltips_set_tip (obj->priv->tooltip,
			      obj->priv->burn,
			      _("Start to burn the contents of the selection"),
			      NULL);
	alignment = gtk_alignment_new (1.0, 0.5, 0.0, 0.0);
	gtk_container_add (GTK_CONTAINER (alignment), obj->priv->burn);
	gtk_box_pack_start (GTK_BOX (box), alignment, FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (obj), box, FALSE, FALSE, 0);


	/* The two panes to put into the notebook */
	obj->priv->audio = brasero_audio_disc_new ();
	g_signal_connect (G_OBJECT (obj->priv->audio),
			  "contents_changed",
			  G_CALLBACK (brasero_project_contents_changed_cb),
			  obj);
	g_signal_connect (G_OBJECT (obj->priv->audio),
			  "size_changed",
			  G_CALLBACK (brasero_project_size_changed_cb),
			  obj);
	g_signal_connect (G_OBJECT (obj->priv->audio),
			  "selection_changed",
			  G_CALLBACK (brasero_project_selection_changed_cb),
			  obj);

	obj->priv->data = brasero_data_disc_new ();
	g_signal_connect (G_OBJECT (obj->priv->data),
			  "contents_changed",
			  G_CALLBACK (brasero_project_contents_changed_cb),
			  obj);
	g_signal_connect (G_OBJECT (obj->priv->data),
			  "size_changed",
			  G_CALLBACK (brasero_project_size_changed_cb),
			  obj);
	g_signal_connect (G_OBJECT (obj->priv->data),
			  "selection_changed",
			  G_CALLBACK (brasero_project_selection_changed_cb),
			  obj);

	obj->priv->discs = gtk_notebook_new ();
	gtk_notebook_set_show_border (GTK_NOTEBOOK (obj->priv->discs), FALSE);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (obj->priv->discs), FALSE);
	gtk_notebook_prepend_page (GTK_NOTEBOOK (obj->priv->discs),
				   obj->priv->data, NULL);
	gtk_notebook_prepend_page (GTK_NOTEBOOK (obj->priv->discs),
				   obj->priv->audio, NULL);

	gtk_box_pack_start (GTK_BOX (obj), obj->priv->discs, TRUE, TRUE, 0);

	/* size widget */
	obj->priv->size_display = brasero_project_size_new ();
	g_signal_connect (G_OBJECT (obj->priv->size_display), 
			  "disc-changed",
			  G_CALLBACK (brasero_project_disc_changed_cb),
			  obj);
	gtk_box_pack_start (GTK_BOX (obj), obj->priv->size_display, FALSE, TRUE, 0);

	obj->priv->empty = 1;
}

static void
brasero_project_finalize (GObject *object)
{
	BraseroProject *cobj;
	cobj = BRASERO_PROJECT(object);

	if (cobj->priv->project)
		g_free (cobj->priv->project);

	if (cobj->priv->tooltip) {
		g_object_ref_sink (GTK_OBJECT (cobj->priv->tooltip));
		cobj->priv->tooltip = NULL;
	}

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
brasero_project_error_size_dialog (BraseroProject *project)
{
	GtkWidget *dialog;
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (project));
	dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					 GTK_DIALOG_DESTROY_WITH_PARENT |
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_CLOSE,
					 _("The size of the project is too large for the disc even with the overburn option:"));

	gtk_window_set_title (GTK_WINDOW (dialog), _("Project size"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  _("you must delete some files."));

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static gboolean
brasero_project_overburn_dialog (BraseroProject *project)
{
	GtkWidget *dialog, *toplevel;
	gint result;

	/* get the current CD length and make sure selection is not too long */
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (project));
	dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					 GTK_DIALOG_DESTROY_WITH_PARENT|
					 GTK_DIALOG_MODAL,
					 GTK_MESSAGE_WARNING,
					 GTK_BUTTONS_NONE,
					 _("The size of the project is too large for the disc:"));

	gtk_window_set_title (GTK_WINDOW (dialog), _("Project size"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  _("Would you like to activate overburn (otherwise you must delete files) ?"
						    "\nNOTE: This option might cause failure."));

	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				GTK_STOCK_NO, GTK_RESPONSE_NO,
				GTK_STOCK_YES, GTK_RESPONSE_YES,
				NULL);

	result = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	if (result == GTK_RESPONSE_YES)
		return TRUE;

	return FALSE;
}

static void
brasero_project_check_size (BraseroProject *project)
{
	gboolean result;
	gboolean overburn;
	GtkAction *action;
	gboolean sensitive;

	result = brasero_project_size_check_status (BRASERO_PROJECT_SIZE (project->priv->size_display),
						    &overburn);

	if (result) {
		project->priv->oversized = 0;
		goto end;
	}

	if (project->priv->is_burning) {
		/* we don't want to show the following dialog while burning */
		if (overburn)
			project->priv->oversized = 0;
		else
			project->priv->oversized = 1;
		return;
	}

	if (overburn) {
		project->priv->oversized = 0;
		goto end;
	}

	/* avoid telling the user the same thing twice */
	if (project->priv->oversized)
		goto end;

	project->priv->oversized = 1;
	brasero_project_error_size_dialog (project);


end:
	if (project->priv->oversized) {
		sensitive = FALSE;
		g_object_set (G_OBJECT (project->priv->data), "reject-file", TRUE, NULL);
		g_object_set (G_OBJECT (project->priv->data), "reject-file", TRUE, NULL);
	}
	else {
		sensitive = TRUE;
		g_object_set (G_OBJECT (project->priv->data), "reject-file", FALSE, NULL);
		g_object_set (G_OBJECT (project->priv->data), "reject-file", FALSE, NULL);
	}

	action = gtk_action_group_get_action (project->priv->action_group, "Add");
	gtk_action_set_sensitive (action, sensitive);
	gtk_widget_set_sensitive (project->priv->add, sensitive);

	/* we need to make sure there is actually something to burn */
	sensitive = (project->priv->empty == FALSE &&
		     project->priv->oversized == FALSE);

	action = gtk_action_group_get_action (project->priv->action_group, "Burn");
	gtk_action_set_sensitive (action, sensitive);
	gtk_widget_set_sensitive (project->priv->burn, sensitive);
}

static void
brasero_project_size_changed_cb (BraseroDisc *disc,
			         gint64 sectors,
			         BraseroProject *project)
{
	project->priv->sectors = sectors;

	brasero_project_size_set_sectors (BRASERO_PROJECT_SIZE (project->priv->size_display),
					  sectors);

	brasero_project_check_size (project);
}

static void
brasero_project_disc_changed_cb (BraseroProjectSize *size,
				 BraseroProject *project)
{
	brasero_project_check_size (project);
}

/***************************** URIContainer ************************************/
static void
brasero_project_selection_changed_cb (BraseroDisc *disc,
				      BraseroProject *project)
{
	/* FIXME! from here we can control the add button state depending on the source URI */
	brasero_uri_container_uri_selected (BRASERO_URI_CONTAINER (project));
}

static char *
brasero_project_get_selected_uri (BraseroURIContainer *container)
{
	BraseroProject *project;

	project = BRASERO_PROJECT (container);
	return brasero_disc_get_selected_uri (project->priv->current);
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

	gtk_window_set_title (GTK_WINDOW (dialog), _("Please wait"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  _("some tasks are not completed yet."));

	gtk_window_set_title (GTK_WINDOW (dialog), _("Waiting for ongoing tasks"));

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
					  _("No song in the project:"));

	gtk_window_set_title (GTK_WINDOW (message), _("Empty project"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
						  _("please add songs to the project."));

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
					  _("Project is empty:"));

	gtk_window_set_title (GTK_WINDOW (message), _("Empty project"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
						  _("please add files to the project."));

	gtk_dialog_run (GTK_DIALOG (message));
	gtk_widget_destroy (message);
}

void
brasero_project_burn (BraseroProject *project)
{
	BraseroBurnFlag flags = BRASERO_BURN_FLAG_NONE;
	BraseroTrackSource *source;
	BraseroImageFormat format;
	NautilusBurnDrive *drive;
	BraseroDiscResult result;
	GtkWidget *toplevel;
	GtkWidget *dialog;
	gboolean overburn = FALSE;
	gboolean checksum = FALSE;
	gboolean destroy;
	gchar *output;
	gchar *label;
	gint speed;

	result = brasero_project_size_check_status (BRASERO_PROJECT_SIZE (project->priv->size_display),
						    &overburn);
	if (result == FALSE) {
		if (!overburn) {
			brasero_project_error_size_dialog (project);
			return;
		}

		if (!brasero_project_overburn_dialog (project))
			return;
	}

	result = brasero_project_check_status (project, project->priv->current);
	if (result != BRASERO_DISC_OK)
		return;

	result = brasero_disc_get_track_source (project->priv->current,
						&source,
						BRASERO_IMAGE_FORMAT_ANY);
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

	project->priv->is_burning = 1;
	destroy = FALSE;

	/* setup, show, and run options dialog */
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (project));

	dialog = brasero_burn_option_dialog_new (source);

	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_widget_show_all (dialog);

	result = gtk_dialog_run (GTK_DIALOG (dialog));
	if (result != GTK_RESPONSE_OK)
		goto end;

	brasero_burn_option_dialog_get_session_param (BRASERO_BURN_OPTION_DIALOG (dialog),
						      &drive,
						      &speed,
						      &output,
						      &flags,
						      &label,
						      &format,
						      &checksum);
	gtk_widget_destroy (dialog);

	if (source->type == BRASERO_TRACK_SOURCE_DATA) {
		if ((format & BRASERO_IMAGE_FORMAT_JOLIET)
		&& !(source->format & BRASERO_IMAGE_FORMAT_JOLIET)) {
			/* the user forced the use of joliet extension */
			brasero_track_source_free (source);
			result = brasero_disc_get_track_source (project->priv->current,
								&source,
								BRASERO_IMAGE_FORMAT_JOLIET);
		}
		else if (!(format & BRASERO_IMAGE_FORMAT_JOLIET)
		      && (source->format & BRASERO_IMAGE_FORMAT_JOLIET))
			source->format &= ~BRASERO_IMAGE_FORMAT_JOLIET;			

		source->contents.data.label = label;
	}

	if (overburn)
		flags |= BRASERO_BURN_FLAG_OVERBURN;

	/* now setup the burn dialog */
	dialog = brasero_burn_dialog_new ();
	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

	gtk_widget_hide (toplevel);
	gtk_widget_show_all (dialog);

	destroy = brasero_burn_dialog_run (BRASERO_BURN_DIALOG (dialog),
					   drive,
					   speed,
					   output,
					   source,
					   flags,
					   project->priv->sectors,
					   checksum);

	if (output)
		g_free (output);

	nautilus_burn_drive_unref (drive);

end:
	brasero_track_source_free (source);
	project->priv->is_burning = 0;
	gtk_widget_destroy (dialog);

	if (destroy)
		gtk_widget_destroy (toplevel);
	else
		gtk_widget_show (toplevel);
}

/********************************     ******************************************/
static void
brasero_project_check_default_burning_app (BraseroProject *project,
					   const gchar *primary,
					   const gchar *secondary,
					   const gchar *key,
					   const gchar *default_command)
{
	GtkResponseType response;
	GConfClient *client;
	GtkWidget *alignment;
	GtkWidget *toplevel;
	GtkWidget *message;
	GtkWidget *check;
	gchar *command;
	gboolean ask;
	GList *children;
	GList *iter;

	client = gconf_client_get_default ();
	command = gconf_client_get_string (client,
					   key,
					   NULL);

	if (command && g_str_has_prefix (command, "brasero")) {
		g_object_unref (client);
		g_free (command);
		return;
	}
	g_free (command);

	ask = gconf_client_get_bool (client,
				     KEY_ASK_DEFAULT_BURNING_APP,
				     NULL);
	if (ask) {
		g_object_unref (client);
		return;
	}

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (project));
	message = gtk_message_dialog_new (GTK_WINDOW (toplevel),
					  GTK_DIALOG_MODAL|
					  GTK_DIALOG_DESTROY_WITH_PARENT,
					  GTK_MESSAGE_QUESTION,
					  GTK_BUTTONS_YES_NO,
					  primary);

	gtk_window_set_title (GTK_WINDOW (message), _("Default burning application"));
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
						  secondary);

	alignment = gtk_alignment_new (0.0, 0.0, 0, 0);
	gtk_widget_show (alignment);

	children = gtk_container_get_children (GTK_CONTAINER (GTK_DIALOG (message)->vbox));
	for (iter = children; iter; iter = iter->next) {
		GtkWidget *child;

		child = children->data;
		if (GTK_IS_HBOX (child)) {
			g_list_free (children);
			children = gtk_container_get_children (GTK_CONTAINER (child));
			for (iter = children; iter; iter = iter->next) {
				child = iter->data;
				if (GTK_IS_VBOX (child)) {
					gtk_box_pack_end (GTK_BOX (child),
							  alignment,
							  FALSE,
							  FALSE,
							  0);
					break;
				}
			}
			g_list_free (children);
			break;
		}
	}

	/*gtk_alignment_set_padding (GTK_ALIGNMENT (alignment),
				   0,
				   0,
				   72,
				   0);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (message)->vbox),
			    alignment,
			    FALSE,
			    FALSE,
			    0);*/

	check = gtk_check_button_new_with_label (_("don't show this dialog again"));
	gtk_container_add (GTK_CONTAINER (alignment), check);
	gtk_widget_show (check);

	response = gtk_dialog_run (GTK_DIALOG (message));
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check)))
		gconf_client_set_bool (client,
				       KEY_ASK_DEFAULT_BURNING_APP,
				       TRUE,
				       NULL);

	gtk_widget_destroy (message);

	if (response == GTK_RESPONSE_YES)
		gconf_client_set_string (client,
					 key,
					 default_command,
					 NULL);

	g_object_unref (client);
}

static void
brasero_project_switch (BraseroProject *project, gboolean audio)
{
	GdkPixbuf *pixbuf;
	GtkAction *action;
	GConfClient *client;

	project->priv->empty = 1;
	brasero_project_size_set_sectors (BRASERO_PROJECT_SIZE (project->priv->size_display),
					  0);

	gtk_action_group_set_visible (project->priv->action_group, TRUE);
	gtk_widget_set_sensitive (project->priv->add, TRUE);

	if (project->priv->current)
		brasero_disc_reset (project->priv->current);

	if (project->priv->project) {
		g_free (project->priv->project);
		project->priv->project = NULL;
	}

	client = gconf_client_get_default ();
	if (audio) {
		pixbuf = brasero_utils_get_icon_for_mime ("gnome-dev-cdrom-audio", 24);
		gtk_label_set_markup (GTK_LABEL (project->priv->label),
				      _("<big><b>Audio project</b></big>"));
		gtk_label_set_markup (GTK_LABEL (project->priv->subtitle),
				      _("<i>No track</i>"));

		project->priv->current = BRASERO_DISC (project->priv->audio);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (project->priv->discs), 0);
		brasero_project_size_set_context (BRASERO_PROJECT_SIZE (project->priv->size_display), TRUE);

		brasero_project_check_default_burning_app (project,
							   _("Brasero is not the default application to burn audio CDs:"),
							   _("Would you like to make brasero the default application to burn audio CDs?"),
							   KEY_DEFAULT_AUDIO_BURNING_APP,
							   "brasero -a");
	}
	else {
		pixbuf = brasero_utils_get_icon_for_mime ("gnome-dev-cdrom", 24);
		gtk_label_set_markup (GTK_LABEL (project->priv->label),
				      _("<big><b>Data project</b></big>"));
		gtk_label_set_markup (GTK_LABEL (project->priv->subtitle),
				      _("<i>Contents of your data project</i>"));

		project->priv->current = BRASERO_DISC (project->priv->data);
		gtk_notebook_set_current_page (GTK_NOTEBOOK (project->priv->discs), 1);
		brasero_project_size_set_context (BRASERO_PROJECT_SIZE (project->priv->size_display), FALSE);

		brasero_project_check_default_burning_app (project,
							   _("Brasero is not the default application to burn data discs:"),
							   _("Would you like to make brasero the default application to burn data discs?"),
							   KEY_DEFAULT_DATA_BURNING_APP,
							   "brasero -d");
	}

	/* set the title */
	gtk_image_set_from_pixbuf (GTK_IMAGE (project->priv->image), pixbuf);
	g_object_unref (pixbuf);
	
	/* update the menus */
	action = gtk_action_group_get_action (project->priv->project_group, "SaveAs");
	gtk_action_set_sensitive (action, TRUE);

	action = gtk_action_group_get_action (project->priv->project_group, "Save");
	gtk_action_set_sensitive (action, FALSE);
}

void
brasero_project_set_audio (BraseroProject *project, GSList *uris)
{
	brasero_project_switch (project, TRUE);

	for (; uris; uris = uris->next) {
		gchar *uri;

		uri = uris->data;
		brasero_disc_add_uri (project->priv->current, uri);
	}
}

void
brasero_project_set_data (BraseroProject *project, GSList *uris)
{
	brasero_project_switch (project, FALSE);

	for (; uris; uris = uris->next) {
		gchar *uri;

		uri = uris->data;
		brasero_disc_add_uri (project->priv->current, uri);
	}
}

void
brasero_project_set_none (BraseroProject *project)
{
	GtkAction *action;

	if (project->priv->current)
		brasero_disc_reset (project->priv->current);

	project->priv->current = NULL;

	/* update button */
	gtk_action_group_set_visible (project->priv->action_group, FALSE);
	gtk_widget_set_sensitive (project->priv->add, FALSE);

	/* update the menus */
	action = gtk_action_group_get_action (project->priv->project_group, "SaveAs");
	gtk_action_set_sensitive (action, FALSE);
	action = gtk_action_group_get_action (project->priv->project_group, "Save");
	gtk_action_set_sensitive (action, FALSE);
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

	sensitive = (project->priv->empty == FALSE);

	action = gtk_action_group_get_action (project->priv->action_group, "Delete");
	gtk_action_set_sensitive (action, sensitive);
	action = gtk_action_group_get_action (project->priv->action_group, "DeleteAll");
	gtk_action_set_sensitive (action, sensitive);
	gtk_widget_set_sensitive (project->priv->remove, sensitive);

	/* the following button/action states depend on the project size too */
	sensitive = (project->priv->oversized == 0 &&
		     project->priv->empty == 0);

	action = gtk_action_group_get_action (project->priv->action_group, "Burn");
	gtk_action_set_sensitive (action, sensitive);
	gtk_widget_set_sensitive (project->priv->burn, sensitive);

	/* the following button/action states depend on the project size only */
	sensitive = (project->priv->oversized == FALSE);

	action = gtk_action_group_get_action (project->priv->action_group, "Add");
	gtk_action_set_sensitive (action, sensitive);
	gtk_widget_set_sensitive (project->priv->add, sensitive);

	/* the state of the following depends on the existence of an opened project */
	action = gtk_action_group_get_action (project->priv->project_group, "Save");
	if (project->priv->project)
		gtk_action_set_sensitive (action, TRUE);
	else
		gtk_action_set_sensitive (action, FALSE);

	if (BRASERO_IS_AUDIO_DISC (disc)) {
		gchar *string;

		if (!nb_files)
			string = g_strdup (_("<i>No track</i>"));
		else if (nb_files)
			string = g_strdup_printf (ngettext ("<i>%d track</i>", "<i>%d tracks</i>", nb_files), nb_files);

		gtk_label_set_markup (GTK_LABEL (project->priv->subtitle), string);
		g_free (string);
	}
}

/**************************** manage the relations with the sources ************/
static void
brasero_project_transfer_uris_from_src (BraseroProject *project)
{
	char **uris;
	char **uri;

	uris = brasero_uri_container_get_selected_uris (project->priv->current_source);
	if (!uris)
		return;

	uri = uris;
	while (*uri) {
		brasero_disc_add_uri (project->priv->current, *uri);
		uri ++;
	}
}

static void
brasero_project_source_uri_activated_cb (BraseroURIContainer *container,
					 BraseroProject *project)
{
	project->priv->current_source = container;
	brasero_project_transfer_uris_from_src (project);
}

static void
brasero_project_source_uri_selected_cb (BraseroURIContainer *container,
					BraseroProject *project)
{
	project->priv->current_source = container;
}

void
brasero_project_add_source (BraseroProject *project,
			    BraseroURIContainer *source)
{
	g_signal_connect (source,
			  "uri-activated",
			  G_CALLBACK (brasero_project_source_uri_activated_cb),
			  project);
	g_signal_connect (source,
			  "uri-selected",
			  G_CALLBACK (brasero_project_source_uri_selected_cb),
			  project);
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
brasero_project_add_uris_cb (GtkAction *action, BraseroProject *project)
{
	brasero_project_transfer_uris_from_src (project);
}

static void
brasero_project_remove_selected_uris_cb (GtkAction *action, BraseroProject *project)
{
	brasero_disc_delete_selected (BRASERO_DISC (project->priv->current));
}

static void
brasero_project_empty_cb (GtkAction *action, BraseroProject *project)
{
	brasero_disc_reset (BRASERO_DISC (project->priv->current));
}

static void
brasero_project_burn_cb (GtkAction *action, BraseroProject *project)
{
	brasero_project_burn (project);
}

static void
brasero_project_add_clicked_cb (GtkButton *button, BraseroProject *project)
{
	brasero_project_transfer_uris_from_src (project);

	g_signal_emit (project,
		       brasero_project_signals [ADD_PRESSED_SIGNAL],
		       0);
}

static void
brasero_project_remove_clicked_cb (GtkButton *button, BraseroProject *project)
{
	brasero_disc_delete_selected (BRASERO_DISC (project->priv->current));
}

static void
brasero_project_burn_clicked_cb (GtkButton *button, BraseroProject *project)
{
	brasero_project_burn (project);
}

void
brasero_project_register_menu (BraseroProject *project, GtkUIManager *manager)
{
	GError *error = NULL;
	GtkAction *action;

	/* menus */
	project->priv->project_group = gtk_action_group_new ("ProjectActions1");
	gtk_action_group_set_translation_domain (project->priv->project_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (project->priv->project_group,
				      entries_project,
				      G_N_ELEMENTS (entries_project),
				      project);

	gtk_ui_manager_insert_action_group (manager, project->priv->project_group, 0);
	if (!gtk_ui_manager_add_ui_from_string (manager,
						description_project,
						-1,
						&error)) {
		g_message ("building menus failed: %s", error->message);
		g_error_free (error);
	}

	action = gtk_action_group_get_action (project->priv->project_group, "Save");
	gtk_action_set_sensitive (action, FALSE);
	action = gtk_action_group_get_action (project->priv->project_group, "SaveAs");
	gtk_action_set_sensitive (action, FALSE);

	project->priv->action_group = gtk_action_group_new ("ProjectActions2");
	gtk_action_group_set_translation_domain (project->priv->action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (project->priv->action_group,
				      entries_actions,
				      G_N_ELEMENTS (entries_actions),
				      project);

	gtk_ui_manager_insert_action_group (manager, project->priv->action_group, 0);
	if (!gtk_ui_manager_add_ui_from_string (manager,
						description_actions,
						-1,
						&error)) {
		g_message ("building menus failed: %s", error->message);
		g_error_free (error);
	}

	action = gtk_action_group_get_action (project->priv->action_group, "Burn");
	gtk_action_set_sensitive (action, FALSE);
	action = gtk_action_group_get_action (project->priv->action_group, "Delete");
	gtk_action_set_sensitive (action, FALSE);
	action = gtk_action_group_get_action (project->priv->action_group, "DeleteAll");
	gtk_action_set_sensitive (action, FALSE);
}

/******************************* common to save/open ***************************/
static void
brasero_project_set_path (BraseroProject *project,
			  const gchar *path,
			  BraseroProjectType type)
{
	char *title;
	GtkAction *action;
	GtkWidget *toplevel;

	/* possibly reset the name of the project */
	if (path) {
		if (project->priv->project)
			g_free (project->priv->project);

		project->priv->project = g_strdup (path);
	}

	/* update the name of the main window */
	if (type == BRASERO_PROJECT_TYPE_DATA)
		title = g_strdup_printf (_("Brasero - %s (data disc)"),
					 project->priv->project);
	else
		title = g_strdup_printf (_("Brasero - %s (audio disc)"),
					 project->priv->project);

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

	gtk_window_set_title (GTK_WINDOW (dialog), _("Project loading error"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  reason);

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static BraseroGraftPt *
_read_graft_point (xmlDocPtr project,
		   xmlNodePtr graft)
{
	BraseroGraftPt *retval;

	retval = g_new0 (BraseroGraftPt, 1);
	while (graft) {
		if (!xmlStrcmp (graft->name, (const xmlChar *) "uri")) {
			if (retval->uri)
				goto error;

			retval->uri = (char*) xmlNodeListGetString (project,
							    graft->xmlChildrenNode,
							    1);
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

			retval->excluded = g_slist_prepend (retval->excluded,
							    excluded);
		}
		else if (graft->type == XML_ELEMENT_NODE)
			goto error;

		graft = graft->next;
	}

	retval->excluded = g_slist_reverse (retval->excluded);
	return retval;

error:
	brasero_graft_point_free (retval);
	return NULL;
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
			BraseroGraftPt *graft;

			graft = _read_graft_point (project, item->xmlChildrenNode);
			if (!graft)
				goto error;

			track->contents.data.grafts = g_slist_prepend (track->contents.data.grafts, graft);
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
		else if (item->type == XML_ELEMENT_NODE)
			goto error;

		item = item->next;
	}

	track->contents.data.grafts = g_slist_reverse (track->contents.data.grafts);
	return (BraseroDiscTrack *) track;

error :
	brasero_track_free ((BraseroDiscTrack*) track);
	return NULL;
}

static BraseroDiscTrack *
_read_audio_track (xmlDocPtr project,
		   xmlNodePtr uris)
{
	BraseroDiscTrack *track;
	BraseroDiscSong *song;

	track = g_new0 (BraseroDiscTrack, 1);
	track->type = BRASERO_DISC_TRACK_AUDIO;

	song = NULL;

	while (uris) {
		if (!xmlStrcmp (uris->name, (const xmlChar *) "uri")) {
			gchar *uri;

			uri = (gchar *) xmlNodeListGetString (project,
							      uris->xmlChildrenNode,
							      1);
			if (!uri)
				goto error;

			song = g_new0 (BraseroDiscSong, 1);
			song->uri = uri;
			track->contents.tracks = g_slist_prepend (track->contents.tracks, song);
		}
		else if (!xmlStrcmp (uris->name, (const xmlChar *) "silence")) {
			gchar *silence;

			if (!song)
				goto error;

			silence = (gchar *) xmlNodeListGetString (project,
								  uris->xmlChildrenNode,
								  1);
			if (!silence)
				goto error;

			song->gap = (gint64) g_ascii_strtoull (silence, NULL, 10);
			g_free (silence);

			/* This is to prevent two gaps in a row */
			song = NULL;
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
		}
		else if (!xmlStrcmp (track_node->name, (const xmlChar *) "data")) {
			if (newtrack)
				goto error;

			newtrack = _read_data_track (project,
						     track_node->xmlChildrenNode);

			if (!newtrack)
				goto error;
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
				  BraseroDiscTrack **track)
{
	xmlNodePtr track_node = NULL;
	xmlDocPtr project;
	xmlNodePtr item;
	gboolean retval;

	/* start parsing xml doc */
	project = xmlParseFile (uri);
	if (!project) {
		brasero_project_invalid_project_dialog (proj, _("the project could not be opened."));
		return FALSE;
	}

	/* parses the "header" */
	item = xmlDocGetRootElement (project);
	if (!item) {
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

	if (!retval)
		brasero_project_invalid_project_dialog (proj,
							_("it doesn't seem to be a valid brasero project."));

	return retval;

error:

	xmlFreeDoc (project);
	brasero_project_invalid_project_dialog (proj, _("it doesn't seem to be a valid brasero project."));
	return FALSE;
}

BraseroProjectType
brasero_project_open_project (BraseroProject *project,
			      const gchar *name)
{
	BraseroDiscTrack *track = NULL;
	BraseroProjectType type;
	GtkWidget *toplevel;
	char *path;

	if (!name) {
		GtkWidget *chooser;
		int answer;

		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (project));
		chooser = gtk_file_chooser_dialog_new (_("Open a project"),
						      GTK_WINDOW (toplevel),
						      GTK_FILE_CHOOSER_ACTION_OPEN,
						      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						      GTK_STOCK_OPEN, GTK_RESPONSE_OK,
						      NULL);
		gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), FALSE);
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser),
						     g_get_home_dir ());
	
		gtk_widget_show (chooser);
		answer = gtk_dialog_run (GTK_DIALOG (chooser));
	
		if (answer == GTK_RESPONSE_OK)
			path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
		else
			path = NULL;
	
		gtk_widget_destroy (chooser);
	}
	else
		path = g_strdup (name);

	if (!path || *path =='\0')
		return BRASERO_PROJECT_TYPE_INVALID;

	if (!brasero_project_open_project_xml (project, path, &track)) {
		g_free (path);
		return BRASERO_PROJECT_TYPE_INVALID;
	}

	brasero_project_size_set_sectors (BRASERO_PROJECT_SIZE (project->priv->size_display),
					  0);

	if (track->type == BRASERO_DISC_TRACK_AUDIO) {
		brasero_project_switch (project, TRUE);
		type = BRASERO_PROJECT_TYPE_AUDIO;
	}
	else if (track->type == BRASERO_DISC_TRACK_DATA) {
		brasero_project_switch (project, FALSE);
		type = BRASERO_PROJECT_TYPE_DATA;
	}
	else 
		return BRASERO_PROJECT_TYPE_INVALID;

	brasero_disc_load_track (project->priv->current, track);
	brasero_track_free (track);

	brasero_project_set_path (project, path, type);
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

	gtk_window_set_title (GTK_WINDOW (dialog), _("Unsaved project"));

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

static gboolean
_save_audio_track_xml (xmlTextWriter *project,
		       BraseroDiscTrack *track)
{
	GSList *iter;
	gint success;

	for (iter = track->contents.tracks; iter; iter = iter->next) {
		BraseroDiscSong *song;

		song = iter->data;
		success = xmlTextWriterWriteElement (project,
						     (xmlChar *) "uri",
						     (xmlChar *) song->uri);
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
	}

	return TRUE;
}

static gboolean
_save_data_track_xml (xmlTextWriter *project,
		      BraseroDiscTrack *track)
{
	char *uri;
	int success;
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
			success = xmlTextWriterWriteElement (project, (xmlChar *) "uri", (xmlChar *) graft->uri);
			if (success < 0)
				return FALSE;
		}

		for (iter = graft->excluded; iter; iter = iter->next) {
			uri = iter->data;
			success = xmlTextWriterWriteElement (project, (xmlChar *) "excluded", (xmlChar *) uri);
			if (success < 0)
				return FALSE;
		}

		success = xmlTextWriterEndElement (project); /* graft */
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
				  const char *path,
				  BraseroDiscTrack *track)
{
	xmlTextWriter *project;
	gboolean retval;
	int success;

	project = xmlNewTextWriterFilename (path, 0);
	if (!project) {
		brasero_project_not_saved_dialog (proj);
		return FALSE;
	}

	xmlTextWriterSetIndent (project, 1);
	xmlTextWriterSetIndentString (project, (xmlChar *) "\t");

	success = xmlTextWriterStartDocument (project,
					      NULL,
					      NULL,
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
	return TRUE;

error:

	xmlTextWriterEndDocument (project);
	xmlFreeTextWriter (project);
	g_remove (path);
	
	brasero_project_not_saved_dialog (proj);
	return FALSE;
}

static gboolean
brasero_project_save_project_real (BraseroProject *project,
				   const char *path)
{
	BraseroDiscResult result;
	BraseroDiscTrack track;
	gboolean retval;

	g_return_val_if_fail (path != NULL || project->priv->project != NULL, FALSE);

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

	retval = brasero_project_save_project_xml (project,
						   path ? path : project->priv->project,
						   &track);

	if (retval)
		brasero_project_set_path (project, path, track.type);

	brasero_track_clear (&track);
	return retval;
}

static char *
brasero_project_save_project_ask_for_path (BraseroProject *project)
{
	GtkWidget *toplevel;
	GtkWidget *chooser;
	char *path = NULL;
	int answer;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (project));
	chooser = gtk_file_chooser_dialog_new (_("Save current project"),
					       GTK_WINDOW (toplevel),
					       GTK_FILE_CHOOSER_ACTION_SAVE,
					       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					       GTK_STOCK_SAVE, GTK_RESPONSE_OK,
					       NULL);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), FALSE);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser),
					     g_get_home_dir ());

	gtk_widget_show (chooser);
	answer = gtk_dialog_run (GTK_DIALOG (chooser));
	if (answer == GTK_RESPONSE_OK) {
		path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
		if (*path == '\0') {
			g_free (path);
			path = NULL;
		}
	}

	gtk_widget_destroy (chooser);
	return path;
}

gboolean
brasero_project_save_project (BraseroProject *project)
{
	char *path = NULL;
	gboolean result;

	if (!project->priv->project
	&&  !(path = brasero_project_save_project_ask_for_path (project)))
		return FALSE;

	result = brasero_project_save_project_real (project, path);
	g_free (path);

	return result;
}

gboolean
brasero_project_save_project_as (BraseroProject *project)
{
	gboolean result;
	char *path;

	path = brasero_project_save_project_ask_for_path (project);
	if (!path)
		return FALSE;

	result = brasero_project_save_project_real (project, path);
	g_free (path);

	return result;
}
