/***************************************************************************
 *            dvd-rw-format.c
 *
 *  sam fév  4 13:50:07 2006
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

#include <stdio.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include <nautilus-burn-drive.h>

#include "burn-basics.h"
#include "burn-dvd-rw-format.h"
#include "burn-recorder.h"
#include "burn-process.h"
#include "brasero-ncb.h"

static void brasero_dvd_rw_format_class_init (BraseroDvdRwFormatClass *klass);
static void brasero_dvd_rw_format_init (BraseroDvdRwFormat *sp);
static void brasero_dvd_rw_format_finalize (GObject *object);
static void brasero_dvd_rw_format_iface_init (BraseroRecorderIFace *iface);

struct BraseroDvdRwFormatPrivate {
	NautilusBurnDrive *drive;

	int dummy:1;
	int blank_fast:1;
};

static BraseroBurnResult
brasero_dvd_rw_format_blank (BraseroRecorder *recorder,
			     GError **error);
static BraseroBurnResult
brasero_dvd_rw_format_set_drive (BraseroRecorder *recorder,
				 NautilusBurnDrive *drive,
				 GError **error);
static BraseroBurnResult
brasero_dvd_rw_format_set_flags (BraseroRecorder *recorder,
				 BraseroRecorderFlag flags,
				 GError **error);

static BraseroBurnResult
brasero_dvd_rw_format_set_argv (BraseroProcess *process,
				GPtrArray *argv,
				gboolean has_master,
				GError **error);
static BraseroBurnResult
brasero_dvd_rw_format_read_stderr (BraseroProcess *process,
				   const char *line);

static GObjectClass *parent_class = NULL;

GType
brasero_dvd_rw_format_get_type ()
{
	static GType type = 0;

	if(type == 0) {
		static const GTypeInfo our_info = {
			sizeof (BraseroDvdRwFormatClass),
			NULL,
			NULL,
			(GClassInitFunc)brasero_dvd_rw_format_class_init,
			NULL,
			NULL,
			sizeof (BraseroDvdRwFormat),
			0,
			(GInstanceInitFunc)brasero_dvd_rw_format_init,
		};

		static const GInterfaceInfo recorder_info =
		{
			(GInterfaceInitFunc) brasero_dvd_rw_format_iface_init,
			NULL,
			NULL
		};

		type = g_type_register_static(BRASERO_TYPE_PROCESS,
					      "BraseroDvdRwFormat",
					      &our_info,
					      0);

		g_type_add_interface_static (type,
					     BRASERO_TYPE_RECORDER,
					     &recorder_info);
	}

	return type;
}

static void
brasero_dvd_rw_format_iface_init (BraseroRecorderIFace *iface)
{
	iface->set_drive = brasero_dvd_rw_format_set_drive;
	iface->set_flags = brasero_dvd_rw_format_set_flags;
	iface->blank = brasero_dvd_rw_format_blank;
}

static void
brasero_dvd_rw_format_class_init (BraseroDvdRwFormatClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	BraseroProcessClass *process_class = BRASERO_PROCESS_CLASS (klass);

	parent_class = g_type_class_peek_parent(klass);
	object_class->finalize = brasero_dvd_rw_format_finalize;

	process_class->set_argv = brasero_dvd_rw_format_set_argv;
	process_class->stderr_func = brasero_dvd_rw_format_read_stderr;
}

static void
brasero_dvd_rw_format_init (BraseroDvdRwFormat *obj)
{
	obj->priv = g_new0 (BraseroDvdRwFormatPrivate, 1);
}

static void
brasero_dvd_rw_format_finalize(GObject *object)
{
	BraseroDvdRwFormat *cobj;
	cobj = BRASERO_DVD_RW_FORMAT (object);

	if (cobj->priv->drive) {
		nautilus_burn_drive_unref (cobj->priv->drive);
		cobj->priv->drive = NULL;
	}

	g_free(cobj->priv);
	G_OBJECT_CLASS(parent_class)->finalize(object);
}

BraseroDvdRwFormat *
brasero_dvd_rw_format_new ()
{
	BraseroDvdRwFormat *obj;
	
	obj = BRASERO_DVD_RW_FORMAT (g_object_new(BRASERO_TYPE_DVD_RW_FORMAT, NULL));
	
	return obj;
}

static BraseroBurnResult
brasero_dvd_rw_format_blank (BraseroRecorder *recorder,
			     GError **error)
{
	BraseroDvdRwFormat *dvdformat;
	NautilusBurnMediaType media;
	BraseroBurnResult result;

	dvdformat = BRASERO_DVD_RW_FORMAT (recorder);
	media = nautilus_burn_drive_get_media_type (dvdformat->priv->drive);

	if (media <= NAUTILUS_BURN_MEDIA_TYPE_CDRW)
		BRASERO_JOB_NOT_SUPPORTED (dvdformat);;

	/* There is no need to format RW+ in a fast way */
        if (media == NAUTILUS_BURN_MEDIA_TYPE_DVD_PLUS_RW && dvdformat->priv->blank_fast)
		return BRASERO_BURN_OK;

	result = brasero_job_run (BRASERO_JOB (dvdformat), error);

	return result;
}

static BraseroBurnResult
brasero_dvd_rw_format_set_drive (BraseroRecorder *recorder,
				 NautilusBurnDrive *drive,
				 GError **error)
{
	BraseroDvdRwFormat *dvdformat;

	dvdformat = BRASERO_DVD_RW_FORMAT (recorder);

	if (dvdformat->priv->drive)
		nautilus_burn_drive_unref (dvdformat->priv->drive);

	nautilus_burn_drive_ref (drive);
	dvdformat->priv->drive = drive;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_dvd_rw_format_set_flags (BraseroRecorder *recorder,
				 BraseroRecorderFlag flags,
				 GError **error)
{
	BraseroDvdRwFormat *dvdformat;

	dvdformat = BRASERO_DVD_RW_FORMAT (recorder);

	/* apparently there is no switch for BRASERO_RECORDER_BLANK_FLAG_NOGRACE */
	dvdformat->priv->blank_fast = (flags & BRASERO_RECORDER_FLAG_FAST_BLANK);
	dvdformat->priv->dummy = (flags & BRASERO_RECORDER_FLAG_DUMMY);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_dvd_rw_format_read_stderr (BraseroProcess *process, const char *line)
{
	BraseroDvdRwFormat *dvdformat;
	float percent;

	dvdformat = BRASERO_DVD_RW_FORMAT (process);

	if ((sscanf (line, "* blanking %f%%,", &percent) == 1)
	||  (sscanf (line, "* formatting %f%%,", &percent) == 1)
	||  (sscanf (line, "* relocating lead-out %f%%,", &percent) == 1))
		brasero_job_set_dangerous (BRASERO_JOB (process), TRUE);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_dvd_rw_format_set_argv (BraseroProcess *process,
				GPtrArray *argv,
				gboolean has_master,
				GError **error)
{
	BraseroDvdRwFormat *dvdformat;
	NautilusBurnMediaType media;
	gchar *dev_str;

	dvdformat = BRASERO_DVD_RW_FORMAT (process);

	if (has_master)
		BRASERO_JOB_NOT_SUPPORTED (dvdformat);;

	brasero_job_set_run_slave (BRASERO_JOB (dvdformat), FALSE);

	g_ptr_array_add (argv, g_strdup ("dvd+rw-format"));

	/* undocumented option to show progress */
	g_ptr_array_add (argv, g_strdup ("-gui"));

	media = nautilus_burn_drive_get_media_type (dvdformat->priv->drive);
        if (media != NAUTILUS_BURN_MEDIA_TYPE_DVD_PLUS_RW) {
		gchar *blank_str;

		blank_str = g_strdup_printf ("-blank%s",
					     dvdformat->priv->blank_fast ? "" : "=full");
		g_ptr_array_add (argv, blank_str);
	}

	/* it seems that dvd-format prefers the device path not cdrecord_id */
	dev_str = g_strdup (NCB_DRIVE_GET_DEVICE (dvdformat->priv->drive));
	g_ptr_array_add (argv, dev_str);

	BRASERO_JOB_TASK_SET_ACTION (dvdformat,
				     BRASERO_BURN_ACTION_ERASING,
				     NULL,
				     FALSE);
	return BRASERO_BURN_OK;
}

