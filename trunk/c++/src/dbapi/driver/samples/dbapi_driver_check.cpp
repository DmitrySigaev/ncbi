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
 * Author:  Vladimir Ivanov
 *
 * Check DBAPI driver DLL existence.
 *
 * Returns 0 exit code if the DBAPI driver DLL with a name specified in
 * a command line exist. Otherwise returns 1.
 *
 */

#include <corelib/ncbiexpt.hpp>
#include <dbapi/driver/driver_mgr.hpp>

USING_NCBI_SCOPE;


int main(int argc, char* argv[])
{
    if (argc != 2) {
        return 1;
    }
    string driver_name = argv[1];

    CException::EnableBackgroundReporting(false);
    
    try {
        C_DriverMgr drv_mgr;
        string err_msg;
        I_DriverContext* my_context = drv_mgr.GetDriverContext(driver_name,
                                                               &err_msg);
        if (!my_context) {
            return 1;
        }
        
    } catch (exception& e) {
        return 1;
    }
    return 0;
}



/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2003/03/11 21:35:13  ivanov
 * Renaming check_dbapi_driver -> dbapi_driver_check
 *
 * Revision 1.1  2003/03/11 17:29:52  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
