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

#ifndef ASYNC_TASK_MANAGER_H
#define ASYNC_TASK_MANAGER_H

#include <glib.h>
#include <glib-object.h>

#include <gio/gio.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_ASYNC_TASK_MANAGER         (brasero_async_task_manager_get_type ())
#define BRASERO_ASYNC_TASK_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_ASYNC_TASK_MANAGER, BraseroAsyncTaskManager))
#define BRASERO_ASYNC_TASK_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_ASYNC_TASK_MANAGER, BraseroAsyncTaskManagerClass))
#define BRASERO_IS_ASYNC_TASK_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_ASYNC_TASK_MANAGER))
#define BRASERO_IS_ASYNC_TASK_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_ASYNC_TASK_MANAGER))
#define BRASERO_ASYNC_TASK_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_ASYNC_TASK_MANAGER, BraseroAsyncTaskManagerClass))

typedef struct BraseroAsyncTaskManagerPrivate BraseroAsyncTaskManagerPrivate;
typedef struct _BraseroAsyncTaskManagerClass BraseroAsyncTaskManagerClass;
typedef struct _BraseroAsyncTaskManager BraseroAsyncTaskManager;

struct _BraseroAsyncTaskManager {
	GObject parent;
	BraseroAsyncTaskManagerPrivate *priv;
};

struct _BraseroAsyncTaskManagerClass {
	GObjectClass parent_class;
};

GType brasero_async_task_manager_get_type (void);

typedef enum {
	BRASERO_ASYNC_TASK_FINISHED		= 0,
	BRASERO_ASYNC_TASK_RESCHEDULE		= 1
} BraseroAsyncTaskResult;

typedef BraseroAsyncTaskResult	(*BraseroAsyncThread)		(BraseroAsyncTaskManager *manager,
								 GCancellable *cancel,
								 gpointer user_data);
typedef void			(*BraseroAsyncDestroy)		(BraseroAsyncTaskManager *manager,
								 gboolean cancelled,
								 gpointer user_data);
typedef gboolean		(*BraseroAsyncFindTask)		(BraseroAsyncTaskManager *manager,
								 gpointer task,
								 gpointer user_data);

struct _BraseroAsyncTaskType {
	BraseroAsyncThread thread;
	BraseroAsyncDestroy destroy;
};
typedef struct _BraseroAsyncTaskType BraseroAsyncTaskType;

typedef enum {
	/* used internally when reschedule */
	BRASERO_ASYNC_RESCHEDULE	= 1,

	BRASERO_ASYNC_IDLE		= 1 << 1,
	BRASERO_ASYNC_NORMAL		= 1 << 2,
	BRASERO_ASYNC_URGENT		= 1 << 3
} BraseroAsyncPriority;

gboolean
brasero_async_task_manager_queue (BraseroAsyncTaskManager *manager,
				  BraseroAsyncPriority priority,
				  const BraseroAsyncTaskType *type,
				  gpointer data);

gboolean
brasero_async_task_manager_foreach_active (BraseroAsyncTaskManager *manager,
					   BraseroAsyncFindTask func,
					   gpointer user_data);
gboolean
brasero_async_task_manager_foreach_active_remove (BraseroAsyncTaskManager *manager,
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

G_END_DECLS

#endif /* ASYNC_JOB_MANAGER_H */
