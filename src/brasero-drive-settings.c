/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Brasero
 * Copyright (C) Philippe Rouquier 2005-2010 <bonfire-app@wanadoo.fr>
 * 
 *  Brasero is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 * brasero is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with brasero.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>

#include "brasero-drive-settings.h"
#include "brasero-session.h"
#include "brasero-session-helper.h"
#include "brasero-drive-properties.h"

typedef struct _BraseroDriveSettingsPrivate BraseroDriveSettingsPrivate;
struct _BraseroDriveSettingsPrivate
{
	BraseroMedia dest_media;
	BraseroDrive *dest_drive;
	BraseroTrackType *src_type;

	GSettings *settings;
	GSettings *config_settings;
	BraseroBurnSession *session;
};

#define BRASERO_DRIVE_SETTINGS_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_DRIVE_SETTINGS, BraseroDriveSettingsPrivate))

#define BRASERO_SCHEMA_DRIVES			"org.gnome.brasero.drives"
#define BRASERO_DRIVE_PROPERTIES_PATH		"/org/gnome/brasero/drives/"
#define BRASERO_PROPS_FLAGS			"flags"
#define BRASERO_PROPS_SPEED			"speed"

#define BRASERO_SCHEMA_CONFIG			"org.gnome.brasero.config"
#define BRASERO_PROPS_TMP_DIR			"tmpdir"

#define BRASERO_DEST_SAVED_FLAGS		(BRASERO_DRIVE_PROPERTIES_FLAGS|BRASERO_BURN_FLAG_MULTI)

G_DEFINE_TYPE (BraseroDriveSettings, brasero_drive_settings, G_TYPE_OBJECT);

static GVariant *
brasero_drive_settings_set_mapping_speed (const GValue *value,
                                          const GVariantType *variant_type,
                                          gpointer user_data)
{
	return g_variant_new_int32 (g_value_get_int64 (value) / 1000);
}

static gboolean
brasero_drive_settings_get_mapping_speed (GValue *value,
                                          GVariant *variant,
                                          gpointer user_data)
{
	if (!g_variant_get_int32 (variant)) {
		BraseroDriveSettingsPrivate *priv;
		BraseroMedium *medium;
		BraseroDrive *drive;

		priv = BRASERO_DRIVE_SETTINGS_PRIVATE (user_data);
		drive = brasero_burn_session_get_burner (priv->session);
		medium = brasero_drive_get_medium (drive);

		/* Must not be NULL since we only bind when a medium is available */
		g_assert (medium != NULL);

		g_value_set_int64 (value, brasero_medium_get_max_write_speed (medium));
	}
	else
		g_value_set_int64 (value, g_variant_get_int32 (variant) * 1000);

	return TRUE;
}

static GVariant *
brasero_drive_settings_set_mapping_flags (const GValue *value,
                                          const GVariantType *variant_type,
                                          gpointer user_data)
{
	return g_variant_new_int32 (g_value_get_int (value) & BRASERO_DEST_SAVED_FLAGS);
}

static gboolean
brasero_drive_settings_get_mapping_flags (GValue *value,
                                          GVariant *variant,
                                          gpointer user_data)
{
	BraseroBurnFlag flags;
	BraseroBurnFlag current_flags;
	BraseroDriveSettingsPrivate *priv;

	priv = BRASERO_DRIVE_SETTINGS_PRIVATE (user_data);

	flags = g_variant_get_int32 (variant);
	if (brasero_burn_session_same_src_dest_drive (priv->session)) {
		/* Special case */
		if (flags == 1) {
			flags = BRASERO_BURN_FLAG_EJECT|
				BRASERO_BURN_FLAG_BURNPROOF;
		}
		else
			flags &= BRASERO_DEST_SAVED_FLAGS;

		flags |= BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE|
			 BRASERO_BURN_FLAG_FAST_BLANK;
	}
	/* This is for the default value when the user has never used it */
	else if (flags == 1) {
		BraseroTrackType *source;

		flags = BRASERO_BURN_FLAG_EJECT|
			BRASERO_BURN_FLAG_BURNPROOF;

		source = brasero_track_type_new ();
		brasero_burn_session_get_input_type (BRASERO_BURN_SESSION (priv->session), source);

		if (!brasero_track_type_get_has_medium (source))
			flags |= BRASERO_BURN_FLAG_NO_TMP_FILES;

		brasero_track_type_free (source);
	}
	else
		flags &= BRASERO_DEST_SAVED_FLAGS;

	current_flags = brasero_burn_session_get_flags (BRASERO_BURN_SESSION (priv->session));
	current_flags &= (~BRASERO_DEST_SAVED_FLAGS);

	g_value_set_int (value, flags|current_flags);
	return TRUE;
}

static void
brasero_drive_settings_bind_session (BraseroDriveSettings *self)
{
	BraseroDriveSettingsPrivate *priv;
	gchar *display_name;
	gchar *path;
	gchar *tmp;

	priv = BRASERO_DRIVE_SETTINGS_PRIVATE (self);

	/* Get the drive name: it's done this way to avoid escaping */
	tmp = brasero_drive_get_display_name (priv->dest_drive);
	display_name = g_strdup_printf ("drive-%u", g_str_hash (tmp));
	g_free (tmp);

	if (brasero_track_type_get_has_medium (priv->src_type))
		path = g_strdup_printf ("%s%s/disc-%i/",
		                        BRASERO_DRIVE_PROPERTIES_PATH,
		                        display_name,
		                        priv->dest_media);
	else if (brasero_track_type_get_has_data (priv->src_type))
		path = g_strdup_printf ("%s%s/data-%i/",
		                        BRASERO_DRIVE_PROPERTIES_PATH,
		                        display_name,
		                        priv->dest_media);
	else if (brasero_track_type_get_has_image (priv->src_type))
		path = g_strdup_printf ("%s%s/image-%i/",
		                        BRASERO_DRIVE_PROPERTIES_PATH,
		                        display_name,
		                        priv->dest_media);
	else if (brasero_track_type_get_has_stream (priv->src_type))
		path = g_strdup_printf ("%s%s/audio-%i/",
		                        BRASERO_DRIVE_PROPERTIES_PATH,
		                        display_name,
		                        priv->dest_media);
	else {
		g_free (display_name);
		return;
	}
	g_free (display_name);

	priv->settings = g_settings_new_with_path (BRASERO_SCHEMA_DRIVES, path);
	g_free (path);

	g_settings_bind_with_mapping (priv->settings, BRASERO_PROPS_SPEED,
	                              priv->session, "speed", G_SETTINGS_BIND_DEFAULT,
	                              brasero_drive_settings_get_mapping_speed,
	                              brasero_drive_settings_set_mapping_speed,
	                              self,
	                              NULL);

	g_settings_bind_with_mapping (priv->settings, BRASERO_PROPS_FLAGS,
	                              priv->session, "flags", G_SETTINGS_BIND_DEFAULT,
	                              brasero_drive_settings_get_mapping_flags,
	                              brasero_drive_settings_set_mapping_flags,
	                              self,
	                              NULL);
}

static void
brasero_drive_settings_unbind_session (BraseroDriveSettings *self)
{
	BraseroDriveSettingsPrivate *priv;

	priv = BRASERO_DRIVE_SETTINGS_PRIVATE (self);

	if (priv->settings) {
		brasero_track_type_free (priv->src_type);
		priv->src_type = NULL;

		g_object_unref (priv->dest_drive);
		priv->dest_drive = NULL;

		priv->dest_media = BRASERO_MEDIUM_NONE;

		g_settings_unbind (priv->settings, "speed");
		g_settings_unbind (priv->settings, "flags");

		g_object_unref (priv->settings);
		priv->settings = NULL;
	}
}

static void
brasero_drive_settings_rebind_session (BraseroDriveSettings *self)
{
	BraseroDriveSettingsPrivate *priv;
	BraseroTrackType *type;
	BraseroMedia new_media;
	BraseroMedium *medium;
	BraseroDrive *drive;

	priv = BRASERO_DRIVE_SETTINGS_PRIVATE (self);

	/* See if we really need to do that:
	 * - check the source type has changed 
	 * - check the output type has changed */
	drive = brasero_burn_session_get_burner (priv->session);
	medium = brasero_drive_get_medium (drive);
	type = brasero_track_type_new ();

	if (!drive
	||  !medium
	||   brasero_drive_is_fake (drive)
	||  !BRASERO_MEDIUM_VALID (brasero_medium_get_status (medium))
	||   brasero_burn_session_get_input_type (BRASERO_BURN_SESSION (priv->session), type) != BRASERO_BURN_OK) {
		brasero_drive_settings_unbind_session (self);
		return;
	}

	new_media = BRASERO_MEDIUM_TYPE (brasero_medium_get_status (medium));

	if (priv->dest_drive == drive
	&&  priv->dest_media == new_media
	&&  priv->src_type && brasero_track_type_equal (priv->src_type, type)) {
		brasero_track_type_free (type);
		return;
	}

	brasero_track_type_free (priv->src_type);
	priv->src_type = type;

	if (priv->dest_drive)
		g_object_unref (priv->dest_drive);

	priv->dest_drive = g_object_ref (drive);

	priv->dest_media = new_media;

	brasero_drive_settings_bind_session (self);
}

static void
brasero_drive_settings_output_changed_cb (BraseroBurnSession *session,
                                          BraseroMedium *former_medium,
                                          BraseroDriveSettings *self)
{
	brasero_drive_settings_rebind_session (self);
}

static void
brasero_drive_settings_track_added_cb (BraseroBurnSession *session,
                                       BraseroTrack *track,
                                       BraseroDriveSettings *self)
{
	brasero_drive_settings_rebind_session (self);
}

static void
brasero_drive_settings_track_removed_cb (BraseroBurnSession *session,
                                         BraseroTrack *track,
                                         guint former_position,
                                         BraseroDriveSettings *self)
{
	brasero_drive_settings_rebind_session (self);
}

static void
brasero_drive_settings_unset_session (BraseroDriveSettings *self)
{
	BraseroDriveSettingsPrivate *priv;

	priv = BRASERO_DRIVE_SETTINGS_PRIVATE (self);

	brasero_drive_settings_unbind_session (self);

	if (priv->session) {
		g_signal_handlers_disconnect_by_func (priv->session,
		                                      brasero_drive_settings_track_added_cb,
		                                      self);
		g_signal_handlers_disconnect_by_func (priv->session,
		                                      brasero_drive_settings_track_removed_cb,
		                                      self);
		g_signal_handlers_disconnect_by_func (priv->session,
		                                      brasero_drive_settings_output_changed_cb,
		                                      self);

		g_settings_unbind (priv->config_settings, "tmpdir");
		g_object_unref (priv->config_settings);

		g_object_unref (priv->session);
		priv->session = NULL;
	}
}

void
brasero_drive_settings_set_session (BraseroDriveSettings *self,
                                    BraseroBurnSession *session)
{
	BraseroDriveSettingsPrivate *priv;

	priv = BRASERO_DRIVE_SETTINGS_PRIVATE (self);

	brasero_drive_settings_unset_session (self);

	priv->session = g_object_ref (session);
	g_signal_connect (session,
	                  "track-added",
	                  G_CALLBACK (brasero_drive_settings_track_added_cb),
	                  self);
	g_signal_connect (session,
	                  "track-removed",
	                  G_CALLBACK (brasero_drive_settings_track_removed_cb),
	                  self);
	g_signal_connect (session,
	                  "output-changed",
	                  G_CALLBACK (brasero_drive_settings_output_changed_cb),
	                  self);
	brasero_drive_settings_rebind_session (self);

	priv->config_settings = g_settings_new (BRASERO_SCHEMA_CONFIG);
	g_settings_bind (priv->config_settings,
	                 BRASERO_PROPS_TMP_DIR, session,
	                 "tmpdir", G_SETTINGS_BIND_DEFAULT);
}

static void
brasero_drive_settings_init (BraseroDriveSettings *object)
{ }

static void
brasero_drive_settings_finalize (GObject *object)
{
	brasero_drive_settings_unset_session (BRASERO_DRIVE_SETTINGS (object));
	G_OBJECT_CLASS (brasero_drive_settings_parent_class)->finalize (object);
}

static void
brasero_drive_settings_class_init (BraseroDriveSettingsClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroDriveSettingsPrivate));

	object_class->finalize = brasero_drive_settings_finalize;
}

BraseroDriveSettings *
brasero_drive_settings_new (void)
{
	return g_object_new (BRASERO_TYPE_DRIVE_SETTINGS, NULL);
}

