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
*      manager object to track show/hide status of objects at various levels
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include "show_hide_manager.hpp"
#include "structure_set.hpp"
#include "molecule.hpp"
#include "residue.hpp"
#include "chemical_graph.hpp"
#include "messenger.hpp"
#include "alignment_manager.hpp"
#include "opengl_renderer.hpp"
#include "cn3d_tools.hpp"
#include "molecule_identifier.hpp"

#include <corelib/ncbistre.hpp>
#include <vector>

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)


// ShowHideInfo is a generic container class to help organize the list of things that can be
// shown/hidden; stuff derived from it allows various types of objects to be shown/hidden

string indent("     ");

class ShowHideInfo
{
protected:
    string label;
public:
    vector < int > parentIndexes;
    void GetLabel(string *str) const { *str = label; }
    virtual bool IsVisible(const ShowHideManager *shm) const = 0;
    virtual void Show(ShowHideManager *shm, bool isShown) const = 0;
};

class ShowHideObject : public ShowHideInfo
{
private:
    const StructureObject *object;
public:
    ShowHideObject(const StructureObject *o) : object(o) { label = o->pdbID; }
    bool IsVisible(const ShowHideManager *shm) const { return shm->IsVisible(object); }
    void Show(ShowHideManager *shm, bool isShown) const { shm->Show(object, isShown); }
};

class ShowHideMolecule : public ShowHideInfo
{
private:
    const Molecule *molecule;
public:
    ShowHideMolecule(const Molecule *m) : molecule(m)
    {
        label = indent + m->identifier->pdbID;
        if (m->identifier->pdbChain != ' ') {
            label += '_';
            label += (char) m->identifier->pdbChain;
        }
    }
    bool IsVisible(const ShowHideManager *shm) const { return shm->IsVisible(molecule); }
    void Show(ShowHideManager *shm, bool isShown) const { shm->Show(molecule, isShown); }
};

class ShowHideDomain : public ShowHideInfo
{
private:
    const Molecule *molecule;
    int domainID;
public:
    ShowHideDomain(const Molecule *m, int d, int labelNum) : molecule(m), domainID(d)
    {
        CNcbiOstrstream oss;
        oss << indent << indent << m->identifier->pdbID;
        if (m->identifier->pdbChain != ' ') oss << '_' << (char) m->identifier->pdbChain;
        oss << " d" << labelNum;
        label = (string) CNcbiOstrstreamToString(oss);
    }
    bool IsVisible(const ShowHideManager *shm) const
    {
        bool isVisible = false;
        for (int i=0; i<molecule->NResidues(); ++i) {
            if (molecule->residueDomains[i] == domainID &&
                shm->IsVisible(molecule->residues.find(i+1)->second)) {
                isVisible = true;   // return true if any residue from this domain is visible
                break;
            }
        }
        return isVisible;
    }
    void Show(ShowHideManager *shm, bool isShown) const
    {
        for (int i=0; i<molecule->NResidues(); ++i)
            if (molecule->residueDomains[i] == domainID)
                shm->Show(molecule->residues.find(i+1)->second, isShown);
    }
};


///// the ShowHideManager class /////

ShowHideManager::~ShowHideManager()
{
    for (unsigned int i=0; i<structureInfo.size(); ++i) delete structureInfo[i];
}

static void PostRedrawEntity(const StructureObject *object, const Molecule *molecule, const Residue *residue)
{
    if (residue) {
        const Molecule *m;
        if (residue->GetParentOfType(&m))
            GlobalMessenger()->PostRedrawMolecule(m);
    }

    else if (molecule) {
        GlobalMessenger()->PostRedrawMolecule(molecule);
    }

    else if (object) {
        // redraw all prot/nuc molecules
        ChemicalGraph::MoleculeMap::const_iterator m, me = object->graph->molecules.end();
        for (m=object->graph->molecules.begin(); m!=me; ++m) {
            if (m->second->IsProtein() || m->second->IsNucleotide())
                GlobalMessenger()->PostRedrawMolecule(m->second);
        }
    }

    GlobalMessenger()->PostRedrawAllSequenceViewers();
}

void ShowHideManager::Show(const StructureBase *entity, bool isShown)
{
    // make sure this is a valid entity
    const StructureObject *object = dynamic_cast<const StructureObject *>(entity);
    const Molecule *molecule = dynamic_cast<const Molecule *>(entity);
    const Residue *residue = dynamic_cast<const Residue *>(entity);
    if (!entity || !(object || molecule || residue)) {
        ERRORMSG("ShowHideManager::Show() - must be a StructureObject, Molecule, or Residue");
        return;
    }

    EntitiesHidden::iterator e = entitiesHidden.find(entity);

    // hide an entity that's not already hidden
    if (!isShown && e == entitiesHidden.end()) {
        entitiesHidden[entity] = true;
        PostRedrawEntity(object, molecule, residue);
        entity->parentSet->renderer->ShowAllFrames();
    }

    // show an entity that's currently hidden
    else if (isShown && e != entitiesHidden.end()) {
        UnHideEntityAndChildren(entity);
        PostRedrawEntity(object, molecule, residue);
        entity->parentSet->renderer->ShowAllFrames();
    }
}

void ShowHideManager::UnHideEntityAndChildren(const StructureBase *entity)
{
    if (!entity || !IsHidden(entity)) return;
    const StructureObject *object = dynamic_cast<const StructureObject *>(entity);
    const Molecule *molecule = dynamic_cast<const Molecule *>(entity);
    const Residue *residue = dynamic_cast<const Residue *>(entity);

    // if entity is residue, just remove it from the list if present
    if (residue) {
        EntitiesHidden::iterator h = entitiesHidden.find(residue);
        if (h != entitiesHidden.end()) entitiesHidden.erase(h);
        return;
    }

    // otherwise, make sure entity and its descendents are visible
    // if it isn't already visible, then unhide this entity and all of its children
    EntitiesHidden::iterator h, he = entitiesHidden.end();
    for (h=entitiesHidden.begin(); h!=he; ) {

        const StructureObject *hObj = dynamic_cast<const StructureObject *>(h->first);
        if (object && !hObj)
            h->first->GetParentOfType(&hObj);
        const Molecule *hMol = dynamic_cast<const Molecule *>(h->first);
        if (molecule && hObj != h->first && !hMol)  // if not StructureObject or Molecule
            h->first->GetParentOfType(&hMol);       // must be residue

        if (entity == h->first ||               // unhide the entity itself
            (object && hObj == object) ||       // unhide children of a StructureObject
            (molecule && hMol == molecule))     // unhide children of a Molecule
        {
            EntitiesHidden::iterator d(h);
            ++h;
            entitiesHidden.erase(d);
        } else {
            ++h;
        }
    }
    PostRedrawEntity(object, molecule, residue);
    entity->parentSet->renderer->ShowAllFrames();
}

bool ShowHideManager::IsHidden(const StructureBase *entity) const
{
    if (entitiesHidden.size() == 0) return false;

    const StructureObject *object;
    const Molecule *molecule;
    const Residue *residue;

    EntitiesHidden::const_iterator e = entitiesHidden.find(entity);

    if ((object = dynamic_cast<const StructureObject *>(entity)) != NULL) {
        return (e != entitiesHidden.end());
    }

    else if ((molecule = dynamic_cast<const Molecule *>(entity)) != NULL) {
        if (!molecule->GetParentOfType(&object)) return false;
        return (entitiesHidden.find(object) != entitiesHidden.end() ||
                e != entitiesHidden.end());
    }

    else if ((residue = dynamic_cast<const Residue *>(entity)) != NULL) {
        if (!residue->GetParentOfType(&molecule) ||
            !molecule->GetParentOfType(&object)) return false;
        return (entitiesHidden.find(object) != entitiesHidden.end() ||
                entitiesHidden.find(molecule) != entitiesHidden.end() ||
                e != entitiesHidden.end());
    }

    ERRORMSG("ShowHideManager::IsHidden() - must be a StructureObject, Molecule, or Residue");
    return false;
}

void ShowHideManager::ConstructShowHideArray(const StructureSet *structureSet)
{
    ShowHideInfo *info;
    int objectIndex, moleculeIndex;

    StructureSet::ObjectList::const_iterator o, oe = structureSet->objects.end();
    for (o=structureSet->objects.begin(); o!=oe; ++o) {

        objectIndex = structureInfo.size();
        structureInfo.push_back(new ShowHideObject(*o));

        // list interesting (prot/nuc) chains
        ChemicalGraph::MoleculeMap::const_iterator m, me = (*o)->graph->molecules.end();
        for (m=(*o)->graph->molecules.begin(); m!=me; ++m) {
            int nDom = 1; // # domains in this chain

            if (m->second->IsProtein() || m->second->IsNucleotide()) {
                moleculeIndex = structureInfo.size();
                info = new ShowHideMolecule(m->second);
                info->parentIndexes.push_back(objectIndex);
                structureInfo.push_back(info);

                // if there at least one domain, enumerate them
                if (m->second->nDomains >= 1) {
                    StructureObject::DomainMap::const_iterator d, de = (*o)->domainMap.end();
                    for (d=(*o)->domainMap.begin(); d!=de; ++d) {
                        if (d->second == m->second) {
                            info = new ShowHideDomain(m->second, d->first, ++nDom);
                            info->parentIndexes.push_back(objectIndex);
                            info->parentIndexes.push_back(moleculeIndex);
                            structureInfo.push_back(info);
                        }
                    }
                }
            }
        }
    }
}

void ShowHideManager::GetShowHideInfo(
    vector < string > *names, vector < bool > *visibilities) const
{
    names->resize(structureInfo.size());
    visibilities->resize(structureInfo.size());
    for (unsigned int i=0; i<structureInfo.size(); ++i) {
        structureInfo[i]->GetLabel(&((*names)[i]));
        (*visibilities)[i] = structureInfo[i]->IsVisible(this);
    }
}

void ShowHideManager::ShowHideCallbackFunction(const vector < bool >& itemsEnabled)
{
    if (itemsEnabled.size() != structureInfo.size()) {
        ERRORMSG("ShowHideManager::ShowHideCallbackFunction() - wrong size list");
        return;
    }

    for (unsigned int i=0; i<itemsEnabled.size(); ++i)
        structureInfo[i]->Show(this, itemsEnabled[i]);
    TRACEMSG("entities hidden: " << entitiesHidden.size());
}

bool ShowHideManager::SelectionChangedCallback(
    const vector < bool >& original, vector < bool >& itemsEnabled)
{
    // count number of changes
    unsigned int i, nChanges = 0, itemChanged, nEnabled = 0, itemEnabled;
    for (i=0; i<itemsEnabled.size(); ++i) {
        if (itemsEnabled[i] != original[i]) {
            ++nChanges;
            itemChanged = i;
        }
        if (itemsEnabled[i]) {
            ++nEnabled;
            itemEnabled = i;
        }
    }

    // if change was a single de/selection, then turn off/on the children of that item
    bool anyChange = false;
    if (nChanges == 1 || nEnabled == 1) {
        int item = (nChanges == 1) ? itemChanged : itemEnabled;
        for (i=item+1; i<structureInfo.size(); ++i) {
            for (unsigned int j=0; j<structureInfo[i]->parentIndexes.size(); ++j) {
                if (structureInfo[i]->parentIndexes[j] == item) {
                    if (itemsEnabled[i] != itemsEnabled[item]) {
                        itemsEnabled[i] = itemsEnabled[item];
                        anyChange = true;
                    }
                }
            }
        }
    }

    // check all items to make sure that when an object is on, its parents are also on
    for (i=0; i<itemsEnabled.size(); ++i) {
        if (itemsEnabled[i]) {
            for (unsigned int j=0; j<structureInfo[i]->parentIndexes.size(); ++j) {
                if (!itemsEnabled[structureInfo[i]->parentIndexes[j]]) {
                    itemsEnabled[structureInfo[i]->parentIndexes[j]] = true;
                    anyChange = true;
                }
            }
        }
    }

    return anyChange;
}

void ShowHideManager::MakeAllVisible(void)
{
    while (entitiesHidden.size() > 0) Show(entitiesHidden.begin()->first, true);
}

void ShowHideManager::ShowAlignedDomains(const StructureSet *set)
{
    MakeAllVisible();
    StructureSet::ObjectList::const_iterator o, oe = set->objects.end();
    for (o=set->objects.begin(); o!=oe; ++o) {
        ChemicalGraph::MoleculeMap::const_iterator m, me = (*o)->graph->molecules.end();
        for (m=(*o)->graph->molecules.begin(); m!=me; ++m) {

            if (m->second->IsNucleotide()) {        // hide all nucleotides
                Show(m->second, false);
                continue;
            }
            if (!m->second->IsProtein()) continue;  // but leave all hets/solvents visible

            if (!set->alignmentManager->IsInAlignment(m->second->sequence)) {
                Show(m->second, false);
                continue;
            }

            map < int, bool > domains;
            Molecule::ResidueMap::const_iterator r, re = m->second->residues.end();

            // first pass determines which domains have any aligned residues
            for (r=m->second->residues.begin(); r!=re; ++r)
                if (set->alignmentManager->IsAligned(m->second->sequence, r->first - 1))
                    domains[m->second->residueDomains[r->first - 1]] = true;

            // second pass does hides domains not represented
            for (r=m->second->residues.begin(); r!=re; ++r)
                if (domains.find(m->second->residueDomains[r->first - 1]) == domains.end())
                    Show(r->second, false);
        }
    }
}


void ShowHideManager::PrivateShowResidues(const StructureSet *set, bool showAligned)
{
    StructureSet::ObjectList::const_iterator o, oe = set->objects.end();
    for (o=set->objects.begin(); o!=oe; ++o) {
        ChemicalGraph::MoleculeMap::const_iterator m, me = (*o)->graph->molecules.end();
        for (m=(*o)->graph->molecules.begin(); m!=me; ++m) {

            if (m->second->IsNucleotide()) {        // hide all nucleotides
                Show(m->second, false);
                continue;
            }
            if (!m->second->IsProtein()) continue;  // but leave all hets/solvents visible

            if (!set->alignmentManager->IsInAlignment(m->second->sequence)) {
                if (showAligned) Show(m->second, false);
                continue;
            }

            Molecule::ResidueMap::const_iterator r, re = m->second->residues.end();
            for (r=m->second->residues.begin(); r!=re; ++r) {
                bool aligned = set->alignmentManager->IsAligned(m->second->sequence, r->first - 1);
                if ((showAligned && !aligned) || (!showAligned && aligned))
                    Show(r->second, false);
            }
        }
    }
}

void ShowHideManager::ShowResidues(const StructureSet *set, bool showAligned)
{
    MakeAllVisible();
    PrivateShowResidues(set, showAligned);
}

void ShowHideManager::ShowUnalignedResiduesInAlignedDomains(const StructureSet *set)
{
    ShowAlignedDomains(set);
    PrivateShowResidues(set, false);
}

void ShowHideManager::ShowSelectedResidues(const StructureSet *set)
{
    MakeAllVisible();
    if (!GlobalMessenger()->IsAnythingHighlighted()) return;

    StructureSet::ObjectList::const_iterator o, oe = set->objects.end();
    for (o=set->objects.begin(); o!=oe; ++o) {
        bool anyResidueInObjectVisible = false;
        ChemicalGraph::MoleculeMap::const_iterator m, me = (*o)->graph->molecules.end();
        for (m=(*o)->graph->molecules.begin(); m!=me; ++m) {
            Molecule::ResidueMap::const_iterator r, re = m->second->residues.end();
            bool anyResidueInMoleculeVisible = false;
            for (r=m->second->residues.begin(); r!=re; ++r) {
                if (!GlobalMessenger()->IsHighlighted(m->second, r->first))
                    Show(r->second, false);
                else
                    anyResidueInMoleculeVisible = anyResidueInObjectVisible = true;
            }
            if (!anyResidueInMoleculeVisible) {
                for (r=m->second->residues.begin(); r!=re; ++r)
                    Show(r->second, true);  // un-flag individual residues
                Show(m->second, false);     // flag whole molecule as hidden
            }
        }
        if (!anyResidueInObjectVisible) {
            for (m=(*o)->graph->molecules.begin(); m!=me; ++m)
                Show(m->second, true);      // un-flag individual molecules
            Show(*o, false);                // flag whole object as hidden
        }
    }
}

void ShowHideManager::ShowDomainsWithHighlights(const StructureSet *set)
{
    // first, show all highlighted stuff
    MakeAllVisible();
    if (!GlobalMessenger()->IsAnythingHighlighted()) return;
    ShowSelectedResidues(set);

    // then, also show all domains that contain highlighted residues
    StructureSet::ObjectList::const_iterator o, oe = set->objects.end();
    for (o=set->objects.begin(); o!=oe; ++o) {
        ChemicalGraph::MoleculeMap::const_iterator m, me = (*o)->graph->molecules.end();
        for (m=(*o)->graph->molecules.begin(); m!=me; ++m) {
            Molecule::ResidueMap::const_iterator r, re = m->second->residues.end();

            // find domains in this molecule that have highlights
            map < int , bool > domains;
            int domain;
            for (r=m->second->residues.begin(); r!=re; ++r) {
                if (GlobalMessenger()->IsHighlighted(m->second, r->first)) {
                    domain = m->second->residueDomains[r->first - 1];
                    if (domain != Molecule::NO_DOMAIN_SET)
                        domains[domain] = true;
                }
            }

            // now show all residues in these domains
            for (r=m->second->residues.begin(); r!=re; ++r) {
                domain = m->second->residueDomains[r->first - 1];
                if (domain != Molecule::NO_DOMAIN_SET && domains.find(domain) != domains.end())
                    Show(r->second, true);
            }
        }
    }
}

END_SCOPE(Cn3D)

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.26  2005/10/26 18:36:05  thiessen
* minor fixes
*
* Revision 1.25  2005/10/22 02:50:34  thiessen
* deal with memory issues, mostly in ostrstream->string conversion
*
* Revision 1.24  2005/10/19 17:28:19  thiessen
* migrate to wxWidgets 2.6.2; handle signed/unsigned issue
*
* Revision 1.23  2004/05/21 21:41:40  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.22  2004/03/15 18:32:03  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.21  2004/02/19 17:05:13  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.20  2003/02/03 19:20:06  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.19  2002/11/10 20:32:04  thiessen
* show/hide optimizations, esp. show domains with highlights
*
* Revision 1.18  2002/10/28 21:36:01  thiessen
* add show domains with highlights
*
* Revision 1.17  2002/06/21 14:40:15  thiessen
* fix show/hide of nucleotides
*
* Revision 1.16  2001/10/08 14:18:33  thiessen
* fix show/hide dialog under wxGTK
*
* Revision 1.15  2001/08/10 15:01:57  thiessen
* fill out shortcuts; add update show/hide menu
*
* Revision 1.14  2001/07/12 17:35:15  thiessen
* change domain mapping ; add preliminary cdd annotation GUI
*
* Revision 1.13  2001/06/21 02:02:34  thiessen
* major update to molecule identification and highlighting ; add toggle highlight (via alt)
*
* Revision 1.12  2001/06/02 17:22:46  thiessen
* fixes for GCC
*
* Revision 1.11  2001/05/31 18:47:09  thiessen
* add preliminary style dialog; remove LIST_TYPE; add thread single and delete all; misc tweaks
*
* Revision 1.10  2001/05/17 18:34:26  thiessen
* spelling fixes; change dialogs to inherit from wxDialog
*
* Revision 1.9  2001/03/09 15:49:05  thiessen
* major changes to add initial update viewer
*
* Revision 1.8  2001/03/01 20:15:51  thiessen
* major rearrangement of sequence viewer code into base and derived classes
*
* Revision 1.7  2001/01/25 20:21:18  thiessen
* fix ostrstream memory leaks
*
* Revision 1.6  2000/12/29 19:23:39  thiessen
* save row order
*
* Revision 1.5  2000/12/19 16:39:09  thiessen
* tweaks to show/hide
*
* Revision 1.4  2000/12/15 15:51:47  thiessen
* show/hide system installed
*
* Revision 1.3  2000/12/01 19:35:57  thiessen
* better domain assignment; basic show/hide mechanism
*
* Revision 1.2  2000/08/17 18:33:12  thiessen
* minor fixes to StyleManager
*
* Revision 1.1  2000/08/03 15:13:59  thiessen
* add skeleton of style and show/hide managers
*
*/
