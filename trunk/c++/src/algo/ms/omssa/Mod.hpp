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
 * Author:  .......
 *
 * File Description:
 *   .......
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using specifications from the data definition file
 *   'omssa.asn'.
 */

#ifndef OBJECTS_OMSSA_MOD_HPP
#define OBJECTS_OMSSA_MOD_HPP


// generated includes
#include <objects/omssa/MSMod.hpp>
#include <objects/omssa/MSSearchSettings.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::


///
/// Modification types
/// there are five kinds of mods:
/// 1. specific to an AA
/// 2. N terminus protein, not specific to an AA
/// 3. N terminus protein, specific to an AA
/// 4. C terminus protein, not specific to an AA
/// 5. C terminus protein, specific to an AA
/// 6. N terminus peptide, not specific
/// 7. N terminus peptide, specific AA
/// 8. C terminus peptide, not specific
/// 9. C terminus peptide, specific AA
///

const int kNumModType = 9;

enum EMSModType {
    eModAA = 0,
    eModN,
    eModNAA,
    eModC,
    eModCAA,
    eModNP,
    eModNPAA,
    eModCP,
    eModCPAA
};


/////////////////////////////////////////////////////////////////////////////
//
//  Informational arrays for mods
//
//  These are separate arrays for speed considerations
//

///
/// categorizes existing mods as the types listed above
///
const EMSModType ModTypes[eMSMod_max] = {
    eModAA,
    eModAA,
    eModAA,
    eModAA,
    eModAA,
    eModAA,
    eModAA,
    eModAA,
    eModAA,
    eModNAA,
    eModN,
    eModN,
    eModN,
    eModAA,
    eModAA,
    eModAA,
    eModAA,
    eModAA,
    eModCP,
    eModAA,
    eModAA,
    eModCP
};

///
/// the names of the various modifications codified in the asn.1
///
char const * const kModNames[eMSMod_max] = {
    "methylation of K",
    "oxidation of M",
    "carboxymethyl C",
    "carbamidomethyl C",
    "deamidation of N and Q",
    "propionamide C",
    "phosphorylation of S",
    "phosphorylation of T",
    "phosphorylation of Y",
    "N-term M removal",
    "N-term acetylation",
    "N-term methylation",
    "N-term trimethylation",
    "beta methythiolation of D",
    "methylation of Q",
    "trimethylation of K",
    "methylation of D",
    "methylation of E",
    "C-term peptide methylation",
    "trideuteromethylation of D",
    "trideuteromethylation of E",
    "C-term peptide trideuteromethylation"
};	   
 
///
/// the characters to compare
/// rows are indexed by mod
/// column are the AA's modified (if any)
///
const char ModChar [3][eMSMod_max] = {
    {'\x0a','\x0c','\x03','\x03','\x0d','\x03','\x11','\x12','\x16','\x0c','\x00','\x00','\x00','\x04','\x0f','\x0a','\x04','\x05','\x00','\x04','\x05','\x00' },
    {'\x00','\x00','\x00','\x00','\x0f','\x00','\x00','\x00','\x00','\x00','\x00','\x00','\x00','\x00','\x00','\x00','\x00','\x00','\x00','\x00','\x00','\x00' },
    {'\x00','\x00','\x00','\x00','\x00','\x00','\x00','\x00','\x00','\x00','\x00','\x00','\x00','\x00','\x00','\x00','\x00','\x00','\x00','\x00','\x00','\x00' }
};

///
/// the number of characters to compare
///
const int NumModChars[eMSMod_max] = { 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0 };

///
/// the modification masses
///
const int ModMass[eMSMod_max] = { 1402, 1600, 5801, 5702, 98, 7104, 7997, 7997, 7997, -13104, 4201, 1402, 4205, 4599, 1402, 4205, 1402, 1402, 1402, 1704, 1704, 1704};

/////////////////////////////////////////////////////////////////////////////
//
//  CMSMod::
//
//  Given a set of variable mods, sorts them into categories for quick access
//

class NCBI_XOMSSA_EXPORT CMSMod {
public:
    CMSMod(void) {};
    CMSMod(const CMSSearchSettings::TVariable &Mods);
    // initialize variable mod type array
    void Init(const CMSSearchSettings::TVariable &Mods);
    const CMSSearchSettings::TVariable &GetAAMods(EMSModType Type) const;    
private:
    CMSSearchSettings::TVariable ModLists[kNumModType];
};

///////////////////  CMSMod  inline methods

inline 
const CMSSearchSettings::TVariable & CMSMod::GetAAMods(EMSModType Type) const
{ 
    return ModLists[Type]; 
}


END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.9  2004/11/30 23:39:57  lewisg
* fix interval query
*
* Revision 1.8  2004/11/17 23:42:11  lewisg
* add cterm pep mods, fix prob for tophitnum
*
* Revision 1.7  2004/11/01 22:04:01  lewisg
* c-term mods
*
* Revision 1.6  2004/06/23 22:34:36  lewisg
* add multiple enzymes
*
* Revision 1.5  2004/06/21 21:19:27  lewisg
* new mods (including n term) and sample perl parser
*
* Revision 1.4  2004/06/08 19:46:21  lewisg
* input validation, additional user settable parameters
*
* Revision 1.3  2004/05/27 20:52:15  lewisg
* better exception checking, use of AutoPtr, command line parsing
*
* Revision 1.2  2004/03/04 02:22:49  lewisg
* add msvc defines
*
* Revision 1.1  2004/03/01 18:24:07  lewisg
* better mod handling
*
*
* ===========================================================================
*/

#endif // OBJECTS_OMSSA_MSMOD_HPP
/* Original file checksum: lines: 63, chars: 1907, CRC32: 6c23d0ae */
