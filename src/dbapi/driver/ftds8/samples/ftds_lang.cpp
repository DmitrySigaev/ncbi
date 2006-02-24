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

#include <corelib/ncbiapp.hpp>
#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/ftds/interfaces.hpp>
#include <dbapi/driver/dbapi_svc_mapper.hpp>
#include <test/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


class CDemoeApp : public CNcbiApplication
{
public:
    virtual ~CDemoeApp(void) {}

    virtual int Run(void);
};

int
CDemoeApp::Run(void)
{
    try {
        DBLB_INSTALL_DEFAULT();
                
        CTDSContext my_context;

        auto_ptr<CDB_Connection> con(my_context.Connect("MS_DEV2", 
                                                        "anyone", 
                                                        "allowed", 
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
                CDB_LongChar dbname(240);
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
    } catch (const CException& e) {
        return 1;
    }

    return 0;
}

int main(int argc, const char* argv[])
{
    return CDemoeApp().AppMain(argc, argv);
}



/*
 * ===========================================================================
 * $Log$
 * Revision 1.10  2006/02/24 19:36:13  ssikorsk
 * Added #include <test/test_assert.h> for test-suite sake
 *
 * Revision 1.9  2006/01/26 12:15:37  ssikorsk
 * Revamp code to include <dbapi/driver/dbapi_svc_mapper.hpp>;
 * Removed protection of DBLB_INSTALL_DEFAULT;
 *
 * Revision 1.8  2006/01/24 14:05:27  ssikorsk
 * Protect DBLB_INSTALL_DEFAULT with HAVE_LIBCONNEXT
 *
 * Revision 1.7  2006/01/24 12:53:25  ssikorsk
 * Revamp demo applications to use CNcbiApplication;
 * Use load balancer and configuration in an ini-file to connect to a
 * secondary server in case of problems with a primary server;
 *
 * Revision 1.6  2004/12/20 16:20:29  ssikorsk
 * Refactoring of dbapi/driver/samples
 *
 * Revision 1.5  2004/12/10 15:26:12  ssikorsk
 * FreeTDS is ported on windows
 *
 * Revision 1.4  2003/10/10 14:03:59  ucko
 * Switch MSSQL server to ms_dev2, since dev1 seems to be down.
 *
 * Revision 1.3  2003/08/05 19:23:45  vakatov
 * MSSQL2 --> MS_DEV1
 *
 * Revision 1.2  2003/04/30 18:22:10  soussov
 * changing datatype for dbname to CDB_LongChar
 *
 * Revision 1.1  2002/12/05 22:47:02  soussov
 * Initial revision
 *
 * Revision 1.3  2002/04/25 20:57:08  soussov
 * makes it plain
 *
 * Revision 1.2  2001/11/06 18:00:03  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.1  2001/10/25 00:46:44  vakatov
 * Initial revision
 *
 * ===========================================================================
 */
