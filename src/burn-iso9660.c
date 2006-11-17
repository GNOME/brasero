/***************************************************************************
 *            burn-iso9660.h
 *
 *  sam oct 7 17:32:17 2006
 *  Copyright  2006  Rouquier Philippe
 *  brasero-app@wanadoo.fr
 ***************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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
#include "burn-volume.h"
#include "burn-basics.h"

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

static gint32
brasero_iso9660_get_733_val (guchar *buffer)
{
	guint32 *ptr;

	ptr = (guint32*) buffer;

	return GUINT32_FROM_LE (*ptr);
}

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
			  gint32 *nb_blocks,
			  GError **error)
{
	BraseroIsoPrimary *vol;

	/* read the size of the volume */
	vol = (BraseroIsoPrimary *) block;
	*nb_blocks = brasero_iso9660_get_733_val (vol->vol_size);

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

static BraseroVolFile *
brasero_iso9660_read_directory_records (FILE *file, gint address, GError **error)
{
	gint offset;
	GSList *iter;
	gint max_record_size;
	BraseroIsoDirRec *record;
	GSList *directories = NULL;
	BraseroVolFile *parent = NULL;
	gchar buffer [ISO9660_BLOCK_SIZE];

	/* The size of all the records is given by size member and its location
	 * by its address member. In a set of directory records the first two 
	 * records are: '.' (id == 0) and '..' (id == 1). So since we've got
	 * the address of the set load the block. */
	if (fseek (file, address * ISO9660_BLOCK_SIZE, SEEK_SET) == -1)
		goto error;

	if (fread (buffer, 1, sizeof (buffer), file) != sizeof (buffer))
		goto error;

	/* setup the parent directory from the first record */
	parent = g_new0 (BraseroVolFile, 1);
	parent->isdir = 1;

	/* skip the second record */
	record = (BraseroIsoDirRec *) buffer;
	max_record_size = brasero_iso9660_get_733_val (record->file_size);

	offset = record->record_size;
	record = (BraseroIsoDirRec *) (buffer + offset);
	offset += record->record_size;
	record = (BraseroIsoDirRec *) (buffer + offset);

	max_record_size -= offset;
	while (record->record_size) {
		BraseroVolFile *volfile;

		/* for the time being just do the file and keep a record
		 * for the directories that'll be done later (we don't 
		 * want to change the reading offset for the moment) */
		if (record->flags & BRASERO_ISO_FILE_DIRECTORY) {
			gpointer copy;

			copy = g_new0 (gchar, record->record_size);
			memcpy (copy, record, record->record_size);
			directories = g_slist_prepend (directories, copy);
		}
		else {
			volfile = g_new0 (BraseroVolFile, 1);

			volfile->parent = parent;
			volfile->name = g_strndup (record->id, record->id_size);
			volfile->specific.file.size_bytes = brasero_iso9660_get_733_val (record->file_size);
			volfile->specific.file.address_block = brasero_iso9660_get_733_val (record->address);
			volfile->isdir = 0;

			parent->specific.children = g_list_prepend (parent->specific.children, volfile);
		}

		offset += record->record_size;
		max_record_size -= record->record_size;
		if (max_record_size <= 0)
			break;

		if (offset >= sizeof (buffer) || buffer [offset] == 0) {
			max_record_size -= (ISO9660_BLOCK_SIZE - offset);
			if (max_record_size < ISO9660_BLOCK_SIZE)
				break;

			/* we reached the end of the block, or, the size of the next
			 * directory record is 0 because we reached a block boundary
			 * and must read another block. We must make sure that reading
			 * another block won't be too much. */
			offset = 0;

			if (fread (buffer, 1, sizeof (buffer), file) != sizeof (buffer))
				goto error;
		}
		record = (BraseroIsoDirRec *) (buffer + offset);
	}

	for (iter = directories; iter; iter = iter->next) {
		BraseroVolFile *volfile;
		gint address;

		record = iter->data;

		address = brasero_iso9660_get_733_val (record->address);
		volfile = brasero_iso9660_read_directory_records (file,
								  address,
								  error);
		volfile->name = g_strndup (record->id, record->id_size);
		volfile->parent = parent;

		parent->specific.children = g_list_prepend (parent->specific.children, volfile);

		g_free (record);
	}
	g_slist_free (directories);

	return parent;

error:

	g_slist_foreach (directories, (GFunc) g_free, NULL);
	g_slist_free (directories);

	g_set_error (error,
		     BRASERO_BURN_ERROR,
		     BRASERO_BURN_ERROR_GENERAL,
		     strerror (errno));

	/* clean parent */
	brasero_volume_file_free (parent);

	return NULL;
}

BraseroVolFile *
brasero_iso9660_get_contents (FILE *file,
			      const gchar *block,
			      GError **error)
{
	BraseroIsoPrimary *primary;

	primary = (BraseroIsoPrimary *) block;
	return brasero_iso9660_read_directory_records (file, brasero_iso9660_get_733_val (primary->root_rec->address), error);
}
