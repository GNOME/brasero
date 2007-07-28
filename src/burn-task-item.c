/*
 * brasero
 * Copyright (C) Philippe Rouquier 2007 <bonfire-app@wanadoo.fr>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib-object.h>

#include "burn-basics.h"
#include "burn-task-ctx.h"
#include "burn-task-item.h"

GType
brasero_task_item_get_type (void)
{
	static GType type = 0;
	
	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (BraseroTaskItemIFace),
			NULL,   /* base_init */
			NULL,   /* base_finalize */
			NULL,   /* class_init */
			NULL,   /* class_finalize */
			NULL,   /* class_data */
			0,
			0,      /* n_preallocs */
			NULL    /* instance_init */
		};
		type = g_type_register_static (G_TYPE_INTERFACE,
					       "BraseroTaskItem",
					       &info,
					       0);
	}
	return type;
}

BraseroBurnResult
brasero_task_item_connect (BraseroTaskItem *input, BraseroTaskItem *output)
{
	BraseroTaskItemIFace *klass;

	g_return_val_if_fail (BRASERO_IS_TASK_ITEM (input), BRASERO_BURN_ERR);
	g_return_val_if_fail (BRASERO_IS_TASK_ITEM (output), BRASERO_BURN_ERR);

	klass = BRASERO_TASK_ITEM_GET_CLASS (input);
	if (klass->connect)
		return klass->connect (input, output);

	klass = BRASERO_TASK_ITEM_GET_CLASS (output);
	if (klass->connect)
		return klass->connect (input, output);

	return BRASERO_BURN_NOT_SUPPORTED;
}

BraseroTaskItem *
brasero_task_item_previous (BraseroTaskItem *item)
{
	BraseroTaskItemIFace *klass;

	g_return_val_if_fail (BRASERO_IS_TASK_ITEM (item), NULL);

	klass = BRASERO_TASK_ITEM_GET_CLASS (item);
	if (klass->previous)
		return klass->previous (item);

	return NULL;
}

BraseroTaskItem *
brasero_task_item_next (BraseroTaskItem *item)
{
	BraseroTaskItemIFace *klass;

	g_return_val_if_fail (BRASERO_IS_TASK_ITEM (item), NULL);

	klass = BRASERO_TASK_ITEM_GET_CLASS (item);
	if (klass->next)
		return klass->next (item);

	return NULL;
}

BraseroBurnResult
brasero_task_item_init (BraseroTaskItem *item,
			BraseroTaskCtx *ctx,
			GError **error)
{
	BraseroTaskItemIFace *klass;

	g_return_val_if_fail (BRASERO_IS_TASK_ITEM (item), BRASERO_BURN_ERR);

	klass = BRASERO_TASK_ITEM_GET_CLASS (item);
	if (klass->init)
		return klass->init (item, ctx, error);

	return BRASERO_BURN_NOT_SUPPORTED;
}

BraseroBurnResult
brasero_task_item_start (BraseroTaskItem *item,
			 BraseroTaskCtx *ctx,
			 GError **error)
{
	BraseroTaskItemIFace *klass;

	g_return_val_if_fail (BRASERO_IS_TASK_ITEM (item), BRASERO_BURN_ERR);

	klass = BRASERO_TASK_ITEM_GET_CLASS (item);
	if (klass->start)
		return klass->start (item, ctx, error);

	return BRASERO_BURN_NOT_SUPPORTED;
}

BraseroBurnResult
brasero_task_item_clock_tick (BraseroTaskItem *item,
			      BraseroTaskCtx *ctx,
			      GError **error)
{
	BraseroTaskItemIFace *klass;

	g_return_val_if_fail (BRASERO_IS_TASK_ITEM (item), BRASERO_BURN_ERR);

	klass = BRASERO_TASK_ITEM_GET_CLASS (item);
	if (klass->clock_tick)
		return klass->clock_tick (item, ctx, error);

	return BRASERO_BURN_NOT_SUPPORTED;
}

BraseroBurnResult
brasero_task_item_stop (BraseroTaskItem *item,
			BraseroTaskCtx *ctx,
			GError **error)
{
	BraseroTaskItemIFace *klass;

	g_return_val_if_fail (BRASERO_IS_TASK_ITEM (item), BRASERO_BURN_ERR);

	klass = BRASERO_TASK_ITEM_GET_CLASS (item);
	if (klass->stop)
		return klass->stop (item, ctx, error);

	return BRASERO_BURN_NOT_SUPPORTED;
}
