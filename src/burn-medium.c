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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <nautilus-burn-drive.h>

#include "burn-basics.h"
#include "burn-medium.h"
#include "scsi-mmc1.h"
#include "scsi-mmc2.h"
#include "scsi-utils.h"
#include "scsi-q-subchannel.h"

typedef struct _BraseroMediumPrivate BraseroMediumPrivate;
struct _BraseroMediumPrivate
{
	GSList * tracks;
	guint64 next_wr_add;
	BraseroMediumInfo info;
	NautilusBurnDrive * drive;
};

#define BRASERO_MEDIUM_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_MEDIUM, BraseroMediumPrivate))

enum
{
	PROP_0,

	PROP_DRIVE
};

static GObjectClass* parent_class = NULL;

BraseroMediumInfo
brasero_medium_get_status (BraseroMedium *medium)
{
	BraseroMediumPrivate *priv;

	priv = BRASERO_MEDIUM_PRIVATE (medium);
	return priv->info;
}

GSList *
brasero_medium_get_tracks (BraseroMedium *medium)
{
	BraseroMediumPrivate *priv;

	priv = BRASERO_MEDIUM_PRIVATE (medium);
	return g_slist_copy (priv->tracks);
}

gint64
brasero_medium_get_last_data_track_address (BraseroMedium *medium)
{
	GSList *iter;
	BraseroMediumPrivate *priv;
	BraseroMediumTrack *track = NULL;

	priv = BRASERO_MEDIUM_PRIVATE (medium);

	for (iter = priv->tracks; iter; iter = iter->next) {
		BraseroMediumTrack *current;

		current = iter->data;
		if (current->type & BRASERO_MEDIUM_TRACK_DATA)
			track = current;
	}

	if (!track)
		return -1;

	return track->start;
}

gint64
brasero_medium_get_next_writable_address (BraseroMedium *medium)
{
	BraseroMediumPrivate *priv;

	priv = BRASERO_MEDIUM_PRIVATE (medium);
	return priv->next_wr_add;
}

static BraseroBurnResult
brasero_medium_get_medium_type (BraseroMedium *self,
				int fd,
				BraseroScsiErrCode *code)
{
	BraseroScsiGetConfigHdr *hdr = NULL;
	BraseroMediumPrivate *priv;
	BraseroScsiResult result;
	int size;

	priv = BRASERO_MEDIUM_PRIVATE (self);
	result = brasero_mmc2_get_configuration_feature (fd,
							 BRASERO_SCSI_FEAT_PROFILES,
							 &hdr,
							 &size,
							 code);

	if (result != BRASERO_SCSI_OK) {
		g_free (hdr);
		return BRASERO_BURN_ERR;
	}

	switch (BRASERO_GET_16 (hdr->current_profile)) {
	case BRASERO_SCSI_PROF_CDROM:
		priv->info = BRASERO_MEDIUM_CD;
		break;

	case BRASERO_SCSI_PROF_CDR:
		priv->info = BRASERO_MEDIUM_CD|
		       	     BRASERO_MEDIUM_WRITABLE;
		break;

	case BRASERO_SCSI_PROF_CDRW:
		priv->info = BRASERO_MEDIUM_CD|
		       	     BRASERO_MEDIUM_WRITABLE|
		       	     BRASERO_MEDIUM_REWRITABLE;
		break;

	case BRASERO_SCSI_PROF_DVD_ROM:
		priv->info = BRASERO_MEDIUM_DVD;
		break;

	case BRASERO_SCSI_PROF_DVD_R:
		priv->info = BRASERO_MEDIUM_DVD|
		       	     BRASERO_MEDIUM_WRITABLE;
		break;

	case BRASERO_SCSI_PROF_DVD_RW_RESTRICTED:
		priv->info = BRASERO_MEDIUM_DVD|
		      	     BRASERO_MEDIUM_WRITABLE|
		      	     BRASERO_MEDIUM_REWRITABLE|
		      	     BRASERO_MEDIUM_RESTRICTED;
		break;
	case BRASERO_SCSI_PROF_DVD_RW_SEQUENTIAL:
		priv->info = BRASERO_MEDIUM_DVD|
			     BRASERO_MEDIUM_WRITABLE|
			     BRASERO_MEDIUM_REWRITABLE|
			     BRASERO_MEDIUM_SEQUENTIAL;
		break;
	case BRASERO_SCSI_PROF_DVD_R_DL_SEQUENTIAL:
		priv->info = BRASERO_MEDIUM_DVD|
			     BRASERO_MEDIUM_WRITABLE|
			     BRASERO_MEDIUM_SEQUENTIAL|
			     BRASERO_MEDIUM_DL;
		break;
	case BRASERO_SCSI_PROF_DVD_R_DL_JUMP:
		priv->info = BRASERO_MEDIUM_DVD|
			     BRASERO_MEDIUM_WRITABLE|
			     BRASERO_MEDIUM_JUMP|
			     BRASERO_MEDIUM_DL;
		break;
	case BRASERO_SCSI_PROF_DVD_RW_PLUS:
		priv->info = BRASERO_MEDIUM_DVD|
			     BRASERO_MEDIUM_WRITABLE|
			     BRASERO_MEDIUM_REWRITABLE|
			     BRASERO_MEDIUM_PLUS;
		break;

	case BRASERO_SCSI_PROF_DVD_R_PLUS:
		priv->info = BRASERO_MEDIUM_DVD|
			     BRASERO_MEDIUM_WRITABLE|
			     BRASERO_MEDIUM_PLUS;
		break;

	case BRASERO_SCSI_PROF_DVD_RW_PLUS_DL:
		priv->info = BRASERO_MEDIUM_DVD|
			     BRASERO_MEDIUM_WRITABLE|
			     BRASERO_MEDIUM_REWRITABLE|
		 	     BRASERO_MEDIUM_PLUS|
		 	     BRASERO_MEDIUM_DL;
		break;

	case BRASERO_SCSI_PROF_DVD_R_PLUS_DL:
		priv->info = BRASERO_MEDIUM_DVD|
			     BRASERO_MEDIUM_WRITABLE|
			     BRASERO_MEDIUM_PLUS|
			     BRASERO_MEDIUM_DL;
		break;

	case BRASERO_SCSI_PROF_NON_REMOVABLE:
	case BRASERO_SCSI_PROF_REMOVABLE:
	case BRASERO_SCSI_PROF_MO_ERASABLE:
	case BRASERO_SCSI_PROF_MO_WRITE_ONCE:
	case BRASERO_SCSI_PROF_MO_ADVANCED_STORAGE:
	case BRASERO_SCSI_PROF_BD_ROM:
	case BRASERO_SCSI_PROF_BR_R_SEQUENTIAL:
	case BRASERO_SCSI_PROF_BR_R_RANDOM:
	case BRASERO_SCSI_PROF_BD_RW:
	case BRASERO_SCSI_PROF_DDCD_ROM:
	case BRASERO_SCSI_PROF_DDCD_R:
	case BRASERO_SCSI_PROF_DDCD_RW:
	case BRASERO_SCSI_PROF_HD_DVD_ROM:
	case BRASERO_SCSI_PROF_HD_DVD_R:
	case BRASERO_SCSI_PROF_HD_DVD_RAM:
	case BRASERO_SCSI_PROF_DVD_RAM:
		g_free (hdr);
		return BRASERO_BURN_NOT_SUPPORTED;
	}

	g_free (hdr);
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_medium_get_info (BraseroMedium *self,
			 int fd,
			 BraseroScsiErrCode *code)
{
	int size;
	BraseroScsiResult result;
	BraseroMediumPrivate *priv;
	BraseroScsiDiscInfoStd *info = NULL;

	priv = BRASERO_MEDIUM_PRIVATE (self);

	result = brasero_mmc1_read_disc_information_std (fd,
							 &info,
							 &size,
							 code);
	if (result != BRASERO_SCSI_OK) {
		g_free (info);
		return BRASERO_BURN_ERR;
	}

	switch (info->status) {
	case BRASERO_SCSI_DISC_EMPTY:
		priv->info |= BRASERO_MEDIUM_BLANK;
		break;
	case BRASERO_SCSI_DISC_INCOMPLETE:
		priv->info |= BRASERO_MEDIUM_APPENDABLE;
		priv->next_wr_add = BRASERO_MSF_TO_LBA (info->last_session_leadin [3],
							info->last_session_leadin [2],
							info->last_session_leadin [1]) + 
							1;
		break;
	case BRASERO_SCSI_DISC_FINALIZED:
	case BRASERO_SCSI_DISC_OTHERS:
		priv->info &= ~BRASERO_MEDIUM_WRITABLE;
	default:
		break;
	}

	g_free (info);
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_medium_get_sessions_info (BraseroMedium *self,
				  int fd,
				  BraseroScsiErrCode *code)
{
	int num, i, size;
	BraseroScsiResult result;
	BraseroScsiTocDesc *desc;
	BraseroMediumPrivate *priv;
	BraseroScsiFormattedTocData *toc = NULL;

	priv = BRASERO_MEDIUM_PRIVATE (self);

	result = brasero_mmc1_read_toc_formatted (fd,
						  1,
						  &toc,
						  &size,
						  code);
	if (result != BRASERO_SCSI_OK) {
		g_free (toc);
		return BRASERO_BURN_ERR;
	}

	num = (BRASERO_GET_16 (toc->hdr->len) +
	      sizeof (toc->hdr->len) -
	      sizeof (BraseroScsiTocPmaAtipHdr)) /
	      sizeof (BraseroScsiTocDesc);
	
	desc = toc->desc;
	for (i = 0; i < num; i ++, desc ++) {
		BraseroMediumTrack *track;

		track = g_new0 (BraseroMediumTrack, 1);
		priv->tracks = g_slist_prepend (priv->tracks, track);
		track->start = BRASERO_GET_32 (desc->track_start);

		if (desc->track_num == BRASERO_SCSI_TRACK_LEADOUT_START) {
			track->type = BRASERO_MEDIUM_TRACK_LEADOUT;
			break;
		}

		if ((desc->control & BRASERO_SCSI_TRACK_DATA) == 0) {
			track->type = BRASERO_MEDIUM_TRACK_AUDIO;
			priv->info |= BRASERO_MEDIUM_HAS_AUDIO;

			if (desc->control & BRASERO_SCSI_TRACK_PREEMP)
				track->type |= BRASERO_MEDIUM_TRACK_PREEMP;

			if (desc->control & BRASERO_SCSI_TRACK_4_CHANNELS)
				track->type |= BRASERO_MEDIUM_TRACK_4_CHANNELS;
		}
		else {
			track->type = BRASERO_MEDIUM_TRACK_DATA;
			priv->info |= BRASERO_MEDIUM_HAS_DATA;

			if (desc->control & BRASERO_SCSI_TRACK_DATA_INCREMENTAL)
				track->type |= BRASERO_MEDIUM_TRACK_INCREMENTAL;
		}

		if (desc->control & BRASERO_SCSI_TRACK_COPY)
			track->type |= BRASERO_MEDIUM_TRACK_COPY;

	}

	priv->tracks = g_slist_reverse (priv->tracks);
	g_free (toc);
	return BRASERO_BURN_OK;
}

static void
brasero_medium_init_real (BraseroMedium *object)
{
	int fd;
	const gchar *path;
	BraseroBurnResult result;
	BraseroMediumPrivate *priv;
	BraseroScsiErrCode code = 0;

	priv = BRASERO_MEDIUM_PRIVATE (object);
	path = nautilus_burn_drive_get_device (priv->drive);

	fd = open (path, O_RDONLY);
	if (fd < 1)
		return;

	result = brasero_medium_get_medium_type (object, fd, &code);
	if (result != BRASERO_BURN_OK)
		goto end;

	result = brasero_medium_get_info (object, fd, &code);
	if (result != BRASERO_BURN_OK)
		goto end;

	result = brasero_medium_get_sessions_info (object, fd, &code);
	if (result != BRASERO_BURN_OK)
		goto end;

end:

	close (fd);
}

static void
brasero_medium_init (BraseroMedium *object)
{
	BraseroMediumPrivate *priv;

	priv = BRASERO_MEDIUM_PRIVATE (object);
	priv->next_wr_add = -1;

	/* we can't do anything here since properties haven't been set yet */
}

static void
brasero_medium_finalize (GObject *object)
{
	BraseroMediumPrivate *priv;

	priv = BRASERO_MEDIUM_PRIVATE (object);

	g_slist_foreach (priv->tracks, (GFunc) g_free, NULL);
	g_slist_free (priv->tracks);

	nautilus_burn_drive_unref (priv->drive);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
brasero_medium_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	BraseroMediumPrivate *priv;

	g_return_if_fail (BRASERO_IS_MEDIUM (object));

	priv = BRASERO_MEDIUM_PRIVATE (object);

	switch (prop_id)
	{
	case PROP_DRIVE:
		priv->drive = g_value_get_object (value);
		nautilus_burn_drive_ref (priv->drive);
		brasero_medium_init_real (BRASERO_MEDIUM (object));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
brasero_medium_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	BraseroMediumPrivate *priv;

	g_return_if_fail (BRASERO_IS_MEDIUM (object));

	priv = BRASERO_MEDIUM_PRIVATE (object);

	switch (prop_id)
	{
	case PROP_DRIVE:
		nautilus_burn_drive_ref (priv->drive);
		g_value_set_object (value, priv->drive);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
brasero_medium_class_init (BraseroMediumClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	parent_class = G_OBJECT_CLASS (g_type_class_peek_parent (klass));

	g_type_class_add_private (klass, sizeof (BraseroMediumPrivate));

	object_class->finalize = brasero_medium_finalize;
	object_class->set_property = brasero_medium_set_property;
	object_class->get_property = brasero_medium_get_property;

	g_object_class_install_property (object_class,
	                                 PROP_DRIVE,
	                                 g_param_spec_object ("drive",
	                                                      "drive",
	                                                      "drive in which medium is inserted",
	                                                      NAUTILUS_BURN_TYPE_DRIVE,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

GType
brasero_medium_get_type (void)
{
	static GType our_type = 0;

	if (our_type == 0)
	{
		static const GTypeInfo our_info =
		{
			sizeof (BraseroMediumClass), /* class_size */
			(GBaseInitFunc) NULL, /* base_init */
			(GBaseFinalizeFunc) NULL, /* base_finalize */
			(GClassInitFunc) brasero_medium_class_init, /* class_init */
			(GClassFinalizeFunc) NULL, /* class_finalize */
			NULL /* class_data */,
			sizeof (BraseroMedium), /* instance_size */
			0, /* n_preallocs */
			(GInstanceInitFunc) brasero_medium_init, /* instance_init */
			NULL /* value_table */
		};

		our_type = g_type_register_static (G_TYPE_OBJECT, "BraseroMedium",
		                                   &our_info, 0);
	}

	return our_type;
}

BraseroMedium *
brasero_medium_new (NautilusBurnDrive *drive)
{
	g_return_val_if_fail (drive != NULL, NULL);
	return BRASERO_MEDIUM (g_object_new (BRASERO_TYPE_MEDIUM,
					     "drive", drive,
					     NULL));
}
