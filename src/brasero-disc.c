/***************************************************************************
 *            disc.c
 *
 *  dim nov 27 14:58:13 2005
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include <gtk/gtktoolbar.h>
#include <gtk/gtkuimanager.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtktreednd.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmisc.h>

#include "brasero-marshal.h"
#include "brasero-disc.h"
#include "burn-session.h"
 
static void brasero_disc_base_init (gpointer g_class);

typedef enum {
	SELECTION_CHANGED_SIGNAL,
	CONTENTS_CHANGED_SIGNAL,
	SIZE_CHANGED_SIGNAL,
	FLAGS_CHANGED_SIGNAL,
	LAST_SIGNAL
} BraseroDiscSignalType;

static guint brasero_disc_signals [LAST_SIGNAL] = { 0 };

GType
brasero_disc_get_type()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroDiscIface),
			brasero_disc_base_init,
			NULL,
			NULL,
			NULL,
			NULL,
			0,
			0,
			NULL
		};

		type = g_type_register_static (G_TYPE_INTERFACE, 
					       "BraseroDisc",
					       &our_info,
					       0);

		g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
	}

	return type;
}

static void
brasero_disc_base_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (initialized)
		return;

	brasero_disc_signals [CONTENTS_CHANGED_SIGNAL] =
	    g_signal_new ("contents_changed",
			  BRASERO_TYPE_DISC,
			  G_SIGNAL_RUN_LAST|G_SIGNAL_ACTION|G_SIGNAL_NO_RECURSE,
			  G_STRUCT_OFFSET (BraseroDiscIface, contents_changed),
			  NULL,
			  NULL,
			  g_cclosure_marshal_VOID__INT,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_INT);
	brasero_disc_signals [SELECTION_CHANGED_SIGNAL] =
	    g_signal_new ("selection_changed",
			  BRASERO_TYPE_DISC,
			  G_SIGNAL_RUN_LAST|G_SIGNAL_ACTION|G_SIGNAL_NO_RECURSE,
			  G_STRUCT_OFFSET (BraseroDiscIface, selection_changed),
			  NULL,
			  NULL,
			  g_cclosure_marshal_VOID__VOID,
			  G_TYPE_NONE,
			  0);
	brasero_disc_signals [SIZE_CHANGED_SIGNAL] =
	    g_signal_new ("size_changed",
			  BRASERO_TYPE_DISC,
			  G_SIGNAL_RUN_LAST|G_SIGNAL_ACTION|G_SIGNAL_NO_RECURSE,
			  G_STRUCT_OFFSET (BraseroDiscIface, size_changed),
			  NULL,
			  NULL,
			  brasero_marshal_VOID__INT64,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_INT64);
	brasero_disc_signals [FLAGS_CHANGED_SIGNAL] =
	    g_signal_new ("flags_changed",
			  BRASERO_TYPE_DISC,
			  G_SIGNAL_RUN_LAST|G_SIGNAL_ACTION|G_SIGNAL_NO_RECURSE,
			  G_STRUCT_OFFSET (BraseroDiscIface, flags_changed),
			  NULL,
			  NULL,
			  g_cclosure_marshal_VOID__INT,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_INT);
	initialized = TRUE;
}

BraseroDiscResult
brasero_disc_can_add_uri (BraseroDisc *disc,
			  const gchar *uri)
{
	BraseroDiscIface *iface;

	g_return_val_if_fail (BRASERO_IS_DISC (disc), BRASERO_DISC_ERROR_UNKNOWN);
	g_return_val_if_fail (uri != NULL, BRASERO_DISC_ERROR_UNKNOWN);
	
	iface = BRASERO_DISC_GET_IFACE (disc);
	if (iface->can_add_uri)
		return (* iface->can_add_uri) (disc, uri);

	/* default to OK */
	return BRASERO_DISC_OK;
}

BraseroDiscResult
brasero_disc_add_uri (BraseroDisc *disc,
		      const gchar *uri)
{
	BraseroDiscIface *iface;

	g_return_val_if_fail (BRASERO_IS_DISC (disc), BRASERO_DISC_ERROR_UNKNOWN);
	g_return_val_if_fail (uri != NULL, BRASERO_DISC_ERROR_UNKNOWN);
	
	iface = BRASERO_DISC_GET_IFACE (disc);
	if (iface->add_uri)
		return (* iface->add_uri) (disc, uri);

	return BRASERO_DISC_ERROR_UNKNOWN;
}

void
brasero_disc_delete_selected (BraseroDisc *disc)
{
	BraseroDiscIface *iface;

	g_return_if_fail (BRASERO_IS_DISC (disc));
	
	iface = BRASERO_DISC_GET_IFACE (disc);
	if (iface->delete_selected)
		(* iface->delete_selected) (disc);
}

void
brasero_disc_clear (BraseroDisc *disc)
{
	BraseroDiscIface *iface;

	g_return_if_fail (BRASERO_IS_DISC (disc));
	
	iface = BRASERO_DISC_GET_IFACE (disc);
	if (iface->clear)
		(* iface->clear) (disc);
}

void
brasero_disc_reset (BraseroDisc *disc)
{
	BraseroDiscIface *iface;

	g_return_if_fail (BRASERO_IS_DISC (disc));
	
	iface = BRASERO_DISC_GET_IFACE (disc);
	if (iface->reset)
		(* iface->reset) (disc);
}

BraseroDiscResult
brasero_disc_get_status (BraseroDisc *disc)
{
	BraseroDiscIface *iface;

	g_return_val_if_fail (BRASERO_IS_DISC (disc), BRASERO_DISC_ERROR_UNKNOWN);
	
	iface = BRASERO_DISC_GET_IFACE (disc);
	if (iface->get_status)
		return (* iface->get_status) (disc);

	return BRASERO_DISC_ERROR_UNKNOWN;
}

BraseroDiscResult
brasero_disc_get_track (BraseroDisc *disc,
			BraseroDiscTrack *track)
{
	BraseroDiscIface *iface;

	g_return_val_if_fail (BRASERO_IS_DISC (disc), BRASERO_DISC_ERROR_UNKNOWN);
	g_return_val_if_fail (track != NULL, BRASERO_DISC_ERROR_UNKNOWN);
	
	iface = BRASERO_DISC_GET_IFACE (disc);
	if (iface->get_track)
		return (* iface->get_track) (disc, track);

	return BRASERO_DISC_ERROR_UNKNOWN;
}

BraseroDiscResult
brasero_disc_set_session_param (BraseroDisc *disc,
				BraseroBurnSession *session)
{
	BraseroDiscIface *iface;

	g_return_val_if_fail (BRASERO_IS_DISC (disc), BRASERO_DISC_ERROR_UNKNOWN);
	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (session), BRASERO_DISC_ERROR_UNKNOWN);
	
	iface = BRASERO_DISC_GET_IFACE (disc);
	if (iface->set_session_param)
		return (* iface->set_session_param) (disc, session);

	return BRASERO_DISC_ERROR_UNKNOWN;
}

BraseroDiscResult
brasero_disc_set_session_contents (BraseroDisc *disc,
				   BraseroBurnSession *session)
{
	BraseroDiscIface *iface;

	g_return_val_if_fail (BRASERO_IS_DISC (disc), BRASERO_DISC_ERROR_UNKNOWN);
	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (session), BRASERO_DISC_ERROR_UNKNOWN);
	
	iface = BRASERO_DISC_GET_IFACE (disc);
	if (iface->set_session_contents)
		return (* iface->set_session_contents) (disc, session);

	return BRASERO_DISC_ERROR_UNKNOWN;
}

BraseroDiscResult
brasero_disc_load_track (BraseroDisc *disc,
			 BraseroDiscTrack *track)
{
	BraseroDiscIface *iface;

	g_return_val_if_fail (BRASERO_IS_DISC (disc), BRASERO_DISC_ERROR_UNKNOWN);
	g_return_val_if_fail (track != NULL, BRASERO_DISC_ERROR_UNKNOWN);
	
	iface = BRASERO_DISC_GET_IFACE (disc);
	if (iface->load_track)
		return (* iface->load_track) (disc, track);

	return BRASERO_DISC_ERROR_UNKNOWN;
}

gboolean
brasero_disc_get_selected_uri (BraseroDisc *disc, gchar **uri)
{
	BraseroDiscIface *iface;

	g_return_val_if_fail (BRASERO_IS_DISC (disc), FALSE);
	iface = BRASERO_DISC_GET_IFACE (disc);
	if (iface->get_selected_uri)
		return (* iface->get_selected_uri) (disc, uri);

	return FALSE;
}

gboolean
brasero_disc_get_boundaries (BraseroDisc *disc,
			     gint64 *start,
			     gint64 *end)
{
	BraseroDiscIface *iface;

	g_return_val_if_fail (BRASERO_IS_DISC (disc), FALSE);
	iface = BRASERO_DISC_GET_IFACE (disc);
	if (iface->get_boundaries)
		return (* iface->get_boundaries) (disc,
						  start,
						  end);

	return FALSE;
}

guint
brasero_disc_add_ui (BraseroDisc *disc, GtkUIManager *manager, GtkWidget *message)
{
	BraseroDiscIface *iface;

	if (!disc)
		return 0;

	g_return_val_if_fail (BRASERO_IS_DISC (disc), 0);
	g_return_val_if_fail (GTK_IS_UI_MANAGER (manager), 0);

	iface = BRASERO_DISC_GET_IFACE (disc);
	if (iface->add_ui)
		return (* iface->add_ui) (disc, manager, message);

	return 0;
}

void
brasero_disc_set_current_drive (BraseroDisc *disc,
				BraseroDrive *drive)
{
	BraseroDiscIface *iface;

	if (!disc)
		return;

	g_return_if_fail (BRASERO_IS_DISC (disc));

	iface = BRASERO_DISC_GET_IFACE (disc);
	if (iface->set_drive)
		(* iface->set_drive) (disc, drive);
}

void
brasero_disc_selection_changed (BraseroDisc *disc)
{
	g_return_if_fail (BRASERO_IS_DISC (disc));
	g_signal_emit (disc,
		       brasero_disc_signals [SELECTION_CHANGED_SIGNAL],
		       0);
}

void
brasero_disc_contents_changed (BraseroDisc *disc, int nb_files)
{
	g_return_if_fail (BRASERO_IS_DISC (disc));
	g_signal_emit (disc,
		       brasero_disc_signals [CONTENTS_CHANGED_SIGNAL],
		       0,
		       nb_files);
}

void
brasero_disc_size_changed (BraseroDisc *disc,
			   gint64 size)
{
	g_return_if_fail (BRASERO_IS_DISC (disc));
	g_signal_emit (disc,
		       brasero_disc_signals [SIZE_CHANGED_SIGNAL],
		       0,
		       size);
}

void
brasero_disc_flags_changed (BraseroDisc *disc,
			    BraseroBurnFlag flags)
{
	g_return_if_fail (BRASERO_IS_DISC (disc));
	g_signal_emit (disc,
		       brasero_disc_signals [FLAGS_CHANGED_SIGNAL],
		       0,
		       flags);
}

/**
 * Used to show the how to
 */
enum {
	TREE_MODEL_ROW = 150,
	FILE_NODES_ROW,
	TARGET_URIS_LIST,
};

static GtkTargetEntry ntables_cd[] = {
	{"GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, TREE_MODEL_ROW},
	{"GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, FILE_NODES_ROW},
	{"text/uri-list", 0, TARGET_URIS_LIST}
};
static guint nb_targets_cd = sizeof (ntables_cd) / sizeof (ntables_cd [0]);

static GtkWidget *
brasero_utils_disc_find_tree_view_in_container (GtkContainer *container)
{
	GList *children;
	GList *iter;

	children = gtk_container_get_children (container);
	for (iter = children; iter; iter = iter->next) {
		GtkWidget *widget;

		widget = iter->data;
		if (GTK_IS_TREE_VIEW (widget)) {
			g_list_free (children);
			return widget;
		}

		if (GTK_IS_CONTAINER (widget)) {
			widget = brasero_utils_disc_find_tree_view_in_container (GTK_CONTAINER (widget));
			if (widget) {
				g_list_free (children);
				return widget;
			}
		}
	}
	g_list_free (children);

	return NULL;
}

static GtkWidget *
brasero_utils_disc_find_tree_view (GtkNotebook *notebook)
{
	GtkWidget *other_widget;

	other_widget = gtk_notebook_get_nth_page (notebook, 1);
	return brasero_utils_disc_find_tree_view_in_container (GTK_CONTAINER (other_widget));
}

static void
brasero_utils_disc_hide_use_info_leave_cb (GtkWidget *widget,
					   GdkDragContext *drag_context,
					   guint time,
					   GtkNotebook *notebook)
{
	GtkWidget *other_widget;

	other_widget = brasero_utils_disc_find_tree_view (notebook);
	if (!other_widget)
		return;

	g_signal_emit_by_name (other_widget,
			       "drag-leave",
			       drag_context,
			       time);
}

static gboolean
brasero_utils_disc_hide_use_info_drop_cb (GtkWidget *widget,
					  GdkDragContext *drag_context,
					  gint x,
					  gint y,
					  guint time,
					  GtkNotebook *notebook)
{
	GdkAtom target = GDK_NONE;
	GtkWidget *other_widget;

	/* here the treeview is not realized so we'll have a warning message
	 * if we ever try to forward the event */
	other_widget = brasero_utils_disc_find_tree_view (notebook);
	if (!other_widget)
		return FALSE;

	target = gtk_drag_dest_find_target (other_widget,
					    drag_context,
					    gtk_drag_dest_get_target_list (GTK_WIDGET (other_widget)));

	if (target != GDK_NONE) {
		gboolean return_value = FALSE;

		/* The widget must be realized to receive such events. */
		gtk_notebook_set_current_page (notebook, 1);
		g_signal_emit_by_name (other_widget,
				       "drag-drop",
				       drag_context,
				       x,
				       y,
				       time,
				       &return_value);
		return return_value;
	}

	return FALSE;
}

static void
brasero_utils_disc_hide_use_info_data_received_cb (GtkWidget *widget,
						   GdkDragContext *drag_context,
						   gint x,
						   gint y,
						   GtkSelectionData *data,
						   guint info,
						   guint time,
						   GtkNotebook *notebook)
{
	GtkWidget *other_widget;

	gtk_notebook_set_current_page (notebook, 1);

	other_widget = brasero_utils_disc_find_tree_view (notebook);
	if (!other_widget)
		return;

	g_signal_emit_by_name (other_widget,
			       "drag-data-received",
			       drag_context,
			       x,
			       y,
			       data,
			       info,
			       time);
}

static gboolean
brasero_utils_disc_hide_use_info_motion_cb (GtkWidget *widget,
					    GdkDragContext *drag_context,
					    gint x,
					    gint y,
					    guint time,
					    GtkNotebook *notebook)
{
	return TRUE;
}

static gboolean
brasero_utils_disc_hide_use_info_button_cb (GtkWidget *widget,
					    GdkEventButton *event,
					    GtkNotebook *notebook)
{
	GtkWidget *other_widget;
	gboolean result;

	if (event->button != 3)
		return TRUE;

	other_widget = brasero_utils_disc_find_tree_view (notebook);
	if (!other_widget)
		return TRUE;

	g_signal_emit_by_name (other_widget,
			       "button-press-event",
			       event,
			       &result);

	return result;
}

static void
brasero_utils_disc_style_changed_cb (GtkWidget *widget,
				     GtkStyle *previous,
				     GtkWidget *event_box)
{
	/* The widget (a treeview here) needs to be realized to get proper style */
	gtk_widget_realize (widget);
	gtk_widget_modify_bg (event_box, GTK_STATE_NORMAL, &widget->style->base[GTK_STATE_NORMAL]);
}

static void
brasero_utils_disc_realized_cb (GtkWidget *event_box,
				GtkNotebook *notebook)
{
	GtkWidget *widget;

	widget = brasero_utils_disc_find_tree_view (notebook);

	if (!widget || !GTK_IS_TREE_VIEW (widget))
		return;

	/* The widget (a treeview here) needs to be realized to get proper style */
	gtk_widget_realize (widget);
	gtk_widget_modify_bg (event_box, GTK_STATE_NORMAL, &widget->style->base[GTK_STATE_NORMAL]);

	g_signal_handlers_disconnect_by_func (widget,
					      brasero_utils_disc_style_changed_cb,
					      event_box);
	g_signal_connect (widget,
			  "style-set",
			  G_CALLBACK (brasero_utils_disc_style_changed_cb),
			  event_box);
}

GtkWidget *
brasero_disc_get_use_info_notebook (void)
{
	GtkWidget *frame;
	GtkWidget *notebook;
	GtkWidget *event_box;
	GtkWidget *first_use;
	gchar     *message_add, *message_add_header;
	gchar     *message_remove, *message_remove_header;
	gchar	  *first_use_message;

	notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook), FALSE);

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
				  frame,
				  NULL);

	/* Now this event box must be 'transparent' to have the same background 
	 * color as a treeview */
	event_box = gtk_event_box_new ();
	gtk_event_box_set_visible_window (GTK_EVENT_BOX (event_box), TRUE);
	gtk_drag_dest_set (event_box, 
			   GTK_DEST_DEFAULT_MOTION,
			   ntables_cd,
			   nb_targets_cd,
			   GDK_ACTION_COPY|
			   GDK_ACTION_MOVE);

	/* the following signals need to be forwarded to the widget underneath */
	g_signal_connect (event_box,
			  "drag-motion",
			  G_CALLBACK (brasero_utils_disc_hide_use_info_motion_cb),
			  notebook);
	g_signal_connect (event_box,
			  "drag-leave",
			  G_CALLBACK (brasero_utils_disc_hide_use_info_leave_cb),
			  notebook);
	g_signal_connect (event_box,
			  "drag-drop",
			  G_CALLBACK (brasero_utils_disc_hide_use_info_drop_cb),
			  notebook);
	g_signal_connect (event_box,
			  "button-press-event",
			  G_CALLBACK (brasero_utils_disc_hide_use_info_button_cb),
			  notebook);
	g_signal_connect (event_box,
			  "drag-data-received",
			  G_CALLBACK (brasero_utils_disc_hide_use_info_data_received_cb),
			  notebook);

	gtk_container_add (GTK_CONTAINER (frame), event_box);

	/* Translators: this messages will appear as a list of possible
	 * actions, like:
	 *   To add/remove files you can:
         *      * perform action one
         *      * perform action two
	 * The full message will be showed in the main area of an empty
	 * project, suggesting users how to add and remove items to project.
	 * You simply have to translate messages in the best form
         * for a list of actions. */
	message_add_header = g_strconcat ("<big>", _("To add files to this project you can:"), "\n</big>", NULL);
	message_add = g_strconcat ("\t* ", _("click the \"Add\" button to show the selection pane"), "\n",
				   "\t* ", _("select files in selection pane and click the \"Add\" button"), "\n",
				   "\t* ", _("drag files in this area from the selection pane or from the file manager"), "\n",
				   "\t* ", _("double click on files in the selection pane"), "\n",

				   "\t* ", _("copy files (from file manager for example) and paste in this area"), "\n",
				   NULL);

	message_remove_header = g_strconcat ("<big>", _("To remove files from this project you can:"), "\n</big>", NULL);
	message_remove = g_strconcat ("\t* ", _("click on the \"Remove\" button to remove selected items in this area"), "\n",
				      "\t* ", _("drag and release items out from this area"), "\n",
				      "\t* ", _("select items in this area, and choose \"Remove\" from context menu"), "\n",
				      "\t* ", _("select items in this area, and press \"Delete\" key"), "\n",
				      NULL);
	

	first_use_message = g_strconcat ("<span foreground='grey50'>",
					 message_add_header, message_add,
					 "\n\n\n",
					 message_remove_header, message_remove,
					 "</span>", NULL);
	first_use = gtk_label_new (first_use_message);
	gtk_misc_set_alignment (GTK_MISC (first_use), 0.50, 0.30);
	gtk_label_set_ellipsize (GTK_LABEL (first_use), PANGO_ELLIPSIZE_END);
	g_free (first_use_message);

	gtk_misc_set_padding (GTK_MISC (first_use), 24, 0);
	gtk_label_set_justify (GTK_LABEL (first_use), GTK_JUSTIFY_LEFT);
	gtk_label_set_use_markup (GTK_LABEL (first_use), TRUE);
	gtk_container_add (GTK_CONTAINER (event_box), first_use);

	gtk_event_box_set_above_child (GTK_EVENT_BOX (event_box), TRUE);

	g_free (message_add_header);
	g_free (message_add);
	g_free (message_remove_header);
	g_free (message_remove);

	g_signal_connect (event_box,
			  "realize",
			  G_CALLBACK (brasero_utils_disc_realized_cb),
			  notebook);

	gtk_widget_show_all (notebook);
	return notebook;
}

/************************************ ******************************************/
static void
brasero_track_clear_song (gpointer data)
{
	BraseroDiscSong *song;

	song = data;

	if (song->info)
		brasero_song_info_free (song->info);

	g_free (song->uri);
	g_free (song);
}

void
brasero_track_clear (BraseroDiscTrack *track)
{
	if (!track)
		return;

	if (track->type == BRASERO_DISC_TRACK_AUDIO) {
		g_slist_foreach (track->contents.tracks, (GFunc) brasero_track_clear_song, NULL);
		g_slist_free (track->contents.tracks);
	}
	else if (track->type == BRASERO_DISC_TRACK_DATA) {
		g_slist_foreach (track->contents.data.grafts, (GFunc) brasero_graft_point_free, NULL);
		g_slist_free (track->contents.data.grafts);
		g_slist_foreach (track->contents.data.restored, (GFunc) g_free, NULL);
		g_slist_free (track->contents.data.restored);
		g_slist_foreach (track->contents.data.excluded, (GFunc) g_free, NULL);
		g_slist_free (track->contents.data.excluded);
	}
}

void
brasero_track_free (BraseroDiscTrack *track)
{
	brasero_track_clear (track);
	g_free (track);
}
