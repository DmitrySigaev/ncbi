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
#include <algo/align/ngalign/banded_aligner.hpp>
//#include "align_instance.hpp"

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

#include <algo/align/contig_assembly/contig_assembly.hpp>
#include <algo/align/nw/nw_band_aligner.hpp>
#include <algo/align/nw/mm_aligner.hpp>

#include <algo/align/util/blast_tabular.hpp>
#include <algo/align/util/compartment_finder.hpp>
#include <algo/sequence/align_cleanup.hpp>


BEGIN_SCOPE(ncbi)
USING_SCOPE(objects);
USING_SCOPE(blast);




CRef<CSeq_loc> s_CoverageSeqLoc(TAlignSetRef Alignments, int Row, CScope& Scope)
{
    CRef<CSeq_loc> Accum(new CSeq_loc);
    ITERATE(CSeq_align_set::Tdata, Iter, Alignments->Get()) {

        const CSeq_align& Curr = **Iter;
        const CDense_seg& Denseg = Curr.GetSegs().GetDenseg();

        size_t Dims = Denseg.GetDim();
        size_t SegCount = Denseg.GetNumseg();
        for(size_t CurrSeg = 0; CurrSeg < SegCount; ++CurrSeg) {
            int Index = (Dims*CurrSeg)+Row;
            int CurrStart = Denseg.GetStarts()[Index];
            if( CurrStart != -1) {
                CRef<CSeq_loc> CurrLoc(new CSeq_loc);
                CurrLoc->SetInt().SetId().Assign( *Denseg.GetIds()[Row] );
                CurrLoc->SetInt().SetFrom() = CurrStart;
                CurrLoc->SetInt().SetTo() = CurrStart + Denseg.GetLens()[CurrSeg];
                if(Denseg.CanGetStrands())
                    CurrLoc->SetInt().SetStrand() = Denseg.GetStrands()[Index];
                Accum->SetMix().Set().push_back(CurrLoc);
            }
        }
    }

    CRef<CSeq_loc> Merged;
    Merged = sequence::Seq_loc_Merge(*Accum, CSeq_loc::fSortAndMerge_All, &Scope);

    return Merged;
}


TSeqPos s_CalcCoverageCount(TAlignSetRef Alignments, int Row, CScope& Scope)
{

    CRef<CSeq_loc> Merged;

    Merged = s_CoverageSeqLoc(Alignments, Row, Scope);
    //cout << MSerial_AsnText << *Merged;

    Merged->ChangeToMix();
    TSeqPos PosCoveredBases = 0, NegCoveredBases = 0;
    ITERATE(CSeq_loc_mix::Tdata, LocIter, Merged->GetMix().Get()) {

//    ITERATE(CPacked_seqint_Base::Tdata, IntIter, Merged->GetPacked_int().Get())
        if( (*LocIter)->Which() == CSeq_loc::e_Int) {
            if( (*LocIter)->GetInt().GetStrand() == eNa_strand_plus)
                PosCoveredBases += (*LocIter)->GetInt().GetLength();
            else
                NegCoveredBases += (*LocIter)->GetInt().GetLength();
        }
    }
    //cerr << "Covered: " << PosCoveredBases << " or " << NegCoveredBases
    //     << "  len: " << m_Scope->GetBioseqHandle(*Merged->GetId()).GetBioseqLength() << endl;

    return max(PosCoveredBases, NegCoveredBases);
}




TAlignResultsRef
CSimpleBandedAligner::GenerateAlignments(CScope& Scope,
                                         ISequenceSet* QuerySet,
                                         ISequenceSet* SubjectSet,
                                         TAlignResultsRef AccumResults)
{
    TAlignResultsRef NewResults(new CAlignResultsSet);

    NON_CONST_ITERATE(CAlignResultsSet::TQueryToSubjectSet,
                      QueryIter, AccumResults->Get()) {

        int BestRank = QueryIter->second->GetBestRank();
        if(BestRank > m_Threshold || BestRank == -1) {
                ERR_POST(Info << "Determined ID: "
                          << QueryIter->second->GetQueryId()->AsFastaString()
                          << " needs Instanced NW Aligner.");
            x_RunBanded(Scope, *QueryIter->second, NewResults);
        }
/*        CAlignResultsSet::TQueryToSubjectSet::iterator ResultsFound;
        ResultsFound = GoodResults->Get().find(QueryIter->first);
        // No results for this query survived the filter, run banded for it
        if(ResultsFound == GoodResults->Get().end()) {
            ERR_POST(Info << "Determined ID: "
                          << QueryIter->second->GetQueryId()->AsFastaString()
                          << " needs Instanced NW Aligner.");
            x_RunBanded(Scope, QueryIter->second, NewResults);
        }*/
    }

    return NewResults;
}




void CSimpleBandedAligner::x_RunBanded(objects::CScope& Scope,
                                       CQuerySet& QueryAligns,
                                       TAlignResultsRef Results)
{

    unsigned int Diag = 0;
    unsigned int DiagFindingWindow = 200;
    ENa_strand Strand;

    TAlignSetRef HighestCoverageSet;
    TSeqPos HighestCoverage = 0;
    CSeq_id Query, Subject;
    Query.Assign(*QueryAligns.GetQueryId());

    NON_CONST_ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter, QueryAligns.Get()) {
        TSeqPos CurrCoverage;
        CurrCoverage = s_CalcCoverageCount(SubjectIter->second, 0, Scope);
        if(CurrCoverage > HighestCoverage) {
            HighestCoverage = CurrCoverage;
            HighestCoverageSet = SubjectIter->second;
            Subject.Assign(SubjectIter->second->Get().front()->GetSeq_id(1));
        }
    }

    if(HighestCoverageSet.IsNull()) {
        ERR_POST(Warning << "No decent coverage set found, to run Banded on " << Query.AsFastaString());
        return;
    } else {
        ERR_POST(Info << "Found Subject " << Subject.AsFastaString()
                      << " with HighestCoverage of " << HighestCoverage
                      << " from " << HighestCoverageSet->Get().size() << " alignments.");
    }

    CContigAssembly::FindDiagFromAlignSet(*HighestCoverageSet, Scope,
                                          DiagFindingWindow,
                                          Strand, Diag);
    ERR_POST(Info << "Banded plans to use Strand: " << Strand << " and Diag: " << Diag);


    CRef<CDense_seg> GlobalDs;
    GlobalDs = x_RunBandedGlobal(//CContigAssembly::BandedGlobalAlignment
                Query, Subject, Strand, Diag, m_BandWidth/2, Scope);

    CRef<CDense_seg> LocalDs;
    LocalDs = CContigAssembly::BestLocalSubAlignment(*GlobalDs, Scope);

    CRef<CSeq_align> Result(new CSeq_align);
    Result->SetSegs().SetDenseg().Assign(*LocalDs);
    Result->SetType() = CSeq_align::eType_partial;

    CRef<CSeq_align_set> ResultSet(new CSeq_align_set);
    ResultSet->Set().push_back(Result);
    //ERR_POST(Info << "Banded Result: " << MSerial_AsnText << *ResultSet);
    //cerr << "Info: " << "Banded Result: " << MSerial_AsnText << *ResultSet;

    Results->Insert(CRef<CQuerySet>(new CQuerySet(*ResultSet)));

}


CRef<CDense_seg> CSimpleBandedAligner::x_RunBandedGlobal(CSeq_id& QueryId,
                                               CSeq_id& SubjectId,
                                               ENa_strand Strand,
                                               TSeqPos Diagonal,
                                               int BandHalfWidth,
                                               CScope& Scope)
{

    // ASSUMPTION: Query is short, Subject is long.
    //     We should be able to align all of Query against a subsection of Subject
    // Banded Memory useage is Longer Sequence * Band Width * unit size (1)
    // The common case is that Subject will be a very sizeable Scaffold.
    //  a bandwidth of anything more than 10-500 can pop RAM on a 16G machine.
    //  So we're gonna snip Subject to a subregion

    CBioseq_Handle::EVectorStrand VecStrand;
    VecStrand = (Strand == eNa_strand_plus ? CBioseq_Handle::eStrand_Plus
                                           : CBioseq_Handle::eStrand_Minus);

    CBioseq_Handle QueryHandle, SubjectHandle;
    QueryHandle = Scope.GetBioseqHandle(QueryId);
    SubjectHandle = Scope.GetBioseqHandle(SubjectId);

    CSeqVector QueryVec, SubjectVec;
    QueryVec = QueryHandle.GetSeqVector(CBioseq_Handle::eCoding_Iupac, VecStrand);
    SubjectVec = SubjectHandle.GetSeqVector(CBioseq_Handle::eCoding_Iupac);

    unsigned int SubjectLengthConst = 3;
    unsigned int SubjectHalfLength = SubjectLengthConst * QueryVec.size();
    TSeqPos SubjectStart = ((Diagonal > SubjectHalfLength)
                         ?  (Diagonal - SubjectHalfLength) : 0);
    TSeqPos SubjectStop  = (((Diagonal + SubjectHalfLength) > SubjectVec.size())
                         ?  SubjectVec.size() : (Diagonal + SubjectHalfLength));

    string QuerySeq, SubjectSeq;
    QueryVec.GetSeqData(0, QueryVec.size(), QuerySeq);
    SubjectVec.GetSeqData(SubjectStart, SubjectStop, SubjectSeq);

    ERR_POST(Info << "Banded Aligning: ");
    ERR_POST(Info << "Query 0 to " << QueryVec.size() << " against");
    ERR_POST(Info << "Subject " << SubjectStart << " to " << SubjectStop);
    // We have the sequence strings, now we try and set up CBandAligner
    Uint1 Direction;
    size_t Shift;

    Diagonal -= SubjectStart;

    if(Diagonal <= QuerySeq.size()) {
        Direction = 0;
        Shift = QuerySeq.size() - 1 - Diagonal;
    } else {
        Direction = 1;
        Shift = Diagonal - (QuerySeq.size() - 1);
    }


    CBandAligner Banded(QuerySeq, SubjectSeq, 0, BandHalfWidth);
    Banded.SetEndSpaceFree(true, true, true, true);
    Banded.SetWms(-3);
    Banded.SetWg(-1);
    Banded.SetWs(-1);
    Banded.SetScoreMatrix(NULL);
    Banded.SetShift(Direction, Shift);
    while(true) {
        Banded.SetBand(BandHalfWidth);
        try {
            //cerr << "Trying BandHalfWidth: " << BandHalfWidth << endl;
            Banded.Run();
            break;
        } catch(CException& e) {
            BandHalfWidth = (int)(BandHalfWidth * 0.8);
            if(BandHalfWidth < 100) {
                cerr << e.ReportAll() << endl;
                throw e;
            }
        }
    }


    // Extract Results
    TSeqPos QueryStart = ((Strand == eNa_strand_plus) ? 0 : (QuerySeq.size() - 1));
    CRef<CDense_seg> ResultDenseg;
    ResultDenseg = Banded.GetDense_seg(QueryStart, Strand, QueryId,
                                       0, eNa_strand_plus, SubjectId);


    ResultDenseg->TrimEndGaps();

    if(ResultDenseg->GetNumseg() > 0)
        ResultDenseg->OffsetRow(1, SubjectStart);


    return ResultDenseg;
}


///////



TAlignResultsRef CInstancedAligner::GenerateAlignments(objects::CScope& Scope,
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
                          << " needs Instanced MM Aligner.");
            x_RunAligner(Scope, *QueryIter->second, NewResults);
        }
    }

    return NewResults;
}




void CInstancedAligner::x_RunAligner(objects::CScope& Scope,
                                     CQuerySet& QueryAligns,
                                     TAlignResultsRef Results)
{
    CRef<CSeq_align_set> ResultSet(new CSeq_align_set);

    vector<CRef<CInstance> > Instances;
    x_GetCleanupInstances(QueryAligns, Scope, Instances);
    x_GetDistanceInstances(QueryAligns, Scope, Instances);
    if(Instances.empty()) {
        ERR_POST(Info << " No instances of " << QueryAligns.GetQueryId()->AsFastaString() << " found.");
        return;
    }


    TSeqPos LongestInstance = 0;
    ITERATE(vector<CRef<CInstance> >, InstIter, Instances) {
        const CInstance& Inst = **InstIter;
        TSeqPos CurrLength = (Inst.Query.GetTo() - Inst.Query.GetFrom());
        LongestInstance = max(CurrLength, LongestInstance);
    }

    CRef<CSeq_align> Result(new CSeq_align);
    ITERATE(vector<CRef<CInstance> >, InstIter, Instances) {

        const CInstance& Inst = **InstIter;

        if( (Inst.Query.GetTo() - Inst.Query.GetFrom()) <= (LongestInstance*0.005)) {
            continue;
        }

        ERR_POST(Info << " Aligning " << Inst.Query.GetId().AsFastaString() << " to "
                                     << Inst.Subject.GetId().AsFastaString());
        ERR_POST(Info << "  q:" << Inst.Query.GetFrom() << ":"
                                << Inst.Query.GetTo() << ":"
                                << (Inst.Query.GetStrand() == eNa_strand_plus ? "+" : "-")
                      << " and s: " << Inst.Subject.GetFrom() << ":"
                                    << Inst.Subject.GetTo() << ":"
                                    << (Inst.Subject.GetStrand() == eNa_strand_plus ? "+" : "-"));

        CRef<CDense_seg> GlobalDs;
        try {
            GlobalDs = x_RunMMGlobal(
                            Inst.Query.GetId(), Inst.Subject.GetId(),
                            Inst.Query.GetStrand(),
                            Inst.Query.GetFrom(), Inst.Query.GetTo(),
                            Inst.Subject.GetFrom(), Inst.Subject.GetTo(),
                            Scope);
        } catch(CException& e) {
            ERR_POST(Error << e.ReportAll());
            ERR_POST(Error << "CInstancedBandedAligner failed.");
            continue;
        }

        GetDiagContext().Extra()
            .Print("instance_query", Inst.Query.GetId().AsFastaString())
            .Print("instance_subject", Inst.Subject.GetId().AsFastaString())
            .Print("instance_align", (GlobalDs.IsNull() ? "false" : "true"));

        if(GlobalDs.IsNull())
            continue;

        Result->SetSegs().SetDenseg().Assign(*GlobalDs);
        Result->SetType() = CSeq_align::eType_partial;
        ResultSet->Set().push_back(Result);

        ERR_POST(Info << " Found alignment ");

/*
        // CContigAssembly::BestLocalSubAlignmentrescores, and finds better-scoring
        // sub-regions of the Denseg, and keeps only the subregion.
        // The practical result is, if the alignment has a large gap near the edges
        //  that gap might be trimmed. If there is more matching region past the
        //  too-big gap, that matching region is lost. Sometimes we don't want that
        //  especially in the Instance case, because we trust the region so much
        // So, (see below) if this function changes the Denseg, we return both, the
        //  trimmed and the untrimmed.
        CRef<CDense_seg> LocalDs;
        LocalDs = CContigAssembly::BestLocalSubAlignment(*GlobalDs, Scope);

        Result->SetSegs().SetDenseg().Assign(*LocalDs);
        Result->SetType() = CSeq_align::eType_partial;
        ResultSet->Set().push_back(Result);

        if(!LocalDs->Equals(*GlobalDs)) {
            CRef<CSeq_align> Result(new CSeq_align);
            Result->SetSegs().SetDenseg().Assign(*GlobalDs);
            Result->SetType() = CSeq_align::eType_partial;
            ResultSet->Set().push_back(Result);
        }
*/
    }

    if(!ResultSet->Get().empty()) {
        Results->Insert(CRef<CQuerySet>(new CQuerySet(*ResultSet)));
    }
}


struct SCallbackData
{
    CTime   StartTime;
    int     TimeOutSeconds;
    size_t  PreviousCount;
    bool    TimedOut;
};


bool s_ProgressCallback(CNWAligner::SProgressInfo* ProgressInfo)
{
    SCallbackData* CallbackData = (SCallbackData*)ProgressInfo->m_data;
    CTime CurrTime(CTime::eCurrent);
    CTimeSpan Span = (CurrTime-CallbackData->StartTime);

    if(CallbackData->TimedOut) {
        return true;
    }

    if(CallbackData->PreviousCount == ProgressInfo->m_iter_done ||
       Span.GetAsDouble() < 1.0) {
        CallbackData->PreviousCount = ProgressInfo->m_iter_done;
        return false;
    }

    CallbackData->PreviousCount = ProgressInfo->m_iter_done;

//    ERR_POST(Info << "Counts: " << ProgressInfo->m_iter_done << " of " << ProgressInfo->m_iter_total);

    double PercentComplete = (double(ProgressInfo->m_iter_done)/ProgressInfo->m_iter_total);
    double PercentRemaining = 1.0-PercentComplete;

    double Factor = PercentRemaining/PercentComplete;


    if( Span.GetCompleteSeconds() > CallbackData->TimeOutSeconds) {
        ERR_POST(Error << " Instanced Aligner took over 5 minutes. Timed out.");
        CallbackData->TimedOut = true;
        return true;
    }

    double TimeEstimated = Span.GetAsDouble() * Factor;

    if(TimeEstimated > CallbackData->TimeOutSeconds) {
        ERR_POST(Error << " Instanced Aligner expected to take " << TimeEstimated
                       << " seconds. More than " << (CallbackData->TimeOutSeconds/60.0)
                       << " minutes. Terminating Early.");
        CallbackData->TimedOut = true;
        return true;
    }


    return false;
}



CRef<CDense_seg> CInstancedAligner::x_RunMMGlobal(const CSeq_id& QueryId,
                                                    const CSeq_id& SubjectId,
                                                    ENa_strand Strand,
                                                    TSeqPos QueryStart,
                                                    TSeqPos QueryStop,
                                                    TSeqPos SubjectStart,
                                                    TSeqPos SubjectStop,
                                                    CScope& Scope)
{

    // ASSUMPTION: Query is short, Subject is long.
    //     We should be able to align all of Query against a subsection of Subject
    // Banded Memory useage is Longer Sequence * Band Width * unit size (1)
    // The common case is that Subject will be a very sizeable Scaffold.
    //  a bandwidth of anything more than 10-500 can pop RAM on a 16G machine.
    //  So we're gonna snip Subject to a subregion
    // CMMAligner uses linear RAM, but we still use the restricted region because
    //  we know where the regions are.

    CBioseq_Handle::EVectorStrand VecStrand;
    VecStrand = (Strand == eNa_strand_plus ? CBioseq_Handle::eStrand_Plus
                                           : CBioseq_Handle::eStrand_Minus);

    CBioseq_Handle QueryHandle, SubjectHandle;
    QueryHandle = Scope.GetBioseqHandle(QueryId);
    SubjectHandle = Scope.GetBioseqHandle(SubjectId);

    CSeqVector QueryVec, SubjectVec;
    QueryVec = QueryHandle.GetSeqVector(CBioseq_Handle::eCoding_Iupac, VecStrand);
    SubjectVec = SubjectHandle.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
/*
    int Wiggle = 0;

    QueryStart = ( QueryStart > Wiggle ? QueryStart-Wiggle : 0);
    QueryStop = ( QueryStop + Wiggle < QueryVec.size() ? QueryStop+Wiggle : QueryVec.size()-1);

    SubjectStart = ( SubjectStart > Wiggle ? SubjectStart-Wiggle : 0);
    SubjectStop = ( SubjectStop + Wiggle < SubjectVec.size() ? SubjectStop+Wiggle : SubjectVec.size()-1);
*/

    // CSeqVector, when making a minus vector, not only compliments the bases,
    // flips the order to. But the start and stops provided are in Plus space.
    // So we need to swap them to Minus space
    TSeqPos QueryStrandedStart, QueryStrandedStop;
    if(Strand == eNa_strand_plus) {
        QueryStrandedStart = QueryStart;
        QueryStrandedStop = QueryStop;
    } else {
        QueryStrandedStart = ( (QueryVec.size()-1) - QueryStop);
        QueryStrandedStop = ( (QueryVec.size()-1) - QueryStart);
    }

    string QuerySeq, SubjectSeq;
    QueryVec.GetSeqData(QueryStrandedStart, QueryStrandedStop+1, QuerySeq);
    SubjectVec.GetSeqData(SubjectStart, SubjectStop+1, SubjectSeq);

/*  ERR_POST(Info << " Query " << QueryStart << " to " << QueryStop << " against");
    ERR_POST(Info << " QStranded " << QueryStrandedStart << " to " << QueryStrandedStop << " against");
    ERR_POST(Info << " Subject " << SubjectStart << " to " << SubjectStop);
*/

    SCallbackData CallbackData;
    CallbackData.StartTime = CTime(CTime::eCurrent);
    CallbackData.TimeOutSeconds = m_TimeOutSeconds;
    CallbackData.PreviousCount = 0;
    CallbackData.TimedOut = false;

    TSeqPos ExtractQueryStart = ((Strand == eNa_strand_plus) ? 0 : QuerySeq.size()-1);
    CRef<CDense_seg> ResultDenseg;

    CMMAligner Aligner(QuerySeq, SubjectSeq);
    Aligner.SetWm(2);
    Aligner.SetWms(-3);
    Aligner.SetWg(-100);
    Aligner.SetWs(-1);
    Aligner.SetScoreMatrix(NULL);
    Aligner.SetProgressCallback(s_ProgressCallback, (void*)&CallbackData);
    try {
        int Score;
        Score = Aligner.Run();
        if(Score == numeric_limits<int>::min()) {
            return CRef<CDense_seg>();
        }
        ResultDenseg = Aligner.GetDense_seg(ExtractQueryStart, Strand, QueryId,
                                            0, eNa_strand_plus, SubjectId);
    } catch(CException& e) {
        if(!CallbackData.TimedOut) {
            ERR_POST(Error << "MMAligner: " << e.ReportAll());
        }
        return CRef<CDense_seg>();
    }


    if(ResultDenseg->GetNumseg() > 0) {
        ResultDenseg->OffsetRow(0, QueryStart);
        ResultDenseg->OffsetRow(1, SubjectStart);

        // TrimEndGaps Segfaults on the only one segment which is a gap case
        if(ResultDenseg->GetNumseg() == 1 &&
           (ResultDenseg->GetStarts()[0] == -1 || ResultDenseg->GetStarts()[1] == -1)) {
            return CRef<CDense_seg>();
        } else {
            ResultDenseg->TrimEndGaps();
        }

    } else {
        return CRef<CDense_seg>();
    }

    return ResultDenseg;
}


CRef<objects::CSeq_align_set>
CInstancedAligner::x_RunCleanup(const objects::CSeq_align_set& AlignSet,
                                objects::CScope& Scope)
{
    CAlignCleanup Cleaner(Scope);
    //Cleaner.FillUnaligned(true);

    list<CConstRef<CSeq_align> > In;
    ITERATE(CSeq_align_set::Tdata, AlignIter, AlignSet.Get()) {
        CConstRef<CSeq_align> Align(*AlignIter);
        In.push_back(Align);
    }

    CRef<CSeq_align_set> Out(new CSeq_align_set);

    try {
        Cleaner.Cleanup(In, Out->Set());
    } catch(CException& e) {
        ERR_POST(Info << "Cleanup Error: " << e.ReportAll());
        return CRef<CSeq_align_set>();
    }

    NON_CONST_ITERATE(CSeq_align_set::Tdata, AlignIter, Out->Set()) {
        CDense_seg& Denseg = (*AlignIter)->SetSegs().SetDenseg();

        if(!Denseg.CanGetStrands() || Denseg.GetStrands().empty()) {
            Denseg.SetStrands().resize(Denseg.GetDim()*Denseg.GetNumseg(), eNa_strand_plus);
        }

        if(Denseg.GetSeqStrand(1) != eNa_strand_plus) {
            Denseg.Reverse();
        }
        //CRef<CDense_seg> Filled = Denseg.FillUnaligned();
        //Denseg.Assign(*Filled);
    }

    return Out;
}


void CInstancedAligner::x_GetCleanupInstances(CQuerySet& QueryAligns, CScope& Scope,
                                       vector<CRef<CInstance> >& Instances)
{
    NON_CONST_ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter,
                      QueryAligns.Get()) {

        CRef<CSeq_align_set> Set = SubjectIter->second;
        CRef<CSeq_align_set> Out;

        Out = x_RunCleanup(*Set, Scope);

        ITERATE(CSeq_align_set::Tdata, AlignIter, Out->Set()) {
            CRef<CInstance> Inst(new CInstance(*AlignIter));
            Instances.push_back(Inst);
        }
    }

}



CInstance::CInstance(CRef<CSeq_align> Align)
{
    Query.SetId().Assign(Align->GetSeq_id(0));
    Subject.SetId().Assign(Align->GetSeq_id(1));

    Query.SetStrand() = Align->GetSeqStrand(0);
    Subject.SetStrand() = Align->GetSeqStrand(1);

    Query.SetFrom() = Align->GetSeqStart(0);
    Subject.SetFrom() = Align->GetSeqStart(1);

    Query.SetTo() = Align->GetSeqStop(0);
    Subject.SetTo() = Align->GetSeqStop(1);

    Alignments.Set().push_back(Align);
}

CInstance::CInstance(const CSeq_align_set& AlignSet)
{
    Query.SetId().Assign(AlignSet.Get().front()->GetSeq_id(0));
    Subject.SetId().Assign(AlignSet.Get().front()->GetSeq_id(1));

    Query.SetStrand() = AlignSet.Get().front()->GetSeqStrand(0);
    Subject.SetStrand() = AlignSet.Get().front()->GetSeqStrand(1);

    Query.SetFrom() = numeric_limits<TSeqPos>::max();
    Subject.SetFrom() = numeric_limits<TSeqPos>::max();

    Query.SetTo() = numeric_limits<TSeqPos>::min();
    Subject.SetTo() = numeric_limits<TSeqPos>::min();

    ITERATE(CSeq_align_set::Tdata, AlignIter, AlignSet.Get()) {
        Query.SetFrom(min(Query.GetFrom(), (*AlignIter)->GetSeqStart(0)));
        Subject.SetFrom(min(Subject.GetFrom(), (*AlignIter)->GetSeqStart(1)));

        Query.SetTo(max(Query.GetTo(), (*AlignIter)->GetSeqStop(0)));
        Subject.SetTo(max(Subject.GetTo(), (*AlignIter)->GetSeqStop(1)));
    }
}


bool CInstance::IsAlignmentContained(const CSeq_align& Align) const
{
    if(Query.GetStrand() != Align.GetSeqStrand(0) ||
       Subject.GetStrand() != Align.GetSeqStrand(1))
        return false;

    if( Align.GetSeqStart(0) >= Query.GetFrom() &&
        Align.GetSeqStop(0)  <= Query.GetTo() &&
        Align.GetSeqStart(1) >= Subject.GetFrom() &&
        Align.GetSeqStop(1)  <= Subject.GetTo()) {
        return true;
    }
    else {
        return false;
    }
}


int CInstance::GapDistance(const CSeq_align& Align) const
{
    int LongestGap = 0;
    LongestGap = max((int)Align.GetSeqStart(0)-(int)Query.GetTo(), LongestGap);
    LongestGap = max((int)Align.GetSeqStart(1)-(int)Subject.GetTo(), LongestGap);
    LongestGap = max((int)Query.GetFrom()-(int)Align.GetSeqStop(0), LongestGap);
    LongestGap = max((int)Subject.GetFrom()-(int)Align.GetSeqStop(1), LongestGap);
   // cerr << "Longest Gap: " << LongestGap << endl;
    return LongestGap;
}


void CInstance::MergeIn(const CRef<CSeq_align> Align)
{
    Query.SetFrom(min(Query.GetFrom(), Align->GetSeqStart(0)));
    Subject.SetFrom(min(Subject.GetFrom(), Align->GetSeqStart(1)));

    Query.SetTo(max(Query.GetTo(), Align->GetSeqStop(0)));
    Subject.SetTo(max(Subject.GetTo(), Align->GetSeqStop(1)));

    Alignments.Set().push_back(Align);
}

void CInstancedAligner::x_GetDistanceInstances(CQuerySet& QueryAligns, CScope& Scope,
                                       vector<CRef<CInstance> >& Instances)
{
    typedef pair<CRef<CInstance>, CRef<CSeq_align_set> > TInstPair;

    // Identify the best pct_coverage for every Subject Id.
    // This will be used to filter out subjects with relatively low
    // best pct_coverage, so that they can be skipped.
    typedef map<string, double> TSubjectCoverage;
    TSubjectCoverage BestCoverage;
    double MaxCoverage = 0;
    ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter, QueryAligns.Get()) {
        CRef<CSeq_align_set> Set = SubjectIter->second;
        string IdStr = Set->Get().front()->GetSeq_id(1).AsFastaString();
        double SubjCoverage = 0;
        ITERATE(CSeq_align_set::Tdata, AlignIter, Set->Get()) {
            double PctCov;
            (*AlignIter)->GetNamedScore("pct_coverage", PctCov);
            SubjCoverage = max(SubjCoverage, PctCov);
        }
        BestCoverage[IdStr] = SubjCoverage;
        MaxCoverage = max(SubjCoverage, MaxCoverage);
    }

    typedef vector<CRef<CInstance> > TInstVector;
    ITERATE(CQuerySet::TSubjectToAlignSet, SubjectIter, QueryAligns.Get()) {

        TInstVector SubjInstances;

        CRef<CSeq_align_set> Set = SubjectIter->second;
        string SubjIdStr = Set->Get().front()->GetSeq_id(1).AsFastaString();
        if(BestCoverage[SubjIdStr] < (MaxCoverage*0.10)) {
            continue; // Skip any subject id that has less than 10% the best pct_coverage
        }

        // Group together alignments that are not contained within each other,
        // but are within a 'small' gap distance of each other into initial
        // instances
        ITERATE(CSeq_align_set::Tdata, AlignIter, Set->Get()) {
            bool Inserted = false;
            bool Contained = false;
            ITERATE(TInstVector, InstIter, SubjInstances) {
                bool CurrContained = (*InstIter)->IsAlignmentContained(**AlignIter);
                Contained |= CurrContained;
            }
            if(Contained)
                continue;
            NON_CONST_ITERATE(TInstVector, InstIter, SubjInstances) {
                int GapDist = (*InstIter)->GapDistance(**AlignIter);
                if(GapDist < 20000) {
                    (*InstIter)->MergeIn(*AlignIter);
                    Inserted = true;
                    break;
                }
            }
            if(!Inserted) {
                CRef<CInstance> Inst(new CInstance(*AlignIter));
                SubjInstances.push_back(Inst);
            }
        }

        // For these initial instances, run CAlignCleanup on the instance
        //  CSeq_align_sets. Then make instances from CAlignCleanup results.
        TInstVector CleanedInstances;

        ITERATE(TInstVector, InstIter, SubjInstances) {
            // If the instance only has one alignment in it, skip it.
            // we already have an identical BLAST result.
            if((*InstIter)->Alignments.Get().size() <= 1)
                continue;

            CRef<CSeq_align_set> Cleaned;
            Cleaned = x_RunCleanup((*InstIter)->Alignments, Scope);
            if(!Cleaned.IsNull()) {
                ITERATE(CSeq_align_set::Tdata, AlignIter, Cleaned->Get()) {
                    // If any of these cleaned alignments are dupes of existing alignments,
                    // skip them
                    bool DupeFound = false;
                    ITERATE(CSeq_align_set::Tdata, SourceIter, (*InstIter)->Alignments.Get()) {
                        if( (*AlignIter)->GetSeqStart(0) == (*SourceIter)->GetSeqStart(0) &&
                            (*AlignIter)->GetSeqStart(1) == (*SourceIter)->GetSeqStart(1) &&
                            (*AlignIter)->GetSeqStop(0) == (*SourceIter)->GetSeqStop(0) &&
                            (*AlignIter)->GetSeqStop(1) == (*SourceIter)->GetSeqStop(1)) {
                            DupeFound = true;
                            break;
                        }
                    }
                    if(DupeFound)
                        continue;

                    // And if any of these cleaned alignments are contained in
                    // established instances, throw them out too.
                    bool Contained = false;
                    ITERATE(TInstVector, CleanIter, CleanedInstances) {
                        bool Curr = (*CleanIter)->IsAlignmentContained(**AlignIter);
                        Contained |= Curr;
                    }
                    if(Contained)
                        continue;

                    // Any Cleaned Seq_align that got this far is a potential instance.
                    bool Dupe = false;
                    CRef<CInstance> Inst(new CInstance(*AlignIter));
                    ITERATE(vector<CRef<CInstance> >, OldInstIter, CleanedInstances) {
                        Dupe |= ((*OldInstIter)->Query.Equals(Inst->Query)
                              && (*OldInstIter)->Subject.Equals(Inst->Subject));
                    }
                    if(!Dupe)
                        CleanedInstances.push_back(Inst);
                }
            }
        }

        copy(CleanedInstances.begin(), CleanedInstances.end(),
            insert_iterator<TInstVector>(Instances, Instances.end()));
    }

}





END_SCOPE(ncbi)

