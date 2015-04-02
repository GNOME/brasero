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

#ifndef JOB_H
#define JOB_H

#include <glib.h>
#include <glib-object.h>

#include "brasero-track.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_JOB         (brasero_job_get_type ())
#define BRASERO_JOB(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_JOB, BraseroJob))
#define BRASERO_JOB_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_JOB, BraseroJobClass))
#define BRASERO_IS_JOB(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_JOB))
#define BRASERO_IS_JOB_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_JOB))
#define BRASERO_JOB_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_JOB, BraseroJobClass))

typedef enum {
	BRASERO_JOB_ACTION_NONE		= 0,
	BRASERO_JOB_ACTION_SIZE,
	BRASERO_JOB_ACTION_IMAGE,
	BRASERO_JOB_ACTION_RECORD,
	BRASERO_JOB_ACTION_ERASE,
	BRASERO_JOB_ACTION_CHECKSUM
} BraseroJobAction;

typedef struct {
	GObject parent;
} BraseroJob;

typedef struct {
	GObjectClass parent_class;

	/**
	 * Virtual functions to implement in each object deriving from
	 * BraseroJob.
	 */

	/**
	 * This function is not compulsory. If not implemented then the library
	 * will act as if OK had been returned.
	 * returns 	BRASERO_BURN_OK on successful initialization
	 *		The job can return BRASERO_BURN_NOT_RUNNING if it should
	 *		not be started.
	 * 		BRASERO_BURN_ERR otherwise
	 */
	BraseroBurnResult	(*activate)		(BraseroJob *job,
							 GError **error);

	/**
	 * This function is compulsory.
	 * returns 	BRASERO_BURN_OK if a loop should be run afterward
	 *		The job can return BRASERO_BURN_NOT_RUNNING if it already
	 *		completed successfully the task or don't need to be run. In this
	 *		case, it's the whole task that will be skipped.
	 *		NOT_SUPPORTED if it can't do the action required. When running
	 *		in fake mode (to get size mostly), the job will be "forgiven" and
	 *		deactivated.
	 *		RETRY if the job is not able to start at the moment but should
	 *		be given another chance later.
	 * 		ERR otherwise
	 */
	BraseroBurnResult	(*start)		(BraseroJob *job,
							 GError **error);

	BraseroBurnResult	(*clock_tick)		(BraseroJob *job);

	BraseroBurnResult	(*stop)			(BraseroJob *job,
							 GError **error);

	/**
	 * you should not connect to this signal. It's used internally to 
	 * "autoconfigure" the backend when an error occurs
	 */
	BraseroBurnResult	(*error)		(BraseroJob *job,
							 BraseroBurnError error);
} BraseroJobClass;

GType brasero_job_get_type (void);

/**
 * These functions are to be used to get information for running jobs.
 * They are only available when a job is running.
 */

BraseroBurnResult
brasero_job_set_nonblocking (BraseroJob *self,
			     GError **error);

BraseroBurnResult
brasero_job_get_action (BraseroJob *job, BraseroJobAction *action);

BraseroBurnResult
brasero_job_get_flags (BraseroJob *job, BraseroBurnFlag *flags);

BraseroBurnResult
brasero_job_get_fd_in (BraseroJob *job, int *fd_in);

BraseroBurnResult
brasero_job_get_tracks (BraseroJob *job, GSList **tracks);

BraseroBurnResult
brasero_job_get_done_tracks (BraseroJob *job, GSList **tracks);

BraseroBurnResult
brasero_job_get_current_track (BraseroJob *job, BraseroTrack **track);

BraseroBurnResult
brasero_job_get_input_type (BraseroJob *job, BraseroTrackType *type);

BraseroBurnResult
brasero_job_get_audio_title (BraseroJob *job, gchar **album);

BraseroBurnResult
brasero_job_get_data_label (BraseroJob *job, gchar **label);

BraseroBurnResult
brasero_job_get_session_output_size (BraseroJob *job,
				     goffset *blocks,
				     goffset *bytes);

/**
 * Used to get information of the destination media
 */

BraseroBurnResult
brasero_job_get_medium (BraseroJob *job, BraseroMedium **medium);

BraseroBurnResult
brasero_job_get_bus_target_lun (BraseroJob *job, gchar **BTL);

BraseroBurnResult
brasero_job_get_device (BraseroJob *job, gchar **device);

BraseroBurnResult
brasero_job_get_media (BraseroJob *job, BraseroMedia *media);

BraseroBurnResult
brasero_job_get_last_session_address (BraseroJob *job, goffset *address);

BraseroBurnResult
brasero_job_get_next_writable_address (BraseroJob *job, goffset *address);

BraseroBurnResult
brasero_job_get_rate (BraseroJob *job, guint64 *rate);

BraseroBurnResult
brasero_job_get_speed (BraseroJob *self, guint *speed);

BraseroBurnResult
brasero_job_get_max_rate (BraseroJob *job, guint64 *rate);

BraseroBurnResult
brasero_job_get_max_speed (BraseroJob *job, guint *speed);

/**
 * necessary for objects imaging either to another or to a file
 */

BraseroBurnResult
brasero_job_get_output_type (BraseroJob *job, BraseroTrackType *type);

BraseroBurnResult
brasero_job_get_fd_out (BraseroJob *job, int *fd_out);

BraseroBurnResult
brasero_job_get_image_output (BraseroJob *job,
			      gchar **image,
			      gchar **toc);
BraseroBurnResult
brasero_job_get_audio_output (BraseroJob *job,
			      gchar **output);

/**
 * get a temporary file that will be deleted once the session is finished
 */
 
BraseroBurnResult
brasero_job_get_tmp_file (BraseroJob *job,
			  const gchar *suffix,
			  gchar **output,
			  GError **error);

BraseroBurnResult
brasero_job_get_tmp_dir (BraseroJob *job,
			 gchar **path,
			 GError **error);

/**
 * Each tag can be retrieved by any job
 */

BraseroBurnResult
brasero_job_tag_lookup (BraseroJob *job,
			const gchar *tag,
			GValue **value);

BraseroBurnResult
brasero_job_tag_add (BraseroJob *job,
		     const gchar *tag,
		     GValue *value);

/**
 * Used to give job results and tell when a job has finished
 */

BraseroBurnResult
brasero_job_add_track (BraseroJob *job,
		       BraseroTrack *track);

BraseroBurnResult
brasero_job_finished_track (BraseroJob *job);

BraseroBurnResult
brasero_job_finished_session (BraseroJob *job);

BraseroBurnResult
brasero_job_error (BraseroJob *job,
		   GError *error);

/**
 * Used to start progress reporting and starts an internal timer to keep track
 * of remaining time among other things
 */

BraseroBurnResult
brasero_job_start_progress (BraseroJob *job,
			    gboolean force);

/**
 * task progress report: you can use only some of these functions
 */

BraseroBurnResult
brasero_job_set_rate (BraseroJob *job,
		      gint64 rate);
BraseroBurnResult
brasero_job_set_written_track (BraseroJob *job,
			       goffset written);
BraseroBurnResult
brasero_job_set_written_session (BraseroJob *job,
				 goffset written);
BraseroBurnResult
brasero_job_set_progress (BraseroJob *job,
			  gdouble progress);
BraseroBurnResult
brasero_job_reset_progress (BraseroJob *job);
BraseroBurnResult
brasero_job_set_current_action (BraseroJob *job,
				BraseroBurnAction action,
				const gchar *string,
				gboolean force);
BraseroBurnResult
brasero_job_get_current_action (BraseroJob *job,
				BraseroBurnAction *action);
BraseroBurnResult
brasero_job_set_output_size_for_current_track (BraseroJob *job,
					       goffset sectors,
					       goffset bytes);

/**
 * Used to tell it's (or not) dangerous to interrupt this job
 */

void
brasero_job_set_dangerous (BraseroJob *job, gboolean value);

/**
 * This is for apps with a jerky current rate (like cdrdao)
 */

BraseroBurnResult
brasero_job_set_use_average_rate (BraseroJob *job,
				  gboolean value);

/**
 * logging facilities
 */

void
brasero_job_log_message (BraseroJob *job,
			 const gchar *location,
			 const gchar *format,
			 ...) G_GNUC_PRINTF (3, 4);

#define BRASERO_JOB_LOG(job, message, ...) 			\
{								\
	brasero_job_log_message (BRASERO_JOB (job),		\
				 G_STRLOC,			\
				 "%s " message,			\
				 G_OBJECT_TYPE_NAME (job),	\
				 ##__VA_ARGS__);		\
}
#define BRASERO_JOB_LOG_ARG(job, message, ...)			\
{								\
	brasero_job_log_message (BRASERO_JOB (job),		\
				 G_STRLOC,			\
				 "\t" message,			\
				 ##__VA_ARGS__);		\
}

#define BRASERO_JOB_NOT_SUPPORTED(job) 					\
	{								\
		BRASERO_JOB_LOG (job, "unsupported operation");		\
		return BRASERO_BURN_NOT_SUPPORTED;			\
	}

#define BRASERO_JOB_NOT_READY(job)						\
	{									\
		BRASERO_JOB_LOG (job, "not ready to operate");	\
		return BRASERO_BURN_NOT_READY;					\
	}


G_END_DECLS

#endif /* JOB_H */
