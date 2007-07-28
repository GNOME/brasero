/***************************************************************************
 *            burn-libburn.h
 *
 *  lun aoû 21 14:33:24 2006
 *  Copyright  2006  Rouquier Philippe
 *  bonfire-app@wanadoo.fr
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <glib-object.h>

#ifndef BURN_LIBBURN_H
#define BURN_LIBBURN_H

G_BEGIN_DECLS

#define BRASERO_TYPE_LIBBURN         (brasero_libburn_get_type (NULL))
#define BRASERO_LIBBURN(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_LIBBURN, BraseroLibburn))
#define BRASERO_LIBBURN_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_LIBBURN, BraseroLibburnClass))
#define BRASERO_IS_LIBBURN(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_LIBBURN))
#define BRASERO_IS_LIBBURN_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_LIBBURN))
#define BRASERO_LIBBURN_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_LIBBURN, BraseroLibburnClass))

G_END_DECLS

#endif /* BURN_LIBBURN_H */
