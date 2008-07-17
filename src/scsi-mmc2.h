/***************************************************************************
 *            scsi-mmc2.h
 *
 *  Mon Oct 23 19:46:20 2006
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

#include <glib.h>

#include "scsi-base.h"
#include "scsi-error.h"
#include "scsi-read-capacity.h"
#include "scsi-get-configuration.h"
#include "scsi-read-disc-structure.h"
#include "scsi-read-format-capacities.h"

#ifndef _SCSI_MMC2_H
#define _SCSI_MMC2_H

G_BEGIN_DECLS

BraseroScsiResult
brasero_mmc2_read_capacity (BraseroDeviceHandle *handle,
			    BraseroScsiReadCapacityData *data,
			    int size,
			    BraseroScsiErrCode *error);

BraseroScsiResult
brasero_mmc2_get_configuration_feature (BraseroDeviceHandle *handle,
					BraseroScsiFeatureType type,
					BraseroScsiGetConfigHdr **data,
					int *size,
					BraseroScsiErrCode *error);

BraseroScsiResult
brasero_mmc2_read_generic_structure (BraseroDeviceHandle *handle,
				     BraseroScsiGenericFormatType type,
				     BraseroScsiReadDiscStructureHdr **data,
				     int *size,
				     BraseroScsiErrCode *error);

BraseroScsiResult
brasero_mmc2_read_format_capacities (BraseroDeviceHandle *handle,
				     BraseroScsiFormatCapacitiesHdr **data,
				     int *size,
				     BraseroScsiErrCode *error);
G_END_DECLS

#endif /* _SCSI_MMC2_H */

 
