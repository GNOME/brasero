/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2007-2008 <bonfire-app@wanadoo.fr>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#include "burn-basics.h"
#include "burn-debug.h"
#include "burn-medium.h"
#include "burn-drive.h"

#include "scsi-device.h"
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
#include "burn-drive.h"

const gchar *icons [] = { 	"iso-image-new",
				"gnome-dev-cdrom",
				"gnome-dev-disc-cdr",
				"gnome-dev-disc-cdrw",
				"gnome-dev-disc-dvdrom",
				"gnome-dev-disc-dvdr",
				"gnome-dev-disc-dvdrw",
				"gnome-dev-disc-dvdr-plus",
				"gnome-dev-disc-dvdram",
				NULL };
const gchar *types [] = {	N_("file"),
				N_("CDROM"),
				N_("CD-R"),
				N_("CD-RW"),
				N_("DVDROM"),
				N_("DVD-R"),
				N_("DVD-RW"),
				N_("DVD+R"),
				N_("DVD+RW"),
				N_("DVD+R dual layer"),
				N_("DVD+RW dual layer"),
				N_("DVD-R dual layer"),
				N_("DVD-RAM"),
				N_("Blu-ray disc"),
				N_("Writable Blu-ray disc"),
				N_("Rewritable Blu-ray disc"),
				NULL };


typedef struct _BraseroMediumPrivate BraseroMediumPrivate;
struct _BraseroMediumPrivate
{
	gint retry_id;

	GSList *tracks;

	const gchar *type;
	const gchar *icon;

	gchar *udi;

	gint max_rd;
	gint max_wrt;

	gint *rd_speeds;
	gint *wr_speeds;

	gint64 block_num;
	gint64 block_size;

	guint64 next_wr_add;
	BraseroMedia info;
	BraseroDrive *drive;
};

#define BRASERO_MEDIUM_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BRASERO_TYPE_MEDIUM, BraseroMediumPrivate))

/**
 * Try to open the drive exclusively but don't block; if drive can't be opened
 * exclusively then retry every second until we're shut or the drive state
 * changes to not busy.
 * No exclusive at the moment since when the medium is mounted we can't use excl
 */

#define BUSY_RETRY_TIME			1000

enum
{
	PROP_0,
	PROP_DRIVE,
	PROP_UDI
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

	if (!medium)
		return NULL;

	priv = BRASERO_MEDIUM_PRIVATE (medium);
	return priv->icon;
}

BraseroMedia
brasero_medium_get_status (BraseroMedium *medium)
{
	BraseroMediumPrivate *priv;

	if (!medium)
		return BRASERO_MEDIUM_NONE;

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

gboolean
brasero_medium_get_last_data_track_address (BraseroMedium *medium,
					    gint64 *byte,
					    gint64 *sector)
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

	if (!track) {
		if (byte)
			*byte = -1;
		if (sector)
			*sector = -1;
		return FALSE;
	}

	if (byte)
		*byte = track->start * priv->block_size;

	if (sector)
		*sector = track->start;

	return TRUE;
}

gboolean
brasero_medium_get_last_data_track_space (BraseroMedium *medium,
					  gint64 *size,
					  gint64 *blocks)
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

	if (!track) {
		if (size)
			*size = -1;
		if (blocks)
			*blocks = -1;
		return FALSE;
	}

	if (size)
		*size = track->blocks_num * priv->block_size;
	if (blocks)
		*blocks = track->blocks_num;

	return TRUE;
}

guint
brasero_medium_get_track_num (BraseroMedium *medium)
{
	guint retval = 0;
	GSList *iter;
	BraseroMediumPrivate *priv;

	priv = BRASERO_MEDIUM_PRIVATE (medium);
	for (iter = priv->tracks; iter; iter = iter->next) {
		BraseroMediumTrack *current;

		current = iter->data;
		if (current->type & BRASERO_MEDIUM_TRACK_LEADOUT)
			break;

		retval ++;
	}

	return retval;
}

static BraseroMediumTrack *
brasero_medium_get_track (BraseroMedium *medium,
			  guint num)
{
	guint i = 1;
	GSList *iter;
	BraseroMediumPrivate *priv;

	priv = BRASERO_MEDIUM_PRIVATE (medium);

	for (iter = priv->tracks; iter; iter = iter->next) {
		BraseroMediumTrack *current;

		current = iter->data;
		if (current->type == BRASERO_MEDIUM_TRACK_LEADOUT)
			break;

		if (i == num)
			return current;

		i++;
	}

	return NULL;
}

gboolean
brasero_medium_get_track_space (BraseroMedium *medium,
				guint num,
				gint64 *size,
				gint64 *blocks)
{
	BraseroMediumPrivate *priv;
	BraseroMediumTrack *track;

	priv = BRASERO_MEDIUM_PRIVATE (medium);

	track = brasero_medium_get_track (medium, num);
	if (!track) {
		if (size)
			*size = -1;
		if (blocks)
			*blocks = -1;
		return FALSE;
	}

	if (size)
		*size = track->blocks_num * priv->block_size;
	if (blocks)
		*blocks = track->blocks_num;

	return TRUE;
}

gboolean
brasero_medium_get_track_address (BraseroMedium *medium,
				  guint num,
				  gint64 *byte,
				  gint64 *sector)
{
	BraseroMediumPrivate *priv;
	BraseroMediumTrack *track;

	priv = BRASERO_MEDIUM_PRIVATE (medium);

	track = brasero_medium_get_track (medium, num);
	if (!track) {
		if (byte)
			*byte = -1;
		if (sector)
			*sector = -1;
		return FALSE;
	}

	if (byte)
		*byte = track->start * priv->block_size;
	if (sector)
		*sector = track->start;

	return TRUE;	
}

gint64
brasero_medium_get_next_writable_address (BraseroMedium *medium)
{
	BraseroMediumPrivate *priv;

	priv = BRASERO_MEDIUM_PRIVATE (medium);

	/* There is one exception to this with closed DVD+RW/DVD-RW restricted */
	if (BRASERO_MEDIUM_IS (priv->info, BRASERO_MEDIUM_DVDRW_PLUS)
	||  BRASERO_MEDIUM_IS (priv->info, BRASERO_MEDIUM_DVDRW_RESTRICTED)
	||  BRASERO_MEDIUM_IS (priv->info, BRASERO_MEDIUM_DVDRW_PLUS_DL)) {
		BraseroMediumTrack *first;

		/* These are always writable so give the next address after the 
		 * last volume. */
		if (!priv->tracks)
			return 0;

		first = priv->tracks->data;

		/* round to the nearest 16th block */
		return (((first->start + first->blocks_num) + 15) / 16) * 16;
	}

	return priv->next_wr_add;
}

gint64
brasero_medium_get_max_write_speed (BraseroMedium *medium)
{
	BraseroMediumPrivate *priv;

	priv = BRASERO_MEDIUM_PRIVATE (medium);
	return priv->max_wrt * 1024;
}

gint64 *
brasero_medium_get_write_speeds (BraseroMedium *medium)
{
	BraseroMediumPrivate *priv;
	gint64 *speeds;
	guint max = 0;
	guint i;

	priv = BRASERO_MEDIUM_PRIVATE (medium);

	while (priv->wr_speeds [max] != 0) max ++;

	speeds = g_new0 (gint64, max + 1);
	for (i = 0; i < max; i ++)
		speeds [i] = priv->wr_speeds [i] * 1024;

	return speeds;
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

	if (!priv->tracks) {
		/* that's probably because it wasn't possible to retrieve info */
		if (size)
			*size = 0;

		if (blocks)
			*blocks = 0;

		return;
	}

	for (iter = priv->tracks; iter; iter = iter->next) {
		BraseroMediumTrack *tmp;

		tmp = iter->data;
		if (tmp->type == BRASERO_MEDIUM_TRACK_LEADOUT)
			break;

		track = iter->data;
	}

	if (size)
		*size = track ? (track->start + track->blocks_num) * priv->block_size: 0;

	if (blocks)
		*blocks = track ? track->start + track->blocks_num: 0;
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

	if (!priv->tracks) {
		/* that's probably because it wasn't possible to retrieve info.
		 * maybe it also happens with unformatted DVD+RW */

		if (priv->info & BRASERO_MEDIUM_CLOSED) {
			if (size)
				*size = 0;

			if (blocks)
				*blocks = 0;
		}
		else {
			if (size)
				*size = priv->block_num * priv->block_size;

			if (blocks)
				*blocks = priv->block_num;
		}

		return;
	}

	for (iter = priv->tracks; iter; iter = iter->next) {
		BraseroMediumTrack *tmp;

		tmp = iter->data;
		if (tmp->type == BRASERO_MEDIUM_TRACK_LEADOUT) {
			track = iter->data;
			break;
		}
	}

	if (size) {
		if (!track) {
			/* No leadout was found so the disc is probably closed:
			 * no free space left. */
			*size = 0;
		}
		else if (track->blocks_num <= 0)
			*size = (priv->block_num - track->start) * priv->block_size;
		else
			*size = track->blocks_num * priv->block_size;
	}

	if (blocks) {
		if (!track) {
			/* No leadout was found so the disc is probably closed:
			 * no free space left. */
			*blocks = 0;
		}
		else if (track->blocks_num <= 0)
			*blocks = priv->block_num - track->blocks_num;
		else
			*blocks = track->blocks_num;
	}
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
	else  if (priv->info & BRASERO_MEDIUM_CLOSED)
		brasero_medium_get_data_size (medium, size, blocks);
	else
		brasero_medium_get_free_space (medium, size, blocks);
}

/**
 * Function to retrieve the capacity of a media
 */

static BraseroBurnResult
brasero_medium_get_capacity_CD_RW (BraseroMedium *self,
				   BraseroDeviceHandle *handle,
				   BraseroScsiErrCode *code)
{
	BraseroScsiAtipData *atip_data = NULL;
	BraseroMediumPrivate *priv;
	BraseroScsiResult result;
	int size = 0;

	priv = BRASERO_MEDIUM_PRIVATE (self);

	BRASERO_BURN_LOG ("Retrieving capacity from atip");

	result = brasero_mmc1_read_atip (handle,
					 &atip_data,
					 &size,
					 NULL);

	if (result != BRASERO_SCSI_OK) {
		BRASERO_BURN_LOG ("READ ATIP failed (scsi error)");
		return BRASERO_BURN_ERR;
	}

	/* check the size of the structure: it must be at least 16 bytes long */
	if (size < 16) {
		if (size)
			g_free (atip_data);

		BRASERO_BURN_LOG ("READ ATIP failed (wrong size)");
		return BRASERO_BURN_ERR;
	}

	priv->block_num = BRASERO_MSF_TO_LBA (atip_data->desc->leadout_mn,
					      atip_data->desc->leadout_sec,
					      atip_data->desc->leadout_frame);
	g_free (atip_data);

	BRASERO_BURN_LOG ("Format capacity %lli %lli",
			  priv->block_num,
			  priv->block_size);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_medium_get_capacity_DVD_RW (BraseroMedium *self,
				    BraseroDeviceHandle *handle,
				    BraseroScsiErrCode *code)
{
	BraseroScsiFormatCapacitiesHdr *hdr = NULL;
	BraseroScsiFormattableCapacityDesc *desc;
	BraseroScsiMaxCapacityDesc *current;
	BraseroMediumPrivate *priv;
	BraseroScsiResult result;
	gint i, max;
	gint size;

	BRASERO_BURN_LOG ("Retrieving format capacity");

	priv = BRASERO_MEDIUM_PRIVATE (self);
	result = brasero_mmc2_read_format_capacities (handle,
						      &hdr,
						      &size,
						      code);
	if (result != BRASERO_SCSI_OK) {
		g_free (hdr);

		BRASERO_BURN_LOG ("READ FORMAT CAPACITIES failed");
		return BRASERO_BURN_ERR;
	}

	/* see if the media is already formatted */
	current = hdr->max_caps;
	if (!(current->type & BRASERO_SCSI_DESC_FORMATTED)) {
		BRASERO_BURN_LOG ("Unformatted media");
		priv->info |= BRASERO_MEDIUM_UNFORMATTED;
	}

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

				/* that can happen */
				if (!priv->block_size)
					priv->block_size = 2048;

				break;
			}
		}
		else if (desc->format_type == BRASERO_SCSI_MAX_PACKET_SIZE_FORMAT) {
			priv->block_num = BRASERO_GET_32 (desc->blocks_num);
			break;
		}
	}

	BRASERO_BURN_LOG ("Format capacity %lli %lli",
			  priv->block_num,
			  priv->block_size);

	g_free (hdr);
	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_medium_get_capacity_by_type (BraseroMedium *self,
				     BraseroDeviceHandle *handle,
				     BraseroScsiErrCode *code)
{
	BraseroMediumPrivate *priv;

	priv = BRASERO_MEDIUM_PRIVATE (self);

	/* For DVDs that's always that block size */
	priv->block_size = 2048;

	if (!(priv->info & BRASERO_MEDIUM_REWRITABLE))
		return BRASERO_BURN_OK;

	if (priv->info & BRASERO_MEDIUM_CD)
		brasero_medium_get_capacity_CD_RW (self, handle, code);
	else
		brasero_medium_get_capacity_DVD_RW (self, handle, code);

	return BRASERO_BURN_OK;
}

/**
 * Functions to retrieve the speed
 */

static BraseroBurnResult
brasero_medium_get_speed_mmc3 (BraseroMedium *self,
			       BraseroDeviceHandle *handle,
			       BraseroScsiErrCode *code)
{
	int size;
	int num_desc, i;
	gint max_rd, max_wrt;
	BraseroScsiResult result;
	BraseroMediumPrivate *priv;
	BraseroScsiWrtSpdDesc *desc;
	BraseroScsiGetPerfData *wrt_perf = NULL;

	BRASERO_BURN_LOG ("Retrieving speed (Get Performance)");

	/* NOTE: this only work if there is RT streaming feature with
	 * wspd bit set to 1. At least an MMC3 drive. */
	priv = BRASERO_MEDIUM_PRIVATE (self);
	result = brasero_mmc3_get_performance_wrt_spd_desc (handle,
							    &wrt_perf,
							    &size,
							    code);

	if (result != BRASERO_SCSI_OK) {
		g_free (wrt_perf);

		BRASERO_BURN_LOG ("GET PERFORMANCE failed");
		return BRASERO_BURN_ERR;
	}

	num_desc = (size - sizeof (BraseroScsiGetPerfHdr)) /
		    sizeof (BraseroScsiWrtSpdDesc);

	if (num_desc <=  0)
		goto end; 

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

end:

	g_free (wrt_perf);

	/* strangely there are so drives (I know one case) which support this
	 * function but don't report any speed. So if our top speed is 0 then
	 * use the other way to get the speed. It was a Teac */
	if (!priv->max_wrt)
		return BRASERO_BURN_ERR;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_medium_get_page_2A_write_speed_desc (BraseroMedium *self,
					     BraseroDeviceHandle *handle,
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

	BRASERO_BURN_LOG ("Retrieving speed (2A speeds)");

	priv = BRASERO_MEDIUM_PRIVATE (self);
	result = brasero_spc1_mode_sense_get_page (handle,
						   BRASERO_SPC_PAGE_STATUS,
						   &data,
						   &size,
						   code);
	if (result != BRASERO_SCSI_OK) {
		g_free (data);

		BRASERO_BURN_LOG ("MODE SENSE failed");
		return BRASERO_BURN_ERR;
	}

	page_2A = (BraseroScsiStatusPage *) &data->page;

	/* FIXME: the following is not necessarily true */
	if (size < sizeof (BraseroScsiStatusPage)) {
		g_free (data);

		BRASERO_BURN_LOG ("wrong size in page");
		return BRASERO_BURN_ERR;
	}

	desc_num = BRASERO_GET_16 (page_2A->wr_speed_desc_num);
	max_num = size -
		  sizeof (BraseroScsiStatusPage) -
		  sizeof (BraseroScsiModeHdr);
	max_num /= sizeof (BraseroScsiWrtSpdDesc);

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
				      BraseroDeviceHandle *handle,
				      BraseroScsiErrCode *code)
{
	BraseroScsiStatusPage *page_2A = NULL;
	BraseroScsiModeData *data = NULL;
	BraseroMediumPrivate *priv;
	BraseroScsiResult result;
	int size = 0;

	BRASERO_BURN_LOG ("Retrieving speed (2A max)");

	priv = BRASERO_MEDIUM_PRIVATE (self);

	result = brasero_spc1_mode_sense_get_page (handle,
						   BRASERO_SPC_PAGE_STATUS,
						   &data,
						   &size,
						   code);
	if (result != BRASERO_SCSI_OK) {
		g_free (data);

		BRASERO_BURN_LOG ("MODE SENSE failed");
		return BRASERO_BURN_ERR;
	}

	page_2A = (BraseroScsiStatusPage *) &data->page;

	if (size < 0x14) {
		g_free (data);

		BRASERO_BURN_LOG ("wrong page size");
		return BRASERO_BURN_ERR;
	}

	priv->max_rd = BRASERO_GET_16 (page_2A->rd_max_speed);
	priv->max_wrt = BRASERO_GET_16 (page_2A->wr_max_speed);

	g_free (data);
	return BRASERO_BURN_OK;
}
 
/**
 * Functions to get information about disc contents
 */

static BraseroBurnResult
brasero_medium_track_volume_size (BraseroMedium *self,
				  BraseroMediumTrack *track,
				  BraseroDeviceHandle *handle)
{
	BraseroMediumPrivate *priv;
	BraseroBurnResult res;
	GError *error = NULL;
	BraseroVolSrc *vol;
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
	vol = brasero_volume_source_open_device_handle (handle, NULL);
	res = brasero_volume_get_size (vol,
				       track->start,
				       &nb_blocks,
				       NULL);
	brasero_volume_source_close (vol);

	if (!res) {
		BRASERO_BURN_LOG ("Failed to retrieve the volume size: %s",
				  error && error->message ? 
				  error->message:"unknown error");

		if (error)
			g_error_free (error);
		return BRASERO_BURN_ERR;
	}

	track->blocks_num = nb_blocks;
	return BRASERO_BURN_OK;
}

static gboolean
brasero_medium_track_written_SAO (BraseroDeviceHandle *handle,
				  int track_num,
				  int track_start)
{
	unsigned char buffer [2048];
	BraseroScsiResult result;

	BRASERO_BURN_LOG ("Checking for TDBs in track pregap.");

	/* The two following sectors are readable */
	result = brasero_mmc1_read_block (handle,
					  TRUE,
					  BRASERO_SCSI_BLOCK_TYPE_ANY,
					  BRASERO_SCSI_BLOCK_HEADER_NONE,
					  BRASERO_SCSI_BLOCK_NO_SUBCHANNEL,
					  track_start - 1,
					  1,
					  buffer,
					  sizeof (buffer),
					  NULL);

	if (result == BRASERO_SCSI_OK) {
		int i;

		if (buffer [0] != 'T' || buffer [1] != 'D' || buffer [2] != 'I') {
			BRASERO_BURN_LOG ("Track was probably recorded in SAO mode - no TDB.");
			return TRUE;
		}

		/* Find the TDU (16 bytes) for the track (there can be for other tracks).
		 * i must be < 128 = ((2048 - 8 (size TDB)) / 16 (size TDU). */
		for (i = 0; i < 128; i ++) {
			if (BRASERO_GET_BCD (buffer [8 + i * 16]) != track_num)
				break;
		}

		if (i >= 128) {
			BRASERO_BURN_LOG ("No appropriate TDU for track");
			return TRUE;
		}

		if (buffer [8 + i * 16] == 0x80 || buffer [8 + i * 16] == 0x00) {
			BRASERO_BURN_LOG ("Track was recorded in TAO mode.");
			return FALSE;
		}

		BRASERO_BURN_LOG ("Track was recorded in Packet mode.");
		return FALSE;
	}

	BRASERO_BURN_LOG ("No pregap. That track must have been recorded in SAO mode.");
	return TRUE;
}

static BraseroBurnResult
brasero_medium_track_get_info (BraseroMedium *self,
			       gboolean multisession,
			       BraseroMediumTrack *track,
			       int track_num,
			       BraseroDeviceHandle *handle,
			       BraseroScsiErrCode *code)
{
	BraseroScsiTrackInfo track_info;
	BraseroMediumPrivate *priv;
	BraseroScsiResult result;
	int size;

	BRASERO_BURN_LOG ("Retrieving track information for %i", track_num);

	priv = BRASERO_MEDIUM_PRIVATE (self);

	/* at this point we know the type of the disc that's why we set the 
	 * size according to this type. That may help to avoid outrange address
	 * errors. */
	if (BRASERO_MEDIUM_IS (priv->info, BRASERO_MEDIUM_DVD_DL|BRASERO_MEDIUM_WRITABLE))
		size = 48;
	else if (BRASERO_MEDIUM_IS (priv->info, BRASERO_MEDIUM_PLUS|BRASERO_MEDIUM_WRITABLE))
		size = 40;
	else
		size = 36;

	result = brasero_mmc1_read_track_info (handle,
					       track_num,
					       &track_info,
					       &size,
					       code);

	if (result != BRASERO_SCSI_OK) {
		BRASERO_BURN_LOG ("READ TRACK INFO failed");
		return BRASERO_BURN_ERR;
	}

	track->blocks_num = BRASERO_GET_32 (track_info.track_size);
	track->session = BRASERO_SCSI_SESSION_NUM (track_info);

	if (track->blocks_num <= 300) {
		/* Now here is a potential bug: we can write tracks (data or
		 * not) shorter than 300 Kio /2 sec but they will be padded to
		 * reach this floor value. It means that blocks_num is always
		 * 300 blocks even if the data length on the track is actually
		 * shorter.
		 * So we read the volume descriptor. If it works, good otherwise
		 * use the old value.
		 * That's important for checksuming to have a perfect account of
		 * the data size. */
		BRASERO_BURN_LOG ("300 sectors size. Checking for real size");
		brasero_medium_track_volume_size (self, track, handle);
	}
	/* NOTE: for multisession CDs only
	 * if the session was incremental (TAO/packet/...) by opposition to DAO
	 * and SAO, then 2 blocks (run-out) have been added at the end of user
	 * track for linking. That's why we have 2 additional sectors when the
	 * track has been recorded in TAO mode
	 * See MMC5
	 * 6.44.3.2 CD-R Fixed Packet, Variable Packet, Track-At-Once
	 * Now, strangely track_get_info always removes two blocks, whereas read
	 * raw toc adds them (always) and this, whatever the mode, the position.
	 * It means that when we detect a SAO session we have to add 2 blocks to
	 * all tracks in it. 
	 * See # for any information:
	 * if first track is recorded in SAO/DAO then the length will be two sec
	 * shorter. If not, if it was recorded in TAO, that's fine.
	 * The other way would be to use read raw toc but then that's the
	 * opposite that happens and that latter will return two more bytes for
	 * TAO recorded session.
	 * So there are 2 workarounds:
	 * - read the volume size (can be unreliable)
	 * - read the 2 last blocks and see if they are run-outs
	 * here we do solution 2 but only for CDRW, not blank, and for first
	 * session only since that's the only one that can be recorded in DAO. */
	else if (track->session == 1
	     && (track->type & BRASERO_MEDIUM_TRACK_DATA)
	     &&  multisession
	     &&  (priv->info & BRASERO_MEDIUM_CD)
	     && !(priv->info & BRASERO_MEDIUM_ROM)) {
		BRASERO_BURN_LOG ("Data track belongs to first session of multisession CD. "
				  "Checking for real size (%i sectors currently).",
				  track->blocks_num);

		/* we test the pregaps blocks for TDB: these are special blocks
		 * filling the pregap of a track when it was recorded as TAO or
		 * as Packet.
		 * NOTE: in this case we need to skip 7 sectors before since if
		 * it was recorded incrementally then there is also 4 runins,
		 * 1 link sector and 2 runouts (at end of pregap). 
		 * we also make sure that the two blocks we're adding are
		 * actually readable. */
		/* Test the last block, the before last and the one before before last */
		result = brasero_mmc1_read_block (handle,
						  FALSE,
						  BRASERO_SCSI_BLOCK_TYPE_ANY,
						  BRASERO_SCSI_BLOCK_HEADER_NONE,
						  BRASERO_SCSI_BLOCK_NO_SUBCHANNEL,
						  track->blocks_num + track->start,
						  2,
						  NULL,
						  0,
						  NULL);

		if (result == BRASERO_SCSI_OK) {
			BRASERO_BURN_LOG ("Following two sectors are readable.");

			if (brasero_medium_track_written_SAO (handle, track_num, track->start)) {
				track->blocks_num += 2;
				BRASERO_BURN_LOG ("Correcting track size (now %i)", track->blocks_num);
			}
		}
		else
			BRASERO_BURN_LOG ("Detected runouts");
	}
	else if (BRASERO_MEDIUM_IS (priv->info, BRASERO_MEDIUM_DVDRW_PLUS)
	     ||  BRASERO_MEDIUM_IS (priv->info, BRASERO_MEDIUM_DVDRW_PLUS_DL)
	     ||  BRASERO_MEDIUM_IS (priv->info, BRASERO_MEDIUM_DVDRW_RESTRICTED)) {
		BRASERO_BURN_LOG ("DVD+RW (DL) or DVD-RW (restricted overwrite) checking volume size");
		brasero_medium_track_volume_size (self, track, handle);
	}


	if (track_info.next_wrt_address_valid)
		priv->next_wr_add = BRASERO_GET_32 (track_info.next_wrt_address);

	BRASERO_BURN_LOG ("Track %i (session %i): type = %i start = %llu size = %llu",
			  track_num,
			  track->session,
			  track->type,
			  track->start,
			  track->blocks_num);

	return BRASERO_BURN_OK;
}

/**
 * NOTE: for DVD-R multisession we lose 28688 blocks for each session
 * so the capacity is the addition of all session sizes + 28688 for each
 * For all multisession DVD-/+R and CDR-RW the remaining size is given 
 * in the leadout. One exception though with DVD+/-RW.
 */

static void
brasero_medium_add_DVD_plus_RW_leadout (BraseroMedium *self,
					gint32 start)
{
	BraseroMediumTrack *leadout;
	BraseroMediumPrivate *priv;

	priv = BRASERO_MEDIUM_PRIVATE (self);

	leadout = g_new0 (BraseroMediumTrack, 1);
	priv->tracks = g_slist_append (priv->tracks, leadout);

	leadout->start = start;
	leadout->type = BRASERO_MEDIUM_TRACK_LEADOUT;

	/* we fabricate the leadout here. We don't really need one in 
	 * fact since it is always at the last sector whatever the
	 * amount of data written. So we need in fact to read the file
	 * system and get the last sector from it. Hopefully it won't be
	 * buggy */
	priv->next_wr_add = 0;

	leadout->blocks_num = priv->block_num;
	if (g_slist_length (priv->tracks) > 1) {
		BraseroMediumTrack *track;

		track = priv->tracks->data;
		leadout->blocks_num -= ((track->blocks_num > 300) ? track->blocks_num : 300);
	}
	BRASERO_BURN_LOG ("Adding fabricated leadout start = %llu length = %llu",
			  leadout->start,
			  leadout->blocks_num);
}

static BraseroBurnResult
brasero_medium_get_sessions_info (BraseroMedium *self,
				  BraseroDeviceHandle *handle,
				  BraseroScsiErrCode *code)
{
	int num, i, size;
	gboolean multisession;
	BraseroScsiResult result;
	BraseroScsiTocDesc *desc;
	BraseroMediumPrivate *priv;
	BraseroScsiFormattedTocData *toc = NULL;

	BRASERO_BURN_LOG ("Reading Toc");

	priv = BRASERO_MEDIUM_PRIVATE (self);
	result = brasero_mmc1_read_toc_formatted (handle,
						  0,
						  &toc,
						  &size,
						  code);
	if (result != BRASERO_SCSI_OK) {
		g_free (toc);

		BRASERO_BURN_LOG ("READ TOC failed");
		return BRASERO_BURN_ERR;
	}

	num = (size - sizeof (BraseroScsiFormattedTocData)) /
	       sizeof (BraseroScsiTocDesc);

	/* remove 1 for leadout */
	multisession = (priv->info & BRASERO_MEDIUM_APPENDABLE) || (num -1) != 1;

	BRASERO_BURN_LOG ("%i track(s) found", num);

	desc = toc->desc;
	for (i = 0; i < num; i ++, desc ++) {
		BraseroMediumTrack *track;

		if (desc->track_num == BRASERO_SCSI_TRACK_LEADOUT_START)
			break;

		track = g_new0 (BraseroMediumTrack, 1);
		priv->tracks = g_slist_prepend (priv->tracks, track);
		track->start = BRASERO_GET_32 (desc->track_start);

		/* we shouldn't request info on a track if the disc is closed */
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
		else {
			track->type |= BRASERO_MEDIUM_TRACK_DATA;
			priv->info |= BRASERO_MEDIUM_HAS_DATA;

			if (desc->control & BRASERO_SCSI_TRACK_DATA_INCREMENTAL)
				track->type |= BRASERO_MEDIUM_TRACK_INCREMENTAL;
		}

		brasero_medium_track_get_info (self,
					       multisession,
					       track,
					       g_slist_length (priv->tracks),
					       handle,
					       code);

		if (desc->control & BRASERO_SCSI_TRACK_COPY)
			track->type |= BRASERO_MEDIUM_TRACK_COPY;

		if (BRASERO_MEDIUM_IS (priv->info, BRASERO_MEDIUM_DVDRW_PLUS)
		||  BRASERO_MEDIUM_IS (priv->info, BRASERO_MEDIUM_DVDRW_RESTRICTED)) {
			BraseroBurnResult result;

			/* a special case for these two kinds of media (DVD+RW)
			 * which have only one track: the first. */
			result = brasero_medium_track_volume_size (self, 
								   track,
								   handle);
			if (result != BRASERO_BURN_OK) {
				priv->tracks = g_slist_remove (priv->tracks, track);
				g_free (track);

				priv->info |= BRASERO_MEDIUM_BLANK;
				priv->info &= ~(BRASERO_MEDIUM_CLOSED|
					        BRASERO_MEDIUM_HAS_DATA);

				BRASERO_BURN_LOG ("Empty first session.");
			}
			else
				priv->next_wr_add = 0;
		}
	}

	/* put the tracks in the right order */
	priv->tracks = g_slist_reverse (priv->tracks);

	if (BRASERO_MEDIUM_IS (priv->info, BRASERO_MEDIUM_DVDRW_PLUS)
	||  BRASERO_MEDIUM_IS (priv->info, BRASERO_MEDIUM_DVDRW_RESTRICTED)) {
		gint32 start;

		/* It starts where the other one finishes */
		if (priv->tracks)
			start = BRASERO_GET_32 (desc->track_start);
		else
			start = 0;

		brasero_medium_add_DVD_plus_RW_leadout (self, start);
	}
	else if (!(priv->info & BRASERO_MEDIUM_CLOSED)) {
		BraseroMediumTrack *track;

		/* we shouldn't request info on leadout if the disc is closed
		 * (except for DVD+/- (restricted) RW (see above) */
		track = g_new0 (BraseroMediumTrack, 1);
		priv->tracks = g_slist_append (priv->tracks, track);
		track->start = BRASERO_GET_32 (desc->track_start);
		track->type = BRASERO_MEDIUM_TRACK_LEADOUT;

		brasero_medium_track_get_info (self,
					       FALSE,
					       track,
					       g_slist_length (priv->tracks),
					       handle,
					       code);
	}

	g_free (toc);

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_medium_get_contents (BraseroMedium *self,
			     BraseroDeviceHandle *handle,
			     BraseroScsiErrCode *code)
{
	int size;
	BraseroScsiResult result;
	BraseroMediumPrivate *priv;
	BraseroScsiDiscInfoStd *info = NULL;

	BRASERO_BURN_LOG ("Retrieving media status");

	priv = BRASERO_MEDIUM_PRIVATE (self);

	result = brasero_mmc1_read_disc_information_std (handle,
							 &info,
							 &size,
							 code);
	if (result != BRASERO_SCSI_OK) {
		g_free (info);
	
		BRASERO_BURN_LOG ("READ DISC INFORMATION failed");
		return BRASERO_BURN_ERR;
	}

	if (info->erasable)
		priv->info |= BRASERO_MEDIUM_REWRITABLE;

	if (info->status == BRASERO_SCSI_DISC_EMPTY) {
		BraseroMediumTrack *track;

		BRASERO_BURN_LOG ("Empty media");

		priv->info |= BRASERO_MEDIUM_BLANK;
		priv->block_size = 2048;

		if (BRASERO_MEDIUM_IS (priv->info, BRASERO_MEDIUM_DVDRW_PLUS)
		||  BRASERO_MEDIUM_IS (priv->info, BRASERO_MEDIUM_DVDRW_RESTRICTED))
			brasero_medium_add_DVD_plus_RW_leadout (self, 0);
		else {
			track = g_new0 (BraseroMediumTrack, 1);
			track->start = 0;
			track->type = BRASERO_MEDIUM_TRACK_LEADOUT;
			priv->tracks = g_slist_prepend (priv->tracks, track);
			
			brasero_medium_track_get_info (self,
						       FALSE,
						       track,
						       1,
						       handle,
						       code);
		}
		goto end;
	}

	if (info->status == BRASERO_SCSI_DISC_INCOMPLETE) {
		priv->info |= BRASERO_MEDIUM_APPENDABLE;
		BRASERO_BURN_LOG ("Appendable media");
	}
	else if (info->status == BRASERO_SCSI_DISC_FINALIZED) {
		priv->info |= BRASERO_MEDIUM_CLOSED;
		BRASERO_BURN_LOG ("Closed media");
	}

	result = brasero_medium_get_sessions_info (self, handle, code);

end:

	g_free (info);
	return result;
}

#if 0

/**
 * These are special routines for old CD-R(W) drives that don't conform to MMC
 */

static void
brasero_medium_set_track_type (BraseroMedium *self,
			       BraseroMediumTrack *track,
			       guchar control)
{
	BraseroMediumPrivate *priv;

	priv = BRASERO_MEDIUM_PRIVATE (self);

	if (control & BRASERO_SCSI_TRACK_COPY)
		track->type |= BRASERO_MEDIUM_TRACK_COPY;

	if (!(control & BRASERO_SCSI_TRACK_DATA)) {
		track->type |= BRASERO_MEDIUM_TRACK_AUDIO;
		priv->info |= BRASERO_MEDIUM_HAS_AUDIO;

		if (control & BRASERO_SCSI_TRACK_PREEMP)
			track->type |= BRASERO_MEDIUM_TRACK_PREEMP;

		if (control & BRASERO_SCSI_TRACK_4_CHANNELS)
			track->type |= BRASERO_MEDIUM_TRACK_4_CHANNELS;
	}
	else {
		track->type |= BRASERO_MEDIUM_TRACK_DATA;
		priv->info |= BRASERO_MEDIUM_HAS_DATA;

		if (control & BRASERO_SCSI_TRACK_DATA_INCREMENTAL)
			track->type |= BRASERO_MEDIUM_TRACK_INCREMENTAL;
	}
}

/**
 * return :
 *  0 when it's not possible to determine (fallback to formatted toc)
 * -1 for BCD
 *  1 for HEX */
static guint
brasero_medium_check_BCD_use (BraseroMedium *self,
			      BraseroDeviceHandle *handle,
			      BraseroScsiRawTocDesc *desc,
			      guint num,
			      BraseroScsiErrCode *code)
{
	guint i;
	int size;
	guint leadout = 0;
	guint track_num = 0;
	gboolean use_BCD = TRUE;
	gboolean use_HEX = TRUE;
	BraseroScsiResult result;
	BraseroScsiTrackInfo track_info;
	guint start_BCD, start_LBA, track_start;

	/* first check if all values are valid BCD numbers in the descriptors */
	for (i = 0; i < num; i++) {
		if (desc [i].adr == 1 && desc [i].point <= BRASERO_SCSI_Q_SUB_CHANNEL_TRACK_START) {
			if (!BRASERO_IS_BCD_VALID (desc [i].p_min)
			||  !BRASERO_IS_BCD_VALID (desc [i].p_sec)
			||  !BRASERO_IS_BCD_VALID (desc [i].p_frame)) {
				use_BCD = FALSE;
				break;
			}
		}
		else if (desc [i].point == BRASERO_SCSI_Q_SUB_CHANNEL_LEADOUT_START) {
			if (!BRASERO_IS_BCD_VALID (desc [i].p_min)
			||  !BRASERO_IS_BCD_VALID (desc [i].p_sec)
			||  !BRASERO_IS_BCD_VALID (desc [i].p_frame)) {
				use_BCD = FALSE;
				break;
			}
		}
	}

	/* then check if there are valid Hex values */
	for (i = 0; i < num; i++) {
		if (desc [i].adr != 1 || desc [i].point > BRASERO_SCSI_Q_SUB_CHANNEL_TRACK_START)
			continue;

		if (desc [i].p_min > 99
		||  desc [i].p_sec > 59
		||  desc [i].p_frame > 74) {
			use_HEX = FALSE;
			break;
		}
	}

	if (use_BCD != use_HEX) {
		if (use_BCD)
			return -1;

		return 1;
	}

	/* To check if the drive uses BCD values or HEX values we ask for the 
	 * track information that contains also the start for the track but in
	 * HEX values. If values are the same then it works. */

	/* NOTE: there could be another way to do it: get first track, in LBA
	 * and BCD it must be 150. */

	/* First find the first track and get track start address in BCD */
	BRASERO_BURN_LOG ("Retrieving track information to determine number format");

	for (i = 0; i < num; i++) {
		if (desc [i].adr == BRASERO_SCSI_Q_SUB_CHANNEL_LEADIN_MODE5
		&&  desc [i].point == BRASERO_SCSI_Q_SUB_CHANNEL_MULTI_NEXT_SESSION) {
			/* store the leadout number just in case */
			leadout = i;
			continue;
		}

		if (desc [i].adr != 1 || desc [i].point > BRASERO_SCSI_Q_SUB_CHANNEL_TRACK_START)
			continue;

		track_num ++;

		start_BCD = BRASERO_MSF_TO_LBA (BRASERO_GET_BCD (desc [i].p_min),
						BRASERO_GET_BCD (desc [i].p_sec),
						BRASERO_GET_BCD (desc [i].p_frame));

		start_LBA = BRASERO_MSF_TO_LBA (desc [i].p_min,
						desc [i].p_sec,
						desc [i].p_frame);

		BRASERO_BURN_LOG ("Comparing to track information from READ TRACK INFO for track %i", track_num);

		size = 36;
		start_LBA -= 150;
		start_BCD -= 150;

/*
		result = brasero_mmc1_read_track_info (handle,
						       track_num,
						       &track_info,
						       &size,
						       code);

		if (result != BRASERO_SCSI_OK) {
			BRASERO_BURN_LOG ("READ TRACK INFO failed");
*/			/* Fallback to formatted toc */
/*			return 0;
		}
*/
		track_start = BRASERO_GET_32 (track_info.start_lba);
		BRASERO_BURN_LOG ("comparing DCB %i and LBA %i to real start address %i",
				  start_BCD, start_LBA, track_start);

		/* try to find a conclusive match */
		if (track_start == start_BCD && track_start != start_LBA)
			return -1;

		if (track_start == start_LBA && track_start != start_BCD)
			return 1;
	}

	/* Our last chance, the leadout.
	 * NOTE: no need to remove 150 sectors here. */
	start_BCD = BRASERO_MSF_TO_LBA (BRASERO_GET_BCD (desc [leadout].min),
					BRASERO_GET_BCD (desc [leadout].sec),
					BRASERO_GET_BCD (desc [leadout].frame));

	start_LBA = BRASERO_MSF_TO_LBA (desc [leadout].min,
					desc [leadout].sec,
					desc [leadout].frame);

	BRASERO_BURN_LOG ("Comparing to track information from READ TRACK INFO for leadout");

	size = 36;

	/* leadout number is number of tracks + 1 */
	result = brasero_mmc1_read_track_info (handle,
					       track_num + 1,
					       &track_info,
					       &size,
					       code);

	if (result != BRASERO_SCSI_OK) {
		BRASERO_BURN_LOG ("READ TRACK INFO failed for leadout");
		/* Fallback to formatted toc */
		return 0;
	}

	track_start = BRASERO_GET_32 (track_info.start_lba);
	BRASERO_BURN_LOG ("comparing DCB %i and LBA %i to real start address %i",
			  start_BCD, start_LBA, track_start);

	/* try to find a conclusive match */
	if (track_start == start_BCD && track_start != start_LBA)
		return -1;

	if (track_start == start_LBA && track_start != start_BCD)
		return 1;

	/* fallback to formatted toc */
	return 0;
}

/**
 * The reason why we use this perhaps more lengthy method is that with
 * multisession discs, the first track is reported to be two sectors shorter
 * than it should. As I don't know why and since the following works we use
 * this one. */
static BraseroBurnResult
brasero_medium_get_CD_sessions_info (BraseroMedium *self,
				     BraseroDeviceHandle *handle,
				     BraseroScsiErrCode *code)
{
	gint use_bcd;
	GSList *iter;
	int num, i, size;
	gint leadout_start = 0;
	BraseroScsiResult result;
	BraseroMediumPrivate *priv;
	BraseroScsiRawTocDesc *desc;
	BraseroScsiRawTocData *toc = NULL;

	BRASERO_BURN_LOG ("Reading Raw Toc");

	priv = BRASERO_MEDIUM_PRIVATE (self);

	size = 0;
	result = brasero_mmc1_read_toc_raw (handle,
					    0,
					    &toc,
					    &size,
					    code);
	if (result != BRASERO_SCSI_OK) {
		BRASERO_BURN_LOG ("READ TOC failed");
		return BRASERO_BURN_ERR;
	}

	num = (size - sizeof (BraseroScsiRawTocData)) /
	       sizeof (BraseroScsiRawTocDesc);

	BRASERO_BURN_LOG ("%i track(s) found", num);

	desc = toc->desc;
	use_bcd = brasero_medium_check_BCD_use (self, handle, desc, num, code);
	if (!use_bcd) {
		g_free (toc);

		BRASERO_BURN_LOG ("Fallback to formatted toc");
		return BRASERO_BURN_ERR;
	}

	if (use_bcd > 0)
		use_bcd = 0;

	if (use_bcd) {
		BRASERO_BURN_LOG ("Using BCD format");
	}
	else {
		BRASERO_BURN_LOG ("Using HEX format");
	}

	for (i = 0; i < num; i++, desc ++) {
		BraseroMediumTrack *track;

		track = NULL;
		if (desc->adr == 1 && desc->point <= BRASERO_SCSI_Q_SUB_CHANNEL_TRACK_START) {
			track = g_new0 (BraseroMediumTrack, 1);
			track->session = desc->session_num;

			brasero_medium_set_track_type (self, track, desc->control);
			if (use_bcd)
				track->start = BRASERO_MSF_TO_LBA (BRASERO_GET_BCD (desc->p_min),
								   BRASERO_GET_BCD (desc->p_sec),
								   BRASERO_GET_BCD (desc->p_frame));
			else
				track->start = BRASERO_MSF_TO_LBA (desc->p_min,
								   desc->p_sec,
								   desc->p_frame);

			track->start -= 150;

			/* if there are tracks and the last previously added track is in
			 * the same session then set the size */
			if (priv->tracks) {
				BraseroMediumTrack *last_track;

				last_track = priv->tracks->data;
				if (last_track->session == track->session)
					last_track->blocks_num = track->start - last_track->start;
			}

			priv->tracks = g_slist_prepend (priv->tracks, track);
		}
		else if (desc->point == BRASERO_SCSI_Q_SUB_CHANNEL_LEADOUT_START) {
			/* NOTE: the leadout session is first in the list. So if
			 * we have tracks in the list set the last session track
			 * size when we reach a new leadout (and therefore a new
			 * session). */

			if (priv->tracks) {
				BraseroMediumTrack *last_track;

				last_track = priv->tracks->data;
				last_track->blocks_num = leadout_start - last_track->start;
			}

			if (use_bcd)
				leadout_start = BRASERO_MSF_TO_LBA (BRASERO_GET_BCD (desc->p_min),
								    BRASERO_GET_BCD (desc->p_sec),
								    BRASERO_GET_BCD (desc->p_frame));
			else
				leadout_start = BRASERO_MSF_TO_LBA (desc->p_min,
								    desc->p_sec,
								    desc->p_frame);
			leadout_start -= 150;
		}
	}

	if (priv->tracks) {
		BraseroMediumTrack *last_track;

		/* set the last found track size */
		last_track = priv->tracks->data;
		last_track->blocks_num = leadout_start - last_track->start;
	}

	/* Add a leadout */
	if (!(priv->info & BRASERO_MEDIUM_CLOSED)) {
		BraseroMediumTrack *track;

		/* we shouldn't request info on leadout if the disc is closed */
		track = g_new0 (BraseroMediumTrack, 1);
		priv->tracks = g_slist_prepend (priv->tracks, track);
		track->start = leadout_start;
		track->type = BRASERO_MEDIUM_TRACK_LEADOUT;

		brasero_medium_track_get_info (self,
					       FALSE,
					       track,
					       g_slist_length (priv->tracks),
					       handle,
					       code); 
	}

	priv->tracks = g_slist_reverse (priv->tracks);

	for (iter = priv->tracks; iter; iter = iter->next) {
		BraseroMediumTrack *track;

		track = iter->data;

		/* check for tracks less that 300 sectors */
		if (track->blocks_num <= 300 && track->type != BRASERO_MEDIUM_TRACK_LEADOUT) {
			BRASERO_BURN_LOG ("300 sectors size. Checking for real size");
			brasero_medium_track_volume_size (self, track, handle);
		}

		BRASERO_BURN_LOG ("Track %i: type = %i start = %llu size = %llu",
				  g_slist_index (priv->tracks, track),
				  track->type,
				  track->start,
				  track->blocks_num);
	}

	g_free (toc);
	return BRASERO_BURN_OK;
}

#endif

static BraseroBurnResult
brasero_medium_old_drive_get_disc_info (BraseroMedium *self,
					BraseroDeviceHandle *handle,
					BraseroScsiErrCode *code)
{
	int size;
	BraseroScsiResult result;
	BraseroMediumPrivate *priv;
	BraseroScsiDiscInfoStd *info = NULL;

	BRASERO_BURN_LOG ("Retrieving media status for old drive");

	priv = BRASERO_MEDIUM_PRIVATE (self);

	result = brasero_mmc1_read_disc_information_std (handle,
							 &info,
							 &size,
							 code);
	if (result != BRASERO_SCSI_OK) {
		g_free (info);
	
		BRASERO_BURN_LOG ("READ DISC INFORMATION failed for old drive");
		return BRASERO_BURN_ERR;
	}

	/* Try to identify the type: can only be CDROM CDR CDRW.
	 * NOTE: since there is no way to distinguish a CDROM and a closed CDR 
	 * if the disc is closed we set it as CDROM (except if it's RW). */
	if (info->erasable)
		priv->info = BRASERO_MEDIUM_CDRW;
	else if (info->status == BRASERO_SCSI_DISC_FINALIZED)
		priv->info = BRASERO_MEDIUM_CDROM;
	else
		priv->info = BRASERO_MEDIUM_CDR;

	if (info->status == BRASERO_SCSI_DISC_EMPTY) {
		priv->info |= BRASERO_MEDIUM_BLANK;
		priv->block_size = 2048;
		priv->next_wr_add = 0;
		BRASERO_BURN_LOG ("Empty media (old drive)");
	}
	else if (info->status == BRASERO_SCSI_DISC_INCOMPLETE) {
		priv->info |= BRASERO_MEDIUM_APPENDABLE;
		priv->block_size = 2048;
		priv->next_wr_add = 0;
		BRASERO_BURN_LOG ("Appendable media (old drive)");
	}
	else if (info->status == BRASERO_SCSI_DISC_FINALIZED) {
		priv->info |= BRASERO_MEDIUM_CLOSED;
		BRASERO_BURN_LOG ("Closed media (old drive)");
	}

	/* get the contents */
	result = brasero_medium_get_sessions_info (self, handle, code);
	return result;
}

static BraseroBurnResult
brasero_medium_check_old_drive (BraseroMedium *self,
				BraseroDeviceHandle *handle,
				BraseroScsiErrCode *code)
{
	gchar *model;
	BraseroMediumPrivate *priv;

	priv = BRASERO_MEDIUM_PRIVATE (self);

	model = brasero_drive_get_display_name (priv->drive);
	if (!model)
		return BRASERO_BURN_ERR;

	if (!strcmp (model, "CD-R55S")) {
		g_free (model);
		priv->max_rd = BRASERO_SPEED_TO_RATE_CD (12);
		priv->max_wrt = BRASERO_SPEED_TO_RATE_CD (4);
		return brasero_medium_old_drive_get_disc_info (self,
							       handle,
							       code);
	}

	BRASERO_BURN_LOG ("Not an old drive model");

	return BRASERO_BURN_ERR;
}

/**
 * Some identification functions
 */

static BraseroBurnResult
brasero_medium_get_medium_type (BraseroMedium *self,
				BraseroDeviceHandle *handle,
				BraseroScsiErrCode *code)
{
	BraseroScsiGetConfigHdr *hdr = NULL;
	BraseroMediumPrivate *priv;
	BraseroScsiResult result;
	int size;

	BRASERO_BURN_LOG ("Retrieving media profile");

	priv = BRASERO_MEDIUM_PRIVATE (self);
	result = brasero_mmc2_get_configuration_feature (handle,
							 BRASERO_SCSI_FEAT_REAL_TIME_STREAM,
							 &hdr,
							 &size,
							 code);
	if (result != BRASERO_SCSI_OK) {
		BraseroScsiAtipData *data = NULL;
		int size = 0;

		BRASERO_BURN_LOG ("GET CONFIGURATION failed");

		/* This could be a MMC1 drive since this command was
		 * introduced in MMC2 and is supported onward. So it
		 * has to be a CD (R/RW). The rest of the information
		 * will be provided by read_disc_information. */

		/* retrieve the speed */
		result = brasero_medium_get_page_2A_max_speed (self,
							       handle,
							       code);

		/* If this fails it means that this drive is probably older than
		 * MMC1 spec or does not conform to it. Try our last chance. */
		if (result != BRASERO_BURN_OK)
			return brasero_medium_check_old_drive (self,
							       handle,
							       code);

		/* The only thing here left to determine is if that's a WRITABLE
		 * or a REWRITABLE. To determine that information, we need to
		 * read TocPmaAtip. It if fails that's a ROM, if it succeeds.
		 * No need to set error code since we consider that it's a ROM
		 * if a failure happens. */
		result = brasero_mmc1_read_atip (handle,
						 &data,
						 &size,
						 NULL);
		if (result != BRASERO_SCSI_OK) {
			/* CDROM */
			priv->info = BRASERO_MEDIUM_CDROM;
			priv->type = types [1];
			priv->icon = icons [1];
		}
		else {
			/* check the size of the structure: it must be at least 8 bytes long */
			if (size < 8) {
				if (size)
					g_free (data);

				BRASERO_BURN_LOG ("READ ATIP failed (wrong size)");
				return BRASERO_BURN_ERR;
			}

			if (data->desc->erasable) {
				/* CDRW */
				priv->info = BRASERO_MEDIUM_CDRW;
				priv->type = types [3];
				priv->icon = icons [3];
			}
			else {
				/* CDR */
				priv->info = BRASERO_MEDIUM_CDR;
				priv->type = types [2];
				priv->icon = icons [2];
			}

			g_free (data);
		}

		return result;
	}

	switch (BRASERO_GET_16 (hdr->current_profile)) {
	case BRASERO_SCSI_PROF_CDROM:
		priv->info = BRASERO_MEDIUM_CDROM;
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
		priv->info = BRASERO_MEDIUM_DVD_ROM;
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

	/* WARNING: these types are recognized, no more */
	case BRASERO_SCSI_PROF_BD_ROM:
		priv->info = BRASERO_MEDIUM_BD_ROM;
		priv->type = types [13];
		priv->icon = icons [4];
		break;

	case BRASERO_SCSI_PROF_BR_R_SEQUENTIAL:
		priv->info = BRASERO_MEDIUM_BDR_SRM;
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

	case BRASERO_SCSI_PROF_DVD_RAM:
		priv->info = BRASERO_MEDIUM_DVD_RAM;
		priv->type = types [12];
		priv->icon = icons [8];
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
		priv->icon = icons [0];
		g_free (hdr);
		return BRASERO_BURN_NOT_SUPPORTED;
	}

	/* try all SCSI functions to get write/read speeds in order */
	if (hdr->desc->add_len >= sizeof (BraseroScsiRTStreamDesc)) {
		BraseroScsiRTStreamDesc *stream;

		/* means it's at least an MMC3 drive */
		stream = (BraseroScsiRTStreamDesc *) hdr->desc->data;
		if (stream->wrt_spd) {
			result = brasero_medium_get_speed_mmc3 (self, handle, code);
			if (result == BRASERO_BURN_OK)
				goto end;
		}

		if (stream->mp2a) {
			result = brasero_medium_get_page_2A_write_speed_desc (self, handle, code);
			if (result == BRASERO_BURN_OK)
				goto end;
		}
	}

	/* fallback for speeds */
	result = brasero_medium_get_page_2A_max_speed (self, handle, code);

end:

	g_free (hdr);

	if (result != BRASERO_BURN_OK)
		return result;

	return BRASERO_BURN_OK;
}

static BraseroBurnResult
brasero_medium_get_css_feature (BraseroMedium *self,
				BraseroDeviceHandle *handle,
				BraseroScsiErrCode *code)
{
	BraseroScsiGetConfigHdr *hdr = NULL;
	BraseroMediumPrivate *priv;
	BraseroScsiResult result;
	int size;

	priv = BRASERO_MEDIUM_PRIVATE (self);

	BRASERO_BURN_LOG ("Testing for Css encrypted media");
	result = brasero_mmc2_get_configuration_feature (handle,
							 BRASERO_SCSI_FEAT_DVD_CSS,
							 &hdr,
							 &size,
							 code);
	if (result != BRASERO_SCSI_OK) {
		g_free (hdr);

		BRASERO_BURN_LOG ("GET CONFIGURATION failed");
		return BRASERO_BURN_ERR;
	}

	if (hdr->desc->add_len < sizeof (BraseroScsiDVDCssDesc)) {
		g_free (hdr);
		return BRASERO_BURN_OK;
	}

	/* here we just need to see if this feature is current or not */
	if (hdr->desc->current) {
		priv->info |= BRASERO_MEDIUM_PROTECTED;
		BRASERO_BURN_LOG ("media is Css protected");
	}

	g_free (hdr);
	return BRASERO_BURN_OK;
}

static void
brasero_medium_init_real (BraseroMedium *object,
			  BraseroDeviceHandle *handle)
{
	guint i;
	gchar *name;
	BraseroBurnResult result;
	BraseroMediumPrivate *priv;
	BraseroScsiErrCode code = 0;

	priv = BRASERO_MEDIUM_PRIVATE (object);

	name = brasero_drive_get_display_name (priv->drive);
	BRASERO_BURN_LOG ("Initializing information for medium in %s", name);
	g_free (name);

	result = brasero_medium_get_medium_type (object, handle, &code);
	if (result != BRASERO_BURN_OK)
		return;

	brasero_medium_get_capacity_by_type (object, handle, &code);

	result = brasero_medium_get_contents (object, handle, &code);
	if (result != BRASERO_BURN_OK)
		return;

	/* assume that css feature is only for DVD-ROM which might be wrong but
	 * some drives wrongly reports that css is enabled for blank DVD+R/W */
	if (BRASERO_MEDIUM_IS (priv->info, (BRASERO_MEDIUM_DVD|BRASERO_MEDIUM_ROM)))
		brasero_medium_get_css_feature (object, handle, &code);

	BRASERO_BURN_LOG_DISC_TYPE (priv->info, "media is ");

	if (!priv->wr_speeds)
		return;

	/* sort write speeds */
	for (i = 0; priv->wr_speeds [i] != 0; i ++) {
		guint j;

		for (j = 0; priv->wr_speeds [j] != 0; j ++) {
			if (priv->wr_speeds [i] > priv->wr_speeds [j]) {
				gint64 tmp;

				tmp = priv->wr_speeds [i];
				priv->wr_speeds [i] = priv->wr_speeds [j];
				priv->wr_speeds [j] = tmp;
			}
		}
	}
}

static gboolean
brasero_medium_retry_open (gpointer object)
{
	const gchar *path;
	BraseroMedium *self;
	BraseroScsiErrCode code;
	BraseroMediumPrivate *priv;
	BraseroDeviceHandle *handle;

	self = BRASERO_MEDIUM (object);
	priv = BRASERO_MEDIUM_PRIVATE (object);
	path = brasero_drive_get_device (priv->drive);

	BRASERO_BURN_LOG ("Retrying to open device %s", path);
	handle = brasero_device_handle_open (path, &code);
	if (!handle) {
		if (code == BRASERO_SCSI_NOT_READY) {
			BRASERO_BURN_LOG ("Device busy");
			/* we'll retry in a second */
			return TRUE;
		}

		BRASERO_BURN_LOG ("Open () failed");
		priv->info = BRASERO_MEDIUM_UNSUPPORTED;
		priv->retry_id = 0;
		return FALSE;
	}

	BRASERO_BURN_LOG ("Open () succeeded\n");
	priv->info = BRASERO_MEDIUM_NONE;
	priv->icon = icons [0];

	priv->retry_id = 0;

	brasero_medium_init_real (self, handle);
	brasero_device_handle_close (handle);

	return FALSE;
}

static void
brasero_medium_try_open (BraseroMedium *self)
{
	const gchar *path;
	BraseroScsiErrCode code;
	BraseroMediumPrivate *priv;
	BraseroDeviceHandle *handle;

	priv = BRASERO_MEDIUM_PRIVATE (self);
	path = brasero_drive_get_device (priv->drive);

	/* the drive might be busy (a burning is going on) so we don't block
	 * but we re-try to open it every second */
	BRASERO_BURN_LOG ("Trying to open device %s", path);
	handle = brasero_device_handle_open (path, &code);
	if (!handle) {
		if (code == BRASERO_SCSI_NOT_READY) {
			BRASERO_BURN_LOG ("Device busy");
			priv->info = BRASERO_MEDIUM_BUSY;
			priv->icon = icons [0];

			priv->retry_id = g_timeout_add (BUSY_RETRY_TIME,
							brasero_medium_retry_open,
							self);
		}

		BRASERO_BURN_LOG ("Open () failed");
		return;
	}

	BRASERO_BURN_LOG ("Open () succeeded");
	brasero_medium_init_real (self, handle);
	brasero_device_handle_close (handle);
}

void
brasero_medium_reload_info (BraseroMedium *self)
{
	BraseroMediumPrivate *priv;

	priv = BRASERO_MEDIUM_PRIVATE (self);

	priv->max_rd = 0;
	priv->max_wrt = 0;
	priv->block_num = 0;
	priv->block_size = 0;
	priv->next_wr_add = -1;
	priv->type = NULL;
	priv->icon = NULL;
	priv->info = BRASERO_MEDIUM_NONE;

	if (priv->retry_id) {
		g_source_remove (priv->retry_id);
		priv->retry_id = 0;
	}

	g_free (priv->rd_speeds);
	priv->rd_speeds = NULL;

	g_free (priv->wr_speeds);
	priv->wr_speeds = NULL;

	g_slist_foreach (priv->tracks, (GFunc) g_free, NULL);
	g_slist_free (priv->tracks);
	priv->tracks = NULL;

	brasero_medium_try_open (self);
}

static void
brasero_medium_init_file (BraseroMedium *self)
{
	BraseroMediumPrivate *priv;

	priv = BRASERO_MEDIUM_PRIVATE (self);

	priv->info = BRASERO_MEDIUM_FILE;
	priv->type = types [0];
	priv->icon = icons [0];
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

	if (priv->udi) {
		g_free (priv->udi);
		priv->udi = NULL;
	}

	if (priv->retry_id) {
		g_source_remove (priv->retry_id);
		priv->retry_id = 0;
	}

	g_free (priv->rd_speeds);
	priv->rd_speeds = NULL;

	g_free (priv->wr_speeds);
	priv->wr_speeds = NULL;

	g_slist_foreach (priv->tracks, (GFunc) g_free, NULL);
	g_slist_free (priv->tracks);
	priv->tracks = NULL;

	priv->drive = NULL;

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
	case PROP_UDI:
		priv->udi = g_strdup (g_value_get_string (value));
		break;
	case PROP_DRIVE:
		/* we don't ref the drive here as it would create a circular
		 * dependency where the drive would hold a reference on the 
		 * medium and the medium on the drive */
		priv->drive = g_value_get_object (value);

		if (brasero_drive_is_fake (priv->drive)) {
			brasero_medium_init_file (BRASERO_MEDIUM (object));
			break;
		}

		brasero_medium_try_open (BRASERO_MEDIUM (object));
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
		g_value_set_object (value, priv->drive);
		break;
	case PROP_UDI:
		g_value_set_string (value, g_strdup (priv->udi));
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
	                                                      BRASERO_TYPE_DRIVE,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class,
	                                 PROP_UDI,
	                                 g_param_spec_string ("udi",
	                                                      "udi",
	                                                      "HAL udi",
	                                                      NULL,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

gboolean
brasero_medium_can_be_written (BraseroMedium *self)
{
	BraseroMediumPrivate *priv;
	BraseroDriveCaps caps;

	priv = BRASERO_MEDIUM_PRIVATE (self);

	if (!(priv->info & BRASERO_MEDIUM_REWRITABLE)
	&&   (priv->info & BRASERO_MEDIUM_CLOSED))
		return FALSE;

	if (priv->info & BRASERO_MEDIUM_FILE)
		return FALSE;

	caps = brasero_drive_get_caps (priv->drive);
	if (BRASERO_MEDIUM_IS (priv->info, BRASERO_MEDIUM_CDR))
		return (caps & BRASERO_DRIVE_CAPS_CDR) != 0;

	if (BRASERO_MEDIUM_IS (priv->info, BRASERO_MEDIUM_DVDR))
		return (caps & BRASERO_DRIVE_CAPS_DVDR) != 0;

	if (BRASERO_MEDIUM_IS (priv->info, BRASERO_MEDIUM_DVDR_PLUS))
		return (caps & BRASERO_DRIVE_CAPS_DVDR_PLUS) != 0;

	if (BRASERO_MEDIUM_IS (priv->info, BRASERO_MEDIUM_CDRW))
		return (caps & BRASERO_DRIVE_CAPS_CDRW) != 0;

	if (BRASERO_MEDIUM_IS (priv->info, BRASERO_MEDIUM_DVDRW))
		return (caps & BRASERO_DRIVE_CAPS_DVDRW) != 0;

	if (BRASERO_MEDIUM_IS (priv->info, BRASERO_MEDIUM_DVDRW_RESTRICTED))
		return (caps & BRASERO_DRIVE_CAPS_DVDRW) != 0;

	if (BRASERO_MEDIUM_IS (priv->info, BRASERO_MEDIUM_DVDRW_PLUS))
		return (caps & BRASERO_DRIVE_CAPS_DVDRW_PLUS) != 0;

	if (BRASERO_MEDIUM_IS (priv->info, BRASERO_MEDIUM_DVDR_PLUS_DL))
		return (caps & BRASERO_DRIVE_CAPS_DVDR_PLUS_DL) != 0;

	if (BRASERO_MEDIUM_IS (priv->info, BRASERO_MEDIUM_DVDRW_PLUS_DL))
		return (caps & BRASERO_DRIVE_CAPS_DVDRW_PLUS_DL) != 0;

	return FALSE;
}

gboolean
brasero_medium_can_be_rewritten (BraseroMedium *self)
{
	BraseroMediumPrivate *priv;
	BraseroDriveCaps caps;

	priv = BRASERO_MEDIUM_PRIVATE (self);

	if (!(priv->info & BRASERO_MEDIUM_REWRITABLE)
	||   (priv->info & BRASERO_MEDIUM_FILE))
		return FALSE;

	caps = brasero_drive_get_caps (priv->drive);
	if (BRASERO_MEDIUM_IS (priv->info, BRASERO_MEDIUM_CDRW))
		return (caps & BRASERO_DRIVE_CAPS_CDRW) != 0;

	if (BRASERO_MEDIUM_IS (priv->info, BRASERO_MEDIUM_DVDRW))
		return (caps & BRASERO_DRIVE_CAPS_DVDRW) != 0;

	if (BRASERO_MEDIUM_IS (priv->info, BRASERO_MEDIUM_DVDRW_RESTRICTED))
		return (caps & BRASERO_DRIVE_CAPS_DVDRW) != 0;

	if (BRASERO_MEDIUM_IS (priv->info, BRASERO_MEDIUM_DVDRW_PLUS))
		return (caps & BRASERO_DRIVE_CAPS_DVDRW_PLUS) != 0;

	if (BRASERO_MEDIUM_IS (priv->info, BRASERO_MEDIUM_DVDRW_PLUS_DL))
		return (caps & BRASERO_DRIVE_CAPS_DVDRW_PLUS_DL) != 0;

	return FALSE;
}

BraseroDrive *
brasero_medium_get_drive (BraseroMedium *self)
{
	BraseroMediumPrivate *priv;

	if (!self)
		return NULL;

	priv = BRASERO_MEDIUM_PRIVATE (self);
	return priv->drive;
}

const gchar *
brasero_medium_get_udi (BraseroMedium *self)
{
	BraseroMediumPrivate *priv;

	priv = BRASERO_MEDIUM_PRIVATE (self);
	return priv->udi;
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
