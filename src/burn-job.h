/***************************************************************************
 *            job.h
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

#ifndef JOB_H
#define JOB_H

#include <glib.h>
#include <glib-object.h>

#include "burn-basics.h"
#include "burn-session.h"
#include "burn-task.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_JOB         (brasero_job_get_type ())
#define BRASERO_JOB(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_JOB, BraseroJob))
#define BRASERO_JOB_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_JOB, BraseroJobClass))
#define BRASERO_IS_JOB(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_JOB))
#define BRASERO_IS_JOB_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_JOB))
#define BRASERO_JOB_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_JOB, BraseroJobClass))

typedef struct BraseroJobPrivate BraseroJobPrivate;

typedef struct {
	GObject parent;
	BraseroJobPrivate *priv;
} BraseroJob;

typedef struct {
	GObjectClass parent_class;

	/* virtual functions */

	BraseroBurnResult	(*start_init)		(BraseroJob *job,
							 gboolean has_master,
							 GError **error);

	BraseroBurnResult	(*start)		(BraseroJob *job,
							 gint in_fd,
							 gint *out_fd,
							 GError **error);

	BraseroBurnResult	(*stop)			(BraseroJob *job,
							 BraseroBurnResult retval,
							 GError **error);

	BraseroBurnResult	(*set_source)		(BraseroJob *job,
							 const BraseroTrackSource *source,
							 GError **error);
	BraseroBurnResult	(*set_rate)		(BraseroJob *job,
							 gint64 rate);

	/* you should not connect to this signal. It's used internally to 
	 * "autoconfigure" the backend */
	BraseroBurnResult	(*error)		(BraseroJob *job,
							 BraseroBurnError error);
} BraseroJobClass;

GType brasero_job_get_type ();

BraseroBurnResult
brasero_job_cancel (BraseroJob *job,
		    gboolean protect);

BraseroBurnResult
brasero_job_set_task (BraseroJob *job,
		      BraseroTask *task);
BraseroBurnResult
brasero_job_get_task (BraseroJob *job,
		      BraseroTask **task,
		      BraseroJob **leader);

BraseroBurnResult
brasero_job_set_rate (BraseroJob *job,
		      gint64 rate);

BraseroBurnResult
brasero_job_set_source (BraseroJob *job,
			const BraseroTrackSource *source,
			GError **error);

gboolean
brasero_job_is_running (BraseroJob *job);

/* The following macros and functions are for implementation purposes */

/* logging facilities */
void
brasero_job_set_session (BraseroJob *job,
			 BraseroBurnSession *session);

void
brasero_job_log_message (BraseroJob *job,
			 const gchar *format,
			 ...);

#define BRASERO_JOB_LOG(job, message, ...) 			\
{								\
	gchar *format;						\
	format = g_strdup_printf ("%s (%s) %s",			\
				  G_STRINGIFY_ARG (job),	\
				  G_OBJECT_TYPE_NAME (job),	\
				  message);			\
	brasero_job_log_message (BRASERO_JOB (job),		\
				 format,		 	\
				 ##__VA_ARGS__);		\
	g_free (format);					\
}
#define BRASERO_JOB_LOG_ARG(job, message, ...)		\
{							\
	gchar *format;					\
	format = g_strdup_printf ("\t%s",		\
				  (gchar*) message);		\
	brasero_job_log_message (BRASERO_JOB (job),	\
				 format,		\
				 ##__VA_ARGS__);	\
	g_free (format);				\
}

#define BRASERO_JOB_NOT_SUPPORTED(job) 					\
	{								\
		BRASERO_JOB_LOG (job,					\
				 "unsupported operation (in %s at %s)",	\
				 G_STRFUNC,				\
				 G_STRLOC);				\
		return BRASERO_BURN_NOT_SUPPORTED;			\
	}

#define BRASERO_JOB_NOT_READY(job)					\
	{								\
		BRASERO_JOB_LOG (job,					\
				 "not ready to operate (in %s at %s)",	\
				 G_STRFUNC,				\
				 G_STRLOC);				\
		return BRASERO_BURN_NOT_READY;				\
	}

/* task progress report */
#define BRASERO_JOB_TASK_CONNECT_TO_CLOCK(job, function, clock_id)	\
{									\
	BraseroTask *task = NULL;					\
	brasero_job_get_task (BRASERO_JOB (job), &task,  NULL);		\
	if (task) {							\
		BRASERO_JOB_LOG (job, "connect_to_clock");		\
		clock_id = g_signal_connect (task,			\
					     "clock-tick",		\
					     G_CALLBACK (function),	\
					     job);			\
	}								\
}

#define BRASERO_JOB_TASK_DISCONNECT_FROM_CLOCK(job, clock_id)		\
{									\
	BraseroTask *task = NULL;					\
	brasero_job_get_task (BRASERO_JOB (job), &task,  NULL);		\
	if (task && clock_id) {							\
		BRASERO_JOB_LOG (job, "disconnect_from_clock");		\
		g_signal_handler_disconnect (task, clock_id);		\
		clock_id = 0;						\
	}								\
}

#define BRASERO_JOB_TASK_SET_ACTION(job, action, string, force)		\
{									\
	BraseroTask *task = NULL;					\
	brasero_job_get_task (BRASERO_JOB (job), &task,  NULL);		\
	if (task) {							\
		BRASERO_JOB_LOG (job, "set_action");			\
		brasero_task_set_action (task, action, string, force);	\
	}								\
}

#define BRASERO_JOB_TASK_GET_ACTION(job, action)			\
{									\
	BraseroTask *task = NULL;					\
	brasero_job_get_task (BRASERO_JOB (job), &task,  NULL);		\
	if (task) {							\
		BRASERO_JOB_LOG (job, "get_action");		\
		brasero_task_get_action (task, action);			\
	}								\
}


#define BRASERO_JOB_TASK_SET(job, action, info, function)	\
{								\
	BraseroTask *task = NULL;				\
	brasero_job_get_task (BRASERO_JOB (job), &task,  NULL);	\
	if (task) {						\
		BRASERO_JOB_LOG (job, action);			\
		function (task, info);				\
	}							\
}

#define BRASERO_JOB_TASK_SET_PROGRESS(job, progress)				\
	BRASERO_JOB_TASK_SET (job, "set_progress", progress, brasero_task_set_progress);

#define BRASERO_JOB_TASK_SET_WRITTEN(job, written)				\
	BRASERO_JOB_TASK_SET (job, "set_written", written, brasero_task_set_written)

#define BRASERO_JOB_TASK_SET_TOTAL(job, total)					\
	BRASERO_JOB_TASK_SET (job, "set_total", total, brasero_task_set_total)

#define BRASERO_JOB_TASK_SET_RATE(job, rate)					\
	BRASERO_JOB_TASK_SET (job, "set_rate", rate, brasero_task_set_rate)

#define BRASERO_JOB_TASK_SET_USE_AVERAGE_RATE(job, value)					\
	BRASERO_JOB_TASK_SET (job, "set_use_average_rate", value, brasero_task_set_use_average_rate)

/* job activation */
BraseroBurnResult
brasero_job_run (BraseroJob *last_job, GError **error);
BraseroBurnResult
brasero_job_finished (BraseroJob *job);
BraseroBurnResult
brasero_job_error (BraseroJob *job, GError *error);

#define BRASERO_JOB_TASK_START_PROGRESS(job, force)				\
	BRASERO_JOB_TASK_SET (job, "start_progress", force, brasero_task_start_progress);

void
brasero_job_set_dangerous (BraseroJob *job, gboolean value);

/* slave management */
BraseroJob *
brasero_job_get_slave (BraseroJob *master);
BraseroBurnResult
brasero_job_set_slave (BraseroJob *master, BraseroJob *slave);

void
brasero_job_set_run_slave (BraseroJob *job, gboolean run_slave);
void
brasero_job_set_relay_slave_signals (BraseroJob *job, gboolean relay);

G_END_DECLS

#endif /* JOB_H */
