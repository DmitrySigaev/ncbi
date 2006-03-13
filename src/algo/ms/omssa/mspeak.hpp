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
 *    code to deal with spectra and m/z ladders
 *
 * ===========================================================================
 */

#ifndef MSPEAK__HPP
#define MSPEAK__HPP

#include <corelib/ncbimisc.hpp>
#include <objects/omssa/omssa__.hpp>
#include <util/rangemap.hpp>
#include <util/itree.hpp>

#include <set>
#include <iostream>
#include <vector>
#include <deque>
#include <map>
#include <string.h>

#include "msms.hpp"
#include "msladder.hpp"
#include "SpectrumSet.hpp"
#include "omssascore.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(omssa)


class CMSPeak;

/**
 *  Class to hold mod information for a hit
 */
class NCBI_XOMSSA_EXPORT CMSModInfo {
public:
	const int GetModEnum(void) const;
	int& SetModEnum(void);

	const int GetSite(void) const;
	int& SetSite(void);

	const int GetIsFixed(void) const;
	int& SetIsFixed(void);

private:
	// the mod type
	int ModEnum;
	// the position in the peptide
	int Site;
    // is it fixed
    int IsFixed;
};


///////////////////  CMSModInfo inline methods

inline
const int CMSModInfo::GetModEnum(void) const
{
	return ModEnum;
}

inline
int& CMSModInfo::SetModEnum(void)
{
	return ModEnum;
}

inline
const int CMSModInfo::GetSite(void) const
{
	return Site;
}

inline
int& CMSModInfo::SetSite(void)
{
	return Site;
}


inline
const int CMSModInfo::GetIsFixed(void) const
{
	return IsFixed;
}

inline
int& CMSModInfo::SetIsFixed(void)
{
	return IsFixed;
}


///////////////////  end CMSModInfo inline methods



/** typedef for holding hit information */
typedef AutoPtr <CMSModInfo, ArrayDeleter<CMSModInfo> > TModInfo;


/**
 *  class to contain preliminary hits.  memory footprint must be kept small.
 */
class NCBI_XOMSSA_EXPORT CMSHit: public CMSSpectrumMatch {
public:
    // tor's
    CMSHit(void);
    CMSHit(int StartIn, int StopIn, int IndexIn);
    CMSHit(int StartIn, int StopIn, int IndexIn, int MassIn, int HitsIn,
	   int ChargeIn);

    /** get sequence start */
    const int GetStart(void) const;

    /** set sequence start */
    int& SetStart();

    /** get sequence stop */
    const int GetStop(void) const;

    /** set sequence stop */
    int& SetStop(void);

    /** get blast oid */
    const int GetSeqIndex(void) const;

    /** set blast oid */
    int& SetSeqIndex(void);

    /**
     * get modification info
     * 
     * @param n array index
     */
	const CMSModInfo& GetModInfo(int n) const;

    /**
     * set modification info
     * 
     * @param n array index
     */
	CMSModInfo& SetModInfo(int n);

    /**
     * get size of modification info array
     */
	const int GetNumModInfo(void) const;

    /**
     *  return number of hits above threshold
     */
    int CountHits(double Threshold, int MaxI);


    /**
     * count hits into two categories: independent hits and hits that are dependent on others
     * 
     * @param Independent count of independent hits
     * @param Dependent count of dependent hits
     */
    void CountHitsByType(int& Independent,
                         int& Dependent,
                         double Threshold, 
                         int MaxI) const;

    // for poisson test
    /**
     *  return number of hits above threshold scaled by m/z positions
     */
    int CountHits(double Threshold, int MaxI, int High);

    /**     
     * Make a record of the hits to the mass ladders
     * 
     * @param BLadder plus one forward ladder
     * @param YLadder plus one backward ladder
     * @param B2Ladder plus two forward ladder
     * @param Y2Ladder plus two backward ladder
     * @param Peaks the experimental spectrum
     * @param ModMask the bit array of modifications
     * @param ModList modification information
     * @param NumMod  number of modifications
     * @param PepStart starting position of peptide
     * @param Searchctermproduct search the c terminal ions
     * @param Searchb1 search the first forward ion?
     * @param TheoreticalMassIn the mass of the theoretical peptide
     */
    void RecordMatches(CLadder& BLadder,
					   CLadder& YLadder, 
					   CLadder& B2Ladder,
					   CLadder& Y2Ladder,
					   CMSPeak *Peaks,
					   unsigned ModMask,
					   CMod ModList[],
					   int NumMod,
					   const char *PepStart,
                       int Searchctermproduct,
                       int Searchb1,
                       int TheoreticalMassIn
						);


	///
	///  Count Modifications in Mask
	///
	
	int CountMods(unsigned ModMask, int NumMod);


    /**
     * Record the modifications used in the hit
     * Note that fixed aa modifications are *not* recorded
     * as these are dealt with by modifying the aa mass
     * and the positions are not recorded anywhere
     */
	void RecordModInfo(unsigned ModMask,
                       CMod ModList[],
                       int NumMod,
                       const char *PepStart
                       );


	/**
     * assignment operator
     * does a copy 
	 */

    CMSHit& operator= (CMSHit& in);

protected:

    /**
     *  helper function for RecordHits that scans thru a single ladder
     * 
     * @param Ladder the ladder to record
     * @param iHitInfo the index of the hit
     * @param Peaks the spectrum that is hit
     * @param Which which noise reduced spectrum to examine
     * @param Offset the numbering offset for the ladder
     */
    void RecordMatchesScan(CLadder& Ladder,
                           int& iHitInfo,
                           CMSPeak *Peaks,
                           int Which,
                           int Offset);

private:

    // disallow copy
    CMSHit(const CMSHit& in) {}

    /**
     * start and stop positions, inclusive, on sequence
     */
    int Start, Stop;

    /**
     * blast ordinal
     */
    int Index;

    /** modification information array */
	TModInfo ModInfo;

    /** size of ModInfo */
	int NumModInfo;
};


/////////////////// CMSHit inline methods

inline 
CMSHit::CMSHit(void)
{
    SetHits() = 0;
}

inline 
CMSHit::CMSHit(int StartIn, int StopIn, int IndexIn):
    Start(StartIn), Stop(StopIn), Index(IndexIn)
{
    SetHits() = 0;
}

inline 
CMSHit::CMSHit(int StartIn, int StopIn, int IndexIn, int MassIn, int HitsIn,
		      int ChargeIn):
    Start(StartIn), Stop(StopIn), Index(IndexIn)
{
    SetHits() = HitsIn;
    SetExpMass() = MassIn;
    SetCharge() = ChargeIn;
}

inline 
const int CMSHit::GetStart(void) const
{ 
    return Start;
}

inline 
int& CMSHit::SetStart(void) 
{ 
    return Start;
}

inline 
const int CMSHit::GetStop(void) const
{ 
    return Stop; 
}

inline 
int& CMSHit::SetStop(void) 
{ 
    return Stop; 
}

inline 
const int CMSHit::GetSeqIndex(void) const
{ 
    return Index; 
}

inline 
int& CMSHit::SetSeqIndex(void) 
{ 
    return Index; 
}

inline 
CMSModInfo& CMSHit::SetModInfo(int n)
{
    return *(ModInfo.get() + n);
}

inline 
const CMSModInfo& CMSHit::GetModInfo(int n) const
{
    return *(ModInfo.get() + n);
}

inline 
const int CMSHit::GetNumModInfo(void) const
{
	return NumModInfo;
}

inline 
CMSHit& CMSHit::operator= (CMSHit& in) 
{ 
    // handle self assignment
    if(this == &in) return *this;

    CMSSpectrumMatch::operator= (in);
    Start = in.Start; 
    Stop = in.Stop;
    Index = in.Index; 
    NumModInfo = in.NumModInfo;
    int i;
    ModInfo.reset();
    if(in.ModInfo) {
        ModInfo.reset(new CMSModInfo[NumModInfo]);
        for(i = 0; i < NumModInfo; i++) 
            SetModInfo(i) = in.SetModInfo(i);
    }
    return *this;
}

/////////////////// end of CMSHit inline methods



/////////////////////////////////////////////////////////////////////////////
//
//  CMZI::
//
//  Used by CMSPeak class to spectral data
//

/**
 *  a class for holding an m/z value, intensity, and rank
 */
class NCBI_XOMSSA_EXPORT CMZI: public CMSBasicPeak {
public:
    CMZI(void);
    CMZI(int MZIn, unsigned IntensityIn);
    CMZI(double MZIn, double IntensityIn);

    /** get the peak rank */
    const TMSRank GetRank(void) const;

    /** set the peak rank */
    TMSRank& SetRank(void);

private:
    /** The intensity rank of the peak. 1 = most intense */
    TMSRank Rank;
};

///////////////////  CMZI inline methods

inline 
CMZI::CMZI(void) 
{}

inline 
CMZI::CMZI(int MZIn, unsigned IntensityIn)
{
    SetMZ() = MZIn;
    SetIntensity() = IntensityIn;
}

inline 
CMZI::CMZI(double MZIn, double IntensityIn)
{
    SetMZ() = MSSCALE2INT(MZIn);
    SetIntensity() = static_cast <unsigned> (IntensityIn);
}

inline
const TMSRank CMZI::GetRank(void) const
{
    return Rank;
}

inline
TMSRank& CMZI::SetRank(void)
{
    return Rank;
}

/////////////////// end of CMZI inline methods


/////////////////////////////////////////////////////////////////////////////
//
//  CMSPeak::
//
//  Class used to hold spectra and convert to mass ladders
//


// for containing hits in mspeak class
// first index is charge
typedef CMSHit * TMSHitList;

// min number of peaks to be considered a hit
#define MSHITMIN 2

// min number of peaks to consider a spectra
// two is absolute minimum in order to get m/z range
#define MSPEAKMIN 5

// size of histogram bin in Daltons
#define MSBIN 100

// culled for single charges
#define MSCULLED1 1
// culled for double charges
#define MSCULLED2 2
// original data
#define MSORIGINAL 0
// only the few most intense peaks
#define MSTOPHITS 3
// number of above cull states
#define MSNUMDATA 4


// the maximum charge state that can be considered
#define MSMAXCHARGE 10

/** 
 * function object for cull iterate 
 */
typedef bool (*TMZIbool) (const CMZI&, const CMZI&, int tol);

enum EChargeState {
    eChargeUnknown, // charge has not been computed
    eCharge1,
    eChargeNot1,  // charge is not +1, but one of +2, +3 ...
    eCharge2,
    eCharge3,
    eCharge4,
    eCharge5 };

// typedef for holding intensity
typedef AutoPtr <unsigned, ArrayDeleter<unsigned> > TIntensity;

// for statistical modelling
// #define MSSTATRUN

/**
 * class to hold spectral data
 * for filtering and statistical characterization
 */

class NCBI_XOMSSA_EXPORT CMSPeak {
public:

    /**
     * CMSPeak ctor
     */
    CMSPeak(void);

    /**
     * CMSPeak ctor
     * 
     * @param HitListSize size of the hit list allowed
     */
    CMSPeak(int HitListSize);


private:

    /**
     *  shared c'tor code
     */
    void xCMSPeak(void);

    /** 
     * writes out dta format
     * 
     * @param FileOut output for dta file
     * @param Temp list of intensities and m/z
     * @param Num number of peaks
     */
    void xWrite(std::ostream& FileOut, const CMZI * const Temp, const int Num) const;

public:

    ~CMSPeak(void);

    /**
     * sort the peak by increasing m/z
     * @param Which which of the mzi arrays should be sorted?
     */
    void Sort(int Which = MSORIGINAL);

    /**
     * Compare the ladder and peaks and return back rank statistics
     * @param Ladder the ladder to compare
     * @param Which which exp spectrum to use
     * @param Intensity keep a record of the intensities
     * @param Sum the sum of ranks
     * @param M the number of matched peaks
     *
     */
    int CompareSortedRank(CLadder& Ladder,
                          int Which, 
                          TIntensity* Intensity, 
                          int& Sum, 
                          int& M);

    /**
     * Rank the given spectrum by intensity.
     * assumes the spectrum is sorted by intensity.
     * highest intensity is given rank 1.
     * 
     * @param Which which experimental spectrum to use
     */
    void Rank(const int Which);

    /**
     * Read a spectrum set into a CMSPeak
     * 
     * @param Spectrum the spectrum itself
     * @param Settings search settings, e.g. experimental tolerances
     */
    int Read(const CMSSpectrum& Spectrum,
             const CMSSearchSettings& Settings);

    /**
     * Read and process a spectrum set into a CMSPeak
     * 
     * @param Spectrum the spectrum itself
     * @param Settings search settings, e.g. experimental tolerances
     */
    void ReadAndProcess(const CMSSpectrum& Spectrum,
                       const CMSSearchSettings& Settings);

    /**
     *  Write out a CMSPeak in dta format (useful for debugging)
     * 
     * @param FileOut the file to write out to
     * @param FileType file format to use
     * @param Which which MZI set to use
     */
    void Write(std::ostream& FileOut, const EFileType FileType = eDTA, const int Which = MSORIGINAL) const;

    // functions used in SmartCull
    
    /**
     * iterate thru peaks, deleting ones that pass the test
     * 
     * @param Temp MZI values to use
     * @param TempLen length of Temp
     * @param FCN function to use to do the test
     */
    void CullIterate(CMZI *Temp, int& TempLen, const TMZIbool FCN);

    /**
     *  cull precursors
     * 
     * @param Temp MZI values to use
     * @param TempLen length of Temp
     * @param scaled precursor value
     * @param Charge precursor charge
     */
    void CullPrecursor(CMZI *Temp, int& TempLen, const int Precursor, const int Charge);

    /**
     *  take out peaks below a threshold
     * 
     * @param Temp MZI values to use
     * @param TempLen length of Temp
     * @param Threshold fraction of highest intensity used to cull
     */
    void CullBaseLine(const double Threshold, CMZI *Temp, int& TempLen);

    /**
     *  cull isotopes using the Markey Method
     * 
     * @param Temp MZI values to use
     * @param TempLen length of Temp
     */
    void CullIsotope(CMZI *Temp, int& TempLen);

    /**
     * cull peaks that are water or ammonia loss
     * note that this only culls the water or ammonia loss if these peaks have a lesser
     * less intensity
     * 
     * @param Temp MZI values to use
     * @param TempLen length of Temp
     */
    void CullH20NH3(CMZI *Temp, int& TempLen);

    /**
     * recursively culls the peaks
     * 
     * @param ConsiderMultProduct assume multiply charged products?
     * @param Temp MZI values to use
     * @param TempLen length of Temp
     * @param Settings search settings, e.g. experimental tolerances
     */
    void SmartCull(const CMSSearchSettings& Settings,
				   CMZI *Temp,
				   int& TempLen,
				   const bool ConsiderMultProduct
				   );

    /**
     *  use smartcull on all charge states
     * 
     * @param Settings search settings, e.g. experimental tolerances
     */
    void CullAll(const CMSSearchSettings& Settings);

	/**
	 * Performs culling based on whether to consider multiply charged ions or not
     * 
     * @param ConsiderMultProduct should we use multiply charged products?
     * @param Settings search settings, e.g. experimental tolerances
	 */
	void CullChargeAndWhich(bool ConsiderMultProduct,  // should we use multiply charged products?
							const CMSSearchSettings& Settings
							);

    /**
     * return the lowest culled peak and the highest culled peak less than the
     * precursor mass passed in
     * 
     * @param NumLo number of peak below mh/2
     * @param NumHi number of peaks above mh/2 and below mh
     */
    void HighLow(int& High,
                 int& Low,
                 int& NumPeaks, 
                 const int PrecursorMass,
                 const int Charge,
                 const double Threshold,
                 int& NumLo,
                 int& NumHi
                 );

    /**
     * count number of AA intervals in spectrum.
     */
    const int CountAAIntervals(const CMassArray& MassArray,
                               const bool Nodup=true,
                               const int Which = MSCULLED1) const;
	
    /**
     *  counts the number of peaks above % of maximum peak
     */
    const int AboveThresh(const double Threshold,
                          const int Which = MSORIGINAL) const;
	
    /**
     *  the number of peaks at and below the precursor ion
     */
    const int PercentBelow(void) const;

    /**
     * return the number of peaks in a range. range is in fractions of MH
     */
    const int CountRange(const double StartFraction,
                         const double StopFraction) const;

    /**
     * return the number of peaks in a range
     * 
     * @param Start inclusive start of range in integer m/z
     * @param Stop exclusive stop of range in integer m/z
     * @param MaxIntensity the minimum intensity that a peak can have for counting
     * @param Which which noise filtered spectrum to use
     */
    const int CountMZRange(const int StartIn,
                           const int StopIn,
                           const double MinIntensity,
                           const int Which) const;
        

    /**
     *  takes the ratio, low/high, of two ranges in the spectrum
     */
    const double RangeRatio(const double Start,
                            const double Middle, 
                            const double Stop) const;


    // various charge functions

    
    void SetPlusOne(const double PlusIn);

    /**
     *  is the data charge +1?
     */
    const bool IsPlus1(const double PercentBelowIn) const;

    /**
     *  calculates charge based on threshold and sets charge value 
     * 
     * @param ChargeHandle contains info on how to deal with charge
     */
    void SetComputedCharge(const CMSChargeHandle& ChargeHandle);

    /**
     * return the computed charge state
     */
    const EChargeState GetComputedCharge(void) const;

    /**
     *  return allowed computed charges
     */
    int * GetCharges(void);

    /**
     *  return number of allowed computed charges
     */
    const int GetNumCharges(void) const;

    /**
     * Get the number of peaks
     * 
     * @param Which which experimental spectrum to use
     */
    const unsigned GetNum(const int Which = MSORIGINAL) const;

    /**
     * Get the m/z and intensity array
     * 
     * @param Which which experimental spectrum to use
     */
    CMZI *GetMZI(const int Which = MSORIGINAL) const;

    /**
     * compare peaks to ladder using ContainsFast
     *
     * @param Ladder the ladder to compare
     * @param Which which experimental spectrum to use
     */
    const int Compare(CLadder& Ladder, const int Which = MSCULLED1) const;

    /**
     * see if value is contained in peaks
     * 
     * @param value the m/z to compare
     * @param Which which experimental spectrum to use
     */
    const bool Contains(const int value,
                        const int Which) const;


    /**
     * see if value is contained in peaks using binary search
     * 
     * @param value the m/z value to compare
     * @param Which which experimental spectrum to use
     */
    const bool ContainsFast(const int value,
                            const int Which) const;

    /**
     * compares only the top hits
     * 
     * @param Ladder ladder to compare to
     */
    const bool CompareTop(CLadder& Ladder) const;

    /**
     * Get Maximum intensity
     * 
     * @param Which which experimental spectrum to use
     */
    const int GetMaxI(const int Which = MSORIGINAL) const;

    /**
     * returns the cull array index
     * 
     * @param Which which experimental spectrum to use
     */
    const int GetWhich(const int Charge) const;

    /**
     * compare assuming all lists are sorted
     * the intensity array holds the intensity if there is a match to the ladder
     * returns total number of matches, which may be more than is recorded in the ladder due to overlap
     * 
     * 
     * @param Intensity is optional argument that allows recording of the intensity
     * @param Which which experimental spectrum to use
     * @param Ladder the ladder to compare to
     */
    const int CompareSorted(CLadder& Ladder, const int Which, TIntensity * Intensity) const;

    /**
     * initializes arrays used to track hits
     * 
     * @param Minhit minimal number of hits for a match
     */
    void InitHitList(const int Minhit
		      );

    /**
     * Get a hit list
     * 
     * @param Index which hit list
     */
    TMSHitList& GetHitList(const int Index);

    /**
     * Get size of hit list
     * 
     * @param Index which hit list
     */
    const int GetHitListIndex(const int Index) const;

    /**
     * add hit to hitlist.  returns true and the added hit if successful
     * 
     * @param in Hit to add
     * @param out the added hit
     */
    const bool AddHit(CMSHit& in, CMSHit*& out);


    /**
     * return number of peptides examine for each charge state
     * 
     * @param ChargeIn charge state
     */
    const int GetPeptidesExamined(const int ChargeIn) const;

    /**
     * set the number of peptides examine for each charge state
     * 
     * @param ChargeIn charge state
     */
    int& SetPeptidesExamined(const int ChargeIn);


    // getter-setters
   
    /**
     *  get precursor m/z
     */
    const int GetPrecursormz(void) const;

    /**
     * calculates neutral mass
     * 
     * @param PrecusorCharge the charge to assume
     */
    const int CalcPrecursorMass(const int PrecursorCharge) const;

	/**
     * gets min precursor charge to consider multiply charged product ions
     */
	const int GetConsiderMult(void) const;  

    /**
     * return any errors in computing on peaks
     */
    const EMSHitError GetError(void) const;

    /**
     * set any errors in computing on peaks
     * 
     * @param ErrorIn what was the error?
     */
    void SetError(const EMSHitError ErrorIn);

    /**
     * set the names of the spectrum
     */
    CMSSpectrum::TIds& SetName(void);

    /**
     * get the names of the spectrum
     */
    const CMSSpectrum::TIds& GetName(void) const;

    /**
     * set the spectrum number
     */
    int& SetNumber(void);

    /**
     * get the spectrum number
     */
    const int GetNumber(void) const;

    /**
     * set the product mass tolerance in Daltons.
     * 
     * @param tolin unscaled mass tolerance
     */
    void SetTolerance(const double tolin);

	/**
     * get the product mass tolerance in Daltons.
     */
    const int GetTol(void) const;

	/**
     * set the precursor mass tolerance in Daltons.
     * 
     * @param tolin precursor mass tolerance
     */
	void SetPrecursorTol(double tolin);

	/**
     * get the precursor mass tolerance in Daltons.
     */
	const int GetPrecursorTol(void) const;

    /**
     * return array to mark peaks as used
     * 
     * @param Which which experimental spectrum to use
     */
    char *SetUsed(const int Which);

    /**
     * clear used arrays for one cull type
     * 
     * @param Which which experimental spectrum to use
     */
    void ClearUsed(const int Which);

    /**
     * clear used arrays for all cull types
     */
    void ClearUsedAll(void);

    // functions for testing if peaks are h2o or nh3 losses
    
    /**
     * check to see if TestMZ is Diff away from BigMZ
     * 
     * @param BigMZ the major ion
     * @param TestMZ the minor ion
     * @param Diff distance between minor and major ions
     * @param tol mass tolerance
     */
    const bool IsAtMZ(const int BigMZ, 
                      const int TestMZ, 
                      const int Diff, 
                      const int tol) const;

    /**
     * see if TestMZ can be associated with BigMZ, e.g. water loss, etc.
     * 
     * @param BigMZ the major ion
     * @param TestMZ the minor ion
     * @param tol mass tolerance
     */
    const bool IsMajorPeak(const int BigMZ, 
                           const int TestMZ, 
                           const int tol) const;



private:
    CMZI *MZI[MSNUMDATA]; // m/z values and intensities, sorted by m/z.  first is original, second is culled
    char *Used[MSNUMDATA];  // used to mark m/z values as used in a match
    unsigned Num[MSNUMDATA]; // number of CMZI.  first is original, second is culled
    bool Sorted[MSNUMDATA]; // have the CMZI been sorted?
    CMZI ** IntensitySort;  // points to CMZI original, sorted.
    int Precursormz;
    int Charges[MSMAXCHARGE];  // Computed allowed charges
    int NumCharges;  // array size of Charges[]
	
    //! product error tolerance of peptide
    int tol;
	//! precursor error tolerance
	int PrecursorTol;

    double PlusOne;  // value used to determine if spectra is +1
    EChargeState ComputedCharge;  // algorithmically calculated 
	int ConsiderMult;  // at what precursor charge should multiply charged products be considered?
	int MaxCharge;  // maximum precursor charge to consider
	int MinCharge;  // minimum precursor charge to consider
    CAA AA;

    CMSSpectrum::TIds Name;  // name taken from spectrum
    int Number;  // spectrum number taken from spectrum

    // list of hits
    TMSHitList HitList[MSMAXCHARGE];
    int HitListSize;  // max size of hit list
    int HitListIndex[MSMAXCHARGE];  // current size of HitList
    int LastHitNum[MSMAXCHARGE];  // the smallest hit currently in List
    int PeptidesExamined[MSMAXCHARGE];  // the number of peptides examined in search

    EMSHitError Error; // errors that have occurred in processing

};


///////////////////   CMSPeak inline methods

inline 
void CMSPeak::SetPlusOne(const double PlusIn) 
{ 
    PlusOne = PlusIn; 
}

inline 
const EChargeState CMSPeak::GetComputedCharge(void) const
{ 
    return ComputedCharge; 
}

inline 
const unsigned CMSPeak::GetNum(const int Which) const
{ 
    return Num[Which];
}

inline 
CMZI * CMSPeak::GetMZI(const int Which) const 
{ 
    return MZI[Which];
}


inline 
TMSHitList& CMSPeak::GetHitList(const int Index)
{ 
    return HitList[Index]; 
}

inline 
const int CMSPeak::GetHitListIndex(const int Index) const
{ 
    return HitListIndex[Index]; 
}

inline 
const int CMSPeak::GetPeptidesExamined(const int ChargeIn) const
{ 
    return PeptidesExamined[ChargeIn - Charges[0]];
}

inline 
int& CMSPeak::SetPeptidesExamined(const int ChargeIn) 
{ 
    return PeptidesExamined[ChargeIn - Charges[0]];
}

inline 
const int CMSPeak::GetPrecursormz(void) const
{
    return Precursormz;
}

inline 
const int CMSPeak::CalcPrecursorMass(const int PrecursorCharge) const
{
    return Precursormz * PrecursorCharge - MSSCALE2INT(PrecursorCharge * kProton);
}


inline
const int CMSPeak::GetConsiderMult(void) const  
{
	return ConsiderMult;
}

inline 
const EMSHitError CMSPeak::GetError(void) const
{
    return Error; 
}

inline 
void CMSPeak::SetError(const EMSHitError ErrorIn) 
{
    Error = ErrorIn; 
}

inline 
CMSSpectrum::TIds& CMSPeak::SetName(void) 
{ 
    return Name; 
}

inline 
const CMSSpectrum::TIds& CMSPeak::GetName(void) const 
{ 
    return Name; 
}

inline 
int& CMSPeak::SetNumber(void) 
{ 
    return Number; 
}

inline 
const int CMSPeak::GetNumber(void) const 
{ 
    return Number; 
}

inline 
void CMSPeak::SetTolerance(const double tolin)
{
    tol = MSSCALE2INT(tolin);
}

inline 
const int CMSPeak::GetTol(void) const
{ 
    return tol; 
}

inline 
void CMSPeak::SetPrecursorTol(double tolin)
{
    PrecursorTol = MSSCALE2INT(tolin);
}

inline 
const int CMSPeak::GetPrecursorTol(void) const
{ 
    return PrecursorTol; 
}

inline 
char *CMSPeak::SetUsed(const int Which)
{
    return Used[Which];
}

inline 
void CMSPeak::ClearUsed(const int Which)
{
    memset(Used[Which], 0, Num[Which]);
}

inline 
void CMSPeak::ClearUsedAll(void)
{
    int iCharges;
    for(iCharges = 0; iCharges < GetNumCharges(); iCharges++)
	ClearUsed(GetWhich(GetCharges()[iCharges]));
    ClearUsed(MSTOPHITS);
}

// returns the cull array index
inline 
const int CMSPeak::GetWhich(const int Charge) const
{
    if(Charge < ConsiderMult) return MSCULLED1;
    else return MSCULLED2;
}

inline
int * CMSPeak::GetCharges(void)
{ 
    return Charges;
}

inline
const int CMSPeak::GetNumCharges(void) const
{ 
    return NumCharges; 
}

/////////////////// end of  CMSPeak  inline methods


/////////////////////////////////////////////////////////////////////////////
//
//  CMSPeakSet::
//
//  Class used to hold sets of CMSPeak and access them quickly
//


typedef deque <CMSPeak *> TPeakSet;

class NCBI_XOMSSA_EXPORT _MassPeak: public CObject {
public:
    int Mass, Peptol;
    int Charge;
    CMSPeak *Peak;
};

typedef _MassPeak TMassPeak;


// range type for peptide mass +/- some tolerance
typedef CRange<TSignedSeqPos> TMassRange;

class NCBI_XOMSSA_EXPORT CMSPeakSet {
public:
    // tor's
    CMSPeakSet(void);
    ~CMSPeakSet();

    void AddPeak(CMSPeak *PeakIn);

    /**
     *  put the pointers into an array sorted by mass
     *
     * @param Peptol the precursor mass tolerance
     * @param Zdep should the tolerance be charge dependent?
     * @return maximum m/z value
     */
    int SortPeaks(
        int Peptol,
        int Zdep
        );
    
    TPeakSet& GetPeaks(void);
    CIntervalTree& SetIntervalTree(void);

private:
    TPeakSet PeakSet;  // peak list for deletion
    CIntervalTree MassIntervals;
};

///////////////////   CMSPeakSet inline methods

inline CMSPeakSet::CMSPeakSet(void)
{}

inline void CMSPeakSet::AddPeak(CMSPeak *PeakIn)
{ 
    PeakSet.push_back(PeakIn); 
}

inline
CIntervalTree& CMSPeakSet::SetIntervalTree(void)
{
    return MassIntervals;
}

inline 
TPeakSet& CMSPeakSet::GetPeaks(void) 
{ 
    return PeakSet; 
}

/////////////////// end of CMSPeakSet inline methods

END_SCOPE(omssa)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif

/*
  $Log$
  Revision 1.37  2006/03/13 15:48:11  lewisg
  omssamerge and intermediate score fixes

  Revision 1.36  2006/01/23 17:47:37  lewisg
  refactor scoring

  Revision 1.35  2005/11/18 15:11:40  lewisg
  move code from CSearch into CMSPeak

  Revision 1.34  2005/10/24 21:46:13  lewisg
  exact mass, peptide size limits, validation, code cleanup

  Revision 1.33  2005/09/21 18:05:59  lewisg
  speed up non-specific search, add fields to result

  Revision 1.32  2005/09/14 18:50:56  lewisg
  add theoretical mass to hit

  Revision 1.31  2005/09/14 15:30:17  lewisg
  neutral loss

  Revision 1.30  2005/08/15 14:24:56  lewisg
  new mod, enzyme; stat test

  Revision 1.29  2005/08/03 17:59:29  lewisg
  *** empty log message ***

  Revision 1.28  2005/08/01 13:44:18  lewisg
  redo enzyme classes, no-enzyme, fix for fixed mod enumeration

  Revision 1.27  2005/05/13 17:57:17  lewisg
  one mod per site and bug fixes

  Revision 1.26  2005/04/21 21:54:03  lewisg
  fix Jeri's mem bug, split off mod file, add aspn and gluc

  Revision 1.25  2005/01/31 17:30:57  lewisg
  adjustable intensity, z dpendence of precursor mass tolerance

  Revision 1.24  2004/12/20 20:24:16  lewisg
  fix uncalc ladder bug, cleanup

  Revision 1.23  2004/12/06 23:35:16  lewisg
  get rid of file charge

  Revision 1.22  2004/12/06 22:57:34  lewisg
  add new file formats

  Revision 1.21  2004/12/03 21:14:16  lewisg
  file loading code

  Revision 1.20  2004/11/30 23:39:57  lewisg
  fix interval query

  Revision 1.19  2004/11/22 23:10:36  lewisg
  add evalue cutoff, fix fixed mods

  Revision 1.18  2004/10/20 22:24:48  lewisg
  neutral mass bugfix, concatenate result and response

  Revision 1.17  2004/09/15 18:35:00  lewisg
  cz ions

  Revision 1.16  2004/07/22 22:22:58  lewisg
  output mods

  Revision 1.15  2004/06/08 19:46:21  lewisg
  input validation, additional user settable parameters

  Revision 1.14  2004/05/27 20:52:15  lewisg
  better exception checking, use of AutoPtr, command line parsing

  Revision 1.13  2004/04/06 19:53:20  lewisg
  allow adjustment of precursor charges that allow multiply charged product ions

  Revision 1.12  2004/03/30 19:36:59  lewisg
  multiple mod code

  Revision 1.11  2004/03/16 20:18:54  gorelenk
  Changed includes of private headers.

  Revision 1.10  2003/12/22 23:03:18  lewisg
  top hit code and variable mod fixes

  Revision 1.9  2003/12/08 17:37:20  ucko
  #include <string.h> rather than <cstring>, since MIPSpro lacks the latter.

  Revision 1.8  2003/12/05 13:10:32  lewisg
  delete GetUsed

  Revision 1.7  2003/12/04 23:39:08  lewisg
  no-overlap hits and various bugfixes

  Revision 1.6  2003/11/14 20:28:05  lewisg
  scale precursor tolerance by charge

  Revision 1.5  2003/11/10 22:24:12  lewisg
  allow hitlist size to vary

  Revision 1.4  2003/10/24 21:28:41  lewisg
  add omssa, xomssa, omssacl to win32 build, including dll

  Revision 1.3  2003/10/21 21:12:17  lewisg
  reorder headers

  Revision 1.2  2003/10/21 03:43:20  lewisg
  fix double default

  Revision 1.1  2003/10/20 21:32:13  lewisg
  ommsa toolkit version

  Revision 1.20  2003/10/07 18:02:28  lewisg
  prep for toolkit

  Revision 1.19  2003/10/06 18:14:17  lewisg
  threshold vary

  Revision 1.18  2003/08/14 23:49:22  lewisg
  first pass at variable mod

  Revision 1.17  2003/08/06 18:29:11  lewisg
  support for filenames, numbers using regex

  Revision 1.16  2003/07/21 20:25:03  lewisg
  fix missing peak bug

  Revision 1.15  2003/07/19 15:07:38  lewisg
  indexed peaks

  Revision 1.14  2003/07/18 20:50:34  lewisg
  *** empty log message ***

  Revision 1.13  2003/07/17 18:45:50  lewisg
  multi dta support

  Revision 1.12  2003/07/07 16:17:51  lewisg
  new poisson distribution and turn off histogram

  Revision 1.11  2003/04/24 18:45:55  lewisg
  performance enhancements to ladder creation and peak compare

  Revision 1.10  2003/04/18 20:46:52  lewisg
  add graphing to omssa

  Revision 1.9  2003/04/02 18:49:51  lewisg
  improved score, architectural changes

  Revision 1.8  2003/03/21 21:14:40  lewisg
  merge ming's code, other stuff

  Revision 1.7  2003/02/07 16:18:23  lewisg
  bugfixes for perf and to work on exp data

  Revision 1.6  2003/02/03 20:39:02  lewisg
  omssa cgi

  Revision 1.5  2003/01/21 22:19:26  lewisg
  get rid of extra 2 aa function

  Revision 1.4  2003/01/21 21:46:12  lewisg
  *** empty log message ***

  Revision 1.3  2002/11/27 00:07:52  lewisg
  fix conflicts

  Revision 1.2  2002/11/26 00:53:34  lewisg
  sync versions

  Revision 1.1  2002/07/16 13:27:09  lewisg
  *** empty log message ***

  
	
*/
