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
#include <glib/gstdio.h>

#include <libgnomevfs/gnome-vfs.h>

#include <nautilus-burn-drive.h>

#include "brasero-marshal.h"
#include "burn-basics.h"
#include "burn-debug.h"
#include "burn-track.h"
#include "burn-session.h"
#include "burn.h"
#include "burn-session.h"
#include "burn-task-ctx.h"
#include "burn-task.h"
#include "burn-caps.h"
#include "burn-volume.h"
#include "brasero-ncb.h"

#ifdef BUILD_DBUS
  #include "burn-dbus.h"
#endif

G_DEFINE_TYPE (BraseroBurn, brasero_burn, G_TYPE_OBJECT);

typedef struct _BraseroBurnPrivate BraseroBurnPrivate;
struct _BraseroBurnPrivate {
	BraseroBurnCaps *caps;
	BraseroBurnSession *session;

	GMainLoop *sleep_loop;

	guint tasks_done;
	guint task_nb;
	BraseroTask *task;

	NautilusBurnDrive *src;
	NautilusBurnDrive *dest;

#ifdef BUILD_DBUS
	gint appcookie;
#endif

	guint src_locked:1;
	guint dest_locked:1;

	guint mounted_by_us:1;
};

#define BRASERO_BURN_NOT_SUPPORTED_LOG(burn)					\
	{									\
		brasero_burn_log (burn,						\
				  "unsupported operation (in %s at %s)",	\
				  G_STRFUNC,					\
				  G_STRLOC);					\
		return BRASERO_BURN_NOT_SUPPORTED;				\
	}

#define BRASERO_BURN_NOT_READY_LOG(burn)					\
	{									\
		brasero_burn_log (burn,						\
				  "not ready to operate (in %s at %s)",		\
				  G_STRFUNC,					\
				  G_STRLOC);					\
		return BRASERO_BURN_NOT_READY;					\
	}

#define BRASERO_BURN_DEBUG(burn, message, ...)					\
	{									\
		gchar *format;							\
		BRASERO_BURN_LOG (message, ##__VA_ARGS__);			\
		format = g_strdup_printf ("%s (%s %s)",				\
					  message,				\
					  G_STRFUNC,				\
					  G_STRLOC);				\
		brasero_burn_log (burn,						\
				  format,					\
				  ##__VA_ARGS__);				\
		g_free (format);						\
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
	DUMMY_SUCCESS_SIGNAL,
	LAST_SIGNAL
} BraseroBurnSignalType;

static guint brasero_burn_signals [LAST_SIGNAL] = { 0 };

#define BRASERO_BURN_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_BURN, BraseroBurnPrivate))

#define MAX_EJECT_WAIT_TIME	20000
#define MAX_MOUNT_ATTEMPS	10
#define MOUNT_TIMEOUT		500

static GObjectClass *parent_class = NULL;

#ifdef BUILD_DBUS

static void
brasero_burn_powermanagement (BraseroBurn *self,
			      gboolean wake)
{
	BraseroBurnPrivate *priv = BRASERO_BURN_PRIVATE (self);

	if (wake)
	  	priv->appcookie = brasero_inhibit_suspend (_("Burning CD/DVD"));
	else
		brasero_uninhibit_suspend (priv->appcookie); 
}

#endif

BraseroBurn *
brasero_burn_new ()
{
	BraseroBurn *obj;
	
	obj = BRASERO_BURN (g_object_new (BRASERO_TYPE_BURN, NULL));

	return obj;
}

static void
brasero_burn_log (BraseroBurn *burn,
		  const gchar *format,
		  ...)
{
	BraseroBurnPrivate *priv = BRASERO_BURN_PRIVATE (burn);
	va_list arg_list;

	va_start (arg_list, format);

	brasero_burn_session_logv (priv->session, format, arg_list);

	va_end (arg_list);
}

static BraseroBurnResult
brasero_burn_emit_signal (BraseroBurn *burn, guint signal, BraseroBurnResult default_answer)
{
	GValue instance_and_params;
	GValue return_value;

	instance_and_params.g_type = 0;
	g_value_init (&instance_and_params, G_TYPE_FROM_INSTANCE (burn));
	g_value_set_instance (&instance_and_params, burn);

	return_value.g_type = 0;
	g_value_init (&return_value, G_TYPE_INT);
	g_value_set_int (&return_value, default_answer);

	g_signal_emitv (&instance_and_params,
			brasero_burn_signals [signal],
			0,
			&return_value);

	g_value_unset (&instance_and_params);

	return g_value_get_int (&return_value);
}

static gboolean
brasero_burn_wakeup (BraseroBurn *burn)
{
	BraseroBurnPrivate *priv = BRASERO_BURN_PRIVATE (burn);

	if (priv->sleep_loop)
		g_main_loop_quit (priv->sleep_loop);

	return FALSE;
}

static BraseroBurnResult
brasero_burn_sleep (BraseroBurn *burn, gint msec)
{
	BraseroBurnPrivate *priv = BRASERO_BURN_PRIVATE (burn);

	priv->sleep_loop = g_main_loop_new (NULL, FALSE);
	g_timeout_add (msec,
		       (GSourceFunc) brasero_burn_wakeup,
		       burn);

	g_main_loop_run (priv->sleep_loop);

	if (priv->sleep_loop) {
		g_main_loop_unref (priv->sleep_loop);
		priv->sleep_loop = NULL;
		return BRASERO_BURN_OK;
	}

	/* if sleep_loop = NULL => We've been cancelled */
	return BRASERO_BURN_CANCEL;
}

static gpointer
_eject_async (gpointer data)
{
	NautilusBurnDrive *drive = NAUTILUS_BURN_DRIVE (data);

	nautilus_burn_drive_eject (drive);
	nautilus_burn_drive_unref (drive);

	return NULL;
}

static void
brasero_burn_eject_async (NautilusBurnDrive *drive)
{
	GError *error = NULL;

	BRASERO_BURN_LOG ("Asynchronous ejection");
	nautilus_burn_drive_ref (drive);
	g_thread_create (_eject_async, drive, FALSE, &error);
	if (error) {
		g_warning ("Could not create thread %s\n", error->message);
		g_error_free (error);

		nautilus_burn_drive_unref (drive);
		nautilus_burn_drive_eject (drive);
	}
}

static BraseroBurnResult
brasero_burn_eject_dest_media (BraseroBurn *self,
			       GError **error)
{
	BraseroBurnPrivate *priv;
	guint elapsed = 0;

	priv = BRASERO_BURN_PRIVATE (self);

	BRASERO_BURN_LOG ("Ejecting destination disc");
	if (!priv->dest)
		return BRASERO_BURN_OK;

	if (nautilus_burn_drive_is_mounted (priv->dest))
		nautilus_burn_drive_unmount (priv->dest);

	if (priv->dest_locked) {
		priv->dest_locked = 0;
		if (!nautilus_burn_drive_unlock (priv->dest)) {
			gchar *name;

			name = nautilus_burn_drive_get_name_for_display (priv->dest);
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("\"%s\" can't be unlocked"),
				     name);
			g_free (name);
			return BRASERO_BURN_ERR;
		}
	}

	brasero_burn_eject_async (priv->dest);

	/* sleep here to make sure that we got time to eject */
	while (NCB_MEDIA_GET_STATUS (priv->dest) != BRASERO_MEDIUM_NONE) {
		brasero_burn_sleep (self, 500);
		elapsed += 500;

		if (elapsed > MAX_EJECT_WAIT_TIME)
			break;
	}

	if (NCB_MEDIA_GET_STATUS (priv->dest) != BRASERO_MEDIUM_NONE) {
		gchar *name;

		name = nautilus_burn_drive_get_name_for_display (priv->dest);

		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the media in %s can't be ejected"),
			     name);

		g_free (name);

		priv->dest = NULL;
		return BRASERO_BURN_ERR;
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_eject_src_media (BraseroBurn *self,
			      GError **error)
{
	BraseroBurnPrivate *priv;
	guint elapsed = 0;

	priv = BRASERO_BURN_PRIVATE (self);

	if (!priv->src)
		return BRASERO_BURN_OK;

	if (nautilus_burn_drive_is_mounted (priv->src)) {
		BraseroBurnResult result;

		result = NCB_DRIVE_UNMOUNT (priv->src, error);
		if (result != BRASERO_BURN_OK)
			return result;
	}

	if (priv->src_locked) {
		priv->src_locked = 0;
		if (!nautilus_burn_drive_unlock (priv->src)) {
			gchar *name;

			name = nautilus_burn_drive_get_name_for_display (priv->src);
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("\"%s\" can't be unlocked"),
				     name);
			g_free (name);
			return BRASERO_BURN_ERR;
		}
	}

	brasero_burn_eject_async (priv->src);

	/* sleep here to make sure that we got time to eject */
	while (NCB_MEDIA_GET_STATUS (priv->src) != BRASERO_MEDIUM_NONE) {
		brasero_burn_sleep (self, 500);
		elapsed += 500;

		if (elapsed > MAX_EJECT_WAIT_TIME)
			break;
	}

	if (NCB_MEDIA_GET_STATUS (priv->src) != BRASERO_MEDIUM_NONE) {
		gchar *name;

		name = nautilus_burn_drive_get_name_for_display (priv->src);

		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the media in %s can't be ejected"),
			     name);

		g_free (name);

		priv->src = NULL;
		return BRASERO_BURN_ERR;
	}

	priv->src = NULL;
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_ask_for_media (BraseroBurn *burn,
			    NautilusBurnDrive *drive,
			    BraseroBurnError error_type,
			    BraseroMedia required_media,
			    GError **error)
{
	GValue instance_and_params [4];
	GValue return_value;

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
brasero_burn_ask_for_src_media (BraseroBurn *burn,
				BraseroBurnError error_type,
				BraseroMedia required_media,
				GError **error)
{
	BraseroMedia media;
	BraseroBurnPrivate *priv = BRASERO_BURN_PRIVATE (burn);

	media = NCB_MEDIA_GET_STATUS (priv->src);
	if (media != BRASERO_MEDIUM_NONE) {
		BraseroBurnResult result;
		result = brasero_burn_eject_src_media (burn, error);
		if (result != BRASERO_BURN_OK)
			return result;
	}

	return brasero_burn_ask_for_media (burn,
					   priv->src,
					   error_type,
					   required_media,
					   error);
}

static BraseroBurnResult
brasero_burn_ask_for_dest_media (BraseroBurn *burn,
				 BraseroBurnError error_type,
				 BraseroMedia required_media,
				 GError **error)
{
	BraseroMedia media;
	BraseroBurnPrivate *priv = BRASERO_BURN_PRIVATE (burn);

	media = NCB_MEDIA_GET_STATUS (priv->dest);
	if (media != BRASERO_MEDIUM_NONE) {
		BraseroBurnResult result;

		result = brasero_burn_eject_dest_media (burn, error);
		if (result != BRASERO_BURN_OK)
			return result;
	}

	return brasero_burn_ask_for_media (burn,
					   priv->dest,
					   error_type,
					   required_media,
					   error);
}

static BraseroBurnResult
brasero_burn_lock_src_media (BraseroBurn *burn,
			     GError **error)
{
	gchar *failure;
	BraseroMedia media;
	BraseroBurnResult result;
	BraseroBurnError error_type;
	BraseroBurnPrivate *priv = BRASERO_BURN_PRIVATE (burn);

	priv->src = brasero_burn_session_get_src_drive (priv->session);
	if (!priv->src) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("no drive specified as source"));
		return BRASERO_BURN_ERR;
	}


again:
	if (nautilus_burn_drive_is_mounted (priv->src)) {
		if (!NCB_DRIVE_UNMOUNT (priv->src, NULL))
			g_warning ("Couldn't unmount volume in drive: %s",
				   NCB_DRIVE_GET_DEVICE (priv->src));
	}

	/* NOTE: we used to unmount the media before now we shouldn't need that
	 * get any information from the drive */
	media = NCB_MEDIA_GET_STATUS (priv->src);
	if (media == BRASERO_MEDIUM_NONE)
		error_type = BRASERO_BURN_ERROR_MEDIA_NONE;
	else if (media == BRASERO_MEDIUM_BUSY)
		error_type = BRASERO_BURN_ERROR_MEDIA_BUSY;
	else if (media == BRASERO_MEDIUM_UNSUPPORTED)
		error_type = BRASERO_BURN_ERROR_MEDIA_UNSUPPORTED;
	else if (media & BRASERO_MEDIUM_BLANK)
		error_type = BRASERO_BURN_ERROR_MEDIA_BLANK;
	else
		error_type = BRASERO_BURN_ERROR_NONE;

	if (media & BRASERO_MEDIUM_BLANK) {
		result = brasero_burn_ask_for_src_media (burn,
							 BRASERO_BURN_ERROR_MEDIA_BLANK,
							 BRASERO_MEDIUM_HAS_DATA,
							 error);
		if (result != BRASERO_BURN_OK)
			return result;

		goto again;
	}

	if (!priv->src_locked
	&&  !nautilus_burn_drive_lock (priv->src, _("ongoing copying process"), &failure)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the drive can't be locked (%s)"),
			     failure);
		return BRASERO_BURN_ERR;
	}

	priv->src_locked = 1;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_reload_src_media (BraseroBurn *burn,
			       BraseroBurnError error_code,
			       GError **error)
{
	BraseroBurnResult result;

	result = brasero_burn_ask_for_src_media (burn,
						 error_code,
						 BRASERO_MEDIUM_HAS_DATA,
						 error);
	if (result != BRASERO_BURN_OK)
		return result;

	result = brasero_burn_lock_src_media (burn, error);
	return result;
}

static BraseroBurnResult
brasero_burn_lock_rewritable_media (BraseroBurn *burn,
					GError **error)
{
	gchar *failure;
	BraseroMedia media;
	BraseroBurnResult result;
	BraseroBurnError error_type;
	BraseroBurnPrivate *priv = BRASERO_BURN_PRIVATE (burn);

	priv->dest = brasero_burn_session_get_burner (priv->session);
	if (!priv->dest) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("no drive specified"));
		return BRASERO_BURN_NOT_SUPPORTED;
	}

	if (!nautilus_burn_drive_can_rewrite (priv->dest)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the drive has no rewriting capabilities"));
		return BRASERO_BURN_NOT_SUPPORTED;
	}

 again:

	if (nautilus_burn_drive_is_mounted (priv->dest)) {
		if (!NCB_DRIVE_UNMOUNT (priv->dest, NULL))
			g_warning ("Couldn't unmount volume in drive: %s",
				   NCB_DRIVE_GET_DEVICE (priv->dest));
	}

	media = NCB_MEDIA_GET_STATUS (priv->dest);
	if (media == BRASERO_MEDIUM_NONE)
		error_type = BRASERO_BURN_ERROR_MEDIA_NONE;
	else if (media == BRASERO_MEDIUM_BUSY)
		error_type = BRASERO_BURN_ERROR_MEDIA_BUSY;
	else if (media == BRASERO_MEDIUM_UNSUPPORTED)
		error_type = BRASERO_BURN_ERROR_MEDIA_UNSUPPORTED;
	else if (!(media & BRASERO_MEDIUM_REWRITABLE))
		error_type = BRASERO_BURN_ERROR_MEDIA_NOT_REWRITABLE;
	else
		error_type = BRASERO_BURN_ERROR_NONE;

	if (error_type != BRASERO_BURN_ERROR_NONE) {
		result = brasero_burn_ask_for_dest_media (burn,
							  error_type,
							  BRASERO_MEDIUM_REWRITABLE|
							  BRASERO_MEDIUM_HAS_DATA,
							  error);

		if (result != BRASERO_BURN_OK)
			return result;

		goto again;
	}

	if (!priv->dest_locked
	&&  !nautilus_burn_drive_lock (priv->dest, _("ongoing blanking process"), &failure)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the drive can't be locked (%s)"),
			     failure);
		return BRASERO_BURN_ERR;
	}

	priv->dest_locked = 1;

	return BRASERO_BURN_OK;
}

/**
 * must_blank indicates whether we'll have to blank the disc before writing 
 * either because it was requested or because we have no choice (the disc can be
 * appended but is rewritable
 */
static BraseroBurnResult
brasero_burn_is_loaded_dest_media_supported (BraseroBurn *burn,
					     BraseroMedia media,
					     gboolean *must_blank)
{
	BraseroMedia required_media;
	BraseroBurnPrivate *priv;
	BraseroBurnResult result;
	BraseroMedia unsupported;
	BraseroTrackType output;
	BraseroBurnFlag flags;
	BraseroMedia missing;

	priv = BRASERO_BURN_PRIVATE (burn);

	/* make sure that media is supported */
	output.type = BRASERO_TRACK_TYPE_DISC;
	output.subtype.media = media;

	result = brasero_burn_caps_is_output_supported (priv->caps,
							priv->session,
							&output);

	flags = brasero_burn_session_get_flags (priv->session);

	if (result == BRASERO_BURN_OK) {
		/* NOTE: this flag is only supported when the media has some
		 * data and/or audio and when we can blank it */
		if (!(flags & BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE))
			*must_blank = FALSE;
		else if (!(flags & (BRASERO_MEDIUM_HAS_AUDIO|BRASERO_MEDIUM_HAS_DATA)))
			*must_blank = FALSE;
		else
			*must_blank = TRUE;
		return BRASERO_BURN_ERROR_NONE;
	}

	if (!(flags & BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE)) {
		*must_blank = FALSE;
		return BRASERO_BURN_ERROR_MEDIA_UNSUPPORTED;
	}

	/* let's see what our media is missing and what's not supported */
	required_media = brasero_burn_caps_get_required_media_type (priv->caps,
								    priv->session);
	missing = required_media & (~media);
	unsupported = media & (~required_media);

	if (missing & (BRASERO_MEDIUM_BLANK|BRASERO_MEDIUM_APPENDABLE)) {
		/* there is a special case if the disc is rewritable */
		if ((media & BRASERO_MEDIUM_REWRITABLE)
		&&   brasero_burn_caps_can_blank (priv->caps, priv->session) == BRASERO_BURN_OK) {
			*must_blank = TRUE;
			return BRASERO_BURN_ERROR_NONE;
		}

		return BRASERO_BURN_ERROR_MEDIA_NOT_WRITABLE;
	}
	else if (unsupported & BRASERO_MEDIUM_DVD)
		return BRASERO_BURN_ERROR_DVD_NOT_SUPPORTED;

	return BRASERO_BURN_ERROR_MEDIA_UNSUPPORTED;
}

static BraseroBurnResult
brasero_burn_lock_dest_media (BraseroBurn *burn, GError **error)
{
	gchar *failure;
	BraseroMedia media;
	gboolean must_blank;
	BraseroBurnFlag flags;
	BraseroTrackType input;
	BraseroBurnError berror;
	BraseroBurnResult result;
	BraseroBurnPrivate *priv = BRASERO_BURN_PRIVATE (burn);

	priv->dest = brasero_burn_session_get_burner (priv->session);
	if (!priv->dest) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("no drive specified"));
		return BRASERO_BURN_ERR;
	}

	brasero_burn_session_get_input_type (priv->session, &input);
	flags = brasero_burn_session_get_flags (priv->session);
	if (!nautilus_burn_drive_can_write (priv->dest)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the drive has no burning capabilities"));
		BRASERO_BURN_NOT_SUPPORTED_LOG (burn);
	}
	result = BRASERO_BURN_OK;

again:

	/* if drive is mounted then unmount before checking anything */
	if (nautilus_burn_drive_is_mounted (priv->dest)) {
		if (!NCB_DRIVE_UNMOUNT (priv->dest, NULL))
			g_warning ("Couldn't unmount volume in drive: %s",
				   NCB_DRIVE_GET_DEVICE (priv->dest));
	}

	berror = BRASERO_BURN_ERROR_NONE;
	media = NCB_MEDIA_GET_STATUS (priv->dest);

	BRASERO_BURN_LOG_WITH_FULL_TYPE (BRASERO_TRACK_TYPE_DISC,
					 media,
					 BRASERO_PLUGIN_IO_NONE,
					 "Waiting for dest drive");

	if (priv->dest_locked) {
		/* NOTE: after a blanking, for nautilus_burn the CD/DVD is still
		 * full of data so if the drive has already been checked there
		 * is no need to do that again since we would be asked if we 
		 * want to blank it again */
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

	if (media == BRASERO_MEDIUM_BUSY) {
		result = BRASERO_BURN_NEED_RELOAD;
		berror = BRASERO_BURN_ERROR_MEDIA_BUSY;
		goto end;
	}

	/* make sure that media is supported and can be written to */
	berror = brasero_burn_is_loaded_dest_media_supported (burn,
							      media,
							      &must_blank);

	if (berror != BRASERO_BURN_ERROR_NONE) {
		BRASERO_BURN_LOG ("the media is not supported");
		result = BRASERO_BURN_NEED_RELOAD;
		goto end;
	}

	if (must_blank) {
		/* There is an error if APPEND was set since this disc is not
		 * supported without a prior blanking. */

		
		/* we warn the user is going to lose data even if in the case of
		 * DVD+/-RW we don't really blank the disc we rather overwrite */
		result = brasero_burn_emit_signal (burn, WARN_DATA_LOSS_SIGNAL, BRASERO_BURN_CANCEL);
		if (result != BRASERO_BURN_OK)
			goto end;
	}
	else if (media & (BRASERO_MEDIUM_HAS_DATA|BRASERO_MEDIUM_HAS_AUDIO)) {
		/* A few special warnings for the discs with data/audio on them
		 * that don't need prior blanking or can't be blanked */
		if (input.type == BRASERO_TRACK_TYPE_AUDIO) {
			/* We'd rather blank and rewrite a disc rather than
			 * append audio to appendable disc. That's because audio
			 * tracks have little chance to be readable by common CD
			 * player as last tracks */
			result = brasero_burn_emit_signal (burn, WARN_AUDIO_TO_APPENDABLE_SIGNAL, BRASERO_BURN_CANCEL);
			if (result != BRASERO_BURN_OK)
				goto end;
		}

		/* NOTE: if input is AUDIO we don't care since the OS
		 * will load the last session of DATA anyway */
		if ((media & BRASERO_MEDIUM_HAS_DATA)
		&&   input.type == BRASERO_TRACK_TYPE_DATA
		&& !(flags & BRASERO_BURN_FLAG_MERGE)) {
			/* warn the users that their previous data
			 * session (s) will not be mounted by default by
			 * the OS and that it'll be invisible */
			result = brasero_burn_emit_signal (burn, WARN_PREVIOUS_SESSION_LOSS_SIGNAL, BRASERO_BURN_CANCEL);
			if (result != BRASERO_BURN_OK)
				goto end;
		}
	}

	if (media & BRASERO_MEDIUM_REWRITABLE) {
		/* emits a warning for the user if it's a rewritable
		 * disc and he wants to write only audio tracks on it */

		/* NOTE: no need to error out here since the only thing
		 * we are interested in is if it is AUDIO or not or if
		 * the disc we are copying has audio tracks only or not */
		if (input.type == BRASERO_TRACK_TYPE_AUDIO) {
			result = brasero_burn_emit_signal (burn, WARN_REWRITABLE_SIGNAL, BRASERO_BURN_CANCEL);
			if (result != BRASERO_BURN_OK)
				goto end;
		}

		/* FIXME: if NO_TMP_FILE is not set then the warning won't get
		 * emitted */
		if (input.type == BRASERO_TRACK_TYPE_DISC
		&& (input.subtype.media & (BRASERO_MEDIUM_HAS_AUDIO|BRASERO_MEDIUM_HAS_DATA)) == BRASERO_MEDIUM_HAS_AUDIO) {
			result = brasero_burn_emit_signal (burn, WARN_REWRITABLE_SIGNAL, BRASERO_BURN_CANCEL);
			if (result != BRASERO_BURN_OK)
				goto end;
		}
	}

	if (!priv->dest_locked
	&&  !nautilus_burn_drive_lock (priv->dest, _("ongoing burning process"), &failure)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the drive can't be locked (%s)"),
			     failure);
		return BRASERO_BURN_ERR;
	}

	priv->dest_locked = 1;

end:

	if (result == BRASERO_BURN_NEED_RELOAD) {
		BraseroMedia required_media;

		required_media = brasero_burn_caps_get_required_media_type (priv->caps,
									    priv->session);

		result = brasero_burn_ask_for_dest_media (burn,
							  berror,
							  required_media,
							  error);
		if (result == BRASERO_BURN_OK)
			goto again;
	}

	if (result != BRASERO_BURN_OK) {
		priv->dest_locked = 0;
		nautilus_burn_drive_unlock (priv->dest);
	}

	return result;
}

static BraseroBurnResult
brasero_burn_reload_dest_media (BraseroBurn *burn,
				BraseroBurnError error_code,
				GError **error)
{
	BraseroBurnPrivate *priv = BRASERO_BURN_PRIVATE (burn);
	BraseroMedia required_media;
	BraseroBurnResult result;

again:

	/* eject and ask the user to reload a disc */
	required_media = brasero_burn_caps_get_required_media_type (priv->caps,
								    priv->session);
	required_media &= (BRASERO_MEDIUM_WRITABLE|BRASERO_MEDIUM_CD|BRASERO_MEDIUM_DVD);

	result = brasero_burn_ask_for_dest_media (burn,
						  error_code,
						  required_media,
						  error);
	if (result != BRASERO_BURN_OK)
		return result;

	result = brasero_burn_lock_dest_media (burn, error);
	if (result == BRASERO_BURN_NEED_RELOAD)
		goto again;

	return result;
}

static BraseroBurnResult
brasero_burn_mount_media (BraseroBurn *self,
			  GError **error)
{
	guint retries = 0;
	BraseroBurnPrivate *priv;

	priv = BRASERO_BURN_PRIVATE (self);

	/* get the mount point */
	g_signal_emit (self,
		       brasero_burn_signals [ACTION_CHANGED_SIGNAL],
		       0,
		       BRASERO_BURN_ACTION_CHECKSUM);

	while (!nautilus_burn_drive_is_mounted (priv->dest)) {
		if (retries++ > MAX_MOUNT_ATTEMPS) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("the disc could not be mounted (max attemps reached)"));
			return BRASERO_BURN_ERR;
		}

		/* NOTE: we don't really care about the return value */
		NCB_DRIVE_MOUNT (priv->dest, error);
		priv->mounted_by_us = TRUE;

		brasero_burn_sleep (self, MOUNT_TIMEOUT);
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_lock_checksum_media (BraseroBurn *burn,
				  GError **error)
{
	gchar *failure;
	BraseroMedia media;
	BraseroBurnResult result;
	BraseroBurnError error_type;
	BraseroBurnPrivate *priv = BRASERO_BURN_PRIVATE (burn);

	priv->dest = brasero_burn_session_get_src_drive (priv->session);

again:

	media = NCB_MEDIA_GET_STATUS (priv->dest);
	error_type = BRASERO_BURN_ERROR_NONE;
	BRASERO_BURN_LOG_DISC_TYPE (media, "Waiting for media to checksum");

	if (media == BRASERO_MEDIUM_NONE) {
		/* NOTE: that's done on purpose since here if the drive is empty
		 * that's because we ejected it */
		result = brasero_burn_ask_for_dest_media (burn,
							  BRASERO_BURN_WARNING_CHECKSUM,
							  BRASERO_MEDIUM_NONE,
							  error);

		if (result != BRASERO_BURN_OK)
			return result;
	}
	else if (media == BRASERO_MEDIUM_BUSY)
		error_type = BRASERO_BURN_ERROR_MEDIA_BUSY;
	else if (media == BRASERO_MEDIUM_UNSUPPORTED)
		error_type = BRASERO_BURN_ERROR_MEDIA_UNSUPPORTED;
	else if (media & BRASERO_MEDIUM_BLANK)
		error_type = BRASERO_BURN_ERROR_MEDIA_BLANK;

	if (error_type != BRASERO_BURN_ERROR_NONE) {
		result = brasero_burn_ask_for_dest_media (burn,
							  BRASERO_BURN_WARNING_CHECKSUM,
							  BRASERO_MEDIUM_NONE,
							  error);
		if (result != BRASERO_BURN_OK)
			return result;

		goto again;
	}

	if (!priv->dest_locked
	&&  !nautilus_burn_drive_lock (priv->dest, _("ongoing checksuming operation"), &failure)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the drive can't be locked (%s)"),
			     failure);
		return BRASERO_BURN_ERR;
	}

	priv->dest_locked = 1;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_unlock_src_media (BraseroBurn *burn)
{
	BraseroBurnPrivate *priv = BRASERO_BURN_PRIVATE (burn);

	if (!priv->src)
		return BRASERO_BURN_OK;

	if (!priv->src_locked) {
		priv->src = NULL;
		return BRASERO_BURN_OK;
	}

	if (priv->mounted_by_us) {
		nautilus_burn_drive_unmount (priv->src);
		priv->mounted_by_us = 0;
	}

	priv->src_locked = 0;
	nautilus_burn_drive_unlock (priv->src);

	if (BRASERO_BURN_SESSION_EJECT (priv->session))
		brasero_burn_eject_async (priv->src);

	priv->src = NULL;
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_unlock_dest_media (BraseroBurn *burn)
{
	BraseroBurnPrivate *priv = BRASERO_BURN_PRIVATE (burn);

	if (!priv->dest)
		return BRASERO_BURN_OK;

	if (!priv->dest_locked) {
		priv->dest = NULL;
		return BRASERO_BURN_OK;
	}

	priv->dest_locked = 0;
	nautilus_burn_drive_unlock (priv->dest);

	if (BRASERO_BURN_SESSION_EJECT (priv->session))
		brasero_burn_eject_async (priv->dest);

	priv->dest = NULL;
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_unlock_medias (BraseroBurn *burn)
{
	brasero_burn_unlock_dest_media (burn);
	brasero_burn_unlock_src_media (burn);

	return BRASERO_BURN_OK;
}

static void
brasero_burn_progress_changed (BraseroTaskCtx *task,
			       BraseroBurn *burn)
{
	BraseroBurnPrivate *priv = BRASERO_BURN_PRIVATE (burn);
	BraseroBurnAction action = BRASERO_BURN_ACTION_NONE;
	gdouble overall_progress = -1.0;
	gdouble task_progress = -1.0;
	glong time_remaining = -1;

	brasero_task_ctx_get_current_action (task, &action);

	/* get the task current progress */
	if (brasero_task_ctx_get_progress (task, &task_progress) == BRASERO_BURN_OK) {
		brasero_task_ctx_get_remaining_time (task, &time_remaining);
		overall_progress = (task_progress + (gdouble) priv->tasks_done) /
				   (gdouble) priv->task_nb;
	}
	else
		overall_progress =  (gdouble) priv->tasks_done / (gdouble) priv->task_nb;

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

void
brasero_burn_get_action_string (BraseroBurn *burn,
				BraseroBurnAction action,
				gchar **string)
{
	BraseroBurnPrivate *priv;

	g_return_if_fail (BRASERO_BURN (burn));
	g_return_if_fail (string != NULL);

	priv = BRASERO_BURN_PRIVATE (burn);
	if (action == BRASERO_BURN_ACTION_FINISHED || !priv->task)
		(*string) = g_strdup (brasero_burn_action_to_string (action));
	else
		brasero_task_ctx_get_current_action_string (BRASERO_TASK_CTX (priv->task),
							    action,
							    string);
}

BraseroBurnResult
brasero_burn_status (BraseroBurn *burn,
		     BraseroMedia *media,
		     gint64 *isosize,
		     gint64 *written,
		     gint64 *rate)
{
	BraseroBurnPrivate *priv;

	g_return_val_if_fail (BRASERO_BURN (burn), BRASERO_BURN_ERR);
	
	priv = BRASERO_BURN_PRIVATE (burn);
	if (!priv->task || !brasero_task_is_running (priv->task))
		return BRASERO_BURN_NOT_READY;

	if (rate)
		brasero_task_ctx_get_rate (BRASERO_TASK_CTX (priv->task), rate);

	if (isosize)
		brasero_task_ctx_get_session_output_size (BRASERO_TASK_CTX (priv->task), NULL, isosize);

	if (written)
		brasero_task_ctx_get_written (BRASERO_TASK_CTX (priv->task), written);

	if (!media)
		return BRASERO_BURN_OK;

	/* return the disc we burn to if:
	 * - that's the last task to perform
	 * - brasero_burn_session_is_dest_file returns FALSE
	 */
	if (priv->tasks_done < priv->task_nb - 1) {
		BraseroTrackType input;

		brasero_burn_session_get_input_type (priv->session, &input);
		if (input.type == BRASERO_TRACK_TYPE_DISC)
			*media = input.subtype.media;
		else
			*media = BRASERO_MEDIUM_NONE;
	}
	else if (brasero_burn_session_is_dest_file (priv->session))
		*media = BRASERO_MEDIUM_FILE;
	else
		*media = brasero_burn_session_get_dest_media (priv->session);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_ask_for_joliet (BraseroBurn *burn)
{
	BraseroBurnPrivate *priv = BRASERO_BURN_PRIVATE (burn);
	BraseroBurnResult result;
	GSList *tracks;
	GSList *iter;

	result = brasero_burn_emit_signal (burn, ASK_DISABLE_JOLIET_SIGNAL, BRASERO_BURN_CANCEL);
	if (result != BRASERO_BURN_OK)
		return result;

	tracks = brasero_burn_session_get_tracks (priv->session);
	for (iter = tracks; iter; iter = iter->next) {
		BraseroTrack *track;

		track = iter->data;
		brasero_track_unset_data_fs (track, BRASERO_IMAGE_FS_JOLIET);
	}

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_burn_run_eraser (BraseroBurn *burn, GError **error)
{
	NautilusBurnDrive *drive;
	BraseroBurnPrivate *priv;

	priv = BRASERO_BURN_PRIVATE (burn);

	drive = brasero_burn_session_get_burner (priv->session);
	if (nautilus_burn_drive_is_mounted (drive)
	&& !NCB_DRIVE_UNMOUNT (drive, NULL)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_BUSY_DRIVE,
			     _("the drive seems to be busy"));
		return BRASERO_BURN_ERR;
	}

	return brasero_task_run (priv->task, error);
}

static BraseroBurnResult
brasero_burn_run_imager (BraseroBurn *burn,
			 gboolean fake,
			 GError **error)
{
	BraseroBurnPrivate *priv = BRASERO_BURN_PRIVATE (burn);
	BraseroBurnError error_code;
	BraseroBurnResult result;
	GError *ret_error = NULL;
	NautilusBurnDrive *src;

	src = brasero_burn_session_get_src_drive (priv->session);

start:

	/* this is just in case */
	if (src
	&&  nautilus_burn_drive_is_mounted (src)
	&& !NCB_DRIVE_UNMOUNT (src, NULL)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_BUSY_DRIVE,
			     _("the drive seems to be busy"));
		return BRASERO_BURN_ERR;
	}

	/* if it succeeds then the new track(s) will be at the top of
	 * session tracks stack and therefore usable by the recorder.
	 * NOTE: it's up to the job to push the current tracks. */
	if (fake)
		result = brasero_task_check (priv->task, &ret_error);
	else
		result = brasero_task_run (priv->task, &ret_error);

	if (result == BRASERO_BURN_OK) {
		if (!fake) {
			g_signal_emit (burn,
				       brasero_burn_signals [PROGRESS_CHANGED_SIGNAL],
				       0,
				       1.0,
				       1.0,
				       -1);
		}
		return BRASERO_BURN_OK;
	}

	if (result != BRASERO_BURN_ERR) {
		g_propagate_error (error, ret_error);
		return result;
	}

	if (!ret_error)
		return result;

	/* See if we can recover from the error */
	error_code = ret_error->code;
	if (error_code == BRASERO_BURN_ERROR_JOLIET_TREE) {
		/* clean the error anyway since at worst the user will cancel */
		g_error_free (ret_error);
		ret_error = NULL;

		/* some files are not conforming to Joliet standard see
		 * if the user wants to carry on with a non joliet disc */
		result = brasero_burn_ask_for_joliet (burn);
		if (result != BRASERO_BURN_OK)
			return result;

		goto start;
	}
	else if (error_code == BRASERO_BURN_ERROR_MEDIA_BLANK) {
		/* clean the error anyway since at worst the user will cancel */
		g_error_free (ret_error);
		ret_error = NULL;

		/* The media hasn't data on it: ask for a new one. */
		result = brasero_burn_reload_src_media (burn,
							error_code,
							error);
		if (result != BRASERO_BURN_OK)
			return result;

		goto start;
	}
	else if (error_code == BRASERO_BURN_ERROR_MEDIA_SPACE) {
		/* clean the error anyway since at worst the user will cancel */
		g_error_free (ret_error);
		ret_error = NULL;

		/* the space left on the media is insufficient */
		result = brasero_burn_reload_dest_media (burn, error_code, error);
		if (result != BRASERO_BURN_OK)
			return result;

		goto start;
	}

	/* not recoverable propagate the error */
	g_propagate_error (error, ret_error);
	return BRASERO_BURN_ERR;
}

static BraseroBurnResult
brasero_burn_run_recorder (BraseroBurn *burn, GError **error)
{
	gint error_code;
	gboolean has_slept;
	NautilusBurnDrive *src;
	GError *ret_error = NULL;
	BraseroBurnResult result;
	NautilusBurnDrive *burner;
	BraseroBurnPrivate *priv = BRASERO_BURN_PRIVATE (burn);

	has_slept = FALSE;
	src = brasero_burn_session_get_src_drive (priv->session);
	burner = brasero_burn_session_get_burner (priv->session);

start:

	/* this is just in case */
	if (BRASERO_BURN_SESSION_NO_TMP_FILE (priv->session)
	&&  src
	&&  nautilus_burn_drive_is_mounted (src)
	&& !NCB_DRIVE_UNMOUNT (src, NULL)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_BUSY_DRIVE,
			     _("the drive seems to be busy"));
		return BRASERO_BURN_ERR;
	}
	else if (nautilus_burn_drive_is_mounted (burner)
	     && !NCB_DRIVE_UNMOUNT (burner, NULL)) {
		ret_error = g_error_new (BRASERO_BURN_ERROR,
					 BRASERO_BURN_ERROR_BUSY_DRIVE,
					 _("the drive seems to be busy"));
		result = BRASERO_BURN_ERR;
	}

	/* actual running of task */
	result = brasero_task_run (priv->task, &ret_error);

	/* let's see the results */
	if (result == BRASERO_BURN_OK) {
		g_signal_emit (burn,
			       brasero_burn_signals [PROGRESS_CHANGED_SIGNAL],
			       0,
			       1.0,
			       1.0,
			       -1);
		return BRASERO_BURN_OK;
	}

	if (result != BRASERO_BURN_ERR
	|| !ret_error
	||  ret_error->domain != BRASERO_BURN_ERROR) {
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
							error);
		if (result != BRASERO_BURN_OK)
			return result;

		goto start;
	}
	else if (error_code == BRASERO_BURN_ERROR_SLOW_DMA) {
		guint64 rate;

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
		rate = brasero_burn_session_get_rate (priv->session);
		if (rate <= BRASERO_SPEED_TO_RATE_CD (8)) {
			rate = rate * 3 / 4;
			if (rate < CD_RATE)
				rate = CD_RATE;
		}
		else
			rate = BRASERO_SPEED_TO_RATE_CD (8);

		brasero_burn_session_set_rate (priv->session, rate);
		goto start;
	}
	else if (error_code >= BRASERO_BURN_ERROR_MEDIA_SPACE
	     &&  error_code <  BRASERO_BURN_ERROR_CD_NOT_SUPPORTED) {
		/* NOTE: these errors can only come from the dest drive */

		/* clean error and indicates this is a recoverable error */
		g_error_free (ret_error);
		ret_error = NULL;

		/* ask for the destination media reload */
		result = brasero_burn_reload_dest_media (burn,
							 error_code,
							 error);

		if (result != BRASERO_BURN_OK)
			return result;

		goto start;
	}

	g_propagate_error (error, ret_error);
	return BRASERO_BURN_ERR;
}

/* FIXME: for the moment we don't allow for mixed CD type */
static BraseroBurnResult
brasero_burn_run_tasks (BraseroBurn *burn,
			gboolean erase_allowed,
			GError **error)
{
	BraseroBurnResult result;
	GSList *tasks, *next, *iter;
	BraseroBurnPrivate *priv = BRASERO_BURN_PRIVATE (burn);

	tasks = brasero_burn_caps_new_task (priv->caps,
					    priv->session,
					    error);
	if (!tasks)
		return BRASERO_BURN_NOT_SUPPORTED;

	priv->tasks_done = 0;
	priv->task_nb = g_slist_length (tasks);
	BRASERO_BURN_LOG ("%i tasks to perform", priv->task_nb);

	/* run all imaging tasks first */
	for (iter = tasks; iter; iter = next) {
		BraseroTaskAction action;

		next = iter->next;
		priv->task = iter->data;
		tasks = g_slist_remove (tasks, priv->task);

		g_signal_connect (priv->task,
				  "progress-changed",
				  G_CALLBACK (brasero_burn_progress_changed),
				  burn);
		g_signal_connect (priv->task,
				  "action-changed",
				  G_CALLBACK (brasero_burn_action_changed),
				  burn);

		/* see what type of task it is. It could be a blank/erase one */
		action = brasero_task_ctx_get_action (BRASERO_TASK_CTX (priv->task));
		if (action == BRASERO_TASK_ACTION_ERASE) {
			/* this is to avoid a potential problem when running a 
			 * dummy session first. When running dummy session the 
			 * media gets erased if need be. Since it is not
			 * reloaded afterwards, for brasero it has still got 
			 * data on it when we get to the real recording. */
			if (erase_allowed) {
				result = brasero_burn_run_eraser (burn, error);
				if (result != BRASERO_BURN_OK)
					break;
			}
			else
				result = BRASERO_BURN_OK;

			g_object_unref (priv->task);
			priv->task = NULL;
			priv->tasks_done ++;
			continue;
		}

		/* Init the task and set the task output size. The task should
		 * then check that the disc has enough space. If the output is
		 * to the hard drive it will be done afterwards when not in fake
		 * mode. */
		result = brasero_burn_run_imager (burn, TRUE, error);
		if (result != BRASERO_BURN_OK)
			break;

		/* see if we reached a recording task: it's the last task */
		if (!next) {
			if (brasero_burn_session_is_dest_file (priv->session))
				result = brasero_burn_run_imager (burn, FALSE, error);
			else
				result = brasero_burn_run_recorder (burn, error);

			if (result == BRASERO_BURN_OK)
				priv->tasks_done ++;

			break;
		}

		/* run the imager */
		result = brasero_burn_run_imager (burn, FALSE, error);
		if (result != BRASERO_BURN_OK)
			break;

		g_object_unref (priv->task);
		priv->task = NULL;
		priv->tasks_done ++;
	}

	if (priv->task) {
		g_object_unref (priv->task);
		priv->task = NULL;
	}

	g_slist_foreach (tasks, (GFunc) g_object_unref, NULL);
	g_slist_free (tasks);

	return result;
}

static BraseroBurnResult
brasero_burn_check_real (BraseroBurn *self,
			 GError **error)
{
	GSList *tracks;
	BraseroTrack *track;
	BraseroTrackType type;
	BraseroBurnResult result;
	BraseroBurnPrivate *priv;
	BraseroChecksumType checksum_type;

	priv = BRASERO_BURN_PRIVATE (self);

	BRASERO_BURN_LOG ("Starting to check track integrity");

	/* NOTE: no need to check for parameters here;
	 * that'll be done when asking for a task */
	tracks = brasero_burn_session_get_tracks (priv->session);
	if (g_slist_length (tracks) != 1) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("only one track at a time can be checked"));
		return BRASERO_BURN_ERR;
	}

	track = tracks->data;

	checksum_type = brasero_track_get_checksum_type (track);
	brasero_track_get_type (track, &type);

	/* if the input is a DISC, ask/mount/unmount and lock it (as dest) */
	if (type.type == BRASERO_TRACK_TYPE_DISC) {
		/* make sure there is a disc. If not, ask one and lock it */
		result = brasero_burn_lock_checksum_media (self, error);
		if (result != BRASERO_BURN_OK)
			return result;

		if (checksum_type == BRASERO_CHECKSUM_MD5_FILE
		&& !nautilus_burn_drive_is_mounted (priv->dest)) {
			result = brasero_burn_mount_media (self, error);
			if (result != BRASERO_BURN_OK)
				return result;
		}
	}

	/* re-ask for the input type (it depends on the media) once loaded */
	brasero_track_get_type (track, &type);

	/* get the task and run it */
	priv->task = brasero_burn_caps_new_checksuming_task (priv->caps,
							     priv->session,
							     error);
	if (priv->task) {
		priv->task_nb = 1;
		priv->tasks_done = 0;
		g_signal_connect (priv->task,
				  "progress-changed",
				  G_CALLBACK (brasero_burn_progress_changed),
				  self);
		g_signal_connect (priv->task,
				  "action-changed",
				  G_CALLBACK (brasero_burn_action_changed),
				  self);

		result = brasero_task_run (priv->task, error);

		if (result == BRASERO_BURN_OK) {
			brasero_burn_action_changed_real (self,
							  BRASERO_BURN_ACTION_FINISHED);
			g_signal_emit (self,
				       brasero_burn_signals [PROGRESS_CHANGED_SIGNAL],
				       0,
				       1.0,
				       1.0,
				       -1);
		}

		g_object_unref (priv->task);
		priv->task = NULL;
	}
	else {
		BRASERO_BURN_LOG ("the track can't be checked");
		result = BRASERO_BURN_NOT_SUPPORTED;
	}

	return result;
}

static BraseroBurnResult
brasero_burn_check_session_consistency (BraseroBurn *burn,
					GError **error)
{
	BraseroTrackType type;
	BraseroBurnFlag flags;
	BraseroBurnFlag retval;
	BraseroBurnResult result;
	BraseroBurnFlag supported = BRASERO_BURN_FLAG_NONE;
	BraseroBurnFlag compulsory = BRASERO_BURN_FLAG_NONE;
	BraseroBurnPrivate *priv = BRASERO_BURN_PRIVATE (burn);

	/* make sure there is a session, a burner */
	brasero_burn_session_get_input_type (priv->session, &type);
	if (type.type == BRASERO_TRACK_TYPE_NONE
	|| !brasero_burn_session_get_tracks (priv->session)) {
		BRASERO_BURN_DEBUG (burn, "No track set");
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("there is no track to be burn"));
		return BRASERO_BURN_ERR;
	}

	/* make sure there is a drive set as burner */
	if (!brasero_burn_session_is_dest_file (priv->session)) {
		NautilusBurnDrive *burner;

		burner = brasero_burn_session_get_burner (priv->session);
		if (!burner) {
			BRASERO_BURN_DEBUG (burn, "No drive set");
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("there is no drive to burn to"));
			return BRASERO_BURN_ERR;	
		}
	}
	else {
		/* check number of copies must be 1 */
		if (brasero_burn_session_get_num_copies (priv->session) != 1)
			brasero_burn_session_set_num_copies (priv->session, 1);
	}

	/* make sure all the flags given are supported if not correct them */
	result = brasero_burn_caps_get_flags (priv->caps,
					      priv->session,
					      &supported,
					      &compulsory);
	if (result != BRASERO_BURN_OK) {
		g_set_error (error,
			    BRASERO_BURN_ERROR,
			    BRASERO_BURN_ERROR_GENERAL,
			    _("this operation is not supported. Try to activate proper plugins"));
		return result;
	}

	flags = brasero_burn_session_get_flags (priv->session);
	retval = flags & supported;

	if ((flags & BRASERO_BURN_FLAG_MERGE)
	&& !(retval & BRASERO_BURN_FLAG_MERGE)) {
		/* we pay attention to one flag in particular (MERGE) if it was
		 * set then it must be supported. Otherwise error out. */
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("merging data is impossible with this disc"));
		return BRASERO_BURN_ERR;
	}

	if (retval != flags)
		BRASERO_BURN_DEBUG (burn,
				    "Some flags were not supported (%i => %i). Corrected",
				    flags,
				    retval);

	if (retval != (retval | compulsory)) {
		BRASERO_BURN_DEBUG (burn,
				    "Some compulsory flags were forgotten (%i => %i). Corrected",
				   (retval & compulsory),
				    compulsory);

		retval |= compulsory;
	}

	/* we check flags consistency 
	 * NOTE: should we return an error if they are not consistent? */
	brasero_burn_session_get_input_type (priv->session, &type);
	if ((type.type != BRASERO_TRACK_TYPE_AUDIO
	&&   type.type != BRASERO_TRACK_TYPE_DATA
	&&   type.type != BRASERO_TRACK_TYPE_DISC)
	||   brasero_burn_session_is_dest_file (priv->session)) {
		if (retval & BRASERO_BURN_FLAG_MERGE) {
			BRASERO_BURN_DEBUG (burn, "Inconsistent flag: you can't use flag merge");
			retval &= ~BRASERO_BURN_FLAG_MERGE;
		}
			
		if (retval & BRASERO_BURN_FLAG_APPEND) {
			BRASERO_BURN_DEBUG (burn, "Inconsistent flags: you can't use flag append");
			retval &= ~BRASERO_BURN_FLAG_APPEND;
		}

		if (retval & BRASERO_BURN_FLAG_NO_TMP_FILES) {
			BRASERO_BURN_DEBUG (burn, "Inconsistent flag: you can't use flag on_the_fly");
			retval &= ~BRASERO_BURN_FLAG_NO_TMP_FILES;
		}
	}

	if ((retval & (BRASERO_BURN_FLAG_MERGE|BRASERO_BURN_FLAG_APPEND)) != 0
	&&  (retval & BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE) != 0) {
		BRASERO_BURN_DEBUG (burn, "Inconsistent flag: you can't use flag blank_before_write");
		retval &= ~BRASERO_BURN_FLAG_BLANK_BEFORE_WRITE;
	}

	/* if we want to leave the session open with DVD+/-R we can't use dao */
	if ((brasero_burn_session_get_dest_media (priv->session) & BRASERO_MEDIUM_DVD)
	&&  (flags & BRASERO_BURN_FLAG_MULTI)
	&&  (flags & BRASERO_BURN_FLAG_DAO)) {
		BRASERO_BURN_DEBUG (burn, "DAO flag can't be used to create multisession DVD+/-R");
		retval &= ~BRASERO_BURN_FLAG_DAO;
	}

	if (brasero_burn_session_is_dest_file (priv->session)
	&& (retval & BRASERO_BURN_FLAG_DONT_CLEAN_OUTPUT) == 0) {
		BRASERO_BURN_DEBUG (burn, "Forgotten flag: you must use flag dont_clean_output");
		retval |= BRASERO_BURN_FLAG_DONT_CLEAN_OUTPUT;
	}

	brasero_burn_session_set_flags (priv->session, retval);
	return BRASERO_BURN_OK;
}

static void
brasero_burn_unset_checksums (BraseroBurn *self)
{
	GSList *tracks;
	BraseroBurnPrivate *priv;

	priv = BRASERO_BURN_PRIVATE (self);

	tracks = brasero_burn_session_get_tracks (priv->session);
	for (; tracks; tracks = tracks->next) {
		BraseroTrack *track;

		/* unset checksum (might depend from copy to another). */
		track = tracks->data;
		brasero_track_set_checksum (track,
					    BRASERO_CHECKSUM_NONE,
					    NULL);
	}
}

static BraseroBurnResult
brasero_burn_record_session (BraseroBurn *burn,
			     gboolean erase_allowed,
			     GError **error)
{
	BraseroTrack *track = NULL;
	BraseroChecksumType type;
	BraseroBurnPrivate *priv;
	BraseroBurnResult result;
	GError *ret_error = NULL;
	GSList *tracks;

	priv = BRASERO_BURN_PRIVATE (burn);

	/* unset checksum since no image has the exact same even if it is 
	 * created from the same files */
	brasero_burn_unset_checksums (burn);

	do {
		/* push the session settings to keep the original session untainted */
		brasero_burn_session_push_settings (priv->session);

		/* check flags consistency */
		result = brasero_burn_check_session_consistency (burn, error);
		if (result != BRASERO_BURN_OK) {
			brasero_burn_session_pop_settings (priv->session);
			break;
		}

		if (ret_error) {
			g_error_free (ret_error);
			ret_error = NULL;
		}

		result = brasero_burn_run_tasks (burn,
						 erase_allowed,
						 &ret_error);

		/* restore the session settings */
		brasero_burn_session_pop_settings (priv->session);
	} while (result == BRASERO_BURN_RETRY);

	if (result != BRASERO_BURN_OK) {
		/* handle errors */
		if (ret_error) {
			g_propagate_error (error, ret_error);
			ret_error = NULL;
		}

		return result;
	}

	/* recording was successful, so tell it */
	brasero_burn_action_changed_real (burn, BRASERO_BURN_ACTION_FINISHED);

	if (brasero_burn_session_get_flags (priv->session) & BRASERO_BURN_FLAG_DUMMY) {
		/* if we are in dummy mode and successfully completed then:
		 * - no need to checksum the media afterward (done later)
		 * - no eject to have automatic real burning */
	
		BRASERO_BURN_DEBUG (burn, "Dummy session successfully finished");

		/* need to try again but this time for real */
		result = brasero_burn_emit_signal (burn,
						   DUMMY_SUCCESS_SIGNAL,
						   BRASERO_BURN_OK);
		if (result != BRASERO_BURN_OK)
			return result;

		/* unset checksum since no image has the exact same even if it
		 * is created from the same files */
		brasero_burn_unset_checksums (burn);

		/* remove dummy flag and restart real burning calling ourselves
		 * NOTE: don't bother to push the session. We know the changes 
		 * that were made. */
		brasero_burn_session_remove_flag (priv->session, BRASERO_BURN_FLAG_DUMMY);
		result = brasero_burn_record_session (burn, FALSE, error);
		brasero_burn_session_add_flag (priv->session, BRASERO_BURN_FLAG_DUMMY);

		return result;
	}

	/* see if we have a checksum generated for the session if so use
	 * it to check if the recording went well remaining on the top of
	 * the session should be the last track burnt/imaged */
	tracks = brasero_burn_session_get_tracks (priv->session);
	if (g_slist_length (tracks) != 1)
		return BRASERO_BURN_OK;

	track = tracks->data;
	type = brasero_track_get_checksum_type (track);
	if (type == BRASERO_CHECKSUM_NONE)
		return BRASERO_BURN_OK;

	/* unlock dest drive that's necessary if we want to check burnt medias
	 * it seems that the kernel caches its contents and can't/don't update
	 * its caches after a blanking/recording. */
	/* NOTE: that work if the disc had not been mounted before. That's the 
	 * mount that triggers the caches. So maybe if the disc was blank (and
	 * therefore couldn't have been previously mounted) we could skip that
	 * unlock/eject step. A better way would be to have a system call to 
	 * force a re-load. */

	result = brasero_burn_eject_dest_media (burn, error);
	if (result != BRASERO_BURN_OK)
		return result;

	priv->dest = NULL;

	if (type == BRASERO_CHECKSUM_MD5) {
		const gchar *checksum = NULL;

		checksum = brasero_track_get_checksum (track);

		/* the idea is to push a new track on the stack with
		 * the current disc burnt and the checksum generated
		 * during the session recording */

		track = brasero_track_new (BRASERO_TRACK_TYPE_DISC);
		brasero_track_set_checksum (track, type, checksum);
	}
	else if (type == BRASERO_CHECKSUM_MD5_FILE) {
		track = brasero_track_new (BRASERO_TRACK_TYPE_DISC);
		brasero_track_set_checksum (track,
					    BRASERO_CHECKSUM_MD5_FILE,
					    BRASERO_MD5_FILE);
	}

	brasero_burn_session_push_tracks (priv->session);

	brasero_track_set_drive_source (track, brasero_burn_session_get_burner (priv->session));
	brasero_burn_session_add_track (priv->session, track);

	result = brasero_burn_check_real (burn, error);
	brasero_burn_session_pop_tracks (priv->session);

	if (result == BRASERO_BURN_CANCEL) {
		/* change the result value so we won't stop here if there are 
		 * other copies to be made */
		result = BRASERO_BURN_OK;
	}

	return result;
}

BraseroBurnResult
brasero_burn_check (BraseroBurn *self,
		    BraseroBurnSession *session,
		    GError **error)
{
	BraseroBurnResult result;
	BraseroBurnPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_BURN (self), BRASERO_BURN_ERR);
	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (session), BRASERO_BURN_ERR);

	priv = BRASERO_BURN_PRIVATE (self);

	g_object_ref (session);
	priv->session = session;

#ifdef BUILD_DBUS
	brasero_burn_powermanagement (self, TRUE);
#endif

	result = brasero_burn_check_real (self, error);

	brasero_burn_unlock_medias (self);


#ifdef BUILD_DBUS
	brasero_burn_powermanagement (self, FALSE);
#endif

	/* no need to check the result of the comparison, it's set in session */
	priv->session = NULL;
	g_object_unref (session);

	return result;
}

static BraseroBurnResult
brasero_burn_same_src_dest (BraseroBurn *self,
			    GError **error)
{
	gchar *toc = NULL;
	gchar *image = NULL;
	BraseroTrack *track;
	BraseroTrackType output;
	BraseroBurnResult result;
	BraseroBurnPrivate *priv;
	BraseroImageFormat format;

	/* we can't create a proper list of tasks here since we don't know the
	 * dest media type yet. So we try to find an intermediate image type and
	 * add it to the session as output */
	priv = BRASERO_BURN_PRIVATE (self);

	/* get the first possible format */
	output.type = BRASERO_TRACK_TYPE_IMAGE;
	format = BRASERO_IMAGE_FORMAT_CDRDAO;
	for (; format != BRASERO_IMAGE_FORMAT_NONE; format >>= 1) {
		output.subtype.img_format = format;
		result = brasero_burn_caps_is_output_supported (priv->caps,
								priv->session,
								&output);
		if (result == BRASERO_BURN_OK)
			break;
	}

	if (format == BRASERO_IMAGE_FORMAT_NONE) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("impossible to find a format for the temporary image"));
		return BRASERO_BURN_ERR;
	}

	/* get a new output. Also ask for both  */
	brasero_burn_session_push_settings (priv->session);
	result = brasero_burn_session_get_tmp_image (priv->session,
						     format,
						     &image,
						     &toc,
						     error);
	if (result != BRASERO_BURN_OK)
		return result;

	/* some, like cdrdao, can't overwrite the files */
	g_remove (image);
	g_remove (toc);

	result = brasero_burn_session_set_image_output_full (priv->session,
							     format,
							     image,
							     toc);
	if (result != BRASERO_BURN_OK)
		return result;

	/* lock drive */
	result = brasero_burn_lock_src_media (self, error);
	if (result != BRASERO_BURN_OK)
		goto end;

	/* run */
	result = brasero_burn_record_session (self, TRUE, error);
	if (result != BRASERO_BURN_OK) {
		brasero_burn_unlock_src_media (self);
		goto end;
	}

	/* reset everything back to normal */
	result = brasero_burn_eject_src_media (self, error);
	if (result != BRASERO_BURN_OK)
		goto end;

	track = brasero_track_new (BRASERO_TRACK_TYPE_IMAGE);
	brasero_track_set_image_source (track, image, toc, format);
	brasero_burn_session_add_track (priv->session, track);

end:
	g_free (image);
	g_free (toc);

	brasero_burn_session_pop_settings (priv->session);

	return result;
}

BraseroBurnResult 
brasero_burn_record (BraseroBurn *burn,
		     BraseroBurnSession *session,
		     GError **error)
{
	BraseroBurnResult result;
	BraseroBurnPrivate *priv;

	g_return_val_if_fail (BRASERO_IS_BURN (burn), BRASERO_BURN_ERR);
	g_return_val_if_fail (BRASERO_IS_BURN_SESSION (session), BRASERO_BURN_ERR);

	priv = BRASERO_BURN_PRIVATE (burn);

	g_object_ref (session);
	priv->session = session;

#ifdef BUILD_DBUS
	brasero_burn_powermanagement (burn, TRUE);
#endif

	/* say to the whole world we started */
	brasero_burn_action_changed_real (burn, BRASERO_BURN_ACTION_PREPARING);

	if (brasero_burn_session_same_src_dest_drive (session)) {
		/* This is a special case */
		result = brasero_burn_same_src_dest (burn, error);
		if (result != BRASERO_BURN_OK)
			goto end;
	}

	/* do some drive locking quite early to make sure we have a media
	 * in the drive so that we'll have all the necessary information */
	if (!brasero_burn_session_is_dest_file (session)) {
		result = brasero_burn_lock_dest_media (burn, error);
		if (result != BRASERO_BURN_OK)
			goto end;
	}

	if (brasero_burn_session_get_input_type (session, NULL) == BRASERO_TRACK_TYPE_DISC) {
		result = brasero_burn_lock_src_media (burn, error);
		if (result != BRASERO_BURN_OK)
			goto end;
	}

	/* burn the session a first time whatever the number of copies required except if dummy session */
	result = brasero_burn_record_session (burn, TRUE, error);
	if (result == BRASERO_BURN_OK) {
		gint num_copies;

		/* burn all other required copies */
		num_copies = brasero_burn_session_get_num_copies (session);
		while (--num_copies > 0 && result == BRASERO_BURN_OK) {
			BRASERO_BURN_LOG ("Burning additional copies (%i left)",
					  num_copies);

			/* we only need to reload and lock dest media */
			result = brasero_burn_reload_dest_media (burn,
								 BRASERO_BURN_WARNING_NEXT_COPY,
								 error);
			if (result != BRASERO_BURN_OK)
				break;

			/* see if we still need it to be locked */
			if (brasero_burn_session_get_input_type (session, NULL) != BRASERO_TRACK_TYPE_DISC)
				brasero_burn_unlock_src_media (burn);

			result = brasero_burn_record_session (burn, TRUE, error);
			if (result != BRASERO_BURN_OK)
				break;
		}
	}

end:

	brasero_burn_unlock_medias (burn);

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
		BRASERO_BURN_DEBUG (burn, "Session cancelled by user");
	}
	else if (result != BRASERO_BURN_OK) {
		if (error && (*error)) {
			BRASERO_BURN_DEBUG (burn,
					    "Session error : %s",
					    (*error)->message);
		}
		else
			BRASERO_BURN_DEBUG (burn, "Session error : unknown");
	}
	else
		BRASERO_BURN_DEBUG (burn, "Session successfully finished");

#ifdef BUILD_DBUS
	brasero_burn_powermanagement (burn, FALSE);
#endif

	/* release session */
	g_object_unref (priv->session);
	priv->session = NULL;

	return result;
}

static BraseroBurnResult
brasero_burn_blank_real (BraseroBurn *burn, GError **error)
{
	BraseroBurnResult result;
	BraseroBurnPrivate *priv;

	priv = BRASERO_BURN_PRIVATE (burn);

	priv->task = brasero_burn_caps_new_blanking_task (priv->caps,
							  priv->session,
							  error);
	if (!priv->task)
		return BRASERO_BURN_NOT_SUPPORTED;

	g_signal_connect (priv->task,
			  "progress-changed",
			  G_CALLBACK (brasero_burn_progress_changed),
			  burn);
	g_signal_connect (priv->task,
			  "action-changed",
			  G_CALLBACK (brasero_burn_action_changed),
			  burn);

	result = brasero_burn_run_eraser (burn, error);
	g_object_unref (priv->task);
	priv->task = NULL;

	if (result == BRASERO_BURN_OK)
		brasero_burn_action_changed_real (burn, BRASERO_BURN_ACTION_FINISHED);

	return result;
}

BraseroBurnResult
brasero_burn_blank (BraseroBurn *burn,
		    BraseroBurnSession *session,
		    GError **error)
{
	BraseroBurnPrivate *priv;
	BraseroBurnResult result;
	GError *ret_error = NULL;

	g_return_val_if_fail (burn != NULL, BRASERO_BURN_ERR);
	g_return_val_if_fail (session != NULL, BRASERO_BURN_ERR);

	priv = BRASERO_BURN_PRIVATE (burn);

	g_object_ref (session);
	priv->session = session;

#ifdef BUILD_DBUS
	brasero_burn_powermanagement (burn, TRUE);
#endif

	/* we wait for the insertion of a media and lock it */
	result = brasero_burn_lock_rewritable_media (burn, error);
	if (result != BRASERO_BURN_OK)
		goto end;

	result = brasero_burn_blank_real (burn, &ret_error);
	while (result == BRASERO_BURN_ERR
	&&     ret_error
	&&     ret_error->code == BRASERO_BURN_ERROR_MEDIA_NOT_REWRITABLE) {
		g_error_free (ret_error);
		ret_error = NULL;

		result = brasero_burn_ask_for_dest_media (burn,
							  BRASERO_BURN_ERROR_MEDIA_NOT_REWRITABLE,
							  BRASERO_MEDIUM_REWRITABLE|
							  BRASERO_MEDIUM_HAS_DATA,
							  error);
		if (result != BRASERO_BURN_OK)
			break;

		result = brasero_burn_lock_rewritable_media (burn, error);
		if (result != BRASERO_BURN_OK)
			break;

		result = brasero_burn_blank_real (burn, &ret_error);
	}

end:
	if (ret_error)
		g_propagate_error (error, ret_error);

	brasero_burn_unlock_medias (burn);

	if (result == BRASERO_BURN_OK)
		brasero_burn_action_changed_real (burn, BRASERO_BURN_ACTION_FINISHED);


#ifdef BUILD_DBUS
	brasero_burn_powermanagement (burn, FALSE);
#endif

	/* release session */
	g_object_unref (priv->session);
	priv->session = NULL;

	return result;
}

BraseroBurnResult
brasero_burn_cancel (BraseroBurn *burn, gboolean protect)
{
	BraseroBurnResult result = BRASERO_BURN_OK;
	BraseroBurnPrivate *priv;

	g_return_val_if_fail (BRASERO_BURN (burn), BRASERO_BURN_ERR);

	priv = BRASERO_BURN_PRIVATE (burn);

	if (priv->sleep_loop) {
		g_main_loop_quit (priv->sleep_loop);
		priv->sleep_loop = NULL;
	}

	if (priv->task && brasero_task_is_running (priv->task))
		result = brasero_task_cancel (priv->task, protect);

	return result;
}

static void
brasero_burn_finalize (GObject *object)
{
	BraseroBurnPrivate *priv = BRASERO_BURN_PRIVATE (object);

	if (priv->sleep_loop) {
		g_main_loop_quit (priv->sleep_loop);
		priv->sleep_loop = NULL;
	}

	if (priv->task) {
		g_object_unref (priv->task);
		priv->task = NULL;
	}

	if (priv->session) {
		g_object_unref (priv->session);
		priv->session = NULL;
	}

	if (priv->caps)
		g_object_unref (priv->caps);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_burn_class_init (BraseroBurnClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroBurnPrivate));

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
        brasero_burn_signals [DUMMY_SUCCESS_SIGNAL] =
		g_signal_new ("dummy_success",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (BraseroBurnClass,
					       dummy_success),
			      NULL, NULL,
			      brasero_marshal_INT__VOID,
			      G_TYPE_INT, 0);
}

static void
brasero_burn_init (BraseroBurn *obj)
{
	BraseroBurnPrivate *priv = BRASERO_BURN_PRIVATE (obj);

	priv->caps = brasero_burn_caps_get_default ();
}
