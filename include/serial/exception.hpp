#ifndef EXCEPTION__HPP
#define EXCEPTION__HPP
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
*   Standard exception classes used in serial package
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiexpt.hpp>

BEGIN_NCBI_SCOPE

// root class for all serialization exceptions
class NCBI_XSERIAL_EXPORT CSerialException : public CException
{
public:
    enum EErrCode {
        eNotImplemented,
        eEOF,
        eIoError,
        eFormatError,
        eOverflow,
        eInvalidData,
        eIllegalCall,
        eFail,
        eNotOpen
    };
    virtual const char* GetErrCodeString(void) const {
        switch ( GetErrCode() ) {
        case eNotImplemented: return "eNotImplemented";
        case eEOF:            return "eEOF";
        case eIoError:        return "eIoError";
        case eFormatError:    return "eFormatError";
        case eOverflow:       return "eOverflow";
        case eInvalidData:    return "eInvalidData";
        case eIllegalCall:    return "eIllegalCall";
        case eFail:           return "eFail";
        case eNotOpen:        return "eNotOpen";
        default:              return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT(CSerialException,CException);
};

class NCBI_XSERIAL_EXPORT CInvalidChoiceSelection : public runtime_error
{
public:
    CInvalidChoiceSelection(const string& current, const string& mustBe) throw();
    CInvalidChoiceSelection(size_t currentIndex, size_t mustBeIndex,
                            const char* const names[], size_t namesCount) throw();
    ~CInvalidChoiceSelection(void) throw();

    static const char* GetName(size_t index,
                               const char* const names[], size_t namesCount);
};

END_NCBI_SCOPE

#endif /* EXCEPTION__HPP */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.9  2003/03/10 18:52:37  gouriano
* use new structured exceptions (based on CException)
*
* Revision 1.8  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.7  2001/04/12 16:59:13  kholodov
* Added: CSerialNotImplemented exception
*
* Revision 1.6  2001/01/05 20:10:34  vasilche
* CByteSource, CIStrBuffer, COStrBuffer, CLightString, CChecksum, CWeakMap
* were moved to util.
*
* Revision 1.5  2000/10/03 17:22:31  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.4  2000/07/03 18:42:33  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.3  2000/04/28 16:58:01  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* Revision 1.2  2000/04/10 21:01:38  vasilche
* Fixed Erase for map/set.
* Added iteratorbase.hpp header for basic internal classes.
*
* Revision 1.1  2000/02/17 20:02:28  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
*
* ===========================================================================
*/
