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
 */

/// @file Query_Select_.hpp
/// Data storage class.
///
/// This file was generated by application DATATOOL
/// using the following specifications:
/// 'twebenv.asn'.
///
/// ATTENTION:
///   Don't edit or commit this file into CVS as this file will
///   be overridden (by DATATOOL) without warning!

#ifndef QUERY_SELECT_BASE_HPP
#define QUERY_SELECT_BASE_HPP

// standard includes
#include <serial/serialbase.hpp>

// generated includes
#include <string>


// forward declarations
class CItem_Set;


// generated classes

/////////////////////////////////////////////////////////////////////////////
class CQuery_Select_Base : public ncbi::CSerialObject
{
    typedef ncbi::CSerialObject Tparent;
public:
    // constructor
    CQuery_Select_Base(void);
    // destructor
    virtual ~CQuery_Select_Base(void);

    // type info
    DECLARE_INTERNAL_TYPE_INFO();

    // types
    typedef std::string TDb;
    typedef CItem_Set TItems;

    // getters
    // setters

    /// mandatory
    /// typedef std::string TDb
    bool IsSetDb(void) const;
    bool CanGetDb(void) const;
    void ResetDb(void);
    const TDb& GetDb(void) const;
    void SetDb(const TDb& value);
    TDb& SetDb(void);

    /// mandatory
    /// typedef CItem_Set TItems
    bool IsSetItems(void) const;
    bool CanGetItems(void) const;
    void ResetItems(void);
    const TItems& GetItems(void) const;
    void SetItems(TItems& value);
    TItems& SetItems(void);

    /// Reset the whole object
    virtual void Reset(void);


private:
    // Prohibit copy constructor and assignment operator
    CQuery_Select_Base(const CQuery_Select_Base&);
    CQuery_Select_Base& operator=(const CQuery_Select_Base&);

    // data
    Uint4 m_set_State[1];
    TDb m_Db;
    ncbi::CRef< TItems > m_Items;
};






///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////
inline
bool CQuery_Select_Base::IsSetDb(void) const
{
    return ((m_set_State[0] & 0x3) != 0);
}

inline
bool CQuery_Select_Base::CanGetDb(void) const
{
    return IsSetDb();
}

inline
const std::string& CQuery_Select_Base::GetDb(void) const
{
    if (!CanGetDb()) {
        ThrowUnassigned(0);
    }
    return m_Db;
}

inline
void CQuery_Select_Base::SetDb(const std::string& value)
{
    m_Db = value;
    m_set_State[0] |= 0x3;
}

inline
std::string& CQuery_Select_Base::SetDb(void)
{
#ifdef _DEBUG
    if (!IsSetDb()) {
        m_Db = ms_UnassignedStr;
    }
#endif
    m_set_State[0] |= 0x1;
    return m_Db;
}

inline
bool CQuery_Select_Base::IsSetItems(void) const
{
    return m_Items.NotEmpty();
}

inline
bool CQuery_Select_Base::CanGetItems(void) const
{
    return true;
}

inline
const CItem_Set& CQuery_Select_Base::GetItems(void) const
{
    if ( !m_Items ) {
        const_cast<CQuery_Select_Base*>(this)->ResetItems();
    }
    return (*m_Items);
}

inline
CItem_Set& CQuery_Select_Base::SetItems(void)
{
    if ( !m_Items ) {
        ResetItems();
    }
    return (*m_Items);
}

///////////////////////////////////////////////////////////
////////////////// end of inline methods //////////////////
///////////////////////////////////////////////////////////






#endif // QUERY_SELECT_BASE_HPP
