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

#include <nautilus-extension.h>

#define BURN_URI        "burn:///"

#define NAUTILUS_TYPE_DISC_BURN  (nautilus_disc_burn_get_type ())
#define NAUTILUS_DISC_BURN(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), NAUTILUS_TYPE_DISC_BURN, NautilusDiscBurn))
#define NAUTILUS_IS_DISC_BURN(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), NAUTILUS_TYPE_DISC_BURN))

typedef struct {
  GObject parent;
} NautilusDiscBurn;

typedef struct {
  GObjectClass parent_class;
} NautilusDiscBurnClass;

typedef struct _NautilusDiscBurnPrivate NautilusDiscBurnPrivate;

struct _NautilusDiscBurnPrivate {
  GFileMonitor *burn_monitor;
  guint empty : 1;

  guint start_monitor_id;
  guint empty_update_id;
};

static GType nautilus_disc_burn_get_type (void);
static void  nautilus_disc_burn_register_type (GTypeModule *module);

static void
nautilus_disc_burn_menu_provider_iface_init (NautilusMenuProviderInterface *iface);

/*#define DEBUG_PRINT(format_MACRO,...)           g_print (format_MACRO, ##__VA_ARGS__); */
#define DEBUG_PRINT(format_MACRO, ...)

G_DEFINE_DYNAMIC_TYPE_EXTENDED (NautilusDiscBurn,
                                nautilus_disc_burn,
                                G_TYPE_OBJECT,
                                0,
                                G_ADD_PRIVATE_DYNAMIC (NautilusDiscBurn)
                                G_IMPLEMENT_INTERFACE_DYNAMIC (NAUTILUS_TYPE_MENU_PROVIDER,
                                                               nautilus_disc_burn_menu_provider_iface_init));

typedef struct {
  gboolean is_optical;
  gboolean is_blank;
  gboolean is_rewritable;
  gboolean has_data;
} OpticalDriveInfo;

static gboolean
get_optical_drive_info (const gchar      *device_path,
                        OpticalDriveInfo *info)
{
  g_autoptr (GDBusConnection) bus = NULL;
  g_autofree gchar *device_name = NULL;
  g_autofree gchar *block_path = NULL;
  g_autoptr (GVariant) block_result = NULL;
  g_autoptr (GVariant) drive_var = NULL;
  g_autoptr (GVariant) drive_result = NULL;
  g_autoptr (GVariant) props = NULL;
  const gchar *drive_path = NULL;
  GVariantDict dict;
  gboolean optical = FALSE;
  gboolean optical_blank = FALSE;
  guint32 num_data_tracks = 0;
  const gchar *media = NULL;

  bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);
  if (bus == NULL) {
    return FALSE;
  }

  device_name = g_path_get_basename (device_path);
  block_path = g_strconcat ("/org/freedesktop/UDisks2/block_devices/", device_name, NULL);
  if (!g_variant_is_object_path (block_path)) {
    return FALSE;
  }

  block_result = g_dbus_connection_call_sync (bus,
                                              "org.freedesktop.UDisks2",
                                              block_path,
                                              "org.freedesktop.DBus.Properties",
                                              "Get",
                                              g_variant_new ("(ss)",
                                                             "org.freedesktop.UDisks2.Block",
                                                             "Drive"),
                                              G_VARIANT_TYPE ("(v)"),
                                              G_DBUS_CALL_FLAGS_NONE,
                                              -1,
                                              NULL,
                                              NULL);
  if (block_result == NULL) {
    return FALSE;
  }

  g_variant_get (block_result, "(v)", &drive_var);
  if (!g_variant_is_of_type (drive_var, G_VARIANT_TYPE_OBJECT_PATH)) {
    return FALSE;
  }

  drive_path = g_variant_get_string (drive_var, NULL);
  if (drive_path == NULL || g_strcmp0 (drive_path, "/") == 0) {
    return FALSE;
  }

  drive_result = g_dbus_connection_call_sync (bus,
                                              "org.freedesktop.UDisks2",
                                              drive_path,
                                              "org.freedesktop.DBus.Properties",
                                              "GetAll",
                                              g_variant_new ("(s)",
                                                             "org.freedesktop.UDisks2.Drive"),
                                              G_VARIANT_TYPE ("(a{sv})"),
                                              G_DBUS_CALL_FLAGS_NONE,
                                              -1,
                                              NULL,
                                              NULL);
  if (drive_result == NULL) {
    return FALSE;
  }

  props = g_variant_get_child_value (drive_result, 0);
  g_variant_dict_init (&dict, props);
  g_variant_dict_lookup (&dict, "Optical", "b", &optical);
  g_variant_dict_lookup (&dict, "OpticalBlank", "b", &optical_blank);
  g_variant_dict_lookup (&dict, "OpticalNumDataTracks", "u", &num_data_tracks);
  g_variant_dict_lookup (&dict, "Media", "&s", &media);

  info->is_optical = optical;
  info->is_blank = optical_blank;
  info->has_data = (num_data_tracks > 0);
  info->is_rewritable = (media != NULL)
                        && (g_strcmp0 (media, "optical_cd_rw") == 0
                            || g_strcmp0 (media, "optical_dvd_rw") == 0
                            || g_strcmp0 (media, "optical_dvd_ram") == 0
                            || g_strcmp0 (media, "optical_dvd_plus_rw") == 0
                            || g_strcmp0 (media, "optical_dvd_plus_rw_dl") == 0
                            || g_strcmp0 (media, "optical_bd_re") == 0
                            || g_strcmp0 (media, "optical_hddvd_rw") == 0);

  g_variant_dict_clear (&dict);

  return TRUE;
}

static gboolean
nautilus_disc_burn_is_empty (void)
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
    DEBUG_PRINT ("Could not open burn uri %s: %s\n",
                 uri,
                 error->message);

    g_object_unref (file);
    g_error_free (error);
    return TRUE;
  }

  info = g_file_enumerator_next_file (enumerator, NULL, NULL);
  g_object_unref (enumerator);
  g_object_unref (file);

  if (!info) {
    return TRUE;
  }

  g_object_unref (info);
  return FALSE;
}

static void
write_disc_activate_cb (NautilusMenuItem *item,
                        gpointer          user_data)
{
  g_autoptr (GAppInfo) app_info = NULL;
  gchar *cmd_line = "brasero --ncb --immediately";

  app_info = g_app_info_create_from_commandline (cmd_line, NULL, 0, NULL);
  g_app_info_launch (app_info, NULL, NULL, NULL);
}

static void
write_iso_activate_cb (NautilusMenuItem *item,
                       gpointer          user_data)
{
  g_autoptr (GAppInfo) app_info = NULL;
  g_autofree gchar *cmd_line = NULL;
  NautilusFileInfo *file_info;
  g_autofree gchar *uri = NULL;

  file_info = g_object_get_data (G_OBJECT (item), "file_info");
  uri = nautilus_file_info_get_uri (file_info);

  cmd_line = g_strconcat ("brasero --image=", uri, NULL);
  app_info = g_app_info_create_from_commandline (cmd_line, NULL, 0, NULL);
  g_app_info_launch (app_info, NULL, NULL, NULL);
}

static void
copy_disc_activate_cb (NautilusMenuItem *item,
                       gpointer          user_data)
{
  const gchar *device_path;
  g_autoptr (GAppInfo) app_info = NULL;
  g_autofree gchar *cmd_line = NULL;

  device_path = g_object_get_data (G_OBJECT (item), "drive_device_path");

  cmd_line = g_strconcat ("brasero --copy=", device_path, NULL);
  app_info = g_app_info_create_from_commandline (cmd_line, NULL, 0, NULL);
  g_app_info_launch (app_info, NULL, NULL, NULL);
}

static void
blank_disc_activate_cb (NautilusMenuItem *item,
                        gpointer          user_data)
{
  g_autoptr (GAppInfo) app_info = NULL;
  g_autofree gchar *cmd_line = NULL;
  const gchar *device_path;

  device_path = g_object_get_data (G_OBJECT (item), "drive_device_path");

  cmd_line = g_strconcat ("brasero --blank=", device_path, NULL);
  app_info = g_app_info_create_from_commandline (cmd_line, NULL, 0, NULL);
  g_app_info_launch (app_info, NULL, NULL, NULL);
}

static void
check_disc_activate_cb (NautilusMenuItem *item,
                        gpointer          user_data)
{
  g_autoptr (GAppInfo) app_info = NULL;
  g_autofree gchar *cmd_line = NULL;
  const gchar *device_path;

  device_path = g_object_get_data (G_OBJECT (item), "drive_device_path");

  cmd_line = g_strconcat ("brasero --check=", device_path, NULL);
  app_info = g_app_info_create_from_commandline (cmd_line, NULL, 0, NULL);
  g_app_info_launch (app_info, NULL, NULL, NULL);
}

static GVolume *
drive_get_first_volume (GDrive *drive)
{
  GVolume *volume;
  GList *volumes;

  volumes = g_drive_get_volumes (drive);

  volume = g_list_nth_data (volumes, 0);

  if (volume != NULL) {
    g_object_ref (volume);
  }

  g_list_foreach (volumes, (GFunc)g_object_unref, NULL);
  g_list_free (volumes);

  return volume;
}

static GList *
nautilus_disc_burn_get_file_items (NautilusMenuProvider *provider,
                                   GList                *selection)
{
  GList *items = NULL;
  NautilusMenuItem *item;
  NautilusFileInfo *file_info;
  GFile *file;
  GMount *mount;
  GVolume *volume;
  GDrive *drive;
  char *mime_type;
  gboolean is_iso;

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
  if (!mime_type) {
    g_object_unref (file);
    return NULL;
  }

  is_iso = (strcmp (mime_type, "application/x-iso-image") == 0)
           || (strcmp (mime_type, "application/vnd.efi.iso") == 0)
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
    g_signal_connect (item, "activate",
                      G_CALLBACK (write_iso_activate_cb), NULL);
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

  if (drive != NULL && volume != NULL) {
    g_autofree gchar *device_path = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
    OpticalDriveInfo info = { 0 };

    if (device_path != NULL
        && get_optical_drive_info (device_path, &info)
        && info.is_optical
        && !info.is_blank) {
      /* user may want to copy it ... */
      item = nautilus_menu_item_new ("NautilusDiscBurn::copy_disc",
                                     _("_Copy Disc…"),
                                     _("Create a copy of this CD or DVD"),
                                     "media-optical-copy");
      g_object_set_data (G_OBJECT (item), "file_info", file_info);
      g_object_set_data_full (G_OBJECT (item), "drive_device_path", g_strdup (device_path), g_free);
      g_signal_connect (item, "activate", G_CALLBACK (copy_disc_activate_cb), NULL);
      items = g_list_append (items, item);

      if (info.is_rewritable) {
        /* ... or if it's a rewritable medium to blank it ... */
        item = nautilus_menu_item_new ("NautilusDiscBurn::blank_disc",
                                       _("_Blank Disc…"),
                                       _("Blank this CD or DVD"),
                                       "media-optical-blank");
        g_object_set_data (G_OBJECT (item), "file_info", file_info);
        g_object_set_data_full (G_OBJECT (item), "drive_device_path", g_strdup (device_path), g_free);
        g_signal_connect (item, "activate",
                          G_CALLBACK (blank_disc_activate_cb), NULL);
        items = g_list_append (items, item);
      }

      if (info.has_data) {
        /* ... or verify medium. */
        item = nautilus_menu_item_new ("NautilusDiscBurn::check_disc",
                                       _("_Check Disc…"),
                                       _("Check the data integrity on this CD or DVD"),
                                       NULL);
        g_object_set_data (G_OBJECT (item), "file_info", file_info);
        g_object_set_data_full (G_OBJECT (item), "drive_device_path", g_strdup (device_path), g_free);
        g_signal_connect (item, "activate",
                          G_CALLBACK (check_disc_activate_cb),
                          NULL);
        items = g_list_append (items, item);
      }
    }
  }

  g_object_unref (file);

  if (mount != NULL) {
    g_object_unref (mount);
  }

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
                                         NautilusFileInfo     *current_folder)
{
  NautilusDiscBurn *self = NAUTILUS_DISC_BURN (provider);
  NautilusDiscBurnPrivate *priv = nautilus_disc_burn_get_instance_private (self);
  GList *items;
  char *scheme;

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
    g_signal_connect_swapped (item, "activate",
                              G_CALLBACK (write_disc_activate_cb),
                              NULL);
    items = g_list_append (items, item);

    g_object_set (item, "sensitive", !priv->empty, NULL);
  }

  g_free (scheme);

  return items;
}

static void
nautilus_disc_burn_menu_provider_iface_init (NautilusMenuProviderInterface *iface)
{
  iface->get_file_items = nautilus_disc_burn_get_file_items;
  iface->get_background_items = nautilus_disc_burn_get_background_items;
}

static gboolean
update_empty_idle (NautilusDiscBurn *burn)
{
  NautilusDiscBurnPrivate *priv = nautilus_disc_burn_get_instance_private (burn);
  gboolean is_empty;

  priv->empty_update_id = 0;

  is_empty = nautilus_disc_burn_is_empty ();

  DEBUG_PRINT ("Dir is %s\n", is_empty ? "empty" : "not empty");

  if (priv->empty != is_empty) {
    priv->empty = is_empty;

    /* Trigger update for menu items */
    nautilus_menu_provider_emit_items_updated_signal (NAUTILUS_MENU_PROVIDER (burn));
  }

  return FALSE;
}

static void
queue_update_empty (NautilusDiscBurn *burn)
{
  NautilusDiscBurnPrivate *priv = nautilus_disc_burn_get_instance_private (burn);

  if (priv->empty_update_id != 0) {
    g_source_remove (priv->empty_update_id);
  }

  priv->empty_update_id = g_idle_add ((GSourceFunc)update_empty_idle, burn);
}

static void
burn_monitor_cb (GFileMonitor      *monitor,
                 GFile             *file,
                 GFile             *other_file,
                 GFileMonitorEvent  event_type,
                 NautilusDiscBurn  *burn)
{
  NautilusDiscBurnPrivate *priv = nautilus_disc_burn_get_instance_private (burn);

  DEBUG_PRINT ("Monitor callback type %d\n", event_type);

  /* only queue the action if it has a chance of changing the state */
  if (event_type == G_FILE_MONITOR_EVENT_CREATED) {
    if (priv->empty) {
      queue_update_empty (burn);
    }
  } else if (event_type == G_FILE_MONITOR_EVENT_DELETED) {
    if (!priv->empty) {
      queue_update_empty (burn);
    }
  }
}

static gboolean
start_monitor (NautilusDiscBurn *burn)
{
  NautilusDiscBurnPrivate *priv = nautilus_disc_burn_get_instance_private (burn);
  GFile *file;
  GError *error;

  file = g_file_new_for_uri (BURN_URI);

  error = NULL;
  priv->burn_monitor = g_file_monitor_directory (file,
                                                 G_FILE_MONITOR_NONE,
                                                 NULL,
                                                 &error);
  if (priv->burn_monitor == NULL) {
    DEBUG_PRINT ("Unable to add monitor: %s", error->message);
    g_error_free (error);
    goto out;
  }

  DEBUG_PRINT ("Starting monitor for %s\n", BURN_URI);
  g_signal_connect (priv->burn_monitor,
                    "changed",
                    G_CALLBACK (burn_monitor_cb),
                    burn);

  priv->empty = nautilus_disc_burn_is_empty ();

  DEBUG_PRINT ("Init burn extension, empty: %d\n", priv->empty);

out:
  g_object_unref (file);

  priv->start_monitor_id = 0;

  return FALSE;
}

static void
nautilus_disc_burn_init (NautilusDiscBurn *burn)
{
  NautilusDiscBurnPrivate *priv = nautilus_disc_burn_get_instance_private (burn);

  priv->start_monitor_id = g_timeout_add_seconds (1,
                                                  (GSourceFunc)start_monitor,
                                                  burn);
}

static void
nautilus_disc_burn_finalize (GObject *object)
{
  NautilusDiscBurn *self = NAUTILUS_DISC_BURN (object);
  NautilusDiscBurnPrivate *priv = nautilus_disc_burn_get_instance_private (self);

  DEBUG_PRINT ("Finalizing burn extension\n");

  if (priv->empty_update_id > 0) {
    g_source_remove (priv->empty_update_id);
  }

  if (priv->start_monitor_id > 0) {
    g_source_remove (priv->start_monitor_id);
  }

  if (priv->burn_monitor != NULL) {
    g_file_monitor_cancel (priv->burn_monitor);
  }

  G_OBJECT_CLASS (nautilus_disc_burn_parent_class)->finalize (object);
}

static void
nautilus_disc_burn_class_init (NautilusDiscBurnClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = nautilus_disc_burn_finalize;
}

static void
nautilus_disc_burn_class_finalize (NautilusDiscBurnClass *klass)
{
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
  /*brasero_media_library_stop (); */
  /*brasero_burn_library_stop (); */
}

void
nautilus_module_list_types (const GType **types,
                            int          *num_types)
{
  static GType type_list[1];

  type_list [0] = NAUTILUS_TYPE_DISC_BURN;

  *types = type_list;
  *num_types = 1;
}
