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
*      Interface to Alejandro's block alignment algorithm
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2002/07/26 15:28:45  thiessen
* add Alejandro's block alignment algorithm
*
* ===========================================================================
*/

#ifndef CN3D_BA_INTERFACE__HPP
#define CN3D_BA_INTERFACE__HPP

#include <corelib/ncbistl.hpp>

#include <list>


BEGIN_SCOPE(Cn3D)

class BlockMultipleAlignment;

class BlockAligner
{
public:
    BlockAligner(void) { }
    ~BlockAligner(void) { }

    typedef std::list < BlockMultipleAlignment * > AlignmentList;

    // creates new pairwise alignments (as two-row BlockMultipleAlignments), each of which has
    // the given master and one of the given sequences. If the alignment algorithm fails to
    // align the new sequence, it will include a null-alignment for that sequence.
    void CreateNewPairwiseAlignmentsByBlockAlignment(const BlockMultipleAlignment *multiple,
        const AlignmentList& toRealign, AlignmentList *newAlignments);
};

END_SCOPE(Cn3D)

#endif // CN3D_BA_INTERFACE__HPP
