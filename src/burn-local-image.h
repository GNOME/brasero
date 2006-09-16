/***************************************************************************
 *            burn-local-image.h
 *
 *  dim jui  9 10:54:14 2006
 *  Copyright  2006  Rouquier Philippe
 *  brasero-app@wanadoo.fr
 ***************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef BURN_LOCAL_IMAGE_H
#define BURN_LOCAL_IMAGE_H

#include <glib.h>
#include <glib-object.h>

#include "burn-job.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_LOCAL_IMAGE         (brasero_local_image_get_type ())
#define BRASERO_LOCAL_IMAGE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_LOCAL_IMAGE, BraseroLocalImage))
#define BRASERO_LOCAL_IMAGE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_LOCAL_IMAGE, BraseroLocalImageClass))
#define BRASERO_IS_LOCAL_IMAGE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_LOCAL_IMAGE))
#define BRASERO_IS_LOCAL_IMAGE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_LOCAL_IMAGE))
#define BRASERO_LOCAL_IMAGE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_LOCAL_IMAGE, BraseroLocalImageClass))

typedef struct _BraseroLocalImage BraseroLocalImage;
typedef struct _BraseroLocalImagePrivate BraseroLocalImagePrivate;
typedef struct _BraseroLocalImageClass BraseroLocalImageClass;

struct _BraseroLocalImage {
	BraseroJob parent;
	BraseroLocalImagePrivate *priv;
};

struct _BraseroLocalImageClass {
	BraseroJobClass parent_class;
};

GType brasero_local_image_get_type();
BraseroLocalImage *brasero_local_image_new();

G_END_DECLS

#endif /* BURN_LOCAL_IMAGE_H */
