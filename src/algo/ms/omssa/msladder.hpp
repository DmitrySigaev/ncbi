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

#include <corelib/ncbimisc.hpp>
#include <objects/omssa/omssa__.hpp>

#include <set>
#include <iostream>
#include <vector>

#include "msms.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(omssa)


// container for mass ladders
typedef int THit;

// max size of ladder
const int kMSLadderMax = 10000;

/////////////////////////////////////////////////////////////////////////////
//
//  CLadder::
//
//  Contains a theoretical m/z ladder
//

class NCBI_XOMSSA_EXPORT CLadder : public CObject {
public:

    // 'tor's
    ~CLadder();
    CLadder(void);
    CLadder(int SizeIn);
    CLadder(const CLadder& Old);

    // vector operations on the ladder
    int& operator [] (int n);
    int size(void);
    void clear(void);

    /**
     *  make a ladder
     * 
     * @param IonType the ion series to create
     * @param Charge the charge of the series
     * @param Sequence the protein sequence
     * @param SeqIndex the position in the blast library
     * @param start start position in the sequence
     * @param stop the stop position in the sequence
     * @param MassArray AA masses
     * @param AA used for mass calculation
     * @param ModMask bit mask of modifications to use
     * @param ModList modification information
     * @param NumMod the total number of mods
     * @param Settings search settings
     * 
     * @return false if fails
     */
    const bool CreateLadder(const int IonType,
                            const int Charge,
                            const char * const Sequence,
                            const int SeqIndex,
                            const int start,
                            const int stop,
                            const int mass,
                            const CMassArray& MassArray, 
                            const CAA &AA,
                            const unsigned ModMask,
                            const CMod ModList[],
                            const int NumMod,
                            const CMSSearchSettings& Settings
                            );

    /**
     * calculate the mass difference
     *
     * @param IntMassArray amino acid masses
     * @param AAMap convert aa to index
     * @param Sequence the sequence
     * @param Offset start position in the sequence
     * @param Direction the direction of the ion series
     * @param NumMod number of modifications
     * @param ModIndex position of the modifications
     * @param ModList modification info
     * @param ModMask modification mask
     * @param i index into sequence
     * @param ion the ladder
     * @param Charge charge state
     * @param Setting search settings
     */
    bool CalcDelta(const int *IntMassArray,
                   const char * const AAMap,
                   const char *Sequence,
                   int Offset, 
                   int Direction, 
                   int NumMod, 
                   int &ModIndex,
                   const CMod ModList[],
                   unsigned ModMask,
                   int i,
                   int& ion,
                   const int Charge, 
                   const CMSSearchSettings& Settings);


    // check if modification mask position is set
    bool MaskSet(unsigned ModMask, int ModIndex);

    // getter setters
    THit * GetHit(void);
    int GetStart(void);
    int GetStop(void);
    int GetSeqIndex(void);
    int GetType(void);
    int GetMass(void);
    int GetCharge(void);

    // sees if ladder contains the given mass value
    bool Contains(int MassIndex, int Tolerance);
    bool ContainsFast(int MassIndex, int Tolerance);

    // or's hitlist with hitlist from other ladder
    // takes into account direction
    void Or(CLadder& LadderIn);

    // count the number of matches
    int HitCount(void);

    // clear the Hitlist
    void ClearHits(void);

private:
    int LadderIndex; // current end of the ladder
    AutoPtr <int, ArrayDeleter<int> > Ladder;
    AutoPtr <THit, ArrayDeleter<THit> > Hit;
    int LadderSize;  // size of allocated buffer
    int Start, Stop;  // inclusive start and stop position in sequence
    int Index;  // gi or position in blastdb
    int Type;  // ion type
    int Mass;  // *neutral* precursor mass (Mr)
    int Charge;
};


/////////////////// CLadder inline methods

inline
bool CLadder::CalcDelta(const int *IntMassArray, 
                        const char * const AAMap, 
                        const char *Sequence,
                        int Offset, 
                        int Direction, 
                        int NumMod, 
                        int &ModIndex,
                        const CMod ModList[], 
                        unsigned ModMask, 
                        int i,
                        int& ion,
                        const int Charge, 
                        const CMSSearchSettings& Settings)
{
    int delta = IntMassArray[AAMap[Sequence[Offset + Direction*i]]];
    if(!delta) return false; // unusable char (-BXZ*)

    // check for mods
    while(NumMod > 0 && ModIndex >= 0 && ModIndex < NumMod &&
        ModList[ModIndex].GetSite() == &(Sequence[Offset + Direction*i])) {
        if (MaskSet(ModMask, ModIndex)) { 
            delta += ModList[ModIndex].GetProductDelta();
        }
        ModIndex += Direction;
    }

    // increment the ladder

    // first check to see if exact mass increment needed
    if(Settings.GetProductsearchtype() == eMSSearchType_exact) {
        if ((ion * Charge)/MSSCALE2INT(Settings.GetExactmass()) != 
            (ion * Charge + delta) / MSSCALE2INT(Settings.GetExactmass()))
            ion += MSSCALE2INT(kNeutron)/Charge;
    }

    ion += delta/Charge;
    return true;
}

inline int& CLadder::operator [] (int n) 
{ 
    return *(Ladder.get() + n); 
}

inline int CLadder::size(void) 
{ 
    return LadderIndex; 
}

inline void CLadder::clear(void) 
{ 
    LadderIndex = 0; 
}

inline THit * CLadder::GetHit(void) 
{ 
    return Hit.get(); 
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

// count the number of matches
inline int CLadder::HitCount(void)
{
    int i, retval(0);
    for(i = 0; i < LadderIndex && i < LadderSize; i++)
	retval += *(Hit.get() + i);
    return retval;
}

// clear the Hitlist
inline void CLadder::ClearHits(void)
{
    int i;
    for(i = 0; i < LadderIndex && i < LadderSize; i++)
	*(Hit.get() + i) = 0;
}

inline bool CLadder::MaskSet(unsigned ModMask, int ModIndex)
{
    return (bool) (ModMask & (1 << ModIndex));
}


/////////////////// end of CLadder inline methods

END_SCOPE(omssa)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif

/*
  $Log$
  Revision 1.18  2005/11/08 20:36:07  lewisg
  exact mass for multiply charged ions

  Revision 1.17  2005/10/24 21:46:13  lewisg
  exact mass, peptide size limits, validation, code cleanup

  Revision 1.16  2005/09/20 21:07:57  lewisg
  get rid of c-toolkit dependencies and nrutil

  Revision 1.15  2005/09/14 15:30:17  lewisg
  neutral loss

  Revision 1.14  2005/05/19 16:59:17  lewisg
  add top-down searching, fix variable mod bugs

  Revision 1.13  2005/05/13 17:57:17  lewisg
  one mod per site and bug fixes

  Revision 1.12  2005/03/14 22:29:54  lewisg
  add mod file input

  Revision 1.11  2005/01/11 21:08:43  lewisg
  average mass search

  Revision 1.10  2004/11/17 23:42:11  lewisg
  add cterm pep mods, fix prob for tophitnum

  Revision 1.9  2004/11/15 15:32:40  lewisg
  memory overwrite fixes

  Revision 1.8  2004/05/27 20:52:15  lewisg
  better exception checking, use of AutoPtr, command line parsing

  Revision 1.7  2004/03/30 19:36:59  lewisg
  multiple mod code

  Revision 1.6  2004/03/16 20:18:54  gorelenk
  Changed includes of private headers.

  Revision 1.5  2003/12/22 21:57:59  lewisg
  top hit code and variable mod fixes

  Revision 1.4  2003/11/18 18:16:03  lewisg
  perf enhancements, ROCn adjusted params made default

  Revision 1.3  2003/10/24 21:28:41  lewisg
  add omssa, xomssa, omssacl to win32 build, including dll

  Revision 1.2  2003/10/21 21:12:16  lewisg
  reorder headers

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
