/***************************************************************************
 *            brasero-project-manager.c
 *
 *  mer mai 24 14:22:56 2006
 *  Copyright  2006  Rouquier Philippe
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
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtkstock.h>
#include <gtk/gtkaction.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtkfilefilter.h>
#include <gtk/gtkfilechooser.h>
#include <gtk/gtkfilechooserwidget.h>
#include <gtk/gtkfilechooserdialog.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkuimanager.h>
#include <gtk/gtkscrolledwindow.h>

#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-file-info.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include "utils.h"
#include "project.h"
#include "brasero-layout.h"
#include "brasero-project-manager.h"
#include "brasero-file-chooser.h"
#include "burn-caps.h"
#include "burn-options-dialog.h"
#include "burn-dialog.h"
#include "project-type-chooser.h"
#include "disc-copy-dialog.h"

#ifdef BUILD_SEARCH
#include "search.h"
#endif

#ifdef BUILD_PLAYLIST
#include "play-list.h"
#endif

#ifdef BUILD_PREVIEW
#include "player.h"
#endif

static void brasero_project_manager_class_init (BraseroProjectManagerClass *klass);
static void brasero_project_manager_init (BraseroProjectManager *sp);
static void brasero_project_manager_finalize (GObject *object);

static void
brasero_project_manager_type_changed_cb (BraseroProjectTypeChooser *chooser,
					 BraseroProjectType type,
					 BraseroProjectManager *manager);

static void
brasero_project_manager_new_empty_prj_cb (GtkAction *action, BraseroProjectManager *manager);
static void
brasero_project_manager_new_audio_prj_cb (GtkAction *action, BraseroProjectManager *manager);
static void
brasero_project_manager_new_data_prj_cb (GtkAction *action, BraseroProjectManager *manager);
static void
brasero_project_manager_new_copy_prj_cb (GtkAction *action, BraseroProjectManager *manager);
static void
brasero_project_manager_new_iso_prj_cb (GtkAction *action, BraseroProjectManager *manager);
static void
brasero_project_manager_open_cb (GtkAction *action, BraseroProjectManager *manager);

static void
brasero_project_manager_switch (BraseroProjectManager *manager,
				BraseroProjectType type,
				GSList *uris,
				const gchar *uri);

/* menus */
static GtkActionEntry entries [] = {
	{"New", GTK_STOCK_NEW, N_("New project"), NULL,
	 N_("Create a new project"), NULL },
	{"NewChoose", GTK_STOCK_NEW, N_("Empty project"), NULL,
	 N_("Let you choose your new project"), G_CALLBACK (brasero_project_manager_new_empty_prj_cb)},
	{"NewAudio", NULL, N_("New audio project"), NULL,
	 N_("Create a new audio project"), G_CALLBACK (brasero_project_manager_new_audio_prj_cb)},
	{"NewData", NULL, N_("New data project"), NULL,
	 N_("Create a new data project"), G_CALLBACK (brasero_project_manager_new_data_prj_cb)},
	{"NewCopy", NULL, N_("Copy disc"), NULL,
	 N_("Copy a disc"), G_CALLBACK (brasero_project_manager_new_copy_prj_cb)},
	{"NewIso", NULL, N_("Burn an image"), NULL,
	 N_("Burn an image"), G_CALLBACK (brasero_project_manager_new_iso_prj_cb)},

	{"Open", GTK_STOCK_OPEN, N_("_Open"), NULL,
	 N_("Open a project"), G_CALLBACK (brasero_project_manager_open_cb)},
};

static const char *description = {
	"<ui>"
	    "<menubar name='menubar' >"
		"<menu action='ProjectMenu'>"
			"<placeholder name='ViewPlaceholder'/>"

			"<menu action='New'>"
				"<menuitem action='NewAudio'/>"
				"<menuitem action='NewData'/>"
				"<menuitem action='NewCopy'/>"	
				"<menuitem action='NewIso'/>"	
			"</menu>"

			"<separator/>"
			"<placeholder name='ProjectPlaceholder'/>"
			    "<menuitem action='Open'/>"
			    "<menuitem action='Recent'/>"
			    "<separator/>"
		"</menu>"
	    "</menubar>"

	    "<toolbar name='toolbar'>"
		"<toolitem action='NewChoose'/>"
		"<toolitem action='Open'/>"
		"<toolitem action='Save'/>"
		"<separator/>"
	    "</toolbar>"
	"</ui>"
};

struct BraseroProjectManagerPrivate {
	GtkWidget *project;
	GtkWidget *layout;

	GtkActionGroup *action_group;
};

static GObjectClass *parent_class = NULL;

GType
brasero_project_manager_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroProjectManagerClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_project_manager_class_init,
			NULL,
			NULL,
			sizeof (BraseroProjectManager),
			0,
			(GInstanceInitFunc)brasero_project_manager_init,
		};

		type = g_type_register_static (GTK_TYPE_NOTEBOOK,
					       "BraseroProjectManager",
					       &our_info,
					       0);
	}

	return type;
}

static void
brasero_project_manager_class_init (BraseroProjectManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_project_manager_finalize;
}

static void
brasero_project_manager_init (BraseroProjectManager *obj)
{
	GtkWidget *type;
	GtkWidget *scroll;
	GtkWidget *chooser;

	obj->priv = g_new0 (BraseroProjectManagerPrivate, 1);

	gtk_notebook_set_show_border (GTK_NOTEBOOK (obj), FALSE);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (obj), FALSE);

	obj->priv->action_group = gtk_action_group_new ("ProjectManagerAction");
	gtk_action_group_set_translation_domain (obj->priv->action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (obj->priv->action_group,
				      entries,
				      G_N_ELEMENTS (entries),
				      obj);

	/* add the project type chooser to the notebook */
	type = brasero_project_type_chooser_new ();
	gtk_widget_show (type);
	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scroll);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scroll), type);

	g_signal_connect (G_OBJECT (type),
			  "chosen",
			  G_CALLBACK (brasero_project_manager_type_changed_cb),
			  obj);

	gtk_notebook_prepend_page (GTK_NOTEBOOK (obj), scroll, NULL);

	/* add the layout */
	obj->priv->layout = brasero_layout_new ();
	gtk_widget_show (obj->priv->layout);
	gtk_notebook_append_page (GTK_NOTEBOOK (obj), obj->priv->layout, NULL);

	/* create the project for audio and data discs */
	obj->priv->project = brasero_project_new ();
	gtk_widget_show_all (obj->priv->project);

#ifdef BUILD_PREVIEW
	GtkWidget *preview;

	preview = brasero_player_new ();
	gtk_widget_show (preview);
	brasero_player_add_source (BRASERO_PLAYER (preview),
				   BRASERO_URI_CONTAINER (obj->priv->project));

#endif /* BUILD_PREVIEW */

	chooser = brasero_file_chooser_new ();
	gtk_widget_show_all (chooser);
	brasero_layout_add_source (BRASERO_LAYOUT (obj->priv->layout),
				   chooser,
				   "Chooser",
				   _("<big><b>File Browser</b></big>"),
				   _("<i>Browse the file system</i>"),
				   _("File browser"),
				   _("Display file browser"),
				   GTK_STOCK_DIRECTORY, 
				   BRASERO_LAYOUT_AUDIO|BRASERO_LAYOUT_DATA);
	brasero_project_add_source (BRASERO_PROJECT (obj->priv->project),
				    BRASERO_URI_CONTAINER (chooser));

#ifdef BUILD_PREVIEW
	brasero_player_add_source (BRASERO_PLAYER (preview),
				   BRASERO_URI_CONTAINER (chooser));
#endif

	brasero_layout_add_project (BRASERO_LAYOUT (obj->priv->layout),
				    obj->priv->project);
#ifdef BUILD_SEARCH
	GtkWidget *search;

	search = brasero_search_new ();
	gtk_widget_show_all (search);
	brasero_layout_add_source (BRASERO_LAYOUT (obj->priv->layout),
				   search,
				   "Search",
				   _("<big><b>Search Files</b></big>"),
				   _("<i>Search files using keywords</i>"),
				   _("Search files"),
				   _("Display search"),
				   GTK_STOCK_FIND, 
				   BRASERO_LAYOUT_AUDIO|BRASERO_LAYOUT_DATA);
	brasero_project_add_source (BRASERO_PROJECT (obj->priv->project),
				    BRASERO_URI_CONTAINER (search));

#ifdef BUILD_PREVIEW
	brasero_player_add_source (BRASERO_PLAYER (preview),
				   BRASERO_URI_CONTAINER (search));
#endif

#endif /* BUILD_SEARCH */

#ifdef BUILD_PLAYLIST
	GtkWidget *playlist;

	playlist = brasero_playlist_new ();
	gtk_widget_show_all (playlist);
	brasero_layout_add_source (BRASERO_LAYOUT (obj->priv->layout),
				   playlist,
				   "Playlist",
				   _("<big><b>Playlists</b></big>"),
				   _("<i>Display playlists and their contents</i>"),
				   _("Playlists"),
				   _("Display playlists"),
				   BRASERO_STOCK_PLAYLIST, 
				   BRASERO_LAYOUT_AUDIO);

	brasero_project_add_source (BRASERO_PROJECT (obj->priv->project),
				    BRASERO_URI_CONTAINER (playlist));

#ifdef BUILD_PREVIEW
	brasero_player_add_source (BRASERO_PLAYER (preview),
				   BRASERO_URI_CONTAINER (playlist));

	brasero_layout_add_preview (BRASERO_LAYOUT (obj->priv->layout),
				    preview);
#endif

#endif /* BUILD_PLAYLIST */
}

static void
brasero_project_manager_finalize (GObject *object)
{
	BraseroProjectManager *cobj;

	cobj = BRASERO_PROJECT_MANAGER (object);
	g_free (cobj->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkWidget *
brasero_project_manager_new ()
{
	BraseroProjectManager *obj;
	
	obj = BRASERO_PROJECT_MANAGER (g_object_new (BRASERO_TYPE_PROJECT_MANAGER, NULL));
	
	return GTK_WIDGET (obj);
}

void
brasero_project_manager_register_menu (BraseroProjectManager *manager,
				       GtkUIManager *ui_manager)
{
	GtkAction *action;
	GError *error = NULL;

	action = gtk_action_new ("Recent",
				 _("Recent projects"),
				 _("Display the projects recently opened"),
				 NULL);
	gtk_action_group_add_action (manager->priv->action_group, action);
	g_object_unref (action);

	gtk_ui_manager_insert_action_group (ui_manager, manager->priv->action_group, 0);
	if (!gtk_ui_manager_add_ui_from_string (ui_manager, description, -1, &error)) {
		g_message ("building menus failed: %s", error->message);
		g_error_free (error);
	}
   	brasero_layout_register_menu (BRASERO_LAYOUT (manager->priv->layout), ui_manager);
	brasero_project_register_menu (BRASERO_PROJECT (manager->priv->project), ui_manager);
}

static void
brasero_project_manager_burn (BraseroProjectManager *manager,
			      NautilusBurnDrive *drive,
			      gint speed,
			      gchar *output,
			      BraseroTrackSource *source,
			      BraseroBurnFlag flags)
{
	GtkWidget *toplevel;
	GtkWidget *dialog;
	gboolean destroy;

	/* now setup the burn dialog */
	dialog = brasero_burn_dialog_new ();

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (manager));
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
					   0,
					   FALSE);

	gtk_widget_destroy (dialog);

	nautilus_burn_drive_unref (drive);
	brasero_track_source_free (source);

	if (output)
		g_free (output);

	if (!destroy) {
		brasero_project_manager_switch (manager,
						BRASERO_PROJECT_TYPE_INVALID,
						NULL,
						NULL);
		gtk_widget_show (toplevel);
	}
	else
		gtk_widget_destroy (toplevel);
}

static void
brasero_project_manager_burn_iso_dialog (BraseroProjectManager *manager,
					 BraseroTrackSource *track)
{
	BraseroBurnFlag flags = BRASERO_BURN_FLAG_NONE;
	NautilusBurnDrive *drive;
	GtkResponseType result;
	gchar *output = NULL;
	GtkWidget *toplevel;
	GtkWidget *dialog;
	gint speed;

	/* setup, show, and run options dialog */
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (manager));

	dialog = brasero_burn_option_dialog_new (track);

	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_widget_show_all (dialog);

	result = gtk_dialog_run (GTK_DIALOG (dialog));
	if (result != GTK_RESPONSE_OK) {
		gtk_widget_destroy (dialog);
		return;
	}

	brasero_burn_option_dialog_get_session_param (BRASERO_BURN_OPTION_DIALOG (dialog),
						      &drive,
						      &speed,
						      &output,
						      &flags,
						      NULL,
						      NULL,
						      NULL);
	gtk_widget_destroy (dialog);

	brasero_project_manager_burn (manager,
				      drive,
				      speed,
				      output,
				      track,
				      flags);
}

static void
brasero_project_manager_burn_iso (BraseroProjectManager *manager,
				  const gchar *escaped_uri)
{
	BraseroTrackSource *track = NULL;
    	GnomeVFSFileInfoOptions flag;
    	GtkRecentManager *recent;
	GnomeVFSFileInfo *info;
	GnomeVFSResult result;
	GtkWidget *toplevel;
	GtkWidget *dialog;
	gchar *uri = NULL;

	if (!escaped_uri) {
		gint answer;

		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (manager));
		dialog = gtk_file_chooser_dialog_new (_("Choose a disc image or a cue file"),
						      GTK_WINDOW (toplevel),
						      GTK_FILE_CHOOSER_ACTION_OPEN,
						      GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
						      GTK_STOCK_OPEN, GTK_RESPONSE_OK,
						      NULL);

		gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), FALSE);
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog),
						     g_get_home_dir ());
	
		gtk_widget_show (dialog);
		answer = gtk_dialog_run (GTK_DIALOG (dialog));

		if (answer != GTK_RESPONSE_OK) {
			gtk_widget_destroy (dialog);
			return;
		}

		uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (dialog));
		gtk_widget_destroy (dialog);
	}
	else
		uri = g_strdup (escaped_uri);

	/* check if it is an iso and if it is not a remote file */
	info = gnome_vfs_file_info_new ();
	flag = GNOME_VFS_FILE_INFO_GET_MIME_TYPE|
	       GNOME_VFS_FILE_INFO_FORCE_SLOW_MIME_TYPE;

retry:
    	result = gnome_vfs_get_file_info (uri, info, flag);
	if (result != GNOME_VFS_OK) {
		g_free (uri);
		goto error;
	}

    	/* Add it to recent file manager */
    	recent = gtk_recent_manager_get_default ();
    	gtk_recent_manager_add_item (recent, uri);
	if (!strcmp (info->mime_type, "application/octet-stream")) {
		track = g_new0 (BraseroTrackSource, 1);

		track->type = BRASERO_TRACK_SOURCE_IMAGE;
		track->format = BRASERO_IMAGE_FORMAT_CLONE;
		track->contents.image.image = uri;
		track->contents.image.toc = g_strdup_printf ("%s.toc", uri);
	}
	else if (!strcmp (info->mime_type, "application/x-cd-image")) {
		track = g_new0 (BraseroTrackSource, 1);
		track->type = BRASERO_TRACK_SOURCE_IMAGE;
		track->format = BRASERO_IMAGE_FORMAT_ISO;
		track->contents.image.image = uri;
	}
	else if (!strcmp (info->mime_type, "application/x-cdrdao-toc")
	     ||  !strcmp (info->mime_type, "application/x-toc")) {
		track = g_new0 (BraseroTrackSource, 1);
		track->type = BRASERO_TRACK_SOURCE_IMAGE;
		track->format = BRASERO_IMAGE_FORMAT_CUE;

		track->contents.image.toc = uri;
		if (g_str_has_suffix (track->contents.image.toc, ".toc"))
			track->contents.image.image = g_strndup (track->contents.image.toc,
								 strlen (track->contents.image.toc) - 4);
	}
	else if (flag != GNOME_VFS_FILE_INFO_GET_MIME_TYPE) {
	    	/* we failed to identify the type of file. We start again
		 * but this time using the fast method which uses the extension
		 */
	    	flag = GNOME_VFS_FILE_INFO_GET_MIME_TYPE;
	    	gnome_vfs_file_info_clear (info);
	    	goto retry;
	}
	else
		goto error;

	gnome_vfs_file_info_unref (info);

	brasero_project_manager_burn_iso_dialog (manager, track);
	return;


error:

	g_free (uri);

	{
		GtkWidget *dialog;
		GtkWidget *toplevel;

		gnome_vfs_file_info_unref (info);

		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (manager));
		dialog = gtk_message_dialog_new (GTK_WINDOW (toplevel),
						 GTK_DIALOG_DESTROY_WITH_PARENT |
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_CLOSE,
						 _("This image can't be burnt:"));

		gtk_window_set_title (GTK_WINDOW (dialog), _("Burning failure"));

		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  _("it doesn't appear to be a valid image or cue file."));
	
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

	}
}

static void
brasero_project_manager_burn_disc (BraseroProjectManager *manager)
{
	BraseroTrackSource *source;
	NautilusBurnDrive *drive;
	GtkResponseType result;
	BraseroBurnFlag flags;
	gchar *output = NULL;
	GtkWidget *toplevel;
	GtkWidget *dialog;
	gint speed;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (manager));

	dialog = brasero_disc_copy_dialog_new ();
	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_widget_show_all (dialog);

	result = gtk_dialog_run (GTK_DIALOG (dialog));
	if (result != GTK_RESPONSE_OK) {
		gtk_widget_destroy (dialog);
		return;
	}

	brasero_disc_copy_dialog_get_session_param (BRASERO_DISC_COPY_DIALOG (dialog),
						    &drive,
						    &speed,
						    &output,
						    &source,
						    &flags);
	gtk_widget_destroy (dialog);

	brasero_project_manager_burn (manager,
				      drive,
				      speed,
				      output,
				      source,
				      flags);
}

static void
brasero_project_manager_switch (BraseroProjectManager *manager,
				BraseroProjectType type,
				GSList *uris,
				const gchar *uri)
{
	GtkWidget *toplevel;
	GtkAction *action;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (manager));
	action = gtk_action_group_get_action (manager->priv->action_group, "NewChoose");

	if (type == BRASERO_PROJECT_TYPE_INVALID) {
		brasero_layout_load (BRASERO_LAYOUT (manager->priv->layout), BRASERO_LAYOUT_NONE);
		brasero_project_set_none (BRASERO_PROJECT (manager->priv->project));

		gtk_notebook_set_current_page (GTK_NOTEBOOK (manager), 0);
		gtk_action_set_sensitive (action, FALSE);

		if (toplevel)
			gtk_window_set_title (GTK_WINDOW (toplevel), "Brasero");
	}
	else if (type == BRASERO_PROJECT_TYPE_AUDIO) {
		brasero_layout_load (BRASERO_LAYOUT (manager->priv->layout), BRASERO_LAYOUT_AUDIO);

		gtk_notebook_set_current_page (GTK_NOTEBOOK (manager), 1);
		gtk_action_set_sensitive (action, TRUE);

		/* tell the BraseroProject object that we want an audio selection */
		brasero_project_set_audio (BRASERO_PROJECT (manager->priv->project), uris);

		if (toplevel)
			gtk_window_set_title (GTK_WINDOW (toplevel), _("Brasero - New audio disc project"));
	}
	else if (type == BRASERO_PROJECT_TYPE_DATA) {
		brasero_layout_load (BRASERO_LAYOUT (manager->priv->layout), BRASERO_LAYOUT_DATA);

		gtk_notebook_set_current_page (GTK_NOTEBOOK (manager), 1);
		gtk_action_set_sensitive (action, TRUE);

		/* tell the BraseroProject object that we want a data selection */
		brasero_project_set_data (BRASERO_PROJECT (manager->priv->project), uris);

		if (toplevel)
			gtk_window_set_title (GTK_WINDOW (toplevel), _("Brasero - New data disc project"));
	}
	else if (type == BRASERO_PROJECT_TYPE_ISO) {
		brasero_layout_load (BRASERO_LAYOUT (manager->priv->layout), BRASERO_LAYOUT_NONE);
		brasero_project_set_none (BRASERO_PROJECT (manager->priv->project));

		gtk_notebook_set_current_page (GTK_NOTEBOOK (manager), 0);
		gtk_action_set_sensitive (action, FALSE);

		if (toplevel)
			gtk_window_set_title (GTK_WINDOW (toplevel), _("Brasero - New image file"));
		brasero_project_manager_burn_iso (manager, uri);
	}
	else if (type == BRASERO_PROJECT_TYPE_COPY) {
		brasero_layout_load (BRASERO_LAYOUT (manager->priv->layout), BRASERO_LAYOUT_NONE);
		brasero_project_set_none (BRASERO_PROJECT (manager->priv->project));

		gtk_notebook_set_current_page (GTK_NOTEBOOK (manager), 0);
		gtk_action_set_sensitive (action, FALSE);

		if (toplevel)
			gtk_window_set_title (GTK_WINDOW (toplevel), _("Brasero - Copy a disc"));

		brasero_project_manager_burn_disc (manager);
	}
}

static void
brasero_project_manager_type_changed_cb (BraseroProjectTypeChooser *chooser,
					 BraseroProjectType type,
					 BraseroProjectManager *manager)
{
	brasero_project_manager_switch (manager, type, NULL, NULL);
}

static void
brasero_project_manager_new_empty_prj_cb (GtkAction *action, BraseroProjectManager *manager)
{
	brasero_project_manager_switch (manager, BRASERO_PROJECT_TYPE_INVALID, NULL, NULL);
}

static void
brasero_project_manager_new_audio_prj_cb (GtkAction *action, BraseroProjectManager *manager)
{
	brasero_project_manager_switch (manager, BRASERO_PROJECT_TYPE_AUDIO, NULL, NULL);
}

static void
brasero_project_manager_new_data_prj_cb (GtkAction *action, BraseroProjectManager *manager)
{
	brasero_project_manager_switch (manager, BRASERO_PROJECT_TYPE_DATA, NULL, NULL);
}

static void
brasero_project_manager_new_copy_prj_cb (GtkAction *action, BraseroProjectManager *manager)
{
	brasero_project_manager_switch (manager, BRASERO_PROJECT_TYPE_COPY, NULL, NULL);
}

static void
brasero_project_manager_new_iso_prj_cb (GtkAction *action, BraseroProjectManager *manager)
{
	brasero_project_manager_switch (manager, BRASERO_PROJECT_TYPE_ISO, NULL, NULL);
}

static void
brasero_project_manager_open_cb (GtkAction *action, BraseroProjectManager *manager)
{
	BraseroProjectType type;

	type = brasero_project_open_project (BRASERO_PROJECT (manager->priv->project), NULL);
	if (type == BRASERO_PROJECT_TYPE_INVALID)
		return;

	if (type == BRASERO_PROJECT_TYPE_DATA)
		brasero_layout_load (BRASERO_LAYOUT (manager->priv->layout), BRASERO_LAYOUT_DATA);
	else
		brasero_layout_load (BRASERO_LAYOUT (manager->priv->layout), BRASERO_LAYOUT_AUDIO);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (manager), 1);

	action = gtk_action_group_get_action (manager->priv->action_group, "NewChoose");
	gtk_action_set_sensitive (action, TRUE);
}

void
brasero_project_manager_audio (BraseroProjectManager *manager, GSList *uris)
{
	brasero_project_manager_switch (manager,
					BRASERO_PROJECT_TYPE_AUDIO,
					uris,
					NULL);
}

void
brasero_project_manager_data (BraseroProjectManager *manager, GSList *uris)
{
	brasero_project_manager_switch (manager,
					BRASERO_PROJECT_TYPE_DATA,
					uris,
					NULL);
}

void
brasero_project_manager_copy (BraseroProjectManager *manager)
{
	brasero_project_manager_switch (manager, BRASERO_PROJECT_TYPE_COPY, NULL, NULL);
}

void
brasero_project_manager_iso (BraseroProjectManager *manager, const gchar *uri)
{
	brasero_project_manager_switch (manager, BRASERO_PROJECT_TYPE_ISO, NULL, uri);
}

void
brasero_project_manager_open (BraseroProjectManager *manager, const gchar *uri)
{
	BraseroProjectType type;

    	gtk_widget_show (manager->priv->layout);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (manager), 1);
	type = brasero_project_open_project (BRASERO_PROJECT (manager->priv->project), uri);

    	if (type == BRASERO_PROJECT_TYPE_INVALID)
		brasero_project_manager_switch (manager, BRASERO_PROJECT_TYPE_INVALID, NULL, NULL);
	else if (type == BRASERO_PROJECT_TYPE_DATA)
		brasero_layout_load (BRASERO_LAYOUT (manager->priv->layout), BRASERO_LAYOUT_DATA);
	else
		brasero_layout_load (BRASERO_LAYOUT (manager->priv->layout), BRASERO_LAYOUT_AUDIO);
}

void
brasero_project_manager_empty (BraseroProjectManager *manager)
{
	brasero_project_manager_switch (manager, BRASERO_PROJECT_TYPE_INVALID, NULL, NULL);
}

gint
brasero_project_manager_get_pos (BraseroProjectManager *manager)
{
	return gtk_paned_get_position (GTK_PANED (manager->priv->layout));
}

void
brasero_project_manager_set_pos (BraseroProjectManager *manager, gint position)
{
	gtk_paned_set_position (GTK_PANED (manager->priv->layout), position);
}
