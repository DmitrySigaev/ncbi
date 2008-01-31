/*
*  $Id$
*
* =========================================================================
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
*  Government do not and cannt warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* =========================================================================
*
*  Author: Vyacheslav Chetvernin
*
* =========================================================================
*/

#include <ncbi_pch.hpp>
#include <algo/align/prosplign/prosplign.hpp>
#include <algo/align/prosplign/prosplign_exception.hpp>

#include "scoring.hpp"
#include "PSeq.hpp"
#include "NSeq.hpp"
#include "nucprot.hpp"
#include "Ali.hpp"
#include "AliSeqAlign.hpp"
#include "Info.hpp"

#include <objects/seqloc/seqloc__.hpp>
#include <objmgr/util/seq_loc_util.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(ncbi::objects);
USING_SCOPE(ncbi::prosplign);


///////////////////////////////////////////////////////////////////////////
CProSplignScoring::CProSplignScoring()
{
    SetMinIntronLen(default_min_intron_len);
    SetGapOpeningCost(default_gap_opening);
    SetGapExtensionCost(default_gap_extension);
    SetFrameshiftOpeningCost(default_frameshift_opening);
    SetGTIntronCost(default_intron_GT);
    SetGCIntronCost(default_intron_GC);
    SetATIntronCost(default_intron_AT);
    SetNonConsensusIntronCost(default_intron_non_consensus);
    SetInvertedIntronExtensionCost(default_inverted_intron_extension);
}

CProSplignScoring& CProSplignScoring::SetMinIntronLen(int val)
{
    min_intron_len = val;
    return *this;
}
int CProSplignScoring::GetMinIntronLen() const
{
    return min_intron_len;
}
CProSplignScoring& CProSplignScoring::SetGapOpeningCost(int val)
{
    gap_opening = val;
    return *this;
}
int CProSplignScoring::GetGapOpeningCost() const
{
    return gap_opening;
}

CProSplignScoring& CProSplignScoring::SetGapExtensionCost(int val)
{
    gap_extension = val;
    return *this;
}
int CProSplignScoring::GetGapExtensionCost() const
{
    return gap_extension;
}

CProSplignScoring& CProSplignScoring::SetFrameshiftOpeningCost(int val)
{
    frameshift_opening = val;
    return *this;
}
int CProSplignScoring::GetFrameshiftOpeningCost() const
{
    return frameshift_opening;
}

CProSplignScoring& CProSplignScoring::SetGTIntronCost(int val)
{
    intron_GT = val;
    return *this;
}
int CProSplignScoring::GetGTIntronCost() const
{
    return intron_GT;
}
CProSplignScoring& CProSplignScoring::SetGCIntronCost(int val)
{
    intron_GC = val;
    return *this;
}
int CProSplignScoring::GetGCIntronCost() const
{
    return intron_GC;
}
CProSplignScoring& CProSplignScoring::SetATIntronCost(int val)
{
    intron_AT = val;
    return *this;
}
int CProSplignScoring::GetATIntronCost() const
{
    return intron_AT;
}

CProSplignScoring& CProSplignScoring::SetNonConsensusIntronCost(int val)
{
    intron_non_consensus = val;
    return *this;
}
int CProSplignScoring::GetNonConsensusIntronCost() const
{
    return intron_non_consensus;
}

CProSplignScoring& CProSplignScoring::SetInvertedIntronExtensionCost(int val)
{
    inverted_intron_extension = val;
    return *this;
}
int CProSplignScoring::GetInvertedIntronExtensionCost() const
{
    return inverted_intron_extension;
}
///////////////////////////////////////////////////////////////////////////
CProSplignOutputOptions::CProSplignOutputOptions(EMode mode)
{
    switch (mode) {
    case eWithHoles:
        SetEatGaps(default_eat_gaps);

        SetFlankPositives(default_flank_positives);
        SetTotalPositives(default_total_positives);
        
        SetMaxBadLen(default_max_bad_len);
        SetMinPositives(default_min_positives);
        
        SetMinFlankingExonLen(default_min_flanking_exon_len);
        SetMinGoodLen(default_min_good_len);
        
        SetStartBonus(default_start_bonus);
        SetStopBonus(default_stop_bonus);

        break;
    case ePassThrough:
        SetEatGaps(false);

        SetFlankPositives(0);
        SetTotalPositives(0);
        
        SetMaxBadLen(0);
        SetMinPositives(0);
        
        SetMinFlankingExonLen(0);
        SetMinGoodLen(0);
        
        SetStartBonus(0);
        SetStopBonus(0);
    }
}

bool CProSplignOutputOptions::IsPassThrough() const
{
    return GetTotalPositives() == 0 && GetFlankPositives() == 0;
}

CProSplignOutputOptions& CProSplignOutputOptions::SetEatGaps(bool val)
{
    eat_gaps = val;
    return *this;
}
bool CProSplignOutputOptions::GetEatGaps() const
{
    return eat_gaps;
}

CProSplignOutputOptions& CProSplignOutputOptions::SetFlankPositives(int val)
{
    flank_positives = val;
    return *this;
}
int CProSplignOutputOptions::GetFlankPositives() const
{
    return flank_positives;
}

CProSplignOutputOptions& CProSplignOutputOptions::SetTotalPositives(int val)
{
    total_positives = val;
    return *this;
}
int CProSplignOutputOptions::GetTotalPositives() const
{
    return total_positives;
}

CProSplignOutputOptions& CProSplignOutputOptions::SetMaxBadLen(int val)
{
    max_bad_len = val;
    return *this;
}
int CProSplignOutputOptions::GetMaxBadLen() const
{
    return max_bad_len;
}

CProSplignOutputOptions& CProSplignOutputOptions::SetMinPositives(int val)
{
    min_positives = val;
    return *this;
}

int CProSplignOutputOptions::GetMinPositives() const
{
    return min_positives;
}

CProSplignOutputOptions& CProSplignOutputOptions::SetMinFlankingExonLen(int val)
{
    min_flanking_exon_len = val;
    return *this;
}

int CProSplignOutputOptions::GetMinFlankingExonLen() const
{
    return min_flanking_exon_len;
}
CProSplignOutputOptions& CProSplignOutputOptions::SetMinGoodLen(int val)
{
    min_good_len = val;
    return *this;
}

int CProSplignOutputOptions::GetMinGoodLen() const
{
    return min_good_len;
}

CProSplignOutputOptions& CProSplignOutputOptions::SetStartBonus(int val)
{
    start_bonus = val;
    return *this;
}

int CProSplignOutputOptions::GetStartBonus() const
{
    return start_bonus;
}
CProSplignOutputOptions& CProSplignOutputOptions::SetStopBonus(int val)
{
    stop_bonus = val;
    return *this;
}

int CProSplignOutputOptions::GetStopBonus() const
{
    return stop_bonus;
}


////////////////////////////////////////////////////////////////////////////////



class CProSplign::CImplementation {
public:
    static CImplementation* create(CProSplignScoring scoring, bool intronless, bool one_stage, bool just_second_stage, bool old);
    CImplementation(CProSplignScoring scoring) :
        m_scoring(scoring), m_matrix("BLOSUM62",m_scoring.sm_koef)
    {
    }
    virtual ~CImplementation() {}
    virtual CImplementation* clone()=0;

    // returns score, bigger is better.
    // if genomic strand is unknown call twice with opposite strands and compare scores
    int FindGlobalAlignment_stage1(CScope& scope, const CSeq_id& protein, const CSeq_loc& genomic);
    CRef<CSeq_align> FindGlobalAlignment_stage2();
    CRef<CSeq_align> FindGlobalAlignment(CScope& scope, const CSeq_id& protein, const CSeq_loc& genomic_orig)
    {
        CSeq_loc genomic;
        genomic.Assign(genomic_orig);
        FindGlobalAlignment_stage1(scope, protein, genomic);
        return FindGlobalAlignment_stage2();
    }

    virtual const vector<pair<int, int> >& GetExons() const
    {
        NCBI_THROW(CProSplignException, eGenericError, "method relevant only for two stage prosplign");
    }
    virtual vector<pair<int, int> >& SetExons()
    {
        NCBI_THROW(CProSplignException, eGenericError, "method relevant only for two stage prosplign");
    }
    virtual void GetFlanks(bool& lgap, bool& rgap) const
    {
        NCBI_THROW(CProSplignException, eGenericError, "method relevant only for two stage prosplign");
    }
    virtual void SetFlanks(bool lgap, bool rgap)
    {
        NCBI_THROW(CProSplignException, eGenericError, "method relevant only for two stage prosplign");
    }

private:
    virtual int stage1() = 0;
    virtual void stage2(CAli& ali) = 0;

protected:
    CProSplignScaledScoring m_scoring;
    CSubstMatrix m_matrix;

    CScope* m_scope;
    const CSeq_id* m_protein;
    CRef<CSeq_loc> m_genomic;
    auto_ptr<CPSeq> m_protseq;
    auto_ptr<CNSeq> m_cnseq;
};

class COneStage : public CProSplign::CImplementation {
public:
    COneStage(CProSplignScoring scoring) : CProSplign::CImplementation(scoring) {}
    virtual COneStage* clone() { return new COneStage(*this); }

private:
    virtual int stage1();
    virtual void stage2(CAli& ali);

    CTBackAlignInfo<CBMode> m_bi;
};

int COneStage::stage1()
{
    m_bi.Init((int)m_protseq->seq.size(), (int)m_cnseq->size());//backtracking
    return AlignFNog(m_bi, m_protseq->seq, *m_cnseq, m_scoring, m_matrix);
}

void COneStage::stage2(CAli& ali)
{
    BackAlignNog(m_bi, ali);
}

class CTwoStage : public CProSplign::CImplementation {
public:
    CTwoStage(CProSplignScoring scoring, bool just_second_stage) :
        CProSplign::CImplementation(scoring),
        m_just_second_stage(just_second_stage), m_lgap(false), m_rgap(false)  {}

    virtual const vector<pair<int, int> >& GetExons() const
    {
        return m_igi;
    }
    virtual vector<pair<int, int> >& SetExons()
    {
        return m_igi;
    }
    virtual void GetFlanks(bool& lgap, bool& rgap) const
    {
        lgap = m_lgap;
        rgap = m_rgap;
    }
    virtual void SetFlanks(bool lgap, bool rgap)
    {
        m_lgap = lgap;
        m_rgap = rgap;
    }
protected:
    bool m_just_second_stage;
    vector<pair<int, int> > m_igi;
    bool m_lgap;//true if the first one in igi is a gap
    bool m_rgap;//true if the last one in igi is a gap
};

class CTwoStageOld : public CTwoStage {
public:
    CTwoStageOld(CProSplignScoring scoring, bool just_second_stage) : CTwoStage(scoring,just_second_stage) {}
    virtual CTwoStageOld* clone() { return new CTwoStageOld(*this); }
private:
    virtual int stage1();
    virtual void stage2(CAli& ali);
};

class CTwoStageNew : public CTwoStage {
public:
    CTwoStageNew(CProSplignScoring scoring, bool just_second_stage) : CTwoStage(scoring,just_second_stage) {}
    virtual CTwoStageNew* clone() { return new CTwoStageNew(*this); }
private:
    virtual int stage1();
    virtual void stage2(CAli& ali);
};

int CTwoStageOld::stage1()
{
    if (m_just_second_stage)
        return 0;
    int score = FindIGapIntrons(m_igi, m_protseq->seq, *m_cnseq,
                                m_scoring.GetGapOpeningCost(),
                                m_scoring.GetGapExtensionCost(),
                                m_scoring.GetFrameshiftOpeningCost(), m_scoring, m_matrix);
    m_lgap = !m_igi.empty() && m_igi.front().first == 0;
    m_rgap = !m_igi.empty() && m_igi.back().first + m_igi.back().second == int(m_cnseq->size());
    return score;
}

void CTwoStageOld::stage2(CAli& ali)
{
    CNSeq cfrnseq;
    cfrnseq.Init(*m_cnseq, m_igi);
            
    CBackAlignInfo bi;
    bi.Init((int)m_protseq->seq.size(), (int)cfrnseq.size()); //backtracking
        
    FrAlign(bi, m_protseq->seq, cfrnseq,
            m_scoring.GetGapOpeningCost(),
            m_scoring.GetGapExtensionCost(),
            m_scoring.GetFrameshiftOpeningCost(), m_scoring, m_matrix);

    FrBackAlign(bi, ali);
    CAli new_ali(m_igi, m_lgap, m_rgap, ali);
    ali = new_ali;
}

int CTwoStageNew::stage1()
{
    if (m_just_second_stage)
        return 0;
    return FindFGapIntronNog(m_igi, m_protseq->seq, *m_cnseq, m_lgap, m_rgap, m_scoring, m_matrix);
}

void CTwoStageNew::stage2(CAli& ali)
{
    CNSeq cfrnseq;
    cfrnseq.Init(*m_cnseq, m_igi);
            
    CBackAlignInfo bi;
    bi.Init((int)m_protseq->seq.size(), (int)cfrnseq.size()); //backtracking
        
    FrAlignFNog1(bi, m_protseq->seq, cfrnseq, m_scoring, m_matrix, m_lgap, m_rgap);

    FrBackAlign(bi, ali);
    CAli new_ali(m_igi, m_lgap, m_rgap, ali);
    ali = new_ali;
}

class CIntronless : public CProSplign::CImplementation {
public:
    CIntronless(CProSplignScoring scoring) : CProSplign::CImplementation(scoring) {}
private:
    virtual void stage2(CAli& ali);
protected:
    CBackAlignInfo m_bi;
};

class CIntronlessOld : public CIntronless {
public:
    CIntronlessOld(CProSplignScoring scoring) : CIntronless(scoring) {}
    virtual CIntronlessOld* clone() { return new CIntronlessOld(*this); }
private:
    virtual int stage1();
};

class CIntronlessNew : public CIntronless {
public:
    CIntronlessNew(CProSplignScoring scoring) : CIntronless(scoring) {}
    virtual CIntronlessNew* clone() { return new CIntronlessNew(*this); }
private:
    virtual int stage1();
};

int CIntronlessOld::stage1()
{
    m_bi.Init((int)m_protseq->seq.size(), (int)m_cnseq->size());//backtracking
    return FrAlign(m_bi, m_protseq->seq, *m_cnseq,
                   m_scoring.GetGapOpeningCost(),
                   m_scoring.GetGapExtensionCost(),
                   m_scoring.GetFrameshiftOpeningCost(), m_scoring, m_matrix); 
}

int CIntronlessNew::stage1()
{ 
    m_bi.Init((int)m_protseq->seq.size(), (int)m_cnseq->size());//backtracking
    return FrAlignFNog1(m_bi, m_protseq->seq, *m_cnseq, m_scoring, m_matrix);
}

void CIntronless::stage2(CAli& ali)
{
    FrBackAlign(m_bi, ali);
}

CProSplign::CImplementation* CProSplign::CImplementation::create(CProSplignScoring scoring, bool intronless, bool one_stage, bool just_second_stage, bool old)
{
    if (intronless) {
        if (old)
            return new CIntronlessOld(scoring);
        else
            return new CIntronlessNew(scoring);
    } else {
        if (one_stage) {
            return new COneStage(scoring);
        } else {
            if (old)
                return new CTwoStageOld(scoring, just_second_stage);
            else
                return new CTwoStageNew(scoring, just_second_stage);
        }
    }
}


const vector<pair<int, int> >& CProSplign::GetExons() const
{
    return m_implementation->GetExons();
}

vector<pair<int, int> >& CProSplign::SetExons()
{
    return m_implementation->SetExons();
}

void CProSplign::GetFlanks(bool& lgap, bool& rgap) const
{
    m_implementation->GetFlanks(lgap, rgap);
}

void CProSplign::SetFlanks(bool lgap, bool rgap)
{
    m_implementation->SetFlanks(lgap, rgap);
}


CProSplign::CProSplign( CProSplignScoring scoring, bool intronless) :
    m_implementation(CImplementation::create(scoring,intronless,false,false,false))
{
}

CProSplign::CProSplign( CProSplignScoring scoring, bool intronless, bool one_stage, bool just_second_stage, bool old) :
    m_implementation(CImplementation::create(scoring,intronless,one_stage,just_second_stage,old))
{
}

CProSplign::~CProSplign()
{
}

namespace {
/// true if first and last aa are aligned, nothing about inside holes
bool IsProteinSpanWhole(const CSpliced_seg& sps)
{
    CSpliced_seg::TExons exons = sps.GetExons();
    if (exons.empty())
        return false;
    const CProt_pos& prot_start_pos = exons.front()->GetProduct_start().GetProtpos();
    const CProt_pos& prot_stop_pos = exons.back()->GetProduct_end().GetProtpos();
       
    return prot_start_pos.GetAmin()==0 && prot_start_pos.GetFrame()==1 &&
        prot_stop_pos.GetAmin()+1 == sps.GetProduct_length() && prot_stop_pos.GetFrame() == 3;
}
}

CRef<CSeq_align> CProSplign::FindGlobalAlignment(CScope& scope, const CSeq_id& protein, const CSeq_loc& genomic_orig)
{
    CSeq_loc genomic;
    genomic.Assign(genomic_orig);
    const CSeq_id& nucid = *genomic.GetId();

    if (genomic.IsWhole()) {
        // change to Interval, because Whole doesn't allow strand change - it's always unknown.
        genomic.SetInt().SetFrom(0);
        genomic.SetInt().SetTo(sequence::GetLength(nucid, &scope)-1);
    }

    CRef<CSeq_align> result;

    switch (genomic.GetStrand()) {
    case eNa_strand_plus:
    case eNa_strand_minus:
        result = m_implementation->FindGlobalAlignment(scope, protein, genomic);
        break;
    case eNa_strand_unknown:
    case eNa_strand_both:
    case eNa_strand_both_rev:
        // do both
        {
            auto_ptr<CImplementation> plus_data(m_implementation->clone());
            genomic.SetStrand(eNa_strand_plus);
            int plus_score = plus_data->FindGlobalAlignment_stage1(scope, protein, genomic);

            genomic.SetStrand(eNa_strand_minus);
            int minus_score = m_implementation->FindGlobalAlignment_stage1(scope, protein, genomic);
            
            if (minus_score <= plus_score)
                m_implementation = plus_data;
        }

        result = m_implementation->FindGlobalAlignment_stage2();
        break;
    default:
        genomic.SetStrand(eNa_strand_plus);
        result = m_implementation->FindGlobalAlignment(scope, protein, genomic);
        break;
    }

    //remove genomic bounds if set
    if (result->CanGetBounds()) {
        NON_CONST_ITERATE(CSeq_align::TBounds, b, result->SetBounds()) {
            if ((*b)->GetId() != NULL && (*b)->GetId()->Match(nucid)) {
                result->SetBounds().erase(b);
                break;
            }
        }
    }
    //add genomic_orig as genomic bounds
    CRef<CSeq_loc> genomic_bounds(new CSeq_loc);
    genomic_bounds->Assign(genomic_orig);
    result->SetBounds().push_back(genomic_bounds);

    return result;
}

int CProSplign::CImplementation::FindGlobalAlignment_stage1(CScope& scope, const CSeq_id& protein, const CSeq_loc& genomic)
{
    m_scope = &scope;
    m_protein = &protein;
    m_genomic.Reset(new CSeq_loc);
    m_genomic->Assign(genomic);
    m_protseq.reset(new CPSeq(*m_scope, *m_protein));
    m_cnseq.reset(new CNSeq(*m_scope, *m_genomic));

    return stage1();
}

CRef<CSeq_align> CProSplign::CImplementation::FindGlobalAlignment_stage2()
{
    CAli ali;
    stage2(ali);

    CAliToSeq_align cpa(ali, *m_scope, *m_protein, *m_genomic);
    CRef<CSeq_align> seq_align = cpa.MakeSeq_align(*m_protseq, *m_cnseq);

    prosplign::SeekStartStop(*seq_align, *m_scope);

    if (!IsProteinSpanWhole(seq_align->GetSegs().GetSpliced()))
        seq_align->SetType(CSeq_align::eType_disc);

    return seq_align;
}

CRef<objects::CSeq_align> CProSplign::RefineAlignment(CScope& scope, const CSeq_align& seq_align, CProSplignOutputOptions output_options)
{
    CProSplignText alignment_text(scope, seq_align, "BLOSUM62");
    list<CNPiece> good_parts = FindGoodParts( alignment_text.GetMatch(), alignment_text.GetProtein(), output_options);

    CRef<CSeq_align> refined_align(new CSeq_align);
    refined_align->Assign(seq_align);

    prosplign::RefineAlignment(scope, *refined_align, good_parts);

    if (good_parts.size()!=1 || !IsProteinSpanWhole(refined_align->GetSegs().GetSpliced())) {
        refined_align->SetType(CSeq_align::eType_disc);
    }

    prosplign::SeekStartStop(*refined_align, scope);

    return refined_align;
}

END_NCBI_SCOPE
