/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2005-2008 <bonfire-app@wanadoo.fr>
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

#include <unistd.h>
#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include <libhal.h>
#include <gio/gio.h>

#include "burn-basics.h"
#include "burn-medium.h"
#include "burn-volume-obj.h"
#include "burn-drive.h"
#include "burn-debug.h"
#include "burn-hal-watch.h"

#include "scsi-mmc1.h"

#if defined(HAVE_STRUCT_USCSI_CMD)
#define DEVICE_MODEL	"info.product"
#define BLOCK_DEVICE	"block.solaris.raw_device"
#else
#define DEVICE_MODEL	"storage.model"
#define BLOCK_DEVICE	"block.device"
#endif

typedef struct _BraseroDrivePrivate BraseroDrivePrivate;
struct _BraseroDrivePrivate
{
	BraseroMedium *medium;
	BraseroDriveCaps caps;
	gchar *path;
	gchar *block_path;
	gchar *udi;

	gint bus;
	gint target;
	gint lun;

	gulong hal_sig;

	guint probed:1;
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

gboolean
brasero_drive_is_door_open (BraseroDrive *self)
{
	BraseroDrivePrivate *priv;
	BraseroDeviceHandle *handle;
	BraseroScsiMechStatusHdr hdr;

	priv = BRASERO_DRIVE_PRIVATE (self);

	if (!priv->udi)
		return FALSE;

	handle = brasero_device_handle_open (priv->path, FALSE, NULL);
	if (!handle)
		return FALSE;

	brasero_mmc1_mech_status (handle,
				  &hdr,
				  NULL);

	brasero_device_handle_close (handle);

	return hdr.door_open;
}

gboolean
brasero_drive_can_use_exclusively (BraseroDrive *self)
{
	BraseroDeviceHandle *handle;
	const gchar *device;

#if defined(HAVE_STRUCT_USCSI_CMD)
	device = brasero_drive_get_block_device (self);
#else
	device = brasero_drive_get_device (self);
#endif

	handle = brasero_device_handle_open (device, TRUE, NULL);
	if (!handle)
		return FALSE;

	brasero_device_handle_close (handle);
	return TRUE;
}

gboolean
brasero_drive_lock (BraseroDrive *self,
		    const gchar *reason,
		    gchar **reason_for_failure)
{
	BraseroDrivePrivate *priv;
	BraseroHALWatch *watch;
	LibHalContext *ctx;
	DBusError error;
	gboolean result;
	gchar *failure;

	priv = BRASERO_DRIVE_PRIVATE (self);

	if (!priv->udi)
		return FALSE;

	watch = brasero_hal_watch_get_default ();
	ctx = brasero_hal_watch_get_ctx (watch);

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

	if (result) {
		BRASERO_BURN_LOG ("Device locked");
	}
	else {
		BRASERO_BURN_LOG ("Device failed to lock");
	}

	return result;
}

gboolean
brasero_drive_unlock (BraseroDrive *self)
{
	BraseroDrivePrivate *priv;
	BraseroHALWatch *watch;
	LibHalContext *ctx;
	DBusError error;
	gboolean result;

	priv = BRASERO_DRIVE_PRIVATE (self);
	if (!priv->udi)
		return FALSE;

	watch = brasero_hal_watch_get_default ();
	ctx = brasero_hal_watch_get_ctx (watch);

	dbus_error_init (&error);
	result = libhal_device_unlock (ctx,
				       priv->udi,
				       &error);

	if (dbus_error_is_set (&error))
		dbus_error_free (&error);

	BRASERO_BURN_LOG ("Device unlocked");
	return result;
}

gchar *
brasero_drive_get_display_name (BraseroDrive *self)
{
	BraseroDrivePrivate *priv;
	BraseroHALWatch *watch;
	LibHalContext *ctx;

	priv = BRASERO_DRIVE_PRIVATE (self);

	/* Translators: This is a fake drive, a file, and means that when we're
	 * writing, we're writing to a file and create an image on the hard 
	 * drive. */
	if (!priv->udi)
		return g_strdup (_("Image File"));;

	watch = brasero_hal_watch_get_default ();
	ctx = brasero_hal_watch_get_ctx (watch);
	return libhal_device_get_property_string (ctx,
						  priv->udi,
	  					  DEVICE_MODEL,
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
brasero_drive_get_block_device (BraseroDrive *self)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (self);
	return priv->block_path;
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

	if (!priv->probed && priv->udi)
		return NULL;

	return priv->medium;
}

BraseroDriveCaps
brasero_drive_get_caps (BraseroDrive *self)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (self);
	return priv->caps;
}

gboolean
brasero_drive_can_write (BraseroDrive *self)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (self);
	return (priv->caps & (BRASERO_DRIVE_CAPS_CDR|
			      BRASERO_DRIVE_CAPS_DVDR|
			      BRASERO_DRIVE_CAPS_DVDR_PLUS|
			      BRASERO_DRIVE_CAPS_CDRW|
			      BRASERO_DRIVE_CAPS_DVDRW|
			      BRASERO_DRIVE_CAPS_DVDRW_PLUS|
			      BRASERO_DRIVE_CAPS_DVDR_PLUS_DL|
			      BRASERO_DRIVE_CAPS_DVDRW_PLUS_DL));
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

	if (priv->block_path) {
		libhal_free_string (priv->block_path);
		priv->block_path = NULL;
	}

	if (priv->medium) {
		g_object_unref (priv->medium);
		priv->medium = NULL;
	}

	if (priv->hal_sig) {
		BraseroHALWatch *watch;
		LibHalContext *ctx;
		DBusError error;

		watch = brasero_hal_watch_get_default ();
		ctx = brasero_hal_watch_get_ctx (watch);

		dbus_error_init (&error);
		libhal_device_remove_property_watch (ctx, priv->udi, &error);

		g_signal_handler_disconnect (watch, priv->hal_sig);
		priv->hal_sig = 0;
	}

	if (priv->udi) {
		g_free (priv->udi);
		priv->udi = NULL;
	}

	G_OBJECT_CLASS (brasero_drive_parent_class)->finalize (object);
}

static void
brasero_drive_medium_probed (BraseroMedium *medium,
			     BraseroDrive *self)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (self);

	/* only when it is probed */
	priv->probed = TRUE;
	g_signal_emit (self,
		       drive_signals [MEDIUM_INSERTED],
		       0,
		       priv->medium);
}

void
brasero_drive_reprobe (BraseroDrive *self)
{
	BraseroDrivePrivate *priv;
	BraseroMedium *medium;

	priv = BRASERO_DRIVE_PRIVATE (self);

	if (!priv->medium)
		return;

	BRASERO_BURN_LOG ("Reprobing inserted medium");

	/* remove current medium */
	medium = priv->medium;
	priv->medium = NULL;

	g_signal_emit (self,
		       drive_signals [MEDIUM_REMOVED],
		       0,
		       medium);
	g_object_unref (medium);
	priv->probed = FALSE;

	/* try to get a new one */
	priv->medium = g_object_new (BRASERO_TYPE_VOLUME,
				     "drive", self,
				     NULL);
	g_signal_connect (priv->medium,
			  "probed",
			  G_CALLBACK (brasero_drive_medium_probed),
			  self);
}

static void
brasero_drive_check_medium_inside (BraseroDrive *self)
{
	BraseroDrivePrivate *priv;
	BraseroHALWatch *watch;
	gboolean has_medium;
	LibHalContext *ctx;
	DBusError error;

	priv = BRASERO_DRIVE_PRIVATE (self);

	watch = brasero_hal_watch_get_default ();
	ctx = brasero_hal_watch_get_ctx (watch);

	BRASERO_BURN_LOG ("Contents changed");

	dbus_error_init (&error);
	has_medium = libhal_device_get_property_bool (ctx,
						      priv->udi,
						      "storage.removable.media_available",
						      &error);
	if (dbus_error_is_set (&error)) {
		g_warning ("Hal connection problem :  %s\n",
			   error.message);
		dbus_error_free (&error);
		return;
	}

	if (has_medium) {
		BRASERO_BURN_LOG ("Medium inserted");

		priv->probed = FALSE;
		priv->medium = g_object_new (BRASERO_TYPE_VOLUME,
					     "drive", self,
					     NULL);

		g_signal_connect (priv->medium,
				  "probed",
				  G_CALLBACK (brasero_drive_medium_probed),
				  self);
	}
	else if (priv->medium) {
		BraseroMedium *medium;

		BRASERO_BURN_LOG ("Medium removed");

		medium = priv->medium;
		priv->medium = NULL;

		g_signal_emit (self,
			       drive_signals [MEDIUM_REMOVED],
			       0,
			       medium);
		g_object_unref (medium);
		priv->probed = FALSE;
	}
}

static void
brasero_drive_medium_inside_property_changed_cb (BraseroHALWatch *watch,
						 const char *udi,
						 const char *key,
						 BraseroDrive *drive)
{
	BraseroDrivePrivate *priv;

	priv = BRASERO_DRIVE_PRIVATE (drive);

	if (key && strcmp (key, "storage.removable.media_available"))
		return;

	if (udi && strcmp (udi, priv->udi))
		return;

	brasero_drive_check_medium_inside (drive);
}

static void
brasero_drive_init_real (BraseroDrive *drive)
{
	BraseroDrivePrivate *priv;
	BraseroHALWatch *watch;
	LibHalContext *ctx;
	DBusError error;
	char *parent;

	priv = BRASERO_DRIVE_PRIVATE (drive);

	watch = brasero_hal_watch_get_default ();
	ctx = brasero_hal_watch_get_ctx (watch);

	priv->path = libhal_device_get_property_string (ctx, priv->udi, BLOCK_DEVICE, NULL);
	if (priv->path [0] == '\0') {
		g_free (priv->path);
		priv->path = NULL;
	}

	priv->block_path = libhal_device_get_property_string (ctx, priv->udi, "block.device", NULL);
	if (priv->block_path [0] == '\0') {
		g_free (priv->block_path);
		priv->block_path = NULL;
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

	/* Now check for the medium */
	brasero_drive_check_medium_inside (drive);

	dbus_error_init (&error);
	libhal_device_add_property_watch (ctx, priv->udi, &error);
	if (dbus_error_is_set (&error)) {
		g_warning ("Hal is not running : %s\n", error.message);
		dbus_error_free (&error);
	}

	priv->hal_sig = g_signal_connect (watch,
					  "property-changed",
					  G_CALLBACK (brasero_drive_medium_inside_property_changed_cb),
					  drive);
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
		if (!priv->udi) {
			priv->medium = g_object_new (BRASERO_TYPE_VOLUME,
						     "drive", object,
						     NULL);
		}
		else
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
