/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Denis Vakatov
 *
 * File Description:
 *    Private aux. code for the "ncbi_*.[ch]"
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.2  2002/03/22 19:52:17  lavr
 * Do not include <stdio.h>: included from ncbi_util.h or ncbi_priv.h
 *
 * Revision 6.1  2000/03/24 22:53:35  vakatov
 * Initial revision
 *
 * ===========================================================================
 */

#include "ncbi_priv.h"
#include <stdarg.h>
#include <string.h>


MT_LOCK g_CORE_MT_Lock;
LOG     g_CORE_Log;
REG     g_CORE_Registry;


extern const char* g_CORE_Sprintf(const char* fmt, ...)
{
  va_list args;
  static char str[4096];

  va_start(args, fmt);
  *str = '\0';
  vsprintf(str, fmt, args);
  assert(strlen(str) < sizeof(str));
  va_end(args);
  return str;
}


extern char* g_CORE_RegistryGET
(const char* section,
 const char* name,
 char*       value,
 size_t      value_size,
 const char* def_value)
{
    char* ret_value;
    CORE_LOCK_READ;
    ret_value = REG_Get(g_CORE_Registry,
                        section, name, value, value_size, def_value);
    CORE_UNLOCK;
    return ret_value;
}
