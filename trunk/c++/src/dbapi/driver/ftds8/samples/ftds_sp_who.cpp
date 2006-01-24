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

#include <ncbi_pch.hpp>
#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/ftds/interfaces.hpp>


USING_NCBI_SCOPE;


int main()
{
    try {
        CTDSContext my_context;

        auto_ptr<CDB_Connection> con(my_context.Connect("MS_DEV2", 
                                                        "anyone", 
                                                        "allowed", 
                                                        0));

        auto_ptr<CDB_RPCCmd> rcmd( con->RPC("sp_who", 0));
        rcmd->Send();

        while (rcmd->HasMoreResults()) {
            auto_ptr<CDB_Result> r(rcmd->Result());
            if (!r.get())
                continue;
            
            if (r->ResultType() == eDB_RowResult) {
                while (r->Fetch()) {
                    for (unsigned int j = 0;  j < r->NofItems(); j++) {
                        EDB_Type rt = r->ItemDataType(j);
                        if (rt == eDB_Char || rt == eDB_VarChar) {
                            CDB_VarChar r_vc;
                            r->GetItem(&r_vc);
                            cout << r->ItemName(j) << ": "
                                 << (r_vc.IsNULL()? "" : r_vc.Value()) << " \t";
                        } else if (rt == eDB_Int ||
                                   rt == eDB_SmallInt ||
                               rt == eDB_TinyInt) {
                            CDB_Int r_in;
                            r->GetItem(&r_in);
                            cout << r->ItemName(j) << ": " << r_in.Value() << ' ';
                        } else
                            r->SkipItem();
                    }
                    cout << endl;
                }
            }
        }
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
 * Revision 1.5  2006/01/24 12:53:25  ssikorsk
 * Revamp demo applications to use CNcbiApplication;
 * Use load balancer and configuration in an ini-file to connect to a
 * secondary server in case of problems with a primary server;
 *
 * Revision 1.4  2004/12/10 15:26:12  ssikorsk
 * FreeTDS is ported on windows
 *
 * Revision 1.3  2003/10/10 14:03:59  ucko
 * Switch MSSQL server to ms_dev2, since dev1 seems to be down.
 *
 * Revision 1.2  2003/08/05 19:23:45  vakatov
 * MSSQL2 --> MS_DEV1
 *
 * Revision 1.1  2002/12/05 22:47:03  soussov
 * Initial revision
 *
 * Revision 1.3  2002/04/25 20:57:08  soussov
 * makes it plain
 *
 * Revision 1.2  2001/11/06 18:00:04  lavr
 * Formatted uniformly as the rest of the library
 *
 * Revision 1.1  2001/10/25 00:46:44  vakatov
 * Initial revision
 *
 * ===========================================================================
 */
