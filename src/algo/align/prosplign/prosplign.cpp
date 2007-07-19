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

#include "scoring.hpp"
#include "PSeq.hpp"
#include "NSeq.hpp"
#include "nucprot.hpp"
#include "Ali.hpp"
#include "AliSeqAlign.hpp"
#include "AliUtil.hpp"
#include "Info.hpp"

#include <objects/seqloc/seqloc__.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(ncbi::objects);


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

struct CProSplign::SImplData {
    SImplData(CProSplignScoring scoring) :
        m_scoring(scoring), m_intronless(false)
    {
        CScoring::Init(m_scoring);
    }

    CProSplignScaledScoring m_scoring;
    bool m_intronless;
};

CProSplign::CProSplign( CProSplignScoring scoring) :
    m_data(new SImplData(scoring))
{
}

CProSplign::~CProSplign()
{
}

void CProSplign::SetIntronlessMode(bool intronless)
{
    m_data->m_intronless = intronless;
}
bool CProSplign::GetIntronlessMode() const
{
    return m_data->m_intronless;
}

CRef<CSeq_align> CProSplign::FindAlignment(CScope& scope, const CSeq_id& protein, const CSeq_loc& genomic, 
                                   CProSplignOutputOptions output_options)
{
    CPSeq protseq(scope, protein);
    PSEQ& pseq = protseq.seq;

    CNSeq cnseq(scope, genomic);

    auto_ptr<CAli> pali;

    if (!m_data->m_intronless) {
        vector<pair<int, int> > igi;
        bool lgap = false;//true if the first one in igi is a gap
        bool rgap = false;//true if the last one in igi is a gap
        
        int iscore1 =
            FindFGapIntronNog(igi, pseq, cnseq, lgap, rgap);
        
        CNSeq cfrnseq;
        cfrnseq.Init(cnseq, igi);
        
        CBackAlignInfo bi;
        bi.Init((int)pseq.size(), (int)cfrnseq.size()); //backtracking
        
        int friscore =
            FrAlignFNog1(bi, pseq, cfrnseq, m_data->m_scoring, lgap, rgap);
        
        pali.reset( new CAli(cfrnseq, protseq) );
        FrBackAlign(bi, *pali);

        _ASSERT(CAliUtil::CountFrIScore(*pali, 2 /* new */, lgap, rgap) == friscore);

        pali.reset( new CAli(cnseq, protseq, igi, lgap, rgap, *pali) );

        _ASSERT(CAliUtil::CountIScore(*pali, 2 /* new */) == iscore1);
        _DEBUG_CODE(CAliUtil::CheckValidity(*pali););
    } else { // flag "no_introns" is set
        pali.reset(new CAli(cnseq, protseq));
        CBackAlignInfo bi;
        bi.Init((int)pseq.size(), (int)cnseq.size());//backtracking
        int friscore = FrAlignFNog1(bi, pseq, cnseq, m_data->m_scoring);
        pali->score = friscore/m_data->m_scoring.GetScale();
        FrBackAlign(bi, *pali);
        _ASSERT(CAliUtil::CountFrIScore(*pali, 2) == friscore);
    }

    auto_ptr<CPosAli> pposali( new CPosAli(*pali, protein, genomic, output_options) );
    pali.reset();

    CAliToSeq_align cpa(scope, *pposali);
    return cpa.MakeSeq_align();
}

END_NCBI_SCOPE
