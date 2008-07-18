/***************************************************************************
 *            burn-sense-data.h
 *
 *  Fri Oct 20 12:24:21 2006
 *  Copyright  2006  Rouquier Philippe
 *  <Rouquier Philippe@localhost.localdomain>
 ****************************************************************************/

/*
 * Brasero is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * Brasero is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */

#include "scsi-error.h"
#include "scsi-base.h"

#ifndef _BURN_SENSE_DATA_H
#define _BURN_SENSE_DATA_H

G_BEGIN_DECLS

#define BRASERO_SENSE_DATA_SIZE		19

BraseroScsiResult
brasero_sense_data_process (uchar *sense_data, BraseroScsiErrCode *err);

G_END_DECLS

#endif /* _BURN_SENSE_DATA_H */

 
