/***************************************************************************
 *            burn.c
 *
 *  ven mar  3 18:50:18 2006
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

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include <libgnomevfs/gnome-vfs.h>

#include <nautilus-burn-drive.h>

#include "brasero-marshal.h"
#include "burn-basics.h"
#include "burn.h"
#include "burn-job.h"
#include "burn-imager.h"
#include "burn-recorder.h"
#include "burn-common.h"
#include "burn-caps.h"
#include "burn-session.h"
#include "burn-volume.h"
#include "brasero-ncb.h"

static void brasero_burn_class_init (BraseroBurnClass *klass);
static void brasero_burn_init (BraseroBurn *sp);
static void brasero_burn_finalize (GObject *object);

struct BraseroBurnPrivate {
	BraseroTask *task;
	BraseroBurnCaps *caps;
	BraseroBurnSession *session;

	GMainLoop *sleep_loop;

	BraseroRecorder *recorder;
	BraseroImager *imager;

	NautilusBurnDrive *drive;

	BraseroMediumInfo src_status;
	BraseroMediumInfo dest_status;

	gint64 image_size;

	gint on_the_fly;

	/* FIXME: that's a trick until we have a proper session object */
	guint automatic_append:1;
};

#define IS_LOCKED	"LOCKED"
#define BRASERO_BURN_NOT_SUPPORTED_LOG(burn, flags)				\
	{									\
		if (burn->priv->session) {					\
			brasero_burn_log (burn,					\
					  flags,				\
					  "unsupported operation (in %s at %s)",	\
					  G_STRFUNC,				\
					  G_STRLOC);				\
		}								\
		return BRASERO_BURN_NOT_SUPPORTED;				\
	}

#define BRASERO_BURN_NOT_READY_LOG(burn, flags)					\
	{									\
		if (burn->priv->session) {					\
			brasero_burn_log (burn,					\
					  flags,				\
					  "not ready to operate (in %s at %s)",	\
					  G_STRFUNC,				\
					  G_STRLOC);				\
		}								\
		return BRASERO_BURN_NOT_READY;				\
	}

typedef enum {
	ASK_DISABLE_JOLIET_SIGNAL,
	WARN_DATA_LOSS_SIGNAL,
	WARN_PREVIOUS_SESSION_LOSS_SIGNAL,
	WARN_AUDIO_TO_APPENDABLE_SIGNAL,
	WARN_REWRITABLE_SIGNAL,
	INSERT_MEDIA_REQUEST_SIGNAL,
	PROGRESS_CHANGED_SIGNAL,
	ACTION_CHANGED_SIGNAL,
	LAST_SIGNAL
} BraseroBurnSignalType;

static guint brasero_burn_signals [LAST_SIGNAL] = { 0 };
static GObjectClass *parent_class = NULL;

GType
brasero_burn_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroBurnClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_burn_class_init,
			NULL,
			NULL,
			sizeof (BraseroBurn),
			0,
			(GInstanceInitFunc)brasero_burn_init,
		};

		type = g_type_register_static (G_TYPE_OBJECT, 
					       "BraseroBurn", 
					       &our_info,
					       0);
	}

	return type;
}

static void
brasero_burn_class_init (BraseroBurnClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_burn_finalize;

	brasero_burn_signals [ASK_DISABLE_JOLIET_SIGNAL] =
		g_signal_new ("disable_joliet",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (BraseroBurnClass,
					       ask_disable_joliet),
			      NULL, NULL,
			      brasero_marshal_INT__VOID,
			      G_TYPE_INT, 0);
        brasero_burn_signals [WARN_DATA_LOSS_SIGNAL] =
		g_signal_new ("warn_data_loss",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (BraseroBurnClass,
					       warn_data_loss),
			      NULL, NULL,
			      brasero_marshal_INT__VOID,
			      G_TYPE_INT, 0);
        brasero_burn_signals [WARN_PREVIOUS_SESSION_LOSS_SIGNAL] =
		g_signal_new ("warn_previous_session_loss",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (BraseroBurnClass,
					       warn_previous_session_loss),
			      NULL, NULL,
			      brasero_marshal_INT__VOID,
			      G_TYPE_INT, 0);
        brasero_burn_signals [WARN_AUDIO_TO_APPENDABLE_SIGNAL] =
		g_signal_new ("warn_audio_to_appendable",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (BraseroBurnClass,
					       warn_audio_to_appendable),
			      NULL, NULL,
			      brasero_marshal_INT__VOID,
			      G_TYPE_INT, 0);
	brasero_burn_signals [WARN_REWRITABLE_SIGNAL] =
		g_signal_new ("warn_rewritable",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (BraseroBurnClass,
					       warn_rewritable),
			      NULL, NULL,
			      brasero_marshal_INT__VOID,
			      G_TYPE_INT, 0);
        brasero_burn_signals [INSERT_MEDIA_REQUEST_SIGNAL] =
		g_signal_new ("insert_media",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (BraseroBurnClass,
					       insert_media_request),
			      NULL, NULL,
			      brasero_marshal_INT__OBJECT_INT_INT,
			      G_TYPE_INT, 
			      3,
			      NAUTILUS_BURN_TYPE_DRIVE,
			      G_TYPE_INT,
			      G_TYPE_INT);
        brasero_burn_signals [PROGRESS_CHANGED_SIGNAL] =
		g_signal_new ("progress_changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (BraseroBurnClass,
					       progress_changed),
			      NULL, NULL,
			      brasero_marshal_VOID__DOUBLE_DOUBLE_LONG,
			      G_TYPE_NONE, 
			      3,
			      G_TYPE_DOUBLE,
			      G_TYPE_DOUBLE,
			      G_TYPE_LONG);
        brasero_burn_signals [ACTION_CHANGED_SIGNAL] =
		g_signal_new ("action_changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (BraseroBurnClass,
					       action_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 
			      1,
			      G_TYPE_INT);
}

static BraseroBurnResult
brasero_burn_emit_signal (BraseroBurn *burn, guint signal)
{
	GValue instance_and_params;
	GValue return_value;

	instance_and_params.g_type = 0;
	g_value_init (&instance_and_params, G_TYPE_FROM_INSTANCE (burn));
	g_value_set_instance (&instance_and_params, burn);

	return_value.g_type = 0;
	g_value_init (&return_value, G_TYPE_INT);
	g_value_set_int (&return_value, BRASERO_BURN_CANCEL);

	g_signal_emitv (&instance_and_params,
			brasero_burn_signals [signal],
			0,
			&return_value);

	g_value_unset (&instance_and_params);

	return g_value_get_int (&return_value);
}

static void
brasero_burn_init (BraseroBurn *obj)
{
	obj->priv = g_new0 (BraseroBurnPrivate, 1);

	obj->priv->caps = brasero_burn_caps_get_default ();
}

static void
brasero_burn_finalize (GObject *object)
{
	BraseroBurn *cobj;
	cobj = BRASERO_BURN(object);

	if (cobj->priv->sleep_loop) {
		g_main_loop_quit (cobj->priv->sleep_loop);
		cobj->priv->sleep_loop = NULL;
	}

	if (cobj->priv->task) {
		g_object_unref (cobj->priv->task);
		cobj->priv->task = NULL;
	}

	if (cobj->priv->session) {
		g_object_unref (cobj->priv->session);
		cobj->priv->session = NULL;
	}

	if (cobj->priv->caps)
		g_object_unref (cobj->priv->caps);
	if (cobj->priv->recorder)
		g_object_unref (cobj->priv->recorder);
	if (cobj->priv->imager)
		g_object_unref (cobj->priv->imager);

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

BraseroBurn *
brasero_burn_new ()
{
	BraseroBurn *obj;
	
	obj = BRASERO_BURN (g_object_new (BRASERO_TYPE_BURN, NULL));

	return obj;
}

void
brasero_burn_set_session (BraseroBurn *burn,
			  BraseroBurnSession *session)
{
	if (burn->priv->session)
		g_object_unref (burn->priv->session);

	g_object_ref (session);
	burn->priv->session = session;
}

static void
brasero_burn_log (BraseroBurn *burn,
		  BraseroBurnFlag flags,
		  const gchar *format,
		  ...)
{
	if (burn->priv->session) {
		va_list arg_list;

		va_start (arg_list, format);
		brasero_burn_session_logv (burn->priv->session, format, arg_list);
		va_end (arg_list);
	}		

	if (flags & BRASERO_BURN_FLAG_DEBUG)
		BRASERO_BURN_LOGV (format);
}

static gboolean
brasero_burn_wakeup (BraseroBurn *burn)
{
	if (burn->priv->sleep_loop)
		g_main_loop_quit (burn->priv->sleep_loop);

	return FALSE;
}

static BraseroBurnResult
brasero_burn_sleep (BraseroBurn *burn, gint msec)
{
	burn->priv->sleep_loop = g_main_loop_new (NULL, FALSE);
	g_timeout_add (msec,
		       (GSourceFunc) brasero_burn_wakeup,
		       burn);

	g_main_loop_run (burn->priv->sleep_loop);

	if (burn->priv->sleep_loop) {
		g_main_loop_unref (burn->priv->sleep_loop);
		burn->priv->sleep_loop = NULL;
		return BRASERO_BURN_OK;
	}

	/* if sleep_loop = NULL => We've been cancelled */
	return BRASERO_BURN_CANCEL;
}

static void
brasero_burn_progress_changed (BraseroTask *task,
			       BraseroBurn *burn)
{
	BraseroBurnAction action = BRASERO_BURN_ACTION_NONE;
	gdouble overall_progress = -1.0;
	gdouble task_progress = -1.0;
	glong time_remaining = -1;
	gboolean is_burning;

	brasero_task_get_action (task, &action);

	/* get the task current progress */
	if (brasero_task_get_progress (task, &task_progress) == BRASERO_BURN_OK) {
		brasero_task_get_remaining_time (task, &time_remaining);
		if (action == BRASERO_BURN_ACTION_ERASING)
			overall_progress = task_progress;
		else if (burn->priv->on_the_fly)
			overall_progress = task_progress;
		else if (burn->priv->drive && NCB_DRIVE_GET_TYPE (burn->priv->drive) == NAUTILUS_BURN_DRIVE_TYPE_FILE)
			overall_progress = task_progress;
		else if (burn->priv->recorder)
			overall_progress = (task_progress + 1.0) / 2.0;
		else
			overall_progress = (task_progress / 2.0);
	}
	else if (action == BRASERO_BURN_ACTION_ERASING
	     ||  burn->priv->on_the_fly
	     ||  !burn->priv->recorder
	     ||  (burn->priv->drive && NCB_DRIVE_GET_TYPE (burn->priv->drive) == NAUTILUS_BURN_DRIVE_TYPE_FILE))
		overall_progress = 0.0;
	else
		overall_progress = 0.5;

	is_burning = (burn->priv->recorder != NULL &&
		      brasero_job_is_running (BRASERO_JOB (burn->priv->recorder)));

	g_signal_emit (burn,
		       brasero_burn_signals [PROGRESS_CHANGED_SIGNAL],
		       0,
		       overall_progress,
		       task_progress,
		       time_remaining);
}

static void
brasero_burn_action_changed_real (BraseroBurn *burn,
				  BraseroBurnAction action)
{
	g_signal_emit (burn,
		       brasero_burn_signals [ACTION_CHANGED_SIGNAL],
		       0,
		       action);
}

static void
brasero_burn_action_changed (BraseroTask *task,
			     BraseroBurnAction action,
			     BraseroBurn *burn)
{
	brasero_burn_action_changed_real (burn, action);
}

static void
brasero_burn_get_task (BraseroBurn *burn)
{
	burn->priv->task = brasero_task_new ();
	g_signal_connect (burn->priv->task,
			  "progress_changed",
			  G_CALLBACK (brasero_burn_progress_changed),
			  burn);
	g_signal_connect (burn->priv->task,
			  "action_changed",
			  G_CALLBACK (brasero_burn_action_changed),
			  burn);
}

void
brasero_burn_get_action_string (BraseroBurn *burn,
				BraseroBurnAction action,
				gchar **string)
{
	g_return_if_fail (string != NULL);

	if (action == BRASERO_BURN_ACTION_FINISHED)
		(*string) = g_strdup (brasero_burn_action_to_string (action));
	else
		brasero_task_get_action_string (burn->priv->task,
						action,
						string);
}

BraseroBurnResult
brasero_burn_status (BraseroBurn *burn,
		     BraseroMediumInfo *media_status,
		     gint64 *isosize,
		     gint64 *written,
		     gint64 *rate)
{
	brasero_task_get_rate (burn->priv->task, rate);
	brasero_task_get_total (burn->priv->task, isosize);
	brasero_task_get_written (burn->priv->task, written);

	if (burn->priv->recorder
	&&  brasero_job_is_running (BRASERO_JOB (burn->priv->recorder))) {
		if (media_status)
			*media_status = burn->priv->dest_status;
	}
	else if (burn->priv->imager) {
		if (media_status)
			*media_status = burn->priv->src_status;
	}
	else
		return BRASERO_BURN_NOT_READY;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_ask_for_media (BraseroBurn *burn,
			    NautilusBurnDrive *drive,
			    BraseroBurnError error_type,
			    BraseroMediumInfo required_media,
			    gboolean eject,
			    GError **error)
{
	gboolean is_mounted;
	BraseroMediumInfo media;

	GValue instance_and_params [4];
	GValue return_value;

	media = NCB_MEDIA_GET_STATUS (drive);

	if (media != BRASERO_MEDIUM_NONE) {
		/* check one more time */
		is_mounted = nautilus_burn_drive_is_mounted (drive);
		if (is_mounted)
			error_type = BRASERO_BURN_ERROR_MEDIA_BUSY;

		if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (drive), IS_LOCKED))
		&& !nautilus_burn_drive_unlock (drive)) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("the drive can't be unlocked"));
			return BRASERO_BURN_ERROR;
		}
		g_object_set_data (G_OBJECT (drive), IS_LOCKED, GINT_TO_POINTER (0));

		if (eject && !nautilus_burn_drive_eject (drive)) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("the disc can't be ejected"));
			return BRASERO_BURN_ERR;
		}
	}
	else
		error_type = BRASERO_BURN_ERROR_MEDIA_NONE;

	instance_and_params [0].g_type = 0;
	g_value_init (instance_and_params, G_TYPE_FROM_INSTANCE (burn));
	g_value_set_instance (instance_and_params, burn);
	
	instance_and_params [1].g_type = 0;
	g_value_init (instance_and_params + 1, G_TYPE_FROM_INSTANCE (drive));
	g_value_set_instance (instance_and_params + 1, drive);
	
	instance_and_params [2].g_type = 0;
	g_value_init (instance_and_params + 2, G_TYPE_INT);
	g_value_set_int (instance_and_params + 2, error_type);
	
	instance_and_params [3].g_type = 0;
	g_value_init (instance_and_params + 3, G_TYPE_INT);
	g_value_set_int (instance_and_params + 3, required_media);
	
	return_value.g_type = 0;
	g_value_init (&return_value, G_TYPE_INT);
	g_value_set_int (&return_value, BRASERO_BURN_CANCEL);

	g_signal_emitv (instance_and_params,
			brasero_burn_signals [INSERT_MEDIA_REQUEST_SIGNAL],
			0,
			&return_value);

	g_value_unset (instance_and_params);
	g_value_unset (instance_and_params + 1);

	return g_value_get_int (&return_value);
}

static BraseroBurnResult
brasero_burn_media_check_basics (BraseroBurn *burn,
				 NautilusBurnDrive *drive,
				 BraseroMediumInfo media,
				 BraseroMediumInfo required_media,
				 gboolean eject,
				 GError **error)
{
	BraseroBurnError error_type;
	BraseroBurnResult result;

	if (media == BRASERO_MEDIUM_NONE)
		error_type = BRASERO_BURN_ERROR_MEDIA_NONE;
	else
		error_type = BRASERO_BURN_ERROR_NONE;

	if (error_type != BRASERO_BURN_ERROR_NONE) {
		result = brasero_burn_ask_for_media (burn,
						     drive,
						     error_type,
						     required_media,
						     eject,
						     error);

		if (result != BRASERO_BURN_OK)
			return result;

		return BRASERO_BURN_NEED_RELOAD;
	}

	return BRASERO_BURN_OK;
}

BraseroBurnResult
brasero_burn_wait_for_source_media (BraseroBurn *burn,
				    NautilusBurnDrive *drive,
				    gboolean eject,
				    GError **error)
{
	gchar *failure;
	BraseroMediumInfo media;
	BraseroBurnResult result;

again:
	if (nautilus_burn_drive_is_mounted (drive)) {
		if (!NCB_DRIVE_UNMOUNT (drive, NULL))
			g_warning ("Couldn't unmount volume in drive: %s",
				   NCB_DRIVE_GET_DEVICE (drive));
	}

	/* NOTE: we used to unmount the media before now we shouldn't need that
	 * get any information from the drive */
	media = NCB_MEDIA_GET_STATUS (drive);
	result = brasero_burn_media_check_basics (burn,
						  drive,
						  media,
						  BRASERO_MEDIUM_HAS_DATA,
						  eject,
						  error);
	if (result == BRASERO_BURN_NEED_RELOAD)
		goto again;

	if (result != BRASERO_BURN_OK)
		return result;

	if (media & BRASERO_MEDIUM_BLANK) {
		result = brasero_burn_ask_for_media (burn,
						     drive,
						     BRASERO_BURN_ERROR_MEDIA_BLANK,
						     BRASERO_MEDIUM_HAS_DATA,
						     eject,
						     error);
		if (result != BRASERO_BURN_OK)
			return result;

		goto again;
	}

	/* we set IS_LOCKED to remind ourselves that we were the ones that locked it */
	if (!GPOINTER_TO_INT (g_object_get_data (G_OBJECT (drive), IS_LOCKED))
	&&  !nautilus_burn_drive_lock (drive, _("ongoing copying process"), &failure)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the drive can't be locked (%s)"),
			     failure);
		return BRASERO_BURN_ERR;
	}

	g_object_set_data (G_OBJECT (drive), IS_LOCKED, GINT_TO_POINTER (1));
	burn->priv->src_status = media;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_reload_src_media (BraseroBurn *burn,
			       BraseroBurnError error_code,
			       BraseroBurnFlag flags,
			       const BraseroTrackSource *source,
			       GError **error)
{
	BraseroBurnResult result;

	if (source->type != BRASERO_TRACK_SOURCE_DISC)
		return BRASERO_BURN_ERR;

	result = brasero_burn_ask_for_media (burn,
					     source->contents.drive.disc,
					     error_code,
					     BRASERO_MEDIUM_HAS_DATA,
					     (flags & BRASERO_BURN_FLAG_EJECT),
					     error);
	if (result != BRASERO_BURN_OK)
		return result;

	result = brasero_burn_wait_for_source_media (burn,
						     source->contents.drive.disc,
						     (flags & BRASERO_BURN_FLAG_EJECT),
						     error);

	return result;
}

static BraseroBurnResult
brasero_burn_wait_for_rewritable_media (BraseroBurn *burn,
					NautilusBurnDrive *drive,
					gboolean fast,
					gboolean eject,
					GError **error)
{
	gchar *failure;
	BraseroMediumInfo media;
	BraseroBurnResult result;

	if (!nautilus_burn_drive_can_rewrite (drive)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the drive has no rewriting capabilities"));
		return BRASERO_BURN_NOT_SUPPORTED;
	}

 again:

	if (nautilus_burn_drive_is_mounted (drive)) {
		if (!NCB_DRIVE_UNMOUNT (drive, NULL))
			g_warning ("Couldn't unmount volume in drive: %s",
				   NCB_DRIVE_GET_DEVICE (drive));
	}

	media = NCB_MEDIA_GET_STATUS (drive);
	result = brasero_burn_media_check_basics (burn,
						  drive,
						  media,
						  BRASERO_MEDIUM_REWRITABLE|
						  BRASERO_MEDIUM_HAS_DATA,
						  eject,
						  error);
	if (result == BRASERO_BURN_NEED_RELOAD)
		goto again;
	
	if (result != BRASERO_BURN_OK)
		return result;

	/* We have and error if medium is not rewritable or if it is blank.
	 * if full blanking is required don't check for the blank */
	if (!(media & BRASERO_MEDIUM_REWRITABLE)) {
		result = brasero_burn_ask_for_media (burn,
						     drive,
						     BRASERO_BURN_ERROR_MEDIA_NOT_REWRITABLE,
						     BRASERO_MEDIUM_REWRITABLE|
						     BRASERO_MEDIUM_HAS_DATA,
						     eject,
						     error);
		if (result != BRASERO_BURN_OK)
			return result;

		goto again;
	}
	else if (fast && (media & BRASERO_MEDIUM_BLANK)) {
		result = brasero_burn_ask_for_media (burn,
						     drive,
						     BRASERO_BURN_ERROR_MEDIA_BLANK,
						     BRASERO_MEDIUM_REWRITABLE|
						     BRASERO_MEDIUM_HAS_DATA,
						     eject,
						     error);
		if (result != BRASERO_BURN_OK)
			return result;

		goto again;
	}

	if (!GPOINTER_TO_INT (g_object_get_data (G_OBJECT (drive), IS_LOCKED))
	&&  !nautilus_burn_drive_lock (drive, _("ongoing blanking process"), &failure)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the drive can't be locked (%s)"),
			     failure);
		return BRASERO_BURN_ERR;
	}

	g_object_set_data (G_OBJECT (drive), IS_LOCKED, GINT_TO_POINTER (1));
	burn->priv->dest_status = media;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_blank_real (BraseroBurn *burn,
			 NautilusBurnDrive *drive,
			 BraseroRecorderFlag flags,
			 gboolean is_debug,
			 GError **error)
{
	BraseroBurnResult result;

	result = brasero_burn_caps_create_recorder_for_blanking (burn->priv->caps,
								 &burn->priv->recorder,
								 burn->priv->dest_status,
								 (flags & BRASERO_RECORDER_FLAG_FAST_BLANK),
								 error);
	if (result != BRASERO_BURN_OK)
		return result;

	if (burn->priv->session)
		brasero_job_set_session (BRASERO_JOB (burn->priv->recorder),
					 burn->priv->session);

	brasero_job_set_debug (BRASERO_JOB (burn->priv->recorder), is_debug);
	result = brasero_recorder_set_drive (burn->priv->recorder,
					     drive,
					     error);
	if (result != BRASERO_BURN_OK)
		return result;

	result = brasero_recorder_set_flags (burn->priv->recorder,
					     flags,
					     error);
	if (result != BRASERO_BURN_OK)
		return result;

	if (nautilus_burn_drive_is_mounted (drive)
	&& !NCB_DRIVE_UNMOUNT (drive, NULL)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_BUSY_DRIVE,
			     _("the drive seems to be busy"));
		result = BRASERO_BURN_ERR;
	}
	else {
		brasero_burn_get_task (burn);
		brasero_job_set_task (BRASERO_JOB (burn->priv->recorder),
				      burn->priv->task);
		result = brasero_recorder_blank (burn->priv->recorder, error);
		brasero_job_set_task (BRASERO_JOB (burn->priv->recorder), NULL);
		g_object_unref (burn->priv->task);
		burn->priv->task = NULL;
	}

	g_object_unref (burn->priv->recorder);
	burn->priv->recorder = NULL;

	return result;
}

BraseroBurnResult
brasero_burn_blank (BraseroBurn *burn,
		    BraseroBurnFlag burn_flags,
		    NautilusBurnDrive *drive,
		    gboolean fast,
		    GError **error)
{
	BraseroRecorderFlag blank_flags;
	BraseroBurnResult result;
	GError *ret_error = NULL;

	/* we wait for the insertion of a media and lock it */
	g_return_val_if_fail (drive != NULL, BRASERO_BURN_ERR);
	result = brasero_burn_wait_for_rewritable_media (burn,
							 drive,
							 fast,
							 (burn_flags & BRASERO_BURN_FLAG_EJECT),
							 error);

	if (result != BRASERO_BURN_OK)
		return result;

	if (burn->priv->session)
		brasero_burn_log (burn,
				  burn_flags,
				  "Starting new session (blanking):\n"
				  "\tflags\t\t\t= %i \n"
				  "\tmedia type\t= %i\n"
				  "\tfast\t\t= %i\n",
				  burn_flags,
				  burn->priv->dest_status != BRASERO_MEDIUM_NONE ? burn->priv->dest_status:burn->priv->src_status,
				  fast);

	blank_flags = BRASERO_RECORDER_FLAG_NONE;
	if (burn_flags & BRASERO_BURN_FLAG_NOGRACE)
		blank_flags |= BRASERO_RECORDER_FLAG_NOGRACE;

	if (burn_flags & BRASERO_BURN_FLAG_DUMMY)
		blank_flags |= BRASERO_RECORDER_FLAG_DUMMY;

	if (fast)
		blank_flags |= BRASERO_RECORDER_FLAG_FAST_BLANK;

	result = brasero_burn_blank_real (burn,
					  drive,
					  blank_flags,
					  (burn_flags & BRASERO_BURN_FLAG_DEBUG) != 0,
					  &ret_error);

	while (result == BRASERO_BURN_ERR
	&&     ret_error
	&&     ret_error->code == BRASERO_BURN_ERROR_MEDIA_NOT_REWRITABLE) {
		g_error_free (ret_error);
		ret_error = NULL;

		result = brasero_burn_ask_for_media (burn,
						     drive,
						     BRASERO_BURN_ERROR_MEDIA_NOT_REWRITABLE,
						     BRASERO_MEDIUM_REWRITABLE|
						     BRASERO_MEDIUM_HAS_DATA,
						     (burn_flags & BRASERO_BURN_FLAG_EJECT),
						     error);
		if (result != BRASERO_BURN_OK)
			break;

		result = brasero_burn_wait_for_rewritable_media (burn,
								 drive,
								 fast,
								 (burn_flags & BRASERO_BURN_FLAG_EJECT),
								 error);
		if (result != BRASERO_BURN_OK)
			break;

		result = brasero_burn_blank_real (burn,
						  drive,
						  blank_flags,
						  (burn_flags & BRASERO_BURN_FLAG_DEBUG) != 0,
						  &ret_error);
	}

	if (ret_error)
		g_propagate_error (error, ret_error);

	nautilus_burn_drive_unlock (drive);
	g_object_set_data (G_OBJECT (drive), IS_LOCKED, GINT_TO_POINTER (0));

	if (burn_flags & BRASERO_BURN_FLAG_EJECT)
		brasero_burn_common_eject_async (drive);

	if (result == BRASERO_BURN_OK)
		brasero_burn_action_changed_real (burn,
						  BRASERO_BURN_ACTION_FINISHED);

	return result;
}

static BraseroBurnResult
brasero_burn_wait_for_dest_media (BraseroBurn *burn,
				  BraseroBurnFlag flags,
				  NautilusBurnDrive *drive,
				  const BraseroTrackSource *source,
				  GError **error)
{
	gchar *failure;
	gint64 media_size;
	BraseroBurnError berror;
	BraseroMediumInfo media;
	BraseroBurnResult result;

	if (!nautilus_burn_drive_can_write (drive)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the drive has no burning capabilities"));
		BRASERO_BURN_NOT_SUPPORTED_LOG (burn, flags);
	}


	result = BRASERO_BURN_OK;
	burn->priv->automatic_append = 0;

again:

	/* if drive is mounted then unmount before checking anything */
	if (nautilus_burn_drive_is_mounted (drive)) {
		if (!NCB_DRIVE_UNMOUNT (drive, NULL))
			g_warning ("Couldn't unmount volume in drive: %s",
				   NCB_DRIVE_GET_DEVICE (drive));
	}

	berror = BRASERO_BURN_ERROR_NONE;
	media = NCB_MEDIA_GET_STATUS (drive);
	if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (drive), IS_LOCKED))) {
		burn->priv->dest_status = media;

		/* NOTE: after a blanking, for nautilus_burn the CD/DVD is still full of
		 * data so if the drive has already been checked there is no need to do
		 * that again since we would be asked if we want to blank it again */
		return result;
	}

	if (media == BRASERO_MEDIUM_NONE) {
		result = BRASERO_BURN_NEED_RELOAD;
		berror = BRASERO_BURN_ERROR_MEDIA_NONE;
		goto end;
	}

	if (media == BRASERO_MEDIUM_UNSUPPORTED) {
		result = BRASERO_BURN_NEED_RELOAD;
		berror = BRASERO_BURN_ERROR_MEDIA_UNSUPPORTED;
		goto end;
	}

	if ((media & BRASERO_MEDIUM_DVD)
	&& !BRASERO_TRACK_SOURCE_ALLOW_DVD (source)) {
		result = BRASERO_BURN_NEED_RELOAD;
		berror = BRASERO_BURN_ERROR_DVD_NOT_SUPPORTED;
		goto end;
	}

	/* make sure we can write something to this disc */
	if (!(media & (BRASERO_MEDIUM_BLANK|
		       BRASERO_MEDIUM_APPENDABLE|
		       BRASERO_MEDIUM_REWRITABLE))) {
		result = BRASERO_BURN_NEED_RELOAD;
		berror = BRASERO_BURN_ERROR_MEDIA_NOT_WRITABLE;
		goto end;
	}

	if (media & BRASERO_MEDIUM_APPENDABLE) {
		/* For the moment let's just allow data to be appended to open
		 * discs. Images should not be allowed. One reason for this is
		 * that an unskilled user could try to add an iso image created
		 * earlier without the "-C" flag of mkisofs. Now this image 
		 * wouldn't work if burnt as a second session as the addresses
		 * would have been set for a first session. I think that would
		 * be the same problem for other FSs as they would think that
		 * the FS starts from sector 0. Maybe one day, once we've got
		 * a working file system handler (at least for iso and udf),
		 * then we might think about changing the addresses? 
		 * Another reason is that we consider DVD+RW and DVD-RW as
		 * appendable whereas they are not really multisession. Only
		 * growisofs can add data to an already ISO FS. Adding an image
		 * to them would be impossible.
		 * When we append data, we warn the users that their previous
		 * session will not be mounted by default by the OS and that it
		 * could be "invisible".
		 * As for for audio we allow it as well but with the warning 
		 * that it might not work as the audio tracks won't have been
		 * burnt in the first session whereas most CD players look into
		 * the first session. NOTE that gnome-cd can play such tracks 
		 */
		if (source->type == BRASERO_TRACK_SOURCE_DATA) {
			gint64 size;

			/* For discs with data tracks we append them rather than
			 * blank and rewrite as we won't lose the data. There is
			 * one exception if we don't have enough space left on
			 * the disc.
			 * NOTE: if MERGE or APPEND flag is on then don't check
			 */
			NCB_MEDIA_GET_FREE_SPACE (drive, &size, NULL);
			if (!(flags & (BRASERO_BURN_FLAG_MERGE|BRASERO_BURN_FLAG_APPEND))
			&&    size > burn->priv->image_size
			&&  !(flags & BRASERO_BURN_FLAG_OVERBURN)) {
				/* only warn if there are some data track
				 * otherwise that is fine since if there is only
				 * audio, these first tracks can still be read
				 */
				if ((media & BRASERO_MEDIUM_HAS_DATA)) {
					result = brasero_burn_emit_signal (burn, WARN_PREVIOUS_SESSION_LOSS_SIGNAL);
					if (result != BRASERO_BURN_OK)
						goto end;
				}

				burn->priv->automatic_append = 1;
				flags |= BRASERO_BURN_FLAG_APPEND;
			}
		}
		else if (source->type == BRASERO_TRACK_SOURCE_SONG) {
			/* We rather blank and rewrite a disc rather than append
			 * data for audio discs. That's because audio tracks
			 * have more chance to be readable by common CD players
			 * as first tracks rather than as last tracks.
			 * MERGE flag is impossible here. */
			if (!(media & BRASERO_MEDIUM_REWRITABLE)
			&&  !(flags & BRASERO_BURN_FLAG_APPEND)) {
				result = brasero_burn_emit_signal (burn, WARN_AUDIO_TO_APPENDABLE_SIGNAL);
				if (result != BRASERO_BURN_OK)
					goto end;

				burn->priv->automatic_append = 1;
				flags |= BRASERO_BURN_FLAG_APPEND;
			}
		}
		else if (!(media & BRASERO_MEDIUM_REWRITABLE)) {
			result = BRASERO_BURN_NEED_RELOAD;
			berror = BRASERO_BURN_ERROR_MEDIA_NOT_WRITABLE;
			goto end;
		}
	}

	/* check that if we copy a CD/DVD we are copying it to an
	 * equivalent media (not a CD => DVD or a DVD => CD) */
	if (source->type == BRASERO_TRACK_SOURCE_DISC) {
		gboolean is_src_DVD;
		gboolean is_dest_DVD;

		is_src_DVD = (burn->priv->src_status & BRASERO_MEDIUM_DVD);
		is_dest_DVD = (media & BRASERO_MEDIUM_DVD);

		if (is_src_DVD != is_dest_DVD) {
			result = BRASERO_BURN_NEED_RELOAD;
			if (is_src_DVD)
				berror = BRASERO_BURN_ERROR_DVD_NOT_SUPPORTED;
			else
				berror = BRASERO_BURN_ERROR_CD_NOT_SUPPORTED;
			goto end;
		}
	}

	/* we check that the image will fit on the media */
	if (flags & (BRASERO_BURN_FLAG_MERGE|BRASERO_BURN_FLAG_APPEND))
		NCB_MEDIA_GET_FREE_SPACE (drive, &media_size, NULL);
	else
		NCB_MEDIA_GET_CAPACITY (drive, &media_size, NULL);

	if (!(flags & BRASERO_BURN_FLAG_OVERBURN)
	&&  media_size < burn->priv->image_size) {
		/* This is a recoverable error so try to ask the user again */
		result = BRASERO_BURN_NEED_RELOAD;
		berror = BRASERO_BURN_ERROR_MEDIA_SPACE;
		goto end;
	}

	if (!nautilus_burn_drive_lock (drive, _("ongoing burning process"), &failure)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the drive can't be locked (%s)"),
			     failure);
		return BRASERO_BURN_ERR;
	}
	g_object_set_data (G_OBJECT (drive), IS_LOCKED, GINT_TO_POINTER (1));

	burn->priv->dest_status = media;

	/* silently ignore if the drive is not rewritable */
	/* NOTE: we corrected such contradictory flags setting (might be an error) as
	 * BRASERO_BURN_FLAG_MERGE|BRASERO_BURN_FLAG_APPEND|BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE */
	if ((flags & (BRASERO_BURN_FLAG_MERGE|BRASERO_BURN_FLAG_APPEND)) == 0
	&&  (flags & BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE)
	&&  (media & BRASERO_MEDIUM_REWRITABLE)
	&& !(media & BRASERO_MEDIUM_BLANK)) {
		result = brasero_burn_emit_signal (burn, WARN_DATA_LOSS_SIGNAL);
		if (result != BRASERO_BURN_OK)
			goto end;

		if (!BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDRW_PLUS)
		&&  !BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDRW_RESTRICTED)) {
			BraseroRecorderFlag blank_flags = BRASERO_RECORDER_FLAG_FAST_BLANK;

			if (flags & BRASERO_BURN_FLAG_DUMMY)
				blank_flags |= BRASERO_RECORDER_FLAG_DUMMY;
	
			if (flags & BRASERO_BURN_FLAG_NOGRACE)
				blank_flags |= BRASERO_RECORDER_FLAG_NOGRACE;
	
			result = brasero_burn_blank_real (burn,
							  drive,
							  blank_flags,
							  (flags & BRASERO_BURN_FLAG_DEBUG) != 0,
							  error);
		}
	}

end:

	if (result == BRASERO_BURN_NEED_RELOAD) {
		BraseroMediumInfo required_media;

		required_media = brasero_burn_caps_get_required_media_type (burn->priv->caps,
									    source);

		result = brasero_burn_ask_for_media (burn,
						     drive,
						     berror,
						     required_media,
						     (flags & BRASERO_BURN_FLAG_EJECT),
						     error);
		if (result == BRASERO_BURN_OK)
			goto again;
	}

	if (result != BRASERO_BURN_OK) {
		g_object_set_data (G_OBJECT (drive), IS_LOCKED, GINT_TO_POINTER (0));
		nautilus_burn_drive_unlock (drive);
	}

	return result;
}

static BraseroBurnResult
brasero_burn_reload_dest_media (BraseroBurn *burn,
				BraseroBurnError error_code,
				BraseroBurnFlag flags,
				NautilusBurnDrive *drive,
				const BraseroTrackSource *source,
				GError **error)
{
	BraseroMediumInfo required_media;
	BraseroBurnResult result;

again:

	/* eject and ask the user to reload a disc */
	if (!BRASERO_TRACK_SOURCE_ALLOW_DVD (source))
		required_media = BRASERO_MEDIUM_WRITABLE|BRASERO_MEDIUM_CD;
	else if (source->type == BRASERO_TRACK_SOURCE_DISC) {
		/* the required media depends on the source */
		if (burn->priv->src_status & BRASERO_MEDIUM_DVD)
			required_media = BRASERO_MEDIUM_WRITABLE|BRASERO_MEDIUM_DVD;
		else
			required_media = BRASERO_MEDIUM_WRITABLE|BRASERO_MEDIUM_CD;
	}
	else /* we accept DVD and CD */
		required_media = BRASERO_MEDIUM_WRITABLE;

	result = brasero_burn_ask_for_media (burn,
					     drive,
					     error_code,
					     required_media,
					     (flags & BRASERO_BURN_FLAG_EJECT),
					     error);
	if (result != BRASERO_BURN_OK)
		return result;

	result = brasero_burn_wait_for_dest_media (burn,
						   flags,
						   drive,
						   source,
						   error);
	if (result == BRASERO_BURN_NEED_RELOAD)
		goto again;

	return result;
}

static BraseroBurnResult
brasero_burn_set_recorder_speed (BraseroBurn *burn,
				 gint speed)
{
	gint64 rate;
	BraseroBurnResult result;

	/* set up the object */
	if (burn->priv->dest_status & BRASERO_MEDIUM_DVD)
		rate = speed * DVD_SPEED;
	else
		rate = speed * CDR_SPEED;

	result = brasero_job_set_rate (BRASERO_JOB (burn->priv->recorder), rate);
	if (result != BRASERO_BURN_OK
	&&  result != BRASERO_BURN_NOT_SUPPORTED)
		return result;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_setup_recorder (BraseroBurn *burn,
			     BraseroBurnFlag flags,
			     NautilusBurnDrive *drive,
			     gint speed,
			     BraseroTrackSource *track,
			     GError **error)
{
	BraseroRecorderFlag rec_flags;
	BraseroBurnResult result;

	if ((flags & BRASERO_BURN_FLAG_DEBUG) != 0)
		brasero_job_set_debug (BRASERO_JOB (burn->priv->recorder), TRUE);

	/* set up the flags */
	rec_flags = BRASERO_RECORDER_FLAG_NONE;

	if (flags & BRASERO_BURN_FLAG_NOGRACE)
		rec_flags |= BRASERO_RECORDER_FLAG_NOGRACE;

	if (flags & BRASERO_BURN_FLAG_OVERBURN)
		rec_flags |= BRASERO_RECORDER_FLAG_OVERBURN;

	if (flags & BRASERO_BURN_FLAG_BURNPROOF)
		rec_flags |= BRASERO_RECORDER_FLAG_BURNPROOF;

	/* we may have added a APPEND flag automatically then ignore DAO */
	if (!burn->priv->automatic_append
	&&   flags & BRASERO_BURN_FLAG_DAO)
		rec_flags |= BRASERO_RECORDER_FLAG_DAO;

	if (flags & BRASERO_BURN_FLAG_DONT_CLOSE)
		rec_flags |= BRASERO_RECORDER_FLAG_MULTI;

	if (flags & BRASERO_BURN_FLAG_DUMMY)
		rec_flags |= BRASERO_RECORDER_FLAG_DUMMY;

	/* set up the object */
	brasero_burn_set_recorder_speed (burn, speed);
	result = brasero_recorder_set_drive (burn->priv->recorder,
					     drive,
					     error);
	if (result != BRASERO_BURN_OK)
		return result;

	result = brasero_recorder_set_flags (burn->priv->recorder,
					     rec_flags,
					     error);
	if (result != BRASERO_BURN_OK)
		return result;

	if (track)
		result = brasero_job_set_source (BRASERO_JOB (burn->priv->recorder),
						 track,
						 error);

	return result;
}

static BraseroBurnResult
brasero_burn_get_recorder (BraseroBurn *burn,
			   BraseroBurnFlag flags,
			   NautilusBurnDrive *drive,
			   BraseroTrackSource *track,
			   gint speed,
			   GError **error)
{
	BraseroBurnResult result;
	BraseroRecorder *recorder = NULL;

	if (burn->priv->recorder) {
		/* just in case */
		g_object_unref (burn->priv->recorder);
		burn->priv->recorder = NULL;
	}

	/* create the appropriate recorder object */
	result = brasero_burn_caps_create_recorder (burn->priv->caps,
						    &recorder,
						    track,
						    burn->priv->dest_status,
						    error);
	if (result != BRASERO_BURN_OK)
		return result;

	if (burn->priv->session)
		brasero_job_set_session (BRASERO_JOB (recorder),
					 burn->priv->session);

	burn->priv->recorder = recorder;
	return brasero_burn_setup_recorder (burn,
					    flags,
					    drive,
					    speed,
					    track,
					    error);
}

static BraseroBurnResult
brasero_burn_setup_imager (BraseroBurn *burn,
			   BraseroBurnFlag flags,
			   NautilusBurnDrive *drive,
			   const char *output,
			   GError **error)
{
	BraseroBurnResult result;

	if (!burn->priv->imager)
		return BRASERO_BURN_OK;

	result = brasero_imager_set_output (burn->priv->imager,
					    output,
					    (flags & BRASERO_BURN_FLAG_DONT_OVERWRITE) == 0,
					    (flags & BRASERO_BURN_FLAG_DONT_CLEAN_OUTPUT) == 0,
					    error);

	if ((result != BRASERO_BURN_OK && result != BRASERO_BURN_NOT_SUPPORTED)
	||  (result == BRASERO_BURN_NOT_SUPPORTED && output != NULL))
		return result;

	if ((flags & (BRASERO_BURN_FLAG_MERGE|BRASERO_BURN_FLAG_APPEND))
	||   burn->priv->automatic_append) {
		result = brasero_imager_set_append (burn->priv->imager,
						    drive,
						    (flags & BRASERO_BURN_FLAG_MERGE) != 0,
						    error);
		if (result != BRASERO_BURN_OK)
			return result;
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_create_imager (BraseroBurn *burn,
			    BraseroBurnFlag flags,
			    NautilusBurnDrive *drive,
			    const BraseroTrackSource *source,
			    GError **error)
{
	BraseroTrackSourceType target = BRASERO_TRACK_SOURCE_DEFAULT;
	BraseroBurnResult result = BRASERO_BURN_OK;
	BraseroImager *imager = NULL;

	result = brasero_burn_caps_create_imager (burn->priv->caps,
						  &imager,
						  source,
						  target,
						  burn->priv->src_status,
						  burn->priv->dest_status,
						  error);
	if (result != BRASERO_BURN_OK)
		return result;

	if (burn->priv->session)
		brasero_job_set_session (BRASERO_JOB (imager),
					 burn->priv->session);

	/* better connect to the signals quite early (especially in the case of 
	 * Mkisofs that might have to download things) */
	burn->priv->imager = imager;

	/* configure the object */
	result = brasero_job_set_source (BRASERO_JOB (imager), source, error);
	if (result != BRASERO_BURN_OK)
		return result;

	result = brasero_imager_set_output_type (imager,
						 target,
						 source->format,
						 error);
	if (result != BRASERO_BURN_OK)
		return result;

	/* special case for imagers that are also recorders (ie, cdrdao, growisofs)
	 * they usually need the drive if we want to calculate the size */
	if (BRASERO_IS_RECORDER (imager)) {
		result = brasero_recorder_set_drive (BRASERO_RECORDER (imager),
						     drive,
						     error);

		if (result != BRASERO_BURN_OK)
			return result;
	}

	if ((flags & BRASERO_BURN_FLAG_DEBUG) != 0)
		brasero_job_set_debug (BRASERO_JOB (imager), TRUE);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_ask_for_joliet (BraseroBurn *burn)
{
	BraseroBurnResult result;

	result = brasero_burn_emit_signal (burn, ASK_DISABLE_JOLIET_SIGNAL);
	if (result != BRASERO_BURN_OK)
		return result;

	result = brasero_imager_set_output_type (burn->priv->imager,
						 BRASERO_TRACK_SOURCE_IMAGE,
						 BRASERO_IMAGE_FORMAT_ISO,
						 NULL);

	return result;
}

static BraseroBurnResult
brasero_burn_check_volume_free_space (BraseroBurn *burn,
				      const char *output,
				      GError **error)
{
	char *dirname;
	char *uri_str;
	GnomeVFSURI *uri;
	GnomeVFSResult result;
	GnomeVFSFileSize vol_size;

	if (!output)
		dirname = g_strdup (g_get_tmp_dir ());
	else
		dirname = g_path_get_dirname (output);

	uri_str = gnome_vfs_get_uri_from_local_path (dirname);
	g_free (dirname);

	uri = gnome_vfs_uri_new (uri_str);
	g_free (uri_str);

	if (uri == NULL)
		return BRASERO_BURN_ERR;

	result = gnome_vfs_get_volume_free_space (uri, &vol_size);
	if (result != GNOME_VFS_OK) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the size of the volume can't be checked (%s)"),
			     gnome_vfs_result_to_string (result));
		gnome_vfs_uri_unref (uri);
		return BRASERO_BURN_ERR;
	}
	gnome_vfs_uri_unref (uri);

	if (burn->priv->image_size > vol_size) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_DISC_SPACE,
			     _("the selected location does not have enough free space to store the disc image (%ld MiB needed)"),
			     (unsigned long) burn->priv->image_size / 1048576);
		return BRASERO_BURN_ERR;
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_get_imager (BraseroBurn *burn, 
			 BraseroBurnFlag flags,
			 NautilusBurnDrive *drive,
			 const BraseroTrackSource *source,
			 const char *output,
			 GError **error)
{
	BraseroBurnResult result;
	GError *ret_error = NULL;

	/* just in case */
	if (burn->priv->imager) {
		g_object_unref (burn->priv->imager);
		burn->priv->imager = NULL;
	}

	result = brasero_burn_create_imager (burn,
					     flags,
					     drive,
					     source,
					     error);
	if (result != BRASERO_BURN_OK)
		return result;

	result = brasero_burn_setup_imager (burn,
					    flags,
					    drive,
					    output,
					    error);
	if (result != BRASERO_BURN_OK)
		return result;

	/* we get the size and (will) check that if we're writing to a disc,
	 * the media is big enough and/or if we're writing to a hard drive,
	 * the volume is big enough */
	/* NOTE: this part is important since it is actually a test that the 
	 * imager _can_ work with such a setup */

	brasero_burn_get_task (burn);
	brasero_job_set_task (BRASERO_JOB (burn->priv->imager),
			      burn->priv->task);
	result = brasero_imager_get_size (BRASERO_IMAGER (burn->priv->imager),
					  &burn->priv->image_size,
					  FALSE,
					  &ret_error);

	if (result == BRASERO_BURN_ERR) {
		BraseroBurnError error_code;

		error_code = ret_error->code;

		/* we try to handle a possible joliet recoverable error */
		if (error_code == BRASERO_BURN_ERROR_JOLIET_TREE)
			result = brasero_burn_ask_for_joliet (burn);

		if (result == BRASERO_BURN_OK) {
			g_error_free (ret_error);
			ret_error = NULL;

			/* we retry without joliet this time */
			result = brasero_imager_get_size (BRASERO_IMAGER (burn->priv->imager),
							  &burn->priv->image_size,
							  FALSE,
							  &ret_error);
		}
	}

	brasero_job_set_task (BRASERO_JOB (burn->priv->imager), NULL);
	g_object_unref (burn->priv->task);
	burn->priv->task = NULL;;

	/* other errors are unrecoverable anyway */
	if (ret_error)
		g_propagate_error (error, ret_error);

	return result;
}

static BraseroBurnResult
brasero_burn_run_imager (BraseroBurn *burn,
			 BraseroBurnFlag flags,
			 const BraseroTrackSource *source,
			 BraseroTrackSource **track,
			 const char *output,
			 GError **error)
{
	BraseroBurnError error_code;
	BraseroBurnResult result;
	GError *ret_error = NULL;


start:

	/* this is just in case */
	if (source->type == BRASERO_TRACK_SOURCE_DISC
	&&  nautilus_burn_drive_is_mounted (source->contents.drive.disc)
	&& !NCB_DRIVE_UNMOUNT (source->contents.drive.disc, NULL)) {
		ret_error = g_error_new (BRASERO_BURN_ERROR,
					 BRASERO_BURN_ERROR_BUSY_DRIVE,
					 _("the drive seems to be busy"));
		result = BRASERO_BURN_ERR;
	}
	else {
		brasero_burn_get_task (burn);
		brasero_job_set_task (BRASERO_JOB (burn->priv->imager), 
				      burn->priv->task);
		result = brasero_imager_get_track (burn->priv->imager,
						   track,
						   &ret_error);
		brasero_job_set_task (BRASERO_JOB (burn->priv->imager), NULL);
		g_object_unref (burn->priv->task);
		burn->priv->task = NULL;;
	}

	if (result != BRASERO_BURN_ERR || !ret_error)
		return result;

	/* See if we can recover from the error */
	error_code = ret_error->code;
	if (error_code == BRASERO_BURN_ERROR_JOLIET_TREE) {
		/* some files are not conforming to Joliet standard see
		 * if the user wants to carry on with a non joliet disc */
		result = brasero_burn_ask_for_joliet (burn);
		if (result != BRASERO_BURN_OK) {
			g_propagate_error (error, ret_error);
			return result;
		}

		g_error_free (ret_error);
		ret_error = NULL;
		goto start;
	}
	else if (error_code == BRASERO_BURN_ERROR_MEDIA_BLANK) {
		/* clean the error anyway since at worst the user will cancel */
		g_error_free (ret_error);
		ret_error = NULL;

		/* The media hasn't data on it: ask for a new one.
		 * NOTE: we'll check the size later after the retry */
		result = brasero_burn_reload_src_media (burn,
							error_code,
							flags,
							source,
							error);
		if (result != BRASERO_BURN_OK)
			return result;

		return BRASERO_BURN_RETRY;
	}

	/* not recoverable propagate the error */
	g_propagate_error (error, ret_error);

	return BRASERO_BURN_ERR;
}

static BraseroBurnResult
brasero_burn_run_recorder (BraseroBurn *burn,
			   BraseroBurnFlag flags,
			   NautilusBurnDrive *drive,
			   gint speed,
			   const BraseroTrackSource *source,
			   BraseroTrackSource *track,
			   const gchar *output,
			   GError **error)
{
	gint error_code;
	gboolean has_slept;
	GError *ret_error = NULL;
	BraseroBurnResult result;

	has_slept = FALSE;

start:

	/* this is just in case */
	if (flags & BRASERO_BURN_FLAG_ON_THE_FLY
	&&  source->type == BRASERO_TRACK_SOURCE_DISC
	&&  nautilus_burn_drive_is_mounted (source->contents.drive.disc)
	&& !NCB_DRIVE_UNMOUNT (source->contents.drive.disc, NULL)) {
		ret_error = g_error_new (BRASERO_BURN_ERROR,
					 BRASERO_BURN_ERROR_BUSY_DRIVE,
					 _("the drive seems to be busy"));
		result = BRASERO_BURN_ERR;
	}
	else if (nautilus_burn_drive_is_mounted (drive)
	     && !nautilus_burn_drive_unmount (drive)) {
		ret_error = g_error_new (BRASERO_BURN_ERROR,
					 BRASERO_BURN_ERROR_BUSY_DRIVE,
					 _("the drive seems to be busy"));
		result = BRASERO_BURN_ERR;
	}
	else {
		brasero_burn_get_task (burn);
		brasero_job_set_task (BRASERO_JOB (burn->priv->recorder),
				      burn->priv->task);
		result = brasero_recorder_record (burn->priv->recorder, &ret_error);
		brasero_job_set_task (BRASERO_JOB (burn->priv->recorder), NULL);
		g_object_unref (burn->priv->task);
		burn->priv->task = NULL;

		if (result == BRASERO_BURN_OK) {
			brasero_burn_action_changed_real (burn,
							  BRASERO_BURN_ACTION_FINISHED);
			g_signal_emit (burn,
				       brasero_burn_signals [PROGRESS_CHANGED_SIGNAL],
				       0,
				       1.0,
				       1.0,
				       -1);
		}
	}

	if (result != BRASERO_BURN_ERR || !ret_error) {
		if (ret_error)
			g_propagate_error (error, ret_error);

		return result;
	}

	/* see if error is recoverable */
	error_code = ret_error->code;
	if (error_code == BRASERO_BURN_ERROR_JOLIET_TREE) {
		/* NOTE: this error can only come from the source when 
		 * burning on the fly => no need to recreate an imager */

		/* some files are not conforming to Joliet standard see
		 * if the user wants to carry on with a non joliet disc */
		result = brasero_burn_ask_for_joliet (burn);
		if (result != BRASERO_BURN_OK) {
			g_propagate_error (error, ret_error);
			return result;
		}

		g_error_free (ret_error);
		ret_error = NULL;
		goto start;
	}
	else if (error_code == BRASERO_BURN_ERROR_MEDIA_BLANK) {
		/* NOTE: this error can only come from the source when 
		 * burning on the fly => no need to recreate an imager */

		/* The source media (when copying on the fly) is empty 
		 * so ask the user to reload another media with data */
		g_error_free (ret_error);
		ret_error = NULL;

		result = brasero_burn_reload_src_media (burn,
							error_code,
							flags,
							source,
							error);
		if (result != BRASERO_BURN_OK)
			return result;

		return BRASERO_BURN_RETRY;
	}
	else if (error_code == BRASERO_BURN_ERROR_SLOW_DMA) {
		/* The whole system has just made a great effort. Sometimes it 
		 * helps to let it rest for a sec or two => that's what we do
		 * before retrying. (That's why usually cdrecord waits a little
	         * bit but sometimes it doesn't). Another solution would be to
		 * lower the speed a little (we could do both) */
		g_error_free (ret_error);
		ret_error = NULL;

		brasero_burn_sleep (burn, 2000);
		has_slept = TRUE;

		/* set speed at 8x max and even less if speed  */
		if (speed <= 8) {
			speed = speed * 3 / 4;
			if (speed < 1)
				speed = 1;
		}
		else
			speed = 8;

		brasero_burn_set_recorder_speed (burn, speed);
		goto start;
	}
	else if (error_code >= BRASERO_BURN_ERROR_DISC_SPACE) {
		/* NOTE: these errors can only come from the dest drive */

		/* clean error and indicates this is a recoverable error */
		g_error_free (ret_error);
		ret_error = NULL;

		/* ask for the destination media reload */
		result = brasero_burn_reload_dest_media (burn,
							 error_code,
							 flags,
							 drive,
							 source, /* this is not an error since the required media depends on the source track */
							 error);

		if (result != BRASERO_BURN_OK)
			return result;

		return BRASERO_BURN_RETRY;
	}

	g_propagate_error (error, ret_error);
	return BRASERO_BURN_ERR;
}

static BraseroBurnResult
brasero_burn_imager_get_track (BraseroBurn *burn,
			       BraseroBurnFlag flags,
			       NautilusBurnDrive *drive,
			       const BraseroTrackSource *source,
			       BraseroTrackSource **track,
			       const gchar *output,
			       GError **error)
{
	GError *ret_error = NULL;
	BraseroBurnResult result;

	result = brasero_burn_get_imager (burn,
					  flags,
					  drive,
					  source,
					  output,
					  &ret_error);

	if (result != BRASERO_BURN_OK) {
		if (ret_error)
			g_propagate_error (error, ret_error);

		return result;
	}

	if (flags & BRASERO_BURN_FLAG_ON_THE_FLY) {
		BraseroTrackSource *source;

		source = g_new0 (BraseroTrackSource, 1);
		source->type = BRASERO_TRACK_SOURCE_IMAGER;
		source->contents.imager.obj = burn->priv->imager;
		burn->priv->imager = NULL;

		if (track)
			*track = source;

		return BRASERO_BURN_OK;
	}

	/* Since we are writing to disc we'd better check there is enough space */
	if (flags & BRASERO_BURN_FLAG_CHECK_SIZE)
		result = brasero_burn_check_volume_free_space (burn,
							       output,
							       &ret_error);

	if (result == BRASERO_BURN_OK)
		result = brasero_burn_run_imager (burn,
						  flags,
						  source,
						  track,
						  output,
						  &ret_error);

	/* propagate error if needed */
	if (ret_error)
		g_propagate_error (error, ret_error);

	return result;
}

static BraseroBurnResult
brasero_burn_get_size (BraseroBurn *burn,
		       BraseroBurnFlag flags,
		       const BraseroTrackSource *source,
		       GError **error)
{
	BraseroBurnResult result = BRASERO_BURN_OK;
	GnomeVFSFileInfo *info;
	GnomeVFSResult res;
	gchar *uri = NULL;

	switch (source->type) {
	case BRASERO_TRACK_SOURCE_INF:
		BRASERO_BURN_NOT_SUPPORTED_LOG (burn, flags);

	case BRASERO_TRACK_SOURCE_IMAGE:
		uri = g_strconcat ("file://", source->contents.image.image, NULL);
		break;

	default:
		BRASERO_BURN_NOT_SUPPORTED_LOG (burn, flags);
	}

	info = gnome_vfs_file_info_new ();
	res = gnome_vfs_get_file_info (uri, 
				       info,
				       GNOME_VFS_FILE_INFO_DEFAULT);

	burn->priv->image_size = info->size;

	gnome_vfs_file_info_unref (info);

	if (res != GNOME_VFS_OK) {
		gchar *name;

		BRASERO_GET_BASENAME_FOR_DISPLAY (uri, name);
		g_free (uri);

		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the file %s can't be opened (%s)"),
			     name,
			     gnome_vfs_result_to_string (res));
		burn->priv->image_size = -1;
		g_free (name);
		return BRASERO_BURN_ERR;
	}

	g_free (uri);
	return result;
}

static BraseroBurnResult
brasero_burn_lock_drives (BraseroBurn *burn,
			  BraseroBurnFlag flags,
			  NautilusBurnDrive *drive,
			  const BraseroTrackSource *source,
			  GError **error)
{
	BraseroBurnResult result;

	burn->priv->src_status = BRASERO_MEDIUM_NONE;
	burn->priv->dest_status = BRASERO_MEDIUM_NONE;

	nautilus_burn_drive_ref (drive);
	burn->priv->drive = drive;

	/* For source drive, the rule is if the source type is a disc, lock it
	 * and if source is not the same as dest, lock dest as well */
	if (source->type == BRASERO_TRACK_SOURCE_DISC) {
		result = brasero_burn_wait_for_source_media (burn,
							     source->contents.drive.disc,
							     (flags & BRASERO_BURN_FLAG_EJECT),
							     error);
		if (result != BRASERO_BURN_OK)
			return result;

		if (nautilus_burn_drive_equal (drive, source->contents.drive.disc)) {
			/* we can't lock the dest since src == dest we
			 * will ask the user to replace the disc later */
			return BRASERO_BURN_OK;
		}
	}

	/* we don't lock the dest drive if we just want an image of it,
	 * except if we append/merge/burn on the fly we need to make sure now
	 * there is a disc in the dest drive so as to lock it, because:
	 * - if it's a DVD we'll use it anyway
	 * - to append or merge cdr* programs need to know where to start the track */
	if (NCB_DRIVE_GET_TYPE (drive) == NAUTILUS_BURN_DRIVE_TYPE_FILE)
		return BRASERO_BURN_OK;

	/* lock the recorder */
	result = brasero_burn_wait_for_dest_media (burn,
						   flags,
						   drive,
						   source,
						   error);

	if (result != BRASERO_BURN_OK)
		return result;

again:

	if (burn->priv->dest_status & BRASERO_MEDIUM_REWRITABLE) {
		BraseroTrackSourceType type = BRASERO_TRACK_SOURCE_UNKNOWN;

		/* emits a warning for the user if it's a rewritable
		 * disc and he wants to write only audio tracks on it */
		result = BRASERO_BURN_OK;
		if (source->type == BRASERO_TRACK_SOURCE_DISC) {
			/* NOTE: no need to check the basics here since it  
			 * was done a little earlier */
			if ((burn->priv->dest_status & BRASERO_MEDIUM_HAS_AUDIO)
			&& !(burn->priv->dest_status & BRASERO_MEDIUM_HAS_DATA))
				type = BRASERO_TRACK_SOURCE_AUDIO;
			else
				type = BRASERO_TRACK_SOURCE_DISC;
		}
		else
			type = source->type;

		/* NOTE: no need to error out here since the only thing
		 * we are interested in is if it is AUDIO or not or if
		 * the disc we are copying has audio tracks only or not */
		if (result == BRASERO_BURN_OK
		&& (type == BRASERO_TRACK_SOURCE_AUDIO
		||  type == BRASERO_TRACK_SOURCE_INF
		||  type == BRASERO_TRACK_SOURCE_SONG)) {
			result = brasero_burn_emit_signal (burn, WARN_REWRITABLE_SIGNAL);
			if (result == BRASERO_BURN_NEED_RELOAD) {
				result = brasero_burn_reload_dest_media (burn,
									 BRASERO_BURN_ERROR_NONE,
									 flags,
									 drive,
									 source,
									 error);
				if (result != BRASERO_BURN_OK)
					return result;

				goto again;
			}
		}
	}

	return result;
}

static BraseroBurnResult
brasero_burn_unlock_drives (BraseroBurn *burn,
			    BraseroBurnFlag flags,
			    NautilusBurnDrive *drive,
			    const BraseroTrackSource *source)
{
	/* dest drive */
	nautilus_burn_drive_unref (burn->priv->drive);
	burn->priv->drive = NULL;

	nautilus_burn_drive_unlock (drive);
	g_object_set_data (G_OBJECT (drive), IS_LOCKED, GINT_TO_POINTER (0));

	if ((flags & BRASERO_BURN_FLAG_EJECT))
		brasero_burn_common_eject_async (drive);

	/* source drive if any */
	if (source->type != BRASERO_TRACK_SOURCE_DISC)
		return BRASERO_BURN_OK;

	if (nautilus_burn_drive_equal (drive, source->contents.drive.disc))
		return BRASERO_BURN_OK;

	nautilus_burn_drive_unlock (source->contents.drive.disc);
	g_object_set_data (G_OBJECT (source->contents.drive.disc),
			   IS_LOCKED,
			   GINT_TO_POINTER (0));

	if ((flags & BRASERO_BURN_FLAG_EJECT))
		brasero_burn_common_eject_async (source->contents.drive.disc);

	return BRASERO_BURN_OK;
}

/* FIXME: for the moment we don't allow for mixed CD type */
static BraseroBurnResult
brasero_burn_record_real (BraseroBurn *burn,
			  BraseroBurnFlag flags,
			  NautilusBurnDrive *drive,
			  gint speed,
			  const BraseroTrackSource *source,
			  const gchar *output,
			  GError **error)
{
	BraseroBurnResult result;
	BraseroTrackSource *track = NULL;

	burn->priv->on_the_fly = (flags & BRASERO_BURN_FLAG_ON_THE_FLY) ||
				 (source->type == BRASERO_TRACK_SOURCE_IMAGE);

	/* transform the source track through an imager if needed */
	if (source->type == BRASERO_TRACK_SOURCE_DATA
	||  source->type == BRASERO_TRACK_SOURCE_GRAFTS
	||  source->type == BRASERO_TRACK_SOURCE_SONG
	||  source->type == BRASERO_TRACK_SOURCE_DISC) {
		result = brasero_burn_imager_get_track (burn,
							flags,
							drive,
							source,
							&track,
							output,
							error);
		if (result != BRASERO_BURN_OK)
			goto end;
	}
	else {
		/* The track is ready to be used as is by a recorder */

		/* we get the size and (will) check that if we're writing to a disc,
		 * the media is big enough and/or if we're writing to a hard drive,
		 * the volume is big enough */
		result = brasero_burn_get_size (burn,
						flags,
						source,
						error);
		if (result != BRASERO_BURN_OK)
			goto end;

		track = brasero_track_source_copy (source);
	}

	if (NCB_DRIVE_GET_TYPE (drive) == NAUTILUS_BURN_DRIVE_TYPE_FILE)
		goto end;

	/* before recording if we are copying a disc check that the source and 
	 * the destination drive are not the same. Otherwise reload the media */
	if (source->type == BRASERO_TRACK_SOURCE_DISC
	&&  nautilus_burn_drive_equal (drive, source->contents.drive.disc)) {
		/* NOTE: we use track->contents.drive.disc here
		 * so as to keep the IS_LOCKED value consistent */
		if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (source->contents.drive.disc), IS_LOCKED)))
			g_object_set_data (G_OBJECT (drive), IS_LOCKED, GINT_TO_POINTER (1));
		else
			g_object_set_data (G_OBJECT (drive), IS_LOCKED, GINT_TO_POINTER (0));

		/* FIXME: we should put a message here saying that all went well */
		result = brasero_burn_reload_dest_media (burn,
							 BRASERO_BURN_ERROR_NONE,
							 flags,
							 drive,
							 source,
							 error);
		if (result != BRASERO_BURN_OK)
			goto end;
	}

	/* it happens with certain objects (cdrdao, growisofs) that they are both
	 * imagers and recorders so there's no need to recreate a recorder */
	if (track->type == BRASERO_TRACK_SOURCE_IMAGER
	&&  BRASERO_IS_RECORDER (track->contents.imager.obj)) {
		g_object_ref (track->contents.imager.obj);
		burn->priv->recorder = BRASERO_RECORDER (track->contents.imager.obj);
		result = brasero_burn_setup_recorder (burn,
						      flags,
						      drive,
						      speed,
						      NULL,
						      error);
	}
	else {
		result = brasero_burn_get_recorder (burn,
						    flags,
						    drive,
						    track,
						    speed,
						    error);
	}

	if (result == BRASERO_BURN_OK)
		result = brasero_burn_run_recorder (burn,
						    flags,
						    drive,
						    speed,
						    source,
						    track,
						    output,
						    error);


end:

	if (track && track != source)
		brasero_track_source_free (track);

	return result;
}

BraseroBurnResult 
brasero_burn_record (BraseroBurn *burn,
		     BraseroBurnFlag flags,
		     NautilusBurnDrive *drive,
		     gint speed,
		     const BraseroTrackSource *source,
		     const gchar *output,
		     GError **error)
{
	GError *ret_error = NULL;
	BraseroBurnResult result;

	g_return_val_if_fail (drive != NULL, BRASERO_BURN_ERR);
	
	if (source->type == BRASERO_TRACK_SOURCE_IMAGER)
		BRASERO_BURN_NOT_SUPPORTED_LOG (burn, flags);

	brasero_burn_log (burn,
			  flags,
			  "Session starting:\n"
			  "\tflags\t\t\t= %i \n"
			  "\tmedia type\t= %i\n"
			  "\tspeed\t\t= %i\n"
			  "\ttrack type\t\t= %i\n"
			  "\ttrack format\t= %i\n"
			  "\toutput\t\t= %s",
			  flags,
			  burn->priv->dest_status != BRASERO_MEDIUM_NONE ? burn->priv->dest_status:burn->priv->src_status,
			  speed,
			  source->type,
			  source->format,
			  output ? output:"none");

	/* we do some drive locking quite early to make sure we have a media
	 * in the drive so that we'll have all the necessary information */
	result = brasero_burn_lock_drives (burn,
					   flags,
					   drive,
					   source,
					   &ret_error);
	if (result != BRASERO_BURN_OK)
		goto end;

	burn->priv->task = brasero_task_new ();
	g_signal_connect (burn->priv->task,
			  "progress_changed",
			  G_CALLBACK (brasero_burn_progress_changed),
			  burn);
	g_signal_connect (burn->priv->task,
			  "action_changed",
			  G_CALLBACK (brasero_burn_action_changed),
			  burn);

	do {
		/* check flags consistency.
		 * NOTE: it's a necessary step when we retry since a supported 
		 * flag with one element could not be supported by its fallback */
		flags = brasero_burn_caps_check_flags_consistency (burn->priv->caps,
								   source,
								   drive,
								   flags);

		if (ret_error) {
			g_error_free (ret_error);
			ret_error = NULL;
		}

		result = brasero_burn_record_real (burn,
						   flags,
						   drive,
						   speed,
						   source,
						   output,
						   &ret_error);

		/* clean up */
		if (burn->priv->recorder) {
			g_object_unref (burn->priv->recorder);
			burn->priv->recorder = NULL;
		}

		if (burn->priv->imager) {
			g_object_unref (burn->priv->imager);
			burn->priv->imager = NULL;
		}
	} while (result == BRASERO_BURN_RETRY);


end:

	/* unlock all drives */
	brasero_burn_unlock_drives (burn, flags, drive, source);

	/* we handle all results/errors here*/
	if (ret_error) {
		g_propagate_error (error, ret_error);
		ret_error = NULL;
	}

	if (error && (*error) == NULL
	&& (result == BRASERO_BURN_NOT_READY
	||  result == BRASERO_BURN_NOT_SUPPORTED
	||  result == BRASERO_BURN_RUNNING
	||  result == BRASERO_BURN_NOT_RUNNING))
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("internal error (code %i)"),
			     result);

	if (result == BRASERO_BURN_CANCEL) {
		brasero_burn_log (burn,
				  flags,
				  "Session cancelled by user");
	}
	else if (result != BRASERO_BURN_OK) {
		if (error && (*error)) {
			brasero_burn_log (burn,
					  flags,
					  "Session error : %s",
					  (*error)->message);
		}
		else if (ret_error) {
			brasero_burn_log (burn,
					  flags,
					  "Session error : %s",
					  ret_error->message);
		}
		else
			brasero_burn_log (burn,
					  flags,
					  "Session error : unknown");
	}
	else
		brasero_burn_log (burn,
				  flags,
				  "Session successfully finished");

	return result;
}

BraseroBurnResult
brasero_burn_cancel (BraseroBurn *burn, gboolean protect)
{
	BraseroBurnResult result = BRASERO_BURN_OK;

	g_return_val_if_fail (burn != NULL, BRASERO_BURN_ERR);

	if (burn->priv->sleep_loop) {
		g_main_loop_quit (burn->priv->sleep_loop);
		burn->priv->sleep_loop = NULL;
	}

	if (burn->priv->recorder
	&&  brasero_job_is_running (BRASERO_JOB (burn->priv->recorder)))
		result = brasero_job_cancel (BRASERO_JOB (burn->priv->recorder), protect);
	else if (burn->priv->imager
	      &&  BRASERO_JOB (burn->priv->imager) != BRASERO_JOB (burn->priv->recorder))
		result = brasero_job_cancel (BRASERO_JOB (burn->priv->imager), protect);
	else if (burn->priv->recorder)
		result = brasero_job_cancel (BRASERO_JOB (burn->priv->recorder), protect);
	else if (burn->priv->imager)
		result = brasero_job_cancel (BRASERO_JOB (burn->priv->imager), protect);

	return result;
}
