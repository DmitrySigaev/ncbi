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
*      Interface to Alejandro's block alignment algorithm
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.h>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/general/Object_id.hpp>

#ifdef __WXMSW__
#include <windows.h>
#include <wx/msw/winundef.h>
#endif
#include <wx/wx.h>

#include "cn3d/cn3d_ba_interface.hpp"
#include "cn3d/block_multiple_alignment.hpp"
#include "cn3d/sequence_set.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/asn_converter.hpp"
#include "cn3d/molecule_identifier.hpp"
#include "cn3d/wx_tools.hpp"
#include "cn3d/cn3d_tools.hpp"
#include "cn3d/cn3d_blast.hpp"

#include "struct_dp/struct_dp.h"

// necessary C-toolkit headers
#include <ncbi.h>
#include <blastkar.h>
#include <posit.h>
#include "cn3d/cn3d_blocka.h"

// functions from cn3d_blockalign.c accessed herein (no header for these yet...)
extern "C" {
extern void allocateAlignPieceMemory(Int4 numBlocks);
void findAllowedGaps(SeqAlign *listOfSeqAligns, Int4 numBlocks, Int4 *allowedGaps,
    Nlm_FloatHi percentile, Int4 gapAddition);
extern void findAlignPieces(Uint1Ptr convertedQuery, Int4 queryLength,
    Int4 startQueryPosition, Int4 endQueryPosition, Int4 numBlocks, Int4 *blockStarts, Int4 *blockEnds,
    Int4 masterLength, BLAST_Score **posMatrix, Int4 *scoreThresholds,
    Int4 *frozenBlocks, Boolean localAlignment);
extern void LIBCALL sortAlignPieces(Int4 numBlocks);
extern SeqAlign *makeMultiPieceAlignments(Uint1Ptr query, Int4 numBlocks, Int4 queryLength, Uint1Ptr seq,
    Int4 seqLength, Int4 *blockStarts, Int4 *blockEnds, Int4 *allowedGaps, Int4 scoreThresholdMultipleBlock,
    SeqIdPtr subject_id, SeqIdPtr query_id, Int4* bestFirstBlock, Int4 *bestLastBlock, Nlm_FloatHi Lambda,
    Nlm_FloatHi K, Int4 searchSpaceSize, Boolean localAlignment, Nlm_FloatHi Ethreshold);
extern void freeBestPairs(Int4 numBlocks);
extern void freeAlignPieceLists(Int4 numBlocks);
extern void freeBestScores(Int4 numBlocks);
extern Boolean ValidateFrozenBlockPositions(Int4 *frozenBlocks,
   Int4 numBlocks, Int4 startQueryRegion, Int4 endQueryRegion,
   Int4 *blockStarts, Int4 *blockEnds, Int4 *allowedGaps);
extern int *parseBlockThresholds(Char *stringToParse, Int4 numBlocks, Boolean localAlignment);
extern Int4 getSearchSpaceSize(Int4 masterLength, Int4 queryLength, Int4 databaseLength, Int4 initSearchSpaceSize);
}

// hack so I can catch memory leaks specific to this module, at the line where allocation occurs
#ifdef _DEBUG
#ifdef MemNew
#undef MemNew
#endif
#define MemNew(sz) memset(malloc(sz), 0, (sz))
#endif

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

class BlockAlignerOptionsDialog : public wxDialog
{
public:
    BlockAlignerOptionsDialog(wxWindow* parent, const BlockAligner::BlockAlignerOptions& init);
    ~BlockAlignerOptionsDialog(void);

    bool GetValues(BlockAligner::BlockAlignerOptions *options);

private:
    IntegerSpinCtrl *iSingle, *iMultiple, *iExtend, *iDataLen, *iSearchLen;
    FloatingPointSpinCtrl *fpPercent, *fpLambda, *fpK;
    wxCheckBox *cGlobal, *cMerge, *cKeepBlocks, *cLongGaps;

    void OnCloseWindow(wxCloseEvent& event);
    void OnButton(wxCommandEvent& event);
    DECLARE_EVENT_TABLE()
};

BlockAligner::BlockAligner(void)
{
    // default options
    currentOptions.singleBlockThreshold = -99;
    currentOptions.multipleBlockThreshold = -99;
    currentOptions.allowedGapExtension = 10;
    currentOptions.gapLengthPercentile = 0.6;
    currentOptions.lambda = 0.0;
    currentOptions.K = 0.0;
    currentOptions.databaseLength = 0;
    currentOptions.searchSpaceLength = 0;
    currentOptions.globalAlignment = true;
    currentOptions.mergeAfterEachSequence = false;
    currentOptions.keepExistingBlocks = false;
    currentOptions.allowLongGaps = true;
}

static Int4 Round(double d)
{
    if (d >= 0.0)
        return (Int4) (d + 0.5);
    else
        return (Int4) (d - 0.5);
}

static BlockMultipleAlignment * UnpackBlockAlignerSeqAlign(CSeq_align& sa,
    const Sequence *master, const Sequence *query)
{
    auto_ptr<BlockMultipleAlignment> bma;

    // make sure the overall structure of this SeqAlign is what we expect
    if (!sa.IsSetDim() || sa.GetDim() != 2 ||
        !((sa.GetSegs().IsDisc() && sa.GetSegs().GetDisc().Get().size() > 0) || sa.GetSegs().IsDenseg()))
    {
        ERRORMSG("Confused by block aligner's result format");
        return NULL;
    }

    // create new alignment structure
    BlockMultipleAlignment::SequenceList *seqs = new BlockMultipleAlignment::SequenceList(2);
    (*seqs)[0] = master;
    (*seqs)[1] = query;
    bma.reset(new BlockMultipleAlignment(seqs, master->parentSet->alignmentManager));

    // get list of segs (can be a single or a set)
    list < CRef < CSeq_align > > segs;
    if (sa.GetSegs().IsDisc())
        segs = sa.GetSegs().GetDisc().Get();
    else
        segs.push_back(CRef<CSeq_align>(&sa));

    // loop through segs, adding aligned block for each starts pair that doesn't describe a gap
    CSeq_align_set::Tdata::const_iterator s, se = segs.end();
    CDense_seg::TStarts::const_iterator i_start;
    CDense_seg::TLens::const_iterator i_len;
    for (s=segs.begin(); s!=se; s++) {

        // check to make sure query is first id, master is second id
        if ((*s)->GetDim() != 2 || !(*s)->GetSegs().IsDenseg() || (*s)->GetSegs().GetDenseg().GetDim() != 2 ||
            (*s)->GetSegs().GetDenseg().GetIds().size() != 2 ||
            !query->identifier->MatchesSeqId(*((*s)->GetSegs().GetDenseg().GetIds().front())) ||
            !master->identifier->MatchesSeqId(*((*s)->GetSegs().GetDenseg().GetIds().back())))
        {
            ERRORMSG("Confused by seg format in block aligner's result");
            return NULL;
        }

        int i, queryStart, masterStart, length;
        i_start = (*s)->GetSegs().GetDenseg().GetStarts().begin();
        i_len = (*s)->GetSegs().GetDenseg().GetLens().begin();
        for (i=0; i<(*s)->GetSegs().GetDenseg().GetNumseg(); i++) {

            // if either start is -1, this is a gap -> skip
            queryStart = *(i_start++);
            masterStart = *(i_start++);
            length = *(i_len++);
            if (queryStart < 0 || masterStart < 0 || length <= 0) continue;

            // add aligned block
            UngappedAlignedBlock *newBlock = new UngappedAlignedBlock(bma.get());
            newBlock->SetRangeOfRow(0, masterStart, masterStart + length - 1);
            newBlock->SetRangeOfRow(1, queryStart, queryStart + length - 1);
            newBlock->width = length;
            bma->AddAlignedBlockAtEnd(newBlock);
        }
    }

    // finalize the alignment
    if (!bma->AddUnalignedBlocks() || !bma->UpdateBlockMapAndColors(false)) {
        ERRORMSG("Error finalizing alignment!");
        return NULL;
    }

    // get scores
    wxString score;
    CSeq_align::TScore::const_iterator c, ce = sa.GetScore().end();
    for (c=sa.GetScore().begin(); c!=ce; c++) {
        if ((*c)->IsSetId() && (*c)->GetId().IsStr()) {

            // raw score
            if ((*c)->GetValue().IsInt() && (*c)->GetId().GetStr() == "score") {
                wxString tmp = score;
                score.Printf("%s raw score: %i", tmp.c_str(), (*c)->GetValue().GetInt());
            }

            // E-value
            if ((*c)->GetValue().IsReal() && (*c)->GetId().GetStr() == "e_value"
                    && (*c)->GetValue().GetReal() > 0.0) {
                wxString tmp = score;
                score.Printf("%s E-value: %g", tmp.c_str(), (*c)->GetValue().GetReal());
            }
        }
    }
    if (score.size() > 0) {
        bma->SetRowStatusLine(0, score.c_str());
        bma->SetRowStatusLine(1, score.c_str());
    }

    // success
    return bma.release();
}

static BlockMultipleAlignment * UnpackDPResult(DP_BlockInfo *blocks, DP_AlignmentResult *result,
    const Sequence *master, const Sequence *query)
{

    // create new alignment structure
    BlockMultipleAlignment::SequenceList *seqs = new BlockMultipleAlignment::SequenceList(2);
    (*seqs)[0] = master;
    (*seqs)[1] = query;
    auto_ptr<BlockMultipleAlignment>
        bma(new BlockMultipleAlignment(seqs, master->parentSet->alignmentManager));

    // unpack result blocks
    for (int b=0; b<result->nBlocks; b++) {
        UngappedAlignedBlock *newBlock = new UngappedAlignedBlock(bma.get());
        newBlock->width = blocks->blockSizes[b - result->firstBlock];
        newBlock->SetRangeOfRow(0,
            blocks->blockPositions[b - result->firstBlock],
            blocks->blockPositions[b - result->firstBlock] + newBlock->width - 1);
        newBlock->SetRangeOfRow(1,
            result->blockPositions[b],
            result->blockPositions[b] + newBlock->width - 1);
        bma->AddAlignedBlockAtEnd(newBlock);
    }

    // finalize the alignment
    if (!bma->AddUnalignedBlocks() || !bma->UpdateBlockMapAndColors(false)) {
        ERRORMSG("Error finalizing alignment!");
        return NULL;
    }

    // get scores
    wxString score;
    score.Printf(" raw score: %i", result->score);
    bma->SetRowStatusLine(0, score.c_str());
    bma->SetRowStatusLine(1, score.c_str());

    // success
    return bma.release();
}

static void FreezeBlocks(const BlockMultipleAlignment *multiple,
    const BlockMultipleAlignment *pairwise, Int4 *frozenBlocks)
{
    BlockMultipleAlignment::UngappedAlignedBlockList multipleABlocks, pairwiseABlocks;
    multiple->GetUngappedAlignedBlocks(&multipleABlocks);
    pairwise->GetUngappedAlignedBlocks(&pairwiseABlocks);

    // if a block in the multiple is contained in the pairwise (looking at master coords),
    // then add a constraint to keep it there
    BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator
        m, me = multipleABlocks.end(), p, pe = pairwiseABlocks.end();
    const Block::Range *multipleRangeMaster, *pairwiseRangeMaster, *pairwiseRangeSlave;
    int i;
    for (i=0, m=multipleABlocks.begin(); m!=me; i++, m++) {
        multipleRangeMaster = (*m)->GetRangeOfRow(0);
        for (p=pairwiseABlocks.begin(); p!=pe; p++) {
            pairwiseRangeMaster = (*p)->GetRangeOfRow(0);
            if (pairwiseRangeMaster->from <= multipleRangeMaster->from &&
                    pairwiseRangeMaster->to >= multipleRangeMaster->to) {
                pairwiseRangeSlave = (*p)->GetRangeOfRow(1);
                frozenBlocks[i] = pairwiseRangeSlave->from +
                    (multipleRangeMaster->from - pairwiseRangeMaster->from);
                break;
            }
        }
        if (p == pe) frozenBlocks[i] = -1;
//        if (frozenBlocks[i] >= 0)
//            TESTMSG("block " << (i+1) << " frozen at query " << (frozenBlocks[i]+1));
//        else
//            TESTMSG("block " << (i+1) << " unfrozen");
    }
}

static bool SameAlignments(const BlockMultipleAlignment *a, const BlockMultipleAlignment *b)
{
    try {
        if (a->NRows() != b->NRows())
            throw "different # rows";

        int row;
        for (row=0; row<a->NRows(); row++)
            if (a->GetSequenceOfRow(row) != b->GetSequenceOfRow(row))
                throw "different sequences (or different order)";

        if (a->GetRowStatusLine(0) != b->GetRowStatusLine(0))
            throw "different status (score)";

        if (a->NBlocks() != b->NBlocks())
            throw "different # blocks";

        BlockMultipleAlignment::UngappedAlignedBlockList au, bu;
        a->GetUngappedAlignedBlocks(&au);
        b->GetUngappedAlignedBlocks(&bu);
        if (au.size() != bu.size())
            throw "different # aligned blocks";

        for (int block=0; block<au.size(); block++) {
            for (row=0; row<a->NRows(); row++) {
                const Block::Range *ar = au[block]->GetRangeOfRow(row), *br = bu[block]->GetRangeOfRow(row);
                if (ar->from != br->from || ar->to != br->to)
                    throw "different block ranges";
            }
        }
    }

    catch (const char *err) {
        ERRORMSG("Alignments are different: " << err);
        return false;
    }

    return true;
}

// global stuff for DP block aligner score callback
DP_BlockInfo *dpBlocks = NULL;
const BLAST_Matrix *dpPSSM = NULL;
const Sequence *dpQuery = NULL;

// sum of scores for residue vs. PSSM
int dpScoreFunction(unsigned int block, unsigned int queryPos)
{
    if (!dpBlocks || !dpPSSM || !dpQuery || block >= dpBlocks->nBlocks ||
        queryPos > dpQuery->Length() - dpBlocks->blockSizes[block])
    {
        ERRORMSG("dpScoreFunction() - bad parameters");
        return DP_NEGATIVE_INFINITY;
    }

    int i, masterPos = dpBlocks->blockPositions[block], score = 0;
    for (i=0; i<dpBlocks->blockSizes[block]; i++)
        score += dpPSSM->matrix[masterPos + i]
            [LookupBLASTResidueNumberFromCharacter(dpQuery->sequenceString[queryPos + i])];

    return score;
}

bool BlockAligner::CreateNewPairwiseAlignmentsByBlockAlignment(BlockMultipleAlignment *multiple,
    const AlignmentList& toRealign, AlignmentList *newAlignments, int *nRowsAddedToMultiple,
    bool canMerge)
{
    // parameters passed to Alejandro's functions
    Int4 numBlocks;
    Uint1Ptr convertedQuery;
    Int4 queryLength;
    Int4 *blockStarts;
    Int4 *blockEnds;
    Int4 masterLength;
    BLAST_Score **thisScoreMat;
    Uint1Ptr masterSequence;
    Int4 *allowedGaps, *currentAllowedGaps;
    SeqIdPtr subject_id;
    SeqIdPtr query_id;
    Int4 bestFirstBlock, bestLastBlock;
    SeqAlignPtr results;
    SeqAlignPtr listOfSeqAligns = NULL;
    Int4 *perBlockThresholds;

    // show options dialog each time block aligner is run
    if (!SetOptions(NULL)) return false;
    if (currentOptions.mergeAfterEachSequence && !canMerge) {
        ERRORMSG("Can only merge when editing is enabled in the sequence window");
        return false;
    }

    // the following would be command-line arguments to Alejandro's standalone program
    Boolean localAlignment = currentOptions.globalAlignment ? FALSE : TRUE;
    Char scoreThresholdsMultipleBlockString[20];
    BLAST_Score scoreThresholdMultipleBlock = currentOptions.multipleBlockThreshold;
    Nlm_FloatHi Lambda = currentOptions.lambda;
    Nlm_FloatHi K = currentOptions.K;
    Nlm_FloatHi percentile = currentOptions.gapLengthPercentile;
    Int4 gapAddition = currentOptions.allowedGapExtension;
    Int4 searchSpaceSize;
    Nlm_FloatHi scaleMult = 1.0;
    Int4 startQueryPosition;
    Int4 endQueryPosition;
    Nlm_FloatHi maxEValue = 10.0;

    newAlignments->clear();
    BlockMultipleAlignment::UngappedAlignedBlockList blocks;
    multiple->GetUngappedAlignedBlocks(&blocks);
    numBlocks = blocks.size();
    BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator b, be = blocks.end();
    if (nRowsAddedToMultiple) *nRowsAddedToMultiple = 0;

    // use Alejandro's per-block threshold parser, to make sure same values are used
    sprintf(scoreThresholdsMultipleBlockString, "%i", currentOptions.singleBlockThreshold);
    perBlockThresholds = parseBlockThresholds(scoreThresholdsMultipleBlockString, numBlocks, localAlignment);

    // master sequence info
    masterLength = multiple->GetMaster()->Length();
    masterSequence = (Uint1Ptr) MemNew(sizeof(Uint1) * masterLength);
    int i;
    for (i=0; i<masterLength; i++)
        masterSequence[i] = ResToInt(multiple->GetMaster()->sequenceString[i]);
    subject_id = multiple->GetMaster()->parentSet->GetOrCreateBioseq(multiple->GetMaster())->id;

    // convert all sequences to Bioseqs
    multiple->GetMaster()->parentSet->CreateAllBioseqs(multiple);

    // create SeqAlign from this BlockMultipleAlignment
    listOfSeqAligns = multiple->CreateCSeqAlign();

    // set up block info
    blockStarts = (Int4*) MemNew(sizeof(Int4) * masterLength);
    blockEnds = (Int4*) MemNew(sizeof(Int4) * masterLength);
    allowedGaps = (Int4*) MemNew(sizeof(Int4) * (numBlocks - 1));
    currentAllowedGaps = (Int4*) MemNew(sizeof(Int4) * (numBlocks - 1));
    for (i=0, b=blocks.begin(); b!=be; i++, b++) {
        const Block::Range *range = (*b)->GetRangeOfRow(0);
        blockStarts[i] = range->from;
        blockEnds[i] = range->to;
    }
    if (listOfSeqAligns) {
        findAllowedGaps(listOfSeqAligns, numBlocks, allowedGaps, percentile, gapAddition);
    } else {
        for (i=0; i<numBlocks-1; i++)
            allowedGaps[i] = blockStarts[i+1] - blockEnds[i] - 1 + gapAddition;
    }
//    for (i=0; i<numBlocks-1; i++)
//        TESTMSG("allowed gap after block " << (i+1) << ": " << allowedGaps[i]);

    // use my own block aligner
    dpBlocks = DP_CreateBlockInfo(numBlocks);
    for (i=0; i<numBlocks; i++) {
        dpBlocks->blockPositions[i] = blockStarts[i];
        dpBlocks->blockSizes[i] = blockEnds[i] - blockStarts[i] + 1;
        if (i < numBlocks-1)
            dpBlocks->maxLoops[i] = allowedGaps[i];
    }

    // set up PSSM
    const BLAST_Matrix *matrix = multiple->GetPSSM();
    thisScoreMat = matrix->matrix;

    Int4 *frozenBlocks = (Int4*) MemNew(numBlocks * sizeof(Int4));

    AlignmentList::const_iterator s, se = toRealign.end();
    for (s=toRealign.begin(); s!=se; s++) {
        if (multiple && (*s)->GetMaster() != multiple->GetMaster())
            ERRORMSG("master sequence mismatch");

        // slave sequence info
        const Sequence *query = (*s)->GetSequenceOfRow(1);
        queryLength = query->Length();
        convertedQuery = (Uint1Ptr) MemNew(sizeof(Uint1) * queryLength);
        for (i=0; i<queryLength; i++)
            convertedQuery[i] = ResToInt(query->sequenceString[i]);
        query_id = query->parentSet->GetOrCreateBioseq(query)->id;
        startQueryPosition =
            ((*s)->alignSlaveFrom >= 0 && (*s)->alignSlaveFrom < query->Length()) ?
                (*s)->alignSlaveFrom : 0;
        endQueryPosition =
            ((*s)->alignSlaveTo >= 0 && (*s)->alignSlaveTo < query->Length()) ?
                ((*s)->alignSlaveTo + 1) : query->Length();

        // set search space size
        searchSpaceSize = getSearchSpaceSize(masterLength, queryLength,
            currentOptions.databaseLength, currentOptions.searchSpaceLength);

        // set frozen blocks
        for (i=0; i<numBlocks-1; i++)
            currentAllowedGaps[i] = allowedGaps[i];
        Boolean validFrozenBlocks = TRUE;
        if (!currentOptions.keepExistingBlocks) {
            for (i=0; i<numBlocks; i++) frozenBlocks[i] = -1;
        } else {
            FreezeBlocks(multiple, *s, frozenBlocks);
            validFrozenBlocks = ValidateFrozenBlockPositions(frozenBlocks,
                numBlocks, startQueryPosition, endQueryPosition,
                blockStarts, blockEnds, currentAllowedGaps);

            // if frozen block positions aren't valid as-is, try relaxing gap length restrictions
            // between adjacent frozen blocks and see if that fixes it
            if (!validFrozenBlocks && currentOptions.allowLongGaps) {
                TRACEMSG("trying to relax gap length restrictions between frozen blocks...");
                for (i=0; i<numBlocks-1; i++)
                    if (frozenBlocks[i] >= 0 && frozenBlocks[i+1] >= 0)
                        currentAllowedGaps[i] = 1000000;
                validFrozenBlocks = ValidateFrozenBlockPositions(frozenBlocks,
                        numBlocks, startQueryPosition, endQueryPosition,
                        blockStarts, blockEnds, currentAllowedGaps);
            }
        }

        for (i=0; i<numBlocks; i++) {
            if (frozenBlocks[i] >= 0)
                dpBlocks->freezeBlocks[i] = frozenBlocks[i];
            else
                dpBlocks->freezeBlocks[i] = DP_UNFROZEN_BLOCK;
        }
        DP_AlignmentResult *dpResult = NULL;
        int dpStatus;

        // actually do the block alignment
        if (validFrozenBlocks) {

            // Alejandro's
            INFOMSG("doing " << (localAlignment ? "local" : "global") << " block alignment of "
                << query->identifier->ToString());
            allocateAlignPieceMemory(numBlocks);
            findAlignPieces(convertedQuery, queryLength, startQueryPosition, endQueryPosition,
                numBlocks, blockStarts, blockEnds, masterLength, thisScoreMat,
                perBlockThresholds, frozenBlocks, localAlignment);
            sortAlignPieces(numBlocks);
            results = makeMultiPieceAlignments(convertedQuery, numBlocks,
                queryLength, masterSequence, masterLength,
                blockStarts, blockEnds, currentAllowedGaps, scoreThresholdMultipleBlock,
                subject_id, query_id, &bestFirstBlock, &bestLastBlock,
                Lambda, K, searchSpaceSize, localAlignment, maxEValue);

            // mine
            dpPSSM = matrix;
            dpQuery = query;
            if (localAlignment)
                dpStatus = DP_LocalBlockAlign(dpBlocks, dpScoreFunction,
                    startQueryPosition, endQueryPosition-1, &dpResult);
            else
                dpStatus = DP_GlobalBlockAlign(dpBlocks, dpScoreFunction,
                    startQueryPosition, endQueryPosition-1, &dpResult);

        } else
            results = NULL;

        // process results; assume first result SeqAlign is the highest scoring
        BlockMultipleAlignment *newAlignment;
        if (results) {
#ifdef _DEBUG
            AsnIoPtr aip = AsnIoOpen("seqalign-ba.txt", "w");
            SeqAlignAsnWrite(results, aip, NULL);
            AsnIoFree(aip, true);
#endif

            CSeq_align best;
            string err;
            if (!ConvertAsnFromCToCPP(results, (AsnWriteFunc) SeqAlignAsnWrite, &best, &err) ||
                (newAlignment=UnpackBlockAlignerSeqAlign(best, multiple->GetMaster(), query)) == NULL) {
                ERRORMSG("conversion of results to BlockMultipleAlignment object failed: " << err);
            } else {

                if (currentOptions.mergeAfterEachSequence) {
                    if (multiple->MergeAlignment(newAlignment)) {
                        delete newAlignment; // if merge is successful, we can delete this alignment;
                        if (nRowsAddedToMultiple)
                            (*nRowsAddedToMultiple)++;
                        else
                            ERRORMSG("BlockAligner::CreateNewPairwiseAlignmentsByBlockAlignment() "
                                "called with merge on, but NULL nRowsAddedToMultiple pointer");
                        // recalculate PSSM
                        matrix = multiple->GetPSSM();
                        thisScoreMat = matrix->matrix;

                    } else {                 // otherwise keep it
                        newAlignments->push_back(newAlignment);
                    }
                }

                else {
                    newAlignments->push_back(newAlignment);
                }
            }

            SeqAlignSetFree(results);
        }

        // no alignment or block aligner failed - add old alignment to list so it doesn't get lost
        else {
            string error;
            if (!validFrozenBlocks)
                error = "invalid frozen block positions";
            else
                error = "block aligner found no significant alignment";
            WARNINGMSG(error << " - current alignment unchanged");
            newAlignment = (*s)->Clone();
            newAlignment->SetRowDouble(0, -1.0);
            newAlignment->SetRowDouble(1, -1.0);
            newAlignment->SetRowStatusLine(0, error);
            newAlignment->SetRowStatusLine(1, error);
            newAlignments->push_back(newAlignment);
        }


        // add result from DP
        if (dpResult) {
            BlockMultipleAlignment *dpAlignment =
                UnpackDPResult(dpBlocks, dpResult, multiple->GetMaster(), query);
            if (dpAlignment) {
                if (!results) {
                    ERRORMSG("DP aligner found alignment, but Alejandro's didn't");
                    dpAlignment->SetRowStatusLine(0,
                        dpAlignment->GetRowStatusLine(0) + " (DP)");
                    newAlignments->push_back(dpAlignment);
                } else if (!SameAlignments(newAlignment, dpAlignment)) {
                    newAlignment->SetRowStatusLine(0,
                        newAlignment->GetRowStatusLine(0) + " (A)");
                    dpAlignment->SetRowStatusLine(0,
                        dpAlignment->GetRowStatusLine(0) + " (DP)");
                    newAlignments->push_back(dpAlignment);
                } else {
                    INFOMSG("Alejandro and DP alignment results are identical");
                    delete dpAlignment;
                }
            }
            DP_DestroyAlignmentResult(dpResult);
        } else {
            if (results)
                ERRORMSG("DP aligner found no alignment, but Alejandro's did");
            else
                WARNINGMSG("DP block aligner returned no alignment");
        }

        // cleanup
        MemFree(convertedQuery);
        if (validFrozenBlocks) {
            freeBestPairs(numBlocks);
            freeAlignPieceLists(numBlocks);
            freeBestScores(numBlocks);
        }
    }

    // cleanup
    MemFree(blockStarts);
    MemFree(blockEnds);
    MemFree(allowedGaps);
    MemFree(currentAllowedGaps);
    if (listOfSeqAligns) SeqAlignSetFree(listOfSeqAligns);
    MemFree(masterSequence);
    MemFree(frozenBlocks);
    MemFree(perBlockThresholds);

    DP_DestroyBlockInfo(dpBlocks);
    dpBlocks = NULL;
    dpPSSM = NULL;
    dpQuery = NULL;

    return true;
}

bool BlockAligner::SetOptions(wxWindow* parent)
{
    BlockAlignerOptionsDialog dialog(parent, currentOptions);
    bool ok = (dialog.ShowModal() == wxOK);
    if (ok && !dialog.GetValues(&currentOptions))
        ERRORMSG("Error getting options from dialog!");
    return ok;
}


/////////////////////////////////////////////////////////////////////////////////////
////// BlockAlignerOptionsDialog stuff
////// taken (and modified) from block_aligner_dialog.wdr code
/////////////////////////////////////////////////////////////////////////////////////

#define ID_TEXT 10000
#define ID_T_SINGLE 10001
#define ID_S_SINGLE 10002
#define ID_T_MULT 10003
#define ID_S_MULT 10004
#define ID_T_EXT 10005
#define ID_S_EXT 10006
#define ID_T_PERCENT 10007
#define ID_S_PERCENT 10008
#define ID_T_LAMBDA 10009
#define ID_S_LAMBDA 10010
#define ID_T_K 10011
#define ID_S_K 10012
#define ID_T_SIZE 10013
#define ID_S_SIZE 10014
#define ID_C_GLOBAL 10015
#define ID_C_MERGE 10016
#define ID_C_KEEP_BLOCKS 10017
#define ID_C_GAPS 10018
#define ID_B_OK 10019
#define ID_B_CANCEL 10020

BEGIN_EVENT_TABLE(BlockAlignerOptionsDialog, wxDialog)
    EVT_BUTTON(-1,  BlockAlignerOptionsDialog::OnButton)
    EVT_CLOSE (     BlockAlignerOptionsDialog::OnCloseWindow)
END_EVENT_TABLE()

BlockAlignerOptionsDialog::BlockAlignerOptionsDialog(
    wxWindow* parent, const BlockAligner::BlockAlignerOptions& init) :
        wxDialog(parent, -1, "Set Block Aligner Options", wxPoint(100,100), wxDefaultSize,
            wxCAPTION | wxSYSTEM_MENU) // not resizable
{
    wxPanel *panel = new wxPanel(this, -1);
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );
    wxStaticBox *item2 = new wxStaticBox( panel, -1, "Block Aligner Options" );
    wxStaticBoxSizer *item1 = new wxStaticBoxSizer( item2, wxVERTICAL );
    wxFlexGridSizer *item3 = new wxFlexGridSizer( 3, 0, 0 );
    item3->AddGrowableCol( 1 );

    wxStaticText *item4 = new wxStaticText( panel, ID_TEXT, "Single block score threshold:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item4, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    iSingle = new IntegerSpinCtrl(panel,
        -100, 100, 1, init.singleBlockThreshold,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    item3->Add(iSingle->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item3->Add(iSingle->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    wxStaticText *item7 = new wxStaticText( panel, ID_TEXT, "Multiple block score threshold:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item7, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    iMultiple = new IntegerSpinCtrl(panel,
        -100, 100, 1, init.multipleBlockThreshold,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    item3->Add(iMultiple->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item3->Add(iMultiple->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    wxStaticText *item10 = new wxStaticText( panel, ID_TEXT, "Allowed gap extension:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item10, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    iExtend = new IntegerSpinCtrl(panel,
        0, 100, 1, init.allowedGapExtension,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    item3->Add(iExtend->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item3->Add(iExtend->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    wxStaticText *item13 = new wxStaticText( panel, ID_TEXT, "Gap length percentile:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item13, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    fpPercent = new FloatingPointSpinCtrl(panel,
        0.0, 1.0, 0.1, init.gapLengthPercentile,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    item3->Add(fpPercent->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item3->Add(fpPercent->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    item3->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );
    item3->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );
    item3->Add( 5, 5, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item16 = new wxStaticText( panel, ID_TEXT, "Lambda:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item16, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    fpLambda = new FloatingPointSpinCtrl(panel,
        0.0, 10.0, 0.1, init.lambda,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    item3->Add(fpLambda->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item3->Add(fpLambda->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    wxStaticText *item19 = new wxStaticText( panel, ID_TEXT, "K:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item19, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    fpK = new FloatingPointSpinCtrl(panel,
        0.0, 1.0, 0.01, init.K,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    item3->Add(fpK->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item3->Add(fpK->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    wxStaticText *item40 = new wxStaticText( panel, ID_TEXT, "Database length:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item40, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    iDataLen = new IntegerSpinCtrl(panel,
        0, kMax_Int, 1000, init.databaseLength,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    item3->Add(iDataLen->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item3->Add(iDataLen->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    wxStaticText *item22 = new wxStaticText( panel, ID_TEXT, "Search space length:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item22, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    iSearchLen = new IntegerSpinCtrl(panel,
        0, kMax_Int, 1000, init.searchSpaceLength,
        wxDefaultPosition, wxSize(80, SPIN_CTRL_HEIGHT), 0,
        wxDefaultPosition, wxSize(-1, SPIN_CTRL_HEIGHT));
    item3->Add(iSearchLen->GetTextCtrl(), 0, wxALIGN_CENTRE|wxLEFT|wxTOP|wxBOTTOM, 5);
    item3->Add(iSearchLen->GetSpinButton(), 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxTOP|wxBOTTOM, 5);

    item3->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );
    item3->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );
    item3->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item23 = new wxStaticText( panel, ID_TEXT, "Global alignment:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item23, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    cGlobal = new wxCheckBox( panel, ID_C_GLOBAL, "", wxDefaultPosition, wxDefaultSize, 0 );
    cGlobal->SetValue(init.globalAlignment);
    item3->Add( cGlobal, 0, wxALIGN_CENTRE|wxALL, 5 );
    item3->Add( 5, 5, 0, wxALIGN_CENTRE, 5 );

    wxStaticText *item28 = new wxStaticText( panel, ID_TEXT, "Merge after each row aligned:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item28, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    cMerge = new wxCheckBox( panel, ID_C_MERGE, "", wxDefaultPosition, wxDefaultSize, 0 );
    cMerge->SetValue(init.mergeAfterEachSequence);
    item3->Add( cMerge, 0, wxALIGN_CENTRE|wxALL, 5 );
    item3->Add( 5, 5, 0, wxALIGN_CENTRE, 5 );

    wxStaticText *item24 = new wxStaticText( panel, ID_TEXT, "Keep existing blocks:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item24, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    cKeepBlocks = new wxCheckBox( panel, ID_C_KEEP_BLOCKS, "", wxDefaultPosition, wxDefaultSize, 0 );
    cKeepBlocks->SetValue(init.keepExistingBlocks);
    item3->Add( cKeepBlocks, 0, wxALIGN_CENTRE|wxALL, 5 );
    item3->Add( 5, 5, 0, wxALIGN_CENTRE, 5 );

    wxStaticText *item31 = new wxStaticText( panel, ID_TEXT, "Allow long gaps between frozen blocks:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item31, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );
    cLongGaps = new wxCheckBox( panel, ID_C_GAPS, "", wxDefaultPosition, wxDefaultSize, 0 );
    cLongGaps->SetValue(init.allowLongGaps);
    item3->Add( cLongGaps, 0, wxALIGN_CENTRE|wxALL, 5 );
    item3->Add( 5, 5, 0, wxALIGN_CENTRE, 5 );

    item1->Add( item3, 0, wxALIGN_CENTRE, 5 );

    item0->Add( item1, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxBoxSizer *item25 = new wxBoxSizer( wxHORIZONTAL );

    wxButton *item26 = new wxButton( panel, ID_B_OK, "OK", wxDefaultPosition, wxDefaultSize, 0 );
    item25->Add( item26, 0, wxALIGN_CENTRE|wxALL, 5 );

    item25->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item27 = new wxButton( panel, ID_B_CANCEL, "Cancel", wxDefaultPosition, wxDefaultSize, 0 );
    item25->Add( item27, 0, wxALIGN_CENTRE|wxALL, 5 );

    item0->Add( item25, 0, wxALIGN_CENTRE|wxALL, 5 );

    panel->SetAutoLayout(true);
    panel->SetSizer(item0);
    item0->Fit(this);
    item0->Fit(panel);
    item0->SetSizeHints(this);
}

BlockAlignerOptionsDialog::~BlockAlignerOptionsDialog(void)
{
    delete iSingle;
    delete iMultiple;
    delete iExtend;
    delete iDataLen;
    delete iSearchLen;
    delete fpPercent;
    delete fpLambda;
    delete fpK;
}

bool BlockAlignerOptionsDialog::GetValues(BlockAligner::BlockAlignerOptions *options)
{
    options->globalAlignment = cGlobal->IsChecked();
    options->mergeAfterEachSequence = cMerge->IsChecked();
    options->keepExistingBlocks = cKeepBlocks->IsChecked();
    options->allowLongGaps = cLongGaps->IsChecked();
    return (
        iSingle->GetInteger(&(options->singleBlockThreshold)) &&
        iMultiple->GetInteger(&(options->multipleBlockThreshold)) &&
        iExtend->GetInteger(&(options->allowedGapExtension)) &&
        iDataLen->GetInteger(&(options->databaseLength)) &&
        iSearchLen->GetInteger(&(options->searchSpaceLength)) &&
        fpPercent->GetDouble(&(options->gapLengthPercentile)) &&
        fpLambda->GetDouble(&(options->lambda)) &&
        fpK->GetDouble(&(options->K))
    );
}

void BlockAlignerOptionsDialog::OnCloseWindow(wxCloseEvent& event)
{
    EndModal(wxCANCEL);
}

void BlockAlignerOptionsDialog::OnButton(wxCommandEvent& event)
{
    if (event.GetId() == ID_B_OK) {
		BlockAligner::BlockAlignerOptions dummy;
        if (GetValues(&dummy))  // can't successfully quit if values aren't valid
            EndModal(wxOK);
        else
            wxBell();
    } else if (event.GetId() == ID_B_CANCEL) {
        EndModal(wxCANCEL);
    } else {
        event.Skip();
    }
}

END_SCOPE(Cn3D)

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.26  2003/07/14 18:35:27  thiessen
* run DP and Alejandro's block aligners, and compare results
*
* Revision 1.25  2003/03/27 18:45:59  thiessen
* update blockaligner code
*
* Revision 1.24  2003/02/03 19:20:02  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.23  2003/01/23 20:03:05  thiessen
* add BLAST Neighbor algorithm
*
* Revision 1.22  2003/01/22 14:47:30  thiessen
* cache PSSM in BlockMultipleAlignment
*
* Revision 1.21  2002/12/20 02:51:46  thiessen
* fix Prinf to self problems
*
* Revision 1.20  2002/12/09 16:25:04  thiessen
* allow negative score threshholds
*
* Revision 1.19  2002/11/06 00:18:10  thiessen
* fixes for new CRef/const rules in objects
*
* Revision 1.18  2002/09/30 17:13:02  thiessen
* change structure import to do sequences as well; change cache to hold mimes; change block aligner vocabulary; fix block aligner dialog bugs
*
* Revision 1.17  2002/09/23 19:12:32  thiessen
* add option to allow long gaps between frozen blocks
*
* Revision 1.16  2002/09/21 12:36:28  thiessen
* add frozen block position validation; add select-other-by-distance
*
* Revision 1.15  2002/09/20 17:48:39  thiessen
* fancier trace statements
*
* Revision 1.14  2002/09/19 14:09:41  thiessen
* position options dialog higher up
*
* Revision 1.13  2002/09/19 12:51:08  thiessen
* fix block aligner / update bug; add distance select for other molecules only
*
* Revision 1.12  2002/09/16 21:24:58  thiessen
* add block freezing to block aligner
*
* Revision 1.11  2002/08/15 22:13:13  thiessen
* update for wx2.3.2+ only; add structure pick dialog; fix MultitextDialog bug
*
* Revision 1.10  2002/08/14 00:02:08  thiessen
* combined block/global aligner from Alejandro
*
* Revision 1.8  2002/08/09 18:24:08  thiessen
* improve/add magic formula to avoid Windows symbol clashes
*
* Revision 1.7  2002/08/04 21:41:05  thiessen
* fix GetObject problem
*
* Revision 1.6  2002/08/01 12:51:36  thiessen
* add E-value display to block aligner
*
* Revision 1.5  2002/08/01 01:55:16  thiessen
* add block aligner options dialog
*
* Revision 1.4  2002/07/29 19:22:46  thiessen
* another blockalign bug fix; set better parameters to block aligner
*
* Revision 1.3  2002/07/29 13:25:42  thiessen
* add range restriction to block aligner
*
* Revision 1.2  2002/07/27 12:29:51  thiessen
* fix block aligner crash
*
* Revision 1.1  2002/07/26 15:28:45  thiessen
* add Alejandro's block alignment algorithm
*
*/
