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
 *   using specifications from the ASN data definition file
 *   'twebenv.asn'.
 *
 * ATTENTION:
 *   Don't edit or check-in this file to the CVS as this file will
 *   be overridden (by DATATOOL) without warning!
 * ===========================================================================
 */

#ifndef WEB_ENV_BASE_HPP
#define WEB_ENV_BASE_HPP

// standard includes
#include <serial/serialbase.hpp>

// generated includes
#include <list>


// forward declarations
class CArgument;
class CDb_Env;
class CQuery_History;


// generated classes

class CWeb_Env_Base : public ncbi::CSerialObject
{
    typedef ncbi::CSerialObject Tparent;
public:
    // constructor
    CWeb_Env_Base(void);
    // destructor
    virtual ~CWeb_Env_Base(void);

    // type info
    DECLARE_INTERNAL_TYPE_INFO();

    // members' types
    typedef std::list< ncbi::CRef< CArgument > > TArguments;
    typedef std::list< ncbi::CRef< CDb_Env > > TDb_Env;
    typedef std::list< ncbi::CRef< CQuery_History > > TQueries;

    // members' getters
    // members' setters
    bool IsSetArguments(void) const;
    void ResetArguments(void);
    const std::list< ncbi::CRef< CArgument > >& GetArguments(void) const;
    std::list< ncbi::CRef< CArgument > >& SetArguments(void);

    bool IsSetDb_Env(void) const;
    void ResetDb_Env(void);
    const std::list< ncbi::CRef< CDb_Env > >& GetDb_Env(void) const;
    std::list< ncbi::CRef< CDb_Env > >& SetDb_Env(void);

    bool IsSetQueries(void) const;
    void ResetQueries(void);
    const std::list< ncbi::CRef< CQuery_History > >& GetQueries(void) const;
    std::list< ncbi::CRef< CQuery_History > >& SetQueries(void);

    // reset whole object
    virtual void Reset(void);

private:
    // Prohibit copy constructor and assignment operator
    CWeb_Env_Base(const CWeb_Env_Base&);
    CWeb_Env_Base& operator=(const CWeb_Env_Base&);

    // members' data
    TArguments m_Arguments;
    TDb_Env m_Db_Env;
    TQueries m_Queries;
};






///////////////////////////////////////////////////////////
///////////////////// inline methods //////////////////////
///////////////////////////////////////////////////////////
inline
bool CWeb_Env_Base::IsSetArguments(void) const
{
    return !(m_Arguments).empty();
}

inline
const CWeb_Env_Base::TArguments& CWeb_Env_Base::GetArguments(void) const
{
    return m_Arguments;
}

inline
CWeb_Env_Base::TArguments& CWeb_Env_Base::SetArguments(void)
{
    return m_Arguments;
}

inline
bool CWeb_Env_Base::IsSetDb_Env(void) const
{
    return !(m_Db_Env).empty();
}

inline
const CWeb_Env_Base::TDb_Env& CWeb_Env_Base::GetDb_Env(void) const
{
    return m_Db_Env;
}

inline
CWeb_Env_Base::TDb_Env& CWeb_Env_Base::SetDb_Env(void)
{
    return m_Db_Env;
}

inline
bool CWeb_Env_Base::IsSetQueries(void) const
{
    return !(m_Queries).empty();
}

inline
const CWeb_Env_Base::TQueries& CWeb_Env_Base::GetQueries(void) const
{
    return m_Queries;
}

inline
CWeb_Env_Base::TQueries& CWeb_Env_Base::SetQueries(void)
{
    return m_Queries;
}

///////////////////////////////////////////////////////////
////////////////// end of inline methods //////////////////
///////////////////////////////////////////////////////////






#endif // WEB_ENV_BASE_HPP
