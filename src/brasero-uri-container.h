/***************************************************************************
 *            brasero-uri-container.h
 *
 *  lun mai 22 08:54:18 2006
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

#ifndef BRASERO_URI_CONTAINER_H
#define BRASERO_URI_CONTAINER_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define BRASERO_TYPE_URI_CONTAINER         (brasero_uri_container_get_type ())
#define BRASERO_URI_CONTAINER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), BRASERO_TYPE_URI_CONTAINER, BraseroURIContainer))
#define BRASERO_IS_URI_CONTAINER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), BRASERO_TYPE_URI_CONTAINER))
#define BRASERO_URI_CONTAINER_GET_IFACE(o) (G_TYPE_INSTANCE_GET_INTERFACE ((o), BRASERO_TYPE_URI_CONTAINER, BraseroURIContainerIFace))


typedef struct _BraseroURIContainer BraseroURIContainer;

typedef struct {
	GTypeInterface g_iface;

	/* signals */
	void		(*uri_selected)		(BraseroURIContainer *container);
	void		(*uri_activated)	(BraseroURIContainer *container);

	/* virtual functions */
	gboolean	(*get_boundaries)	(BraseroURIContainer *container,
						 gint64 *start,
						 gint64 *end);
	gchar*		(*get_selected_uri)	(BraseroURIContainer *container);
	gchar**		(*get_selected_uris)	(BraseroURIContainer *container);

} BraseroURIContainerIFace;


GType brasero_uri_container_get_type (void);

gboolean
brasero_uri_container_get_boundaries (BraseroURIContainer *container,
				      gint64 *start,
				      gint64 *end);
gchar *
brasero_uri_container_get_selected_uri (BraseroURIContainer *container);
gchar **
brasero_uri_container_get_selected_uris (BraseroURIContainer *container);

void
brasero_uri_container_uri_selected (BraseroURIContainer *container);
void
brasero_uri_container_uri_activated (BraseroURIContainer *container);

G_END_DECLS

#endif /* BRASERO_URI_CONTAINER_H */
