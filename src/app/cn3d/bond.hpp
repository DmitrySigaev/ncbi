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
*      Classes to hold chemical bonds
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2000/07/11 13:49:26  thiessen
* add modules to parse chemical graph; many improvements
*
* ===========================================================================
*/

#ifndef CN3D_BOND__HPP
#define CN3D_BOND__HPP

#include <objects/mmdb1/Inter_residue_bond.hpp>
#include <objects/mmdb1/Atom_pntr.hpp>

#include "cn3d/structure_base.hpp"

USING_NCBI_SCOPE;
using namespace objects;

BEGIN_SCOPE(Cn3D)

// A Bond is a link between two atoms, referenced by Atom-pntr (molecule,
// residue, and atom IDs).

class Bond : public StructureBase
{
public:
    enum eBondOrder {
        eSingle = CInter_residue_bond::eBond_order_single, 
        ePartialDouble = CInter_residue_bond::eBond_order_partial_double, 
        eAromatic = CInter_residue_bond::eBond_order_aromatic, 
        eDouble = CInter_residue_bond::eBond_order_double,
        eTriple = CInter_residue_bond::eBond_order_triple,
        eOther = CInter_residue_bond::eBond_order_other,
        eUnknown = CInter_residue_bond::eBond_order_unknown 
    };

    Bond(StructureBase *parent, const CAtom_pntr& atomPtr1, const CAtom_pntr& atomPtr2, 
        int bondOrder = eUnknown);
    Bond(StructureBase *parent, int mID1, int rID1, int aID1, int mID2, int rID2, int aID2, 
        int bondOrder = eUnknown);
    //~Bond(void);

    // public data
    typedef struct {
        int mID, rID, aID;
    } AtomPntr;
    AtomPntr atom1, atom2;
    eBondOrder order;

    // public methods
    bool Draw(void) const;

private:
};

END_SCOPE(Cn3D)

#endif // CN3D_BOND__HPP
