/***************************************************************************
 *            async-task-manager.h
 *
 *  ven avr  7 14:39:35 2006
 *  Copyright  2006  Rouquier Philippe
 *  brasero-app@wanadoo.fr
 ***************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef ASYNC_TASK_MANAGER_H
#define ASYNC_TASK_MANAGER_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_ASYNC_TASK_MANAGER         (brasero_async_task_manager_get_type ())
#define BRASERO_ASYNC_TASK_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_ASYNC_TASK_MANAGER, BraseroAsyncTaskManager))
#define BRASERO_ASYNC_TASK_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_ASYNC_TASK_MANAGER, BraseroAsyncTaskManagerClass))
#define BRASERO_IS_ASYNC_TASK_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_ASYNC_TASK_MANAGER))
#define BRASERO_IS_ASYNC_TASK_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_ASYNC_TASK_MANAGER))
#define BRASERO_ASYNC_TASK_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_ASYNC_TASK_MANAGER, BraseroAsyncTaskManagerClass))

typedef struct BraseroAsyncTaskManagerPrivate BraseroAsyncTaskManagerPrivate;

typedef struct {
	GObject parent;
	BraseroAsyncTaskManagerPrivate *priv;
} BraseroAsyncTaskManager;

typedef struct {
	GObjectClass parent_class;
} BraseroAsyncTaskManagerClass;

typedef void		(*BraseroAsyncThread)		(BraseroAsyncTaskManager *manager, gpointer user_data);
typedef void		(*BraseroSyncResult)		(BraseroAsyncTaskManager *manager, gpointer user_data);

typedef gboolean	(*BraseroAsyncFindTask)		(BraseroAsyncTaskManager *manager, gpointer task, gpointer user_data);

GType brasero_async_task_manager_get_type ();

typedef guint BraseroAsyncTaskTypeID;

gboolean
brasero_async_task_manager_queue (BraseroAsyncTaskManager *manager,
				  BraseroAsyncTaskTypeID type,
				  gpointer data);

gboolean
brasero_async_task_manager_foreach_active (BraseroAsyncTaskManager *manager,
					   BraseroAsyncFindTask func,
					   gpointer user_data);
gboolean
brasero_async_task_manager_foreach_processed_remove (BraseroAsyncTaskManager *self,
						     BraseroAsyncFindTask func,
						     gpointer user_data);
gboolean
brasero_async_task_manager_foreach_unprocessed_remove (BraseroAsyncTaskManager *self,
						       BraseroAsyncFindTask func,
						       gpointer user_data);

gboolean
brasero_async_task_manager_find_urgent_task (BraseroAsyncTaskManager *manager,
					     BraseroAsyncFindTask func,
					     gpointer user_data);
BraseroAsyncTaskTypeID
brasero_async_task_manager_register_type (BraseroAsyncTaskManager *manager,
					  BraseroAsyncThread thread,
					  BraseroSyncResult result);

#endif /* ASYNC_JOB_MANAGER_H */
