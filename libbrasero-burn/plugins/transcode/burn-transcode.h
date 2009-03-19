/***************************************************************************
 *            transcode.h
 *
 *  ven jui  8 16:15:04 2005
 *  Copyright  2005  Philippe Rouquier
 *  Brasero-app@wanadoo.fr
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

#ifndef TRANSCODE_H
#define TRANSCODE_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_TRANSCODE         (brasero_transcode_get_type ())
#define BRASERO_TRANSCODE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_TRANSCODE, BraseroTranscode))
#define BRASERO_TRANSCODE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), BRASERO_TYPE_TRANSCODE, BraseroTranscodeClass))
#define BRASERO_IS_TRANSCODE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_TRANSCODE))
#define BRASERO_IS_TRANSCODE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), BRASERO_TYPE_TRANSCODE))
#define BRASERO_TRANSCODE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), BRASERO_TYPE_TRANSCODE, BraseroTranscodeClass))

G_END_DECLS

#endif				/* TRANSCODE_H */
