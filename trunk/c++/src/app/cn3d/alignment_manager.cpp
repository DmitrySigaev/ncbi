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
* Revision 1.16  2000/10/12 16:22:45  thiessen
* working block split
*
* Revision 1.15  2000/10/12 02:14:56  thiessen
* working block boundary editing
*
* Revision 1.14  2000/10/05 18:34:43  thiessen
* first working editing operation
*
* Revision 1.13  2000/10/04 17:41:29  thiessen
* change highlight color (cell background) handling
*
* Revision 1.12  2000/10/02 23:25:19  thiessen
* working sequence identifier window in sequence viewer
*
* Revision 1.11  2000/09/20 22:22:26  thiessen
* working conservation coloring; split and center unaligned justification
*
* Revision 1.10  2000/09/15 19:24:22  thiessen
* allow repeated structures w/o different local id
*
* Revision 1.9  2000/09/12 01:47:38  thiessen
* fix minor but obscure bug
*
* Revision 1.8  2000/09/11 22:57:31  thiessen
* working highlighting
*
* Revision 1.7  2000/09/11 14:06:28  thiessen
* working alignment coloring
*
* Revision 1.6  2000/09/11 01:46:13  thiessen
* working messenger for sequence<->structure window communication
*
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
#include "cn3d/molecule.hpp"
#include "cn3d/messenger.hpp"
#include "cn3d/style_manager.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/conservation_colorer.hpp"
#include "cn3d/sequence_viewer.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

///// AlignmentManager methods /////

AlignmentManager::AlignmentManager(const SequenceSet *sSet, AlignmentSet *aSet,
    Messenger *mesg) :
    sequenceSet(sSet), alignmentSet(aSet), currentMultipleAlignment(NULL),
    messenger(mesg), sequenceViewer(NULL)
{
    NewAlignments(sSet, aSet);
}

void AlignmentManager::NewAlignments(const SequenceSet *sSet, AlignmentSet *aSet)
{
    // create a sequence viewer for this alignment
    if (!sequenceViewer) {
        sequenceViewer = new SequenceViewer(messenger);
        messenger->AddSequenceViewer(sequenceViewer);
    }

    if (!alignmentSet) {
        sequenceViewer->DisplaySequences(&(sequenceSet->sequences));
        return;
    }

	// remove old alignment
	if (currentMultipleAlignment) delete currentMultipleAlignment;

    // for testing, make a multiple with all rows
    AlignmentList alignments;
    AlignmentSet::AlignmentList::const_iterator a, ae=alignmentSet->alignments.end();
    for (a=alignmentSet->alignments.begin(); a!=ae; a++) alignments.push_back(*a);
    currentMultipleAlignment = CreateMultipleFromPairwiseWithIBM(alignments);

    sequenceViewer->DisplayAlignment(currentMultipleAlignment);
}

AlignmentManager::~AlignmentManager(void)
{
    if (currentMultipleAlignment) delete currentMultipleAlignment;
    messenger->RemoveSequenceViewer(sequenceViewer);
    delete sequenceViewer;
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

BlockMultipleAlignment *
AlignmentManager::CreateMultipleFromPairwiseWithIBM(const AlignmentList& alignments)
{
    AlignmentList::const_iterator a, ae = alignments.end();

    // create sequence list; fill with sequences of master + slaves
    BlockMultipleAlignment::SequenceList
        *sequenceList = new BlockMultipleAlignment::SequenceList(alignments.size() + 1);
    BlockMultipleAlignment::SequenceList::iterator s = sequenceList->begin();
    *(s++) = alignmentSet->master;
    for (a=alignments.begin(); a!=ae; a++, s++) *s = (*a)->slave;
    BlockMultipleAlignment *multipleAlignment = new BlockMultipleAlignment(sequenceList, messenger);

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
        multipleAlignment->AddAlignedBlockAtEnd(newBlock);

        // start looking for next block
        masterFrom = masterTo + 1;
    }

    if (!multipleAlignment->AddUnalignedBlocks() ||
        !multipleAlignment->UpdateBlockMapAndConservationColors()) {
        ERR_POST(Error << "AlignmentManager::CreateMultipleFromPairwiseWithIBM() - "
            "error finalizing alignment");
        return NULL;
    }

    return multipleAlignment;
}


///// BlockMultipleAlignment methods /////

BlockMultipleAlignment::BlockMultipleAlignment(const SequenceList *sequenceList, Messenger *mesg) :
    sequences(sequenceList), currentJustification(eRight),
    messenger(mesg), conservationColorer(NULL)
{
    InitCache();
    UnemphasizeBlocks();

    // create conservation colorer
    conservationColorer = new ConservationColorer();
}

void BlockMultipleAlignment::InitCache(void)
{
    prevRow = -1;
    prevBlock = NULL;
    blockIterator = blocks.begin();
}

BlockMultipleAlignment::~BlockMultipleAlignment(void)
{
    if (sequences) delete sequences;

    BlockList::iterator i, ie = blocks.end();
    for (i=blocks.begin(); i!=ie; i++) if (*i) delete *i;
}

bool BlockMultipleAlignment::CheckAlignedBlock(const Block *block) const
{
    if (!block || !block->IsAligned()) {
        ERR_POST("MultipleAlignment::CheckAlignedBlock() - checks aligned blocks only");
        return false;
    }
    if (block->NSequences() != sequences->size()) {
        ERR_POST("MultipleAlignment::CheckAlignedBlock() - block size mismatch");
        return false;
    }

    // make sure ranges are reasonable for each sequence
    int row;
    const Block
        *prevBlock = GetBlockBefore(block),
        *nextBlock = GetBlockAfter(block);
    const Block::Range *range, *prevRange = NULL, *nextRange = NULL;
    SequenceList::const_iterator sequence = sequences->begin();
    for (row=0; row<block->NSequences(); row++, sequence++) {
        range = block->GetRangeOfRow(row);
        if (prevBlock) prevRange = prevBlock->GetRangeOfRow(row);
        if (nextBlock) nextRange = nextBlock->GetRangeOfRow(row);
        if (range->to - range->from + 1 != block->width ||       // check block width
            (prevRange && range->from <= prevRange->to) ||          // check for range overlap
            (nextRange && range->to >= nextRange->from) ||          // check for range overlap
            range->from > range->to ||                              // check range values
            range->to >= (*sequence)->Length()) {                   // check bounds of end
            ERR_POST(Error << "MultipleAlignment::CheckAlignedBlock() - range error");
            return false;
        }
    }

    return true;
}

bool BlockMultipleAlignment::AddAlignedBlockAtEnd(UngappedAlignedBlock *newBlock)
{
    blocks.push_back(newBlock);
    return CheckAlignedBlock(newBlock);
}

Block * BlockMultipleAlignment::
    CreateNewUnalignedBlockBetween(const Block *leftBlock, const Block *rightBlock)
{
    if ((leftBlock && !leftBlock->IsAligned()) || 
        (rightBlock && !rightBlock->IsAligned())) {
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

bool BlockMultipleAlignment::AddUnalignedBlocks(void)
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

    TESTMSG("alignment display size: " << totalWidth << " x " << NRows());
    return true;
}

bool BlockMultipleAlignment::UpdateBlockMapAndConservationColors(void)
{
    int i = 0, j;
    BlockList::iterator a, ae = blocks.end();
    conservationColorer->Clear();

    // fill out the block map
    blockMap.resize(totalWidth);
    for (a=blocks.begin(); a!=ae; a++) {
        for (j=0; j<(*a)->width; j++, i++) {
            blockMap[i].block = *a;
            blockMap[i].blockColumn = j;
        }
        UngappedAlignedBlock *uaBlock = dynamic_cast<UngappedAlignedBlock*>(*a);
        if (uaBlock) conservationColorer->AddBlock(uaBlock);
    }

    // do conservation coloring
    conservationColorer->CalculateConservationColors();

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

    if (sequence->molecule && seqIndex >= 0) {
        *color = sequence->molecule->GetResidueColor(seqIndex);
    } else {
        if (isAligned)
            *color = *GetAlignmentColor(row, seqIndex);
        else
            color->Set(.4, .4, .4);  // dark gray
    }

    if (seqIndex >= 0)
        *isHighlighted = messenger->IsHighlighted(sequence, seqIndex);
    else
        *isHighlighted = false;

    return true;
}

bool BlockMultipleAlignment::GetSequenceAndIndexAt(int alignmentColumn, int row,
        const Sequence **sequence, int *index, bool *isAligned) const
{
    *sequence = sequences->at(row);

    const BlockInfo& blockInfo = blockMap[alignmentColumn];
    eUnalignedJustification justification;

    if (!blockInfo.block->IsAligned()) {
        *isAligned = false;
        if (blockInfo.block == blocks.back()) // also true if there's a single aligned block
            justification = eLeft;
        else if (blockInfo.block == blocks.front())
            justification = eRight;
        else
            justification = currentJustification;
    } else
        *isAligned = true;

    *index = blockInfo.block->GetIndexAt(blockInfo.blockColumn, row, justification);

    return true;
}

int BlockMultipleAlignment::GetRowForSequence(const Sequence *sequence) const
{
    // this only works for structured sequences, since non-structure sequences can
    // be repeated any number of times in the alignment; assumes repeated structures 
    // will each have a unique Sequence object
    if (!sequence || !sequence->molecule) {
        ERR_POST(Error << "BlockMultipleAlignment::GetRowForSequence() - Sequence must have associated structure");
        return -1; 
    }

    if (prevRow < 0 || sequence != sequences->at(prevRow)) {
        for (int row=0; row<NRows(); row++) if (sequences->at(row) == sequence) break;
        if (row == NRows()) {
            ERR_POST(Error << "BlockMultipleAlignment::GetRowForSequence() - can't find given Sequence");
            return -1;
        }
        (const_cast<BlockMultipleAlignment*>(this))->prevRow = row;
    }
    return prevRow;
}

bool BlockMultipleAlignment::IsAligned(const Sequence *sequence, int seqIndex) const
{
    int row = GetRowForSequence(sequence);
    if (row < 0) return false;

    const Block *block = GetBlock(row, seqIndex);
    if (block && block->IsAligned())
        return true;
    else
        return false;
}

const Vector * BlockMultipleAlignment::GetAlignmentColor(int row, int seqIndex) const
{
    static const Vector noColor(0,0,0); // black

    const UngappedAlignedBlock *block = dynamic_cast<const UngappedAlignedBlock*>(GetBlock(row, seqIndex));
    if (!block || !block->IsAligned()) {
        ERR_POST(Warning << "BlockMultipleAlignment::GetAlignmentColor() called on unaligned residue");
        return &noColor;
    }

    const Sequence *sequence = sequences->at(row);
    StyleSettings::eColorScheme colorScheme;
    if (sequence->molecule) {
        const StructureObject *object;
        if (!sequence->molecule->GetParentOfType(&object)) return &noColor;
        // assume residueID is seqIndex + 1
        colorScheme = sequence->parentSet->styleManager->
            GetStyleForResidue(object, sequence->molecule->id, seqIndex + 1).proteinBackbone.colorScheme;
    } else {
        colorScheme = sequence->parentSet->styleManager->GetGlobalStyle().proteinBackbone.colorScheme;
    }

    const Vector *alignedColor;

    switch (colorScheme) {
        case StyleSettings::eAligned:
            alignedColor = &ConservationColorer::MaximumConservationColor;
            break;
        case StyleSettings::eIdentity:
            alignedColor = conservationColorer->
                GetIdentityColor(block, seqIndex - block->GetRangeOfRow(row)->from);
            break;
        case StyleSettings::eVariety:
            alignedColor = conservationColorer->
                GetVarietyColor(block, seqIndex - block->GetRangeOfRow(row)->from);
            break;
        case StyleSettings::eWeightedVariety:
            alignedColor = conservationColorer->
                GetWeightedVarietyColor(block, seqIndex - block->GetRangeOfRow(row)->from);
            break;
        default:
            alignedColor = &noColor;
    }

    return alignedColor;
}

const Vector * BlockMultipleAlignment::GetAlignmentColor(const Sequence *sequence, int seqIndex) const
{
    int row = GetRowForSequence(sequence);
    if (row < 0) return NULL;
    return GetAlignmentColor(row, seqIndex);
}

const Block * BlockMultipleAlignment::GetBlock(int row, int seqIndex) const
{
    // make sure we're in range for this sequence
    if (seqIndex < 0 || seqIndex >= sequences->at(row)->sequenceString.size()) {
        ERR_POST(Error << "BlockMultipleAlignment::GetBlock() - seqIndex out of range");
        return false;
    }
    
    const Block::Range *range;

    // first check to see if it's in the same block as last time. (Yes, there are
    // a lot of const_casts... but this ugliness is unfortunately necessary to be able
    // to call this function on const objects, while still being able to change
    // the cache values.)
    if (prevBlock) {
        range = prevBlock->GetRangeOfRow(row);
        if (seqIndex >= range->from && seqIndex <= range->to)
            return prevBlock;
        (const_cast<BlockMultipleAlignment*>(this))->blockIterator++; // start search at next block
    } else {
        (const_cast<BlockMultipleAlignment*>(this))->blockIterator = blocks.begin();
    }

    // otherwise, perform block search. This search is most efficient when queries
    // happen in order from left to right along a given row.
    do {
        if (blockIterator == blocks.end())
            (const_cast<BlockMultipleAlignment*>(this))->blockIterator = blocks.begin();
        range = (*blockIterator)->GetRangeOfRow(row);
        if (seqIndex >= range->from && seqIndex <= range->to) {
            (const_cast<BlockMultipleAlignment*>(this))->prevBlock = *blockIterator; // cache this block
            return prevBlock;
        }
        (const_cast<BlockMultipleAlignment*>(this))->blockIterator++;
    } while (1);
}

int BlockMultipleAlignment::GetFirstAlignedBlockPosition(void) const
{
    BlockList::const_iterator b = blocks.begin();
    if (blocks.size() > 0 && (*b)->IsAligned()) // first block is aligned
        return 0;
    else if (blocks.size() >= 2 && (*(++b))->IsAligned()) // second block is aligned
        return blocks.front()->width;
    else
        return -1;
}

void BlockMultipleAlignment::SelectedRange(int row, int from, int to) const
{
    // translate from,to (alignment columns) into sequence indexes
    const Sequence *sequence;
    int fromIndex, toIndex;
    bool ignored;

    // find first residue within range
    while (from <= to) {
        GetSequenceAndIndexAt(from, row, &sequence, &fromIndex, &ignored);
        if (fromIndex >= 0) break;
        from++;
    }
    if (from > to) return;

    // find last residue within range
    while (to >= from) {
        GetSequenceAndIndexAt(to, row, &sequence, &toIndex, &ignored);
        if (toIndex >= 0) break;
        to--;
    }
        
    messenger->AddHighlights(sequence, fromIndex, toIndex);
}

void BlockMultipleAlignment::GetAlignedBlockPosition(int alignmentIndex, 
    int *blockColumn, int *blockWidth) const
{
    *blockColumn = *blockWidth = -1;
    const BlockInfo& info = blockMap[alignmentIndex];
    if (info.block->IsAligned()) {
        *blockColumn = info.blockColumn;
        *blockWidth = info.block->width;
    }
}

Block * BlockMultipleAlignment::GetBlockBefore(const Block *block) const
{
    Block *prevBlock = NULL;
    BlockList::const_iterator b, be = blocks.end();
    for (b=blocks.begin(); b!=be; b++) {
        if (*b == block) break;
        prevBlock = *b;
    }
    return prevBlock;
}

Block * BlockMultipleAlignment::GetBlockAfter(const Block *block) const
{
    BlockList::const_iterator b, be = blocks.end();
    for (b=blocks.begin(); b!=be; b++) {
        if (*b == block) {
            b++;
            if (b == be) break;
            return *b;
        }
    }
    return NULL;
}

void BlockMultipleAlignment::InsertBlockBefore(Block *newBlock, const Block *insertAt)
{
    BlockList::iterator b, be = blocks.end();
    for (b=blocks.begin(); b!=be; b++) {
        if (*b == insertAt) {
            blocks.insert(b, newBlock);
            return;
        }
    }
    ERR_POST(Warning << "BlockMultipleAlignment::InsertBlockBefore() - couldn't find insertAt block");
}

void BlockMultipleAlignment::InsertBlockAfter(const Block *insertAt, Block *newBlock)
{
    BlockList::iterator b, be = blocks.end();
    for (b=blocks.begin(); b!=be; b++) {
        if (*b == insertAt) {
            b++;
            blocks.insert(b, newBlock);
            return;
        }
    }
    ERR_POST(Warning << "BlockMultipleAlignment::InsertBlockBefore() - couldn't find insertAt block");
}

void BlockMultipleAlignment::RemoveBlock(Block *block)
{
    BlockList::iterator b, be = blocks.end();
    for (b=blocks.begin(); b!=be; b++) {
        if (*b == block) {
            blocks.erase(b);
            delete *b;
            InitCache();
            return;
        }
    }
    ERR_POST(Warning << "BlockMultipleAlignment::RemoveBlock() - couldn't find block");
}

bool BlockMultipleAlignment::MoveBlockBoundary(int columnFrom, int columnTo)
{
    int blockColumn, blockWidth;
    GetAlignedBlockPosition(columnFrom, &blockColumn, &blockWidth);
    if (blockColumn < 0 || blockWidth <= 0) return false;

    TESTMSG("trying to move block boundary from " << columnFrom << " to " << columnTo);

    const BlockInfo& info = blockMap[columnFrom];
    int row, requestedShift = columnTo - columnFrom, actualShift = 0;
    const Block::Range *range;

    // shrink block from left
    if (blockColumn == 0 && requestedShift > 0 && requestedShift < info.block->width) {
        actualShift = requestedShift;
        TESTMSG("shrinking block from left");
        for (row=0; row<NRows(); row++) {
            range = info.block->GetRangeOfRow(row);
            info.block->SetRangeOfRow(row, range->from + requestedShift, range->to);
        }
        info.block->width -= requestedShift;
        Block *prevBlock = GetBlockBefore(info.block);
        if (prevBlock && !prevBlock->IsAligned()) {
            for (row=0; row<NRows(); row++) {
                range = prevBlock->GetRangeOfRow(row);
                prevBlock->SetRangeOfRow(row, range->from, range->to + requestedShift);
            }
            prevBlock->width += requestedShift;
        } else {
            Block *newUnalignedBlock = CreateNewUnalignedBlockBetween(prevBlock, info.block);
            if (newUnalignedBlock) InsertBlockBefore(newUnalignedBlock, info.block);
            TESTMSG("added new unaligned block");
        }
    }

    // shrink block from right (requestedShift < 0)
    else if (blockColumn == info.block->width - 1 &&
                requestedShift < 0 && requestedShift > -info.block->width) {
        actualShift = requestedShift;
        TESTMSG("shrinking block from right");
        for (row=0; row<NRows(); row++) {
            range = info.block->GetRangeOfRow(row);
            info.block->SetRangeOfRow(row, range->from, range->to + requestedShift);
        }
        info.block->width += requestedShift;
        Block *nextBlock = GetBlockAfter(info.block);
        if (nextBlock && !nextBlock->IsAligned()) {
            for (row=0; row<NRows(); row++) {
                range = nextBlock->GetRangeOfRow(row);
                nextBlock->SetRangeOfRow(row, range->from + requestedShift, range->to);
            }
            nextBlock->width -= requestedShift;
        } else {
            Block *newUnalignedBlock = CreateNewUnalignedBlockBetween(info.block, nextBlock);
            if (newUnalignedBlock) InsertBlockAfter(info.block, newUnalignedBlock);
            TESTMSG("added new unaligned block");
        }
    }

    // grow block to right
    else if (blockColumn == info.block->width - 1 && requestedShift > 0) {
        Block *nextBlock = GetBlockAfter(info.block);
        if (nextBlock && !nextBlock->IsAligned()) {
            int nRes;
            actualShift = requestedShift;
            for (row=0; row<NRows(); row++) {
                range = nextBlock->GetRangeOfRow(row);
                nRes = range->to - range->from + 1;
                if (nRes < actualShift) actualShift = nRes;
            }
            if (actualShift) {
                TESTMSG("growing block to right");
                for (row=0; row<NRows(); row++) {
                    range = info.block->GetRangeOfRow(row);
                    info.block->SetRangeOfRow(row, range->from, range->to + actualShift);
                    range = nextBlock->GetRangeOfRow(row);
                    nextBlock->SetRangeOfRow(row, range->from + actualShift, range->to);
                }
                info.block->width += actualShift;
                nextBlock->width -= actualShift;
                if (nextBlock->width == 0) {
                    RemoveBlock(nextBlock);
                    TESTMSG("removed empty block");
                }
            }
        }
    }

    // grow block to left (requestedShift < 0)
    else if (blockColumn == 0 && requestedShift < 0) {
        Block *prevBlock = GetBlockBefore(info.block);
        if (prevBlock && !prevBlock->IsAligned()) {
            int nRes;
            actualShift = requestedShift;
            for (row=0; row<NRows(); row++) {
                range = prevBlock->GetRangeOfRow(row);
                nRes = range->to - range->from + 1;
                if (nRes < -actualShift) actualShift = -nRes;
            }
            if (actualShift) {
                TESTMSG("growing block to left");
                for (row=0; row<NRows(); row++) {
                    range = info.block->GetRangeOfRow(row);
                    info.block->SetRangeOfRow(row, range->from + actualShift, range->to);
                    range = prevBlock->GetRangeOfRow(row);
                    prevBlock->SetRangeOfRow(row, range->from, range->to + actualShift);
                }
                info.block->width -= actualShift;
                prevBlock->width += actualShift;
                if (prevBlock->width == 0) {
                    RemoveBlock(prevBlock);
                    TESTMSG("removed empty block");
                }
            }
        }
    }

    if (actualShift != 0) {
        UpdateBlockMapAndConservationColors();
        messenger->PostRedrawSequenceViewers();
        return true;
    } else
        return false;
}

bool BlockMultipleAlignment::SplitBlock(int alignmentIndex)
{
    const BlockInfo& info = blockMap[alignmentIndex];
    if (!info.block->IsAligned() || info.block->width < 2 || info.blockColumn == 0)
        return false;

    UngappedAlignedBlock *newAlignedBlock = new UngappedAlignedBlock(sequences);
    newAlignedBlock->width = info.block->width - info.blockColumn;
    info.block->width = info.blockColumn;

    const Block::Range *range;
    int oldTo;
    for (int row=0; row < info.block->NSequences(); row++) {
        range = info.block->GetRangeOfRow(row);
        oldTo = range->to;
        info.block->SetRangeOfRow(row, range->from, range->from + info.block->width - 1);
        newAlignedBlock->SetRangeOfRow(row, oldTo - newAlignedBlock->width + 1, oldTo);
    }

    InsertBlockAfter(info.block, newAlignedBlock);
    if (!CheckAlignedBlock(info.block) || !CheckAlignedBlock(newAlignedBlock))
        ERR_POST(Error << "BlockMultipleAlignment::SplitBlock() - split failed to create valid blocks");

    UpdateBlockMapAndConservationColors();
    messenger->PostRedrawSequenceViewers();
    return true;
}

void BlockMultipleAlignment::EmphasizeBlock(int alignmentIndex, int row)
{
    const Block *block = blockMap[alignmentIndex].block;
    emphasizedBlock2 = emphasizedBlock1;
    emphasizedBlock1 = block;
}


///// UngappedAlignedBlock methods /////

char UngappedAlignedBlock::GetCharacterAt(int blockColumn, int row) const
{
    return sequences->at(row)->sequenceString[GetIndexAt(blockColumn, row)];
}


///// UnalignedBlock methods /////

int UnalignedBlock::GetIndexAt(int blockColumn, int row,
        BlockMultipleAlignment::eUnalignedJustification justification) const
{
    const Block::Range *range = GetRangeOfRow(row);
    int seqIndex, rangeWidth, extraSpace;

    switch (justification) {
        case BlockMultipleAlignment::eLeft:
            seqIndex = range->from + blockColumn;
            break;
        case BlockMultipleAlignment::eRight:
            seqIndex = range->to - width + blockColumn + 1;
            break;
        case BlockMultipleAlignment::eCenter:
            rangeWidth = (range->to - range->from + 1);
            extraSpace = (width - rangeWidth) / 2;
            if (blockColumn < extraSpace || blockColumn >= extraSpace + rangeWidth)
                seqIndex = -1;
            else
                seqIndex = range->from + blockColumn - extraSpace;
            break;
        case BlockMultipleAlignment::eSplit:
            rangeWidth = (range->to - range->from + 1);
            extraSpace = width - rangeWidth;
            if (blockColumn < rangeWidth / 2)
                seqIndex = range->from + blockColumn;
            else if (blockColumn >= extraSpace + rangeWidth / 2)
                seqIndex = range->to - width + blockColumn + 1;
            else
                seqIndex = -1;
            break;
    }
    if (seqIndex < range->from || seqIndex > range->to) seqIndex = -1;

    return seqIndex;
}


END_SCOPE(Cn3D)

