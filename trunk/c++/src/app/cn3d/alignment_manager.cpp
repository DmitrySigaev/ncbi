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
*      Classes to manipulate alignments
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.5  2000/09/08 20:16:55  thiessen
* working dynamic alignment views
*
* Revision 1.4  2000/09/03 18:46:47  thiessen
* working generalized sequence viewer
*
* Revision 1.3  2000/08/30 23:46:26  thiessen
* working alignment display
*
* Revision 1.2  2000/08/30 19:48:41  thiessen
* working sequence window
*
* Revision 1.1  2000/08/29 04:34:35  thiessen
* working alignment manager, IBM
*
* ===========================================================================
*/

#include <corelib/ncbidiag.hpp>

#include "cn3d/alignment_manager.hpp"
#include "cn3d/sequence_set.hpp"
#include "cn3d/alignment_set.hpp"
#include "cn3d/sequence_viewer.hpp"
#include "cn3d/molecule.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

AlignmentManager::AlignmentManager(const SequenceSet *sSet, const AlignmentSet *aSet,
    SequenceViewer * const *seqViewer) :
    sequenceSet(sSet), alignmentSet(aSet), currentMultipleAlignment(NULL),
    sequenceViewer(seqViewer)
{
    if (!alignmentSet) {
        (*sequenceViewer)->DisplaySequences(&(sequenceSet->sequences));
        return;
    }

    // for testing, make a multiple with all rows
    AlignmentList alignments;
    AlignmentSet::AlignmentList::const_iterator a, ae=alignmentSet->alignments.end();
    for (a=alignmentSet->alignments.begin(); a!=ae; a++) alignments.push_back(*a);
    CreateMultipleFromPairwiseWithIBM(alignments);

    (*sequenceViewer)->DisplayAlignment(currentMultipleAlignment);
}

AlignmentManager::~AlignmentManager(void)
{
    if (currentMultipleAlignment) delete currentMultipleAlignment;
    (*sequenceViewer)->ClearGUI();
}

static bool AlignedToAllSlaves(int masterResidue,
    const AlignmentManager::AlignmentList& alignments)
{
	AlignmentManager::AlignmentList::const_iterator a, ae = alignments.end();
    for (a=alignments.begin(); a!=ae; a++) {
        if ((*a)->masterToSlave[masterResidue] == -1) return false;
    }
    return true;
}

static bool NoSlaveInsertionsBetween(int masterFrom, int masterTo,
    const AlignmentManager::AlignmentList& alignments)
{
	AlignmentManager::AlignmentList::const_iterator a, ae = alignments.end();
    for (a=alignments.begin(); a!=ae; a++) {
        if (((*a)->masterToSlave[masterTo] - (*a)->masterToSlave[masterFrom]) !=
            (masterTo - masterFrom)) return false;
    }
    return true;
}

const BlockMultipleAlignment *
AlignmentManager::CreateMultipleFromPairwiseWithIBM(const AlignmentList& alignments)
{
    if (currentMultipleAlignment) delete currentMultipleAlignment;

    AlignmentList::const_iterator a, ae = alignments.end();

    // create sequence list; fill with sequences of master + slaves
    BlockMultipleAlignment::SequenceList
        *sequenceList = new BlockMultipleAlignment::SequenceList(alignments.size() + 1);
    BlockMultipleAlignment::SequenceList::iterator s = sequenceList->begin();
    *(s++) = alignmentSet->master;
    for (a=alignments.begin(); a!=ae; a++, s++) *s = (*a)->slave;
    currentMultipleAlignment = new BlockMultipleAlignment(sequenceList);

    // each block is a continuous region on the master, over which each master
    // residue is aligned to a residue of each slave, and where there are no
    // insertions relative to the master in any of the slaves
    int masterFrom = 0, masterTo, row;
    UngappedAlignedBlock *newBlock;

    while (masterFrom < alignmentSet->master->Length()) {

        // look for first all-aligned residue
        if (!AlignedToAllSlaves(masterFrom, alignments)) {
            masterFrom++;
            continue;
        }

        // find all next continuous all-aligned residues
        for (masterTo=masterFrom+1;
                masterTo < alignmentSet->master->Length() &&
                AlignedToAllSlaves(masterTo, alignments) &&
                NoSlaveInsertionsBetween(masterFrom, masterTo, alignments);
             masterTo++) ;
        masterTo--; // after loop, masterTo = first non-all-aligned residue

        // create new block with ranges from master and all slaves
        newBlock = new UngappedAlignedBlock(sequenceList);
        newBlock->SetRangeOfRow(0, masterFrom, masterTo);
        newBlock->width = masterTo - masterFrom + 1;

        //TESTMSG("masterFrom " << masterFrom+1 << ", masterTo " << masterTo+1);
        for (a=alignments.begin(), row=1; a!=ae; a++, row++) {
            newBlock->SetRangeOfRow(row, 
                (*a)->masterToSlave[masterFrom],
                (*a)->masterToSlave[masterTo]);

            //TESTMSG("slave->from " << b->from+1 << ", slave->to " << b->to+1);
        }

        // copy new block into alignment
        currentMultipleAlignment->AddAlignedBlockAtEnd(newBlock);

        // start looking for next block
        masterFrom = masterTo + 1;
    }

    if (!currentMultipleAlignment->AddUnalignedBlocksAndIndex()) {
        ERR_POST(Error << "AlignmentManager::CreateMultipleFromPairwiseWithIBM() - "
            "AddUnalignedBlocksAndIndex() failed");
        return NULL;
    }

    return currentMultipleAlignment;
}


BlockMultipleAlignment::BlockMultipleAlignment(const SequenceList *sequenceList) :
    sequences(sequenceList), currentJustification(eRight)
{
}

BlockMultipleAlignment::~BlockMultipleAlignment(void)
{
    if (sequences) delete sequences;

    BlockList::iterator i, ie = blocks.end();
    for (i=blocks.begin(); i!=ie; i++) if (*i) delete *i;
}

bool BlockMultipleAlignment::AddAlignedBlockAtEnd(Block *newBlock)
{
    if (!newBlock || !newBlock->isAligned) {
        ERR_POST("MultipleAlignment::AddAlignedBlockAtEnd() - add aligned blocks only");
        return false;
    }
    if (newBlock->NSequences() != sequences->size()) {
        ERR_POST("MultipleAlignment::AddAlignedBlockAtEnd() - block size mismatch");
        return false;
    }

    // make sure ranges are reasonable for each sequence
    int row;
    const Block::Range *range, *prevRange = NULL;
    SequenceList::const_iterator sequence = sequences->begin();
    for (row=0; row<newBlock->NSequences(); row++, sequence++) {
        range = newBlock->GetRangeOfRow(row);
        if (blocks.size() > 0) prevRange = blocks.back()->GetRangeOfRow(row);
        if (range->to - range->from + 1 != newBlock->width ||       // check block width
            (prevRange && range->from <= prevRange->to) ||          // check for range overlap
            range->from > range->to ||                              // check range values
            range->to >= (*sequence)->Length()) {                   // check bounds of end
            ERR_POST(Error << "MultipleAlignment::AddAlignedBlockAtEnd() - range error");
            return false;
        }
    }

    blocks.push_back(newBlock);
    return true;
}

Block * BlockMultipleAlignment::
    CreateNewUnalignedBlockBetween(const Block *leftBlock, const Block *rightBlock)
{
    if ((leftBlock && !leftBlock->isAligned) || 
        (rightBlock && !rightBlock->isAligned)) {
        ERR_POST(Error << "CreateNewUnalignedBlockBetween() - passed an unaligned block");
        return NULL;
    }

    int row, from, to, length;
    SequenceList::const_iterator s, se = sequences->end();

    Block *newBlock = new UnalignedBlock(sequences);
    newBlock->width = 0;

    for (row=0, s=sequences->begin(); s!=se; row++, s++) {

        if (leftBlock)
            from = leftBlock->GetRangeOfRow(row)->to + 1;
        else
            from = 0;

        if (rightBlock)
            to = rightBlock->GetRangeOfRow(row)->from - 1;
        else
            to = (*s)->sequenceString.size() - 1;

        newBlock->SetRangeOfRow(row, from, to);

        length = to - from + 1;
        if (length < 0) { // just to make sure...
            ERR_POST(Error << "CreateNewUnalignedBlockBetween() - unaligned length < 0");
            return NULL;
        }
        if (length > newBlock->width) newBlock->width = length;
    }

    if (newBlock->width == 0) {
        delete newBlock;
        return NULL;
    } else
        return newBlock;
}

bool BlockMultipleAlignment::AddUnalignedBlocksAndIndex(void)
{
    BlockList::iterator a, ae = blocks.end();
    const Block *alignedBlock = NULL, *prevAlignedBlock = NULL;
    Block *newUnalignedBlock;

    totalWidth = 0;

    // unaligned blocks to the left of each aligned block
    for (a=blocks.begin(); a!=ae; a++) {
        alignedBlock = *a;
        totalWidth += alignedBlock->width;
        newUnalignedBlock = CreateNewUnalignedBlockBetween(prevAlignedBlock, alignedBlock);
        if (newUnalignedBlock) {
            blocks.insert(a, newUnalignedBlock);
            totalWidth += newUnalignedBlock->width;
        }
        prevAlignedBlock = alignedBlock;
    }

    // right tail
    newUnalignedBlock = CreateNewUnalignedBlockBetween(alignedBlock, NULL);
    if (newUnalignedBlock) {
        blocks.insert(a, newUnalignedBlock);
        totalWidth += newUnalignedBlock->width;
    }

    // now fill out the block map
    blockMap.resize(totalWidth);
    int i = 0, j;
    for (a=blocks.begin(); a!=ae; a++) {
        for (j=0; j<(*a)->width; j++, i++) {
            blockMap[i].block = *a;
            blockMap[i].blockColumn = j;
        }
    }

    TESTMSG("alignment display size: " << totalWidth << " x " << NSequences());
    return true;
}

bool BlockMultipleAlignment::GetCharacterTraitsAt(int alignmentColumn, int row,
    char *character, Vector *color, bool *isHighlighted) const
{
    const Sequence *sequence;
    int seqIndex;
    bool isAligned;
    
    if (!GetSequenceAndIndexAt(alignmentColumn, row, &sequence, &seqIndex, &isAligned))
        return false;

    *character = (seqIndex >= 0) ? sequence->sequenceString[seqIndex] : '~';
    if (isAligned)
        *character = toupper(*character);
    else
        *character = tolower(*character);

    if (sequence->molecule && seqIndex >= 0)
        *color = sequence->molecule->GetResidueColor(seqIndex);
    else
        color->Set(0,0,0);

    *isHighlighted = false;

    return true;
}

bool BlockMultipleAlignment::GetSequenceAndIndexAt(int alignmentColumn, int row,
        const Sequence **sequence, int *index, bool *isAligned) const
{
    *sequence = sequences->at(row);

    const BlockInfo& blockInfo = blockMap[alignmentColumn];
    eUnalignedJustification justification;

    if (!blockInfo.block->isAligned) {
        *isAligned = false;
        if (blockInfo.block == blocks.front())
            justification = eRight;
        else if (blockInfo.block == blocks.back())
            justification = eLeft;
        else
            justification = currentJustification;
    } else
        *isAligned = true;

    *index = blockInfo.block->GetIndexAt(blockInfo.blockColumn, row, justification);

    return true;
}

int UnalignedBlock::GetIndexAt(int blockColumn, int row,
        BlockMultipleAlignment::eUnalignedJustification justification) const
{
    const Block::Range *range = GetRangeOfRow(row);
    int seqIndex;

    switch (justification) {
        case BlockMultipleAlignment::eLeft:
            seqIndex = range->from + blockColumn;
            break;
        case BlockMultipleAlignment::eRight:
            seqIndex = range->to - width + blockColumn + 1;
            break;
        case BlockMultipleAlignment::eCenter:
            seqIndex = -1;
            break;
        case BlockMultipleAlignment::eSplit:
            seqIndex = -1;
            break;
    }
    if (seqIndex < range->from || seqIndex > range->to) seqIndex = -1;

    return seqIndex;
}

END_SCOPE(Cn3D)

