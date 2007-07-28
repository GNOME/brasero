/***************************************************************************
 *            burn-sum.h
 *
 *  ven aoû  4 19:46:34 2006
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

#ifndef BURN_SUM_H
#define BURN_SUM_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_MD5SUM		(brasero_md5sum_get_type (NULL))
#define BRASERO_MD5SUM(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_MD5SUM, BraseroMd5sum))
#define BRASERO_MD5SUM_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_MD5SUM, BraseroMd5sumClass))
#define BRASERO_IS_MD5SUM(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_MD5SUM))
#define BRASERO_IS_MD5SUM_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_MD5SUM))
#define BRASERO_MD5SUM_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_MD5SUM, BraseroMd5sumClass))

#define BRASERO_CHECKSUM_FILE	".checksum.md5"

G_END_DECLS

#endif /* BURN_SUM_H */
