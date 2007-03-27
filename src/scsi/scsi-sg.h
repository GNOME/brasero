/***************************************************************************
 *            burn-sg.h
 *
 *  Wed Oct 18 14:55:25 2006
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

#include <string.h>
#include <unistd.h>
#include <scsi/sg.h>

#include "scsi-error.h"

#ifndef _BURN_SG_H
#define _BURN_SG_H

#ifdef __cplusplus
extern "C"
{
#endif

BraseroScsiResult
brasero_sg_send_command (int fd, struct sg_io_hdr *command, BraseroScsiErrCode *error);

#ifdef __cplusplus
}
#endif

#endif /* _BURN_SG_H */

 
