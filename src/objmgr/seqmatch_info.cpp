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
* Author: Aleksey Grichenko
*
* File Description:
*   CSeqMatch_Info -- used internally by CScope and CDataSource
*
*/


#include <objects/objmgr/seqmatch_info.hpp>

#include "data_source.hpp"
#include "tse_info.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CSeqMatch_Info::CSeqMatch_Info(void)
    : m_TSE(0), m_DataSource(0)
{
}


CSeqMatch_Info::CSeqMatch_Info(const CSeq_id_Handle& h,
                               CTSE_Info& tse,
                               CDataSource& ds)
    : m_Handle(h), m_TSE(&tse), m_DataSource(&ds)
{
}


CSeqMatch_Info::CSeqMatch_Info(const CSeqMatch_Info& info)
    : m_Handle(info.m_Handle),
      m_TSE(info.m_TSE),
      m_DataSource(info.m_DataSource)
{
}


CSeqMatch_Info&
CSeqMatch_Info::operator= (const CSeqMatch_Info& info)
{
    if (&info != this) {
        m_Handle = info.m_Handle;
        m_TSE = info.m_TSE;
        m_DataSource = info.m_DataSource;
    }
    return *this;
}


bool CSeqMatch_Info::operator< (const CSeqMatch_Info& info) const
{
    if (m_TSE->m_Dead  &&  !info.m_TSE->m_Dead)
        return false; // info is better;
    return true;
}


CSeqMatch_Info::operator bool (void)
{
    return bool(m_Handle);
}


bool CSeqMatch_Info::operator! (void)
{
    return !m_Handle;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  2002/12/26 16:39:24  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.4  2002/07/08 20:51:02  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.3  2002/05/06 03:28:48  vakatov
* OM/OM1 renaming
*
* Revision 1.2  2002/02/25 21:05:29  grichenk
* Removed seq-data references caching. Increased MT-safety. Fixed typos.
*
* Revision 1.1  2002/02/21 19:21:04  grichenk
* Initial revision
*
*
* ===========================================================================
*/
