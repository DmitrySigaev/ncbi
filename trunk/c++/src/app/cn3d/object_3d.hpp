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
*      holds 3d-objects - helix cylinders and strand bricks
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2000/08/16 14:18:20  thiessen
* map 3-d objects to molecules
*
* Revision 1.2  2000/08/13 02:42:13  thiessen
* added helix and strand objects
*
* Revision 1.1  2000/08/11 18:24:41  thiessen
* add 3-d objects code
*
* ===========================================================================
*/

#ifndef CN3D_OBJECT_3D__HPP
#define CN3D_OBJECT_3D__HPP

#include <objects/mmdb3/Residue_pntrs.hpp>
#include <objects/mmdb3/Cylinder.hpp>
#include <objects/mmdb3/Brick.hpp>

#include "cn3d/structure_base.hpp"
#include "cn3d/vector_math.hpp"


BEGIN_SCOPE(Cn3D)

class Object3D : public StructureBase
{
public:
    static const int NOT_SET;
    int moleculeID, fromResidueID, toResidueID;

protected:
    Object3D(StructureBase *parent,
        const ncbi::objects::CResidue_pntrs& residues);
};

class Helix3D : public Object3D
{
public:
    Helix3D(StructureBase *parent,
        const ncbi::objects::CCylinder& cylinder, const ncbi::objects::CResidue_pntrs& residues);

    Vector Nterm, Cterm;

    bool Draw(const AtomSet *data) const;
};

class Strand3D : public Object3D
{
public:
    Strand3D(StructureBase *parent,
        const ncbi::objects::CBrick& brick, const ncbi::objects::CResidue_pntrs& residues);

    Vector Nterm, Cterm, unitNormal;

    bool Draw(const AtomSet *data) const;
};

END_SCOPE(Cn3D)

#endif // CN3D_OBJECT_3D__HPP
