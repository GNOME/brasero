/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-burn
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-burn is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-burn authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-burn. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-burn is distributed in the hope that it will be useful,
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

#ifndef _BRASERO_SESSION_SPAN_H_
#define _BRASERO_SESSION_SPAN_H_

#include <glib-object.h>

#include <brasero-session.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_SESSION_SPAN             (brasero_session_span_get_type ())
#define BRASERO_SESSION_SPAN(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_SESSION_SPAN, BraseroSessionSpan))
#define BRASERO_SESSION_SPAN_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_SESSION_SPAN, BraseroSessionSpanClass))
#define BRASERO_IS_SESSION_SPAN(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_SESSION_SPAN))
#define BRASERO_IS_SESSION_SPAN_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_SESSION_SPAN))
#define BRASERO_SESSION_SPAN_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_SESSION_SPAN, BraseroSessionSpanClass))

typedef struct _BraseroSessionSpanClass BraseroSessionSpanClass;
typedef struct _BraseroSessionSpan BraseroSessionSpan;

struct _BraseroSessionSpanClass
{
	BraseroBurnSessionClass parent_class;
};

struct _BraseroSessionSpan
{
	BraseroBurnSession parent_instance;
};

GType brasero_session_span_get_type (void) G_GNUC_CONST;

BraseroSessionSpan *
brasero_session_span_new (void);

BraseroBurnResult
brasero_session_span_again (BraseroSessionSpan *session);

BraseroBurnResult
brasero_session_span_possible (BraseroSessionSpan *session);

BraseroBurnResult
brasero_session_span_start (BraseroSessionSpan *session);

BraseroBurnResult
brasero_session_span_next (BraseroSessionSpan *session);

goffset
brasero_session_span_get_max_space (BraseroSessionSpan *session);

void
brasero_session_span_stop (BraseroSessionSpan *session);

G_END_DECLS

#endif /* _BRASERO_SESSION_SPAN_H_ */
