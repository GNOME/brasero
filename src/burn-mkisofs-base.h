/***************************************************************************
 *            mkisofs-case.h
 *
 *  mar jan 24 16:41:02 2006
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

#ifndef MKISOFS_BASE_H
#define MKISOFS_BASE_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

/**
 * This is used by both growisofs and mkisofs objects to write grafts to a file
 */
BraseroBurnResult
brasero_mkisofs_base_write_to_files (GSList *grafts,
				     GSList *excluded,
				     const gchar *emptydir,
				     const gchar *videodir,
				     const gchar *grafts_path,
				     const gchar *excluded_path,
				     GError **error);

G_END_DECLS

#endif /* MKISOFS_CASE_H */
