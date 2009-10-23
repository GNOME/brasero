/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-burn
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-burn is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-burn authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-burn. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-burn is distributed in the hope that it will be useful,
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

#ifndef BRASERO_TOOL_DIALOG_H
#define BRASERO_TOOL_DIALOG_H

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtk.h>

#include <brasero-medium.h>
#include <brasero-medium-monitor.h>

#include <brasero-session.h>
#include <brasero-burn.h>


G_BEGIN_DECLS

#define BRASERO_TYPE_TOOL_DIALOG         (brasero_tool_dialog_get_type ())
#define BRASERO_TOOL_DIALOG(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_TOOL_DIALOG, BraseroToolDialog))
#define BRASERO_TOOL_DIALOG_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_TOOL_DIALOG, BraseroToolDialogClass))
#define BRASERO_IS_TOOL_DIALOG(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_TOOL_DIALOG))
#define BRASERO_IS_TOOL_DIALOG_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_TOOL_DIALOG))
#define BRASERO_TOOL_DIALOG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_TOOL_DIALOG, BraseroToolDialogClass))

typedef struct _BraseroToolDialog BraseroToolDialog;
typedef struct _BraseroToolDialogClass BraseroToolDialogClass;

struct _BraseroToolDialog {
	GtkDialog parent;
};

struct _BraseroToolDialogClass {
	GtkDialogClass parent_class;

	/* Virtual functions */
	gboolean	(*activate)		(BraseroToolDialog *dialog,
						 BraseroMedium *medium);
	gboolean	(*cancel)		(BraseroToolDialog *dialog);
	void		(*medium_changed)	(BraseroToolDialog *dialog,
						 BraseroMedium *medium);
};

GType brasero_tool_dialog_get_type (void);

gboolean
brasero_tool_dialog_cancel (BraseroToolDialog *dialog);

gboolean
brasero_tool_dialog_set_medium (BraseroToolDialog *dialog,
				BraseroMedium *medium);

G_END_DECLS

#endif /* BRASERO_TOOL_DIALOG_H */
