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
* Author:  Clifford Clausen
*
* File Description:
*   Sequence utilities requiring CScope
*/

#include <ncbi_pch.hpp>
#include <serial/iterator.hpp>
#include <util/static_map.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_vector_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/impl/synonyms.hpp>

#include <objects/general/Int_fuzz.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/seqport_util.hpp>

#include <objects/seqloc/Packed_seqpnt.hpp>
#include <objects/seqloc/Seq_bond.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqloc/Seq_point.hpp>

#include <objects/seqset/Seq_entry.hpp>

#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Code_break.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqfeat/Genetic_code_table.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <util/strsearch.hpp>

#include <list>
#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(sequence)


const COrg_ref& GetOrg_ref(const CBioseq_Handle& handle)
{
    {{
        CSeqdesc_CI desc(handle, CSeqdesc::e_Source);
        if (desc) {
            return desc->GetSource().GetOrg();
        }
    }}

    {{
        CSeqdesc_CI desc(handle, CSeqdesc::e_Org);
        if (desc) {
            return desc->GetOrg();
        }
    }}

    NCBI_THROW(CException, eUnknown, "No organism set");
}


int GetTaxId(const CBioseq_Handle& handle)
{
    try {
        return GetOrg_ref(handle).GetTaxId();
    }
    catch (...) {
        return 0;
    }
}

const CMolInfo* GetMolInfo(const CBioseq_Handle& handle)
{
    CSeqdesc_CI desc_iter(handle, CSeqdesc::e_Molinfo);
    for ( ;  desc_iter;  ++desc_iter) {
        return &desc_iter->GetMolinfo();
    }

    return NULL;
}



CBioseq_Handle GetBioseqFromSeqLoc
(const CSeq_loc& loc,
 CScope& scope,
 CScope::EGetBioseqFlag flag)
{
    CBioseq_Handle retval;

    try {
        if (IsOneBioseq(loc, &scope)) {
            return scope.GetBioseqHandle(GetId(loc, &scope), flag);
        } 

        // assuming location is annotated on parts of a segmented bioseq
        for (CSeq_loc_CI it(loc); it; ++it) {
            CBioseq_Handle part = scope.GetBioseqHandle(it.GetSeq_id(), flag);
            if (part) {
                retval = GetParentForPart(part);
            }
            break;  // check only the first part
        }

        // if multiple intervals and not parts, look for the first loaded bioseq
        if (!retval) {
            for (CSeq_loc_CI it(loc); it; ++it) {
                retval = 
                    scope.GetBioseqHandle(it.GetSeq_id_Handle(), CScope::eGetBioseq_Loaded);
                if (retval) {
                    break;
                }
            }
        }

        if (!retval  &&  flag == CScope::eGetBioseq_All) {
            for (CSeq_loc_CI it(loc); it; ++it) {
                retval = 
                    scope.GetBioseqHandle(it.GetSeq_id_Handle(), flag);
                if (retval) {
                    break;
                }
            }
        }
    } catch (CException&) {
        retval.Reset();
    }

    return retval;
}


class CSeqIdFromHandleException : EXCEPTION_VIRTUAL_BASE public CException
{
public:
    // Enumerated list of document management errors
    enum EErrCode {
        eNoSynonyms,
        eRequestedIdNotFound
    };

    // Translate the specific error code into a string representations of
    // that error code.
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eNoSynonyms:           return "eNoSynonyms";
        case eRequestedIdNotFound:  return "eRequestedIdNotFound";
        default:                    return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CSeqIdFromHandleException, CException);
};


int ScoreSeqIdHandle(const CSeq_id_Handle& idh)
{
    CConstRef<CSeq_id> id = idh.GetSeqId();
    CRef<CSeq_id> id_non_const
        (const_cast<CSeq_id*>(id.GetPointer()));
    return CSeq_id::Score(id_non_const);
}


CSeq_id_Handle x_GetId(const CScope::TIds& ids, EGetIdType type)
{
    if ( ids.empty() ) {
        return CSeq_id_Handle();
    }

    switch (type) {
    case eGetId_ForceGi:
        {{
            ITERATE (CScope::TIds, iter, ids) {
                if (iter->IsGi()) {
                    return *iter;
                }
            }
        }}
        NCBI_THROW(CSeqIdFromHandleException, eRequestedIdNotFound,
                "sequence::GetId(): gi seq-id not found in the list");
        break;

    case eGetId_ForceAcc:
        {{
            CSeq_id_Handle best = x_GetId(ids, eGetId_Best);
            if (best  &&
                best.GetSeqId()->GetTextseq_Id() != NULL  &&
                best.GetSeqId()->GetTextseq_Id()->IsSetAccession()) {
                return best;
            }
        }}
        NCBI_THROW(CSeqIdFromHandleException, eRequestedIdNotFound,
                "sequence::GetId(): text seq-id not found in the list");
        break;

    case eGetId_Best:
        {{
            return FindBestChoice(ids, ScoreSeqIdHandle);
        }}

    default:
        break;
    }
    return CSeq_id_Handle();
}


CSeq_id_Handle GetId(const CSeq_id& id, CScope& scope, EGetIdType type)
{
    return GetId(CSeq_id_Handle::GetHandle(id), scope, type);
}


CSeq_id_Handle GetId(const CSeq_id_Handle& idh, CScope& scope,
                     EGetIdType type)
{
    if ( type == eGetId_ForceGi  &&  idh.IsGi() ) {
        return idh;
    }
    CScope::TIds ids = scope.GetIds(idh);
    CSeq_id_Handle ret = x_GetId(ids, type);
    return ret ? ret : idh;
}


CSeq_id_Handle GetId(const CBioseq_Handle& handle,
                     EGetIdType type)
{
    _ASSERT(handle);

    const CScope::TIds& ids = handle.GetId();
    CSeq_id_Handle idh = x_GetId(ids, type);

    if ( !idh ) {
        NCBI_THROW(CSeqIdFromHandleException, eRequestedIdNotFound,
                   "Unable to get Seq-id from handle");
    }

    return idh;
}


int GetGiForAccession(const string& acc, CScope& scope)
{
    try {
        CSeq_id acc_id(acc);
        return GetId(acc_id, scope, eGetId_ForceGi).GetGi();
    } catch (CException& e) {
         ERR_POST(Warning << e.what());
    }

    return 0;
}


string GetAccessionForGi
(int           gi,
 CScope&       scope,
 EAccessionVersion use_version)
{
    bool with_version = (use_version == eWithAccessionVersion);

    try {
        CSeq_id gi_id(CSeq_id::e_Gi, gi);
        return GetId(gi_id, scope, eGetId_ForceAcc).GetSeqId()->
            GetSeqIdString(with_version);
    } catch (CException& e) {
        ERR_POST(Warning << e.what());
    }

    return kEmptyStr;
}


CRef<CSeq_loc> SourceToProduct(const CSeq_feat& feat,
                               const CSeq_loc& source_loc, TS2PFlags flags,
                               CScope* scope, int* frame)
{
    SRelLoc::TFlags rl_flags = 0;
    if (flags & fS2P_NoMerge) {
        rl_flags |= SRelLoc::fNoMerge;
    }
    SRelLoc rl(feat.GetLocation(), source_loc, scope, rl_flags);
    _ASSERT(!rl.m_Ranges.empty());
    rl.m_ParentLoc.Reset(&feat.GetProduct());
    if (feat.GetData().IsCdregion()) {
        // 3:1 ratio
        const CCdregion& cds         = feat.GetData().GetCdregion();
        int              base_frame  = cds.GetFrame();
        if (base_frame > 0) {
            --base_frame;
        }
        if (frame) {
            *frame = 3 - (rl.m_Ranges.front()->GetFrom() + 2 - base_frame) % 3;
        }
        TSeqPos prot_length;
        try {
            prot_length = GetLength(feat.GetProduct(), scope);
        } catch (CObjmgrUtilException) {
            prot_length = numeric_limits<TSeqPos>::max();
        }
        NON_CONST_ITERATE (SRelLoc::TRanges, it, rl.m_Ranges) {
            if (IsReverse((*it)->GetStrand())) {
                ERR_POST(Warning
                         << "SourceToProduct:"
                         " parent and child have opposite orientations");
            }
            (*it)->SetFrom(((*it)->GetFrom() - base_frame) / 3);
            (*it)->SetTo  (((*it)->GetTo()   - base_frame) / 3);
            if ((flags & fS2P_AllowTer)  &&  (*it)->GetTo() == prot_length) {
                --(*it)->SetTo();
            }
        }
    } else {
        if (frame) {
            *frame = 0; // not applicable; explicitly zero
        }
    }

    return rl.Resolve(scope, rl_flags);
}


CRef<CSeq_loc> ProductToSource(const CSeq_feat& feat, const CSeq_loc& prod_loc,
                               TP2SFlags flags, CScope* scope)
{
    SRelLoc rl(feat.GetProduct(), prod_loc, scope);
    _ASSERT(!rl.m_Ranges.empty());
    rl.m_ParentLoc.Reset(&feat.GetLocation());
    if (feat.GetData().IsCdregion()) {
        // 3:1 ratio
        const CCdregion& cds        = feat.GetData().GetCdregion();
        int              base_frame = cds.GetFrame();
        if (base_frame > 0) {
            --base_frame;
        }
        TSeqPos nuc_length, prot_length;
        try {
            nuc_length = GetLength(feat.GetLocation(), scope);
        } catch (CObjmgrUtilException) {
            nuc_length = numeric_limits<TSeqPos>::max();
        }
        try {
            prot_length = GetLength(feat.GetProduct(), scope);
        } catch (CObjmgrUtilException) {
            prot_length = numeric_limits<TSeqPos>::max();
        }
        NON_CONST_ITERATE(SRelLoc::TRanges, it, rl.m_Ranges) {
            _ASSERT( !IsReverse((*it)->GetStrand()) );
            TSeqPos from, to;
            if ((flags & fP2S_Extend)  &&  (*it)->GetFrom() == 0) {
                from = 0;
            } else {
                from = (*it)->GetFrom() * 3 + base_frame;
            }
            if ((flags & fP2S_Extend)  &&  (*it)->GetTo() == prot_length - 1) {
                to = nuc_length - 1;
            } else {
                to = (*it)->GetTo() * 3 + base_frame + 2;
            }
            (*it)->SetFrom(from);
            (*it)->SetTo  (to);
        }
    }

    return rl.Resolve(scope);
}


typedef pair<Int8, CConstRef<CSeq_feat> > TFeatScore;
typedef vector<TFeatScore> TFeatScores;

template <class T, class U>
struct SPairLessByFirst
    : public binary_function< pair<T,U>, pair<T,U>, bool >
{
    bool operator()(const pair<T,U>& p1, const pair<T,U>& p2) const
    {
        return p1.first < p2.first;
    }
};

template <class T, class U>
struct SPairLessBySecond
    : public binary_function< pair<T,U>, pair<T,U>, bool >
{
    bool operator()(const pair<T,U>& p1, const pair<T,U>& p2) const
    {
        return p1.second < p2.second;
    }
};

static
void x_GetBestOverlappingFeat(const CSeq_loc& loc,
                              CSeqFeatData::E_Choice feat_type,
                              CSeqFeatData::ESubtype feat_subtype,
                              EOverlapType overlap_type,
                              TFeatScores& feats,
                              CScope& scope)
{
    bool revert_locations = false;
    SAnnotSelector::EOverlapType annot_overlap_type;
    switch (overlap_type) {
    case eOverlap_Simple:
    case eOverlap_Contained:
    case eOverlap_Contains:
        // Require total range overlap
        annot_overlap_type = SAnnotSelector::eOverlap_TotalRange;
        break;
    case eOverlap_Subset:
    case eOverlap_CheckIntervals:
    case eOverlap_Interval:
    case eOverlap_CheckIntRev:
        revert_locations = true;
        // there's no break here - proceed to "default"
    default:
        // Require intervals overlap
        annot_overlap_type = SAnnotSelector::eOverlap_Intervals;
        break;
    }

    CConstRef<CSeq_feat> feat_ref;

    // Check if the sequence is circular
    TSeqPos circular_length = kInvalidSeqPos;
    try {
        const CSeq_id* single_id = 0;
        try {
            loc.CheckId(single_id);
        }
        catch (CException&) {
            single_id = 0;
        }
        if ( single_id ) {
            CBioseq_Handle h = scope.GetBioseqHandle(*single_id);
            if ( h
                &&  h.IsSetInst_Topology()
                &&  h.GetInst_Topology() == CSeq_inst::eTopology_circular ) {
                circular_length = h.GetBioseqLength();
            }
        }
    }
    catch (CException& _DEBUG_ARG(e)) {
        _TRACE("test for circularity failed: " << e.GetMsg());
    }

    try {
        SAnnotSelector sel;
        sel.SetFeatType(feat_type)
            .SetFeatSubtype(feat_subtype)
            .SetOverlapType(annot_overlap_type)
            .SetResolveTSE();
        CFeat_CI feat_it(scope, loc, sel);
        for ( ;  feat_it;  ++feat_it) {
            // treat subset as a special case
            Int8 cur_diff = ( !revert_locations ) ?
                TestForOverlap(feat_it->GetLocation(),
                               loc,
                               overlap_type,
                               circular_length,
                               &scope) :
                TestForOverlap(loc,
                               feat_it->GetLocation(),
                               overlap_type,
                               circular_length,
                               &scope);
            if (cur_diff < 0) {
                continue;
            }

            TFeatScore sc(cur_diff,
                          CConstRef<CSeq_feat>(&feat_it->GetMappedFeature()));
            feats.push_back(sc);
        }
    }
    catch (CException&) {
        _TRACE("x_GetBestOverlappingFeat(): error: feature iterator failed");
    }

    std::sort(feats.begin(), feats.end(),
              SPairLessByFirst< Int8, CConstRef<CSeq_feat> >());
}


CConstRef<CSeq_feat> GetBestOverlappingFeat(const CSeq_loc& loc,
                                            CSeqFeatData::E_Choice feat_type,
                                            EOverlapType overlap_type,
                                            CScope& scope,
                                            TBestFeatOpts opts)
{
    TFeatScores scores;
    x_GetBestOverlappingFeat(loc,
                             feat_type, CSeqFeatData::eSubtype_any,
                             overlap_type, scores, scope);
    if (scores.size()) {
        if (opts & fBestFeat_FavorLonger) {
            return scores.back().second;
        } else {
            return scores.front().second;
        }
    }
    return CConstRef<CSeq_feat>();
}


CConstRef<CSeq_feat> GetBestOverlappingFeat(const CSeq_loc& loc,
                                            CSeqFeatData::ESubtype feat_type,
                                            EOverlapType overlap_type,
                                            CScope& scope,
                                            TBestFeatOpts opts)
{
    TFeatScores scores;
    x_GetBestOverlappingFeat(loc,
        CSeqFeatData::GetTypeFromSubtype(feat_type), feat_type,
        overlap_type, scores, scope);

    if (scores.size()) {
        if (opts & fBestFeat_FavorLonger) {
            return scores.back().second;
        } else {
            return scores.front().second;
        }
    }
    return CConstRef<CSeq_feat>();
}


static
CConstRef<CSeq_feat> x_GetBestOverlapForSNP(const CSeq_feat& snp_feat,
                                            CSeqFeatData::E_Choice type,
                                            CSeqFeatData::ESubtype subtype,
                                            CScope& scope,
                                            bool search_both_strands = true)
{
    TFeatScores scores;
    CConstRef<CSeq_feat> overlap;
    x_GetBestOverlappingFeat(snp_feat.GetLocation(),
                             type, subtype,
                             eOverlap_Contains, scores,
                             scope);
    if (scores.size()) {
        overlap = scores.front().second;
    }

    if (search_both_strands  &&  !overlap) {
        CRef<CSeq_loc> loc(new CSeq_loc);
        loc->Assign(snp_feat.GetLocation());

        ENa_strand strand = GetStrand(*loc, &scope);
        if (strand == eNa_strand_plus  ||  strand == eNa_strand_minus) {
            loc->FlipStrand();
        } else if (strand == eNa_strand_unknown) {
            loc->SetStrand(eNa_strand_minus);
        }

        scores.clear();
        x_GetBestOverlappingFeat(*loc,
                                 type, subtype,
                                 eOverlap_Contains, scores,
                                 scope);
        if (scores.size()) {
            overlap = scores.front().second;
        }
    }

    return overlap;
}


CConstRef<CSeq_feat> GetBestOverlapForSNP(const CSeq_feat& snp_feat,
                                          CSeqFeatData::E_Choice type,
                                          CScope& scope,
                                          bool search_both_strands)
{
    return x_GetBestOverlapForSNP(snp_feat, type, CSeqFeatData::eSubtype_any,
                                  scope, search_both_strands);
}


CConstRef<CSeq_feat> GetBestOverlapForSNP(const CSeq_feat& snp_feat,
                                          CSeqFeatData::ESubtype subtype,
                                          CScope& scope,
                                          bool search_both_strands)
{
    return x_GetBestOverlapForSNP(snp_feat,
        CSeqFeatData::GetTypeFromSubtype(subtype), subtype, scope,
        search_both_strands);
}


CConstRef<CSeq_feat> GetOverlappingGene(const CSeq_loc& loc, CScope& scope)
{
    return GetBestOverlappingFeat(loc, CSeqFeatData::eSubtype_gene,
                                  eOverlap_Contains, scope);
}


CConstRef<CSeq_feat> GetOverlappingmRNA(const CSeq_loc& loc, CScope& scope)
{
    return GetBestOverlappingFeat(loc, CSeqFeatData::eSubtype_mRNA,
                                  eOverlap_Contains, scope);
}


CConstRef<CSeq_feat> GetOverlappingCDS(const CSeq_loc& loc, CScope& scope)
{
    return GetBestOverlappingFeat(loc, CSeqFeatData::eSubtype_cdregion,
                                  eOverlap_Contains, scope);
}


CConstRef<CSeq_feat> GetOverlappingPub(const CSeq_loc& loc, CScope& scope)
{
    return GetBestOverlappingFeat(loc, CSeqFeatData::eSubtype_pub,
                                  eOverlap_Contains, scope);
}


CConstRef<CSeq_feat> GetOverlappingSource(const CSeq_loc& loc, CScope& scope)
{
    return GetBestOverlappingFeat(loc, CSeqFeatData::eSubtype_biosrc,
                                  eOverlap_Contains, scope);
}


CConstRef<CSeq_feat> GetOverlappingOperon(const CSeq_loc& loc, CScope& scope)
{
    return GetBestOverlappingFeat(loc, CSeqFeatData::eSubtype_operon,
                                  eOverlap_Contains, scope);
}


CConstRef<CSeq_feat> GetBestMrnaForCds(const CSeq_feat& cds_feat,
                                       CScope& scope,
                                       TBestFeatOpts opts)
{
    _ASSERT(cds_feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_cdregion);
    CConstRef<CSeq_feat> mrna_feat;

    // search for a best overlapping mRNA
    // we start with a scan through the product accessions because we need
    // to insure that the chosen transcript does indeed match what we want
    TFeatScores feats;
    x_GetBestOverlappingFeat(cds_feat.GetLocation(),
                             CSeqFeatData::e_Rna,
                             CSeqFeatData::eSubtype_mRNA,
                             eOverlap_CheckIntRev,
                             feats, scope);
    /// easy out: 0 or 1 possible features
    if (feats.size() < 2) {
        if (feats.size() == 1) {
            mrna_feat = feats.front().second;
        }
        return mrna_feat;
    }

    if (cds_feat.IsSetProduct()) {
        try {
            // this may throw, if the product spans multiple sequences
            // this would be extremely unlikely, but we catch anyway
            const CSeq_id& product_id =
                sequence::GetId(cds_feat.GetProduct(), &scope);

            ITERATE (TFeatScores, feat_iter, feats) {
                const CSeq_feat& feat = *feat_iter->second;
                if ( !feat.IsSetExt() ) {
                    continue;
                }

                /// scan the user object in the ext field
                /// we look for a user object of type MrnaProteinLink
                /// this should contain a seq-d string that we can match
                CTypeConstIterator<CUser_object> obj_iter(feat);
                for ( ;  obj_iter;  ++obj_iter) {
                    if (obj_iter->IsSetType()  &&
                        obj_iter->GetType().IsStr()  &&
                        obj_iter->GetType().GetStr() == "MrnaProteinLink") {
                        string prot_id_str = obj_iter->GetField("protein seqID")
                            .GetData().GetStr();
                        CSeq_id prot_id(prot_id_str);
                        vector<CSeq_id_Handle> ids = scope.GetIds(prot_id);
                        ids.push_back(CSeq_id_Handle::GetHandle(prot_id));
                        ITERATE (vector<CSeq_id_Handle>, id_iter, ids) {
                            if (product_id.Match(*id_iter->GetSeqId())) {
                                mrna_feat.Reset(&feat);
                                return mrna_feat;
                            }
                        }
                    }
                }
            }
        }
        catch (CException&) {
        }
    }

    if (cds_feat.IsSetProduct()  &&  !(opts & fBestFeat_NoExpensive) ) {
        try {
            // this may throw, if the product spans multiple sequences
            // this would be extremely unlikely, but we catch anyway
            const CSeq_id& product_id =
                sequence::GetId(cds_feat.GetProduct(), &scope);

            ITERATE (TFeatScores, feat_iter, feats) {

                // we grab the mRNA product, if available, and scan it for
                // a CDS feature.  the CDS feature should point to the same
                // product as our current feature.
                const CSeq_feat& mrna = *feat_iter->second;
                if ( !mrna.IsSetProduct() ) {
                    continue;
                }

                CBioseq_Handle handle =
                    scope.GetBioseqHandle(mrna.GetProduct());
                if ( !handle ) {
                    continue;
                }

                SAnnotSelector cds_sel;
                cds_sel.SetOverlapIntervals()
                    .ExcludeNamedAnnots("SNP")
                    .SetResolveTSE()
                    .SetFeatSubtype(CSeqFeatData::eSubtype_cdregion);
                CFeat_CI other_iter(scope, mrna.GetProduct(), cds_sel);
                for ( ;  other_iter  &&  !mrna_feat;  ++other_iter) {
                    const CSeq_feat& cds = other_iter->GetOriginalFeature();
                    if ( !cds.IsSetProduct() ) {
                        continue;
                    }

                    CBioseq_Handle prot_handle =
                        scope.GetBioseqHandle(cds.GetProduct());
                    if ( !prot_handle ) {
                        continue;
                    }

                    if (prot_handle.IsSynonym(product_id)) {
                        // got it!
                        mrna_feat.Reset(&mrna);
                        return mrna_feat;
                    }
                }
            }
        }
        catch (CException&) {
        }
    }

    // check for transcript_id; this is a fast check
    string transcript_id = cds_feat.GetNamedQual("transcript_id");
    if ( !transcript_id.empty() ) {
        ITERATE (vector<TFeatScore>, feat_iter, feats) {
            const CSeq_feat& feat = *feat_iter->second;
            string other_transcript_id =
                feat.GetNamedQual("transcript_id");
            if (transcript_id == other_transcript_id) {
                mrna_feat.Reset(&feat);
                return mrna_feat;
            }
        }
    }

    //
    // try to find the best by overlaps alone
    //

    if ( !mrna_feat  &&  !(opts & fBestFeat_StrictMatch) ) {
        if (opts & fBestFeat_FavorLonger) {
            mrna_feat = feats.back().second;
        } else {
            mrna_feat = feats.front().second;
        }
    }

    return mrna_feat;
}


CConstRef<CSeq_feat>
GetBestCdsForMrna(const CSeq_feat& mrna_feat,
                  CScope& scope,
                  TBestFeatOpts opts)
{
    _ASSERT(mrna_feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA);
    CConstRef<CSeq_feat> cds_feat;

    // search for a best overlapping CDS
    // we start with a scan through the product accessions because we need
    // to insure that the chosen transcript does indeed match what we want
    TFeatScores feats;
    x_GetBestOverlappingFeat(mrna_feat.GetLocation(),
                             CSeqFeatData::e_Cdregion,
                             CSeqFeatData::eSubtype_cdregion,
                             eOverlap_CheckIntervals,
                             feats, scope);

    /// easy out: 0 or 1 possible features
    if (feats.size() < 2) {
        if (feats.size() == 1) {
            cds_feat = feats.front().second;
        }
        return cds_feat;
    }

    if (mrna_feat.IsSetExt()) {
        /// scan the user object in the ext field
        /// we look for a user object of type MrnaProteinLink
        /// this should contain a seq-d string that we can match
        string prot_id_str;
        CTypeConstIterator<CUser_object> obj_iter(mrna_feat);
        for ( ;  obj_iter;  ++obj_iter) {
            if (obj_iter->IsSetType()  &&
                obj_iter->GetType().IsStr()  &&
                obj_iter->GetType().GetStr() == "MrnaProteinLink") {
                string prot_id_str = obj_iter->GetField("protein seqID")
                    .GetData().GetStr();
                break;
            }
        }
        if ( !prot_id_str.empty() ) {
            CSeq_id prot_id(prot_id_str);
            vector<CSeq_id_Handle> ids = scope.GetIds(prot_id);
            ids.push_back(CSeq_id_Handle::GetHandle(prot_id));

            try {
                /// look for a CDS feature that matches this expected ID
                ITERATE (TFeatScores, feat_iter, feats) {
                    const CSeq_feat& feat = *feat_iter->second;
                    if ( !feat.IsSetProduct() ) {
                        continue;
                    }
                    const CSeq_id& id =
                        sequence::GetId(feat.GetLocation(), &scope);
                    ITERATE (vector<CSeq_id_Handle>, id_iter, ids) {
                        if (id.Match(*id_iter->GetSeqId())) {
                            cds_feat.Reset(&feat);
                            return cds_feat;
                        }
                    }
                }
            }
            catch (CException&) {
            }
        }
    }

    // scan through the product accessions because we need to insure that the
    // chosen transcript does indeed match what we want
    if (mrna_feat.IsSetProduct()  &&  !(opts & fBestFeat_NoExpensive) ) {
        do {
            try {
                // this may throw, if the product spans multiple sequences
                // this would be extremely unlikely, but we catch anyway
                const CSeq_id& mrna_product  =
                    sequence::GetId(mrna_feat.GetProduct(), &scope);
                CBioseq_Handle mrna_handle =
                    scope.GetBioseqHandle(mrna_product);

                // find the ID of the protein accession we're looking for
                CConstRef<CSeq_id> protein_id;
                {{
                    SAnnotSelector sel;
                    sel.SetOverlapIntervals()
                        .ExcludeNamedAnnots("SNP")
                        .SetResolveTSE()
                        .SetFeatSubtype(CSeqFeatData::eSubtype_cdregion);

                     CFeat_CI iter(mrna_handle, sel);
                     for ( ;  iter;  ++iter) {
                         if (iter->IsSetProduct()) {
                             protein_id.Reset
                                 (&sequence::GetId(iter->GetProduct(),
                                 &scope));
                             break;
                         }
                     }
                 }}

                if ( !protein_id ) {
                    break;
                }

                TFeatScores::const_iterator feat_iter = feats.begin();
                TFeatScores::const_iterator feat_end  = feats.end();
                for ( ;  feat_iter != feat_end  &&  !cds_feat;  ++feat_iter) {
                    /// look for all contained CDS features; for each, check
                    /// to see if the protein product is the expected protein
                    /// product
                    const CSeq_feat& cds = *feat_iter->second;
                    if ( !cds.IsSetProduct() ) {
                        continue;
                    }

                    CBioseq_Handle prot_handle =
                        scope.GetBioseqHandle(cds.GetProduct());
                    if ( !prot_handle ) {
                        continue;
                    }

                    if (prot_handle.IsSynonym(*protein_id)) {
                        // got it!
                        cds_feat.Reset(&cds);
                        return cds_feat;
                    }
                }
            }
            catch (...) {
            }
        }
        while (false);
    }

    // check for transcript_id
    // this is generally only available in GTF/GFF-imported features
    string transcript_id = mrna_feat.GetNamedQual("transcript_id");
    if ( !transcript_id.empty() ) {
        ITERATE (TFeatScores, feat_iter, feats) {
            const CSeq_feat& feat = *feat_iter->second;
            string other_transcript_id =
                feat.GetNamedQual("transcript_id");
            if (transcript_id == other_transcript_id) {
                cds_feat.Reset(&feat);
                return cds_feat;
            }
        }
    }

    //
    // try to find the best by overlaps alone
    //

    if ( !cds_feat  &&  !(opts & fBestFeat_StrictMatch) ) {
        if (opts & fBestFeat_FavorLonger) {
            cds_feat = feats.back().second;
        } else {
            cds_feat = feats.front().second;
        }
    }

    return cds_feat;
}


CConstRef<CSeq_feat> GetBestGeneForMrna(const CSeq_feat& mrna_feat,
                                          CScope& scope,
                                          TBestFeatOpts opts)
{
    _ASSERT(mrna_feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA);
    CConstRef<CSeq_feat> gene_feat;

    // search for a best overlapping gene
    TFeatScores feats;
    x_GetBestOverlappingFeat(mrna_feat.GetLocation(),
                             CSeqFeatData::e_Gene,
                             CSeqFeatData::eSubtype_any,
                             eOverlap_CheckIntRev,
                             feats, scope);
    /// easy out: 0 or 1 possible features
    if (feats.size() < 2) {
        if (feats.size() == 1) {
            gene_feat = feats.front().second;
        }
        return gene_feat;
    }

    ///
    /// compare gene xrefs to see if ew can find a match
    ///
    const CGene_ref* ref = mrna_feat.GetGeneXref();
    if (ref) {
        if (ref->IsSuppressed()) {
            /// 'suppress' case
            return gene_feat;
        }

        string ref_str;
        ref->GetLabel(&ref_str);

        ITERATE (TFeatScores, feat_it, feats) {
            const CSeq_feat& feat      = *feat_it->second;
            const CGene_ref& other_ref = feat.GetData().GetGene();
            string other_ref_str;
            other_ref.GetLabel(&other_ref_str);
            if (ref_str == other_ref_str) {
                gene_feat = &feat;
                return gene_feat;
            }
        }
    }

    ///
    /// compare by dbxrefs
    ///
    if (mrna_feat.IsSetDbxref()) {
        int gene_id = 0;
        ITERATE (CSeq_feat::TDbxref, dbxref, mrna_feat.GetDbxref()) {
            if ((*dbxref)->GetDb() == "GeneID"  ||
                (*dbxref)->GetDb() == "LocusID") {
                gene_id = (*dbxref)->GetTag().GetId();
                break;
            }
        }

        if (gene_id != 0) {
            ITERATE (TFeatScores, feat_it, feats) {
                const CSeq_feat& feat = *feat_it->second;
                ITERATE (CSeq_feat::TDbxref, dbxref, feat.GetDbxref()) {
                    const string& db = (*dbxref)->GetDb();
                    if ((db == "GeneID"  ||  db == "LocusID")  &&
                        (*dbxref)->GetTag().GetId() == gene_id) {
                        gene_feat = &feat;
                        return gene_feat;
                    }
                }
            }
        }
    }

    if ( !gene_feat  &&  !(opts & fBestFeat_StrictMatch) ) {
        if (opts & fBestFeat_FavorLonger) {
            gene_feat = feats.back().second;
        } else {
            gene_feat = feats.front().second;
        }
    }

    return gene_feat;
}


CConstRef<CSeq_feat> GetBestGeneForCds(const CSeq_feat& cds_feat,
                                         CScope& scope,
                                         TBestFeatOpts opts)
{
    _ASSERT(cds_feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_cdregion);

    CConstRef<CSeq_feat> feat_ref;

    // search for a best overlapping gene
    TFeatScores feats;
    x_GetBestOverlappingFeat(cds_feat.GetLocation(),
                             CSeqFeatData::e_Gene,
                             CSeqFeatData::eSubtype_any,
                             eOverlap_CheckIntRev,
                             feats, scope);
    /// easy out: 0 or 1 possible features
    if (feats.size() < 2) {
        if (feats.size() == 1) {
            feat_ref = feats.front().second;
        }
        return feat_ref;
    }

    // next: see if we can match based on gene xref
    const CGene_ref* ref = cds_feat.GetGeneXref();
    if (ref) {
        if (ref->IsSuppressed()) {
            /// 'suppress' case
            return feat_ref;
        }

        string ref_str;
        ref->GetLabel(&ref_str);

        ITERATE (TFeatScores, feat_it, feats) {
            const CSeq_feat& feat = *feat_it->second;

            string ref_str;
            ref->GetLabel(&ref_str);

            const CGene_ref& other_ref = feat.GetData().GetGene();
            string other_ref_str;
            other_ref.GetLabel(&other_ref_str);
            if (ref_str == other_ref_str) {
                feat_ref = &feat;
                return feat_ref;
            }
        }
    }

    /// last check: expensive: need to proxy through mRNA match
    if ( !feat_ref  &&  !(opts & fBestFeat_NoExpensive) ) {
        feat_ref = GetBestMrnaForCds(cds_feat, scope,
                                     opts | fBestFeat_StrictMatch);
        if (feat_ref) {
            feat_ref = GetBestGeneForMrna(*feat_ref, scope, opts);
            if (feat_ref) {
                return feat_ref;
            }
        }
    }

    if ( !feat_ref  &&  !(opts & fBestFeat_StrictMatch) ) {
        feat_ref = feats.front().second;
    }
    return feat_ref;
}


void GetMrnasForGene(const CSeq_feat& gene_feat, CScope& scope,
                     list< CConstRef<CSeq_feat> >& mrna_feats,
                     TBestFeatOpts opts)
{
    _ASSERT(gene_feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_gene);
    SAnnotSelector sel;
    sel.SetResolveTSE()
        .SetAdaptiveDepth()
        .IncludeFeatSubtype(CSeqFeatData::eSubtype_mRNA);
    CFeat_CI feat_it(scope, gene_feat.GetLocation(), sel);
    if (feat_it.GetSize() == 0) {
        return;
    }

    ///
    /// pass 1: compare by gene xref
    ///
    {{
         const CGene_ref& ref = gene_feat.GetData().GetGene();
         string ref_str;
         ref.GetLabel(&ref_str);
         size_t count = 0;
         for ( ;  feat_it;  ++feat_it) {

             const CGene_ref* other_ref =
                 feat_it->GetOriginalFeature().GetGeneXref();
             if ( !other_ref  ||  other_ref->IsSuppressed() ) {
                 continue;
             }

             string other_ref_str;
             other_ref->GetLabel(&other_ref_str);
             if (other_ref_str != ref_str) {
                 continue;
             }

             ECompare comp = sequence::Compare(gene_feat.GetLocation(),
                                               feat_it->GetLocation(),
                                               &scope);
             if (comp != eSame  &&  comp != eContains) {
                 continue;
             }

             CConstRef<CSeq_feat> feat_ref(&feat_it->GetOriginalFeature());
             mrna_feats.push_back(feat_ref);
             ++count;
         }

         if (count) {
             return;
         }
     }}

    ///
    /// pass 2: compare by gene id
    ///
    {{
        int gene_id = 0;
        if (gene_feat.IsSetDbxref()) {
            ITERATE (CSeq_feat::TDbxref, dbxref, gene_feat.GetDbxref()) {
                if ((*dbxref)->GetDb() == "GeneID"  ||
                    (*dbxref)->GetDb() == "LocusID") {
                    gene_id = (*dbxref)->GetTag().GetId();
                    break;
                }
            }
        }

        if (gene_id) {
            size_t count = 0;
            feat_it.Rewind();
            for ( ;  feat_it;  ++feat_it) {
                /// check the suppress case
                /// regardless of the gene-id binding, we always ignore these
                const CGene_ref* other_ref =
                    feat_it->GetOriginalFeature().GetGeneXref();
                if ( other_ref  &&  other_ref->IsSuppressed() ) {
                    continue;
                }

                CConstRef<CSeq_feat> ref(&feat_it->GetOriginalFeature());

                ECompare comp = sequence::Compare(gene_feat.GetLocation(),
                                                feat_it->GetLocation(),
                                                &scope);
                if (comp != eSame  &&  comp != eContains) {
                    continue;
                }

                if (feat_it->IsSetDbxref()) {
                    ITERATE (CSeq_feat::TDbxref, dbxref, feat_it->GetDbxref()) {
                        if (((*dbxref)->GetDb() == "GeneID"  ||
                            (*dbxref)->GetDb() == "LocusID")  &&
                            (*dbxref)->GetTag().GetId() == gene_id) {
                            mrna_feats.push_back(ref);
                            ++count;
                            break;
                        }
                    }
                }
            }

            if (count) {
                return;
            }
        }
    }}

    // gene doesn't have a gene_id or a gene ref
    CConstRef<CSeq_feat> feat =
        sequence::GetBestOverlappingFeat(gene_feat.GetLocation(),
                                         CSeqFeatData::eSubtype_mRNA,
                                         sequence::eOverlap_Contains,
                                         scope, opts);
    if (feat) {
        mrna_feats.push_back(feat);
    }
}


void GetCdssForGene(const CSeq_feat& gene_feat, CScope& scope,
                    list< CConstRef<CSeq_feat> >& cds_feats,
                    TBestFeatOpts opts)
{
    _ASSERT(gene_feat.GetData().GetSubtype() == CSeqFeatData::eSubtype_gene);
    list< CConstRef<CSeq_feat> > mrna_feats;
    GetMrnasForGene(gene_feat, scope, mrna_feats, opts);
    if (mrna_feats.size()) {
        ITERATE (list< CConstRef<CSeq_feat> >, iter, mrna_feats) {
            CConstRef<CSeq_feat> cds = GetBestCdsForMrna(**iter, scope, opts);
            if (cds) {
                cds_feats.push_back(cds);
            }
        }
    } else {
        CConstRef<CSeq_feat> feat =
            sequence::GetBestOverlappingFeat(gene_feat.GetLocation(),
                                             CSeqFeatData::eSubtype_cdregion,
                                             sequence::eOverlap_CheckIntervals,
                                             scope, opts);
        if (feat) {
            cds_feats.push_back(feat);
        }
    }
}


CConstRef<CSeq_feat>
GetBestOverlappingFeat(const CSeq_feat& feat,
                       CSeqFeatData::E_Choice feat_type,
                       sequence::EOverlapType overlap_type,
                       CScope& scope,
                       TBestFeatOpts opts)
{
    CConstRef<CSeq_feat> feat_ref;
    switch (feat_type) {
    case CSeqFeatData::e_Gene:
        return GetBestOverlappingFeat(feat,
                                      CSeqFeatData::eSubtype_gene,
                                      overlap_type, scope, opts);

    case CSeqFeatData::e_Rna:
        feat_ref = GetBestOverlappingFeat(feat,
                                          CSeqFeatData::eSubtype_mRNA,
                                          overlap_type, scope, opts);
        break;

    case CSeqFeatData::e_Cdregion:
        return GetBestOverlappingFeat(feat,
                                      CSeqFeatData::eSubtype_cdregion,
                                      overlap_type, scope, opts);

    default:
        break;
    }

    if ( !feat_ref ) {
        feat_ref = sequence::GetBestOverlappingFeat
            (feat.GetLocation(), feat_type, overlap_type, scope, opts);
    }

    return feat_ref;
}


CConstRef<CSeq_feat>
GetBestOverlappingFeat(const CSeq_feat& feat,
                       CSeqFeatData::ESubtype subtype,
                       sequence::EOverlapType overlap_type,
                       CScope& scope,
                       TBestFeatOpts opts)
{
    CConstRef<CSeq_feat> feat_ref;
    switch (feat.GetData().GetSubtype()) {
    case CSeqFeatData::eSubtype_mRNA:
        switch (subtype) {
        case CSeqFeatData::eSubtype_gene:
            return GetBestGeneForMrna(feat, scope, opts);

        case CSeqFeatData::eSubtype_cdregion:
            return GetBestCdsForMrna(feat, scope, opts);

        default:
            break;
        }
        break;

    case CSeqFeatData::eSubtype_cdregion:
        switch (subtype) {
        case CSeqFeatData::eSubtype_mRNA:
            return GetBestMrnaForCds(feat, scope, opts);

        case CSeqFeatData::eSubtype_gene:
            return GetBestGeneForCds(feat, scope, opts);

        default:
            break;
        }
        break;

	case CSeqFeatData::eSubtype_variation:
		return GetBestOverlapForSNP(feat, subtype, scope, true);

    default:
        break;
    }

    if ( !feat_ref ) {
        feat_ref = GetBestOverlappingFeat
            (feat.GetLocation(), subtype, overlap_type, scope, opts);
    }

    return feat_ref;
}


// Get the encoding CDS feature of a given protein sequence.
const CSeq_feat* GetCDSForProduct(const CBioseq& product, CScope* scope)
{
    if ( scope == 0 ) {
        return 0;
    }

    return GetCDSForProduct(scope->GetBioseqHandle(product));
}

const CSeq_feat* GetCDSForProduct(const CBioseq_Handle& bsh)
{
    if ( bsh ) {
        CFeat_CI fi(bsh,
                    SAnnotSelector(CSeqFeatData::e_Cdregion)
                    .SetByProduct());
        if ( fi ) {
            // return the first one (should be the one packaged on the
            // nuc-prot set).
            return &(fi->GetOriginalFeature());
        }
    }

    return 0;
}


// Get the mature peptide feature of a protein
const CSeq_feat* GetPROTForProduct(const CBioseq& product, CScope* scope)
{
    if ( scope == 0 ) {
        return 0;
    }

    return GetPROTForProduct(scope->GetBioseqHandle(product));
}

const CSeq_feat* GetPROTForProduct(const CBioseq_Handle& bsh)
{
    if ( bsh ) {
        CFeat_CI fi(bsh, SAnnotSelector(CSeqFeatData::e_Prot).SetByProduct());
        if ( fi ) {
            return &(fi->GetOriginalFeature());
        }
    }

    return 0;
}



// Get the encoding mRNA feature of a given mRNA (cDNA) bioseq.
const CSeq_feat* GetmRNAForProduct(const CBioseq& product, CScope* scope)
{
    if ( scope == 0 ) {
        return 0;
    }

    return GetmRNAForProduct(scope->GetBioseqHandle(product));
}

const CSeq_feat* GetmRNAForProduct(const CBioseq_Handle& bsh)
{
    if ( bsh ) {
        SAnnotSelector as(CSeqFeatData::eSubtype_mRNA);
        as.SetByProduct();
 
        CFeat_CI fi(bsh, as);
        if ( fi ) {
            return &(fi->GetOriginalFeature());
        }
    }

    return 0;
}


// Get the encoding sequence of a protein
const CBioseq* GetNucleotideParent(const CBioseq& product, CScope* scope)
{
    if ( scope == 0 ) {
        return 0;
    }
    CBioseq_Handle bsh = GetNucleotideParent(scope->GetBioseqHandle(product));
    return bsh ? bsh.GetCompleteBioseq() : reinterpret_cast<const CBioseq*>(0);
}

CBioseq_Handle GetNucleotideParent(const CBioseq_Handle& bsh)
{
    // If protein use CDS to get to the encoding Nucleotide.
    // if nucleotide (cDNA) use mRNA feature.
    const CSeq_feat* sfp = bsh.GetInst().IsAa() ?
        GetCDSForProduct(bsh) : GetmRNAForProduct(bsh);

    CBioseq_Handle ret;
    if ( sfp ) {
        ret = bsh.GetScope().GetBioseqHandle(sfp->GetLocation());
    }
    return ret;
}


CBioseq_Handle GetParentForPart(const CBioseq_Handle& part)
{
    CBioseq_Handle seg;

    if (part) {
        CSeq_entry_Handle segset =
            part.GetExactComplexityLevel(CBioseq_set::eClass_segset);
        if (segset) {
            for (CSeq_entry_CI it(segset); it; ++it) {
                if (it->IsSeq()) {
                    seg = it->GetSeq();
                    break;
                }
            }
        }
    }

    return seg;
}


CRef<CBioseq> CreateBioseqFromBioseq(const CBioseq_Handle& bsh,
                                     TSeqPos from, TSeqPos to,
                                     const CSeq_id_Handle& new_seq_id,
                                     TCreateBioseqFlags opts,
                                     int delta_seq_level)
{
    CRef<CBioseq> bioseq(new CBioseq);
    CSeq_inst& inst = bioseq->SetInst();

    if (opts & fBioseq_CreateDelta) {
        ///
        /// create a delta-seq to match the base sequence in this range
        ///
        inst.SetRepr(CSeq_inst::eRepr_delta);

        SSeqMapSelector sel(CSeqMap::fDefaultFlags);
        sel.SetRange(from, to)
            .SetResolveCount(delta_seq_level);
        CSeqMap_CI map_iter(bsh, sel);

        TSeqPos bioseq_len = 0;
        for ( ;  map_iter;  ++map_iter) {
            TSeqPos seq_start = map_iter.GetRefPosition();
            TSeqPos seq_end   = map_iter.GetRefEndPosition() - 1;

            if (map_iter.GetEndPosition() > to) {
                seq_end -= map_iter.GetEndPosition() - to;
            }

            TSeqPos len = seq_end - seq_start + 1;

            switch (map_iter.GetType()) {
            case CSeqMap::eSeqGap:
                /// add a gap only for our length
                inst.SetExt().SetDelta().AddLiteral(len);
                bioseq_len += len;
                break;

            case CSeqMap::eSeqData:
                {{
                    ///
                    /// copy the data chunk
                    /// this is potentially truncated
                    ///
                    CRef<CDelta_seq> seq(new CDelta_seq);
                    seq->SetLiteral().SetLength(len);
                    CSeq_data& data = seq->SetLiteral().SetSeq_data();
                    data.Assign(map_iter.GetRefData());
                    if (len != map_iter.GetLength()) {
                        switch (data.Which()) {
                        case CSeq_data::e_Iupacna:
                            data.SetIupacna().Set().resize(len);
                            break;
                        case CSeq_data::e_Ncbi2na:
                            data.SetNcbi2na().Set()
                                .resize(len / 4 + (len % 4 ? 1 : 0));
                            break;
                        case CSeq_data::e_Ncbi4na:
                            data.SetNcbi4na().Set()
                                .resize(len / 2 + (len % 2 ? 1 : 0));
                            break;
                        case CSeq_data::e_Ncbi8na:
                            data.SetNcbi8na().Set().resize(len);
                            break;
                        case CSeq_data::e_Ncbipna:
                            data.SetNcbipna().Set().resize(len * 5);
                            break;
                        case CSeq_data::e_Iupacaa:
                            data.SetIupacaa().Set().resize(len);
                            break;
                        case CSeq_data::e_Ncbi8aa:
                            data.SetNcbi8aa().Set().resize(len);
                            break;
                        case CSeq_data::e_Ncbieaa:
                            data.SetNcbieaa().Set().resize(len);
                            break;
                        case CSeq_data::e_Ncbipaa:
                            data.SetNcbipaa().Set().resize(len * 25);
                            break;
                        case CSeq_data::e_Ncbistdaa:
                            data.SetNcbistdaa().Set().resize(len);
                            break;
                        default:
                            break;
                        }
                    }
                    inst.SetExt().SetDelta().Set().push_back(seq);
                    bioseq_len += len;
                }}
                break;

            case CSeqMap::eSeqSubMap:
                break;

            case CSeqMap::eSeqRef:
                {{
                    ///
                    /// create a segment referring to our far delta seq
                    ///
                    inst.SetExt().SetDelta()
                        .AddSeqRange(*map_iter.GetRefSeqid().GetSeqId(),
                                    seq_start, seq_end,
                                    (map_iter.GetRefMinusStrand() ?
                                    eNa_strand_minus : eNa_strand_plus));
                    bioseq_len += len;
                }}
                break;
            case CSeqMap::eSeqEnd:
                break;

            case CSeqMap::eSeqChunk:
                break;
            }

            if (map_iter.GetEndPosition() > to) {
                break;
            }
        }
        inst.SetLength(bioseq_len);
    } else {
        ///
        /// just create a raw sequence
        ///
        CSeqVector vec(bsh, CBioseq_Handle::eCoding_Iupac);
        string seq;
        vec.GetSeqData(from, to, seq);
        TSeqPos len = seq.size();
        if (bsh.IsNa()) {
            inst.SetSeq_data().SetIupacna().Set().swap(seq);
            CSeqportUtil::Pack(&inst.SetSeq_data(), len);
        } else {
            inst.SetSeq_data().SetIupacaa().Set().swap(seq);
        }
        inst.SetRepr(CSeq_inst::eRepr_raw);
        inst.SetLength(len);
    }

    /// Make sure we copy our mol-type
    inst.SetMol(bsh.GetInst_Mol());

    /// set our seq-id
    CRef<CSeq_id> id(new CSeq_id);
    id->Assign(*new_seq_id.GetSeqId());
    bioseq->SetId().push_back(id);

    ///
    /// copy descriptors, if requested
    ///
    if (opts & fBioseq_CopyDescriptors) {
        CSeqdesc_CI desc_it(bsh);
        for ( ;  desc_it;  ++desc_it) {
            CRef<CSeqdesc> desc(new CSeqdesc);
            desc->Assign(*desc_it);
            bioseq->SetDescr().Set().push_back(desc);
        }
    }

    ///
    /// copy annotations, if requested
    ///
    if (opts & fBioseq_CopyAnnot) {
        CSeq_loc from_loc;
        from_loc.SetInt().SetFrom(from);
        from_loc.SetInt().SetTo  (to);
        from_loc.SetId(*bsh.GetSeqId());

        CSeq_loc to_loc;
        to_loc.SetInt().SetFrom(0);
        to_loc.SetInt().SetTo(to - from);
        to_loc.SetId(*id);

        CSeq_loc_Mapper mapper(from_loc, to_loc, &bsh.GetScope());

        CRef<CSeq_annot> annot(new CSeq_annot);

        SAnnotSelector sel;
        sel.SetResolveAll()
            .SetResolveDepth(delta_seq_level);
        CFeat_CI feat_iter(bsh, TSeqRange(from, to), sel);
        for ( ;  feat_iter;  ++feat_iter) {
            CRef<CSeq_loc> loc = mapper.Map(feat_iter->GetLocation());
            if ( !loc ) {
                continue;
            }
            CRef<CSeq_feat> feat(new CSeq_feat);
            feat->Assign(feat_iter->GetOriginalFeature());
            feat->SetLocation(*loc);
            annot->SetData().SetFtable().push_back(feat);
        }

        if (annot->IsSetData()) {
            bioseq->SetAnnot().push_back(annot);
        }
    }

    return bioseq;
}



END_SCOPE(sequence)


void CFastaOstream::Write(const CSeq_entry_Handle& handle,
                          const CSeq_loc* location)
{
    for (CBioseq_CI it(handle);  it;  ++it) {
        if ( !SkipBioseq(*it) ) {
            if (location) {
                CSeq_loc loc2;
                loc2.SetWhole().Assign(*it->GetSeqId());
                int d = sequence::TestForOverlap
                    (*location, loc2, sequence::eOverlap_Interval,
                     kInvalidSeqPos, &handle.GetScope());
                if (d < 0) {
                    continue;
                }
            }
            Write(*it, location);
        }
    }
}


void CFastaOstream::Write(const CBioseq_Handle& handle,
                          const CSeq_loc* location)
{
    WriteTitle(handle);
    WriteSequence(handle, location);
}


void CFastaOstream::WriteTitle(const CBioseq_Handle& handle)
{
    m_Out << '>' << CSeq_id::GetStringDescr(*handle.GetBioseqCore(),
                                            CSeq_id::eFormat_FastA)
          << ' ' << sequence::GetTitle(handle) << NcbiEndl;
}


void CFastaOstream::WriteSequence(const CBioseq_Handle& handle,
                                  const CSeq_loc* location)
{
    if ( !(m_Flags & eAssembleParts)  &&  !handle.IsSetInst_Seq_data() ) {
        return; // XXX - too extreme?
    }

    CScope& scope = handle.GetScope();

    CSeqVector v;
    if (location) {
        CRef<CSeq_loc> merged
            = sequence::Seq_loc_Merge(*location, CSeq_loc::fMerge_All, &scope);
        v = CSeqVector(*merged, scope, CBioseq_Handle::eCoding_Iupac);
    } else {
        v = handle.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
    }

    unsigned int line_pos = 0;
    ITERATE (CSeqVector, it, v) {
        if ( !(m_Flags & eInstantiateGaps)  &&  it.SkipGap() ) {
            m_Out << "-\n";
            line_pos = 0;
        } else {
            m_Out << *it;
            if (++line_pos >= m_Width) {
                m_Out << '\n';
                line_pos = 0;
            }
        }
    }
    if (line_pos > 0) {
        m_Out << '\n';
    }
    m_Out << NcbiFlush;
}


void CFastaOstream::Write(const CSeq_entry& entry, const CSeq_loc* location)
{
    CScope scope(*CObjectManager::GetInstance());

    Write(scope.AddTopLevelSeqEntry(entry), location);
}


void CFastaOstream::Write(const CBioseq& seq, const CSeq_loc* location)
{
    CScope scope(*CObjectManager::GetInstance());

    Write(scope.AddBioseq(seq), location);
}



/////////////////////////////////////////////////////////////////////////////
//
// sequence translation
//


template <class Container>
void x_Translate(const Container& seq,
                 string& prot,
                 const CGenetic_code* code,
                 bool include_stop,
                 bool remove_trailing_X)
{
    // reserve our space
    const size_t mod = seq.size() % 3;
    prot.erase();
    prot.reserve(seq.size() / 3 + (mod ? 1 : 0));

    // get appropriate translation table
    const CTrans_table & tbl =
        (code ? CGen_code_table::GetTransTable(*code) :
                CGen_code_table::GetTransTable(1));

    // main loop through bases
    typename Container::const_iterator start = seq.begin();

    size_t i;
    size_t k;
    size_t state = 0;
    size_t length = seq.size() / 3;
    for (i = 0;  i < length;  ++i) {

        // loop through one codon at a time
        for (k = 0;  k < 3;  ++k, ++start) {
            state = tbl.NextCodonState(state, *start);
        }

        // save translated amino acid
        prot.append(1, tbl.GetCodonResidue(state));
    }

    if (mod) {
        LOG_POST(Warning <<
                 "translation of sequence whose length "
                 "is not an even number of codons");
        for (k = 0;  k < mod;  ++k, ++start) {
            state = tbl.NextCodonState(state, *start);
        }

        for ( ;  k < 3;  ++k) {
            state = tbl.NextCodonState(state, 'N');
        }

        // save translated amino acid
        prot.append(1, tbl.GetCodonResidue(state));
    }

    if ( !include_stop ) {
        string::size_type pos = prot.find_first_of("*");
        if (pos != string::npos) {
            prot.erase(pos);
        }
    }

    if (remove_trailing_X) {
        string::size_type pos = prot.find_last_not_of("X");
        if (pos != string::npos) {
            ++pos;
            prot.erase(pos);
        }

    }
}


void CSeqTranslator::Translate(const string& seq, string& prot,
                               const CGenetic_code* code,
                               bool include_stop,
                               bool remove_trailing_X)
{
    x_Translate(seq, prot, code, include_stop, remove_trailing_X);
}


void CSeqTranslator::Translate(const CSeqVector& seq, string& prot,
                               const CGenetic_code* code,
                               bool include_stop,
                               bool remove_trailing_X)
{
    x_Translate(seq, prot, code, include_stop, remove_trailing_X);
}


void CSeqTranslator::Translate(const CSeq_loc& loc,
                               const CBioseq_Handle& handle,
                               string& prot,
                               const CGenetic_code* code,
                               bool include_stop,
                               bool remove_trailing_X)
{
    CSeqVector seq(loc, handle.GetScope(), CBioseq_Handle::eCoding_Iupac);
    x_Translate(seq, prot, code, include_stop, remove_trailing_X);
}




void CCdregion_translate::ReadSequenceByLocation (string& seq,
                                                  const CBioseq_Handle& bsh,
                                                  const CSeq_loc& loc)

{
    // get vector of sequence under location
    CSeqVector seqv(loc, bsh.GetScope(), CBioseq_Handle::eCoding_Iupac);
    seqv.GetSeqData(0, seqv.size(), seq);
}

void CCdregion_translate::TranslateCdregion (string& prot,
                                             const CBioseq_Handle& bsh,
                                             const CSeq_loc& loc,
                                             const CCdregion& cdr,
                                             bool include_stop,
                                             bool remove_trailing_X,
                                             bool* alt_start)
{
    // clear contents of result string
    prot.erase();
    if ( alt_start != 0 ) {
        *alt_start = false;
    }

    // copy bases from coding region location
    string bases = "";
    ReadSequenceByLocation (bases, bsh, loc);

    // calculate offset from frame parameter
    int offset = 0;
    if (cdr.IsSetFrame ()) {
        switch (cdr.GetFrame ()) {
            case CCdregion::eFrame_two :
                offset = 1;
                break;
            case CCdregion::eFrame_three :
                offset = 2;
                break;
            default :
                break;
        }
    }

    int dnalen = bases.size () - offset;
    if (dnalen < 1) return;

    // pad bases string if last codon is incomplete
    bool incomplete_last_codon = false;
    int mod = dnalen % 3;
    if ( mod != 0 ) {
        incomplete_last_codon = true;
        bases += (mod == 1) ? "NN" : "N";
        dnalen += 3 - mod;
    }
    _ASSERT((dnalen >= 3)  &&  ((dnalen % 3) == 0));

    // resize output protein translation string
    prot.resize(dnalen / 3);

    // get appropriate translation table
    const CTrans_table& tbl = cdr.IsSetCode() ?
        CGen_code_table::GetTransTable(cdr.GetCode()) :
        CGen_code_table::GetTransTable(1);

    // main loop through bases
    string::const_iterator it = bases.begin();
    string::const_iterator end = bases.end();
    for ( int i = 0; i < offset; ++i ) {
        ++it;
    }
    unsigned int state = 0, j = 0;
    while ( it != end ) {
        // get one codon at a time
        state = tbl.NextCodonState(state, *it++);
        state = tbl.NextCodonState(state, *it++);
        state = tbl.NextCodonState(state, *it++);

        // save translated amino acid
        prot[j++] = tbl.GetCodonResidue(state);
    }

    // check for alternative start codon
    if ( alt_start != 0 ) {
        state = 0;
        state = tbl.NextCodonState (state, bases[0]);
        state = tbl.NextCodonState (state, bases[1]);
        state = tbl.NextCodonState (state, bases[2]);
        if ( tbl.IsAltStart(state) ) {
            *alt_start = true;
        }
    }

    // if complete at 5' end, require valid start codon
    if (offset == 0  &&  (! loc.IsPartialStart (eExtreme_Biological))) {
        state = tbl.SetCodonState (bases [offset], bases [offset + 1], bases [offset + 2]);
        prot [0] = tbl.GetStartResidue (state);
    }

    // code break substitution
    if (cdr.IsSetCode_break ()) {
        SIZE_TYPE protlen = prot.size ();
        ITERATE (CCdregion::TCode_break, code_break, cdr.GetCode_break ()) {
            const CRef <CCode_break> brk = *code_break;
            const CSeq_loc& cbk_loc = brk->GetLoc ();
            TSeqPos seq_pos = sequence::LocationOffset (loc, cbk_loc, sequence::eOffset_FromStart, &bsh.GetScope ());
            seq_pos -= offset;
            SIZE_TYPE i = seq_pos / 3;
            if (i >= 0 && i < protlen) {
              const CCode_break::C_Aa& c_aa = brk->GetAa ();
              if (c_aa.IsNcbieaa ()) {
                prot [i] = c_aa.GetNcbieaa ();
              }
            }
        }
    }

    // optionally truncate at first terminator
    if (! include_stop) {
        SIZE_TYPE protlen = prot.size ();
        for (SIZE_TYPE i = 0; i < protlen; i++) {
            if (prot [i] == '*') {
                prot.resize (i);
                return;
            }
        }
    }

    // if padding was needed, trim ambiguous last residue
    if (incomplete_last_codon) {
        int protlen = prot.size ();
        if (protlen > 0 && prot [protlen - 1] == 'X') {
            protlen--;
            prot.resize (protlen);
        }
    }

    // optionally remove trailing X on 3' partial coding region
    if (remove_trailing_X) {
        int protlen = prot.size ();
        while (protlen > 0 && prot [protlen - 1] == 'X') {
            protlen--;
        }
        prot.resize (protlen);
    }
}


void CCdregion_translate::TranslateCdregion
(string& prot,
 const CSeq_feat& cds,
 CScope& scope,
 bool include_stop,
 bool remove_trailing_X,
 bool* alt_start)
{
    _ASSERT(cds.GetData().IsCdregion());

    prot.erase();

    CBioseq_Handle bsh = scope.GetBioseqHandle(cds.GetLocation());
    if ( !bsh ) {
        return;
    }

    CCdregion_translate::TranslateCdregion(
        prot,
        bsh,
        cds.GetLocation(),
        cds.GetData().GetCdregion(),
        include_stop,
        remove_trailing_X,
        alt_start);
}


SRelLoc::SRelLoc(const CSeq_loc& parent, const CSeq_loc& child, CScope* scope,
                 SRelLoc::TFlags flags)
    : m_ParentLoc(&parent)
{
    typedef CSeq_loc::TRange TRange0;
    for (CSeq_loc_CI cit(child);  cit;  ++cit) {
        const CSeq_id& cseqid  = cit.GetSeq_id();
        TRange0        crange  = cit.GetRange();
        if (crange.IsWholeTo()  &&  scope) {
            // determine actual end
            crange.SetToOpen(sequence::GetLength(cit.GetSeq_id(), scope));
        }
        ENa_strand     cstrand = cit.GetStrand();
        TSeqPos        pos     = 0;
        for (CSeq_loc_CI pit(parent);  pit;  ++pit) {
            ENa_strand pstrand = pit.GetStrand();
            TRange0    prange  = pit.GetRange();
            if (prange.IsWholeTo()  &&  scope) {
                // determine actual end
                prange.SetToOpen(sequence::GetLength(pit.GetSeq_id(), scope));
            }
            if ( !sequence::IsSameBioseq(cseqid, pit.GetSeq_id(), scope) ) {
                pos += prange.GetLength();
                continue;
            }
            CRef<TRange>         intersection(new TRange);
            TSeqPos              abs_from, abs_to;
            CConstRef<CInt_fuzz> fuzz_from, fuzz_to;
            if (crange.GetFrom() >= prange.GetFrom()) {
                abs_from  = crange.GetFrom();
                fuzz_from = cit.GetFuzzFrom();
                if (abs_from == prange.GetFrom()) {
                    // subtract out parent fuzz, if any
                    const CInt_fuzz* pfuzz = pit.GetFuzzFrom();
                    if (pfuzz) {
                        if (fuzz_from) {
                            CRef<CInt_fuzz> f(new CInt_fuzz);
                            f->Assign(*fuzz_from);
                            f->Subtract(*pfuzz, abs_from, abs_from);
                            if (f->IsP_m()  &&  !f->GetP_m() ) {
                                fuzz_from.Reset(); // cancelled
                            } else {
                                fuzz_from = f;
                            }
                        } else {
                            fuzz_from = pfuzz->Negative(abs_from);
                        }
                    }
                }
            } else {
                abs_from  = prange.GetFrom();
                // fuzz_from = pit.GetFuzzFrom();
                CRef<CInt_fuzz> f(new CInt_fuzz);
                f->SetLim(CInt_fuzz::eLim_lt);
                fuzz_from = f;
            }
            if (crange.GetTo() <= prange.GetTo()) {
                abs_to  = crange.GetTo();
                fuzz_to = cit.GetFuzzTo();
                if (abs_to == prange.GetTo()) {
                    // subtract out parent fuzz, if any
                    const CInt_fuzz* pfuzz = pit.GetFuzzTo();
                    if (pfuzz) {
                        if (fuzz_to) {
                            CRef<CInt_fuzz> f(new CInt_fuzz);
                            f->Assign(*fuzz_to);
                            f->Subtract(*pfuzz, abs_to, abs_to);
                            if (f->IsP_m()  &&  !f->GetP_m() ) {
                                fuzz_to.Reset(); // cancelled
                            } else {
                                fuzz_to = f;
                            }
                        } else {
                            fuzz_to = pfuzz->Negative(abs_to);
                        }
                    }
                }
            } else {
                abs_to  = prange.GetTo();
                // fuzz_to = pit.GetFuzzTo();
                CRef<CInt_fuzz> f(new CInt_fuzz);
                f->SetLim(CInt_fuzz::eLim_gt);
                fuzz_to = f;
            }
            if (abs_from <= abs_to) {
                if (IsReverse(pstrand)) {
                    TSeqPos sigma = pos + prange.GetTo();
                    intersection->SetFrom(sigma - abs_to);
                    intersection->SetTo  (sigma - abs_from);
                    if (fuzz_from) {
                        intersection->SetFuzz_to().AssignTranslated
                            (*fuzz_from, intersection->GetTo(), abs_from);
                        intersection->SetFuzz_to().Negate
                            (intersection->GetTo());
                    }
                    if (fuzz_to) {
                        intersection->SetFuzz_from().AssignTranslated
                            (*fuzz_to, intersection->GetFrom(), abs_to);
                        intersection->SetFuzz_from().Negate
                            (intersection->GetFrom());
                    }
                    if (cstrand == eNa_strand_unknown) {
                        intersection->SetStrand(pstrand);
                    } else {
                        intersection->SetStrand(Reverse(cstrand));
                    }
                } else {
                    TSignedSeqPos delta = pos - prange.GetFrom();
                    intersection->SetFrom(abs_from + delta);
                    intersection->SetTo  (abs_to   + delta);
                    if (fuzz_from) {
                        intersection->SetFuzz_from().AssignTranslated
                            (*fuzz_from, intersection->GetFrom(), abs_from);
                    }
                    if (fuzz_to) {
                        intersection->SetFuzz_to().AssignTranslated
                            (*fuzz_to, intersection->GetTo(), abs_to);
                    }
                    if (cstrand == eNa_strand_unknown) {
                        intersection->SetStrand(pstrand);
                    } else {
                        intersection->SetStrand(cstrand);
                    }
                }
                // add to m_Ranges, combining with the previous
                // interval if possible
                if ( !(flags & fNoMerge)  &&  !m_Ranges.empty()
                    &&  SameOrientation(intersection->GetStrand(),
                                        m_Ranges.back()->GetStrand()) ) {
                    if (m_Ranges.back()->GetTo() == intersection->GetFrom() - 1
                        &&  !IsReverse(intersection->GetStrand()) ) {
                        m_Ranges.back()->SetTo(intersection->GetTo());
                        if (intersection->IsSetFuzz_to()) {
                            m_Ranges.back()->SetFuzz_to
                                (intersection->SetFuzz_to());
                        } else {
                            m_Ranges.back()->ResetFuzz_to();
                        }
                    } else if (m_Ranges.back()->GetFrom()
                               == intersection->GetTo() + 1
                               &&  IsReverse(intersection->GetStrand())) {
                        m_Ranges.back()->SetFrom(intersection->GetFrom());
                        if (intersection->IsSetFuzz_from()) {
                            m_Ranges.back()->SetFuzz_from
                                (intersection->SetFuzz_from());
                        } else {
                            m_Ranges.back()->ResetFuzz_from();
                        }
                    } else {
                        m_Ranges.push_back(intersection);
                    }
                } else {
                    m_Ranges.push_back(intersection);
                }
            }
            pos += prange.GetLength();
        }
    }
}


// Bother trying to merge?
CRef<CSeq_loc> SRelLoc::Resolve(const CSeq_loc& new_parent, CScope* scope,
                                SRelLoc::TFlags /* flags */)
    const
{
    typedef CSeq_loc::TRange TRange0;
    CRef<CSeq_loc> result(new CSeq_loc);
    CSeq_loc_mix&  mix = result->SetMix();
    ITERATE (TRanges, it, m_Ranges) {
        _ASSERT((*it)->GetFrom() <= (*it)->GetTo());
        TSeqPos pos = 0, start = (*it)->GetFrom();
        bool    keep_going = true;
        for (CSeq_loc_CI pit(new_parent);  pit;  ++pit) {
            TRange0 prange = pit.GetRange();
            if (prange.IsWholeTo()  &&  scope) {
                // determine actual end
                prange.SetToOpen(sequence::GetLength(pit.GetSeq_id(), scope));
            }
            TSeqPos length = prange.GetLength();
            if (start >= pos  &&  start < pos + length) {
                TSeqPos              from, to;
                CConstRef<CInt_fuzz> fuzz_from, fuzz_to;
                ENa_strand           strand;
                if (IsReverse(pit.GetStrand())) {
                    TSeqPos sigma = pos + prange.GetTo();
                    from = sigma - (*it)->GetTo();
                    to   = sigma - start;
                    if (from < prange.GetFrom()  ||  from > sigma) {
                        from = prange.GetFrom();
                        keep_going = true;
                    } else {
                        keep_going = false;
                    }
                    if ( !(*it)->IsSetStrand()
                        ||  (*it)->GetStrand() == eNa_strand_unknown) {
                        strand = pit.GetStrand();
                    } else {
                        strand = Reverse((*it)->GetStrand());
                    }
                    if (from == prange.GetFrom()) {
                        fuzz_from = pit.GetFuzzFrom();
                    }
                    if ( !keep_going  &&  (*it)->IsSetFuzz_to() ) {
                        CRef<CInt_fuzz> f(new CInt_fuzz);
                        if (fuzz_from) {
                            f->Assign(*fuzz_from);
                        } else {
                            f->SetP_m(0);
                        }
                        f->Subtract((*it)->GetFuzz_to(), from, (*it)->GetTo(),
                                    CInt_fuzz::eAmplify);
                        if (f->IsP_m()  &&  !f->GetP_m() ) {
                            fuzz_from.Reset(); // cancelled
                        } else {
                            fuzz_from = f;
                        }
                    }
                    if (to == prange.GetTo()) {
                        fuzz_to = pit.GetFuzzTo();
                    }
                    if (start == (*it)->GetFrom()
                        &&  (*it)->IsSetFuzz_from()) {
                        CRef<CInt_fuzz> f(new CInt_fuzz);
                        if (fuzz_to) {
                            f->Assign(*fuzz_to);
                        } else {
                            f->SetP_m(0);
                        }
                        f->Subtract((*it)->GetFuzz_from(), to,
                                    (*it)->GetFrom(), CInt_fuzz::eAmplify);
                        if (f->IsP_m()  &&  !f->GetP_m() ) {
                            fuzz_to.Reset(); // cancelled
                        } else {
                            fuzz_to = f;
                        }
                    }
                } else {
                    TSignedSeqPos delta = prange.GetFrom() - pos;
                    from = start          + delta;
                    to   = (*it)->GetTo() + delta;
                    if (to > prange.GetTo()) {
                        to = prange.GetTo();
                        keep_going = true;
                    } else {
                        keep_going = false;
                    }
                    if ( !(*it)->IsSetStrand()
                        ||  (*it)->GetStrand() == eNa_strand_unknown) {
                        strand = pit.GetStrand();
                    } else {
                        strand = (*it)->GetStrand();
                    }
                    if (from == prange.GetFrom()) {
                        fuzz_from = pit.GetFuzzFrom();
                    }
                    if (start == (*it)->GetFrom()
                        &&  (*it)->IsSetFuzz_from()) {
                        CRef<CInt_fuzz> f(new CInt_fuzz);
                        if (fuzz_from) {
                            f->Assign(*fuzz_from);
                            f->Add((*it)->GetFuzz_from(), from,
                                   (*it)->GetFrom());
                        } else {
                            f->AssignTranslated((*it)->GetFuzz_from(), from,
                                                (*it)->GetFrom());
                        }
                        if (f->IsP_m()  &&  !f->GetP_m() ) {
                            fuzz_from.Reset(); // cancelled
                        } else {
                            fuzz_from = f;
                        }
                    }
                    if (to == prange.GetTo()) {
                        fuzz_to = pit.GetFuzzTo();
                    }
                    if ( !keep_going  &&  (*it)->IsSetFuzz_to() ) {
                        CRef<CInt_fuzz> f(new CInt_fuzz);
                        if (fuzz_to) {
                            f->Assign(*fuzz_to);
                            f->Add((*it)->GetFuzz_to(), to, (*it)->GetTo());
                        } else {
                            f->AssignTranslated((*it)->GetFuzz_to(), to,
                                                (*it)->GetTo());
                        }
                        if (f->IsP_m()  &&  !f->GetP_m() ) {
                            fuzz_to.Reset(); // cancelled
                        } else {
                            fuzz_to = f;
                        }
                    }
                }
                if (from == to
                    &&  (fuzz_from == fuzz_to
                         ||  (fuzz_from.GetPointer()  &&  fuzz_to.GetPointer()
                              &&  fuzz_from->Equals(*fuzz_to)))) {
                    // just a point
                    CRef<CSeq_loc> loc(new CSeq_loc);
                    CSeq_point& point = loc->SetPnt();
                    point.SetPoint(from);
                    if (strand != eNa_strand_unknown) {
                        point.SetStrand(strand);
                    }
                    if (fuzz_from) {
                        point.SetFuzz().Assign(*fuzz_from);
                    }
                    point.SetId().Assign(pit.GetSeq_id());
                    mix.Set().push_back(loc);
                } else {
                    CRef<CSeq_loc> loc(new CSeq_loc);
                    CSeq_interval& ival = loc->SetInt();
                    ival.SetFrom(from);
                    ival.SetTo(to);
                    if (strand != eNa_strand_unknown) {
                        ival.SetStrand(strand);
                    }
                    if (fuzz_from) {
                        ival.SetFuzz_from().Assign(*fuzz_from);
                    }
                    if (fuzz_to) {
                        ival.SetFuzz_to().Assign(*fuzz_to);
                    }
                    ival.SetId().Assign(pit.GetSeq_id());
                    mix.Set().push_back(loc);
                }
                if (keep_going) {
                    start = pos + length;
                } else {
                    break;
                }
            }
            pos += length;
        }
        if (keep_going) {
            TSeqPos total_length;
            string  label;
            new_parent.GetLabel(&label);
            try {
                total_length = sequence::GetLength(new_parent, scope);
                ERR_POST(Warning << "SRelLoc::Resolve: Relative position "
                         << start << " exceeds length (" << total_length
                         << ") of parent location " << label);
            } catch (CObjmgrUtilException) {
                ERR_POST(Warning << "SRelLoc::Resolve: Relative position "
                         << start
                         << " exceeds length (?\?\?) of parent location "
                         << label);
            }            
        }
    }
    // clean up output
    switch (mix.Get().size()) {
    case 0:
        result->SetNull();
        break;
    case 1:
    {{
        CRef<CSeq_loc> first = mix.Set().front();
        result = first;
        break;
    }}
    default:
        break;
    }
    return result;
}


//============================================================================//
//                                 SeqSearch                                  //
//============================================================================//

// Public:
// =======

// Constructors and Destructors:
CSeqSearch::CSeqSearch(IClient *client, TSearchFlags flags) :
    m_Client(client), m_Flags(flags), m_LongestPattern(0), m_Fsa(true)
{
}


CSeqSearch::~CSeqSearch(void)
{
}


typedef pair<Char, Char> TCharPair;
static const TCharPair sc_comp_tbl[32] = {
    // uppercase
    TCharPair('A', 'T'),
    TCharPair('B', 'V'),
    TCharPair('C', 'G'),
    TCharPair('D', 'H'),
    TCharPair('G', 'C'),
    TCharPair('H', 'D'),
    TCharPair('K', 'M'),
    TCharPair('M', 'K'),
    TCharPair('N', 'N'),
    TCharPair('R', 'Y'),
    TCharPair('S', 'S'),
    TCharPair('T', 'A'),
    TCharPair('U', 'A'),
    TCharPair('V', 'B'),
    TCharPair('W', 'W'),
    TCharPair('Y', 'R'),
    // lowercase
    TCharPair('a', 'T'),
    TCharPair('b', 'V'),
    TCharPair('c', 'G'),
    TCharPair('d', 'H'),
    TCharPair('g', 'C'),
    TCharPair('h', 'D'),
    TCharPair('k', 'M'),
    TCharPair('m', 'K'),
    TCharPair('n', 'N'),
    TCharPair('r', 'Y'),
    TCharPair('s', 'S'),
    TCharPair('t', 'A'),
    TCharPair('u', 'A'),
    TCharPair('v', 'B'),
    TCharPair('w', 'W'),
    TCharPair('y', 'R'),
};
typedef CStaticArrayMap<Char, Char> TComplement;
static const TComplement sc_Complement(sc_comp_tbl, sizeof(sc_comp_tbl));
static const TComplement::const_iterator comp_end = sc_Complement.end();


inline
static char s_GetComplement(char c)
{
    TComplement::const_iterator comp_it = sc_Complement.find(c);
    return (comp_it != comp_end) ? comp_it->second : '\0';
}


static string s_GetReverseComplement(const string& sequence)
{
    string revcomp;
    revcomp.reserve(sequence.length());
    string::const_reverse_iterator rend = sequence.rend();

    for (string::const_reverse_iterator rit = sequence.rbegin(); rit != rend; ++rit) {
        revcomp += s_GetComplement(*rit);
    }

    return revcomp;
}


void CSeqSearch::AddNucleotidePattern
(const string& name,
 const string& sequence,
 Int2          cut_site,
 TSearchFlags  flags)
{
    if (NStr::IsBlank(name)  ||  NStr::IsBlank(sequence)) {
        NCBI_THROW(CUtilException, eNoInput, "Empty input value");
    }

    // cleanup pattern
    string pattern = sequence;
    NStr::TruncateSpaces(pattern);
    NStr::ToUpper(pattern);

    string revcomp = s_GetReverseComplement(pattern);
    bool symmetric = (pattern == revcomp);
    ENa_strand strand = symmetric ? eNa_strand_both : eNa_strand_plus;

    // record expansion of entered pattern
    x_AddNucleotidePattern(name, pattern, cut_site, strand, flags);

    // record expansion of reverse complement of asymmetric pattern
    if (!symmetric  &&  (!x_IsJustTopStrand(flags))) {
        size_t revcomp_cut_site = pattern.length() - cut_site;
        x_AddNucleotidePattern(name, revcomp, revcomp_cut_site,
            eNa_strand_minus, flags);
    }
}


// Program passes each character in turn to finite state machine.
int CSeqSearch::Search
(int  current_state,
 char ch,
 int  position,
 int  length)
{
    if (m_Client == NULL) {
        return 0;
    }

    // on first character, populate state transition table
    if (!m_Fsa.IsPrimed()) {
        m_Fsa.Prime();
    }
    
    int next_state = m_Fsa.GetNextState(current_state, ch);
    
    // report matches (if any)
    if (m_Fsa.IsMatchFound(next_state)) {
        ITERATE(vector<TPatternInfo>, it, m_Fsa.GetMatches(next_state)) {
            int start = position - it->GetSequence().length() + 1;

            // prevent multiple reports of patterns for circular sequences.
            if (start < length) {
                bool keep_going = m_Client->OnPatternFound(*it, start);
                if (!keep_going) {
                    break;
                }
            }
        }
    }

    return next_state;
}


// Search entire bioseq.
void CSeqSearch::Search(const CBioseq_Handle& bsh)
{
    if (!bsh  ||  m_Client == NULL) {
        return;
    }

    CSeqVector seq_vec = bsh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
    size_t seq_len = seq_vec.size();
    size_t search_len = seq_len;

    // handle circular bioseqs
    CSeq_inst::ETopology topology = bsh.GetInst_Topology();
    if (topology == CSeq_inst::eTopology_circular) {
        search_len += m_LongestPattern - 1;
    }
    
    int state = m_Fsa.GetInitialState();

    for (size_t i = 0; i < search_len; ++i) {
        state = Search(state, seq_vec[i % seq_len], i, seq_len);
    }
}


// Private:
// ========

/// translation finite state machine base codes - ncbi4na
enum EBaseCode {
    eBase_A = 1,  ///< A
    eBase_C,      ///< C
    eBase_M,      ///< AC
    eBase_G,      ///< G
    eBase_R,      ///< AG
    eBase_S,      ///< CG
    eBase_V,      ///< ACG
    eBase_T,      ///< T
    eBase_W,      ///< AT
    eBase_Y,      ///< CT
    eBase_H,      ///< ACT
    eBase_K,      ///< GT
    eBase_D,      ///< AGT
    eBase_B,      ///< CGT
    eBase_N       ///< ACGT
};

/// conversion table from Ncbi4na / Iupacna to EBaseCode
static const EBaseCode sc_CharToEnum[256] = {
    // Ncbi4na
    eBase_N, eBase_A, eBase_C, eBase_M,
    eBase_G, eBase_R, eBase_S, eBase_V,
    eBase_T, eBase_W, eBase_Y, eBase_H,
    eBase_K, eBase_D, eBase_B, eBase_N,

    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    // Iupacna (uppercase)
    eBase_N, eBase_A, eBase_B, eBase_C,
    eBase_D, eBase_N, eBase_N, eBase_G,
    eBase_H, eBase_N, eBase_N, eBase_K,
    eBase_N, eBase_M, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_R, eBase_S,
    eBase_T, eBase_T, eBase_V, eBase_W,
    eBase_N, eBase_Y, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    // Iupacna (lowercase)
    eBase_N, eBase_A, eBase_B, eBase_C,
    eBase_D, eBase_N, eBase_N, eBase_G,
    eBase_H, eBase_N, eBase_N, eBase_K,
    eBase_N, eBase_M, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_R, eBase_S,
    eBase_T, eBase_T, eBase_V, eBase_W,
    eBase_N, eBase_Y, eBase_N, eBase_N,

    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N,
    eBase_N, eBase_N, eBase_N, eBase_N
};

static const char sc_EnumToChar[16] = {
    '\0', 'A', 'C', 'M', 'G', 'R', 'S', 'V', 'T', 'W', 'Y', 'H', 'K', 'D', 'B', 'N'
};


void CSeqSearch::x_AddNucleotidePattern
(const string& name,
 string& pattern,
 Int2 cut_site,
 ENa_strand strand,
 TSearchFlags flags)
{
    if (pattern.length() > m_LongestPattern) {
        m_LongestPattern = pattern.length();
    }
    
    TPatternInfo pat_info(name, kEmptyStr, cut_site);
    pat_info.m_Strand = strand;

    if (!x_IsExpandPattern(flags)) {
        pat_info.m_Sequence = pattern;
        x_AddPattern(pat_info, pattern, flags);
    } else {
        string buffer;
        buffer.reserve(pattern.length());

        x_ExpandPattern(pattern, buffer, 0, pat_info, flags);
    }
}


void CSeqSearch::x_ExpandPattern
(string& sequence,
 string& buf,
 size_t pos,
 TPatternInfo& pat_info,
 TSearchFlags flags)
{
    static const EBaseCode expansion[] = { eBase_A, eBase_C, eBase_G, eBase_T };

    if (pos < sequence.length()) {
        Uint4 code = static_cast<Uint4>(sc_CharToEnum[static_cast<Uint1>(sequence[pos])]);

        for (int i = 0; i < 4; ++i) {
            if ((code & expansion[i]) != 0) {
                buf += sc_EnumToChar[expansion[i]];
                x_ExpandPattern(sequence, buf, pos + 1, pat_info, flags);
                buf.erase(pos);
            }
        }
    } else {
        // when position reaches pattern length, store one expanded string.
        x_AddPattern(pat_info, buf, flags);
    }
}


void CSeqSearch::x_AddPattern(TPatternInfo& pat_info, string& sequence, TSearchFlags flags)
{
    x_StorePattern(pat_info, sequence);

    if (x_IsAllowMismatch(flags)) {
        // put 'N' at every position if a single mismatch is allowed.
        char ch = 'N';
        NON_CONST_ITERATE (string, it, sequence) {
            swap(*it, ch);
        
            x_StorePattern(pat_info, sequence);

            // restore proper character, go on to put N in next position.
            swap(*it, ch);
        }
    }
}


void CSeqSearch::x_StorePattern(TPatternInfo& pat_info, string& sequence)
{
    pat_info.m_Sequence = sequence;
    m_Fsa.AddWord(sequence, pat_info);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.143  2006/07/20 22:19:01  grichenk
* Added eOverlap_CheckIntRev, use it in GetBestXXXForCds().
*
* Revision 1.142  2006/07/19 18:44:08  dicuccio
* GetBestCdsForMrna(): use CheckIntervals
*
* Revision 1.141  2006/06/13 19:08:58  dicuccio
* Cleaned up logic and bugs in GetBestXxxForXxx(), GetXxxForXxx() functions
*
* Revision 1.140  2006/06/05 13:43:17  vasilche
* Fixed warning.
*
* Revision 1.139  2006/05/16 15:32:26  dicuccio
* Implement translation options for non-CDS-based translation
*
* Revision 1.138  2006/04/24 14:54:08  dicuccio
* Honor fBestFeat_FavorLonger flag in GetBestMrnaForCds, GetBestGeneForMrna,
* GetBestGeneForCds
*
* Revision 1.137  2006/04/04 13:21:42  dicuccio
* Added option to favor the longest item among the best matching features.
* Added options to all versions of GetBestOverlappingFeature().
* Tweaked order of comparisons in GetBestGeneForMrna(), GetBestGeneForCds().
* Added CreateBioseqFromBioseq()
*
* Revision 1.136  2006/03/01 19:06:54  vasilche
* Avoid premature deletion of Seq-loc.
*
* Revision 1.135  2006/02/09 20:30:28  ucko
* The versions of CFastaOstream::Write that set up a temporary OM scope
* no longer need to take non-const arguments.
*
* Revision 1.134  2006/02/09 18:49:25  ucko
* CFastaOstream: support a saner (handle-based) SkipBioseq interface
* that delegates to the non-handle version by default for compatibility
* with older code.
*
* Revision 1.133  2005/11/07 15:40:19  vasilche
* Fixed warning in switch (enum).
*
* Revision 1.132  2005/08/31 21:43:55  ucko
* CFastaOstream::WriteSequence: don't forget a trailing newline!
*
* Revision 1.131  2005/08/18 20:48:07  ucko
* Rework CFastaOstream to use the object manager in a more modern fashion;
* in particular, take advantage of CSeqVector's support for gap reporting.
* Also, stop forcing linebreaks around instantiated gaps; their presence
* turns out not to have been in keeping with the C Toolkit after all.
*
* Revision 1.130  2005/06/22 14:05:32  grichenk
* Fixed error messages in x_GetId()
*
* Revision 1.129  2005/06/02 15:13:15  dicuccio
* Added GetMolInfo() - returns a pointer to the MolInfo object for a bioseq handle
*
* Revision 1.128  2005/04/28 17:54:33  dicuccio
* Updated to GetBestOverlappingFeature():
* - Modified x_GetBestOverlappingFeature() to return a list of possible features
* - Modified use of x_GetBestOverlappingFeature() to use the feature list as
*   appropriate
* - Modified GetbestGeneForMrna(), GetBestMrnaForCds(), GetBestCdsForMrna(),
*   GetBestGeneForCds() to place fast and easy checks first to improve
*   performance
*
* Revision 1.127  2005/04/26 15:36:18  vasilche
* Fixed warning about unused variable.
*
* Revision 1.126  2005/04/11 14:47:57  dicuccio
* Optimized feature overlap functions GetBestCdsForMrna(), GetBestMrnaForCds(),
* GetBestGenForMrna(), GetBestMrnaForGene(), GetBestGeneForCds(): rearranged
* order of tests; provide early check for only one possible feature and avoid
* complex checks if so
*
* Revision 1.125  2005/03/16 21:01:48  dicuccio
* Honor options provided to GetBestOverlappingFeat()
*
* Revision 1.124  2005/03/15 19:25:05  dicuccio
* Added additional check in GetBestMrnaForCds() and GetBestCdsForMrna(): check
* the NCBI-supplied build object for pointers to related CDS features (avoids
* retrieving product sequence)
*
* Revision 1.123  2005/03/14 18:19:02  grichenk
* Added SAnnotSelector(TFeatSubtype), fixed initialization of CFeat_CI and
* SAnnotSelector.
*
* Revision 1.122  2005/02/28 17:10:56  vasilche
* Do not exclude external annotations as they are marked properly by ID2 reader now.
*
* Revision 1.121  2005/02/23 19:20:21  dicuccio
* Guard against loading external annotations where not needed
*
* Revision 1.120  2005/02/18 15:03:43  shomrat
* IsPartialLeft changed to IsPartialStart
*
* Revision 1.119  2005/02/17 20:09:39  grichenk
* GetId(CBioseq_Handle) uses existing vector of ids from the handle.
*
* Revision 1.118  2005/02/17 15:58:42  grichenk
* Changes sequence::GetId() to return CSeq_id_Handle
*
* Revision 1.117  2005/02/07 14:15:53  dicuccio
* Bug fix: trap exceptions from CFeat_CI in x_GetBestOverlappingFeat(); return
* NULL CRef<> in such cases
*
* Revision 1.116  2005/02/02 19:49:55  grichenk
* Fixed more warnings
*
* Revision 1.115  2005/01/26 15:44:19  ucko
* CFastaOstream::WriteSequence: fix gap-instantiation logic to avoid
* infinite loops with the gap length isn't an exact multiple of the
* line width.
*
* Revision 1.114  2005/01/13 15:24:16  dicuccio
* Added optional flags to GetBestXxxForXxx() functions to control the types of
* checks performed
*
* Revision 1.113  2005/01/10 15:10:11  shomrat
* Changed GetAccessionForGi
*
* Revision 1.112  2005/01/06 21:04:25  ucko
* CFastaOstream: support CSeq_entry_Handle, and improve support for
* limiting the output to a particular location.
*
* Revision 1.111  2005/01/04 14:52:11  dicuccio
* Bug Fixes:
* - sequence::GetId(..., eGetId_ForceAcc): removed unententional fall-through
* - sequence::GetId(..., eGetId_Best): at least one ID is always available
*
* Revision 1.110  2004/12/16 20:48:01  shomrat
* cast char to Uint1 for array access
*
* Revision 1.109  2004/12/09 18:09:55  shomrat
* Changes to CSeqSearch (use static tables, added search flags
*
* Revision 1.108  2004/12/06 17:54:10  grichenk
* Replaced calls to deprecated methods
*
* Revision 1.107  2004/12/06 15:05:11  shomrat
* Added GetParentForPart and GetBioseqFromSeqLoc
*
* Revision 1.106  2004/11/22 16:09:14  dicuccio
* Revert to standard GetBestOverlappingFeat() in GetMrnasForGene() and
* GetCdssForGene() if no closely linked feature is found
*
* Revision 1.105  2004/11/19 15:10:10  shomrat
* Added GetBestOverlapForSNP
*
* Revision 1.104  2004/11/18 21:27:40  grichenk
* Removed default value for scope argument in seq-loc related functions.
*
* Revision 1.103  2004/11/17 21:25:13  grichenk
* Moved seq-loc related functions to seq_loc_util.[hc]pp.
* Replaced CNotUnique and CNoLength exceptions with CObjmgrUtilException.
*
* Revision 1.102  2004/11/02 15:44:55  ucko
* Tweak GetGiForAccession to compile on WorkShop.
*
* Revision 1.101  2004/11/01 19:33:09  grichenk
* Removed deprecated methods
*
* Revision 1.100  2004/11/01 17:14:35  shomrat
* + GetGiForAccession and GetAccessionForGi
*
* Revision 1.99  2004/10/26 14:18:18  grichenk
* Added processing of multi-strand locations in TestForOverlap
*
* Revision 1.98  2004/10/20 18:12:17  grichenk
* Fixed seq-id ranking.
*
* Revision 1.97  2004/10/13 12:57:10  shomrat
* Fixed overlap containmnet where applicable
*
* Revision 1.96  2004/10/12 18:57:57  dicuccio
* Added variant of sequence::GetId() that takes a seq-id instead of a bioseq
* handle
*
* Revision 1.95  2004/10/12 13:57:21  dicuccio
* Added convenience routines for finding: best mRNA for CDS feature; best gene
* for mRNA; best gene for CDS; all mRNAs for a gene; all CDSs for a gene.  Added
* new variant of GetBestOverlappingFeat() that takes a feature and uses the
* convenience routines above.
*
* Revision 1.94  2004/10/07 16:01:22  ucko
* Fix "invalid conversion" error brought on by last commit.
*
* Revision 1.93  2004/10/07 15:55:28  ucko
* Eliminate more uses of GetBioseq(Core).
*
* Revision 1.92  2004/10/01 19:16:05  grichenk
* Fixed bioseq length in TestForOverlap
*
* Revision 1.91  2004/10/01 15:33:46  grichenk
* TestForOverlap -- try to get bioseq's length for whole locations.
* Perform all calculations with Int8, check for overflow when
* returning int.
*
* Revision 1.90  2004/09/03 19:03:34  grichenk
* Catch exceptions from CSeq_loc::CheckId()
*
* Revision 1.89  2004/09/03 16:56:38  dicuccio
* Wrap test for circularity in a try/catch to trap errors in which a location
* spans multiple IDs
*
* Revision 1.88  2004/09/01 15:33:44  grichenk
* Check strand in GetStart and GetEnd. Circular length argument
* made optional.
*
* Revision 1.87  2004/08/26 18:26:27  grichenk
* Added check for circular sequence in x_GetBestOverlappingFeat()
*
* Revision 1.86  2004/08/24 16:42:03  vasilche
* Removed TAB symbols in sources.
*
* Revision 1.85  2004/08/19 15:26:56  shomrat
* Use Seq_loc_Mapper in SeqLocMerge
*
* Revision 1.84  2004/07/21 15:51:25  grichenk
* CObjectManager made singleton, GetInstance() added.
* CXXXXDataLoader constructors made private, added
* static RegisterInObjectManager() and GetLoaderNameFromArgs()
* methods.
*
* Revision 1.83  2004/07/12 16:54:43  vasilche
* Fixed compilation warnings.
*
* Revision 1.82  2004/05/27 13:48:24  jcherry
* Replaced some calls to deprecated CBioseq_Handle::GetBioseq() with
* GetCompleteBioseq() or GetBioseqCore()
*
* Revision 1.81  2004/05/25 21:06:34  johnson
* Bug fix in x_Translate
*
* Revision 1.80  2004/05/21 21:42:14  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.79  2004/05/06 17:39:43  shomrat
* Fixes to SeqLocMerge
*
* Revision 1.78  2004/04/06 14:03:15  dicuccio
* Added API to extract the single Org-ref from a bioseq handle.  Added API to
* retrieve a single tax-id from a bioseq handle
*
* Revision 1.77  2004/04/05 15:56:14  grichenk
* Redesigned CAnnotTypes_CI: moved all data and data collecting
* functions to CAnnotDataCollector. CAnnotTypes_CI is no more
* inherited from SAnnotSelector.
*
* Revision 1.76  2004/03/25 20:02:30  vasilche
* Added several method variants with CBioseq_Handle as argument.
*
* Revision 1.75  2004/03/01 18:28:18  dicuccio
* Changed sequence::Compare() such that a seq-interval of length 1 and a
* corresponding seq-point compare as the same
*
* Revision 1.74  2004/03/01 18:24:22  shomrat
* Removed branching in the main cdregion translation loop; Added alternative start flag
*
* Revision 1.73  2004/02/19 18:16:48  shomrat
* Implemented SeqlocRevCmp
*
* Revision 1.72  2004/02/09 14:43:18  vasilche
* Added missing typename keyword.
*
* Revision 1.71  2004/02/05 19:41:46  shomrat
* Convenience functions for popular overlapping types
*
* Revision 1.70  2004/02/04 18:05:41  grichenk
* Added annotation filtering by set of types/subtypes.
* Renamed *Choice to *Type in SAnnotSelector.
*
* Revision 1.69  2004/01/30 17:49:29  ucko
* Add missing "typename"
*
* Revision 1.68  2004/01/30 17:22:53  dicuccio
* Added sequence::GetId(const CBioseq_Handle&) - returns a selected ID class
* (best, GI).  Added CSeqTranslator - utility class to translate raw sequence
* data
*
* Revision 1.67  2004/01/28 17:19:04  shomrat
* Implemented SeqLocMerge
*
* Revision 1.66  2003/12/16 19:37:43  shomrat
* Retrieve encoding feature and bioseq of a protein
*
* Revision 1.65  2003/11/19 22:18:05  grichenk
* All exceptions are now CException-derived. Catch "exception" rather
* than "runtime_error".
*
* Revision 1.64  2003/10/17 20:55:27  ucko
* SRelLoc::Resolve: fix a fuzz-handling paste-o.
*
* Revision 1.63  2003/10/16 11:55:19  dicuccio
* Fix for brain-dead MSVC and ambiguous operator&&
*
* Revision 1.62  2003/10/15 19:52:18  ucko
* More adjustments to SRelLoc: support fuzz, opposite-strand children,
* and resolving against an alternate parent.
*
* Revision 1.61  2003/10/08 21:08:38  ucko
* CCdregion_translate: take const Bioseq_Handles, since there's no need
* to modify them.
* TestForOverlap: don't consider sequences circular if
* circular_len == kInvalidSeqPos
*
* Revision 1.60  2003/09/22 18:38:14  grichenk
* Fixed circular seq-locs processing by TestForOverlap()
*
* Revision 1.59  2003/07/17 21:00:50  vasilche
* Added missing include <list>
*
* Revision 1.58  2003/06/19 17:11:43  ucko
* SRelLoc::SRelLoc: remember to update the base position when skipping
* parent ranges whose IDs don't match.
*
* Revision 1.57  2003/06/13 17:23:32  grichenk
* Added special processing of multi-ID seq-locs in TestForOverlap()
*
* Revision 1.56  2003/06/05 18:08:36  shomrat
* Allow empty location when computing SeqLocPartial; Adjust GetSeqData in cdregion translation
*
* Revision 1.55  2003/06/02 18:58:25  dicuccio
* Fixed #include directives
*
* Revision 1.54  2003/06/02 18:53:32  dicuccio
* Fixed bungled commit
*
* Revision 1.52  2003/05/27 19:44:10  grichenk
* Added CSeqVector_CI class
*
* Revision 1.51  2003/05/15 19:27:02  shomrat
* Compare handle only if both valid; Check IsLim before GetLim
*
* Revision 1.50  2003/05/09 15:37:00  ucko
* Take const CBioseq_Handle references in CFastaOstream::Write et al.
*
* Revision 1.49  2003/05/06 19:34:36  grichenk
* Fixed GetStrand() (worked fine only for plus and unknown strands)
*
* Revision 1.48  2003/05/01 17:55:17  ucko
* Fix GetLength(const CSeq_id&, CScope*) to return ...::max() rather
* than throwing if it can't resolve the ID to a handle.
*
* Revision 1.47  2003/04/24 16:15:58  vasilche
* Added missing includes and forward class declarations.
*
* Revision 1.46  2003/04/16 19:44:26  grichenk
* More fixes to TestForOverlap() and GetStrand()
*
* Revision 1.45  2003/04/15 20:11:21  grichenk
* Fixed strands problem in TestForOverlap(), replaced type
* iterator with container iterator in s_GetStrand().
*
* Revision 1.44  2003/04/03 19:03:17  grichenk
* Two more cases to revert locations in GetBestOverlappingFeat()
*
* Revision 1.43  2003/04/02 16:58:59  grichenk
* Change location and feature in GetBestOverlappingFeat()
* for eOverlap_Subset.
*
* Revision 1.42  2003/03/25 22:00:20  grichenk
* Added strand checking to TestForOverlap()
*
* Revision 1.41  2003/03/18 21:48:35  grichenk
* Removed obsolete class CAnnot_CI
*
* Revision 1.40  2003/03/11 16:00:58  kuznets
* iterate -> ITERATE
*
* Revision 1.39  2003/02/19 16:25:14  grichenk
* Check strands in GetBestOverlappingFeat()
*
* Revision 1.38  2003/02/14 15:41:00  shomrat
* Minor implementation changes in SeqLocPartialTest
*
* Revision 1.37  2003/02/13 14:35:40  grichenk
* + eOverlap_Contains
*
* Revision 1.36  2003/02/10 15:54:01  grichenk
* Use CFeat_CI->GetMappedFeature() and GetOriginalFeature()
*
* Revision 1.35  2003/02/06 22:26:27  vasilche
* Use CSeq_id::Assign().
*
* Revision 1.34  2003/02/06 20:59:16  shomrat
* Bug fix in SeqLocPartialCheck
*
* Revision 1.33  2003/01/22 21:02:23  ucko
* Fix stupid logic error in SRelLoc's constructor; change LocationOffset
* to return -1 rather than crashing if the locations don't intersect.
*
* Revision 1.32  2003/01/22 20:15:02  vasilche
* Removed compiler warning.
*
* Revision 1.31  2003/01/22 18:17:09  ucko
* SRelLoc::SRelLoc: change intersection to a CRef, so we don't have to
* worry about it going out of scope while still referenced (by m_Ranges).
*
* Revision 1.30  2003/01/08 20:43:10  ucko
* Adjust SRelLoc to use (ID-less) Seq-intervals for ranges, so that it
* will be possible to add support for fuzz and strandedness/orientation.
*
* Revision 1.29  2002/12/30 19:38:35  vasilche
* Optimized CGenbankWriter::WriteSequence.
* Implemented GetBestOverlappingFeat() with CSeqFeatData::ESubtype selector.
*
* Revision 1.28  2002/12/26 21:45:29  grichenk
* + GetBestOverlappingFeat()
*
* Revision 1.27  2002/12/26 21:17:06  dicuccio
* Minor tweaks to avoid compiler warnings in MSVC (remove unused variables)
*
* Revision 1.26  2002/12/20 17:14:18  kans
* added SeqLocPartialCheck
*
* Revision 1.25  2002/12/19 20:24:55  grichenk
* Updated usage of CRange<>
*
* Revision 1.24  2002/12/10 16:56:41  ucko
* CFastaOstream::WriteTitle: restore leading > accidentally dropped in R1.19.
*
* Revision 1.23  2002/12/10 16:34:45  kans
* implement code break processing in TranslateCdregion
*
* Revision 1.22  2002/12/09 20:38:41  ucko
* +sequence::LocationOffset
*
* Revision 1.21  2002/12/06 15:36:05  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.20  2002/12/04 15:38:22  ucko
* SourceToProduct, ProductToSource: just check whether the feature is a coding
* region rather than attempting to determine molecule types; drop s_DeduceMol.
*
* Revision 1.19  2002/11/26 15:14:04  dicuccio
* Changed CFastaOStream::WriteTitle() to make use of CSeq_id::GetStringDescr().
*
* Revision 1.18  2002/11/25 21:24:46  grichenk
* Added TestForOverlap() function.
*
* Revision 1.17  2002/11/18 19:59:23  shomrat
* Add CSeqSearch - a nucleotide search utility
*
* Revision 1.16  2002/11/12 20:00:25  ucko
* +SourceToProduct, ProductToSource, SRelLoc
*
* Revision 1.15  2002/10/23 19:22:39  ucko
* Move the FASTA reader from objects/util/sequence.?pp to
* objects/seqset/Seq_entry.?pp because it doesn't need the OM.
*
* Revision 1.14  2002/10/23 18:23:59  ucko
* Clean up #includes.
* Add a FASTA reader (known to compile, but not otherwise tested -- take care)
*
* Revision 1.13  2002/10/23 16:41:12  clausen
* Fixed warning in WorkShop53
*
* Revision 1.12  2002/10/23 15:33:50  clausen
* Fixed local variable scope warnings
*
* Revision 1.11  2002/10/08 12:35:37  clausen
* Fixed bugs in GetStrand(), ChangeSeqId() & SeqLocCheck()
*
* Revision 1.10  2002/10/07 17:11:16  ucko
* Fix usage of ERR_POST (caught by KCC)
*
* Revision 1.9  2002/10/03 18:44:09  clausen
* Removed extra whitespace
*
* Revision 1.8  2002/10/03 16:33:55  clausen
* Added functions needed by validate
*
* Revision 1.7  2002/09/13 15:30:43  ucko
* Change resize(0) to erase() at Denis's suggestion.
*
* Revision 1.6  2002/09/13 14:28:34  ucko
* string::clear() doesn't exist under g++ 2.95, so use resize(0) instead. :-/
*
* Revision 1.5  2002/09/12 21:39:13  kans
* added CCdregion_translate and CCdregion_translate
*
* Revision 1.4  2002/09/03 21:27:04  grichenk
* Replaced bool arguments in CSeqVector constructor and getters
* with enums.
*
* Revision 1.3  2002/08/27 21:41:15  ucko
* Add CFastaOstream.
*
* Revision 1.2  2002/06/07 16:11:09  ucko
* Move everything into the "sequence" namespace.
* Drop the anonymous-namespace business; "sequence" should provide
* enough protection, and anonymous namespaces ironically interact poorly
* with WorkShop's caching when rebuilding shared libraries.
*
* Revision 1.1  2002/06/06 18:41:41  clausen
* Initial version
*
* ===========================================================================
*/
