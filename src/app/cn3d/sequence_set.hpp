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
* Revision 1.4  2000/08/29 04:34:15  thiessen
* working alignment manager, IBM
*
* Revision 1.3  2000/08/28 23:46:46  thiessen
* functional denseg and dendiag alignment parsing
*
* Revision 1.2  2000/08/28 18:52:18  thiessen
* start unpacking alignments
*
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
class MasterSlaveAlignment;

class SequenceSet : public StructureBase
{
public:
    SequenceSet(StructureBase *parent, const SeqEntryList& seqEntries);

    typedef LIST_TYPE < const Sequence * > SequenceList;
    SequenceList sequences;

    // there is one and only one sequence in this set that will be the master
    // for all alignments of sequences from this set
    const Sequence *master;

    bool Draw(const AtomSet *atomSet = NULL) const { return false; } // not drawable
};

class Sequence : public StructureBase
{
public:
    Sequence(StructureBase *parent, const ncbi::objects::CBioseq& bioseq);

    static const int NOT_SET;
    int gi, pdbChain;
    std::string pdbID, sequenceString;

    // corresponding protein chain
    const Molecule *molecule;

    // corresponding alignment with master (if slave)
    const MasterSlaveAlignment *alignment;

    int Length(void) const { return sequenceString.size(); }
};

END_SCOPE(Cn3D)

// check match by gi or pdbID
#define SAME_SEQUENCE(a, b) \
    (((a)->gi > 0 && (a)->gi == (b)->gi) || \
     ((a)->pdbID.size() > 0 && (a)->pdbID == (b)->pdbID && (a)->pdbChain == (b)->pdbChain))

#endif // CN3D_SEQUENCE_SET__HPP
