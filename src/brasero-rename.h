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

#ifndef _BRASERO_RENAME_H_
#define _BRASERO_RENAME_H_

#include <glib-object.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef gboolean (*BraseroRenameCallback)	(GtkTreeModel *model,
						 GtkTreeIter *iter,
						 const gchar *old_name,
						 const gchar *new_name);

#define BRASERO_TYPE_RENAME             (brasero_rename_get_type ())
#define BRASERO_RENAME(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_RENAME, BraseroRename))
#define BRASERO_RENAME_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_RENAME, BraseroRenameClass))
#define BRASERO_IS_RENAME(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_RENAME))
#define BRASERO_IS_RENAME_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_RENAME))
#define BRASERO_RENAME_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_RENAME, BraseroRenameClass))

typedef struct _BraseroRenameClass BraseroRenameClass;
typedef struct _BraseroRename BraseroRename;

struct _BraseroRenameClass
{
	GtkVBoxClass parent_class;
};

struct _BraseroRename
{
	GtkVBox parent_instance;
};

GType brasero_rename_get_type (void) G_GNUC_CONST;

GtkWidget *
brasero_rename_new (void);

gboolean
brasero_rename_do (BraseroRename *self,
		   GtkTreeSelection *selection,
		   guint column_num,
		   BraseroRenameCallback callback);

G_END_DECLS

#endif /* _BRASERO_RENAME_H_ */
