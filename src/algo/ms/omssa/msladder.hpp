/* $Id$
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
 *  Please cite the authors in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors:  Lewis Y. Geer
 *
 * File Description:
 *    Classes to deal with ladders of m/z values
 *
 * ===========================================================================
 */

#ifndef MSLADDER__HPP
#define MSLADDER__HPP

#include <msms.hpp>

#include <set>
#include <iostream>
#include <vector>

#include <corelib/ncbistd.hpp>
#include <objects/omssa/omssa__.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(omssa)


// container for mass ladders
typedef int * THit;

// max size of ladder
static const int kMSLadderMax = 10000;

/////////////////////////////////////////////////////////////////////////////
//
//  CLadder::
//
//  Contains a theoretical m/z ladder
//

class CLadder {
public:

    // 'tor's
    ~CLadder();
    CLadder(void);
    CLadder(int SizeIn);
    CLadder(const CLadder& Old);

    // vector operations on the ladder
    int& operator [] (int n);
    unsigned size(void);
    void push_back(int val);
    void clear(void);

    // make a ladder
    bool CreateLadder(int IonType, int Charge, char *Sequence, int SeqIndex,
		      int start, int stop, int mass,
		      CMassArray& MassArray, CAA &AA,
		      bool *ModMask,
		      int NumMod);
    
    // getter setters
    THit GetHit(void);
    int GetStart(void);
    int GetStop(void);
    int GetSeqIndex(void);
    int GetType(void);
    int GetMass(void);
    int GetCharge(void);

    // sees if ladder contains the given mass value
    bool Contains(int MassIndex, int Tolerance);

    // or's hitlist with hitlist from other ladder
    // takes into account direction
    void Or(CLadder& LadderIn);

    // count the number of matches
    int HitCount(void);

    // clear the Hitlist
    void ClearHits(void);

private:
    int *Ladder;
    unsigned LadderSize;  // size of allocated buffer
    int LadderIndex; // current end of the ladder
    THit Hit;
    int Start, Stop;  // inclusive start and stop position in sequence
    int Index;  // gi or position in blastdb
    int Type;  // ion type
    int Mass;  // *neutral* precursor mass (Mr)
    int Charge;
};


/////////////////// CLadder inline methods

inline int& CLadder::operator [] (int n) 
{ 
    return Ladder[n]; 
}

inline unsigned CLadder::size(void) 
{ 
    return LadderIndex; 
}

inline void CLadder::push_back(int val) 
{ 
    Ladder[LadderIndex] = val; 
    LadderIndex++;
}

inline void CLadder::clear(void) 
{ 
    LadderIndex = 0; 
}

inline THit CLadder::GetHit(void) 
{ 
    return Hit; 
}

inline int CLadder::GetStart(void) 
{ 
    return Start; 
}

inline int CLadder::GetStop(void) 
{ 
    return Stop;
}

inline int CLadder::GetSeqIndex(void) 
{ 
    return Index; 
}

inline int CLadder::GetType(void) 
{ 
    return Type; 
}

inline int CLadder::GetMass(void) 
{ 
    return Mass;
}

inline int CLadder::GetCharge(void) 
{ 
    return Charge;
}

/////////////////// end of CLadder inline methods

END_SCOPE(omssa)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif

/*
  $Log$
  Revision 1.1  2003/10/20 21:32:13  lewisg
  ommsa toolkit version

  Revision 1.10  2003/10/07 18:02:28  lewisg
  prep for toolkit

  Revision 1.9  2003/10/06 18:14:17  lewisg
  threshold vary

  Revision 1.8  2003/08/14 23:49:22  lewisg
  first pass at variable mod

  Revision 1.7  2003/07/17 18:45:49  lewisg
  multi dta support

  Revision 1.6  2003/07/07 16:17:51  lewisg
  new poisson distribution and turn off histogram

  Revision 1.5  2003/05/01 14:52:10  lewisg
  fixes to scoring

  Revision 1.4  2003/04/24 18:45:55  lewisg
  performance enhancements to ladder creation and peak compare

  Revision 1.3  2003/04/18 20:46:52  lewisg
  add graphing to omssa

  Revision 1.2  2003/04/02 18:49:50  lewisg
  improved score, architectural changes

  Revision 1.1  2003/03/21 21:14:40  lewisg
  merge ming's code, other stuff


*/
