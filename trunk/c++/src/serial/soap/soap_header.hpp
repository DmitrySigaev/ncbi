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
 *   Holds SOAP message Header contents
 *
 * ===========================================================================
 */

#ifndef SOAP_HEADER_HPP
#define SOAP_HEADER_HPP

// standard includes
#include <serial/serialbase.hpp>

// generated includes
#include <list>


BEGIN_NCBI_SCOPE
// generated classes

class CSoapHeader : public CSerialObject
{
    typedef CSerialObject Tparent;
public:
    // constructor
    CSoapHeader(void);
    // destructor
    virtual ~CSoapHeader(void);

    // type info
    DECLARE_INTERNAL_TYPE_INFO();

    class C_E : public CSerialObject
    {
        typedef CSerialObject Tparent;
    public:
        // constructor
        C_E(void);
        // destructor
        ~C_E(void);
    
        // type info
        DECLARE_INTERNAL_TYPE_INFO();
    
        // types
        typedef CAnyContentObject TAnyContent;
    
        // getters
        // setters
    
        // mandatory
        // typedef CAnyContentObject TAnyContent
        bool IsSetAnyContent(void) const;
        bool CanGetAnyContent(void) const;
        void ResetAnyContent(void);
        const TAnyContent& GetAnyContent(void) const;
        void SetAnyContent(TAnyContent& value);
        TAnyContent& SetAnyContent(void);
    
        // reset whole object
        void Reset(void);
    
    
    private:
        // Prohibit copy constructor and assignment operator
        C_E(const C_E&);
        C_E& operator=(const C_E&);
    
        // data
        CRef< TAnyContent > m_AnyContent;
    };
    // types
    typedef std::list< CRef< C_E > > Tdata;

    // getters
    // setters

    // mandatory
    // typedef std::list< CRef< C_E > > Tdata
    bool IsSet(void) const;
    bool CanGet(void) const;
    void Reset(void);
    const Tdata& Get(void) const;
    Tdata& Set(void);
    operator const Tdata& (void) const;
    operator Tdata& (void);


private:
    // Prohibit copy constructor and assignment operator
    CSoapHeader(const CSoapHeader&);
    CSoapHeader& operator=(const CSoapHeader&);

    // data
    Uint4 m_set_State[1];
    Tdata m_data;
};


///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////
inline
bool CSoapHeader::C_E::IsSetAnyContent(void) const
{
    return m_AnyContent.NotEmpty();
}

inline
bool CSoapHeader::C_E::CanGetAnyContent(void) const
{
    return IsSetAnyContent();
}

inline
const CAnyContentObject& CSoapHeader::C_E::GetAnyContent(void) const
{
    if (!CanGetAnyContent()) {
        ThrowUnassigned(0);
    }
    return (*m_AnyContent);
}

inline
CAnyContentObject& CSoapHeader::C_E::SetAnyContent(void)
{
    return (*m_AnyContent);
}

inline
bool CSoapHeader::IsSet(void) const
{
    return ((m_set_State[0] & 0x3) != 0);
}

inline
bool CSoapHeader::CanGet(void) const
{
    return true;
}

inline
const std::list< CRef< CSoapHeader::C_E > >& CSoapHeader::Get(void) const
{
    return m_data;
}

inline
std::list< CRef< CSoapHeader::C_E > >& CSoapHeader::Set(void)
{
    m_set_State[0] |= 0x1;
    return m_data;
}

inline
CSoapHeader::operator const std::list< CRef< CSoapHeader::C_E > >& (void) const
{
    return m_data;
}

inline
CSoapHeader::operator std::list< CRef< CSoapHeader::C_E > >& (void)
{
    m_set_State[0] |= 0x1;
    return m_data;
}

///////////////////////////////////////////////////////////
////////////////// end of inline methods //////////////////
///////////////////////////////////////////////////////////
END_NCBI_SCOPE

#endif // SOAP_HEADER_HPP

/* --------------------------------------------------------------------------
* $Log$
* Revision 1.3  2005/01/12 17:00:09  vasilche
* Avoid performance warning on MSVC.
*
* Revision 1.2  2003/09/25 19:45:33  gouriano
* Added soap Fault object
*
*
* ===========================================================================
*/
