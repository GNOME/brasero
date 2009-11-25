/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-misc
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-misc is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-misc authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-misc. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-misc is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>

#include <gtk/gtk.h>

#include "brasero-notify.h"
#include "brasero-disc-message.h"

struct _BraseroNotifyPrivate
{
	GtkSizeGroup *message;
};

typedef struct _BraseroNotifyPrivate BraseroNotifyPrivate;

#define BRASERO_NOTIFY_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_NOTIFY, BraseroNotifyPrivate))

G_DEFINE_TYPE (BraseroNotify, brasero_notify, GTK_TYPE_VBOX);

void
brasero_notify_remove_all_messages (BraseroNotify *self)
{
	GList *children;
	GList *iter;

	GDK_THREADS_ENTER ();

	children = gtk_container_get_children (GTK_CONTAINER (self));
	for (iter = children; iter; iter = iter->next) {
		GtkWidget *widget;

		widget = iter->data;
		brasero_disc_message_destroy (BRASERO_DISC_MESSAGE (widget));
	}
	g_list_free (children);

	GDK_THREADS_LEAVE ();
}

GtkWidget *
brasero_notify_get_message_by_context_id (BraseroNotify *self,
					  guint context_id)
{
	GtkWidget *retval = NULL;
	GList *children;
	GList *iter;

	GDK_THREADS_ENTER ();

	children = gtk_container_get_children (GTK_CONTAINER (self));
	for (iter = children; iter; iter = iter->next) {
		GtkWidget *widget;

		widget = iter->data;
		if (brasero_disc_message_get_context (BRASERO_DISC_MESSAGE (widget)) == context_id) {
			retval = widget;
			break;
		}
	}
	g_list_free (children);

	GDK_THREADS_LEAVE ();

	return retval;
}

void
brasero_notify_message_remove (BraseroNotify *self,
			       guint context_id)
{
	BraseroNotifyPrivate *priv;
	GList *children;
	GList *iter;

	priv = BRASERO_NOTIFY_PRIVATE (self);

	GDK_THREADS_ENTER ();

	children = gtk_container_get_children (GTK_CONTAINER (self));
	for (iter = children; iter; iter = iter->next) {
		GtkWidget *widget;

		widget = iter->data;
		if (brasero_disc_message_get_context (BRASERO_DISC_MESSAGE (widget)) == context_id) {
			brasero_disc_message_destroy (BRASERO_DISC_MESSAGE (widget));
		}
	}
	g_list_free (children);

	GDK_THREADS_LEAVE ();
}

GtkWidget *
brasero_notify_message_add (BraseroNotify *self,
			    const gchar *primary,
			    const gchar *secondary,
			    gint timeout,
			    guint context_id)
{
	BraseroNotifyPrivate *priv;
	GtkWidget *message;

	priv = BRASERO_NOTIFY_PRIVATE (self);

	brasero_notify_message_remove (self, context_id);

	GDK_THREADS_ENTER ();

	message = brasero_disc_message_new ();
	gtk_size_group_add_widget (priv->message, message);
	brasero_disc_message_set_context (BRASERO_DISC_MESSAGE (message), context_id);
	brasero_disc_message_set_primary (BRASERO_DISC_MESSAGE (message), primary);
	brasero_disc_message_set_secondary (BRASERO_DISC_MESSAGE (message), secondary);
	if (timeout > 0)
		brasero_disc_message_set_timeout (BRASERO_DISC_MESSAGE (message), timeout);

	gtk_widget_show (message);
	gtk_box_pack_start (GTK_BOX (self), message, FALSE, TRUE, 0);

	GDK_THREADS_LEAVE ();

	return message;
}

static void
brasero_notify_init (BraseroNotify *object)
{
	BraseroNotifyPrivate *priv;

	priv = BRASERO_NOTIFY_PRIVATE (object);
	priv->message = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);
}

static void
brasero_notify_finalize (GObject *object)
{
	BraseroNotifyPrivate *priv;

	priv = BRASERO_NOTIFY_PRIVATE (object);

	if (priv->message) {
		g_object_unref (priv->message);
		priv->message = NULL;
	}
	
	G_OBJECT_CLASS (brasero_notify_parent_class)->finalize (object);
}

static void
brasero_notify_class_init (BraseroNotifyClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroNotifyPrivate));

	object_class->finalize = brasero_notify_finalize;
}

GtkWidget *
brasero_notify_new (void)
{
	return g_object_new (BRASERO_TYPE_NOTIFY, NULL);
}
