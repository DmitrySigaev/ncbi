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
 * Authors:
 *
 * File Description:  Write gff file
 *
 */

#include <ncbi_pch.hpp>

#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objmgr/util/sequence.hpp> 
#include <objtools/alnmgr/alnmap.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Score.hpp>

#include <objtools/writers/gff3flybase_record.hpp>
#include <objtools/writers/gff3flybase_writer.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

#define INSERTION(sf, tf) ( ((sf) &  CAlnMap::fSeq) && !((tf) &  CAlnMap::fSeq) )
#define DELETION(sf, tf) ( !((sf) &  CAlnMap::fSeq) && ((tf) &  CAlnMap::fSeq) )
#define MATCH(sf, tf) ( ((sf) &  CAlnMap::fSeq) && ((tf) &  CAlnMap::fSeq) )

//  ----------------------------------------------------------------------------
static CConstRef<CSeq_id> s_GetSourceId(
    const CSeq_id& id, CScope& scope )
//  ----------------------------------------------------------------------------
{
    try {
        return sequence::GetId(id, scope, sequence::eGetId_Best).GetSeqId();
    }
    catch (CException&) {
    }
    return CConstRef<CSeq_id>(&id);
}


//  ----------------------------------------------------------------------------
bool CGff3FlybaseWriter::WriteHeader()
//  ----------------------------------------------------------------------------
{
    if (!m_bHeaderWritten) {
        m_Os << "##gff-version 3" << '\n';
        m_Os << "#!gff-spec-version 1.20" << '\n';
        m_Os << "##!gff-variant flybase" << '\n';
        m_Os << "# This variant of GFF3 interprets ambiguities in the" << '\n';
        m_Os << "# GFF3 specifications in accordance with the views of Flybase." << '\n';
        m_Os << "# This impacts the feature tag set, and meaning of the phase." << '\n';
        m_Os << "#!processor NCBI annotwriter" << '\n';
        m_bHeaderWritten = true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3FlybaseWriter::xWriteAlignDisc(
    const CSeq_align& align,
    const string& aid)
//  ----------------------------------------------------------------------------
{
    if (!CGff3Writer::xWriteAlignDisc(align, aid)) {
        return false;
    }
    m_Os << "###" << '\n';
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3FlybaseWriter::xAssignAlignmentSplicedLocation(
    CGffAlignRecord& record,
    const CSpliced_seg& spliced,
    const CSpliced_exon& exon)
//  ----------------------------------------------------------------------------
{
    string id;
    CSeq_id::EAccessionInfo productInfo;
    const CSeq_id& productId = spliced.GetProduct_id();
    CSeq_id_Handle bestH = sequence::GetId(
        productId, *m_pScope, sequence::eGetId_Best);
    if (bestH) {
        bestH.GetSeqId()->GetLabel(&id, CSeq_id::eContent);
        productInfo = bestH.GetSeqId()->IdentifyAccession();
    }
    else {
        productId.GetLabel(&id, CSeq_id::eContent);
        productInfo = productId.IdentifyAccession();
    }
    const int tgtWidth = (productInfo & CSeq_id::fAcc_prot) ? 3 : 1;

    unsigned int seqStart = exon.GetProduct_start().AsSeqPos()/tgtWidth+1;
    unsigned int seqStop = exon.GetProduct_end().AsSeqPos()/tgtWidth+1;
    ENa_strand seqStrand = eNa_strand_plus;
    if (spliced.CanGetProduct_strand()  &&  
            spliced.GetProduct_strand() == objects::eNa_strand_minus) {
         seqStrand = eNa_strand_minus;
    }
    record.SetLocation(seqStart, seqStop, seqStrand);
    return true;
}


//  ----------------------------------------------------------------------------
bool CGff3FlybaseWriter::xAssignAlignmentSplicedTarget(
    CGffAlignRecord& record,
    const CSpliced_seg& spliced,
    const CSpliced_exon& exon)
//  ----------------------------------------------------------------------------
{
    string genomicLabel;
    const CSeq_id& genomicId = spliced.GetGenomic_id();
    CSeq_id_Handle bestH = sequence::GetId(
        genomicId, *m_pScope, sequence::eGetId_Best);
    if (bestH) {
        bestH.GetSeqId()->GetLabel(&genomicLabel, CSeq_id::eContent);
    }
    else {
        genomicId.GetLabel(&genomicLabel, CSeq_id::eContent);
    }

    string seqStart = NStr::IntToString(exon.GetGenomic_start());
    string seqStop = NStr::IntToString(exon.GetGenomic_end());
    string seqStrand = "+";
    if (spliced.IsSetGenomic_strand()  &&  
            spliced.GetGenomic_strand() == objects::eNa_strand_minus) {
         seqStrand = "-";
    }

    string target = genomicLabel;
    target += " " + seqStart;
    target += " " + seqStop;
    target += " " + seqStrand;
    record.SetAttribute("Target", target); 
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3FlybaseWriter::xAssignAlignmentSplicedSeqId(
    CGffAlignRecord& record,
    const CSpliced_seg& spliced,
    const CSpliced_exon& exon)
//  ----------------------------------------------------------------------------
{
    string seqId;
    const CSeq_id& genomicId = spliced.GetProduct_id();
    CSeq_id_Handle bestH = sequence::GetId(
         genomicId, *m_pScope, sequence::eGetId_Best);
    bestH.GetSeqId()->GetLabel(&seqId, CSeq_id::eContent);
    record.SetSeqId(seqId);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3FlybaseWriter::xAssignAlignmentScores(
    CGffAlignRecord& record,
    const CSeq_align& align)
//  ----------------------------------------------------------------------------
{
    static const vector<string> supportedScores{
        "Gap", "ambiguous_orientation", "consensus_splices",
        "pct_coverage", "pct_identity_gap", "pct_identity_ungap",
        "rank", "score"
    };

    typedef vector<CRef<CScore> > SCORES;
    if (!align.IsSetScore()) {
        return true;
    }
    const SCORES& scores = align.GetScore();
    for (SCORES::const_iterator cit = scores.begin(); cit != scores.end(); 
            ++cit) {
        const CScore& score = **cit;
        if (!score.IsSetId()  ||  !score.GetId().IsStr()) {
            continue;
        }
        string key = score.GetId().GetStr();
        if (std::find(supportedScores.begin(), supportedScores.end(), key)
                == supportedScores.end()) {
            continue;
        }
        record.SetScore(**cit);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3FlybaseWriter::xAssignAlignmentDensegSeqId(
    CGffAlignRecord& record,
    const CAlnMap& alnMap,
    unsigned int srcRow)
//  ----------------------------------------------------------------------------
{
    //const CSeq_id& targetId = alnMap.GetSeqId(srcRow);
    const CSeq_id& targetId = alnMap.GetSeqId(0);
    CBioseq_Handle targetH = m_pScope->GetBioseqHandle(targetId);
    CSeq_id_Handle targetIdH = targetH.GetSeq_id_Handle();
    try {
        CSeq_id_Handle best = sequence::GetId(
            targetH, sequence::eGetId_ForceAcc);
        if (best) {
            targetIdH = best;
        }
    }
    catch(std::exception&) {};
    CConstRef<CSeq_id> pTargetId = targetIdH.GetSeqId();
    string seqId;
    pTargetId->GetLabel( &seqId, CSeq_id::eContent );
    record.SetSeqId(seqId);
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3FlybaseWriter::xAssignAlignmentDensegTarget(
    CGffAlignRecord& record,
    const CAlnMap& alnMap,
    unsigned int srcRow)
//  ----------------------------------------------------------------------------
{
    const CSeq_id& sourceId = alnMap.GetSeqId(srcRow);
    CBioseq_Handle sourceH = m_pScope->GetBioseqHandle(sourceId);
    CSeq_id_Handle sourceIdH = sourceH.GetSeq_id_Handle();
    try {
        CSeq_id_Handle best = sequence::GetId(
            sourceH, sequence::eGetId_ForceAcc);
        if (best) {
            sourceIdH = best;
        }
    }
    catch(std::exception&) {};
    CConstRef<CSeq_id> pSourceId = sourceIdH.GetSeqId();

    string target;
    pSourceId->GetLabel(&target, CSeq_id::eContent);

    ENa_strand strand = 
        (alnMap.StrandSign(srcRow) == -1) ? eNa_strand_minus : eNa_strand_plus;
    int numSegs = alnMap.GetNumSegs();

    int start2 = -1;
    int start_seg = 0;
    while (start2 < 0 && start_seg < numSegs) { // Skip over -1 start coords
        start2 = alnMap.GetStart(srcRow, start_seg++);
    }

    int stop2 = -1;
    int stop_seg = numSegs-1;
    while (stop2 < 0 && stop_seg >= 0) { // Skip over -1 stop coords
        stop2 = alnMap.GetStart(srcRow, stop_seg--);
    }

    if (strand == eNa_strand_minus) {
        swap(start2, stop2);
        stop2 += alnMap.GetLen(start_seg-1)-1;
    } 
    else {
        stop2 += alnMap.GetLen(stop_seg+1)-1;
    }


    CSeq_id::EAccessionInfo sourceInfo = pSourceId->IdentifyAccession();
    const int tgtWidth = (sourceInfo & CSeq_id::fAcc_prot) ? 3 : 1;

    target += " " + NStr::IntToString(start2/tgtWidth + 1);
    target += " " + NStr::IntToString(stop2/tgtWidth + 1);
    target += " " + string(strand == eNa_strand_plus ? "+" : "-");
    record.SetAttribute("Target", target); 
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3FlybaseWriter::xAssignAlignmentDensegLocation(
    CGffAlignRecord& record,
    const CAlnMap& alnMap,
    unsigned int)
//  ----------------------------------------------------------------------------
{
    unsigned int seqStart = alnMap.GetSeqStart(0);
    unsigned int seqStop = alnMap.GetSeqStop(0);
    ENa_strand seqStrand = (alnMap.StrandSign(0) == 1 ? 
        eNa_strand_plus : 
        eNa_strand_minus);
    record.SetLocation(seqStart, seqStop, seqStrand);
    return true;
}

END_NCBI_SCOPE
