/*  $RCSfile$  $Revision$  $Date$
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
#include <ncbistd.hpp>
#include <node.hpp>
#include <html.hpp> //remove!
BEGIN_NCBI_SCOPE

// append a child
CNCBINode * CNCBINode::AppendChild(CNCBINode * childNode)
{
    if(!childNode) return NULL;
    m_ChildNodes.push_back(childNode);
    ((CNCBINode *)childNode)->m_ParentNode = this;
    ((CNCBINode *)childNode)->m_SelfIter = --(m_ChildNodes.end()); // don't forget that end() points beyond the list
    return childNode;
}


// insert a child before the given node
CNCBINode * CNCBINode::InsertBefore(CNCBINode * newChild, CNCBINode * refChild)
{
    if(!newChild | ! refChild) return NULL;
    ((CNCBINode *)newChild)->m_ParentNode = this;
    ((CNCBINode *)newChild)->m_SelfIter = (list<CNCBINode *>::iterator) m_ChildNodes.insert(((CNCBINode *)refChild)->m_SelfIter, newChild);
    return newChild;
}


// we leak memory like crazy until the sunpro compiler is upgraded
// it can't handle a virtual destructor in any form for some reason.

CNCBINode::~CNCBINode(void)
{
#if 0
    list <CNCBINode *>::iterator iChildren;
    CNCBINode * temp;

    iChildren = m_ChildNodes.begin();
    while ( iChildren != m_ChildNodes.end()) {     
        temp = (CNCBINode *) *iChildren; // need an extra copy as the iterator gets destroyed by the child
        iChildren++;
        delete temp;
    }

    if(m_ParentNode) {
         ((CNCBINode *)m_ParentNode)->m_ChildNodes.erase(m_SelfIter);
    }
#endif
}


END_NCBI_SCOPE



