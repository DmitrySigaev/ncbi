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
*       class to isolate and run the threader
*
* ===========================================================================
*/

#ifndef CN3D_THREADER__HPP
#define CN3D_THREADER__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>

#include <map>
#include <list>
#include <vector>

#include <thrdatd.h>
#include <blastkar.h>

#include "vector_math.hpp"


BEGIN_SCOPE(Cn3D)

// for convenience, optional threading parameters are all specified in this block
class ThreaderOptions
{
public:
    bool mergeAfterEachSequence;
    bool freezeIsolatedBlocks;
    double weightPSSM;
    double loopLengthMultiplier;
    int nRandomStarts;
	int nResultAlignments;
    int terminalResidueCutoff;

    ThreaderOptions(void);
};

extern ThreaderOptions globalThreaderOptions;

class BlockMultipleAlignment;
class Sequence;
class StructureBase;

class Threader
{
public:
    Threader(void);
    ~Threader(void);

    static const int SCALING_FACTOR;
    static const std::string ThreaderResidues;

    // create new BlockMultipleAlignments from the given multiple and master/slave pairs; returns
    // true if threading successful. If so, depending on options, nRowsAddedToMultiple will be
    // merged into the multiple, and newAlignments will contain all un-merged master/slave pairs
    typedef std::list < BlockMultipleAlignment * > AlignmentList;
    bool Realign(const ThreaderOptions& options, BlockMultipleAlignment *masterMultiple,
        const AlignmentList *originalAlignments,
        int *nRowsAddedToMultiple, AlignmentList *newAlignments);

    // calculate scores for each row in the alignment (and store them in the alignment itself)
    bool CalculateScores(const BlockMultipleAlignment *multiple, double weightPSSM);

    // geometry violations - for each row of an alignment, get a list of seqIndex
    // (from, to) pairs for offending regions; return total number of violations
    typedef std::list < std::pair < int, int > > IntervalList;
    typedef std::vector < IntervalList > GeometryViolationsForRow;
    int GetGeometryViolations(const BlockMultipleAlignment *multiple,
        GeometryViolationsForRow *violations);

    // estimate the number of random starts needed to thread an alignment based on
    // the number of blocks to be aligned
    static int EstimateNRandomStarts(const BlockMultipleAlignment *coreAlignment,
        const BlockMultipleAlignment *toBeThreaded);

    // to hold virtual residue, sidechain positions
    enum { MISSING_COORDINATE = 0, VIRTUAL_RESIDUE, VIRTUAL_PEPTIDE };
    typedef struct {
        unsigned char type;
        Vector coord;
        int disulfideWith;    // if Cysteine, virtual coord index of any disulfide-bound Cys; -1 otherwise
    } VirtualCoordinate;
    typedef std::vector < VirtualCoordinate > VirtualCoordinateList;

    // for (temporary) storage of contacts
    typedef struct {
        int
          vc1, vc2,     // virtual coord index
          distanceBin;
    } Contact;
    typedef std::list < Contact > ContactList;

private:
    // holds Fld_Mtf structures already calculated for a given object (Molecule or Sequence)
    typedef std::map < const StructureBase *, Fld_Mtf * > ContactMap;
    ContactMap contacts;

    // threading structure setups
    Cor_Def * CreateCorDef(const BlockMultipleAlignment *multiple, double loopLengthMultiplier);
    Qry_Seq * CreateQrySeq(const BlockMultipleAlignment *multiple,
        const BlockMultipleAlignment *pairwise, int terminalCutoff);
    Rcx_Ptl * CreateRcxPtl(double weightContacts);
    Gib_Scd * CreateGibScd(bool fast, int nRandomStarts);
    Fld_Mtf * CreateFldMtf(const Sequence *masterSequence);
public:
    // also used by blast module
    static Seq_Mtf * CreateSeqMtf(const BlockMultipleAlignment *multiple,
        double weightPSSM, BLAST_KarlinBlkPtr karlinBlock);
};

END_SCOPE(Cn3D)

#endif // CN3D_THREADER__HPP

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.19  2004/02/19 17:04:52  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.18  2003/11/06 18:52:32  thiessen
* make geometry violations shown on/off; allow multiple pmid entry in ref dialog
*
* Revision 1.17  2003/02/03 19:20:03  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.16  2003/01/23 20:03:05  thiessen
* add BLAST Neighbor algorithm
*
* Revision 1.15  2002/07/12 13:24:10  thiessen
* fixes for PSSM creation to agree with cddumper/RPSBLAST
*
* Revision 1.14  2002/03/28 14:06:02  thiessen
* preliminary BLAST/PSSM ; new CD startup style
*
* Revision 1.13  2002/02/21 12:26:30  thiessen
* fix row delete bug ; remember threader options
*
* Revision 1.12  2001/10/08 00:00:02  thiessen
* estimate threader N random starts; edit CDD name
*
* Revision 1.11  2001/09/27 15:36:48  thiessen
* decouple sequence import and BLAST
*
* Revision 1.10  2001/06/01 21:48:02  thiessen
* add terminal cutoff to threading
*
* Revision 1.9  2001/04/12 18:54:22  thiessen
* fix memory leak for PSSM-only threading
*
* Revision 1.8  2001/04/12 18:09:40  thiessen
* add block freezing
*
* Revision 1.7  2001/04/05 22:54:50  thiessen
* change bg color handling ; show geometry violations
*
* Revision 1.6  2001/04/04 00:27:21  thiessen
* major update - add merging, threader GUI controls
*
* Revision 1.5  2001/03/30 03:07:08  thiessen
* add threader score calculation & sorting
*
* Revision 1.4  2001/03/28 23:01:38  thiessen
* first working full threading
*
* Revision 1.3  2001/03/22 00:32:36  thiessen
* initial threading working (PSSM only); free color storage in undo stack
*
* Revision 1.2  2001/02/13 01:03:03  thiessen
* backward-compatible domain ID's in output; add ability to delete rows
*
* Revision 1.1  2001/02/08 23:01:24  thiessen
* hook up C-toolkit stuff for threading; working PSSM calculation
*
*/
