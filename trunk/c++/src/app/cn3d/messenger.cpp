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
* Revision 1.25  2002/01/19 02:34:42  thiessen
* fixes for changes in asn serialization API
*
* Revision 1.24  2001/12/12 14:04:13  thiessen
* add missing object headers after object loader change
*
* Revision 1.23  2001/10/23 20:10:23  thiessen
* fix scaling of fonts in high-res PNG output
*
* Revision 1.22  2001/10/01 16:04:24  thiessen
* make CDD annotation window non-modal; add SetWindowTitle to viewers
*
* Revision 1.21  2001/08/27 00:06:23  thiessen
* add structure evidence to CDD annotation
*
* Revision 1.20  2001/08/14 17:18:22  thiessen
* add user font selection, store in registry
*
* Revision 1.19  2001/07/04 19:39:17  thiessen
* finish user annotation system
*
* Revision 1.18  2001/06/29 18:13:57  thiessen
* initial (incomplete) user annotation system
*
* Revision 1.17  2001/06/21 02:02:33  thiessen
* major update to molecule identification and highlighting ; add toggle highlight (via alt)
*
* Revision 1.16  2001/05/11 02:10:42  thiessen
* add better merge fail indicators; tweaks to windowing/taskbar
*
* Revision 1.15  2001/03/30 14:43:41  thiessen
* show threader scores in status line; misc UI tweaks
*
* Revision 1.14  2001/03/22 00:33:17  thiessen
* initial threading working (PSSM only); free color storage in undo stack
*
* Revision 1.13  2001/03/01 20:15:51  thiessen
* major rearrangement of sequence viewer code into base and derived classes
*
* Revision 1.12  2001/02/22 00:30:06  thiessen
* make directories global ; allow only one Sequence per StructureObject
*
* Revision 1.11  2000/12/15 15:51:47  thiessen
* show/hide system installed
*
* Revision 1.10  2000/11/30 15:49:38  thiessen
* add show/hide rows; unpack sec. struc. and domain features
*
* Revision 1.9  2000/11/02 16:56:02  thiessen
* working editor undo; dynamic slave transforms
*
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
#include <corelib/ncbistd.hpp>

#include <objects/mmdb1/Biostruc_id.hpp>
#include <objects/mmdb1/Mmdb_id.hpp>
#include <objects/mmdb3/Biostruc_feature_set.hpp>
#include <objects/mmdb3/Biostruc_feature_set_id.hpp>
#include <objects/mmdb3/Biostruc_feature.hpp>
#include <objects/mmdb3/Chem_graph_pntrs.hpp>
#include <objects/mmdb3/Residue_pntrs.hpp>
#include <objects/mmdb3/Residue_interval_pntr.hpp>
#include <objects/mmdb1/Molecule_id.hpp>
#include <objects/mmdb1/Residue_id.hpp>

#include "cn3d/messenger.hpp"
#include "cn3d/sequence_viewer.hpp"
#include "cn3d/opengl_renderer.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/chemical_graph.hpp"
#include "cn3d/sequence_set.hpp"
#include "cn3d/cn3d_main_wxwin.hpp"
#include "cn3d/molecule_identifier.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

// the global Messenger object
static Messenger messenger;

Messenger * GlobalMessenger(void)
{
    return &messenger;
}


void Messenger::PostRedrawAllStructures(void)
{
    redrawAllStructures = true;
    redrawMolecules.clear();
}

void Messenger::PostRedrawMolecule(const Molecule *molecule)
{
    if (!redrawAllStructures) redrawMolecules[molecule] = true;
}

void Messenger::PostRedrawAllSequenceViewers(void)
{
    redrawAllSequenceViewers = true;
    redrawSequenceViewers.clear();
}
void Messenger::PostRedrawSequenceViewer(ViewerBase *viewer)
{
    if (!redrawAllSequenceViewers) redrawSequenceViewers[viewer] = true;
}

void Messenger::UnPostRedrawAllSequenceViewers(void)
{
    redrawAllSequenceViewers = false;
    redrawSequenceViewers.clear();
}

void Messenger::UnPostRedrawSequenceViewer(ViewerBase *viewer)
{
    if (redrawAllSequenceViewers) {
        SequenceViewerList::const_iterator q, qe = sequenceViewers.end();
        for (q=sequenceViewers.begin(); q!=qe; q++) redrawSequenceViewers[*q] = true;
        redrawAllSequenceViewers = false;
    }
    RedrawSequenceViewerList::iterator f = redrawSequenceViewers.find(viewer);
    if (f != redrawSequenceViewers.end()) redrawSequenceViewers.erase(f);
}

void Messenger::UnPostStructureRedraws(void)
{
    redrawAllStructures = false;
    redrawMolecules.clear();
}

void Messenger::ProcessRedraws(void)
{
    if (redrawAllSequenceViewers) {
        SequenceViewerList::const_iterator q, qe = sequenceViewers.end();
        for (q=sequenceViewers.begin(); q!=qe; q++) (*q)->Refresh();
        redrawAllSequenceViewers = false;
    }
    else if (redrawSequenceViewers.size() > 0) {
        RedrawSequenceViewerList::const_iterator q, qe = redrawSequenceViewers.end();
        for (q=redrawSequenceViewers.begin(); q!=qe; q++) q->first->Refresh();
        redrawSequenceViewers.clear();
    }

    if (redrawAllStructures) {
        structureWindow->glCanvas->SetCurrent();
		structureWindow->glCanvas->renderer->Construct();
		structureWindow->glCanvas->Refresh(false);
		redrawAllStructures = false;
    }
    else if (redrawMolecules.size() > 0 && structureWindow != NULL) {
        std::map < const StructureObject * , bool > hetsRedrawn;
        RedrawMoleculeList::const_iterator m, me = redrawMolecules.end();
        for (m=redrawMolecules.begin(); m!=me; m++) {
            const StructureObject *object;
            if (!m->first->GetParentOfType(&object)) continue;

            // hets/solvents are always redrawn with each molecule, so don't need to repeat
            if ((m->first->IsSolvent() || m->first->IsHeterogen()) &&
                hetsRedrawn.find(object) != hetsRedrawn.end()) continue;

            object->graph->RedrawMolecule(m->first->id);
            hetsRedrawn[object] = true;
        }
        redrawMolecules.clear();
        structureWindow->glCanvas->Refresh(false);
    }
}

void Messenger::RemoveStructureWindow(const Cn3DMainFrame *window)
{
    if (window != structureWindow)
        ERR_POST(Error << "Messenger::RemoveStructureWindow() - window mismatch");
    structureWindow = NULL;
}

void Messenger::RemoveSequenceViewer(const ViewerBase *sequenceViewer)
{
    SequenceViewerList::iterator t, te = sequenceViewers.end();
    for (t=sequenceViewers.begin(); t!=te; t++) {
        if (*t == sequenceViewer) sequenceViewers.erase(t);
        break;
    }
}

void Messenger::SequenceWindowsSave(void)
{
    SequenceViewerList::const_iterator q, qe = sequenceViewers.end();
    for (q=sequenceViewers.begin(); q!=qe; q++)
        (*q)->SaveDialog();
}

void Messenger::NewSequenceViewerFont(void)
{
    SequenceViewerList::const_iterator q, qe = sequenceViewers.end();
    for (q=sequenceViewers.begin(); q!=qe; q++)
        (*q)->NewFont();
}


///// highlighting functions /////

bool Messenger::IsHighlighted(const MoleculeIdentifier *identifier, int index) const
{
    if (highlightingSuspended) return false;

    MoleculeHighlightMap::const_iterator h = highlights.find(identifier);
    if (h == highlights.end()) return false;

    if (index < 0 || index >= h->second.size()) {
        ERR_POST(Error << "Messenger::IsHighlighted() - index out of range");
        return false;
    } else
        return h->second[index];
}

bool Messenger::IsHighlighted(const Molecule *molecule, int residueID) const
{
    return IsHighlighted(molecule->identifier, residueID - 1);  // assume index = id - 1
}

bool Messenger::IsHighlighted(const Sequence *sequence, int seqIndex) const
{
    return IsHighlighted(sequence->identifier, seqIndex);
}

void Messenger::RedrawMoleculesWithIdentifier(const MoleculeIdentifier *identifier, const StructureSet *set)
{
    StructureSet::ObjectList::const_iterator o, oe = set->objects.end();
    ChemicalGraph::MoleculeMap::const_iterator m, me;
    for (o=set->objects.begin(); o!=oe; o++) {
        for (m=(*o)->graph->molecules.begin(), me=(*o)->graph->molecules.end(); m!=me; m++) {
            if (m->second->identifier == identifier)
                PostRedrawMolecule(m->second);
        }
    }
}

void Messenger::AddHighlights(const Sequence *sequence, int seqIndexFrom, int seqIndexTo)
{
    if (seqIndexFrom < 0 || seqIndexTo < 0 || seqIndexFrom > seqIndexTo ||
        seqIndexFrom >= sequence->Length() ||
        seqIndexTo >= sequence->Length()) {
        ERR_POST(Error << "Messenger::AddHighlights() - seqIndex out of range");
        return;
    }

    MoleculeHighlightMap::iterator h = highlights.find(sequence->identifier);
    if (h == highlights.end()) {
        highlights[sequence->identifier].resize(sequence->Length(), false);
        h = highlights.find(sequence->identifier);
    }

    for (int i=seqIndexFrom; i<=seqIndexTo; i++) h->second[i] = true;

    PostRedrawAllSequenceViewers();
    RedrawMoleculesWithIdentifier(sequence->identifier, sequence->parentSet);
}

void Messenger::RemoveHighlights(const Sequence *sequence, int seqIndexFrom, int seqIndexTo)
{
    if (seqIndexFrom < 0 || seqIndexTo < 0 || seqIndexFrom > seqIndexTo ||
        seqIndexFrom >= sequence->Length() ||
        seqIndexTo >= sequence->Length()) {
        ERR_POST(Error << "Messenger::RemoveHighlights() - seqIndex out of range");
        return;
    }

    MoleculeHighlightMap::iterator h = highlights.find(sequence->identifier);
    if (h != highlights.end()) {
        int i;
        for (i=seqIndexFrom; i<=seqIndexTo; i++) h->second[i] = false;

        // remove sequence from store if no highlights left
        for (i=0; i<sequence->Length(); i++)
            if (h->second[i] == true) break;
        if (i == sequence->Length())
            highlights.erase(h);

        PostRedrawAllSequenceViewers();
        RedrawMoleculesWithIdentifier(sequence->identifier, sequence->parentSet);
    }
}

void Messenger::ToggleHighlights(const MoleculeIdentifier *identifier, int indexFrom, int indexTo,
    const StructureSet *set)
{
    if (indexFrom < 0 || indexTo < 0 || indexFrom > indexTo ||
        indexFrom >= identifier->nResidues || indexTo >= identifier->nResidues) {
        ERR_POST(Error << "Messenger::ToggleHighlights() - index out of range");
        return;
    }

    MoleculeHighlightMap::iterator h = highlights.find(identifier);
    if (h == highlights.end()) {
        highlights[identifier].resize(identifier->nResidues, false);
        h = highlights.find(identifier);
    }

    int i;
    for (i=indexFrom; i<=indexTo; i++) h->second[i] = !h->second[i];

    // remove sequence from store if no highlights left
    for (i=0; i<h->second.size(); i++)
        if (h->second[i] == true) break;
    if (i == h->second.size())
        highlights.erase(h);

    PostRedrawAllSequenceViewers();
    RedrawMoleculesWithIdentifier(identifier, set);
}

void Messenger::ToggleHighlights(const Sequence *sequence, int seqIndexFrom, int seqIndexTo)
{
    ToggleHighlights(sequence->identifier, seqIndexFrom, seqIndexTo, sequence->parentSet);
}

void Messenger::ToggleHighlight(const Molecule *molecule, int residueID)
{
    // assume index = id - 1
    ToggleHighlights(molecule->identifier, residueID - 1, residueID - 1, molecule->parentSet);
}

bool Messenger::RemoveAllHighlights(bool postRedraws)
{
    bool anyRemoved = highlights.size() > 0;

    if (postRedraws) {
        if (anyRemoved) PostRedrawAllSequenceViewers();

        MoleculeHighlightMap::const_iterator h, he = highlights.end();
        for (h=highlights.begin(); h!=he; h++)
            RedrawMoleculesWithIdentifier(h->first, structureWindow->glCanvas->structureSet);
    }

    highlights.clear();

    return anyRemoved;
}

void Messenger::SetHighlights(const MoleculeHighlightMap& newHighlights)
{
    RemoveAllHighlights(true);
    highlights = newHighlights;

    PostRedrawAllSequenceViewers();
    MoleculeHighlightMap::const_iterator h, he = highlights.end();
    for (h=highlights.begin(); h!=he; h++)
        RedrawMoleculesWithIdentifier(h->first, structureWindow->glCanvas->structureSet);
}

void Messenger::SuspendHighlighting(bool suspend)
{
    if (highlightingSuspended != suspend) {
        highlightingSuspended = suspend;
        if (IsAnythingHighlighted()) {
            PostRedrawAllStructures();
            PostRedrawAllSequenceViewers();
        }
    }
}

bool Messenger::GetHighlightedResiduesWithStructure(MoleculeHighlightMap *residues) const
{
    residues->clear();
    if (!IsAnythingHighlighted()) return false;

    MoleculeHighlightMap::const_iterator h, he = highlights.end();
    for (h=highlights.begin(); h!=he; h++) {
        if (h->first->HasStructure())
            (*residues)[h->first] = h->second;
    }

    return (residues->size() > 0);
}

CBiostruc_annot_set * Messenger::CreateBiostrucAnnotSetForHighlightsOnSingleObject(void) const
{
    if (!IsAnythingHighlighted()) {
        ERR_POST(Error << "Nothing highlighted");
        return NULL;
    }

    // check to see that all highlights are on a single structure object
    int mmdbID;
    MoleculeHighlightMap::const_iterator h, he = highlights.end();
    for (h=highlights.begin(); h!=he; h++) {
        if (h == highlights.begin()) mmdbID = h->first->mmdbID;
        if (h->first->mmdbID == MoleculeIdentifier::VALUE_NOT_SET || h->first->mmdbID != mmdbID) {
            ERR_POST(Error << "All highlights must be on a single PDB structure");
            return NULL;
        }
        if (h->first->moleculeID == MoleculeIdentifier::VALUE_NOT_SET) {
            ERR_POST(Error << "internal error - MoleculeIdentifier has no moleculeID");
            return NULL;
        }
    }

    // create the Biostruc-annot-set
    CRef < CBiostruc_annot_set > bas(new CBiostruc_annot_set());

    // set id
    CRef < CBiostruc_id > bid(new CBiostruc_id());
    bas->SetId().push_back(bid);
    bid->SetMmdb_id().Set(mmdbID);

    // create feature set and feature
    CRef < CBiostruc_feature_set > bfs(new CBiostruc_feature_set());
    bas->SetFeatures().push_back(bfs);
    bfs->SetId().Set(1);
    CRef < CBiostruc_feature > bf(new CBiostruc_feature());
    bfs->SetFeatures().push_back(bf);

    // create Chem-graph-pntrs with residues
    CChem_graph_pntrs *cgp = new CChem_graph_pntrs();
    bf->SetLocation().SetSubgraph(*cgp);
    CResidue_pntrs *rp = new CResidue_pntrs();
    cgp->SetResidues(*rp);

    // add all residue intervals
    for (h=highlights.begin(); h!=he; h++) {
        int first = 0, last = 0;
        while (first < h->second.size()) {

            // find first highlighted residue
            while (first < h->second.size() && !h->second[first]) first++;
            if (first >= h->second.size()) break;
            // find last in contiguous stretch of highlighted residues
            last = first;
            while (last + 1 < h->second.size() && h->second[last + 1]) last++;

            // add new interval to list
            CRef < CResidue_interval_pntr > rip(new CResidue_interval_pntr());
            rip->SetMolecule_id().Set(h->first->moleculeID);
            rip->SetFrom().Set(first + 1);  // assume residueID == index + 1
            rip->SetTo().Set(last + 1);
            rp->SetInterval().push_back(rip);

            first = last + 2;
        }
    }

    return bas.Release();
}

void Messenger::SetSequenceViewerTitles(const std::string& title) const
{
    SequenceViewerList::const_iterator q, qe = sequenceViewers.end();
    for (q=sequenceViewers.begin(); q!=qe; q++)
        (*q)->SetWindowTitle(title);
}

END_SCOPE(Cn3D)

