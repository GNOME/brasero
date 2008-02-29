/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2008 <bonfire-app@wanadoo.fr>
 * 
 * brasero is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
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
#include <glib/gi18n-lib.h>

#include <nautilus-burn-init.h>
#include <nautilus-burn-drive-monitor.h>

#include "burn-drive.h"

#include "burn-medium.h"
#include "burn-medium-monitor.h"

typedef struct _BraseroMediumMonitorPrivate BraseroMediumMonitorPrivate;
struct _BraseroMediumMonitorPrivate
{
	GSList *media;
	GSList *drives;

	NautilusBurnDriveMonitor *monitor;
};

#define BRASERO_MEDIUM_MONITOR_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_MEDIUM_MONITOR, BraseroMediumMonitorPrivate))

enum
{
	MEDIUM_INSERTED,
	MEDIUM_REMOVED,

	LAST_SIGNAL
};


static guint medium_monitor_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (BraseroMediumMonitor, brasero_medium_monitor, G_TYPE_OBJECT);


GSList *
brasero_medium_monitor_get_media (BraseroMediumMonitor *self,
				  BraseroMediaType type)
{
	GSList *iter;
	GSList *list = NULL;
	BraseroMediumMonitorPrivate *priv;

	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (self);

	for (iter = priv->media; iter; iter = iter->next) {
		BraseroMedium *medium;

		medium = iter->data;
		if ((type & BRASERO_MEDIA_TYPE_READABLE)
		&& !(brasero_medium_get_status (medium) & BRASERO_MEDIUM_FILE)
		&&  (brasero_medium_get_status (medium) & (BRASERO_MEDIUM_HAS_AUDIO|BRASERO_MEDIUM_HAS_DATA))) {
			list = g_slist_prepend (list, medium);
			g_object_ref (medium);
			continue;
		}

		if (type & BRASERO_MEDIA_TYPE_WRITABLE) {
			if (brasero_medium_can_be_written (medium)) {
				list = g_slist_prepend (list, medium);
				g_object_ref (medium);
				continue;
			}
		}

		if (type & BRASERO_MEDIA_TYPE_REWRITABLE) {
			if (brasero_medium_can_be_rewritten (medium)) {
				list = g_slist_prepend (list, medium);
				g_object_ref (medium);
				continue;
			}
		}

		if (type & BRASERO_MEDIA_TYPE_FILE) {
			if (brasero_medium_get_status (medium) & BRASERO_MEDIUM_FILE) {
				list = g_slist_prepend (list, medium);
				g_object_ref (medium);
			}
		}
	}

	return list;
}

static void
brasero_medium_monitor_inserted_cb (NautilusBurnDriveMonitor *monitor,
				    NautilusBurnDrive *ndrive,
				    BraseroMediumMonitor *self)
{
	BraseroMediumMonitorPrivate *priv;
	BraseroDrive *drive = NULL;
	BraseroMedium *medium;
	GSList *iter;

	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (self);

	/* the drive must have been created first */
	for (iter = priv->drives; iter; iter = iter->next) {
		BraseroDrive *tmp;

		tmp = iter->data;
		if (nautilus_burn_drive_equal (brasero_drive_get_nautilus_drive (tmp), ndrive)) {
			drive = tmp;
			break;
		}
	}

	if (!drive) {
		drive = brasero_drive_new (ndrive);
		priv->drives = g_slist_prepend (priv->drives, drive);
	}

	medium = brasero_medium_new (drive);

	priv->media = g_slist_prepend (priv->media, medium);
	g_object_ref (medium);

	brasero_drive_set_medium (drive, medium);

	g_signal_emit (self,
		       medium_monitor_signals [MEDIUM_INSERTED],
		       0,
		       medium);
}

static void
brasero_medium_monitor_removed_cb (NautilusBurnDriveMonitor *monitor,
				   NautilusBurnDrive *ndrive,
				   BraseroMediumMonitor *self)
{
	BraseroMediumMonitorPrivate *priv;
	GSList *iter;

	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (self);
	for (iter = priv->drives; iter; iter = iter->next) {
		BraseroDrive *drive;

		drive = iter->data;
		if (nautilus_burn_drive_equal (brasero_drive_get_nautilus_drive (drive), ndrive)) {
			BraseroMedium *medium;

			medium = brasero_drive_get_medium (drive);
			brasero_drive_set_medium (drive, NULL);

			if (!medium)
				return;

			priv->media = g_slist_remove (priv->media, medium);
			g_signal_emit (self,
				       medium_monitor_signals [MEDIUM_REMOVED],
				       0,
				       medium);

			g_object_unref (medium);

			break;
		}
	}
}

static void
brasero_burn_medium_monitor_add_file (BraseroMediumMonitor *self)
{
	BraseroMediumMonitorPrivate *priv;
	BraseroMedium *medium;
	BraseroDrive *drive;

	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (self);

	drive = brasero_drive_new (nautilus_burn_drive_monitor_get_drive_for_image (priv->monitor));
	priv->drives = g_slist_prepend (priv->drives, drive);
	g_object_ref (drive);
	
	medium = g_object_new (BRASERO_TYPE_MEDIUM,
			       "drive", drive,
			       NULL);
	priv->media = g_slist_prepend (priv->media, medium);
	g_object_ref (medium);
}

static void
brasero_medium_monitor_init (BraseroMediumMonitor *object)
{
	BraseroMediumMonitorPrivate *priv;
	GList *iter, *list;

	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (object);

	nautilus_burn_init ();
	priv->monitor = nautilus_burn_get_drive_monitor ();

	list = nautilus_burn_drive_monitor_get_drives (priv->monitor);
	for (iter = list; iter; iter = iter->next) {
		BraseroDrive *drive;
		BraseroMedium *medium;
		NautilusBurnDrive *ndrive;

		ndrive = iter->data;

		drive = brasero_drive_new (ndrive);
		priv->drives = g_slist_prepend (priv->drives, drive);
		if (nautilus_burn_drive_get_media_type (ndrive) < NAUTILUS_BURN_MEDIA_TYPE_CD)
			continue;

		medium = brasero_medium_new (drive);
		if (!medium)
			continue;

		brasero_drive_set_medium (drive, medium);
		priv->media = g_slist_prepend (priv->media, medium);
	}
	g_list_free (list);

	brasero_burn_medium_monitor_add_file (object);

	g_signal_connect (priv->monitor,
			  "media-added",
			  G_CALLBACK (brasero_medium_monitor_inserted_cb),
			  object);
	g_signal_connect (priv->monitor,
			  "media-removed",
			  G_CALLBACK (brasero_medium_monitor_removed_cb),
			  object);
}

static void
brasero_medium_monitor_finalize (GObject *object)
{
	BraseroMediumMonitorPrivate *priv;

	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (object);

	if (priv->monitor) {
		g_object_unref (priv->monitor);
		priv->monitor = NULL;
	}

	if (priv->media) {
		g_slist_foreach (priv->media, (GFunc) g_object_unref, NULL);
		g_slist_free (priv->media);
		priv->media = NULL;
	}

	nautilus_burn_shutdown ();

	G_OBJECT_CLASS (brasero_medium_monitor_parent_class)->finalize (object);
}

static void
brasero_medium_monitor_class_init (BraseroMediumMonitorClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroMediumMonitorPrivate));

	object_class->finalize = brasero_medium_monitor_finalize;

	medium_monitor_signals[MEDIUM_INSERTED] =
		g_signal_new ("medium_added",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_ACTION,
		              0,
		              NULL, NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE, 1,
		              BRASERO_TYPE_MEDIUM);

	medium_monitor_signals[MEDIUM_REMOVED] =
		g_signal_new ("medium_removed",
		              G_OBJECT_CLASS_TYPE (klass),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_ACTION,
		              0,
		              NULL, NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE, 1,
		              BRASERO_TYPE_MEDIUM);
}

static BraseroMediumMonitor *singleton = NULL;

BraseroMediumMonitor *
brasero_medium_monitor_get_default (void)
{
	if (singleton) {
		g_object_ref (singleton);
		return singleton;
	}

	singleton = g_object_new (BRASERO_TYPE_MEDIUM_MONITOR, NULL);
	g_object_ref (singleton);
	return singleton;
}
