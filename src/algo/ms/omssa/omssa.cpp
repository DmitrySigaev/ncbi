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
* Author:  Lewis Y. Geer
*  
* File Description:
*    code to do the ms/ms search and score matches
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbi_limits.hpp>

#include <fstream>
#include <string>
#include <list>
#include <deque>
#include <algorithm>
#include <math.h>

#include "SpectrumSet.hpp"
#include "omssa.hpp"

#include "nrutil.h"
#include <ncbimath.h>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(omssa);

/////////////////////////////////////////////////////////////////////////////
//
//  CSearch::
//
//  Performs the ms/ms search
//


CSearch::CSearch(void): rdfp(0)
{
}

int CSearch::InitBlast(const char *blastdb, bool InitDb)
{
    if(!blastdb) return 0;
    if(rdfp) readdb_destruct(rdfp);
    rdfp = readdb_new_ex2((char *)blastdb,TRUE, READDB_NEW_DO_TAXDB | 
			  READDB_NEW_INDEX, NULL, NULL);
    if(!rdfp) return 1;
	
    numseq = readdb_get_num_entries(rdfp);
	
    if(InitDb) {
	int iSearch, length;
	char testchar;
	unsigned char *Sequence;
		
	for(iSearch = 0; iSearch < numseq; iSearch++) {
	    length = readdb_get_sequence(rdfp, iSearch, &Sequence);
	    testchar = Sequence[0];
	}
    }
    return 0;
	
}


// create the ladders from sequence

int CSearch::CreateLadders(unsigned char *Sequence, int iSearch, int position,
			   int endposition,
			   int *Masses, int iMissed, CAA& AA, 
			   CLadder& BLadder,
			   CLadder& YLadder, CLadder& B2Ladder,
			   CLadder& Y2Ladder,
			   unsigned ModMask,
			   const char **Site,
			   int *DeltaMass,
			   int NumMod)
{
    if(!BLadder.CreateLadder(kBIon, 1, (char *)Sequence, iSearch,
			     position, endposition, Masses[iMissed], 
			     MassArray, AA, ModMask, Site, DeltaMass,
			     NumMod)) return 1;
    if(!YLadder.CreateLadder(kYIon, 1, (char *)Sequence, iSearch,
			 position, endposition, Masses[iMissed], 
			     MassArray, AA, ModMask, Site, DeltaMass,
			     NumMod)) return 1;
    B2Ladder.CreateLadder(kBIon, 2, (char *)Sequence, iSearch,
			  position, endposition, 
			  Masses[iMissed], 
			  MassArray, AA, ModMask, Site, DeltaMass,
			  NumMod);
    Y2Ladder.CreateLadder(kYIon, 2, (char *)Sequence, iSearch,
			  position, endposition,
			  Masses[iMissed], 
			  MassArray, AA, ModMask, Site, DeltaMass,
			  NumMod);
    
    return 0;
}


// compare ladders to experiment
int CSearch::CompareLadders(CLadder& BLadder,
			    CLadder& YLadder, CLadder& B2Ladder,
			    CLadder& Y2Ladder, CMSPeak *Peaks,
			    bool OrLadders,  TMassPeak *MassPeak)
{
    if(MassPeak && MassPeak->Charge > 2 ) {
	Peaks->CompareSorted(BLadder, MSCULLED2, 0); 
	Peaks->CompareSorted(YLadder, MSCULLED2, 0); 
	Peaks->CompareSorted(B2Ladder, MSCULLED2, 0); 
	Peaks->CompareSorted(Y2Ladder, MSCULLED2, 0);
	if(OrLadders) {
	    BLadder.Or(B2Ladder);
	    YLadder.Or(Y2Ladder);	
	}
    }
    else {
	Peaks->CompareSorted(BLadder, MSCULLED1, 0); 
	Peaks->CompareSorted(YLadder, MSCULLED1, 0); 
    }
    return 0;
}

// compare ladders to experiment
bool CSearch::CompareLaddersTop(CLadder& BLadder,
			    CLadder& YLadder, CLadder& B2Ladder,
			    CLadder& Y2Ladder, CMSPeak *Peaks,
			    TMassPeak *MassPeak)
{
    if(MassPeak && MassPeak->Charge > 2 ) {
	if(Peaks->CompareTop(BLadder)) return true; 
	if(Peaks->CompareTop(YLadder)) return true; 
	if(Peaks->CompareTop(B2Ladder)) return true; 
	if(Peaks->CompareTop(Y2Ladder)) return true;
    }
    else {
	if(Peaks->CompareTop(BLadder)) return true; 
	if(Peaks->CompareTop(YLadder)) return true; 
    }
    return false;
}

#ifdef _DEBUG
#define CHECKGI
#ifdef CHECKGI
bool CheckGi(int gi)
{
    if(gi == 21742354) {
	ERR_POST(Info << "test seq");
	return true;
    }
    return false;
}
#endif
#endif

// loads spectra into peaks
void CSearch::Spectrum2Peak(CMSRequest& MyRequest, CMSPeakSet& PeakSet)
{
    CSpectrumSet::Tdata::const_iterator iSpectrum;
    CMSPeak* Peaks;
	
    iSpectrum = MyRequest.GetSpectra().Get().begin();
    for(; iSpectrum != MyRequest.GetSpectra().Get().end(); iSpectrum++) {
	CRef <CMSSpectrum> Spectrum =  *iSpectrum;
	if(!Spectrum) {
	    ERR_POST(Error << "omssa: unable to find spectrum");
	    return;
	}
		
	Peaks = new CMSPeak(MyRequest.GetHitlistlen());
	Peaks->Read(*Spectrum, MyRequest.GetMsmstol());
	if(Spectrum->CanGetName()) Peaks->SetName() = Spectrum->GetName();
	if(Spectrum->CanGetNumber())
	    Peaks->SetNumber() = Spectrum->GetNumber();
		
	Peaks->Sort();
	Peaks->SetComputedCharge(3);
	Peaks->InitHitList();
	Peaks->CullAll(MyRequest.GetCutlo(), MyRequest.GetSinglewin(),
		       MyRequest.GetDoublewin(), MyRequest.GetSinglenum(),
		       MyRequest.GetDoublenum());
		
	if(Peaks->GetNum(MSCULLED1) < MSPEAKMIN)  {
	    ERR_POST(Info << "omssa: not enough peaks in spectra");
	    Peaks->SetError(eMSHitError_notenuffpeaks);
	}
	PeakSet.AddPeak(Peaks);
		
    }
    PeakSet.SortPeaks(static_cast <int> (MyRequest.GetPeptol()*MSSCALE));
	
}

// compares TMassMasks.  Lower m/z first in sort.
struct CMassMaskCompare {
    bool operator() (const TMassMask& x, const TMassMask& y)
    {
	if(x.Mass < y.Mass) return true;
	return false;
    }
};

// update sites and masses for new peptide
void CSearch::UpdateWithNewPep(int Missed,
			       const char *PepStart[],
			       const char *PepEnd[], 
			       int NumMod[], 
			       const char *Site[][MAXMOD],
			       int DeltaMass[][MAXMOD],
			       int Masses[],
			       int EndMasses[])
{
    // iterate over missed cleavages
    int iMissed;
    // maximum mods allowed
    int ModMax; 
    // iterate over mods
    int iMod;
    

    // update the longer peptides to add the new peptide (Missed-1) on the end
    for(iMissed = 0; iMissed < Missed - 1; iMissed++) {
	// skip start
	if(PepStart[iMissed] == (const char *)-1) continue;
	// reset the end sequences
	PepEnd[iMissed] = PepEnd[Missed - 1];
				
	// update new mod masses to add in any new mods from new peptide

	// first determine the maximum value for updated mod list
	if(NumMod[iMissed] + NumMod[Missed-1] >= MAXMOD)
	    ModMax = MAXMOD - NumMod[iMissed];
	else ModMax = NumMod[Missed-1];

	// now interatu thru the new entries
	for(iMod = NumMod[iMissed]; 
	    iMod < NumMod[iMissed] + ModMax;
	    iMod++) {
	    Site[iMissed][iMod] = 
		Site[Missed-1][iMod - NumMod[iMissed]];

	    DeltaMass[iMissed][iMod] = 
		DeltaMass[Missed-1][iMod - NumMod[iMissed]];

	    //		    Masses[iMissed][iMod] = 
	    //			Masses[Missed-1][iMod - NumMod[iMissed] + 1] +
	    //			Masses[iMissed][NumMod[iMissed] - 1];
	}
				
	// update old masses
	Masses[iMissed] += Masses[Missed - 1];
				
	// update end masses
	EndMasses[iMissed] = EndMasses[Missed - 1];
			
	// update number of Mods
	NumMod[iMissed] += ModMax;
    }	    
}


// create the various combinations of mods
void CSearch::CreateModCombinations(int Missed,
				    const char *PepStart[],
				    TMassMask MassAndMask[][MAXMOD2],
				    int Masses[],
				    int EndMasses[],
				    int NumMod[],
				    int DeltaMass[][MAXMOD],
				    unsigned NumMassAndMask[])
{
    // need to iterate thru combinations that have iMod.
    // i.e. iMod = 3 and NumMod=5
    // 00111, 01011, 10011, 10101, 11001, 11010, 11100, 01101,
    // 01110
    // i[0] = 0 --> 5-3, i[1] = i[0]+1 -> 5-2, i[3] = i[1]+1 -> 5-1
    // then construct bool mask
	    
    // holders for calculated modification mask and modified peptide masses
    unsigned Mask, MassOfMask;
    // iterate thru active mods
    int iiMod;
    // keep track of the number of unique masks created.  each corresponds to a ladder
    int iModCount;
    // missed cleavage
    int iMissed;
    // number of mods to consider
    int iMod;
    // positions of mods
    int ModIndex[MAXMOD];

    // go thru missed cleaves
    for(iMissed = 0; iMissed < Missed; iMissed++) {
	// skip start
	if(PepStart[iMissed] == (const char *)-1) continue;
	iModCount = 0;

	// set up non-modified mass
	MassAndMask[iMissed][iModCount].Mass = 
	    Masses[iMissed] + EndMasses[iMissed];
	MassAndMask[iMissed][iModCount].Mask = 0;
	iModCount++;
		
	// go thru number of mods allowed
	for(iMod = 0; iMod < NumMod[iMissed] && iModCount < MAXMOD2; iMod++) {
		    
	    // initialize ModIndex that points to mod sites
	    InitModIndex(ModIndex, iMod);
	    do {
			
		// calculate mass
		MassOfMask = Masses[iMissed] + EndMasses[iMissed];
		for(iiMod = 0; iiMod <= iMod; iiMod++ ) 
		    MassOfMask += DeltaMass[iMissed][ModIndex[iiMod]];
		// make bool mask
		Mask = MakeBoolMask(ModIndex, iMod);
		// put mass and mask into storage
		MassAndMask[iMissed][iModCount].Mass = MassOfMask;
		MassAndMask[iMissed][iModCount].Mask = Mask;
		// keep track of the  number of ladders
		iModCount++;

	    } while(iModCount < MAXMOD2 &&
		    CalcModIndex(ModIndex, iMod, NumMod[iMissed]));
	} // iMod

	// sort mask and mass by mass
	sort(MassAndMask[iMissed], MassAndMask[iMissed] + iModCount,
	     CMassMaskCompare());
	// keep track of number of MassAndMask
	NumMassAndMask[iMissed] = iModCount;

    } // iMissed
}


int CSearch::Search(CMSRequest& MyRequest, CMSResponse& MyResponse)
{
    int length;
    CTrypsin trypsin;
    unsigned header;
    char *blastdefline;
    SeqId *sip;
    CLadder BLadder[MAXMOD2], YLadder[MAXMOD2], B2Ladder[MAXMOD2],
	Y2Ladder[MAXMOD2];
    bool LadderCalc[MAXMOD2];  // have the ladders been calculated?
    CAA AA;
		
    int Missed = MyRequest.GetMissedcleave()+1;  // number of missed cleaves allowed + 1
    int iMissed; // iterate thru missed cleavages
    
    int iSearch, hits;
    unsigned char *Sequence;  // start position
    int endposition, position;
    MassArray.Init(MyRequest.GetFixed(), MyRequest.GetSearchtype());
    VariableMods.Init(MyRequest.GetVariable());
    const int *IntMassArray = MassArray.GetIntMass();
    const char *PepStart[MAXMISSEDCLEAVE];
    const char *PepEnd[MAXMISSEDCLEAVE];

    // the position within the peptide of a variable modification
    const char *Site[MAXMISSEDCLEAVE][MAXMOD];
    // the modification mass at the Site
    int DeltaMass[MAXMISSEDCLEAVE][MAXMOD];
    // the number of modified sites + 1 unmodified
    int NumMod[MAXMISSEDCLEAVE];


    // calculated masses and masks
    TMassMask MassAndMask[MAXMISSEDCLEAVE][MAXMOD2];
    // the number of masses and masks for each peptide
    unsigned NumMassAndMask[MAXMISSEDCLEAVE];
	
    // set up mass array, indexed by missed cleavage
    // note that EndMasses is the end mass of peptide, kept separate to allow
    // reuse of Masses array in missed cleavage calc
    int Masses[MAXMISSEDCLEAVE];
    int EndMasses[MAXMISSEDCLEAVE];


    int iMod;   // used to iterate thru modifications
	
    bool SequenceDone;  // are we done iterating through the sequences?
    int taxid;
    const CMSRequest::TTaxids& Tax = MyRequest.GetTaxids();
    CMSRequest::TTaxids::const_iterator iTax;
    CMSHit NewHit;  // a new hit of a ladder to an m/z value
    CMSHit *NewHitOut;  // copy of new hit
	
	
    CMSPeakSet PeakSet;
    TMassPeak *MassPeak; // peak currently in consideration
    CMSPeak* Peaks;
	
    Spectrum2Peak(MyRequest, PeakSet);
		
    // iterate through sequences
    for(iSearch = 0; iSearch < numseq; iSearch++) {
	if(iSearch/10000*10000 == iSearch) ERR_POST(Info << "sequence " << 
						    iSearch);
	header = 0;
#ifdef CHECKGI
	int testgi;
#endif 
	while(readdb_get_header_ex(rdfp, iSearch, &header, &sip,
				   &blastdefline, &taxid, NULL, NULL)) {
			
#ifdef CHECKGI
	    SeqId *bestid = SeqIdFindBest(sip, SEQID_GI);
	    CheckGi(bestid->data.intvalue);
	    testgi = bestid->data.intvalue;
	    //			    cout << testgi << endl;
#endif 
			
	    MemFree(blastdefline);
	    SeqIdSetFree(sip);
	    if(MyRequest.IsSetTaxids()) {
		for(iTax = Tax.begin(); iTax != Tax.end(); iTax++) {
		    if(taxid == *iTax) goto TaxContinue;
		}
	    } 
	    else goto TaxContinue;
	}
	continue;
    TaxContinue:
	length = readdb_get_sequence(rdfp, iSearch, &Sequence);
	SequenceDone = false;
	//	PepStart = (const char *)Sequence[0];
		
	// initialize missed cleavage matrix
	for(iMissed = 0; iMissed < Missed; iMissed++) {
	    PepStart[iMissed] = (const char *)-1; // mark start
	    PepEnd[iMissed] = (const char *)Sequence;
	    Masses[iMissed] = 0;
	    EndMasses[iMissed] = 0;
	    NumMod[iMissed] = 0;

	    DeltaMass[iMissed][0] = 0;
	    Site[iMissed][0] = (const char *)-1;
	}
	PepStart[Missed - 1] = (const char *)Sequence;
		
	// iterate thru the sequence by digesting it
	while(!SequenceDone) {
			
	    // zero out no missed cleavage peptide mass and mods
	    // note that Masses and EndMass are separate to reuse
	    // masses during the missed cleavage calculation
	    Masses[Missed - 1] = 0;
	    EndMasses[Missed - 1] = 0;
	    NumMod[Missed - 1] = 0;
	    // init no modification elements
	    Site[Missed - 1][0] = (const char *)-1;
	    DeltaMass[Missed - 1][0] = 0;
			
	    // calculate new stop and mass
	    SequenceDone = 
		trypsin.CalcAndCut((const char *)Sequence,
				   (const char *)Sequence + length - 1, 
				   &(PepEnd[Missed - 1]),
				   &(Masses[Missed - 1]),
				   NumMod[Missed - 1],
				   MAXMOD,
				   &(EndMasses[Missed - 1]),
				   VariableMods,
				   Site[Missed - 1],
				   DeltaMass[Missed - 1],
				   IntMassArray);
			

	    UpdateWithNewPep(Missed, PepStart, PepEnd, NumMod, Site,
			     DeltaMass, Masses, EndMasses);
	
	    CreateModCombinations(Missed, PepStart, MassAndMask, Masses,
				  EndMasses, NumMod, DeltaMass, NumMassAndMask);


	    int OldMass;  // keeps the old peptide mass for comparison
	    bool NoMassMatch;  // was there a match to the old mass?

	    for(iMissed = 0; iMissed < Missed; iMissed++) {	    
		if(PepStart[iMissed] == (const char *)-1) continue;  // skip start
				
		// get the start and stop position, inclusive, of the peptide
		position =  PepStart[iMissed] - (const char *)Sequence;
		endposition = PepEnd[iMissed] - (const char *)Sequence;
		
		// init bool for "Has ladder been calculated?"
		for(iMod = 0; iMod < MAXMOD2; iMod++) LadderCalc[iMod] = false;
		
		OldMass = 0;
		NoMassMatch = true;
				
		// go thru total number of mods
		for(iMod = 0; iMod < NumMassAndMask[iMissed]; iMod++) {
		    
		    // have we seen this mass before?
		    if(MassAndMask[iMissed][iMod].Mass == OldMass &&
		       NoMassMatch) continue;
		    NoMassMatch = true;
		    OldMass = MassAndMask[iMissed][iMod].Mass;
		    // return peak where theoretical mass is < precursor mass + tol
		    MassPeak = PeakSet.GetIndexLo(OldMass);
					
		    for(;MassPeak < PeakSet.GetEndMassPeak() && 
			    OldMass > MassPeak->Mass - MassPeak->Peptol;
			MassPeak++) {
			Peaks = MassPeak->Peak;
			// make sure we look thru other mod masks with the same mass
			NoMassMatch = false;
						
#ifdef CHECKGI
			SeqId *bestid;
						
			header = 0;
			readdb_get_header(rdfp, iSearch, &header, &sip, &blastdefline);
			bestid = SeqIdFindBest(sip, SEQID_GI);
			if(/*Peaks->GetNumber() == 245 && */CheckGi(bestid->data.intvalue))
			    cerr << "hello" << endl;
			//			int testgi = bestid->data.intvalue;
			MemFree(blastdefline);
			SeqIdSetFree(sip);
#endif
						
						
			if(!LadderCalc[iMod]) {
			    LadderCalc[iMod] = true;	
			    if(CreateLadders(Sequence, iSearch, position,
					     endposition,
					     Masses,
					     iMissed, AA,
					     BLadder[iMod],
					     YLadder[iMod],
					     B2Ladder[iMod],
					     Y2Ladder[iMod],
					     MassAndMask[iMissed][iMod].Mask,
					     Site[iMissed],
					     DeltaMass[iMissed],
					     NumMod[iMissed]) != 
			       0) continue;
			    // continue to next sequence if ladders not successfully made
			}
			else {
			    BLadder[iMod].ClearHits();
			    YLadder[iMod].ClearHits();
			    B2Ladder[iMod].ClearHits();
			    Y2Ladder[iMod].ClearHits();
			}
				
			//start of new addition
			if(CompareLaddersTop(BLadder[iMod], 
					     YLadder[iMod],
					     B2Ladder[iMod], 
					     Y2Ladder[iMod],
					     Peaks, MassPeak)) {
			    // end of new addition


			    Peaks->SetPeptidesExamined(MassPeak->Charge)++;
#ifdef CHECKGI
			    /*if(Peaks->GetNumber() == 245)*/
			    //			    cout << testgi << " " << position << " " << endposition << endl;
#endif
			    Peaks->ClearUsedAll();
			    CompareLadders(BLadder[iMod], 
					   YLadder[iMod],
					   B2Ladder[iMod], 
					   Y2Ladder[iMod],
					   Peaks, false, MassPeak);
			    hits = BLadder[iMod].HitCount() + 
				YLadder[iMod].HitCount() +
				B2Ladder[iMod].HitCount() +
				Y2Ladder[iMod].HitCount();
			    if(hits >= MSHITMIN) {
				// need to save mods.  bool map?
				NewHit.SetHits(hits);	
				NewHit.SetCharge(MassPeak->Charge);
				// only record if hit kept
				if(Peaks->AddHit(NewHit, NewHitOut)) {
				    NewHitOut->SetStart(position);
				    NewHitOut->SetStop(endposition);
				    NewHitOut->SetSeqIndex(iSearch);
				    NewHitOut->SetMass(MassPeak->Mass);
				    // record the hits
				    Peaks->ClearUsedAll();
				    NewHitOut->
					RecordMatches(BLadder[iMod],
						      YLadder[iMod],
						      B2Ladder[iMod], 
						      Y2Ladder[iMod],
						      Peaks);
				}
			    }
			} // new addition
		    } // MassPeak
		} //iMod
	    } // iMissed
	    if(!SequenceDone) {
		// get rid of longest peptide and move the other peptides down the line
		for(iMissed = 0; iMissed < Missed - 1; iMissed++) {
		    // move masses to next missed cleavage
		    NumMod[iMissed] = NumMod[iMissed + 1];
		    Masses[iMissed] = Masses[iMissed + 1];
		    // don't move EndMasses as they are recalculated

		    // move the modification data
		    for(iMod = 0; iMod < NumMod[iMissed]; iMod++) {
			DeltaMass[iMissed][iMod] = 
			    DeltaMass[iMissed + 1][iMod];
			Site[iMissed][iMod] = 
			    Site[iMissed + 1][iMod];
		    }

		    // copy starts to next missed cleavage
		    PepStart[iMissed] = PepStart[iMissed + 1];
		}
			
			
		// init new start from old stop
		PepEnd[Missed-1] += 1;
		PepStart[Missed-1] = PepEnd[Missed-1];
		// PepStart = PepEnd + 1;
	    } 
	}
    }
	
    //    free_imatrix(Masses, 0, MAXMISSEDCLEAVE, 0, MAXMOD);
	
    // read out hits
    SetResult(PeakSet, MyResponse, MyRequest.GetCutlo(), 
	      MyRequest.GetCuthi(), MyRequest.GetCutinc());
	
    return 0;
}


void CSearch::SetResult(CMSPeakSet& PeakSet, CMSResponse& MyResponse,
			double ThreshStart, double ThreshEnd,
			double ThreshInc)
{
    CMSPeak* Peaks;
    TPeakSet::iterator iPeakSet;
	
    TScoreList ScoreList;
    TScoreList::iterator iScoreList;
    CMSHit * MSHit;
    unsigned header;
    char *blastdefline;
    SeqId *sip, *bestid;
    int length;
    unsigned char *Sequence;
    int iSearch; // counter for the length of hit list
	
    for(iPeakSet = PeakSet.GetPeaks().begin();
	iPeakSet != PeakSet.GetPeaks().end();
	iPeakSet++ ) {
	Peaks = *iPeakSet;
		
	// add to hitset
	CRef< CMSHitSet > HitSet (new CMSHitSet);
	if(!HitSet) {
	    ERR_POST(Error << "omssa: unable to allocate hitset");
	    return;
	}
	HitSet->SetNumber(Peaks->GetNumber());
	HitSet->SetFilename(Peaks->GetName());
	MyResponse.SetHitsets().push_back(HitSet);
		
	if(Peaks->GetError() == eMSHitError_notenuffpeaks) {
	    ERR_POST(Info << "empty set");
	    HitSet->SetError(eMSHitError_notenuffpeaks);
	    continue;
	}
		
	double Threshold, MinThreshold(ThreshStart), MinEval(1000000.0L);
	// now calculate scores and sort
	for(Threshold = ThreshStart; Threshold <= ThreshEnd; 
	    Threshold += ThreshInc) {
	    CalcNSort(ScoreList, Threshold, Peaks, false);
	    if(!ScoreList.empty()) {
		_TRACE("Threshold = " << Threshold <<
		       "EVal = " << ScoreList.begin()->first);
	    }
	    if(!ScoreList.empty() && ScoreList.begin()->first < MinEval) {
		MinEval = ScoreList.begin()->first;
		MinThreshold = Threshold;
	    }
	    ScoreList.clear();
	}
	_TRACE("Min Threshold = " << MinThreshold);
	CalcNSort(ScoreList, MinThreshold, Peaks, true);
		
		
	// keep a list of redundant peptides
	map <string, CMSHits * > PepDone;
	// add to hitset by score
	for(iScoreList = ScoreList.begin(), iSearch = 0;
	    iScoreList != ScoreList.end();
	    iScoreList++, iSearch++) {
	    
	    CMSHits * Hit;
	    CMSPepHit * Pephit;
			
	    MSHit = iScoreList->second;
	    header = 0;
	    while (readdb_get_header(rdfp, MSHit->GetSeqIndex(), &header,
				     &sip,
				     &blastdefline)) {
		bestid = SeqIdFindBest(sip, SEQID_GI);
		if(!bestid) continue;
				
		string seqstring;
		string deflinestring(blastdefline);
		int iseq;
		length = readdb_get_sequence(rdfp, MSHit->GetSeqIndex(),
					     &Sequence);
				
		for (iseq = MSHit->GetStart(); iseq <= MSHit->GetStop();
		     iseq++) {
		    seqstring += UniqueAA[Sequence[iseq]];
		}
#ifdef CHECKGI
		CheckGi(bestid->data.intvalue);
#endif
		if(PepDone.find(seqstring) != PepDone.end()) {
		    Hit = PepDone[seqstring];
		}
		else {
		    Hit = new CMSHits;
		    Hit->SetPepstring(seqstring);
		    double Score = iScoreList->first;
		    Hit->SetEvalue(Score);
		    Hit->SetPvalue(Score/Peaks->
				   GetPeptidesExamined(MSHit->
						       GetCharge()));	   
		    Hit->SetCharge(MSHit->GetCharge());
		    CRef<CMSHits> hitref(Hit);
		    HitSet->SetHits().push_back(hitref);  
		    PepDone[seqstring] = Hit;
		}
		
		Pephit = new CMSPepHit;
				
		Pephit->SetStart(MSHit->GetStart());
		Pephit->SetStop(MSHit->GetStop());
		Pephit->SetGi(bestid->data.intvalue);
		Pephit->SetDefline(deflinestring);
		CRef<CMSPepHit> pepref(Pephit);
		Hit->SetPephits().push_back(pepref);
		MemFree(blastdefline);
		SeqIdSetFree(sip);
	    }
	}
	ScoreList.clear();
    }
}


// calculate the evalues of the top hits and sort
void CSearch::CalcNSort(TScoreList& ScoreList, double Threshold, CMSPeak* Peaks, bool NewScore)
{
    int iCharges;
    int iHitList;
    int length;
    unsigned char *Sequence;
	
    for(iCharges = 0; iCharges < Peaks->GetNumCharges(); iCharges++) {
		
	TMSHitList& HitList = Peaks->GetHitList(iCharges);   
	for(iHitList = 0; iHitList != Peaks->GetHitListIndex(iCharges);
	    iHitList++) {
#ifdef CHECKGI
	    unsigned header;
	    char *blastdefline;
	    SeqId *sip, *bestid;
			
	    header = 0;
	    readdb_get_header(rdfp, HitList[iHitList].GetSeqIndex(),
			      &header, &sip,&blastdefline);
	    bestid = SeqIdFindBest(sip, SEQID_GI);
	    if(!bestid) continue;
	    CheckGi(bestid->data.intvalue);
	    MemFree(blastdefline);
	    SeqIdSetFree(sip);
#endif
			
	    // recompute ladder
	    length = readdb_get_sequence(rdfp, 
					 HitList[iHitList].GetSeqIndex(),
					 &Sequence);
			
	    int tempMass = HitList[iHitList].GetMass();
	    int tempStart = HitList[iHitList].GetStart();
	    int tempStop = HitList[iHitList].GetStop();
	    int Charge = HitList[iHitList].GetCharge();
	    int Which = Peaks->GetWhich(Charge);
			
	    double a = CalcPoissonMean(tempStart, tempStop, tempMass, Peaks,
				       Charge, 
				       Threshold);
	    if ( a < 0) continue;  // error return;
	    if(HitList[iHitList].GetHits() < a) continue;
	    double pval;
	    if(NewScore) {
		int High, Low, NumPeaks, NumLo, NumHi;
		Peaks->HighLow(High, Low, NumPeaks, tempMass, Charge, Threshold, NumLo, NumHi);
 
		double TopHitProb = ((double)MSNUMTOP)/NumPeaks;
		double Normal = CalcNormalTopHit(a, TopHitProb);
		pval = CalcPvalueTopHit(a, 
					HitList[iHitList].GetHits(Threshold, Peaks->GetMaxI(Which)),
					Peaks->GetPeptidesExamined(Charge), Normal, TopHitProb);
		cerr << "pval = " << pval << " Normal = " << Normal << endl;
	    }
	    else {
		pval =
		    CalcPvalue(a, HitList[iHitList].
			       GetHits(Threshold, Peaks->GetMaxI(Which)),
			       Peaks->GetPeptidesExamined(Charge));
	    }
	    double eval = pval * Peaks->GetPeptidesExamined(Charge);
	    ScoreList.insert(pair<const double, CMSHit *> 
			     (eval, &(HitList[iHitList])));
	}   
    } 
}


//calculates the poisson times the top hit probability
double CSearch::CalcPoissonTopHit(double Mean, int i, double TopHitProb)
{
double retval;
#ifdef NCBI_OS_MSWIN
    retval =  exp(-Mean) * pow(Mean, i) / exp(LnGamma(i+1));
#else
    retval =  exp(-Mean) * pow(Mean, i) / exp(lgamma(i+1));
#endif
retval *= 1.0L - pow((1.0L-TopHitProb), i);
return retval;
}

double CSearch::CalcPoisson(double Mean, int i)
{
#ifdef NCBI_OS_MSWIN
    return exp(-Mean) * pow(Mean, i) / exp(LnGamma(i+1));
#else
    return exp(-Mean) * pow(Mean, i) / exp(lgamma(i+1));
#endif
}

double CSearch::CalcPvalue(double Mean, int Hits, int n)
{
    if(Hits <= 0) return 1.0L;
	
    int i;
    double retval(0.0L);
	
    for(i = 0; i < Hits; i++)
		retval += CalcPoisson(Mean, i);
	
    retval = 1.0L - pow(retval, n);
    if(retval <= 0.0L) retval = numeric_limits<double>::min();
    return retval;
}

#define MSDOUBLELIMIT 0.0000000000000001L
// do full summation of probability distribution
double CSearch::CalcNormalTopHit(double Mean, double TopHitProb)
{
    int i;
    double retval(0.0L), before(-1.0L), increment;
	
    for(i = 1; i < 1000; i++) {
	increment = CalcPoissonTopHit(Mean, i, TopHitProb);
	// convergence hack -- on linux (at least) convergence test doesn't work
	// for gcc release build
	if(increment <= MSDOUBLELIMIT) break;
	//	if(increment <= numeric_limits<double>::epsilon()) break;
	retval += increment;
	if(retval == before) break;  // convergence
	before = retval;
    }
#if 0
    if(isnan(retval)) {
	before = 1;
    }
#endif
    return retval;
}


double CSearch::CalcPvalueTopHit(double Mean, int Hits, int n, double Normal, double TopHitProb)
{
    if(Hits <= 0) return 1.0L;
	
    int i;
    double retval(0.0L), increment;
	
    for(i = 1; i < Hits; i++) {
	increment = CalcPoissonTopHit(Mean, i, TopHitProb);
	if(increment <= MSDOUBLELIMIT) break;
	retval += increment;
    }

    retval /= Normal;
	
    retval = 1.0L - pow(retval, n);
    if(retval <= 0.0L) retval = numeric_limits<double>::min();
    return retval;
}


double CSearch::CalcPoissonMean(int Start, int Stop, int Mass, CMSPeak *Peaks,
								int Charge, double Threshold)
{
    double retval(1.0);
    
    double m; // mass of ladder
    double o; // min m/z value of peaks
    double r; // max m/z value of peaks
    double h; // number of calculated m/z values in one ion series
    double v1; // number of m/z values above mh/2 and below mh (+1 product ions)
    double v2; // number of m/z value below mh/2
    double v;  // number of peaks
    double t; // mass tolerance width
	
    int High, Low, NumPeaks, NumLo, NumHi;
    Peaks->HighLow(High, Low, NumPeaks, Mass, Charge, Threshold, NumLo, NumHi);
    if(High == -1) return -1.0; // error
    o = (double)Low/MSSCALE;
    r = (double)High/MSSCALE;
    v1 = NumHi;
    v2 = NumLo;
    v = NumPeaks;
    m = Mass/(double)MSSCALE;
    h = Stop - Start;
    t = (Peaks->GetTol())/(double)MSSCALE;
    // see 12/13/02 notebook, pg. 127
	
    retval = 4.0 * t * h * v / m;
    if(Charge > 2) {
	retval *= (m - 3*o + r)/(r - o);
    }    
#if 0
    // variation that counts +1 and +2 separately
    // see 8/19/03 notebook, pg. 71
    retval = 4.0 * t * h / m;
    if(Charge > 2) {
	//		retval *= (m - 3*o + r)/(r - o);
	retval *= (3 * v2 + v1);
    }
    else retval *= (v2 + v1);
#endif
#ifdef DEBUG_PEAKS
    _TRACE("o:" << o << " r:" << r << " h:" << h << " m:" << m << " v:" << v << " t:" << t);
    _TRACE("charge:" << Peaks->GetCharge() <<  "  Calculated average:" << retval);
#endif
	
    return retval;
}



CSearch::~CSearch()
{
    if(rdfp) readdb_destruct(rdfp);
}

/*
$Log$
Revision 1.17  2004/04/05 20:49:16  lewisg
fix missed mass bug and reorganize code

Revision 1.16  2004/03/31 02:00:26  ucko
+<algorithm> for sort()

Revision 1.15  2004/03/30 19:36:59  lewisg
multiple mod code

Revision 1.14  2004/03/16 20:18:54  gorelenk
Changed includes of private headers.

Revision 1.13  2004/03/01 18:24:08  lewisg
better mod handling

Revision 1.12  2003/12/22 21:58:00  lewisg
top hit code and variable mod fixes

Revision 1.11  2003/12/04 23:39:08  lewisg
no-overlap hits and various bugfixes

Revision 1.10  2003/11/20 22:32:50  lewisg
fix end of sequence X bug

Revision 1.9  2003/11/20 15:40:53  lewisg
fix hitlist bug, change defaults

Revision 1.8  2003/11/17 18:36:56  lewisg
fix cref use to make sure ref counts are decremented at loop termination

Revision 1.7  2003/11/14 20:28:06  lewisg
scale precursor tolerance by charge

Revision 1.6  2003/11/13 19:07:38  lewisg
bugs: iterate completely over nr, don't initialize blastdb by iteration

Revision 1.5  2003/11/10 22:24:12  lewisg
allow hitlist size to vary

  Revision 1.4  2003/10/24 21:28:41  lewisg
  add omssa, xomssa, omssacl to win32 build, including dll
  
	Revision 1.3  2003/10/22 15:03:32  lewisg
	limits and string compare changed for gcc 2.95 compatibility
	
	  Revision 1.2  2003/10/21 21:12:17  lewisg
	  reorder headers
	  
		Revision 1.1  2003/10/20 21:32:13  lewisg
		ommsa toolkit version
		
		  Revision 1.18  2003/10/07 18:02:28  lewisg
		  prep for toolkit
		  
			Revision 1.17  2003/10/06 18:14:17  lewisg
			threshold vary
			
			  Revision 1.16  2003/08/14 23:49:22  lewisg
			  first pass at variable mod
			  
				Revision 1.15  2003/08/06 18:29:11  lewisg
				support for filenames, numbers using regex
				
				  Revision 1.14  2003/07/21 20:25:03  lewisg
				  fix missing peak bug
				  
					Revision 1.13  2003/07/19 15:07:38  lewisg
					indexed peaks
					
					  Revision 1.12  2003/07/18 20:50:34  lewisg
					  *** empty log message ***
					  
						Revision 1.11  2003/07/17 18:45:50  lewisg
						multi dta support
						
						  Revision 1.10  2003/07/07 16:17:51  lewisg
						  new poisson distribution and turn off histogram
						  
							Revision 1.9  2003/05/01 14:52:10  lewisg
							fixes to scoring
							
							  Revision 1.8  2003/04/18 20:46:52  lewisg
							  add graphing to omssa
							  
								Revision 1.7  2003/04/07 16:47:16  lewisg
								pc fixes
								
								  Revision 1.6  2003/04/04 18:43:51  lewisg
								  tax cut
								  
									Revision 1.5  2003/04/02 18:49:51  lewisg
									improved score, architectural changes
									
									  Revision 1.4  2003/03/21 21:14:40  lewisg
									  merge ming's code, other stuff
									  
										Revision 1.3  2003/02/10 19:37:56  lewisg
										perf and web page cleanup
										
										  Revision 1.2  2003/02/07 16:18:23  lewisg
										  bugfixes for perf and to work on exp data
										  
											Revision 1.1  2003/02/03 20:39:02  lewisg
											omssa cgi
											
											  
												
*/
