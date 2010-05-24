/***************************************************************************
 *            multi-dnd.h
 *
 *  Wed Sep 27 17:43:05 2006
 *  Copyright  2006  Rouquier Philippe
 *  <bonfire-app@wanadoo.fr>
 ****************************************************************************/

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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */
 
#ifndef _MULTI_DND_H
#define _MULTI_DND_H

#include <glib.h>


G_BEGIN_DECLS

void
brasero_enable_multi_DND (void);

gboolean
brasero_enable_multi_DND_for_model_type (GType type);

G_END_DECLS

#endif /* _MULTI_DND_H */

 
