/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero-git-trunk
 * Copyright (C) Philippe Rouquier 2008 <bonfire-app@wanadoo.fr>
 * 
 * brasero-git-trunk is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * brasero-git-trunk is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _BRASERO_SRC_INFO_H_
#define _BRASERO_SRC_INFO_H_

#include <glib-object.h>
#include <gtk/gtkhbox.h>

#include "burn-medium.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_SRC_INFO             (brasero_src_info_get_type ())
#define BRASERO_SRC_INFO(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_SRC_INFO, BraseroSrcInfo))
#define BRASERO_SRC_INFO_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_SRC_INFO, BraseroSrcInfoClass))
#define BRASERO_IS_SRC_INFO(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_SRC_INFO))
#define BRASERO_IS_SRC_INFO_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_SRC_INFO))
#define BRASERO_SRC_INFO_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_SRC_INFO, BraseroSrcInfoClass))

typedef struct _BraseroSrcInfoClass BraseroSrcInfoClass;
typedef struct _BraseroSrcInfo BraseroSrcInfo;

struct _BraseroSrcInfoClass
{
	GtkHBoxClass parent_class;
};

struct _BraseroSrcInfo
{
	GtkHBox parent_instance;
};

GType brasero_src_info_get_type (void) G_GNUC_CONST;

GtkWidget *
brasero_src_info_new (void);

void
brasero_src_info_set_medium (BraseroSrcInfo *info,
			     BraseroMedium *medium);

G_END_DECLS

#endif /* _BRASERO_SRC_INFO_H_ */
