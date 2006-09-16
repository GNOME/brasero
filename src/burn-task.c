/***************************************************************************
 *            burn-task.c
 *
 *  mer sep 13 09:16:29 2006
 *  Copyright  2006  Rouquier Philippe
 *  bonfire-app@wanadoo.fr
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

#include "burn-task.h"
#include "burn-basics.h"
#include "burn-common.h"

static void brasero_task_class_init (BraseroTaskClass *klass);
static void brasero_task_init (BraseroTask *sp);
static void brasero_task_finalize (GObject *object);

struct _BraseroTaskPrivate {
	/* The loop for the task */
	GMainLoop *loop;

	/* used to poll for progress (every 0.5 sec) */
	gint progress_report_id;
	gdouble progress;

	gint64 written;
	gint64 total;

	/* keep track of time */
	GTimer *timer;
	gint64 first_written;

	/* used for immediate rate */
	gint64 current_written;
	gdouble current_elapsed;
	gint64 last_written;
	gdouble last_elapsed;

	/* used for remaining time */
	GSList *times;
	gdouble total_time;

	/* used for rates that certain jobs are able to report */
	gint64 rate;

	/* the current action */
	BraseroBurnAction action;
	gchar *action_string;

	GMutex *action_mutex;

	/* result of the task */
	BraseroBurnResult retval;
	GError *error;

	gint action_changed:1;
	gint written_changed:1;
	gint use_average_rate:1;
};

enum _BraseroTaskSignalType {
	CLOCK_TICK_SIGNAL,
	ACTION_CHANGED_SIGNAL,
	PROGRESS_CHANGED_SIGNAL,
	LAST_SIGNAL
};

static guint brasero_task_signals [LAST_SIGNAL] = { 0 };
static GObjectClass *parent_class = NULL;

GType
brasero_task_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroTaskClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_task_class_init,
			NULL,
			NULL,
			sizeof (BraseroTask),
			0,
			(GInstanceInitFunc)brasero_task_init,
		};

		type = g_type_register_static (G_TYPE_OBJECT, 
					       "BraseroTask",
					       &our_info,
					       0);
	}

	return type;
}

static void
brasero_task_class_init (BraseroTaskClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_task_finalize;

	brasero_task_signals [CLOCK_TICK_SIGNAL] =
	    g_signal_new ("clock-tick",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (BraseroTaskClass,
					   clock_tick),
			  NULL, NULL,
			  g_cclosure_marshal_VOID__VOID,
			  G_TYPE_NONE,
			  0);

	brasero_task_signals [PROGRESS_CHANGED_SIGNAL] =
	    g_signal_new ("progress_changed",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (BraseroTaskClass,
					   progress_changed),
			  NULL, NULL,
			  g_cclosure_marshal_VOID__VOID,
			  G_TYPE_NONE,
			  0);

	brasero_task_signals [ACTION_CHANGED_SIGNAL] =
	    g_signal_new ("action_changed",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST,
			  G_STRUCT_OFFSET (BraseroTaskClass,
					   action_changed),
			  NULL, NULL,
			  g_cclosure_marshal_VOID__INT,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_INT);
}

static void
brasero_task_init (BraseroTask *obj)
{
	obj->priv = g_new0 (BraseroTaskPrivate, 1);
}

static void
brasero_task_stop_real (BraseroTask *task)
{
	if (task->priv->action_string) {
		g_free (task->priv->action_string);
		task->priv->action_string = NULL;
	}
	task->priv->action = BRASERO_BURN_ACTION_NONE;
	task->priv->action_changed = 0;

	/* stop all progress reporting thing */
	if (task->priv->progress_report_id) {
		g_source_remove (task->priv->progress_report_id);
		task->priv->progress_report_id = 0;
	}

	if (task->priv->timer) {
		g_timer_destroy (task->priv->timer);
		task->priv->timer = NULL;
	}
	task->priv->first_written = 0;

	g_mutex_lock (task->priv->action_mutex);
	if (task->priv->times) {
		g_slist_free (task->priv->times);
		task->priv->times = NULL;
	}
	g_mutex_unlock (task->priv->action_mutex);
}

static void
brasero_task_finalize (GObject *object)
{
	BraseroTask *cobj;

	cobj = BRASERO_TASK (object);

	brasero_task_stop_real (cobj);

	if (cobj->priv->action_mutex) {
		g_mutex_free (cobj->priv->action_mutex);
		cobj->priv->action_mutex = NULL;
	}

	if (cobj->priv->error) {
		g_error_free (cobj->priv->error);
		cobj->priv->error = NULL;
	}

	g_free (cobj->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

BraseroTask *
brasero_task_new ()
{
	BraseroTask *obj;
	
	obj = BRASERO_TASK (g_object_new (BRASERO_TYPE_TASK, NULL));
	obj->priv->action_mutex = g_mutex_new ();
	return obj;
}

static gboolean
brasero_task_report_progress_cb (gpointer data)
{
	BraseroTask *task = BRASERO_TASK (data);
	gdouble progress, elapsed;

	/* some jobs need to be called periodically to update their status
	 * because the main process run in a thread. We do it before calling
	 * progress/action changed so they can update the task on time */
	g_signal_emit (task,
		       brasero_task_signals [CLOCK_TICK_SIGNAL],
		       0);

	if (task->priv->action_changed) {
		g_signal_emit (task,
			       brasero_task_signals [ACTION_CHANGED_SIGNAL],
			       0,
			       task->priv->action);

		task->priv->action_changed = 0;
	}

	if (task->priv->written_changed) {
		task->priv->written_changed = 0;
		g_signal_emit (task,
			       brasero_task_signals [PROGRESS_CHANGED_SIGNAL],
			       0);
	}

	if (!task->priv->timer)
		return TRUE;

	elapsed = g_timer_elapsed (task->priv->timer, NULL);
	if (brasero_task_get_progress (task, &progress) == BRASERO_BURN_OK) {
		gdouble total_time;

		total_time = (gdouble) elapsed / (gdouble) progress;

		g_mutex_lock (task->priv->action_mutex);
		task->priv->total_time = brasero_burn_common_get_average (&task->priv->times,
									  total_time);
		g_mutex_unlock (task->priv->action_mutex);
	}

	return TRUE;
}

static void
brasero_task_reset (BraseroTask *task)
{
	task->priv->loop = NULL;
	task->priv->progress_report_id = 0;
	task->priv->progress = -1.0;
	task->priv->written = -1;
	task->priv->written_changed = 0;
	task->priv->timer = NULL;
	task->priv->current_written = 0;
	task->priv->current_elapsed = 0;
	task->priv->last_written = 0;
	task->priv->last_elapsed = 0;
	task->priv->retval = BRASERO_BURN_OK;

	if (task->priv->error) {
		g_error_free (task->priv->error);
		task->priv->error = NULL;
	}

	if (task->priv->times) {
		g_slist_free (task->priv->times);
		task->priv->times = NULL;
	}
}

BraseroBurnResult
brasero_task_start (BraseroTask *task,
		    GError **error)
{
	if (!task)
		return BRASERO_BURN_NOT_RUNNING;

	brasero_task_reset (task);

	task->priv->loop = g_main_loop_new (NULL, FALSE);

	g_signal_emit (task,
		       brasero_task_signals [ACTION_CHANGED_SIGNAL],
		       0,
		       task->priv->action);

	g_signal_emit (task,
		       brasero_task_signals [PROGRESS_CHANGED_SIGNAL],
		       0);

	task->priv->progress_report_id = g_timeout_add (500,
							brasero_task_report_progress_cb,
							task);

	g_main_loop_run (task->priv->loop);

	if (task->priv->error) {
		g_propagate_error (error, task->priv->error);
		task->priv->error = NULL;
	}

	brasero_task_stop_real (task);

	if (task->priv->retval == BRASERO_BURN_OK
	&&  brasero_task_get_progress (task, NULL) == BRASERO_BURN_OK) {
		task->priv->progress = 1.0;
		task->priv->written = -1;
	}

	g_signal_emit (task,
		       brasero_task_signals [PROGRESS_CHANGED_SIGNAL],
		       0);

	return task->priv->retval;	
}

BraseroBurnResult
brasero_task_start_progress (BraseroTask *task,
			     gboolean force)
{
	if (!task)
		return BRASERO_BURN_NOT_RUNNING;

	if (!task->priv->timer) {
		task->priv->timer = g_timer_new ();
		task->priv->first_written = task->priv->written;
	}
	else if (force) {
		g_timer_start (task->priv->timer);
		task->priv->first_written = task->priv->written;
	}

	return BRASERO_BURN_OK;
}

void
brasero_task_stop (BraseroTask *task,
		   BraseroBurnResult retval,
		   GError *error)
{
	if (!task)
		return;

	task->priv->retval = retval;
	task->priv->error = error;

	if (task->priv->loop
	&&  g_main_loop_is_running (task->priv->loop))
		g_main_loop_quit (task->priv->loop);
}

/* used to set the different values of the task by the jobs */
BraseroBurnResult
brasero_task_set_progress (BraseroTask *task, gdouble progress)
{
	if (!task)
		return BRASERO_BURN_NOT_RUNNING;

	task->priv->progress = progress;
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_task_set_action (BraseroTask *task,
			 BraseroBurnAction action,
			 const gchar *string,
			 gboolean force)
{
	if (!task)
		return BRASERO_BURN_NOT_RUNNING;

	if (!force && task->priv->action == action)
		return BRASERO_BURN_OK;

	g_mutex_lock (task->priv->action_mutex);

	task->priv->action = action;
	task->priv->action_changed = 1;

	if (task->priv->action_string)
		g_free (task->priv->action_string);

	task->priv->action_string = string ? g_strdup (string): NULL;

	if (!force) {
		g_slist_free (task->priv->times);
		task->priv->times = NULL;
	}

	g_mutex_unlock (task->priv->action_mutex);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_task_set_rate (BraseroTask *task,
		       gint64 rate)
{
	if (!task)
		return BRASERO_BURN_NOT_RUNNING;

	task->priv->rate = rate;
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_task_set_total (BraseroTask *task,
			gint64 total)
{
	if (!task)
		return BRASERO_BURN_NOT_RUNNING;

	task->priv->total = total;
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_task_set_written (BraseroTask *task,
			  gint64 written)
{
	gdouble elapsed = 0.0;

	if (!task)
		return BRASERO_BURN_NOT_RUNNING;

	task->priv->written = written;
	task->priv->written_changed = 1;

	if (task->priv->use_average_rate)
		return BRASERO_BURN_OK;

	if (task->priv->timer)
		elapsed = g_timer_elapsed (task->priv->timer, NULL);

	if ((elapsed - task->priv->last_elapsed) > 0.5) {
		task->priv->last_written = task->priv->current_written;
		task->priv->last_elapsed = task->priv->current_elapsed;
		task->priv->current_written = written;
		task->priv->current_elapsed = elapsed;
	}

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_task_get_action (BraseroTask *task,
			 BraseroBurnAction *action)
{
	if (!task)
		return BRASERO_BURN_NOT_RUNNING;

	g_mutex_lock (task->priv->action_mutex);
	*action = task->priv->action;
	g_mutex_unlock (task->priv->action_mutex);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_task_get_action_string (BraseroTask *task,
				BraseroBurnAction action,
				gchar **string)
{
	if (!task)
		return BRASERO_BURN_NOT_RUNNING;

	if (!string)
		return BRASERO_BURN_OK;

	if (action != task->priv->action)
		return BRASERO_BURN_ERR;

	*string = task->priv->action_string ? g_strdup (task->priv->action_string):
					      g_strdup (brasero_burn_action_to_string (task->priv->action));

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_task_get_progress (BraseroTask *task, gdouble *progress)
{
	if (!task)
		return BRASERO_BURN_NOT_RUNNING;

	if (task->priv->progress >= 0.0) {
		if (progress)
			*progress = task->priv->progress;

		return BRASERO_BURN_OK;
	}

	if (task->priv->written < 0 || task->priv->total <= 0)
		return BRASERO_BURN_NOT_READY;

	if (!progress)
		return BRASERO_BURN_OK;

	*progress = (gdouble) task->priv->written /
		    (gdouble) task->priv->total;

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_task_get_elapsed (BraseroTask *task, gdouble *elapsed)
{
	if (!task)
		return BRASERO_BURN_NOT_RUNNING;

	if (!task->priv->timer)
		return BRASERO_BURN_NOT_READY;

	if (elapsed)
		*elapsed = g_timer_elapsed (task->priv->timer, NULL);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_task_get_rate (BraseroTask *task, gint64 *rate)
{
	if (!task)
		return BRASERO_BURN_NOT_RUNNING;

	if (!rate)
		return BRASERO_BURN_OK;

	if (task->priv->action != BRASERO_BURN_ACTION_WRITING
	&&  task->priv->action != BRASERO_BURN_ACTION_DRIVE_COPY) {
		*rate = -1;
		return BRASERO_BURN_OK;
	}

	if (task->priv->rate) {
		*rate = task->priv->rate;
		return BRASERO_BURN_OK;
	}

	if (task->priv->use_average_rate){
		gdouble elapsed;

		if (!task->priv->written || !task->priv->timer)
			return BRASERO_BURN_NOT_READY;

		elapsed = g_timer_elapsed (task->priv->timer, NULL);
		*rate = (gdouble) task->priv->written / elapsed;
	}
	else {
		if (!task->priv->last_written)
			return BRASERO_BURN_NOT_READY;

		*rate = (gdouble) (task->priv->current_written - task->priv->last_written) /
			(gdouble) (task->priv->current_elapsed - task->priv->last_elapsed);
	}

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_task_get_average_rate (BraseroTask *task, gint64 *rate)
{
	gdouble elapsed;

	if (!task)
		return BRASERO_BURN_NOT_RUNNING;

	if (!rate)
		return BRASERO_BURN_OK;

	if (!task->priv->timer)
		return BRASERO_BURN_NOT_READY;

	elapsed = g_timer_elapsed (task->priv->timer, NULL);
	if (!elapsed)
		return BRASERO_BURN_NOT_READY;

	/* calculate average rate */
	*rate = ((task->priv->written - task->priv->first_written) / elapsed);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_task_get_remaining_time (BraseroTask *task, long *remaining)
{
	gdouble elapsed;
	gint len;

	if (!task)
		return BRASERO_BURN_NOT_RUNNING;

	if (!remaining)
		return BRASERO_BURN_OK;

	g_mutex_lock (task->priv->action_mutex);
	len = g_slist_length (task->priv->times);
	g_mutex_unlock (task->priv->action_mutex);

	if (len < MAX_VALUE_AVERAGE)
		return BRASERO_BURN_NOT_READY;

	elapsed = g_timer_elapsed (task->priv->timer, NULL);
	*remaining = (gdouble) task->priv->total_time - (gdouble) elapsed;

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_task_get_written (BraseroTask *task, gint64 *written)
{
	if (!task)
		return BRASERO_BURN_NOT_RUNNING;

	if (task->priv->written <= 0)
		return BRASERO_BURN_NOT_READY;

	if (!written)
		return BRASERO_BURN_OK;

	*written = task->priv->written;
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_task_get_total (BraseroTask *task, gint64 *total)
{
	if (!task)
		return BRASERO_BURN_NOT_RUNNING;

	if (task->priv->total <= 0
	&&  task->priv->written
	&&  task->priv->progress)
		return BRASERO_BURN_NOT_READY;

	if (!total)
		return BRASERO_BURN_OK;

	if (task->priv->total <= 0)
		*total = task->priv->written / task->priv->progress;
	else
		*total = task->priv->total;

	return BRASERO_BURN_OK;
}

void
brasero_task_set_use_average_rate (BraseroTask *task, gboolean value)
{
	if (!task)
		return;

	task->priv->use_average_rate = value;
}
