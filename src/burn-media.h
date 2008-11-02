/***************************************************************************
 *            burn-media.h
 *
 *  Wed Oct  8 16:42:17 2008
 *  Copyright  2008  ykw
 *  <ykw@localhost.localdomain>
 ****************************************************************************/

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */
 
#ifndef _BURN_MEDIA_H
#define _BURN_MEDIA_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
	BRASERO_MEDIUM_UNSUPPORTED		= -2,
	BRASERO_MEDIUM_BUSY			= -1,
	BRASERO_MEDIUM_NONE			= 0,

	/* types */
	BRASERO_MEDIUM_FILE			= 1,

	BRASERO_MEDIUM_CD			= 1 << 1,

	BRASERO_MEDIUM_DVD			= 1 << 2,

	BRASERO_MEDIUM_BD			= 1 << 3,

	/* subtype for DVD and BD types */
	BRASERO_MEDIUM_DUAL_L			= 1 << 4,

	/* DVD and DVD DL subtypes */
	BRASERO_MEDIUM_RAM			= 1 << 5,
	BRASERO_MEDIUM_PLUS			= 1 << 6,
	BRASERO_MEDIUM_SEQUENTIAL		= 1 << 7,
	BRASERO_MEDIUM_RESTRICTED		= 1 << 8,	/* DVD-RW only */

	/* DVD-R dual layer only subtype */
	BRASERO_MEDIUM_JUMP			= 1 << 9,

	/* BD subtypes */
	BRASERO_MEDIUM_RANDOM			= 1 << 10,
	BRASERO_MEDIUM_SRM			= 1 << 11,
	BRASERO_MEDIUM_POW			= 1 << 12,	/* This is Pseudo OverWrite */

	/* discs attributes */
	BRASERO_MEDIUM_REWRITABLE		= 1 << 14,
	BRASERO_MEDIUM_WRITABLE			= 1 << 15,
	BRASERO_MEDIUM_ROM			= 1 << 16,

	/* status of the disc */
	BRASERO_MEDIUM_BLANK			= 1 << 17,
	BRASERO_MEDIUM_CLOSED			= 1 << 18,
	BRASERO_MEDIUM_APPENDABLE		= 1 << 19,

	/* Only used for DVD+RW, DVD-RW restricted */
	BRASERO_MEDIUM_UNFORMATTED		= 1 << 20,

	BRASERO_MEDIUM_PROTECTED		= 1 << 21,
	BRASERO_MEDIUM_HAS_DATA			= 1 << 22,
	BRASERO_MEDIUM_HAS_AUDIO		= 1 << 23,
} BraseroMedia;

#define BRASERO_MEDIUM_CDROM		(BRASERO_MEDIUM_CD|		\
					 BRASERO_MEDIUM_ROM)
#define BRASERO_MEDIUM_CDR		(BRASERO_MEDIUM_CD|		\
					 BRASERO_MEDIUM_WRITABLE)
#define BRASERO_MEDIUM_CDRW		(BRASERO_MEDIUM_CD|		\
					 BRASERO_MEDIUM_REWRITABLE)
#define BRASERO_MEDIUM_DVD_ROM		(BRASERO_MEDIUM_DVD|		\
					 BRASERO_MEDIUM_ROM)
#define BRASERO_MEDIUM_DVDR		(BRASERO_MEDIUM_DVD|		\
					 BRASERO_MEDIUM_SEQUENTIAL|	\
					 BRASERO_MEDIUM_WRITABLE)
#define BRASERO_MEDIUM_DVDRW		(BRASERO_MEDIUM_DVD|		\
					 BRASERO_MEDIUM_SEQUENTIAL|	\
					 BRASERO_MEDIUM_REWRITABLE)
#define BRASERO_MEDIUM_DVDRW_RESTRICTED	(BRASERO_MEDIUM_DVD|		\
					 BRASERO_MEDIUM_REWRITABLE|	\
					 BRASERO_MEDIUM_RESTRICTED)
#define BRASERO_MEDIUM_DVDR_DL		(BRASERO_MEDIUM_DVD|		\
					 BRASERO_MEDIUM_DUAL_L|		\
					 BRASERO_MEDIUM_WRITABLE|	\
					 BRASERO_MEDIUM_SEQUENTIAL)
#define BRASERO_MEDIUM_DVDR_JUMP_DL	(BRASERO_MEDIUM_DVD|		\
					 BRASERO_MEDIUM_DUAL_L|		\
					 BRASERO_MEDIUM_WRITABLE|	\
					 BRASERO_MEDIUM_JUMP)
#define BRASERO_MEDIUM_DVDR_PLUS	(BRASERO_MEDIUM_DVD|		\
					 BRASERO_MEDIUM_WRITABLE|	\
					 BRASERO_MEDIUM_PLUS)
#define BRASERO_MEDIUM_DVDRW_PLUS	(BRASERO_MEDIUM_DVD|		\
					 BRASERO_MEDIUM_REWRITABLE|	\
					 BRASERO_MEDIUM_PLUS)
#define BRASERO_MEDIUM_DVDR_PLUS_DL	(BRASERO_MEDIUM_DVD|		\
					 BRASERO_MEDIUM_DUAL_L|		\
					 BRASERO_MEDIUM_WRITABLE|	\
					 BRASERO_MEDIUM_PLUS)
#define BRASERO_MEDIUM_DVDRW_PLUS_DL	(BRASERO_MEDIUM_DVD|		\
					 BRASERO_MEDIUM_DUAL_L|		\
					 BRASERO_MEDIUM_REWRITABLE|	\
					 BRASERO_MEDIUM_PLUS)

#define BRASERO_MEDIUM_DVD_RAM		(BRASERO_MEDIUM_DVD|		\
					 BRASERO_MEDIUM_RAM)

/* BD types all exist in BD-R(E) DL as well */
#define BRASERO_MEDIUM_BD_ROM		(BRASERO_MEDIUM_BD|		\
					 BRASERO_MEDIUM_ROM)
#define BRASERO_MEDIUM_BDR_SRM		(BRASERO_MEDIUM_BD|		\
					 BRASERO_MEDIUM_SRM|		\
					 BRASERO_MEDIUM_WRITABLE)
#define BRASERO_MEDIUM_BDR_SRM_POW	(BRASERO_MEDIUM_BD|		\
					 BRASERO_MEDIUM_POW|		\
					 BRASERO_MEDIUM_SRM|		\
					 BRASERO_MEDIUM_WRITABLE)

/* This seems to be a really rare mode for BD-R */
#define BRASERO_MEDIUM_BDR_RANDOM	(BRASERO_MEDIUM_BD|		\
					 BRASERO_MEDIUM_WRITABLE|	\
					 BRASERO_MEDIUM_RANDOM)
/* This is always RANDOM write */
#define BRASERO_MEDIUM_BDRE		(BRASERO_MEDIUM_BD|			\
					 BRASERO_MEDIUM_REWRITABLE)



#define BRASERO_MEDIUM_VALID(media)	((media) != BRASERO_MEDIUM_NONE	&&	\
					 (media) != BRASERO_MEDIUM_BUSY	&&	\
					 (media) != BRASERO_MEDIUM_UNSUPPORTED)


#define BRASERO_MEDIUM_TYPE(media)	((media) & 0x003F)
#define BRASERO_MEDIUM_ATTR(media)	((media) & 0x1C000)
#define BRASERO_MEDIUM_STATUS(media)	((media) & 0xE0000)
#define BRASERO_MEDIUM_SUBTYPE(media)	((media) & 0x1FC0)
#define BRASERO_MEDIUM_INFO(media)	((media) & 0xFE0000)

#define BRASERO_MEDIUM_IS(media, type)	(((media)&(type))==(type))

/* These behave like random writable:
 * they can be appendable or closed, rewritable or writable */
#define BRASERO_MEDIUM_RANDOM_WRITABLE(media)					\
	(BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDRW_PLUS) ||		\
	 BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVDRW_RESTRICTED) ||		\
	 BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_DVD_RAM) || 			\
	 BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_BDR_RANDOM) ||		\
	 BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_BDR_SRM_POW) ||		\
	 BRASERO_MEDIUM_IS (media, BRASERO_MEDIUM_BDRE))

GSList *
brasero_media_get_all_list (BraseroMedia type);

BraseroMedia
brasero_media_capabilities (BraseroMedia media);

G_END_DECLS

#endif /* _BURN_MEDIA_H */

 
