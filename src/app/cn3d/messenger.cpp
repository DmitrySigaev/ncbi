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
*      Classes to handle messaging and communication between sequence
*      and structure windows
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.8  2000/10/19 12:40:54  thiessen
* avoid multiple sequence redraws with scroll set
*
* Revision 1.7  2000/10/12 02:14:56  thiessen
* working block boundary editing
*
* Revision 1.6  2000/10/02 23:25:21  thiessen
* working sequence identifier window in sequence viewer
*
* Revision 1.5  2000/09/15 19:24:22  thiessen
* allow repeated structures w/o different local id
*
* Revision 1.4  2000/09/14 14:55:34  thiessen
* add row reordering; misc fixes
*
* Revision 1.3  2000/09/11 22:57:32  thiessen
* working highlighting
*
* Revision 1.2  2000/09/11 14:06:28  thiessen
* working alignment coloring
*
* Revision 1.1  2000/09/11 01:46:14  thiessen
* working messenger for sequence<->structure window communication
*
* ===========================================================================
*/

#include <wx/string.h> // name conflict kludge

#include "cn3d/messenger.hpp"
#include "cn3d/sequence_viewer.hpp"
#include "cn3d/opengl_renderer.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/chemical_graph.hpp"
#include "cn3d/sequence_set.hpp"

#include "cn3d/cn3d_main_wxwin.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

void Messenger::PostRedrawAllStructures(void)
{
    redrawAllStructures = true;
    redrawMolecules.clear();
}

void Messenger::PostRedrawMolecule(const Molecule *molecule)
{
    if (!redrawAllStructures) {
        RedrawMoleculeList::const_iterator m, me = redrawMolecules.end();
        for (m=redrawMolecules.begin(); m!=me; m++)
            if (*m == molecule) break;
        if (m == me) redrawMolecules.push_back(molecule);
    }
}

void Messenger::PostRedrawSequenceViewers(void)
{
    redrawSequenceViewers = true;
}

void Messenger::UnPostRedrawSequenceViewers(void)
{
    redrawSequenceViewers = false;
}

void Messenger::ProcessRedraws(void)
{
    if (redrawSequenceViewers) {
        SequenceViewerList::const_iterator q, qe = sequenceViewers.end();
        for (q=sequenceViewers.begin(); q!=qe; q++)
            (*q)->Refresh();
        redrawSequenceViewers = false;
    }

    if (redrawAllStructures) {
        StructureWindowList::const_iterator t, te = structureWindows.end();
        for (t=structureWindows.begin(); t!=te; t++) {
            (*t)->glCanvas->renderer.Construct();
            (*t)->glCanvas->Refresh(false);
        }
        redrawAllStructures = false;
    }

    else if (redrawMolecules.size() > 0) {
        RedrawMoleculeList::const_iterator m, me = redrawMolecules.end();
        for (m=redrawMolecules.begin(); m!=me; m++) {
            const StructureObject *object;
            if (!(*m)->GetParentOfType(&object)) continue;
            object->graph->RedrawMolecule((*m)->id);
        }
        redrawMolecules.clear();

        StructureWindowList::const_iterator t, te = structureWindows.end();
        for (t=structureWindows.begin(); t!=te; t++)
            (*t)->glCanvas->Refresh(false);
    }
}

void Messenger::RemoveStructureWindow(const Cn3DMainFrame *structureWindow)
{
    StructureWindowList::iterator t, te = structureWindows.end();
    for (t=structureWindows.begin(); t!=te; t++) {
        if (*t == structureWindow) structureWindows.erase(t);
        break;
    }
}

void Messenger::RemoveSequenceViewer(const SequenceViewer *sequenceViewer)
{
    SequenceViewerList::iterator t, te = sequenceViewers.end();
    for (t=sequenceViewers.begin(); t!=te; t++) {
        if (*t == sequenceViewer) sequenceViewers.erase(t);
        break;
    }
}


///// highlighting functions /////

bool Messenger::IsHighlighted(const Molecule *molecule, int residueID) const
{
    // use sequence-wise highlight stores
    if (molecule->sequence) {
        return IsHighlighted(molecule->sequence, residueID - 1);
    }

    // use residue-wise highlight stores
    else {
        ResidueIdentifier rid = std::make_pair(molecule, residueID);
        PerResidueHighlightStore::const_iterator rh = residueHighlights.find(rid);
        if (rh != residueHighlights.end())
            return true;
        else
            return false;
    }
}

bool Messenger::IsHighlighted(const Sequence *sequence, int seqIndex) const
{
    if (seqIndex < 0 || seqIndex >= sequence->sequenceString.size()) {
        ERR_POST(Error << "Messenger::IsHighlighted() - seqIndex out of range");
        return false;
    }

    PerSequenceHighlightStore::const_iterator sh, she = sequenceHighlights.end();
    for (sh=sequenceHighlights.begin(); sh!=she; sh++) {
        if (SAME_SEQUENCE(sequence, sh->first)) {
            if (sh->second[seqIndex])
                return true;
            else
                return false;
        }
    }
    return false;
}

void Messenger::RedrawMoleculesOfSameSequence(const Sequence *sequence)
{
    SequenceSet::SequenceList::const_iterator s,
        se = sequence->molecule->parentSet->sequenceSet->sequences.end();
    for (s=sequence->molecule->parentSet->sequenceSet->sequences.begin(); s!=se; s++) {
        if ((*s)->molecule && SAME_SEQUENCE(sequence, *s)) {
            PostRedrawMolecule((*s)->molecule);
        }
    }
}

void Messenger::AddHighlights(const Sequence *sequence, int seqIndexFrom, int seqIndexTo)
{
    if (seqIndexFrom < 0 || seqIndexTo < 0 || seqIndexFrom > seqIndexTo ||
        seqIndexFrom >= sequence->sequenceString.size() || 
        seqIndexTo >= sequence->sequenceString.size()) {
        ERR_POST(Error << "Messenger::AddHighlights() - seqIndex out of range");
        return;
    }

    PerSequenceHighlightStore::iterator sh, she = sequenceHighlights.end();
    for (sh=sequenceHighlights.begin(); sh!=she; sh++) {
        if (SAME_SEQUENCE(sequence, sh->first)) break;
    }
    if (sh == she) {
        sequenceHighlights[sequence].resize(sequence->sequenceString.size());
        sh = sequenceHighlights.find(sequence);
    }

    for (int i=seqIndexFrom; i<=seqIndexTo; i++) sh->second[i] = true;

    PostRedrawSequenceViewers();
    if (sequence->molecule) RedrawMoleculesOfSameSequence(sequence);
}

void Messenger::RemoveHighlights(const Sequence *sequence, int seqIndexFrom, int seqIndexTo)
{
    if (seqIndexFrom < 0 || seqIndexTo < 0 || seqIndexFrom > seqIndexTo ||
        seqIndexFrom >= sequence->sequenceString.size() || 
        seqIndexTo >= sequence->sequenceString.size()) {
        ERR_POST(Error << "Messenger::RemoveHighlights() - seqIndex out of range");
        return;
    }

    PerSequenceHighlightStore::iterator sh, she = sequenceHighlights.end();
    for (sh=sequenceHighlights.begin(); sh!=she; sh++) {
        if (SAME_SEQUENCE(sequence, sh->first)) break;
    }
    if (sh != she) {
        int i;
        for (i=seqIndexFrom; i<=seqIndexTo; i++) sh->second[i] = false;

        // remove sequence from store if no highlights left
        for (i=0; i<sequence->sequenceString.size(); i++)
            if (sh->second[i] == true) break;
        if (i == sequence->sequenceString.size())
            sequenceHighlights.erase(sh);

        PostRedrawSequenceViewers();
        if (sequence->molecule) RedrawMoleculesOfSameSequence(sequence);
    }
}

void Messenger::ToggleHighlightOnAnyResidue(const Molecule *molecule, int residueID)
{
    // use sequence-wise highlight stores
    if (molecule->sequence) {
        int seqIndex = residueID - 1;
        if (IsHighlighted(molecule->sequence, seqIndex))
            RemoveHighlights(molecule->sequence, seqIndex, seqIndex);
        else
            AddHighlights(molecule->sequence, seqIndex, seqIndex);
    }

    // use residue-wise highlight stores
    else {
        ResidueIdentifier rid = std::make_pair(molecule, residueID);
        PerResidueHighlightStore::iterator rh = residueHighlights.find(rid);
        if (rh != residueHighlights.end())
            residueHighlights.erase(rh);    // remove highlight
        else
            residueHighlights[rid] = true;  // add highlight
    }

    PostRedrawMolecule(molecule);
}

bool Messenger::RemoveAllHighlights(bool postRedraws)
{
    bool anyRemoved = (sequenceHighlights.size() > 0 || residueHighlights.size() > 0);

    if (postRedraws) {
        if (anyRemoved) PostRedrawSequenceViewers();

        PerSequenceHighlightStore::iterator sh, she = sequenceHighlights.end();
        for (sh=sequenceHighlights.begin(); sh!=she; sh++) {
            if (sh->first->molecule) RedrawMoleculesOfSameSequence(sh->first);
        }

        PerResidueHighlightStore::const_iterator rh, rhe = residueHighlights.end();
        for (rh = residueHighlights.begin(); rh!=rhe; rh++) {
            PostRedrawMolecule(rh->first.first);
        }
    }

    sequenceHighlights.clear();
    residueHighlights.clear();

    return anyRemoved;
}

END_SCOPE(Cn3D)

