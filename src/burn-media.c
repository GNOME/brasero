/***************************************************************************
 *            burn-media.c
 *
 *  Wed Oct  8 16:40:48 2008
 *  Copyright  2008  ykw
 *  <ykw@localhost.localdomain>
 ****************************************************************************/

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

#include <glib.h>

#include "burn-media.h"

static GSList *
brasero_media_add_to_list (GSList *retval,
			   BraseroMedia media)
{
	retval = g_slist_prepend (retval, GINT_TO_POINTER (media));
	return retval;
}

static GSList *
brasero_media_new_status (GSList *retval,
			  BraseroMedia media,
			  BraseroMedia type)
{
	if ((type & BRASERO_MEDIUM_BLANK)
	&& !(media & BRASERO_MEDIUM_ROM)) {
		/* If media is blank there is no other possible property.
		 * BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDRW_RESTRICTED)
		 * condition is checked but in fact it's never valid since
		 * such a medium cannot exist if it hasn't been formatted before
		 * which is in contradiction with the fact is unformatted. */
		if (BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDRW_PLUS)
		||  BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDRW_RESTRICTED)
		||  BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDRW)
		||  BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDRW_PLUS_DL)) {
			/* This is only for above types */
			retval = brasero_media_add_to_list (retval,
							    media|
							    BRASERO_MEDIUM_BLANK);
			if (type & BRASERO_MEDIUM_UNFORMATTED)
				retval = brasero_media_add_to_list (retval,
								    media|
								    BRASERO_MEDIUM_BLANK|
								    BRASERO_MEDIUM_UNFORMATTED);
		}
		else
			retval = brasero_media_add_to_list (retval,
							    media|
							    BRASERO_MEDIUM_BLANK);
	}

	if (type & BRASERO_MEDIUM_CLOSED) {
		if (media & (BRASERO_MEDIUM_DVD|BRASERO_MEDIUM_DVD_DL))
			retval = brasero_media_add_to_list (retval,
							    media|
							    BRASERO_MEDIUM_CLOSED|
							    (type & BRASERO_MEDIUM_HAS_DATA)|
							    (type & BRASERO_MEDIUM_PROTECTED));
		else {
			if (type & BRASERO_MEDIUM_HAS_AUDIO)
				retval = brasero_media_add_to_list (retval,
								    media|
								    BRASERO_MEDIUM_CLOSED|
								    BRASERO_MEDIUM_HAS_AUDIO);
			if (type & BRASERO_MEDIUM_HAS_DATA)
				retval = brasero_media_add_to_list (retval,
								    media|
								    BRASERO_MEDIUM_CLOSED|
								    BRASERO_MEDIUM_HAS_DATA);
			if (BRASERO_MEDIUM_IS (type, BRASERO_MEDIUM_HAS_AUDIO|BRASERO_MEDIUM_HAS_DATA))
				retval = brasero_media_add_to_list (retval,
								    media|
								    BRASERO_MEDIUM_CLOSED|
								    BRASERO_MEDIUM_HAS_DATA|
								    BRASERO_MEDIUM_HAS_AUDIO);
		}
	}

	if ((type & BRASERO_MEDIUM_APPENDABLE)
	&& !(media & BRASERO_MEDIUM_ROM)
	&& !(media & BRASERO_MEDIUM_RESTRICTED)
	&& ! BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVD|BRASERO_MEDIUM_PLUS|BRASERO_MEDIUM_REWRITABLE)
	&& ! BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVD_DL|BRASERO_MEDIUM_PLUS|BRASERO_MEDIUM_REWRITABLE)) {
		if (media & BRASERO_MEDIUM_DVD)
			retval = brasero_media_add_to_list (retval,
							    media|
							    BRASERO_MEDIUM_APPENDABLE|
							    BRASERO_MEDIUM_HAS_DATA);
		else {
			if (type & BRASERO_MEDIUM_HAS_AUDIO)
				retval = brasero_media_add_to_list (retval,
								    media|
								    BRASERO_MEDIUM_APPENDABLE|
								    BRASERO_MEDIUM_HAS_AUDIO);
			if (type & BRASERO_MEDIUM_HAS_DATA)
				retval = brasero_media_add_to_list (retval,
								    media|
								    BRASERO_MEDIUM_APPENDABLE|
								    BRASERO_MEDIUM_HAS_DATA);
			if (BRASERO_MEDIUM_IS (type, BRASERO_MEDIUM_HAS_AUDIO|BRASERO_MEDIUM_HAS_DATA))
				retval = brasero_media_add_to_list (retval,
								    media|
								    BRASERO_MEDIUM_HAS_DATA|
								    BRASERO_MEDIUM_APPENDABLE|
								    BRASERO_MEDIUM_HAS_AUDIO);
		}
	}

	return retval;
}

static GSList *
brasero_media_new_attribute (GSList *retval,
			     BraseroMedia media,
			     BraseroMedia type)
{
	if (type & BRASERO_MEDIUM_REWRITABLE) {
		/* Always true for + media there are both single and dual layer */
		if (media & BRASERO_MEDIUM_PLUS)
			retval = brasero_media_new_status (retval,
							   media|BRASERO_MEDIUM_REWRITABLE,
							   type);
		/* There is no dual layer DVD-RW */
		else if (!(media & BRASERO_MEDIUM_DVD_DL))
			retval = brasero_media_new_status (retval,
							   media|BRASERO_MEDIUM_REWRITABLE,
							   type);
	}

	if ((type & BRASERO_MEDIUM_WRITABLE)
	&& !(media & BRASERO_MEDIUM_RESTRICTED))
		retval = brasero_media_new_status (retval,
						   media|BRASERO_MEDIUM_WRITABLE,
						   type);

	if (type & BRASERO_MEDIUM_ROM)
		retval = brasero_media_new_status (retval,
						   media|BRASERO_MEDIUM_ROM,
						   type);

	return retval;
}

static GSList *
brasero_media_new_subtype (GSList *retval,
			   BraseroMedia media,
			   BraseroMedia type)
{
	if (media & BRASERO_MEDIUM_BD) {
		if (type & BRASERO_MEDIUM_RANDOM)
			retval = brasero_media_new_attribute (retval,
							      media|BRASERO_MEDIUM_RANDOM,
							      type);
		if (type & BRASERO_MEDIUM_SRM)
			retval = brasero_media_new_attribute (retval,
							      media|BRASERO_MEDIUM_SRM,
							      type);
		if (type & BRASERO_MEDIUM_POW)
			retval = brasero_media_new_attribute (retval,
							      media|BRASERO_MEDIUM_POW,
							      type);
	}

	if (media & BRASERO_MEDIUM_DVD) {
		if (type & BRASERO_MEDIUM_SEQUENTIAL)
			retval = brasero_media_new_attribute (retval,
							      media|BRASERO_MEDIUM_SEQUENTIAL,
							      type);

		if (type & BRASERO_MEDIUM_RESTRICTED)
			retval = brasero_media_new_attribute (retval,
							      media|BRASERO_MEDIUM_RESTRICTED,
							      type);

		if (type & BRASERO_MEDIUM_PLUS)
			retval = brasero_media_new_attribute (retval,
							      media|BRASERO_MEDIUM_PLUS,
							      type);
		if (type & BRASERO_MEDIUM_ROM)
			retval = brasero_media_new_status (retval,
							   media|BRASERO_MEDIUM_ROM,
							   type);
	}

	if (media & BRASERO_MEDIUM_DVD_DL) {
		/* There is no such thing as DVD-RW DL */
		if ((type & BRASERO_MEDIUM_SEQUENTIAL) && !(type & BRASERO_MEDIUM_REWRITABLE))
			retval = brasero_media_new_attribute (retval,
							      media|BRASERO_MEDIUM_SEQUENTIAL,
							      type);

		if ((type & BRASERO_MEDIUM_JUMP) && !(type & BRASERO_MEDIUM_REWRITABLE))
			retval = brasero_media_new_attribute (retval,
							      media|BRASERO_MEDIUM_JUMP,
							      type);

		if (type & BRASERO_MEDIUM_PLUS)
			retval = brasero_media_new_attribute (retval,
							      media|BRASERO_MEDIUM_PLUS,
							      type);

		if (type & BRASERO_MEDIUM_ROM)
			retval = brasero_media_new_status (retval,
							   media|BRASERO_MEDIUM_ROM,
							   type);
	}

	return retval;
}

GSList *
brasero_media_get_all_list (BraseroMedia type)
{
	GSList *retval = NULL;

	if (type & BRASERO_MEDIUM_FILE)
		retval = brasero_media_add_to_list (retval, BRASERO_MEDIUM_FILE);					       

	if (type & BRASERO_MEDIUM_CD)
		retval = brasero_media_new_attribute (retval,
						      BRASERO_MEDIUM_CD,
						      type);

	if (type & BRASERO_MEDIUM_DVD)
		retval = brasero_media_new_subtype (retval,
						    BRASERO_MEDIUM_DVD,
						    type);

	if (type & BRASERO_MEDIUM_DVD_DL)
		retval = brasero_media_new_subtype (retval,
						    BRASERO_MEDIUM_DVD_DL,
						    type);

	/* RAM media are always rewritable */
	if (type & BRASERO_MEDIUM_RAM)
		retval = brasero_media_new_status (retval,
						   BRASERO_MEDIUM_RAM|
						   BRASERO_MEDIUM_REWRITABLE,
						   type);

	if (type & BRASERO_MEDIUM_BD)
		retval = brasero_media_new_subtype (retval,
						    BRASERO_MEDIUM_BD,
						    type);

	return retval;
}
