/***************************************************************************
 *            burn-iso9660.h
 *
 *  sam oct 7 17:32:17 2006
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <errno.h>
#include <string.h>
#include <stdio.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#include "burn-iso9660.h"
#include "burn-iso-field.h"
#include "burn-susp.h"
#include "burn-volume.h"
#include "burn-basics.h"
#include "burn-debug.h"

struct _BraseroIsoCtx {
	gint num_blocks;

	gchar buffer [ISO9660_BLOCK_SIZE];
	gint offset;
	BraseroVolSrc *vol;

	gchar *spare_record;

	guint64 data_blocks;
	GError *error;

	guchar susp_skip;

	guint is_root:1;
	guint has_susp:1;
};
typedef struct _BraseroIsoCtx BraseroIsoCtx;

typedef enum {
BRASERO_ISO_FILE_EXISTENCE		= 1,
BRASERO_ISO_FILE_DIRECTORY		= 1 << 1,
BRASERO_ISO_FILE_ASSOCIATED		= 1 << 2,
BRASERO_ISO_FILE_RECORD			= 1 << 3,
BRASERO_ISO_FILE_PROTECTION		= 1 << 4,
	/* Reserved */
BRASERO_ISO_FILE_MULTI_EXTENT_FINAL	= 1 << 7
} BraseroIsoFileFlag;

struct _BraseroIsoDirRec {
	guchar record_size;
	guchar x_attr_size;
	guchar address			[8];
	guchar file_size		[8];
	guchar date_time		[7];
	guchar flags;
	guchar file_unit;
	guchar gap_size;
	guchar volseq_num		[4];
	guchar id_size;
	gchar id			[0];
};
typedef struct _BraseroIsoDirRec BraseroIsoDirRec;

struct _BraseroIsoPrimary {
	guchar type;
	gchar id			[5];
	guchar version;

	guchar unused_0;

	gchar system_id			[32];
	gchar vol_id			[32];

	guchar unused_1			[8];

	guchar vol_size			[8];

	guchar escapes			[32];
	guchar volset_size		[4];
	guchar sequence_num		[4];
	guchar block_size		[4];
	guchar path_table_size		[8];
	guchar L_table_loc		[4];
	guchar opt_L_table_loc		[4];
	guchar M_table_loc		[4];
	guchar opt_M_table_loc		[4];

	/* the following has a fixed size of 34 bytes */
	BraseroIsoDirRec root_rec	[0];

	/* to be continued if needed */
};
typedef struct _BraseroIsoPrimary BraseroIsoPrimary;

typedef enum {
	BRASERO_ISO_OK,
	BRASERO_ISO_END,
	BRASERO_ISO_ERROR
} BraseroIsoResult;

#define ISO9660_BYTES_TO_BLOCKS(size)			BRASERO_SIZE_TO_SECTORS ((size), ISO9660_BLOCK_SIZE)

static BraseroVolFile *
brasero_iso9660_read_directory_records (BraseroIsoCtx *ctx, gint address);

gboolean
brasero_iso9660_is_primary_descriptor (const char *buffer,
				       GError **error)
{
	BraseroVolDesc *vol;

	/* must be CD001 */
	vol = (BraseroVolDesc *) buffer;
	if (memcmp (vol->id, "CD001", 5)) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("it does not appear to be a primary volume descriptor"));
		return FALSE;
	}

	/* must be "1" for primary volume */
	if (vol->type != 1) {
		g_set_error (error,
			     BRASERO_BURN_ERROR,
			     BRASERO_BURN_ERROR_GENERAL,
			     _("it does not appear to be a primary volume descriptor"));
		return FALSE;
	}

	return TRUE;
}

gboolean
brasero_iso9660_get_size (const gchar *block,
			  gint64 *nb_blocks,
			  GError **error)
{
	BraseroIsoPrimary *vol;

	/* read the size of the volume */
	vol = (BraseroIsoPrimary *) block;
	*nb_blocks = (gint64) brasero_iso9660_get_733_val (vol->vol_size);

	return TRUE;
}

gboolean
brasero_iso9660_get_label (const gchar *block,
			   gchar **label,
			   GError **error)
{
	BraseroIsoPrimary *vol;

	/* read the identifier */
	vol = (BraseroIsoPrimary *) block;
	*label = g_strndup (vol->vol_id, sizeof (vol->vol_id));

	return TRUE;	
}

static BraseroIsoResult
brasero_iso9660_seek (BraseroIsoCtx *ctx, gint address)
{
	ctx->offset = 0;
	ctx->num_blocks = 1;

	/* The size of all the records is given by size member and its location
	 * by its address member. In a set of directory records the first two 
	 * records are: '.' (id == 0) and '..' (id == 1). So since we've got
	 * the address of the set load the block. */
	if (!BRASERO_VOL_SRC_SEEK (ctx->vol, address, SEEK_SET, &(ctx->error)))
		return BRASERO_ISO_ERROR;

	if (!BRASERO_VOL_SRC_READ (ctx->vol, ctx->buffer, 1, &(ctx->error)))
		return BRASERO_ISO_ERROR;


	return BRASERO_ISO_OK;
}

static BraseroIsoResult
brasero_iso9660_next_block (BraseroIsoCtx *ctx)
{
	ctx->offset = 0;
	ctx->num_blocks ++;

	if (!BRASERO_VOL_SRC_READ (ctx->vol, ctx->buffer, 1, &(ctx->error)))
		return BRASERO_ISO_ERROR;

	return BRASERO_ISO_OK;
}

static gchar *
brasero_iso9660_get_susp (BraseroIsoCtx *ctx,
			  BraseroIsoDirRec *record,
			  gint *susp_len)
{
	gchar *susp_block;
	gint start;
	gint len;

	start = sizeof (BraseroIsoDirRec) + record->id_size;
	/* padding byte when id_size is an even number */
	if (start & 1)
		start ++;

	if (ctx->susp_skip)
		start += ctx->susp_skip;

	len = record->record_size - start;
	if (len < 0)
		return NULL;

	if (susp_len)
		*susp_len = len;

	if (len <= 0)
		return NULL;

	susp_block = ((gchar *) record) + start;

	BRASERO_BURN_LOG ("Got susp block");
	return susp_block;
}

static BraseroIsoResult
brasero_iso9660_next_record (BraseroIsoCtx *ctx, BraseroIsoDirRec **retval)
{
	BraseroIsoDirRec *record;

	if (ctx->offset > sizeof (ctx->buffer)) {
		BRASERO_BURN_LOG ("Invalid record size");
		goto error;
	}

	if (ctx->offset == sizeof (ctx->buffer)) {
		BRASERO_BURN_LOG ("No next record");
		return BRASERO_ISO_END;
	}

	/* record_size already checked last time function was called */
	record = (BraseroIsoDirRec *) (ctx->buffer + ctx->offset);
	if (!record->record_size) {
		BRASERO_BURN_LOG ("Last record");
		return BRASERO_ISO_END;
	}

	if (record->record_size > (sizeof (ctx->buffer) - ctx->offset)) {
		gint part_one, part_two;

		/* This is for cross sector boundary records */
		BRASERO_BURN_LOG ("Cross sector boundary record");

		/* some implementations write across block boundary which is
		 * "forbidden" by ECMA-119. But linux kernel accepts it, so ...
		 */
/*		ctx->error = g_error_new (BRASERO_BURN_ERROR,
					  BRASERO_BURN_ERROR_GENERAL,
					  _("directory record written across block boundary"));
		goto error;
*/
		if (ctx->spare_record)
			g_free (ctx->spare_record);

		ctx->spare_record = g_new0 (gchar, record->record_size);

		part_one = sizeof (ctx->buffer) - ctx->offset;
		part_two = record->record_size - part_one;
		
		memcpy (ctx->spare_record,
			ctx->buffer + ctx->offset,
			part_one);
		
		if (brasero_iso9660_next_block (ctx) == BRASERO_ISO_ERROR)
			goto error;

		memcpy (ctx->spare_record + part_one,
			ctx->buffer,
			part_two);
		ctx->offset = part_two;

		record = (BraseroIsoDirRec *) ctx->spare_record;
	}
	else
		ctx->offset += record->record_size;

	*retval = record;
	return BRASERO_ISO_OK;

error:
	if (!ctx->error)
		ctx->error = g_error_new (BRASERO_BURN_ERROR,
					  BRASERO_BURN_ERROR_GENERAL,
					  _("invalid directory record"));
	return BRASERO_ISO_ERROR;
}

static gboolean
brasero_iso9660_read_record_iso_name (BraseroIsoCtx *ctx,
				      BraseroIsoDirRec *record,
				      gchar *iso_name)
{
	if (record->id_size > record->record_size - sizeof (BraseroIsoDirRec)) {
		ctx->error = g_error_new (BRASERO_BURN_ERROR,
					  BRASERO_BURN_ERROR_GENERAL,
					  _("file name is too long"));
		return FALSE;
	}

	memcpy (iso_name, record->id, record->id_size);
	iso_name [record->id_size] = '\0';

	return TRUE;
}

static gboolean
brasero_iso9660_read_record_rr_name (BraseroIsoCtx *ctx,
				     BraseroIsoDirRec *record,
				     gchar *rr_name)
{
	gchar *susp;
	gint susp_len;
	BraseroSuspCtx susp_ctx;

	if (!ctx->has_susp)
		return FALSE;

	BRASERO_BURN_LOG ("Directory with susp area");

	susp = brasero_iso9660_get_susp (ctx, record, &susp_len);
	if (!brasero_susp_read (&susp_ctx, susp, susp_len)) {
		BRASERO_BURN_LOG ("Could not read susp area");
		return FALSE;
	}

	if (susp_ctx.rr_name) {
		BRASERO_BURN_LOG ("Got a susp (RR) %s", susp_ctx.rr_name);
		strcpy (rr_name, susp_ctx.rr_name);
	}

	brasero_susp_ctx_clean (&susp_ctx);
	return TRUE;
}

static BraseroVolFile *
brasero_iso9660_read_file_record (BraseroIsoCtx *ctx,
				  BraseroIsoDirRec *record)
{
	gchar *susp;
	gint susp_len;
	BraseroVolFile *file;
	BraseroSuspCtx susp_ctx;
	BraseroVolFileExtent *extent;

	if (record->id_size > record->record_size - sizeof (BraseroIsoDirRec)) {
		ctx->error = g_error_new (BRASERO_BURN_ERROR,
					  BRASERO_BURN_ERROR_GENERAL,
					  _("file name is too long"));
		return NULL;
	}

	file = g_new0 (BraseroVolFile, 1);
	file->isdir = 0;
	file->name = g_new0 (gchar, record->id_size + 1);
	memcpy (file->name, record->id, record->id_size);

	file->specific.file.size_bytes = brasero_iso9660_get_733_val (record->file_size);

	/* NOTE a file can be in multiple places */
	extent = g_new (BraseroVolFileExtent, 1);
	extent->block = brasero_iso9660_get_733_val (record->address);
	extent->size = brasero_iso9660_get_733_val (record->file_size);
	file->specific.file.extents = g_slist_prepend (file->specific.file.extents, extent);

	/* see if we've got a susp area */
	if (!ctx->has_susp) {
		BRASERO_BURN_LOG ("New file %s", file->name);
		return file;
	}

	BRASERO_BURN_LOG ("New file %s with a suspend area", file->name);

	susp = brasero_iso9660_get_susp (ctx, record, &susp_len);
	if (!brasero_susp_read (&susp_ctx, susp, susp_len)) {
		BRASERO_BURN_LOG ("Could not read susp area");
		brasero_volume_file_free (file);
		return NULL;
	}

	if (susp_ctx.rr_name) {
		BRASERO_BURN_LOG ("Got a susp (RR) %s", susp_ctx.rr_name);

		file->rr_name = susp_ctx.rr_name;
		susp_ctx.rr_name = NULL;
	}

	brasero_susp_ctx_clean (&susp_ctx);
	return file;
}

static BraseroVolFile *
brasero_iso9660_read_directory_record (BraseroIsoCtx *ctx,
				       BraseroIsoDirRec *record)
{
	gchar *susp;
	gint address;
	gint susp_len;
	BraseroSuspCtx susp_ctx;
	BraseroVolFile *directory;

	if (record->id_size > record->record_size - sizeof (BraseroIsoDirRec)) {
		ctx->error = g_error_new (BRASERO_BURN_ERROR,
					  BRASERO_BURN_ERROR_GENERAL,
					  _("file name is too long"));
		return NULL;
	}

	address = brasero_iso9660_get_733_val (record->address);
	directory = brasero_iso9660_read_directory_records (ctx, address);
	if (!directory)
		return NULL;

	directory->name = g_new0 (gchar, record->id_size + 1);
	memcpy (directory->name, record->id, record->id_size);

	if (!ctx->has_susp) {
		BRASERO_BURN_LOG ("New directory %s", directory->name);
		return directory;
	}

	BRASERO_BURN_LOG ("New directory %s with susp area", directory->name);

	/* see if we've got a susp area */
	susp = brasero_iso9660_get_susp (ctx, record, &susp_len);
	if (!brasero_susp_read (&susp_ctx, susp, susp_len)) {
		BRASERO_BURN_LOG ("Could not read susp area");

		brasero_volume_file_free (directory);
		return NULL;
	}

	if (susp_ctx.rr_name) {
		BRASERO_BURN_LOG ("Got a susp (RR) %s", susp_ctx.rr_name);

		directory->rr_name = susp_ctx.rr_name;
		susp_ctx.rr_name = NULL;
	}

	brasero_susp_ctx_clean (&susp_ctx);

	return directory;
}

static BraseroVolFile *
brasero_iso9660_read_directory_records (BraseroIsoCtx *ctx, gint address)
{
	GSList *iter;
	gint max_block;
	gint max_record_size;
	BraseroVolFile *entry;
	BraseroIsoResult result;
	BraseroIsoDirRec *record;
	GSList *directories = NULL;
	BraseroVolFile *parent = NULL;

	BRASERO_BURN_LOG ("Reading directory record");

	result = brasero_iso9660_seek (ctx, address);
	if (result != BRASERO_ISO_OK)
		return NULL;

	/* "." */
	result = brasero_iso9660_next_record (ctx, &record);
	if (result != BRASERO_ISO_OK)
		return NULL;

	/* look for "SP" SUSP if it's root directory */
	if (ctx->is_root) {
		BraseroSuspCtx susp_ctx;
		gint susp_len;
		gchar *susp;

		susp = brasero_iso9660_get_susp (ctx, record, &susp_len);
		brasero_susp_read (&susp_ctx, susp, susp_len);

		ctx->has_susp = susp_ctx.has_SP;
		ctx->susp_skip = susp_ctx.skip;
		ctx->is_root = FALSE;

		brasero_susp_ctx_clean (&susp_ctx);
	}

	max_record_size = brasero_iso9660_get_733_val (record->file_size);
	max_block = ISO9660_BYTES_TO_BLOCKS (max_record_size);
	BRASERO_BURN_LOG ("Maximum directory record length %i block (= %i bytes)", max_block, max_record_size);

	/* skip ".." */
	result = brasero_iso9660_next_record (ctx, &record);
	if (result != BRASERO_ISO_OK)
		return NULL;

	BRASERO_BURN_LOG ("Skipped '.' and '..'");
	parent = g_new0 (BraseroVolFile, 1);
	parent->isdir = 1;

	while (1) {
		BraseroIsoResult result;

		result = brasero_iso9660_next_record (ctx, &record);
		if (result == BRASERO_ISO_END) {
			if (ctx->num_blocks >= max_block)
				break;

			result = brasero_iso9660_next_block (ctx);
			if (result != BRASERO_ISO_OK)
				goto error;

			continue;
		}
		else if (result == BRASERO_ISO_ERROR)
			goto error;

		if (!record)
			break;

		/* if it's a directory, keep the record for later (we don't 
		 * want to change the reading offset for the moment) */
		if (record->flags & BRASERO_ISO_FILE_DIRECTORY) {
			gpointer copy;

			copy = g_new0 (gchar, record->record_size);
			memcpy (copy, record, record->record_size);
			directories = g_slist_prepend (directories, copy);
		}
		else {
			entry = brasero_iso9660_read_file_record (ctx, record);
			if (!entry)
				goto error;

			entry->parent = parent;

			/* check that we don't have another file record for the
			 * same file (usually files > 4G). It always follows
			 * its sibling */
			if (parent->specific.dir.children) {
				BraseroVolFile *last;

				last = parent->specific.dir.children->data;
				if (!last->isdir && !strcmp (BRASERO_VOLUME_FILE_NAME (last), BRASERO_VOLUME_FILE_NAME (entry))) {
					/* add size and addresses */
					ctx->data_blocks += ISO9660_BYTES_TO_BLOCKS (entry->specific.file.size_bytes);
					last = brasero_volume_file_merge (last, entry);
					BRASERO_BURN_LOG ("Multi extent file");
					continue;
				}
			}
			parent->specific.dir.children = g_list_prepend (parent->specific.dir.children, entry);
			ctx->data_blocks += ISO9660_BYTES_TO_BLOCKS (entry->specific.file.size_bytes);
		}
	}

	/* takes care of the directories: we accumulate them not to change the
	 * offset of file descriptor FILE */
	for (iter = directories; iter; iter = iter->next) {
		record = iter->data;

		entry = brasero_iso9660_read_directory_record (ctx, record);
		if (!entry)
			goto error;

		entry->parent = parent;
		parent->specific.dir.children = g_list_prepend (parent->specific.dir.children, entry);
	}
	g_slist_foreach (directories, (GFunc) g_free, NULL);
	g_slist_free (directories);

	return parent;

error:

	g_slist_foreach (directories, (GFunc) g_free, NULL);
	g_slist_free (directories);

	brasero_volume_file_free (parent);
	return NULL;
}

static void
brasero_iso9660_ctx_init (BraseroIsoCtx *ctx, BraseroVolSrc *vol)
{
	memset (ctx, 0, sizeof (BraseroIsoCtx));

	ctx->is_root = TRUE;
	ctx->vol = vol;
	ctx->offset = 0;
}

BraseroVolFile *
brasero_iso9660_get_contents (BraseroVolSrc *vol,
			      const gchar *block,
			      gint64 *data_blocks,
			      GError **error)
{
	BraseroIsoPrimary *primary;
	BraseroVolFile *volfile;
	BraseroIsoDirRec *root;
	BraseroIsoCtx ctx;
	gint address;

	primary = (BraseroIsoPrimary *) block;
	root = primary->root_rec;

	brasero_iso9660_ctx_init (&ctx, vol);

	address = brasero_iso9660_get_733_val (root->address);

	BRASERO_BURN_LOG ("Reading root directory record at %i", address);
	volfile = brasero_iso9660_read_directory_records (&ctx, address);

	if (ctx.spare_record)
		g_free (ctx.spare_record);

	if (data_blocks)
		*data_blocks = ctx.data_blocks;

	if (error && ctx.error)
		g_propagate_error (error, ctx.error);

	return volfile;
}

static BraseroVolFile *
brasero_iso9660_lookup_directory_record (BraseroIsoCtx *ctx,
					 const gchar *path,
					 gint address)
{
	guint len;
	gchar *end;
	gint max_block;
	gint max_record_size;
	BraseroIsoResult result;
	BraseroIsoDirRec *record;
	BraseroVolFile *file = NULL;

	BRASERO_BURN_LOG ("Reading directory record");

	result = brasero_iso9660_seek (ctx, address);
	if (result != BRASERO_ISO_OK)
		return NULL;

	/* "." */
	result = brasero_iso9660_next_record (ctx, &record);
	if (result != BRASERO_ISO_OK)
		return NULL;

	/* look for "SP" SUSP if it's root directory */
	if (ctx->is_root) {
		BraseroSuspCtx susp_ctx;
		gint susp_len;
		gchar *susp;

		susp = brasero_iso9660_get_susp (ctx, record, &susp_len);
		brasero_susp_read (&susp_ctx, susp, susp_len);

		ctx->has_susp = susp_ctx.has_SP;
		ctx->susp_skip = susp_ctx.skip;
		ctx->is_root = FALSE;

		brasero_susp_ctx_clean (&susp_ctx);
	}

	max_record_size = brasero_iso9660_get_733_val (record->file_size);
	max_block = ISO9660_BYTES_TO_BLOCKS (max_record_size);
	BRASERO_BURN_LOG ("Maximum directory record length %i block (= %i bytes)", max_block, max_record_size);

	/* skip ".." */
	result = brasero_iso9660_next_record (ctx, &record);
	if (result != BRASERO_ISO_OK)
		return NULL;

	BRASERO_BURN_LOG ("Skipped '.' and '..'");

	end = strchr (path, '/');
	if (!end)
		/* reached the final file */
		len = 0;
	else
		len = end - path;

	while (1) {
		BraseroIsoResult result;
		gchar record_name [256];

		result = brasero_iso9660_next_record (ctx, &record);
		if (result == BRASERO_ISO_END) {
			if (ctx->num_blocks >= max_block) {
				BRASERO_BURN_LOG ("Reached the end of directory record");
				break;
			}

			result = brasero_iso9660_next_block (ctx);
			if (result != BRASERO_ISO_OK) {
				BRASERO_BURN_LOG ("Failed to load next block");
				return NULL;
			}

			continue;
		}
		else if (result == BRASERO_ISO_ERROR) {
			BRASERO_BURN_LOG ("Error retrieving next record");
			return NULL;
		}

		if (!record) {
			BRASERO_BURN_LOG ("No record !!!");
			break;
		}

		if (!brasero_iso9660_read_record_rr_name (ctx, record, record_name)
		&&  !brasero_iso9660_read_record_iso_name (ctx, record, record_name))
			continue;

		/* if it's a directory, keep the record for later (we don't 
		 * want to change the reading offset for the moment) */
		if (!len && !(record->flags & BRASERO_ISO_FILE_DIRECTORY)) {
			BraseroVolFile *entry;

			/* see if we are looking for a file */
			if (len)
				continue;

			/* see if that the record we're looking for */
			if (strcmp (record_name, path))
				continue;

			/* carry on with the search in case there are other extents */
			entry = brasero_iso9660_read_file_record (ctx, record);
			if (!entry)
				return NULL;

			if (file) {
				/* add size and addresses */
				file = brasero_volume_file_merge (file, entry);
				BRASERO_BURN_LOG ("Multi extent file");
			}
			else
				file = entry;

			continue;
		}

		if (len && !strncmp (record_name, path, len)) {
			gint address;

			/* move path forward */
			path += len;
			path ++;

			address = brasero_iso9660_get_733_val (record->address);
			file = brasero_iso9660_lookup_directory_record (ctx,
									path,
									address);
			break;
		}
	}

	return file;
}

BraseroVolFile *
brasero_iso9660_get_file (BraseroVolSrc *vol,
			  const gchar *path,
			  const gchar *block,
			  GError **error)
{
	BraseroIsoPrimary *primary;
	BraseroIsoDirRec *root;
	BraseroVolFile *entry;
	BraseroIsoCtx ctx;
	gint address;

	primary = (BraseroIsoPrimary *) block;
	root = primary->root_rec;
	address = brasero_iso9660_get_733_val (root->address);

	brasero_iso9660_ctx_init (&ctx, vol);

	/* now that we have root block address, skip first "/" and go. */
	path ++;
	entry = brasero_iso9660_lookup_directory_record (&ctx, path, address);

	/* clean context */
	if (ctx.spare_record)
		g_free (ctx.spare_record);

	if (error && ctx.error)
		g_propagate_error (error, ctx.error);

	return entry;
}
