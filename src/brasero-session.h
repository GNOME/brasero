/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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

#ifdef __cplusplus
extern "C"
{
#endif
gboolean
brasero_session_connect (BraseroApp *app);
void
brasero_session_disconnect (BraseroApp *app);
gboolean
brasero_session_save (BraseroApp *app);
gboolean
brasero_session_load (BraseroApp *app);

#ifdef __cplusplus
}
#endif

#endif /* _BRASERO-SESSION_H */
