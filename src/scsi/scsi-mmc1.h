/***************************************************************************
 *            burn-mmc1.h
 *
 *  Thu Oct 19 14:17:47 2006
 *  Copyright  2006  Rouquier Philippe
 *  <Rouquier Philippe@localhost.localdomain>
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

#include "scsi-error.h"
#include "scsi-read-disc-info.h"
#include "scsi-read-toc-pma-atip.h"


#ifndef _BURN_MMC1_H
#define _BURN_MMC1_H

G_BEGIN_DECLS

BraseroScsiResult
brasero_mmc1_read_disc_information_std (int fd,
					BraseroScsiDiscInfoStd **info_return,
					int *size,
					BraseroScsiErrCode *error);

BraseroScsiResult
brasero_mmc1_read_toc_formatted (int fd,
				 int track_num,
				 BraseroScsiFormattedTocData **data,
				 int *size,
				 BraseroScsiErrCode *error);

G_END_DECLS

#endif /* _BURN_MMC1_H */

 
