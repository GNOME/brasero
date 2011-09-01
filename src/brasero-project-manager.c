/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/***************************************************************************
 *            brasero-project-manager.c
 *
 *  mer mai 24 14:22:56 2006
 *  Copyright  2006  Rouquier Philippe
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
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtk.h>

#include "brasero-io.h"

#include "brasero-units.h"
#include "brasero-medium-monitor.h"

#include "brasero-app.h"
#include "brasero-utils.h"
#include "brasero-project.h"
#include "brasero-layout.h"
#include "brasero-project-manager.h"
#include "brasero-file-chooser.h"
#include "brasero-uri-container.h"
#include "brasero-project-type-chooser.h"

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
brasero_project_manager_new_cover_cb (GtkAction *action, BraseroProjectManager *manager);
static void
brasero_project_manager_new_empty_prj_cb (GtkAction *action, BraseroProjectManager *manager);
static void
brasero_project_manager_new_audio_prj_cb (GtkAction *action, BraseroProjectManager *manager);
static void
brasero_project_manager_new_data_prj_cb (GtkAction *action, BraseroProjectManager *manager);
static void
brasero_project_manager_new_video_prj_cb (GtkAction *action, BraseroProjectManager *manager);
static void
brasero_project_manager_new_copy_prj_cb (GtkAction *action, BraseroProjectManager *manager);
static void
brasero_project_manager_new_iso_prj_cb (GtkAction *action, BraseroProjectManager *manager);
static void
brasero_project_manager_open_cb (GtkAction *action, BraseroProjectManager *manager);

static void
brasero_project_manager_selected_uris_changed (BraseroURIContainer *container,
					       BraseroProjectManager *manager);

/* menus */
static GtkActionEntry entries [] = {
	{"Cover", NULL, N_("_Cover Editor"), NULL,
	 N_("Design and print covers for CDs"), G_CALLBACK (brasero_project_manager_new_cover_cb)},
	 {"New", GTK_STOCK_NEW, N_("_New Project"), NULL,
	 N_("Create a new project"), NULL },
	{"NewChoose", GTK_STOCK_NEW, N_("_Empty Project"), NULL,
	 N_("Let you choose your new project"), G_CALLBACK (brasero_project_manager_new_empty_prj_cb)},
	{"NewAudio", "media-optical-audio-new", N_("New _Audio Project"), NULL,
	 N_("Create a traditional audio CD that will be playable on computers and stereos"), G_CALLBACK (brasero_project_manager_new_audio_prj_cb)},
	{"NewData", "media-optical-data-new", N_("New _Data Project"), NULL,
	 N_("Create a CD/DVD containing any type of data that can only be read on a computer"), G_CALLBACK (brasero_project_manager_new_data_prj_cb)},
	{"NewVideo", "media-optical-video-new", N_("New _Video Project"), NULL,
	 N_("Create a video DVD or an SVCD that is readable on TV readers"), G_CALLBACK (brasero_project_manager_new_video_prj_cb)},
	{"NewCopy", "media-optical-copy", N_("Copy _Disc…"), NULL,
	 N_("Create a 1:1 copy of an audio CD or a data CD/DVD on your hard disk or on another CD/DVD"), G_CALLBACK (brasero_project_manager_new_copy_prj_cb)},
	{"NewIso", "iso-image-burn", N_("_Burn Image…"), NULL,
	 N_("Burn an existing CD/DVD image to disc"), G_CALLBACK (brasero_project_manager_new_iso_prj_cb)},

	{"Open", GTK_STOCK_OPEN, N_("_Open…"), NULL,
	 N_("Open a project"), G_CALLBACK (brasero_project_manager_open_cb)},
};

static const char *description = {
	"<ui>"
	    "<menubar name='menubar' >"
		"<menu action='ProjectMenu'>"
			"<placeholder name='ProjectPlaceholder'>"
				"<menu action='New'>"
					"<menuitem action='NewAudio'/>"
					"<menuitem action='NewData'/>"
					"<menuitem action='NewVideo'/>"
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

		"<menu action='ToolMenu'>"

			"<placeholder name='DiscPlaceholder'>"
			    "<separator/>"
			    "<menuitem action='Cover'/>"
			    "<separator/>"
			"</placeholder>"

		"</menu>"
	    "</menubar>"
	"</ui>"
};

struct BraseroProjectManagerPrivate {
	BraseroProjectType type;
	BraseroIOJobBase *size_preview;

	GtkWidget *project;
	GtkWidget *layout;

	gchar **selected;
	guint preview_id;

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
brasero_project_manager_new_cover_cb (GtkAction *action,
				      BraseroProjectManager *manager)
{
	BraseroJacketEdit *edit;
	GtkWidget *toplevel;
	GtkWidget *dialog;

	if (manager->priv->type == BRASERO_PROJECT_TYPE_AUDIO) {
		brasero_project_create_audio_cover (BRASERO_PROJECT (manager->priv->project));
		return;
	}

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (manager));
	dialog = brasero_jacket_edit_dialog_new (toplevel, &edit);

	/* This strange hack is a way to workaround #568358.
	 * At one point we'll need to hide the dialog which means it
	 * will anwer with a GTK_RESPONSE_NONE */
	while (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_NONE)
		gtk_widget_show (GTK_WIDGET (dialog));

	gtk_widget_destroy (dialog);
}

static void
brasero_project_manager_set_statusbar (BraseroProjectManager *manager,
				       guint64 files_size,
				       gint invalid_num,
				       gint files_num)
{
	gchar *status_string = NULL;
	GtkWidget *status;
	gint valid_num;

	status = brasero_app_get_statusbar1 (brasero_app_get_default ());

	if (!manager->priv->status_ctx)
		manager->priv->status_ctx = gtk_statusbar_get_context_id (GTK_STATUSBAR (status),
									  "size_info");

	gtk_statusbar_pop (GTK_STATUSBAR (status), manager->priv->status_ctx);

	valid_num = files_num - invalid_num;
	if (!invalid_num && valid_num) {
		gchar *size_string;

		if (manager->priv->type == BRASERO_PROJECT_TYPE_AUDIO
		||  manager->priv->type == BRASERO_PROJECT_TYPE_VIDEO)
			size_string = brasero_units_get_time_string (files_size, TRUE, FALSE);
		else if (manager->priv->type == BRASERO_PROJECT_TYPE_DATA)
			size_string = g_format_size (files_size);
		else
			return;

		/* Translators: the %s is a string representing the total size
		 * of the file selection */
		status_string = g_strdup_printf (ngettext ("%d file selected (%s)", "%d files selected (%s)", files_num),
						 files_num,
						 size_string);
		g_free (size_string);
	}
	else if (valid_num) {
		gchar *size_string = NULL;

		if (manager->priv->type == BRASERO_PROJECT_TYPE_AUDIO
		||  manager->priv->type == BRASERO_PROJECT_TYPE_VIDEO) {
			size_string = brasero_units_get_time_string (files_size, TRUE, FALSE);
			status_string = g_strdup_printf (ngettext ("%d file is supported (%s)", "%d files are supported (%s)", valid_num),
							 valid_num,
							 size_string);
		}
		else if (manager->priv->type == BRASERO_PROJECT_TYPE_DATA) {
			size_string = g_format_size (files_size);
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
		else if (manager->priv->type == BRASERO_PROJECT_TYPE_AUDIO
		     ||  manager->priv->type == BRASERO_PROJECT_TYPE_VIDEO)
			status_string = g_strdup_printf (ngettext ("No file is supported (%i selected file)",
								   "No file is supported (%i selected files)",
								   files_num),
							 files_num);
	}
	else
		status_string = g_strdup (_("No file selected"));

	gtk_statusbar_push (GTK_STATUSBAR (status), manager->priv->status_ctx,
			    status_string);
	g_free (status_string);
}

static void
brasero_project_manager_size_preview (GObject *object,
				      GError *error,
				      const gchar *uri,
				      GFileInfo *info,
				      gpointer user_data)
{
	BraseroProjectManager *manager = BRASERO_PROJECT_MANAGER (object);
	guint64 files_size;
	gint invalid_num;
	gint files_num;

	invalid_num = g_file_info_get_attribute_uint32 (info, BRASERO_IO_COUNT_INVALID);
	files_size = g_file_info_get_attribute_uint64 (info, BRASERO_IO_COUNT_SIZE);
	files_num = g_file_info_get_attribute_uint32 (info, BRASERO_IO_COUNT_NUM);
	brasero_project_manager_set_statusbar (manager,
					       files_size,
					       invalid_num,
					       files_num);
}
/*
static void
brasero_project_manager_size_preview_progress (GObject *object,
					       BraseroIOJobProgress *progress,
					       gpointer user_data)
{
	BraseroProjectManager *manager = BRASERO_PROJECT_MANAGER (object);
	guint64 files_size;
	gint files_num;

	files_size = brasero_io_job_progress_get_total (progress);
	files_num = brasero_io_job_progress_get_file_processed (progress);
	brasero_project_manager_set_statusbar (manager,
					       files_size,
					       0,
					       files_num);
}
*/
static gboolean
brasero_project_manager_selected_uris_preview (gpointer data)
{
	BraseroProjectManager *manager = BRASERO_PROJECT_MANAGER (data);
	BraseroIOFlags flags;
	GSList *list = NULL;
	gchar **iter;

	if (!manager->priv->size_preview)
		manager->priv->size_preview = brasero_io_register (G_OBJECT (manager),
								   brasero_project_manager_size_preview,
								   NULL,
								   NULL);

	for (iter = manager->priv->selected; iter && *iter; iter ++)
		list = g_slist_prepend (list, *iter);

	flags = BRASERO_IO_INFO_IDLE;
	if (manager->priv->type == BRASERO_PROJECT_TYPE_AUDIO
	||  manager->priv->type == BRASERO_PROJECT_TYPE_VIDEO)
		flags |= BRASERO_IO_INFO_METADATA;
	else if (manager->priv->type == BRASERO_PROJECT_TYPE_DATA)
		flags |= BRASERO_IO_INFO_RECURSIVE;

	brasero_io_get_file_count (list,
				   manager->priv->size_preview,
				   flags,
				   NULL);
			       
	g_slist_free (list);
	manager->priv->preview_id = 0;
	return FALSE;
}

void
brasero_project_manager_selected_uris_changed (BraseroURIContainer *container,
					       BraseroProjectManager *manager)
{
	gchar **uris;

	/* Before cancelling everything, check that the size really changed
	 * like in the case of double clicking or if the user selected one
	 * file and double clicked on it afterwards.
	 * NOTE: the following expects each URI to be unique. */
	uris = brasero_uri_container_get_selected_uris (container);
	if (uris) {
		gchar **iter;
		guint num = 0;
		gboolean found = FALSE;

		for (iter = manager->priv->selected; iter && *iter; iter ++) {
			gchar **uri;

			found = FALSE;
			for (uri = uris; uri && *uri; uri ++) {
				if (!strcmp (*uri, *iter)) {
					found = TRUE;
					break;
				}
			}

			if (!found)
				break;

			num ++;
		}

		if (found) {
			guint num_new = 0;

			for (iter = uris; iter && *iter; iter ++)
				num_new ++;

			if (num_new == num) {
				/* same uris no need to update anything. */
				g_strfreev (uris);
				return;
			}
		}
	}

	/* if we are in the middle of an unfinished size seek then
	 * cancel it and re-initialize */
	if (manager->priv->size_preview)
		brasero_io_cancel_by_base (manager->priv->size_preview);

	if (manager->priv->selected) {
		g_strfreev (manager->priv->selected);
		manager->priv->selected = NULL;
	}

	if (manager->priv->preview_id) {
		g_source_remove (manager->priv->preview_id);
		manager->priv->preview_id = 0;
	}

	manager->priv->selected = uris;
	if (!manager->priv->selected) {
 		GtkWidget *status;
 
 		status = brasero_app_get_statusbar1 (brasero_app_get_default ());
 
 		if (!manager->priv->status_ctx)
 			manager->priv->status_ctx = gtk_statusbar_get_context_id (GTK_STATUSBAR (status),
 										  "size_info");
 
 		gtk_statusbar_pop (GTK_STATUSBAR (status), manager->priv->status_ctx);
 		gtk_statusbar_push (GTK_STATUSBAR (status),
  				    manager->priv->status_ctx,
  				    _("No file selected"));
		return;
	}

	manager->priv->preview_id = g_timeout_add (500,
						   brasero_project_manager_selected_uris_preview,
						   manager);
}

static void
brasero_project_manager_sidepane_changed (BraseroLayout *layout,
					  gboolean visible,
					  BraseroProjectManager *manager)
{
	if (!visible) {
 		GtkWidget *status;
 
		/* If sidepane is disabled, remove any text about selection */
		if (manager->priv->size_preview)
			brasero_io_cancel_by_base (manager->priv->size_preview);

 		status = brasero_app_get_statusbar1 (brasero_app_get_default ());

 		gtk_statusbar_pop (GTK_STATUSBAR (status), manager->priv->status_ctx);

		if (manager->priv->selected) {
			g_strfreev (manager->priv->selected);
			manager->priv->selected = NULL;
		}

		if (manager->priv->preview_id) {
			g_source_remove (manager->priv->preview_id);
			manager->priv->preview_id = 0;
		}
	}
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

void
brasero_project_manager_switch (BraseroProjectManager *manager,
				BraseroProjectType type,
				gboolean reset)
{
	GtkWidget *toplevel;
	GtkAction *action;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (manager));

	if (manager->priv->type == BRASERO_PROJECT_TYPE_AUDIO
	||  manager->priv->type == BRASERO_PROJECT_TYPE_DATA
	||  manager->priv->type == BRASERO_PROJECT_TYPE_VIDEO) {
		BraseroBurnResult result;
		gboolean keep_files = FALSE;

		if (type == BRASERO_PROJECT_TYPE_AUDIO
		||  type == BRASERO_PROJECT_TYPE_DATA
		||  type == BRASERO_PROJECT_TYPE_VIDEO) {
			if (manager->priv->type != type
			&& (((manager->priv->type == BRASERO_PROJECT_TYPE_AUDIO ||
			      manager->priv->type == BRASERO_PROJECT_TYPE_VIDEO) &&
			     type == BRASERO_PROJECT_TYPE_DATA)
			||  manager->priv->type == BRASERO_PROJECT_TYPE_DATA))
				keep_files = TRUE;
		}

		result = brasero_project_confirm_switch (BRASERO_PROJECT (manager->priv->project), keep_files);
		if (result == BRASERO_BURN_CANCEL)
			return;

		if (result == BRASERO_BURN_RETRY) {
			if (manager->priv->type == BRASERO_PROJECT_TYPE_AUDIO
			||  manager->priv->type == BRASERO_PROJECT_TYPE_VIDEO)
				type = brasero_project_convert_to_data (BRASERO_PROJECT (manager->priv->project));
			else if (manager->priv->type == BRASERO_PROJECT_TYPE_DATA) {
				BraseroProjectType new_type;

				/* Keep type untouched */
				new_type = brasero_project_convert_to_stream (BRASERO_PROJECT (manager->priv->project),
									      type == BRASERO_PROJECT_TYPE_VIDEO);
				if (new_type == BRASERO_PROJECT_TYPE_INVALID)
					type = new_type;
			}

			if (type != BRASERO_PROJECT_TYPE_INVALID)
				reset = FALSE;
			else
				reset = TRUE;
		}
	}

	if (manager->priv->status_ctx) {
		GtkWidget *status;

		status = brasero_app_get_statusbar1 (brasero_app_get_default ());
		gtk_statusbar_pop (GTK_STATUSBAR (status), manager->priv->status_ctx);
	}

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
			brasero_project_set_audio (BRASERO_PROJECT (manager->priv->project));
		}

		if (toplevel)
			gtk_window_set_title (GTK_WINDOW (toplevel), _("Brasero — New Audio Disc Project"));
	}
	else if (type == BRASERO_PROJECT_TYPE_DATA) {
		brasero_layout_load (BRASERO_LAYOUT (manager->priv->layout), BRASERO_LAYOUT_DATA);

		gtk_notebook_set_current_page (GTK_NOTEBOOK (manager), 1);
		gtk_action_set_sensitive (action, TRUE);

		if (reset) {
			/* tell the BraseroProject object that we want a data selection */
			brasero_project_set_data (BRASERO_PROJECT (manager->priv->project));
		}

		if (toplevel)
			gtk_window_set_title (GTK_WINDOW (toplevel), _("Brasero — New Data Disc Project"));
	}
	else if (type == BRASERO_PROJECT_TYPE_VIDEO) {
		brasero_layout_load (BRASERO_LAYOUT (manager->priv->layout), BRASERO_LAYOUT_VIDEO);

		gtk_notebook_set_current_page (GTK_NOTEBOOK (manager), 1);
		gtk_action_set_sensitive (action, TRUE);

		if (reset) {
			/* tell the BraseroProject object that we want a data selection */
			brasero_project_set_video (BRASERO_PROJECT (manager->priv->project));
		}

		if (toplevel)
			gtk_window_set_title (GTK_WINDOW (toplevel), _("Brasero — New Video Disc Project"));
	}
	else if (type == BRASERO_PROJECT_TYPE_ISO) {
		brasero_layout_load (BRASERO_LAYOUT (manager->priv->layout), BRASERO_LAYOUT_NONE);
		brasero_project_set_none (BRASERO_PROJECT (manager->priv->project));

		gtk_notebook_set_current_page (GTK_NOTEBOOK (manager), 0);
		gtk_action_set_sensitive (action, FALSE);

		if (toplevel)
			gtk_window_set_title (GTK_WINDOW (toplevel), _("Brasero — New Image File"));

		brasero_project_manager_switch (manager,
						BRASERO_PROJECT_TYPE_INVALID,
						TRUE);
		brasero_app_image (brasero_app_get_default (), NULL, NULL, FALSE);
	}
	else if (type == BRASERO_PROJECT_TYPE_COPY) {
		brasero_layout_load (BRASERO_LAYOUT (manager->priv->layout), BRASERO_LAYOUT_NONE);
		brasero_project_set_none (BRASERO_PROJECT (manager->priv->project));

		gtk_notebook_set_current_page (GTK_NOTEBOOK (manager), 0);
		gtk_action_set_sensitive (action, FALSE);

		if (toplevel)
			gtk_window_set_title (GTK_WINDOW (toplevel), _("Brasero — Disc Copy"));

		brasero_project_manager_switch (manager,
						BRASERO_PROJECT_TYPE_INVALID,
						TRUE);
		brasero_app_copy_disc (brasero_app_get_default (), NULL, NULL, NULL, FALSE);
	}
}

static void
brasero_project_manager_type_changed_cb (BraseroProjectTypeChooser *chooser,
					 BraseroProjectType type,
					 BraseroProjectManager *manager)
{
	brasero_project_manager_switch (manager, type, TRUE);
}

static void
brasero_project_manager_new_empty_prj_cb (GtkAction *action, BraseroProjectManager *manager)
{
	brasero_project_manager_switch (manager, BRASERO_PROJECT_TYPE_INVALID, TRUE);
}

static void
brasero_project_manager_new_audio_prj_cb (GtkAction *action, BraseroProjectManager *manager)
{
	brasero_project_manager_switch (manager, BRASERO_PROJECT_TYPE_AUDIO, TRUE);
}

static void
brasero_project_manager_new_data_prj_cb (GtkAction *action, BraseroProjectManager *manager)
{
	brasero_project_manager_switch (manager, BRASERO_PROJECT_TYPE_DATA, TRUE);
}

static void
brasero_project_manager_new_video_prj_cb (GtkAction *action, BraseroProjectManager *manager)
{
	brasero_project_manager_switch (manager, BRASERO_PROJECT_TYPE_VIDEO, TRUE);
}

static void
brasero_project_manager_new_copy_prj_cb (GtkAction *action, BraseroProjectManager *manager)
{
	brasero_project_manager_switch (manager, BRASERO_PROJECT_TYPE_COPY, TRUE);
}

static void
brasero_project_manager_new_iso_prj_cb (GtkAction *action, BraseroProjectManager *manager)
{
	brasero_project_manager_switch (manager, BRASERO_PROJECT_TYPE_ISO, TRUE);
}

gboolean
brasero_project_manager_open_session (BraseroProjectManager *manager,
                                      BraseroSessionCfg *session)
{
	GtkAction *action;
	BraseroProjectType type;

	type = brasero_project_open_session (BRASERO_PROJECT (manager->priv->project), session);
	if (type == BRASERO_PROJECT_TYPE_INVALID) {
		brasero_project_manager_switch (manager, BRASERO_PROJECT_TYPE_INVALID, TRUE);
		return FALSE;
	}

	brasero_project_manager_switch (manager,
					type,
					FALSE);

	action = gtk_action_group_get_action (manager->priv->action_group, "NewChoose");
	gtk_action_set_sensitive (action, TRUE);
	return TRUE;
}

static void
brasero_project_manager_open_cb (GtkAction *action, BraseroProjectManager *manager)
{
	gchar *uri;
	gint answer;
	GtkWidget *chooser;
	GtkWidget *toplevel;

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (manager));
	chooser = gtk_file_chooser_dialog_new (_("Open Project"),
					      GTK_WINDOW (toplevel),
					      GTK_FILE_CHOOSER_ACTION_OPEN,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OPEN, GTK_RESPONSE_OK,
					      NULL);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), TRUE);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser),
					     g_get_home_dir ());
	
	gtk_widget_show (chooser);
	answer = gtk_dialog_run (GTK_DIALOG (chooser));
	if (answer != GTK_RESPONSE_OK) {
		gtk_widget_destroy (chooser);
		return;
	}

	uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (chooser));
	gtk_widget_destroy (chooser);

	brasero_app_open_uri (brasero_app_get_default (), uri, TRUE);
	g_free (uri);
}

static void
brasero_project_manager_recent_clicked_cb (BraseroProjectTypeChooser *chooser,
					   const gchar *uri,
					   BraseroProjectManager *manager)
{
	brasero_app_open_uri (brasero_app_get_default (), uri, TRUE);
}

void
brasero_project_manager_empty (BraseroProjectManager *manager)
{
	brasero_project_manager_switch (manager, BRASERO_PROJECT_TYPE_INVALID, TRUE);
}

static void
brasero_project_manager_last_saved_clicked_cb (BraseroProjectTypeChooser *chooser,
					       const gchar *path,
					       BraseroProjectManager *manager)
{
	if (path) {
		gchar *uri;

		uri = g_filename_to_uri (path, NULL, NULL);
		brasero_app_open_project (brasero_app_get_default (),
					  NULL,
		                          uri,
		                          FALSE, // not a playlist
		                          FALSE, // should work so don't warn user
		                          FALSE); // don't burn right away
		g_free (uri);
	}
}

gboolean
brasero_project_manager_save_session (BraseroProjectManager *manager,
				      const gchar *path,
				      gchar **saved_uri,
				      gboolean cancellable)
{
    	gboolean result = FALSE;

    	if (path) {
		gchar *uri;

		/* if we want to save the current open project, this need a
		 * modification in BraseroProject to bypass ask_status in case
	 	 * DataDisc has not finished exploration */
		uri = g_filename_to_uri (path, NULL, NULL);
    		result = brasero_project_save_session (BRASERO_PROJECT (manager->priv->project),
						       uri,
						       saved_uri,
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
		      "short-label", _("_New"), /* for toolbar buttons */
		      NULL);
	action = gtk_action_group_get_action (obj->priv->action_group, "Open");
	g_object_set (action,
		      "short-label", _("_Open"), /* for toolbar buttons */
		      NULL);

	/* add the project type chooser to the notebook */
	type = brasero_project_type_chooser_new ();
	gtk_widget_show (type);
	g_signal_connect (type,
			  "chosen",
			  G_CALLBACK (brasero_project_manager_type_changed_cb),
			  obj);
	g_signal_connect (type,
			  "recent-clicked",
			  G_CALLBACK (brasero_project_manager_recent_clicked_cb),
			  obj);
	g_signal_connect (type,
			  "last-saved",
			  G_CALLBACK (brasero_project_manager_last_saved_clicked_cb),
			  obj);
	gtk_notebook_prepend_page (GTK_NOTEBOOK (obj), type, NULL);

	/* add the layout */
	obj->priv->layout = brasero_layout_new ();
	gtk_widget_show (obj->priv->layout);
	gtk_notebook_append_page (GTK_NOTEBOOK (obj), obj->priv->layout, NULL);

	g_signal_connect (obj->priv->layout,
			  "sidepane",
			  G_CALLBACK (brasero_project_manager_sidepane_changed),
			  obj);

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
				   GTK_STOCK_DIRECTORY,
				   BRASERO_LAYOUT_AUDIO|BRASERO_LAYOUT_DATA|BRASERO_LAYOUT_VIDEO);

#ifdef BUILD_PREVIEW
	brasero_preview_add_source (BRASERO_PREVIEW (preview),
				    BRASERO_URI_CONTAINER (chooser));
#endif

	brasero_layout_add_project (BRASERO_LAYOUT (obj->priv->layout),
				    obj->priv->project);
	gtk_widget_show (obj->priv->project);

#ifdef BUILD_PLAYLIST
	GtkWidget *playlist;

	playlist = brasero_playlist_new ();
    	BRASERO_PROJECT_MANAGER_CONNECT_CHANGED (obj, playlist);
	gtk_widget_show_all (playlist);
	brasero_layout_add_source (BRASERO_LAYOUT (obj->priv->layout),
				   playlist,
				   "Playlist",
				   _("Display playlists and their contents"),
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

	if (cobj->priv->size_preview) {
		brasero_io_cancel_by_base (cobj->priv->size_preview);
		brasero_io_job_base_free (cobj->priv->size_preview);
		cobj->priv->size_preview = NULL;
	}

	if (cobj->priv->preview_id) {
		g_source_remove (cobj->priv->preview_id);
		cobj->priv->preview_id = 0;
	}

	if (cobj->priv->selected) {
		g_strfreev (cobj->priv->selected);
		cobj->priv->selected = NULL;
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
