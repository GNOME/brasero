/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2007-2008 <bonfire-app@wanadoo.fr>
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
#include <glib/gi18n-lib.h>

#include "burn-basics.h"
#include "burn-caps.h"

#include "burn-drive.h"

#include "brasero-data-session.h"
#include "brasero-data-project.h"
#include "brasero-file-node.h"

typedef struct _BraseroDataSessionPrivate BraseroDataSessionPrivate;
struct _BraseroDataSessionPrivate
{
	BraseroDrive *drive;
	GSList *nodes;

	guint multi_inserted:1;
};

#define BRASERO_DATA_SESSION_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_DATA_SESSION, BraseroDataSessionPrivate))

G_DEFINE_TYPE (BraseroDataSession, brasero_data_session, BRASERO_TYPE_DATA_PROJECT);

enum {
	AVAILABLE_SIGNAL,
	LOADED_SIGNAL,
	LAST_SIGNAL
};

static gulong brasero_data_session_signals [LAST_SIGNAL] = { 0 };


void
brasero_data_session_remove_last (BraseroDataSession *self)
{
	BraseroDataSessionPrivate *priv;
	GSList *iter;

	priv = BRASERO_DATA_SESSION_PRIVATE (self);

	if (!priv->nodes)
		return;

	/* go through the top nodes and remove all the imported nodes */
	for (iter = priv->nodes; iter; iter = iter->next) {
		BraseroFileNode *node;

		node = iter->data;
		brasero_data_project_destroy_node (BRASERO_DATA_PROJECT (self), node);
	}

	g_slist_free (priv->nodes);
	priv->nodes = NULL;

	g_signal_emit (self,
		       brasero_data_session_signals [LOADED_SIGNAL],
		       0,
		       FALSE);
}

static void
brasero_data_session_add_children_files (BraseroDataSession *self,
					 BraseroFileNode *parent,
					 GList *children)
{
	for (; children; children = children->next) {
		BraseroFileNode *node;
		BraseroVolFile *child;

		child = children->data;
		node = brasero_data_project_add_imported_session_file (BRASERO_DATA_PROJECT (self),
								       child,
								       parent);

		/* There is little chance that a NULL node will be returned and
		 * logically that shouldn't be the case. But who knows bugs
		 * happen, let's try not to crash. ;) */
		if (node && !node->is_file)
			brasero_data_session_add_children_files (self,
								 node,
								 child->specific.dir.children);
	}
}

gboolean
brasero_data_session_add_last (BraseroDataSession *self,
			       GError **error)
{
	BraseroDataSessionPrivate *priv;
	BraseroVolFile *volume;
	BraseroMedium *medium;
	const gchar *device;
	BraseroVolSrc *vol;
	gint64 block;
	GList *iter;

	priv = BRASERO_DATA_SESSION_PRIVATE (self);

	if (!priv->multi_inserted) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("there isn't any available session on the disc"));
		return FALSE;
	}

	/* get the address for the last track and retrieve the file list */
	medium = brasero_drive_get_medium (priv->drive);
	brasero_medium_get_last_data_track_address (medium,
						    NULL,
						    &block);
	if (block == -1) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("there isn't any available session on the disc"));
		return FALSE;
	}

	device = brasero_drive_get_device (priv->drive);
	vol = brasero_volume_source_open_file (device, error);
	volume = brasero_volume_get_files (vol,
					   block,
					   NULL,
					   NULL,
					   NULL,
					   error);
	brasero_volume_source_close (vol);
	if (*error) {
		if (volume)
			brasero_volume_file_free (volume);
		return FALSE;
	}

	if (!volume) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("unknown volume type"));
		return FALSE;
	}

	/* add all the files/folders at the root of the session */
	for (iter = volume->specific.dir.children; iter; iter = iter->next) {
		BraseroVolFile *file;
		BraseroFileNode *node;

		file = iter->data;
		node = brasero_data_project_add_imported_session_file (BRASERO_DATA_PROJECT (self),
								       file,
								       NULL);

		if (!node)
			continue;

		if (!node->is_file)
			brasero_data_session_add_children_files (self,
								 node,
								 file->specific.dir.children);

		priv->nodes = g_slist_prepend (priv->nodes, node);
	}

	/* put this here in case we have to replace one file at the root
	 * brasero_data_disc_is_session_path_deleted would think it needs
	 * to restore a session file */
	//priv->session = volume;

	brasero_volume_file_free (volume);

	g_signal_emit (self,
		       brasero_data_session_signals [LOADED_SIGNAL],
		       0,
		       TRUE);

	return TRUE;
}

void
brasero_data_session_set_drive (BraseroDataSession *self,
				BraseroDrive *drive)
{
	BraseroDataSessionPrivate *priv;
	BraseroMedia media_status;
	BraseroMedium *medium;
	BraseroBurnCaps *caps;
	BraseroMedia media;

	priv = BRASERO_DATA_SESSION_PRIVATE (self);

	if (priv->drive == drive)
		return;

	/* Remove the old imported session if any */
	if (priv->nodes)
		brasero_data_session_remove_last (self);

	if (priv->drive)
		g_object_unref (priv->drive);
	
	priv->drive = drive;

	if (drive)
		g_object_ref (drive);

	/* Now test for a multisession medium inserted and signal */
	medium = brasero_drive_get_medium (priv->drive);
	media = brasero_medium_get_status (medium);

	caps = brasero_burn_caps_get_default ();
	media_status = brasero_burn_caps_media_capabilities (caps, media);
	g_object_unref (caps);

	priv->multi_inserted = (media_status & BRASERO_MEDIUM_WRITABLE) &&
			       (media & BRASERO_MEDIUM_HAS_DATA) &&
			       (brasero_medium_get_last_data_track_address (medium, NULL, NULL) != -1);

	g_signal_emit (self,
		       brasero_data_session_signals [AVAILABLE_SIGNAL],
		       0,
		       priv->multi_inserted);
}

BraseroDrive *
brasero_data_session_get_loaded_medium (BraseroDataSession *self)
{
	BraseroDataSessionPrivate *priv;

	priv = BRASERO_DATA_SESSION_PRIVATE (self);
	if (!priv->multi_inserted || !priv->nodes)
		return NULL;

	return priv->drive;
}

static void
brasero_data_session_init (BraseroDataSession *object)
{}

static void
brasero_data_session_finalize (GObject *object)
{
	BraseroDataSessionPrivate *priv;

	priv = BRASERO_DATA_SESSION_PRIVATE (object);
	if (priv->drive) {
		g_object_unref (priv->drive);
		priv->drive = NULL;
	}

	if (priv->nodes) {
		g_slist_free (priv->nodes);
		priv->nodes = NULL;
	}

	/* don't care about the nodes since they will be automatically
	 * destroyed */

	G_OBJECT_CLASS (brasero_data_session_parent_class)->finalize (object);
}

static void
brasero_data_session_class_init (BraseroDataSessionClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroDataSessionPrivate));

	object_class->finalize = brasero_data_session_finalize;

	brasero_data_session_signals [AVAILABLE_SIGNAL] = 
	    g_signal_new ("session_available",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__BOOLEAN,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_BOOLEAN);
	brasero_data_session_signals [LOADED_SIGNAL] = 
	    g_signal_new ("session_loaded",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__BOOLEAN,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_BOOLEAN);
}
