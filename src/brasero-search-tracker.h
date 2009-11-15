/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Rouquier Philippe 2009 <bonfire-app@wanadoo.fr>
 * 
 * brasero is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * brasero is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _BRASERO_SEARCH_TRACKER_H_
#define _BRASERO_SEARCH_TRACKER_H_

#include <glib-object.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_SEARCH_TRACKER             (brasero_search_tracker_get_type ())
#define BRASERO_SEARCH_TRACKER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_SEARCH_TRACKER, BraseroSearchTracker))
#define BRASERO_SEARCH_TRACKER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_SEARCH_TRACKER, BraseroSearchTrackerClass))
#define BRASERO_IS_SEARCH_TRACKER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_SEARCH_TRACKER))
#define BRASERO_IS_SEARCH_TRACKER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_SEARCH_TRACKER))
#define BRASERO_SEARCH_TRACKER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_SEARCH_TRACKER, BraseroSearchTrackerClass))

typedef struct _BraseroSearchTrackerClass BraseroSearchTrackerClass;
typedef struct _BraseroSearchTracker BraseroSearchTracker;

struct _BraseroSearchTrackerClass
{
	GObjectClass parent_class;
};

struct _BraseroSearchTracker
{
	GObject parent_instance;
};

GType brasero_search_tracker_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* _BRASERO_SEARCH_TRACKER_H_ */
