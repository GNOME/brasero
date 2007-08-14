/***************************************************************************
 *            wodim.c
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
#include <gmodule.h>

#include <gconf/gconf-client.h>

#include <nautilus-burn-drive.h>

#include "burn-basics.h"
#include "burn-job.h"
#include "burn-process.h"
#include "burn-plugin.h"
#include "burn-wodim.h"

BRASERO_PLUGIN_BOILERPLATE (BraseroWoodim, brasero_wodim, BRASERO_TYPE_PROCESS, BraseroProcess);

struct _BraseroWoodimPrivate {
	gint64 current_track_end_pos;
	gint64 current_track_written;

	gint current_track_num;
	gint track_count;

	gint minbuf;

	guint immediate:1;
};
typedef struct _BraseroWoodimPrivate BraseroWoodimPrivate;
#define BRASERO_WODIM_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_WODIM, BraseroWoodimPrivate))

static GObjectClass *parent_class = NULL;

#define GCONF_KEY_IMMEDIATE_FLAG	"/apps/brasero/config/immed_flag"
#define GCONF_KEY_MINBUF_VALUE		"/apps/brasero/config/minbuf_value"

static BraseroBurnResult
brasero_wodim_stderr_read (BraseroProcess *process, const gchar *line)
{
	BraseroWoodim *wodim = BRASERO_WODIM (process);
	BraseroWoodimPrivate *priv;
	BraseroBurnFlag flags;

	priv = BRASERO_WODIM_PRIVATE (wodim);
	brasero_job_get_flags (BRASERO_JOB (wodim), &flags);

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
	else if (!(flags & BRASERO_BURN_FLAG_OVERBURN)
	     &&  strstr (line, "Data may not fit on current disk")) {
		/* we don't error out if overburn was chosen */
		brasero_job_error (BRASERO_JOB (process),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_MEDIA_SPACE,
						_("The files selected did not fit on the CD")));
	}
	else if (strstr (line ,"wodim: A write error occured")) {
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
	else if (strstr (line, "wodim: No such file or directory. Cannot open")) {
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
brasero_wodim_compute (BraseroWoodim *wodim,
			gint mb_written,
			gint mb_total,
			gint track_num)
{
	gboolean track_num_changed = FALSE;
	BraseroWoodimPrivate *priv;
	gchar *action_string;
	gint64 this_remain;
	gint64 bytes;
	gint64 total;

	priv = BRASERO_WODIM_PRIVATE (wodim);
	if (mb_total <= 0)
		return;

	total = mb_total * 1048576;

	if (track_num > priv->current_track_num) {
		track_num_changed = TRUE;
		priv->current_track_num = track_num;
		priv->current_track_end_pos += mb_total * 1048576;
	}

	this_remain = (mb_total - mb_written) * 1048576;
	bytes = (total - priv->current_track_end_pos) + this_remain;
	brasero_job_set_written (BRASERO_JOB (wodim), total - bytes);

	action_string = g_strdup_printf ("Writing track %02i", track_num);
	brasero_job_set_current_action (BRASERO_JOB (wodim),
					BRASERO_BURN_ACTION_RECORDING,
					action_string,
					track_num_changed);
	g_free (action_string);
}

static BraseroBurnResult
brasero_wodim_stdout_read (BraseroProcess *process, const gchar *line)
{
	guint track;
	guint speed_1, speed_2;
	BraseroWoodim *wodim;
	BraseroWoodimPrivate *priv;
	int mb_written = 0, mb_total = 0, fifo = 0, buf = 0;

	wodim = BRASERO_WODIM (process);
	priv = BRASERO_WODIM_PRIVATE (wodim);

	if (sscanf (line, "Track %2u: %d of %d MB written (fifo %d%%) [buf %d%%] %d.%dx.",
		    &track, &mb_written, &mb_total, &fifo, &buf, &speed_1, &speed_2) == 7) {
		gdouble current_rate;

		current_rate = (gdouble) ((gdouble) speed_1 +
			       (gdouble) speed_2 / 10.0) *
			       (gdouble) CD_RATE;
		brasero_job_set_rate (BRASERO_JOB (wodim), current_rate);

		priv->current_track_written = mb_written * 1048576;
		brasero_wodim_compute (wodim,
					  mb_written,
					  mb_total,
					  track);

		brasero_job_start_progress (BRASERO_JOB (wodim), FALSE);
	} 
	else if (sscanf (line, "Track %2u:    %d MB written (fifo %d%%) [buf  %d%%]  %d.%dx.",
			 &track, &mb_written, &fifo, &buf, &speed_1, &speed_2) == 6) {
		gdouble current_rate;

		/* this line is printed when wodim writes on the fly */
		current_rate = (gdouble) ((gdouble) speed_1 +
			       (gdouble) speed_2 / 10.0) *
			       (gdouble) CD_RATE;
		brasero_job_set_rate (BRASERO_JOB (wodim), current_rate);

		priv->current_track_written = mb_written * 1048576;
		if (brasero_job_get_fd_in (BRASERO_JOB (wodim), NULL) == BRASERO_BURN_OK) {
			gint64 bytes = 0;

			/* we must ask the imager what is the total size */
			brasero_job_get_session_output_size (BRASERO_JOB (wodim),
							     NULL,
							     &bytes);
			mb_total = bytes / 1048576;
			brasero_wodim_compute (wodim,
						mb_written,
						mb_total,
						track);
		}

		brasero_job_start_progress (BRASERO_JOB (wodim), FALSE);
	}
	else if (sscanf (line, "Track %*d: %*s %d MB ", &mb_total) == 1) {
/*		if (mb_total > 0)
			priv->tracks_total_bytes += mb_total * 1048576;
*/	}
	else if (strstr (line, "Sending CUE sheet")) {
		BraseroTrackType type;

		/* See if we are in an audio case which would mean we're writing
		 * CD-TEXT */
		brasero_job_get_input_type (BRASERO_JOB (wodim), &type);
		brasero_job_set_current_action (BRASERO_JOB (process),
						BRASERO_BURN_ACTION_RECORDING_CD_TEXT,
						(type.type == BRASERO_TRACK_TYPE_AUDIO) ? NULL:_("Writing cue sheet"),
						FALSE);
	}
	else if (g_str_has_prefix (line, "Re-load disk and hit <CR>")
	     ||  g_str_has_prefix (line, "send SIGUSR1 to continue")) {
		BraseroBurnAction action = BRASERO_BURN_ACTION_NONE;

		brasero_job_get_current_action (BRASERO_JOB (process), &action);

		/* NOTE: There seems to be a BUG somewhere when writing raw images
		 * with clone mode. After disc has been written and fixated wodim
		 * asks the media to be reloaded. So we simply ignore this message
		 * and returns that everything went well. Which is indeed the case */
		if (action == BRASERO_BURN_ACTION_FIXATING) {
			brasero_job_finished (BRASERO_JOB (process), NULL);
			return BRASERO_BURN_OK;
		}

		brasero_job_error (BRASERO_JOB (process),
				   g_error_new (BRASERO_BURN_ERROR,
						BRASERO_BURN_ERROR_RELOAD_MEDIA,
						_("The media needs to be reloaded before being recorded")));
	}
	else if (g_str_has_prefix (line, "Fixating...")
	     ||  g_str_has_prefix (line, "Writing Leadout...")) {
		brasero_job_set_current_action (BRASERO_JOB (process),
						BRASERO_BURN_ACTION_FIXATING,
						NULL,
						FALSE);
	}
	else if (g_str_has_prefix (line, "Last chance to quit, ")) {
		brasero_job_set_dangerous (BRASERO_JOB (process), TRUE);
	}
	else if (g_str_has_prefix (line, "Blanking PMA, TOC, pregap")
	     ||  strstr (line, "Blanking entire disk")) {
		brasero_job_start_progress (BRASERO_JOB (wodim), FALSE);
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
brasero_wodim_write_inf (BraseroWoodim *wodim,
			  GPtrArray *argv,
			  BraseroTrack *track,
			  const gchar *tmpdir,
			  const gchar *album,
			  gint index,
			  gint start,
			  GError **error)
{
	gint fd;
	gint size;
	gchar *path;
	gint64 length;
	gchar *string;
	gint b_written;
	gint64 sectors;
	gchar buffer [128];
	BraseroSongInfo *info;
	BraseroWoodimPrivate *priv;

	priv = BRASERO_WODIM_PRIVATE (wodim);

	/* NOTE: about the .inf files: they should have the exact same path
	 * but the ending suffix file is replaced by inf:
	 * example : /path/to/file.mp3 => /path/to/file.inf */
	path = brasero_track_get_audio_source (track, TRUE);
	if (path) {
		gchar *dot, *separator;

		dot = strrchr (path, '.');
		separator = strrchr (path, G_DIR_SEPARATOR);

		if (dot && dot > separator)
			path = g_strdup_printf ("%.*s.inf",
						dot - path,
						path);
		else
			path = g_strdup_printf ("%s.inf",
						path);
	}
	else
		path = g_strdup_printf ("%s/Track%02i.inf",
					tmpdir,
					index);

	fd = open (path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if (fd < 0)
		goto error;

	BRASERO_JOB_LOG (wodim, "writing inf (%s)", path);

	info = brasero_track_get_audio_info (track);

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

	brasero_track_get_audio_length (track, &length);
	sectors = BRASERO_DURATION_TO_SECTORS (length);
	string = g_strdup_printf ("Tracklength=\t%"G_GINT64_FORMAT", 0\n", sectors);
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

	string = g_strdup_printf ("Index0=\t\t%"G_GINT64_FORMAT"\n", sectors);
	size = strlen (string);
	b_written = write (fd, string, size);
	if (b_written != size)
		goto error;

	close (fd);
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
brasero_wodim_write_infs (BraseroWoodim *wodim,
			   GPtrArray *argv,
			   GError **error)
{
	BraseroWoodimPrivate *priv;
	BraseroBurnResult result;
	gchar *tmpdir = NULL;
	GSList *tracks;
	GSList *iter;
	gchar *album;
	gint index;
	gint start;

	priv = BRASERO_WODIM_PRIVATE (wodim);
	if (brasero_job_get_fd_in (BRASERO_JOB (wodim), NULL)) {
		/* if burning on the fly we need a tmp directory for the infs */
		result = brasero_job_get_tmp_dir (BRASERO_JOB (wodim),
						  &tmpdir,
						  error);
		if (result != BRASERO_BURN_OK)
			return result;
	}

	brasero_job_get_audio_title (BRASERO_JOB (wodim), &album);
	brasero_job_get_tracks (BRASERO_JOB (wodim), &tracks);
	index = 0;
	start = 0;

	for (iter = tracks; iter; iter = iter->next) {
		gint64 length;
		BraseroTrack *track;

		track = iter->data;
		result = brasero_wodim_write_inf (wodim,
						   argv,
						   track,
						   tmpdir,
						   album,
						   BRASERO_DURATION_TO_SECTORS (start),
						   index,
						   error);
		if (result != BRASERO_BURN_OK)
			return result;

		index ++;
		length = 0;
		brasero_track_get_audio_length (track, &length);
		start += length;
	}

	g_slist_free (tracks);
	g_free (tmpdir);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_wodim_set_argv_record (BraseroWoodim *wodim,
				GPtrArray *argv, 
				GError **error)
{
	guint speed;
	BraseroBurnFlag flags;
	BraseroTrackType type;
	BraseroWoodimPrivate *priv;

	priv = BRASERO_WODIM_PRIVATE (wodim);

	if (priv->immediate) {
		g_ptr_array_add (argv, g_strdup ("-immed"));
		g_ptr_array_add (argv, g_strdup_printf ("minbuf=%i", priv->minbuf));
	}

	if (brasero_job_get_speed (BRASERO_JOB (wodim), &speed) == BRASERO_BURN_OK) {
		gchar *speed_str;

		speed_str = g_strdup_printf ("speed=%d", speed);
		g_ptr_array_add (argv, speed_str);
	}

	brasero_job_get_flags (BRASERO_JOB (wodim), &flags);
	if (flags & BRASERO_BURN_FLAG_OVERBURN)
		g_ptr_array_add (argv, g_strdup ("-overburn"));
	if (flags & BRASERO_BURN_FLAG_BURNPROOF)
		g_ptr_array_add (argv, g_strdup ("driveropts=burnfree"));
	if (flags & BRASERO_BURN_FLAG_MULTI)
		g_ptr_array_add (argv, g_strdup ("-multi"));

	brasero_job_get_input_type (BRASERO_JOB (wodim), &type);
	if (brasero_job_get_fd_in (BRASERO_JOB (wodim), NULL) == BRASERO_BURN_OK) {
		BraseroBurnResult result;
		int buffer_size;
		gint64 sectors;
		
		/* we need to know what is the type of the track (audio / data) */
		result = brasero_job_get_input_type (BRASERO_JOB (wodim), &type);
		if (result != BRASERO_BURN_OK) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("imager doesn't seem to be ready"));
			return BRASERO_BURN_ERR;
		}
		
		/* ask the size */
		result = brasero_job_get_session_output_size (BRASERO_JOB (wodim),
							      &sectors,
							      NULL);
		if (result != BRASERO_BURN_OK) {
			g_set_error (error,
				     BRASERO_BURN_ERROR,
				     BRASERO_BURN_ERROR_GENERAL,
				     _("the size of the session cannot be retrieved"));
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
		if (type.type == BRASERO_TRACK_TYPE_IMAGE) {
			if (type.subtype.img_format == BRASERO_IMAGE_FORMAT_BIN) {
				g_ptr_array_add (argv, g_strdup_printf ("tsize=%Lis", sectors));

				/* DAO can't be used if we're appending to a 
				 * disc with audio track(s) on it */
				if (flags & BRASERO_BURN_FLAG_DAO)
					g_ptr_array_add (argv, g_strdup ("-dao"));

				g_ptr_array_add (argv, g_strdup ("-data"));
				g_ptr_array_add (argv, g_strdup ("-nopad"));
				g_ptr_array_add (argv, g_strdup ("-"));
			}
			else
				BRASERO_JOB_NOT_SUPPORTED (wodim);;
		}
		else if (type.type == BRASERO_TRACK_TYPE_AUDIO) {
			/* now set the rest of the arguments */
			if (flags & BRASERO_BURN_FLAG_DAO)
				g_ptr_array_add (argv, g_strdup ("-dao"));

			g_ptr_array_add (argv, g_strdup ("-swab"));
			g_ptr_array_add (argv, g_strdup ("-audio"));
			g_ptr_array_add (argv, g_strdup ("-useinfo"));
			g_ptr_array_add (argv, g_strdup ("-text"));

			result = brasero_wodim_write_infs (wodim,
							   argv,
							   error);
			if (result != BRASERO_BURN_OK)
				return result;
		}
		else
			BRASERO_JOB_NOT_SUPPORTED (wodim);
	}
	else if (type.type == BRASERO_TRACK_TYPE_AUDIO) {
		BraseroBurnResult result;

		/* CD-text cannot be written in tao mode (which is the default) */
		if (flags & BRASERO_BURN_FLAG_DAO)
			g_ptr_array_add (argv, g_strdup ("-dao"));

		g_ptr_array_add (argv, g_strdup ("fs=16m"));
		g_ptr_array_add (argv, g_strdup ("-audio"));
		g_ptr_array_add (argv, g_strdup ("-swab"));
		g_ptr_array_add (argv, g_strdup ("-pad"));
	
		g_ptr_array_add (argv, g_strdup ("-useinfo"));
		g_ptr_array_add (argv, g_strdup ("-text"));

		result = brasero_wodim_write_infs (wodim,
						   argv,
						   error);
		if (result != BRASERO_BURN_OK)
			return result;
	}
	else if (type.type == BRASERO_TRACK_TYPE_IMAGE) {
		BraseroTrack *track = NULL;

		brasero_job_get_current_track (BRASERO_JOB (wodim), &track);
		if (!track)
			BRASERO_JOB_NOT_READY (wodim);

		if (type.subtype.img_format == BRASERO_IMAGE_FORMAT_NONE) {
			gchar *image_path;

			image_path = brasero_track_get_image_source (track, FALSE);
			if (!image_path)
				BRASERO_JOB_NOT_READY (wodim);

			if (flags & BRASERO_BURN_FLAG_DAO)
				g_ptr_array_add (argv, g_strdup ("-dao"));

			g_ptr_array_add (argv, g_strdup ("fs=16m"));
			g_ptr_array_add (argv, g_strdup ("-data"));
			g_ptr_array_add (argv, g_strdup ("-nopad"));
			g_ptr_array_add (argv, image_path);
		}
		else if (type.subtype.img_format == BRASERO_IMAGE_FORMAT_BIN) {
			gchar *isopath;

			isopath = brasero_track_get_image_source (track, FALSE);
			if (!isopath)
				BRASERO_JOB_NOT_READY (wodim);

			if (flags & BRASERO_BURN_FLAG_DAO)
				g_ptr_array_add (argv, g_strdup ("-dao"));

			g_ptr_array_add (argv, g_strdup ("fs=16m"));
			g_ptr_array_add (argv, g_strdup ("-data"));
			g_ptr_array_add (argv, g_strdup ("-nopad"));
			g_ptr_array_add (argv, isopath);
		}
		else if (type.subtype.img_format == BRASERO_IMAGE_FORMAT_CLONE) {
			gchar *rawpath;

			rawpath = brasero_track_get_image_source (track, FALSE);
			if (!rawpath)
				BRASERO_JOB_NOT_READY (wodim);

			if (flags & BRASERO_BURN_FLAG_DAO)
				return BRASERO_BURN_ERR;

			g_ptr_array_add (argv, g_strdup ("fs=16m"));
			g_ptr_array_add (argv, g_strdup ("-raw96r"));
			g_ptr_array_add (argv, g_strdup ("-clone"));
			g_ptr_array_add (argv, rawpath);
		}
		else if (type.subtype.img_format == BRASERO_IMAGE_FORMAT_CUE) {
			gchar *cue_str;
			gchar *cuepath;

			cuepath = brasero_track_get_toc_source (track, FALSE);
			if (!cuepath)
				BRASERO_JOB_NOT_READY (wodim);

			if (flags & BRASERO_BURN_FLAG_DAO)
				g_ptr_array_add (argv, g_strdup ("-dao"));

			g_ptr_array_add (argv, g_strdup ("fs=16m"));

			cue_str = g_strdup_printf ("cuefile=%s", cuepath);
			g_ptr_array_add (argv, cue_str);
			g_free (cuepath);
		}
		else
			BRASERO_JOB_NOT_SUPPORTED (wodim);
	}
	else
		BRASERO_JOB_NOT_SUPPORTED (wodim);

	brasero_job_set_current_action (BRASERO_JOB (wodim),
					BRASERO_BURN_ACTION_PREPARING,
					NULL,
					FALSE);
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_wodim_set_argv_blank (BraseroWoodim *wodim, GPtrArray *argv)
{
	gchar *blank_str;
	BraseroBurnFlag flags;

	brasero_job_get_flags (BRASERO_JOB (wodim), &flags);
	blank_str = g_strdup_printf ("blank=%s",
				    (flags & BRASERO_BURN_FLAG_FAST_BLANK) ? "fast" : "all");
	g_ptr_array_add (argv, blank_str);

	brasero_job_set_current_action (BRASERO_JOB (wodim),
					BRASERO_BURN_ACTION_BLANKING,
					NULL,
					FALSE);
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_wodim_set_argv (BraseroProcess *process,
			 GPtrArray *argv,
			 GError **error)
{
	BraseroWoodimPrivate *priv;
	BraseroBurnResult result;
	BraseroJobAction action;
	BraseroWoodim *wodim;
	BraseroBurnFlag flags;
	gchar *dev_str;
	gchar *device;

	wodim = BRASERO_WODIM (process);
	priv = BRASERO_WODIM_PRIVATE (wodim);

	brasero_job_get_action (BRASERO_JOB (wodim), &action);
	if (action == BRASERO_JOB_ACTION_SIZE) {
		BraseroTrackType input = { 0 };
		BraseroTrack *track = NULL;

		if (brasero_job_get_fd_in (BRASERO_JOB (process), NULL) == BRASERO_BURN_OK)
			return BRASERO_BURN_NOT_RUNNING;

		/* there is just one case where we can set the output size which
		 * is when the input is IMAGE type */
		brasero_job_get_current_track (BRASERO_JOB (process), &track);
		brasero_track_get_type (track, &input);
		if (input.type == BRASERO_TRACK_TYPE_IMAGE) {
			gint64 sectors = 0;
			gint64 size = 0;

			result = brasero_track_get_image_size (track,
							       NULL,
							       &sectors,
							       &size,
							       error);
			if (result != BRASERO_BURN_OK)
				return result;

			brasero_job_set_output_size_for_current_track (BRASERO_JOB (process),
								       sectors,
								       size);
		}

		return BRASERO_BURN_NOT_RUNNING;
	}

	/* This is to support cdrkit. We give it the priority. */
	g_ptr_array_add (argv, g_strdup ("wodim"));
	g_ptr_array_add (argv, g_strdup ("-v"));

	brasero_job_get_device (BRASERO_JOB (wodim), &device);
	dev_str = g_strdup_printf ("dev=%s", device);
	g_ptr_array_add (argv, dev_str);
	g_free (device);

	brasero_job_get_flags (BRASERO_JOB (wodim), &flags);
        if (flags & BRASERO_BURN_FLAG_DUMMY)
		g_ptr_array_add (argv, g_strdup ("-dummy"));

	if (flags & BRASERO_BURN_FLAG_NOGRACE)
		g_ptr_array_add (argv, g_strdup ("gracetime=0"));

	if (action == BRASERO_JOB_ACTION_RECORD)
		result = brasero_wodim_set_argv_record (wodim, argv, error);
	else if (action == BRASERO_JOB_ACTION_ERASE)
		result = brasero_wodim_set_argv_blank (wodim, argv);
	else
		BRASERO_JOB_NOT_SUPPORTED (wodim);

	return result;	
}

static void
brasero_wodim_class_init (BraseroWoodimClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroProcessClass *process_class = BRASERO_PROCESS_CLASS (klass);

	g_type_class_add_private (klass, sizeof (BraseroWoodimPrivate));

	parent_class = g_type_class_peek_parent (klass);
	object_class->finalize = brasero_wodim_finalize;

	process_class->stderr_func = brasero_wodim_stderr_read;
	process_class->stdout_func = brasero_wodim_stdout_read;
	process_class->set_argv = brasero_wodim_set_argv;
}

static void
brasero_wodim_init (BraseroWoodim *obj)
{
	GConfClient *client;
	BraseroWoodimPrivate *priv;

	/* load our "configuration" */
	priv = BRASERO_WODIM_PRIVATE (obj);

	client = gconf_client_get_default ();
	priv->immediate = gconf_client_get_bool (client,
						 GCONF_KEY_IMMEDIATE_FLAG,
						 NULL);
	priv->minbuf = gconf_client_get_int (client,
					     GCONF_KEY_MINBUF_VALUE,
					     NULL);
	if (priv->minbuf > 95 || priv->minbuf < 25)
		priv->minbuf = 30;

	g_object_unref (client);
}

static void
brasero_wodim_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

G_MODULE_EXPORT GType
brasero_plugin_register (BraseroPlugin *plugin, gchar **error)
{
	const BraseroMedia media = BRASERO_MEDIUM_CD|
				   BRASERO_MEDIUM_WRITABLE|
				   BRASERO_MEDIUM_REWRITABLE|
				   BRASERO_MEDIUM_BLANK|
				   BRASERO_MEDIUM_APPENDABLE|
				   BRASERO_MEDIUM_HAS_AUDIO|
				   BRASERO_MEDIUM_HAS_DATA;
	const BraseroMedia media_rw = BRASERO_MEDIUM_CD|
				      BRASERO_MEDIUM_REWRITABLE|
				      BRASERO_MEDIUM_APPENDABLE|
				      BRASERO_MEDIUM_CLOSED|
				      BRASERO_MEDIUM_HAS_AUDIO|
				      BRASERO_MEDIUM_HAS_DATA;
	gchar *prog_name;
	GSList *output;
	GSList *input;

	/* First see if this plugin can be used, i.e. if wodim is in da path */
	prog_name = g_find_program_in_path ("wodim");
	if (!prog_name) {
		*error = g_strdup (_("wodim could not be found in the path"));
		return G_TYPE_NONE;
	}
	g_free (prog_name);

	brasero_plugin_define (plugin,
			       "wodim",
			       _("use wodim to burn CDs"),
			       "Philippe Rouquier",
			       1);

	/* for recording */
	output = brasero_caps_disc_new (media);
	input = brasero_caps_image_new (BRASERO_PLUGIN_IO_ACCEPT_PIPE|
					BRASERO_PLUGIN_IO_ACCEPT_FILE,
					BRASERO_IMAGE_FORMAT_BIN);

	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (input);

	input = brasero_caps_image_new (BRASERO_PLUGIN_IO_ACCEPT_FILE,
					BRASERO_IMAGE_FORMAT_CUE|
					BRASERO_IMAGE_FORMAT_CLONE);

	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (input);

	input = brasero_caps_audio_new (BRASERO_PLUGIN_IO_ACCEPT_PIPE|
					BRASERO_PLUGIN_IO_ACCEPT_FILE,
					BRASERO_AUDIO_FORMAT_RAW);

	brasero_plugin_link_caps (plugin, output, input);
	g_slist_free (output);
	g_slist_free (input);

	brasero_plugin_set_flags (plugin,
				  media,
				  BRASERO_BURN_FLAG_DAO|
				  BRASERO_BURN_FLAG_BURNPROOF|
				  BRASERO_BURN_FLAG_OVERBURN|
				  BRASERO_BURN_FLAG_MULTI|
				  BRASERO_BURN_FLAG_DUMMY|
				  BRASERO_BURN_FLAG_NOGRACE,
				  BRASERO_BURN_FLAG_NONE);

	/* for blanking */
	output = brasero_caps_disc_new (media_rw);
	brasero_plugin_blank_caps (plugin, output);
	g_slist_free (output);

	brasero_plugin_set_blank_flags (plugin,
					media_rw,
					BRASERO_BURN_FLAG_DUMMY|
					BRASERO_BURN_FLAG_NOGRACE|
					BRASERO_BURN_FLAG_FAST_BLANK,
					BRASERO_BURN_FLAG_NONE);

	return brasero_wodim_get_type (plugin);
}
