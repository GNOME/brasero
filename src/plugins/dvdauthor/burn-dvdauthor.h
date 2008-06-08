/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * burn-dvdauthor.c
 * Copyright (C) Philippe Rouquier 2008 <bonfire-app@wanadoo.fr>
 * 
 * burn-dvdauthor.c is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * burn-dvdauthor.c is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with burn-dvdauthor.c.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifndef _BRASERO_DVD_AUTHOR_H_
#define _BRASERO_DVD_AUTHOR_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_DVD_AUTHOR             (brasero_dvd_author_get_type ())
#define BRASERO_DVD_AUTHOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_DVD_AUTHOR, BraseroDvdAuthor))
#define BRASERO_DVD_AUTHOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_DVD_AUTHOR, BraseroDvdAuthorClass))
#define BRASERO_IS_DVD_AUTHOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_DVD_AUTHOR))
#define BRASERO_IS_DVD_AUTHOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_DVD_AUTHOR))
#define BRASERO_DVD_AUTHOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_DVD_AUTHOR, BraseroDvdAuthorClass))

G_END_DECLS

#endif /* _BRASERO_DVD_AUTHOR_H_ */
