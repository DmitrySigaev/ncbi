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
*      Base class for structure data
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.9  2000/11/11 21:15:55  thiessen
* create Seq-annot from BlockMultipleAlignment
*
* Revision 1.8  2000/08/18 23:07:09  thiessen
* minor efficiency tweaks
*
* Revision 1.7  2000/08/11 12:58:31  thiessen
* added worm; get 3d-object coords from asn1
*
* Revision 1.6  2000/08/07 00:21:18  thiessen
* add display list mechanism
*
* Revision 1.5  2000/08/04 22:49:04  thiessen
* add backbone atom classification and selection feedback mechanism
*
* Revision 1.4  2000/08/03 15:12:23  thiessen
* add skeleton of style and show/hide managers
*
* Revision 1.3  2000/07/16 23:19:11  thiessen
* redo of drawing system
*
* Revision 1.2  2000/07/11 13:45:31  thiessen
* add modules to parse chemical graph; many improvements
*
* Revision 1.1  2000/07/01 15:47:18  thiessen
* major improvements to StructureBase functionality
*
* ===========================================================================
*/

#include "cn3d/structure_base.hpp"
#include "cn3d/structure_set.hpp"

USING_NCBI_SCOPE;

BEGIN_SCOPE(Cn3D)

const int AtomPntr::NO_ID = -1;

// store children in parent upon construction
StructureBase::StructureBase(StructureBase *parent)
{
    if (parent) {
        parent->_AddChild(this);
        if (!(parentSet = parent->parentSet))
            ERR_POST(Error << "NULL parent->parentSet");
    } else {
        parentSet = NULL;
    }
    _parent = parent;
}

// delete children upon destruction
StructureBase::~StructureBase(void)
{
    _ChildList::iterator i, e=_children.end();
    for (i=_children.begin(); i!=e; i++) 
        delete *i;
}

// draws the object and all its children - halt upon false return from Draw or DrawAll
bool StructureBase::DrawAll(const AtomSet *atomSet) const
{
    if (!parentSet->renderer) return false;
    if (!Draw(atomSet)) return true;
    _ChildList::const_iterator i, e=_children.end();
    for (i=_children.begin(); i!=e; i++) 
        if (!((*i)->DrawAll(atomSet))) return true;
	return true;
}

void StructureBase::_AddChild(StructureBase *child)
{
    _children.push_back(child);
}

void StructureBase::_RemoveChild(const StructureBase *child)
{
    _ChildList::iterator i, ie=_children.end();
    for (i=_children.begin(); i!=ie; i++) {
        if (*i == child) {
            _children.erase(i);
            return;
        }
    }
    ERR_POST(Warning << "attempted to remove non-existent child");
}

END_SCOPE(Cn3D)

