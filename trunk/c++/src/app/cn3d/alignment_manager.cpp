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
* Revision 1.70  2002/02/19 14:59:38  thiessen
* add CDD reject and purge sequence
*
* Revision 1.69  2002/02/12 17:19:20  thiessen
* first working structure import
*
* Revision 1.68  2001/12/06 23:13:43  thiessen
* finish import/align new sequences into single-structure data; many small tweaks
*
* Revision 1.67  2001/11/30 14:02:04  thiessen
* progress on sequence imports to single structures
*
* Revision 1.66  2001/11/27 16:26:05  thiessen
* major update to data management system
*
* Revision 1.65  2001/09/27 15:37:57  thiessen
* decouple sequence import and BLAST
*
* Revision 1.64  2001/06/21 02:02:33  thiessen
* major update to molecule identification and highlighting ; add toggle highlight (via alt)
*
* Revision 1.63  2001/06/07 19:05:37  thiessen
* functional (although incomplete) render settings panel ; highlight title - not sequence - upon mouse click
*
* Revision 1.62  2001/06/05 13:21:08  thiessen
* fix structure alignment list problems
*
* Revision 1.61  2001/06/02 17:22:45  thiessen
* fixes for GCC
*
* Revision 1.60  2001/05/31 18:47:05  thiessen
* add preliminary style dialog; remove LIST_TYPE; add thread single and delete all; misc tweaks
*
* Revision 1.59  2001/05/24 13:32:15  thiessen
* fix merge status reporting bug
*
* Revision 1.58  2001/05/15 23:48:35  thiessen
* minor adjustments to compile under Solaris/wxGTK
*
* Revision 1.57  2001/05/11 02:10:41  thiessen
* add better merge fail indicators; tweaks to windowing/taskbar
*
* Revision 1.56  2001/05/03 14:39:14  thiessen
* put ViewableAlignment in its own (non-wx) header
*
* Revision 1.55  2001/05/02 13:46:26  thiessen
* major revision of stuff relating to saving of updates; allow stored null-alignments
*
* Revision 1.54  2001/04/20 18:02:57  thiessen
* don't open update viewer right away
*
* Revision 1.53  2001/04/19 12:58:32  thiessen
* allow merge and delete of individual updates
*
* Revision 1.52  2001/04/17 20:15:38  thiessen
* load 'pending' Cdd alignments into update window
*
* Revision 1.51  2001/04/05 22:55:34  thiessen
* change bg color handling ; show geometry violations
*
* Revision 1.50  2001/04/04 00:27:13  thiessen
* major update - add merging, threader GUI controls
*
* Revision 1.49  2001/03/30 03:07:33  thiessen
* add threader score calculation & sorting
*
* Revision 1.48  2001/03/22 18:14:49  thiessen
* fix null-pointer error in alignment retrieval
*
* Revision 1.47  2001/03/22 00:33:16  thiessen
* initial threading working (PSSM only); free color storage in undo stack
*
* Revision 1.46  2001/03/17 14:06:48  thiessen
* more workarounds for namespace/#define conflicts
*
* Revision 1.45  2001/03/13 01:25:05  thiessen
* working undo system for >1 alignment (e.g., update window)
*
* Revision 1.44  2001/03/10 22:23:02  thiessen
* damn wx/string problem again
*
* Revision 1.43  2001/03/09 15:49:03  thiessen
* major changes to add initial update viewer
*
* Revision 1.42  2001/03/06 20:20:50  thiessen
* progress towards >1 alignment in a SequenceDisplay ; misc minor fixes
*
* Revision 1.41  2001/03/02 15:32:52  thiessen
* minor fixes to save & show/hide dialogs, wx string headers
*
* Revision 1.40  2001/03/01 20:15:50  thiessen
* major rearrangement of sequence viewer code into base and derived classes
*
* Revision 1.39  2001/02/16 00:40:01  thiessen
* remove unused sequences from asn data
*
* Revision 1.38  2001/02/13 01:03:55  thiessen
* backward-compatible domain ID's in output; add ability to delete rows
*
* Revision 1.37  2001/02/09 20:17:32  thiessen
* ignore atoms w/o alpha when doing structure realignment
*
* Revision 1.36  2001/02/08 23:01:48  thiessen
* hook up C-toolkit stuff for threading; working PSSM calculation
*
* Revision 1.35  2001/02/02 20:17:33  thiessen
* can read in CDD with multi-structure but no struct. alignments
*
* Revision 1.34  2001/01/30 20:51:18  thiessen
* minor fixes
*
* Revision 1.33  2001/01/18 19:37:28  thiessen
* save structure (re)alignments to asn output
*
* Revision 1.32  2000/12/29 19:23:38  thiessen
* save row order
*
* Revision 1.31  2000/12/26 16:47:36  thiessen
* preserve block boundaries
*
* Revision 1.30  2000/12/19 16:39:08  thiessen
* tweaks to show/hide
*
* Revision 1.29  2000/12/15 15:51:46  thiessen
* show/hide system installed
*
* Revision 1.28  2000/11/30 15:49:34  thiessen
* add show/hide rows; unpack sec. struc. and domain features
*
* Revision 1.27  2000/11/17 19:48:13  thiessen
* working show/hide alignment row
*
* Revision 1.26  2000/11/13 18:06:52  thiessen
* working structure re-superpositioning
*
* Revision 1.25  2000/11/12 04:02:59  thiessen
* working file save including alignment edits
*
* Revision 1.24  2000/11/11 21:15:53  thiessen
* create Seq-annot from BlockMultipleAlignment
*
* Revision 1.23  2000/11/09 18:14:40  vasilche
* Fixed nonstandard behaviour of 'for' statement on MS VC.
*
* Revision 1.22  2000/11/03 01:12:44  thiessen
* fix memory problem with alignment cloning
*
* Revision 1.21  2000/11/02 16:56:00  thiessen
* working editor undo; dynamic slave transforms
*
* Revision 1.20  2000/10/17 14:35:06  thiessen
* added row shift - editor basically complete
*
* Revision 1.19  2000/10/16 20:03:06  thiessen
* working block creation
*
* Revision 1.18  2000/10/16 14:25:47  thiessen
* working alignment fit coloring
*
* Revision 1.17  2000/10/12 19:20:45  thiessen
* working block deletion
*
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

#include <corelib/ncbistd.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbistl.hpp>

#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seqalign/Dense_seg.hpp>

#include <memory>

#include "cn3d/alignment_manager.hpp"
#include "cn3d/sequence_set.hpp"
#include "cn3d/alignment_set.hpp"
#include "cn3d/block_multiple_alignment.hpp"
#include "cn3d/messenger.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/sequence_viewer.hpp"
#include "cn3d/molecule.hpp"
#include "cn3d/show_hide_manager.hpp"
#include "cn3d/cn3d_threader.hpp"
#include "cn3d/update_viewer.hpp"
#include "cn3d/sequence_display.hpp"
#include "cn3d/cn3d_tools.hpp"
#include "cn3d/molecule_identifier.hpp"
#include "cn3d/cn3d_blast.hpp"
#include "cn3d/style_manager.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

///// AlignmentManager methods /////

void AlignmentManager::Init(void)
{
    sequenceViewer = new SequenceViewer(this);
    GlobalMessenger()->AddSequenceViewer(sequenceViewer);

    updateViewer = new UpdateViewer(this);
    GlobalMessenger()->AddSequenceViewer(updateViewer);

    threader = new Threader();
    blaster = new BLASTer();
}

AlignmentManager::AlignmentManager(const SequenceSet *sSet, const AlignmentSet *aSet)
{
    Init();
    NewAlignments(sSet, aSet);
}

AlignmentManager::AlignmentManager(const SequenceSet *sSet,
    const AlignmentSet *aSet, const UpdateAlignList& updates)
{
    Init();
    NewAlignments(sSet, aSet);

    // create BlockMultipleAlignments from updates; add to update viewer
    PairwiseAlignmentList pairwise(1);
    UpdateViewer::AlignmentList updateAlignments;
    UpdateAlignList::const_iterator u, ue = updates.end();
    for (u=updates.begin(); u!=ue; u++) {
        if (u->GetObject().IsSetSeqannot() && u->GetObject().GetSeqannot().GetData().IsAlign()) {
            CSeq_annot::C_Data::TAlign::const_iterator
                s, se = u->GetObject().GetSeqannot().GetData().GetAlign().end();
            for (s=u->GetObject().GetSeqannot().GetData().GetAlign().begin(); s!=se; s++) {

                // determine master sequence
                const Sequence *master = NULL;
                if (aSet) {
                    master = aSet->master;
                } else if (updateAlignments.size() > 0) {
                    master = updateAlignments.front()->GetMaster();
                } else {
                    typedef std::list < CRef < CSeq_id > > SeqIdList;
                    const SeqIdList& firstSids = (*s)->GetSegs().IsDendiag() ?
                        (*s)->GetSegs().GetDendiag().front()->GetIds() :
                        (*s)->GetSegs().GetDenseg().GetIds();
                    SequenceSet::SequenceList::const_iterator s, se = sSet->sequences.end();
                    for (s=sSet->sequences.begin(); s!=se; s++) {
                        if ((*s)->identifier->MatchesSeqId(firstSids.front().GetObject())) {
                            master = *s;
                            break;
                        }
                    }
                }
                if (!master) {
                    ERR_POST(Error << "AlignmentManager::AlignmentManager() - "
                        << "can't determine master sequence for updates");
                    return;
                }

                const MasterSlaveAlignment *alignment =
                    new MasterSlaveAlignment(NULL, master, s->GetObject());
                pairwise.front() = alignment;
                BlockMultipleAlignment *multiple = CreateMultipleFromPairwiseWithIBM(pairwise);
                multiple->updateOrigin = *u;    // to keep track of which Update-align this came from
                updateAlignments.push_back(multiple);
                delete alignment;
            }
        }
    }
    updateViewer->AddAlignments(updateAlignments);

    // set this set of updates as the initial state of the editor's undo stack
    updateViewer->SetInitialState();
}

AlignmentManager::~AlignmentManager(void)
{
    GlobalMessenger()->RemoveSequenceViewer(sequenceViewer);
    GlobalMessenger()->RemoveSequenceViewer(updateViewer);
    delete sequenceViewer;
    delete updateViewer;
    delete threader;
    delete blaster;
}

void AlignmentManager::NewAlignments(const SequenceSet *sSet, const AlignmentSet *aSet)
{
    sequenceSet = sSet;
    alignmentSet = aSet;

    if (!alignmentSet) {
        sequenceViewer->DisplaySequences(&(sequenceSet->sequences));
        return;
    }

    // all slaves start out visible
    slavesVisible.resize(alignmentSet->alignments.size());
    for (int i=0; i<slavesVisible.size(); i++) slavesVisible[i] = true;

    NewMultipleWithRows(slavesVisible);
}

void AlignmentManager::SavePairwiseFromMultiple(const BlockMultipleAlignment *multiple,
    const std::vector < int >& rowOrder)
{
    // create new AlignmentSet based on this multiple alignment, feed back into StructureSet
    AlignmentSet *newAlignmentSet =
        AlignmentSet::CreateFromMultiple(multiple->GetMaster()->parentSet, multiple, rowOrder);
    if (newAlignmentSet) {
        multiple->GetMaster()->parentSet->ReplaceAlignmentSet(newAlignmentSet);
        alignmentSet = newAlignmentSet;
    } else {
        ERR_POST(Error << "Couldn't create pairwise alignments from the current multiple!\n"
            << "Alignment data in output file will be left unchanged.");
    }
}

const BlockMultipleAlignment * AlignmentManager::GetCurrentMultipleAlignment(void) const
{
    const ViewerBase::AlignmentList *currentAlignments = sequenceViewer->GetCurrentAlignments();
    return ((currentAlignments) ? currentAlignments->front() : NULL);
}

static bool AlignedToAllSlaves(int masterResidue,
    const AlignmentManager::PairwiseAlignmentList& alignments)
{
    AlignmentManager::PairwiseAlignmentList::const_iterator a, ae = alignments.end();
    for (a=alignments.begin(); a!=ae; a++) {
        if ((*a)->masterToSlave[masterResidue] == -1) return false;
    }
    return true;
}

static bool NoSlaveInsertionsBetween(int masterFrom, int masterTo,
    const AlignmentManager::PairwiseAlignmentList& alignments)
{
    AlignmentManager::PairwiseAlignmentList::const_iterator a, ae = alignments.end();
    for (a=alignments.begin(); a!=ae; a++) {
        if (((*a)->masterToSlave[masterTo] - (*a)->masterToSlave[masterFrom]) !=
            (masterTo - masterFrom)) return false;
    }
    return true;
}

static bool NoBlockBoundariesBetween(int masterFrom, int masterTo,
    const AlignmentManager::PairwiseAlignmentList& alignments)
{
    AlignmentManager::PairwiseAlignmentList::const_iterator a, ae = alignments.end();
    for (a=alignments.begin(); a!=ae; a++) {
        if ((*a)->blockStructure[masterTo] != (*a)->blockStructure[masterFrom])
            return false;
    }
    return true;
}

BlockMultipleAlignment *
AlignmentManager::CreateMultipleFromPairwiseWithIBM(const PairwiseAlignmentList& alignments)
{
    PairwiseAlignmentList::const_iterator a, ae = alignments.end();

    // create sequence list; fill with sequences of master + slaves
    BlockMultipleAlignment::SequenceList
        *sequenceList = new BlockMultipleAlignment::SequenceList(alignments.size() + 1);
    BlockMultipleAlignment::SequenceList::iterator s = sequenceList->begin();
    *(s++) = alignments.front()->master;
    for (a=alignments.begin(); a!=ae; a++) {
        *(s++) = (*a)->slave;
        if ((*a)->master != sequenceList->front()) {
            ERR_POST(Error << "AlignmentManager::CreateMultipleFromPairwiseWithIBM() -\n"
                << "all pairwise alignments must have the same master sequence");
            return NULL;
        }
    }
    BlockMultipleAlignment *multipleAlignment = new BlockMultipleAlignment(sequenceList, this);

    // each block is a continuous region on the master, over which each master
    // residue is aligned to a residue of each slave, and where there are no
    // insertions relative to the master in any of the slaves
    int masterFrom = 0, masterTo, row;
    UngappedAlignedBlock *newBlock;

    while (masterFrom < multipleAlignment->GetMaster()->Length()) {

        // look for first all-aligned residue
        if (!AlignedToAllSlaves(masterFrom, alignments)) {
            masterFrom++;
            continue;
        }

        // find all next continuous all-aligned residues, but checking for
        // block boundaries from the original master-slave pairs, so that
        // blocks don't get merged
        for (masterTo=masterFrom+1;
                masterTo < multipleAlignment->GetMaster()->Length() &&
                AlignedToAllSlaves(masterTo, alignments) &&
                NoSlaveInsertionsBetween(masterFrom, masterTo, alignments) &&
                NoBlockBoundariesBetween(masterFrom, masterTo, alignments);
             masterTo++) ;
        masterTo--; // after loop, masterTo = first residue past block

        // create new block with ranges from master and all slaves
        newBlock = new UngappedAlignedBlock(multipleAlignment);
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
        !multipleAlignment->UpdateBlockMapAndColors()) {
        ERR_POST(Error << "AlignmentManager::CreateMultipleFromPairwiseWithIBM() - "
            "error finalizing alignment");
        return NULL;
    }

    return multipleAlignment;
}

static void GetAlignedResidueIndexes(
    BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator& b,
    BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator& be,
    int row, int *seqIndexes)
{
    int i = 0, c;
    const Block::Range *range;
    for (; b!=be; b++) {
        range = (*b)->GetRangeOfRow(row);
        for (c=0; c<(*b)->width; c++) {
            seqIndexes[i++] = range->from + c;
        }
    }
}

void AlignmentManager::RealignAllSlaveStructures(void) const
{
    const BlockMultipleAlignment *multiple = GetCurrentMultipleAlignment();
    if (!multiple) return;
    auto_ptr<BlockMultipleAlignment::UngappedAlignedBlockList> blocks(multiple->GetUngappedAlignedBlocks());
    if (!blocks.get()) {
        ERR_POST(Warning << "Can't realign slaves with no aligned residues!");
        return;
    }
    BlockMultipleAlignment::UngappedAlignedBlockList::const_iterator b, be = blocks->end();
    int nResidues = 0;
    for (b=blocks->begin(); b!=be; b++) nResidues += (*b)->width;
    if (nResidues <= 2) {
        ERR_POST(Warning << "Can't realign slaves with < 3 aligned residues!");
        return;
    }

    const Sequence *masterSeq = multiple->GetSequenceOfRow(0), *slaveSeq;
    const Molecule *masterMol, *slaveMol;
    if (!masterSeq || !(masterMol = masterSeq->molecule)) {
        ERR_POST(Warning << "Can't realign slaves to non-structured master!");
        return;
    }

    int *masterSeqIndexes = new int[nResidues], *slaveSeqIndexes = new int[nResidues];
    b = blocks->begin();
    GetAlignedResidueIndexes(b, be, 0, masterSeqIndexes);

    double *weights = new double[nResidues];

    const StructureObject *slaveObj;

    typedef const Vector * CVP;
    CVP *masterCoords = new CVP[nResidues], *slaveCoords = new CVP[nResidues];
    if (!masterMol->GetAlphaCoords(nResidues, masterSeqIndexes, masterCoords)) {
        ERR_POST(Warning << "Can't get master alpha coords");
    } else if (masterSeq->GetOrSetMMDBLink() == MoleculeIdentifier::VALUE_NOT_SET) {
        ERR_POST(Warning << "Don't know master MMDB ID");
    } else {

        masterMol->parentSet->InitStructureAlignments(masterSeq->identifier->mmdbID);

        int nStructureAlignments = 0;
        for (int i=1; i<multiple->NRows(); i++) {
            slaveSeq = multiple->GetSequenceOfRow(i);
            if (!slaveSeq || !(slaveMol = slaveSeq->molecule)) continue;

            b = blocks->begin();
            GetAlignedResidueIndexes(b, be, i, slaveSeqIndexes);
            if (!slaveMol->GetAlphaCoords(nResidues, slaveSeqIndexes, slaveCoords)) continue;

            if (!slaveMol->GetParentOfType(&slaveObj)) continue;

            // if any Vector* is NULL, make sure that weight is 0 so the pointer won't be accessed
            for (int j=0; j<nResidues; j++) {
                if (!masterCoords[j] || !slaveCoords[j])
                    weights[j] = 0.0;
                else
                    weights[j] = 1.0; // for now, just use flat weighting
            }

            TESTMSG("realigning slave " << slaveSeq->identifier->pdbID << " against master " << masterSeq->identifier->pdbID);
            (const_cast<StructureObject*>(slaveObj))->
                RealignStructure(nResidues, masterCoords, slaveCoords, weights, i);
            nStructureAlignments++;
        }

        // if no structure alignments, remove the list entirely
        if (nStructureAlignments == 0) masterMol->parentSet->RemoveStructureAlignments();
    }

    delete masterSeqIndexes;
    delete slaveSeqIndexes;
    delete masterCoords;
    delete slaveCoords;
    delete weights;
    return;
}

void AlignmentManager::GetAlignmentSetSlaveSequences(std::vector < const Sequence * > *sequences) const
{
    sequences->resize(alignmentSet->alignments.size());

    AlignmentSet::AlignmentList::const_iterator a, ae = alignmentSet->alignments.end();
    int i = 0;
    for (a=alignmentSet->alignments.begin(); a!=ae; a++, i++) {
        (*sequences)[i] = (*a)->slave;
    }
}

void AlignmentManager::GetAlignmentSetSlaveVisibilities(std::vector < bool > *visibilities) const
{
    if (slavesVisible.size() != alignmentSet->alignments.size()) // can happen if row is added/deleted
        slavesVisible.resize(alignmentSet->alignments.size(), true);

    // copy visibility list
    *visibilities = slavesVisible;
}

void AlignmentManager::ShowHideCallbackFunction(const std::vector < bool >& itemsEnabled)
{
    if (itemsEnabled.size() != slavesVisible.size() ||
        itemsEnabled.size() != alignmentSet->alignments.size()) {
        ERR_POST(Error << "AlignmentManager::ShowHideCallbackFunction() - wrong size list");
        return;
    }

    slavesVisible = itemsEnabled;
    NewMultipleWithRows(slavesVisible);

    AlignmentSet::AlignmentList::const_iterator
        a = alignmentSet->alignments.begin(), ae = alignmentSet->alignments.end();
    const StructureObject *object;

    if ((*a)->master->molecule) {
        // Show() redraws whole StructurObject only if necessary
        if ((*a)->master->molecule->GetParentOfType(&object))
            object->parentSet->showHideManager->ShowObject(object, true);
        // always redraw aligned molecule, in case alignment colors change
        GlobalMessenger()->PostRedrawMolecule((*a)->master->molecule);
    }
    for (int i=0; a!=ae; a++, i++) {
        if ((*a)->slave->molecule) {
            if ((*a)->slave->molecule->GetParentOfType(&object))
                object->parentSet->showHideManager->ShowObject(object, slavesVisible[i]);
            GlobalMessenger()->PostRedrawMolecule((*a)->slave->molecule);
        }
    }

    // do necessary redraws + show/hides: sequences + chains in the alignment
    sequenceViewer->Refresh();
    GlobalMessenger()->PostRedrawAllSequenceViewers();
    GlobalMessenger()->UnPostRedrawSequenceViewer(sequenceViewer);  // Refresh() does this already
}

void AlignmentManager::NewMultipleWithRows(const std::vector < bool >& visibilities)
{
    if (visibilities.size() != alignmentSet->alignments.size()) {
        ERR_POST(Error << "AlignmentManager::NewMultipleWithRows() - wrong size visibility vector");
        return;
    }

    // make a multiple from all visible rows
    PairwiseAlignmentList alignments;
    AlignmentSet::AlignmentList::const_iterator a, ae=alignmentSet->alignments.end();
    int i = 0;
    for (a=alignmentSet->alignments.begin(); a!=ae; a++, i++) {
        if (visibilities[i])
            alignments.push_back(*a);
    }

    // sequenceViewer will own the resulting alignment
    sequenceViewer->DisplayAlignment(CreateMultipleFromPairwiseWithIBM(alignments));
}

bool AlignmentManager::IsAligned(const Sequence *sequence, int seqIndex) const
{
    const BlockMultipleAlignment *currentAlignment = GetCurrentMultipleAlignment();
    if (currentAlignment)
        return currentAlignment->IsAligned(sequence, seqIndex);
    else
        return false;
}

bool AlignmentManager::IsInAlignment(const Sequence *sequence) const
{
    if (!sequence) return false;
    const BlockMultipleAlignment *currentAlignment = GetCurrentMultipleAlignment();
    if (currentAlignment) {
        for (int i=0; i<currentAlignment->sequences->size(); i++) {
            if ((*(currentAlignment->sequences))[i] == sequence)
                return true;
        }
    }
    return false;
}

const Vector * AlignmentManager::GetAlignmentColor(const Sequence *sequence, int seqIndex) const
{
    const BlockMultipleAlignment *currentAlignment = GetCurrentMultipleAlignment();
    if (currentAlignment)
        return currentAlignment->GetAlignmentColor(sequence, seqIndex);
    else
        return NULL;
}

void AlignmentManager::ShowUpdateWindow(void) const
{
    updateViewer->CreateUpdateWindow();
}

void AlignmentManager::RealignSlaveSequences(
    BlockMultipleAlignment *multiple, const std::vector < int >& slavesToRealign)
{
    if (!multiple || !sequenceViewer->GetCurrentAlignments() ||
            multiple != sequenceViewer->GetCurrentAlignments()->front()) {
        ERR_POST(Error << "AlignmentManager::RealignSlaveSequences() - wrong multiple alignment");
        return;
    }
    if (slavesToRealign.size() == 0) return;

    // create alignments for each master/slave pair, then update displays
    UpdateViewer::AlignmentList alignments;
    TESTMSG("extracting rows");
    if (multiple->ExtractRows(slavesToRealign, &alignments)) {
        TESTMSG("recreating display");
        sequenceViewer->GetCurrentDisplay()->RowsRemoved(slavesToRealign, multiple);
        TESTMSG("adding to update window");
        SetDiagPostLevel(eDiag_Warning);    // otherwise, info messages take a long time if lots of rows
        updateViewer->AddAlignments(alignments);
        SetDiagPostLevel(eDiag_Info);
        TESTMSG("done");
        updateViewer->CreateUpdateWindow();
    }
}

void AlignmentManager::ThreadUpdate(const ThreaderOptions& options, BlockMultipleAlignment *single)
{
    const ViewerBase::AlignmentList *currentAlignments = sequenceViewer->GetCurrentAlignments();
    if (!currentAlignments) return;

    // make sure the editor is on in the sequenceViewer if merge is selected
    if (!sequenceViewer->EditorIsOn() && options.mergeAfterEachSequence) {
        ERR_POST(Error << "Can only merge updates when editing is enabled in the sequence window");
        return;
    }

    // run the threader on the given alignment
    UpdateViewer::AlignmentList singleList, replacedList;
    Threader::AlignmentList newAlignments;
    int nRowsAddedToMultiple;
    bool foundSingle = false;   // sanity check

    singleList.push_back(single);
    if (threader->Realign(
            options, currentAlignments->front(), &singleList,
            &nRowsAddedToMultiple, &newAlignments)) {

        // replace threaded alignment with new one(s) (or leftover where threader/merge failed)
        UpdateViewer::AlignmentList::const_iterator a, ae = updateViewer->GetCurrentAlignments()->end();
        for (a=updateViewer->GetCurrentAlignments()->begin(); a!=ae; a++) {
            if (*a == single) {
                Threader::AlignmentList::const_iterator n, ne = newAlignments.end();
                for (n=newAlignments.begin(); n!=ne; n++)
                    replacedList.push_back(*n);
                foundSingle = true;
            } else
                replacedList.push_back((*a)->Clone());
        }
        if (!foundSingle) ERR_POST(Error
            << "AlignmentManager::ThreadUpdate() - threaded alignment not found in update viewer!");
        updateViewer->ReplaceAlignments(replacedList);

        // tell the sequenceViewer that rows have been merged into the multiple
        if (nRowsAddedToMultiple > 0)
            sequenceViewer->GetCurrentDisplay()->
                RowsAdded(nRowsAddedToMultiple, currentAlignments->front());
    }
}

void AlignmentManager::ThreadAllUpdates(const ThreaderOptions& options)
{
    const ViewerBase::AlignmentList *currentAlignments = sequenceViewer->GetCurrentAlignments();
    if (!currentAlignments) return;

    // make sure the editor is on in the sequenceViewer
    if (!sequenceViewer->EditorIsOn() && options.mergeAfterEachSequence) {
        ERR_POST(Error << "Can only merge updates when editing is enabled in the sequence window");
        return;
    }

    // run the threader on update pairwise alignments
    Threader::AlignmentList newAlignments;
    int nRowsAddedToMultiple;
    if (threader->Realign(
            options, currentAlignments->front(), updateViewer->GetCurrentAlignments(),
            &nRowsAddedToMultiple, &newAlignments)) {

        // replace update alignments with new ones (or leftovers where threader/merge failed)
        updateViewer->ReplaceAlignments(newAlignments);

        // tell the sequenceViewer that rows have been merged into the multiple
        if (nRowsAddedToMultiple > 0)
            sequenceViewer->GetCurrentDisplay()->
                RowsAdded(nRowsAddedToMultiple, currentAlignments->front());
    }
}

void AlignmentManager::MergeUpdates(const AlignmentManager::UpdateMap& updatesToMerge)
{
    if (updatesToMerge.size() == 0) return;
    const ViewerBase::AlignmentList *currentUpdates = updateViewer->GetCurrentAlignments();
    if (!currentUpdates) return;

    // transform this structure view into an alignment view, and turn on the editor.
    ViewerBase::AlignmentList::const_iterator u, ue = currentUpdates->end();
    const BlockMultipleAlignment *newMultiple = NULL;
    if (!sequenceViewer->GetCurrentAlignments()) {

        for (u=currentUpdates->begin(); u!=ue; u++) {   // find first update alignment
            if (updatesToMerge.find(*u) != updatesToMerge.end()) {
                newMultiple = *u;

                // create new alignment, then call SavePairwiseFromMultiple to create
                // an AlignmentSet and the initial ASN data
                sequenceViewer->DisplayAlignment(newMultiple->Clone());
                std::vector < int > rowOrder(newMultiple->NRows());
                for (int i=0; i<newMultiple->NRows(); i++) rowOrder[i] = i;
                SavePairwiseFromMultiple(newMultiple, rowOrder);

                // editor needs to be on if >1 update is to be merged in
                sequenceViewer->TurnOnEditor();

                // set default alignment-type style
                newMultiple->GetMaster()->parentSet->styleManager->
                    SetGlobalRenderingStyle(StyleSettings::eTubeShortcut);
                newMultiple->GetMaster()->parentSet->styleManager->
                    SetGlobalColorScheme(StyleSettings::eAlignedShortcut);
                GlobalMessenger()->PostRedrawAllStructures();
                break;
            }
        }
    }

    BlockMultipleAlignment *multiple =
        sequenceViewer->GetCurrentAlignments() ? sequenceViewer->GetCurrentAlignments()->front() : NULL;
    if (!multiple) {
        ERR_POST(Error << "Must have an alignment in the sequence viewer to merge with");
        return;
    }

    // make sure the editor is on in the sequenceViewer
    if (!sequenceViewer->EditorIsOn()) {
        ERR_POST(Error << "Can only merge updates when editing is enabled in the sequence window");
        return;
    }

    int nSuccessfulMerges = 0;
    ViewerBase::AlignmentList updatesToKeep;
    for (u=currentUpdates->begin(); u!=ue; u++) {
        if (*u == newMultiple) continue;
        bool merged = false;
        if (updatesToMerge.find(*u) != updatesToMerge.end()) {
            merged = multiple->MergeAlignment(*u);
            if (merged) {
                nSuccessfulMerges += (*u)->NRows() - 1;
            } else {
                for (int i=0; i<(*u)->NRows(); i++) {
                    std::string status = (*u)->GetRowStatusLine(i);
                    if (status.size() > 0)
                        status += "; merge failed!";
                    else
                        status = "Merge failed!";
                    (*u)->SetRowStatusLine(i, status);
                }
            }
        }
        if (!merged) {
            BlockMultipleAlignment *keep = (*u)->Clone();
            updatesToKeep.push_back(keep);
        }
    }

    updateViewer->ReplaceAlignments(updatesToKeep);
    if (nSuccessfulMerges > 0)
        sequenceViewer->GetCurrentDisplay()->RowsAdded(nSuccessfulMerges, multiple);

    // add pending imported structures to asn data
    updateViewer->SavePendingStructures();
}

void AlignmentManager::CalculateRowScoresWithThreader(double weightPSSM)
{
    threader->CalculateScores(GetCurrentMultipleAlignment(), weightPSSM);
}

void AlignmentManager::GetUpdateSequences(std::list < const Sequence * > *updateSequences) const
{
    updateSequences->clear();
    const ViewerBase::AlignmentList *currentUpdates = updateViewer->GetCurrentAlignments();
    if (!currentUpdates) return;
    ViewerBase::AlignmentList::const_iterator u, ue = currentUpdates->end();
    for (u=currentUpdates->begin(); u!=ue; u++)
        updateSequences->push_back((*u)->GetSequenceOfRow(1));  // assume update aln has just one slave...
}

bool AlignmentManager::GetStructureProteins(std::vector < const Sequence * > *chains) const
{
    if (!chains || GetCurrentMultipleAlignment() != NULL ||
        !sequenceViewer || !sequenceViewer->GetCurrentDisplay()) return false;

    sequenceViewer->GetCurrentDisplay()->GetProteinSequences(chains);
    return (chains->size() > 0);
}

void AlignmentManager::ReplaceUpdatesInASN(const ncbi::objects::CCdd::TPending& newUpdates) const
{
    if (sequenceSet)
        sequenceSet->parentSet->ReplaceUpdates(newUpdates);
    else
        ERR_POST(Error << "AlignmentManager::ReplaceUpdatesInASN() - can't get StructureSet");
}

void AlignmentManager::PurgeSequence(const MoleculeIdentifier *identifier)
{
    BlockMultipleAlignment *multiple =
        sequenceViewer->GetCurrentAlignments() ? sequenceViewer->GetCurrentAlignments()->front() : NULL;
    if (!multiple) return;

    // remove matching rows from multiple alignment
    std::vector < int > rowsToRemove;
    int i;
    for (i=1; i<multiple->NRows(); i++)
        if (multiple->GetSequenceOfRow(i)->identifier == identifier)
            rowsToRemove.push_back(i);

    if (rowsToRemove.size() > 0) {

        // turn on editor, and update multiple pointer
        if (!sequenceViewer->EditorIsOn()) {
            sequenceViewer->TurnOnEditor();
            multiple = sequenceViewer->GetCurrentAlignments()->front();
        }

        if (!multiple->ExtractRows(rowsToRemove, NULL)) {
            ERR_POST("AlignmentManager::PurgeSequence() - ExtractRows failed!");
            return;
        }

        // remove rows from SequenceDisplay
        SequenceDisplay *display = sequenceViewer->GetCurrentDisplay();
        if (!display) {
            ERR_POST("AlignmentManager::PurgeSequence() - can't get SequenceDisplay!");
            return;
        }
        display->RowsRemoved(rowsToRemove, multiple);
    }

    // remove matching alignments from Update window
    const ViewerBase::AlignmentList *currentUpdates = updateViewer->GetCurrentAlignments();
    if (!currentUpdates) return;
    ViewerBase::AlignmentList::const_iterator u, ue = currentUpdates->end();

    for (u=currentUpdates->begin(); u!=ue; u++) // quick check if any match found
        if ((*u)->GetSequenceOfRow(1)->identifier == identifier) break;

    if (u != ue) {
        ViewerBase::AlignmentList updatesToKeep;
        for (u=currentUpdates->begin(); u!=ue; u++) {
            if ((*u)->GetSequenceOfRow(1)->identifier != identifier) {
                BlockMultipleAlignment *keep = (*u)->Clone();
                updatesToKeep.push_back(keep);
            }
        }
        updateViewer->ReplaceAlignments(updatesToKeep);
    }
}

END_SCOPE(Cn3D)

