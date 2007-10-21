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

#include "brasero-utils.h"
#include "brasero-project.h"
#include "brasero-layout.h"
#include "brasero-project-manager.h"
#include "brasero-file-chooser.h"
#include "brasero-uri-container.h"
#include "brasero-image-option-dialog.h"
#include "brasero-burn-dialog.h"
#include "brasero-project-type-chooser.h"
#include "brasero-disc-copy-dialog.h"
#include "brasero-vfs.h"
#include "burn-caps.h"

#ifdef BUILD_SEARCH
#include "brasero-search-beagle.h"
#endif

#ifdef BUILD_PLAYLIST
#include "brasero-playlist.h"
#endif

#ifdef BUILD_PREVIEW
#include "brasero-preview.h"
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
				const gchar *uri,
				gboolean reset);

void
brasero_project_manager_selected_uris_changed (BraseroURIContainer *container,
					       BraseroProjectManager *manager);

/* menus */
static GtkActionEntry entries [] = {
	{"New", GTK_STOCK_NEW, N_("_New Project"), NULL,
	 N_("Create a new project"), NULL },
	{"NewChoose", GTK_STOCK_NEW, N_("_Empty Project"), NULL,
	 N_("Let you choose your new project"), G_CALLBACK (brasero_project_manager_new_empty_prj_cb)},
	{"NewAudio", "media-optical-audio-new", N_("New _Audio Project"), NULL,
	 N_("Create a new audio project"), G_CALLBACK (brasero_project_manager_new_audio_prj_cb)},
	{"NewData", "media-optical-data-new", N_("New _Data Project"), NULL,
	 N_("Create a new data project"), G_CALLBACK (brasero_project_manager_new_data_prj_cb)},
	{"NewCopy", "media-optical-copy", N_("Copy _Disc..."), NULL,
	 N_("Copy a disc"), G_CALLBACK (brasero_project_manager_new_copy_prj_cb)},
	{"NewIso", "media-optical-burn", N_("_Burn Image..."), NULL,
	 N_("Burn an image"), G_CALLBACK (brasero_project_manager_new_iso_prj_cb)},

	{"Open", GTK_STOCK_OPEN, N_("_Open..."), NULL,
	 N_("Open a project"), G_CALLBACK (brasero_project_manager_open_cb)},
};

static const char *description = {
	"<ui>"
	    "<menubar name='menubar' >"
		"<menu action='ProjectMenu'>"
			"<placeholder name='ProjectPlaceholder'>"
				"<placeholder name='ViewPlaceholder'/>"
				"<menu action='New'>"
					"<menuitem action='NewAudio'/>"
					"<menuitem action='NewData'/>"
					"<menuitem action='NewCopy'/>"	
					"<menuitem action='NewIso'/>"	
			"</menu>"
			"</placeholder>"

			"<placeholder name='ProjectPlaceholder'>"
			    "<separator/>"
			    "<menuitem action='Open'/>"
			    "<menuitem action='RecentProjects'/>"
			    "<separator/>"
			"</placeholder>"
		"</menu>"
	    "</menubar>"
	    "<toolbar name='Toolbar'>"
		"<toolitem action='NewChoose'/>"
		"<toolitem action='Open'/>"
	    "</toolbar>"
	"</ui>"
};

struct BraseroProjectManagerPrivate {
	BraseroVFS *vfs;
	BraseroProjectType type;
	BraseroVFSDataID size_preview;

	GtkWidget *project;
	GtkWidget *layout;
	GtkWidget *status;

	guint status_ctx;

	GtkActionGroup *action_group;
};

#define BRASERO_PROJECT_MANAGER_CONNECT_CHANGED(manager, container)		\
	g_signal_connect (container,						\
			  "uri-selected",					\
			  G_CALLBACK (brasero_project_manager_selected_uris_changed),	\
			  manager);

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
brasero_project_manager_size_preview (BraseroVFS *vfs,
				      GObject *object,
				      gint files_num,
				      gint invalid_num,
				      gint64 files_size,
				      gpointer user_data)
{
	BraseroProjectManager *manager = BRASERO_PROJECT_MANAGER (object);
	gint valid_num = files_num - invalid_num;
    	gchar *status_string = NULL;

	gtk_statusbar_pop (GTK_STATUSBAR (manager->priv->status),
			   manager->priv->status_ctx);

	if (!invalid_num && valid_num) {
		gchar *size_string;

		if (manager->priv->type == BRASERO_PROJECT_TYPE_AUDIO)
			size_string = brasero_utils_get_time_string (files_size, TRUE, FALSE);
		else if (manager->priv->type == BRASERO_PROJECT_TYPE_DATA)
			size_string = gnome_vfs_format_file_size_for_display (files_size);
		else
			return;

		status_string = g_strdup_printf (ngettext ("%d file selected (%s)", "%d files selected (%s)", files_num),
						 files_num,
						 size_string);
		g_free (size_string);
	}
	else if (valid_num) {
		gchar *size_string = NULL;

		if (manager->priv->type == BRASERO_PROJECT_TYPE_AUDIO) {
			size_string = brasero_utils_get_time_string (files_size, TRUE, FALSE);
			status_string = g_strdup_printf (ngettext ("%d file is supported (%s)", "%d files are supported (%s)", valid_num),
							 valid_num,
							 size_string);
		}
		else if (manager->priv->type == BRASERO_PROJECT_TYPE_DATA) {
			size_string = gnome_vfs_format_file_size_for_display (files_size);
			status_string = g_strdup_printf (ngettext ("%d file can be added (%s)", "%d selected files can be added (%s)", valid_num),
							 valid_num,
							 size_string);
		}
		else
			return;

		g_free (size_string);
	}
	else if (invalid_num) {
		if (manager->priv->type == BRASERO_PROJECT_TYPE_DATA)
			status_string = g_strdup_printf (ngettext ("No file can be added (%i selected file)",
								   "No file can be added (%i selected files)",
								   files_num),
							 files_num);
		else if (manager->priv->type == BRASERO_PROJECT_TYPE_AUDIO)
			status_string = g_strdup_printf (ngettext ("No file is supported (%i selected file)",
								   "No file is supported (%i selected files)",
								   files_num),
							 files_num);
	}
	else
		status_string = g_strdup (_("No file selected"));

	gtk_statusbar_push (GTK_STATUSBAR (manager->priv->status),
			    manager->priv->status_ctx,
			    status_string);
	g_free (status_string);
}

void
brasero_project_manager_selected_uris_changed (BraseroURIContainer *container,
					       BraseroProjectManager *manager)
{
    	gchar **uris, **iter;
    	GList *list = NULL;

	/* if we are in the middle of an unfinished size seek then
	 * cancel it and re-initialize */
	if (manager->priv->vfs)
		brasero_vfs_cancel (manager->priv->vfs, manager);

	uris = brasero_uri_container_get_selected_uris (container);
    	if (!uris) {
		gtk_statusbar_pop (GTK_STATUSBAR (manager->priv->status),
				   manager->priv->status_ctx);
		gtk_statusbar_push (GTK_STATUSBAR (manager->priv->status),
				    manager->priv->status_ctx,
				    _("No file selected"));
		return;
	}

    	for (iter = uris; iter && *iter; iter ++)
		list = g_list_prepend (list, *iter);

	if (!manager->priv->vfs)
		manager->priv->vfs = brasero_vfs_get_default ();

	if (!manager->priv->size_preview)
		manager->priv->size_preview = brasero_vfs_register_data_type (manager->priv->vfs,
									      G_OBJECT (manager),
									      G_CALLBACK (brasero_project_manager_size_preview),
									      NULL);
	brasero_vfs_get_count (manager->priv->vfs,
			       list,
			      (manager->priv->type == BRASERO_PROJECT_TYPE_AUDIO),
			       manager->priv->size_preview,
			       NULL);
			       
	g_list_free (list);
	g_strfreev (uris);
}
void
brasero_project_manager_set_status (BraseroProjectManager *manager,
				    GtkWidget *status)
{
	manager->priv->status = status;
	manager->priv->status_ctx = gtk_statusbar_get_context_id (GTK_STATUSBAR (status),
								  "size_info");
}

void
brasero_project_manager_register_ui (BraseroProjectManager *manager,
				     GtkUIManager *ui_manager)
{
	GError *error = NULL;

	gtk_ui_manager_insert_action_group (ui_manager, manager->priv->action_group, 0);
	if (!gtk_ui_manager_add_ui_from_string (ui_manager, description, -1, &error)) {
		g_message ("building UI failed: %s", error->message);
		g_error_free (error);
	}

	brasero_project_register_ui (BRASERO_PROJECT (manager->priv->project), ui_manager);
   	brasero_layout_register_ui (BRASERO_LAYOUT (manager->priv->layout), ui_manager);
}

static void
brasero_project_manager_burn (BraseroProjectManager *manager,
			      BraseroBurnSession *session)
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
					   session);

	gtk_widget_destroy (dialog);

	if (!destroy) {
		brasero_project_manager_switch (manager,
						BRASERO_PROJECT_TYPE_INVALID,
						NULL,
						NULL,
						TRUE);
		gtk_widget_show (toplevel);
	}
	else
		gtk_widget_destroy (toplevel);
}

static void
brasero_project_manager_burn_iso_dialog (BraseroProjectManager *manager,
					 const gchar *uri)
{
	BraseroBurnSession *session;
	GtkResponseType result;
	GtkWidget *toplevel;
	GtkWidget *dialog;

	/* setup, show, and run options dialog */
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (manager));

	dialog = brasero_image_option_dialog_new ();
	brasero_image_option_dialog_set_image_uri (BRASERO_IMAGE_OPTION_DIALOG (dialog), uri);

	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_widget_show (dialog);

	result = gtk_dialog_run (GTK_DIALOG (dialog));
	if (result != GTK_RESPONSE_OK) {
		gtk_widget_destroy (dialog);
		return;
	}

	session = brasero_image_option_dialog_get_session (BRASERO_IMAGE_OPTION_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	if (!session)
		return;

	brasero_project_manager_burn (manager, session);
	g_object_unref (session);
}

static void
brasero_project_manager_burn_disc (BraseroProjectManager *manager)
{
	BraseroBurnSession *session;
	GtkResponseType result;
	GtkWidget *toplevel;
	GtkWidget *dialog;

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

	session = brasero_disc_copy_dialog_get_session (BRASERO_DISC_COPY_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	brasero_project_manager_burn (manager, session);
	g_object_unref (session);
}

static void
brasero_project_manager_switch (BraseroProjectManager *manager,
				BraseroProjectType type,
				GSList *uris,
				const gchar *uri,
				gboolean reset)
{
	GtkWidget *toplevel;
	GtkAction *action;

	if ((manager->priv->type == BRASERO_PROJECT_TYPE_AUDIO
	||   manager->priv->type == BRASERO_PROJECT_TYPE_DATA)
	&&  !brasero_project_confirm_switch (BRASERO_PROJECT (manager->priv->project)))
		return;

	gtk_statusbar_pop (GTK_STATUSBAR (manager->priv->status),
			   manager->priv->status_ctx);

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (manager));
	action = gtk_action_group_get_action (manager->priv->action_group, "NewChoose");

	manager->priv->type = type;

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

		if (reset) {
			/* tell the BraseroProject object that we want an audio selection */
			brasero_project_set_audio (BRASERO_PROJECT (manager->priv->project), uris);
		}

		if (toplevel)
			gtk_window_set_title (GTK_WINDOW (toplevel), _("Brasero - New audio disc project"));
	}
	else if (type == BRASERO_PROJECT_TYPE_DATA) {
		brasero_layout_load (BRASERO_LAYOUT (manager->priv->layout), BRASERO_LAYOUT_DATA);

		gtk_notebook_set_current_page (GTK_NOTEBOOK (manager), 1);
		gtk_action_set_sensitive (action, TRUE);

		if (reset) {
			/* tell the BraseroProject object that we want a data selection */
			brasero_project_set_data (BRASERO_PROJECT (manager->priv->project), uris);
		}

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
		brasero_project_manager_burn_iso_dialog (manager, uri);
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
	brasero_project_manager_switch (manager, type, NULL, NULL, TRUE);
}

static void
brasero_project_manager_new_empty_prj_cb (GtkAction *action, BraseroProjectManager *manager)
{
	brasero_project_manager_switch (manager, BRASERO_PROJECT_TYPE_INVALID, NULL, NULL, TRUE);
}

static void
brasero_project_manager_new_audio_prj_cb (GtkAction *action, BraseroProjectManager *manager)
{
	brasero_project_manager_switch (manager, BRASERO_PROJECT_TYPE_AUDIO, NULL, NULL, TRUE);
}

static void
brasero_project_manager_new_data_prj_cb (GtkAction *action, BraseroProjectManager *manager)
{
	brasero_project_manager_switch (manager, BRASERO_PROJECT_TYPE_DATA, NULL, NULL, TRUE);
}

static void
brasero_project_manager_new_copy_prj_cb (GtkAction *action, BraseroProjectManager *manager)
{
	brasero_project_manager_switch (manager, BRASERO_PROJECT_TYPE_COPY, NULL, NULL, TRUE);
}

static void
brasero_project_manager_new_iso_prj_cb (GtkAction *action, BraseroProjectManager *manager)
{
	brasero_project_manager_switch (manager, BRASERO_PROJECT_TYPE_ISO, NULL, NULL, TRUE);
}

static void
brasero_project_manager_open_cb (GtkAction *action, BraseroProjectManager *manager)
{
	BraseroProjectType type;

	type = brasero_project_open_project (BRASERO_PROJECT (manager->priv->project), NULL);
	if (type == BRASERO_PROJECT_TYPE_INVALID)
		return;

	manager->priv->type = type;

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
					NULL,
					TRUE);
}

void
brasero_project_manager_data (BraseroProjectManager *manager, GSList *uris)
{
	brasero_project_manager_switch (manager,
					BRASERO_PROJECT_TYPE_DATA,
					uris,
					NULL,
					TRUE);
}

void
brasero_project_manager_copy (BraseroProjectManager *manager)
{
	brasero_project_manager_switch (manager, BRASERO_PROJECT_TYPE_COPY, NULL, NULL, TRUE);
}

void
brasero_project_manager_iso (BraseroProjectManager *manager, const gchar *uri)
{
	brasero_project_manager_switch (manager, BRASERO_PROJECT_TYPE_ISO, NULL, uri, TRUE);
}

static void
brasero_project_manager_uri_clicked_cb (BraseroProjectTypeChooser *chooser,
					const gchar *uri,
					BraseroProjectManager *manager)
{
	brasero_project_manager_switch (manager, BRASERO_PROJECT_TYPE_ISO, NULL, uri, TRUE);
}

void
brasero_project_manager_open (BraseroProjectManager *manager, const gchar *uri)
{
	BraseroProjectType type;
	GtkAction *action;

    	gtk_widget_show (manager->priv->layout);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (manager), 1);
	type = brasero_project_open_project (BRASERO_PROJECT (manager->priv->project), uri);

	manager->priv->type = type;

    	if (type == BRASERO_PROJECT_TYPE_INVALID) {
		brasero_project_manager_switch (manager, BRASERO_PROJECT_TYPE_INVALID, NULL, NULL, TRUE);
		return;
	}

	if (type == BRASERO_PROJECT_TYPE_DATA)
		brasero_layout_load (BRASERO_LAYOUT (manager->priv->layout), BRASERO_LAYOUT_DATA);
	else
		brasero_layout_load (BRASERO_LAYOUT (manager->priv->layout), BRASERO_LAYOUT_AUDIO);

	action = gtk_action_group_get_action (manager->priv->action_group, "NewChoose");
	gtk_action_set_sensitive (action, TRUE);
}

static void
brasero_project_manager_project_clicked_cb (BraseroProjectTypeChooser *chooser,
					    const gchar *uri,
					    BraseroProjectManager *manager)
{
	brasero_project_manager_open (manager, uri);
}

void
brasero_project_manager_empty (BraseroProjectManager *manager)
{
	brasero_project_manager_switch (manager, BRASERO_PROJECT_TYPE_INVALID, NULL, NULL, TRUE);
}

gboolean
brasero_project_manager_load_session (BraseroProjectManager *manager,
				      const gchar *path)
{
    	if (path) {
		gchar *uri;
		BraseroProjectType type;

		uri = gnome_vfs_make_uri_from_input (path);
    		type = brasero_project_load_session (BRASERO_PROJECT (manager->priv->project),
						     uri);
		g_free (uri);

		brasero_project_manager_switch (manager, type, NULL, NULL, FALSE);
	}

    	return TRUE;
}

gboolean
brasero_project_manager_save_session (BraseroProjectManager *manager,
				      const gchar *path,
				      gboolean cancellable)
{
    	gboolean result = FALSE;

    	if (path) {
		gchar *uri;

		/* if we want to save the current open project, this need a
		 * modification in BraseroProject to bypass ask_status in case
	 	 * DataDisc has not finished exploration */
		uri = gnome_vfs_make_uri_from_input (path);
    		result = brasero_project_save_session (BRASERO_PROJECT (manager->priv->project),
						       uri,
						       cancellable);
		g_free (uri);
	}

    	return result;
}

static void
brasero_project_manager_init (BraseroProjectManager *obj)
{
	GtkWidget *type;
	GtkAction *action;
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

	action = gtk_action_group_get_action (obj->priv->action_group, "NewChoose");
	g_object_set (action,
		      "short-label", _("New"), /* for toolbar buttons */
		      NULL);
	action = gtk_action_group_get_action (obj->priv->action_group, "Open");
	g_object_set (action,
		      "short-label", _("Open"), /* for toolbar buttons */
		      NULL);

	/* add the project type chooser to the notebook */
	type = brasero_project_type_chooser_new ();
	gtk_widget_show (type);
	g_signal_connect (type,
			  "chosen",
			  G_CALLBACK (brasero_project_manager_type_changed_cb),
			  obj);
	g_signal_connect (type,
			  "uri-clicked",
			  G_CALLBACK (brasero_project_manager_uri_clicked_cb),
			  obj);
	g_signal_connect (type,
			  "project-clicked",
			  G_CALLBACK (brasero_project_manager_project_clicked_cb),
			  obj);
	gtk_notebook_prepend_page (GTK_NOTEBOOK (obj), type, NULL);

	/* add the layout */
	obj->priv->layout = brasero_layout_new ();
	gtk_widget_show (obj->priv->layout);
	gtk_notebook_append_page (GTK_NOTEBOOK (obj), obj->priv->layout, NULL);

	/* create the project for audio and data discs */
	obj->priv->project = brasero_project_new ();

#ifdef BUILD_PREVIEW

	GtkWidget *preview;

	preview = brasero_preview_new ();
	gtk_widget_show (preview);
	brasero_preview_add_source (BRASERO_PREVIEW (preview),
				    BRASERO_URI_CONTAINER (obj->priv->project));

#endif /* BUILD_PREVIEW */

	chooser = brasero_file_chooser_new ();
    	BRASERO_PROJECT_MANAGER_CONNECT_CHANGED (obj, chooser);

	gtk_widget_show_all (chooser);
	brasero_layout_add_source (BRASERO_LAYOUT (obj->priv->layout),
				   chooser,
				   "Chooser",
				   _("Browse the file system"),
				   _("Display file browser"),
				   GTK_STOCK_DIRECTORY,
				   BRASERO_LAYOUT_AUDIO|BRASERO_LAYOUT_DATA);

#ifdef BUILD_PREVIEW
	brasero_preview_add_source (BRASERO_PREVIEW (preview),
				    BRASERO_URI_CONTAINER (chooser));
#endif

	brasero_layout_add_project (BRASERO_LAYOUT (obj->priv->layout),
				    obj->priv->project);
	gtk_widget_show_all (obj->priv->project);

#ifdef BUILD_SEARCH
	GtkWidget *search;

	search = brasero_search_new ();
    	BRASERO_PROJECT_MANAGER_CONNECT_CHANGED (obj, search);

	gtk_widget_show_all (search);
	brasero_layout_add_source (BRASERO_LAYOUT (obj->priv->layout),
				   search,
				   "Search",
				   _("Search files using keywords"),
				   _("Display search"),
				   GTK_STOCK_FIND,
				   BRASERO_LAYOUT_AUDIO|BRASERO_LAYOUT_DATA);

#ifdef BUILD_PREVIEW
	brasero_preview_add_source (BRASERO_PREVIEW (preview),
				    BRASERO_URI_CONTAINER (search));
#endif

#endif /* BUILD_SEARCH */

#ifdef BUILD_PLAYLIST
	GtkWidget *playlist;

	playlist = brasero_playlist_new ();
    	BRASERO_PROJECT_MANAGER_CONNECT_CHANGED (obj, playlist);
	gtk_widget_show_all (playlist);
	brasero_layout_add_source (BRASERO_LAYOUT (obj->priv->layout),
				   playlist,
				   "Playlist",
				   _("Display playlists and their contents"),
				   _("Display playlists"),
				   "audio-x-generic", 
				   BRASERO_LAYOUT_AUDIO);

#ifdef BUILD_PREVIEW
	brasero_preview_add_source (BRASERO_PREVIEW (preview),
				    BRASERO_URI_CONTAINER (playlist));
#endif

#endif /* BUILD_PLAYLIST */

#ifdef BUILD_PREVIEW
	brasero_layout_add_preview (BRASERO_LAYOUT (obj->priv->layout),
				    preview);
#endif
	
}

static void
brasero_project_manager_finalize (GObject *object)
{
	BraseroProjectManager *cobj;

	cobj = BRASERO_PROJECT_MANAGER (object);

	if (cobj->priv->vfs) {
		brasero_vfs_cancel (cobj->priv->vfs, object);
		g_object_unref (cobj->priv->vfs);
		cobj->priv->vfs = NULL;
	}

	g_free (cobj->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_project_manager_class_init (BraseroProjectManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_project_manager_finalize;
}

GtkWidget *
brasero_project_manager_new ()
{
	BraseroProjectManager *obj;
	
	obj = BRASERO_PROJECT_MANAGER (g_object_new (BRASERO_TYPE_PROJECT_MANAGER, NULL));
	
	return GTK_WIDGET (obj);
}
