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
* Author:  Lewis Geer
*
* File Description:
*   code for CNCBINode
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.15  1999/11/01 14:32:04  vasilche
* Fixed null pointer reference in MapTagAll
*
* Revision 1.14  1999/10/28 13:40:35  vasilche
* Added reference counters to CNCBINode.
*
* Revision 1.13  1999/05/27 21:43:30  vakatov
* Get rid of some minor compiler warnings
*
* Revision 1.12  1999/05/20 16:52:34  pubmed
* SaveAsText action for query; minor changes in filters,labels, tabletemplate
*
* Revision 1.11  1999/05/04 00:03:18  vakatov
* Removed the redundant severity arg from macro ERR_POST()
*
* Revision 1.10  1999/04/30 19:21:08  vakatov
* Added more details and more control on the diagnostics
* See #ERR_POST, EDiagPostFlag, and ***DiagPostFlag()
*
* Revision 1.9  1999/03/18 17:54:50  vasilche
* CNCBINode will try to call PrintEnd if exception in PrintChildren
* occures
*
* Revision 1.8  1999/01/25 19:32:44  vasilche
* Virtual destructors now work properly.
*
* Revision 1.7  1999/01/04 20:06:14  vasilche
* Redesigned CHTML_table.
* Added selection support to HTML forms (via hidden values).
*
* Revision 1.6  1998/12/28 20:29:19  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.5  1998/12/23 21:21:04  vasilche
* Added more HTML tags (almost all).
* Importent ones: all lists (OL, UL, DIR, MENU), fonts (FONT, BASEFONT).
*
* Revision 1.4  1998/12/21 22:25:04  vasilche
* A lot of cleaning.
*
* Revision 1.3  1998/11/23 23:42:29  lewisg
* *** empty log message ***
*
* Revision 1.2  1998/10/29 16:13:06  lewisg
* version 2
*
* Revision 1.1  1998/10/06 20:36:05  lewisg
* new html lib and test program
*
* ===========================================================================
*/

#include <html/node.hpp>

BEGIN_NCBI_SCOPE

CNCBINode::CNCBINode(void)
    : m_RefCount(0)
{
}

CNCBINode::CNCBINode(const string& name)
    : m_RefCount(0), m_Name(name)
{
}

CNCBINode::~CNCBINode(void)
{
    if ( m_RefCount != 0 ) {
        THROW1_TRACE(runtime_error,
                     "~CNCBINode(" + m_Name + "): refCount = " +
                     NStr::IntToString(m_RefCount));
    }
}

void CNCBINode::Destroy(void)
{
    if ( m_RefCount < 0 ) {
        THROW1_TRACE(runtime_error,
                     "CNCBINode::Destroy(" + m_Name + "): refCount = " +
                     NStr::IntToString(m_RefCount));
    }
    delete this;
}

void CNCBINode::BadRef(void)
{
    if ( m_RefCount <= 0 ) {
        THROW1_TRACE(runtime_error,
                     "CNCBINode::BadRef: Reference count was negative");
    }
}

// append a child
void CNCBINode::DoAppendChild(CNCBINode* child)
{
    GetChildren().push_back(child);
}

void CNCBINode::RemoveAllChildren(void)
{
#if NCBI_LIGHTWEIGHT_LIST
    m_Children.clear();
#else
    m_Children.reset(0);
#endif
}

bool CNCBINode::HaveAttribute(const string& name) const
{
    return HaveAttributes() && Attributes().find(name) != Attributes().end();
}

const string& CNCBINode::GetAttribute(const string& name) const
{
    if ( HaveAttributes() ) {
        TAttributes::const_iterator ptr = Attributes().find(name);
        if ( ptr != Attributes().end() ) {
            return ptr->second;
        }
    }
    return NcbiEmptyString;
}

const string* CNCBINode::GetAttributeValue(const string& name) const
{
    if ( HaveAttributes() ) {
        TAttributes::const_iterator ptr = Attributes().find(name);
        if ( ptr != Attributes().end() ) {
            return &ptr->second;
        }
    }
    return 0;
}

void CNCBINode::SetAttribute(const string& name)
{
    GetAttributes()[name];
}

void CNCBINode::SetAttribute(const string& name, const string& value)
{
    GetAttributes()[name] = value;
}

void CNCBINode::SetAttribute(const char* name)
{
    GetAttributes()[name];
}

void CNCBINode::SetAttribute(const char* name, const string& value)
{
    GetAttributes()[name] = value;
}

// this function searches for a text string in a Text node and replaces it with a node
CNCBINode* CNCBINode::MapTag(const string& /*tagname*/)
{
    return 0;
}

// this function searches for a text string in a Text node and replaces it with a node
CNodeRef CNCBINode::MapTagAll(const string& tagname, const TMode& mode)
{
    const TMode* context = &mode;
    do {
        CNCBINode* stackNode = context->GetNode();
        if ( stackNode ) {
            CNCBINode* mapNode = stackNode->MapTag(tagname);
            if ( mapNode )
                return mapNode;
        }
        context = context->GetPreviousContext();
    } while ( context );
    return 0;
}

// print the whole node tree (with possible initialization)
CNcbiOstream& CNCBINode::Print(CNcbiOstream& out, TMode prev)
{
    if ( !HaveChildren() ) { // if there is no children
        CreateSubNodes();             // create them
    }

    TMode mode(&prev, this);
    PrintBegin(out, mode);

    try {
        PrintChildren(out, mode);
    }
    catch (...) {
        ERR_POST("CNCBINode::Print: exception in PrintChildren, trying to PrintEnd");
        PrintEnd(out, mode);
        throw;
    }

    PrintEnd(out, mode);
    return out;
}

CNcbiOstream& CNCBINode::PrintBegin(CNcbiOstream& out, TMode)
{
    return out;
}

CNcbiOstream& CNCBINode::PrintEnd(CNcbiOstream& out, TMode)
{
    return out;
}

// print all children
CNcbiOstream& CNCBINode::PrintChildren(CNcbiOstream& out, TMode mode)
{
    if ( HaveChildren() ) {
        for ( TChildren::iterator i = Children().begin();
              i != Children().end(); ++i) {
            Node(i)->Print(out, mode);
        }
    }
    return out;
}

// by default, we don't create any subnode
void CNCBINode::CreateSubNodes(void)
{
}

END_NCBI_SCOPE
