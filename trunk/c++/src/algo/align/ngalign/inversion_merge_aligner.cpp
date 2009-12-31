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
 * Author:  Nathan Bouk
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbi_system.hpp>
#include <math.h>

#include <algo/align/ngalign/inversion_merge_aligner.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/scope.hpp>
#include <algo/blast/api/blast_types.hpp>
#include <algo/blast/api/bl2seq.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/blast_nucl_options.hpp>
#include <algo/blast/api/disc_nucl_options.hpp>
#include <algo/align/util/score_builder.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objmgr/seq_vector.hpp>

#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/blastinput/blastn_args.hpp>


#include <algo/sequence/align_cleanup.hpp>

BEGIN_SCOPE(ncbi)
USING_SCOPE(objects);



TAlignResultsRef
CInversionMergeAligner::GenerateAlignments(objects::CScope& Scope,
                                    ISequenceSet* QuerySet,
                                    ISequenceSet* SubjectSet,
                                    TAlignResultsRef AccumResults)
{
    TAlignResultsRef NewResults(new CAlignResultsSet);

    NON_CONST_ITERATE(CAlignResultsSet::TQueryToSubjectSet, QueryIter,
                      AccumResults->Get()) {
        int BestRank = QueryIter->second->GetBestRank();
        if(BestRank > m_Threshold || BestRank == -1) {
            ERR_POST(Info << "Determined ID: "
                          << QueryIter->second->GetQueryId()->AsFastaString()
                          << " needs Inversion Merger.");
            x_RunMerger(Scope, *QueryIter->second, NewResults);
        }
    }

    return NewResults;
}


bool CInversionMergeAligner::s_SortByPctCoverage(const CRef<CSeq_align>& A,
                                                 const CRef<CSeq_align>& B)
{
    bool PctCovExists[2];
    double PctCovs[2];

    PctCovExists[0] = A->GetNamedScore("pct_coverage", PctCovs[0]);
    PctCovExists[1] = B->GetNamedScore("pct_coverage", PctCovs[1]);

    if(!PctCovExists[0] || !PctCovExists[1])
        return true;

    return ( PctCovs[0] >= PctCovs[1] );
}


void CInversionMergeAligner::x_RunMerger(objects::CScope& Scope,
                                     CQuerySet& QueryAligns,
                                     TAlignResultsRef Results)
{
    CRef<CSeq_align_set> ResultSet(new CSeq_align_set);


    ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter,
                      QueryAligns.Get()) {

        CRef<CSeq_align_set> SubjectSet = SubjectIter->second;

        CRef<CSeq_align_set> Pluses(new CSeq_align_set),
                             Minuses(new CSeq_align_set);

        x_SplitAlignmentsByStrand(*SubjectSet, *Pluses, *Minuses);

        //x_HandleSingleStrandMerging(*Pluses, *ResultSet, Scope);
        //x_HandleSingleStrandMerging(*Minuses, *ResultSet, Scope);

        if(Pluses->Set().empty() || Minuses->Set().empty()) {
            continue;
        }

        x_SortAlignSet(*Pluses);
        x_SortAlignSet(*Minuses);


        double PctCovs[2] = { 0.0, 0.0 };

        Pluses->Get().front()->GetNamedScore("pct_coverage", PctCovs[0]);
        Minuses->Get().front()->GetNamedScore("pct_coverage", PctCovs[1]);


        CRef<CSeq_align_set> Dominant, NonDominant;
        if(PctCovs[0] >= PctCovs[1]) {
            Dominant = Pluses;
            NonDominant = Minuses;
        } else {
            Dominant = Minuses;
            NonDominant = Pluses;
        }

        bool Made = false;
        ITERATE(CSeq_align_set::Tdata, DomIter, Dominant->Get()) {
            if(Made)
                break;
            ITERATE(CSeq_align_set::Tdata, NonIter, NonDominant->Get()) {

                (*DomIter)->GetNamedScore("pct_coverage", PctCovs[0]);
                (*NonIter)->GetNamedScore("pct_coverage", PctCovs[1]);

                CRange<TSeqPos> DomSubjRange = (*DomIter)->GetSeqRange(1);
                CRange<TSeqPos> NonSubjRange = (*NonIter)->GetSeqRange(1);

                TSeqPos Dist;
                if(DomSubjRange.IntersectingWith(NonSubjRange)) {
                    Dist = 0;
                } else {
                    if(DomSubjRange.GetTo() > NonSubjRange.GetTo())
                        Dist = DomSubjRange.GetFrom() - NonSubjRange.GetTo();
                    else
                        Dist = NonSubjRange.GetFrom() - DomSubjRange.GetTo();
                }
                if( (PctCovs[0]/PctCovs[1]) <= 10.0 &&
                    (PctCovs[0]/PctCovs[1]) >= 1.0 &&
                    Dist <= 100000) {
                    // Trim Non.
                    CRef<CSeq_align> Disc(new CSeq_align);
                    Disc = x_CreateDiscAlignment(**DomIter, **NonIter, Scope);
                    if(!Disc.IsNull()) {
                        ResultSet->Set().push_back(Disc);
                        Made = true;
                        break;
                    }
                }
            }
        }
    } // end Subject Set Loop

    if(!ResultSet->Get().empty()) {
        Results->Insert(CRef<CQuerySet>(new CQuerySet(*ResultSet)));
    }
}


void CInversionMergeAligner::x_SortAlignSet(CSeq_align_set& AlignSet)
{
    vector<CRef<CSeq_align> > TempVec;
    TempVec.reserve(AlignSet.Set().size());
    TempVec.resize(AlignSet.Set().size());
    copy(AlignSet.Set().begin(), AlignSet.Set().end(), TempVec.begin());
    sort(TempVec.begin(), TempVec.end(), s_SortByPctCoverage);
    AlignSet.Set().clear();
    copy(TempVec.begin(), TempVec.end(),
        insert_iterator<CSeq_align_set::Tdata>(AlignSet.Set(), AlignSet.Set().end()));
}


void CInversionMergeAligner::x_SplitAlignmentsByStrand(const CSeq_align_set& Source,
                           CSeq_align_set& Pluses, CSeq_align_set& Minuses)
{
    ITERATE(CSeq_align_set::Tdata, AlignIter, Source.Get()) {
        if( (*AlignIter)->GetSeqStrand(0) == eNa_strand_plus)
            Pluses.Set().push_back(*AlignIter);
        else if( (*AlignIter)->GetSeqStrand(0) == eNa_strand_minus)
            Minuses.Set().push_back(*AlignIter);
    }
}


void CInversionMergeAligner::x_HandleSingleStrandMerging(CSeq_align_set& Source,
                                                   CSeq_align_set& Results,
                                                   CScope& Scope)
{
    CScoreBuilder Scorer;

    if(Source.CanGet() && !Source.Get().empty()) {
        CRef<CSeq_align_set> Out;
        Out = x_MergeSeqAlignSet(Source, Scope);
        if(!Out.IsNull() && Out->CanGet() && !Out->Get().empty()) {
            NON_CONST_ITERATE(CSeq_align_set::Tdata, MergedIter, Out->Set()) {
                Scorer.AddScore(Scope, **MergedIter, CScoreBuilder::eScore_PercentIdentity);
                Scorer.AddScore(Scope, **MergedIter, CScoreBuilder::eScore_PercentCoverage);
                Source.Set().push_back(*MergedIter);
                Results.Set().push_back(*MergedIter);
            }
        }
    }
}



CRef<CSeq_align>
CInversionMergeAligner::x_CreateDiscAlignment(const CSeq_align& Dom, const CSeq_align& Non, CScope& Scope)
{
    if(Dom.GetSegs().Which() != CSeq_align::C_Segs::e_Denseg ||
       Non.GetSegs().Which() !=CSeq_align::C_Segs::e_Denseg) {
        return CRef<CSeq_align>();
    }

    CRef<CSeq_align> Disc(new CSeq_align);
    Disc->SetType() = CSeq_align::eType_disc;

    CRef<CSeq_align> DomRef(new CSeq_align), NonRef(new CSeq_align);
    DomRef->Assign(Dom);
    NonRef->Assign(Non);

    // Run Clean up on the two parts, as an attempt to uniqify what they cover.
    CRef<CSeq_align_set> Source(new CSeq_align_set), Cleaned;
    Source->Set().push_back(DomRef);
    Source->Set().push_back(NonRef);
    Cleaned = x_MergeSeqAlignSet(*Source, Scope);

    if(Cleaned.IsNull() || Cleaned->Set().size() != 2) {
        return CRef<CSeq_align>();
    } else {
        DomRef = Cleaned->Set().front();
        NonRef = *( ++(Cleaned->Set().begin()) );
    }

    CDense_seg& DomSeg = DomRef->SetSegs().SetDenseg();
    CDense_seg& NonSeg = NonRef->SetSegs().SetDenseg();

// Find dupes
    int DomIndex = 0, NonIndex = 0;
    for(DomIndex = 0; DomIndex < DomSeg.GetNumseg(); DomIndex++) {

        if(DomSeg.GetStarts()[DomIndex*DomSeg.GetDim()] == -1)
            continue;

        CRange<TSeqPos> DomRange(DomSeg.GetStarts()[DomIndex*DomSeg.GetDim()],
                                 DomSeg.GetStarts()[DomIndex*DomSeg.GetDim()]+
                                 DomSeg.GetLens()[DomIndex]-1);

        for(NonIndex = 0; NonIndex < NonSeg.GetNumseg(); NonIndex++) {

            if(NonSeg.GetStarts()[NonIndex*NonSeg.GetDim()] == -1)
                continue;

            CRange<TSeqPos> NonRange(NonSeg.GetStarts()[NonIndex*NonSeg.GetDim()],
                                     NonSeg.GetStarts()[NonIndex*NonSeg.GetDim()]+
                                     NonSeg.GetLens()[NonIndex]-1);

            if(DomRange.IntersectingWith(NonRange)) {
                ;
            }
        }
    }

// Find data in gaps
    DomIndex = 0, NonIndex = 0;
    for(DomIndex = 0; DomIndex < DomSeg.GetNumseg(); DomIndex++) {

        CRange<TSeqPos> DomRange;

        if(DomSeg.GetStarts()[(DomIndex*DomSeg.GetDim())+1] == -1) {
            DomRange.SetFrom(DomSeg.GetStarts()[DomIndex*DomSeg.GetDim()]);
            DomRange.SetLength(DomSeg.GetLens()[DomIndex]);
        } else {
            continue;
        }

        for(NonIndex = 0; NonIndex < NonSeg.GetNumseg(); NonIndex++) {

            if(NonSeg.GetStarts()[NonIndex*NonSeg.GetDim()] == -1 ||
               NonSeg.GetStarts()[(NonIndex*NonSeg.GetDim())+1] == -1)
                continue;

            CRange<TSeqPos> NonRange(NonSeg.GetStarts()[NonIndex*NonSeg.GetDim()],
                                     NonSeg.GetStarts()[NonIndex*NonSeg.GetDim()]+
                                     NonSeg.GetLens()[NonIndex]-1);

            if(DomRange.IntersectingWith(NonRange)) {
                if(NonRange.GetLength() <= 16) {
                    NonSeg.SetStarts()[NonIndex*NonSeg.GetDim()] = -1;
                }
            }
        }
    }

    NonSeg.RemovePureGapSegs();
    NonSeg.Compact();
    NonSeg.TrimEndGaps();
    NonSeg.Assign(*NonSeg.FillUnaligned());


    Disc->SetSegs().SetDisc().Set().push_back(DomRef);
    Disc->SetSegs().SetDisc().Set().push_back(NonRef);

    return Disc;
}




CRef<objects::CSeq_align_set>
CInversionMergeAligner::x_MergeSeqAlignSet(const CSeq_align_set& InAligns, objects::CScope& Scope)
{
    list<CConstRef<CSeq_align> > In;
    ITERATE(CSeq_align_set::Tdata, AlignIter, InAligns.Get()) {
        CConstRef<CSeq_align> Align(*AlignIter);
        In.push_back(Align);
    }

    CRef<CSeq_align_set> Out(new CSeq_align_set);

    try {
        CAlignCleanup Cleaner(Scope);
        Cleaner.FillUnaligned(true);
        Cleaner.Cleanup(In, Out->Set());
    } catch(CException& e) {
        ERR_POST(Info << "Cleanup Error: " << e.ReportAll());
        return CRef<CSeq_align_set>();
    }

    NON_CONST_ITERATE(CSeq_align_set::Tdata, AlignIter, Out->Set()) {
        CRef<CSeq_align> Align(*AlignIter);
        CDense_seg& Denseg = Align->SetSegs().SetDenseg();

        if(!Denseg.CanGetStrands() || Denseg.GetStrands().empty()) {
            Denseg.SetStrands().resize(Denseg.GetDim()*Denseg.GetNumseg(), eNa_strand_plus);
        }

        if(Denseg.GetSeqStrand(1) != eNa_strand_plus) {
            Denseg.Reverse();
        }

        CRef<CDense_seg> Filled = Denseg.FillUnaligned();
        Denseg.Assign(*Filled);

        Align->SetNamedScore("merged_alignment", 1);
    }

    if(Out->Set().empty())
        return CRef<CSeq_align_set>();
    return Out;
}




END_SCOPE(ncbi)
//end
