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
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  2000/03/07 14:06:21  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
* Added generation of reference counted objects.
*
* Revision 1.4  1999/12/28 18:55:49  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.3  1999/12/17 19:05:01  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.2  1999/09/14 18:54:15  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.1  1999/09/07 20:57:57  vasilche
* Forgot to add some files.
*
* ===========================================================================
*/

#include <serial/autoptrinfo.hpp>
#include <serial/typemap.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

BEGIN_NCBI_SCOPE

static CTypeInfoMap<CAutoPointerTypeInfo> CAutoPointerTypeInfo_map;

CAutoPointerTypeInfo::CAutoPointerTypeInfo(TTypeInfo type)
    : CParent(type->GetName(), type)
{
}

CAutoPointerTypeInfo::~CAutoPointerTypeInfo(void)
{
}

TTypeInfo CAutoPointerTypeInfo::GetTypeInfo(TTypeInfo base)
{
    return CAutoPointerTypeInfo_map.GetTypeInfo(base);
}

void CAutoPointerTypeInfo::WriteData(CObjectOStream& out,
                                     TConstObjectPtr object) const
{
    TConstObjectPtr data = GetObjectPointer(object);
    if ( data == 0 )
        THROW1_TRACE(runtime_error, "null auto pointer");
    TTypeInfo dataType = GetDataTypeInfo();
    if ( dataType->GetRealTypeInfo(data) != dataType )
        THROW1_TRACE(runtime_error, "auto pointer have different type");
    out.WriteExternalObject(data, dataType);
}

void CAutoPointerTypeInfo::ReadData(CObjectIStream& in,
                                    TObjectPtr object) const
{
    TObjectPtr data = const_cast<TObjectPtr>(GetObjectPointer(object));
    TTypeInfo dataType = GetDataTypeInfo();
    if ( data == 0 ) {
        _TRACE("null auto pointer");
        SetObjectPointer(object, data = dataType->Create());
    }
    else if ( dataType->GetRealTypeInfo(data) != dataType )
        THROW1_TRACE(runtime_error, "auto pointer have different type");
    in.ReadExternalObject(data, dataType);
}

void CAutoPointerTypeInfo::SkipData(CObjectIStream& in) const
{
    in.SkipExternalObject(GetDataTypeInfo());
}

END_NCBI_SCOPE
