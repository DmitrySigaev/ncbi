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
 * This simple program illustartes how to use the language command
 */

#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/ctlib/interfaces.hpp>
#include <dbapi/driver/samples/dbapi_driver_samples.hpp>


USING_NCBI_SCOPE;


int main()
{
    try {
        CTLibContext my_context;

        SampleDBAPI_Lang(my_context);
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
 * Revision 1.1  2001/10/23 20:52:11  lavr
 * Initial revision (derived from former sample programs)
 *
 * Revision 1.1  2001/10/02 20:34:28  soussov
 * Initial revision
 *
 */    
