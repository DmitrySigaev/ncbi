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
 * Author:  Victor Sapojnikov
 *
 * This simple program illustrates how to read text field data.
 *
 */

#include <ncbi_pch.hpp>
#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/gateway/interfaces.hpp>
#include <dbapi/driver/samples/dbapi_driver_samples.hpp>


USING_NCBI_SCOPE;


int main()
{
    try {
        CLogger log(&cerr);
        CSSSConnection sssConnection(&log);
        string sHost = "stmartin";
        int iPort = 8765;

        if( sssConnection.connect(sHost.c_str(), iPort) != CSSSConnection::eOk ) {
            cerr<< "FATAL(" << sssConnection.getRetCodeDesc()
                << "):Failed to connect to SSS server on host:\""
                << sHost << "\" port:" << iPort << endl
                << "\tReason:" << sssConnection.getErrMsg() << endl;
            return 1;
        }

        CGWContext my_context(sssConnection);

        SampleDBAPI_Blob(my_context, "MSSQL3");
    } catch (CDB_Exception& e) {
        CDB_UserHandler_Stream myExHandler(&cerr);

        myExHandler.HandleIt(&e);
        puts("\nException\n");
		getchar();
        return 1;
    }
    puts("\nOK\n");
	getchar();
    return 0;
}



/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2004/05/17 21:14:41  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.2  2002/03/14 22:53:22  sapojnik
 * Inheriting from I_ interfaces instead of CDB_ classes from driver/public.hpp
 *
 * Revision 1.1  2002/03/14 20:00:42  sapojnik
 * A driver that communicates with a dbapi driver on another machine via CompactProtocol(aka ssssrv)
 *
 * Revision 1.1  2002/02/13 20:59:08  sapojnik
 * rdblib: remote dblib driver = MS SQL + sss comprot
 *
 * Revision 1.1  2002/01/14 20:28:13  sapojnik
 * new SampleDBAPI_Blob
 *
 *
 * ===========================================================================
 */
