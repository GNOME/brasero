/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2008 <bonfire-app@wanadoo.fr>
 * 
 * brasero is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * brasero is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _BRASERO_VCD_IMAGER_H_
#define _BRASERO_VCD_IMAGER_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_VCD_IMAGER             (brasero_vcd_imager_get_type ())
#define BRASERO_VCD_IMAGER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_VCD_IMAGER, BraseroVcdImager))
#define BRASERO_VCD_IMAGER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_VCD_IMAGER, BraseroVcdImagerClass))
#define BRASERO_IS_VCD_IMAGER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_VCD_IMAGER))
#define BRASERO_IS_VCD_IMAGER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_VCD_IMAGER))
#define BRASERO_VCD_IMAGER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_VCD_IMAGER, BraseroVcdImagerClass))

G_END_DECLS

#endif /* _BRASERO_VCD_IMAGER_H_ */
