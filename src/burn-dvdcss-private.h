/***************************************************************************
 *            burn-dvdcss-private.h
 *
 *  Thu Nov 16 16:20:39 2006
 *  Copyright  2006  Rouquier Philippe
 *  <Rouquier Philippe@localhost.localdomain>
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


#include <gmodule.h>

#ifndef _BURN_DVDCSS_PRIVATE_H
#define _BURN_DVDCSS_PRIVATE_H

#ifdef __cplusplus
extern "C"
{
#endif

static gboolean css_ready = FALSE;

typedef gpointer dvdcss_handle;

#define DVDCSS_NOFLAGS		0x00

#define DVDCSS_READ_DECRYPT	(1 << 0)

#define DVDCSS_SEEK_MPEG	(1 << 0)
#define DVDCSS_SEEK_KEY		(1 << 1)

#define DVDCSS_BLOCK_SIZE	2048

static dvdcss_handle *
(*dvdcss_open)	(gchar *device) = NULL;

static gint
(*dvdcss_close)	(dvdcss_handle *handle) = NULL;

static gint
(*dvdcss_read)	(dvdcss_handle *handle, gpointer p_buffer, gint i_blocks, gint i_flags) = NULL;

static gint
(*dvdcss_seek)	(dvdcss_handle *handle, gint i_blocks, gint i_flags) = NULL;

static gchar *
(*dvdcss_error)	(dvdcss_handle *handle) = NULL;

#ifdef __cplusplus
}
#endif

#endif /* _BURN_DVDCSS_PRIVATE_H */

 
