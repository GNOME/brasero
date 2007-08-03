/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2007 <bonfire-app@wanadoo.fr>
 * 
 * brasero is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * brasero is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with brasero.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include "burn-basics.h"
#include "burn-session.h"
#include "burn-debug.h"
#include "burn-task-ctx.h"

typedef struct _BraseroTaskCtxPrivate BraseroTaskCtxPrivate;
struct _BraseroTaskCtxPrivate
{
	/* these two are set at creation time and can't be changed */
	BraseroTaskAction action;
	BraseroBurnSession *session;

	GMutex *lock;

	BraseroTrack *current_track;
	GSList *tracks;

	/* used to poll for progress (every 0.5 sec) */
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
	/* FIXME: we need two types of actions */
	BraseroBurnAction current_action;
	gchar *action_string;

	guint dangerous;

	guint fake:1;
	guint added_track:1;
	guint action_changed:1;
	guint written_changed:1;
	guint progress_changed:1;
	guint use_average_rate:1;
};

#define BRASERO_TASK_CTX_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_TASK_CTX, BraseroTaskCtxPrivate))

G_DEFINE_TYPE (BraseroTaskCtx, brasero_task_ctx, G_TYPE_OBJECT);

#define MAX_VALUE_AVERAGE	16

enum _BraseroTaskCtxSignalType {
	ACTION_CHANGED_SIGNAL,
	PROGRESS_CHANGED_SIGNAL,
	LAST_SIGNAL
};
static guint brasero_task_ctx_signals [LAST_SIGNAL] = { 0 };

enum
{
	PROP_0,
	PROP_ACTION,
	PROP_SESSION
};

static GObjectClass* parent_class = NULL;

void
brasero_task_ctx_set_dangerous (BraseroTaskCtx *self, gboolean value)
{
	BraseroTaskCtxPrivate *priv;

	priv = BRASERO_TASK_CTX_PRIVATE (self);
	if (value)
		priv->dangerous ++;
	else
		priv->dangerous --;
}

guint
brasero_task_ctx_get_dangerous (BraseroTaskCtx *self)
{
	BraseroTaskCtxPrivate *priv;

	priv = BRASERO_TASK_CTX_PRIVATE (self);
	return priv->dangerous;
}

void
brasero_task_ctx_reset (BraseroTaskCtx *self)
{
	BraseroTaskCtxPrivate *priv;

	priv = BRASERO_TASK_CTX_PRIVATE (self);

	priv->tracks = brasero_burn_session_get_tracks (priv->session);
	BRASERO_BURN_LOG ("Setting current track (%i tracks)", g_slist_length (priv->tracks));
	if (priv->tracks) {
		if (priv->current_track)
			brasero_track_unref (priv->current_track);

		priv->current_track = priv->tracks->data;
		brasero_track_ref (priv->current_track);
	}
	else
		BRASERO_BURN_LOG ("no tracks");

	if (priv->timer) {
		g_timer_destroy (priv->timer);
		priv->timer = NULL;
	}

	priv->dangerous = 0;
	priv->progress = -1.0;
	priv->written = -1;
	priv->written_changed = 0;
	priv->current_written = 0;
	priv->current_elapsed = 0;
	priv->last_written = 0;
	priv->last_elapsed = 0;

	if (priv->times) {
		g_slist_free (priv->times);
		priv->times = NULL;
	}

	if (priv->tracks) {
		brasero_track_unref (priv->current_track);
		priv->current_track = priv->tracks->data;
		brasero_track_ref (priv->current_track);
	}
}

void
brasero_task_ctx_set_fake (BraseroTaskCtx *ctx,
			   gboolean fake)
{
	BraseroTaskCtxPrivate *priv;

	priv = BRASERO_TASK_CTX_PRIVATE (ctx);
	priv->fake = fake;
}

/**
 * Used to get config
 */

BraseroBurnSession *
brasero_task_ctx_get_session (BraseroTaskCtx *self)
{
	BraseroTaskCtxPrivate *priv;

	priv = BRASERO_TASK_CTX_PRIVATE (self);
	if (!priv->session)
		return NULL;

	return priv->session;
}

BraseroBurnResult
brasero_task_ctx_get_tracks (BraseroTaskCtx *self,
			     GSList **tracks)
{
	BraseroTaskCtxPrivate *priv;

	g_return_val_if_fail (tracks != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_TASK_CTX_PRIVATE (self);
	if (!priv->current_track)
		return BRASERO_BURN_ERR;

	*tracks = priv->tracks;
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_task_ctx_get_current_track (BraseroTaskCtx *self,
				    BraseroTrack **track)
{
	BraseroTaskCtxPrivate *priv;

	g_return_val_if_fail (track != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_TASK_CTX_PRIVATE (self);
	if (!priv->current_track)
		return BRASERO_BURN_ERR;

	*track = priv->current_track;
	return BRASERO_BURN_OK;
}

BraseroTaskAction
brasero_task_ctx_get_action (BraseroTaskCtx *self)
{
	BraseroTaskCtxPrivate *priv;

	priv = BRASERO_TASK_CTX_PRIVATE (self);

	if (priv->fake)
		return BRASERO_TASK_ACTION_NONE;

	return priv->action;
}

/**
 * Used to report task status
 */

static gboolean
brasero_task_ctx_set_next_track (BraseroTaskCtx *self)
{
	BraseroTaskCtxPrivate *priv;
	GSList *node;

	priv = BRASERO_TASK_CTX_PRIVATE (self);

	/* see if there is another track left */
	node = g_slist_find (priv->tracks, priv->current_track);
	if (!node || !node->next)
		return BRASERO_BURN_OK;

	if (priv->current_track)
		brasero_track_unref (priv->current_track);

	priv->current_track = node->next->data;
	brasero_track_ref (priv->current_track);

	return BRASERO_BURN_RETRY;
}

BraseroBurnResult
brasero_task_ctx_next_track (BraseroTaskCtx *self,
			     BraseroTrack *track)

{
	BraseroTaskCtxPrivate *priv;
	BraseroBurnResult retval;

	g_return_val_if_fail (BRASERO_IS_TASK_CTX (self), BRASERO_BURN_ERR);

	priv = BRASERO_TASK_CTX_PRIVATE (self);

	retval = brasero_task_ctx_set_next_track (self);
	if (retval == BRASERO_BURN_RETRY) {
		BraseroTaskCtxClass *klass;

		BRASERO_BURN_LOG ("Setting next track to be processed");

		if (!priv->added_track) {
			/* push the current tracks to add the new track */
			priv->added_track = 1;
			brasero_burn_session_push_tracks (priv->session);
		}
		brasero_burn_session_add_track (priv->session, track);

		klass = BRASERO_TASK_CTX_GET_CLASS (self);
		if (!klass->finished)
			return BRASERO_BURN_NOT_SUPPORTED;

		klass->finished (self,
				 BRASERO_BURN_RETRY,
				 NULL);
	}

	BRASERO_BURN_LOG ("No next track to process");
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_task_ctx_finished (BraseroTaskCtx *self,
			   BraseroTrack *track)
{
	BraseroTaskCtxPrivate *priv;
	BraseroTaskCtxClass *klass;
	BraseroBurnResult retval;
	GError *error = NULL;

	priv = BRASERO_TASK_CTX_PRIVATE (self);
	klass = BRASERO_TASK_CTX_GET_CLASS (self);
	if (!klass->finished)
		return BRASERO_BURN_NOT_SUPPORTED;

	retval = brasero_task_ctx_set_next_track (self);
	if (retval != BRASERO_BURN_OK) {
		/* there are some tracks left ! */
		retval = BRASERO_BURN_ERR;
		error = g_error_new (BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("there are some tracks left"));
	}

	if (track) {
		gint64 size;
		gint64 blocks;
		gint64 block_size;

		brasero_track_get_estimated_size (priv->current_track,
						  &block_size,
						  &blocks,
						  &size);

		brasero_track_set_estimated_size (track,
						  block_size,
						  blocks,
						  size);

		BRASERO_BURN_LOG ("Adding track (type = %i, size = %lli) %s",
				  brasero_track_get_type (track, NULL),
				  size,
				  priv->added_track? "already some tracks":"");

		if (!priv->added_track) {
			/* push the current tracks to add the new track */
			priv->added_track = 1;
			brasero_burn_session_push_tracks (priv->session);
		}

		brasero_burn_session_add_track (priv->session, track);
	}

	klass->finished (self,
			 retval,
			 error);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_task_ctx_error (BraseroTaskCtx *self,
			BraseroBurnResult retval,
			GError *error)
{
	BraseroTaskCtxClass *klass;
	BraseroTaskCtxPrivate *priv;

	priv = BRASERO_TASK_CTX_PRIVATE (self);
	klass = BRASERO_TASK_CTX_GET_CLASS (self);
	if (!klass->finished)
		return BRASERO_BURN_NOT_SUPPORTED;

	if (priv->added_track)
		brasero_burn_session_pop_tracks (priv->session);

	klass->finished (self,
			 retval,
			 error);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_task_ctx_start_progress (BraseroTaskCtx *self,
				 gboolean force)

{
	BraseroTaskCtxPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TASK_CTX (self), BRASERO_BURN_ERR);

	priv = BRASERO_TASK_CTX_PRIVATE (self);

	if (!priv->timer) {
		priv->timer = g_timer_new ();
		priv->first_written = priv->written;
	}
	else if (force) {
		g_timer_start (priv->timer);
		priv->first_written = priv->written;
	}

	return BRASERO_BURN_OK;
}

static gdouble
brasero_task_ctx_get_average (GSList **values, gdouble value)
{
	const unsigned int scale = 10000;
	unsigned int num = 0;
	gdouble average;
	gint32 int_value;
	GSList *l;

	if (value * scale < G_MAXINT)
		int_value = (gint32) ceil (scale * value);
	else if (value / scale < G_MAXINT)
		int_value = (gint32) ceil (-1.0 * value / scale);
	else
		return value;
		
	*values = g_slist_prepend (*values, GINT_TO_POINTER (int_value));

	average = 0;
	for (l = *values; l; l = l->next) {
		gdouble r = (gdouble) GPOINTER_TO_INT (l->data);

		if (r < 0)
			r *= scale * -1.0;
		else
			r /= scale;

		average += r;
		num++;
		if (num == MAX_VALUE_AVERAGE && l->next)
			l = g_slist_delete_link (l, l->next);
	}

	average /= num;
	return average;
}

void
brasero_task_ctx_report_progress (BraseroTaskCtx *self)
{
	BraseroTaskCtxPrivate *priv;
	gdouble progress, elapsed;

	priv = BRASERO_TASK_CTX_PRIVATE (self);

	if (priv->action_changed) {
		g_signal_emit (self,
			       brasero_task_ctx_signals [ACTION_CHANGED_SIGNAL],
			       0,
			       priv->current_action);
		priv->action_changed = 0;
	}

	if (priv->timer) {
		elapsed = g_timer_elapsed (priv->timer, NULL);
		if (brasero_task_ctx_get_progress (self, &progress) == BRASERO_BURN_OK) {
			gdouble total_time;

			total_time = (gdouble) elapsed / (gdouble) progress;

			g_mutex_lock (priv->lock);
			priv->total_time = brasero_task_ctx_get_average (&priv->times,
									 total_time);
			g_mutex_unlock (priv->lock);
		}
	}

	if (priv->progress_changed) {
		priv->progress_changed = 0;
		g_signal_emit (self,
			       brasero_task_ctx_signals [PROGRESS_CHANGED_SIGNAL],
			       0);
	}
	else if (priv->written_changed) {
		priv->written_changed = 0;
		g_signal_emit (self,
			       brasero_task_ctx_signals [PROGRESS_CHANGED_SIGNAL],
			       0);
	}
}

BraseroBurnResult
brasero_task_ctx_set_rate (BraseroTaskCtx *self,
			   gint64 rate)
{
	BraseroTaskCtxPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TASK_CTX (self), BRASERO_BURN_ERR);

	priv = BRASERO_TASK_CTX_PRIVATE (self);
	priv->rate = rate;
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_task_ctx_set_track_size (BraseroTaskCtx *self,
				 guint64 block_size,
				 guint64 sectors,
				 gint64 size)
{
	BraseroTaskCtxPrivate *priv;
	GSList *iter;

	g_return_val_if_fail (BRASERO_IS_TASK_CTX (self), BRASERO_BURN_ERR);

	priv = BRASERO_TASK_CTX_PRIVATE (self);

	brasero_track_set_estimated_size (priv->current_track,
					  block_size,
					  sectors,
					  size);
	priv->total = -1;

	for (iter = priv->tracks; iter; iter = iter->next) {
		BraseroTrack *track;
		gint64 blocks;

		track = iter->data;
		brasero_track_get_estimated_size (track,
						  NULL,
						  &blocks,
						  NULL);
		priv->total += blocks;		
	}

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_task_ctx_set_written (BraseroTaskCtx *self,
			      gint64 written)
{
	BraseroTaskCtxPrivate *priv;
	gdouble elapsed = 0.0;

	g_return_val_if_fail (BRASERO_IS_TASK_CTX (self), BRASERO_BURN_ERR);

	priv = BRASERO_TASK_CTX_PRIVATE (self);

	priv->written = written;
	priv->written_changed = 1;

	if (priv->use_average_rate)
		return BRASERO_BURN_OK;

	if (priv->timer)
		elapsed = g_timer_elapsed (priv->timer, NULL);

	if ((elapsed - priv->last_elapsed) > 0.5) {
		priv->last_written = priv->current_written;
		priv->last_elapsed = priv->current_elapsed;
		priv->current_written = written;
		priv->current_elapsed = elapsed;
	}

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_task_ctx_set_progress (BraseroTaskCtx *self,
			       gdouble progress)
{
	BraseroTaskCtxPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TASK_CTX (self), BRASERO_BURN_ERR);

	priv = BRASERO_TASK_CTX_PRIVATE (self);
	priv->progress_changed = 1;
	priv->progress = progress;

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_task_ctx_set_current_action (BraseroTaskCtx *self,
				     BraseroBurnAction action,
				     const gchar *string,
				     gboolean force)
{
	BraseroTaskCtxPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TASK_CTX (self), BRASERO_BURN_ERR);

	priv = BRASERO_TASK_CTX_PRIVATE (self);

	if (!force && priv->current_action == action)
		return BRASERO_BURN_OK;

	g_mutex_lock (priv->lock);

	priv->current_action = action;
	priv->action_changed = 1;

	if (priv->action_string)
		g_free (priv->action_string);

	priv->action_string = string ? g_strdup (string): NULL;

	if (!force) {
		g_slist_free (priv->times);
		priv->times = NULL;
	}

	g_mutex_unlock (priv->lock);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_task_ctx_set_use_average (BraseroTaskCtx *self,
				  gboolean use_average)
{
	BraseroTaskCtxPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TASK_CTX (self), BRASERO_BURN_ERR);

	priv = BRASERO_TASK_CTX_PRIVATE (self);
	priv->use_average_rate = use_average;
	return BRASERO_BURN_OK;
}

/**
 * Used to retrieve the values for a given task
 */

BraseroBurnResult
brasero_task_ctx_get_rate (BraseroTaskCtx *self,
			   gint64 *rate)
{
	BraseroTaskCtxPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TASK_CTX (self), BRASERO_BURN_ERR);
	g_return_val_if_fail (rate != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_TASK_CTX_PRIVATE (self);

	if (priv->current_action != BRASERO_BURN_ACTION_RECORDING
	&&  priv->current_action != BRASERO_BURN_ACTION_DRIVE_COPY) {
		*rate = -1;
		return BRASERO_BURN_OK;
	}

	if (priv->rate) {
		*rate = priv->rate;
		return BRASERO_BURN_OK;
	}

	if (priv->use_average_rate) {
		gdouble elapsed;

		if (!priv->written || !priv->timer)
			return BRASERO_BURN_NOT_READY;

		elapsed = g_timer_elapsed (priv->timer, NULL);
		*rate = (gdouble) priv->written / elapsed;
	}
	else {
		if (!priv->last_written)
			return BRASERO_BURN_NOT_READY;
			
		*rate = (gdouble) (priv->current_written - priv->last_written) /
			(gdouble) (priv->current_elapsed - priv->last_elapsed);
	}

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_task_ctx_get_average_rate (BraseroTaskCtx *self,
				   gint64 *rate)
{
	gdouble elapsed;
	BraseroTaskCtxPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TASK_CTX (self), BRASERO_BURN_ERR);
	g_return_val_if_fail (rate != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_TASK_CTX_PRIVATE (self);

	if (!priv->timer)
		return BRASERO_BURN_NOT_READY;

	elapsed = g_timer_elapsed (priv->timer, NULL);
	if (!elapsed)
		return BRASERO_BURN_NOT_READY;

	/* calculate average rate */
	*rate = ((priv->written - priv->first_written) / elapsed);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_task_ctx_get_remaining_time (BraseroTaskCtx *self,
				     long *remaining)
{
	BraseroTaskCtxPrivate *priv;
	gdouble elapsed;
	gint len;

	g_return_val_if_fail (BRASERO_IS_TASK_CTX (self), BRASERO_BURN_ERR);
	g_return_val_if_fail (remaining != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_TASK_CTX_PRIVATE (self);

	g_mutex_lock (priv->lock);
	len = g_slist_length (priv->times);
	g_mutex_unlock (priv->lock);

	if (len < MAX_VALUE_AVERAGE)
		return BRASERO_BURN_NOT_READY;

	elapsed = g_timer_elapsed (priv->timer, NULL);
	*remaining = (gdouble) priv->total_time - (gdouble) elapsed;

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_task_ctx_get_total (BraseroTaskCtx *self,
			    gint64 *total)
{
	BraseroTaskCtxPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TASK_CTX (self), BRASERO_BURN_ERR);
	g_return_val_if_fail (total != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_TASK_CTX_PRIVATE (self);

	if (priv->total <= 0
	&&  priv->written
	&&  priv->progress)
		return BRASERO_BURN_NOT_READY;

	if (!total)
		return BRASERO_BURN_OK;

	if (priv->total <= 0)
		*total = priv->written / priv->progress;
	else
		*total = priv->total;

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_task_ctx_get_written (BraseroTaskCtx *self,
			      gint64 *written)
{
	BraseroTaskCtxPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_TASK_CTX (self), BRASERO_BURN_ERR);
	g_return_val_if_fail (written != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_TASK_CTX_PRIVATE (self);

	if (priv->written <= 0)
		return BRASERO_BURN_NOT_READY;

	if (!written)
		return BRASERO_BURN_OK;

	*written = priv->written;
	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_task_ctx_get_current_action_string (BraseroTaskCtx *self,
					    BraseroBurnAction action,
					    gchar **string)
{
	BraseroTaskCtxPrivate *priv;

	g_return_val_if_fail (string != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_TASK_CTX_PRIVATE (self);

	if (action != priv->current_action)
		return BRASERO_BURN_ERR;

	*string = priv->action_string ? g_strdup (priv->action_string):
					g_strdup (brasero_burn_action_to_string (priv->current_action));

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_task_ctx_get_elapsed (BraseroTaskCtx *self,
			      gdouble *elapsed)
{
	BraseroTaskCtxPrivate *priv;

	g_return_val_if_fail (elapsed != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_TASK_CTX_PRIVATE (self);

	if (!priv->timer)
		return BRASERO_BURN_NOT_READY;

	*elapsed = g_timer_elapsed (priv->timer, NULL);

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_task_ctx_get_progress (BraseroTaskCtx *self, 
			       gdouble *progress)
{
	BraseroTaskCtxPrivate *priv;
	gint64 total = -1;

	priv = BRASERO_TASK_CTX_PRIVATE (self);

	if (priv->progress >= 0.0) {
		if (progress)
			*progress = priv->progress;

		return BRASERO_BURN_OK;
	}

	brasero_task_ctx_get_total (self, &total);
	if (priv->written < 0 || total <= 0)
		return BRASERO_BURN_NOT_READY;

	if (!progress)
		return BRASERO_BURN_OK;

	*progress = (gdouble) priv->written /
		    (gdouble) total;

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_task_ctx_get_current_action (BraseroTaskCtx *self,
				     BraseroBurnAction *action)
{
	BraseroTaskCtxPrivate *priv;

	g_return_val_if_fail (action != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_TASK_CTX_PRIVATE (self);

	g_mutex_lock (priv->lock);
	*action = priv->current_action;
	g_mutex_unlock (priv->lock);

	return BRASERO_BURN_OK;
}

void
brasero_task_ctx_stop_progress (BraseroTaskCtx *self)
{
	BraseroTaskCtxPrivate *priv;

	priv = BRASERO_TASK_CTX_PRIVATE (self);

	priv->current_action = BRASERO_BURN_ACTION_NONE;
	priv->action_changed = 0;

	if (priv->timer) {
		g_timer_destroy (priv->timer);
		priv->timer = NULL;
	}
	priv->first_written = 0;

	g_mutex_lock (priv->lock);

	if (priv->action_string) {
		g_free (priv->action_string);
		priv->action_string = NULL;
	}

	if (priv->times) {
		g_slist_free (priv->times);
		priv->times = NULL;
	}

	g_mutex_unlock (priv->lock);
}

static void
brasero_task_ctx_init (BraseroTaskCtx *object)
{
	BraseroTaskCtxPrivate *priv;

	priv = BRASERO_TASK_CTX_PRIVATE (object);
	priv->lock = g_mutex_new ();
	priv->total = -1;
}

static void
brasero_task_ctx_finalize (GObject *object)
{
	BraseroTaskCtxPrivate *priv;

	priv = BRASERO_TASK_CTX_PRIVATE (object);

	if (priv->lock) {
		g_mutex_free (priv->lock);
		priv->lock = NULL;
	}

	if (priv->timer) {
		g_timer_destroy (priv->timer);
		priv->timer = NULL;
	}

	if (priv->current_track) {
		brasero_track_unref (priv->current_track);
		priv->current_track = NULL;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_task_ctx_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	BraseroTaskCtx *self;
	BraseroTaskCtxPrivate *priv;

	g_return_if_fail (BRASERO_IS_TASK_CTX (object));

	self = BRASERO_TASK_CTX (object);
	priv = BRASERO_TASK_CTX_PRIVATE (self);

	switch (prop_id)
	{
	case PROP_ACTION:
		priv->action = g_value_get_int (value);
		break;
	case PROP_SESSION:
		priv->session = g_value_get_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
brasero_task_ctx_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	BraseroTaskCtx *self;
	BraseroTaskCtxPrivate *priv;

	g_return_if_fail (BRASERO_IS_TASK_CTX (object));

	self = BRASERO_TASK_CTX (object);
	priv = BRASERO_TASK_CTX_PRIVATE (self);

	switch (prop_id)
	{
	case PROP_ACTION:
		g_value_set_int (value, priv->action);
		break;
	case PROP_SESSION:
		g_value_set_object (value, priv->session);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
brasero_task_ctx_class_init (BraseroTaskCtxClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	parent_class = G_OBJECT_CLASS (g_type_class_peek_parent (klass));

	g_type_class_add_private (klass, sizeof (BraseroTaskCtxPrivate));

	object_class->finalize = brasero_task_ctx_finalize;
	object_class->set_property = brasero_task_ctx_set_property;
	object_class->get_property = brasero_task_ctx_get_property;

	brasero_task_ctx_signals [PROGRESS_CHANGED_SIGNAL] =
	    g_signal_new ("progress_changed",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__VOID,
			  G_TYPE_NONE,
			  0);

	brasero_task_ctx_signals [ACTION_CHANGED_SIGNAL] =
	    g_signal_new ("action_changed",
			  G_TYPE_FROM_CLASS (klass),
			  G_SIGNAL_RUN_LAST,
			  0,
			  NULL, NULL,
			  g_cclosure_marshal_VOID__INT,
			  G_TYPE_NONE,
			  1,
			  G_TYPE_INT);

	g_object_class_install_property (object_class,
	                                 PROP_ACTION,
	                                 g_param_spec_int ("action",
							   "The action the task must perform",
							   "The action the task must perform",
							   BRASERO_TASK_ACTION_ERASE,
							   BRASERO_TASK_ACTION_CHECKSUM,
							   BRASERO_TASK_ACTION_NORMAL,
							   G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class,
	                                 PROP_SESSION,
	                                 g_param_spec_object ("session",
	                                                      "The session this object is tied to",
	                                                      "The session this object is tied to",
	                                                      BRASERO_TYPE_BURN_SESSION,
	                                                      G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}
