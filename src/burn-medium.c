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
#include "scsi-mmc3.h"
#include "scsi-spc1.h"
#include "scsi-utils.h"
#include "scsi-mode-pages.h"
#include "scsi-status-page.h"
#include "scsi-q-subchannel.h"
#include "scsi-dvd-structures.h"
#include "burn-volume.h"
#include "brasero-ncb.h"

const gchar *icons [] = { 	"gnome-dev-removable",
				"gnome-dev-cdrom",
				"gnome-dev-disc-cdr",
				"gnome-dev-disc-cdrw",
				"gnome-dev-disc-dvdrom",
				"gnome-dev-disc-dvdr",
				"gnome-dev-disc-dvdrw",
				"gnome-dev-disc-dvdr-plus",
				"gnome-dev-disc-dvdram",
				NULL };
const gchar *types [] = {	"file",
				"CDROM",
				"CD-R",
				"CD-RW",
				"DVDROM",
				"DVD-R",
				"DVD-RW",
				"DVD+R",
				"DVD+RW",
				"DVD+R dual layer",
				"DVD+RW dual layer",
				"DVD-R dual layer",
				"DVD-RAM",
				"blue ray disc",
				"writable blue ray disc",
				"rewritable blue ray disc",
				NULL };


typedef struct _BraseroMediumPrivate BraseroMediumPrivate;
struct _BraseroMediumPrivate
{
	GSList * tracks;

	const gchar *type;
	const gchar *icon;

	gint max_rd;
	gint max_wrt;

	gint *rd_speeds;
	gint *wr_speeds;

	gint64 block_num;
	gint64 block_size;

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

const gchar *
brasero_medium_get_type_string (BraseroMedium *medium)
{
	BraseroMediumPrivate *priv;

	priv = BRASERO_MEDIUM_PRIVATE (medium);
	return priv->type;
}

const gchar *
brasero_medium_get_icon (BraseroMedium *medium)
{
	BraseroMediumPrivate *priv;

	priv = BRASERO_MEDIUM_PRIVATE (medium);
	return priv->icon;
}

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

gint
brasero_medium_get_max_write_speed (BraseroMedium *medium)
{
	BraseroMediumPrivate *priv;

	priv = BRASERO_MEDIUM_PRIVATE (medium);
	return priv->max_wrt;
}

/**
 * NOTEs about the following functions:
 * for all closed media (including ROM types) capacity == size of data and 
 * should be the size of all data on the disc, free space is 0
 * for all blank -R types capacity == free space and size of data == 0
 * for all multisession -R types capacity == free space since having the real
 * capacity of the media would be useless as we can only use this type of media
 * to append more data
 * for all -RW types capacity = free space + size of data. Here they can be 
 * appended (use free space) or rewritten (whole capacity).
 *
 * Usually:
 * the free space is the size of the leadout track
 * the size of data is the sum of track sizes (excluding leadout)
 * the capacity depends on the media:
 * for closed discs == sum of track sizes
 * for multisession discs == free space (leadout size)
 * for blank discs == (free space) leadout size
 * for rewritable/blank == use SCSI functions to get capacity (see below)
 *
 * In fact we should really need the size of data in DVD+/-RW cases since the
 * session is always equal to the size of the disc. 
 */

void
brasero_medium_get_data_size (BraseroMedium *medium,
			      gint64 *size,
			      gint64 *blocks)
{
	GSList *iter;
	BraseroMediumPrivate *priv;
	BraseroMediumTrack *track = NULL;

	priv = BRASERO_MEDIUM_PRIVATE (medium);

	for (iter = priv->tracks; iter; iter = iter->next) {
		BraseroMediumTrack *tmp;

		tmp = iter->data;
		if (tmp->type == BRASERO_MEDIUM_TRACK_LEADOUT)
			break;

		track = iter->data;
	}

	if (size)
		*size = track ? (track->start + track->blocks_num) * priv->block_size: -1;

	if (blocks)
		*blocks = track ? track->start + track->blocks_num: -1;
}

void
brasero_medium_get_free_space (BraseroMedium *medium,
			       gint64 *size,
			       gint64 *blocks)
{
	GSList *iter;
	BraseroMediumPrivate *priv;
	BraseroMediumTrack *track = NULL;

	priv = BRASERO_MEDIUM_PRIVATE (medium);

	for (iter = priv->tracks; iter; iter = iter->next) {
		BraseroMediumTrack *tmp;

		tmp = iter->data;
		if (tmp->type == BRASERO_MEDIUM_TRACK_LEADOUT) {
			track = iter->data;
			break;
		}
	}

	if (size)
		*size = track ? track->blocks_num * priv->block_size: -1;

	if (blocks)
		*blocks = track ? track->blocks_num: -1;
}

void
brasero_medium_get_capacity (BraseroMedium *medium,
			     gint64 *size,
			     gint64 *blocks)
{
	BraseroMediumPrivate *priv;

	priv = BRASERO_MEDIUM_PRIVATE (medium);

	if (priv->info & BRASERO_MEDIUM_REWRITABLE) {
		if (size)
			*size = priv->block_num * priv->block_size;

		if (blocks)
			*blocks = priv->block_num;
	}
	else
		brasero_medium_get_free_space (medium, size, blocks);
}

/**
 * Function to retrieve the capacity of a media
 */

static BraseroBurnResult
brasero_medium_get_capacity_CD_RW (BraseroMedium *self,
				   int fd,
				   BraseroScsiErrCode *code)
{
	BraseroScsiAtipData atip_data;
	BraseroMediumPrivate *priv;
	BraseroScsiResult result;

	priv = BRASERO_MEDIUM_PRIVATE (self);

	result = brasero_mmc1_read_atip (fd,
					 &atip_data,
					 sizeof (atip_data),
					 NULL);

	if (result != BRASERO_SCSI_OK)
		return BRASERO_BURN_ERR;

	priv->block_num = BRASERO_MSF_TO_LBA (atip_data.desc->leadout_mn,
					      atip_data.desc->leadout_sec,
					      atip_data.desc->leadout_frame);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_medium_get_capacity_DVD_RW (BraseroMedium *self,
				    int fd,
				    BraseroScsiErrCode *code)
{
	BraseroScsiFormatCapacitiesHdr *hdr = NULL;
	BraseroScsiMaxCapacityDesc *current;
	BraseroMediumPrivate *priv;
	BraseroScsiResult result;
	gint size;

	priv = BRASERO_MEDIUM_PRIVATE (self);
	result = brasero_mmc2_read_format_capacities (fd,
						      &hdr,
						      &size,
						      code);
	if (result != BRASERO_SCSI_OK) {
		g_free (hdr);
		return BRASERO_BURN_ERR;
	}

	current = hdr->max_caps;

	/* see if the media is already formatted */
	if (current->type != BRASERO_SCSI_DESC_FORMATTED) {
		int i, max;
		BraseroScsiFormattableCapacityDesc *desc;

		max = (hdr->len - 
		      sizeof (BraseroScsiMaxCapacityDesc)) /
		      sizeof (BraseroScsiFormattableCapacityDesc);

		desc = hdr->desc;
		for (i = 0; i < max; i ++, desc ++) {
			/* search for the correct descriptor */
			if (BRASERO_MEDIUM_IS (priv->info, BRASERO_MEDIUM_DVDRW_PLUS)) {
				if (desc->format_type == BRASERO_SCSI_DVDRW_PLUS) {
					priv->block_num = BRASERO_GET_32 (desc->blocks_num);
					priv->block_size = BRASERO_GET_24 (desc->type_param);
					break;
				}
			}
			else if (desc->format_type == BRASERO_SCSI_BLOCK_SIZE_DEFAULT_AND_DB) {
				priv->block_num = BRASERO_GET_32 (desc->blocks_num);
				priv->block_size = BRASERO_GET_24 (desc->type_param);
				break;
			}
		}
	}
	else {
		priv->block_num = BRASERO_GET_32 (current->blocks_num);
		priv->block_size = BRASERO_GET_24 (current->block_size);
	}

	g_free (hdr);
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_medium_get_capacity_by_type (BraseroMedium *self,
				     int fd,
				     BraseroScsiErrCode *code)
{
	BraseroMediumPrivate *priv;

	priv = BRASERO_MEDIUM_PRIVATE (self);

	priv->block_size = 2048;

	if (!(priv->info & BRASERO_MEDIUM_REWRITABLE))
		return BRASERO_BURN_OK;

	if (priv->info & BRASERO_MEDIUM_CD)
		brasero_medium_get_capacity_CD_RW (self, fd, code);
	else
		brasero_medium_get_capacity_DVD_RW (self, fd, code);

	return BRASERO_BURN_OK;
}

/**
 * Functions to retrieve the speed
 */

static BraseroBurnResult
brasero_medium_get_speed_mmc3 (BraseroMedium *self,
			       int fd,
			       BraseroScsiErrCode *code)
{
	int size;
	int num_desc, i;
	gint max_rd, max_wrt;
	BraseroScsiResult result;
	BraseroMediumPrivate *priv;
	BraseroScsiWrtSpdDesc *desc;
	BraseroScsiGetPerfData *wrt_perf = NULL;

	/* NOTE: this only work if there is RT streaming feature with
	 * wspd bit set to 1. At least an MMC3 drive. */
	priv = BRASERO_MEDIUM_PRIVATE (self);
	result = brasero_mmc3_get_performance_wrt_spd_desc (fd,
							    &wrt_perf,
							    &size,
							    code);

	if (result != BRASERO_SCSI_OK) {
		g_free (wrt_perf);
		return BRASERO_BURN_ERR;
	}

	num_desc = (size - sizeof (BraseroScsiGetPerfHdr)) /
		    sizeof (BraseroScsiWrtSpdDesc);

	priv->rd_speeds = g_new0 (gint, num_desc + 1);
	priv->wr_speeds = g_new0 (gint, num_desc + 1);

	max_rd = 0;
	max_wrt = 0;

	desc = (BraseroScsiWrtSpdDesc*) &wrt_perf->data;
	for (i = 0; i < num_desc; i ++, desc ++) {
		priv->rd_speeds [i] = BRASERO_GET_32 (desc->rd_speed);
		priv->wr_speeds [i] = BRASERO_GET_32 (desc->wr_speed);

		max_rd = MAX (max_rd, priv->rd_speeds [i]);
		max_wrt = MAX (max_wrt, priv->wr_speeds [i]);
	}

	priv->max_rd = max_rd;
	priv->max_wrt = max_wrt;

	g_free (wrt_perf);

	/* strangely there are so drives (I know one case) which support this
	 * function but don't report any speed. So if our top speed is 0 then
	 * use the other way to get the speed. It was a Teac */
	if (!priv->max_wrt)
		return BRASERO_BURN_RETRY;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_medium_get_page_2A_write_speed_desc (BraseroMedium *self,
					     int fd,
					     BraseroScsiErrCode *code)
{
	BraseroScsiStatusPage *page_2A = NULL;
	BraseroScsiStatusWrSpdDesc *desc;
	BraseroScsiModeData *data = NULL;
	BraseroMediumPrivate *priv;
	BraseroScsiResult result;
	gint desc_num, i;
	gint max_wrt = 0;
	gint max_num;
	int size = 0;

	priv = BRASERO_MEDIUM_PRIVATE (self);
	result = brasero_spc1_mode_sense_get_page (fd,
						   BRASERO_SPC_PAGE_STATUS,
						   &data,
						   &size,
						   code);
	if (result != BRASERO_SCSI_OK) {
		g_free (data);
		return BRASERO_BURN_ERR;
	}

	page_2A = (BraseroScsiStatusPage *) &data->page;

	if (size < sizeof (BraseroScsiStatusPage)) {
		g_free (data);
		return BRASERO_BURN_ERR;
	}

	desc_num = BRASERO_GET_16 (page_2A->wr_speed_desc_num);
	max_num = size -
		  sizeof (BraseroScsiStatusPage) -
		  sizeof (BraseroScsiModeHdr);

	if (max_num < 0)
		max_num = 0;

	if (desc_num > max_num)
		desc_num = max_num;

	priv->wr_speeds = g_new0 (gint, desc_num + 1);
	desc = page_2A->wr_spd_desc;
	for (i = 0; i < desc_num; i ++, desc ++) {
		priv->wr_speeds [i] = BRASERO_GET_16 (desc->speed);
		max_wrt = MAX (max_wrt, priv->wr_speeds [i]);
	}

	if (!max_wrt)
		priv->max_wrt = BRASERO_GET_16 (page_2A->wr_max_speed);
	else
		priv->max_wrt = max_wrt;

	priv->max_rd = BRASERO_GET_16 (page_2A->rd_max_speed);
	g_free (data);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_medium_get_page_2A_max_speed (BraseroMedium *self,
				      int fd,
				      BraseroScsiErrCode *code)
{
	BraseroScsiStatusPage *page_2A = NULL;
	BraseroScsiModeData *data = NULL;
	BraseroMediumPrivate *priv;
	BraseroScsiResult result;
	int size = 0;

	priv = BRASERO_MEDIUM_PRIVATE (self);

	result = brasero_spc1_mode_sense_get_page (fd,
						   BRASERO_SPC_PAGE_STATUS,
						   &data,
						   &size,
						   code);
	if (result != BRASERO_SCSI_OK) {
		g_free (data);
		return BRASERO_BURN_ERR;
	}

	page_2A = (BraseroScsiStatusPage *) &data->page;

	if (size < 0x14) {
		g_free (data);
		return BRASERO_BURN_ERR;
	}

	priv->max_rd = BRASERO_GET_16 (page_2A->rd_max_speed);
	priv->max_wrt = BRASERO_GET_16 (page_2A->wr_max_speed);

	g_free (data);
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_medium_get_medium_type (BraseroMedium *self,
				int fd,
				BraseroScsiErrCode *code)
{
	BraseroScsiGetConfigHdr *hdr = NULL;
	BraseroScsiRTStreamDesc *stream;
	BraseroMediumPrivate *priv;
	BraseroScsiResult result;
	int size;

	priv = BRASERO_MEDIUM_PRIVATE (self);
	result = brasero_mmc2_get_configuration_feature (fd,
							 BRASERO_SCSI_FEAT_REAL_TIME_STREAM,
							 &hdr,
							 &size,
							 code);
	if (result == BRASERO_SCSI_INVALID_COMMAND) {
		g_free (hdr);

		/* This is probably a MMC1 drive since this command was
		 * introduced in MMC2 and is supported onward. So it
		 * has to be a CD (R/RW). The rest of the information
		 * will be provided by read_disc_information. */
		priv->info = BRASERO_MEDIUM_CD;

		result = brasero_medium_get_page_2A_max_speed (self, fd, code);
		return result;
	}

	if (result != BRASERO_SCSI_OK) {
		/* All other commands means an error */
		g_free (hdr);
		return BRASERO_BURN_ERR;
	}

	switch (BRASERO_GET_16 (hdr->current_profile)) {
	case BRASERO_SCSI_PROF_CDROM:
		priv->info = BRASERO_MEDIUM_CD;
		priv->type = types [1];
		priv->icon = icons [1];
		break;

	case BRASERO_SCSI_PROF_CDR:
		priv->info = BRASERO_MEDIUM_CDR;
		priv->type = types [2];
		priv->icon = icons [2];
		break;

	case BRASERO_SCSI_PROF_CDRW:
		priv->info = BRASERO_MEDIUM_CDRW;
		priv->type = types [3];
		priv->icon = icons [3];
		break;

	case BRASERO_SCSI_PROF_DVD_ROM:
		priv->info = BRASERO_MEDIUM_DVD;
		priv->type = types [4];
		priv->icon = icons [4];
		break;

	case BRASERO_SCSI_PROF_DVD_R:
		priv->info = BRASERO_MEDIUM_DVDR;
		priv->type = types [5];
		priv->icon = icons [5];
		break;

	case BRASERO_SCSI_PROF_DVD_RW_RESTRICTED:
		priv->info = BRASERO_MEDIUM_DVDRW_RESTRICTED;
		priv->type = types [6];
		priv->icon = icons [6];
		break;

	case BRASERO_SCSI_PROF_DVD_RW_SEQUENTIAL:
		priv->info = BRASERO_MEDIUM_DVDRW;
		priv->type = types [6];
		priv->icon = icons [6];
		break;

	case BRASERO_SCSI_PROF_DVD_R_PLUS:
		priv->info = BRASERO_MEDIUM_DVDR_PLUS;
		priv->type = types [7];
		priv->icon = icons [7];
		break;

	case BRASERO_SCSI_PROF_DVD_RW_PLUS:
		priv->info = BRASERO_MEDIUM_DVDRW_PLUS;
		priv->type = types [8];
		priv->icon = icons [7];
		break;

	case BRASERO_SCSI_PROF_DVD_R_PLUS_DL:
		priv->info = BRASERO_MEDIUM_DVDR_PLUS_DL;
		priv->type = types [9];
		priv->icon = icons [7];
		break;

	case BRASERO_SCSI_PROF_DVD_RW_PLUS_DL:
		priv->info = BRASERO_MEDIUM_DVDRW_PLUS_DL;
		priv->type = types [10];
		priv->icon = icons [7];
		break;

	case BRASERO_SCSI_PROF_DVD_R_DL_SEQUENTIAL:
		priv->info = BRASERO_MEDIUM_DVDR_DL;
		priv->type = types [11];
		priv->icon = icons [5];
		break;

	case BRASERO_SCSI_PROF_DVD_R_DL_JUMP:
		priv->info = BRASERO_MEDIUM_DVDR_JUMP_DL;
		priv->type = types [11];
		priv->icon = icons [5];
		break;

	case BRASERO_SCSI_PROF_DVD_RAM:
		priv->info = BRASERO_MEDIUM_DVD_RAM;
		priv->type = types [12];
		priv->icon = icons [8];
		break;

	case BRASERO_SCSI_PROF_BD_ROM:
		priv->info = BRASERO_MEDIUM_BD_ROM;
		priv->type = types [13];
		priv->icon = icons [4];
		break;

	case BRASERO_SCSI_PROF_BR_R_SEQUENTIAL:
		priv->info = BRASERO_MEDIUM_BDR;
		priv->type = types [14];
		priv->icon = icons [5];
		break;

	case BRASERO_SCSI_PROF_BR_R_RANDOM:
		priv->info = BRASERO_MEDIUM_BDR_RANDOM;
		priv->type = types [14];
		priv->icon = icons [5];
		break;

	case BRASERO_SCSI_PROF_BD_RW:
		priv->info = BRASERO_MEDIUM_BDRW;
		priv->type = types [15];
		priv->icon = icons [6];
		break;

	case BRASERO_SCSI_PROF_NON_REMOVABLE:
	case BRASERO_SCSI_PROF_REMOVABLE:
	case BRASERO_SCSI_PROF_MO_ERASABLE:
	case BRASERO_SCSI_PROF_MO_WRITE_ONCE:
	case BRASERO_SCSI_PROF_MO_ADVANCED_STORAGE:
	case BRASERO_SCSI_PROF_DDCD_ROM:
	case BRASERO_SCSI_PROF_DDCD_R:
	case BRASERO_SCSI_PROF_DDCD_RW:
	case BRASERO_SCSI_PROF_HD_DVD_ROM:
	case BRASERO_SCSI_PROF_HD_DVD_R:
	case BRASERO_SCSI_PROF_HD_DVD_RAM:
		priv->info = BRASERO_MEDIUM_UNSUPPORTED;
		g_free (hdr);
		return BRASERO_BURN_NOT_SUPPORTED;
	}

	/* see how we should get the speeds */
	if (hdr->desc->add_len != sizeof (BraseroScsiRTStreamDesc)) {
		g_free (hdr);
		return BRASERO_BURN_ERR;
	}

	/* try all SCSI functions to get write/read speeds in order */
	stream = (BraseroScsiRTStreamDesc *) hdr->desc->data;
	if (stream->wrt_spd) {
		result = brasero_medium_get_speed_mmc3 (self, fd, code);
		if (result != BRASERO_BURN_RETRY)
			goto end;
	}

	if (stream->mp2a)
		result = brasero_medium_get_page_2A_write_speed_desc (self, fd, code);
	else
		result = brasero_medium_get_page_2A_max_speed (self, fd, code);


end:

	g_free (hdr);

	if (result != BRASERO_BURN_OK)
		return BRASERO_BURN_ERR;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_medium_get_css_feature (BraseroMedium *self,
				int fd,
				BraseroScsiErrCode *code)
{
	BraseroScsiGetConfigHdr *hdr = NULL;
	BraseroMediumPrivate *priv;
	BraseroScsiResult result;
	int size;

	priv = BRASERO_MEDIUM_PRIVATE (self);
	result = brasero_mmc2_get_configuration_feature (fd,
							 BRASERO_SCSI_FEAT_DVD_CSS,
							 &hdr,
							 &size,
							 code);
	if (result != BRASERO_SCSI_OK) {
		g_free (hdr);
		return BRASERO_BURN_ERR;
	}

	if (hdr->desc->add_len != sizeof (BraseroScsiDVDCssDesc)) {
		g_free (hdr);
		return BRASERO_BURN_ERR;
	}

	/* here we just need to see if this feature is current or not */
	if (hdr->desc->current)
		priv->info |= BRASERO_MEDIUM_PROTECTED;

	g_free (hdr);
	return BRASERO_BURN_OK;
}

/**
 * Functions to get information about disc contents
 */

/**
 * NOTE: for DVD-R multisession we lose 28688 blocks for each session
 * so the capacity is the addition of all session sizes + 28688 for each
 * For all multisession DVD-/+R and CDR-RW the remaining size is given 
 * in the leadout. One exception though with DVD+/-RW.
 */

static BraseroBurnResult
brasero_medium_track_volume_size (BraseroMedium *self,
				  BraseroMediumTrack *track,
				  int fd)
{
	BraseroMediumPrivate *priv;
	BraseroBurnResult res;
	gint64 nb_blocks;

	if (!track)
		return BRASERO_BURN_ERR;

	priv = BRASERO_MEDIUM_PRIVATE (self);

	/* This is a special case. For DVD+RW and DVD-RW in restricted
	 * mode, there is only one session that takes the whole disc size
	 * once formatted. That doesn't necessarily means they have data
	 * Note also that they are reported as complete though you can
	 * still add data (with growisofs). It is nevertheless on the 
	 * condition that the fs is valid.
	 * So we check if their first and only volume is valid. 
	 * That's also used when the track size is reported a 300 Kio
	 * see below */
	res = brasero_volume_get_size_fd (fd,
					  track->start,
					  &nb_blocks,
					  NULL);
	if (!res)
		return BRASERO_BURN_ERR;

	track->blocks_num = nb_blocks;
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_medium_track_get_info (BraseroMedium *self,
			       BraseroMediumTrack *track,
			       int track_num,
			       int fd,
			       BraseroScsiErrCode *code)
{
	BraseroScsiTrackInfo track_info;
	BraseroMediumPrivate *priv;
	BraseroScsiResult result;

	priv = BRASERO_MEDIUM_PRIVATE (self);

	result = brasero_mmc1_read_track_info (fd,
					       track_num,
					       &track_info,
					       sizeof (BraseroScsiTrackInfo),
					       code);

	if (result != BRASERO_SCSI_OK)
		return BRASERO_BURN_ERR;

	track->blocks_num = BRASERO_GET_32 (track_info.track_size);

	/* Now here is a potential bug: we can write tracks (data or not)
	 * shorter than 300 Kio /2 sec but they will be padded to reach this
	 * floor value. That means that is blocks_num is 300 blocks that may
	 * mean that the data length on the track is actually shorter.
	 * So we read the volume descriptor. If it works, good otherwise
	 * use the old value.
	 * That's important for checksuming to have a perfect account of the 
	 * data size. */
	if (track->blocks_num == 300)
		brasero_medium_track_volume_size (self, track, fd);

	if (track_info.next_wrt_address_valid)
		priv->next_wr_add = BRASERO_GET_32 (track_info.next_wrt_address);

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

		if (desc->track_num == BRASERO_SCSI_TRACK_LEADOUT_START)
			break;

		track = g_new0 (BraseroMediumTrack, 1);
		priv->tracks = g_slist_prepend (priv->tracks, track);
		track->start = BRASERO_GET_32 (desc->track_start);

		/* we shouldn't request info on a track if the disc is closed */
		brasero_medium_track_get_info (self,
					       track,
					       g_slist_length (priv->tracks),
					       fd,
					       code);

		if (desc->control & BRASERO_SCSI_TRACK_COPY)
			track->type |= BRASERO_MEDIUM_TRACK_COPY;

		if (!(desc->control & BRASERO_SCSI_TRACK_DATA)) {
			track->type |= BRASERO_MEDIUM_TRACK_AUDIO;
			priv->info |= BRASERO_MEDIUM_HAS_AUDIO;

			if (desc->control & BRASERO_SCSI_TRACK_PREEMP)
				track->type |= BRASERO_MEDIUM_TRACK_PREEMP;

			if (desc->control & BRASERO_SCSI_TRACK_4_CHANNELS)
				track->type |= BRASERO_MEDIUM_TRACK_4_CHANNELS;
		}
		else if (BRASERO_MEDIUM_IS (priv->info, BRASERO_MEDIUM_DVDRW_PLUS)
		     ||  BRASERO_MEDIUM_IS (priv->info, BRASERO_MEDIUM_DVDRW_RESTRICTED)) {
				BraseroBurnResult result;

			/* a special case for these two kinds of media
			 * which have only one track: the first. */
			result = brasero_medium_track_volume_size (self, 
								   track,
								   fd);
			if (result == BRASERO_BURN_OK) {
				track->type |= BRASERO_MEDIUM_TRACK_DATA;
				priv->info |= BRASERO_MEDIUM_APPENDABLE|
					      BRASERO_MEDIUM_HAS_DATA;
				priv->next_wr_add = 0;

				if (desc->control & BRASERO_SCSI_TRACK_DATA_INCREMENTAL)
					track->type |= BRASERO_MEDIUM_TRACK_INCREMENTAL;
			}
			else {
				priv->tracks = g_slist_remove (priv->tracks, track);
				g_free (track);

				priv->info |= BRASERO_MEDIUM_BLANK;
			}
		}
		else {
			track->type |= BRASERO_MEDIUM_TRACK_DATA;
			priv->info |= BRASERO_MEDIUM_HAS_DATA;

			if (desc->control & BRASERO_SCSI_TRACK_DATA_INCREMENTAL)
				track->type |= BRASERO_MEDIUM_TRACK_INCREMENTAL;
		}
	}

	/* we shouldn't request info on leadout if the disc is closed */
	if (priv->info & (BRASERO_MEDIUM_APPENDABLE|BRASERO_MEDIUM_BLANK)) {
		BraseroMediumTrack *track;

		track = g_new0 (BraseroMediumTrack, 1);
		priv->tracks = g_slist_prepend (priv->tracks, track);
		track->start = BRASERO_GET_32 (desc->track_start);
		track->type = BRASERO_MEDIUM_TRACK_LEADOUT;

		brasero_medium_track_get_info (self,
					       track,
					       g_slist_length (priv->tracks),
					       fd,
					       code);
	}

	/* put the tracks in the right order */
	priv->tracks = g_slist_reverse (priv->tracks);

	if (priv->tracks
	&& (BRASERO_MEDIUM_IS (priv->info, BRASERO_MEDIUM_DVDRW_PLUS)
	||  BRASERO_MEDIUM_IS (priv->info, BRASERO_MEDIUM_DVDRW_RESTRICTED))) {
		GSList *node;
		BraseroMediumTrack *leadout, *track;

		track = priv->tracks->data;
		node = g_slist_last (priv->tracks);
		leadout = node->data;
		leadout->blocks_num -= track->blocks_num;
	}

	g_free (toc);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_medium_get_contents (BraseroMedium *self,
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

	if (info->erasable)
		priv->info |= BRASERO_MEDIUM_REWRITABLE;

	if (info->status == BRASERO_SCSI_DISC_EMPTY) {
		BraseroMediumTrack *track;

		priv->info |= BRASERO_MEDIUM_BLANK;
		priv->block_size = 2048;

		track = g_new0 (BraseroMediumTrack, 1);
		track->start = 0;
		track->type = BRASERO_MEDIUM_TRACK_LEADOUT;
		priv->tracks = g_slist_prepend (priv->tracks, track);
		brasero_medium_track_get_info (self, track, 1, fd, code);
		goto end;
	}

	if (info->status == BRASERO_SCSI_DISC_INCOMPLETE) {
		priv->info |= BRASERO_MEDIUM_APPENDABLE;
	}

	result = brasero_medium_get_sessions_info (self, fd, code);
	if (result != BRASERO_BURN_OK)
		goto end;

end:

	g_free (info);
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

	if (priv->info & BRASERO_MEDIUM_DVD) {
		result = brasero_medium_get_css_feature (object, fd, &code);
		if (result != BRASERO_BURN_OK)
			goto end;
	}

	result = brasero_medium_get_contents (object, fd, &code);
	if (result != BRASERO_BURN_OK)
		goto end;

	brasero_medium_get_capacity_by_type (object, fd, &code);

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

	g_free (priv->rd_speeds);
	g_free (priv->wr_speeds);

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
