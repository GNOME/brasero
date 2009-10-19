/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Brasero
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-media is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-media authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-media. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-media is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-media is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#include <glib.h>

#include "scsi-base.h"
#include "scsi-device.h"

#include "scsi-error.h"
#include "scsi-mode-pages.h"
#include "scsi-inquiry.h"

#ifndef _BURN_SPC1_H
#define _BURN_SPC1_H

G_BEGIN_DECLS

BraseroScsiResult
brasero_spc1_test_unit_ready (BraseroDeviceHandle *handle,
			      BraseroScsiErrCode *error);

BraseroScsiResult
brasero_spc1_mode_sense_get_page (BraseroDeviceHandle *handle,
				  BraseroSPCPageType num,
				  BraseroScsiModeData **data,
				  int *data_size,
				  BraseroScsiErrCode *error);

BraseroScsiResult
brasero_spc1_mode_select (BraseroDeviceHandle *handle,
			  BraseroScsiModeData *data,
			  int size,
			  BraseroScsiErrCode *error);

BraseroScsiResult
brasero_spc1_inquiry (BraseroDeviceHandle *handle,
                      BraseroScsiInquiry *hdr,
                      BraseroScsiErrCode *error);

BraseroScsiResult
brasero_spc1_inquiry_is_optical_drive (BraseroDeviceHandle *handle,
                                       BraseroScsiErrCode *error);

G_END_DECLS

#endif /* _BURN_SPC1_H */

 
