/***************************************************************************
 *            burn-spc1.h
 *
 *  Fri Oct 20 13:18:29 2006
 *  Copyright  2006  algernon
 *  <algernon@localhost.localdomain>
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
#include "scsi-mode-pages.h"

#ifndef _BURN_SPC1_H
#define _BURN_SPC1_H

G_BEGIN_DECLS

BraseroScsiResult
brasero_spc1_mode_sense_get_page (BraseroDeviceHandle *handle,
				  BraseroSPCPageType num,
				  BraseroScsiModeData **data,
				  int *data_size,
				  BraseroScsiErrCode *error);

G_END_DECLS

#endif /* _BURN_SPC1_H */

 
