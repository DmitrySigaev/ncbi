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
*      Classes to hold sets of structure data
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.7  2000/07/12 02:00:39  thiessen
* add basic wxWindows GUI
*
* Revision 1.6  2000/07/11 13:49:30  thiessen
* add modules to parse chemical graph; many improvements
*
* Revision 1.5  2000/07/01 15:44:23  thiessen
* major improvements to StructureBase functionality
*
* Revision 1.4  2000/06/29 19:18:19  thiessen
* improved atom map
*
* Revision 1.3  2000/06/29 14:35:20  thiessen
* new atom_set files
*
* Revision 1.2  2000/06/28 13:08:13  thiessen
* store alt conf ensembles
*
* Revision 1.1  2000/06/27 20:08:14  thiessen
* initial checkin
*
* ===========================================================================
*/

#ifndef CN3D_STRUCTURESET__HPP
#define CN3D_STRUCTURESET__HPP

#include <string>

#include <objects/ncbimime/Ncbi_mime_asn1.hpp>
#include <objects/mmdb1/Biostruc.hpp>

#include "cn3d/structure_base.hpp"

USING_NCBI_SCOPE;
using namespace objects;

BEGIN_SCOPE(Cn3D)

// StructureSet is the top-level container. It holds a set of SturctureObjects;
// A SturctureObject is basically the contents of one PDB entry.

class StructureObject;

class StructureSet : public StructureBase
{
public:
    StructureSet(const CNcbi_mime_asn1& mime);
    //~StructureSet(void);

    // public data
    typedef LIST_TYPE < const StructureObject * > ObjectList;
    ObjectList objects;

    // public methods
    bool Draw(void) const;

private:
};

class ChemicalGraph;
class CoordSet;

class StructureObject : public StructureBase
{
public:
    StructureObject(StructureBase *parent, const CBiostruc& biostruc, bool master);
    //~StructureObject(void);

    // public data
    const bool isMaster;
    static const int NO_MMDB_ID;
    int mmdbID;
    std::string pdbID;

    // an object has one ChemicalGraph that can be applied to one or more 
    // CoordSets to generate the object's model(s)
    ChemicalGraph *graph;
    typedef LIST_TYPE < const CoordSet * > CoordSetList;
    CoordSetList coordSets;

    // public methods
    bool Draw(void) const;

private:
};

END_SCOPE(Cn3D)

#endif // CN3D_STRUCTURESET__HPP
