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
 *  and reliability of the software and data, the NLM 
 *  Although all reasonable efforts have been taken to ensure the accuracyand the U.S.
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
 *   Code for loading in spectrum datasets
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using specifications from the data definition file
 *   'omssa.asn'.
 */

#ifndef SPECTRUMSET_HPP
#define SPECTRUMSET_HPP


// generated includes
#include <objects/omssa/MSSpectrumset.hpp>

#include <iostream>

// generated classes
BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// proton mass
const double kProton = 1.008;

enum EFileType { eDTA, eDTABlank, eDTAXML, eASC, ePKL, ePKS, eSCIEX, eMGF, eUnknown };

class NCBI_XOMSSA_EXPORT CSpectrumSet : public CMSSpectrumset {
    //    typedef CMSSpectrumset_Base Tparent;
    typedef CMSSpectrumset Tparent;

public:

    // constructor
    CSpectrumSet(void);
    // destructor
    ~CSpectrumSet(void);

    ///
    /// wrapper for various file loaders
    ///
    int LoadFile(EFileType FileType, std::istream& DTA, int Max = 0);

    ///
    /// load in a single dta file
    ///
    int LoadDTA(
		std::istream& DTA  // stream containing dta
		);

    ///
    /// load in multiple dta files with <dta> tag separators
    ///
    /// returns -1 if more than Max spectra read
    int LoadMultDTA(
		    std::istream& DTA,  // stream containing tag delimited dtas
            int Max = 0   // maximum number of dtas to read in, 0= no limit
		    );

    ///
    /// load multiple dta's separated by a blank line
    ///
    /// returns -1 if more than Max spectra read
    int LoadMultBlankLineDTA(
			     std::istream& DTA,  // stream containing blank delimited dtas
                 int Max = 0,   // maximum number of dtas to read in, 0= no limit
                 bool isPKL = false     // pkl formatted?
			     );

    ///
    /// load mgf file
    ///
    /// returns -1 if more than Max spectra read
    int LoadMGF(
			     std::istream& DTA,  // stream containing mgf file
                 int Max = 0   // maximum number of dtas to read in, 0= no limit
			     );

protected:

    ///
    ///  Read in the header of a DTA file
    ///
    bool GetDTAHeader(
		      std::istream& DTA,  // input stream
		      CRef <CMSSpectrum>& MySpectrum,   // asn.1 container for spectra
              bool isPKL = false     // pkl formatted?
		      );

    ///
    /// Read in the body of a dta file
    ///
    bool GetDTABody(
				  std::istream& DTA,   // input stream
				  CRef <CMSSpectrum>& MySpectrum   // asn.1 container for spectra
				  );

    ///
    /// Read in an ms/ms block in an mgf file
    ///
    int GetMGFBlock(std::istream& DTA, CRef <CMSSpectrum>& MySpectrum); 

private:
    // Prohibit copy constructor and assignment operator
    CSpectrumSet(const CSpectrumSet& value);
    CSpectrumSet& operator=(const CSpectrumSet& value);

};



/////////////////// CSpectrumSet inline methods

// constructor
inline
CSpectrumSet::CSpectrumSet(void)
{
}


// destructor
inline
CSpectrumSet::~CSpectrumSet(void)
{
}

/////////////////// end of CSpectrumSet inline methods



/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.7  2004/12/06 22:57:34  lewisg
 * add new file formats
 *
 * Revision 1.6  2004/12/03 21:14:16  lewisg
 * file loading code
 *
 * Revision 1.5  2004/11/01 22:04:01  lewisg
 * c-term mods
 *
 * Revision 1.4  2004/05/27 20:52:15  lewisg
 * better exception checking, use of AutoPtr, command line parsing
 *
 * Revision 1.3  2004/03/16 20:18:54  gorelenk
 * Changed includes of private headers.
 *
 * Revision 1.2  2003/10/24 21:28:41  lewisg
 * add omssa, xomssa, omssacl to win32 build, including dll
 *
 * Revision 1.1  2003/10/20 21:32:13  lewisg
 * ommsa toolkit version
 *
 *
 *
 * ===========================================================================
 */

END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

#endif // SPECTRUMSET_HPP
/* Original file checksum: lines: 85, chars: 2278, CRC32: c22a6458 */
