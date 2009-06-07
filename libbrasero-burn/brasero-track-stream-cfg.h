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

#ifndef _BRASERO_TRACK_STREAM_CFG_H_
#define _BRASERO_TRACK_STREAM_CFG_H_

#include <glib-object.h>

#include <brasero-track-stream.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_TRACK_STREAM_CFG             (brasero_track_stream_cfg_get_type ())
#define BRASERO_TRACK_STREAM_CFG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_TRACK_STREAM_CFG, BraseroTrackStreamCfg))
#define BRASERO_TRACK_STREAM_CFG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_TRACK_STREAM_CFG, BraseroTrackStreamCfgClass))
#define BRASERO_IS_TRACK_STREAM_CFG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_TRACK_STREAM_CFG))
#define BRASERO_IS_TRACK_STREAM_CFG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_TRACK_STREAM_CFG))
#define BRASERO_TRACK_STREAM_CFG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_TRACK_STREAM_CFG, BraseroTrackStreamCfgClass))

typedef struct _BraseroTrackStreamCfgClass BraseroTrackStreamCfgClass;
typedef struct _BraseroTrackStreamCfg BraseroTrackStreamCfg;

struct _BraseroTrackStreamCfgClass
{
	BraseroTrackStreamClass parent_class;
};

struct _BraseroTrackStreamCfg
{
	BraseroTrackStream parent_instance;
};

GType brasero_track_stream_cfg_get_type (void) G_GNUC_CONST;

BraseroTrackStreamCfg *
brasero_track_stream_cfg_new (void);

G_END_DECLS

#endif /* _BRASERO_TRACK_STREAM_CFG_H_ */
