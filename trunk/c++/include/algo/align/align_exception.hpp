#ifndef ALGO_EXCEPTION__HPP
#define ALGO_EXCEPTION__HPP

/* $Id$
* ===========================================================================
*
*                            public DOMAIN NOTICE                          
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
* Author:  Yuri Kapustin, Andrei Gourianov
*
* File Description:
*   Algo library exceptions
*
*/

#include <corelib/ncbiexpt.hpp>


/** @addtogroup AlgoExcep
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class NCBI_XALGOALIGN_EXPORT CAlgoException : public CException
{
    NCBI_EXCEPTION_DEFAULT(CAlgoException,CException);
};

class NCBI_XALGOALIGN_EXPORT CAlgoAlignException : public CAlgoException 
{
public:
    enum EErrCode {
        eInternal,
        eBadParameter,
        eMemoryLimit,
        eInvalidCharacter,
        eIncorrectSequenceOrder,
        eInvalidSpliceTypeIndex
    };
    virtual const char* GetErrCodeString(void) const {
        switch ( GetErrCode() ) {
        case eInternal:
            return "Internal error";
        case eBadParameter:
            return "One or more parameters passed are invalid";
        case eMemoryLimit:
            return "Memory limit exceeded";
        case eInvalidCharacter:
            return "Sequence contains one or more invalid characters";
        case eIncorrectSequenceOrder:
            return "mRna should go first";
        case eInvalidSpliceTypeIndex:
            return "Splice type index out of range";
        default:
            return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT(CAlgoAlignException, CAlgoException);
};


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2003/08/04 15:43:19  dicuccio
 * Modified export specifiers to be more flexible
 *
 * Revision 1.5  2003/06/17 17:20:28  kapustin
 * CNWAlignerException -> CAlgoAlignException
 *
 * Revision 1.4  2003/04/10 19:04:27  siyan
 * Added doxygen support
 *
 * Revision 1.3  2003/03/25 22:07:09  kapustin
 * Add eInvalidSpliceTypeIndex exception type
 *
 * Revision 1.2  2003/03/14 19:15:28  kapustin
 * Add eMemoryLimit exception type
 *
 * Revision 1.1  2003/02/26 21:30:32  gouriano
 * modify C++ exceptions thrown by this library
 *
 *
 * ===========================================================================
 */

#endif  /* ALGO_EXCEPTION__HPP */
