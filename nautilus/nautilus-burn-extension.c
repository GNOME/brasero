/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2003 Novell, Inc.
 * Copyright (C) 2003-2004 Red Hat, Inc.
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2008-2009 Philippe Rouquier <bonfire-app@wanadoo.fr> (modified to work with brasero)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <libnautilus-extension/nautilus-menu-provider.h>
#include <libnautilus-extension/nautilus-location-widget-provider.h>

#include "brasero-media.h"
#include "brasero-medium-monitor.h"
#include "brasero-drive.h"
#include "brasero-medium.h"

#include "brasero-burn-lib.h"
#include "brasero-track.h"
#include "brasero-track-data-cfg.h"
#include "brasero-track-image-cfg.h"
#include "brasero-track-disc.h"
#include "brasero-session.h"
#include "brasero-session-cfg.h"

#include "brasero-tool-dialog.h"
#include "brasero-blank-dialog.h"
#include "brasero-sum-dialog.h"

#include "brasero-burn-options.h"
#include "brasero-burn-dialog.h"

#include "nautilus-burn-bar.h"

#include "brasero-misc.h"

#include "brasero-media-private.h"
#include "burn-debug.h"

#define BURN_URI	"burn:///"
#define WINDOW_KEY      "NautilusWindow"

#define NAUTILUS_TYPE_DISC_BURN  (nautilus_disc_burn_get_type ())
#define NAUTILUS_DISC_BURN(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), NAUTILUS_TYPE_DISC_BURN, NautilusDiscBurn))
#define NAUTILUS_IS_DISC_BURN(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), NAUTILUS_TYPE_DISC_BURN))

typedef struct _NautilusDiscBurnPrivate NautilusDiscBurnPrivate;

typedef struct
{
        GObject              parent_slot;
        NautilusDiscBurnPrivate *priv;
} NautilusDiscBurn;

typedef struct
{
        GObjectClass parent_slot;
} NautilusDiscBurnClass;

#define NAUTILUS_DISC_BURN_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NAUTILUS_TYPE_DISC_BURN, NautilusDiscBurnPrivate))

struct _NautilusDiscBurnPrivate
{
        GFileMonitor *burn_monitor;
        guint         empty : 1;

        guint         start_monitor_id;
        guint         empty_update_id;

        GSList       *widget_list;

	gchar        *title;
	gchar        *icon;
};

static GType nautilus_disc_burn_get_type      (void);
static void  nautilus_disc_burn_register_type (GTypeModule *module);

static GObjectClass *parent_class;

//#define DEBUG_PRINT(format_MACRO,...)           g_print (format_MACRO, ##__VA_ARGS__);
#define DEBUG_PRINT(format_MACRO,...)             

#define BRASERO_SCHEMA				"org.gnome.brasero"
#define BRASERO_PROPS_NAUTILUS_EXT_DEBUG	"nautilus-extension-debug"

/* do not call brasero_*_start() at nautilus startup, they are very expensive;
 * lazily initialize those instead */
static void
ensure_initialized ()
{
	static gboolean initialized = FALSE;

	if (!initialized) {
		GSettings *settings;

		settings = g_settings_new (BRASERO_SCHEMA);
		if (g_settings_get_boolean (settings, BRASERO_PROPS_NAUTILUS_EXT_DEBUG)) {
			brasero_media_library_set_debug (TRUE);
			brasero_burn_library_set_debug (TRUE);
		}
		g_object_unref (settings);

		brasero_media_library_start ();
		brasero_burn_library_start (NULL, NULL);

		DEBUG_PRINT ("Libbrasero-media started\n");
		initialized = TRUE;
	}
}

static void
launch_brasero_on_window_session (BraseroSessionCfg	*session,
                                  const gchar		*dialog_title,
				  GtkWidget		*options,
				  GtkWindow		*window)
{
	GtkResponseType		 result;
	const gchar		*icon_name;
	GtkWidget		*dialog;

	/* Get the icon for the window */
	if (window)
		icon_name = gtk_window_get_icon_name (window);
	else
		icon_name = "brasero";

	/* run option dialog */
	dialog = brasero_burn_options_new (session);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), icon_name);

	if (dialog_title)
		gtk_window_set_title (GTK_WINDOW (dialog), dialog_title);

	if (options)
		brasero_burn_options_add_options (BRASERO_BURN_OPTIONS (dialog), options);

	gtk_widget_show (GTK_WIDGET (dialog));
	result = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	if (result != GTK_RESPONSE_OK
	&&  result != GTK_RESPONSE_ACCEPT)
		return;

	/* now run burn dialog */
	dialog = brasero_burn_dialog_new ();

	gtk_window_set_icon_name (GTK_WINDOW (dialog), icon_name);

	if (dialog_title)
		gtk_window_set_title (GTK_WINDOW (dialog), dialog_title);

	brasero_session_cfg_disable (session);

	gtk_widget_show (dialog);
	gtk_window_present (GTK_WINDOW (dialog));

	if (result == GTK_RESPONSE_OK)
		brasero_burn_dialog_run (BRASERO_BURN_DIALOG (dialog),
		                                   BRASERO_BURN_SESSION (session));
	else
		brasero_burn_dialog_run_multi (BRASERO_BURN_DIALOG (dialog),
		                                         BRASERO_BURN_SESSION (session));

	gtk_widget_destroy (dialog);
}

static gboolean
nautilus_disc_burn_is_empty (GtkWindow *toplevel)
{
	GFileEnumerator *enumerator;
	GFileInfo *info = NULL;
	GError *error = NULL;
	GFile *file;

	file = g_file_new_for_uri (BURN_URI);
	enumerator = g_file_enumerate_children (file,
						G_FILE_ATTRIBUTE_STANDARD_NAME,
						G_FILE_QUERY_INFO_NONE,
						NULL,
						&error);
	if (!enumerator) {
		gchar *string;

		DEBUG_PRINT ("Could not open burn uri %s: %s\n",
                             uri,
                             error->message);

		if (!toplevel) {
			g_error_free (error);
			return TRUE;
		}

		string = g_strdup_printf ("%s.", _("An internal error occurred"));
		brasero_utils_message_dialog (GTK_WIDGET (toplevel),
					      string,
					      error ? error->message:NULL,
					      GTK_MESSAGE_ERROR);
		g_free (string);

		g_object_unref (file);
		g_error_free (error);
		return TRUE;
	}

	info = g_file_enumerator_next_file (enumerator, NULL, NULL);
	g_object_unref (enumerator);
	g_object_unref (file);

	if (!info) {
		if (!toplevel)
			return TRUE;

		brasero_utils_message_dialog (GTK_WIDGET (toplevel),
					      _("Please add files."),
					      _("There are no files to write to disc"),
					      GTK_MESSAGE_ERROR);
		return TRUE;
	}

	g_object_unref (info);
	return FALSE;
}

static void
write_activate (NautilusDiscBurn *burn,
                GtkWindow *toplevel)
{
	BraseroTrackDataCfg	*track;
	BraseroSessionCfg	*session;

	if (nautilus_disc_burn_is_empty (toplevel))
		return;

	ensure_initialized ();

	track = brasero_track_data_cfg_new ();
	brasero_track_data_cfg_add (track, BURN_URI, NULL);

	if (burn->priv->icon)
		brasero_track_data_cfg_set_icon (BRASERO_TRACK_DATA_CFG (track),
		                                 burn->priv->icon,
		                                 NULL);

	session = brasero_session_cfg_new ();
	brasero_burn_session_add_track (BRASERO_BURN_SESSION (session),
					BRASERO_TRACK (track),
					NULL);
	g_object_unref (track);

	if (burn->priv->title)
		brasero_burn_session_set_label (BRASERO_BURN_SESSION (session),
		                                burn->priv->title);

	/* NOTE: set the disc we're handling */
	launch_brasero_on_window_session (session,
	                                  _("CD/DVD Creator"),
	                                  NULL,
	                                  toplevel);

	/* cleanup */
	g_object_unref (session);
}

static void
write_activate_cb (NautilusMenuItem *item,
                   gpointer          user_data)
{
	write_activate (NAUTILUS_DISC_BURN (user_data),
	                GTK_WINDOW (g_object_get_data (G_OBJECT (item), WINDOW_KEY)));
}

static void
launch_brasero_on_window_track (BraseroTrack	*track,
                                const gchar	*dialog_title,
				GtkWidget	*options,
				GtkWindow	*window)
{
	BraseroSessionCfg *session;

	/* create a session and add track */
	session = brasero_session_cfg_new ();
	brasero_burn_session_add_track (BRASERO_BURN_SESSION (session),
					BRASERO_TRACK (track),
					NULL);

	launch_brasero_on_window_session (session,
	                                  dialog_title,
	                                  options,
	                                  window);
	g_object_unref (session);
}

static void
brasero_nautilus_track_changed_cb (BraseroTrack *track,
				   gpointer user_data)
{
	launch_brasero_on_window_track (track,
	                                _("Write to Disc"),
	                                NULL,
	                                GTK_WINDOW (user_data));
	g_object_unref (track);
}

static void
write_iso_activate_cb (NautilusMenuItem *item,
                       gpointer          user_data)
{
	BraseroTrackImageCfg	*track;
        NautilusFileInfo	*file_info;
        char			*uri;

	ensure_initialized();

        file_info = g_object_get_data (G_OBJECT (item), "file_info");
        uri = nautilus_file_info_get_uri (file_info);

	track = brasero_track_image_cfg_new ();
	brasero_track_image_cfg_set_source (track, uri);

	g_signal_connect (track, "changed",
			  G_CALLBACK (brasero_nautilus_track_changed_cb), user_data);
}

static void
copy_disc_activate_cb (NautilusMenuItem *item,
                       gpointer          user_data)
{
        char            	*device_path;
	BraseroMediumMonitor	*monitor;
	BraseroTrackDisc	*track;
	BraseroDrive		*drive;

	ensure_initialized();

        device_path = g_object_get_data (G_OBJECT (item), "drive_device_path");
	monitor = brasero_medium_monitor_get_default ();
	drive = brasero_medium_monitor_get_drive (monitor, device_path);
	g_object_unref (monitor);

	track = brasero_track_disc_new ();
	brasero_track_disc_set_drive (track, drive);
	g_object_unref (drive);

	launch_brasero_on_window_track (BRASERO_TRACK (track),
	                                _("Copy Disc"),
	                                NULL,
	                                GTK_WINDOW (user_data));
	g_object_unref (track);
}

static void
tool_dialog_run (BraseroToolDialog	*dialog,
		 GtkWindow		*toplevel,
		 NautilusMenuItem	*item)
{
	char			*device_path;
	BraseroDrive		*drive;
	BraseroMediumMonitor	*monitor;

	device_path = g_object_get_data (G_OBJECT (item), "drive_device_path");
	if (!device_path) {
		g_warning ("Drive device path not specified");
		return;
	}

	monitor = brasero_medium_monitor_get_default ();
	drive = brasero_medium_monitor_get_drive (monitor, device_path);
	g_object_unref (monitor);

	if (drive) {
		brasero_tool_dialog_set_medium (BRASERO_TOOL_DIALOG (dialog),
						brasero_drive_get_medium (drive));
		g_object_unref (drive);
	}

	/* Get the icon for the window */
	if (toplevel)
		gtk_window_set_icon_name (GTK_WINDOW (dialog), gtk_window_get_icon_name (toplevel));
	else
		gtk_window_set_icon_name (GTK_WINDOW (dialog), "brasero");

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
blank_disc_activate_cb (NautilusMenuItem *item,
                        gpointer          user_data)
{
	BraseroBlankDialog *dialog;

	ensure_initialized();

	dialog = brasero_blank_dialog_new ();
	tool_dialog_run (BRASERO_TOOL_DIALOG (dialog),
			 GTK_WINDOW (user_data),
			 item);
}

static void
check_disc_activate_cb (NautilusMenuItem *item,
                        gpointer          user_data)
{
	BraseroSumDialog *dialog;

	ensure_initialized ();

	dialog = brasero_sum_dialog_new ();
	tool_dialog_run (BRASERO_TOOL_DIALOG (dialog),
			 GTK_WINDOW (user_data),
			 item);
}

static gboolean
volume_is_blank (GVolume *volume)
{
        BraseroMediumMonitor *monitor;
        BraseroMedium        *medium;
        BraseroDrive         *drive;
        gchar                *device;
        gboolean              is_blank;

        is_blank = FALSE;

        device = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
        if (!device)
                return FALSE;

        DEBUG_PRINT ("Got device: %s\n", device);

        monitor = brasero_medium_monitor_get_default ();
        drive = brasero_medium_monitor_get_drive (monitor, device);
        g_object_unref (monitor);
        g_free (device);

        if (drive == NULL)
                return FALSE;

        medium = brasero_drive_get_medium (drive);
        is_blank = (brasero_medium_get_status (medium) & BRASERO_MEDIUM_BLANK);
        g_object_unref (drive);

        return is_blank;
}

static GVolume *
drive_get_first_volume (GDrive *drive)
{
        GVolume *volume;
        GList   *volumes;

        volumes = g_drive_get_volumes (drive);

        volume = g_list_nth_data (volumes, 0);

        if (volume != NULL) {
                g_object_ref (volume);
        }

        g_list_foreach (volumes, (GFunc) g_object_unref, NULL);
        g_list_free (volumes);

        return volume;
}

static gboolean
drive_is_cd_device (GDrive *gdrive)
{
        BraseroMediumMonitor *monitor;
        BraseroDrive         *drive;
        gchar                *device;

        device = g_drive_get_identifier (gdrive, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
        if (!device)
                return FALSE;

        DEBUG_PRINT ("Got device: %s\n", device);

	/* FIXME: since we call the monitor, the library should be initialized.
	 * To avoid all the initializing we'll be able to use the new GIO API
	 * (#594649 -  Have a way to detect optical drives) */
	ensure_initialized();

	monitor = brasero_medium_monitor_get_default ();
        drive = brasero_medium_monitor_get_drive (monitor, device);
        g_object_unref (monitor);
        g_free (device);

        if (drive == NULL)
                return FALSE;
        
        g_object_unref (drive);
        return TRUE;
}

static GList *
nautilus_disc_burn_get_file_items (NautilusMenuProvider *provider,
                                   GtkWidget            *window,
                                   GList                *selection)
{
        GList            *items = NULL;
        NautilusMenuItem *item;
        NautilusFileInfo *file_info;
        GFile            *file;
        GMount           *mount;
        GVolume          *volume;
        GDrive           *drive;
        char             *mime_type;
        gboolean          is_iso;

        DEBUG_PRINT ("Getting file items\n");

        if (!selection || selection->next != NULL) {
                return NULL;
        }

        file_info = NAUTILUS_FILE_INFO (selection->data);

        if (nautilus_file_info_is_gone (file_info)) {
                return NULL;
        }

        file = nautilus_file_info_get_location (file_info);

        if (file == NULL) {
                DEBUG_PRINT ("No file found\n");
                return NULL;
        }

        mime_type = nautilus_file_info_get_mime_type (file_info);
        DEBUG_PRINT ("Mime type: %s\n", mime_type);
        if (! mime_type) {
                g_object_unref (file);
                return NULL;
        }

        is_iso = (strcmp (mime_type, "application/x-iso-image") == 0)
                || (strcmp (mime_type, "application/x-cd-image") == 0)
                || (strcmp (mime_type, "application/x-cue") == 0)
                || (strcmp (mime_type, "application/x-toc") == 0)
                || (strcmp (mime_type, "application/x-cdrdao-toc") == 0);

        if (is_iso) {
                /* Whether or not this file is local is not a problem */
                item = nautilus_menu_item_new ("NautilusDiscBurn::write_iso",
                                               _("_Write to Disc…"),
                                               _("Write disc image to a CD or DVD"),
                                               "media-optical-data-new");
                g_object_set_data (G_OBJECT (item), "file_info", file_info);
                g_object_set_data (G_OBJECT (item), "window", window);
                g_signal_connect (item, "activate",
                                  G_CALLBACK (write_iso_activate_cb), window);
                items = g_list_append (items, item);
        }

        /*
         * We handle two cases here.  The selection is:
         *  A) a volume
         *  B) a drive
         *
         * This is because there is very little distinction between
         * the two for CD/DVD media
         */

        drive = NULL;
        volume = NULL;

        mount = nautilus_file_info_get_mount (file_info);
        if (mount != NULL) {
                drive = g_mount_get_drive (mount);
                volume = g_mount_get_volume (mount);
        } else {
                char *uri = g_file_get_uri (file);
                DEBUG_PRINT ("Mount not found: %s\n", uri);
                g_free (uri);
        }

        if (drive == NULL && volume != NULL) {
                /* case A */
                drive = g_volume_get_drive (volume);
        } else if (volume == NULL && drive != NULL) {
                /* case B */
                volume = drive_get_first_volume (drive);
                if (volume == NULL) {
                        DEBUG_PRINT ("Volume not found\n");
                }
        }

        if (drive != NULL
            && volume != NULL
            && drive_is_cd_device (drive)
            && ! volume_is_blank (volume)) {
                char			*device_path;
		BraseroMediumMonitor	*monitor;
		BraseroDrive		*bdrive;
		BraseroMedium		*medium;
		BraseroMedia		 media;
		BraseroTrackType	*type;

		/* Reminder: the following is not needed since it is already 
		 * called in drive_is_cd_device ().
		 * ensure_initialized();
		 */

                device_path = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
		monitor = brasero_medium_monitor_get_default ();
		bdrive = brasero_medium_monitor_get_drive (monitor, device_path);
		g_object_unref (monitor);

		medium = brasero_drive_get_medium (bdrive);
		media = brasero_medium_get_status (medium);
		g_object_unref (bdrive);

		type = brasero_track_type_new ();
		brasero_track_type_set_has_medium (type);
		brasero_track_type_set_medium_type (type, media);
		if (brasero_burn_library_input_supported (type) == BRASERO_BURN_OK) {
			/* user may want to copy it ... */
			item = nautilus_menu_item_new ("NautilusDiscBurn::copy_disc",
						       _("_Copy Disc…"),
						       _("Create a copy of this CD or DVD"),
						       "media-optical-copy");
			g_object_set_data (G_OBJECT (item), "file_info", file_info);
			g_object_set_data (G_OBJECT (item), "window", window);
			g_object_set_data_full (G_OBJECT (item), "drive_device_path", g_strdup (device_path), g_free);
			g_signal_connect (item, "activate", G_CALLBACK (copy_disc_activate_cb), window);
			items = g_list_append (items, item);
		}
		brasero_track_type_free (type);

		if (brasero_burn_library_get_media_capabilities (media) & BRASERO_MEDIUM_REWRITABLE) {
			/* ... or if it's a rewritable medium to blank it ... */
			item = nautilus_menu_item_new ("NautilusDiscBurn::blank_disc",
						       _("_Blank Disc…"),
						       _("Blank this CD or DVD"),
						       "media-optical-blank");
			g_object_set_data (G_OBJECT (item), "file_info", file_info);
			g_object_set_data (G_OBJECT (item), "window", window);
			g_object_set_data_full (G_OBJECT (item), "drive_device_path", g_strdup (device_path), g_free);
			g_signal_connect (item, "activate",
					  G_CALLBACK (blank_disc_activate_cb), window);
			items = g_list_append (items, item);
		}

		/* - library should be able to checksum
		 * - disc must have a data session */
		if (brasero_burn_library_can_checksum ()
		&& (media & BRASERO_MEDIUM_HAS_DATA)) {
			/* ... or verify medium. */
			item = nautilus_menu_item_new ("NautilusDiscBurn::check_disc",
						       _("_Check Disc…"),
						       _("Check the data integrity on this CD or DVD"),
						       NULL);
			g_object_set_data (G_OBJECT (item), "file_info", file_info);
			g_object_set_data (G_OBJECT (item), "window", window);
			g_object_set_data_full (G_OBJECT (item), "drive_device_path", g_strdup (device_path), g_free);
			g_signal_connect (item, "activate",
					  G_CALLBACK (check_disc_activate_cb),
			                  window);
			items = g_list_append (items, item);
		}

                g_free (device_path);
        }

        g_object_unref (file);

        if (drive != NULL) {
                g_object_unref (drive);
        }
        if (volume != NULL) {
                g_object_unref (volume);
        }

        g_free (mime_type);

        DEBUG_PRINT ("Items returned\n");
        return items;
}

static GList *
nautilus_disc_burn_get_background_items (NautilusMenuProvider *provider,
                                         GtkWidget            *window,
                                         NautilusFileInfo     *current_folder)
{
        GList *items;
        char  *scheme;

        items = NULL;

        scheme = nautilus_file_info_get_uri_scheme (current_folder);
        if (!scheme)
                return NULL;

        if (strcmp (scheme, "burn") == 0) {
                NautilusMenuItem *item;

                item = nautilus_menu_item_new ("NautilusDiscBurn::write_menu",
                                               _("_Write to Disc…"),
                                               _("Write contents to a CD or DVD"),
                                               "brasero");
		g_object_set_data (G_OBJECT (item), WINDOW_KEY, window);
                g_signal_connect (item, "activate",
                                  G_CALLBACK (write_activate_cb),
                                  NAUTILUS_DISC_BURN (provider));
                items = g_list_append (items, item);

                g_object_set (item, "sensitive", ! NAUTILUS_DISC_BURN (provider)->priv->empty, NULL);
        }

        g_free (scheme);

        return items;
}

static void
nautilus_disc_burn_menu_provider_iface_init (NautilusMenuProviderIface *iface)
{
        iface->get_file_items = nautilus_disc_burn_get_file_items;
        iface->get_background_items = nautilus_disc_burn_get_background_items;
}

static void
bar_activated_cb (NautilusDiscBurnBar	*bar,
                  gpointer		 user_data)
{
	write_activate (NAUTILUS_DISC_BURN (user_data),
	                GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (bar))));
}

static void
title_changed_cb (NautilusDiscBurnBar	*bar,
                  NautilusDiscBurn	*burn)
{
	if (burn->priv->title)
		g_free (burn->priv->title);
	burn->priv->title = g_strdup (nautilus_disc_burn_bar_get_title (bar));
}

static void
icon_changed_cb (NautilusDiscBurnBar	*bar,
                 NautilusDiscBurn	*burn)
{
	if (burn->priv->icon)
		g_free (burn->priv->icon);
	burn->priv->icon = g_strdup (nautilus_disc_burn_bar_get_icon (bar));
}

static void
destroyed_callback (GtkWidget    *widget,
                    NautilusDiscBurn *burn)
{
        burn->priv->widget_list = g_slist_remove (burn->priv->widget_list, widget);
}

static void
sense_widget (NautilusDiscBurn *burn,
              GtkWidget    *widget)
{
        gtk_widget_set_sensitive (widget, !burn->priv->empty);

        burn->priv->widget_list = g_slist_prepend (burn->priv->widget_list, widget);

        g_signal_connect (widget, "destroy",
                          G_CALLBACK (destroyed_callback),
                          burn);
}

static GtkWidget *
nautilus_disc_burn_get_location_widget (NautilusLocationWidgetProvider *iface,
                                        const char                     *uri,
                                        GtkWidget                      *window)
{
        if (g_str_has_prefix (uri, "burn:")) {
                GtkWidget    *bar;
                NautilusDiscBurn *burn;

                DEBUG_PRINT ("Get location widget for burn\n");

                burn = NAUTILUS_DISC_BURN (iface);

                bar = nautilus_disc_burn_bar_new ();
		nautilus_disc_burn_bar_set_title (NAUTILUS_DISC_BURN_BAR (bar),
		                                  burn->priv->title);
		nautilus_disc_burn_bar_set_icon (NAUTILUS_DISC_BURN_BAR (bar),
		                                 burn->priv->icon);
                sense_widget (burn, nautilus_disc_burn_bar_get_button (NAUTILUS_DISC_BURN_BAR (bar)));

                g_signal_connect (bar, "activate",
                                  G_CALLBACK (bar_activated_cb),
                                  burn);
		g_signal_connect (bar, "title-changed",
		                  G_CALLBACK (title_changed_cb),
		                  burn);
		g_signal_connect (bar, "icon-changed",
		                  G_CALLBACK (icon_changed_cb),
		                  burn);

                gtk_widget_show (bar);

                return bar;
        }

        return NULL;
}

static void
nautilus_disc_burn_location_widget_provider_iface_init (NautilusLocationWidgetProviderIface *iface)
{
        iface->get_widget = nautilus_disc_burn_get_location_widget;
}

static void
update_widget_sensitivity (GtkWidget    *widget,
                           NautilusDiscBurn *burn)
{
        gtk_widget_set_sensitive (widget, !burn->priv->empty);
}

static gboolean
update_empty_idle (NautilusDiscBurn *burn)
{
        gboolean is_empty;

        burn->priv->empty_update_id = 0;

        is_empty = nautilus_disc_burn_is_empty (NULL);

        DEBUG_PRINT ("Dir is %s\n", is_empty ? "empty" : "not empty");

        if (burn->priv->empty != is_empty) {
                burn->priv->empty = is_empty;
                /* update bar */
                g_slist_foreach (burn->priv->widget_list, (GFunc)update_widget_sensitivity, burn);

                /* Trigger update for menu items */
                nautilus_menu_provider_emit_items_updated_signal (NAUTILUS_MENU_PROVIDER (burn));
        }

        return FALSE;
}

static void
queue_update_empty (NautilusDiscBurn *burn)
{
        if (burn->priv->empty_update_id != 0) {
                g_source_remove (burn->priv->empty_update_id);
        }

        burn->priv->empty_update_id = g_idle_add ((GSourceFunc)update_empty_idle, burn);
}

static void
burn_monitor_cb (GFileMonitor     *monitor,
                 GFile            *file,
                 GFile            *other_file,
                 GFileMonitorEvent event_type,
                 NautilusDiscBurn     *burn)
{
        DEBUG_PRINT ("Monitor callback type %d\n", event_type);

        /* only queue the action if it has a chance of changing the state */
        if (event_type == G_FILE_MONITOR_EVENT_CREATED) {
                if (burn->priv->empty) {
                        queue_update_empty (burn);
                }
        } else if (event_type == G_FILE_MONITOR_EVENT_DELETED) {
                if (! burn->priv->empty) {
                        queue_update_empty (burn);
                }
        }
}

static gboolean
start_monitor (NautilusDiscBurn *burn)
{
        GFile  *file;
        GError *error;

        file = g_file_new_for_uri (BURN_URI);

        error = NULL;
        burn->priv->burn_monitor = g_file_monitor_directory (file,
                                                             G_FILE_MONITOR_NONE,
                                                             NULL,
                                                             &error);
        if (burn->priv->burn_monitor == NULL) {
                DEBUG_PRINT ("Unable to add monitor: %s", error->message);
                g_error_free (error);
                goto out;
        }

        DEBUG_PRINT ("Starting monitor for %s\n", BURN_URI);
        g_signal_connect (burn->priv->burn_monitor,
                          "changed",
                          G_CALLBACK (burn_monitor_cb),
                          burn);

        burn->priv->empty = nautilus_disc_burn_is_empty (NULL);

        DEBUG_PRINT ("Init burn extension, empty: %d\n", burn->priv->empty);

 out:
        g_object_unref (file);

        burn->priv->start_monitor_id = 0;

        return FALSE;
}

static void
nautilus_disc_burn_instance_init (NautilusDiscBurn *burn)
{
        burn->priv = NAUTILUS_DISC_BURN_GET_PRIVATE (burn);
        burn->priv->start_monitor_id = g_timeout_add_seconds (1,
                                                              (GSourceFunc)start_monitor,
                                                              burn);
}

static void
nautilus_disc_burn_finalize (GObject *object)
{
        NautilusDiscBurn *burn;

        g_return_if_fail (object != NULL);
        g_return_if_fail (NAUTILUS_IS_DISC_BURN (object));

        DEBUG_PRINT ("Finalizing burn extension\n");

        burn = NAUTILUS_DISC_BURN (object);

        g_return_if_fail (burn->priv != NULL);

	if (burn->priv->icon) {
		g_free (burn->priv->icon);
		burn->priv->icon = NULL;
	}

	if (burn->priv->title) {
		g_free (burn->priv->title);
		burn->priv->title = NULL;
	}

        if (burn->priv->empty_update_id > 0) {
                g_source_remove (burn->priv->empty_update_id);
        }

        if (burn->priv->start_monitor_id > 0) {
                g_source_remove (burn->priv->start_monitor_id);
        }

        if (burn->priv->burn_monitor != NULL) {
                g_file_monitor_cancel (burn->priv->burn_monitor);
        }

        if (burn->priv->widget_list != NULL) {
                g_slist_free (burn->priv->widget_list);
        }

        G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nautilus_disc_burn_class_init (NautilusDiscBurnClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        parent_class = g_type_class_peek_parent (klass);

        object_class->finalize = nautilus_disc_burn_finalize;

        g_type_class_add_private (klass, sizeof (NautilusDiscBurnPrivate));
}

static GType burn_type = 0;

static GType
nautilus_disc_burn_get_type (void)
{
        return burn_type;
}

static void
nautilus_disc_burn_register_type (GTypeModule *module)
{
        static const GTypeInfo info = {
                sizeof (NautilusDiscBurnClass),
                (GBaseInitFunc) NULL,
                (GBaseFinalizeFunc) NULL,
                (GClassInitFunc) nautilus_disc_burn_class_init,
                NULL,
                NULL,
                sizeof (NautilusDiscBurn),
                0,
                (GInstanceInitFunc) nautilus_disc_burn_instance_init,
        };

        static const GInterfaceInfo menu_provider_iface_info = {
                (GInterfaceInitFunc) nautilus_disc_burn_menu_provider_iface_init,
                NULL,
                NULL
        };
        static const GInterfaceInfo location_widget_provider_iface_info = {
                (GInterfaceInitFunc) nautilus_disc_burn_location_widget_provider_iface_init,
                NULL,
                NULL
        };

        burn_type = g_type_module_register_type (module,
                                                 G_TYPE_OBJECT,
                                                 "NautilusDiscBurn",
                                                 &info, 0);

        g_type_module_add_interface (module,
                                     burn_type,
                                     NAUTILUS_TYPE_MENU_PROVIDER,
                                     &menu_provider_iface_info);
        g_type_module_add_interface (module,
                                     burn_type,
                                     NAUTILUS_TYPE_LOCATION_WIDGET_PROVIDER,
                                     &location_widget_provider_iface_info);
}

void
nautilus_module_initialize (GTypeModule *module)
{
        DEBUG_PRINT ("Initializing nautilus-disc-recorder\n");
        nautilus_disc_burn_register_type (module);
}

void
nautilus_module_shutdown (void)
{
        DEBUG_PRINT ("Shutting down nautilus disc recorder\n");

        /* Don't do that in case another module would need the library */
        //brasero_media_library_stop ();
        //brasero_burn_library_stop ();
}

void
nautilus_module_list_types (const GType **types,
                            int          *num_types)
{
        static GType type_list [1];

        type_list [0] = NAUTILUS_TYPE_DISC_BURN;

        *types = type_list;
        *num_types = 1;
}
