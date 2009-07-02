/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * brasero-video-options.h
 * Copyright (C) Philippe Rouquier 2009 <bonfire-app@wanadoo.fr>
 * 
 * brasero-video-options.h is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * brasero-video-options.h is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _BRASERO_VIDEO_OPTIONS_H_
#define _BRASERO_VIDEO_OPTIONS_H_

#include <glib-object.h>

#include <gtk/gtk.h>

#include "brasero-session.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_VIDEO_OPTIONS             (brasero_video_options_get_type ())
#define BRASERO_VIDEO_OPTIONS(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_VIDEO_OPTIONS, BraseroVideoOptions))
#define BRASERO_VIDEO_OPTIONS_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_VIDEO_OPTIONS, BraseroVideoOptionsClass))
#define BRASERO_IS_VIDEO_OPTIONS(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_VIDEO_OPTIONS))
#define BRASERO_IS_VIDEO_OPTIONS_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_VIDEO_OPTIONS))
#define BRASERO_VIDEO_OPTIONS_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_VIDEO_OPTIONS, BraseroVideoOptionsClass))

typedef struct _BraseroVideoOptionsClass BraseroVideoOptionsClass;
typedef struct _BraseroVideoOptions BraseroVideoOptions;

struct _BraseroVideoOptionsClass
{
	GtkAlignmentClass parent_class;
};

struct _BraseroVideoOptions
{
	GtkAlignment parent_instance;
};

GType brasero_video_options_get_type (void) G_GNUC_CONST;

GtkWidget *
brasero_video_options_new (BraseroBurnSession *session);

void
brasero_video_options_set_session (BraseroVideoOptions *options,
                                   BraseroBurnSession *session);

G_END_DECLS

#endif /* _BRASERO_VIDEO_OPTIONS_H_ */
