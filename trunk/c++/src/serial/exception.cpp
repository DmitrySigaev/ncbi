/*  $Id$
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
* Author: Eugene Vasilchenko
*
* File Description:
*   Standard serialization exceptions
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.6  2001/04/17 03:58:39  vakatov
* CSerialNotImplemented:: -- constructor and destructor added
*
* Revision 1.5  2001/01/05 20:10:50  vasilche
* CByteSource, CIStrBuffer, COStrBuffer, CLightString, CChecksum, CWeakMap
* were moved to util.
*
* Revision 1.4  2000/07/05 13:20:22  vasilche
* Fixed bug on 64 bit compilers.
*
* Revision 1.3  2000/07/03 20:47:22  vasilche
* Removed unused variables/functions.
*
* Revision 1.2  2000/07/03 18:42:43  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.1  2000/02/17 20:02:43  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* ===========================================================================
*/

#include <serial/exception.hpp>

BEGIN_NCBI_SCOPE

CSerialException::CSerialException(const string& msg) THROWS_NONE
    : runtime_error(msg)
{
}

CSerialException::~CSerialException(void) THROWS_NONE
{
}


CSerialNotImplemented::CSerialNotImplemented(const string& msg) THROWS_NONE
    : CSerialException(msg)
{
    return;
}


CSerialNotImplemented::~CSerialNotImplemented(void) THROWS_NONE
{
    return;
}


CInvalidChoiceSelection::CInvalidChoiceSelection(const string& current,
                                                 const string& mustBe)
    THROWS_NONE
    : runtime_error("Invalid choice selection: "+current+". "
                    "Expected: "+mustBe)
{
}

CInvalidChoiceSelection::CInvalidChoiceSelection(size_t currentIndex,
                                                 size_t mustBeIndex,
                                                 const char* const names[],
                                                 size_t namesCount) THROWS_NONE
    : runtime_error(string("Invalid choice selection: ")+
                    GetName(currentIndex, names, namesCount)+". "
                    "Expected: "+
                    GetName(mustBeIndex, names, namesCount))
{
}

const char* CInvalidChoiceSelection::GetName(size_t index,
                                             const char* const names[],
                                             size_t namesCount)
{
    if ( index > namesCount )
        return "?unknown?";
    return names[index];
    
}

CInvalidChoiceSelection::~CInvalidChoiceSelection(void) THROWS_NONE
{
}

END_NCBI_SCOPE
