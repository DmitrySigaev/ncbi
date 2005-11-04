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
* Authors:  Paul Thiessen
*
* File Description:
*      module for aligning with BLAST and related algorithms
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>

#include <algo/blast/api/psibl2seq.hpp>
#include <algo/blast/api/objmgrfree_query_data.hpp>
// avoids conflicts between algo/blast stuff and C-toolkit stuff
#undef INT1_MIN
#undef INT1_MAX
#undef INT2_MIN
#undef INT2_MAX

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/general/Object_id.hpp>

#include "cn3d_blast.hpp"
#include "block_multiple_alignment.hpp"
#include "cn3d_pssm.hpp"
#include "sequence_set.hpp"
#include "cn3d_tools.hpp"
#include "structure_set.hpp"
#include "molecule_identifier.hpp"
#include "asn_reader.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

//    bob->db_length = 1000000;
//    SetEffectiveSearchSpaceSize(options, slaveSeqInt->to - slaveSeqInt->from + 1);
//    bob->searchsp_eff = BLASTCalculateSearchSpace(bob, 1, bob->db_length, queryLength);

class TruncatedSequence : public CObject
{
public:
    const Sequence *originalFullSequence;
    CRef < CSeq_entry > truncatedSequence;
    int fromIndex, toIndex;
};

static CRef < TruncatedSequence > CreateTruncatedSequence(const BlockMultipleAlignment *multiple,
    const BlockMultipleAlignment *pair, int alnNum, bool isMaster, int extension)
{
    CRef < TruncatedSequence > ts(new TruncatedSequence);

    // master sequence (only used for blast-two-sequences)
    if (isMaster) {

        ts->originalFullSequence = pair->GetMaster();
        BlockMultipleAlignment::UngappedAlignedBlockList uaBlocks;

        // use alignMasterTo/From if present and reasonable
        if (pair->alignMasterFrom >= 0 && pair->alignMasterFrom < (int)ts->originalFullSequence->Length() &&
            pair->alignMasterTo >= 0 && pair->alignMasterTo < (int)ts->originalFullSequence->Length() &&
            pair->alignMasterFrom <= pair->alignMasterTo)
        {
            ts->fromIndex = pair->alignMasterFrom;
            ts->toIndex = pair->alignMasterTo;
        }

        // use aligned footprint + extension if multiple has any aligned blocks
        else if (multiple && multiple->GetUngappedAlignedBlocks(&uaBlocks) > 0)
        {
            ts->fromIndex = uaBlocks.front()->GetRangeOfRow(0)->from - extension;
            if (ts->fromIndex < 0)
                ts->fromIndex = 0;
            ts->toIndex = uaBlocks.back()->GetRangeOfRow(0)->to + extension;
            if (ts->toIndex >= (int)ts->originalFullSequence->Length())
                ts->toIndex = ts->originalFullSequence->Length() - 1;
        }

        // otherwise, just use the whole sequence
        else {
            ts->fromIndex = 0;
            ts->toIndex = ts->originalFullSequence->Length() - 1;
        }
    }

    // slave sequence
    else {

        ts->originalFullSequence = pair->GetSequenceOfRow(1);

        // use alignSlaveTo/From if present and reasonable
        if (pair->alignSlaveFrom >= 0 && pair->alignSlaveFrom < (int)ts->originalFullSequence->Length() &&
            pair->alignSlaveTo >= 0 && pair->alignSlaveTo < (int)ts->originalFullSequence->Length() &&
            pair->alignSlaveFrom <= pair->alignSlaveTo)
        {
            ts->fromIndex = pair->alignSlaveFrom;
            ts->toIndex = pair->alignSlaveTo;
        }

        // otherwise, just use the whole sequence
        else {
            ts->fromIndex = 0;
            ts->toIndex = ts->originalFullSequence->Length() - 1;
        }
    }

    // create new Bioseq (contained in a Seq-entry) with the truncated sequence
    ts->truncatedSequence.Reset(new CSeq_entry);
    CBioseq& bioseq = ts->truncatedSequence->SetSeq();
    CRef < CSeq_id > id(new CSeq_id);
    id->SetLocal().SetStr(NStr::IntToString(alnNum));
    bioseq.SetId().push_back(id);
    bioseq.SetInst().SetRepr(CSeq_inst::eRepr_raw);
    bioseq.SetInst().SetMol(CSeq_inst::eMol_aa);
    bioseq.SetInst().SetLength(ts->toIndex - ts->fromIndex + 1);
    bioseq.SetInst().SetSeq_data().SetNcbistdaa().Set().resize(ts->toIndex - ts->fromIndex + 1);
    for (int j=ts->fromIndex; j<=ts->toIndex; ++j)
        bioseq.SetInst().SetSeq_data().SetNcbistdaa().Set()[j - ts->fromIndex] =
            LookupNCBIStdaaNumberFromCharacter(ts->originalFullSequence->sequenceString[j]);

    return ts;
}

static bool SeqIdMatchesMaster(const CSeq_id& sid, const Sequence *master, bool usePSSM)
{
    // if blast-sequence-vs-pssm, master will be Sequence master
    if (usePSSM)
        return (master->identifier->MatchesSeqId(sid));

    // if blast-two-sequences, master will be local str "-1"
    else
        return (sid.IsLocal() && sid.GetLocal().IsStr() && sid.GetLocal().GetStr() == "-1");
}

void BLASTer::CreateNewPairwiseAlignmentsByBlast(const BlockMultipleAlignment *multiple,
    const AlignmentList& toRealign, AlignmentList *newAlignments, bool usePSSM)
{
    newAlignments->clear();
    if (usePSSM && (!multiple || multiple->HasNoAlignedBlocks())) {
        ERRORMSG("usePSSM true, but NULL or zero-aligned block multiple alignment");
        return;
    }
    if (!usePSSM && toRealign.size() > 1) {
        ERRORMSG("CreateNewPairwiseAlignmentsByBlast() - currently can only do single blast-2-sequences at a time");
        return;
    }
    if (toRealign.size() == 0)
        return;

    try {
        const Sequence *master = (multiple ? multiple->GetMaster() : NULL);

        int extension = 0;
        if (!RegistryGetInteger(REG_ADVANCED_SECTION, REG_FOOTPRINT_RES, &extension))
            WARNINGMSG("Can't get footprint residue extension from registry");

        // collect subject(s) - second sequence of each realignment
        vector < CRef < TruncatedSequence > > subjectTSs;
        CBioseq_set bss;
        int localID = 0;
        AlignmentList::const_iterator a, ae = toRealign.end();
        for (a=toRealign.begin(); a!=ae; ++a, ++localID) {
            if (!master)
                master = (*a)->GetMaster();
            if ((*a)->GetMaster() != master) {
                ERRORMSG("CreateNewPairwiseAlignmentsByBlast() - all masters must be the same");
                return;
            }
            if ((*a)->NRows() != 2) {
                ERRORMSG("CreateNewPairwiseAlignmentsByBlast() - can only realign pairwise alignments");
                return;
            }
            subjectTSs.push_back(CreateTruncatedSequence(multiple, *a, localID, false, extension));
            bss.SetSeq_set().push_back(subjectTSs.back()->truncatedSequence);
        }
        CRef < blast::IQueryFactory > sequenceSubjects(new blast::CObjMgrFree_QueryFactory(CConstRef<CBioseq_set>(&bss)));

        // main blast engine
        CRef < blast::CPsiBl2Seq > blastEngine;

        // setup searches: blast-sequence-vs-pssm
        CRef < CPssmWithParameters > pssmQuery;
        CRef < blast::CPSIBlastOptionsHandle > pssmOptions;
        if (usePSSM) {
            pssmQuery = CreatePSSM(multiple);
            pssmOptions.Reset(new blast::CPSIBlastOptionsHandle);
            blastEngine.Reset(new
                blast::CPsiBl2Seq(
                    pssmQuery,
                    sequenceSubjects,
                    CConstRef < blast::CPSIBlastOptionsHandle > (pssmOptions.GetPointer())));
        }

        // setup searches: blast-two-sequences
        CRef < TruncatedSequence > masterTS;
        CRef < blast::IQueryFactory > sequenceQuery;
        CRef < blast::CBlastProteinOptionsHandle > sequenceOptions;
        if (!usePSSM) {
            masterTS = CreateTruncatedSequence(multiple, toRealign.front(), -1, true, extension);
            sequenceQuery.Reset(new
                blast::CObjMgrFree_QueryFactory(
                    CConstRef < CBioseq > (&(masterTS->truncatedSequence->GetSeq()))));
            sequenceOptions.Reset(new blast::CBlastProteinOptionsHandle);
            sequenceOptions->SetMatrixName("BLOSUM62");
            blastEngine.Reset(new
                blast::CPsiBl2Seq(
                    sequenceQuery,
                    sequenceSubjects,
                    CConstRef < blast::CBlastProteinOptionsHandle > (sequenceOptions.GetPointer())));
        }

        // actually do the alignment(s)
        CRef < blast::CSearchResults > results = blastEngine->Run();
        string err;
        WriteASNToFile("Seq-align-set.txt", results->GetSeqAlign().GetObject(), false, &err);

        // parse the alignments
        if (results->GetSeqAlign()->Get().size() != toRealign.size()) {
            ERRORMSG("CreateNewPairwiseAlignmentsByBlast() - bad # result alignments");
            return;
        }
        CSeq_align_set::Tdata::const_iterator r, re = results->GetSeqAlign()->Get().end();
        localID = 0;
        for (r=results->GetSeqAlign()->Get().begin(); r!=re; ++r, ++localID) {

            // create new alignment structure
            BlockMultipleAlignment::SequenceList *seqs = new BlockMultipleAlignment::SequenceList(2);
            (*seqs)[0] = master;
            (*seqs)[1] = subjectTSs[localID]->originalFullSequence;
            string slaveTitle = subjectTSs[localID]->originalFullSequence->identifier->ToString();
            auto_ptr < BlockMultipleAlignment > newAlignment(
                new BlockMultipleAlignment(seqs, master->parentSet->alignmentManager));
            newAlignment->SetRowDouble(0, kMax_Double);
            newAlignment->SetRowDouble(1, kMax_Double);

            // check for valid or empty alignment
            if ((*r)->GetType() != CSeq_align::eType_disc || !(*r)->GetSegs().IsDisc()) {
                ERRORMSG("CreateNewPairwiseAlignmentsByBlast() - returned alignment not in expected format (disc)");
            } else if (!(*r)->IsSetDim() && (*r)->GetSegs().GetDisc().Get().size() == 0) {
                WARNINGMSG("BLAST did not find a significant alignment for "
                    << slaveTitle << " with " << (usePSSM ? string("PSSM") : master->identifier->ToString()));
            } else {

                // unpack Seq-align; use first one, which assumes blast returns the highest scoring alignment first
                const CSeq_align& sa = (*r)->GetSegs().GetDisc().Get().front().GetObject();
                if (!sa.IsSetDim() || sa.GetDim() != 2 || sa.GetType() != CSeq_align::eType_partial) {
                    ERRORMSG("CreateNewPairwiseAlignmentsByBlast() - returned alignment not in expected format (dim 2, partial)");
                } else if (sa.GetSegs().IsDenseg()) {

                    // unpack Dense-seg
                    const CDense_seg& ds = sa.GetSegs().GetDenseg();
                    if (!ds.IsSetDim() || ds.GetDim() != 2 || ds.GetIds().size() != 2 ||
                            (int)ds.GetLens().size() != ds.GetNumseg() || (int)ds.GetStarts().size() != 2 * ds.GetNumseg()) {
                        ERRORMSG("CreateNewPairwiseAlignmentsByBlast() - returned alignment format error (denseg dims)");
                    } else if (!SeqIdMatchesMaster(ds.GetIds().front().GetObject(), master, usePSSM) ||
                            !ds.GetIds().back()->IsLocal() || !ds.GetIds().back()->GetLocal().IsStr() ||
                            ds.GetIds().back()->GetLocal().GetStr() != NStr::IntToString(localID)) {
                        ERRORMSG("CreateNewPairwiseAlignmentsByBlast() - returned alignment format error (ids)");
                    } else {
                        // unpack segs
                        CDense_seg::TStarts::const_iterator s = ds.GetStarts().begin();
                        CDense_seg::TLens::const_iterator l, le = ds.GetLens().end();
                        for (l=ds.GetLens().begin(); l!=le; ++l) {
                            int masterStart = *(s++), slaveStart = *(s++);
                            if (masterStart >= 0 && slaveStart >= 0) {  // skip gaps
                                if (!usePSSM)
                                    masterStart += masterTS->fromIndex;
                                slaveStart += subjectTSs[localID]->fromIndex;
                                UngappedAlignedBlock *newBlock = new UngappedAlignedBlock(newAlignment.get());
                                newBlock->SetRangeOfRow(0, masterStart, masterStart + (*l) - 1);
                                newBlock->SetRangeOfRow(1, slaveStart, slaveStart + (*l) - 1);
                                newBlock->width = *l;
                                newAlignment->AddAlignedBlockAtEnd(newBlock);
                            }
                        }
                    }

                } else {
                    ERRORMSG("CreateNewPairwiseAlignmentsByBlast() - returned alignment in unrecognized format");
                }

                // unpack score
                if (!sa.IsSetScore() || sa.GetScore().size() == 0) {
                    WARNINGMSG("BLAST did not return an alignment score for " << slaveTitle);
                } else {
                    CNcbiOstrstream oss;
                    oss << "BLAST result scores for " << slaveTitle << " vs. "
                        << (usePSSM ? string("PSSM") : master->identifier->ToString()) << ':';

                    bool haveE = false;
                    CSeq_align::TScore::const_iterator sc, sce = sa.GetScore().end();
                    for (sc=sa.GetScore().begin(); sc!=sce; ++sc) {
                        if ((*sc)->IsSetId() && (*sc)->GetId().IsStr()) {

                            // E-value (put in status line and double values)
                            if ((*sc)->GetValue().IsReal() && (*sc)->GetId().GetStr() == "e_value") {
                                haveE = true;
                                newAlignment->SetRowDouble(0, (*sc)->GetValue().GetReal());
                                newAlignment->SetRowDouble(1, (*sc)->GetValue().GetReal());
                                string status = string("E-value: ") + NStr::DoubleToString((*sc)->GetValue().GetReal());
                                newAlignment->SetRowStatusLine(0, status);
                                newAlignment->SetRowStatusLine(1, status);
                                oss << ' ' << status;
                            }

                            // raw score
                            else if ((*sc)->GetValue().IsInt() && (*sc)->GetId().GetStr() == "score") {
                                oss << " raw: " << (*sc)->GetValue().GetInt();
                            }

                            // bit score
                            else if ((*sc)->GetValue().IsReal() && (*sc)->GetId().GetStr() == "bit_score") {
                                oss << " bit score: " << (*sc)->GetValue().GetReal();
                            }
                        }
                    }

                    INFOMSG((string) CNcbiOstrstreamToString(oss));
                    if (!haveE)
                        WARNINGMSG("BLAST did not return an E-value for " << slaveTitle);
                }
            }

            // finalize and and add new alignment to list
            if (newAlignment->AddUnalignedBlocks() && newAlignment->UpdateBlockMapAndColors(false))
                newAlignments->push_back(newAlignment.release());
            else
                ERRORMSG("error finalizing alignment");
        }

    } catch (exception& e) {
        ERRORMSG("CreateNewPairwiseAlignmentsByBlast() failed with exception: " << e.what());
    }
}

void BLASTer::CalculateSelfHitScores(const BlockMultipleAlignment *multiple)
{
    if (!multiple) {
        ERRORMSG("NULL multiple alignment");
        return;
    }

    unsigned int row;
    for (row=0; row<multiple->NRows(); ++row) {

        double score = -1.0;

        // set score in row
        multiple->SetRowDouble(row, score);
        string status;
        if (score >= 0.0)
            status = string("Self hit E-value: %g") + NStr::DoubleToString(score);
        else
            status = "(self-hit not yet implemented)";//"No detectable self hit";
        multiple->SetRowStatusLine(row, status);
    }

    // print out overall self-hit rate
    unsigned int nSelfHits = 0;
    static const double threshold = 0.01;
    for (row=0; row<multiple->NRows(); ++row) {
        if (multiple->GetRowDouble(row) >= 0.0 && multiple->GetRowDouble(row) <= threshold)
            ++nSelfHits;
    }
    INFOMSG("Self hits with E-value <= " << setprecision(3) << threshold << ": "
        << (100.0*nSelfHits/multiple->NRows()) << "% ("
        << nSelfHits << '/' << multiple->NRows() << ')' << setprecision(6));
}

END_SCOPE(Cn3D)

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.41  2005/11/04 12:26:10  thiessen
* working C++ blast-sequence-vs-pssm
*
* Revision 1.40  2005/11/03 22:31:32  thiessen
* major reworking of the BLAST core; C++ blast-two-sequences working
*
* Revision 1.39  2005/11/01 02:44:07  thiessen
* fix GCC warnings; switch threader to C++ PSSMs
*
* Revision 1.38  2005/10/19 17:28:18  thiessen
* migrate to wxWidgets 2.6.2; handle signed/unsigned issue
*
* Revision 1.37  2005/03/25 15:10:45  thiessen
* matched self-hit E-values with CDTree2
*
* Revision 1.36  2005/03/08 17:22:31  thiessen
* apparently working C++ PSSM generation
*
* Revision 1.35  2004/07/27 17:38:12  thiessen
* don't call GetPSSM() w/ no aligned blocks
*
* Revision 1.34  2004/05/21 21:41:39  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.33  2004/03/15 18:19:23  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.32  2004/02/19 17:04:49  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.31  2003/07/14 18:37:07  thiessen
* change GetUngappedAlignedBlocks() param types; other syntax changes
*
* Revision 1.30  2003/05/29 16:38:27  thiessen
* set db length for CDD 1.62
*
* Revision 1.29  2003/02/03 19:20:02  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.28  2003/01/31 17:18:58  thiessen
* many small additions and changes...
*
* Revision 1.27  2003/01/23 20:03:05  thiessen
* add BLAST Neighbor algorithm
*
* Revision 1.26  2003/01/22 14:47:30  thiessen
* cache PSSM in BlockMultipleAlignment
*
* Revision 1.25  2002/12/20 02:51:46  thiessen
* fix Prinf to self problems
*
* Revision 1.24  2002/10/25 19:00:02  thiessen
* retrieve VAST alignment from vastalign.cgi on structure import
*
* Revision 1.23  2002/09/19 14:21:03  thiessen
* add search space size calculation to match scores with rpsblast (finally!)
*
* Revision 1.22  2002/09/05 17:39:53  thiessen
* add bit score printout for BLAST/PSSM
*
* Revision 1.21  2002/09/05 13:04:33  thiessen
* restore output stream precision
*
* Revision 1.20  2002/09/04 15:51:52  thiessen
* turn off options->tweak_parameters
*
* Revision 1.19  2002/08/30 16:52:10  thiessen
* progress on trying to match scores with RPS-BLAST
*
* Revision 1.18  2002/08/15 22:13:13  thiessen
* update for wx2.3.2+ only; add structure pick dialog; fix MultitextDialog bug
*
* Revision 1.17  2002/08/01 12:51:36  thiessen
* add E-value display to block aligner
*
* Revision 1.16  2002/07/26 15:28:45  thiessen
* add Alejandro's block alignment algorithm
*
* Revision 1.15  2002/07/23 15:46:49  thiessen
* print out more BLAST info; tweak save file name
*
* Revision 1.14  2002/07/12 13:24:10  thiessen
* fixes for PSSM creation to agree with cddumper/RPSBLAST
*
* Revision 1.13  2002/06/17 19:31:54  thiessen
* set db_length in blast options
*
* Revision 1.12  2002/06/13 14:54:07  thiessen
* add sort by self-hit
*
* Revision 1.11  2002/06/13 13:32:39  thiessen
* add self-hit calculation
*
* Revision 1.10  2002/05/22 17:17:08  thiessen
* progress on BLAST interface ; change custom spin ctrl implementation
*
* Revision 1.9  2002/05/17 19:10:27  thiessen
* preliminary range restriction for BLAST/PSSM
*
* Revision 1.8  2002/05/07 20:22:47  thiessen
* fix for BLAST/PSSM
*
* Revision 1.7  2002/05/02 18:40:25  thiessen
* do BLAST/PSSM for debug builds only, for testing
*
* Revision 1.6  2002/03/28 14:06:02  thiessen
* preliminary BLAST/PSSM ; new CD startup style
*
* Revision 1.5  2001/11/27 16:26:07  thiessen
* major update to data management system
*
* Revision 1.4  2001/10/18 19:57:32  thiessen
* fix redundant creation of C bioseqs
*
* Revision 1.3  2001/09/27 15:37:58  thiessen
* decouple sequence import and BLAST
*
* Revision 1.2  2001/09/19 22:55:39  thiessen
* add preliminary net import and BLAST
*
* Revision 1.1  2001/09/18 03:10:45  thiessen
* add preliminary sequence import pipeline
*
*/
