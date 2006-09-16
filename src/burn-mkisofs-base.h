/***************************************************************************
 *            mkisofs-case.h
 *
 *  mar jan 24 16:41:02 2006
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

#ifndef MKISOFS_CASE_H
#define MKISOFS_CASE_H

#include <glib.h>
#include <glib-object.h>

#include "burn-process.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_MKISOFS_BASE         (brasero_mkisofs_base_get_type ())
#define BRASERO_MKISOFS_BASE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_MKISOFS_BASE, BraseroMkisofsBase))
#define BRASERO_MKISOFS_BASE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_MKISOFS_BASE, BraseroMkisofsBaseClass))
#define BRASERO_IS_MKISOFS_BASE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_MKISOFS_BASE))
#define BRASERO_IS_MKISOFS_BASE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_MKISOFS_BASE))
#define BRASERO_MKISOFS_BASE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_MKISOFS_BASE, BraseroMkisofsBaseClass))

typedef struct BraseroMkisofsBasePrivate BraseroMkisofsBasePrivate;

typedef struct {
	BraseroJob parent;
	BraseroMkisofsBasePrivate *priv;
} BraseroMkisofsBase;

typedef struct {
	BraseroJobClass parent_class;

	/* virtual methods */
	BraseroBurnResult	(*set_image_src)	(BraseroMkisofsBase *base,
							 const gchar *label,
							 const gchar *grafts_list,
							 const gchar *excluded_list,
							 gboolean use_utf8,
							 GError **error);
} BraseroMkisofsBaseClass;

GType brasero_mkisofs_base_get_type ();

#endif /* MKISOFS_CASE_H */
