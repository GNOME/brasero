/***************************************************************************
 *            tray.c
 *
 *  ven avr 14 16:56:49 2006
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include <gtk/gtkwidget.h>
#include <gtk/gtklabel.h>
#include <gtk/gtktooltips.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtkaction.h>
#include <gtk/gtktoggleaction.h>
#include <gtk/gtkuimanager.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkiconfactory.h>
#include <gtk/gtkstatusicon.h>

#include "burn-basics.h"
#include "utils.h"
#include "tray.h"

static void brasero_tray_icon_class_init (BraseroTrayIconClass *klass);
static void brasero_tray_icon_init (BraseroTrayIcon *sp);
static void brasero_tray_icon_finalize (GObject *object);

static void
brasero_tray_icon_menu_popup_cb (BraseroTrayIcon *tray,
				 guint button,
				 guint time,
				 gpointer user_data);
static void
brasero_tray_icon_activate_cb (BraseroTrayIcon *tray,
			       gpointer user_data);

static void
brasero_tray_icon_cancel_cb (GtkAction *action, BraseroTrayIcon *tray);
static void
brasero_tray_icon_show_cb (GtkAction *action, BraseroTrayIcon *tray);
static void
brasero_tray_icon_close_toggled_cb (GtkToggleAction *action, BraseroTrayIcon *tray);

struct BraseroTrayIconPrivate {
	BraseroBurnAction action;
	GtkUIManager *manager;

	int first_burning_percent;
	int rounded_percent;
	int percent;

	int show_disc:1;
};

typedef enum {
	CANCEL_SIGNAL,
	CLOSE_AFTER_SIGNAL,
	SHOW_DIALOG_SIGNAL,
	LAST_SIGNAL
} BraseroTrayIconSignalType;

static guint brasero_tray_icon_signals[LAST_SIGNAL] = { 0 };
static GObjectClass *parent_class = NULL;

static GtkActionEntry entries[] = {
	{"ContextualMenu", NULL, N_("Menu")},
	{"Cancel", GTK_STOCK_CANCEL, N_("Cancel"), NULL, N_("Cancel ongoing burning"),
	 G_CALLBACK (brasero_tray_icon_cancel_cb)},
};

static GtkToggleActionEntry toggle_entries[] = {
	{"Close", NULL, N_("Close if successfull"), NULL, N_("Display file chooser"),
	 G_CALLBACK (brasero_tray_icon_close_toggled_cb), FALSE},
	{"Show", NULL, N_("Show dialog"), NULL, N_("Show dialog"),
	 G_CALLBACK (brasero_tray_icon_show_cb), TRUE,},
};

static const char *description = {
	"<ui>"
	"<popup action='ContextMenu'>"
		"<menuitem action='Cancel'/>"
		"<menuitem action='Show'/>"
		"<separator/>"
		"<menuitem action='Close'/>"
	"</popup>"
	"</ui>"
};

GType
brasero_tray_icon_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroTrayIconClass),
			NULL,
			NULL,
			(GClassInitFunc) brasero_tray_icon_class_init,
			NULL,
			NULL,
			sizeof (BraseroTrayIcon),
			0,
			(GInstanceInitFunc) brasero_tray_icon_init,
		};

		type = g_type_register_static(GTK_TYPE_STATUS_ICON, 
					      "BraseroTrayIcon",
					      &our_info,
					      0);
	}

	return type;
}

static void
brasero_tray_icon_class_init (BraseroTrayIconClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_tray_icon_finalize;

	brasero_tray_icon_signals[SHOW_DIALOG_SIGNAL] =
	    g_signal_new ("show_dialog",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
			  G_STRUCT_OFFSET (BraseroTrayIconClass,
					   show_dialog), NULL, NULL,
			  g_cclosure_marshal_VOID__BOOLEAN,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_BOOLEAN);
	brasero_tray_icon_signals[CANCEL_SIGNAL] =
	    g_signal_new ("cancel",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
			  G_STRUCT_OFFSET (BraseroTrayIconClass,
					   cancel), NULL, NULL,
			  g_cclosure_marshal_VOID__VOID,
			  G_TYPE_NONE,
			  0);
	brasero_tray_icon_signals[CLOSE_AFTER_SIGNAL] =
	    g_signal_new ("close_after",
			  G_OBJECT_CLASS_TYPE (object_class),
			  G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
			  G_STRUCT_OFFSET (BraseroTrayIconClass,
					   close_after), NULL, NULL,
			  g_cclosure_marshal_VOID__BOOLEAN,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_BOOLEAN);
}

static void
brasero_tray_icon_build_menu (BraseroTrayIcon *tray)
{
	GtkActionGroup *action_group;
	GError *error = NULL;

	action_group = gtk_action_group_new ("MenuAction");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (action_group,
				      entries,
				      G_N_ELEMENTS (entries),
				      tray);
	gtk_action_group_add_toggle_actions (action_group,
					     toggle_entries,
					     G_N_ELEMENTS (toggle_entries),
					     tray);

	tray->priv->manager = gtk_ui_manager_new ();
	gtk_ui_manager_insert_action_group (tray->priv->manager,
					    action_group,
					    0);

	if (!gtk_ui_manager_add_ui_from_string (tray->priv->manager,
						description,
						-1,
						&error)) {
		g_message ("building menus failed: %s", error->message);
		g_error_free (error);
	}
}

static void
brasero_tray_icon_init (BraseroTrayIcon *obj)
{
	GdkPixbuf *pixbuf;

	obj->priv = g_new0 (BraseroTrayIconPrivate, 1);
	brasero_tray_icon_build_menu (obj);
	g_signal_connect (obj,
			  "popup-menu",
			  G_CALLBACK (brasero_tray_icon_menu_popup_cb),
			  NULL);
	g_signal_connect (obj,
			  "activate",
			  G_CALLBACK (brasero_tray_icon_activate_cb),
			  NULL);

	pixbuf = gdk_pixbuf_new_from_file (BRASERO_DATADIR G_DIR_SEPARATOR_S "disc-00.png", NULL);
	if (pixbuf) {
		gtk_status_icon_set_from_pixbuf (GTK_STATUS_ICON (obj), pixbuf);
		g_object_unref (pixbuf);
	}
	else
		g_warning ("Faulty installation. \"%s\" can't be found.\n",
			   BRASERO_DATADIR G_DIR_SEPARATOR_S "disc-00.png");

	gtk_status_icon_set_tooltip (GTK_STATUS_ICON (obj), _("waiting"));
	obj->priv->first_burning_percent = -1;
}

static void
brasero_tray_icon_finalize (GObject *object)
{
	BraseroTrayIcon *cobj;

	cobj = BRASERO_TRAYICON (object);

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

BraseroTrayIcon *
brasero_tray_icon_new ()
{
	BraseroTrayIcon *obj;
	
	obj = BRASERO_TRAYICON (g_object_new (BRASERO_TYPE_TRAYICON, NULL));

	return obj;
}

static void
brasero_tray_icon_set_tooltip (BraseroTrayIcon *tray,
			       glong remaining)
{
	gchar *text;
	const gchar *action_string;

	action_string = brasero_burn_action_to_string (tray->priv->action);

	if (remaining > 0) {
		gchar *remaining_string;

		remaining_string = brasero_utils_get_time_string ((double) remaining * 1000000000, TRUE, FALSE);
		text = g_strdup_printf (_("%s, %02i%% done, %s remaining"),
					action_string,
					tray->priv->percent,
					remaining_string);
		g_free (remaining_string);
	}
	else if (tray->priv->percent > 0)
		text = g_strdup_printf (_("%s, %02i%% done"),
					action_string,
					tray->priv->percent);
	else
		text = g_strdup (action_string);

	gtk_status_icon_set_tooltip (GTK_STATUS_ICON (tray), text);
	g_free (text);
}

void
brasero_tray_icon_set_action (BraseroTrayIcon *tray,
			      BraseroBurnAction action)
{
	if (action == BRASERO_BURN_ACTION_DRIVE_COPY
	||  action == BRASERO_BURN_ACTION_WRITING)
		tray->priv->show_disc = TRUE;

	tray->priv->action = action;
	brasero_tray_icon_set_tooltip (tray, -1);
}

void
brasero_tray_icon_set_progress (BraseroTrayIcon *tray,
				gdouble fraction,
				long remaining)
{
	int percent;
	int remains;
	char *icon_name;
	GdkPixbuf *pixbuf;

	percent = fraction * 100;
	tray->priv->percent = percent;

	/* set the tooltip */
	brasero_tray_icon_set_tooltip (tray, remaining);

	if (!tray->priv->show_disc)
		return;

	if (tray->priv->first_burning_percent == -1)
		tray->priv->first_burning_percent = percent;

	if (tray->priv->first_burning_percent != 0) {
		percent -= tray->priv->first_burning_percent;
		percent = (100 - tray->priv->first_burning_percent) != 0 ?
			   percent * 100 / (100 - tray->priv->first_burning_percent) :
			   0;
	}

	/* change image if need be */
	remains = percent % 5;
	if (remains > 3)
		percent += 5 - remains;
	else
		percent -= remains;

	if (tray->priv->rounded_percent == percent
	||  percent < 0 || percent > 100)
		return;

	tray->priv->rounded_percent = percent;

	icon_name = g_strdup_printf (BRASERO_DATADIR G_DIR_SEPARATOR_S "disc-%02i.png", percent);
	pixbuf = gdk_pixbuf_new_from_file (icon_name, NULL);
	g_free (icon_name);

	if (pixbuf)
		gtk_status_icon_set_from_pixbuf (GTK_STATUS_ICON (tray), pixbuf);
	else
		g_warning ("Faulty installation. \"%s\" can't be found.\n", icon_name);
}

static void
brasero_tray_icon_change_show_dialog_state (BraseroTrayIcon *tray)
{
	GtkAction *action;
	gboolean active;

	/* update menu */
	action = gtk_ui_manager_get_action (tray->priv->manager, "/ContextMenu/Show");
	active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));

	/* signal show dialog was requested the dialog again */
	g_signal_emit (tray,
		       brasero_tray_icon_signals [SHOW_DIALOG_SIGNAL],
		       0,
		       active);
}

static void
brasero_tray_icon_menu_popup_cb (BraseroTrayIcon *tray,
				 guint button,
				 guint time,
				 gpointer user_data)
{
	GtkWidget *menu;

	menu = gtk_ui_manager_get_widget (tray->priv->manager,"/ContextMenu");
	gtk_menu_popup (GTK_MENU (menu),
			NULL,
			NULL,
			gtk_status_icon_position_menu,
			tray,
			button,
			time);
}

static void
brasero_tray_icon_activate_cb (BraseroTrayIcon *tray,
			       gpointer user_data)
{
	GtkAction *action;
	gboolean show;
	
	/* update menu */
	action = gtk_ui_manager_get_action (tray->priv->manager, "/ContextMenu/Show");
	show = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	show = show ? FALSE:TRUE;
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), show);
}

static void
brasero_tray_icon_cancel_cb (GtkAction *action, BraseroTrayIcon *tray)
{
	g_signal_emit (tray,
		       brasero_tray_icon_signals [CANCEL_SIGNAL],
		       0);
}

static void
brasero_tray_icon_show_cb (GtkAction *action, BraseroTrayIcon *tray)
{
	brasero_tray_icon_change_show_dialog_state (tray);
}

static void
brasero_tray_icon_close_toggled_cb (GtkToggleAction *action,
				    BraseroTrayIcon *tray)
{
	gboolean active;

	active = gtk_toggle_action_get_active (action);

	g_signal_emit (tray,
		       brasero_tray_icon_signals [CLOSE_AFTER_SIGNAL],
		       0,
		       active);
}

void
brasero_tray_icon_set_show_dialog (BraseroTrayIcon *tray, gboolean show)
{
	GtkAction *action;

	/* update menu */
	action = gtk_ui_manager_get_action (tray->priv->manager, "/ContextMenu/Show");
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), show);
}
