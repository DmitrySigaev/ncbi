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
*
* File Description:
*   
*
* ===========================================================================
*/
#include <corelib/ncbistd.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seq/Seq_hist_rec.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seqfeat/BioSource.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seq_map.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/sequence.hpp>

#include <algorithm>

#include <objtools/format/flat_file_generator.hpp>
#include <objtools/format/item_ostream.hpp>
#include <objtools/format/flat_expt.hpp>
#include <objtools/format/items/locus_item.hpp>
#include <objtools/format/items/defline_item.hpp>
#include <objtools/format/items/accession_item.hpp>
#include <objtools/format/items/version_item.hpp>
#include <objtools/format/items/keywords_item.hpp>
#include <objtools/format/items/source_item.hpp>
#include <objtools/format/items/reference_item.hpp>
#include <objtools/format/items/comment_item.hpp>
#include <objtools/format/items/basecount_item.hpp>
#include <objtools/format/items/sequence_item.hpp>
#include <objtools/format/items/feature_item.hpp>
#include <objtools/format/items/segment_item.hpp>
#include <objtools/format/items/ctrl_items.hpp>
#include <objtools/format/gather_items.hpp>
#include <objtools/format/genbank_gather.hpp>
#include <objtools/format/embl_gather.hpp>
#include <objtools/format/gff_gather.hpp>
#include <objtools/format/context.hpp>
#include "utils.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
USING_SCOPE(sequence);


/////////////////////////////////////////////////////////////////////////////
//
// Public:

// "virtual constructor"
CFlatGatherer* CFlatGatherer::New(TFormat format)
{
    switch ( format ) {
    case eFormat_GenBank:
        //case CFlatFileGenerator<>::eFormat_GBSeq:
        //case CFlatFileGenerator<>::eFormat_Index:
        return new CGenbankGatherer;
        
    case eFormat_EMBL:
        return new CEmblGatherer;

    case eFormat_GFF:
        return new CGFFGatherer;
        
    case eFormat_DDBJ:
    case eFormat_GBSeq:
    case eFormat_FTable:
    default:
        NCBI_THROW(CFlatException, eNotSupported, 
            "This format is currently not supported");
        break;
    }

    return 0;
}


void CFlatGatherer::Gather(CFFContext& ctx, CFlatItemOStream& os) const
{
    m_ItemOS.Reset(&os);
    m_Context.Reset(&ctx);

    os << new CStartItem(ctx);
    x_GatherSeqEntry(ctx.GetTSE());
    os << new CEndItem(ctx);
}


CFlatGatherer::~CFlatGatherer(void)
{
}







/////////////////////////////////////////////////////////////////////////////
//
// Protected:


void CFlatGatherer::x_GatherHeader(CFFContext& ctx, CFlatItemOStream& item_os) const
{
    //item_os << new CLocusItem(ctx);
    //item_os << new CKeywordsItem(ctx);
}


/////////////////////////////////////////////////////////////////////////////
//
// Private:

void CFlatGatherer::x_GatherSeqEntry(const CSeq_entry& entry) const
{
    if ( entry.IsSet() ) {
        CBioseq_set::TClass clss = entry.GetSet().GetClass();
        if ( clss == CBioseq_set::eClass_genbank  ||
             clss == CBioseq_set::eClass_mut_set  ||
             clss == CBioseq_set::eClass_pop_set  ||
             clss == CBioseq_set::eClass_phy_set  ||
             clss == CBioseq_set::eClass_eco_set  ||
             clss == CBioseq_set::eClass_wgs_set  ||
             clss == CBioseq_set::eClass_gen_prod_set ) {
            ITERATE(CBioseq_set::TSeq_set, iter, entry.GetSet().GetSeq_set()) {
                x_GatherSeqEntry(**iter);
            }
            return;
        }
    }

    // visit each bioseq in the entry (excluding segments)
    CBioseq_CI seq_iter(m_Context->GetScope(),
        entry,
        CSeq_inst::eMol_not_set,
        CBioseq_CI::eLevel_Mains);
    for ( ; seq_iter; ++seq_iter ) {
        x_GatherBioseq(seq_iter->GetBioseq());
    }
} 


// a defualt implementation for GenBank /  DDBJ formats
void CFlatGatherer::x_GatherBioseq(const CBioseq& seq) const
{
    if ( !seq.CanGetInst() ) {
        return;
    }

    bool segmented = seq.GetInst().CanGetRepr()  &&
        seq.GetInst().GetRepr() == CSeq_inst::eRepr_seg;
    if ( segmented  && 
         (m_Context->IsStyleNormal()  ||  m_Context->IsStyleSegment()) ) {
        // display individual segments (multiple sections)
        x_DoMultipleSections(seq);
    } else {
        // display as a single bioseq (single section)
        x_DoSingleSection(seq);
    }   
}


void CFlatGatherer::x_DoMultipleSections(const CBioseq& seq) const
{
    m_Context->SetMasterBioseq(seq);

    // we need to remove the const because of the SeqMap creation interface.
    CBioseq& non_const_seq = const_cast<CBioseq&>(seq);

    CConstRef<CSeqMap> seqmap = CSeqMap::CreateSeqMapForBioseq(non_const_seq);
    SSeqMapSelector selector;
    selector.SetResolveCount(1); // do not resolve segments

    CScope& scope = m_Context->GetScope();

    // iterate over the segments, gathering a segment at a time.
    ITERATE (CSeg_ext::Tdata, seg, seq.GetInst().GetExt().GetSeg().Get()) {
        // skip gaps
        if ( (*seg)->IsNull() ) {
            continue;
        }

        // !!! set the context location

        const CSeq_id& seg_id = sequence::GetId(**seg, &scope);
        CBioseq_Handle bsh = scope.GetBioseqHandle(seg_id);
        if ( bsh ) {
            x_DoSingleSection(bsh.GetBioseq());
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
//
// REFERENCES

bool s_FilterPubdesc(const CPubdesc& pubdesc, CFFContext& ctx)
{
    if ( pubdesc.CanGetComment() ) {
        const string& comment = pubdesc.GetComment();
        bool is_gene_rif = NStr::StartsWith(comment, "GeneRIF", NStr::eNocase);

        if ( (ctx.HideGeneRIFs()  &&  is_gene_rif)  ||
             ((ctx.OnlyGeneRIFs()  ||  ctx.LatestGeneRIFs())  &&  !is_gene_rif) ) {
            return true;
        }
    }

    return false;
}


void CFlatGatherer::x_GatherReferences(void) const
{
    CFFContext::TReferences refs = m_Context->SetReferences();

    // gather references from descriptors
    for (CSeqdesc_CI it(m_Context->GetHandle(), CSeqdesc::e_Pub); it; ++it) {
        const CPubdesc& pubdesc = it->GetPub();
        if ( s_FilterPubdesc(pubdesc, *m_Context) ) {
            continue;
        }
        
        refs.push_back(CFFContext::TRef(new CReferenceItem(*it, *m_Context)));
    }

    // gather references from features
    CFeat_CI it(m_Context->GetHandle(), 0, 0, CSeqFeatData::e_Pub);
    for ( ; it; ++it) {
        refs.push_back(CFFContext::TRef(new CReferenceItem(it->GetData().GetPub(),
                                        *m_Context, &it->GetLocation())));
    }
    CReferenceItem::Rearrange(refs, *m_Context);
    ITERATE (CFFContext::TReferences, it, refs) {
        *m_ItemOS << *it;
    }
}


/////////////////////////////////////////////////////////////////////////////
//
// COMMENTS


void CFlatGatherer::x_GatherComments(void) const
{
    CFFContext& ctx = *m_Context;

    // Gather comments related to the seq-id
    x_IdComments(ctx);
    x_RefSeqComments(ctx);

    if ( CCommentItem::NsAreGaps(ctx.GetActiveBioseq(), ctx) ) {
        x_AddComment(new CCommentItem(CCommentItem::kNsAreGaps, ctx));
    }

    x_HistoryComments(ctx);
    x_WGSComment(ctx);
    if ( ctx.ShowGBBSource() ) {
        x_GBBSourceComment(ctx);
    }
    x_DescComments(ctx);
    x_MaplocComments(ctx);
    x_RegionComments(ctx);
    x_HTGSComments(ctx);
    x_FeatComments(ctx);

    x_FlushComments();
}


void CFlatGatherer::x_AddComment(CCommentItem* comment) const
{
    CRef<CCommentItem> com(comment);
    if ( !com->Skip() ) {
        if ( !m_Comments.empty() ) {
            m_Comments.push_back(com);
        } else {
            *m_ItemOS << com;
        }
    }
}


void CFlatGatherer::x_AddGSDBComment
(const CDbtag& dbtag,
 CFFContext& ctx) const
{
    CRef<CCommentItem> gsdb_comment(new CGsdbComment(dbtag, ctx));
    if ( !gsdb_comment->Skip() ) {
        m_Comments.push_back(gsdb_comment);
    }
}


void CFlatGatherer::x_FlushComments(void) const
{
    if ( m_Comments.empty() ) {
        return;
    }

    // the first one is GSDB comment
    if ( m_Comments.size() > 1 ) {
        // if there are subsequent comments, add period after GSDB id
        CRef<CGsdbComment> gsdb_comment(dynamic_cast<CGsdbComment*>(m_Comments.front().GetPointer()));
        if ( gsdb_comment ) {
            gsdb_comment->AddPeriod();
        }
    }

    ITERATE (vector< CRef<CCommentItem> >, it, m_Comments) {
        *m_ItemOS << *it;
    }

    m_Comments.clear();
}


string s_GetGenomeBuildNumber(const CBioseq_Handle& bsh)
{
    for (CSeqdesc_CI it(bsh, CSeqdesc::e_User);  it;  ++it) {
        const CUser_object& uo = it->GetUser();
        if ( uo.IsSetType()  &&  uo.GetType().IsStr()  &&
             uo.GetType().GetStr() == "GenomeBuild" ) {
            if ( uo.HasField("NcbiAnnotation") ) {
                const CUser_field& uf = uo.GetField("NcbiAnnotation");
                if ( uf.CanGetData()  &&  uf.GetData().IsStr()  &&
                     !uf.GetData().GetStr().empty() ) {
                    return uf.GetData().GetStr();
                }
            } else if ( uo.HasField("Annotation") ) {
                const CUser_field& uf = uo.GetField("Annotation");
                if ( uf.CanGetData()  &&  uf.GetData().IsStr()  &&
                     !uf.GetData().GetStr().empty() ) {
                    static const string prefix = "NCBI build ";
                    if ( NStr::StartsWith(uf.GetData().GetStr(), prefix) ) {
                        return uf.GetData().GetStr().substr(prefix.length());
                    }
                }
            }
        }
    }

    return kEmptyStr;
}


bool s_HasRefTrackStatus(const CBioseq_Handle& bsh) {
    for (CSeqdesc_CI it(bsh, CSeqdesc::e_User);  it;  ++it) {
        if ( !CCommentItem::GetStatusForRefTrack(it->GetUser()).empty() ) {
            return true;
        }
    }

    return false;
}


void CFlatGatherer::x_IdComments(CFFContext& ctx) const
{
    const CObject_id* local_id = 0;

    string genome_build_number = s_GetGenomeBuildNumber(ctx.GetHandle());
    bool has_ref_track_status = s_HasRefTrackStatus(ctx.GetHandle());

    ITERATE( CBioseq::TId, id_iter, ctx.GetActiveBioseq().GetId() ) {
        const CSeq_id& id = **id_iter;

        switch ( id.Which() ) {
        case CSeq_id::e_Other:
            {{
                if ( ctx.IsRSCompleteGenomic() ) {
                    if ( !genome_build_number.empty() ) {
                        x_AddComment(new CGenomeAnnotComment(ctx, genome_build_number));
                    }
                }
                if ( ctx.IsRSContig()  ||  ctx.IsRSIntermedWGS() ) {
                    if ( !has_ref_track_status ) {
                        x_AddComment(new CGenomeAnnotComment(ctx, genome_build_number));
                    }
                }
                if ( ctx.IsRSPredictedProtein()  ||
                     ctx.IsRSPredictedMRna()     ||
                     ctx.IsRSPredictedNCRna()    ||
                     ctx.IsRSWGSProt() ) {
                    SModelEvidance me;
                    if ( GetModelEvidance(ctx.GetHandle(), ctx.GetScope(), me) ) {
                        string str = CCommentItem::GetStringForModelEvidance(me);
                        if ( !str.empty() ) {
                            x_AddComment(new CCommentItem(str, ctx));
                        }
                    }
                }
            }}
            break;
        case CSeq_id::e_General:
            {{
                const CDbtag& dbtag = id.GetGeneral();
                if ( dbtag.CanGetDb()  &&  dbtag.GetDb() == "GSDB"  &&
                     dbtag.CanGetTag()  &&  dbtag.GetTag().IsId() ) {
                    x_AddGSDBComment(dbtag, ctx);
                }
            }}
            break;
        case CSeq_id::e_Local:
            {{
                local_id = &(id.GetLocal());
            }}
            break;
        default:
            break;
        }
    }

    if ( local_id != 0 ) {
        if ( ctx.IsTPA()  ||  ctx.IsGED() ) {
            if ( ctx.IsModeGBench()  ||  ctx.IsModeDump() ) {
                // !!! create a CLocalIdComment(*local_id, ctx)
            }
        }
    }
}


void CFlatGatherer::x_RefSeqComments(CFFContext& ctx) const
{
    bool did_tpa = false, did_ref_track = false, did_genome = false;

    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_User);  it;  ++it) {
        const CUser_object& uo = it->GetUser();

        // TPA
        {{
            if ( !did_tpa ) {
                string str = CCommentItem::GetStringForTPA(uo, ctx);
                if ( !str.empty() ) {
                    x_AddComment(new CCommentItem(str, ctx, &uo));
                    did_tpa = true;
                }
            }
        }}

        // BankIt
        {{
            if ( !ctx.HideBankItComment() ) {
                string str = CCommentItem::GetStringForBankIt(uo);
                if ( !str.empty() ) {
                    x_AddComment(new CCommentItem(str, ctx, &uo));
                }
            }
        }}

        // RefTrack
        {{
            if ( !did_ref_track ) {
                string str = CCommentItem::GetStatusForRefTrack(uo);
                if ( !str.empty() ) {
                    x_AddComment(new CCommentItem(str, ctx, &uo));
                }
            }
        }}

        // Genome
        {{
            if ( !did_genome ) {
                // !!! Not implememnted in the C version. should it be?
            }
        }}
    }
}


void CFlatGatherer::x_HistoryComments(CFFContext& ctx) const
{
    const CBioseq& seq = ctx.GetActiveBioseq();
    if ( !seq.GetInst().CanGetHist() ) {
        return;
    }

    const CSeq_hist& hist = seq.GetInst().GetHist();

    if ( hist.CanGetReplaced_by() ) {
        const CSeq_hist::TReplaced_by& r = hist.GetReplaced_by();
        if ( r.CanGetDate()  &&  !r.GetIds().empty() ) {
            x_AddComment(new CHistComment(CHistComment::eReplaced_by,
                hist, ctx));
        }
    }

    if ( hist.IsSetReplaces()  &&  ctx.IsModeGBench() ) {
        const CSeq_hist::TReplaces& r = hist.GetReplaces();
        if ( r.CanGetDate()  &&  !r.GetIds().empty() ) {
            x_AddComment(new CHistComment(CHistComment::eReplaces,
                hist, ctx));
        }
    }
}


void CFlatGatherer::x_WGSComment(CFFContext& ctx) const
{
    if ( !ctx.IsWGSMaster()  ||  ctx.GetWGSMasterName().empty() ) {
        return;
    }

    if ( ctx.GetTech() == CMolInfo::eTech_wgs ) {
        string str = CCommentItem::GetStringForWGS(ctx);
        if ( !str.empty() ) {
            x_AddComment(new CCommentItem(str, ctx));
        }
    }
}


void CFlatGatherer::x_GBBSourceComment(CFFContext& ctx) const
{
    _ASSERT(ctx.ShowGBBSource());

    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_Genbank); it; ++it) {
        const CGB_block& gbb = it->GetGenbank();
        if ( gbb.CanGetSource()  &&  !gbb.GetSource().empty() ) {
            x_AddComment(new CCommentItem(
                "Original source text: " + gbb.GetSource(),
                ctx,
                &(*it)));
        }
    }

}


void CFlatGatherer::x_DescComments(CFFContext& ctx) const
{
    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_Comment); it; ++it) {
        x_AddComment(new CCommentItem(*it, ctx));
    }
}


void CFlatGatherer::x_MaplocComments(CFFContext& ctx) const
{
    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_Maploc); it; ++it) {
        x_AddComment(new CCommentItem(*it, ctx));
    }
}


void CFlatGatherer::x_RegionComments(CFFContext& ctx) const
{
    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_Region); it; ++it) {
        x_AddComment(new CCommentItem(*it, ctx));
    }
}


void CFlatGatherer::x_HTGSComments(CFFContext& ctx) const
{
    CSeqdesc_CI mi_iter(ctx.GetHandle(), CSeqdesc::e_Molinfo);
    const CMolInfo& mi = mi_iter->GetMolinfo();

    if ( ctx.IsRefSeq()  &&  mi.CanGetCompleteness()  &&
         mi.GetCompleteness() != CMolInfo::eCompleteness_unknown ) {
        string str = CCommentItem::GetStringForMolinfo(mi, ctx);
        if ( !str.empty() ) {
            x_AddComment(new CCommentItem(str, ctx, &(*mi_iter)));
        }
    }

    CMolInfo::TTech tech = ctx.GetTech();
    if ( tech == CMolInfo::eTech_htgs_0  ||
         tech == CMolInfo::eTech_htgs_1  ||
         tech == CMolInfo::eTech_htgs_2 ) {
        x_AddComment(new CCommentItem(
            CCommentItem::GetStringForHTGS(mi, ctx), ctx, &(*mi_iter)));
    } else {
        const string& tech_str = GetTechString(tech);
        if ( !tech_str.empty() ) {
            x_AddComment(new CCommentItem("Method: " + tech_str, ctx, &(*mi_iter)));
        }
    }
}


void CFlatGatherer::x_FeatComments(CFFContext& ctx) const
{

}


/////////////////////////////////////////////////////////////////////////////
//
// ORIGIN

// We use multiple items to represent the sequence.
void CFlatGatherer::x_GatherSequence(void) const
{
    static const TSeqPos kChunkSize = 2400; // 20 lines
    
    bool first = true;
    TSeqPos size = m_Context->GetHandle(). GetBioseqLength();
    for ( TSeqPos start = 0; start < size; start += kChunkSize ) {
        TSeqPos end = min(start + kChunkSize, size);
        *m_ItemOS << new CSequenceItem(start, end, first, *m_Context);
        first = false;
    }
}


/////////////////////////////////////////////////////////////////////////////
//
// FEATURES


// source

void CFlatGatherer::x_CollectSourceDescriptors
(CBioseq_Handle& bh,
 const CRange<TSeqPos>& range,
 CFFContext& ctx,
 TSourceFeatSet& srcs) const
{
    CRef<CSourceFeatureItem> sf;
    CScope* scope = &ctx.GetScope();

    for ( CSeqdesc_CI dit(bh, CSeqdesc::e_Source); dit;  ++dit) {
        sf.Reset(new CSourceFeatureItem(dit->GetSource(), range, ctx));
        srcs.push_back(sf);
    }
    
    // if segmented collect descriptors from segments
    if ( ctx.IsSegmented() ) {
        const CSeqMap& seq_map = bh.GetSeqMap();
        
        // iterate over segments
        CSeqMap_CI smit(seq_map.begin_resolved(scope, 1, CSeqMap::fFindRef));
        for ( ; smit; ++smit ) {
            CBioseq_Handle segh = scope->GetBioseqHandle(smit.GetRefSeqid());
            if ( !segh ) {
                continue;
            }
            CRange<TSeqPos> seg_range(smit.GetPosition(), smit.GetEndPosition());
            // collect descriptors only from the segment 
            const CBioseq& seq = segh.GetBioseq();
            if ( seq.CanGetDescr() ) {
                ITERATE(CBioseq::TDescr::Tdata, it, seq.GetDescr().Get()) {
                    if ( (*it)->IsSource() ) {
                        sf.Reset(new CSourceFeatureItem((*it)->GetSource(), seg_range, ctx));
                        srcs.push_back(sf);
                    }
                }
            }
        }
    }
}


void CFlatGatherer::x_CollectSourceFeatures
(CBioseq_Handle& bh,
 const CRange<TSeqPos>& range,
 CFFContext& ctx,
 TSourceFeatSet& srcs) const
{
    SAnnotSelector as;
    as.SetFeatType(CSeqFeatData::e_Biosrc);
    as.SetOverlapIntervals();
    as.SetCombineMethod(SAnnotSelector::eCombine_All);
    as.SetResolveDepth(1);  // in case segmented
    as.SetNoMapping(false);

    for ( CFeat_CI fi(bh, range.GetFrom(), range.GetTo(), as); fi; ++fi ) {
        TSeqPos stop = fi->GetLocation().GetTotalRange().GetTo();
        if ( stop >= range.GetFrom()  &&  stop  <= range.GetTo() ) {
            CRef<CSourceFeatureItem> sf(new CSourceFeatureItem(*fi, ctx));
            srcs.push_back(sf);
        }
    }
}


void CFlatGatherer::x_CollectBioSourcesOnBioseq
(CBioseq_Handle bh,
 CRange<TSeqPos> range,
 CFFContext& ctx,
 TSourceFeatSet& srcs) const
{
    // collect biosources descriptors on bioseq
    if ( !ctx.IsFormatFTable()  ||  ctx.IsModeDump() ) {
        x_CollectSourceDescriptors(bh, range, ctx, srcs);
    }

    // collect biosources features on bioseq
    if ( !ctx.DoContigStyle()  ||  ctx.ShowContigSources() ) {
        x_CollectSourceFeatures(bh, range, ctx, srcs);
    }
}


void CFlatGatherer::x_CollectBioSources(TSourceFeatSet& srcs) const
{
    CFFContext& ctx = *m_Context;
    CScope* scope = &ctx.GetScope();

    x_CollectBioSourcesOnBioseq(ctx.GetHandle(),
                                CRange<TSeqPos>::GetWhole(),
                                ctx,
                                srcs);
    
    // if protein with no sources, get sources applicable to DNA location of CDS
    if ( srcs.empty()  &&  ctx.IsProt() ) {
        const CSeq_feat* cds = GetCDSForProduct(ctx.GetActiveBioseq(), scope);
        if ( cds != 0 ) {
            const CSeq_loc& cds_loc = cds->GetLocation();
            x_CollectBioSourcesOnBioseq(
                scope->GetBioseqHandle(cds_loc),
                cds_loc.GetTotalRange(),
                ctx,
                srcs);
        }
    }

    // if no source found create one (only in FTable format or Dump mode)
    if ( !ctx.IsFormatFTable()  ||  ctx.IsModeDump() ) {
        // !!!  
    }
}



void CFlatGatherer::x_MergeEqualBioSources(TSourceFeatSet& srcs) const
{
    if ( srcs.size() < 2 ) {
        return;
    }
    // !!! To Do:
    // !!! * sort based on biosource
    // !!! * merge equal biosources (merge locations)
}


void CFlatGatherer::x_SubtractFromFocus(TSourceFeatSet& srcs) const
{
    if ( srcs.size() < 2 ) {
        return;
    }
    // !!! To Do:
    // !!! * implement SeqLocSubtract
}


struct SSortByLoc
{
    bool operator()(const CRef<CSourceFeatureItem>& sfp1,
                    const CRef<CSourceFeatureItem>& sfp2) 
    {
        // descriptor always goes first
        if ( sfp1->WasDesc()  &&  !sfp2->WasDesc() ) {
            return true;
        }

        
        CSeq_loc::TRange range1 = sfp1->GetLoc().GetTotalRange();
        CSeq_loc::TRange range2 = sfp2->GetLoc().GetTotalRange();
        // feature with smallest left extreme is first
        if ( range1.GetFrom() != range2.GetFrom() ) {
            return range1.GetFrom() < range2.GetFrom();
        }
        
        // shortest first (just for flatfile)
        if ( range1.GetToOpen() != range2.GetToOpen() ) {
            return range1.GetToOpen() < range2.GetToOpen();
        }
        
        return false;
    }
};


void CFlatGatherer::x_GatherSourceFeatures(void) const
{
    TSourceFeatSet srcs;
    
    x_CollectBioSources(srcs);
    x_MergeEqualBioSources(srcs);
    
    // sort by type (descriptor / feature) and location
    sort(srcs.begin(), srcs.end(), SSortByLoc());
    
    // if the descriptor has a focus, subtract out all other source locations.
    if ( srcs.front()->GetSource().IsSetIs_focus() ) {
        x_SubtractFromFocus(srcs);

        // if features completely subtracted descriptor intervals,
        // suppress in release, entrez modes.
        if ( srcs.front()->GetLoc().GetTotalRange().GetLength() == 0  &&
             m_Context->HideEmptySource()  &&  srcs.size() > 1 ) {
            srcs.pop_front();
        }
    }
  
    ITERATE( TSourceFeatSet, it, srcs ) {
        *m_ItemOS << *it;
    }

}


void s_SetSelection(SAnnotSelector& sel, CFFContext& ctx)
{
    sel.SetFeatType(CSeqFeatData::e_not_set);  // get all feature
    sel.SetOverlapType(SAnnotSelector::eOverlap_Intervals);
    sel.SetResolveMethod(SAnnotSelector::eResolve_TSE);     // !!! should be set according to user flags
    if ( GetStrand(*ctx.GetLocation(), &ctx.GetScope()) == eNa_strand_minus ) {
        sel.SetSortOrder(SAnnotSelector::eSortOrder_Reverse);  // sort in reverse biological order
    } else {
        sel.SetSortOrder(SAnnotSelector::eSortOrder_Normal);
    }
}


void CFlatGatherer::x_GatherFeatures(void) const
{
    CFFContext& ctx = *m_Context;
    CScope& scope = ctx.GetScope();
    typedef CConstRef<CFeatureItemBase> TFFRef;
    list<TFFRef> l, l2;
    CFlatItemOStream& out = *m_ItemOS;

    SAnnotSelector sel;
    const SAnnotSelector* selp = ctx.GetAnnotSelector();
    if ( selp == 0 ) {
        s_SetSelection(sel, ctx);
        selp = &sel;
    }

    // optionally map gene from genomic onto cDNA
    if ( ctx.IsGPS()  &&  ctx.CopyGeneToCDNA()  &&
         ctx.GetBiomol() == CMolInfo::eBiomol_mRNA ) {
        const CSeq_feat* mrna = GetmRNAForProduct(ctx.GetActiveBioseq(), &scope);
        if ( mrna != 0 ) {
            CConstRef<CSeq_feat> gene = 
                GetOverlappingGene(mrna->GetLocation(), scope);
            if ( gene != 0 ) {
                CRef<CSeq_loc> loc = 
                    SourceToProduct(*mrna, gene->GetLocation(), 0, &scope);
                out << new CFeatureItem(*gene, ctx, loc);
            }
        }
    }

    // collect features
    for ( CFeat_CI it(scope, *ctx.GetLocation(), *selp); it; ++it ) {
        out << new CFeatureItem(*it, ctx);

        // Add more features depending on user preferences
        switch ( it->GetData().GetSubtype() ) {
        case CSeqFeatData::eSubtype_mRNA:
            {{
                // optionally map CDS from cDNA onto genomic
                if ( ctx.IsGPS()  &&  ctx.IsNa()  &&  ctx.CopyCDSFromCDNA() ) {
                    if ( it->IsSetProduct() ) {
                        CBioseq_Handle cdna = scope.GetBioseqHandle(it->GetProduct());
                        if ( cdna ) {
                            // There is only one CDS on an mRNA
                            CFeat_CI cds(cdna, 0, 0, CSeqFeatData::e_Cdregion);
                            if ( cds ) {
                                CRef<CSeq_loc> loc = 
                                    ProductToSource(cds->GetOriginalFeature(),
                                                    it->GetLocation(),
                                                    0, &scope);
                                out << new CFeatureItem(cds->GetOriginalFeature(), ctx, loc);
                            }
                        }
                    }
                }
                break;
            }}
        case CSeqFeatData::eSubtype_cdregion:
            {{  
                if ( !ctx.IsFormatFTable() ) {
                    x_GetFeatsOnCdsProduct(it->GetOriginalFeature(), ctx);
                }
                break;
            }}
        default:
            break;
        }
    }

    if ( ctx.IsProt() ) {
        CFeat_CI it(scope, *ctx.GetLocation(), CSeqFeatData::e_not_set,
                         SAnnotSelector::eOverlap_Intervals,
                         CFeat_CI::eResolve_All, CFeat_CI::e_Product);
        while ( it ) {
            switch ( it->GetData().Which() ) {
            case CSeqFeatData::e_Cdregion:
                out << new CFeatureItem(*it, ctx, &it->GetProduct(), true);
                break;
            case CSeqFeatData::e_Prot:
                // for RefSeq records or GenBank not release_mode
                if ( ctx.IsRefSeq()  ||  !ctx.ForGBRelease() ) {
                    out << new CFeatureItem(*it, ctx, &it->GetProduct(), true);
                }
                break;
            default:
                break;
            }
            ++it;
        }
    }
}


static bool s_IsCDD(const CSeq_feat& feat)
{
    ITERATE(CSeq_feat::TDbxref, it, feat.GetDbxref()) {
        if ( (*it)->GetType() == CDbtag::eDbtagType_CDD ) {
            return true;
        }
    }
    return false;
}


void CFlatGatherer::x_GetFeatsOnCdsProduct
(const CSeq_feat& feat,
 CFFContext& ctx) const
{
    _ASSERT(feat.GetData().IsCdregion());

    if ( ctx.HideCDSProdFeats() ) {
        return;
    }
    
    if ( !feat.CanGetProduct() ) {
        return;
    }

    CBioseq_Handle  prot = ctx.GetScope().GetBioseqHandle(feat.GetProduct());
    
    CFeat_CI prev;
    bool first = true;
    // explore mat_peptides, sites, etc.
    for ( CFeat_CI it(prot, 0, 0); it; ++it ) {
        CSeqFeatData::ESubtype subtype = it->GetData().GetSubtype();
        if ( !(subtype == CSeqFeatData::eSubtype_region)              &&
             !(subtype == CSeqFeatData::eSubtype_site)                &&
             !(subtype == CSeqFeatData::eSubtype_bond)                &&
             !(subtype == CSeqFeatData::eSubtype_mat_peptide_aa)      &&
             !(subtype == CSeqFeatData::eSubtype_sig_peptide_aa)      &&
             !(subtype == CSeqFeatData::eSubtype_transit_peptide_aa)  &&
             !(subtype == CSeqFeatData::eSubtype_preprotein) ) {
            continue;
        }

        if ( ctx.HideCDDFeats()  &&
             subtype == CSeqFeatData::eSubtype_region  &&
             s_IsCDD(it->GetOriginalFeature()) ) {
            // passing this test prevents mapping of COG CDD region features
            continue;
        }

        // suppress duplicate features (on protein)
        if ( !first ) {
            const CSeq_loc& loc_curr = it->GetLocation();
            const CSeq_loc& loc_prev = prev->GetLocation();
            const CSeq_feat& feat_curr = it->GetOriginalFeature();
            const CSeq_feat& feat_prev = prev->GetOriginalFeature();

            if ( feat_prev.CompareNonLocation(feat_curr, loc_curr, loc_prev) == 0 ) {
                continue;
            }
        }

        // make sure feature is within sublocation
        CRef<CSeq_loc> loc;
        if ( ctx.GetLocation() != 0 ) {
            loc = ProductToSource(feat, it->GetLocation(),
                0, &ctx.GetScope());
            if ( !loc ) {
                continue;
            }
            if ( ctx.IsStyleMaster()  &&  ctx.IsPart() ) {
                loc = ctx.Master()->GetHandle().MapLocation(*loc);
                if ( !loc ) {
                    continue;
                }
                if ( Compare(*ctx.GetLocation(), *loc) != eContains ) {
                    continue;
                }
            }
        }

        *m_ItemOS << new CFeatureItem(it->GetOriginalFeature(), ctx, loc);

        prev = it;
        first = false;
    }    
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.5  2004/02/11 22:52:41  shomrat
* using types in flag file
*
* Revision 1.4  2004/02/11 16:52:12  shomrat
* completed implementation of featture gathering
*
* Revision 1.3  2004/01/14 16:15:03  shomrat
* minor changes to accomodate for GFF format
*
* Revision 1.2  2003/12/18 17:43:34  shomrat
* context.hpp moved
*
* Revision 1.1  2003/12/17 20:21:48  shomrat
* Initial Revision (adapted from flat lib)
*
*
* ===========================================================================
*/
