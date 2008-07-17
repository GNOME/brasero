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

#ifndef _BRASERO_DATA_TREE_MODEL_H_
#define _BRASERO_DATA_TREE_MODEL_H_

#include <glib-object.h>

#include "brasero-data-vfs.h"
#include "brasero-file-node.h"

G_BEGIN_DECLS

/* This DND target when moving nodes inside ourselves */
#define BRASERO_DND_TARGET_SELF_FILE_NODES	"GTK_TREE_MODEL_ROW"

struct _BraseroDNDDataContext {
	GtkTreeModel *model;
	GList *references;
};
typedef struct _BraseroDNDDataContext BraseroDNDDataContext;

typedef enum {
	BRASERO_DATA_TREE_MODEL_NAME		= 0,
	BRASERO_DATA_TREE_MODEL_MIME_DESC,
	BRASERO_DATA_TREE_MODEL_MIME_ICON,
	BRASERO_DATA_TREE_MODEL_SIZE,
	BRASERO_DATA_TREE_MODEL_SHOW_PERCENT,
	BRASERO_DATA_TREE_MODEL_PERCENT,
	BRASERO_DATA_TREE_MODEL_STYLE,
	BRASERO_DATA_TREE_MODEL_COLOR,
	BRASERO_DATA_TREE_MODEL_EDITABLE,
	BRASERO_DATA_TREE_MODEL_COL_NUM
} BraseroDataProjectColumn;

#define BRASERO_TYPE_DATA_TREE_MODEL             (brasero_data_tree_model_get_type ())
#define BRASERO_DATA_TREE_MODEL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_DATA_TREE_MODEL, BraseroDataTreeModel))
#define BRASERO_DATA_TREE_MODEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_DATA_TREE_MODEL, BraseroDataTreeModelClass))
#define BRASERO_IS_DATA_TREE_MODEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_DATA_TREE_MODEL))
#define BRASERO_IS_DATA_TREE_MODEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_DATA_TREE_MODEL))
#define BRASERO_DATA_TREE_MODEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_DATA_TREE_MODEL, BraseroDataTreeModelClass))

typedef struct _BraseroDataTreeModelClass BraseroDataTreeModelClass;
typedef struct _BraseroDataTreeModel BraseroDataTreeModel;

struct _BraseroDataTreeModelClass
{
	BraseroDataVFSClass parent_class;
};

struct _BraseroDataTreeModel
{
	BraseroDataVFS parent_instance;
};

GType brasero_data_tree_model_get_type (void) G_GNUC_CONST;

BraseroDataTreeModel *
brasero_data_tree_model_new (void);

BraseroFileNode *
brasero_data_tree_model_path_to_node (BraseroDataTreeModel *self,
				      GtkTreePath *path);
GtkTreePath *
brasero_data_tree_model_node_to_path (BraseroDataTreeModel *self,
				      BraseroFileNode *node);

G_END_DECLS

#endif /* _BRASERO_DATA_TREE_MODEL_H_ */
