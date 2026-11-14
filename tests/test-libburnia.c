/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-burn unit tests for libburnia default engine & plugins
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include "brasero-burn-lib.h"
#include "brasero-enums.h"
#include "brasero-plugin.h"
#include "brasero-plugin-information.h"
#include "brasero-plugin-private.h"
#include "brasero-plugin-registration.h"
#include "brasero-session.h"
#include "brasero-session-helper.h"
#include "brasero-tags.h"
#include "brasero-track-disc.h"
#include "burn-caps.h"
#include "burn-job.h"
#include "burn-plugin-manager.h"
#include "burn-task.h"

#define BRASERO_SCHEMA_CONFIG "org.gnome.brasero.config"
#define BRASERO_ENGINE_GROUP_KEY "engine-group"

static BraseroPlugin *
load_test_plugin (const gchar *plugin_name,
                  const gchar *soname)
{
  BraseroPluginManager *manager;
  GSList *plugins;
  GSList *iter;
  gchar *path;
  BraseroPlugin *plugin;

  manager = brasero_plugin_manager_get_default ();
  plugins = brasero_plugin_manager_get_plugins_list (manager);
  for (iter = plugins; iter; iter = iter->next) {
    BraseroPlugin *loaded = BRASERO_PLUGIN (iter->data);
    if (g_strcmp0 (brasero_plugin_get_name (loaded), plugin_name) == 0) {
      g_slist_free (plugins);
      return loaded;
    }
  }
  g_slist_free (plugins);

  path = g_build_filename (TEST_PLUGIN_DIR, soname, NULL);
  g_assert_true (g_file_test (path, G_FILE_TEST_EXISTS));

  plugin = brasero_plugin_new (path);
  g_free (path);

  g_assert_nonnull (plugin);
  g_assert_true (brasero_plugin_get_gtype (plugin) != G_TYPE_NONE);
  return plugin;
}

static void
test_default_engine_group (void)
{
  GSettings *settings;
  gchar *engine_group;
  BraseroBurnCaps *caps;
  BraseroPlugin *plugin;

  settings = g_settings_new (BRASERO_SCHEMA_CONFIG);
  engine_group = g_settings_get_string (settings, BRASERO_ENGINE_GROUP_KEY);
  g_assert_cmpstr (engine_group, ==, "libburnia burning suite");
  g_free (engine_group);

  caps = brasero_burn_caps_get_default ();
  g_assert_nonnull (caps);

  /* Verify group registration matches the default engine group */
  plugin = g_object_new (BRASERO_TYPE_PLUGIN, NULL);
  brasero_plugin_register_group (plugin, "libburnia burning suite");
  g_assert_cmpuint (brasero_plugin_get_group (plugin), !=, 0);

  g_object_unref (caps);
  g_object_unref (settings);
}

#ifdef HAVE_LIBBURN
static void
test_libburn_caps (void)
{
  BraseroBurnCaps *caps;
  BraseroPlugin *plugin;
  BraseroBurnFlag supported = BRASERO_BURN_FLAG_NONE;
  BraseroBurnFlag compulsory = BRASERO_BURN_FLAG_NONE;
  gboolean res;

  caps = brasero_burn_caps_get_default ();
  plugin = load_test_plugin ("libburn", "libbrasero-libburn.so");

  g_assert_cmpstr (brasero_plugin_get_name (plugin), ==, "libburn");
  g_assert_cmpuint (brasero_plugin_get_priority (plugin), ==, 15);
  g_assert_cmpint (brasero_plugin_can_burn (plugin), ==, BRASERO_BURN_OK);

  /* CD-R and CD-RW should support BRASERO_BURN_FLAG_DUMMY */
  res = brasero_plugin_get_record_flags (plugin,
                                         BRASERO_MEDIUM_CDR |
                                         BRASERO_MEDIUM_BLANK,
                                         BRASERO_BURN_FLAG_NONE,
                                         &supported,
                                         &compulsory);
  g_assert_true (res);
  g_assert_true ((supported & BRASERO_BURN_FLAG_DUMMY) != 0);

  supported = BRASERO_BURN_FLAG_NONE;
  compulsory = BRASERO_BURN_FLAG_NONE;
  res = brasero_plugin_get_record_flags (plugin,
                                         BRASERO_MEDIUM_CDRW |
                                         BRASERO_MEDIUM_BLANK,
                                         BRASERO_BURN_FLAG_NONE,
                                         &supported,
                                         &compulsory);
  g_assert_true (res);
  g_assert_true ((supported & BRASERO_BURN_FLAG_DUMMY) != 0);

  /* DVD-RAM blanking support */
  supported = BRASERO_BURN_FLAG_NONE;
  compulsory = BRASERO_BURN_FLAG_NONE;
  res = brasero_plugin_get_blank_flags (plugin,
                                        BRASERO_MEDIUM_DVD |
                                        BRASERO_MEDIUM_RAM |
                                        BRASERO_MEDIUM_APPENDABLE |
                                        BRASERO_MEDIUM_CLOSED |
                                        BRASERO_MEDIUM_HAS_DATA,
                                        BRASERO_BURN_FLAG_NONE,
                                        &supported,
                                        &compulsory);
  g_assert_true (res);

  g_object_unref (caps);
}

static void
set_uint64_tag (BraseroTrack *track,
                const gchar  *tag,
                guint64       val)
{
  GValue *value;

  value = g_new0 (GValue, 1);
  g_value_init (value, G_TYPE_UINT64);
  g_value_set_uint64 (value, val);
  brasero_track_tag_add (track, tag, value);
}

static void
test_libburn_read_caps_and_size (void)
{
  BraseroBurnCaps *caps;
  BraseroPlugin *plugin;
  GType job_type;
  BraseroJob *job;
  BraseroBurnSession *session;
  BraseroTrackDisc *track;
  BraseroTask *task;
  GError *error = NULL;
  goffset blocks = 0;
  goffset bytes = 0;
  BraseroBurnResult res;

  caps = brasero_burn_caps_get_default ();
  plugin = load_test_plugin ("libburn-read", "libbrasero-libburn-read.so");

  g_assert_cmpstr (brasero_plugin_get_name (plugin), ==, "libburn-read");
  g_assert_cmpuint (brasero_plugin_get_priority (plugin), ==, 15);
  g_assert_cmpint (brasero_plugin_can_image (plugin), ==, BRASERO_BURN_OK);

  g_assert_true (g_type_module_use (G_TYPE_MODULE (plugin)));
  job_type = brasero_plugin_get_gtype (plugin);

  /* Test valid START/END sector boundary calculation */
  job = BRASERO_JOB (g_object_new (job_type, NULL));
  session = brasero_burn_session_new ();
  track = brasero_track_disc_new ();
  set_uint64_tag (BRASERO_TRACK (track), BRASERO_TRACK_MEDIUM_ADDRESS_START_TAG, 100);
  set_uint64_tag (BRASERO_TRACK (track), BRASERO_TRACK_MEDIUM_ADDRESS_END_TAG, 350);
  brasero_burn_session_add_track (session, BRASERO_TRACK (track), NULL);
  brasero_burn_session_start (session);

  task = BRASERO_TASK (g_object_new (BRASERO_TYPE_TASK,
                                     "session", session,
                                     "action", BRASERO_TASK_ACTION_NORMAL,
                                     NULL));
  brasero_task_add_item (task, BRASERO_TASK_ITEM (job));

  res = brasero_task_check (task, &error);
  g_assert_no_error (error);
  g_assert_cmpint (res, ==, BRASERO_BURN_OK);

  brasero_task_ctx_get_session_output_size (BRASERO_TASK_CTX (task), &blocks, &bytes);
  g_assert_cmpint (blocks, ==, 250);
  g_assert_cmpint (bytes, ==, 250 * 2048);

  brasero_burn_session_stop (session);
  g_object_unref (task);
  g_object_unref (track);
  g_object_unref (session);
  g_object_unref (job);

  /* Test missing END_TAG returns error cleanly instead of crashing */
  job = BRASERO_JOB (g_object_new (job_type, NULL));
  session = brasero_burn_session_new ();
  track = brasero_track_disc_new ();
  set_uint64_tag (BRASERO_TRACK (track), BRASERO_TRACK_MEDIUM_ADDRESS_START_TAG, 100);
  brasero_burn_session_add_track (session, BRASERO_TRACK (track), NULL);
  brasero_burn_session_start (session);

  task = BRASERO_TASK (g_object_new (BRASERO_TYPE_TASK,
                                     "session", session,
                                     "action", BRASERO_TASK_ACTION_NORMAL,
                                     NULL));
  brasero_task_add_item (task, BRASERO_TASK_ITEM (job));

  res = brasero_task_check (task, &error);
  g_assert_cmpint (res, ==, BRASERO_BURN_ERR);
  g_assert_nonnull (error);
  g_clear_error (&error);

  brasero_burn_session_stop (session);
  g_object_unref (task);
  g_object_unref (track);
  g_object_unref (session);
  g_object_unref (job);

  g_type_module_unuse (G_TYPE_MODULE (plugin));
  g_object_unref (caps);
}
#endif

#ifdef HAVE_LIBISOFS
static void
test_libisofs_caps (void)
{
  BraseroBurnCaps *caps;
  BraseroPlugin *plugin;
  BraseroBurnFlag supported = BRASERO_BURN_FLAG_NONE;
  BraseroBurnFlag compulsory = BRASERO_BURN_FLAG_NONE;
  gboolean res;

  caps = brasero_burn_caps_get_default ();
  plugin = load_test_plugin ("libisofs", "libbrasero-libisofs.so");

  g_assert_cmpstr (brasero_plugin_get_name (plugin), ==, "libisofs");
  g_assert_cmpint (brasero_plugin_can_image (plugin), ==, BRASERO_BURN_OK);

  /* BD-R SRM appendable support */
  res = brasero_plugin_get_image_flags (plugin,
                                        BRASERO_MEDIUM_BDR_SRM |
                                        BRASERO_MEDIUM_APPENDABLE |
                                        BRASERO_MEDIUM_HAS_DATA,
                                        BRASERO_BURN_FLAG_NONE,
                                        &supported,
                                        &compulsory);
  g_assert_true (res);
  g_assert_true ((supported & BRASERO_BURN_FLAG_APPEND) != 0);

  /* BD-RE support */
  supported = BRASERO_BURN_FLAG_NONE;
  res = brasero_plugin_get_image_flags (plugin,
                                        BRASERO_MEDIUM_BDRE |
                                        BRASERO_MEDIUM_CLOSED |
                                        BRASERO_MEDIUM_HAS_DATA,
                                        BRASERO_BURN_FLAG_NONE,
                                        &supported,
                                        &compulsory);
  g_assert_true (res);
  g_assert_true ((supported & BRASERO_BURN_FLAG_MERGE) != 0);

  /* DVD-RAM support */
  supported = BRASERO_BURN_FLAG_NONE;
  res = brasero_plugin_get_image_flags (plugin,
                                        BRASERO_MEDIUM_DVD |
                                        BRASERO_MEDIUM_RAM |
                                        BRASERO_MEDIUM_CLOSED |
                                        BRASERO_MEDIUM_HAS_DATA,
                                        BRASERO_BURN_FLAG_NONE,
                                        &supported,
                                        &compulsory);
  g_assert_true (res);
  g_assert_true ((supported & BRASERO_BURN_FLAG_APPEND) != 0);

  g_object_unref (caps);
}
#endif

int
main (int   argc,
      char *argv[])
{
  int res;

  g_test_init (&argc, &argv, NULL);
  g_assert_true (gtk_init_check (&argc, &argv));
  brasero_burn_library_start (&argc, &argv);

  g_test_add_func ("/libburnia/default-engine-group", test_default_engine_group);
#ifdef HAVE_LIBBURN
  g_test_add_func ("/libburnia/libburn-caps", test_libburn_caps);
  g_test_add_func ("/libburnia/libburn-read-caps-and-size", test_libburn_read_caps_and_size);
#endif
#ifdef HAVE_LIBISOFS
  g_test_add_func ("/libburnia/libisofs-caps", test_libisofs_caps);
#endif

  res = g_test_run ();
  return res;
}
