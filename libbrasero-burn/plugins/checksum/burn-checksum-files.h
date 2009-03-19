/***************************************************************************
 *            burn-sum.h
 *
 *  ven aoû  4 19:46:34 2006
 *  Copyright  2006  Rouquier Philippe
 *  brasero-app@wanadoo.fr
 ***************************************************************************/

/*
 *  Brasero is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Brasero is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifndef BURN_SUM_FILE_H
#define BURN_SUM_FILE_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_CHECKSUM_FILES		(brasero_checksum_files_get_type ())
#define BRASERO_CHECKSUM_FILES(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_CHECKSUM_FILES, BraseroChecksumFiles))
#define BRASERO_CHECKSUM_FILES_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_CHECKSUM_FILES, BraseroChecksumFilesClass))
#define BRASERO_IS_CHECKSUM_FILES(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_CHECKSUM_FILES))
#define BRASERO_IS_CHECKSUM_FILES_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_CHECKSUM_FILES))
#define BRASERO_CHECKSUM_FILES_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_CHECKSUM_FILES, BraseroChecksumFilesClass))

G_END_DECLS

#endif /* BURN_SUM_FILE_H */
