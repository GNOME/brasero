/***************************************************************************
 *            burn-mmc3.h
 *
 *  Thu Oct 19 14:18:35 2006
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

#include "scsi-error.h"
#include "scsi-get-performance.h"
#include "scsi-read-toc-pma-atip.h"

#ifndef _BURN_MMC3_H
#define _BURN_MMC3_H

#ifdef __cplusplus
extern "C"
{
#endif

BraseroScsiResult
brasero_mmc3_read_cd_text (int fd,
			   BraseroScsiCDTextData **data,
			   int *size,
			   BraseroScsiErrCode *error);

BraseroScsiResult
brasero_mmc3_get_performance_wrt_spd_desc (int fd,
					   BraseroScsiGetPerfData **data,
					   int *data_size,
					   BraseroScsiErrCode *error);

#ifdef __cplusplus
}
#endif

#endif /* _BURN_MMC3_H */

 
