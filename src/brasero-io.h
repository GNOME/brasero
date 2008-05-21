/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * trunk
 * Copyright (C) Philippe Rouquier 2008 <bonfire-app@wanadoo.fr>
 * 
 * trunk is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * trunk is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with trunk.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#ifndef _BRASERO_IO_H_
#define _BRASERO_IO_H_

#include <glib-object.h>

#include "brasero-async-task-manager.h"

G_BEGIN_DECLS

#define BRASERO_TYPE_IO             (brasero_io_get_type ())
#define BRASERO_IO(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BRASERO_TYPE_IO, BraseroIO))
#define BRASERO_IO_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BRASERO_TYPE_IO, BraseroIOClass))
#define BRASERO_IS_IO(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BRASERO_TYPE_IO))
#define BRASERO_IS_IO_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BRASERO_TYPE_IO))
#define BRASERO_IO_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BRASERO_TYPE_IO, BraseroIOClass))

typedef struct _BraseroIOClass BraseroIOClass;
typedef struct _BraseroIO BraseroIO;

struct _BraseroIOClass
{
	BraseroAsyncTaskManagerClass parent_class;
};

struct _BraseroIO
{
	BraseroAsyncTaskManager parent_instance;
};

GType brasero_io_get_type (void) G_GNUC_CONST;

typedef enum {
	BRASERO_IO_INFO_NONE			= 0,
	BRASERO_IO_INFO_MIME			= 1,
	BRASERO_IO_INFO_ICON			= 1,
	BRASERO_IO_INFO_PERM			= 1 << 1,
	BRASERO_IO_INFO_METADATA		= 1 << 2,
	BRASERO_IO_INFO_RECURSIVE		= 1 << 3,
	BRASERO_IO_INFO_CHECK_PARENT_SYMLINK	= 1 << 4,
	BRASERO_IO_INFO_METADATA_MISSING_CODEC	= 1 << 5,

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

#define BRASERO_IO_COUNT_NUM		"count::num"
#define BRASERO_IO_COUNT_SIZE		"count::size"
#define BRASERO_IO_COUNT_INVALID	"count::invalid"

#define BRASERO_IO_LEN			"metadata::length"
#define BRASERO_IO_ISRC			"metadata::isrc"
#define BRASERO_IO_TITLE		"metadata::title"
#define BRASERO_IO_ARTIST		"metadata::artist"
#define BRASERO_IO_ALBUM		"metadata::album"
#define BRASERO_IO_ALBUM		"metadata::album"
#define BRASERO_IO_GENRE		"metadata::genre"
#define BRASERO_IO_COMPOSER		"metadata::composer"
#define BRASERO_IO_HAS_AUDIO		"metadata::has_audio"
#define BRASERO_IO_HAS_VIDEO		"metadata::has_video"
#define BRASERO_IO_IS_SEEKABLE		"metadata::is_seekable"

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

struct _BraseroIOJobBase {
	GObject *object;
	BraseroIOResultCallback callback;
	BraseroIODestroyCallback destroy;
	BraseroIOProgressCallback progress;
};
typedef struct _BraseroIOJobBase BraseroIOJobBase;


BraseroIO *
brasero_io_get_default (void);

BraseroIOJobBase *
brasero_io_register (GObject *object,
		     BraseroIOResultCallback callback,
		     BraseroIODestroyCallback destroy,
		     BraseroIOProgressCallback progress);

void
brasero_io_cancel_by_data (BraseroIO *self,
			   gpointer callback_data);

void
brasero_io_cancel_by_base (BraseroIO *self,
			   BraseroIOJobBase *base);

void
brasero_io_find_urgent (BraseroIO *self,
			const BraseroIOJobBase *base,
			BraseroIOCompareCallback callback,
			gpointer callback_data);			

void
brasero_io_load_directory (BraseroIO *self,
			   const gchar *uri,
			   const BraseroIOJobBase *base,
			   BraseroIOFlags options,
			   gpointer callback_data);
void
brasero_io_get_file_info (BraseroIO *self,
			  const gchar *uri,
			  const BraseroIOJobBase *base,
			  BraseroIOFlags options,
			  gpointer callback_data);
void
brasero_io_get_file_count (BraseroIO *self,
			   GSList *uris,
			   const BraseroIOJobBase *base,
			   BraseroIOFlags options,
			   gpointer callback_data);
void
brasero_io_parse_playlist (BraseroIO *self,
			   const gchar *uri,
			   const BraseroIOJobBase *base,
			   BraseroIOFlags options,
			   gpointer callback_data);
void
brasero_io_xfer (BraseroIO *self,
		 const gchar *uri,
		 const gchar *dest_path,
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
