/***************************************************************************
 *            scsi-read-capacity.h
 *
 *  Sat Oct 28 12:08:37 2006
 *  Copyright  2006  algernon
 *  <algernon@localhost.localdomain>
 ****************************************************************************/

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */

#include <glib.h>

#include "scsi-base.h"

#ifndef _SCSI_READ_CAPACITY_H
#define _SCSI_READ_CAPACITY_H

G_BEGIN_DECLS

/* NOTE: lba is dependent on the media type and block size is always 2048 */
struct _BraseroScsiReadCapacityData {
	uchar lba		[4];
	uchar block_size	[4];
};
typedef struct _BraseroScsiReadCapacityData BraseroScsiReadCapacityData;

G_END_DECLS

#endif /* _SCSI_READ_CAPACITY_H */

 
