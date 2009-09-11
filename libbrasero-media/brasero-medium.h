/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-media
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

#include <glib-object.h>

#include <brasero-media.h>

#ifndef _BURN_MEDIUM_H_
#define _BURN_MEDIUM_H_

G_BEGIN_DECLS

#define BRASERO_TYPE_MEDIUM             (brasero_medium_get_type ())
#define BRASERO_MEDIUM(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_MEDIUM, BraseroMedium))
#define BRASERO_MEDIUM_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_MEDIUM, BraseroMediumClass))
#define BRASERO_IS_MEDIUM(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_MEDIUM))
#define BRASERO_IS_MEDIUM_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_MEDIUM))
#define BRASERO_MEDIUM_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_MEDIUM, BraseroMediumClass))

typedef struct _BraseroMediumClass BraseroMediumClass;

/**
 * BraseroMedium:
 *
 * Represents a physical medium currently inserted in a #BraseroDrive.
 **/
typedef struct _BraseroMedium BraseroMedium;

/**
 * BraseroDrive:
 *
 * Represents a physical drive currently connected.
 **/
typedef struct _BraseroDrive BraseroDrive;

struct _BraseroMediumClass
{
	GObjectClass parent_class;
};

struct _BraseroMedium
{
	GObject parent_instance;
};

GType brasero_medium_get_type (void) G_GNUC_CONST;


BraseroMedia
brasero_medium_get_status (BraseroMedium *medium);

guint64
brasero_medium_get_max_write_speed (BraseroMedium *medium);

guint64 *
brasero_medium_get_write_speeds (BraseroMedium *medium);

void
brasero_medium_get_free_space (BraseroMedium *medium,
			       goffset *bytes,
			       goffset *blocks);

void
brasero_medium_get_capacity (BraseroMedium *medium,
			     goffset *bytes,
			     goffset *blocks);

void
brasero_medium_get_data_size (BraseroMedium *medium,
			      goffset *bytes,
			      goffset *blocks);

gint64
brasero_medium_get_next_writable_address (BraseroMedium *medium);

gboolean
brasero_medium_can_be_rewritten (BraseroMedium *medium);

gboolean
brasero_medium_can_be_written (BraseroMedium *medium);

const gchar *
brasero_medium_get_CD_TEXT_title (BraseroMedium *medium);

const gchar *
brasero_medium_get_type_string (BraseroMedium *medium);

gchar *
brasero_medium_get_tooltip (BraseroMedium *medium);

BraseroDrive *
brasero_medium_get_drive (BraseroMedium *medium);

guint
brasero_medium_get_track_num (BraseroMedium *medium);

gboolean
brasero_medium_get_last_data_track_space (BraseroMedium *medium,
					  goffset *bytes,
					  goffset *sectors);

gboolean
brasero_medium_get_last_data_track_address (BraseroMedium *medium,
					    goffset *bytes,
					    goffset *sectors);

gboolean
brasero_medium_get_track_space (BraseroMedium *medium,
				guint num,
				goffset *bytes,
				goffset *sectors);

gboolean
brasero_medium_get_track_address (BraseroMedium *medium,
				  guint num,
				  goffset *bytes,
				  goffset *sectors);

gboolean
brasero_medium_can_use_dummy_for_sao (BraseroMedium *medium);

gboolean
brasero_medium_can_use_dummy_for_tao (BraseroMedium *medium);

gboolean
brasero_medium_can_use_burnfree (BraseroMedium *medium);

gboolean
brasero_medium_can_use_sao (BraseroMedium *medium);

gboolean
brasero_medium_can_use_tao (BraseroMedium *medium);

G_END_DECLS

#endif /* _BURN_MEDIUM_H_ */
