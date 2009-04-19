/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-burn
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-burn is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-burn authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-burn. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-burn is distributed in the hope that it will be useful,
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

#ifndef _BURN_TRACK_IMAGE_CFG_H_
#define _BURN_TRACK_IMAGE_CFG_H_

#include <glib-object.h>

#include <brasero-track.h>
#include <brasero-track-image.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_TRACK_IMAGE_CFG             (brasero_track_image_cfg_get_type ())
#define BRASERO_TRACK_IMAGE_CFG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_TRACK_IMAGE_CFG, BraseroTrackImageCfg))
#define BRASERO_TRACK_IMAGE_CFG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_TRACK_IMAGE_CFG, BraseroTrackImageCfgClass))
#define BRASERO_IS_TRACK_IMAGE_CFG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_TRACK_IMAGE_CFG))
#define BRASERO_IS_TRACK_IMAGE_CFG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_TRACK_IMAGE_CFG))
#define BRASERO_TRACK_IMAGE_CFG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_TRACK_IMAGE_CFG, BraseroTrackImageCfgClass))

typedef struct _BraseroTrackImageCfgClass BraseroTrackImageCfgClass;
typedef struct _BraseroTrackImageCfg BraseroTrackImageCfg;

struct _BraseroTrackImageCfgClass
{
	BraseroTrackImageClass parent_class;
};

struct _BraseroTrackImageCfg
{
	BraseroTrackImage parent_instance;
};

GType brasero_track_image_cfg_get_type (void) G_GNUC_CONST;

BraseroTrackImageCfg *
brasero_track_image_cfg_new (void);

BraseroBurnResult
brasero_track_image_cfg_set_source (BraseroTrackImageCfg *track,
				    const gchar *uri);

BraseroBurnResult
brasero_track_image_cfg_force_format (BraseroTrackImageCfg *track,
				      BraseroImageFormat format);

BraseroImageFormat
brasero_track_image_cfg_get_forced_format (BraseroTrackImageCfg *track);

G_END_DECLS

#endif /* _BURN_TRACK_IMAGE_CFG_H_ */
