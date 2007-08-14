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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include "burn-basics.h"
#include "burn-debug.h"
#include "burn-session.h"
#include "burn-task.h"
#include "burn-task-item.h"
#include "burn-task-ctx.h"

static void brasero_task_class_init (BraseroTaskClass *klass);
static void brasero_task_init (BraseroTask *sp);
static void brasero_task_finalize (GObject *object);

typedef struct _BraseroTaskPrivate BraseroTaskPrivate;

struct _BraseroTaskPrivate {
	/* The loop for the task */
	GMainLoop *loop;

	/* used to poll for progress (every 0.5 sec) */
	gint clock_id;

	BraseroTaskItem *leader;

	/* result of the task */
	BraseroBurnResult retval;
	GError *error;
};

#define BRASERO_TASK_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_TASK, BraseroTaskPrivate))
G_DEFINE_TYPE (BraseroTask, brasero_task, BRASERO_TYPE_TASK_CTX);

static GObjectClass *parent_class = NULL;

void
brasero_task_add_item (BraseroTask *task, BraseroTaskItem *item)
{
	BraseroTaskPrivate *priv;

	g_return_if_fail (BRASERO_IS_TASK (task));
	g_return_if_fail (BRASERO_IS_TASK_ITEM (item));

	priv = BRASERO_TASK_PRIVATE (task);

	if (priv->leader) {
		brasero_task_item_connect (priv->leader, item);
		g_object_unref (priv->leader);
	}

	priv->leader = item;
	g_object_ref (priv->leader);
}

static void
brasero_task_reset_real (BraseroTask *task)
{
	BraseroTaskPrivate *priv;

	priv = BRASERO_TASK_PRIVATE (task);

	if (priv->loop)
		g_main_loop_unref (priv->loop);

	priv->loop = NULL;
	priv->clock_id = 0;
	priv->retval = BRASERO_BURN_OK;

	if (priv->error) {
		g_error_free (priv->error);
		priv->error = NULL;
	}

	brasero_task_ctx_reset (BRASERO_TASK_CTX (task));
}

void
brasero_task_reset (BraseroTask *task)
{
	BraseroTaskPrivate *priv;

	priv = BRASERO_TASK_PRIVATE (task);

	if (brasero_task_is_running (task))
		brasero_task_cancel (task, TRUE);

	g_object_unref (priv->leader);
	brasero_task_reset_real (task);
}

static gboolean
brasero_task_clock_tick (gpointer data)
{
	BraseroTask *task = BRASERO_TASK (data);
	BraseroTaskPrivate *priv;
	BraseroTaskItem *item;

	priv = BRASERO_TASK_PRIVATE (task);

	/* some jobs need to be called periodically to update their status
	 * because the main process run in a thread. We do it before calling
	 * progress/action changed so they can update the task on time */
	for (item = priv->leader; item; item = brasero_task_item_previous (item)) {
		BraseroTaskItemIFace *klass;

		klass = BRASERO_TASK_ITEM_GET_CLASS (item);
		if (klass->clock_tick)
			klass->clock_tick (item, BRASERO_TASK_CTX (task), NULL);
	}

	/* now call ctx to update progress */
	brasero_task_ctx_report_progress (BRASERO_TASK_CTX (data));

	return TRUE;
}

/**
 * Used to run/stop task
 */

static BraseroBurnResult
brasero_task_send_stop_signal (BraseroTask *task,
			       BraseroBurnResult retval,
			       GError **error)
{
	BraseroTaskItem *item;
	BraseroTaskPrivate *priv;
	GError *local_error = NULL;
	BraseroBurnResult result = retval;

	priv = BRASERO_TASK_PRIVATE (task);

	item = priv->leader;
	while (brasero_task_item_previous (item))
		item = brasero_task_item_previous (item);

	/* we stop all the slaves first and then go up the list */
	for (; item; item = brasero_task_item_next (item)) {
		BraseroTaskItemIFace *klass;
		GError *item_error;

		item_error = NULL;

		/* stop task for real now */
		BRASERO_BURN_LOG ("stopping %s", G_OBJECT_TYPE_NAME (item));

		klass = BRASERO_TASK_ITEM_GET_CLASS (item);
		if (klass->stop)
			result = klass->stop (item,
					      BRASERO_TASK_CTX (task),
					      &item_error);

		BRASERO_BURN_LOG ("stopped %s", G_OBJECT_TYPE_NAME (item));

		if (item_error) {
			g_error_free (local_error);
			local_error = item_error;
		}
	};

	if (local_error) {
		if (error && *error == NULL)
			g_propagate_error (error, local_error);
		else
			g_error_free (local_error);
	}

	/* we don't want to lose the original result if it was not OK */
	return (result == BRASERO_BURN_OK? retval:result);
}

static BraseroBurnResult
brasero_task_start_item (BraseroTask *task,
			 BraseroTaskItem *item,
			 GError **error)
{
	BraseroBurnResult result;
	BraseroTaskItemIFace *klass;

	klass = BRASERO_TASK_ITEM_GET_CLASS (item);
	if (!klass->start) {
		BRASERO_BURN_LOG ("no start method %s", G_OBJECT_TYPE_NAME (item));
		BRASERO_JOB_NOT_SUPPORTED (item);
	}

	BRASERO_BURN_LOG ("start method %s", G_OBJECT_TYPE_NAME (item));

	result = klass->start (item, BRASERO_TASK_CTX (task), error);
	return result;
}

static BraseroBurnResult
brasero_task_init_item (BraseroTask *task,
		        BraseroTaskItem *item,
		        GError **error)
{
	BraseroTaskItemIFace *klass;
	BraseroBurnResult result = BRASERO_BURN_OK;

	klass = BRASERO_TASK_ITEM_GET_CLASS (item);
	if (!klass->init) {
		BRASERO_BURN_LOG ("no init method %s", G_OBJECT_TYPE_NAME (item));
		return BRASERO_BURN_ERR;
	}

	BRASERO_BURN_LOG ("init method %s", G_OBJECT_TYPE_NAME (item));

	result = klass->init (item, BRASERO_TASK_CTX (task), error);
	return result;
}

static void
brasero_task_stop (BraseroTask *task,
		   BraseroBurnResult retval,
		   GError *error)
{
	BraseroBurnResult result;
	BraseroTaskPrivate *priv;

	priv = BRASERO_TASK_PRIVATE (task);

	/* brasero_job_error/brasero_job_finished should not be called during
	 * ::init and ::start. Instead a job should return any error directly */
	result = brasero_task_send_stop_signal (task, retval, &error);

	priv->retval = retval;
	priv->error = error;

	if (priv->loop && g_main_loop_is_running (priv->loop))
		g_main_loop_quit (priv->loop);
	else
		BRASERO_BURN_LOG ("Task was asked to stop (%i/%i) during ::init or ::start",
				  result, retval);
}

BraseroBurnResult
brasero_task_cancel (BraseroTask *task,
		     gboolean protect)
{
	BraseroTaskPrivate *priv;

	priv = BRASERO_TASK_PRIVATE (task);
	if (protect && brasero_task_ctx_get_dangerous (BRASERO_TASK_CTX (task)))
		return BRASERO_BURN_DANGEROUS;

	brasero_task_stop (task, BRASERO_BURN_CANCEL, NULL);
	return BRASERO_BURN_OK;
}

gboolean
brasero_task_is_running (BraseroTask *task)
{
	BraseroTaskPrivate *priv;

	priv = BRASERO_TASK_PRIVATE (task);
	return (priv->loop && g_main_loop_is_running (priv->loop));
}

static void
brasero_task_finished (BraseroTaskCtx *ctx,
		       BraseroBurnResult retval,
		       GError *error)
{
	BraseroTask *self;
	BraseroTaskPrivate *priv;

	self = BRASERO_TASK (ctx);
	priv = BRASERO_TASK_PRIVATE (self);

	if (retval == BRASERO_BURN_RETRY) {
		BraseroTaskItem *item;
		GError *error_item = NULL;

		/* There are some tracks left, get the first task item and
		 * restart it. */
		item = priv->leader;
		while (brasero_task_item_previous (item))
			item = brasero_task_item_previous (item);

		if(brasero_task_item_init (item, ctx, &error_item) != BRASERO_BURN_OK
		|| brasero_task_item_start (item, ctx, &error_item) != BRASERO_BURN_OK)
			brasero_task_stop (self, BRASERO_BURN_ERR, error_item);

		return;
	}

	brasero_task_stop (self, retval, error);
}

static BraseroBurnResult
brasero_task_run_real (BraseroTask *self,
		       GError **error)
{
	BraseroTaskPrivate *priv;

	priv = BRASERO_TASK_PRIVATE (self);

	brasero_task_ctx_report_progress (BRASERO_TASK_CTX (self));

	priv->clock_id = g_timeout_add (500,
					brasero_task_clock_tick,
					self);

	priv->loop = g_main_loop_new (NULL, FALSE);
	BRASERO_BURN_LOG ("entering loop");
	g_main_loop_run (priv->loop);
	BRASERO_BURN_LOG ("got out of loop");
	g_main_loop_unref (priv->loop);
	priv->loop = NULL;

	if (priv->error) {
		g_propagate_error (error, priv->error);
		priv->error = NULL;
	}

	/* stop all progress reporting thing */
	if (priv->clock_id) {
		g_source_remove (priv->clock_id);
		priv->clock_id = 0;
	}

	if (priv->retval == BRASERO_BURN_OK
	&&  brasero_task_ctx_get_progress (BRASERO_TASK_CTX (self), NULL) == BRASERO_BURN_OK) {
		brasero_task_ctx_set_progress (BRASERO_TASK_CTX (self), 1.0);
		brasero_task_ctx_set_written (BRASERO_TASK_CTX (self), -1.0);
		brasero_task_ctx_report_progress (BRASERO_TASK_CTX (self));
	}

	brasero_task_ctx_stop_progress (BRASERO_TASK_CTX (self));
	return priv->retval;	
}

static BraseroBurnResult
brasero_task_start (BraseroTask *self,
		    gboolean fake,
		    GError **error)
{
	BraseroBurnResult result = BRASERO_BURN_OK;
	BraseroTaskItem *item, *first, *last;
	BraseroTaskPrivate *priv;

	priv = BRASERO_TASK_PRIVATE (self);

	BRASERO_BURN_LOG ("Starting %s task (%i)",
			  fake ? "fake":"normal",
			  brasero_task_ctx_get_action (BRASERO_TASK_CTX (self)));

	/* check the task is not running */
	if (brasero_task_is_running (self)) {
		BRASERO_BURN_LOG ("task is already running");
		return BRASERO_BURN_RUNNING;
	}

	if (!priv->leader) {
		BRASERO_BURN_LOG ("no jobs");
		return BRASERO_BURN_RUNNING;
	}

	brasero_task_ctx_set_fake (BRASERO_TASK_CTX (self), fake);
	brasero_task_ctx_reset (BRASERO_TASK_CTX (self));

	/* first init all jobs starting from the master down to the slave */
	last = NULL;
	first = NULL;
	item = priv->leader;
	for (; item; item = brasero_task_item_previous (item)) {
		result = brasero_task_init_item (self, item, error);
		if (result == BRASERO_BURN_NOT_RUNNING) {
			/* Some jobs don't need to/can't run in fake mode or 
			 * have been already completed in ::init. So they return
			 * this value to skip ::start.
			 * NOTE: it is strictly forbidden for a job to call 
			 * brasero_job_finished within an init method. Only 
			 * brasero_job_error can be called from ::init. */
			BRASERO_BURN_LOG ("init method skipped for %s",
					  G_OBJECT_TYPE_NAME (item));
			last = item;
			result = BRASERO_BURN_OK;
			continue;
		}

		if (result != BRASERO_BURN_OK)
			goto error;

		first = item;
	}	

	/* now start from the slave up to the master */
	for (item = first; item && item != last; item = brasero_task_item_next (item)) {
		result = brasero_task_start_item (self, item, error);

		/* For ::start the only successful value is BRASERO_BURN_OK. All
		 * others are considered as errors. If a job is not always sure
		 * to run then it must implement ::init and tell us through the 
		 * return value. Again ::start is bound to succeed or fail. */
		if (result != BRASERO_BURN_OK)
			goto error;
	}

	if (first)
		result = brasero_task_run_real (self, error);

	return result;

error:
	brasero_task_send_stop_signal (self, result, NULL);
	return result;
}

BraseroBurnResult
brasero_task_check (BraseroTask *self,
		    GError **error)
{
	BraseroTaskAction action;

	g_return_val_if_fail (BRASERO_IS_TASK (self), BRASERO_BURN_ERR);

	/* the task MUST be of a BRASERO_TASK_ACTION_NORMAL type */
	action = brasero_task_ctx_get_action (BRASERO_TASK_CTX (self));
	if (action != BRASERO_TASK_ACTION_NORMAL)
		return BRASERO_BURN_OK;

	/* The main purpose of this function is to get the final size of the
	 * task output whether it be recorded to disc or stored as a file later.
	 * That size will be stored by task-ctx.
	 * To do this we run all the task in fake mode that means we don't write
	 * anything to disc or hard drive. Only the last running job in the
	 * chain will be aware that we're running in fake mode / get-size mode.
	 * All others will be told to image. To determine what should be the
	 * last job and therefore the one telling the final size of the output,
	 * we start to call ::init for each job starting from the leader. we
	 * don't skip recording jobs in case they modify the contents (and
	 * therefore the output size). When a job returns NOT_RUNNING after
	 * ::init then we skip it; this return value will mean that it won't
	 * change the output size or that it has already determined the output
	 * size. Only the last running job is allowed to set the final size (see
	 * burn-jobs.c), all values from other jobs will be ignored. */

	return brasero_task_start (self, TRUE, error);
}

BraseroBurnResult
brasero_task_run (BraseroTask *self,
		  GError **error)
{
	g_return_val_if_fail (BRASERO_IS_TASK (self), BRASERO_BURN_ERR);

	return brasero_task_start (self, FALSE, error);
}

static void
brasero_task_class_init (BraseroTaskClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroTaskCtxClass *ctx_class = BRASERO_TASK_CTX_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroTaskPrivate));

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_task_finalize;

	ctx_class->finished = brasero_task_finished;
}

static void
brasero_task_init (BraseroTask *obj)
{ }

static void
brasero_task_finalize (GObject *object)
{
	BraseroTask *cobj;
	BraseroTaskPrivate *priv;

	cobj = BRASERO_TASK (object);
	priv = BRASERO_TASK_PRIVATE (cobj);

	if (priv->leader) {
		g_object_unref (priv->leader);
		priv->leader = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

BraseroTask *
brasero_task_new ()
{
	BraseroTask *obj;
	
	obj = BRASERO_TASK (g_object_new (BRASERO_TYPE_TASK, NULL));
	return obj;
}
