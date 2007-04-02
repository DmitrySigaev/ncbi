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

/// @file soap_fault_.hpp
/// Data storage class.
///
/// This file was generated by application DATATOOL
/// using the following specifications:
/// 'soap_11.xsd'.
///
/// ATTENTION:
///   Don't edit or commit this file into CVS as this file will
///   be overridden (by DATATOOL) without warning!

#ifndef SOAP_FAULT_BASE_HPP
#define SOAP_FAULT_BASE_HPP

// standard includes
#include <serial/serialbase.hpp>

// generated includes
#include <string>

BEGIN_NCBI_SCOPE

// forward declarations
class CSoapDetail;


// generated classes

/////////////////////////////////////////////////////////////////////////////
class CSoapFault_Base : public ncbi::CSerialObject
{
    typedef ncbi::CSerialObject Tparent;
public:
    // constructor
    CSoapFault_Base(void);
    // destructor
    virtual ~CSoapFault_Base(void);

    // type info
    DECLARE_INTERNAL_TYPE_INFO();

    // types
    typedef std::string TFaultcode;
    typedef std::string TFaultstring;
    typedef std::string TFaultactor;
    typedef CSoapDetail TDetail;

    // getters
    // setters

    /// mandatory
    /// typedef std::string TFaultcode
    bool IsSetFaultcode(void) const;
    bool CanGetFaultcode(void) const;
    void ResetFaultcode(void);
    const TFaultcode& GetFaultcode(void) const;
    void SetFaultcode(const TFaultcode& value);
    TFaultcode& SetFaultcode(void);

    /// mandatory
    /// typedef std::string TFaultstring
    bool IsSetFaultstring(void) const;
    bool CanGetFaultstring(void) const;
    void ResetFaultstring(void);
    const TFaultstring& GetFaultstring(void) const;
    void SetFaultstring(const TFaultstring& value);
    TFaultstring& SetFaultstring(void);

    /// optional
    /// typedef std::string TFaultactor
    bool IsSetFaultactor(void) const;
    bool CanGetFaultactor(void) const;
    void ResetFaultactor(void);
    const TFaultactor& GetFaultactor(void) const;
    void SetFaultactor(const TFaultactor& value);
    TFaultactor& SetFaultactor(void);

    /// optional
    /// typedef CSoapDetail TDetail
    bool IsSetDetail(void) const;
    bool CanGetDetail(void) const;
    void ResetDetail(void);
    const TDetail& GetDetail(void) const;
    void SetDetail(TDetail& value);
    TDetail& SetDetail(void);

    /// Reset the whole object
    virtual void Reset(void);


private:
    // Prohibit copy constructor and assignment operator
    CSoapFault_Base(const CSoapFault_Base&);
    CSoapFault_Base& operator=(const CSoapFault_Base&);

    // data
    Uint4 m_set_State[1];
    TFaultcode m_Faultcode;
    TFaultstring m_Faultstring;
    TFaultactor m_Faultactor;
    ncbi::CRef< TDetail > m_Detail;
};






///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////
inline
bool CSoapFault_Base::IsSetFaultcode(void) const
{
    return ((m_set_State[0] & 0x3) != 0);
}

inline
bool CSoapFault_Base::CanGetFaultcode(void) const
{
    return IsSetFaultcode();
}

inline
const CSoapFault_Base::TFaultcode& CSoapFault_Base::GetFaultcode(void) const
{
    if (!CanGetFaultcode()) {
        ThrowUnassigned(0);
    }
    return m_Faultcode;
}

inline
void CSoapFault_Base::SetFaultcode(const CSoapFault_Base::TFaultcode& value)
{
    m_Faultcode = value;
    m_set_State[0] |= 0x3;
}

inline
CSoapFault_Base::TFaultcode& CSoapFault_Base::SetFaultcode(void)
{
#ifdef _DEBUG
    if (!IsSetFaultcode()) {
        m_Faultcode = ms_UnassignedStr;
    }
#endif
    m_set_State[0] |= 0x1;
    return m_Faultcode;
}

inline
bool CSoapFault_Base::IsSetFaultstring(void) const
{
    return ((m_set_State[0] & 0xc) != 0);
}

inline
bool CSoapFault_Base::CanGetFaultstring(void) const
{
    return IsSetFaultstring();
}

inline
const CSoapFault_Base::TFaultstring& CSoapFault_Base::GetFaultstring(void) const
{
    if (!CanGetFaultstring()) {
        ThrowUnassigned(1);
    }
    return m_Faultstring;
}

inline
void CSoapFault_Base::SetFaultstring(const CSoapFault_Base::TFaultstring& value)
{
    m_Faultstring = value;
    m_set_State[0] |= 0xc;
}

inline
CSoapFault_Base::TFaultstring& CSoapFault_Base::SetFaultstring(void)
{
#ifdef _DEBUG
    if (!IsSetFaultstring()) {
        m_Faultstring = ms_UnassignedStr;
    }
#endif
    m_set_State[0] |= 0x4;
    return m_Faultstring;
}

inline
bool CSoapFault_Base::IsSetFaultactor(void) const
{
    return ((m_set_State[0] & 0x30) != 0);
}

inline
bool CSoapFault_Base::CanGetFaultactor(void) const
{
    return IsSetFaultactor();
}

inline
const CSoapFault_Base::TFaultactor& CSoapFault_Base::GetFaultactor(void) const
{
    if (!CanGetFaultactor()) {
        ThrowUnassigned(2);
    }
    return m_Faultactor;
}

inline
void CSoapFault_Base::SetFaultactor(const CSoapFault_Base::TFaultactor& value)
{
    m_Faultactor = value;
    m_set_State[0] |= 0x30;
}

inline
CSoapFault_Base::TFaultactor& CSoapFault_Base::SetFaultactor(void)
{
#ifdef _DEBUG
    if (!IsSetFaultactor()) {
        m_Faultactor = ms_UnassignedStr;
    }
#endif
    m_set_State[0] |= 0x10;
    return m_Faultactor;
}

inline
bool CSoapFault_Base::IsSetDetail(void) const
{
    return m_Detail.NotEmpty();
}

inline
bool CSoapFault_Base::CanGetDetail(void) const
{
    return IsSetDetail();
}

inline
const CSoapFault_Base::TDetail& CSoapFault_Base::GetDetail(void) const
{
    if (!CanGetDetail()) {
        ThrowUnassigned(3);
    }
    return (*m_Detail);
}

///////////////////////////////////////////////////////////
////////////////// end of inline methods //////////////////
///////////////////////////////////////////////////////////




END_NCBI_SCOPE


#endif // SOAP_FAULT_BASE_HPP
