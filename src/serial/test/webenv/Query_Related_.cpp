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
 * File Description:
 *   This code is generated by application DATATOOL
 *   using specifications from the data definition file
 *   'twebenv.asn'.
 *
 * ATTENTION:
 *   Don't edit or check-in this file to the CVS as this file will
 *   be overridden (by DATATOOL) without warning!
 * ===========================================================================
 */

// standard includes
#include <ncbi_pch.hpp>
#include <serial/serialimpl.hpp>

// generated includes
#include "Query_Related.hpp"
#include "Item_Set.hpp"
#include "Query_Command.hpp"

// generated classes

void CQuery_Related_Base::C_Items::Reset(void)
{
    switch ( m_choice ) {
    case e_Items:
        m_object->RemoveReference();
        break;
    default:
        break;
    }
    m_choice = e_not_set;
}

void CQuery_Related_Base::C_Items::DoSelect(E_Choice index)
{
    switch ( index ) {
    case e_Items:
        (m_object = new CItem_Set())->AddReference();
        break;
    case e_ItemCount:
        m_ItemCount = 0;
        break;
    default:
        break;
    }
    m_choice = index;
}

const char* const CQuery_Related_Base::C_Items::sm_SelectionNames[] = {
    "not set",
    "items",
    "itemCount"
};

NCBI_NS_STD::string CQuery_Related_Base::C_Items::SelectionName(E_Choice index)
{
    return NCBI_NS_NCBI::CInvalidChoiceSelection::GetName(index, sm_SelectionNames, sizeof(sm_SelectionNames)/sizeof(sm_SelectionNames[0]));
}

void CQuery_Related_Base::C_Items::ThrowInvalidSelection(E_Choice index) const
{
    throw NCBI_NS_NCBI::CInvalidChoiceSelection(DIAG_COMPILE_INFO,m_choice, index, sm_SelectionNames, sizeof(sm_SelectionNames)/sizeof(sm_SelectionNames[0]));
}

const CItem_Set& CQuery_Related_Base::C_Items::GetItems(void) const
{
    CheckSelected(e_Items);
    return *static_cast<const TItems*>(m_object);
}

CItem_Set& CQuery_Related_Base::C_Items::SetItems(void)
{
    Select(e_Items, NCBI_NS_NCBI::eDoNotResetVariant);
    return *static_cast<TItems*>(m_object);
}

void CQuery_Related_Base::C_Items::SetItems(CItem_Set& value)
{
    TItems* ptr = &value;
    if ( m_choice != e_Items || m_object != ptr ) {
        Reset();
        (m_object = ptr)->AddReference();
        m_choice = e_Items;
    }
}

// helper methods

// type info
BEGIN_NAMED_CHOICE_INFO("", CQuery_Related_Base::C_Items)
{
    ADD_NAMED_REF_CHOICE_VARIANT("items", m_object, CItem_Set);
    ADD_NAMED_STD_CHOICE_VARIANT("itemCount", m_ItemCount);
}
END_CHOICE_INFO

// constructor
CQuery_Related_Base::C_Items::C_Items(void)
    : m_choice(e_not_set)
{
}

// destructor
CQuery_Related_Base::C_Items::~C_Items(void)
{
    if ( m_choice != e_not_set )
        Reset();
}


void CQuery_Related_Base::ResetBase(void)
{
    (*m_Base).Reset();
}

void CQuery_Related_Base::SetBase(CQuery_Command& value)
{
    m_Base.Reset(&value);
}

void CQuery_Related_Base::ResetRelation(void)
{
    m_Relation.erase();
    m_set_State[0] &= ~0xc;
}

void CQuery_Related_Base::ResetDb(void)
{
    m_Db.erase();
    m_set_State[0] &= ~0x30;
}

void CQuery_Related_Base::ResetItems(void)
{
    (*m_Items).Reset();
}

void CQuery_Related_Base::SetItems(CQuery_Related_Base::C_Items& value)
{
    m_Items.Reset(&value);
}

void CQuery_Related_Base::Reset(void)
{
    ResetBase();
    ResetRelation();
    ResetDb();
    ResetItems();
}

BEGIN_NAMED_BASE_CLASS_INFO("Query-Related", CQuery_Related)
{
    SET_CLASS_MODULE("NCBI-Env");
    ADD_NAMED_REF_MEMBER("base", m_Base, CQuery_Command);
    ADD_NAMED_STD_MEMBER("relation", m_Relation)->SetSetFlag(MEMBER_PTR(m_set_State[0]));
    ADD_NAMED_STD_MEMBER("db", m_Db)->SetSetFlag(MEMBER_PTR(m_set_State[0]));
    ADD_NAMED_REF_MEMBER("items", m_Items, C_Items);
    info->RandomOrder();
}
END_CLASS_INFO

// constructor
CQuery_Related_Base::CQuery_Related_Base(void)
    : m_Base(new CQuery_Command()), m_Items(new C_Items())
{
    memset(m_set_State,0,sizeof(m_set_State));
}

// destructor
CQuery_Related_Base::~CQuery_Related_Base(void)
{
}



