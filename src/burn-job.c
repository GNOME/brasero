/***************************************************************************
 *            job.c
 *
 *  dim jan 22 10:40:26 2006
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include "burn-job.h"
#include "burn-basics.h"
#include "burn-debug.h"
#include "burn-session.h"
#include "burn-task.h"
#include "brasero-marshal.h"
 
static void brasero_job_class_init (BraseroJobClass *klass);
static void brasero_job_init (BraseroJob *sp);
static void brasero_job_finalize (GObject *object);

struct BraseroJobPrivate {
	BraseroBurnSession *session;
	BraseroTask *task;

	BraseroJob *master;
	BraseroJob *slave;

	guint relay_slave_signal:1;
	guint run_slave:1;

	guint is_running:1;
	guint dangerous:1;
};

typedef enum {
	ERROR_SIGNAL,
	LAST_SIGNAL
} BraseroJobSignalType;

static guint brasero_job_signals [LAST_SIGNAL] = { 0 };
static GObjectClass *parent_class = NULL;

GType
brasero_job_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroJobClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_job_class_init,
			NULL,
			NULL,
			sizeof (BraseroJob),
			0,
			(GInstanceInitFunc)brasero_job_init,
		};

		type = g_type_register_static(G_TYPE_OBJECT, 
			"BraseroJob", &our_info, 0);
	}

	return type;
}

static void
brasero_job_class_init (BraseroJobClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_job_finalize;

	brasero_job_signals [ERROR_SIGNAL] =
	    g_signal_new ("error",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  G_STRUCT_OFFSET (BraseroJobClass, error),
			  NULL, NULL,
			  brasero_marshal_INT__INT,
			  G_TYPE_INT, 1, G_TYPE_INT);
}

static void
brasero_job_init (BraseroJob *obj)
{
	obj->priv = g_new0 (BraseroJobPrivate, 1);
	obj->priv->relay_slave_signal = 1;
}

static void
brasero_job_finalize (GObject *object)
{
	BraseroJob *cobj;

	cobj = BRASERO_JOB (object);

	if (cobj->priv->is_running)
		brasero_job_cancel (cobj, TRUE);

	/* NOTE: it can't reach this function and have a
	 * master since the master holds a reference on it */
	if (cobj->priv->slave) {
		cobj->priv->slave->priv->master = NULL;
		g_object_unref (cobj->priv->slave);
		cobj->priv->slave = NULL;
	}

	if (cobj->priv->session) {
		g_object_unref (cobj->priv->session);
		cobj->priv->session = NULL;
	}

	if (cobj->priv->task) {
		g_object_unref (cobj->priv->task);
		cobj->priv->task = NULL;
	}

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

void
brasero_job_set_dangerous (BraseroJob *job, gboolean value)
{
	job->priv->dangerous = value;
}

void
brasero_job_set_session (BraseroJob *job, BraseroBurnSession *session)
{
	if (job->priv->session)
		g_object_unref (job->priv->session);

	if (session)
		g_object_ref (session);

	job->priv->session = session;
}

BraseroBurnResult
brasero_job_set_task (BraseroJob *job, BraseroTask *task)
{
	g_return_val_if_fail (job, BRASERO_BURN_ERR);

	BRASERO_JOB_LOG (job, "set_task");

	if (job->priv->master) {
		BRASERO_JOB_LOG (job, "A task can be assigned to a job with a master");
		return BRASERO_BURN_ERR;
	}

	if (job->priv->task)
		g_object_unref (job->priv->task);

	if (task)
		g_object_ref (task);

	job->priv->task = task;

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_task (BraseroJob *job, BraseroTask **task, BraseroJob **leader)
{
	BraseroJob *iter;

	/* it could happen that a master has been asked about a task being
	 * performed by one of his slave so we check the slaves to see if 
	 * they are not the leaders of some task */
	iter = job;
	while (iter && !iter->priv->task) {
		iter = iter->priv->master;
		if (!iter)
			return BRASERO_BURN_NOT_RUNNING;

		/* see if this parent accepts to relay to its children */
		if (!iter->priv->relay_slave_signal)
			return BRASERO_BURN_NOT_RUNNING;
	}

	if (!iter->priv->task)
		return BRASERO_BURN_NOT_RUNNING;

	if (leader)
		*leader = iter;

	if (task && iter)
		*task = iter->priv->task;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_job_plug (BraseroJob *job,
		  BraseroJob *prev_job,
		  int *in_fd,
		  int *out_fd,
		  GError **error)
{
	BraseroBurnResult result;
	BraseroJobClass *klass;

	klass = BRASERO_JOB_GET_CLASS (job);
	if (!klass->start) {
		BRASERO_JOB_LOG (job, "no start method");
		BRASERO_JOB_NOT_SUPPORTED (job);
	}

	result = klass->start (job,
			       *in_fd,
			       out_fd,
			       error);

	if (result != BRASERO_BURN_OK)
		return result;

	if (out_fd && *out_fd == -1) {
		BRASERO_JOB_LOG (job, "can't be plugged");
		BRASERO_JOB_NOT_SUPPORTED (job);
	}

	job->priv->is_running = 1;

	if (out_fd && *out_fd != -1) {
		*in_fd = *out_fd;
		*out_fd = -1;
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_job_send_stop_signal (BraseroJob *leader,
			      BraseroBurnResult retval,
			      GError **error)
{
	BraseroJob *iter;
	BraseroJobClass *klass;
	GError *local_error = NULL;
	BraseroBurnResult result = retval;

	/* we stop all the first slave first and then go up the list.
	 * That's why we reverse the list here. */
	iter = leader;
	while (iter->priv->slave && iter->priv->slave->priv->is_running)
		iter = iter->priv->slave;

	do {
		GError *job_error;

		job_error = NULL;

		/* stop task for real now */
		klass = BRASERO_JOB_GET_CLASS (iter);

		BRASERO_JOB_LOG (iter, "stopping");
		if (klass->stop)
			result = klass->stop (iter, result, &job_error);
		BRASERO_JOB_LOG (iter, "stopped");

		if (job_error) {
			g_error_free (local_error);
			local_error = job_error;
		}

		iter->priv->is_running = 0;
		iter = iter->priv->master;
	} while (iter && iter != leader->priv->master);

	if (local_error) {
		if (error && *error == NULL)
			g_propagate_error (error, local_error);
		else
			g_error_free (local_error);
	}

	/* we don't want to lose the original result if it was not OK */
	if (retval != BRASERO_BURN_OK)
		return retval;

	return result;
}

static BraseroBurnResult
brasero_job_pre_init (BraseroJob *job,
		      gboolean has_master_running,
		      GError **error)
{
	BraseroJobClass *klass;
	BraseroBurnResult result = BRASERO_BURN_OK;

	klass = BRASERO_JOB_GET_CLASS (job);

	if (!BRASERO_IS_JOB (job)) {
		BRASERO_JOB_LOG (job, "not a job");
		return BRASERO_BURN_ERR;
	}

	if (klass->start_init)
		result = klass->start_init (job,
					    has_master_running,
					    error);

	return result;
}

BraseroBurnResult
brasero_job_run (BraseroJob *job, GError **error)
{
	BraseroBurnResult result = BRASERO_BURN_OK;
	BraseroJob *prev_job = NULL;
	BraseroTask *task = NULL;
	BraseroJob *iter;
	int out_fd = -1;
	int in_fd = -1;

	/* check the job or one of its master is not running */
	if (brasero_job_is_running (job)) {
		BRASERO_JOB_LOG (job, "is already running");
		return BRASERO_BURN_RUNNING;
	}

	result = brasero_job_get_task (job, &task, NULL);
	if (result != BRASERO_BURN_OK) {
		BRASERO_JOB_LOG (job, "is not associated with a task");
		return result;
	}

	/* we first init all jobs starting from the master down to the slave */
	iter = job;
	while (1) {
		result = brasero_job_pre_init (iter, (iter != job), error);
		if (result != BRASERO_BURN_OK)
			goto error;

		if (!iter->priv->run_slave)
			break;

		if (!iter->priv->slave)
			break;

		iter = iter->priv->slave;
	}	

	/* now start from the slave up to the master */
	prev_job = NULL;
	while (iter != job) {
		result = brasero_job_plug (iter,
					   prev_job,
					   &in_fd,
					   &out_fd,
					   error);

		if (result != BRASERO_BURN_OK)
			goto error;
	
		prev_job = iter;
		iter = iter->priv->master;
	}

	result = brasero_job_plug (iter,
				   prev_job,
				   &in_fd,
				   NULL,
				   error);

	if (result != BRASERO_BURN_OK)
		goto error;

	result = brasero_task_start (task, error);
	return result;

error:
	brasero_job_send_stop_signal (job, result, NULL);
	return result;
}

gboolean
brasero_job_is_running (BraseroJob *job)
{
	g_return_val_if_fail (job != NULL, FALSE);

	if (job->priv->is_running)
		return TRUE;

	return FALSE;
}

static BraseroBurnResult
brasero_job_stop (BraseroJob *job,
		  BraseroBurnResult retval,
		  GError *error)
{
	BraseroBurnResult result = BRASERO_BURN_OK;
	BraseroTask *task = NULL;
	BraseroJob *leader;

	if (error) {
		BRASERO_JOB_LOG (job,
				 "asked to stop\n"
				 "\tstatus\t= %i\n"
				 "\terror\t\t= %i\n"
				 "\tmessage\t= \"%s\"",
				 retval,
				 error->code,
				 error->message);
	}
	else
		BRASERO_JOB_LOG (job,
				 "asked to stop\n"
				 "\tstatus\t= %i",
				 retval);

	/* we need two things the first master's running and the task */
	task = NULL;
	brasero_job_get_task (job, &task, NULL);
	if (!task) {
		BRASERO_JOB_LOG (job, "this job is not associated with a task.");
		return BRASERO_BURN_NOT_RUNNING;
	}

	/* search the first job running (the highest master) */
	if (job->priv->is_running) {
		BraseroJob *iter = job;

		/* search the master which is running. */
		while (iter) {
			if (iter->priv->is_running)
				leader = iter;

			iter = iter->priv->master;
		}

		/* we discard all messages from slaves saying that all is good
		 * since a slave can be finished before his master has finished
		 * processing all the data it was sent. */
		if (job != leader && retval == BRASERO_BURN_OK) {
			BRASERO_JOB_LOG (job, "is not a leader");
			return BRASERO_BURN_OK;
		}
	}
	else {
		BraseroJob *iter = job->priv->slave;

		/* the job itself is not running but maybe one of its slaves is */
		leader = NULL;
		while (iter && !iter->priv->is_running)
			iter = iter->priv->slave;

		leader = iter;

		if (!leader) {
			BRASERO_JOB_LOG (job, "the job isn't running (nor his slaves)");
			return BRASERO_BURN_NOT_RUNNING;
		}
	}
	
	/* tell all the jobs we've stopped */
	result = brasero_job_send_stop_signal (leader, retval, &error);
	brasero_task_stop (task, retval, error);
	BRASERO_JOB_LOG (job, "got out of loop");
	return result;
}

BraseroBurnResult
brasero_job_cancel (BraseroJob *job, gboolean protect)
{
	g_return_val_if_fail (job != NULL, BRASERO_BURN_ERR);

	if (protect && job->priv->dangerous)
		return BRASERO_BURN_DANGEROUS;

	return brasero_job_stop (job, BRASERO_BURN_CANCEL, NULL);
}

/* used for implementation */
BraseroBurnResult
brasero_job_finished (BraseroJob *job)
{
	return brasero_job_stop (job, BRASERO_BURN_OK, NULL);
}

BraseroBurnResult
brasero_job_error (BraseroJob *job, GError *error)
{
	GValue instance_and_params [2];
	GValue return_value;

	instance_and_params [0].g_type = 0;
	g_value_init (instance_and_params, G_TYPE_FROM_INSTANCE (job));
	g_value_set_instance (instance_and_params, job);

	instance_and_params [1].g_type = 0;
	g_value_init (instance_and_params + 1, G_TYPE_INT);
	g_value_set_int (instance_and_params + 1, error->code);

	return_value.g_type = 0;
	g_value_init (&return_value, G_TYPE_INT);
	g_value_set_int (&return_value, BRASERO_BURN_ERR);

	/* There was an error: signal it. That's mainly done
	 * for BraseroBurnCaps to override the result value */
	g_signal_emitv (instance_and_params,
			brasero_job_signals [ERROR_SIGNAL],
			0,
			&return_value);

	g_value_unset (instance_and_params);

	return brasero_job_stop (job, g_value_get_int (&return_value), error);
}

/*******************************************************************************/
void
brasero_job_set_run_slave (BraseroJob *job, gboolean run_slave)
{
	g_return_if_fail (BRASERO_IS_JOB (job));
	job->priv->run_slave = (run_slave == TRUE);
}

void
brasero_job_set_relay_slave_signals (BraseroJob *job, gboolean relay)
{
	g_return_if_fail (BRASERO_IS_JOB (job));
	job->priv->relay_slave_signal = relay;
}

BraseroJob *
brasero_job_get_slave (BraseroJob *master)
{
	g_return_val_if_fail (BRASERO_IS_JOB (master), NULL);
	return master->priv->slave;
}

BraseroBurnResult
brasero_job_set_slave (BraseroJob *master, BraseroJob *slave)
{
	g_return_val_if_fail (BRASERO_IS_JOB (master), BRASERO_BURN_ERR);
	g_return_val_if_fail (master != slave, BRASERO_BURN_ERR);

	if (slave)
		g_return_val_if_fail (BRASERO_IS_JOB (slave), BRASERO_BURN_ERR);

	if (slave && slave->priv->task) {
		BRASERO_JOB_LOG (slave, "the slave has a task");
		return BRASERO_BURN_RUNNING;
	}

	/* check if one of them is running */
	if (brasero_job_is_running (master)
	|| (slave && brasero_job_is_running (slave)))
		return BRASERO_BURN_RUNNING;

	/* set */
	if (master->priv->slave) {
		master->priv->slave->priv->master = NULL;
		g_object_unref (master->priv->slave);
	}

	master->priv->slave = slave;

	if (!slave)
		return BRASERO_BURN_OK;

	/* NOTE: the slave may already have a reference from a master,
	 * that's why we don't unref it to ref it just afterwards in 
	 * case its reference count reaches zero in between*/
	if (slave->priv->master)
		slave->priv->master->priv->slave = NULL;
	else
		g_object_ref (slave);

	slave->priv->master = master;
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_set_rate (BraseroJob *job,
		      gint64 rate)
{
	BraseroJobClass *klass;

	g_return_val_if_fail (BRASERO_IS_JOB (job), BRASERO_BURN_ERR);

	BRASERO_JOB_LOG (job, "set_rate");

	if (brasero_job_is_running (job))
		return BRASERO_BURN_RUNNING;

	klass = BRASERO_JOB_GET_CLASS (job);
	if (!klass->set_rate)
		BRASERO_JOB_NOT_SUPPORTED (job);

	return (* klass->set_rate) (job, rate);
}

BraseroBurnResult
brasero_job_set_source (BraseroJob *job,
			const BraseroTrackSource *source,
			GError **error)
{
	BraseroJobClass *klass;

	g_return_val_if_fail (BRASERO_IS_JOB (job), BRASERO_BURN_ERR);

	BRASERO_JOB_LOG (job, "set_source");

	if (brasero_job_is_running (job))
		return BRASERO_BURN_RUNNING;

	klass = BRASERO_JOB_GET_CLASS (job);
	if (!klass->set_source)
		BRASERO_JOB_NOT_SUPPORTED (job);

	return (* klass->set_source) (job,
				       source,
				       error);
}

/************************** used for debugging *********************************/
void
brasero_job_log_message (BraseroJob *job,
			 const gchar *format,
			 ...)
{
	g_return_if_fail (job != NULL);
	g_return_if_fail (format != NULL);

	if (job->priv->session) {
		va_list arg_list;

		va_start (arg_list, format);
		brasero_burn_session_logv (job->priv->session, format, arg_list);
		va_end (arg_list);
	}

	/* it all depends on the master */
	while (job->priv->master) job = job->priv->master;

	BRASERO_BURN_LOGV (format);
}
