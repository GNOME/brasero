/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * brasero
 * Copyright (C) Philippe Rouquier 2008 <bonfire-app@wanadoo.fr>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifndef _BRASERO_VIDEO_PROJECT_H_
#define _BRASERO_VIDEO_PROJECT_H_

#include <glib-object.h>
#include <gdk/gdk.h>

#ifdef BUILD_INOTIFY

#include "brasero-file-monitor.h"

#endif

#include "burn-track.h"
#include "brasero-disc.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_VIDEO_PROJECT             (brasero_video_project_get_type ())
#define BRASERO_VIDEO_PROJECT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_VIDEO_PROJECT, BraseroVideoProject))
#define BRASERO_VIDEO_PROJECT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_VIDEO_PROJECT, BraseroVideoProjectClass))
#define BRASERO_IS_VIDEO_PROJECT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_VIDEO_PROJECT))
#define BRASERO_IS_VIDEO_PROJECT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_VIDEO_PROJECT))
#define BRASERO_VIDEO_PROJECT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_VIDEO_PROJECT, BraseroVideoProjectClass))

typedef struct _BraseroVideoProjectClass BraseroVideoProjectClass;
typedef struct _BraseroVideoProject BraseroVideoProject;

typedef struct _BraseroVideoFile BraseroVideoFile;
struct _BraseroVideoFile {
	BraseroVideoFile *prev;
	BraseroVideoFile *next;

	gchar *name;
	gchar *uri;

	BraseroSongInfo *info;

	guint64 start;
	guint64 end;

	GdkPixbuf *snapshot;

	guint editable:1;
	guint is_loading:1;
	guint is_reloading:1;
	guint is_monitored:1;
};

struct _BraseroVideoProjectClass
{
#ifdef BUILD_INOTIFY
	BraseroFileMonitorClass parent_class;
#else
	GObjectClass parent_class;
#endif

	/* virtual functions */

	/**
	 * num_nodes is the number of nodes that were at the root of the 
	 * project.
	 */
	void		(*reset)		(BraseroVideoProject *project,
						 guint num_nodes);

	/* NOTE: node_added is also called when there is a moved node;
	 * in this case a node_removed is first called and then the
	 * following function is called (mostly to match GtkTreeModel
	 * API). To detect such a case look at uri which will then be
	 * set to NULL.
	 * NULL uri can also happen when it's a created directory.
	 * if return value is FALSE, node was invalidated during call */
	gboolean	(*node_added)		(BraseroVideoProject *project,
						 BraseroVideoFile *node);

	/* This is more an unparent signal. It shouldn't be assumed that the
	 * node was destroyed or not destroyed. Like the above function, it is
	 * also called when a node is moved. */
	void		(*node_removed)		(BraseroVideoProject *project,
						 BraseroVideoFile *node);

	void		(*node_changed)		(BraseroVideoProject *project,
						 BraseroVideoFile *node);

	/* NOTE: there is no node reordered as this list order cannot be changed */
};

struct _BraseroVideoProject
{
#ifdef BUILD_INOTIFY
	BraseroFileMonitor parent_instance;
#else
	GObject parent_instance;
#endif
};

GType brasero_video_project_get_type (void) G_GNUC_CONST;

void
brasero_video_file_free (BraseroVideoFile *file);

guint64
brasero_video_project_get_size (BraseroVideoProject *project);

guint
brasero_video_project_get_file_num (BraseroVideoProject *project);

void
brasero_video_project_reset (BraseroVideoProject *project);

void
brasero_video_project_move (BraseroVideoProject *project,
			    BraseroVideoFile *file,
			    BraseroVideoFile *next);

void
brasero_video_project_rename (BraseroVideoProject *project,
			      BraseroVideoFile *file,
			      const gchar *name);

void
brasero_video_project_remove_file (BraseroVideoProject *project,
				   BraseroVideoFile *file);

BraseroVideoFile *
brasero_video_project_add_uri (BraseroVideoProject *project,
			       const gchar *uri,
			       BraseroVideoFile *sibling,
			       gint64 start,
			       gint64 end);

BraseroDiscResult
brasero_video_project_get_status (BraseroVideoProject *project);

GSList *
brasero_video_project_get_contents (BraseroVideoProject *project);

BraseroVideoFile *
brasero_video_project_get_nth_item (BraseroVideoProject *project,
				    guint nth);
guint
brasero_video_project_get_item_index (BraseroVideoProject *project,
				      BraseroVideoFile *file);

G_END_DECLS

#endif /* _BRASERO_VIDEO_PROJECT_H_ */
