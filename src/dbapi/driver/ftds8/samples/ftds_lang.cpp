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

#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/ftds/interfaces.hpp>

USING_NCBI_SCOPE;


int main()
{
    try {
        CTDSContext my_context;

        CDB_Connection* con = my_context.Connect("MSSQL2", "anyone", "allowed", 0);

        CDB_LangCmd* lcmd =
            con->LangCmd("select name, crdate from sysdatabases");
        lcmd->Send();

        while (lcmd->HasMoreResults()) {
            CDB_Result* r = lcmd->Result();
            if (!r)
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
            delete r;
        }
        delete lcmd;
        delete con;
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
