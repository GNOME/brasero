/***************************************************************************
 *            cdrecord.c
 *
 *  dim jan 22 15:22:52 2006
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

#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <nautilus-burn-drive.h>

#include "burn-basics.h"
#include "burn-common.h"
#include "burn-cdrecord.h"
#include "burn-process.h"
#include "burn-recorder.h"
#include "burn-imager.h"
#include "brasero-ncb.h"

static void brasero_cdrecord_class_init (BraseroCDRecordClass *klass);
static void brasero_cdrecord_init (BraseroCDRecord *sp);
static void brasero_cdrecord_finalize (GObject *object);
static void brasero_cdrecord_iface_init_record (BraseroRecorderIFace *iface);

static BraseroBurnResult
brasero_cdrecord_stderr_read (BraseroProcess *process,
			      const char *line);
static BraseroBurnResult
brasero_cdrecord_stdout_read (BraseroProcess *process,
			      const char *line);
static BraseroBurnResult
brasero_cdrecord_set_argv (BraseroProcess *process,
			   GPtrArray *argv,
			   gboolean has_master, 
			   GError **error);
static BraseroBurnResult
brasero_cdrecord_post (BraseroProcess *process,
		       BraseroBurnResult retval);

static BraseroBurnResult
brasero_cdrecord_set_drive (BraseroRecorder *recorder,
			    NautilusBurnDrive *drive,
			    GError **error);
static BraseroBurnResult
brasero_cdrecord_set_source (BraseroJob *job,
			     const BraseroTrackSource *track,
			     GError **error);
static BraseroBurnResult
brasero_cdrecord_set_flags (BraseroRecorder *recorder,
			    BraseroRecorderFlag flags,
			    GError **error);
static BraseroBurnResult
brasero_cdrecord_set_rate (BraseroJob *job,
			   gint64 rate);

static BraseroBurnResult
brasero_cdrecord_record (BraseroRecorder *recorder,
			 GError **error);
static BraseroBurnResult
brasero_cdrecord_blank (BraseroRecorder *recorder,
			GError **error);

typedef enum {
	BRASERO_CD_RECORD_ACTION_NONE,
	BRASERO_CD_RECORD_ACTION_BLANK,
	BRASERO_CD_RECORD_ACTION_RECORD
} BraseroCDRecordAction;

struct BraseroCDRecordPrivate {
	BraseroCDRecordAction action;

	NautilusBurnDrive *drive;
	BraseroTrackSource *track;
	GSList *infs;
	gint rate;

	gchar *tmpdir;

	gint64 current_track_end_pos;
	gint64 current_track_written;
	gint64 tracks_total_bytes;

	gint current_track_num;
	gint track_count;

	gint minbuf;

	gint dao:1;
	gint dummy:1;
	gint multi:1;
	gint nograce:1;
	gint overburn:1;
	gint immediate:1;
	gint burnproof:1;

	gint blank_fast:1;
};

static GObjectClass *parent_class = NULL;

GType
brasero_cdrecord_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroCDRecordClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_cdrecord_class_init,
			NULL,
			NULL,
			sizeof (BraseroCDRecord),
			0,
			(GInstanceInitFunc)brasero_cdrecord_init,
		};

		static const GInterfaceInfo recorder_info =
		{
			(GInterfaceInitFunc) brasero_cdrecord_iface_init_record,
			NULL,
			NULL
		};

		type = g_type_register_static (BRASERO_TYPE_PROCESS, 
					       "BraseroCDRecord", 
					       &our_info,
					       0);
		g_type_add_interface_static (type,
					     BRASERO_TYPE_RECORDER,
					     &recorder_info);
	}

	return type;
}

static void
brasero_cdrecord_class_init (BraseroCDRecordClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroJobClass *job_class = BRASERO_JOB_CLASS (klass);
	BraseroProcessClass *process_class = BRASERO_PROCESS_CLASS (klass);

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_cdrecord_finalize;

	job_class->set_source = brasero_cdrecord_set_source;
	job_class->set_rate = brasero_cdrecord_set_rate;

	process_class->stderr_func = brasero_cdrecord_stderr_read;
	process_class->stdout_func = brasero_cdrecord_stdout_read;
	process_class->set_argv = brasero_cdrecord_set_argv;
	process_class->post = brasero_cdrecord_post;
}

static void
brasero_cdrecord_iface_init_record (BraseroRecorderIFace *iface)
{
	iface->blank = brasero_cdrecord_blank;
	iface->record = brasero_cdrecord_record;
	iface->set_drive = brasero_cdrecord_set_drive;
	iface->set_flags = brasero_cdrecord_set_flags;
}

static void
brasero_cdrecord_init (BraseroCDRecord *obj)
{
	obj->priv = g_new0 (BraseroCDRecordPrivate, 1);
}

static void
brasero_cdrecord_stop_real (BraseroCDRecord *cdrecord)
{
	brasero_job_set_slave (BRASERO_JOB (cdrecord), NULL);

	if (cdrecord->priv->infs) {
		g_slist_foreach (cdrecord->priv->infs,
				 (GFunc) g_remove,
				 NULL);
		g_slist_foreach (cdrecord->priv->infs,
				 (GFunc) g_free,
				 NULL);
		g_slist_free (cdrecord->priv->infs);
		cdrecord->priv->infs = NULL;
	}

	if (cdrecord->priv->tmpdir) {
		g_remove (cdrecord->priv->tmpdir);
		g_free (cdrecord->priv->tmpdir);
		cdrecord->priv->tmpdir = NULL;
	}
}

static void
brasero_cdrecord_finalize (GObject *object)
{
	BraseroCDRecord *cobj;

	cobj = BRASERO_CD_RECORD (object);

	brasero_cdrecord_stop_real (cobj);

	if (cobj->priv->drive) {
		nautilus_burn_drive_unref (cobj->priv->drive);
		cobj->priv->drive = NULL;
	}

	if (cobj->priv->track) {
		brasero_track_source_free (cobj->priv->track);
		cobj->priv->track = NULL;
	}

	g_free (cobj->priv);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

BraseroCDRecord *
brasero_cdrecord_new ()
{
	BraseroCDRecord *obj;
	
	obj = BRASERO_CD_RECORD(g_object_new(BRASERO_TYPE_CD_RECORD, NULL));
	
	return obj;
}

static BraseroBurnResult
brasero_cdrecord_stderr_read (BraseroProcess *process, const gchar *line)
{
	BraseroCDRecord *cdrecord = BRASERO_CD_RECORD (process);

	if (strstr (line, "Cannot open SCSI driver.")) {
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_PERMISSION,
						_("You don't seem to have the required permission to use this drive")));
	}
	else if (strstr (line, "No disk / Wrong disk!") != NULL) {
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_MEDIA_NONE,
						_("There doesn't seem to be a disc in the drive")));
	}
	else if (strstr (line, "Input buffer error, aborting") != NULL) {
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_MEDIA_NONE,
						_("input buffer error")));
	}
	else if (strstr (line, "This means that we are checking recorded media.") != NULL) {
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_MEDIA_NOT_WRITABLE,
						_("The CD has already been recorded")));
	}
	else if (strstr (line, "Cannot blank disk, aborting.") != NULL) {
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_MEDIA_NOT_REWRITABLE,
						_("The CD cannot be blanked")));
	}
	else if (!cdrecord->priv->overburn
	      &&  strstr (line, "Data may not fit on current disk")) {
		/* we don't error out if overburn was chosen */
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_MEDIA_SPACE,
						_("The files selected did not fit on the CD")));
	}
	else if (strstr (line ,"cdrecord: A write error occured")) {
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_GENERAL,
						_("a write error occured which was likely due to overburning the disc")));
	}
	else if (strstr (line, "Inappropriate audio coding")) {
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_INCOMPATIBLE_FORMAT,
						_("All audio files must be stereo, 16-bit digital audio with 44100Hz samples")));
	}
	else if (strstr (line, "cannot write medium - incompatible format") != NULL) {
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_INCOMPATIBLE_FORMAT,
						_("The image does not seem to be a proper iso9660 file system")));
	}
	else if (strstr (line, "DMA speed too slow") != NULL) {
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_SLOW_DMA,
						_("The system is too slow to write the CD at this speed. Try a lower speed")));
	}
	else if (strstr (line, "Operation not permitted. Cannot send SCSI cmd via ioctl")) {
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_SCSI_IOCTL,
						_("You don't seem to have the required permission to use this drive")));
	}
	else if (strstr (line, "Device or resource busy")) {
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_BUSY_DRIVE,
						_("The drive seems to be busy (maybe check you have proper permissions to use it)")));
	}
	else if (strstr (line, "Illegal write mode for this drive")) {
		/* NOTE : when it happened I had to unlock the
		 * drive with cdrdao and eject it. Should we ? */
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_BUSY_DRIVE,
						_("The drive seems to be busy (maybe you should reload the media)")));
	}
	else if (strstr (line, "cdrecord: No such file or directory. Cannot open")) {
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_GENERAL,
						_("the image file cannot be found")));
	}
	else if (strstr (line, "Bad file descriptor. read error on input file")
	      ||  strstr (line, "No tracks specified. Need at least one.")) {
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_GENERAL,
						_("internal error")));
	}
	else if (strstr (line, "Could not write Lead-in")) {
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_GENERAL,
						_("the cd information could not be written")));
	}
	else if (strstr (line, "Cannot fixate disk")) {
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_GENERAL,
						_("the disc could not be closed")));
	}
	else if (strstr (line, "Bad audio track size")) {
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_GENERAL,
						_("the audio tracks are too short or not a multiple of 2352")));
	}

	return BRASERO_BURN_OK;
}

static void
brasero_cdrecord_compute (BraseroCDRecord *cdrecord,
			  gint mb_written,
			  gint mb_total,
			  gint track_num)
{
	gboolean track_num_changed = FALSE;
	gchar *action_string;
	gint64 this_remain;
	gint64 bytes;
	gint64 total;

	if (cdrecord->priv->tracks_total_bytes > 0)
		total = cdrecord->priv->tracks_total_bytes;
	else
		total = mb_total * 1048576;

	BRASERO_JOB_TASK_SET_TOTAL (cdrecord, total);

	if (track_num > cdrecord->priv->current_track_num) {
		track_num_changed = TRUE;
		cdrecord->priv->current_track_num = track_num;
		cdrecord->priv->current_track_end_pos += mb_total * 1048576;
	}

	this_remain = (mb_total - mb_written) * 1048576;
	bytes = (total - cdrecord->priv->current_track_end_pos) + this_remain;
	BRASERO_JOB_TASK_SET_WRITTEN (cdrecord, total - bytes);

	action_string = g_strdup_printf ("Writing track %02i", track_num);
	BRASERO_JOB_TASK_SET_ACTION (cdrecord,
				     BRASERO_BURN_ACTION_WRITING,
				     action_string,
				     track_num_changed);
	g_free (action_string);
}

static BraseroBurnResult
brasero_cdrecord_stdout_read (BraseroProcess *process, const gchar *line)
{
	guint track;
	guint speed_1, speed_2;
	BraseroCDRecord *cdrecord;
	int mb_written = 0, mb_total = 0, fifo = 0, buf = 0;

	cdrecord = BRASERO_CD_RECORD (process);

	if (sscanf (line, "Track %2u: %d of %d MB written (fifo %d%%) [buf %d%%] %d.%dx.",
		    &track, &mb_written, &mb_total, &fifo, &buf, &speed_1, &speed_2) == 7) {
		gdouble current_rate;

		current_rate = (gdouble) ((gdouble) speed_1 +
			       (gdouble) speed_2 / 10.0) *
			       (gdouble) CDR_SPEED;
		BRASERO_JOB_TASK_SET_RATE (cdrecord, current_rate);

		cdrecord->priv->current_track_written = mb_written * 1048576;
		brasero_cdrecord_compute (cdrecord,
					  mb_written,
					  mb_total,
					  track);

		BRASERO_JOB_TASK_START_PROGRESS (cdrecord, FALSE);
	} 
	else if (sscanf (line, "Track %2u:    %d MB written (fifo %d%%) [buf  %d%%]  %d.%dx.",
			 &track, &mb_written, &fifo, &buf, &speed_1, &speed_2) == 6) {
		gdouble current_rate;

		/* this line is printed when cdrecord writes on the fly */
		current_rate = (gdouble) ((gdouble) speed_1 +
			       (gdouble) speed_2 / 10.0) *
			       (gdouble) CDR_SPEED;
		BRASERO_JOB_TASK_SET_RATE (cdrecord, current_rate);

		cdrecord->priv->current_track_written = mb_written * 1048576;

		if (cdrecord->priv->track->type == BRASERO_TRACK_SOURCE_IMAGER) {
			/* we must ask the imager what is the total size */
			brasero_imager_get_size (BRASERO_IMAGER (cdrecord->priv->track->contents.imager.obj),
						 &cdrecord->priv->tracks_total_bytes,
						 FALSE, 
						 NULL);

			mb_total = cdrecord->priv->tracks_total_bytes / 1048576;
			brasero_cdrecord_compute (cdrecord,
						  mb_written,
						  mb_total,
						  track);
		}

		BRASERO_JOB_TASK_START_PROGRESS (cdrecord, FALSE);
	}
	else if (sscanf (line, "Track %*d: %*s %d MB ", &mb_total) == 1) {
		if (mb_total > 0) {
			cdrecord->priv->tracks_total_bytes += mb_total * 1048576;
		}
	}
	else if (strstr (line, "Sending CUE sheet")) {
		gboolean is_audio = FALSE;
		BraseroTrackSource *source;
		BraseroImageFormat format;
		BraseroTrackSourceType type = BRASERO_TRACK_SOURCE_UNKNOWN;

		source = cdrecord->priv->track;

		/* See if we are in an audio case which would mean we're writing
		 * CD-TEXT */
		if (source->type == BRASERO_TRACK_SOURCE_IMAGER)
			brasero_imager_get_track_type (source->contents.imager.obj,
						       &type,
						       &format);
		else
			type = source->type;

		if (type == BRASERO_TRACK_SOURCE_AUDIO || type == BRASERO_TRACK_SOURCE_INF)
			is_audio = TRUE;

		BRASERO_JOB_TASK_SET_ACTION (process,
					     BRASERO_BURN_ACTION_WRITING_CD_TEXT,
					     is_audio ? NULL:_("Writing cue sheet"),
					     FALSE);
	}
	else if (g_str_has_prefix (line, "Re-load disk and hit <CR>")
	      ||  g_str_has_prefix (line, "send SIGUSR1 to continue")) {
		BraseroBurnAction action = BRASERO_BURN_ACTION_NONE;

		BRASERO_JOB_TASK_GET_ACTION (process, &action);
		 
		/* NOTE: There seems to be a BUG somewhere when writing raw images
		 * with clone mode. After disc has been written and fixated cdrecord
		 * asks the media to be reloaded. So we simply ignore this message
		 * and returns that everything went well. Which is indeed the case */
		 if (action == BRASERO_BURN_ACTION_FIXATING) {
			brasero_job_finished (BRASERO_JOB (process));
			return BRASERO_BURN_OK;
		 }

		/* This is not supposed to happen since we checked for the cd
		   before starting, but we try to handle it anyway, since mmc
		   profiling can fail. */
		/* NOTE : nautilus_burn_recorder used to send sigusr1 or return */
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_RELOAD_MEDIA,
						_("The media needs to be reloaded before being recorded")));
	}
	else if (g_str_has_prefix (line, "Fixating...")
	      ||  g_str_has_prefix (line, "Writing Leadout...")) {
		BRASERO_JOB_TASK_SET_ACTION (process,
					     BRASERO_BURN_ACTION_FIXATING,
					     NULL,
					     FALSE);
	}
	else if (g_str_has_prefix (line, "Last chance to quit, ")) {
		brasero_job_set_dangerous (BRASERO_JOB (process), TRUE);
	}
	else if (g_str_has_prefix (line, "Blanking PMA, TOC, pregap")
	      ||  strstr (line, "Blanking entire disk")) {
		BRASERO_JOB_TASK_START_PROGRESS (cdrecord, FALSE);
	}
	else if (strstr (line, "Use tsize= option in SAO mode to specify track size")) {
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_GENERAL,
						_("internal error")));
	}

	return BRASERO_BURN_OK;
}

static gboolean
brasero_cdrecord_write_inf (BraseroCDRecord *cdrecord,
			    GPtrArray *argv,
			    const gchar *album,
			    gint index,
			    gint start,
			    BraseroSongInfo *info,
			    GError **error)
{
	gint fd;
	gint size;
	gchar *path;
	gchar *string;
	gint b_written;
	gchar buffer [128];

	/* NOTE: about the .inf files: they should have the exact same path
	 * but the ending suffix file is replaced by inf:
	 * example : /path/to/file.mp3 => /path/to/file.inf */
	if (info->path) {
		gchar *dot, *separator;

		dot = strrchr (info->path, '.');
		separator = strrchr (info->path, G_DIR_SEPARATOR);

		if (dot && dot > separator)
			path = g_strdup_printf ("%.*s.inf",
						dot - info->path,
						info->path);
		else
			path = g_strdup_printf ("%s.inf",
						info->path);
	}
	else
		path = g_strdup_printf ("%s/Track%02i.inf",
					cdrecord->priv->tmpdir,
					index);

	fd = open (path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if (fd < 0)
		goto error;

	BRASERO_JOB_LOG (cdrecord, "writing inf (%s)", path);

	strcpy (buffer, "# created by brasero\n");
	size = strlen (buffer);
	b_written = write (fd, buffer, size);
	if (b_written != size)
		goto error;

	strcpy (buffer, "MCN=\t\n");
	size = strlen (buffer);
	b_written = write (fd, buffer, size);
	if (b_written != size)
		goto error;

	if (info->isrc > 0)
		string = g_strdup_printf ("ISRC=\t%i\n", info->isrc);
	else
		string = g_strdup ("ISRC=\t\n");
	size = strlen (string);
	b_written = write (fd, string, size);
	g_free (string);
	if (b_written != size)
		goto error;

	strcpy (buffer, "Albumperformer=\t\n");
	size = strlen (buffer);
	b_written = write (fd, buffer, size);
	if (b_written != size)
		goto error;

	if (album)
		string = g_strdup_printf ("Albumtitle=\t%s\n", album);
	else
		string = strdup ("Albumtitle=\t\n");
	size = strlen (string);
	b_written = write (fd, string, size);
	g_free (string);
	if (b_written != size)
		goto error;

	if (info->artist)
		string = g_strdup_printf ("Performer=\t%s\n", info->artist);
	else
		string = strdup ("Performer=\t\n");
	size = strlen (string);
	b_written = write (fd, string, size);
	g_free (string);
	if (b_written != size)
		goto error;

	if (info->composer)
		string = g_strdup_printf ("Composer=\t%s\n", info->composer);
	else
		string = strdup ("Composer=\t\n");
	size = strlen (string);
	b_written = write (fd, string, size);
	g_free (string);
	if (b_written != size)
		goto error;

	if (info->title)
		string = g_strdup_printf ("Tracktitle=\t%s\n", info->title);
	else
		string = strdup ("Tracktitle=\t\n");
	size = strlen (string);
	b_written = write (fd, string, size);
	g_free (string);
	if (b_written != size)
		goto error;

	string = g_strdup_printf ("Tracknumber=\t%i\n", index);
	size = strlen (string);
	b_written = write (fd, string, size);
	g_free (string);
	if (b_written != size)
		goto error;

	string = g_strdup_printf ("Trackstart=\t%i\n", start);
	size = strlen (string);
	b_written = write (fd, string, size);
	g_free (string);
	if (b_written != size)
		goto error;

	string = g_strdup_printf ("Tracklength=\t%i, 0\n", info->sectors);
	size = strlen (string);
	b_written = write (fd, string, size);
	g_free (string);
	if (b_written != size)
		goto error;

	strcpy (buffer, "Pre-emphasis=\tyes\n");
	size = strlen (buffer);
	b_written = write (fd, buffer, size);
	if (b_written != size)
		goto error;

	strcpy (buffer, "Channels=\t2\n");
	size = strlen (buffer);
	b_written = write (fd, buffer, size);
	if (b_written != size)
		goto error;

	strcpy (buffer, "Copy_permitted=\tyes\n");
	size = strlen (buffer);
	b_written = write (fd, buffer, size);
	if (b_written != size)
		goto error;

	strcpy (buffer, "Endianess=\tlittle\n");
	size = strlen (buffer);
	b_written = write (fd, buffer, size);
	if (b_written != size)
		goto error;

	strcpy (buffer, "Index=\t\t0\n");
	size = strlen (buffer);
	b_written = write (fd, buffer, size);
	if (b_written != size)
		goto error;

	string = g_strdup_printf ("Index0=\t\t%i\n", info->sectors);
	size = strlen (string);
	b_written = write (fd, string, size);
	if (b_written != size)
		goto error;

	close (fd);

	cdrecord->priv->infs = g_slist_prepend (cdrecord->priv->infs,
						g_strdup (path));

	if (info->path) {
		g_ptr_array_add (argv, g_strdup (info->path));
		g_free (path);
	}
	else
		g_ptr_array_add (argv, path);

	return BRASERO_BURN_OK;


error:
	g_remove (path);
	g_free (path);

	g_set_error (error,
		     BRASERO_BURN_ERROR,
		     BRASERO_BURN_ERROR_GENERAL,
		     _("the inf file can't be written : %s"), 
		     strerror (errno));

	return BRASERO_BURN_ERR;
}

static BraseroBurnResult
brasero_cdrecord_write_infs (BraseroCDRecord *cdrecord,
			     GPtrArray *argv,
			     const BraseroTrackSource *infs,
			     GError **error)
{
	GSList *iter;
	gchar *album;
	gint index;
	gint start;

	if (cdrecord->priv->infs) {
		g_slist_foreach (cdrecord->priv->infs, (GFunc) g_remove, NULL);

		g_slist_foreach (cdrecord->priv->infs, (GFunc) g_free, NULL);
		g_slist_free (cdrecord->priv->infs);
		cdrecord->priv->infs = NULL;
	}

	if (!cdrecord->priv->tmpdir) {
		cdrecord->priv->tmpdir = g_strdup_printf ("%s/" BRASERO_BURN_TMP_FILE_NAME,
							  g_get_tmp_dir ());

		if (!mkdtemp (cdrecord->priv->tmpdir)) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("a temporary directory couldn't be created"));
			return BRASERO_BURN_ERR;
		}
	}

	album = infs->contents.audio.album;
	index = 0;
	start = 0;

	for (iter = infs->contents.audio.infos; iter; iter = iter->next) {
		BraseroSongInfo *info;
		BraseroBurnResult result;

		info = iter->data;
		result = brasero_cdrecord_write_inf (cdrecord,
						     argv,
						     album,
						     index,
						     start,
						     info,
						     error);

		if (result != BRASERO_BURN_OK)
			return result;

		index ++;
		start += info->sectors;
	}

	cdrecord->priv->infs = g_slist_reverse (cdrecord->priv->infs);
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_cdrecord_set_argv_record (BraseroCDRecord *cdrecord,
				  GPtrArray *argv, 
				  GError **error)
{
	BraseroTrackSource *source;

	if (!cdrecord->priv->track)
		BRASERO_JOB_NOT_READY (cdrecord);

	source = cdrecord->priv->track;

	if (cdrecord->priv->immediate) {
		g_ptr_array_add (argv, g_strdup ("-immed"));
		g_ptr_array_add (argv, g_strdup_printf ("minbuf=%i", cdrecord->priv->minbuf));
	}

        if (cdrecord->priv->rate > 0) {
		gchar *speed_str;

		speed_str = g_strdup_printf ("speed=%d", cdrecord->priv->rate);
		g_ptr_array_add (argv, speed_str);
	}

	if (cdrecord->priv->overburn)
		g_ptr_array_add (argv, g_strdup ("-overburn"));
	if (cdrecord->priv->burnproof)
		g_ptr_array_add (argv, g_strdup ("driveropts=burnfree"));
	if (cdrecord->priv->multi)
		g_ptr_array_add (argv, g_strdup ("-multi"));

	if (source->type == BRASERO_TRACK_SOURCE_IMAGER) {
		BraseroTrackSourceType track_type;
		BraseroImageFormat format;
		BraseroBurnResult result;
		BraseroImager *imager;
		int buffer_size;
		gint64 sectors;
		
		imager = cdrecord->priv->track->contents.imager.obj;

		/* we need to know what is the type of the track (audio / data) */
		result = brasero_imager_get_track_type (imager,
							&track_type,
							&format);
		if (result != BRASERO_BURN_OK) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("imager doesn't seem to be ready"));
			return BRASERO_BURN_ERR;
		}
		
		/* ask the size */
		result = brasero_imager_get_size (imager, &sectors, TRUE, error);
		if (result != BRASERO_BURN_OK) {
			if (!error)
				g_set_error (error,
					     BRASERO_BURN_ERROR,
					     BRASERO_BURN_ERROR_GENERAL,
					     _("imager doesn't seem to be ready"));
			return BRASERO_BURN_ERR;
		}

		/* we create a buffer depending on the size 
		 * buffer 4m> < 64m and is 1/25th of size otherwise */
		buffer_size = sectors * 2352 / 1024 / 1024 / 25;
		if (buffer_size > 32)
			buffer_size = 32;
		else if (buffer_size < 4)
			buffer_size = 4;

		g_ptr_array_add (argv, g_strdup_printf ("fs=%im", buffer_size));

		brasero_job_set_slave (BRASERO_JOB (cdrecord), BRASERO_JOB (imager));
		brasero_job_set_run_slave (BRASERO_JOB (cdrecord), TRUE);

		if (track_type == BRASERO_TRACK_SOURCE_IMAGE) {
			if ((format & BRASERO_IMAGE_FORMAT_ISO)) {
				g_ptr_array_add (argv, g_strdup_printf ("tsize=%Lis", sectors));

				if (cdrecord->priv->dao)
					g_ptr_array_add (argv, g_strdup ("-dao"));

				g_ptr_array_add (argv, g_strdup ("-data"));
				g_ptr_array_add (argv, g_strdup ("-nopad"));
				g_ptr_array_add (argv, g_strdup ("-"));
			}
#if 0
			else if ((format & BRASERO_IMAGE_FORMAT_CLONE)) {
				g_ptr_array_add (argv, g_strdup_printf ("tsize=%Lis", sectors));

				g_ptr_array_add (argv, g_strdup ("-raw96r"));
				g_ptr_array_add (argv, g_strdup ("-clone"));
				g_ptr_array_add (argv, g_strdup ("-"));

				/* we need to generate the toc first */

				if (result != BRASERO_BURN_OK)
					return result;
			} 
#endif
			else
				BRASERO_JOB_NOT_SUPPORTED (cdrecord);;
		}
		else if (track_type == BRASERO_TRACK_SOURCE_AUDIO) {
			BraseroTrackSource *infs;

			/* we need to get the inf first */
			result = brasero_imager_set_output_type (imager,
								 BRASERO_TRACK_SOURCE_INF,
								 BRASERO_IMAGE_FORMAT_NONE,
								 error);
			if (result != BRASERO_BURN_OK)
				return result;

			brasero_job_set_relay_slave_signals (BRASERO_JOB (cdrecord), TRUE);
			result = brasero_imager_get_track (imager,
							   &infs,
							   error);

			if (result != BRASERO_BURN_OK)
				return result;
	
			result = brasero_imager_set_output_type (imager,
								 BRASERO_TRACK_SOURCE_AUDIO,
								 BRASERO_IMAGE_FORMAT_NONE,
								 error);
			if (result != BRASERO_BURN_OK)
				return result;

			/* now we set the rate of the slave slightly above the speed */
			brasero_job_set_rate (BRASERO_JOB (imager),
					      (cdrecord->priv->rate + 1) * CDR_SPEED);

			/* now set the rest of the arguments */
			g_ptr_array_add (argv, g_strdup ("-dao"));
			g_ptr_array_add (argv, g_strdup ("-swab"));
			g_ptr_array_add (argv, g_strdup ("-audio"));
			g_ptr_array_add (argv, g_strdup ("-useinfo"));
			g_ptr_array_add (argv, g_strdup ("-text"));

			result = brasero_cdrecord_write_infs (cdrecord,
							      argv,
							      infs,
							      error);
			brasero_track_source_free (infs);

			if (result != BRASERO_BURN_OK)
				return result;
		}
		else
			BRASERO_JOB_NOT_SUPPORTED (cdrecord);

		brasero_job_set_relay_slave_signals (BRASERO_JOB (cdrecord), FALSE);
	}
	else if (source->type == BRASERO_TRACK_SOURCE_AUDIO) {
		BraseroBurnResult result;

		/* CD-text cannot be written in tao mode (which is the default) */
		if (cdrecord->priv->dao)
			g_ptr_array_add (argv, g_strdup ("-dao"));

		g_ptr_array_add (argv, g_strdup ("fs=16m"));
		g_ptr_array_add (argv, g_strdup ("-audio"));
		g_ptr_array_add (argv, g_strdup ("-swab"));
		g_ptr_array_add (argv, g_strdup ("-pad"));
	
		g_ptr_array_add (argv, g_strdup ("-useinfo"));
		g_ptr_array_add (argv, g_strdup ("-text"));

		result = brasero_cdrecord_write_infs (cdrecord,
						      argv,
						      source,
						      error);

		if (result != BRASERO_BURN_OK)
			return result;

		brasero_job_set_run_slave (BRASERO_JOB (cdrecord), FALSE);
	}
	else if (source->type == BRASERO_TRACK_SOURCE_IMAGE) {
		if (source->format == BRASERO_IMAGE_FORMAT_NONE) {
			gchar *image_path;

			image_path = brasero_track_source_get_image_localpath (cdrecord->priv->track);
			if (!image_path)
				return BRASERO_BURN_ERR;

			if (cdrecord->priv->dao)
				g_ptr_array_add (argv, g_strdup ("-dao"));

			g_ptr_array_add (argv, g_strdup ("fs=16m"));
			g_ptr_array_add (argv, g_strdup ("-data"));
			g_ptr_array_add (argv, g_strdup ("-nopad"));
			g_ptr_array_add (argv, image_path);

			brasero_job_set_run_slave (BRASERO_JOB (cdrecord), FALSE);
		}
		else if (source->format & BRASERO_IMAGE_FORMAT_ISO) {
			gchar *isopath;

			isopath = brasero_track_source_get_image_localpath (cdrecord->priv->track);
			if (!isopath)
				return BRASERO_BURN_ERR;

			if (cdrecord->priv->dao)
				g_ptr_array_add (argv, g_strdup ("-dao"));

			g_ptr_array_add (argv, g_strdup ("fs=16m"));
			g_ptr_array_add (argv, g_strdup ("-data"));
			g_ptr_array_add (argv, g_strdup ("-nopad"));
			g_ptr_array_add (argv, isopath);

			brasero_job_set_run_slave (BRASERO_JOB (cdrecord), FALSE);
		}
		else if (source->format & BRASERO_IMAGE_FORMAT_CLONE) {
			gchar *rawpath;

			rawpath = brasero_track_source_get_raw_localpath (cdrecord->priv->track);
			if (!rawpath)
				return BRASERO_BURN_ERR;

			g_ptr_array_add (argv, g_strdup ("fs=16m"));
			g_ptr_array_add (argv, g_strdup ("-raw96r"));
			g_ptr_array_add (argv, g_strdup ("-clone"));
			g_ptr_array_add (argv, rawpath);

			brasero_job_set_run_slave (BRASERO_JOB (cdrecord), FALSE);
		}
		else if (source->format & BRASERO_IMAGE_FORMAT_CUE) {
			gchar *cue_str;
			gchar *cuepath;

			if (cdrecord->priv->dao)
				g_ptr_array_add (argv, g_strdup ("-dao"));

			g_ptr_array_add (argv, g_strdup ("fs=16m"));

			cuepath = brasero_track_source_get_cue_localpath (cdrecord->priv->track);
			if (!cuepath)
				return BRASERO_BURN_ERR;

			cue_str = g_strdup_printf ("cuefile=%s", cuepath);
			g_free (cuepath);

			g_ptr_array_add (argv, cue_str);

			brasero_job_set_run_slave (BRASERO_JOB (cdrecord), FALSE);
		}
	}
	else
		BRASERO_JOB_NOT_SUPPORTED (cdrecord);

	BRASERO_JOB_TASK_SET_ACTION (cdrecord,
				     BRASERO_BURN_ACTION_PREPARING,
				     NULL,
				     FALSE);
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_cdrecord_set_argv_blank (BraseroCDRecord *cdrecord, GPtrArray *argv)
{
	gchar *blank_str;

	blank_str = g_strdup_printf ("blank=%s", cdrecord->priv->blank_fast ? "fast" : "all");
	g_ptr_array_add (argv, blank_str);

	BRASERO_JOB_TASK_SET_ACTION (cdrecord,
				     BRASERO_BURN_ACTION_ERASING,
				     NULL,
				     FALSE);
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_cdrecord_set_argv (BraseroProcess *process,
			   GPtrArray *argv,
			   gboolean has_master,
			   GError **error)
{
	BraseroCDRecord *cdrecord;
	BraseroBurnResult result;
	gchar *dev_str;

	cdrecord = BRASERO_CD_RECORD (process);

	if (has_master)
		BRASERO_JOB_NOT_SUPPORTED (cdrecord);

	if (!cdrecord->priv->drive)
		BRASERO_JOB_NOT_READY (cdrecord);

	g_ptr_array_add (argv, g_strdup ("cdrecord"));
	g_ptr_array_add (argv, g_strdup ("-v"));

	dev_str = g_strdup_printf ("dev=%s",
				   NCB_DRIVE_GET_DEVICE (cdrecord->priv->drive));

	g_ptr_array_add (argv, dev_str);

        if (cdrecord->priv->dummy)
		g_ptr_array_add (argv, g_strdup ("-dummy"));

	if (cdrecord->priv->nograce)
		g_ptr_array_add (argv, g_strdup ("gracetime=0"));

	if (cdrecord->priv->action == BRASERO_CD_RECORD_ACTION_RECORD)
		result = brasero_cdrecord_set_argv_record (cdrecord, argv, error);
	else if (cdrecord->priv->action == BRASERO_CD_RECORD_ACTION_BLANK)
		result = brasero_cdrecord_set_argv_blank (cdrecord, argv);
	else
		BRASERO_JOB_NOT_READY (cdrecord);

	return result;	
}

static BraseroBurnResult
brasero_cdrecord_post (BraseroProcess *process,
		       BraseroBurnResult retval)
{
	BraseroCDRecord *cdrecord;

	cdrecord = BRASERO_CD_RECORD (process);

	brasero_cdrecord_stop_real (cdrecord);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_cdrecord_set_drive (BraseroRecorder *recorder,
			    NautilusBurnDrive *drive,
			    GError **error)
{
	BraseroCDRecord *cdrecord;
	NautilusBurnMediaType media;

	cdrecord = BRASERO_CD_RECORD (recorder);

	media = nautilus_burn_drive_get_media_type (drive);
	if (media > NAUTILUS_BURN_MEDIA_TYPE_CDRW)
		BRASERO_JOB_NOT_SUPPORTED (cdrecord);

	if (cdrecord->priv->drive) {
		nautilus_burn_drive_unref (cdrecord->priv->drive);
		cdrecord->priv->drive = NULL;
	}

	cdrecord->priv->drive = drive;
	nautilus_burn_drive_ref (drive);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_cdrecord_set_source (BraseroJob *job,
			     const BraseroTrackSource *track,
			     GError **error)
{
	BraseroCDRecord *cdrecord;

	cdrecord = BRASERO_CD_RECORD (job);

	if (track->type != BRASERO_TRACK_SOURCE_IMAGE
	&&  track->type != BRASERO_TRACK_SOURCE_IMAGER
	&&  track->type != BRASERO_TRACK_SOURCE_AUDIO
	&&  track->type != BRASERO_TRACK_SOURCE_INF)
		BRASERO_JOB_NOT_SUPPORTED (cdrecord);

	if (cdrecord->priv->infs) {
		g_slist_foreach (cdrecord->priv->infs,
				 (GFunc) g_remove,
				 NULL);
		g_slist_foreach (cdrecord->priv->infs,
				 (GFunc) g_free,
				 NULL);
		g_slist_free (cdrecord->priv->infs);
		cdrecord->priv->infs = NULL;
	}

	if (cdrecord->priv->track)
		brasero_track_source_free (cdrecord->priv->track);

	cdrecord->priv->track = brasero_track_source_copy (track);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_cdrecord_set_flags (BraseroRecorder *recorder,
			    BraseroRecorderFlag flags,
			    GError **error)
{
	BraseroCDRecord *cdrecord;

	cdrecord = BRASERO_CD_RECORD (recorder);

	cdrecord->priv->dummy = (flags & BRASERO_RECORDER_FLAG_DUMMY) != 0;
	cdrecord->priv->dao = (flags & BRASERO_RECORDER_FLAG_DAO) != 0;
	cdrecord->priv->nograce = (flags & BRASERO_RECORDER_FLAG_NOGRACE) != 0;
	cdrecord->priv->burnproof = (flags & BRASERO_RECORDER_FLAG_BURNPROOF) != 0;
	cdrecord->priv->overburn = (flags & BRASERO_RECORDER_FLAG_OVERBURN) != 0;
	cdrecord->priv->blank_fast = (flags & BRASERO_RECORDER_FLAG_FAST_BLANK) != 0;
	cdrecord->priv->multi = (flags & BRASERO_RECORDER_FLAG_MULTI) != 0;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_cdrecord_set_rate (BraseroJob *job,
			   gint64 speed)
{
	BraseroCDRecord *cdrecord;

	if (brasero_job_is_running (job))
		return BRASERO_BURN_RUNNING;

	cdrecord = BRASERO_CD_RECORD (job);
	cdrecord->priv->rate = speed / CDR_SPEED;
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_cdrecord_blank (BraseroRecorder *recorder,
			GError **error)
{
	BraseroCDRecord *cdrecord;
	BraseroBurnResult result;

	cdrecord = BRASERO_CD_RECORD (recorder);

	if (!nautilus_burn_drive_can_rewrite (cdrecord->priv->drive)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("the drive cannot rewrite CDs or DVDs"));
		return BRASERO_BURN_ERR;
	}

	cdrecord->priv->action = BRASERO_CD_RECORD_ACTION_BLANK;
	result = brasero_job_run (BRASERO_JOB (cdrecord), error);
	cdrecord->priv->action = BRASERO_CD_RECORD_ACTION_NONE;

	return result;	
}

static BraseroBurnResult
brasero_cdrecord_record (BraseroRecorder *recorder,
			 GError **error)
{
	BraseroCDRecord *cdrecord;
	BraseroBurnResult result;

	cdrecord = BRASERO_CD_RECORD (recorder);

	if (!cdrecord->priv->track)
		BRASERO_JOB_NOT_READY (cdrecord);

	cdrecord->priv->action = BRASERO_CD_RECORD_ACTION_RECORD;
	result = brasero_job_run (BRASERO_JOB (cdrecord), error);
	cdrecord->priv->action = BRASERO_CD_RECORD_ACTION_NONE;

	return result;				
}

void
brasero_cdrecord_set_immediate (BraseroCDRecord *cdrecord, gint minbuf)
{
	g_return_if_fail (BRASERO_IS_CD_RECORD (cdrecord));

	if (minbuf > 95 || minbuf < 25)
		minbuf = 30;

	cdrecord->priv->immediate = 1;
	cdrecord->priv->minbuf = minbuf;
}
