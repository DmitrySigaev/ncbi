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
* Revision 1.5  1999/05/28 16:32:15  vasilche
* Fixed memory leak in page tag mappers.
*
* Revision 1.4  1999/01/06 21:35:37  vasilche
* Avoid use of Clone.
* Fixed default CHTML_text width.
*
* Revision 1.3  1998/12/28 20:29:19  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.2  1998/12/24 16:15:42  vasilche
* Added CHTMLComment class.
* Added TagMappers from static functions.
*
* Revision 1.1  1998/12/22 16:39:15  vasilche
* Added ReadyTagMapper to map tags to precreated nodes.
*
* ===========================================================================
*/

#include <html/page.hpp>

// This is to use the ANSI C++ standard templates without the "std::" prefix
// NCBI_USING_NAMESPACE_STD;

// This is to use the ANSI C++ standard templates without the "std::" prefix
// and to use NCBI C++ entities without the "ncbi::" prefix
// USING_NCBI_SCOPE;

BEGIN_NCBI_SCOPE


BaseTagMapper::~BaseTagMapper()
{
}

StaticTagMapper::StaticTagMapper(CNCBINode* (*function)(void))
    : m_Function(function)
{
}

BaseTagMapper* StaticTagMapper::Clone(void) const
{
    return new StaticTagMapper(*this);
}

CNCBINode* StaticTagMapper::MapTag(CNCBINode*, const string&) const
{
    return (*m_Function)();
}

StaticTagMapperByName::StaticTagMapperByName(CNCBINode* (*function)(const string& name))
    : m_Function(function)
{
}

BaseTagMapper* StaticTagMapperByName::Clone(void) const
{
    return new StaticTagMapperByName(*this);
}

CNCBINode* StaticTagMapperByName::MapTag(CNCBINode*, const string& name) const
{
    return (*m_Function)(name);
}

ReadyTagMapper::ReadyTagMapper(CNCBINode* node)
    : m_Node(node)
{
}

BaseTagMapper* ReadyTagMapper::Clone(void) const
{
    return new ReadyTagMapper(m_Node->Clone());
}

CNCBINode* ReadyTagMapper::MapTag(CNCBINode*, const string&) const
{
    // we decided to avoid use of cloning
    // also to avoid double delete of node we release it
    if ( !m_Node.get() )
        ERR_POST("double call of ReadyTagMapper::MapTag");
    return const_cast<ReadyTagMapper*>(this)->m_Node.release();
}

END_NCBI_SCOPE
