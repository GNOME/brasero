/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2008 <bonfire-app@wanadoo.fr>
 * 
 * brasero is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * brasero is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _BRASERO_NOTIFY_H_
#define _BRASERO_NOTIFY_H_

#include <glib-object.h>

#include <gtk/gtk.h>

#include "brasero-disc-message.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_NOTIFY             (brasero_notify_get_type ())
#define BRASERO_NOTIFY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_NOTIFY, BraseroNotify))
#define BRASERO_NOTIFY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_NOTIFY, BraseroNotifyClass))
#define BRASERO_IS_NOTIFY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_NOTIFY))
#define BRASERO_IS_NOTIFY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_NOTIFY))
#define BRASERO_NOTIFY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_NOTIFY, BraseroNotifyClass))

typedef struct _BraseroNotifyClass BraseroNotifyClass;
typedef struct _BraseroNotify BraseroNotify;

struct _BraseroNotifyClass
{
	GtkVBoxClass parent_class;
};

struct _BraseroNotify
{
	GtkVBox parent_instance;
};

typedef enum {
	BRASERO_NOTIFY_CONTEXT_NONE		= 0,
	BRASERO_NOTIFY_CONTEXT_SIZE		= 1,
	BRASERO_NOTIFY_CONTEXT_LOADING		= 2,
	BRASERO_NOTIFY_CONTEXT_MULTISESSION	= 3,
} BraseroNotifyContextId;

GType brasero_notify_get_type (void) G_GNUC_CONST;

GtkWidget *brasero_notify_new (void);

GtkWidget *
brasero_notify_message_add (BraseroNotify *notify,
			    const gchar *primary,
			    const gchar *secondary,
			    gint timeout,
			    guint context_id);
void
brasero_notify_button_add (BraseroNotify *notify,
			   BraseroDiscMessage *message,
			   const gchar *text,
			   const gchar *tooltip,
			   GtkResponseType response);

void
brasero_notify_message_remove (BraseroNotify *notify,
			       guint context_id);

void
brasero_notify_remove_all_messages (BraseroNotify *notify);

GtkWidget *
brasero_notify_get_message_by_context_id (BraseroNotify *notify,
					  guint context_id);

G_END_DECLS

#endif /* _BRASERO_NOTIFY_H_ */
