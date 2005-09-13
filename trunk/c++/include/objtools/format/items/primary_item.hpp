#ifndef OBJTOOLS_FORMAT_ITEMS___PRIMARY_ITEM__HPP
#define OBJTOOLS_FORMAT_ITEMS___PRIMARY_ITEM__HPP

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
* Author:  Aaron Ucko, NCBI
*          Mati Shomrat
*
* File Description:
*   Primary item for flat-file generator
*
*/
#include <corelib/ncbistd.hpp>
#include <util/range.hpp>

#include <list>
#include <map>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objtools/alnmgr/alnmap.hpp>
#include <objtools/format/items/item_base.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CBioseqContext;
class IFormatter;


///////////////////////////////////////////////////////////////////////////
//
// PRIMARY

class NCBI_FORMAT_EXPORT CPrimaryItem : public CFlatItem
{
public:
    CPrimaryItem(CBioseqContext& ctx);
    void Format(IFormatter& formatter, IFlatTextOStream& text_os) const;

    const string& GetString(void) const { return m_Str; }

private:
    // types
    typedef CConstRef<CDense_seg>                 TDenseRef;
    typedef list< CRef< CSeq_align > >            TAlnList;
    typedef multimap<CAlnMap::TRange,  TDenseRef> TDense_seg_Map;

    void x_GatherInfo(CBioseqContext& ctx);
    void x_GetStrForPrimary(CBioseqContext& ctx);
    void x_CollectSegments(TDense_seg_Map&, const TAlnList& aln_list);
    void x_CollectSegments(TDense_seg_Map&, const CSeq_align& aln);

    string m_Str;
};


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.5  2005/09/13 17:16:38  jcherry
* Added export specifiers
*
* Revision 1.4  2004/08/30 15:06:56  shomrat
* use multimap for denseseg map
*
* Revision 1.3  2004/04/22 15:38:48  shomrat
* New implementation of Primary item
*
* Revision 1.2  2004/02/12 20:21:38  shomrat
* added include directive
*
* Revision 1.1  2003/12/17 19:49:08  shomrat
* Initial revision (adapted from flat lib)
*
*
* ===========================================================================
*/


#endif  /* OBJTOOLS_FORMAT_ITEMS___PRIMARY_ITEM__HPP */
