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
 * Author: Philip Johnson
 *
 * File Description: CSeqalignException class
 *
 */

#ifndef OBJECTS_SEQALIGN_SEQALIGN_EXCEPTION_HPP
#define OBJECTS_SEQALIGN_SEQALIGN_EXCEPTION_HPP

#include <corelib/ncbiexpt.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

class NCBI_SEQALIGN_EXPORT CSeqalignException :
    EXCEPTION_VIRTUAL_BASE public CException
{
public:
    enum EErrCode {
        eUnsupported,
        eInvalidInputAlignment,
        eOutOfRange
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eUnsupported:           return "eUnsupported";
        case eInvalidInputAlignment: return "eInvalidInputAlignment";
        case eOutOfRange:            return "eOutOfRange";
        default:                     return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CSeqalignException, CException);
};

END_objects_SCOPE
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.3  2003/08/26 20:28:38  johnson
* added 'SwapRows' method
*
* Revision 1.2  2003/08/19 21:09:39  todorov
* +eInvalidInputAlignment
*
* Revision 1.1  2003/08/13 18:11:03  johnson
* initial revision; created so CSeq_align::Reverse can throw if asked to
* reverse an (as yet) unsupported segment type
*
*
* ===========================================================================
*/

#endif // OBJECTS_SEQALIGN_SEQALIGN_EXCEPTION_HPP
