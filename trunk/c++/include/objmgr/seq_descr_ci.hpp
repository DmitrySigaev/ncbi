#ifndef DESC_CI__HPP
#define DESC_CI__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman
*
* File Description:
*   Object manager iterators
*
*/

#include <objects/seq/Seq_descr.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/** @addtogroup ObjectManagerIterators
 *
 * @{
 */


class CBioseq_Handle;

/////////////////////////////////////////////////////////////////////////////
///
///  CSeq_descr_CI --
///
///  Enumerate CSeq_descr objects from a Bioseq or Seq-entry handle
///

class NCBI_XOBJMGR_EXPORT CSeq_descr_CI
{
public:
    /// Create an empty iterator
    CSeq_descr_CI(void);

    /// Create an iterator that enumerates CSeq_descr objects 
    /// from a bioseq with limit number of seq-entries
    /// to "search_depth" (0 = unlimited).
    explicit CSeq_descr_CI(const CBioseq_Handle& handle,
                           size_t search_depth = 0);

    /// Create an iterator that enumerates CSeq_descr objects 
    /// from a seq-entry, limit number of seq-entries
    /// to "search_depth" (0 = unlimited).
    explicit CSeq_descr_CI(const CSeq_entry_Handle& entry,
                           size_t search_depth = 0);

    CSeq_descr_CI(const CSeq_descr_CI& iter);
    ~CSeq_descr_CI(void);

    CSeq_descr_CI& operator= (const CSeq_descr_CI& iter);

    /// Move to the next object in iterated sequence
    CSeq_descr_CI& operator++ (void);

    /// Check if iterator points to an object
    operator bool (void) const;

    const CSeq_descr& operator*  (void) const;
    const CSeq_descr* operator-> (void) const;

    CSeq_entry_Handle GetSeq_entry_Handle(void) const;

private:
    // Move to the next entry containing a descriptor
    void x_Next(void);
    void x_Step(void);
    void x_Settle(void);

    CSeq_entry_Handle     m_CurrentEntry;
    size_t                m_ParentLimit;
};


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.13  2004/10/07 14:03:32  vasilche
* Use shared among TSEs CTSE_Split_Info.
* Use typedefs and methods for TSE and DataSource locking.
* Load split CSeqdesc on the fly in CSeqdesc_CI.
*
* Revision 1.12  2004/10/01 15:22:28  kononenk
* Added doxygen formatting
*
* Revision 1.11  2004/03/16 15:47:26  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.10  2004/02/09 19:18:49  grichenk
* Renamed CDesc_CI to CSeq_descr_CI. Redesigned CSeq_descr_CI
* and CSeqdesc_CI to avoid using data directly.
*
* Revision 1.9  2003/06/02 16:01:36  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.8  2002/12/26 20:42:55  dicuccio
* Added Win32 export specifier.  Removed unimplemented (private) operator++(int)
*
* Revision 1.7  2002/12/05 19:28:29  grichenk
* Prohibited postfix operator ++()
*
* Revision 1.6  2002/07/08 20:50:56  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.5  2002/05/06 03:30:35  vakatov
* OM/OM1 renaming
*
* Revision 1.4  2002/02/21 19:27:00  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.3  2002/01/23 21:59:29  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.2  2002/01/16 16:26:37  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:04:01  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#endif  // DESC_CI__HPP
