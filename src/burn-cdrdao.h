/***************************************************************************
 *            cdrdao.h
 *
 *  dim jan 22 15:38:18 2006
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

#ifndef CDRDAO_H
#define CDRDAO_H

#include <glib.h>
#include <glib-object.h>

#include "burn-process.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_CDRDAO         (brasero_cdrdao_get_type ())
#define BRASERO_CDRDAO(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_CDRDAO, BraseroCdrdao))
#define BRASERO_CDRDAO_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_CDRDAO, BraseroCdrdaoClass))
#define BRASERO_IS_CDRDAO(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_CDRDAO))
#define BRASERO_IS_CDRDAO_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_CDRDAO))
#define BRASERO_CDRDAO_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_CDRDAO, BraseroCdrdaoClass))

typedef struct BraseroCdrdaoPrivate BraseroCdrdaoPrivate;

typedef struct {
	BraseroProcess process;
	BraseroCdrdaoPrivate *priv;
} BraseroCdrdao;

typedef struct {
	BraseroProcessClass parent_class;
} BraseroCdrdaoClass;

GType brasero_cdrdao_get_type();

BraseroCdrdao *brasero_cdrdao_new();

#endif /* CDRDAO_H */
