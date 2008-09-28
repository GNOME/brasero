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

#include "scsi-device.h"

#include "burn-drive.h"
#include "burn-medium.h"
#include "burn-medium-monitor.h"

#include "brasero-data-session.h"
#include "brasero-data-project.h"
#include "brasero-file-node.h"
#include "brasero-io.h"

#include "brasero-marshal.h"

typedef struct _BraseroDataSessionPrivate BraseroDataSessionPrivate;
struct _BraseroDataSessionPrivate
{
	BraseroIO *io;
	BraseroIOJobBase *load_dir;

	/* Multisession drives that are inserted */
	GSList *media;

	/* Drive whose session is loaded */
	BraseroMedium *loaded;

	/* Nodes from the loaded session in the tree */
	GSList *nodes;

	glong size_changed_sig;

	guint is_oversized:1;
	guint is_overburn:1;
};

#define BRASERO_DATA_SESSION_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_DATA_SESSION, BraseroDataSessionPrivate))

G_DEFINE_TYPE (BraseroDataSession, brasero_data_session, BRASERO_TYPE_DATA_PROJECT);

enum {
	OVERSIZE_SIGNAL,
	AVAILABLE_SIGNAL,
	LOADED_SIGNAL,
	LAST_SIGNAL
};

static gulong brasero_data_session_signals [LAST_SIGNAL] = { 0 };

static void
brasero_data_session_check_size (BraseroDataSession *self)
{
	BraseroDataSessionPrivate *priv;
	gint64 max_sectors = 0;
	gint64 medium_sect = 0;
	gint64 sectors = 0;

	priv = BRASERO_DATA_SESSION_PRIVATE (self);

	sectors = brasero_data_project_get_size (BRASERO_DATA_PROJECT (self));
	brasero_medium_get_free_space (priv->loaded,
				       NULL,
				       &medium_sect);

	/* NOTE: This is not good since with a DVD 3% of 4.3G may be too much
	 * with 3% we are slightly over the limit of the most overburnable discs
	 * but at least users can try to overburn as much as they can. */

	/* The idea would be to test write the disc with cdrecord from /dev/null
	 * until there is an error and see how much we were able to write. So,
	 * when we propose overburning to the user, we could ask if he wants
	 * us to determine how much data can be written to a particular disc
	 * provided he has chosen a real disc. */
	max_sectors = medium_sect * 103 / 100;

	if (medium_sect < sectors) {
		/* send it once */
		if (!priv->is_oversized || priv->is_overburn) {
			gboolean overburn;

			/* see if overburn is possible */
			overburn = (sectors < max_sectors);
			if (!priv->is_overburn && overburn)
				g_signal_emit (self,
					       brasero_data_session_signals [OVERSIZE_SIGNAL],
					       0,
					       TRUE,
					       overburn);
			else if (!overburn)
				g_signal_emit (self,
					       brasero_data_session_signals [OVERSIZE_SIGNAL],
					       0,
					       TRUE,
					       overburn);

			priv->is_overburn = overburn;
		}

		priv->is_oversized = TRUE;
	}
	else {
		if (priv->is_oversized || priv->is_overburn)
			g_signal_emit (self,
				       brasero_data_session_signals [OVERSIZE_SIGNAL],
				       0,
				       FALSE,
				       FALSE);

		priv->is_oversized = FALSE;
		priv->is_overburn = FALSE;
	}
}

static void
brasero_data_session_size_changed (BraseroDataProject *project,
				   gpointer NULL_data)
{
	brasero_data_session_check_size (BRASERO_DATA_SESSION (project));
}

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
		       priv->loaded,
		       FALSE);

	if (priv->loaded) {
		g_object_unref (priv->loaded);
		priv->loaded = NULL;
	}

	if (priv->size_changed_sig) {
		g_signal_handler_disconnect (self, priv->size_changed_sig);
		priv->size_changed_sig = 0;
	}

	priv->is_oversized = FALSE;
	priv->is_overburn = FALSE;
}

static void
brasero_data_session_load_dir_destroy (GObject *object,
				       gboolean cancelled,
				       gpointer data)
{
	gint reference;
	BraseroFileNode *parent;

	/* reference */
	reference = GPOINTER_TO_INT (data);
	if (reference <= 0)
		return;

	parent = brasero_data_project_reference_get (BRASERO_DATA_PROJECT (object), reference);
	if (parent)
		parent->is_exploring = FALSE;

	brasero_data_project_reference_free (BRASERO_DATA_PROJECT (object), reference);
}

static void
brasero_data_session_load_dir_result (GObject *owner,
				      GError *error,
				      const gchar *dev_image,
				      GFileInfo *info,
				      gpointer data)
{
	BraseroDataSessionPrivate *priv;
	BraseroFileNode *parent;
	BraseroFileNode *node;
	gint reference;

	priv = BRASERO_DATA_SESSION_PRIVATE (owner);

	if (!info) {
		g_signal_emit (owner,
			       brasero_data_session_signals [LOADED_SIGNAL],
			       0,
			       priv->loaded,
			       FALSE);
/*		error = g_error_new (BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("unknown volume type"));
*/		return;
	}

	reference = GPOINTER_TO_INT (data);
	if (reference > 0)
		parent = brasero_data_project_reference_get (BRASERO_DATA_PROJECT (owner),
							     reference);
	else
		parent = NULL;

	/* add all the files/folders at the root of the session */
	node = brasero_data_project_add_imported_session_file (BRASERO_DATA_PROJECT (owner),
							       info,
							       parent);
	if (!node) {
		/* a problem ? */
		g_signal_emit (owner,
			       brasero_data_session_signals [LOADED_SIGNAL],
			       0,
			       priv->loaded,
			       FALSE);
		return;
 	}

	/* Only if we're exploring root directory */
	if (!parent)
		priv->nodes = g_slist_prepend (priv->nodes, node);

	g_signal_emit (owner,
		       brasero_data_session_signals [LOADED_SIGNAL],
		       0,
		       priv->loaded,
		       TRUE);
}

static gboolean
brasero_data_session_load_directory_contents_real (BraseroDataSession *self,
						   BraseroFileNode *node,
						   GError **error)
{
	BraseroDataSessionPrivate *priv;
	gint64 session_block;
	const gchar *device;
	gint reference = -1;

	if (node && !node->is_fake)
		return TRUE;

	priv = BRASERO_DATA_SESSION_PRIVATE (self);
	device = brasero_drive_get_device (brasero_medium_get_drive (priv->loaded));
	brasero_medium_get_last_data_track_address (priv->loaded,
						    NULL,
						    &session_block);
	if (!priv->io)
		priv->io = brasero_io_get_default ();

	if (!priv->load_dir)
		priv->load_dir = brasero_io_register (G_OBJECT (self),
						      brasero_data_session_load_dir_result,
						      brasero_data_session_load_dir_destroy,
						      NULL);

	/* If there aren't any node then that's root */
	if (node) {
		reference = brasero_data_project_reference_new (BRASERO_DATA_PROJECT (self), node);
		node->is_exploring = TRUE;
	}

	brasero_io_load_image_directory (priv->io,
					 device,
					 session_block,
					 BRASERO_FILE_NODE_IMPORTED_ADDRESS (node),
					 priv->load_dir,
					 BRASERO_IO_INFO_URGENT,
					 GINT_TO_POINTER (reference));

	if (node)
		node->is_fake = FALSE;

	return TRUE;
}

gboolean
brasero_data_session_load_directory_contents (BraseroDataSession *self,
					      BraseroFileNode *node,
					      GError **error)
{
	return brasero_data_session_load_directory_contents_real (self, node, error);
}

gboolean
brasero_data_session_add_last (BraseroDataSession *self,
			       BraseroMedium *medium,
			       GError **error)
{
	BraseroDataSessionPrivate *priv;

	priv = BRASERO_DATA_SESSION_PRIVATE (self);
	priv->loaded = medium;
	g_object_ref (medium);

	priv->size_changed_sig = g_signal_connect (self,
						   "size-changed",
						   G_CALLBACK (brasero_data_session_size_changed),
						   NULL);

	return brasero_data_session_load_directory_contents_real (self, NULL, error);
}

gboolean
brasero_data_session_has_available_media (BraseroDataSession *self)
{
	BraseroDataSessionPrivate *priv;

	priv = BRASERO_DATA_SESSION_PRIVATE (self);

	return priv->media != NULL;
}

GSList *
brasero_data_session_get_available_media (BraseroDataSession *self)
{
	GSList *retval;
	BraseroDataSessionPrivate *priv;

	priv = BRASERO_DATA_SESSION_PRIVATE (self);

	retval = g_slist_copy (priv->media);
	g_slist_foreach (retval, (GFunc) g_object_ref, NULL);

	return retval;
}

BraseroMedium *
brasero_data_session_get_loaded_medium (BraseroDataSession *self)
{
	BraseroDataSessionPrivate *priv;

	priv = BRASERO_DATA_SESSION_PRIVATE (self);
	if (!priv->media || !priv->nodes)
		return NULL;

	return priv->loaded;
}

static gboolean
brasero_data_session_is_valid_multi (BraseroMedium *medium)
{
	BraseroMedia media;
	BraseroBurnCaps *caps;
	BraseroMedia media_status;

	media = brasero_medium_get_status (medium);

	caps = brasero_burn_caps_get_default ();
	media_status = brasero_burn_caps_media_capabilities (caps, media);
	g_object_unref (caps);

	return (media_status & BRASERO_MEDIUM_WRITABLE) &&
	       (media & BRASERO_MEDIUM_HAS_DATA) &&
	       (brasero_medium_get_last_data_track_address (medium, NULL, NULL) != -1);
}

static void
brasero_data_session_disc_added_cb (BraseroMediumMonitor *monitor,
				    BraseroMedium *medium,
				    BraseroDataSession *self)
{
	BraseroDataSessionPrivate *priv;

	priv = BRASERO_DATA_SESSION_PRIVATE (self);

	if (!brasero_data_session_is_valid_multi (medium))
		return;

	g_object_ref (medium);
	priv->media = g_slist_prepend (priv->media, medium);

	g_signal_emit (self,
		       brasero_data_session_signals [AVAILABLE_SIGNAL],
		       0,
		       medium,
		       TRUE);
}

static void
brasero_data_session_disc_removed_cb (BraseroMediumMonitor *monitor,
				      BraseroMedium *medium,
				      BraseroDataSession *self)
{
	GSList *iter;
	GSList *next;
	BraseroDataSessionPrivate *priv;

	priv = BRASERO_DATA_SESSION_PRIVATE (self);

	/* see if that's the current loaded one */
	if (priv->loaded && priv->loaded == medium)
		brasero_data_session_remove_last (self);

	/* remove it from our list */
	for (iter = priv->media; iter; iter = next) {
		BraseroMedium *iter_medium;

		iter_medium = iter->data;
		next = iter->next;

		if (medium == iter_medium) {
			g_signal_emit (self,
				       brasero_data_session_signals [AVAILABLE_SIGNAL],
				       0,
				       medium,
				       FALSE);

			priv->media = g_slist_remove (priv->media, iter_medium);
			g_object_unref (iter_medium);
		}
	}
}

static void
brasero_data_session_init (BraseroDataSession *object)
{
	GSList *iter, *list;
	BraseroMediumMonitor *monitor;
	BraseroDataSessionPrivate *priv;

	priv = BRASERO_DATA_SESSION_PRIVATE (object);

	monitor = brasero_medium_monitor_get_default ();
	g_signal_connect (monitor,
			  "medium-added",
			  G_CALLBACK (brasero_data_session_disc_added_cb),
			  object);
	g_signal_connect (monitor,
			  "medium-removed",
			  G_CALLBACK (brasero_data_session_disc_removed_cb),
			  object);

	list = brasero_medium_monitor_get_media (monitor,
						 BRASERO_MEDIA_TYPE_WRITABLE|
						 BRASERO_MEDIA_TYPE_REWRITABLE);
	g_object_unref (monitor);

	/* check for a multisession medium already in */
	for (iter = list; iter; iter = iter->next) {
		BraseroMedium *medium;

		medium = iter->data;
		if (brasero_data_session_is_valid_multi (medium)) {
			g_object_ref (medium);
			priv->media = g_slist_prepend (priv->media, medium);
		}
	}
	g_slist_foreach (list, (GFunc) g_object_unref, NULL);
	g_slist_free (list);
}

static void
brasero_data_session_stop_io (BraseroDataSession *self)
{
	BraseroDataSessionPrivate *priv;

	priv = BRASERO_DATA_SESSION_PRIVATE (self);

	if (priv->io) {
		brasero_io_cancel_by_base (priv->io, priv->load_dir);

		g_free (priv->load_dir);
		priv->load_dir = NULL;
	}
}

static void
brasero_data_session_reset (BraseroDataProject *project,
			    guint num_nodes)
{
	brasero_data_session_stop_io (BRASERO_DATA_SESSION (project));

	/* chain up this function except if we invalidated the node */
	if (BRASERO_DATA_PROJECT_CLASS (brasero_data_session_parent_class)->reset)
		BRASERO_DATA_PROJECT_CLASS (brasero_data_session_parent_class)->reset (project, num_nodes);
}

static void
brasero_data_session_finalize (GObject *object)
{
	BraseroDataSessionPrivate *priv;

	priv = BRASERO_DATA_SESSION_PRIVATE (object);
	if (priv->loaded) {
		g_object_unref (priv->loaded);
		priv->loaded = NULL;
	}

	if (priv->media) {
		g_slist_foreach (priv->media, (GFunc) g_object_unref, NULL);
		g_slist_free (priv->media);
		priv->media = NULL;
	}

	if (priv->nodes) {
		g_slist_free (priv->nodes);
		priv->nodes = NULL;
	}

	/* NOTE no need to clean up size_changed_sig since it's connected to 
	 * ourselves. It disappears with use. */

	brasero_data_session_stop_io (BRASERO_DATA_SESSION (object));

	/* don't care about the nodes since they will be automatically
	 * destroyed */

	G_OBJECT_CLASS (brasero_data_session_parent_class)->finalize (object);
}


static void
brasero_data_session_class_init (BraseroDataSessionClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	BraseroDataProjectClass *project_class = BRASERO_DATA_PROJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroDataSessionPrivate));

	object_class->finalize = brasero_data_session_finalize;

	project_class->reset = brasero_data_session_reset;

	brasero_data_session_signals [AVAILABLE_SIGNAL] = 
	    g_signal_new ("session_available",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST,
			  0,
			  NULL, NULL,
			  brasero_marshal_VOID__OBJECT_BOOLEAN,
			  G_TYPE_NONE,
			  2,
			  G_TYPE_OBJECT,
			  G_TYPE_BOOLEAN);
	brasero_data_session_signals [LOADED_SIGNAL] = 
	    g_signal_new ("session_loaded",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST,
			  0,
			  NULL, NULL,
			  brasero_marshal_VOID__OBJECT_BOOLEAN,
			  G_TYPE_NONE,
			  2,
			  G_TYPE_OBJECT,
			  G_TYPE_BOOLEAN);
	brasero_data_session_signals [OVERSIZE_SIGNAL] = 
	    g_signal_new ("oversize",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST,
			  0,
			  NULL, NULL,
			  brasero_marshal_VOID__BOOLEAN_BOOLEAN,
			  G_TYPE_NONE,
			  2,
			  G_TYPE_BOOLEAN,
			  G_TYPE_BOOLEAN);
}
