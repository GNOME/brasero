/***************************************************************************
 *            process.c
 *
 *  dim jan 22 10:39:50 2006
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

#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include "burn-basics.h"
#include "burn-process.h"
#include "burn-job.h"

static void brasero_process_class_init (BraseroProcessClass *klass);
static void brasero_process_init (BraseroProcess *sp);
static void brasero_process_finalize (GObject *object);

static BraseroBurnResult
brasero_process_pre_init (BraseroJob *job,
			  gboolean has_master,
			  GError **error);
static BraseroBurnResult
brasero_process_start (BraseroJob *job,
		       int in_fd,
		       int *out_fd,
		       GError **error);
static BraseroBurnResult
brasero_process_stop (BraseroJob *job,
		      BraseroBurnResult retval,
		      GError **error);
enum {
	BRASERO_CHANNEL_STDOUT	= 0,
	BRASERO_CHANNEL_STDERR
};

static const gchar *debug_prefixes [] = {	"stdout: %s",
						"stderr: %s",
						NULL };

typedef BraseroBurnResult	(*BraseroProcessReadFunc)	(BraseroProcess *process,
								 const char *line);
struct BraseroProcessPrivate {
	GPtrArray *argv;

	GIOChannel *std_out;
	GString *out_buffer;

	GIOChannel *std_error;
	GString *err_buffer;

	GPid pid;
	gint io_out;
	gint io_err;
	gint io_in;
};

static GObjectClass *parent_class = NULL;

GType
brasero_process_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroProcessClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_process_class_init,
			NULL,
			NULL,
			sizeof (BraseroProcess),
			0,
			(GInstanceInitFunc)brasero_process_init,
		};

		type = g_type_register_static(BRASERO_TYPE_JOB, 
			"BraseroProcess", &our_info, 0);
	}

	return type;
}

static void
brasero_process_class_init (BraseroProcessClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroJobClass *job_class = BRASERO_JOB_CLASS (klass);

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_process_finalize;

	job_class->start_init = brasero_process_pre_init;
	job_class->start = brasero_process_start;
	job_class->stop = brasero_process_stop;
}

static void
brasero_process_init (BraseroProcess *obj)
{
	obj->priv = g_new0 (BraseroProcessPrivate, 1);
}

static void
brasero_process_finalize (GObject *object)
{
	BraseroProcess *cobj;
	cobj = BRASERO_PROCESS(object);

	if (cobj->priv->io_out) {
		g_source_remove (cobj->priv->io_out);
		cobj->priv->io_out = 0;
	}

	if (cobj->priv->std_out) {
		g_io_channel_unref (cobj->priv->std_out);
		cobj->priv->std_out = NULL;
	}

	if (cobj->priv->out_buffer) {
		g_string_free (cobj->priv->out_buffer, TRUE);
		cobj->priv->out_buffer = NULL;
	}

	if (cobj->priv->io_err) {
		g_source_remove (cobj->priv->io_err);
		cobj->priv->io_err = 0;
	}

	if (cobj->priv->std_error) {
		g_io_channel_unref (cobj->priv->std_error);
		cobj->priv->std_error = NULL;
	}

	if (cobj->priv->err_buffer) {
		g_string_free (cobj->priv->err_buffer, TRUE);
		cobj->priv->err_buffer = NULL;
	}

	if (cobj->priv->pid) {
		kill (cobj->priv->pid, SIGKILL);
		cobj->priv->pid = 0;
	}

	if (cobj->priv->argv) {
		g_strfreev ((gchar**) cobj->priv->argv->pdata);
		g_ptr_array_free (cobj->priv->argv, FALSE);
		cobj->priv->argv = NULL;
	}

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static BraseroBurnResult
brasero_process_pre_init (BraseroJob *job,
			  gboolean has_master,
			  GError **error)
{
	BraseroProcessClass *klass = BRASERO_PROCESS_GET_CLASS (job);
	BraseroProcess *process = BRASERO_PROCESS (job);
	BraseroBurnResult result;
	int i;

	if (process->priv->pid)
		return BRASERO_BURN_RUNNING;

	if (!klass->set_argv)
		BRASERO_JOB_NOT_SUPPORTED (process);

	BRASERO_JOB_LOG (process, "getting varg");

	if (process->priv->argv) {
		g_strfreev ((gchar**) process->priv->argv->pdata);
		g_ptr_array_free (process->priv->argv, FALSE);
	}

	process->priv->argv = g_ptr_array_new ();
	result = klass->set_argv (process,
				  process->priv->argv,
				  has_master,
				  error);
	g_ptr_array_add (process->priv->argv, NULL);

	BRASERO_JOB_LOG (process, "got varg:");

	for (i = 0; process->priv->argv->pdata [i]; i++)
		BRASERO_JOB_LOG_ARG (process,
				     process->priv->argv->pdata [i]);

	if (result != BRASERO_BURN_OK) {
		g_strfreev ((gchar**) process->priv->argv->pdata);
		g_ptr_array_free (process->priv->argv, FALSE);
		process->priv->argv = NULL;
		return result;
	}

	return BRASERO_BURN_OK;
}

static gboolean
brasero_process_read (BraseroProcess *process,
		      GIOChannel *channel,
		      GIOCondition condition,
		      gint channel_type,
		      BraseroProcessReadFunc read)
{
	GString *buffer;
	GIOStatus status;
	BraseroBurnResult result = BRASERO_BURN_OK;

	if (!channel)
		return FALSE;

	if (channel_type == BRASERO_CHANNEL_STDERR)
		buffer = process->priv->err_buffer;
	else
		buffer = process->priv->out_buffer;

	if (condition & G_IO_IN) {
		gsize term;
		gchar *line = NULL;

		status = g_io_channel_read_line (channel,
						 &line,
						 &term,
						 NULL,
						 NULL);

		if (status == G_IO_STATUS_AGAIN) {
			gchar character;
			/* line is NULL since there wasn't any character ending the line and so it wasn't
			 * read at all.
			 * some processes (like cdrecord/cdrdao) produce an extra character sometimes */
			status = g_io_channel_read_chars (channel,
							  &character,
							  1,
							  NULL,
							  NULL);

			if (status == G_IO_STATUS_NORMAL) {
				g_string_append_c (buffer, character);

				switch (character) {
				case '\n':
				case '\r':
				case '\xe2':
				case '\0':
					BRASERO_JOB_LOG (process,
							 debug_prefixes [channel_type],
							 buffer->str);

					if (read)
						result = read (process, buffer->str);

					/* a subclass could have stopped or errored out.
					 * in this case brasero_process_stop will have 
					 * been called and the buffer deallocated. So we
					 * check that it still exists */
					if (channel_type == BRASERO_CHANNEL_STDERR)
						buffer = process->priv->err_buffer;
					else
						buffer = process->priv->out_buffer;

					if (buffer)
						g_string_set_size (buffer, 0);

					if (result != BRASERO_BURN_OK)
						return FALSE;

					break;
				default:
					break;
				}
			}
		}
		else if (status == G_IO_STATUS_NORMAL) {
			if (term)
				line [term - 1] = '\0';

			g_string_append (buffer, line);
			g_free (line);

			BRASERO_JOB_LOG (process,
					 debug_prefixes [channel_type],
					 buffer->str);

			if (read)
				result = read (process, buffer->str);

			/* a subclass could have stopped or errored out.
			 * in this case brasero_process_stop will have 
			 * been called and the buffer deallocated. So we
			 * check that it still exists */
			if (channel_type == BRASERO_CHANNEL_STDERR)
				buffer = process->priv->err_buffer;
			else
				buffer = process->priv->out_buffer;

			if (buffer)
				g_string_set_size (buffer, 0);

			if (result != BRASERO_BURN_OK)
				return FALSE;
		}
		else if (status == G_IO_STATUS_EOF) {
			BRASERO_JOB_LOG (process, 
					 debug_prefixes [channel_type],
					 "EOF");
			return FALSE;
		}
		else
			return FALSE;
	}
	else if (condition & G_IO_HUP) {
		/* only handle the HUP when we have read all available lines of output */
		BRASERO_JOB_LOG (process,
				 debug_prefixes [channel_type],
				 "HUP");
		return FALSE;
	}

	return TRUE;
}

static gboolean
brasero_process_read_stderr (GIOChannel *source,
			     GIOCondition condition,
			     BraseroProcess *process)
{
	gboolean result;
	BraseroProcessClass *klass;

	if (!process->priv->err_buffer)
		process->priv->err_buffer = g_string_new (NULL);

	klass = BRASERO_PROCESS_GET_CLASS (process);
	result = brasero_process_read (process,
				       source,
				       condition,
				       BRASERO_CHANNEL_STDERR,
				       klass->stderr_func);

	if (result)
		return result;

	process->priv->io_err = 0;

	g_io_channel_unref (process->priv->std_error);
	process->priv->std_error = NULL;

	g_string_free (process->priv->err_buffer, TRUE);
	process->priv->err_buffer = NULL;
	
	if (process->priv->pid
	&& !process->priv->io_err
	&& !process->priv->io_out)
		brasero_job_finished (BRASERO_JOB (process));

	return FALSE;
}

static gboolean
brasero_process_read_stdout (GIOChannel *source,
			     GIOCondition condition,
			     BraseroProcess *process)
{
	gboolean result;
	BraseroProcessClass *klass;

	if (!process->priv->out_buffer)
		process->priv->out_buffer = g_string_new (NULL);

	klass = BRASERO_PROCESS_GET_CLASS (process);
	result = brasero_process_read (process,
				       source,
				       condition,
				       BRASERO_CHANNEL_STDOUT,
				       klass->stdout_func);

	if (result)
		return result;

	process->priv->io_out = 0;

	if (process->priv->std_out) {
		g_io_channel_unref (process->priv->std_out);
		process->priv->std_out = NULL;
	}

	g_string_free (process->priv->out_buffer, TRUE);
	process->priv->out_buffer = NULL;

	if (process->priv->pid
	&& !process->priv->io_err
	&& !process->priv->io_out)
		brasero_job_finished (BRASERO_JOB (process));

	return FALSE;
}

static void
brasero_process_setup_stdin (gpointer data)
{
	int stdin_pipe = GPOINTER_TO_INT (data);

	if (stdin_pipe == -1)
		return;

	if (dup2 (stdin_pipe, 0) == -1)
		g_warning ("Dup2 failed\n");
}

static GIOChannel *
brasero_process_setup_channel (BraseroProcess *process,
			       int pipe,
			       gint *watch,
			       GIOFunc function)
{
	GIOChannel *channel;

	fcntl (pipe, F_SETFL, O_NONBLOCK);
	channel = g_io_channel_unix_new (pipe);
	g_io_channel_set_flags (channel,
				g_io_channel_get_flags (channel) | G_IO_FLAG_NONBLOCK,
				NULL);
	g_io_channel_set_encoding (channel, NULL, NULL);
	*watch = g_io_add_watch (channel,
				(G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL),
				 function,
				 process);

	g_io_channel_set_close_on_unref (channel, TRUE);
	return channel;
}

static BraseroBurnResult
brasero_process_start (BraseroJob *job, int in_fd, int *out_fd, GError **error)
{
	BraseroProcess *process = BRASERO_PROCESS (job);
	int stdout_pipe, stderr_pipe;
	/* that's to make sure programs are not translated */
	gchar *envp [] = {	"LANG=C",
				"LANGUAGE=C"
				"LC_ALL=C",
				NULL};

	if (process->priv->pid)
		return BRASERO_BURN_RUNNING;

	BRASERO_JOB_LOG (process, "launching command");

	if (!g_spawn_async_with_pipes (NULL,
				       (gchar **) process->priv->argv->pdata,
				       (gchar **) envp,
				       G_SPAWN_SEARCH_PATH,
				       brasero_process_setup_stdin,
				       GINT_TO_POINTER (in_fd),
				       &process->priv->pid,
				       NULL,
				       &stdout_pipe,
				       &stderr_pipe,
				       error))
		return BRASERO_BURN_ERR;

	/* error channel */
	process->priv->std_error = brasero_process_setup_channel (process,
								  stderr_pipe,
								  &process->priv->io_err,
								  (GIOFunc) brasero_process_read_stderr);

	/* we only watch stdout coming from the last object in the queue */
	if (!out_fd)
		process->priv->std_out = brasero_process_setup_channel (process,
									stdout_pipe,
									&process->priv->io_out,
									(GIOFunc) brasero_process_read_stdout);
	else
		*out_fd = stdout_pipe;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_process_stop (BraseroJob *job,
		      BraseroBurnResult retval,
		      GError **error)
{
	BraseroBurnResult result = BRASERO_BURN_OK;
	BraseroProcessClass *klass;
	BraseroProcess *process;
	GIOCondition condition;

	process = BRASERO_PROCESS (job);

	/* it might happen that the slave detected an error triggered by the master
	 * BEFORE the master so we finish reading whatever is in the pipes to see: 
	 * fdsink will notice cdrecord closed the pipe before cdrecord reports it */
	if (process->priv->pid) {
		GPid pid;

		pid = process->priv->pid;
		process->priv->pid = 0;

		if (kill (pid, SIGQUIT) == -1 && errno != ESRCH) {
			BRASERO_JOB_LOG (process, 
					 "process (%s) couldn't be killed: terminating",
					 strerror (errno));
			kill (pid, SIGKILL);
		}
		else
			BRASERO_JOB_LOG (process, "got killed");

		g_spawn_close_pid (pid);
	}

	/* read every pending data and close the pipes */
	if (process->priv->io_out) {
		g_source_remove (process->priv->io_out);
		process->priv->io_out = 0;
	}

	if (process->priv->std_out) {
		condition = g_io_channel_get_buffer_condition (process->priv->std_out);
		if (condition == G_IO_IN) {
			BraseroProcessClass *klass;
	
			klass = BRASERO_PROCESS_GET_CLASS (process);
			while (brasero_process_read (process,
						     process->priv->std_out,
						     G_IO_IN,
						     BRASERO_CHANNEL_STDOUT,
						     klass->stdout_func));
		}

	    	/* NOTE: we already checked if process->priv->std_out wasn't 
		 * NULL but brasero_process_read could have closed it */
	    	if (process->priv->std_out) {
			g_io_channel_unref (process->priv->std_out);
			process->priv->std_out = NULL;
		}
	}

	if (process->priv->out_buffer) {
		g_string_free (process->priv->out_buffer, TRUE);
		process->priv->out_buffer = NULL;
	}

	if (process->priv->io_err) {
		g_source_remove (process->priv->io_err);
		process->priv->io_err = 0;
	}

	if (process->priv->std_error) {
		condition = g_io_channel_get_buffer_condition (process->priv->std_error);
		if (condition == G_IO_IN) {
			BraseroProcessClass *klass;
	
			klass = BRASERO_PROCESS_GET_CLASS (process);
			while (brasero_process_read (process,
						     process->priv->std_error,
						     G_IO_IN,
						     BRASERO_CHANNEL_STDERR,
						     klass->stderr_func));
		}

	    	/* NOTE: we already checked if process->priv->std_out wasn't 
		 * NULL but brasero_process_read could have closed it */
	    	if (process->priv->std_error) {
			g_io_channel_unref (process->priv->std_error);
			process->priv->std_error = NULL;
		}
	}

	if (process->priv->err_buffer) {
		g_string_free (process->priv->err_buffer, TRUE);
		process->priv->err_buffer = NULL;
	}

	if (process->priv->argv) {
		g_strfreev ((gchar**) process->priv->argv->pdata);
		g_ptr_array_free (process->priv->argv, FALSE);
		process->priv->argv = NULL;
	}

	klass = BRASERO_PROCESS_GET_CLASS (process);
	if (klass->post) {
		result = klass->post (process, retval);
		return result;
	}

	return retval;
}
