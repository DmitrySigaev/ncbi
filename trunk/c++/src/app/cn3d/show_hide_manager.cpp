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
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2000/12/01 19:35:57  thiessen
* better domain assignment; basic show/hide mechanism
*
* Revision 1.2  2000/08/17 18:33:12  thiessen
* minor fixes to StyleManager
*
* Revision 1.1  2000/08/03 15:13:59  thiessen
* add skeleton of style and show/hide managers
*
* ===========================================================================
*/

#include "cn3d/show_hide_manager.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/molecule.hpp"
#include "cn3d/residue.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

void ShowHideManager::Show(const StructureBase *entity, bool isShown)
{
    // make sure this is a valid entity
    if (!entity || !(
            dynamic_cast<const StructureObject *>(entity) ||
            dynamic_cast<const Molecule *>(entity) ||
            dynamic_cast<const Residue *>(entity))) {
        ERR_POST(Error << "ShowHideManager::Show() - must be a StructureObject, Molecule, or Residue");
        return;
    }

    EntitiesHidden::iterator e = entitiesHidden.find(entity);

    // hide an entity
    if (!isShown) {
        entitiesHidden[entity] = true;
    }

    // show an entity
    else {
        if (e != entitiesHidden.end()) entitiesHidden.erase(e);
    }
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

    ERR_POST(Error << "ShowHideManager::IsHidden() - must be a StructureObject, Molecule, or Residue");
    return false;
}

END_SCOPE(Cn3D)
