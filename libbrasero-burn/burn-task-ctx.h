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

#ifndef _BURN_TASK_CTX_H_
#define _BURN_TASK_CTX_H_

#include <glib-object.h>

#include "burn-basics.h"
#include "brasero-session.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_TASK_CTX             (brasero_task_ctx_get_type ())
#define BRASERO_TASK_CTX(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_TASK_CTX, BraseroTaskCtx))
#define BRASERO_TASK_CTX_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_TASK_CTX, BraseroTaskCtxClass))
#define BRASERO_IS_TASK_CTX(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_TASK_CTX))
#define BRASERO_IS_TASK_CTX_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_TASK_CTX))
#define BRASERO_TASK_CTX_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_TASK_CTX, BraseroTaskCtxClass))

typedef enum {
	BRASERO_TASK_ACTION_NONE		= 0,
	BRASERO_TASK_ACTION_ERASE,
	BRASERO_TASK_ACTION_NORMAL,
	BRASERO_TASK_ACTION_CHECKSUM,
} BraseroTaskAction;

typedef struct _BraseroTaskCtxClass BraseroTaskCtxClass;
typedef struct _BraseroTaskCtx BraseroTaskCtx;

struct _BraseroTaskCtxClass
{
	GObjectClass parent_class;

	void			(* finished)		(BraseroTaskCtx *ctx,
							 BraseroBurnResult retval,
							 GError *error);

	/* signals */
	void			(*progress_changed)	(BraseroTaskCtx *task,
							 gdouble fraction,
							 glong remaining_time);
	void			(*action_changed)	(BraseroTaskCtx *task,
							 BraseroBurnAction action);
};

struct _BraseroTaskCtx
{
	GObject parent_instance;
};

GType brasero_task_ctx_get_type (void) G_GNUC_CONST;

void
brasero_task_ctx_reset (BraseroTaskCtx *ctx);

void
brasero_task_ctx_set_fake (BraseroTaskCtx *ctx,
			   gboolean fake);

void
brasero_task_ctx_set_dangerous (BraseroTaskCtx *ctx,
				gboolean value);

guint
brasero_task_ctx_get_dangerous (BraseroTaskCtx *ctx);

/**
 * Used to get the session it is associated with
 */

BraseroBurnSession *
brasero_task_ctx_get_session (BraseroTaskCtx *ctx);

BraseroTaskAction
brasero_task_ctx_get_action (BraseroTaskCtx *ctx);

BraseroBurnResult
brasero_task_ctx_get_stored_tracks (BraseroTaskCtx *ctx,
				    GSList **tracks);

BraseroBurnResult
brasero_task_ctx_get_current_track (BraseroTaskCtx *ctx,
				    BraseroTrack **track);

/**
 * Used to give job results and tell when a job has finished
 */

BraseroBurnResult
brasero_task_ctx_add_track (BraseroTaskCtx *ctx,
			    BraseroTrack *track);

BraseroBurnResult
brasero_task_ctx_next_track (BraseroTaskCtx *ctx);

BraseroBurnResult
brasero_task_ctx_finished (BraseroTaskCtx *ctx);

BraseroBurnResult
brasero_task_ctx_error (BraseroTaskCtx *ctx,
			BraseroBurnResult retval,
			GError *error);

/**
 * Used to start progress reporting and starts an internal timer to keep track
 * of remaining time among other things
 */

BraseroBurnResult
brasero_task_ctx_start_progress (BraseroTaskCtx *ctx,
				 gboolean force);

void
brasero_task_ctx_report_progress (BraseroTaskCtx *ctx);

void
brasero_task_ctx_stop_progress (BraseroTaskCtx *ctx);

/**
 * task progress report for jobs
 */

BraseroBurnResult
brasero_task_ctx_set_rate (BraseroTaskCtx *ctx,
			   gint64 rate);

BraseroBurnResult
brasero_task_ctx_set_written_session (BraseroTaskCtx *ctx,
				      gint64 written);
BraseroBurnResult
brasero_task_ctx_set_written_track (BraseroTaskCtx *ctx,
				    gint64 written);
BraseroBurnResult
brasero_task_ctx_reset_progress (BraseroTaskCtx *ctx);
BraseroBurnResult
brasero_task_ctx_set_progress (BraseroTaskCtx *ctx,
			       gdouble progress);
BraseroBurnResult
brasero_task_ctx_set_current_action (BraseroTaskCtx *ctx,
				     BraseroBurnAction action,
				     const gchar *string,
				     gboolean force);
BraseroBurnResult
brasero_task_ctx_set_use_average (BraseroTaskCtx *ctx,
				  gboolean use_average);
BraseroBurnResult
brasero_task_ctx_set_output_size_for_current_track (BraseroTaskCtx *ctx,
						    goffset sectors,
						    goffset bytes);

/**
 * task progress for library
 */

BraseroBurnResult
brasero_task_ctx_get_rate (BraseroTaskCtx *ctx,
			   guint64 *rate);
BraseroBurnResult
brasero_task_ctx_get_remaining_time (BraseroTaskCtx *ctx,
				     long *remaining);
BraseroBurnResult
brasero_task_ctx_get_session_output_size (BraseroTaskCtx *ctx,
					  goffset *blocks,
					  goffset *bytes);
BraseroBurnResult
brasero_task_ctx_get_written (BraseroTaskCtx *ctx,
			      goffset *written);
BraseroBurnResult
brasero_task_ctx_get_current_action_string (BraseroTaskCtx *ctx,
					    BraseroBurnAction action,
					    gchar **string);
BraseroBurnResult
brasero_task_ctx_get_progress (BraseroTaskCtx *ctx, 
			       gdouble *progress);
BraseroBurnResult
brasero_task_ctx_get_current_action (BraseroTaskCtx *ctx,
				     BraseroBurnAction *action);

G_END_DECLS

#endif /* _BURN_TASK_CTX_H_ */
