/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Libbrasero-misc
 * Copyright (C) Philippe Rouquier 2005-2009 <bonfire-app@wanadoo.fr>
 *
 * Libbrasero-misc is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Libbrasero-misc authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Libbrasero-misc. This permission is above and beyond the permissions granted
 * by the GPL license by which Libbrasero-burn is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * Libbrasero-misc is distributed in the hope that it will be useful,
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

#ifndef _BRASERO_IO_H_
#define _BRASERO_IO_H_

#include <glib-object.h>
#include <gtk/gtk.h>

#include "brasero-async-task-manager.h"

G_BEGIN_DECLS

typedef enum {
	BRASERO_IO_INFO_NONE			= 0,
	BRASERO_IO_INFO_MIME			= 1,
	BRASERO_IO_INFO_ICON			= 1,
	BRASERO_IO_INFO_PERM			= 1 << 1,
	BRASERO_IO_INFO_METADATA		= 1 << 2,
	BRASERO_IO_INFO_METADATA_THUMBNAIL	= 1 << 3,
	BRASERO_IO_INFO_RECURSIVE		= 1 << 4,
	BRASERO_IO_INFO_CHECK_PARENT_SYMLINK	= 1 << 5,
	BRASERO_IO_INFO_METADATA_MISSING_CODEC	= 1 << 6,

	BRASERO_IO_INFO_FOLLOW_SYMLINK		= 1 << 7,

	BRASERO_IO_INFO_URGENT			= 1 << 9,
	BRASERO_IO_INFO_IDLE			= 1 << 10
} BraseroIOFlags;


typedef enum {
	BRASERO_IO_PHASE_START		= 0,
	BRASERO_IO_PHASE_DOWNLOAD,
	BRASERO_IO_PHASE_END
} BraseroIOPhase;

#define BRASERO_IO_XFER_DESTINATION	"xfer::destination"

#define BRASERO_IO_PLAYLIST_TITLE	"playlist::title"
#define BRASERO_IO_IS_PLAYLIST		"playlist::is_playlist"
#define BRASERO_IO_PLAYLIST_ENTRIES_NUM	"playlist::entries_num"

#define BRASERO_IO_COUNT_NUM		"count::num"
#define BRASERO_IO_COUNT_SIZE		"count::size"
#define BRASERO_IO_COUNT_INVALID	"count::invalid"

#define BRASERO_IO_THUMBNAIL		"metadata::thumbnail"

#define BRASERO_IO_LEN			"metadata::length"
#define BRASERO_IO_ISRC			"metadata::isrc"
#define BRASERO_IO_TITLE		"metadata::title"
#define BRASERO_IO_ARTIST		"metadata::artist"
#define BRASERO_IO_ALBUM		"metadata::album"
#define BRASERO_IO_GENRE		"metadata::genre"
#define BRASERO_IO_COMPOSER		"metadata::composer"
#define BRASERO_IO_HAS_AUDIO		"metadata::has_audio"
#define BRASERO_IO_HAS_VIDEO		"metadata::has_video"
#define BRASERO_IO_IS_SEEKABLE		"metadata::is_seekable"

#define BRASERO_IO_HAS_DTS			"metadata::audio::wav::has_dts"

#define BRASERO_IO_CHANNELS		"metadata::audio::channels"
#define BRASERO_IO_RATE				"metadata::audio::rate"

#define BRASERO_IO_DIR_CONTENTS_ADDR	"image::directory::address"

typedef struct _BraseroIOJobProgress BraseroIOJobProgress;

typedef void		(*BraseroIOResultCallback)	(GObject *object,
							 GError *error,
							 const gchar *uri,
							 GFileInfo *info,
							 gpointer callback_data);

typedef void		(*BraseroIOProgressCallback)	(GObject *object,
							 BraseroIOJobProgress *info,
							 gpointer callback_data);

typedef void		(*BraseroIODestroyCallback)	(GObject *object,
							 gboolean cancel,
							 gpointer callback_data);

typedef gboolean	(*BraseroIOCompareCallback)	(gpointer data,
							 gpointer user_data);


struct _BraseroIOJobCallbacks {
	BraseroIOResultCallback callback;
	BraseroIODestroyCallback destroy;
	BraseroIOProgressCallback progress;

	guint ref;

	/* Whether we are returning something for this base */
	guint in_use:1;
};
typedef struct _BraseroIOJobCallbacks BraseroIOJobCallbacks;

struct _BraseroIOJobBase {
	GObject *object;
	BraseroIOJobCallbacks *methods;
};
typedef struct _BraseroIOJobBase BraseroIOJobBase;

struct _BraseroIOResultCallbackData {
	gpointer callback_data;
	gint ref;
};
typedef struct _BraseroIOResultCallbackData BraseroIOResultCallbackData;

struct _BraseroIOJob {
	gchar *uri;
	BraseroIOFlags options;

	const BraseroIOJobBase *base;
	BraseroIOResultCallbackData *callback_data;
};
typedef struct _BraseroIOJob BraseroIOJob;

#define BRASERO_IO_JOB(data)	((BraseroIOJob *) (data))

void
brasero_io_job_free (gboolean cancelled,
		     BraseroIOJob *job);

void
brasero_io_set_job (BraseroIOJob *self,
		    const BraseroIOJobBase *base,
		    const gchar *uri,
		    BraseroIOFlags options,
		    BraseroIOResultCallbackData *callback_data);

void
brasero_io_push_job (BraseroIOJob *job,
		     const BraseroAsyncTaskType *type);

void
brasero_io_return_result (const BraseroIOJobBase *base,
			  const gchar *uri,
			  GFileInfo *info,
			  GError *error,
			  BraseroIOResultCallbackData *callback_data);


typedef GtkWindow *	(* BraseroIOGetParentWinCb)	(gpointer user_data);

void
brasero_io_set_parent_window_callback (BraseroIOGetParentWinCb callback,
                                       gpointer user_data);

void
brasero_io_shutdown (void);

/* NOTE: The split in methods and objects was
 * done to prevent jobs sharing the same methods
 * to return their results concurently. In other
 * words only one job among those sharing the
 * same methods can return its results. */
 
BraseroIOJobBase *
brasero_io_register (GObject *object,
		     BraseroIOResultCallback callback,
		     BraseroIODestroyCallback destroy,
		     BraseroIOProgressCallback progress);

BraseroIOJobBase *
brasero_io_register_with_methods (GObject *object,
                                  BraseroIOJobCallbacks *methods);

BraseroIOJobCallbacks *
brasero_io_register_job_methods (BraseroIOResultCallback callback,
                                 BraseroIODestroyCallback destroy,
                                 BraseroIOProgressCallback progress);

void
brasero_io_job_base_free (BraseroIOJobBase *base);

void
brasero_io_cancel_by_base (BraseroIOJobBase *base);

void
brasero_io_find_urgent (const BraseroIOJobBase *base,
			BraseroIOCompareCallback callback,
			gpointer callback_data);			

void
brasero_io_load_directory (const gchar *uri,
			   const BraseroIOJobBase *base,
			   BraseroIOFlags options,
			   gpointer callback_data);
void
brasero_io_get_file_info (const gchar *uri,
			  const BraseroIOJobBase *base,
			  BraseroIOFlags options,
			  gpointer callback_data);
void
brasero_io_get_file_count (GSList *uris,
			   const BraseroIOJobBase *base,
			   BraseroIOFlags options,
			   gpointer callback_data);
void
brasero_io_parse_playlist (const gchar *uri,
			   const BraseroIOJobBase *base,
			   BraseroIOFlags options,
			   gpointer callback_data);

guint64
brasero_io_job_progress_get_read (BraseroIOJobProgress *progress);

guint64
brasero_io_job_progress_get_total (BraseroIOJobProgress *progress);

BraseroIOPhase
brasero_io_job_progress_get_phase (BraseroIOJobProgress *progress);

guint
brasero_io_job_progress_get_file_processed (BraseroIOJobProgress *progress);

G_END_DECLS

#endif /* _BRASERO_IO_H_ */
