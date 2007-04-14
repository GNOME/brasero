/***************************************************************************
 *            burn-utils.h
 *
 *  Thu Oct 19 14:19:16 2006
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

#include <errno.h>
#include <glib.h>

#include "scsi-base.h"
#include "burn-debug.h"

#ifndef _BURN_UTILS_H
#define _BURN_UTILS_H

G_BEGIN_DECLS

#define BRASERO_GET_BCD(data)		((((uchar)(data)&0xF0)>>4)*10+((uchar)(data)&0x0F))
#define BRASERO_GET_16(data)		(((uchar)(data)[0]<<8)+(uchar)(data)[1])
#define BRASERO_GET_24(data)		(((uchar)(data)[0]<<16)+((uchar)(data)[1]<<8)+((uchar)(data)[2]))
#define BRASERO_GET_32(data)		(((uchar)(data)[0]<<24)+((uchar)(data)[1]<<16)+((uchar)(data)[2]<<8)+(uchar)(data)[3])

#define BRASERO_SET_BCD(data, num)	(uchar)(data)=((((num)/10)<<4)&0xF0)|((num)-(((num)/10)*10))
#define BRASERO_SET_16(data, num)	(data)[0]=(((num)>>8)&0xFF);(data)[1]=(uchar)((num)&0xFF)
#define BRASERO_SET_32(data, num)	(data)[0]=(uchar)(((num)>>24)&0xFF);(data)[1]=(uchar)(((num)>>16)&0xFF);(data)[2]=(uchar)(((num)>>8)&0xFF);(data)[3]=(uchar)((num)&0xFF)

#define BRASERO_MSF_TO_LBA(minute, second, frame)	(((minute)*60+(second))*75+frame)

/**
 * Used to report errors and have some sort of debug output easily
 */

#define BRASERO_SCSI_SET_ERRCODE(err, code)					\
{										\
	if (code == BRASERO_SCSI_ERRNO)	 {					\
		BRASERO_BURN_LOG ("SCSI command error: %s",			\
				  strerror (errno));				\
	} else {								\
		BRASERO_BURN_LOG ("SCSI command error: %s",			\
				  brasero_scsi_strerror (errno));		\
	}									\
	if (err)								\
		*(err) = code;							\
}

G_END_DECLS

#endif /* _BURN_UTILS_H */

 
