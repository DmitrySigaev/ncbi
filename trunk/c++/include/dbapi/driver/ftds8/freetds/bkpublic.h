/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-1999  Brian Bruns
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _bkpublic_h_
#define _bkpublic_h_

static char  rcsid_bkpublic_h [ ] =
         "$Id$";
static void *no_unused_bkpublic_h_warn[]={rcsid_bkpublic_h, no_unused_bkpublic_h_warn};

/* seperate this stuff out later */
#include <cspublic.h>

#ifdef __cplusplus
extern "C" {
#endif


/* buld properties start with 1 i guess */
#define BLK_IDENTITY 1

#ifdef __cplusplus
}
#endif


#endif

