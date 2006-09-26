/***************************************************************************
 *            async-task-manager.c
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

#include "brasero-async-task-manager.h"
 
static void brasero_async_task_manager_class_init (BraseroAsyncTaskManagerClass *klass);
static void brasero_async_task_manager_init (BraseroAsyncTaskManager *sp);
static void brasero_async_task_manager_finalize (GObject *object);

struct BraseroAsyncTaskManagerPrivate {
	GCond *thread_finished;
	GCond *task_finished;
	GCond *new_task;
	GMutex *lock;

	GSList *waiting_tasks;
	GSList *active_tasks;
	GSList *results;

	gint num_threads;
	gint unused_threads;

	gint results_id;

	GHashTable *types;
	BraseroAsyncTaskTypeID type_num;

	gint cancelled:1;
};

struct _BraseroAsyncTaskType {
	BraseroAsyncThread thread;
	BraseroSyncResult result;
};
typedef struct _BraseroAsyncTaskType BraseroAsyncTaskType;

struct _BraseroAsyncTaskCtx {
	BraseroAsyncTaskType *common;
	gpointer data;
};
typedef struct _BraseroAsyncTaskCtx BraseroAsyncTaskCtx;

#define MANAGER_MAX_THREAD 1

static GObjectClass *parent_class = NULL;

GType
brasero_async_task_manager_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroAsyncTaskManagerClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_async_task_manager_class_init,
			NULL,
			NULL,
			sizeof (BraseroAsyncTaskManager),
			0,
			(GInstanceInitFunc)brasero_async_task_manager_init,
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "BraseroAsyncTaskManager",
					       &our_info,
					       0);
	}

	return type;
}

static void
brasero_async_task_manager_class_init (BraseroAsyncTaskManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_async_task_manager_finalize;
}

static void
brasero_async_task_manager_init (BraseroAsyncTaskManager *obj)
{
	obj->priv = g_new0 (BraseroAsyncTaskManagerPrivate, 1);

	obj->priv->thread_finished = g_cond_new ();
	obj->priv->task_finished = g_cond_new ();
	obj->priv->new_task = g_cond_new ();

	obj->priv->lock = g_mutex_new ();
}

static void
brasero_async_task_manager_finalize (GObject *object)
{
	BraseroAsyncTaskManager *cobj;

	cobj = BRASERO_ASYNC_TASK_MANAGER (object);

	/* THIS DOESN'T FREE ALL IT'S JUST BETTER THAN NOTHING
	 * THE DERIVED OBJECT MUST ENSURE TO EMPTY EVERYTHING 
	 * ESPECIALLY DATA ASSOCIATED WITH THE CONTEXTS */

	/* stop the threads first */
	g_mutex_lock (cobj->priv->lock);
	cobj->priv->cancelled = TRUE;

	/* remove all the waiting tasks */
	g_slist_foreach (cobj->priv->waiting_tasks,
			 (GFunc) g_free,
			 NULL);
	g_slist_free (cobj->priv->waiting_tasks);
	cobj->priv->waiting_tasks = NULL;

	/* terminate all sleeping threads */
	g_cond_broadcast (cobj->priv->new_task);

	/* Now we wait for the active thread queue to return */
	while (cobj->priv->num_threads)
		g_cond_wait (cobj->priv->thread_finished, cobj->priv->lock);

	if (cobj->priv->results_id) {
		g_source_remove (cobj->priv->results_id);
		cobj->priv->results_id = 0;
	}

	/* remove all the results in case one got added */
	g_slist_foreach (cobj->priv->results,
			 (GFunc) g_free,
			 NULL);
	g_slist_free (cobj->priv->results);
	cobj->priv->results = NULL;

	g_mutex_unlock (cobj->priv->lock);

	if (cobj->priv->types) {
		g_hash_table_destroy (cobj->priv->types);
		cobj->priv->types = NULL;
	}

	if (cobj->priv->task_finished) {
		g_cond_free (cobj->priv->task_finished);
		cobj->priv->task_finished = NULL;
	}

	if (cobj->priv->thread_finished) {
		g_cond_free (cobj->priv->thread_finished);
		cobj->priv->thread_finished = NULL;
	}

	if (cobj->priv->new_task) {
		g_cond_free (cobj->priv->new_task);
		cobj->priv->new_task = NULL;
	}

	if (cobj->priv->lock) {
		g_mutex_free (cobj->priv->lock);
		cobj->priv->lock = NULL;
	}

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

BraseroAsyncTaskTypeID
brasero_async_task_manager_register_type (BraseroAsyncTaskManager *self,
					  BraseroAsyncThread thread,
					  BraseroSyncResult result)
{
	BraseroAsyncTaskType *type;

	g_return_val_if_fail (self != 0, 0);

	self->priv->type_num ++;
	if (self->priv->type_num == G_MAXINT) {
		self->priv->type_num = 1;

		while (g_hash_table_lookup (self->priv->types, GINT_TO_POINTER (self->priv->type_num))) {
			self->priv->type_num ++;

			if (self->priv->type_num == G_MAXINT) {
				g_warning ("ERROR: reached the max number of types\n");
				return 0;
			}
		}
	}
	
	type = g_new0 (BraseroAsyncTaskType, 1);
	type->thread = thread;
	type->result = result;

	if (!self->priv->types)
		self->priv->types = g_hash_table_new_full (g_direct_hash,
							   g_direct_equal,
							   NULL,
							   g_free);

	g_hash_table_insert (self->priv->types,
			     GINT_TO_POINTER (self->priv->type_num),
			     type);

	return self->priv->type_num;
}

static gboolean
brasero_async_task_manager_result (BraseroAsyncTaskManager *self)
{
	BraseroAsyncTaskCtx *ctx;

	g_mutex_lock (self->priv->lock);

	if (!self->priv->results) {
		self->priv->results_id = 0;
		g_mutex_unlock (self->priv->lock);
		return FALSE;
	}

	ctx = self->priv->results->data;
	self->priv->results = g_slist_remove (self->priv->results, ctx);

	g_mutex_unlock (self->priv->lock);

	ctx->common->result (self, ctx->data);
	g_free (ctx);

	return TRUE;
}

static gpointer
brasero_async_task_manager_thread (BraseroAsyncTaskManager *self)
{
	gboolean result;
	BraseroAsyncTaskCtx *ctx;

	g_mutex_lock (self->priv->lock);

	while (1) {
		/* say we are unused */
		self->priv->unused_threads ++;
	
		/* see if a task is waiting to be executed */
		while (!self->priv->waiting_tasks) {
			if (self->priv->cancelled)
				goto end;

			/* we always keep one thread ready */
			if (self->priv->num_threads - self->priv->unused_threads > 0) {
				GTimeVal timeout;

				/* wait to be woken up for 10 sec otherwise quit */
				g_get_current_time (&timeout);
				g_time_val_add (&timeout, 5000000);
				result = g_cond_timed_wait (self->priv->new_task,
							    self->priv->lock,
							    &timeout);

				if (!result)
					goto end;
			}
			else
				g_cond_wait (self->priv->new_task,
					     self->priv->lock);
		}
	
		/* say that we are active again */
		self->priv->unused_threads --;
	
		/* get the data from the list */
		ctx = self->priv->waiting_tasks->data;
		self->priv->waiting_tasks = g_slist_remove (self->priv->waiting_tasks, ctx);
		self->priv->active_tasks = g_slist_prepend (self->priv->active_tasks, ctx);
	
		g_mutex_unlock (self->priv->lock);
		ctx->common->thread (self, ctx->data);
		g_mutex_lock (self->priv->lock);

		/* we remove the task from the list and signal it is finished */
		self->priv->active_tasks = g_slist_remove (self->priv->active_tasks, ctx);
		g_cond_signal (self->priv->task_finished);

		/* insert the task in the results queue */
		self->priv->results = g_slist_append (self->priv->results, ctx);
		if (!self->priv->results_id)
			self->priv->results_id = g_idle_add ((GSourceFunc) brasero_async_task_manager_result, self);
	}

end:

	self->priv->unused_threads --;
	self->priv->num_threads --;

	/* maybe finalize is waiting for us to terminate */
	g_cond_signal (self->priv->thread_finished);
	g_mutex_unlock (self->priv->lock);

	g_thread_exit (NULL);

	return NULL;
}

gboolean
brasero_async_task_manager_queue (BraseroAsyncTaskManager *self,
				  BraseroAsyncTaskTypeID type,
				  gpointer data)
{
	BraseroAsyncTaskCtx *ctx;
	BraseroAsyncTaskType *task_type;

	g_return_val_if_fail (self != NULL, FALSE);
	g_return_val_if_fail (type > 0, FALSE);

	task_type = g_hash_table_lookup (self->priv->types, GINT_TO_POINTER (type));
	if (!task_type)
		return FALSE;

	ctx = g_new0 (BraseroAsyncTaskCtx, 1);
	ctx->data = data;
	ctx->common = task_type;

	g_mutex_lock (self->priv->lock);
	self->priv->waiting_tasks = g_slist_append (self->priv->waiting_tasks, ctx);

	if (self->priv->unused_threads) {
		/* wake up one thread in the list */
		g_cond_signal (self->priv->new_task);
	}
	else if (self->priv->num_threads < MANAGER_MAX_THREAD) {
		GError *error = NULL;
		GThread *thread;

		/* we have to start a new thread */
		thread = g_thread_create ((GThreadFunc) brasero_async_task_manager_thread,
					  self,
					  FALSE,
					  &error);

		if (!thread) {
			g_warning ("Can't start thread : %s\n", error->message);
			g_error_free (error);

			self->priv->waiting_tasks = g_slist_remove (self->priv->waiting_tasks, ctx);
			g_mutex_unlock (self->priv->lock);

			g_free (ctx);
			return FALSE;
		}

		self->priv->num_threads++;
	}
	/* else we wait for a currently active thread to be available */
	g_mutex_unlock (self->priv->lock);

	return TRUE;
}

gboolean
brasero_async_task_manager_foreach_active (BraseroAsyncTaskManager *self,
					   BraseroAsyncFindTask func,
					   gpointer user_data)
{
	GSList *iter, *tasks = NULL;
	BraseroAsyncTaskCtx *ctx;

	g_return_val_if_fail (self != NULL, FALSE);
	g_return_val_if_fail (func != NULL, FALSE);

	g_mutex_lock (self->priv->lock);

	for (iter = self->priv->active_tasks; iter; iter = iter->next) {
		ctx = iter->data;
		if (func (self, ctx->data, user_data))
			tasks = g_slist_prepend (tasks, ctx);
	}

	while (tasks) {
		/* Now we wait for all these active tasks to be finished */
		g_cond_wait (self->priv->task_finished, self->priv->lock);

		for (iter = tasks; iter; iter = iter->next) {
			ctx = iter->data;

			if (g_slist_find (self->priv->active_tasks, ctx) == NULL) {
				tasks = g_slist_remove (tasks, ctx);
				break;
			}
		}
	}

	g_mutex_unlock (self->priv->lock);
	return TRUE;
}

static GSList *
brasero_async_task_manager_foreach_remove (BraseroAsyncTaskManager *self,
					   GSList *list,
					   BraseroAsyncFindTask func,
					   gpointer user_data)
{
	BraseroAsyncTaskCtx *ctx;
	GSList *iter, *next;

	for (iter = list; iter; iter = next) {
		ctx = iter->data;
		next = iter->next;

		if (func (self, ctx->data, user_data)) {
			list = g_slist_remove (list, ctx);
			g_free (ctx);
		}
	}

	return list;
}

gboolean
brasero_async_task_manager_foreach_unprocessed_remove (BraseroAsyncTaskManager *self,
						       BraseroAsyncFindTask func,
						       gpointer user_data)
{
	g_return_val_if_fail (self != NULL, FALSE);
	g_return_val_if_fail (func != NULL, FALSE);

	g_mutex_lock (self->priv->lock);
	self->priv->waiting_tasks = brasero_async_task_manager_foreach_remove (self,
									       self->priv->waiting_tasks,
									       func,
									       user_data);
	g_mutex_unlock (self->priv->lock);

	return TRUE;
}

gboolean
brasero_async_task_manager_foreach_processed_remove (BraseroAsyncTaskManager *self,
						     BraseroAsyncFindTask func,
						     gpointer user_data)
{
	g_return_val_if_fail (self != NULL, FALSE);
	g_return_val_if_fail (func != NULL, FALSE);

	g_mutex_lock (self->priv->lock);
	self->priv->results = brasero_async_task_manager_foreach_remove (self,
									 self->priv->results,
									 func,
									 user_data);
	g_mutex_unlock (self->priv->lock);

	return TRUE;
}

gboolean
brasero_async_task_manager_find_urgent_task (BraseroAsyncTaskManager *self,
					     BraseroAsyncFindTask func,
					     gpointer user_data)
{
	GSList *iter;
	BraseroAsyncTaskCtx *ctx;

	g_return_val_if_fail (self != NULL, FALSE);
	g_return_val_if_fail (func != NULL, FALSE);

	g_mutex_lock (self->priv->lock);
	for (iter = self->priv->waiting_tasks; iter; iter = iter->next) {
		ctx = iter->data;

		if (func (self, ctx->data, user_data)) {
			self->priv->waiting_tasks = g_slist_remove (self->priv->waiting_tasks, ctx);
			self->priv->waiting_tasks = g_slist_prepend (self->priv->waiting_tasks, ctx);
			g_mutex_unlock (self->priv->lock);
			return TRUE;
		}
	}
	g_mutex_unlock (self->priv->lock);

	return FALSE;
}
