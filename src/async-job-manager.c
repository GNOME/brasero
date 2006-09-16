/***************************************************************************
 *            async-job-manager.c
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

#include "async-job-manager.h"
 
static void brasero_async_job_manager_class_init (BraseroAsyncJobManagerClass *klass);
static void brasero_async_job_manager_init (BraseroAsyncJobManager *sp);
static void brasero_async_job_manager_finalize (GObject *object);

struct BraseroAsyncJobManagerPrivate {
	GCond *thread_finished;
	GCond *job_finished;
	GCond *new_job;
	GMutex *lock;

	GSList *waiting_jobs;
	GSList *active_jobs;
	GSList *results;

	int num_threads;
	int unused_threads;

	gint results_id;

	GHashTable *types;
	int type_num;

	gint cancel:1;
};

static BraseroAsyncJobManager *manager = NULL;

struct _BraseroAsyncJobType {
	GObject *obj;
	BraseroAsyncRunJob run;
	BraseroSyncGetResult results;
	BraseroAsyncDestroy destroy;
	BraseroAsyncCancelJob cancel;
};
typedef struct _BraseroAsyncJobType BraseroAsyncJobType;

struct _BraseroAsyncJobCtx {
	BraseroAsyncJobType *common;
	gpointer data;

	int cancel:1;
};
typedef struct _BraseroAsyncJobCtx BraseroAsyncJobCtx;

#define MANAGER_MAX_THREAD 1

static GObjectClass *parent_class = NULL;

GType
brasero_async_job_manager_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroAsyncJobManagerClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_async_job_manager_class_init,
			NULL,
			NULL,
			sizeof (BraseroAsyncJobManager),
			0,
			(GInstanceInitFunc)brasero_async_job_manager_init,
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "BraseroAsyncJobManager",
					       &our_info,
					       0);
	}

	return type;
}

static void
brasero_async_job_manager_class_init (BraseroAsyncJobManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_async_job_manager_finalize;
}

static void
brasero_async_job_manager_init (BraseroAsyncJobManager *obj)
{
	obj->priv = g_new0 (BraseroAsyncJobManagerPrivate, 1);

	obj->priv->thread_finished = g_cond_new ();
	obj->priv->job_finished = g_cond_new ();
	obj->priv->new_job = g_cond_new ();

	obj->priv->lock = g_mutex_new ();
}

static void
brasero_async_job_destroy (BraseroAsyncJobCtx *ctx, gboolean call_destroy)
{
	if (ctx->common->destroy && call_destroy)
		ctx->common->destroy (ctx->common->obj, ctx->data);

	g_free (ctx);
}

static void
brasero_async_job_cancel (BraseroAsyncJobCtx *ctx)
{
	ctx->cancel = 1;

	if (!ctx->common->cancel)
		return;

	ctx->common->cancel (ctx->data);
}

static void
brasero_async_job_manager_finalize (GObject *object)
{
	BraseroAsyncJobManager *cobj;

	cobj = BRASERO_ASYNC_JOB_MANAGER (object);

	/* stop the threads first */
	g_mutex_lock (cobj->priv->lock);
	cobj->priv->cancel = TRUE;

	/* remove all the waiting jobs */
	g_slist_foreach (manager->priv->waiting_jobs,
			 (GFunc) brasero_async_job_destroy,
			 NULL);
	g_slist_free (manager->priv->waiting_jobs);
	manager->priv->waiting_jobs = NULL;

	/* cancel all the active jobs */
	g_slist_foreach (manager->priv->active_jobs,
			 (GFunc) brasero_async_job_cancel,
			 NULL);

	/* terminate all sleeping threads */
	g_cond_broadcast (cobj->priv->new_job);

	/* Now we wait for the active thread queue to return */
	while (cobj->priv->num_threads)
		g_cond_wait (cobj->priv->thread_finished, cobj->priv->lock);

	if (cobj->priv->results_id) {
		g_source_remove (cobj->priv->results_id);
		cobj->priv->results_id = 0;
	}

	/* remove all the results in case one got added */
	g_slist_foreach (manager->priv->results,
			 (GFunc) brasero_async_job_destroy,
			 NULL);
	g_slist_free (manager->priv->results);
	manager->priv->results = NULL;

	g_mutex_unlock (cobj->priv->lock);

	if (cobj->priv->types) {
		g_hash_table_destroy (cobj->priv->types);
		cobj->priv->types = NULL;
	}

	if (cobj->priv->job_finished) {
		g_cond_free (cobj->priv->job_finished);
		cobj->priv->job_finished = NULL;
	}

	if (cobj->priv->thread_finished) {
		g_cond_free (cobj->priv->thread_finished);
		cobj->priv->thread_finished = NULL;
	}

	if (cobj->priv->new_job) {
		g_cond_free (cobj->priv->new_job);
		cobj->priv->new_job = NULL;
	}

	if (cobj->priv->lock) {
		g_mutex_free (cobj->priv->lock);
		cobj->priv->lock = NULL;
	}

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);

	manager = NULL;
}

BraseroAsyncJobManager *
brasero_async_job_manager_get_default ()
{
	if (!manager)
		manager = BRASERO_ASYNC_JOB_MANAGER (g_object_new (BRASERO_TYPE_ASYNC_JOB_MANAGER, NULL));
	else
		g_object_ref (manager);

	return manager;
}

gint
brasero_async_job_manager_register_type (BraseroAsyncJobManager *manager,
					 GObject *object,
					 BraseroAsyncRunJob run,
					 BraseroSyncGetResult results,
					 BraseroAsyncDestroy destroy,
					 BraseroAsyncCancelJob cancel)
{
	BraseroAsyncJobType *type;

	g_return_val_if_fail (manager != 0, 0);

	manager->priv->type_num ++;
	if (manager->priv->type_num == G_MAXINT) {
		manager->priv->type_num = 1;

		while (g_hash_table_lookup (manager->priv->types, GINT_TO_POINTER (manager->priv->type_num))) {
			manager->priv->type_num ++;

			if (manager->priv->type_num == G_MAXINT) {
				g_warning ("ERROR: reached the max number of types\n");
				return 0;
			}
		}
	}
	
	type = g_new0 (BraseroAsyncJobType, 1);
	type->obj = object;
	type->run = run;
	type->results = results;
	type->destroy = destroy;

	if (!manager->priv->types)
		manager->priv->types = g_hash_table_new_full (g_direct_hash,
							      g_direct_equal,
							      NULL,
							      g_free);

	g_hash_table_insert (manager->priv->types,
			     GINT_TO_POINTER (manager->priv->type_num),
			     type);

	return manager->priv->type_num;
}

static GSList *
brasero_async_job_manager_remove_from_queue_by_type (GSList *list,
						     BraseroAsyncJobType *job_type)
{
	BraseroAsyncJobCtx *ctx;
	GSList *iter, *next;

	/* find all waiting jobs with this object */
	for (iter = list; iter; iter = next) {
		ctx = iter->data;
		next = iter->next;

		if (ctx->common == job_type) {
			list = g_slist_remove (list, ctx);
			brasero_async_job_destroy (ctx, TRUE);
		}
	}

	return list;
}

void
brasero_async_job_manager_unregister_type (BraseroAsyncJobManager *manager,
					   gint type)
{
	BraseroAsyncJobType *job_type;
	BraseroAsyncJobCtx *ctx;

	g_return_if_fail (manager != NULL);

	/* first we check that there isn't any jobs of this type in one of the
	 * queues: remove the jobs in the waiting and results queues and wait
	 * for the jobs in the active queue to finish. */
	g_mutex_lock (manager->priv->lock);

	/* we remove the type from the hash table to make it unusable but don't
	 * destroy it yet as some active jobs may still need it */
	job_type = g_hash_table_lookup (manager->priv->types,
					GINT_TO_POINTER (type));

	if (!job_type) {
		g_mutex_unlock (manager->priv->lock);
		return;
	}

	g_hash_table_steal (manager->priv->types, GINT_TO_POINTER (type));

	/* find all waiting jobs with this type and remove them */
	manager->priv->waiting_jobs = brasero_async_job_manager_remove_from_queue_by_type (manager->priv->waiting_jobs, job_type);

	if (manager->priv->active_jobs) {
		GSList *iter;

		/* check the active jobs and cancel every one of them with this type */
		for (iter = manager->priv->active_jobs; iter; iter = iter->next) {
			ctx = iter->data;

			/* we simply call cancel not destroy since it will be done by
			 * the thread handling the job. It's up to the callback func to
			 * return FALSE */
			if (ctx->common == job_type)
				brasero_async_job_cancel (ctx);
		}

wait:
		/* Now we wait for all the active jobs of this type to be finished */
		g_cond_wait (manager->priv->job_finished, manager->priv->lock);
		for (iter = manager->priv->active_jobs; iter; iter = iter->next) {
			if (ctx->common == job_type)
				goto wait;
		}
	}

	/* same as above with already finished jobs */
	manager->priv->results = brasero_async_job_manager_remove_from_queue_by_type (manager->priv->results, job_type);

	g_free (job_type);
	g_mutex_unlock (manager->priv->lock);
}

static gboolean
brasero_async_job_manager_wait_for_results (BraseroAsyncJobManager *manager)
{
	BraseroAsyncJobCtx *ctx;
	gboolean result;

	g_mutex_lock (manager->priv->lock);

	if (!manager->priv->results) {
		manager->priv->results_id = 0;
		g_mutex_unlock (manager->priv->lock);
		return FALSE;
	}

	ctx = manager->priv->results->data;
	manager->priv->results = g_slist_remove (manager->priv->results, ctx);

	g_mutex_unlock (manager->priv->lock);

	/* There can't be a problem here with references, since finalize empty
	 * this queue */
	if (ctx->cancel) {
		brasero_async_job_destroy (ctx, TRUE);
		return TRUE;
	}

	result = ctx->common->results (ctx->common->obj, ctx->data);
	brasero_async_job_destroy (ctx, result);

	return TRUE;
}

static gpointer
brasero_async_job_manager_thread (BraseroAsyncJobManager *manager)
{
	gboolean result;
	BraseroAsyncJobCtx *ctx;

	g_mutex_lock (manager->priv->lock);

	while (1) {
		/* say we are unused */
		manager->priv->unused_threads ++;
	
		/* see if a job is waiting to be executed */
		while (!manager->priv->waiting_jobs) {
			if (manager->priv->cancel)
				goto end;

			/* we always keep one thread ready */
			if (manager->priv->num_threads - manager->priv->unused_threads > 0) {
				GTimeVal timeout;

				/* wait to be woken up for 10 sec otherwise quit */
				g_get_current_time (&timeout);
				g_time_val_add (&timeout, 10000000);
				result = g_cond_timed_wait (manager->priv->new_job,
							    manager->priv->lock,
							    &timeout);

				if (!result)
					goto end;
			}
			else
				g_cond_wait (manager->priv->new_job,
					     manager->priv->lock);
		}
	
		/* say that we are active again */
		manager->priv->unused_threads --;
	
		/* get the data from the list */
		ctx = manager->priv->waiting_jobs->data;
		manager->priv->waiting_jobs = g_slist_remove (manager->priv->waiting_jobs, ctx);
		manager->priv->active_jobs = g_slist_prepend (manager->priv->active_jobs, ctx);
	
		g_mutex_unlock (manager->priv->lock);
	
		result = ctx->common->run (ctx->common->obj, ctx->data);

		g_mutex_lock (manager->priv->lock);

		/* we remove the job from the list and signal it is finished */
		manager->priv->active_jobs = g_slist_remove (manager->priv->active_jobs, ctx);
		g_cond_signal (manager->priv->job_finished);

		if (!result || manager->priv->cancel) {
			/* we mark it for destruction (must be done in main loop)
			 * for safety reasons */
			ctx->cancel = 1;
		}
	
		/* insert the job in the results queue */
		manager->priv->results = g_slist_append (manager->priv->results, ctx);
		if (!manager->priv->results_id)
			manager->priv->results_id = g_idle_add ((GSourceFunc) brasero_async_job_manager_wait_for_results, manager);
	}

end:
	manager->priv->unused_threads --;
	manager->priv->num_threads --;

	/* maybe finalize is waiting for us to terminate */
	g_cond_signal (manager->priv->thread_finished);
	g_mutex_unlock (manager->priv->lock);

	g_thread_exit (NULL);

	return NULL;
}

gboolean
brasero_async_job_manager_queue (BraseroAsyncJobManager *manager,
				 gint type,
				 gpointer data)
{
	BraseroAsyncJobCtx *ctx;
	BraseroAsyncJobType *job_type;

	g_return_val_if_fail (manager != NULL, FALSE);
	g_return_val_if_fail (type > 0, FALSE);

	job_type = g_hash_table_lookup (manager->priv->types, GINT_TO_POINTER (type));
	if (!job_type)
		return FALSE;

	ctx = g_new0 (BraseroAsyncJobCtx, 1);
	ctx->data = data;
	ctx->common = job_type;

	g_mutex_lock (manager->priv->lock);
	manager->priv->waiting_jobs = g_slist_append (manager->priv->waiting_jobs, ctx);

	if (manager->priv->unused_threads) {
		/* wake up one thread in the list */
		g_cond_signal (manager->priv->new_job);
	}
	else if (manager->priv->num_threads < MANAGER_MAX_THREAD) {
		GError *error = NULL;
		GThread *thread;

		/* we have to start a new thread */
		thread = g_thread_create ((GThreadFunc) brasero_async_job_manager_thread,
					  manager,
					  FALSE,
					  &error);

		if (!thread) {
			g_warning ("Can't start thread : %s\n", error->message);
			g_error_free (error);

			manager->priv->waiting_jobs = g_slist_remove (manager->priv->waiting_jobs, ctx);
			g_mutex_unlock (manager->priv->lock);

			brasero_async_job_destroy (ctx, TRUE);
			return FALSE;
		}

		manager->priv->num_threads++;
	}
	/* else we wait for a currently active thread to be available */
	g_mutex_unlock (manager->priv->lock);

	return TRUE;
}

static GSList *
brasero_async_job_manager_remove_from_queue_by_object (GSList *list, GObject *obj)
{
	BraseroAsyncJobCtx *ctx;
	GSList *iter, *next;

	/* find all waiting jobs with this object */
	for (iter = list; iter; iter = next) {
		ctx = iter->data;
		next = iter->next;

		if (ctx->common->obj == obj) {
			list = g_slist_remove (list, ctx);
			brasero_async_job_destroy (ctx, TRUE);
		}
	}

	return list;
}

void
brasero_async_job_manager_cancel_by_object (BraseroAsyncJobManager *manager,
					    GObject *obj)
{
	GSList *iter;
	BraseroAsyncJobCtx *ctx;

	g_return_if_fail (manager != NULL);
	g_return_if_fail (obj != NULL);

	g_mutex_lock (manager->priv->lock);

	/* find all waiting jobs with this object */
	manager->priv->waiting_jobs = brasero_async_job_manager_remove_from_queue_by_object (manager->priv->waiting_jobs, obj);

	if (manager->priv->active_jobs) {
		/* have a look at the active_jobs */
		for (iter = manager->priv->active_jobs; iter; iter = iter->next) {
			ctx = iter->data;

			/* we simply call cancel not destroy since it will be done by
			 * the thread handling the job. It's up to the callback func to
			 * return FALSE */
			if (ctx->common->obj == obj)
				brasero_async_job_cancel (ctx);
		}
wait:
		/* Now we wait for all the active jobs of this type to be finished */
		g_cond_wait (manager->priv->job_finished, manager->priv->lock);
		for (iter = manager->priv->active_jobs; iter; iter = iter->next) {
			if (ctx->common->obj == obj)
				goto wait;
		}
	}

	/* find all the already done jobs in results with this object */
	manager->priv->results = brasero_async_job_manager_remove_from_queue_by_object (manager->priv->results, obj);

	g_mutex_unlock (manager->priv->lock);
}

gboolean
brasero_async_job_manager_find_urgent_job (BraseroAsyncJobManager *manager,
					   gint type,
					   BraseroAsyncFindJob func,
					   gpointer user_data)
{
	GSList *iter;
	BraseroAsyncJobCtx *ctx;
	BraseroAsyncJobType *real_type;

	g_return_val_if_fail (manager != NULL, FALSE);
	g_return_val_if_fail (func != NULL, FALSE);

	real_type = g_hash_table_lookup (manager->priv->types, GINT_TO_POINTER (type));

	g_mutex_lock (manager->priv->lock);
	for (iter = manager->priv->waiting_jobs; iter; iter = iter->next) {
		ctx = iter->data;

		if (ctx->common == real_type && func (ctx->data, user_data)) {
			manager->priv->waiting_jobs = g_slist_remove (manager->priv->waiting_jobs, ctx);
			manager->priv->waiting_jobs = g_slist_prepend (manager->priv->waiting_jobs, ctx);
			g_mutex_unlock (manager->priv->lock);
			return TRUE;
		}
	}
	g_mutex_unlock (manager->priv->lock);

	return FALSE;
}
