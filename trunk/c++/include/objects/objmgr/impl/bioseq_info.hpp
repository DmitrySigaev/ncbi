#ifndef OBJECTS_OBJMGR_IMPL___BIOSEQ_INFO__HPP
#define OBJECTS_OBJMGR_IMPL___BIOSEQ_INFO__HPP

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

#include <objects/objmgr/seq_id_handle.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <corelib/ncbiobj.hpp>
#include <set>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CBioseq_Info::
//
//    Structure to keep bioseq's parent seq-entry along with the list
//    of seq-id synonyms for the bioseq.
//


// forward declaration
class CSeq_entry;
class CSeq_id_Handle;

class NCBI_XOBJMGR_EXPORT CBioseq_Info : public CObject
{
public:
    typedef set<CSeq_id_Handle> TSynonyms;

    // 'ctors
    CBioseq_Info(void);
    CBioseq_Info(CSeq_entry& entry);
    CBioseq_Info(const CBioseq_Info& info);
    virtual ~CBioseq_Info(void);

    CBioseq_Info& operator= (const CBioseq_Info& info);

    bool operator== (const CBioseq_Info& info);
    bool operator!= (const CBioseq_Info& info);
    bool operator<  (const CBioseq_Info& info);

    virtual void DebugDump(CDebugDumpContext ddc, unsigned int depth) const;

    CRef<CSeq_entry> m_Entry;
    TSynonyms        m_Synonyms;
};



/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.7  2003/02/05 17:57:41  dicuccio
 * Moved into include/objects/objmgr/impl to permit data loaders to be defined
 * outside of xobjmgr.
 *
 * Revision 1.6  2002/12/26 21:03:33  dicuccio
 * Added Win32 export specifier.  Moved file from src/objects/objmgr to
 * include/objects/objmgr.
 *
 * Revision 1.5  2002/07/08 20:51:01  grichenk
 * Moved log to the end of file
 * Replaced static mutex (in CScope, CDataSource) with the mutex
 * pool. Redesigned CDataSource data locking.
 *
 * Revision 1.4  2002/05/29 21:21:13  gouriano
 * added debug dump
 *
 * Revision 1.3  2002/05/06 03:28:46  vakatov
 * OM/OM1 renaming
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

#endif  /* OBJECTS_OBJMGR_IMPL___BIOSEQ_INFO__HPP */
