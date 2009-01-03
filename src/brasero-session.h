/*
 * Brasero is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * Brasero is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */
 
/***************************************************************************
 *            brasero-session.h
 *
 *  Thu May 18 22:25:47 2006
 *  Copyright  2006  Philippe Rouquier
 *  <brasero-app@wanadoo.fr>
 ****************************************************************************/

 
#ifndef _BRASERO_SESSION_H
#define _BRASERO_SESSION_H

#include <glib.h>

#include "brasero-app.h"

G_BEGIN_DECLS

gboolean
brasero_session_connect (BraseroApp *app);
void
brasero_session_disconnect (BraseroApp *app);

G_END_DECLS

#endif /* _BRASERO-SESSION_H */
