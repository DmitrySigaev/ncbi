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

#include <tds_config.h>
#include "tdsutil.h"
#include "tds.h"
#include "sybfront.h"
#include "sybdb.h"
#include "dblib.h"
#ifdef NCBI_FTDS
#ifndef WIN32
#include <unistd.h>
#endif
#endif


static char  software_version[]   = "$Id$";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};


RETCODE dbrpcinit(DBPROCESS *dbproc,char *rpcname,DBSMALLINT options)
{
	return SUCCEED;
}
RETCODE dbrpcparam(DBPROCESS *dbproc, char *paramname, BYTE status, int type,
	DBINT maxlen, DBINT datalen, BYTE *value)
{
	return SUCCEED;
}
RETCODE dbrpcsend(DBPROCESS *dbproc)
{
	return SUCCEED;
}
