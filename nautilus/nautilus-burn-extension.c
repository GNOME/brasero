/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8 -*-
 *
 * Copyright (C) 2003 Novell, Inc.
 * Copyright (C) 2003-2004 Red Hat, Inc.
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2008 Philippe Rouquier <bonfire-app@wanadoo.fr> (modified to work with brasero)
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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
#include "nautilus-burn-bar.h"

#define BURN_URI "burn:///"

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
};

static GType nautilus_disc_burn_get_type      (void);
static void  nautilus_disc_burn_register_type (GTypeModule *module);

static GObjectClass *parent_class;

#undef DEBUG_ENABLE

#ifdef DEBUG_ENABLE
#ifdef G_HAVE_ISO_VARARGS
#  define DEBUG_PRINT(...) debug_print (__VA_ARGS__);
#elif defined(G_HAVE_GNUC_VARARGS)
#  define DEBUG_PRINT(args...) debug_print (args);
#endif
#else
#ifdef G_HAVE_ISO_VARARGS
#  define DEBUG_PRINT(...)
#elif defined(G_HAVE_GNUC_VARARGS)
#  define DEBUG_PRINT(args...)
#endif
#endif

#ifdef DEBUG_ENABLE
static FILE *debug_out = NULL;

static void
debug_init (void)
{
        const char path [50] = "burn_extension_debug_XXXXXX";
        int  fd = g_file_open_tmp (path, NULL, NULL);
        if (fd >= 0) {
                debug_out = fdopen (fd, "a");
        }
}

static void
debug_print (const char *format, ...)
{
        va_list args;

        va_start (args, format);
        vfprintf ((debug_out ? debug_out : stderr), format, args);
        vfprintf (stderr, format, args);
        va_end (args);
        if (debug_out)
                fflush (debug_out);
}
#endif

static void
launch_process (char **argv, gint next_arg, GtkWindow *parent)
{
        GtkWidget *dialog;
        GError *error;

        if (parent && GTK_WIDGET (parent)->window) {
                guint xid;

		xid = gdk_x11_drawable_get_xid (GDK_DRAWABLE (GTK_WIDGET (parent)->window));
                if (xid > 0) {
                        argv [next_arg++] = g_strdup ("-x");
                        argv [next_arg] = g_strdup_printf ("%d", xid);
                }
        }

        error = NULL;
        if (!g_spawn_async (NULL,
                            argv, NULL,
                            0,
                            NULL, NULL,
                            NULL,
                            &error)) {

                dialog = gtk_message_dialog_new (parent,
                                                 GTK_DIALOG_MODAL,
                                                 GTK_MESSAGE_WARNING,
                                                 GTK_BUTTONS_OK,
                                                 _("Unable to launch the cd burner application"));

                gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                          "%s",
                                                          error->message);

                gtk_dialog_run (GTK_DIALOG (dialog));
                gtk_widget_destroy (dialog);


                g_error_free (error);
        }
}

static void
launch_brasero_on_window (GtkWindow *window)
{
        int i;
        char *argv [5] = { NULL, };

        argv [0] = g_build_filename (BINDIR, "brasero", NULL);
        argv [1] = g_strdup ("-n");

        launch_process (argv, 2, window);
	for (i = 0; argv [i]; i++)
		g_free (argv [i]);
}

static void
write_activate_cb (NautilusMenuItem *item,
                   gpointer          user_data)
{
        launch_brasero_on_window (GTK_WINDOW (user_data));
}

static char *
uri_to_path (const char *uri)
{
        GFile *file;
        char  *path;

        file = g_file_new_for_uri (uri);
        path = g_file_get_path (file);
        g_object_unref (file);
        return path;
}

static void
write_iso_activate_cb (NautilusMenuItem *item,
                       gpointer          user_data)
{
        NautilusFileInfo *file_info;
        char             *argv [6] = { NULL, };
        char             *uri;
        char             *image_name;
        int               i;

        file_info = g_object_get_data (G_OBJECT (item), "file_info");

        uri = nautilus_file_info_get_uri (file_info);
        image_name = uri_to_path (uri);

        if (image_name == NULL) {
                g_warning ("Can not get local path for URI %s", uri);
                g_free (uri);
                return;
        }

        g_free (uri);

        argv [0] = g_build_filename (BINDIR, "brasero", NULL);
        argv [1] = g_strdup ("-i");
        argv [2] = image_name;

        launch_process (argv, 3, GTK_WINDOW (user_data));

	for (i = 0; argv [i]; i++)
		g_free (argv [i]);
}

static void
copy_disc_activate_cb (NautilusMenuItem *item,
                       gpointer          user_data)
{
        int               i;
        char             *argv [6] = { NULL, };
        char             *device_path;

        device_path = g_object_get_data (G_OBJECT (item), "drive_device_path");

        if (!device_path) {
                g_warning ("Drive device path not specified");
                return;
        }

        argv [0] = g_build_filename (BINDIR, "brasero", NULL);
        argv [1] = g_strdup ("-c");
        argv [2] = device_path;

        launch_process (argv, 3, GTK_WINDOW (user_data));

	for (i = 0; argv [i]; i++)
		g_free (argv [i]);
}

static void
blank_disc_activate_cb (NautilusMenuItem *item,
                        gpointer          user_data)
{
        int               i;
        char             *argv [6]= { NULL, };
        char             *device_path;

        device_path = g_object_get_data (G_OBJECT (item), "drive_device_path");

        if (!device_path) {
                g_warning ("Drive device path not specified");
                return;
        }

        argv [0] = g_build_filename (BINDIR, "brasero", NULL);
        argv [1] = g_strdup ("-b");
        argv [2] = device_path;

        launch_process (argv, 3, GTK_WINDOW (user_data));

	for (i = 0; argv [i]; i++)
		g_free (argv [i]);
}

static void
check_disc_activate_cb (NautilusMenuItem *item,
                        gpointer          user_data)
{
        int               i;
        char             *argv [6] = { NULL, };
        char             *device_path;

        device_path = g_object_get_data (G_OBJECT (item), "drive_device_path");

        if (!device_path) {
                g_warning ("Drive device path not specified");
                return;
        }

        argv [0] = g_build_filename (BINDIR, "brasero", NULL);
        argv [1] = g_strdup ("-k");
        argv [2] = device_path;

        launch_process (argv, 3, GTK_WINDOW (user_data));

	for (i = 0; argv [i]; i++)
		g_free (argv [i]);
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
                                               _("_Write to Disc..."),
                                               _("Write disc image to a CD or DVD disc"),
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
                char *device_path;

                device_path = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);

                /* user may want to copy it ... */
                item = nautilus_menu_item_new ("NautilusDiscBurn::copy_disc",
                                               _("_Copy Disc..."),
                                               _("Create a copy of this CD or DVD disc"),
                                               "media-optical-copy");
                g_object_set_data (G_OBJECT (item), "file_info", file_info);
                g_object_set_data (G_OBJECT (item), "window", window);
                g_object_set_data_full (G_OBJECT (item), "drive_device_path", g_strdup (device_path), g_free);
                g_signal_connect (item, "activate",
                                  G_CALLBACK (copy_disc_activate_cb), window);
                items = g_list_append (items, item);

                /* ... or if it's a rewritable medium to blank it ... */
                item = nautilus_menu_item_new ("NautilusDiscBurn::blank_disc",
                                               _("_Blank Disc..."),
                                               _("Blank this CD or DVD disc"),
                                               "media-optical-blank");
                g_object_set_data (G_OBJECT (item), "file_info", file_info);
                g_object_set_data (G_OBJECT (item), "window", window);
                g_object_set_data_full (G_OBJECT (item), "drive_device_path", g_strdup (device_path), g_free);
                g_signal_connect (item, "activate",
                                  G_CALLBACK (blank_disc_activate_cb), window);
                items = g_list_append (items, item);

                /* ... or verify medium. */
                item = nautilus_menu_item_new ("NautilusDiscBurn::check_disc",
                                               _("_Check Disc..."),
                                               _("Check the data integrity on this CD or DVD disc"),
                                               NULL);
                g_object_set_data (G_OBJECT (item), "file_info", file_info);
                g_object_set_data (G_OBJECT (item), "window", window);
                g_object_set_data_full (G_OBJECT (item), "drive_device_path", g_strdup (device_path), g_free);
                g_signal_connect (item, "activate",
                                  G_CALLBACK (check_disc_activate_cb), window);
                items = g_list_append (items, item);

                g_free (device_path);
        }

        if (drive != NULL) {
                g_object_unref (drive);
        }
        if (volume != NULL) {
                g_object_unref (volume);
        }

        g_free (mime_type);

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

        if (strcmp (scheme, "burn") == 0) {
                NautilusMenuItem *item;

                item = nautilus_menu_item_new ("NautilusDiscBurn::write_menu",
                                               _("_Write to Disc..."),
                                               _("Write contents to a CD or DVD disc"),
                                               "brasero");
                g_signal_connect (item, "activate",
                                  G_CALLBACK (write_activate_cb),
                                  window);
                items = g_list_append (items, item);

                g_object_set (item, "sensitive", ! NAUTILUS_DISC_BURN (provider)->priv->empty, NULL);
        }

        g_free (scheme);

        return items;
}

static GList *
nautilus_disc_burn_get_toolbar_items (NautilusMenuProvider *provider,
                                      GtkWidget            *window,
                                      NautilusFileInfo     *current_folder)
{
        GList *items;

        items = NULL;

        return items;
}

static void
nautilus_disc_burn_menu_provider_iface_init (NautilusMenuProviderIface *iface)
{
        iface->get_file_items = nautilus_disc_burn_get_file_items;
        iface->get_background_items = nautilus_disc_burn_get_background_items;
        iface->get_toolbar_items = nautilus_disc_burn_get_toolbar_items;
}

static void
bar_activated_cb (NautilusDiscBurnBar *bar,
                  gpointer         data)
{
        launch_brasero_on_window (GTK_WINDOW (data));
}

static gboolean
dir_is_empty (const char *uri)
{
        GFile           *file;
        GFileEnumerator *enumerator;
        GError          *error;
        gboolean         found_file;

        file = g_file_new_for_uri (uri);

        error = NULL;
        enumerator = g_file_enumerate_children (file,
                                                G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                                G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                                G_FILE_ATTRIBUTE_ACCESS_CAN_READ,
                                                0,
                                                NULL,
                                                &error);
        if (enumerator == NULL) {
                DEBUG_PRINT ("Could not open burn uri %s: %s\n",
                             uri,
                             error->message);
                g_error_free (error);
                return TRUE;
        }

        found_file = FALSE;

        while (TRUE) {
                GFileInfo *info;

                info = g_file_enumerator_next_file (enumerator, NULL, NULL);
                if (info == NULL) {
                        break;
                }

                g_object_unref (info);

                found_file = TRUE;
                break;
        }

        g_object_unref (enumerator);
        g_object_unref (file);

        return !found_file;
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

                sense_widget (burn, nautilus_disc_burn_bar_get_button (NAUTILUS_DISC_BURN_BAR (bar)));

                g_signal_connect (bar, "activate",
                                  G_CALLBACK (bar_activated_cb),
                                  window);

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

        is_empty = dir_is_empty (BURN_URI);

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
                g_warning ("Unable to add monitor: %s", error->message);
                g_error_free (error);
                goto out;
        }

        DEBUG_PRINT ("Starting monitor for %s\n", BURN_URI);
        g_signal_connect (burn->priv->burn_monitor,
                          "changed",
                          G_CALLBACK (burn_monitor_cb),
                          burn);

        burn->priv->empty = dir_is_empty (BURN_URI);

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

#ifdef DEBUG_ENABLE
        debug_init ();
#endif

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
        brasero_media_library_start ();

        nautilus_disc_burn_register_type (module);
}

void
nautilus_module_shutdown (void)
{
        /* Don't do that in case another module would need the library */
        //brasero_media_library_stop ();
}

void
nautilus_module_list_types (const GType **types,
                            int          *num_types)
{
        static GType type_list [1];

        type_list[0] = NAUTILUS_TYPE_DISC_BURN;

        *types = type_list;
        *num_types = 1;
}
