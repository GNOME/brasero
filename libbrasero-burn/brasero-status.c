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


#include "brasero-status.h"


typedef struct _BraseroStatusPrivate BraseroStatusPrivate;
struct _BraseroStatusPrivate
{
	BraseroBurnResult res;
	GError * error;
	gdouble progress;
	gchar * current_action;
};

#define BRASERO_STATUS_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_STATUS, BraseroStatusPrivate))

G_DEFINE_TYPE (BraseroStatus, brasero_status, G_TYPE_OBJECT);


/**
 * brasero_status_new:
 *
 * Creates a new #BraseroStatus object.
 *
 * Return value: a #BraseroStatus.
 **/

BraseroStatus *
brasero_status_new (void)
{
	return g_object_new (BRASERO_TYPE_STATUS, NULL);
}

/**
 * brasero_status_get_result:
 * @status: a #BraseroStatus.
 *
 * After an object (see brasero_burn_track_get_status ()) has
 * been requested its status, this function returns that status.
 *
 * Return value: a #BraseroBurnResult.
 * BRASERO_BURN_OK if the object is ready.
 * BRASERO_BURN_NOT_READY if some time should be given to the object before it is ready.
 * BRASERO_BURN_ERR if there is an error.
 **/

BraseroBurnResult
brasero_status_get_result (BraseroStatus *status)
{
	BraseroStatusPrivate *priv;

	g_return_val_if_fail (status != NULL, BRASERO_BURN_ERR);
	g_return_val_if_fail (BRASERO_IS_STATUS (status), BRASERO_BURN_ERR);

	priv = BRASERO_STATUS_PRIVATE (status);
	return priv->res;
}

/**
 * brasero_status_get_progress:
 * @status: a #BraseroStatus.
 *
 * If brasero_status_get_result () returned BRASERO_BURN_NOT_READY,
 * this function returns the progress regarding the operation completion.
 *
 * Return value: a #gdouble
 **/

gdouble
brasero_status_get_progress (BraseroStatus *status)
{
	BraseroStatusPrivate *priv;

	g_return_val_if_fail (status != NULL, -1.0);
	g_return_val_if_fail (BRASERO_IS_STATUS (status), -1.0);

	priv = BRASERO_STATUS_PRIVATE (status);
	if (priv->res == BRASERO_BURN_OK)
		return 1.0;

	if (priv->res != BRASERO_BURN_NOT_READY)
		return -1.0;

	return priv->progress;
}

/**
 * brasero_status_get_error:
 * @status: a #BraseroStatus.
 *
 * If brasero_status_get_result () returned BRASERO_BURN_ERR,
 * this function returns the error.
 *
 * Return value: a #GError
 **/

GError *
brasero_status_get_error (BraseroStatus *status)
{
	BraseroStatusPrivate *priv;

	g_return_val_if_fail (status != NULL, NULL);
	g_return_val_if_fail (BRASERO_IS_STATUS (status), NULL);

	priv = BRASERO_STATUS_PRIVATE (status);
	if (priv->res != BRASERO_BURN_ERR)
		return NULL;

	return g_error_copy (priv->error);
}

/**
 * brasero_status_get_current_action:
 * @status: a #BraseroStatus.
 *
 * If brasero_status_get_result () returned BRASERO_BURN_NOT_READY,
 * this function returns a string describing the operation currently performed.
 * Free the string when it is not needed anymore.
 *
 * Return value: a #gchar.
 **/

gchar *
brasero_status_get_current_action (BraseroStatus *status)
{
	gchar *string;
	BraseroStatusPrivate *priv;

	g_return_val_if_fail (status != NULL, NULL);
	g_return_val_if_fail (BRASERO_IS_STATUS (status), NULL);

	priv = BRASERO_STATUS_PRIVATE (status);

	if (priv->res != BRASERO_BURN_NOT_READY)
		return NULL;

	string = g_strdup (priv->current_action);
	return string;

}

/**
 * brasero_status_set_completed:
 * @status: a #BraseroStatus.
 *
 * Sets the status for a request to BRASERO_BURN_OK.
 *
 **/

void
brasero_status_set_completed (BraseroStatus *status)
{
	BraseroStatusPrivate *priv;

	g_return_if_fail (status != NULL);
	g_return_if_fail (BRASERO_IS_STATUS (status));

	priv = BRASERO_STATUS_PRIVATE (status);

	priv->res = BRASERO_BURN_OK;
	priv->progress = 1.0;
}

/**
 * brasero_status_set_not_ready:
 * @status: a #BraseroStatus.
 * @progress: a #gdouble or -1.0.
 * @current_action: a #gchar or NULL.
 *
 * Sets the status for a request to BRASERO_BURN_NOT_READY.
 * Allows to set a string describing the operation currently performed
 * as well as the progress regarding the operation completion.
 *
 **/

void
brasero_status_set_not_ready (BraseroStatus *status,
			      gdouble progress,
			      const gchar *current_action)
{
	BraseroStatusPrivate *priv;

	g_return_if_fail (status != NULL);
	g_return_if_fail (BRASERO_IS_STATUS (status));

	priv = BRASERO_STATUS_PRIVATE (status);

	priv->res = BRASERO_BURN_NOT_READY;
	priv->progress = progress;

	if (priv->current_action)
		g_free (priv->current_action);
	priv->current_action = g_strdup (current_action);
}

/**
 * brasero_status_set_running:
 * @status: a #BraseroStatus.
 * @progress: a #gdouble or -1.0.
 * @current_action: a #gchar or NULL.
 *
 * Sets the status for a request to BRASERO_BURN_RUNNING.
 * Allows to set a string describing the operation currently performed
 * as well as the progress regarding the operation completion.
 *
 **/

void
brasero_status_set_running (BraseroStatus *status,
			    gdouble progress,
			    const gchar *current_action)
{
	BraseroStatusPrivate *priv;

	g_return_if_fail (status != NULL);
	g_return_if_fail (BRASERO_IS_STATUS (status));

	priv = BRASERO_STATUS_PRIVATE (status);

	priv->res = BRASERO_BURN_RUNNING;
	priv->progress = progress;

	if (priv->current_action)
		g_free (priv->current_action);
	priv->current_action = g_strdup (current_action);
}

/**
 * brasero_status_set_error:
 * @status: a #BraseroStatus.
 * @error: a #GError or NULL.
 *
 * Sets the status for a request to BRASERO_BURN_ERR.
 *
 **/

void
brasero_status_set_error (BraseroStatus *status,
			  GError *error)
{
	BraseroStatusPrivate *priv;

	g_return_if_fail (status != NULL);
	g_return_if_fail (BRASERO_IS_STATUS (status));

	priv = BRASERO_STATUS_PRIVATE (status);

	priv->res = BRASERO_BURN_ERR;
	priv->progress = -1.0;

	if (priv->error)
		g_error_free (priv->error);
	priv->error = error;
}

static void
brasero_status_init (BraseroStatus *object)
{}

static void
brasero_status_finalize (GObject *object)
{
	BraseroStatusPrivate *priv;

	priv = BRASERO_STATUS_PRIVATE (object);
	if (priv->error)
		g_error_free (priv->error);

	if (priv->current_action)
		g_free (priv->current_action);

	G_OBJECT_CLASS (brasero_status_parent_class)->finalize (object);
}

static void
brasero_status_class_init (BraseroStatusClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroStatusPrivate));

	object_class->finalize = brasero_status_finalize;
}

