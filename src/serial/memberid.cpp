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
* Revision 1.5  2000/01/05 19:43:53  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.4  1999/12/17 19:05:02  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.3  1999/07/22 19:40:54  vasilche
* Fixed bug with complex object graphs (pointers to members of other objects).
*
* Revision 1.2  1999/07/02 21:31:54  vasilche
* Implemented reading from ASN.1 binary format.
*
* Revision 1.1  1999/06/30 16:04:51  vasilche
* Added support for old ASN.1 structures.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/memberid.hpp>

BEGIN_NCBI_SCOPE

CMemberId::CMemberId(const string& name)
    : m_Name(name), m_Tag(-1)
{
}

CMemberId::CMemberId(const string& name, TTag tag)
    : m_Name(name), m_Tag(tag)
{
}

CMemberId::CMemberId(const char* name)
    : m_Name(name), m_Tag(-1)
{
}

CMemberId::CMemberId(const char* name, TTag tag)
    : m_Name(name), m_Tag(tag)
{
}

void CMemberId::SetName(const string& name)
{
    m_Name = name;
}

void CMemberId::UpdateName(const CMemberId& id)
{
    if ( GetName().empty() ) {
        _ASSERT(GetTag() == id.GetTag() || id.GetTag() < 0);
        m_Name = id.GetName();
    }
    else {
        _ASSERT(GetName() == id.GetName());
    }
}

CMemberId* CMemberId::SetTag(TTag tag)
{
    m_Tag = tag;
    return this;
}

void CMemberId::SetNext(const CMemberId& id)
{
    m_Name = id.GetName();
    int tag = id.GetTag();
    if ( tag < 0 ) {
        ++m_Tag;
    }
    else {
        m_Tag = tag;
    }
}

bool CMemberId::operator==(const CMemberId& id) const
{
    if ( id.GetTag() < 0 )
        return GetName() == id.GetName();
    else
        return GetTag() == id.GetTag();
}

string CMemberId::ToString(void) const
{
    if ( !m_Name.empty() )
        return m_Name;
    else if ( m_Tag >= 0 )
        return '[' + NStr::IntToString(m_Tag) + ']';
    else
        return "";
}

END_NCBI_SCOPE
