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
 * Author: Andrei Gourianov
 *
 * File Description:
 *   SOAP Detail object
 *
 * ===========================================================================
 */

// standard includes
#include <serial/serialimpl.hpp>

// generated includes
#include <serial/soap/soap_detail.hpp>

BEGIN_NCBI_SCOPE
// generated classes

void CSoapDetail::C_E::ResetAnyContent(void)
{
    (*m_AnyContent).Reset();
}

void CSoapDetail::C_E::SetAnyContent(CAnyContentObject& value)
{
    m_AnyContent.Reset(&value);
}

void CSoapDetail::C_E::Reset(void)
{
    ResetAnyContent();
}

BEGIN_NAMED_CLASS_INFO("", CSoapDetail::C_E)
{
    ADD_NAMED_REF_MEMBER("AnyContent", m_AnyContent, CAnyContentObject)->SetNoPrefix()->SetNotag()->SetAnyContent();
}
END_CLASS_INFO

// constructor
CSoapDetail::C_E::C_E(void)
    : m_AnyContent(0/*new CAnyContentObject()*/)
{
}

// destructor
CSoapDetail::C_E::~C_E(void)
{
}


void CSoapDetail::Reset(void)
{
    m_data.clear();
    m_set_State[0] &= ~0x3;
}

BEGIN_NAMED_IMPLICIT_CLASS_INFO("Detail", CSoapDetail)
{
    SET_CLASS_MODULE("soap");
    ADD_NAMED_MEMBER("", m_data, STL_list, (STL_CRef, (CLASS, (C_E))))->SetSetFlag(MEMBER_PTR(m_set_State[0]));
    info->RandomOrder();
}
END_CLASS_INFO

// constructor
CSoapDetail::CSoapDetail(void)
{
    memset(m_set_State,0,sizeof(m_set_State));
}

// destructor
CSoapDetail::~CSoapDetail(void)
{
}

END_NCBI_SCOPE

/* --------------------------------------------------------------------------
* $Log$
* Revision 1.1  2003/09/25 19:45:33  gouriano
* Added soap Fault object
*
*
* ===========================================================================
*/
