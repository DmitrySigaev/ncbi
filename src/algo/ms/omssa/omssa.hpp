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
 *    code to do the ms/ms search and score matches
 *
 * ===========================================================================
 */

#ifndef OMSSA__HPP
#define OMSSA__HPP

#include <objects/omssa/omssa__.hpp>

#include <msms.hpp>
#include <msladder.hpp>
#include <mspeak.hpp>

#include <readdb.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(omssa)

// max number missed cleavages + 1
#define MAXMISSEDCLEAVE 4
// max variable mods in peptide
#define MAXMOD 5
// number of possible ladders -- 2^MAXMOD
#define MAXMOD2 32

// for holding hits sorted by score
typedef multimap <double, CMSHit *> TScoreList;

/////////////////////////////////////////////////////////////////////////////
//
//  CSearch::
//
//  Performs the ms/ms search
//

class NCBI_XOMSSA_EXPORT CSearch {
public:
    CSearch(bool average); // is the search monoisotopic or average mass?
    ~CSearch();

    // init blast databases.  stream thru db if InitDB true
    int InitBlast(const char *blastdb, bool InitDb);

    // loads spectra into peaks
    void Spectrum2Peak(CMSRequest& MyRequest, CMSPeakSet& PeakSet,
		       int SingleWindow, int DoubleWindow);
    int Search(CMSRequest& MyRequest, CMSResponse& MyResponse, int Cutoff,
	       int SingleWindow, int DoubleWindow);

    // create the ladders from sequence
    int CreateLadders(unsigned char *Sequence, int iSearch, int position,
		      int endposition,
		      int **Masses, int iMissed, CAA& AA, 
		      CLadder& BLadder,
		      CLadder& YLadder, CLadder& B2Ladder,
		      CLadder& Y2Ladder,
		      bool *ModMask,
		      int NumMod);

    // compare ladders to experiment
    int CompareLadders(CLadder& BLadder,
		       CLadder& YLadder, CLadder& B2Ladder,
		       CLadder& Y2Ladder, CMSPeak *Peaks,
		       bool OrLadders,  TMassPeak *MassPeak);

    void InitModIndex(int *ModIndex, int& iMod, int& NumMod);
    void MakeBoolMap(bool *ModMask, int *ModIndex, int& iMod, int& NumMod);
    bool CalcModIndex(int *ModIndex, int& iMod, int& NumMod);
    unsigned MakeIntFromBoolMap(bool *ModMask,  int& iMod);
    ReadDBFILEPtr Getrdfp(void) { return rdfp; }
    int Getnumseq(void) { return numseq; }
    double CalcPoisson(double Mean, int i);
    double CalcPoissonMean(int Start, int Stop, int Mass, CMSPeak *Peaks,
			   int Charge, double Threshold);
    double CalcPvalue(double Mean, int Hits, int n);

    // take hitlist for a peak and insert it into the response
    void SetResult(CMSPeakSet& PeakSet, CMSResponse& MyResponse,
		   int Cutoff, double ThreshStart, double ThreshEnd,
		   double ThreshInc);
    // calculate the evalues of the top hits and sort
    void CalcNSort(TScoreList& ScoreList, double Threshold, CMSPeak* Peaks);

private:
    ReadDBFILEPtr rdfp; 
    //    CMSHistSet HistSet;  // score arrays
    CMassArray MassArray;  // amino acid mass arrays
    const double *AAMass;
    int numseq; // number of sequences in blastdb
};

///////////////////  CSearch inline methods

inline void CSearch::InitModIndex(int *ModIndex, int& iMod, int& NumMod)
{
    // pack all the mods to the first possible sites
    int j;
    for(j = 0; j < iMod; j++) ModIndex[j] = j;
}

inline void CSearch::MakeBoolMap(bool *ModMask, int *ModIndex, int& iMod, int& NumMod)
{
    int j;
    // zero out mask
    for(j = 0; j < NumMod - 1; j++)
	ModMask[j] = false;
    // mask at the possible sites according to the index
    for(j = 0; j < iMod; j++) 
	ModMask[ModIndex[j]] = true;
}

inline unsigned CSearch::MakeIntFromBoolMap(bool *ModMask,  int& iMod)
{
    int j, retval(0);
    for(j = 0; j < iMod; j++)
	if(ModMask[j]) retval |= 1 << j;
    return retval;
}

inline bool CSearch::CalcModIndex(int *ModIndex, int& iMod, int& NumMod)
{
    int j;
    // iterate over indices
    for(j = iMod - 1; j >= 0; j--) {
	if(ModIndex[j] < NumMod - 2 - j) {
	    ModIndex[j]++;
	    return true;
	}
    }
    return false;
}

/////////////////// end of CSearch inline methods

END_SCOPE(omssa)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif

/*
  $Log$
  Revision 1.3  2003/10/24 21:28:41  lewisg
  add omssa, xomssa, omssacl to win32 build, including dll

  Revision 1.2  2003/10/21 21:12:17  lewisg
  reorder headers

  Revision 1.1  2003/10/20 21:32:13  lewisg
  ommsa toolkit version

  Revision 1.8  2003/10/07 18:02:28  lewisg
  prep for toolkit

  Revision 1.7  2003/10/06 18:14:17  lewisg
  threshold vary

  Revision 1.6  2003/08/14 23:49:22  lewisg
  first pass at variable mod

  Revision 1.5  2003/07/17 18:45:50  lewisg
  multi dta support

  Revision 1.4  2003/07/07 16:17:51  lewisg
  new poisson distribution and turn off histogram

  Revision 1.3  2003/04/18 20:46:52  lewisg
  add graphing to omssa

  Revision 1.2  2003/04/02 18:49:51  lewisg
  improved score, architectural changes

  Revision 1.1  2003/02/03 20:39:03  lewisg
  omssa cgi

  *
  */
