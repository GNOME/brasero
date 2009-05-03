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

#ifndef _BRASERO_FILE_MONITOR_H_
#define _BRASERO_FILE_MONITOR_H_

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
	BRASERO_FILE_MONITOR_FILE,
	BRASERO_FILE_MONITOR_FOLDER
} BraseroFileMonitorType;

typedef	gboolean	(*BraseroMonitorFindFunc)	(gpointer data, gpointer callback_data);

#define BRASERO_TYPE_FILE_MONITOR             (brasero_file_monitor_get_type ())
#define BRASERO_FILE_MONITOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_FILE_MONITOR, BraseroFileMonitor))
#define BRASERO_FILE_MONITOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_FILE_MONITOR, BraseroFileMonitorClass))
#define BRASERO_IS_FILE_MONITOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_FILE_MONITOR))
#define BRASERO_IS_FILE_MONITOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_FILE_MONITOR))
#define BRASERO_FILE_MONITOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_FILE_MONITOR, BraseroFileMonitorClass))

typedef struct _BraseroFileMonitorClass BraseroFileMonitorClass;
typedef struct _BraseroFileMonitor BraseroFileMonitor;

struct _BraseroFileMonitorClass
{
	GObjectClass parent_class;

	/* Virtual functions */

	/* if name is NULL then it's happening on the callback_data */
	void		(*file_added)		(BraseroFileMonitor *monitor,
						 gpointer callback_data,
						 const gchar *name);

	/* NOTE: there is no dest_type here as it must be a FOLDER type */
	void		(*file_moved)		(BraseroFileMonitor *self,
						 BraseroFileMonitorType src_type,
						 gpointer callback_src,
						 const gchar *name_src,
						 gpointer callback_dest,
						 const gchar *name_dest);
	void		(*file_renamed)		(BraseroFileMonitor *monitor,
						 BraseroFileMonitorType type,
						 gpointer callback_data,
						 const gchar *old_name,
						 const gchar *new_name);
	void		(*file_removed)		(BraseroFileMonitor *monitor,
						 BraseroFileMonitorType type,
						 gpointer callback_data,
						 const gchar *name);
	void		(*file_modified)	(BraseroFileMonitor *monitor,
						 gpointer callback_data,
						 const gchar *name);
};

struct _BraseroFileMonitor
{
	GObject parent_instance;
};

GType brasero_file_monitor_get_type (void) G_GNUC_CONST;

gboolean
brasero_file_monitor_single_file (BraseroFileMonitor *monitor,
				  const gchar *uri,
				  gpointer callback_data);
gboolean
brasero_file_monitor_directory_contents (BraseroFileMonitor *monitor,
				  	 const gchar *uri,
				       	 gpointer callback_data);
void
brasero_file_monitor_reset (BraseroFileMonitor *monitor);

void
brasero_file_monitor_foreach_cancel (BraseroFileMonitor *self,
				     BraseroMonitorFindFunc func,
				     gpointer callback_data);

G_END_DECLS

#endif /* _BRASERO_FILE_MONITOR_H_ */
