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
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Lewis Y. Geer
 *
 * File Description:
 *   Contains code for reading in spectrum data sets.
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using specifications from the data definition file
 *   'omssa.asn'.
 */

// standard includes
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.h>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbistr.hpp>
#include <util/regexp.hpp>
#include <objects/omssa/MSSpectrum.hpp>


// generated includes
#include "SpectrumSet.hpp"

// added includes
#include "msms.hpp"
#include <math.h>

// generated classes
BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::


/////////////////////////////////////////////////////////////////////////////
//
//  CSpectrumSet::
//


///
/// wrapper for various file loaders
///

int CSpectrumSet::LoadFile(EFileType FileType, std::istream& DTA, int Max)
{
    switch (FileType) {
    case eDTA:
        return LoadDTA(DTA);
        break;
    case eDTABlank:
        return LoadMultBlankLineDTA(DTA, Max);
        break;
    case eDTAXML:
        return LoadMultDTA(DTA, Max);
        break;
    case ePKL:
        return LoadMultBlankLineDTA(DTA, Max, true);
        break;
    case eMGF:
        return LoadMGF(DTA, Max);
        break;
    case eASC:
    case ePKS:
    case eSCIEX:
    case eUnknown:
    default:
        break;
    }
    return 1;  // not supported
}

///
/// load multiple dta's in xml-like format
///
int CSpectrumSet::LoadMultDTA(std::istream& DTA, int Max)
{   
    CRef <CMSSpectrum> MySpectrum;
    int iIndex(-1); // the spectrum index
    int Count(0);  // total number of spectra
    string Line;
    //    double dummy;
    bool GotOne(false);  // has a spectrum been read?
    try {
        //       DTA.exceptions(ios_base::failbit | ios_base::badbit);
        do {
            do {
                getline(DTA, Line);
            } while (NStr::Compare(Line, 0, 4, "<dta") != 0 && DTA && !DTA.eof());
            if (!DTA || DTA.eof()) {
                if (GotOne) {
                    ERR_POST(Info << "LoadMultDTA: end of xml");
                    return 0;
                }
                else {
                    ERR_POST(Info << "LoadMultDTA: bad end of xml");
                    return 1;
                }
            }
//            GotOne = true;
            Count++;
            if (Max > 0 && Count > Max) {
                ERR_POST(Info << "LoadMultDTA: too many spectra in xml");
                return -1;  // too many
            }

            MySpectrum = new CMSSpectrum;
            CRegexp RxpGetNum("\\sid\\s*=\\s*(\"(\\S+)\"|(\\S+)\b)");
            string Match;
            if ((Match = RxpGetNum.GetMatch(Line.c_str(), 0, 2)) != "" ||
                (Match = RxpGetNum.GetMatch(Line.c_str(), 0, 3)) != "") {
                MySpectrum->SetNumber(NStr::StringToInt(Match));
            } else {
                MySpectrum->SetNumber(iIndex);
                iIndex--;
            }

            CRegexp RxpGetName("\\sname\\s*=\\s*(\"(\\S+)\"|(\\S+)\b)");
            if ((Match = RxpGetName.GetMatch(Line.c_str(), 0, 2)) != "" ||
                (Match = RxpGetName.GetMatch(Line.c_str(), 0, 3)) != "") {
                MySpectrum->SetIds().push_back(Match);
            }

            if(!GetDTAHeader(DTA, MySpectrum)) {
                ERR_POST(Info << "LoadMultDTA: not able to get header");
                return 1;
            }
            getline(DTA, Line);
            getline(DTA, Line);
            TInputPeaks InputPeaks;

            while (NStr::Compare(Line, 0, 5, "</dta") != 0) {
                CNcbiIstrstream istr(Line.c_str());
                if (!GetDTABody(istr, InputPeaks)) 
                    break;
                GotOne = true;
                getline(DTA, Line);
            } 
            if(GotOne) {
                Peaks2Spectrum(InputPeaks, MySpectrum);
                Set().push_back(MySpectrum);
            }
        } while (DTA && !DTA.eof());

    if (!GotOne) {
        ERR_POST(Info << "LoadMultDTA: didn't get one");
        return 1;
    }
        
    } catch (NCBI_NS_STD::exception& e) {
        ERR_POST(Info << "Exception in CSpectrumSet::LoadMultDTA: " << e.what());
        throw;
    } catch (...) {
        ERR_POST(Info << "Exception in CSpectrumSet::LoadMultDTA: " );
        throw;
    }
    return 0;
}

///
/// load multiple dta's separated by a blank line
///
int CSpectrumSet::LoadMultBlankLineDTA(std::istream& DTA, int Max, bool isPKL)
{   
    CRef <CMSSpectrum> MySpectrum;
    int iIndex(0); // the spectrum index
    int Count(0);  // total number of spectra
    string Line;
    bool GotOne(false);  // has a spectrum been read?
    try {
//        DTA.exceptions(ios_base::failbit | ios_base::badbit);
        do {
//            GotOne = true;
            Count++;
            if (Max > 0 && Count > Max) 
                return -1;  // too many

            MySpectrum = new CMSSpectrum;
            MySpectrum->SetNumber(iIndex);
            iIndex++;

            if (!GetDTAHeader(DTA, MySpectrum, isPKL)) 
                if(DTA.eof() && GotOne) 
                    break;  // probably the end of the file
                else
                    return 1;
            getline(DTA, Line);
            getline(DTA, Line);

            if (!DTA || DTA.eof()) {
                if (GotOne) 
                    return 0;
                else 
                    return 1;
            }

            TInputPeaks InputPeaks;
            while (Line != "") {
                CNcbiIstrstream istr(Line.c_str());
                if (!GetDTABody(istr, InputPeaks)) 
                    break;
                GotOne = true;
                getline(DTA, Line);
            } 
            if(GotOne) {
                Peaks2Spectrum(InputPeaks, MySpectrum);
                Set().push_back(MySpectrum);
            }
        } while (DTA && !DTA.eof());

        if (!GotOne) 
            return 1;

    } catch (NCBI_NS_STD::exception& e) {
        ERR_POST(Info << "Exception in CSpectrumSet::LoadMultBlankLineDTA: " << e.what());
        throw;
    } catch (...) {
        ERR_POST(Info << "Exception in CSpectrumSet::LoadMultBlankLineDTA: " );
        throw;
    }

    return 0;
}


///
///  Read in the header of a DTA file
///
bool CSpectrumSet::GetDTAHeader(std::istream& DTA, CRef <CMSSpectrum>& MySpectrum,
                                bool isPKL)
{
    double dummy(0.0L);
    double precursor(0.0L);

    // read in precursor
    DTA >> precursor;
    if (precursor <= 0) {
        return false;
    }

    // read in pkl intensity
    if(isPKL) {
        DTA >> dummy;
        if (dummy <= 0) {
            return false;
        }
    }

    // read in charge
    DTA >> dummy;
    if (dummy <= 0) {
        return false;
    }
    MySpectrum->SetCharge().push_back(static_cast <int> (dummy)); 

    // correct dta MH+ to true precursor
    if(!isPKL) {
        precursor = (precursor + (dummy - 1)*kProton ) / dummy;
    }
    MySpectrum->SetPrecursormz(static_cast <int> (precursor*MSSCALE));

    return true;
}


//! Convert peak list to spectrum
/*!
    \param InputPeaks list of the input m/z and intensity values
    \param MySpectrum the spectrum to receive the scaled input
    \return success
*/

bool CSpectrumSet::Peaks2Spectrum(const TInputPeaks& InputPeaks, CRef <CMSSpectrum>& MySpectrum) const
{
    int i;

    float MaxI(0.0);

    // find max peak
    for (i = 0; i < InputPeaks.size(); ++i) {
        if(InputPeaks[i].Intensity > MaxI) MaxI = InputPeaks[i].Intensity;
    }

    // set scale
    double Scale = 1000000000.0/MaxI;
    // normalize it to a power of 10
    Scale = pow(10.0, floor(log10(Scale)));
    MySpectrum->SetIscale(Scale);

    // loop thru
    for (i = 0; i < InputPeaks.size(); ++i) {
        // push back m/z
        MySpectrum->SetMz().push_back(InputPeaks[i].mz);
        // convert and push back intensity 
        MySpectrum->SetAbundance().push_back(static_cast <int> (InputPeaks[i].Intensity*Scale));
    }
    return true;
}

///
/// Read in the body of a dta file
///
bool CSpectrumSet::GetDTABody(std::istream& DTA, TInputPeaks& InputPeaks)
{
    float dummy(0.0);
    TInputPeak InputPeak;

    DTA >> dummy;
    if (dummy <= 0) 
        return false;
    if (dummy > kMax_Int) dummy = kMax_Int/MSSCALE;
    InputPeak.mz = static_cast <int> (dummy*MSSCALE);
//    MySpectrum->SetMz().push_back(static_cast <int> (dummy*MSSCALE));

    DTA >> dummy;
    if (dummy <= 0) 
        return false;
//    if (dummy > kMax_Int) dummy = kMax_Int/MSISCALE;
    InputPeak.Intensity = dummy;
//    MySpectrum->SetAbundance().push_back(static_cast <int> (dummy*MSISCALE));

    InputPeaks.push_back(InputPeak);
    return true;
}


///
/// load in a single dta file
///
int CSpectrumSet::LoadDTA(std::istream& DTA)
{   
    CRef <CMSSpectrum> MySpectrum;
    bool GotOne(false);  // has a spectrum been read?

    try {
//        DTA.exceptions(ios_base::failbit | ios_base::badbit);

        MySpectrum = new CMSSpectrum;
        MySpectrum->SetNumber(1);
        if(!GetDTAHeader(DTA, MySpectrum)) return 1;

        TInputPeaks InputPeaks;
        while (DTA) {
            if (!GetDTABody(DTA, InputPeaks)) break;
            GotOne = true;
        } 

        if(GotOne) {
            Peaks2Spectrum(InputPeaks, MySpectrum);
            Set().push_back(MySpectrum);
        }

    } catch (NCBI_NS_STD::exception& e) {
        ERR_POST(Info << "Exception in CSpectrumSet::LoadDTA: " << e.what());
        throw;
    } catch (...) {
        ERR_POST(Info << "Exception in CSpectrumSet::LoadDTA: " );
        throw;
    }

    if (!GotOne) return 1;
    return 0;
}

///
/// load mgf
///

int CSpectrumSet::LoadMGF(std::istream& DTA, int Max)
{
    CRef <CMSSpectrum> MySpectrum;
    int iIndex(0); // the spectrum index
    int Count(0);  // total number of spectra
    int retval;
    bool GotOne(false);  // has a spectrum been read?
    try {
        
        do {
            Count++;
            if (Max > 0 && Count > Max) 
                return -1;  // too many

            MySpectrum = new CMSSpectrum;
            MySpectrum->SetNumber(iIndex);
            iIndex++;
            retval = GetMGFBlock(DTA, MySpectrum);

            if (retval == 0 ) {
                GotOne = true;
            }
            else if (retval == 1) {
                return 1;  // something went wrong
            }
            // retval = -1 means no more records found
            if (retval != -1) Set().push_back(MySpectrum);

        } while (DTA && !DTA.eof() && retval != -1);

        if (!GotOne) 
            return 1;

    } catch (NCBI_NS_STD::exception& e) {
        ERR_POST(Info << "Exception in CSpectrumSet::LoadMGF: " << e.what());
        throw;
    } catch (...) {
        ERR_POST(Info << "Exception in CSpectrumSet::LoadMGF: " );
        throw;
    }

    return 0;
}


///
/// Read in an ms/ms block in an mgf file
///
int CSpectrumSet::GetMGFBlock(std::istream& DTA, CRef <CMSSpectrum>& MySpectrum)
{
    string Line;
    bool GotMass(false);
    double dummy;
    TInputPeaks InputPeaks;

    // find the start of the block
    do {
        getline(DTA, Line);
        if(!DTA || DTA.eof()) 
            return -1;
    } while (NStr::CompareNocase(Line, 0, 10, "BEGIN IONS") != 0);

    // scan in headers
    do {
        getline(DTA, Line);
        if(!DTA || DTA.eof()) 
            return 1;
        if (NStr::CompareNocase(Line, 0, 6, "TITLE=") == 0) {
            MySpectrum->SetIds().push_back(Line.substr(6, Line.size()-6));
        }
        else if (NStr::CompareNocase(Line, 0, 8, "PEPMASS=") == 0) {
            // mandatory.
            GotMass = true;
            // create istrstream as some broken mgf generators insert whitespace separated cruft after mass
            string LastLine(Line.substr(8, Line.size()-8));
            CNcbiIstrstream istr(LastLine.c_str());
            double precursor;
            istr >> precursor;
            MySpectrum->SetPrecursormz(static_cast <int> (precursor*MSSCALE));
            MySpectrum->SetCharge().push_back(1);   // required in asn.1 (but shouldn't be)
            }
        // keep looping while the first character is not numeric
    } while (Line.substr(0, 1).find_first_not_of("0123456789.-") == 0);

    if(!GotMass) 
        return 1;

    while (NStr::CompareNocase(Line, 0, 8, "END IONS") != 0) {
        if(!DTA || DTA.eof()) 
            return 1;
        CNcbiIstrstream istr(Line.c_str());
        // get rid of blank lines (technically not allowed, but they do occur)
        if(Line.empty()) 
            goto skipone;
        // skip comments (technically not allowed)
        if(Line.find_first_of("#;/!") == 0) 
            goto skipone;

        if(!GetDTABody(istr, InputPeaks)) return 1;
skipone:
        getline(DTA, Line);
    } 
    Peaks2Spectrum(InputPeaks, MySpectrum);

    return 0;
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.16  2005/01/31 17:30:57  lewisg
 * adjustable intensity, z dpendence of precursor mass tolerance
 *
 * Revision 1.15  2004/12/06 23:39:28  lewisg
 * add fake charge to mgf
 *
 * Revision 1.14  2004/12/06 22:57:34  lewisg
 * add new file formats
 *
 * Revision 1.13  2004/12/03 21:14:16  lewisg
 * file loading code
 *
 * Revision 1.12  2004/11/09 17:57:05  lewisg
 * dta parsing fix
 *
 * Revision 1.11  2004/11/01 22:04:01  lewisg
 * c-term mods
 *
 * Revision 1.10  2004/10/20 22:24:48  lewisg
 * neutral mass bugfix, concatenate result and response
 *
 * Revision 1.9  2004/06/08 19:46:21  lewisg
 * input validation, additional user settable parameters
 *
 * Revision 1.8  2004/05/27 20:52:15  lewisg
 * better exception checking, use of AutoPtr, command line parsing
 *
 * Revision 1.7  2004/05/21 21:41:03  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.6  2004/03/30 19:36:59  lewisg
 * multiple mod code
 *
 * Revision 1.5  2004/03/16 20:18:54  gorelenk
 * Changed includes of private headers.
 *
 * Revision 1.4  2003/10/22 15:03:32  lewisg
 * limits and string compare changed for gcc 2.95 compatibility
 *
 * Revision 1.3  2003/10/21 21:20:57  lewisg
 * use strstream instead of sstream for gcc 2.95
 *
 * Revision 1.2  2003/10/21 21:12:16  lewisg
 * reorder headers
 *
 * Revision 1.1  2003/10/20 21:32:13  lewisg
 * ommsa toolkit version
 *
 *
 * ===========================================================================
 */
/* Original file checksum: lines: 56, chars: 1753, CRC32: bdc55e21 */
