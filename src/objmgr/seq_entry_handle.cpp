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
* Author: Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*    Handle to Seq-entry object
*
*/

#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/scope.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CBioseq_Handle CSeq_entry_Handle::GetBioseq(void) const
{
    _ASSERT( IsSeq() );
    CBioseq_Handle ret;
    CConstRef<CBioseq_Info> info(&m_Entry_Info->GetBioseq_Info());
    const CSeq_entry& entry = m_Entry_Info->GetSeq_entry();
    const CBioseq_Info::TSynonyms& syns = info->GetSynonyms();
    ITERATE(CBioseq_Info::TSynonyms, id, syns) {
        const CSeq_id& seq_id = *CSeq_id_Mapper::GetSeq_id(*id);
        ret = m_Scope->GetBioseqHandleFromTSE(seq_id, entry);
        if ( ret ) {
            break;
        }
    }
    return ret;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2003/11/28 15:12:31  grichenk
* Initial revision
*
*
* ===========================================================================
*/
