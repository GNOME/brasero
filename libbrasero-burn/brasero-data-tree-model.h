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

#ifndef _BRASERO_DATA_TREE_MODEL_H_
#define _BRASERO_DATA_TREE_MODEL_H_

#include <glib-object.h>

#include "brasero-data-vfs.h"
#include "brasero-file-node.h"

G_BEGIN_DECLS

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

	/* Signals */
	void		(*row_added)		(BraseroDataTreeModel *model,
						 BraseroFileNode *node);
	void		(*row_changed)		(BraseroDataTreeModel *model,
						 BraseroFileNode *node);
	void		(*row_removed)		(BraseroDataTreeModel *model,
						 BraseroFileNode *former_parent,
						 guint former_position,
						 BraseroFileNode *node);
	void		(*rows_reordered)	(BraseroDataTreeModel *model,
						 BraseroFileNode *parent,
						 guint *new_order);
};

GType brasero_data_tree_model_get_type (void) G_GNUC_CONST;

BraseroDataTreeModel *
brasero_data_tree_model_new (void);

G_END_DECLS

#endif /* _BRASERO_DATA_TREE_MODEL_H_ */
