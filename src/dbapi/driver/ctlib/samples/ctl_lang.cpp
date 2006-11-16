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
 * This simple program illustrates how to use the language command
 *
 */

#include <ncbi_pch.hpp>

#include "../../dbapi_driver_sample_base.hpp"
#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/ctlib/interfaces.hpp>
#include <dbapi/driver/dbapi_svc_mapper.hpp>
#include <test/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


class CDemoApp : public CDbapiDriverSampleApp
{
public:
    CDemoApp(const string& server_name,
              int tds_version = GetCtlibTdsVersion());
    virtual ~CDemoApp(void);

    virtual int RunSample(void);
};


CDemoApp::CDemoApp(const string& server_name, int tds_version) :
    CDbapiDriverSampleApp(server_name, tds_version)
{
}


CDemoApp::~CDemoApp(void)
{
    return;
}


int
CDemoApp::RunSample(void)
{
    try {
        DBLB_INSTALL_DEFAULT();

        CTLibContext my_context(true, GetTDSVersion());

        auto_ptr<CDB_Connection> con(my_context.Connect(GetServerName(),
                                                        GetUserName(),
                                                        GetPassword(),
                                                        0));

        auto_ptr<CDB_LangCmd> lcmd
            (con->LangCmd("select name, crdate from sysdatabases"));
        lcmd->Send();

        while (lcmd->HasMoreResults()) {
            auto_ptr<CDB_Result> r(lcmd->Result());
            if (!r.get())
                continue;

            cout
                << r->ItemName(0) << " \t\t\t"
                << r->ItemName(1) << endl
                << "-----------------------------------------------------"
                << endl;

            while (r->Fetch()) {
                CDB_Char dbname(24);
                CDB_DateTime crdate;

                r->GetItem(&dbname);
                r->GetItem(&crdate);

                cout
                    << dbname.Value() << ' '
                    << crdate.Value().AsString("M/D/Y h:m") << endl;
            }
        }
    } catch (CDB_Exception& e) {
        CDB_UserHandler_Stream myExHandler(&cerr);

        myExHandler.HandleIt(&e);
        return 1;
    } catch (const CException&) {
        return 1;
    }

    return 0;
}

int main(int argc, const char* argv[])
{
    return CDemoApp("SCHUMANN").AppMain(argc, argv);
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.14  2006/11/16 21:50:08  ssikorsk
 * Revamp code to use CDbapiDriverSampleApp.
 *
 * Revision 1.13  2006/08/31 18:46:11  ssikorsk
 * Get rid of unused variables.
 *
 * Revision 1.12  2006/02/24 19:36:12  ssikorsk
 * Added #include <test/test_assert.h> for test-suite sake
 *
 * Revision 1.11  2006/01/26 12:15:36  ssikorsk
 * Revamp code to include <dbapi/driver/dbapi_svc_mapper.hpp>;
 * Removed protection of DBLB_INSTALL_DEFAULT;
 *
 * Revision 1.10  2006/01/24 14:05:27  ssikorsk
 * Protect DBLB_INSTALL_DEFAULT with HAVE_LIBCONNEXT
 *
 * Revision 1.9  2006/01/24 12:53:24  ssikorsk
 * Revamp demo applications to use CNcbiApplication;
 * Use load balancer and configuration in an ini-file to connect to a
 * secondary server in case of problems with a primary server;
 *
 * Revision 1.8  2005/08/16 11:13:27  ssikorsk
 * Use SCHUMANN instead of BARTOK as a Sybase server.
 *
 * Revision 1.7  2004/09/01 21:31:29  vakatov
 * Use BARTOK instead of MOZART as the test Sybase-11.0.3 SQL server
 *
 * Revision 1.6  2004/05/17 21:12:13  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.5  2002/04/25 16:43:17  soussov
 * makes it plain
 *
 * Revision 1.4  2001/11/06 17:59:57  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.3  2001/10/25 00:18:02  vakatov
 * SampleDBAPI_XXX() to accept yet another arg -- server name
 *
 * Revision 1.2  2001/10/24 16:37:09  lavr
 * Finish log with horizontal rule
 *
 * Revision 1.1  2001/10/23 20:52:11  lavr
 * Initial revision (derived from former sample programs)
 *
 * Revision 1.1  2001/10/02 20:34:28  soussov
 * Initial revision
 *
 * ===========================================================================
 */
