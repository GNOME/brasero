/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2007-2008 <bonfire-app@wanadoo.fr>
 * 
 *  Brasero is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 * 
 * brasero is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with brasero.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef BUILD_PREVIEW

#ifndef _BRASERO_PREVIEW_H_
#define _BRASERO_PREVIEW_H_

#include <glib-object.h>
#include <gtk/gtk.h>

#include "brasero-uri-container.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_PREVIEW             (brasero_preview_get_type ())
#define BRASERO_PREVIEW(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_PREVIEW, BraseroPreview))
#define BRASERO_PREVIEW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_PREVIEW, BraseroPreviewClass))
#define BRASERO_IS_PREVIEW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_PREVIEW))
#define BRASERO_IS_PREVIEW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_PREVIEW))
#define BRASERO_PREVIEW_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_PREVIEW, BraseroPreviewClass))

typedef struct _BraseroPreviewClass BraseroPreviewClass;
typedef struct _BraseroPreview BraseroPreview;

struct _BraseroPreviewClass
{
	GtkAlignmentClass parent_class;
};

struct _BraseroPreview
{
	GtkAlignment parent_instance;
};

GType brasero_preview_get_type (void) G_GNUC_CONST;
GtkWidget *brasero_preview_new (void);

void
brasero_preview_add_source (BraseroPreview *preview,
			    BraseroURIContainer *source);

void
brasero_preview_hide (BraseroPreview *preview);

void
brasero_preview_set_enabled (BraseroPreview *self,
			     gboolean preview);

G_END_DECLS

#endif /* _BRASERO_PREVIEW_H_ */

#endif /* BUILD_PREVIEW */
