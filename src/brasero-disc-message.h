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

#ifndef _BRASERO_DISC_MESSAGE_H_
#define _BRASERO_DISC_MESSAGE_H_

#include <glib-object.h>

#include <gtk/gtkbin.h>
#include <gtk/gtkdialog.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_DISC_MESSAGE             (brasero_disc_message_get_type ())
#define BRASERO_DISC_MESSAGE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_DISC_MESSAGE, BraseroDiscMessage))
#define BRASERO_DISC_MESSAGE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_DISC_MESSAGE, BraseroDiscMessageClass))
#define BRASERO_IS_DISC_MESSAGE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_DISC_MESSAGE))
#define BRASERO_IS_DISC_MESSAGE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_DISC_MESSAGE))
#define BRASERO_DISC_MESSAGE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_DISC_MESSAGE, BraseroDiscMessageClass))

typedef struct _BraseroDiscMessageClass BraseroDiscMessageClass;
typedef struct _BraseroDiscMessage BraseroDiscMessage;

struct _BraseroDiscMessageClass
{
	GtkBinClass parent_class;

	/* Signals */
	void	(*response)	(BraseroDiscMessage *message,
				 GtkResponseType response);
};

struct _BraseroDiscMessage
{
	GtkBin parent_instance;
};

GType brasero_disc_message_get_type (void) G_GNUC_CONST;

GtkWidget *
brasero_disc_message_new (void);

void
brasero_disc_message_set_primary (BraseroDiscMessage *message,
				  const gchar *text);
void
brasero_disc_message_set_secondary (BraseroDiscMessage *message,
				    const gchar *text);
void
brasero_disc_message_set_image (BraseroDiscMessage *message,
				const gchar *image);
void
brasero_disc_message_set_progress_active (BraseroDiscMessage *message,
					  gboolean active);
void
brasero_disc_message_set_progress (BraseroDiscMessage *self,
				   gdouble progress);
void
brasero_disc_message_add_button (BraseroDiscMessage *message,
				 GtkSizeGroup *group,
				 const gchar *text,
				 const gchar *tooltip,
				 GtkResponseType type);
void
brasero_disc_message_add_close_button (BraseroDiscMessage *message);

void
brasero_disc_message_remove_buttons (BraseroDiscMessage *message);

void
brasero_disc_message_add_errors (BraseroDiscMessage *message,
				 GSList *errors);
void
brasero_disc_message_remove_errors (BraseroDiscMessage *message);

void
brasero_disc_message_set_timeout (BraseroDiscMessage *message,
				  guint mseconds);

void
brasero_disc_message_set_context (BraseroDiscMessage *message,
				  guint context_id);

guint
brasero_disc_message_get_context (BraseroDiscMessage *message);

G_END_DECLS

#endif /* _BRASERO_DISC_MESSAGE_H_ */
