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

#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include <gio/gio.h>

#include "brasero-drive.h"
#include "brasero-medium.h"

#include "burn-basics.h"
#include "burn-debug.h"
#include "brasero-session.h"
#include "brasero-session-helper.h"
#include "brasero-plugin-information.h"
#include "burn-job.h"
#include "burn-task-ctx.h"
#include "burn-task-item.h"
#include "libbrasero-marshal.h"

#include "brasero-track-type-private.h"

typedef struct _BraseroJobOutput {
	gchar *image;
	gchar *toc;
} BraseroJobOutput;

typedef struct _BraseroJobInput {
	int out;
	int in;
} BraseroJobInput;

static void brasero_job_iface_init_task_item (BraseroTaskItemIFace *iface);
G_DEFINE_TYPE_WITH_CODE (BraseroJob, brasero_job, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (BRASERO_TYPE_TASK_ITEM,
						brasero_job_iface_init_task_item));

typedef struct BraseroJobPrivate BraseroJobPrivate;
struct BraseroJobPrivate {
	BraseroJob *next;
	BraseroJob *previous;

	BraseroTaskCtx *ctx;

	/* used if job reads data from a pipe */
	BraseroJobInput *input;

	/* output type (sets at construct time) */
	BraseroTrackType type;

	/* used if job writes data to a pipe (link is then NULL) */
	BraseroJobOutput *output;
	BraseroJob *linked;
};

#define BRASERO_JOB_DEBUG(job_MACRO)						\
{										\
	const gchar *class_name_MACRO = NULL;					\
	if (BRASERO_IS_JOB (job_MACRO))						\
		class_name_MACRO = G_OBJECT_TYPE_NAME (job_MACRO);		\
	brasero_job_log_message (job_MACRO, G_STRLOC,				\
				 "%s called %s", 				\
				 class_name_MACRO,				\
				 G_STRFUNC);					\
}

#define BRASERO_JOB_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_JOB, BraseroJobPrivate))

enum {
	PROP_NONE,
	PROP_OUTPUT,
};

typedef enum {
	ERROR_SIGNAL,
	LAST_SIGNAL
} BraseroJobSignalType;

static guint brasero_job_signals [LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;


/**
 * Task item virtual functions implementation
 */

static BraseroTaskItem *
brasero_job_item_next (BraseroTaskItem *item)
{
	BraseroJob *self;
	BraseroJobPrivate *priv;

	self = BRASERO_JOB (item);
	priv = BRASERO_JOB_PRIVATE (self);

	if (!priv->next)
		return NULL;

	return BRASERO_TASK_ITEM (priv->next);
}

static BraseroTaskItem *
brasero_job_item_previous (BraseroTaskItem *item)
{
	BraseroJob *self;
	BraseroJobPrivate *priv;

	self = BRASERO_JOB (item);
	priv = BRASERO_JOB_PRIVATE (self);

	if (!priv->previous)
		return NULL;

	return BRASERO_TASK_ITEM (priv->previous);
}

static BraseroBurnResult
brasero_job_item_link (BraseroTaskItem *input,
		       BraseroTaskItem *output)
{
	BraseroJobPrivate *priv_input;
	BraseroJobPrivate *priv_output;

	priv_input = BRASERO_JOB_PRIVATE (input);
	priv_output = BRASERO_JOB_PRIVATE (output);

	priv_input->next = BRASERO_JOB (output);
	priv_output->previous = BRASERO_JOB (input);

	g_object_ref (input);
	return BRASERO_BURN_OK;
}

static gboolean
brasero_job_is_last_active (BraseroJob *self)
{
	BraseroJobPrivate *priv;
	BraseroJob *next;

	priv = BRASERO_JOB_PRIVATE (self);
	if (!priv->ctx)
		return FALSE;

	next = priv->next;
	while (next) {
		priv = BRASERO_JOB_PRIVATE (next);
		if (priv->ctx)
			return FALSE;
		next = priv->next;
	}

	return TRUE;
}

static gboolean
brasero_job_is_first_active (BraseroJob *self)
{
	BraseroJobPrivate *priv;
	BraseroJob *prev;

	priv = BRASERO_JOB_PRIVATE (self);
	if (!priv->ctx)
		return FALSE;

	prev = priv->previous;
	while (prev) {
		priv = BRASERO_JOB_PRIVATE (prev);
		if (priv->ctx)
			return FALSE;
		prev = priv->previous;
	}

	return TRUE;
}

static void
brasero_job_input_free (BraseroJobInput *input)
{
	if (!input)
		return;

	if (input->in > 0)
		close (input->in);

	if (input->out > 0)
		close (input->out);

	g_free (input);
}

static void
brasero_job_output_free (BraseroJobOutput *output)
{
	if (!output)
		return;

	if (output->image) {
		g_free (output->image);
		output->image = NULL;
	}

	if (output->toc) {
		g_free (output->toc);
		output->toc = NULL;
	}

	g_free (output);
}

static void
brasero_job_deactivate (BraseroJob *self)
{
	BraseroJobPrivate *priv;

	priv = BRASERO_JOB_PRIVATE (self);

	BRASERO_JOB_LOG (self, "deactivating");

	/* ::start hasn't been called yet */
	if (priv->ctx) {
		g_object_unref (priv->ctx);
		priv->ctx = NULL;
	}

	if (priv->input) {
		brasero_job_input_free (priv->input);
		priv->input = NULL;
	}

	if (priv->output) {
		brasero_job_output_free (priv->output);
		priv->output = NULL;
	}

	if (priv->linked)
		priv->linked = NULL;
}

static BraseroBurnResult
brasero_job_allow_deactivation (BraseroJob *self,
				BraseroBurnSession *session,
				GError **error)
{
	BraseroJobPrivate *priv;
	BraseroTrackType input;

	priv = BRASERO_JOB_PRIVATE (self);

	/* This job refused to work. This is allowed in three cases:
	 * - the job is the only one in the task (no other job linked) and the
	 *   track type as input is the same as the track type of the output
	 *   except if type is DISC as input or output
	 * - if the job hasn't got any job linked before the next linked job
	 *   accepts the track type of the session as input
	 * - the output of the previous job is the same as this job output type
	 */

	/* there can't be two recorders in a row so ... */
	if (priv->type.type == BRASERO_TRACK_TYPE_DISC)
		goto error;

	if (priv->previous) {
		BraseroJobPrivate *previous;
		previous = BRASERO_JOB_PRIVATE (priv->previous);
		memcpy (&input, &previous->type, sizeof (BraseroTrackType));
	}
	else
		brasero_burn_session_get_input_type (session, &input);

	if (!brasero_track_type_equal (&input, &priv->type))
		goto error;

	return BRASERO_BURN_NOT_RUNNING;

error:

	g_set_error (error,
		     BRASERO_BURN_ERR,
		     BRASERO_BURN_ERROR_PLUGIN_MISBEHAVIOR,
		     /* Translators: %s is the plugin name */
		     _("\"%s\" did not behave properly"),
		     G_OBJECT_TYPE_NAME (self));
	return BRASERO_BURN_ERR;
}

static BraseroBurnResult
brasero_job_item_activate (BraseroTaskItem *item,
			   BraseroTaskCtx *ctx,
			   GError **error)
{
	BraseroJob *self;
	BraseroJobClass *klass;
	BraseroJobPrivate *priv;
	BraseroBurnSession *session;
	BraseroBurnResult result = BRASERO_BURN_OK;

	self = BRASERO_JOB (item);
	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (ctx);

	g_object_ref (ctx);
	priv->ctx = ctx;

	klass = BRASERO_JOB_GET_CLASS (self);

	/* see if this job needs to be deactivated (if no function then OK) */
	if (klass->activate)
		result = klass->activate (self, error);
	else
		BRASERO_BURN_LOG ("no ::activate method %s",
				  G_OBJECT_TYPE_NAME (item));

	if (result != BRASERO_BURN_OK) {
		g_object_unref (ctx);
		priv->ctx = NULL;

		if (result == BRASERO_BURN_NOT_RUNNING)
			result = brasero_job_allow_deactivation (self, session, error);

		return result;
	}

	return BRASERO_BURN_OK;
}

static gboolean
brasero_job_item_is_active (BraseroTaskItem *item)
{
	BraseroJob *self;
	BraseroJobPrivate *priv;

	self = BRASERO_JOB (item);
	priv = BRASERO_JOB_PRIVATE (self);

	return (priv->ctx != NULL);
}

static BraseroBurnResult
brasero_job_check_output_disc_space (BraseroJob *self,
				     GError **error)
{
	BraseroBurnSession *session;
	goffset output_blocks = 0;
	goffset media_blocks = 0;
	BraseroJobPrivate *priv;
	BraseroBurnFlag flags;
	BraseroMedium *medium;
	BraseroDrive *drive;

	priv = BRASERO_JOB_PRIVATE (self);

	brasero_task_ctx_get_session_output_size (priv->ctx,
						  &output_blocks,
						  NULL);

	session = brasero_task_ctx_get_session (priv->ctx);
	drive = brasero_burn_session_get_burner (session);
	medium = brasero_drive_get_medium (drive);
	flags = brasero_burn_session_get_flags (session);

	/* FIXME: if we can't recover the size of the medium 
	 * what should we do ? do as if we could ? */

	/* see if we are appending or not */
	if (flags & (BRASERO_BURN_FLAG_APPEND|BRASERO_BURN_FLAG_MERGE))
		brasero_medium_get_free_space (medium, NULL, &media_blocks);
	else
		brasero_medium_get_capacity (medium, NULL, &media_blocks);

	/* This is not really an error, we'll probably ask the 
	 * user to load a new disc */
	if (output_blocks > media_blocks) {
		gchar *media_blocks_str;
		gchar *output_blocks_str;

		BRASERO_BURN_LOG ("Insufficient space on disc %"G_GINT64_FORMAT"/%"G_GINT64_FORMAT,
				  media_blocks,
				  output_blocks);

		media_blocks_str = g_strdup_printf ("%"G_GINT64_FORMAT, media_blocks);
		output_blocks_str = g_strdup_printf ("%"G_GINT64_FORMAT, output_blocks);

		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_MEDIUM_SPACE,
			     /* Translators: the first %s is the size of the free space on the medium
			      * and the second %s is the size of the space required by the data to be
			      * burnt. */
			     _("Not enough space available on the disc (%s available for %s)"),
			     media_blocks_str,
			     output_blocks_str);

		g_free (media_blocks_str);
		g_free (output_blocks_str);
		return BRASERO_BURN_NEED_RELOAD;
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_job_check_output_volume_space (BraseroJob *self,
				       GError **error)
{
	GFileInfo *info;
	gchar *directory;
	GFile *file = NULL;
	struct rlimit limit;
	guint64 vol_size = 0;
	gint64 output_size = 0;
	BraseroJobPrivate *priv;
	const gchar *filesystem;

	/* now that the job has a known output we must check that the volume the
	 * job is writing to has enough space for all output */

	priv = BRASERO_JOB_PRIVATE (self);

	/* Get the size and filesystem type for the volume first.
	 * NOTE: since any plugin must output anything LOCALLY, we can then use
	 * all libc API. */
	if (!priv->output)
		return BRASERO_BURN_ERR;

	directory = g_path_get_dirname (priv->output->image);
	file = g_file_new_for_path (directory);
	g_free (directory);

	if (file == NULL)
		goto error;

	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE,
				  G_FILE_QUERY_INFO_NONE,
				  NULL,
				  error);
	if (!info)
		goto error;

	/* Check permissions first */
	if (!g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE)) {
		BRASERO_JOB_LOG (self, "No permissions");

		g_object_unref (info);
		g_object_unref (file);
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_PERMISSION,
			     _("You do not have the required permission to write at this location"));
		return BRASERO_BURN_ERR;
	}
	g_object_unref (info);

	/* Now size left */
	info = g_file_query_filesystem_info (file,
					     G_FILE_ATTRIBUTE_FILESYSTEM_FREE ","
					     G_FILE_ATTRIBUTE_FILESYSTEM_TYPE,
					     NULL,
					     error);
	if (!info)
		goto error;

	g_object_unref (file);
	file = NULL;

	/* Now check the filesystem type: the problem here is that some
	 * filesystems have a maximum file size limit of 4 GiB and more than
	 * often we need a temporary file size of 4 GiB or more. */
	filesystem = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_FILESYSTEM_TYPE);
	BRASERO_BURN_LOG ("%s filesystem detected", filesystem);

	brasero_job_get_session_output_size (self, NULL, &output_size);

	if (output_size >= 2147483648ULL
	&&  filesystem
	&& !strcmp (filesystem, "msdos")) {
		g_object_unref (info);
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_DISK_SPACE,
			     _("The filesystem you chose to store the temporary image on cannot hold files with a size over 2 GiB"));
		return BRASERO_BURN_ERR;
	}

	vol_size = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE);
	g_object_unref (info);

	/* get the size of the output this job is supposed to create */
	BRASERO_BURN_LOG ("Volume size %" G_GUINT64_FORMAT ", output size %" G_GINT64_FORMAT, vol_size, output_size);

	/* it's fine here to check size in bytes */
	if (output_size > vol_size) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_DISK_SPACE,
			     _("The location you chose to store the temporary image on does not have enough free space for the disc image (%ld MiB needed)"),
			     (unsigned long) output_size / 1048576);
		return BRASERO_BURN_ERR;
	}

	/* Last but not least, use getrlimit () to check that we are allowed to
	 * write a file of such length and that quotas won't get in our way */
	if (getrlimit (RLIMIT_FSIZE, &limit)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_DISK_SPACE,
			     "%s",
			     g_strerror (errno));
		return BRASERO_BURN_ERR;
	}

	if (limit.rlim_cur < output_size) {
		BRASERO_BURN_LOG ("User not allowed to write such a large file");

		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_DISK_SPACE,
			     _("The location you chose to store the temporary image on does not have enough free space for the disc image (%ld MiB needed)"),
			     (unsigned long) output_size / 1048576);
		return BRASERO_BURN_ERR;
	}

	return BRASERO_BURN_OK;

error:

	if (error && *error == NULL)
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("The size of the volume could not be retrieved"));

	if (file)
		g_object_unref (file);

	return BRASERO_BURN_ERR;
}

static BraseroBurnResult
brasero_job_set_output_file (BraseroJob *self,
			     GError **error)
{
	BraseroTrackType *session_output;
	BraseroBurnSession *session;
	BraseroBurnResult result;
	BraseroJobPrivate *priv;
	goffset output_size = 0;
	gchar *image = NULL;
	gchar *toc = NULL;
	gboolean is_last;

	priv = BRASERO_JOB_PRIVATE (self);

	/* no next job so we need a file pad */
	session = brasero_task_ctx_get_session (priv->ctx);

	/* If the plugin is not supposed to output anything, then don't test */
	brasero_job_get_session_output_size (BRASERO_JOB (self), NULL, &output_size);

	/* This should be re-enabled when we make sure all plugins (like vcd)
	 * don't advertize an output of 0 whereas it's not true. Maybe we could
	 * use -1 for plugins that don't output. */
	/* if (!output_size)
		return BRASERO_BURN_OK; */

	/* check if that's the last task */
	session_output = brasero_track_type_new ();
	brasero_burn_session_get_output_type (session, session_output);
	is_last = brasero_track_type_equal (session_output, &priv->type);
	brasero_track_type_free (session_output);

	if (priv->type.type == BRASERO_TRACK_TYPE_IMAGE) {
		if (is_last) {
			BraseroTrackType input = { 0, };

			result = brasero_burn_session_get_output (session,
								  &image,
								  &toc);

			/* check paths are set */
			if (!image
			|| (priv->type.subtype.img_format != BRASERO_IMAGE_FORMAT_BIN && !toc)) {
				g_set_error (error,
					     BRASERO_BURN_ERROR,
					     BRASERO_BURN_ERROR_OUTPUT_NONE,
					     _("No path was specified for the image output"));
				return BRASERO_BURN_ERR;
			}

			brasero_burn_session_get_input_type (session,
							     &input);

			/* if input is the same as output, then that's a
			 * processing task and there's no need to check if the
			 * output already exists since it will (but that's OK) */
			if (input.type == BRASERO_TRACK_TYPE_IMAGE
			&&  input.subtype.img_format == priv->type.subtype.img_format) {
				BRASERO_BURN_LOG ("Processing task, skipping check size");
				priv->output = g_new0 (BraseroJobOutput, 1);
				priv->output->image = image;
				priv->output->toc = toc;
				return BRASERO_BURN_OK;
			}
		}
		else {
			/* NOTE: no need to check for the existence here */
			result = brasero_burn_session_get_tmp_image (session,
								     priv->type.subtype.img_format,
								     &image,
								     &toc,
								     error);
			if (result != BRASERO_BURN_OK)
				return result;
		}

		BRASERO_JOB_LOG (self, "output set (IMAGE) image = %s toc = %s",
				 image,
				 toc ? toc : "none");
	}
	else if (priv->type.type == BRASERO_TRACK_TYPE_STREAM) {
		/* NOTE: this one can only a temporary file */
		result = brasero_burn_session_get_tmp_file (session,
							    ".cdr",
							    &image,
							    error);
		BRASERO_JOB_LOG (self, "Output set (AUDIO) image = %s", image);
	}
	else /* other types don't need an output */
		return BRASERO_BURN_OK;

	if (result != BRASERO_BURN_OK)
		return result;

	priv->output = g_new0 (BraseroJobOutput, 1);
	priv->output->image = image;
	priv->output->toc = toc;

	if (brasero_burn_session_get_flags (session) & BRASERO_BURN_FLAG_CHECK_SIZE)
		return brasero_job_check_output_volume_space (self, error);

	return result;
}

static BraseroJob *
brasero_job_get_next_active (BraseroJob *self)
{
	BraseroJobPrivate *priv;
	BraseroJob *next;

	/* since some jobs can return NOT_RUNNING after ::init, skip them */
	priv = BRASERO_JOB_PRIVATE (self);
	if (!priv->next)
		return NULL;

	next = priv->next;
	while (next) {
		priv = BRASERO_JOB_PRIVATE (next);

		if (priv->ctx)
			return next;

		next = priv->next;
	}

	return NULL;
}

static BraseroBurnResult
brasero_job_item_start (BraseroTaskItem *item,
		        GError **error)
{
	BraseroJob *self;
	BraseroJobClass *klass;
	BraseroJobAction action;
	BraseroJobPrivate *priv;
	BraseroBurnResult result;

	/* This function is compulsory */
	self = BRASERO_JOB (item);
	priv = BRASERO_JOB_PRIVATE (self);

	/* skip jobs that are not active */
	if (!priv->ctx)
		return BRASERO_BURN_OK;

	/* set the output if need be */
	brasero_job_get_action (self, &action);
	priv->linked = brasero_job_get_next_active (self);

	if (!priv->linked) {
		/* that's the last job so is action is image it needs a file */
		if (action == BRASERO_JOB_ACTION_IMAGE) {
			result = brasero_job_set_output_file (self, error);
			if (result != BRASERO_BURN_OK)
				return result;
		}
		else if (action == BRASERO_JOB_ACTION_RECORD) {
			BraseroBurnFlag flags;
			BraseroBurnSession *session;

			session = brasero_task_ctx_get_session (priv->ctx);
			flags = brasero_burn_session_get_flags (session);
			if (flags & BRASERO_BURN_FLAG_CHECK_SIZE
			&& !(flags & BRASERO_BURN_FLAG_OVERBURN)) {
				result = brasero_job_check_output_disc_space (self, error);
				if (result != BRASERO_BURN_OK)
					return result;
			}
		}
	}
	else
		BRASERO_JOB_LOG (self, "linked to %s", G_OBJECT_TYPE_NAME (priv->linked));

	if (!brasero_job_is_first_active (self)) {
		int fd [2];

		BRASERO_JOB_LOG (self, "creating input");

		/* setup a pipe */
		if (pipe (fd)) {
                        int errsv = errno;

			BRASERO_BURN_LOG ("A pipe couldn't be created");
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("An internal error occurred (%s)"),
				     g_strerror (errsv));

			return BRASERO_BURN_ERR;
		}

		/* NOTE: don't set O_NONBLOCK automatically as some plugins 
		 * don't like that (genisoimage, mkisofs) */
		priv->input = g_new0 (BraseroJobInput, 1);
		priv->input->in = fd [0];
		priv->input->out = fd [1];
	}

	klass = BRASERO_JOB_GET_CLASS (self);
	if (!klass->start) {
		BRASERO_JOB_LOG (self, "no ::start method");
		BRASERO_JOB_NOT_SUPPORTED (self);
	}

	result = klass->start (self, error);
	if (result == BRASERO_BURN_NOT_RUNNING) {
		/* this means that the task is already completed. This 
		 * must be returned by the last active job of the task
		 * (and usually the only one?) */

		if (priv->linked) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_PLUGIN_MISBEHAVIOR,
				     _("\"%s\" did not behave properly"),
				     G_OBJECT_TYPE_NAME (self));
			return BRASERO_BURN_ERR;
		}
	}

	if (result == BRASERO_BURN_NOT_SUPPORTED) {
		/* only forgive this error when that's the last job and we're
		 * searching for a job to set the current track size */
		if (action != BRASERO_JOB_ACTION_SIZE) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_PLUGIN_MISBEHAVIOR,
				     _("\"%s\" did not behave properly"),
				     G_OBJECT_TYPE_NAME (self));
			return BRASERO_BURN_ERR;
		}

		/* deactivate it */
		brasero_job_deactivate (self);
	}

	return result;
}

static BraseroBurnResult
brasero_job_item_clock_tick (BraseroTaskItem *item,
			     BraseroTaskCtx *ctx,
			     GError **error)
{
	BraseroJob *self;
	BraseroJobClass *klass;
	BraseroJobPrivate *priv;
	BraseroBurnResult result = BRASERO_BURN_OK;

	self = BRASERO_JOB (item);
	priv = BRASERO_JOB_PRIVATE (self);
	if (!priv->ctx)
		return BRASERO_BURN_OK;

	klass = BRASERO_JOB_GET_CLASS (self);
	if (klass->clock_tick)
		result = klass->clock_tick (self);

	return result;
}

static BraseroBurnResult
brasero_job_disconnect (BraseroJob *self,
			GError **error)
{
	BraseroJobPrivate *priv;
	BraseroBurnResult result = BRASERO_BURN_OK;

	priv = BRASERO_JOB_PRIVATE (self);

	/* NOTE: this function is only called when there are no more track to 
	 * process */

	if (priv->linked) {
		BraseroJobPrivate *priv_link;

		BRASERO_JOB_LOG (self,
				 "disconnecting %s from %s",
				 G_OBJECT_TYPE_NAME (self),
				 G_OBJECT_TYPE_NAME (priv->linked));

		priv_link = BRASERO_JOB_PRIVATE (priv->linked);

		/* only close the input to tell the other end that we're
		 * finished with writing to the pipe */
		if (priv_link->input->out > 0) {
			close (priv_link->input->out);
			priv_link->input->out = 0;
		}
	}
	else if (priv->output) {
		brasero_job_output_free (priv->output);
		priv->output = NULL;
	}

	if (priv->input) {
		BRASERO_JOB_LOG (self,
				 "closing connection for %s",
				 G_OBJECT_TYPE_NAME (self));

		brasero_job_input_free (priv->input);
		priv->input = NULL;
	}

	return result;
}

static BraseroBurnResult
brasero_job_item_stop (BraseroTaskItem *item,
		       BraseroTaskCtx *ctx,
		       GError **error)
{
	BraseroJob *self;
	BraseroJobClass *klass;
	BraseroJobPrivate *priv;
	BraseroBurnResult result = BRASERO_BURN_OK;
	
	self = BRASERO_JOB (item);
	priv = BRASERO_JOB_PRIVATE (self);

	if (!priv->ctx)
		return BRASERO_BURN_OK;

	BRASERO_JOB_LOG (self, "stopping");

	/* the order is important here */
	klass = BRASERO_JOB_GET_CLASS (self);
	if (klass->stop)
		result = klass->stop (self, error);

	brasero_job_disconnect (self, error);

	if (priv->ctx) {
		g_object_unref (priv->ctx);
		priv->ctx = NULL;
	}

	return result;
}

static void
brasero_job_iface_init_task_item (BraseroTaskItemIFace *iface)
{
	iface->next = brasero_job_item_next;
	iface->previous = brasero_job_item_previous;
	iface->link = brasero_job_item_link;
	iface->activate = brasero_job_item_activate;
	iface->is_active = brasero_job_item_is_active;
	iface->start = brasero_job_item_start;
	iface->stop = brasero_job_item_stop;
	iface->clock_tick = brasero_job_item_clock_tick;
}

BraseroBurnResult
brasero_job_tag_lookup (BraseroJob *self,
			const gchar *tag,
			GValue **value)
{
	BraseroJobPrivate *priv;
	BraseroBurnSession *session;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);

	session = brasero_task_ctx_get_session (priv->ctx);
	return brasero_burn_session_tag_lookup (session,
						tag,
						value);
}

BraseroBurnResult
brasero_job_tag_add (BraseroJob *self,
		     const gchar *tag,
		     GValue *value)
{
	BraseroJobPrivate *priv;
	BraseroBurnSession *session;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);

	if (!brasero_job_is_last_active (self))
		return BRASERO_BURN_ERR;

	session = brasero_task_ctx_get_session (priv->ctx);
	brasero_burn_session_tag_add (session,
				      tag,
				      value);

	return BRASERO_BURN_OK;
}

/**
 * Means a job successfully completed its task.
 * track can be NULL, depending on whether or not the job created a track.
 */

BraseroBurnResult
brasero_job_add_track (BraseroJob *self,
		       BraseroTrack *track)
{
	BraseroJobPrivate *priv;
	BraseroJobAction action;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);

	/* to add a track to the session, a job :
	 * - must be the last running in the chain
	 * - the action for the job must be IMAGE */

	action = BRASERO_JOB_ACTION_NONE;
	brasero_job_get_action (self, &action);
	if (action != BRASERO_JOB_ACTION_IMAGE)
		return BRASERO_BURN_ERR;

	if (!brasero_job_is_last_active (self))
		return BRASERO_BURN_ERR;

	return brasero_task_ctx_add_track (priv->ctx, track);
}

BraseroBurnResult
brasero_job_finished_session (BraseroJob *self)
{
	GError *error = NULL;
	BraseroJobClass *klass;
	BraseroJobPrivate *priv;
	BraseroBurnResult result;

	priv = BRASERO_JOB_PRIVATE (self);

	BRASERO_JOB_LOG (self, "Finished successfully session");

	if (brasero_job_is_last_active (self))
		return brasero_task_ctx_finished (priv->ctx);

	if (!brasero_job_is_first_active (self)) {
		/* This job is apparently a go between job.
		 * It should only call for a stop on an error. */
		BRASERO_JOB_LOG (self, "is not a leader");
		error = g_error_new (BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_PLUGIN_MISBEHAVIOR,
				     _("\"%s\" did not behave properly"),
				     G_OBJECT_TYPE_NAME (self));
		return brasero_task_ctx_error (priv->ctx,
					       BRASERO_BURN_ERR,
					       error);
	}

	/* call the stop method of the job since it's finished */ 
	klass = BRASERO_JOB_GET_CLASS (self);
	if (klass->stop) {
		result = klass->stop (self, &error);
		if (result != BRASERO_BURN_OK)
			return brasero_task_ctx_error (priv->ctx,
						       result,
						       error);
	}

	/* this job is finished but it's not the leader so
	 * the task is not finished. Close the pipe on
	 * one side to let the next job know that there
	 * isn't any more data to be expected */
	result = brasero_job_disconnect (self, &error);
	g_object_unref (priv->ctx);
	priv->ctx = NULL;

	if (result != BRASERO_BURN_OK)
		return brasero_task_ctx_error (priv->ctx,
					       result,
					       error);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_finished_track (BraseroJob *self)
{
	GError *error = NULL;
	BraseroJobPrivate *priv;
	BraseroBurnResult result;

	priv = BRASERO_JOB_PRIVATE (self);

	BRASERO_JOB_LOG (self, "Finished track successfully");

	/* we first check if it's the first job */
	if (brasero_job_is_first_active (self)) {
		BraseroJobClass *klass;

		/* call ::stop for the job since it's finished */ 
		klass = BRASERO_JOB_GET_CLASS (self);
		if (klass->stop) {
			result = klass->stop (self, &error);

			if (result != BRASERO_BURN_OK)
				return brasero_task_ctx_error (priv->ctx,
							       result,
							       error);
		}

		/* see if there is another track to process */
		result = brasero_task_ctx_next_track (priv->ctx);
		if (result == BRASERO_BURN_RETRY) {
			/* there is another track to process: don't close the
			 * input of the next connected job. Leave it active */
			return BRASERO_BURN_OK;
		}

		if (!brasero_job_is_last_active (self)) {
			/* this job is finished but it's not the leader so the
			 * task is not finished. Close the pipe on one side to
			 * let the next job know that there isn't any more data
			 * to be expected */
			result = brasero_job_disconnect (self, &error);

			brasero_job_deactivate (self);

			if (result != BRASERO_BURN_OK)
				return brasero_task_ctx_error (priv->ctx,
							       result,
							       error);

			return BRASERO_BURN_OK;
		}
	}
	else if (!brasero_job_is_last_active (self)) {
		/* This job is apparently a go between job. It should only call
		 * for a stop on an error. */
		BRASERO_JOB_LOG (self, "is not a leader");
		error = g_error_new (BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_PLUGIN_MISBEHAVIOR,
				     _("\"%s\" did not behave properly"),
				     G_OBJECT_TYPE_NAME (self));
		return brasero_task_ctx_error (priv->ctx, BRASERO_BURN_ERR, error);
	}

	return brasero_task_ctx_finished (priv->ctx);
}

/**
 * means a job didn't successfully completed its task
 */

BraseroBurnResult
brasero_job_error (BraseroJob *self, GError *error)
{
	GValue instance_and_params [2];
	BraseroJobPrivate *priv;
	GValue return_value;

	BRASERO_JOB_DEBUG (self);

	BRASERO_JOB_LOG (self, "finished with an error");

	priv = BRASERO_JOB_PRIVATE (self);

	instance_and_params [0].g_type = 0;
	g_value_init (instance_and_params, G_TYPE_FROM_INSTANCE (self));
	g_value_set_instance (instance_and_params, self);

	instance_and_params [1].g_type = 0;
	g_value_init (instance_and_params + 1, G_TYPE_INT);

	if (error)
		g_value_set_int (instance_and_params + 1, error->code);
	else
		g_value_set_int (instance_and_params + 1, BRASERO_BURN_ERROR_GENERAL);

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

	BRASERO_JOB_LOG (self,
			 "asked to stop because of an error\n"
			 "\terror\t\t= %i\n"
			 "\tmessage\t= \"%s\"",
			 error ? error->code:0,
			 error ? error->message:"no message");

	return brasero_task_ctx_error (priv->ctx, g_value_get_int (&return_value), error);
}

/**
 * Used to retrieve config for a job
 * If the parameter is missing for the next 4 functions
 * it allows to test if they could be used
 */

BraseroBurnResult
brasero_job_get_fd_in (BraseroJob *self, int *fd_in)
{
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);

	if (!priv->input)
		return BRASERO_BURN_ERR;

	if (!fd_in)
		return BRASERO_BURN_OK;

	*fd_in = priv->input->in;
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_job_set_nonblocking_fd (int fd, GError **error)
{
	long flags = 0;

	if (fcntl (fd, F_GETFL, &flags) != -1) {
		/* Unfortunately some plugin (mkisofs/genisofs don't like 
		 * O_NONBLOCK (which is a shame) so we don't set them
		 * automatically but still offer that possibility. */
		flags |= O_NONBLOCK;
		if (fcntl (fd, F_SETFL, flags) == -1) {
			BRASERO_BURN_LOG ("couldn't set non blocking mode");
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("An internal error occurred"));
			return BRASERO_BURN_ERR;
		}
	}
	else {
		BRASERO_BURN_LOG ("couldn't get pipe flags");
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("An internal error occurred"));
		return BRASERO_BURN_ERR;
	}

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_set_nonblocking (BraseroJob *self,
			     GError **error)
{
	BraseroBurnResult result;
	int fd;

	BRASERO_JOB_DEBUG (self);

	fd = -1;
	if (brasero_job_get_fd_in (self, &fd) == BRASERO_BURN_OK) {
		result = brasero_job_set_nonblocking_fd (fd, error);
		if (result != BRASERO_BURN_OK)
			return result;
	}

	fd = -1;
	if (brasero_job_get_fd_out (self, &fd) == BRASERO_BURN_OK) {
		result = brasero_job_set_nonblocking_fd (fd, error);
		if (result != BRASERO_BURN_OK)
			return result;
	}

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_current_track (BraseroJob *self,
			       BraseroTrack **track)
{
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);
	if (!track)
		return BRASERO_BURN_OK;

	return brasero_task_ctx_get_current_track (priv->ctx, track);
}

BraseroBurnResult
brasero_job_get_done_tracks (BraseroJob *self, GSList **tracks)
{
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	/* tracks already done are those that are in session */
	priv = BRASERO_JOB_PRIVATE (self);
	return brasero_task_ctx_get_stored_tracks (priv->ctx, tracks);
}

BraseroBurnResult
brasero_job_get_tracks (BraseroJob *self, GSList **tracks)
{
	BraseroJobPrivate *priv;
	BraseroBurnSession *session;

	BRASERO_JOB_DEBUG (self);

	g_return_val_if_fail (tracks != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (priv->ctx);
	*tracks = brasero_burn_session_get_tracks (session);
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_fd_out (BraseroJob *self, int *fd_out)
{
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);

	if (!priv->linked)
		return BRASERO_BURN_ERR;

	if (!fd_out)
		return BRASERO_BURN_OK;

	priv = BRASERO_JOB_PRIVATE (priv->linked);
	if (!priv->input)
		return BRASERO_BURN_ERR;

	*fd_out = priv->input->out;
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_image_output (BraseroJob *self,
			      gchar **image,
			      gchar **toc)
{
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);

	if (!priv->output)
		return BRASERO_BURN_ERR;

	if (image)
		*image = g_strdup (priv->output->image);

	if (toc)
		*toc = g_strdup (priv->output->toc);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_audio_output (BraseroJob *self,
			      gchar **path)
{
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);
	if (!priv->output)
		return BRASERO_BURN_ERR;

	if (path)
		*path = g_strdup (priv->output->image);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_flags (BraseroJob *self, BraseroBurnFlag *flags)
{
	BraseroBurnSession *session;
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	g_return_val_if_fail (flags != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (priv->ctx);
	*flags = brasero_burn_session_get_flags (session);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_input_type (BraseroJob *self, BraseroTrackType *type)
{
	BraseroBurnResult result = BRASERO_BURN_OK;
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);
	if (!priv->previous) {
		BraseroBurnSession *session;

		session = brasero_task_ctx_get_session (priv->ctx);
		result = brasero_burn_session_get_input_type (session, type);
	}
	else {
		BraseroJobPrivate *prev_priv;

		prev_priv = BRASERO_JOB_PRIVATE (priv->previous);
		memcpy (type, &prev_priv->type, sizeof (BraseroTrackType));
		result = BRASERO_BURN_OK;
	}

	return result;
}

BraseroBurnResult
brasero_job_get_output_type (BraseroJob *self, BraseroTrackType *type)
{
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);

	memcpy (type, &priv->type, sizeof (BraseroTrackType));
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_action (BraseroJob *self, BraseroJobAction *action)
{
	BraseroJobPrivate *priv;
	BraseroTaskAction task_action;

	BRASERO_JOB_DEBUG (self);

	g_return_val_if_fail (action != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_JOB_PRIVATE (self);

	if (!brasero_job_is_last_active (self)) {
		*action = BRASERO_JOB_ACTION_IMAGE;
		return BRASERO_BURN_OK;
	}

	task_action = brasero_task_ctx_get_action (priv->ctx);
	switch (task_action) {
	case BRASERO_TASK_ACTION_NONE:
		*action = BRASERO_JOB_ACTION_SIZE;
		break;

	case BRASERO_TASK_ACTION_ERASE:
		*action = BRASERO_JOB_ACTION_ERASE;
		break;

	case BRASERO_TASK_ACTION_NORMAL:
		if (priv->type.type == BRASERO_TRACK_TYPE_DISC)
			*action = BRASERO_JOB_ACTION_RECORD;
		else
			*action = BRASERO_JOB_ACTION_IMAGE;
		break;

	case BRASERO_TASK_ACTION_CHECKSUM:
		*action = BRASERO_JOB_ACTION_CHECKSUM;
		break;

	default:
		*action = BRASERO_JOB_ACTION_NONE;
		break;
	}

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_bus_target_lun (BraseroJob *self, gchar **BTL)
{
	BraseroBurnSession *session;
	BraseroJobPrivate *priv;
	BraseroDrive *drive;

	BRASERO_JOB_DEBUG (self);

	g_return_val_if_fail (BTL != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (priv->ctx);

	drive = brasero_burn_session_get_burner (session);
	*BTL = brasero_drive_get_bus_target_lun_string (drive);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_device (BraseroJob *self, gchar **device)
{
	BraseroBurnSession *session;
	BraseroJobPrivate *priv;
	BraseroDrive *drive;
	const gchar *path;

	BRASERO_JOB_DEBUG (self);

	g_return_val_if_fail (device != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (priv->ctx);

	drive = brasero_burn_session_get_burner (session);
	path = brasero_drive_get_device (drive);
	*device = g_strdup (path);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_media (BraseroJob *self, BraseroMedia *media)
{
	BraseroBurnSession *session;
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	g_return_val_if_fail (media != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (priv->ctx);
	*media = brasero_burn_session_get_dest_media (session);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_medium (BraseroJob *job, BraseroMedium **medium)
{
	BraseroBurnSession *session;
	BraseroJobPrivate *priv;
	BraseroDrive *drive;

	BRASERO_JOB_DEBUG (job);

	g_return_val_if_fail (medium != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_JOB_PRIVATE (job);
	session = brasero_task_ctx_get_session (priv->ctx);
	drive = brasero_burn_session_get_burner (session);
	*medium = brasero_drive_get_medium (drive);

	if (!(*medium))
		return BRASERO_BURN_ERR;

	g_object_ref (*medium);
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_last_session_address (BraseroJob *self, goffset *address)
{
	BraseroBurnSession *session;
	BraseroJobPrivate *priv;
	BraseroMedium *medium;
	BraseroDrive *drive;

	BRASERO_JOB_DEBUG (self);

	g_return_val_if_fail (address != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (priv->ctx);
	drive = brasero_burn_session_get_burner (session);
	medium = brasero_drive_get_medium (drive);
	if (brasero_medium_get_last_data_track_address (medium, NULL, address))
		return BRASERO_BURN_OK;

	return BRASERO_BURN_ERR;
}

BraseroBurnResult
brasero_job_get_next_writable_address (BraseroJob *self, goffset *address)
{
	BraseroBurnSession *session;
	BraseroJobPrivate *priv;
	BraseroMedium *medium;
	BraseroDrive *drive;

	BRASERO_JOB_DEBUG (self);

	g_return_val_if_fail (address != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (priv->ctx);
	drive = brasero_burn_session_get_burner (session);
	medium = brasero_drive_get_medium (drive);
	*address = brasero_medium_get_next_writable_address (medium);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_rate (BraseroJob *self, guint64 *rate)
{
	BraseroBurnSession *session;
	BraseroJobPrivate *priv;

	g_return_val_if_fail (rate != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (priv->ctx);
	*rate = brasero_burn_session_get_rate (session);

	return BRASERO_BURN_OK;
}

static int
_round_speed (float number)
{
	int retval = (gint) number;

	/* NOTE: number must always be positive */
	number -= (float) retval;
	if (number >= 0.5)
		return retval + 1;

	return retval;
}

BraseroBurnResult
brasero_job_get_speed (BraseroJob *self, guint *speed)
{
	BraseroBurnSession *session;
	BraseroJobPrivate *priv;
	BraseroMedia media;
	guint64 rate;

	BRASERO_JOB_DEBUG (self);

	g_return_val_if_fail (speed != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (priv->ctx);
	rate = brasero_burn_session_get_rate (session);

	media = brasero_burn_session_get_dest_media (session);
	if (media & BRASERO_MEDIUM_DVD)
		*speed = _round_speed (BRASERO_RATE_TO_SPEED_DVD (rate));
	else if (media & BRASERO_MEDIUM_BD)
		*speed = _round_speed (BRASERO_RATE_TO_SPEED_BD (rate));
	else
		*speed = _round_speed (BRASERO_RATE_TO_SPEED_CD (rate));

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_max_rate (BraseroJob *self, guint64 *rate)
{
	BraseroBurnSession *session;
	BraseroJobPrivate *priv;
	BraseroMedium *medium;
	BraseroDrive *drive;

	BRASERO_JOB_DEBUG (self);

	g_return_val_if_fail (rate != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (priv->ctx);

	drive = brasero_burn_session_get_burner (session);
	medium = brasero_drive_get_medium (drive);

	if (!medium)
		return BRASERO_BURN_NOT_READY;

	*rate = brasero_medium_get_max_write_speed (medium);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_max_speed (BraseroJob *self, guint *speed)
{
	BraseroBurnSession *session;
	BraseroJobPrivate *priv;
	BraseroMedium *medium;
	BraseroDrive *drive;
	BraseroMedia media;
	guint64 rate;

	BRASERO_JOB_DEBUG (self);

	g_return_val_if_fail (speed != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (priv->ctx);

	drive = brasero_burn_session_get_burner (session);
	medium = brasero_drive_get_medium (drive);
	if (!medium)
		return BRASERO_BURN_NOT_READY;

	rate = brasero_medium_get_max_write_speed (medium);
	media = brasero_medium_get_status (medium);
	if (media & BRASERO_MEDIUM_DVD)
		*speed = _round_speed (BRASERO_RATE_TO_SPEED_DVD (rate));
	else if (media & BRASERO_MEDIUM_BD)
		*speed = _round_speed (BRASERO_RATE_TO_SPEED_BD (rate));
	else
		*speed = _round_speed (BRASERO_RATE_TO_SPEED_CD (rate));

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_tmp_file (BraseroJob *self,
			  const gchar *suffix,
			  gchar **output,
			  GError **error)
{
	BraseroBurnSession *session;
	BraseroJobPrivate *priv;

	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (priv->ctx);
	return brasero_burn_session_get_tmp_file (session,
						  suffix,
						  output,
						  error);
}

BraseroBurnResult
brasero_job_get_tmp_dir (BraseroJob *self,
			 gchar **output,
			 GError **error)
{
	BraseroBurnSession *session;
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (priv->ctx);
	brasero_burn_session_get_tmp_dir (session,
					  output,
					  error);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_audio_title (BraseroJob *self, gchar **album)
{
	BraseroBurnSession *session;
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	g_return_val_if_fail (album != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (priv->ctx);

	*album = g_strdup (brasero_burn_session_get_label (session));
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_data_label (BraseroJob *self, gchar **label)
{
	BraseroBurnSession *session;
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	g_return_val_if_fail (label != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (priv->ctx);

	*label = g_strdup (brasero_burn_session_get_label (session));
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_job_get_session_output_size (BraseroJob *self,
				     goffset *blocks,
				     goffset *bytes)
{
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);
	return brasero_task_ctx_get_session_output_size (priv->ctx, blocks, bytes);
}

/**
 * Starts task internal timer 
 */

BraseroBurnResult
brasero_job_start_progress (BraseroJob *self,
			    gboolean force)
{
	BraseroJobPrivate *priv;

	/* Turn this off as otherwise it floods bug reports */
	// BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);
	if (priv->next)
		return BRASERO_BURN_NOT_RUNNING;

	return brasero_task_ctx_start_progress (priv->ctx, force);
}

BraseroBurnResult
brasero_job_reset_progress (BraseroJob *self)
{
	BraseroJobPrivate *priv;

	priv = BRASERO_JOB_PRIVATE (self);
	if (priv->next)
		return BRASERO_BURN_ERR;

	return brasero_task_ctx_reset_progress (priv->ctx);
}

/**
 * these should be used to set the different values of the task by the jobs
 */

BraseroBurnResult
brasero_job_set_progress (BraseroJob *self,
			  gdouble progress)
{
	BraseroJobPrivate *priv;

	/* Turn this off as it floods bug reports */
	//BRASERO_JOB_LOG (self, "Called brasero_job_set_progress (%lf)", progress);

	priv = BRASERO_JOB_PRIVATE (self);
	if (priv->next)
		return BRASERO_BURN_ERR;

	if (progress < 0.0 || progress > 1.0) {
		BRASERO_JOB_LOG (self, "Tried to set an insane progress value (%lf)", progress);
		return BRASERO_BURN_ERR;
	}

	return brasero_task_ctx_set_progress (priv->ctx, progress);
}

BraseroBurnResult
brasero_job_set_current_action (BraseroJob *self,
				BraseroBurnAction action,
				const gchar *string,
				gboolean force)
{
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);
	if (!brasero_job_is_last_active (self))
		return BRASERO_BURN_NOT_RUNNING;

	return brasero_task_ctx_set_current_action (priv->ctx,
						    action,
						    string,
						    force);
}

BraseroBurnResult
brasero_job_get_current_action (BraseroJob *self,
				BraseroBurnAction *action)
{
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	g_return_val_if_fail (action != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_JOB_PRIVATE (self);

	if (!priv->ctx) {
		BRASERO_JOB_LOG (self,
				 "called %s whereas it wasn't running",
				 G_STRFUNC);
		return BRASERO_BURN_NOT_RUNNING;
	}

	return brasero_task_ctx_get_current_action (priv->ctx, action);
}

BraseroBurnResult
brasero_job_set_rate (BraseroJob *self,
		      gint64 rate)
{
	BraseroJobPrivate *priv;

	/* Turn this off as otherwise it floods bug reports */
	// BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);
	if (priv->next)
		return BRASERO_BURN_NOT_RUNNING;

	return brasero_task_ctx_set_rate (priv->ctx, rate);
}

BraseroBurnResult
brasero_job_set_output_size_for_current_track (BraseroJob *self,
					       goffset sectors,
					       goffset bytes)
{
	BraseroJobPrivate *priv;

	/* this function can only be used by the last job which is not recording
	 * all other jobs trying to set this value will be ignored.
	 * It should be used mostly during a fake running. This value is stored
	 * by the task context as the amount of bytes/blocks produced by a task.
	 * That's why it's not possible to set the DATA type number of files.
	 * NOTE: the values passed on by this function to context may be added 
	 * to other when there are multiple tracks. */
	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);

	if (!brasero_job_is_last_active (self))
		return BRASERO_BURN_ERR;

	return brasero_task_ctx_set_output_size_for_current_track (priv->ctx,
								   sectors,
								   bytes);
}

BraseroBurnResult
brasero_job_set_written_track (BraseroJob *self,
			       goffset written)
{
	BraseroJobPrivate *priv;

	/* Turn this off as otherwise it floods bug reports */
	// BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);
	if (priv->next)
		return BRASERO_BURN_NOT_RUNNING;

	return brasero_task_ctx_set_written_track (priv->ctx, written);
}

BraseroBurnResult
brasero_job_set_written_session (BraseroJob *self,
				 goffset written)
{
	BraseroJobPrivate *priv;

	/* Turn this off as otherwise it floods bug reports */
	// BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);
	if (priv->next)
		return BRASERO_BURN_NOT_RUNNING;

	return brasero_task_ctx_set_written_session (priv->ctx, written);
}

BraseroBurnResult
brasero_job_set_use_average_rate (BraseroJob *self, gboolean value)
{
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);
	if (priv->next)
		return BRASERO_BURN_NOT_RUNNING;

	return brasero_task_ctx_set_use_average (priv->ctx, value);
}

void
brasero_job_set_dangerous (BraseroJob *self, gboolean value)
{
	BraseroJobPrivate *priv;

	BRASERO_JOB_DEBUG (self);

	priv = BRASERO_JOB_PRIVATE (self);
	if (priv->ctx)
		brasero_task_ctx_set_dangerous (priv->ctx, value);
}

/**
 * used for debugging
 */

void
brasero_job_log_message (BraseroJob *self,
			 const gchar *location,
			 const gchar *format,
			 ...)
{
	va_list arg_list;
	BraseroJobPrivate *priv;
	BraseroBurnSession *session;

	g_return_if_fail (BRASERO_IS_JOB (self));
	g_return_if_fail (format != NULL);

	priv = BRASERO_JOB_PRIVATE (self);
	session = brasero_task_ctx_get_session (priv->ctx);

	va_start (arg_list, format);
	brasero_burn_session_logv (session, format, arg_list);
	va_end (arg_list);

	va_start (arg_list, format);
	brasero_burn_debug_messagev (location, format, arg_list);
	va_end (arg_list);
}

/**
 * Object creation stuff
 */

static void
brasero_job_get_property (GObject *object,
			  guint prop_id,
			  GValue *value,
			  GParamSpec *pspec)
{
	BraseroJobPrivate *priv;
	BraseroTrackType *ptr;

	priv = BRASERO_JOB_PRIVATE (object);

	switch (prop_id) {
	case PROP_OUTPUT:
		ptr = g_value_get_pointer (value);
		memcpy (ptr, &priv->type, sizeof (BraseroTrackType));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
brasero_job_set_property (GObject *object,
			  guint prop_id,
			  const GValue *value,
			  GParamSpec *pspec)
{
	BraseroJobPrivate *priv;
	BraseroTrackType *ptr;

	priv = BRASERO_JOB_PRIVATE (object);

	switch (prop_id) {
	case PROP_OUTPUT:
		ptr = g_value_get_pointer (value);
		if (!ptr) {
			priv->type.type = BRASERO_TRACK_TYPE_NONE;
			priv->type.subtype.media = BRASERO_MEDIUM_NONE;
		}
		else
			memcpy (&priv->type, ptr, sizeof (BraseroTrackType));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
brasero_job_finalize (GObject *object)
{
	BraseroJobPrivate *priv;

	priv = BRASERO_JOB_PRIVATE (object);

	if (priv->ctx) {
		g_object_unref (priv->ctx);
		priv->ctx = NULL;
	}

	if (priv->previous) {
		g_object_unref (priv->previous);
		priv->previous = NULL;
	}

	if (priv->input) {
		brasero_job_input_free (priv->input);
		priv->input = NULL;
	}

	if (priv->linked)
		priv->linked = NULL;

	if (priv->output) {
		brasero_job_output_free (priv->output);
		priv->output = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_job_class_init (BraseroJobClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroJobPrivate));

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_job_finalize;
	object_class->set_property = brasero_job_set_property;
	object_class->get_property = brasero_job_get_property;

	brasero_job_signals [ERROR_SIGNAL] =
	    g_signal_new ("error",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST|G_SIGNAL_NO_RECURSE,
			  G_STRUCT_OFFSET (BraseroJobClass, error),
			  NULL, NULL,
			  brasero_marshal_INT__INT,
			  G_TYPE_INT, 1, G_TYPE_INT);

	g_object_class_install_property (object_class,
					 PROP_OUTPUT,
					 g_param_spec_pointer ("output",
							       "The type the job must output",
							       "The type the job must output",
							       G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY));
}

static void
brasero_job_init (BraseroJob *obj)
{ }
