/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * trunk
 * Copyright (C) Philippe Rouquier 2008 <bonfire-app@wanadoo.fr>
 * 
 * trunk is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * trunk is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with trunk.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <unistd.h>
#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include <libhal.h>
#include <gio/gio.h>

#include "burn-basics.h"
#include "burn-medium.h"
#include "burn-drive.h"
#include "burn-debug.h"

#include "scsi-mmc1.h"

typedef struct _BraseroDrivePrivate BraseroDrivePrivate;
struct _BraseroDrivePrivate
{
	BraseroMedium *medium;
	BraseroDriveCaps caps;
	gchar *path;
	gchar *udi;

	gint bus;
	gint target;
	gint lun;
};

#define BRASERO_DRIVE_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_DRIVE, BraseroDrivePrivate))

enum {
	MEDIUM_REMOVED,
	MEDIUM_INSERTED,
	LAST_SIGNAL
};
static gulong drive_signals [LAST_SIGNAL] = {0, };

enum {
	PROP_NONE	= 0,
	PROP_DEVICE,
	PROP_UDI
};

G_DEFINE_TYPE (BraseroDrive, brasero_drive, G_TYPE_OBJECT);

static LibHalContext *hal_context = NULL;

gboolean
brasero_drive_get_bus_target_lun (BraseroDrive *self,
				  guint *bus,
				  guint *target,
				  guint *lun)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (self);

	if (!priv->udi)
		return FALSE;

	if (!bus || !target || !lun)
		return FALSE;

	if (priv->bus < 0)
		return FALSE;

	*bus = priv->bus;
	*target = priv->target;
	*lun = priv->lun;
	return TRUE;
}

gchar *
brasero_drive_get_bus_target_lun_string (BraseroDrive *self)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (self);

	if (!priv->udi)
		return NULL;

	if (priv->bus < 0)
		return NULL;

	return g_strdup_printf ("%i,%i,%i", priv->bus, priv->target, priv->lun);
}

gboolean
brasero_drive_is_fake (BraseroDrive *self)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (self);
	return (priv->path == NULL);
}

static LibHalContext *
brasero_drive_get_hal_context (void)
{
	DBusError error;
	DBusConnection *dbus_connection;

	if (hal_context)
		return hal_context;

	hal_context = libhal_ctx_new ();
	if (hal_context == NULL) {
		g_warning ("Cannot initialize hal library.");
		goto error;
	}

	dbus_error_init (&error);
	dbus_connection = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
	if (dbus_error_is_set (&error)) {
		g_warning ("Cannot connect to DBus %s.", error.message);
		dbus_error_free (&error);
		goto error;
	}

	libhal_ctx_set_dbus_connection (hal_context, dbus_connection);
	if (libhal_ctx_init (hal_context, &error) == FALSE) {
		g_warning ("Failed to initialize hal : %s", error.message);
		dbus_error_free (&error);
		goto error;
	}

	return hal_context;

error:
	libhal_ctx_shutdown (hal_context, NULL);
	libhal_ctx_free (hal_context);
	hal_context = NULL;
	return NULL;
}

gboolean
brasero_drive_is_door_open (BraseroDrive *self)
{
	BraseroDrivePrivate *priv;
	BraseroDeviceHandle *handle;
	BraseroScsiMechStatusHdr hdr;

	priv = BRASERO_DRIVE_PRIVATE (self);

	handle = brasero_device_handle_open (priv->path, NULL);
	if (!handle)
		return FALSE;

	brasero_mmc1_mech_status (handle,
				  &hdr,
				  NULL);

	brasero_device_handle_close (handle);

	return hdr.door_open;
}

gboolean
brasero_drive_lock (BraseroDrive *self,
		    const gchar *reason,
		    gchar **reason_for_failure)
{
	BraseroDrivePrivate *priv;
	LibHalContext *ctx;
	DBusError error;
	gboolean result;
	gchar *failure;

	priv = BRASERO_DRIVE_PRIVATE (self);

	ctx = brasero_drive_get_hal_context ();

	dbus_error_init (&error);
	result = libhal_device_lock (ctx,
				     priv->udi,
				     reason,
				     &failure,
				     &error);

	if (dbus_error_is_set (&error))
		dbus_error_free (&error);

	if (reason_for_failure)
		*reason_for_failure = g_strdup (failure);

	if (failure)
		dbus_free (failure);

	return result;
}

gboolean
brasero_drive_unlock (BraseroDrive *self)
{
	BraseroDrivePrivate *priv;
	LibHalContext *ctx;
	DBusError error;
	gboolean result;

	priv = BRASERO_DRIVE_PRIVATE (self);

	ctx = brasero_drive_get_hal_context ();

	dbus_error_init (&error);
	result = libhal_device_unlock (ctx,
				       priv->udi,
				       &error);

	if (dbus_error_is_set (&error))
		dbus_error_free (&error);

	return result;
}

gchar *
brasero_drive_get_display_name (BraseroDrive *self)
{
	BraseroDrivePrivate *priv;
	LibHalContext *ctx;

	priv = BRASERO_DRIVE_PRIVATE (self);

	ctx = brasero_drive_get_hal_context ();
	return libhal_device_get_property_string (ctx,
						  priv->udi,
						  "storage.model",
						  NULL);
}

const gchar *
brasero_drive_get_device (BraseroDrive *self)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (self);
	return priv->path;
}

const gchar *
brasero_drive_get_udi (BraseroDrive *self)
{
	BraseroDrivePrivate *priv;

	if (!self)
		return NULL;

	priv = BRASERO_DRIVE_PRIVATE (self);
	return priv->udi;
}

BraseroMedium *
brasero_drive_get_medium (BraseroDrive *self)
{
	BraseroDrivePrivate *priv;

	if (!self)
		return NULL;

	priv = BRASERO_DRIVE_PRIVATE (self);
	return priv->medium;
}

void
brasero_drive_set_medium (BraseroDrive *self,
			  BraseroMedium *medium)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (self);

	if (priv->medium) {
		g_signal_emit (self,
			       drive_signals [MEDIUM_REMOVED],
			       0,
			       priv->medium);

		g_object_unref (priv->medium);
		priv->medium = NULL;
	}

	priv->medium = medium;

	if (medium) {
		g_object_ref (medium);
		g_signal_emit (self,
			       drive_signals [MEDIUM_INSERTED],
			       0,
			       medium);
	}
}

BraseroDriveCaps
brasero_drive_get_caps (BraseroDrive *self)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (self);
	return priv->caps;
}

static void
brasero_drive_init (BraseroDrive *object)
{ }

static void
brasero_drive_finalize (GObject *object)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (object);
	if (priv->path) {
		libhal_free_string (priv->path);
		priv->path = NULL;
	}

	if (priv->udi) {
		g_free (priv->udi);
		priv->udi = NULL;
	}

	if (priv->medium) {
		g_object_unref (priv->medium);
		priv->medium = NULL;
	}

	G_OBJECT_CLASS (brasero_drive_parent_class)->finalize (object);
}

static void
brasero_drive_init_real (BraseroDrive *drive)
{
	BraseroDrivePrivate *priv;
	LibHalContext *ctx;
	char *parent;

	priv = BRASERO_DRIVE_PRIVATE (drive);

	ctx = brasero_drive_get_hal_context ();

	priv->path = libhal_device_get_property_string (ctx, priv->udi, "block.device", NULL);
	if (priv->path [0] == '\0') {
		g_free (priv->path);
		priv->path = NULL;
	}

	if (libhal_device_get_property_bool (ctx, priv->udi, "storage.cdrom.cdr", NULL))
		priv->caps |= BRASERO_DRIVE_CAPS_CDR;
	if (libhal_device_get_property_bool (ctx, priv->udi, "storage.cdrom.cdrw", NULL))
		priv->caps |= BRASERO_DRIVE_CAPS_CDRW;
	if (libhal_device_get_property_bool (ctx, priv->udi, "storage.cdrom.dvdr", NULL))
		priv->caps |= BRASERO_DRIVE_CAPS_DVDR;
	if (libhal_device_get_property_bool (ctx, priv->udi, "storage.cdrom.dvdrw", NULL))
		priv->caps |= BRASERO_DRIVE_CAPS_DVDRW;
	if (libhal_device_get_property_bool (ctx, priv->udi, "storage.cdrom.dvdplusr", NULL))
		priv->caps |= BRASERO_DRIVE_CAPS_DVDR_PLUS;
	if (libhal_device_get_property_bool (ctx, priv->udi, "storage.cdrom.dvdplusrw", NULL))
		priv->caps |= BRASERO_DRIVE_CAPS_DVDRW_PLUS;
	if (libhal_device_get_property_bool (ctx, priv->udi, "storage.cdrom.dvdplusrdl", NULL))
		priv->caps |= BRASERO_DRIVE_CAPS_DVDR_PLUS_DL;
	if (libhal_device_get_property_bool (ctx, priv->udi, "storage.cdrom.dvdplusrwdl", NULL))
		priv->caps |= BRASERO_DRIVE_CAPS_DVDRW_PLUS_DL;

	/* Also get its parent to retrieve the bus, host, lun values */
	parent = libhal_device_get_property_string (ctx, priv->udi, "info.parent", NULL);
	if (!parent) {
		priv->bus = -1;
		priv->lun = -1;
		priv->target = -1;
		return;
	}

	/* Check it is a SCSI interface */
	if (!libhal_device_property_exists (ctx, parent, "scsi.host", NULL)
	||  !libhal_device_property_exists (ctx, parent, "scsi.lun", NULL)
	||  !libhal_device_property_exists (ctx, parent, "scsi.target", NULL)) {
		g_free (parent);

		priv->bus = -1;
		priv->lun = -1;
		priv->target = -1;
		return;
	}

	priv->bus = libhal_device_get_property_int (ctx, parent, "scsi.host", NULL);
	priv->lun = libhal_device_get_property_int (ctx, parent, "scsi.lun", NULL);
	priv->target = libhal_device_get_property_int (ctx, parent, "scsi.target", NULL);

	BRASERO_BURN_LOG ("Drive %s has bus,target,lun = %i %i %i", priv->path, priv->bus, priv->target, priv->lun);
	libhal_free_string (parent);
}

static void
brasero_drive_set_property (GObject *object,
			    guint prop_id,
			    const GValue *value,
			    GParamSpec *pspec)
{
	BraseroDrivePrivate *priv;

	g_return_if_fail (BRASERO_IS_DRIVE (object));

	priv = BRASERO_DRIVE_PRIVATE (object);

	switch (prop_id)
	{
	case PROP_UDI:
		priv->udi = g_strdup (g_value_get_string (value));
		if (priv->udi)
			brasero_drive_init_real (BRASERO_DRIVE (object));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
brasero_drive_get_property (GObject *object,
			    guint prop_id,
			    GValue *value,
			    GParamSpec *pspec)
{
	BraseroDrivePrivate *priv;

	g_return_if_fail (BRASERO_IS_DRIVE (object));

	priv = BRASERO_DRIVE_PRIVATE (object);

	switch (prop_id)
	{
	case PROP_UDI:
		g_value_set_string (value, g_strdup (priv->udi));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
brasero_drive_class_init (BraseroDriveClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroDrivePrivate));

	object_class->finalize = brasero_drive_finalize;
	object_class->set_property = brasero_drive_set_property;
	object_class->get_property = brasero_drive_get_property;

	drive_signals[MEDIUM_INSERTED] =
		g_signal_new ("medium_added",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_ACTION,
		              0,
		              NULL, NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE, 1,
		              BRASERO_TYPE_MEDIUM);

	drive_signals[MEDIUM_REMOVED] =
		g_signal_new ("medium_removed",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE,
		              0,
		              NULL, NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE, 1,
		              BRASERO_TYPE_MEDIUM);

	g_object_class_install_property (object_class,
	                                 PROP_UDI,
	                                 g_param_spec_string("udi",
	                                                      "udi",
	                                                      "HAL udi",
	                                                      NULL,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

BraseroDrive *
brasero_drive_new (const gchar *udi)
{
	return g_object_new (BRASERO_TYPE_DRIVE,
			     "udi", udi,
			     NULL);
}
