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
*      Classes to hold residues
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2000/07/11 13:49:29  thiessen
* add modules to parse chemical graph; many improvements
*
* ===========================================================================
*/

#ifndef CN3D_RESIDUE__HPP
#define CN3D_RESIDUE__HPP

#include <map>

#include <objects/mmdb1/Residue_graph.hpp>
#include <objects/mmdb1/Residue.hpp>

#include "cn3d/structure_base.hpp"

USING_NCBI_SCOPE;
using namespace objects;

BEGIN_SCOPE(Cn3D)

typedef list< CRef< CResidue_graph > > ResidueGraphList;

// a Residue is a set of bonds that connect one residue of a larger Molecule.
// Its constructor is where most of the work of decoding the ASN1 graph is done,
// based on the standard and local residue dictionaries. Each Residue also holds
// information (AtomInfo) about the nature of the atoms it contains.

class Bond;

class Residue : public StructureBase
{
public:
    Residue(StructureBase *parent,
        const CResidue& residue, int moleculeID,
        const ResidueGraphList& standardDictionary,
        const ResidueGraphList& localDictionary);
    ~Residue(void);

    // public data
    int id;
    static const char NO_CODE;
    char code;
    std::string name;

    typedef struct {
        std::string name, code;
        int atomicNumber;
        bool isIonizableProton;
    } AtomInfo;

    typedef LIST_TYPE < const Bond * > BondList;
    BondList bonds;

    // public methods
    bool HasCode(void) const { return (code != NO_CODE); }
    bool HasName(void) const { return (!name.empty()); }
    bool Draw(void) const;

private:
    typedef std::map < int , const AtomInfo * > AtomInfoMap;
    AtomInfoMap atomInfos;

public:
    const AtomInfo * GetAtomInfo(int aID) const
    { 
        AtomInfoMap::const_iterator info=atomInfos.find(aID);
        if (info != atomInfos.end()) return (*info).second;
        ERR_POST(Warning << "Residue #" << id << ": can't find atom #" << aID);
        return NULL;
    }
};

END_SCOPE(Cn3D)

#endif // CN3D_RESIDUE__HPP
