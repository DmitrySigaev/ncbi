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
*      Classes to hold sets of sequences
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2000/08/27 18:50:56  thiessen
* extract sequence information
*
* ===========================================================================
*/

#ifndef CN3D_SEQUENCE_SET__HPP
#define CN3D_SEQUENCE_SET__HPP

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Bioseq.hpp>

#include "cn3d/structure_base.hpp"


BEGIN_SCOPE(Cn3D)

typedef list< ncbi::CRef< ncbi::objects::CSeq_entry > > SeqEntryList;

class Sequence;
class Molecule;

class SequenceSet : public StructureBase
{
public:
    SequenceSet(StructureBase *parent, const SeqEntryList& seqEntries);

    typedef LIST_TYPE < const Sequence * > SequenceList;
    SequenceList sequences;

    bool Draw(const AtomSet *atomSet = NULL) const { return false; } // not drawable
};

class Sequence : public StructureBase
{
public:
    Sequence(StructureBase *parent, const ncbi::objects::CBioseq& bioseq);

    static const int NOT_SET;
    int gi, pdbChain;
    std::string pdbID, sequenceString;

    typedef LIST_TYPE < const Molecule * > MoleculeList;
    MoleculeList molecules;
};

END_SCOPE(Cn3D)

#endif // CN3D_SEQUENCE_SET__HPP
