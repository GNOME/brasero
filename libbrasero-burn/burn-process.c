/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-burn
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-burn is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-burn authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-burn. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-burn is distributed in the hope that it will be useful,
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

#include <sys/types.h>
#include <sys/wait.h>
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

#include <gio/gio.h>

#include "burn-basics.h"
#include "burn-process.h"
#include "burn-job.h"

#include "brasero-track-stream.h"
#include "brasero-track-image.h"

G_DEFINE_TYPE (BraseroProcess, brasero_process, BRASERO_TYPE_JOB);

enum {
	BRASERO_CHANNEL_STDOUT	= 0,
	BRASERO_CHANNEL_STDERR
};

typedef BraseroBurnResult	(*BraseroProcessReadFunc)	(BraseroProcess *process,
								 const gchar *line);

typedef struct _BraseroProcessPrivate BraseroProcessPrivate;
struct _BraseroProcessPrivate {
	GPtrArray *argv;

	/* deferred error that will be used if the process doesn't return 0 */
	GError *error;

	GIOChannel *std_out;
	GString *out_buffer;

	GIOChannel *std_error;
	GString *err_buffer;

	gchar *working_directory;

	GPid pid;
	gint io_out;
	gint io_err;
	gint io_in;

	guint watch;
	guint return_status;

	guint process_finished:1;
};

#define BRASERO_PROCESS_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_PROCESS, BraseroProcessPrivate))

void
brasero_process_deferred_error (BraseroProcess *self,
				GError *error)
{
	BraseroProcessPrivate *priv;

	priv = BRASERO_PROCESS_PRIVATE (self);
	if (priv->error)
		g_error_free (priv->error);

	priv->error = error;
}

static BraseroBurnResult
brasero_process_ask_argv (BraseroJob *job,
			  GError **error)
{
	BraseroProcessClass *klass = BRASERO_PROCESS_GET_CLASS (job);
	BraseroProcessPrivate *priv = BRASERO_PROCESS_PRIVATE (job);
	BraseroProcess *process = BRASERO_PROCESS (job);
	BraseroBurnResult result;
	int i;

	if (priv->pid)
		return BRASERO_BURN_RUNNING;

	if (!klass->set_argv)
		BRASERO_JOB_NOT_SUPPORTED (process);

	BRASERO_JOB_LOG (process, "getting varg");

	if (priv->argv) {
		g_strfreev ((gchar**) priv->argv->pdata);
		g_ptr_array_free (priv->argv, FALSE);
	}

	priv->argv = g_ptr_array_new ();
	result = klass->set_argv (process,
				  priv->argv,
				  error);
	g_ptr_array_add (priv->argv, NULL);

	BRASERO_JOB_LOG (process, "got varg:");

	for (i = 0; priv->argv->pdata [i]; i++)
		BRASERO_JOB_LOG_ARG (process,
				     "%s",
				     priv->argv->pdata [i]);

	if (result != BRASERO_BURN_OK) {
		g_strfreev ((gchar**) priv->argv->pdata);
		g_ptr_array_free (priv->argv, FALSE);
		priv->argv = NULL;
		return result;
	}

	return BRASERO_BURN_OK;
}

static void
brasero_process_add_automatic_track (BraseroProcess *self)
{
	BraseroBurnResult result;
	BraseroTrack *track = NULL;
	BraseroTrackType *type = NULL;
	BraseroJobAction action = BRASERO_BURN_ACTION_NONE;
	BraseroProcessPrivate *priv = BRASERO_PROCESS_PRIVATE (self);

	/* On error, don't automatically add a track */
	if (priv->return_status)
		return;

	/* See if the plugin already added some new
	 * tracks while it was running; if so, don't add it
	 * automatically */
	if (brasero_job_get_done_tracks (BRASERO_JOB (self), NULL) == BRASERO_BURN_OK)
		return;

	/* Only the last running job when it images to a
	 * file should add a track.
	 * NOTE: the last job in a task is the one that
	 * does not pipe anything. */
	if (brasero_job_get_fd_out (BRASERO_JOB (self), NULL) == BRASERO_BURN_OK)
		return;

	brasero_job_get_action (BRASERO_JOB (self), &action);
	if (action != BRASERO_JOB_ACTION_IMAGE)
		return;

	/* Now add a new track */
	type = brasero_track_type_new ();
	result = brasero_job_get_output_type (BRASERO_JOB (self), type);
	if (result != BRASERO_BURN_OK) {
		brasero_track_type_free (type);
		return;
	}

	BRASERO_JOB_LOG (self, "Automatically adding track");

	/* NOTE: we are only able to handle the two
	 * following track types. For other ones, the
	 * plugin is supposed to handle that itself */
	if (brasero_track_type_get_has_image (type)) {
		gchar *toc = NULL;
		gchar *image = NULL;
		goffset blocks = 0;

		track = BRASERO_TRACK (brasero_track_image_new ());
		brasero_job_get_image_output (BRASERO_JOB (self),
					      &image,
					      &toc);

		brasero_track_image_set_source (BRASERO_TRACK_IMAGE (track),
						image,
						toc,
						brasero_track_type_get_image_format (type));

		g_free (image);
		g_free (toc);

		brasero_job_get_session_output_size (BRASERO_JOB (self), &blocks, NULL);
		brasero_track_image_set_block_num (BRASERO_TRACK_IMAGE (track), blocks);
	}
	else if (brasero_track_type_get_has_stream (type)) {
		gchar *uri = NULL;

		track = BRASERO_TRACK (brasero_track_stream_new ());
		brasero_job_get_audio_output (BRASERO_JOB (self), &uri);
		brasero_track_stream_set_source (BRASERO_TRACK_STREAM (track), uri);
		brasero_track_stream_set_format (BRASERO_TRACK_STREAM (track),
						 brasero_track_type_get_stream_format (type));

		g_free (uri);
	}
	brasero_track_type_free (type);

	if (track) {
		brasero_job_add_track (BRASERO_JOB (self), track);

		/* It's good practice to unref the track afterwards as we don't
		 * need it anymore. BraseroTaskCtx refs it. */
		g_object_unref (track);
	}
}

static BraseroBurnResult
brasero_process_finished (BraseroProcess *self)
{
	BraseroProcessPrivate *priv = BRASERO_PROCESS_PRIVATE (self);
	BraseroProcessClass *klass = BRASERO_PROCESS_GET_CLASS (self);

	priv->process_finished = TRUE;

	/* check if an error went undetected */
	if (priv->return_status) {
		if (priv->error) {
			brasero_job_error (BRASERO_JOB (self),
					   g_error_new (BRASERO_BURN_ERROR,
							BRASERO_BURN_ERROR_GENERAL,
						        /* Translators: %s is the name of the brasero element */
							_("Process \"%s\" ended with an error code (%i)"),
							G_OBJECT_TYPE_NAME (self),
							priv->return_status));
		}
		else {
			brasero_job_error (BRASERO_JOB (self), priv->error);
			priv->error = NULL;
		}

		return BRASERO_BURN_OK;
	}

	/* This is a deferred error that is an error that
	 * was by a plugin and that should be only used
	 * if the process finishes with a bad return value */
	if (priv->error) {
		g_error_free (priv->error);
		priv->error = NULL;
	}

	/* Tell the world we're done */
	return klass->post (BRASERO_JOB (self));
}

static gboolean
brasero_process_watch_child (gpointer data)
{
	int status;
	BraseroBurnResult result;
	BraseroProcessPrivate *priv = BRASERO_PROCESS_PRIVATE (data);

	if (waitpid (priv->pid, &status, WNOHANG) <= 0)
		return TRUE;

	/* store the return value it will be checked only if no 
	 * brasero_job_finished/_error is called before the pipes are closed so
	 * as to let plugins read stderr / stdout till the end and set a better
	 * error message or simply decide all went well, in one word override */
	priv->return_status = WEXITSTATUS (status);
	priv->watch = 0;
	priv->pid = 0;

	BRASERO_JOB_LOG (data, "process finished with status %i", WEXITSTATUS (status));

	result = brasero_process_finished (data);
	if (result == BRASERO_BURN_RETRY) {
		GError *error = NULL;
		BraseroJobClass *job_class;

		priv->process_finished = FALSE;

		job_class = BRASERO_JOB_GET_CLASS (data);
		if (job_class->stop) {
			result = job_class->stop (data, &error);
			if (result != BRASERO_BURN_OK) {
				brasero_job_error (data, error);
				return FALSE;
			}
		}

		if (job_class->start) {
			/* we were asked by the plugin to restart it */
			result = job_class->start (data, &error);
			if (result != BRASERO_BURN_OK)
				brasero_job_error (data, error);
		}
	}

	return FALSE;
}

#define PROCESS_READ_LOG(msg)					\
{								\
	if (channel_type == BRASERO_CHANNEL_STDERR)		\
		BRASERO_JOB_LOG (process, "stderr: %s", msg)	\
	else							\
		BRASERO_JOB_LOG (process, "stdout: %s", msg)	\
}

static gboolean
brasero_process_read (BraseroProcess *process,
		      GIOChannel *channel,
		      GIOCondition condition,
		      gint channel_type,
		      BraseroProcessReadFunc readfunc)
{
	GString *buffer;
	GIOStatus status;
	BraseroBurnResult result = BRASERO_BURN_OK;
	BraseroProcessPrivate *priv = BRASERO_PROCESS_PRIVATE (process);

	if (!channel)
		return FALSE;

	if (channel_type == BRASERO_CHANNEL_STDERR)
		buffer = priv->err_buffer;
	else
		buffer = priv->out_buffer;

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
			/* there wasn't any character ending the line.
			 * some processes (like cdrecord/cdrdao) produce an
			 * extra character sometimes */
			status = g_io_channel_read_chars (channel,
							  &character,
							  1,
							  NULL,
							  NULL);

			if (status == G_IO_STATUS_NORMAL) {
				g_string_append_c (buffer, character);

				switch (character) {
				case '\b':
				case '\n':
				case '\r':
				case '\xe2': /* Unicode paragraph separator */
				case '\0':
					PROCESS_READ_LOG (buffer->str);

					if (readfunc && buffer->str [0] != '\0')
						result = readfunc (process, buffer->str);

					/* a subclass could have stopped or errored out.
					 * in this case brasero_process_stop will have 
					 * been called and the buffer deallocated. So we
					 * check that it still exists */
					if (channel_type == BRASERO_CHANNEL_STDERR)
						buffer = priv->err_buffer;
					else
						buffer = priv->out_buffer;

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

			PROCESS_READ_LOG (buffer->str);

			if (readfunc && buffer->str [0] != '\0')
				result = readfunc (process, buffer->str);

			/* a subclass could have stopped or errored out.
			 * in this case brasero_process_stop will have 
			 * been called and the buffer deallocated. So we
			 * check that it still exists */
			if (channel_type == BRASERO_CHANNEL_STDERR)
				buffer = priv->err_buffer;
			else
				buffer = priv->out_buffer;

			if (buffer)
				g_string_set_size (buffer, 0);

			if (result != BRASERO_BURN_OK)
				return FALSE;
		}
		else if (status == G_IO_STATUS_EOF) {
			PROCESS_READ_LOG ("EOF");
			return FALSE;
		}
		else
			return FALSE;
	}
	else if (condition & G_IO_HUP) {
		/* only handle the HUP when we have read all available lines of output */
		PROCESS_READ_LOG ("HUP");
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
	BraseroProcessPrivate *priv = BRASERO_PROCESS_PRIVATE (process);

	if (!priv->err_buffer)
		priv->err_buffer = g_string_new (NULL);

	klass = BRASERO_PROCESS_GET_CLASS (process);
	result = brasero_process_read (process,
				       source,
				       condition,
				       BRASERO_CHANNEL_STDERR,
				       klass->stderr_func);

	if (result)
		return result;

	priv->io_err = 0;

	g_io_channel_unref (priv->std_error);
	priv->std_error = NULL;

	g_string_free (priv->err_buffer, TRUE);
	priv->err_buffer = NULL;

	/* What if the above function (brasero_process_read called
	 * brasero_job_finished */
	if (priv->pid
	&& !priv->io_err
	&& !priv->io_out) {
		/* setup a child watch callback to be warned when it finishes so
		 * as to check the return value for errors.
		 * need to reap our children by ourselves g_child_watch_add
		 * doesn't work well with multiple processes. regularly poll
		 * with waitpid ()*/
		priv->watch = g_timeout_add (500, brasero_process_watch_child, process);
	}
	return FALSE;
}

static gboolean
brasero_process_read_stdout (GIOChannel *source,
			     GIOCondition condition,
			     BraseroProcess *process)
{
	gboolean result;
	BraseroProcessClass *klass;
	BraseroProcessPrivate *priv = BRASERO_PROCESS_PRIVATE (process);

	if (!priv->out_buffer)
		priv->out_buffer = g_string_new (NULL);

	klass = BRASERO_PROCESS_GET_CLASS (process);
	result = brasero_process_read (process,
				       source,
				       condition,
				       BRASERO_CHANNEL_STDOUT,
				       klass->stdout_func);

	if (result)
		return result;

	priv->io_out = 0;

	if (priv->std_out) {
		g_io_channel_unref (priv->std_out);
		priv->std_out = NULL;
	}

	g_string_free (priv->out_buffer, TRUE);
	priv->out_buffer = NULL;

	if (priv->pid
	&& !priv->io_err
	&& !priv->io_out) {
		/* setup a child watch callback to be warned when it finishes so
		 * as to check the return value for errors.
		 * need to reap our children by ourselves g_child_watch_add
		 * doesn't work well with multiple processes. regularly poll
		 * with waitpid ()*/
		priv->watch = g_timeout_add (500, brasero_process_watch_child, process);
	}

	return FALSE;
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

	/* It'd be good if we were allowed to add some to the default line
	 * separator */
//	g_io_channel_set_line_term (channel, "\b", -1);

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

static void
brasero_process_setup (gpointer data)
{
	BraseroProcess *process;
	int fd;

	process = BRASERO_PROCESS (data);

	fd = -1;
	if (brasero_job_get_fd_in (BRASERO_JOB (process), &fd) == BRASERO_BURN_OK) {
		if (dup2 (fd, 0) == -1)
			BRASERO_JOB_LOG (process, "Dup2 failed: %s", g_strerror (errno));
	}

	fd = -1;
	if (brasero_job_get_fd_out (BRASERO_JOB (process), &fd) == BRASERO_BURN_OK) {
		if (dup2 (fd, 1) == -1)
			BRASERO_JOB_LOG (process, "Dup2 failed: %s", g_strerror (errno));
	}
}

static BraseroBurnResult
brasero_process_start (BraseroJob *job, GError **error)
{
	BraseroProcessPrivate *priv = BRASERO_PROCESS_PRIVATE (job);
	BraseroProcess *process = BRASERO_PROCESS (job);
	int stdout_pipe, stderr_pipe;
	BraseroProcessClass *klass;
	BraseroBurnResult result;
	gboolean read_stdout;
	/* that's to make sure programs are not translated */
	gchar *envp [] = {	"LANG=C",
				"LANGUAGE=C",
				"LC_ALL=C",
				NULL};

	if (priv->pid)
		return BRASERO_BURN_RUNNING;

	/* ask the arguments for the program */
	result = brasero_process_ask_argv (job, error);
	if (result != BRASERO_BURN_OK)
		return result;

	if (priv->working_directory) {
		BRASERO_JOB_LOG (process, "Launching command in %s", priv->working_directory);
	}
	else {
		BRASERO_JOB_LOG (process, "Launching command");
	}

	klass = BRASERO_PROCESS_GET_CLASS (process);

	/* only watch stdout coming from the last object in the queue */
	read_stdout = (klass->stdout_func &&
		       brasero_job_get_fd_out (BRASERO_JOB (process), NULL) != BRASERO_BURN_OK);

	priv->process_finished = FALSE;
	priv->return_status = 0;

	if (!g_spawn_async_with_pipes (priv->working_directory,
				       (gchar **) priv->argv->pdata,
				       (gchar **) envp,
				       G_SPAWN_SEARCH_PATH|
				       G_SPAWN_DO_NOT_REAP_CHILD,
				       brasero_process_setup,
				       process,
				       &priv->pid,
				       NULL,
				       read_stdout ? &stdout_pipe : NULL,
				       &stderr_pipe,
				       error)) {
		return BRASERO_BURN_ERR;
	}

	/* error channel */
	priv->std_error = brasero_process_setup_channel (process,
							 stderr_pipe,
							&priv->io_err,
							(GIOFunc) brasero_process_read_stderr);

	if (read_stdout)
		priv->std_out = brasero_process_setup_channel (process,
							       stdout_pipe,
							       &priv->io_out,
							       (GIOFunc) brasero_process_read_stdout);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_process_stop (BraseroJob *job,
		      GError **error)
{
	BraseroBurnResult result = BRASERO_BURN_OK;
	BraseroProcessPrivate *priv;
	BraseroProcess *process;

	process = BRASERO_PROCESS (job);
	priv = BRASERO_PROCESS_PRIVATE (process);

	if (priv->watch) {
		/* if the child is still running at this stage 
		 * that means that we were cancelled or
		 * that we decided to stop ourselves so
		 * don't check the returned value */
		g_source_remove (priv->watch);
		priv->watch = 0;
	}

	/* it might happen that the slave detected an
	 * error triggered by the master BEFORE the
	 * master so we finish reading whatever is in
	 * the pipes to see: fdsink will notice cdrecord
	 * closed the pipe before cdrecord reports it */
	if (priv->pid) {
		GPid pid;

		pid = priv->pid;
		priv->pid = 0;

		/* Reminder: -1 is here to send the signal
		 * to all children of the process with pid as
		 * well */
		if (pid > 0 && kill ((-1) * pid, SIGTERM) == -1 && errno != ESRCH) {
			BRASERO_JOB_LOG (process, 
					 "process (%s) couldn't be killed: terminating",
					 g_strerror (errno));
			kill ((-1) * pid, SIGKILL);
		}
		else
			BRASERO_JOB_LOG (process, "got killed");

		g_spawn_close_pid (pid);
	}

	/* read every pending data and close the pipes */
	if (priv->io_out) {
		g_source_remove (priv->io_out);
		priv->io_out = 0;
	}

	if (priv->std_out) {
		if (error && !(*error)) {
			BraseroProcessClass *klass;

			/* we need to nullify the buffer since we've just read a line
			 * that got the job to stop so if we don't do that following 
			 * data will be appended to this line but each will provoke the
			 * same stop every time */
			if (priv->out_buffer)
				g_string_set_size (priv->out_buffer, 0);

			klass = BRASERO_PROCESS_GET_CLASS (process);
			while (priv->std_out && g_io_channel_get_buffer_condition (priv->std_out) == G_IO_IN)
				brasero_process_read (process,
						      priv->std_out,
						      G_IO_IN,
						      BRASERO_CHANNEL_STDOUT,
						      klass->stdout_func);
		}

	    	/* NOTE: we already checked if priv->std_out wasn't 
		 * NULL but brasero_process_read could have closed it */
	    	if (priv->std_out) {
			g_io_channel_unref (priv->std_out);
			priv->std_out = NULL;
		}
	}

	if (priv->out_buffer) {
		g_string_free (priv->out_buffer, TRUE);
		priv->out_buffer = NULL;
	}

	if (priv->io_err) {
		g_source_remove (priv->io_err);
		priv->io_err = 0;
	}

	if (priv->std_error) {
		if (error && !(*error)) {
			BraseroProcessClass *klass;
		
			/* we need to nullify the buffer since we've just read a line
			 * that got the job to stop so if we don't do that following 
			 * data will be appended to this line but each will provoke the
			 * same stop every time */
			if (priv->err_buffer)
				g_string_set_size (priv->err_buffer, 0);

			klass = BRASERO_PROCESS_GET_CLASS (process);
			while (priv->std_error && g_io_channel_get_buffer_condition (priv->std_error) == G_IO_IN)
				brasero_process_read (process,
						     priv->std_error,
						     G_IO_IN,
						     BRASERO_CHANNEL_STDERR,
						     klass->stderr_func);
		}

	    	/* NOTE: we already checked if priv->std_out wasn't 
		 * NULL but brasero_process_read could have closed it */
	    	if (priv->std_error) {
			g_io_channel_unref (priv->std_error);
			priv->std_error = NULL;
		}
	}

	if (priv->err_buffer) {
		g_string_free (priv->err_buffer, TRUE);
		priv->err_buffer = NULL;
	}

	if (priv->argv) {
		g_strfreev ((gchar**) priv->argv->pdata);
		g_ptr_array_free (priv->argv, FALSE);
		priv->argv = NULL;
	}

	if (priv->error) {
		g_error_free (priv->error);
		priv->error = NULL;
	}

	/* See if we need to automatically add a track */
	if (priv->process_finished)
		brasero_process_add_automatic_track (BRASERO_PROCESS (job));

	return result;
}

void
brasero_process_set_working_directory (BraseroProcess *process,
				       const gchar *directory)
{
	BraseroProcessPrivate *priv = BRASERO_PROCESS_PRIVATE (process);

	if (priv->working_directory) {
		g_free (priv->working_directory);
		priv->working_directory = NULL;
	}

	priv->working_directory = g_strdup (directory);
}

static void
brasero_process_finalize (GObject *object)
{
	BraseroProcessPrivate *priv = BRASERO_PROCESS_PRIVATE (object);

	if (priv->watch) {
		g_source_remove (priv->watch);
		priv->watch = 0;
	}

	if (priv->io_out) {
		g_source_remove (priv->io_out);
		priv->io_out = 0;
	}

	if (priv->std_out) {
		g_io_channel_unref (priv->std_out);
		priv->std_out = NULL;
	}

	if (priv->out_buffer) {
		g_string_free (priv->out_buffer, TRUE);
		priv->out_buffer = NULL;
	}

	if (priv->io_err) {
		g_source_remove (priv->io_err);
		priv->io_err = 0;
	}

	if (priv->std_error) {
		g_io_channel_unref (priv->std_error);
		priv->std_error = NULL;
	}

	if (priv->err_buffer) {
		g_string_free (priv->err_buffer, TRUE);
		priv->err_buffer = NULL;
	}

	if (priv->pid) {
		kill (priv->pid, SIGKILL);
		priv->pid = 0;
	}

	if (priv->argv) {
		g_strfreev ((gchar**) priv->argv->pdata);
		g_ptr_array_free (priv->argv, FALSE);
		priv->argv = NULL;
	}

	if (priv->error) {
		g_error_free (priv->error);
		priv->error = NULL;
	}

	if (priv->working_directory) {
		g_free (priv->working_directory);
		priv->working_directory = NULL;
	}

	G_OBJECT_CLASS (brasero_process_parent_class)->finalize (object);
}

static void
brasero_process_class_init (BraseroProcessClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroJobClass *job_class = BRASERO_JOB_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroProcessPrivate));

	object_class->finalize = brasero_process_finalize;

	job_class->start = brasero_process_start;
	job_class->stop = brasero_process_stop;

	klass->post = brasero_job_finished_track;
}

static void
brasero_process_init (BraseroProcess *obj)
{ }
