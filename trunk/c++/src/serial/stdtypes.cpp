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
* Revision 1.3  1999/06/24 14:45:01  vasilche
* Added binary ASN.1 output.
*
* Revision 1.2  1999/06/04 20:51:49  vasilche
* First compilable version of serialization.
*
* Revision 1.1  1999/05/19 19:56:57  vasilche
* Commit just in case.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/stdtypes.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

BEGIN_NCBI_SCOPE

size_t CStdTypeInfo<void>::GetSize(void) const
{
    return 0;
}

TObjectPtr CStdTypeInfo<void>::Create(void) const
{
    throw runtime_error("void cannot be created");
}
    
TTypeInfo CStdTypeInfo<void>::GetTypeInfo(void)
{
    static TTypeInfo typeInfo = new CStdTypeInfo;
    return typeInfo;
}

TConstObjectPtr CStdTypeInfo<void>::GetDefault(void) const
{
    throw runtime_error("void does not have default");
}

bool CStdTypeInfo<void>::Equals(TConstObjectPtr , TConstObjectPtr ) const
{
    throw runtime_error("void cannot compare");
}

void CStdTypeInfo<void>::ReadData(CObjectIStream& , TObjectPtr ) const
{
    throw runtime_error("void cannot be read");
}
    
void CStdTypeInfo<void>::WriteData(CObjectOStream& , TConstObjectPtr ) const
{
    throw runtime_error("void cannot be written");
}

TObjectPtr CStdTypeInfo<string>::Create(void) const
{
    return new TObjectType();
}

TTypeInfo CStdTypeInfo<string>::GetTypeInfo(void)
{
    static TTypeInfo typeInfo = new CStdTypeInfo;
    return typeInfo;
}

TConstObjectPtr CStdTypeInfo<string>::GetDefault(void) const
{
    static string def;
    return &def;
}

END_NCBI_SCOPE
