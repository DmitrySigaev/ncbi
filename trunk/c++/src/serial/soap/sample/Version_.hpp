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

/// @Version_.hpp
/// Data storage class.
///
/// This file was generated by application DATATOOL
/// using the following specifications:
/// 'samplesoap.dtd'.
///
/// ATTENTION:
///   Don't edit or commit this file into CVS as this file will
///   be overridden (by DATATOOL) without warning!

#ifndef VERSION_BASE_HPP
#define VERSION_BASE_HPP

// standard includes
#include <serial/serialbase.hpp>

// generated includes
#include <string>


// generated classes

/////////////////////////////////////////////////////////////////////////////
class CVersion_Base : public ncbi::CSerialObject
{
    typedef ncbi::CSerialObject Tparent;
public:
    // constructor
    CVersion_Base(void);
    // destructor
    virtual ~CVersion_Base(void);

    // type info
    DECLARE_INTERNAL_TYPE_INFO();

    // types
    typedef std::string TClientID;

    // getters
    // setters

    /// mandatory
    /// typedef std::string TClientID
    bool IsSetClientID(void) const;
    bool CanGetClientID(void) const;
    void ResetClientID(void);
    const TClientID& GetClientID(void) const;
    void SetClientID(const TClientID& value);
    TClientID& SetClientID(void);

    /// Reset the whole object
    virtual void Reset(void);


private:
    // Prohibit copy constructor and assignment operator
    CVersion_Base(const CVersion_Base&);
    CVersion_Base& operator=(const CVersion_Base&);

    // data
    Uint4 m_set_State[1];
    TClientID m_ClientID;
};






///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////
inline
bool CVersion_Base::IsSetClientID(void) const
{
    return ((m_set_State[0] & 0x3) != 0);
}

inline
bool CVersion_Base::CanGetClientID(void) const
{
    return IsSetClientID();
}

inline
const std::string& CVersion_Base::GetClientID(void) const
{
    if (!CanGetClientID()) {
        ThrowUnassigned(0);
    }
    return m_ClientID;
}

inline
void CVersion_Base::SetClientID(const std::string& value)
{
    m_ClientID = value;
    m_set_State[0] |= 0x3;
}

inline
std::string& CVersion_Base::SetClientID(void)
{
#ifdef _DEBUG
    if (!IsSetClientID()) {
        m_ClientID = ms_UnassignedStr;
    }
#endif
    m_set_State[0] |= 0x1;
    return m_ClientID;
}

///////////////////////////////////////////////////////////
////////////////// end of inline methods //////////////////
///////////////////////////////////////////////////////////






#endif // VERSION_BASE_HPP
