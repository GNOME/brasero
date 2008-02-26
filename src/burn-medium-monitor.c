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

#include <nautilus-burn-drive-monitor.h>

#include "brasero-ncb.h"

#include "burn-medium.h"
#include "burn-medium-monitor.h"

typedef struct _BraseroMediumMonitorPrivate BraseroMediumMonitorPrivate;
struct _BraseroMediumMonitorPrivate
{
	GSList * media;
	BraseroMedium *file_medium;
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

static BraseroMedium *
brasero_burn_medium_get_file (BraseroMediumMonitor *self)
{
	BraseroMediumMonitorPrivate *priv;

	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (self);

	if (priv->file_medium) {
		g_object_ref (priv->file_medium);
		return priv->file_medium;
	}

	priv->file_medium = g_object_new (BRASERO_TYPE_MEDIUM,
					  "drive", nautilus_burn_drive_monitor_get_drive_for_image (priv->monitor),
					  NULL);
	g_object_ref (priv->file_medium);
	return priv->file_medium;
}

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
			}
		}
	}

	if (type & BRASERO_MEDIA_TYPE_FILE)
		list = g_slist_append (list, brasero_burn_medium_get_file (self));

	return list;
}

static void
brasero_medium_monitor_inserted_cb (NautilusBurnDriveMonitor *monitor,
				    NautilusBurnDrive *drive,
				    BraseroMedium *self)
{
	BraseroMediumMonitorPrivate *priv;
	BraseroMedium *medium;

	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (self);

	medium = brasero_medium_new (drive);
	NCB_DRIVE_SET_MEDIUM (drive, medium);

	priv->media = g_slist_prepend (priv->media, medium);
	g_object_ref (medium);

	g_signal_emit (self,
		       medium_monitor_signals [MEDIUM_INSERTED],
		       0,
		       medium);
}

static void
brasero_medium_monitor_removed_cb (NautilusBurnDriveMonitor *monitor,
				   NautilusBurnDrive *drive,
				   BraseroMedium *self)
{
	BraseroMediumMonitorPrivate *priv;
	BraseroMedium *medium;

	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (self);

	medium = NCB_DRIVE_GET_MEDIUM (drive);
	NCB_DRIVE_SET_MEDIUM (drive, NULL);

	if (!medium)
		return;

	priv->media = g_slist_remove (priv->media, medium);
	g_signal_emit (self,
		       medium_monitor_signals [MEDIUM_REMOVED],
		       0,
		       medium);

	g_object_unref (medium);
}

static void
brasero_medium_monitor_init (BraseroMediumMonitor *object)
{
	BraseroMediumMonitorPrivate *priv;
	GList *iter, *list;

	priv = BRASERO_MEDIUM_MONITOR_PRIVATE (object);

	priv->monitor = nautilus_burn_get_drive_monitor ();

	list = nautilus_burn_drive_monitor_get_drives (priv->monitor);
	for (iter = list; iter; iter = iter->next) {
		BraseroMedium *medium;
		NautilusBurnDrive *drive;

		drive = iter->data;
		medium = brasero_medium_new (drive);

		if (nautilus_burn_drive_get_media_type (drive) < NAUTILUS_BURN_MEDIA_TYPE_CD)
			continue;

		if (!medium)
			continue;

		priv->media = g_slist_prepend (priv->media, medium);
		g_object_ref (medium);

		NCB_DRIVE_SET_MEDIUM (drive, medium);
	}
	g_list_free (list);

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

	if (priv->file_medium) {
		g_object_unref (priv->file_medium);
		priv->file_medium = NULL;
	}

	if (priv->media) {
		g_slist_foreach (priv->media, (GFunc) g_object_unref, NULL);
		g_slist_free (priv->media);
		priv->media = NULL;
	}

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
