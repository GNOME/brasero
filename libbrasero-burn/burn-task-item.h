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

#ifndef _BRASERO_TASK_ITEM_H
#define _BRASERO_TASK_ITEM_H

#include <glib-object.h>

#include "burn-basics.h"
#include "burn-task-ctx.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_TASK_ITEM			(brasero_task_item_get_type ())
#define BRASERO_TASK_ITEM(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_TASK_ITEM, BraseroTaskItem))
#define BRASERO_TASK_ITEM_CLASS(vtable)		(G_TYPE_CHECK_CLASS_CAST ((vtable), BRASERO_TYPE_TASK_ITEM, BraseroTaskItemIFace))
#define BRASERO_IS_TASK_ITEM(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_TASK_ITEM))
#define BRASERO_IS_TASK_ITEM_CLASS(vtable)	(G_TYPE_CHECK_CLASS_TYPE ((vtable), BRASERO_TYPE_TASK_ITEM))
#define BRASERO_TASK_ITEM_GET_CLASS(inst)	(G_TYPE_INSTANCE_GET_INTERFACE ((inst), BRASERO_TYPE_TASK_ITEM, BraseroTaskItemIFace))

typedef struct _BraseroTaskItem BraseroTaskItem;
typedef struct _BraseroTaskItemIFace BraseroTaskItemIFace;

struct _BraseroTaskItemIFace {
	GTypeInterface parent;

	BraseroBurnResult	(*link)		(BraseroTaskItem *input,
						 BraseroTaskItem *output);
	BraseroTaskItem *	(*previous)	(BraseroTaskItem *item);
	BraseroTaskItem *	(*next)		(BraseroTaskItem *item);

	gboolean		(*is_active)	(BraseroTaskItem *item);
	BraseroBurnResult	(*activate)	(BraseroTaskItem *item,
						 BraseroTaskCtx *ctx,
						 GError **error);
	BraseroBurnResult	(*start)	(BraseroTaskItem *item,
						 GError **error);
	BraseroBurnResult	(*clock_tick)	(BraseroTaskItem *item,
						 BraseroTaskCtx *ctx,
						 GError **error);
	BraseroBurnResult	(*stop)		(BraseroTaskItem *item,
						 BraseroTaskCtx *ctx,
						 GError **error);
};

GType
brasero_task_item_get_type (void);

BraseroBurnResult
brasero_task_item_link (BraseroTaskItem *input,
			BraseroTaskItem *output);

BraseroTaskItem *
brasero_task_item_previous (BraseroTaskItem *item);

BraseroTaskItem *
brasero_task_item_next (BraseroTaskItem *item);

gboolean
brasero_task_item_is_active (BraseroTaskItem *item);

BraseroBurnResult
brasero_task_item_activate (BraseroTaskItem *item,
			    BraseroTaskCtx *ctx,
			    GError **error);

BraseroBurnResult
brasero_task_item_start (BraseroTaskItem *item,
			 GError **error);

BraseroBurnResult
brasero_task_item_clock_tick (BraseroTaskItem *item,
			      BraseroTaskCtx *ctx,
			      GError **error);

BraseroBurnResult
brasero_task_item_stop (BraseroTaskItem *item,
			BraseroTaskCtx *ctx,
			GError **error);

G_END_DECLS

#endif
