/* $Id$
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
 * Author:  Vladimir Soussov
 *
 * This simple program illustrates how to use the RPC command
 *
 */

#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/dblib/interfaces.hpp>
#include <dbapi/driver/samples/dbapi_driver_samples.hpp>


USING_NCBI_SCOPE;


int main()
{
    try {
        CDBLibContext my_context;

#ifdef NCBI_OS_MSWIN
       SampleDBAPI_SpWho(my_context, "NLMSQL");
#else
       SampleDBAPI_SpWho(my_context, "MOZART");
#endif
    } catch (CDB_Exception& e) {
        CDB_UserHandler_Stream myExHandler(&cerr);

        myExHandler.HandleIt(&e);
        return 1;
    }
    return 0;
}



/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2002/01/03 15:46:23  sapojnik
 * ported to MS SQL (about 12 'ifdef NCBI_OS_MSWIN' in 6 files)
 *
 * Revision 1.4  2001/11/06 18:00:00  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.3  2001/10/25 00:18:04  vakatov
 * SampleDBAPI_XXX() to accept yet another arg -- server name
 *
 * Revision 1.2  2001/10/24 16:37:32  lavr
 * Finish log with horizontal rule
 *
 * Revision 1.1  2001/10/23 20:52:14  lavr
 * Initial revision (derived from former sample programs)
 *
 * Revision 1.1  2001/10/22 15:23:04  lavr
 * Initial revision derived from corresponding CTLib version
 *
 * ===========================================================================
 */
