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
*   Bioseq info for data source
*
*/


#include <objects/objmgr/impl/bioseq_info.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CBioseq_Info::
//
//    Structure to keep bioseq's parent seq-entry along with the list
//    of seq-id synonyms for the bioseq.
//


CBioseq_Info::CBioseq_Info(void)
    : m_Entry(0)
{
    return;
}


CBioseq_Info::CBioseq_Info(CSeq_entry& entry)
    : m_Entry(&entry)
{
    return;
}


CBioseq_Info::CBioseq_Info(const CBioseq_Info& info)
{
    if ( &info != this )
        *this = info;
}


CBioseq_Info::~CBioseq_Info(void)
{
    return;
}


CBioseq_Info& CBioseq_Info::operator= (const CBioseq_Info& info)
{
    m_Entry.Reset(const_cast<CBioseq_Info&>(info).m_Entry);
    ITERATE ( TSynonyms, it, info.m_Synonyms ) {
        m_Synonyms.insert(*it);
    }
    return *this;
}

void CBioseq_Info::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
    ddc.SetFrame("CBioseq_Info");
    CObject::DebugDump( ddc, depth);

    ddc.Log("m_Entry", m_Entry.GetPointer(),0);
    if (depth == 0) {
        DebugDumpValue(ddc, "m_Synonyms.size()", m_Synonyms.size());
    } else {
        DebugDumpValue(ddc, "m_Synonyms.type",
            "set<CSeq_id_Handle>");
        CDebugDumpContext ddc2(ddc,"m_Synonyms");
        TSynonyms::const_iterator it;
        int n;
        for (n=0, it=m_Synonyms.begin(); it!=m_Synonyms.end(); ++n, ++it) {
            string member_name = "m_Synonyms[ " +
                NStr::IntToString(n) +" ]";
            ddc2.Log(member_name, (*it).AsString());
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.8  2003/03/11 15:51:06  kuznets
* iterate -> ITERATE
*
* Revision 1.7  2003/02/05 17:59:17  dicuccio
* Moved formerly private headers into include/objects/objmgr/impl
*
* Revision 1.6  2002/12/26 20:55:17  dicuccio
* Moved seq_id_mapper.hpp, tse_info.hpp, and bioseq_info.hpp -> include/ tree
*
* Revision 1.5  2002/11/04 21:29:12  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.4  2002/07/08 20:51:01  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.3  2002/05/29 21:21:13  gouriano
* added debug dump
*
* Revision 1.2  2002/02/21 19:27:05  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.1  2002/02/07 21:25:05  grichenk
* Initial revision
*
*
* ===========================================================================
*/
