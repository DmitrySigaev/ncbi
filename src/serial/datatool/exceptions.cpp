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
*   datatool exceptions
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2000/04/07 19:26:26  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.1  2000/03/15 21:24:12  vasilche
* Error diagnostic about ambiguous types made more clear.
*
* Revision 1.1  2000/02/01 21:46:17  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.3  1999/11/15 19:36:14  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <serial/tool/exceptions.hpp>
#include <serial/tool/type.hpp>
#include <corelib/ncbiutil.hpp>

BEGIN_NCBI_SCOPE

CTypeNotFound::CTypeNotFound(const string& msg)
    THROWS_NONE
    : runtime_error(msg)
{
}

CTypeNotFound::~CTypeNotFound(void)
    THROWS_NONE
{
}

CModuleNotFound::CModuleNotFound(const string& msg)
    THROWS_NONE
    : CTypeNotFound(msg)
{
}

CModuleNotFound::~CModuleNotFound(void)
    THROWS_NONE
{
}

CAmbiguiousTypes::CAmbiguiousTypes(const string& msg,
                                   const list<CDataType*>& types)
    THROWS_NONE
    : CTypeNotFound(msg), m_Types(types)
{
}

CAmbiguiousTypes::~CAmbiguiousTypes(void)
    THROWS_NONE
{
}

CResolvedTypeSet::CResolvedTypeSet(const string& name)
    : m_Name(name)
{
}

CResolvedTypeSet::CResolvedTypeSet(const string& module, const string& name)
    : m_Module(module), m_Name(name)
{
}

CResolvedTypeSet::~CResolvedTypeSet(void)
{
}

void CResolvedTypeSet::Add(CDataType* type)
{
    m_Types.push_back(type);
}

void CResolvedTypeSet::Add(const CAmbiguiousTypes& types)
{
    iterate ( list<CDataType*>, i, types.GetTypes() ) {
        m_Types.push_back(*i);
    }
}

CDataType* CResolvedTypeSet::GetType(void) const
    THROWS((CTypeNotFound))
{
    if ( m_Types.empty() ) {
        string msg = "type not found: ";
        if ( !m_Module.empty() ) {
            msg += m_Module;
            msg += '.';
        }
        THROW1_TRACE(CTypeNotFound, msg+m_Name);
    }

    {
        list<CDataType*>::const_iterator i = m_Types.begin();
        CDataType* type = *i;
        ++i;
        if ( i == m_Types.end() )
            return type;
    }
    string msg = "ambiguous types: ";
    if ( !m_Module.empty() ) {
        msg += m_Module;
        msg += '.';
    }
    msg += m_Name;
    msg += " defined in:";
    iterate ( list<CDataType*>, i, m_Types ) {
        msg += ' ';
        msg += (*i)->GetSourceFileName();
        msg += ':';
        msg += NStr::IntToString((*i)->GetSourceLine());
    }
    THROW_TRACE(CAmbiguiousTypes, (msg, m_Types));
}

END_NCBI_SCOPE
