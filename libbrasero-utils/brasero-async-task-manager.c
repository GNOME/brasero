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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <gio/gio.h>
#include <glib-object.h>

#include "brasero-async-task-manager.h"
 
static void brasero_async_task_manager_class_init (BraseroAsyncTaskManagerClass *klass);
static void brasero_async_task_manager_init (BraseroAsyncTaskManager *sp);
static void brasero_async_task_manager_finalize (GObject *object);

typedef struct _BraseroAsyncTaskManagerPrivate BraseroAsyncTaskManagerPrivate;
struct _BraseroAsyncTaskManagerPrivate {
	GCond *thread_finished;
	GCond *task_finished;
	GCond *new_task;
	GMutex *lock;

	GSList *waiting_tasks;
	GSList *active_tasks;

	gint num_threads;
	gint unused_threads;

	gint cancelled:1;
};

struct _BraseroAsyncTaskCtx {
	BraseroAsyncPriority priority;
	const BraseroAsyncTaskType *type;
	GCancellable *cancel;
	gpointer data;
};
typedef struct _BraseroAsyncTaskCtx BraseroAsyncTaskCtx;

#define MANAGER_MAX_THREAD 2

G_DEFINE_TYPE_WITH_PRIVATE (BraseroAsyncTaskManager, brasero_async_task_manager, G_TYPE_OBJECT);

static void
brasero_async_task_manager_class_init (BraseroAsyncTaskManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = brasero_async_task_manager_finalize;
}

static void
brasero_async_task_manager_init (BraseroAsyncTaskManager *obj)
{
	BraseroAsyncTaskManagerPrivate *priv = brasero_async_task_manager_get_instance_private (obj);

	priv->thread_finished = g_cond_new ();
	priv->task_finished = g_cond_new ();
	priv->new_task = g_cond_new ();

	priv->lock = g_mutex_new ();
}

static void
brasero_async_task_manager_finalize (GObject *object)
{
	BraseroAsyncTaskManager *self = BRASERO_ASYNC_TASK_MANAGER (object);
	BraseroAsyncTaskManagerPrivate *priv = brasero_async_task_manager_get_instance_private (self);

	/* THIS DOESN'T FREE ALL IT'S JUST BETTER THAN NOTHING
	 * THE DERIVED OBJECT MUST ENSURE TO EMPTY EVERYTHING 
	 * ESPECIALLY DATA ASSOCIATED WITH THE CONTEXTS */

	/* stop the threads first */
	g_mutex_lock (priv->lock);
	priv->cancelled = TRUE;

	/* remove all the waiting tasks */
	g_slist_foreach (priv->waiting_tasks,
			 (GFunc) g_free,
			 NULL);
	g_slist_free (priv->waiting_tasks);
	priv->waiting_tasks = NULL;

	/* terminate all sleeping threads */
	g_cond_broadcast (priv->new_task);

	/* Now we wait for the active thread queue to return */
	while (priv->num_threads)
		g_cond_wait (priv->thread_finished, priv->lock);

	g_mutex_unlock (priv->lock);

	if (priv->task_finished) {
		g_cond_free (priv->task_finished);
		priv->task_finished = NULL;
	}

	if (priv->thread_finished) {
		g_cond_free (priv->thread_finished);
		priv->thread_finished = NULL;
	}

	if (priv->new_task) {
		g_cond_free (priv->new_task);
		priv->new_task = NULL;
	}

	if (priv->lock) {
		g_mutex_free (priv->lock);
		priv->lock = NULL;
	}

	G_OBJECT_CLASS (brasero_async_task_manager_parent_class)->finalize (object);
}

static void
brasero_async_task_manager_insert_task (BraseroAsyncTaskManager *self,
					BraseroAsyncTaskCtx *ctx)
{
	BraseroAsyncTaskManagerPrivate *priv = brasero_async_task_manager_get_instance_private (self);
	GSList *iter;
	GSList *node;
	BraseroAsyncTaskCtx *tmp;

	node = g_slist_alloc ();
	node->data = ctx;

	if (!priv->waiting_tasks) {
		priv->waiting_tasks = node;
		return;
	}

	tmp = priv->waiting_tasks->data;

	if (tmp->priority < ctx->priority) {
		node->next = priv->waiting_tasks;
		priv->waiting_tasks = node;
		return;
	}

	for (iter = priv->waiting_tasks; iter->next; iter = iter->next) {
		tmp = iter->next->data;

		if (tmp->priority < ctx->priority) {
			node->next = iter->next;
			iter->next = node;
			return;
		}
	}

	iter->next = node;
}

static gpointer
brasero_async_task_manager_thread (BraseroAsyncTaskManager *self)
{
	BraseroAsyncTaskManagerPrivate *priv = brasero_async_task_manager_get_instance_private (self);
	gboolean result;
	GCancellable *cancel;
	BraseroAsyncTaskCtx *ctx;

	cancel = g_cancellable_new ();

	g_mutex_lock (priv->lock);

	while (1) {
		BraseroAsyncTaskResult res;

		/* say we are unused */
		priv->unused_threads ++;
	
		/* see if a task is waiting to be executed */
		while (!priv->waiting_tasks) {
			if (priv->cancelled)
				goto end;

			/* we always keep one thread ready */
			if (priv->num_threads - priv->unused_threads > 0) {
				GTimeVal timeout;

				/* wait to be woken up for 10 sec otherwise quit */
				g_get_current_time (&timeout);
				g_time_val_add (&timeout, 5000000);
				result = g_cond_timed_wait (priv->new_task,
							    priv->lock,
							    &timeout);

				if (!result)
					goto end;
			}
			else
				g_cond_wait (priv->new_task,
					     priv->lock);
		}
	
		/* say that we are active again */
		priv->unused_threads --;
	
		/* get the data from the list */
		ctx = priv->waiting_tasks->data;
		ctx->cancel = cancel;
		ctx->priority &= ~BRASERO_ASYNC_RESCHEDULE;

		priv->waiting_tasks = g_slist_remove (priv->waiting_tasks, ctx);
		priv->active_tasks = g_slist_prepend (priv->active_tasks, ctx);
	
		g_mutex_unlock (priv->lock);
		res = ctx->type->thread (self, cancel, ctx->data);
		g_mutex_lock (priv->lock);

		/* we remove the task from the list and signal it is finished */
		priv->active_tasks = g_slist_remove (priv->active_tasks, ctx);
		g_cond_signal (priv->task_finished);

		/* NOTE: when threads are cancelled then they are destroyed in
		 * the function that cancelled them to destroy callback_data in
		 * the active main loop */
		if (!g_cancellable_is_cancelled (cancel)) {
			if (res == BRASERO_ASYNC_TASK_RESCHEDULE) {
				if (priv->waiting_tasks) {
					BraseroAsyncTaskCtx *next;

					next = priv->waiting_tasks->data;
					if (next->priority > ctx->priority)
						brasero_async_task_manager_insert_task (self, ctx);
					else
						priv->waiting_tasks = g_slist_prepend (priv->waiting_tasks, ctx);
				}
				else
					priv->waiting_tasks = g_slist_prepend (priv->waiting_tasks, ctx);
			}
			else {
				if (ctx->type->destroy)
					ctx->type->destroy (self, FALSE, ctx->data);
				g_free (ctx);
			}
		}
		else
			g_cancellable_reset (cancel);
	}

end:

	priv->unused_threads --;
	priv->num_threads --;

	/* maybe finalize is waiting for us to terminate */
	g_cond_signal (priv->thread_finished);
	g_mutex_unlock (priv->lock);

	g_object_unref (cancel);

	g_thread_exit (NULL);

	return NULL;
}

gboolean
brasero_async_task_manager_queue (BraseroAsyncTaskManager *self,
				  BraseroAsyncPriority priority,
				  const BraseroAsyncTaskType *type,
				  gpointer data)
{
	BraseroAsyncTaskManagerPrivate *priv;
	BraseroAsyncTaskCtx *ctx;

	g_return_val_if_fail (self != NULL, FALSE);

	priv = brasero_async_task_manager_get_instance_private (self);

	ctx = g_new0 (BraseroAsyncTaskCtx, 1);
	ctx->priority = priority;
	ctx->type = type;
	ctx->data = data;

	g_mutex_lock (priv->lock);
	if (priority == BRASERO_ASYNC_IDLE)
		priv->waiting_tasks = g_slist_append (priv->waiting_tasks, ctx);
	else if (priority == BRASERO_ASYNC_NORMAL)
		brasero_async_task_manager_insert_task (self, ctx);
	else if (priority == BRASERO_ASYNC_URGENT)
		priv->waiting_tasks = g_slist_prepend (priv->waiting_tasks, ctx);

	if (priv->unused_threads) {
		/* wake up one thread in the list */
		g_cond_signal (priv->new_task);
	}
	else if (priv->num_threads < MANAGER_MAX_THREAD) {
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

			priv->waiting_tasks = g_slist_remove (priv->waiting_tasks, ctx);
			g_mutex_unlock (priv->lock);

			g_free (ctx);
			return FALSE;
		}

		priv->num_threads++;
	}
	/* else we wait for a currently active thread to be available */
	g_mutex_unlock (priv->lock);

	return TRUE;
}

gboolean
brasero_async_task_manager_foreach_active (BraseroAsyncTaskManager *self,
					   BraseroAsyncFindTask func,
					   gpointer user_data)
{
	BraseroAsyncTaskManagerPrivate *priv;
	GSList *iter;
	BraseroAsyncTaskCtx *ctx;
	gboolean result = FALSE;

	g_return_val_if_fail (self != NULL, FALSE);
	g_return_val_if_fail (func != NULL, FALSE);

	priv = brasero_async_task_manager_get_instance_private (self);

	g_mutex_lock (priv->lock);
	for (iter = priv->active_tasks; iter; iter = iter->next) {
		ctx = iter->data;
		if (func (self, ctx->data, user_data))
			result = TRUE;
	}
	g_mutex_unlock (priv->lock);

	return result;
}

gboolean
brasero_async_task_manager_foreach_active_remove (BraseroAsyncTaskManager *self,
						  BraseroAsyncFindTask func,
						  gpointer user_data)
{
	BraseroAsyncTaskManagerPrivate *priv;
	GSList *iter, *tasks = NULL;
	BraseroAsyncTaskCtx *ctx;

	g_return_val_if_fail (self != NULL, FALSE);
	g_return_val_if_fail (func != NULL, FALSE);

	priv = brasero_async_task_manager_get_instance_private (self);

	g_mutex_lock (priv->lock);

	for (iter = priv->active_tasks; iter; iter = iter->next) {
		ctx = iter->data;
		if (func (self, ctx->data, user_data)) {
			g_cancellable_cancel (ctx->cancel);
			tasks = g_slist_prepend (tasks, ctx);
		}
	}

	while (tasks && priv->active_tasks) {
		GSList *next;

		/* Now we wait for all these active tasks to be finished */
		g_cond_wait (priv->task_finished, priv->lock);

		for (iter = tasks; iter; iter = next) {
			ctx = iter->data;
			next = iter->next;

			if (g_slist_find (priv->active_tasks, ctx))
				continue;

			tasks = g_slist_remove (tasks, ctx);

			/* destroy it */
			if (ctx->type->destroy)
				ctx->type->destroy (self, TRUE, ctx->data);

			g_free (ctx);
		}
	}

	g_mutex_unlock (priv->lock);

	return TRUE;
}

gboolean
brasero_async_task_manager_foreach_unprocessed_remove (BraseroAsyncTaskManager *self,
						       BraseroAsyncFindTask func,
						       gpointer user_data)
{
	BraseroAsyncTaskManagerPrivate *priv;
	BraseroAsyncTaskCtx *ctx;
	GSList *iter, *next;

	g_return_val_if_fail (self != NULL, FALSE);
	g_return_val_if_fail (func != NULL, FALSE);

	priv = brasero_async_task_manager_get_instance_private (self);

	g_mutex_lock (priv->lock);

	for (iter = priv->waiting_tasks; iter; iter = next) {
		ctx = iter->data;
		next = iter->next;

		if (func (self, ctx->data, user_data)) {
			priv->waiting_tasks = g_slist_remove (priv->waiting_tasks, ctx);

			/* call the destroy callback */
			if (ctx->type->destroy)
				ctx->type->destroy (self, TRUE, ctx->data);

			g_free (ctx);
		}
	}
	g_mutex_unlock (priv->lock);

	return TRUE;
}

gboolean
brasero_async_task_manager_find_urgent_task (BraseroAsyncTaskManager *self,
					     BraseroAsyncFindTask func,
					     gpointer user_data)
{
	BraseroAsyncTaskManagerPrivate *priv;
	GSList *iter;
	BraseroAsyncTaskCtx *ctx;

	g_return_val_if_fail (self != NULL, FALSE);
	g_return_val_if_fail (func != NULL, FALSE);

	priv = brasero_async_task_manager_get_instance_private (self);

	g_mutex_lock (priv->lock);
	for (iter = priv->waiting_tasks; iter; iter = iter->next) {
		ctx = iter->data;

		if (func (self, ctx->data, user_data)) {
			ctx->priority = BRASERO_ASYNC_URGENT;

			priv->waiting_tasks = g_slist_remove (priv->waiting_tasks, ctx);
			priv->waiting_tasks = g_slist_prepend (priv->waiting_tasks, ctx);
			g_mutex_unlock (priv->lock);
			return TRUE;
		}
	}
	g_mutex_unlock (priv->lock);

	return FALSE;
}
