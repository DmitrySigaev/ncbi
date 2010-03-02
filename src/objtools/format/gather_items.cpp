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
*          Mati Shomrat, NCBI
*
* File Description:
*   
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seq/Seq_hist_rec.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/seq_map.hpp>
#include <objmgr/seq_map_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/annot_selector.hpp>

#include <algorithm>

#include <objtools/format/item_ostream.hpp>
#include <objtools/format/flat_expt.hpp>
#include <objtools/format/items/contig_item.hpp>
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
#include <objtools/format/items/alignment_item.hpp>
#include <objtools/format/items/gap_item.hpp>
#include <objtools/format/gather_items.hpp>
#include <objtools/format/genbank_gather.hpp>
#include <objtools/format/embl_gather.hpp>
#include <objtools/format/gff_gather.hpp>
#include <objtools/format/ftable_gather.hpp>
#include <objtools/format/feature_gather.hpp>
#include <objtools/format/context.hpp>
#include <objtools/error_codes.hpp>
#include "utils.hpp"


#define NCBI_USE_ERRCODE_X   Objtools_Fmt_Gather


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
USING_SCOPE(sequence);


/////////////////////////////////////////////////////////////////////////////
//
// Public:

// "virtual constructor"
CFlatGatherer* CFlatGatherer::New(CFlatFileConfig::TFormat format)
{
    switch ( format ) {
    case CFlatFileConfig::eFormat_GenBank:
    case CFlatFileConfig::eFormat_GBSeq:
        //case CFlatFileGenerator<>::eFormat_Index:
        return new CGenbankGatherer;
        
    case CFlatFileConfig::eFormat_EMBL:
        return new CEmblGatherer;

    case CFlatFileConfig::eFormat_GFF:
    case CFlatFileConfig::eFormat_GFF3:
        return new CGFFGatherer;
    
    case CFlatFileConfig::eFormat_FTable:
        return new CFtableGatherer;

	case CFlatFileConfig::eFormat_FeaturesOnly:
		return new CFeatureGatherer;

    case CFlatFileConfig::eFormat_DDBJ:
    default:
        NCBI_THROW(CFlatException, eNotSupported, 
            "This format is currently not supported");
    }

    return 0;
}


void CFlatGatherer::Gather(CFlatFileContext& ctx, CFlatItemOStream& os) const
{
    m_ItemOS.Reset(&os);
    m_Context.Reset(&ctx);

    CConstRef<IFlatItem> item;
    item.Reset( new CStartItem() );
    os << item;
    x_GatherSeqEntry(ctx.GetEntry());
    item.Reset( new CEndItem() );
    os << item;
}


CFlatGatherer::~CFlatGatherer(void)
{
}


/////////////////////////////////////////////////////////////////////////////
//
// Protected:


void CFlatGatherer::x_GatherSeqEntry(const CSeq_entry_Handle& entry) const
{
    if ( entry.IsSet()  &&  entry.GetSet().IsSetClass() ) {
        CBioseq_set::TClass clss = entry.GetSet().GetClass();
        if ( clss == CBioseq_set::eClass_genbank  ||
             clss == CBioseq_set::eClass_mut_set  ||
             clss == CBioseq_set::eClass_pop_set  ||
             clss == CBioseq_set::eClass_phy_set  ||
             clss == CBioseq_set::eClass_eco_set  ||
             clss == CBioseq_set::eClass_wgs_set  ||
             clss == CBioseq_set::eClass_gen_prod_set ) {
            for ( CSeq_entry_CI it(entry); it; ++it ) {
                x_GatherSeqEntry(*it);
            }
            return;
        }
    }

    CSeq_inst::TMol mol_type;
    const CFlatFileConfig& cfg = Config();
    if (cfg.IsViewAll()) {
        mol_type = CSeq_inst::eMol_not_set;
    } else if (cfg.IsViewNuc()) {
        mol_type = CSeq_inst::eMol_na;
    } else if (cfg.IsViewProt()) {
        mol_type = CSeq_inst::eMol_aa;
    } else {
        return;
    }

    // visit bioseqs in the entry (excluding segments)
    CBioseq_CI seq_iter(entry, mol_type, CBioseq_CI::eLevel_Mains);
    for ( ; seq_iter; ++seq_iter ) {
        CSeq_id_Handle id = GetId(*seq_iter, eGetId_Best);
        if (id.GetSeqId()->IsLocal()  &&  cfg.SuppressLocalId()) {
            continue;
        }
        x_GatherBioseq(*seq_iter);
    }
} 


static bool s_LocationsTouch( const CSeq_loc& loc1, const CSeq_loc& loc2 )
{
    CRange<TSeqPos> rg1, rg2;
    try {
        rg1 = loc1.GetTotalRange();
        rg2 = loc2.GetTotalRange();
    }
    catch( ... ) {
        return false;
    }
    return (rg1.GetFrom() == rg2.GetTo() + 1) || (rg1.GetTo() + 1 == rg2.GetFrom());
};


static bool s_LocationsOverlap( const CSeq_loc& loc1, const CSeq_loc& loc2 )
{
    return ( -1 != TestForOverlap( loc1, loc2, eOverlap_Simple, kInvalidSeqPos, 0 ) );
};


static bool s_IsSegmented(const CBioseq_Handle& seq)
{
    return seq  &&
           seq.IsSetInst()  &&
           seq.IsSetInst_Repr()  &&
           seq.GetInst_Repr() == CSeq_inst::eRepr_seg;
}


static bool s_HasSegments(const CBioseq_Handle& seq)
{
    CSeq_entry_Handle h =
        seq.GetExactComplexityLevel(CBioseq_set::eClass_segset);
    if (h) {
        for (CSeq_entry_CI it(h); it; ++it) {
            if (it->IsSet()  &&  it->GetSet().IsSetClass()  &&
                it->GetSet().GetClass() == CBioseq_set::eClass_parts) {
                return true;
            }
        }
    }
    return false;
}

bool s_BioSeqHasContig( 
    const CBioseq_Handle& seq, 
    CFlatFileContext& ctx )
{
    CBioseqContext* pbsc = new CBioseqContext(seq, ctx );
    CContigItem* pContig = new CContigItem( * pbsc );
    CSeq_loc::E_Choice choice = pContig->GetLoc().Which();
    delete pContig;
    delete pbsc;
        
    return ( choice != CSeq_loc::e_not_set );
}

// a default implementation for GenBank /  DDBJ formats
void CFlatGatherer::x_GatherBioseq(const CBioseq_Handle& seq) const
{
    const CFlatFileConfig& cfg = Config();
    if ( cfg.IsModeRelease() && cfg.IsStyleContig() && 
      ! s_BioSeqHasContig( seq, *m_Context ) ) {
        NcbiCerr << "Release mode failure." << endl;
        return;
    }

    // Do multiple sections (segmented style) if:
    // a. the bioseq is segmented and has near parts
    // b. style is normal or segmented (not master)
    // c. user didn't specify a location
    // d. not FTable format
    if ( s_IsSegmented(seq)  &&  s_HasSegments(seq)       &&
         (cfg.IsStyleNormal()  ||  cfg.IsStyleSegment())  &&
         (m_Context->GetLocation() == 0)                  &&
         !cfg.IsFormatFTable() ) {
        x_DoMultipleSections(seq);
    } else {
        // display as a single bioseq (single section)
        m_Current.Reset(new CBioseqContext(seq, *m_Context));
        m_Context->AddSection(m_Current);
        x_DoSingleSection(*m_Current);
    }   
}


void CFlatGatherer::x_DoMultipleSections(const CBioseq_Handle& seq) const
{
    CRef<CMasterContext> mctx(new CMasterContext(seq));

    CScope* scope = &seq.GetScope();
    const CSeqMap& seqmap = seq.GetSeqMap();

    CSeqMap_CI it = seqmap.BeginResolved(scope,
                                         SSeqMapSelector()
                                         .SetResolveCount(1)
                                         .SetFlags(CSeqMap::fFindRef));
    while ( it ) {
        CSeq_id_Handle id = it.GetRefSeqid();
        CBioseq_Handle part = scope->GetBioseqHandleFromTSE(id, seq);
        if (part) {
            // do only non-virtual parts
            CSeq_inst::TRepr repr = part.IsSetInst_Repr() ?
                part.GetInst_Repr() : CSeq_inst::eRepr_not_set;
            if (repr != CSeq_inst::eRepr_virtual) {
                m_Current.Reset(new CBioseqContext(part, *m_Context, mctx));
                m_Context->AddSection(m_Current);
                x_DoSingleSection(*m_Current);
            }
        }
        ++it;
    }
}

    
/////////////////////////////////////////////////////////////////////////////
//
// REFERENCES

bool s_IsJustUids( const CPubdesc& pubdesc ) 
{
    const CPubdesc::TPub& pub = pubdesc.GetPub();
    ITERATE ( CPub_equiv::Tdata, it, pub.Get() ) {
        
        switch( (*it)->Which() ) {

        case CPub::e_Gen:
        case CPub::e_Sub:
        case CPub::e_Article:
        case CPub::e_Journal:
        case CPub::e_Book:
        case CPub::e_Proc:
        case CPub::e_Patent:
        case CPub::e_Man:
            return false;
        default:
            /* placate gcc */
            break;
        }
    }
    return true;
}

bool s_FilterPubdesc(const CPubdesc& pubdesc, CBioseqContext& ctx)
{
    if ( s_IsJustUids(pubdesc) ) {
        return true;
    }
    if ( pubdesc.CanGetComment() ) {
        const string& comment = pubdesc.GetComment();
        bool is_gene_rif = NStr::StartsWith(comment, "GeneRIF", NStr::eNocase);

        const CFlatFileConfig& cfg = ctx.Config();
        if ( (cfg.HideGeneRIFs()  &&  is_gene_rif)  ||
             ((cfg.OnlyGeneRIFs()  ||  cfg.LatestGeneRIFs())  &&  !is_gene_rif) ) {
            return true;
        }
    }

    return false;
}

void CFlatGatherer::x_GatherReferences(const CSeq_loc& loc, TReferences& refs) const
{
    CScope& scope = m_Current->GetScope();
    
    CBioseq_Handle seq = GetBioseqFromSeqLoc(loc, scope);
    if (!seq) {
        return;
    }

    // gather references from descriptors
    for (CSeqdesc_CI it(seq, CSeqdesc::e_Pub); it; ++it) {
        const CPubdesc& pubdesc = it->GetPub();
        if ( s_FilterPubdesc(pubdesc, *m_Current) ) {
            continue;
        }
        refs.push_back(CBioseqContext::TRef(new CReferenceItem(*it, *m_Current)));
    }

    // if near segmented, collect pubs from segments under location
    CSeq_entry_Handle segset =
        seq.GetExactComplexityLevel(CBioseq_set::eClass_segset);
    if (segset  &&  seq.GetInst_Repr() == CSeq_inst::eRepr_seg) {
        CConstRef<CSeqMap> seqmap = CSeqMap::CreateSeqMapForSeq_loc(loc, &scope);
        if (seqmap) {
            SSeqMapSelector mapsel;
            mapsel.SetFlags(CSeqMap::eSeqRef)
                  .SetResolveCount(1)
                  .SetLimitTSE(m_Current->GetTopLevelEntry());
            for (CSeqMap_CI smit(seqmap, &scope, mapsel); smit; ++smit) {
                // NB: search already limited to TSE ...
                CBioseq_Handle part;
                try {
                    // ... but not necessarily to just references, it seems.
                    // The following line has been observed to throw almost
                    // every time when run against a pool of sample files.
                    part = scope.GetBioseqHandle(smit.GetRefSeqid());
                }
                catch ( ... ) {
                    // Seemingly not a reference. Nothing to do in this 
                    // iteration.
                    continue;
                }
                if (part) {
                    for (CSeqdesc_CI dit(CSeq_descr_CI(part, 1), CSeqdesc::e_Pub); dit; ++dit) {
                        const CPubdesc& pubdesc = dit->GetPub();
                        if ( s_FilterPubdesc(pubdesc, *m_Current) ) {
                            continue;
                        }

                        refs.push_back(CBioseqContext::TRef(new CReferenceItem(*dit, *m_Current)));
                    }
                }
            }
        }
    }

    // gather references from features
    CFeat_CI fci(scope, loc, CSeqFeatData::e_Pub);
    for ( ; fci; ++fci) {
        CBioseqContext::TRef ref(new CReferenceItem( fci->GetOriginalFeature(),
            *m_Current));
        refs.push_back(ref);
    }

    // add seq-submit citation
    if (m_Current->GetSubmitBlock() != NULL) {
        CBioseqContext::TRef ref(new CReferenceItem(*m_Current->GetSubmitBlock(),
            *m_Current));
        refs.push_back(ref);
    }
}


void CFlatGatherer::x_GatherCDSReferences(TReferences& refs) const
{
    _ASSERT(m_Current->IsProt());

    const CSeq_feat* cds = GetCDSForProduct(m_Current->GetHandle());
    if (cds == NULL) {
        return;
    }
    const CSeq_loc& cds_loc = cds->GetLocation();
    const CSeq_loc& cds_prod = cds->GetProduct();

    CScope& scope = m_Current->GetScope();

    for (CFeat_CI it(m_Current->GetScope(), cds_loc, CSeqFeatData::e_Pub); it; ++it) {
        const CSeq_feat& feat = it->GetOriginalFeature();
        if (TestForOverlap(cds_loc, feat.GetLocation(), eOverlap_Contains, kInvalidSeqPos, &scope) > 0) {
            CBioseqContext::TRef ref(new CReferenceItem(feat, *m_Current, &cds_prod));
            refs.push_back(ref);
        }
    }
}


void CFlatGatherer::x_GatherReferences(void) const
{
    TReferences& refs = m_Current->SetReferences();

    x_GatherReferences(m_Current->GetLocation(), refs);

    // if protein with no pubs, get pubs applicable to DNA location of CDS
    if (refs.empty()  &&  m_Current->IsProt()) {
        x_GatherCDSReferences(refs);
    }

    // re-sort references
    CReferenceItem::Rearrange(refs, *m_Current);

    CConstRef<IFlatItem> item;
    ITERATE (TReferences, ref, refs) {
        item.Reset( *ref );
        *m_ItemOS << item;
    }
}


/////////////////////////////////////////////////////////////////////////////
//
// COMMENTS

static bool s_NsAreGaps(const CBioseq_Handle& seq, CBioseqContext& ctx)
{
    if (!seq.IsSetInst()  ||  !seq.IsSetInst_Ext()) {
        return false;
    }

    if (ctx.IsDelta()  &&  ctx.IsWGS()  &&  seq.GetInst_Ext().IsDelta()) {
        ITERATE (CDelta_ext::Tdata, iter, seq.GetInst_Ext().GetDelta().Get()) {
            const CDelta_seq& dseg = **iter;
            if (dseg.IsLiteral()) {
                const CSeq_literal& lit = dseg.GetLiteral();
                if (!lit.CanGetSeq_data()  &&  lit.CanGetLength()  &&
                     lit.GetLength() > 0 ) {
                    return true;
                }
            }
        }
    }

    return false;
}


void CFlatGatherer::x_GatherComments(void) const
{
    CBioseqContext& ctx = *m_Current;

    // Gather comments related to the seq-id
    x_IdComments(ctx);
    x_RefSeqComments(ctx);

    if ( s_NsAreGaps(ctx.GetHandle(), ctx) ) {
        x_AddComment(new CCommentItem(CCommentItem::GetNsAreGapsStr(), ctx));
    }

    x_HistoryComments(ctx);
    x_WGSComment(ctx);
    if ( ctx.ShowGBBSource() ) {
        x_GBBSourceComment(ctx);
    }
    x_DescComments(ctx);
    x_MaplocComments(ctx);
    x_RegionComments(ctx);
    x_NameComments(ctx);
    x_HTGSComments(ctx);
//    x_FeatComments(ctx);

    x_FlushComments();
}


void CFlatGatherer::x_AddComment(CCommentItem* comment) const
{
    CRef<CCommentItem> com(comment);
    if ( !com->Skip() ) {
        m_Comments.push_back(com);
    }
}


void CFlatGatherer::x_AddGSDBComment
(const CDbtag& dbtag,
 CBioseqContext& ctx) const
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
    // add a period to the last comment (if needed)
    if (m_Comments.back()->NeedPeriod()) {
        m_Comments.back()->AddPeriod();
    }
    
    // add a period to a GSDB comment (if exist and not last)
    TCommentVec::iterator last = m_Comments.end();
    --last;
    
    CConstRef<IFlatItem> item;
    NON_CONST_ITERATE (TCommentVec, it, m_Comments) {
        CGsdbComment* gsdb = dynamic_cast<CGsdbComment*>(it->GetPointerOrNull());
        if ( gsdb != 0   &&  it != last ) {
            gsdb->AddPeriod();
        }
        item.Reset( *it );
        *m_ItemOS << item;
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
        CCommentItem::TRefTrackStatus status = 
            CCommentItem::GetRefTrackStatus(it->GetUser());
        if ( status != CCommentItem::eRefTrackStatus_Unknown ) { 
            return true;
        }
    }

    return false;
}


void CFlatGatherer::x_IdComments(CBioseqContext& ctx) const
{
    const CObject_id* local_id = 0;

    string genome_build_number = s_GetGenomeBuildNumber(ctx.GetHandle());
    bool has_ref_track_status = s_HasRefTrackStatus(ctx.GetHandle());
    CCommentItem::ECommentFormat format = ctx.Config().DoHTML() ?
        CCommentItem::eFormat_Html : CCommentItem::eFormat_Text;

    ITERATE( CBioseq::TId, id_iter, ctx.GetBioseqIds() ) {
        const CSeq_id& id = **id_iter;

        switch ( id.Which() ) {
        case CSeq_id::e_Other:
            {{
                if ( ctx.IsRSCompleteGenomic() ) {  // NC
                    if ( !genome_build_number.empty() ) {
                        x_AddComment(new CGenomeAnnotComment(ctx, genome_build_number));
                    }
                }
                if ( ctx.IsRSContig()  ||  ctx.IsRSIntermedWGS() ) {
                    if ( ctx.IsEncode() ) {
                        string encode = CCommentItem::GetStringForEncode(ctx);
                        if ( !NStr::IsBlank(encode) ) {
                            x_AddComment(new CCommentItem(encode, ctx));
                        }
                    } else if ( !has_ref_track_status ) {
                        x_AddComment(new CGenomeAnnotComment(ctx, genome_build_number));
                    }
                }
                if ( ctx.IsRSPredictedProtein()  ||
                     ctx.IsRSPredictedMRna()     ||
                     ctx.IsRSPredictedNCRna()    ||
                     ctx.IsRSWGSProt() ) {
                    SModelEvidance me;
                    if ( GetModelEvidance(ctx.GetHandle(), me) ) {
                        string str = CCommentItem::GetStringForModelEvidance(me, format);
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
            if ( ctx.Config().IsModeGBench()  ||  ctx.Config().IsModeDump() ) {
                x_AddComment(new CLocalIdComment(*local_id, ctx));
            }
        }
    }
}


void CFlatGatherer::x_RefSeqComments(CBioseqContext& ctx) const
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
            if ( !ctx.Config().HideBankItComment() ) {
                string str = CCommentItem::GetStringForBankIt(uo);
                if ( !str.empty() ) {
                    x_AddComment(new CCommentItem(str, ctx, &uo));
                }
            }
        }}

        // RefTrack
        {{
            if ( !did_ref_track ) {
                CCommentItem::ECommentFormat format = ctx.Config().DoHTML() ?
                    CCommentItem::eFormat_Html : CCommentItem::eFormat_Text;
                string str = 
                    CCommentItem::GetStringForRefTrack(uo, ctx.GetHandle(), format);
                if ( !str.empty() ) {
                    x_AddComment(new CCommentItem(str, ctx, &uo));
                    did_ref_track = true;
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


void CFlatGatherer::x_HistoryComments(CBioseqContext& ctx) const
{
    const CBioseq_Handle& seq = ctx.GetHandle();
    if ( !seq.IsSetInst_Hist() ) {
        return;
    }

    const CSeq_hist& hist = seq.GetInst_Hist();

    if ( hist.CanGetReplaced_by() ) {
        const CSeq_hist::TReplaced_by& r = hist.GetReplaced_by();
        if ( r.CanGetDate()  &&  !r.GetIds().empty() ) {
            x_AddComment(new CHistComment(CHistComment::eReplaced_by,
                hist, ctx));
        }
    }

    if ( hist.IsSetReplaces()  &&  !ctx.Config().IsModeGBench() ) {
        const CSeq_hist::TReplaces& r = hist.GetReplaces();
        if ( r.CanGetDate()  &&  !r.GetIds().empty() ) {
            x_AddComment(new CHistComment(CHistComment::eReplaces,
                hist, ctx));
        }
    }
}


void CFlatGatherer::x_WGSComment(CBioseqContext& ctx) const
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


void CFlatGatherer::x_GBBSourceComment(CBioseqContext& ctx) const
{
    if (!ctx.ShowGBBSource()) {
        return;
    }

    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_Genbank); it; ++it) {
        const CGB_block& gbb = it->GetGenbank();
        if ( gbb.CanGetSource()  &&  !gbb.GetSource().empty() ) {
            string comment = "Original source text: " + gbb.GetSource();
            ncbi::objects::AddPeriod(comment);
            x_AddComment(new CCommentItem(comment, ctx, &(*it)));
        }
    }
}


void CFlatGatherer::x_DescComments(CBioseqContext& ctx) const
{
    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_Comment); it; ++it) {
        x_AddComment(new CCommentItem(*it, ctx));
    }
}


void CFlatGatherer::x_MaplocComments(CBioseqContext& ctx) const
{
    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_Maploc); it; ++it) {
        x_AddComment(new CCommentItem(*it, ctx));
    }
}


void CFlatGatherer::x_RegionComments(CBioseqContext& ctx) const
{
    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_Region); it; ++it) {
        x_AddComment(new CCommentItem(*it, ctx));
    }
}


void CFlatGatherer::x_NameComments(CBioseqContext& ctx) const
{
    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_Name); it; ++it) {
        x_AddComment(new CCommentItem(*it, ctx));
    }
}


void CFlatGatherer::x_HTGSComments(CBioseqContext& ctx) const
{
    CSeqdesc_CI desc(ctx.GetHandle(), CSeqdesc::e_Molinfo);
    if ( !desc ) {
        return;
    }
    const CMolInfo& mi = *ctx.GetMolinfo();

    if ( ctx.IsRefSeq()  &&  
         mi.GetCompleteness() != CMolInfo::eCompleteness_unknown ) {
        string str = CCommentItem::GetStringForMolinfo(mi, ctx);
        if ( !str.empty() ) {
            AddPeriod(str);
            x_AddComment(new CCommentItem(str, ctx, &(*desc)));
        }
    }

    CMolInfo::TTech tech = mi.GetTech();
    if ( tech == CMolInfo::eTech_htgs_0  ||
         tech == CMolInfo::eTech_htgs_1  ||
         tech == CMolInfo::eTech_htgs_2 ) {
        x_AddComment(new CCommentItem(
            CCommentItem::GetStringForHTGS(ctx), ctx, &(*desc)));
    } else {
        string tech_str = GetTechString(tech);
        if (!NStr::IsBlank(tech_str)) {
            objects::AddPeriod(tech_str);
            x_AddComment(new CCommentItem("Method: " + tech_str, ctx, &(*desc)));
        }
    }
}

// add comment features that are full length on appropriate segment
void CFlatGatherer::x_FeatComments(CBioseqContext& ctx) const
{
    CScope *scope = &ctx.GetScope();
    const CSeq_loc& loc = ctx.GetLocation();

    for (CFeat_CI it(ctx.GetScope(), loc, CSeqFeatData::e_Comment);
        it; ++it) {
        ECompare comp = Compare(it->GetLocation(), loc, scope);

        if ((comp == eSame)  ||  (comp == eContains)) {
            x_AddComment(new CCommentItem(it->GetOriginalFeature(), ctx));
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
//
// SEQUENCE

// We use multiple items to represent the sequence.
void CFlatGatherer::x_GatherSequence(void) const
{ 
    static const TSeqPos kChunkSize = 4800;
    
    bool first = true;
    TSeqPos size = GetLength(m_Current->GetLocation(), &m_Current->GetScope());

    CConstRef<IFlatItem> item;
    for ( TSeqPos start = 1; start <= size; start += kChunkSize ) {
        TSeqPos end = min(start + kChunkSize - 1, size);
        item.Reset( new CSequenceItem(start, end, first, *m_Current) );
        *m_ItemOS << item;
        first = false;
    }
}


/////////////////////////////////////////////////////////////////////////////
//
// FEATURES


// source

void CFlatGatherer::x_CollectSourceDescriptors
(const CBioseq_Handle& bh,
 CBioseqContext& ctx,
 TSourceFeatSet& srcs) const
{
    CRef<CSourceFeatureItem> sf;
    CScope* scope = &ctx.GetScope();
    const CSeq_loc& loc = ctx.GetLocation();

    TRange print_range(0, GetLength(loc, scope) - 1);

    // if SWISS-PROT, may have multiple source descriptors
    bool loop = ctx.IsSP(), okay = false;

    // collect biosources on bioseq
    for (CSeqdesc_CI dit(bh, CSeqdesc::e_Source); dit;  ++dit) {
        const CBioSource& bsrc = dit->GetSource();
        if (bsrc.IsSetOrg()) {
            sf.Reset(new CSourceFeatureItem(bsrc, print_range, ctx));
            srcs.push_back(sf);
            okay = true;
        }
        if(!loop  &&  okay) {
            break;
        }
    }

    // if segmented collect descriptors from local segments
    if (bh.GetInst_Repr() == CSeq_inst::eRepr_seg) {
        CTSE_Handle tse = bh.GetTSE_Handle();
        CSeqMap_CI smit(bh, SSeqMapSelector(CSeqMap::fFindRef));
        for (; smit; ++smit) {
            // biosource descriptors only on parts
            CBioseq_Handle segh = 
                scope->GetBioseqHandleFromTSE(smit.GetRefSeqid(), tse);
            if (!segh) {
                continue;
            }

            CSeqdesc_CI src_it(CSeq_descr_CI(segh, 1), CSeqdesc::e_Source);
            for (; src_it; ++src_it) {
                CRange<TSeqPos> seg_range(smit.GetPosition(), smit.GetEndPosition());
                // collect descriptors only from the segment 
                const CBioSource& bsrc = src_it->GetSource();
                if (bsrc.IsSetOrg()) {
                    sf.Reset(new CSourceFeatureItem(bsrc, seg_range, ctx));
                    srcs.push_back(sf);
                }
            }
        }
    }
}


void CFlatGatherer::x_CollectSourceFeatures
(const CBioseq_Handle& bh,
 const TRange& range,
 CBioseqContext& ctx,
 TSourceFeatSet& srcs) const
{
    SAnnotSelector as;
    as.SetFeatType(CSeqFeatData::e_Biosrc)
      .SetOverlapIntervals()
      .SetResolveDepth(1) // in case segmented
      .SetNoMapping(false)
      .SetLimitTSE(ctx.GetHandle().GetTopLevelEntry());

    for ( CFeat_CI fi(bh, range, as); fi; ++fi ) {
        TSeqPos stop = fi->GetLocation().GetTotalRange().GetTo();
        if ( stop >= range.GetFrom()  &&  stop  <= range.GetTo() ) {
            CRef<CSourceFeatureItem> sf(new CSourceFeatureItem(*fi, ctx));
            srcs.push_back(sf);
        }
    }
}


void CFlatGatherer::x_CollectBioSourcesOnBioseq
(const CBioseq_Handle& bh,
 const TRange& range,
 CBioseqContext& ctx,
 TSourceFeatSet& srcs) const
{
    const CFlatFileConfig& cfg = ctx.Config();

    // collect biosources descriptors on bioseq
    if ( !cfg.IsFormatFTable()  ||  cfg.IsModeDump() ) {
        x_CollectSourceDescriptors(bh, ctx, srcs);
    }

    // collect biosources features on bioseq
    if ( !ctx.DoContigStyle()  ||  cfg.ShowContigSources() ) {
        x_CollectSourceFeatures(bh, range, ctx, srcs);
    }
}


void CFlatGatherer::x_CollectBioSources(TSourceFeatSet& srcs) const
{
    CBioseqContext& ctx = *m_Current;
    CScope* scope = &ctx.GetScope();
    const CFlatFileConfig& cfg = ctx.Config();

    x_CollectBioSourcesOnBioseq(ctx.GetHandle(),
                                ctx.GetLocation().GetTotalRange(),
                                ctx,
                                srcs);
    
    // if protein with no sources, get sources applicable to DNA location of CDS
    if ( srcs.empty()  &&  ctx.IsProt() ) {
        const CSeq_feat* cds = GetCDSForProduct(ctx.GetHandle());
        if ( cds != 0 ) {
            const CSeq_loc& cds_loc = cds->GetLocation();
            x_CollectBioSourcesOnBioseq(
                scope->GetBioseqHandle(cds_loc),
                cds_loc.GetTotalRange(),
                ctx,
                srcs);
        }
    }

    // if no source found create one (only if not FTable format or Dump mode)
    if ( srcs.empty()  &&  ! cfg.IsFormatFTable()  &&  ! cfg.IsModeDump() ) {
        CRef<CBioSource> bsrc(new CBioSource);
        bsrc->SetOrg();
        CRef<CSourceFeatureItem> sf(new CSourceFeatureItem(*bsrc, CRange<TSeqPos>::GetWhole(), ctx));
        srcs.push_back(sf);
    }
}


void CFlatGatherer::x_SubtractFromFocus(TSourceFeatSet& srcs) const
{
    if ( srcs.size() < 2 ) {
        return;
    }

    CRef<CSourceFeatureItem> focus;
    NON_CONST_ITERATE(TSourceFeatSet, it, srcs) {
        if (it == srcs.begin()) {
            focus = *it;
        } else {
            _ASSERT(focus);
            focus->Subtract(**it, m_Current->GetScope());
        }
    }
}


struct SSortSourceByLoc
{
    bool operator()(const CRef<CSourceFeatureItem>& sfp1,
                    const CRef<CSourceFeatureItem>& sfp2) 
    {
        // descriptor always goes first
        if (sfp1->WasDesc()  &&  !sfp2->WasDesc()) {
            return true;
        } else if (!sfp1->WasDesc()  &&  sfp2->WasDesc()) {
            return false;
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
    if ( srcs.empty() ) {
        return;
    }

    if (!m_Current->Config().IsModeDump()) {
        x_MergeEqualBioSources(srcs);
    }
    
    // sort by type (descriptor / feature) and location
    sort(srcs.begin(), srcs.end(), SSortSourceByLoc());

    // if the descriptor has a focus (by now sorted to be first),
    // subtract out all other source locations.
//    if (srcs.front()->IsFocus()  &&  !srcs.front()->IsSynthetic()) {
    if (srcs.front()->IsFocus()) {
        x_SubtractFromFocus(srcs);

        // if features completely subtracted descriptor intervals,
        // suppress in release, entrez modes.
        if ( srcs.front()->GetLoc().GetTotalRange().GetLength() == 0  &&
             m_Current->Config().HideEmptySource()  &&  srcs.size() > 1 ) {
            srcs.pop_front();
        }
    }
  
    CConstRef<IFlatItem> item;
    ITERATE( TSourceFeatSet, it, srcs ) {
        item.Reset( *it );
        *m_ItemOS << item;
    }
}


void CFlatGatherer::x_MergeEqualBioSources(TSourceFeatSet& srcs) const
{
    if ( srcs.size() < 2 ) {
        return;
    }

    // merge equal sources
    TSourceFeatSet::iterator it = srcs.begin();
    while (it != srcs.end()) {
        TSourceFeatSet::iterator it2 = it++;
        const CSeq_feat& f2 = (*it2)->GetFeat();
        const string& c2 = f2.IsSetComment() ? f2.GetComment() : kEmptyStr;
        while (it != srcs.end()) {
            const CSeq_feat& f = (*it)->GetFeat();
            const string& c = f.IsSetComment() ? f.GetComment() : kEmptyStr;
            if (NStr::EqualNocase(c2, c)  &&
                (*it2)->GetSource().Equals((*it)->GetSource())) 
            {
                if ( s_LocationsOverlap( (*it)->GetLoc(), (*it2)->GetLoc() ) ||
                    s_LocationsTouch(  (*it)->GetLoc(), (*it2)->GetLoc() ) )
                {
                    CRef<CSeq_loc> merged_loc = 
                        Seq_loc_Add((*it2)->GetLoc(),(*it)->GetLoc(),
                                    CSeq_loc::fSort | CSeq_loc::fMerge_All,
                                    &m_Current->GetScope());
                    (*it2)->SetLoc(*merged_loc);
                    it = srcs.erase(it);
                    continue;
                }
            }
            ++it;
        }
        it = ++it2;
    }
}


void s_SetSelection(SAnnotSelector& sel, CBioseqContext& ctx)
{
    const CFlatFileConfig& cfg = ctx.Config();

    // set feature types to be collected
    {{
        //sel.SetAnnotType(CSeq_annot::C_Data::e_Ftable);
        // source features are collected elsewhere
        sel.ExcludeFeatType(CSeqFeatData::e_Biosrc);
        // pub features are used in the REFERENCES section
        sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_pub);
        // some feature types are always excluded (deprecated?)
        sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_non_std_residue)
           .ExcludeFeatSubtype(CSeqFeatData::eSubtype_rsite)
           .ExcludeFeatSubtype(CSeqFeatData::eSubtype_seq);
        // exclude other types based on user flags
        if ( cfg.HideImpFeatures() ) {
            sel.ExcludeFeatType(CSeqFeatData::e_Imp);
        }
        if ( cfg.HideRemoteImpFeatures() ) {
            sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_variation)
               .ExcludeFeatSubtype(CSeqFeatData::eSubtype_exon)
               .ExcludeFeatSubtype(CSeqFeatData::eSubtype_intron)
               .ExcludeFeatSubtype(CSeqFeatData::eSubtype_misc_feature);
        }
        if ( cfg.HideSNPFeatures() ) {
            sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_variation);
        }
        if ( cfg.HideExonFeatures() ) {
            sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_exon);
        }
        if ( cfg.HideIntronFeatures() ) {
            sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_intron);
        }
        if ( cfg.HideMiscFeatures() ) {
            sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_misc_feature);
        }
        if ( cfg.HideGapFeatures() ) {
            sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_gap);
        }
        if (ctx.IsNuc()) {
            sel.ExcludeFeatType(CSeqFeatData::e_Het);
        }
    }}
    // only for non-user selector
    if (ctx.GetAnnotSelector() == NULL) {
        sel.SetOverlapType(SAnnotSelector::eOverlap_Intervals);
        if (GetStrand(ctx.GetLocation(), &ctx.GetScope()) == eNa_strand_minus) {
            sel.SetSortOrder(SAnnotSelector::eSortOrder_Reverse);  // sort in reverse biological order
        } else {
            sel.SetSortOrder(SAnnotSelector::eSortOrder_Normal);
        }
        if (cfg.ShowContigFeatures()) {
            sel.SetResolveAll()
               .SetAdaptiveDepth(true);
        } else {
            sel.SetLimitTSE(ctx.GetHandle().GetTSE_Handle())
               .SetResolveTSE();
        }
    }
}


static bool s_SeqLocEndsOnBioseq(const CSeq_loc& loc, const CBioseq_Handle& seq)
{
    CSeq_loc_CI last;
    for ( CSeq_loc_CI it(loc); it; ++it ) {
        last = it;
    }
    return (last  &&  seq.IsSynonym(last.GetSeq_id()));
}

/* gcc warning: "defined but not used"
static CSeq_loc_Mapper* s_CreateMapper(CBioseqContext& ctx)
{
    if ( ctx.GetMapper() != 0 ) {
        return ctx.GetMapper();
    }
    const CFlatFileConfig& cfg = ctx.Config();

    // do not create mapper if:
    // 1 .segmented but not doing master style.
    if (ctx.IsSegmented()  &&  !cfg.IsStyleMaster()) {
        return 0;
    } else if (!ctx.IsSegmented()) {
        // 2. not delta, or delta and supress contig featuers
        if (!ctx.IsDelta()  ||  !cfg.ShowContigFeatures()) {
            return 0;
        }
    }

    // ... otherwise
    CSeq_loc_Mapper* mapper = new CSeq_loc_Mapper(ctx.GetHandle(),
        CSeq_loc_Mapper::eSeqMap_Up);
    if (mapper != NULL) {
        mapper->SetMergeAbutting();
        mapper->KeepNonmappingRanges();
    }
    return mapper;
}
*/

static bool s_CopyCDSFromCDNA(CBioseqContext& ctx)
{
    return ctx.IsInGPS()  &&  !ctx.IsInNucProt()  &&  ctx.Config().CopyCDSFromCDNA();
}

CSeqMap_CI s_CreateGapMapIter(const CSeq_loc& loc, CBioseqContext& ctx)
{
    CSeqMap_CI gap_it;

    // EMBL and DDBJ have gap imp features
    if (ctx.IsEMBL()  ||  ctx.IsDDBJ()) {
        return gap_it;
    }
    
    // do only near delta (for now)
    if (!ctx.IsDelta()  ||  !ctx.IsDeltaLitOnly()) {
        return gap_it;
    }

    if (ctx.Config().HideGapFeatures()) {
        return gap_it;
    }

    CConstRef<CSeqMap> seqmap = CSeqMap::CreateSeqMapForSeq_loc(loc, &ctx.GetScope());
    if (!seqmap) {
        ERR_POST_X(1, "Failed to create CSeqMap for gap iteration");
        return gap_it;
    }

    SSeqMapSelector sel;
    sel.SetFlags(CSeqMap::fFindGap)   // only iterate gaps
       .SetResolveCount(1);           // starting with a Seq-loc resolve 1 level
    gap_it = CSeqMap_CI(seqmap, &ctx.GetScope(), sel);

    return gap_it;
}


static CRef<CGapItem> s_NewGapItem(CSeqMap_CI& gap_it, CBioseqContext& ctx)
{
    TSeqPos pos     = gap_it.GetPosition();
    TSeqPos end_pos = gap_it.GetEndPosition();

    CRef<CGapItem> retval(gap_it.IsUnknownLength() ? 
        new CGapItem(pos, end_pos, ctx) :
        new CGapItem(pos, end_pos, gap_it.GetLength(), ctx));
    return retval;
}


static bool s_IsDuplicateFeatures(const CSeq_feat_Handle& f1, const CSeq_feat_Handle& f2)
{
    _ASSERT(f1  &&  f2);

    return !f1.IsTableSNP()  &&  !f2.IsTableSNP()       &&
           f1.GetFeatSubtype() == f2.GetFeatSubtype()   &&
           f1.GetLocation().Equals(f2.GetLocation())    &&
           f1.GetSeq_feat()->Equals(*f2.GetSeq_feat());
}


static string s_GetFeatDesc(const CSeq_feat_Handle& feat)
{
    string desc;
    feature::GetLabel(*feat.GetSeq_feat(), &desc, feature::eBoth, &feat.GetScope());

    // Add feature location part of label
    string loc_label;
    feat.GetLocation().GetLabel(&loc_label);
    if (loc_label.size() > 100) {
        loc_label.replace(97, NPOS, "...");
    }
    desc += loc_label;
    return desc.c_str();
}


void CFlatGatherer::x_GatherFeaturesOnLocation
(const CSeq_loc& loc,
 SAnnotSelector& sel,
 CBioseqContext& ctx) const
{
    CScope& scope = ctx.GetScope();
    CFlatItemOStream& out = *m_ItemOS;

    CSeqMap_CI gap_it = s_CreateGapMapIter(loc, ctx);

    CSeq_feat_Handle prev_feat;
    CConstRef<IFlatItem> item;
    for (CFeat_CI it(scope, loc, sel); it; ++it) {
        try {
            CSeq_feat_Handle feat = it->GetSeq_feat_Handle();
            const CSeq_feat& original_feat = it->GetOriginalFeature();

            // supress dupliacte features
            if (prev_feat  &&  s_IsDuplicateFeatures(prev_feat, feat)) {
                continue;
            }
            prev_feat = feat;

            CConstRef<CSeq_loc> feat_loc(&it->GetLocation());
        
            // make sure location ends on the current bioseq
            if (ctx.IsPart()) {
                if (!s_SeqLocEndsOnBioseq(*feat_loc, ctx.GetHandle())) {
                    // may need to map sig_peptide on a different segment
                    if (feat.GetData().IsCdregion()) {
                        if (!ctx.Config().IsFormatFTable()) {
                            x_GetFeatsOnCdsProduct(original_feat, *feat_loc, ctx);
                        }
                    }
                    continue;
                }
            }
        
            // handle gaps
            TSeqPos feat_start = feat_loc->GetStart(eExtreme_Positional);
            while (gap_it) {
                // if feature after gap first output the gap
                if (feat_start >= gap_it.GetPosition()) {
                    item.Reset( s_NewGapItem(gap_it, ctx) );
                    out << item;
                    ++gap_it;
                } else {
                    break;
                }
            }

            // format feature
            item.Reset( x_NewFeatureItem(original_feat, ctx, feat_loc) );
            out << item;

            // Add more features depending on user preferences
            switch (feat.GetFeatSubtype()) {
                case CSeqFeatData::eSubtype_mRNA:
                {{
                    // optionally map CDS from cDNA onto genomic
                    if (s_CopyCDSFromCDNA(ctx)   &&  feat.IsSetProduct()) {
                        x_CopyCDSFromCDNA(original_feat, ctx);
                    }
                    break;
                }}
                case CSeqFeatData::eSubtype_cdregion:
                    {{  
                        // map features from protein
                        if (!ctx.Config().IsFormatFTable()) {
                            x_GetFeatsOnCdsProduct(original_feat, *feat_loc, ctx);
                        }
                        break;
                    }}
                default:
                    break;
            }
        } catch (CException& e) {
            // post to log, go on to next feature
            LOG_POST_X(2, Error << "Error processing feature "
                                << s_GetFeatDesc(it->GetSeq_feat_Handle())
                                << " [" << e.what() << "]");
        }
    }  //  end of for loop

    // when all features are done, output remaining gaps
    while (gap_it) {
        item.Reset( s_NewGapItem(gap_it, ctx) );
        out << item;
        ++gap_it;
    }
}


void CFlatGatherer::x_CopyCDSFromCDNA
(const CSeq_feat& feat,
 CBioseqContext& ctx) const
{
    CScope& scope = ctx.GetScope();

    CBioseq_Handle cdna = scope.GetBioseqHandle(feat.GetProduct());
    if ( !cdna ) {
        return;
    }
    // NB: There is only one CDS on an mRNA
    CFeat_CI cds(cdna, CSeqFeatData::e_Cdregion);
    if ( cds ) {
        // map mRNA location to the genomic
        CSeq_loc_Mapper mapper(feat,
                               CSeq_loc_Mapper::eProductToLocation,
                               &scope);
        CRef<CSeq_loc> cds_loc = mapper.Map(cds->GetLocation());

        CConstRef<IFlatItem> item( 
            x_NewFeatureItem(cds->GetOriginalFeature(), ctx, cds_loc,
                CFeatureItem::eMapped_from_cdna) );
        *m_ItemOS << item;
    }
}


void CFlatGatherer::x_GatherFeatures(void) const
{
    CBioseqContext& ctx = *m_Current;
    const CFlatFileConfig& cfg = ctx.Config();
    CScope& scope = ctx.GetScope();
    CFlatItemOStream& out = *m_ItemOS;
    CConstRef<IFlatItem> item;

    SAnnotSelector sel;
    SAnnotSelector* selp = &sel;
    if (ctx.GetAnnotSelector() != NULL) {
        selp = &ctx.SetAnnotSelector();
    }
    s_SetSelection(*selp, ctx);

    // optionally map gene from genomic onto cDNA
    if ( ctx.IsInGPS()  &&  cfg.CopyGeneToCDNA()  &&
         ctx.GetBiomol() == CMolInfo::eBiomol_mRNA ) {
        const CSeq_feat* mrna = GetmRNAForProduct(ctx.GetHandle());
        if (mrna != NULL) {
            CConstRef<CSeq_feat> gene = GetBestGeneForMrna(*mrna, scope);
            if (gene) {
                CRef<CSeq_loc> loc(new CSeq_loc);
                loc->SetWhole(*ctx.GetPrimaryId());
                item.Reset( 
                    x_NewFeatureItem(*gene, ctx, loc, CFeatureItem::eMapped_from_genomic) );
                out << item;
            }
        }
    }

    CSeq_loc loc;
    if ( ctx.GetMasterLocation() != 0 ) {
        loc.Assign(*ctx.GetMasterLocation());
    } else {
        loc.Assign(*ctx.GetHandle().GetRangeSeq_loc(0, 0));
    }

    // collect features
    if ( ctx.IsSegmented()  &&  cfg.IsStyleMaster()  &&  cfg.OldFeaturesOrder() ) {
        if (ctx.GetAnnotSelector() == NULL) {
            selp->SetResolveNone();
        }
        
        // first do the master bioeseq
        x_GatherFeaturesOnLocation(loc, *selp, ctx);

        // map the location on the segments        
        CSeq_loc_Mapper mapper(1, ctx.GetHandle(),
                               CSeq_loc_Mapper::eSeqMap_Down);
        CRef<CSeq_loc> seg_loc(mapper.Map(loc));
        if ( seg_loc ) {
            // now go over each of the segments
            for ( CSeq_loc_CI it(*seg_loc); it; ++it ) {
                x_GatherFeaturesOnLocation(it.GetSeq_loc(), *selp, ctx);
            }
        }
    } else {
        x_GatherFeaturesOnLocation(loc, *selp, ctx);
    }
    
    if ( ctx.IsProt() ) {
        // Also collect features which this protein is their product.
        // Currently there are only two possible candidates: Coding regions
        // and Prot features (rare).
        
        // look for the Cdregion feature for this protein
        const CSeq_feat* cds = GetCDSForProduct(ctx.GetHandle());
        if ( cds != 0 ) {
            item.Reset(
                x_NewFeatureItem(*cds, ctx, &cds->GetProduct(), 
                    CFeatureItem::eMapped_from_cdna) );
            out << item;
        }

        // look for Prot features (only for RefSeq records or
        // GenBank not release_mode).
        if ( ctx.IsRefSeq()  ||  !cfg.ForGBRelease() ) {
            SAnnotSelector prod_sel(CSeqFeatData::e_Prot, true);
            prod_sel.SetLimitTSE(ctx.GetHandle().GetTopLevelEntry());
            prod_sel.SetResolveMethod(SAnnotSelector::eResolve_TSE);
            prod_sel.SetOverlapType(SAnnotSelector::eOverlap_Intervals);
            for (CFeat_CI it(ctx.GetHandle(), prod_sel); it; ++it) {  
                item.Reset( 
                    x_NewFeatureItem(it->GetOriginalFeature(),
                                        ctx,
                                        &it->GetProduct(),
                                        CFeatureItem::eMapped_from_prot) );
                out << item;
            }
        }
    }
}


static bool s_IsCDD(const CSeq_feat_Handle& feat)
{
    ITERATE(CSeq_feat::TDbxref, it, feat.GetDbxref()) {
        if ( (*it)->GetType() == CDbtag::eDbtagType_CDD ) {
            return true;
        }
    }
    return false;
}


SAnnotSelector s_GetCdsProductSel(CBioseqContext& ctx)
{
    SAnnotSelector sel;
    bool initialized = false;

    if (!initialized) {
        sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_region)
           .IncludeFeatSubtype(CSeqFeatData::eSubtype_site)
           .IncludeFeatSubtype(CSeqFeatData::eSubtype_bond)
           .IncludeFeatSubtype(CSeqFeatData::eSubtype_mat_peptide_aa)
           .IncludeFeatSubtype(CSeqFeatData::eSubtype_sig_peptide_aa)
           .IncludeFeatSubtype(CSeqFeatData::eSubtype_transit_peptide_aa)
           .IncludeFeatSubtype(CSeqFeatData::eSubtype_preprotein);
        initialized = true;
    }
    sel.SetLimitTSE(ctx.GetHandle().GetTSE_Handle());

    return sel;
}


void CFlatGatherer::x_GetFeatsOnCdsProduct
(const CSeq_feat& feat,
 const CSeq_loc& mapped_loc,
 CBioseqContext& ctx) const
{
    const CFlatFileConfig& cfg = ctx.Config();

    if (!feat.GetData().IsCdregion()  ||  !feat.CanGetProduct()) {
        return;
    }

    if (cfg.HideCDSProdFeatures()) {
        return;
    }

    CScope& scope = ctx.GetScope();
    CConstRef<CSeq_id> prot_id(feat.GetProduct().GetId());
    if (!prot_id) {
        return;
    }
    
    CBioseq_Handle  prot;

    prot = scope.GetBioseqHandleFromTSE(*prot_id, ctx.GetHandle());
    // !!! need a flag for fetching far proteins
    if (!prot) {
        return;
    }
    CFeat_CI it(prot, s_GetCdsProductSel(ctx));
    if (!it) {
        return;
    }

    // map from cds product to nucleotide
    CSeq_loc_Mapper prot_to_cds(feat, CSeq_loc_Mapper::eProductToLocation, &scope);
    
    CSeq_feat_Handle prev;  // keep track of the previous feature
    for ( ; it; ++it ) {
        CSeq_feat_Handle curr = it->GetSeq_feat_Handle();
        const CSeq_loc& curr_loc = curr.GetLocation();
        CSeqFeatData::ESubtype subtype = curr.GetFeatSubtype();

        if ( cfg.HideCDDFeatures()  &&
             subtype == CSeqFeatData::eSubtype_region  &&
             s_IsCDD(curr) ) {
            // passing this test prevents mapping of COG CDD region features
            continue;
        }

        // suppress duplicate features (on protein)
        if (prev  &&  s_IsDuplicateFeatures(curr, prev)) {
            continue;
        }

        // map prot location to nuc location
        CRef<CSeq_loc> loc(prot_to_cds.Map(curr_loc));
        if (loc) {
            if (loc->IsMix()  ||  loc->IsPacked_int()) {
                loc = Seq_loc_Merge(*loc, CSeq_loc::fMerge_Abutting, &scope);
            }
        }
        if (!loc  ||  loc->IsNull()) {
            continue;
        }
        if (ctx.IsPart()  &&  !s_SeqLocEndsOnBioseq(*loc, ctx.GetHandle())) {
            continue;
        }

        // make sure feature is within sublocation
        if ( ctx.GetMasterLocation() != 0 ) {
            const CSeq_loc& mloc = *ctx.GetMasterLocation();
            if ( Compare(mloc, *loc, &scope) != eContains ) {
                continue;
            }
        }
                    
        CConstRef<IFlatItem> item( x_NewFeatureItem(*curr.GetSeq_feat(), ctx, 
            loc, CFeatureItem::eMapped_from_prot) );
        *m_ItemOS << item;

        prev = curr;
    }    
}


/////////////////////////////////////////////////////////////////////////////
//
// ALIGNMENTS


void CFlatGatherer::x_GatherAlignments(void) const
{
    CBioseqContext&  ctx    = *m_Current;
    CSeq_loc_Mapper* mapper = ctx.GetMapper();
    CConstRef<IFlatItem> item;
    for (CAlign_CI it(ctx.GetScope(), ctx.GetLocation());  it;  ++it) {
        if (mapper) {
            item.Reset( new CAlignmentItem(*mapper->Map(*it), ctx) );
            *m_ItemOS << item;
        } else {
            item.Reset( new CAlignmentItem(const_cast<CSeq_align&>(*it), ctx) );
            *m_ItemOS << item;
        }
    }
}



END_SCOPE(objects)
END_NCBI_SCOPE
