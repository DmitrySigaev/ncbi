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
*   Object manager iterators
*
*/

#include <objmgr/feat_ci.hpp>
#include <objmgr/impl/annot_object.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/general/Dbtag.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


const CSeq_feat& CMappedFeat::x_MakeMappedFeature(void) const
{
    if (!m_MappedFeat) {
        if ( bool(m_MappedLoc)  ||  bool(m_MappedProd) ) {
            CSeq_feat& dst = *new CSeq_feat;
            m_MappedFeat = &dst;
            CSeq_feat& src = const_cast<CSeq_feat&>(*m_Feat);
            if ( src.IsSetId() )
                dst.SetId(src.SetId());
            dst.SetData(src.SetData());
            if ( src.IsSetPartial() )
                dst.SetPartial(src.GetPartial());
            if ( src.IsSetExcept() )
                dst.SetExcept(src.GetExcept());
            if ( src.IsSetComment() )
                dst.SetComment(src.GetComment());
            if ( src.IsSetProduct() )
                dst.SetProduct(m_MappedProd?
                               const_cast<CSeq_loc&>(*m_MappedProd):
                               src.SetProduct());
            dst.SetLocation(m_MappedLoc?
                            const_cast<CSeq_loc&>(*m_MappedLoc):
                            src.SetLocation());
            if ( src.IsSetQual() )
                dst.SetQual() = src.SetQual();
            if ( src.IsSetTitle() )
                dst.SetTitle(src.GetTitle());
            if ( src.IsSetExt() )
                dst.SetExt(src.SetExt());
            if ( src.IsSetCit() )
                dst.SetCit(src.SetCit());
            if ( src.IsSetExp_ev() )
                dst.SetExp_ev(src.GetExp_ev());
            if ( src.IsSetXref() )
                dst.SetXref() = src.SetXref();
            if ( src.IsSetDbxref() )
                dst.SetDbxref() = src.SetDbxref();
            if ( src.IsSetPseudo() )
                dst.SetPseudo(src.GetPseudo());
            if ( src.IsSetExcept_text() )
                dst.SetExcept_text(src.GetExcept_text());
        }
        else {
            m_MappedFeat = m_Feat;
        }
    }
    return *m_MappedFeat;
}


CMappedFeat& CMappedFeat::Set(const CAnnotObject_Ref& annot)
{
    _ASSERT(annot.Get().IsFeat());
    m_Feat = &annot.Get().GetFeat();
    m_MappedFeat.Reset();
    m_Partial = annot.IsPartial();
    if ( annot.IsMappedLoc() )
        m_MappedLoc = &annot.GetMappedLoc();
    else
        m_MappedLoc.Reset();
    if ( annot.IsMappedProd() )
        m_MappedProd = &annot.GetMappedProd();
    else
        m_MappedProd.Reset();
    return *this;
}


const CMappedFeat& CFeat_CI::operator* (void) const
{
    return m_Feat.Set(Get());
}


const CMappedFeat* CFeat_CI::operator-> (void) const
{
    m_Feat.Set(Get());
    return &m_Feat;
}

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.18  2003/06/02 16:06:37  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.17  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.16  2003/02/24 21:35:22  vasilche
* Reduce checks in CAnnotObject_Ref comparison.
* Fixed compilation errors on MS Windows.
* Removed obsolete file src/objects/objmgr/annot_object.hpp.
*
* Revision 1.15  2003/02/24 18:57:22  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.14  2003/02/13 14:34:34  grichenk
* Renamed CAnnotObject -> CAnnotObject_Info
* + CSeq_annot_Info and CAnnotObject_Ref
* Moved some members of CAnnotObject to CSeq_annot_Info
* and CAnnotObject_Ref.
* Added feat/align/graph to CAnnotObject_Info map
* to CDataSource.
*
* Revision 1.13  2003/02/10 15:50:45  grichenk
* + CMappedFeat, CFeat_CI resolves to CMappedFeat
*
* Revision 1.12  2002/12/20 20:54:24  grichenk
* Added optional location/product switch to CFeat_CI
*
* Revision 1.11  2002/12/06 15:36:00  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.10  2002/11/04 21:29:12  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.9  2002/07/08 20:51:01  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.8  2002/05/06 03:28:47  vakatov
* OM/OM1 renaming
*
* Revision 1.7  2002/05/03 21:28:09  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.6  2002/04/05 21:26:19  grichenk
* Enabled iteration over annotations defined on segments of a
* delta-sequence.
*
* Revision 1.5  2002/03/05 16:08:14  grichenk
* Moved TSE-restriction to new constructors
*
* Revision 1.4  2002/03/04 15:07:48  grichenk
* Added "bioseq" argument to CAnnotTypes_CI constructor to iterate
* annotations from a single TSE.
*
* Revision 1.3  2002/02/21 19:27:05  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.2  2002/01/16 16:25:57  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:18  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/
