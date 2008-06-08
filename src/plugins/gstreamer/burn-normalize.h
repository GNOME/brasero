/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero-normalize.c
 * Copyright (C) Rouquier Philippe 2008 <bonfire-app@wanadoo.fr>
 * 
 * brasero-normalize.c is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * brasero-normalize.c is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with brasero-normalize.c.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifndef _BRASERO_NORMALIZE_H_
#define _BRASERO_NORMALIZE_H_

#include <glib-object.h>

#include "burn-job.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_NORMALIZE             (brasero_normalize_get_type ())
#define BRASERO_NORMALIZE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_NORMALIZE, BraseroNormalize))
#define BRASERO_NORMALIZE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_NORMALIZE, BraseroNormalizeClass))
#define BRASERO_IS_NORMALIZE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_NORMALIZE))
#define BRASERO_IS_NORMALIZE_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_NORMALIZE))
#define BRASERO_NORMALIZE_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_NORMALIZE, BraseroNormalizeClass))

#define BRASERO_ALBUM_PEAK_VALUE	"peak_value"
#define BRASERO_ALBUM_GAIN_VALUE	"gain_value"
#define BRASERO_TRACK_PEAK_VALUE	"peak_value"
#define BRASERO_TRACK_GAIN_VALUE	"gain_value"

G_END_DECLS

#endif /* _BRASERO_NORMALIZE_H_ */
