#ifndef FORMATGUESS__HPP
#define FORMATGUESS__HPP

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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:  Different "fuzzy-logic" methods to identify file formats.
 *
 */

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////
//
// Class implements different ad-hoc unreliable file format 
// identifications.
//

class NCBI_XUTIL_EXPORT CFormatGuess
{
public:
    enum EFormat {
        eUnknown = 0,
        eBinaryASN,
        eTextASN,
        eFasta,
        eXml,
        eRmo,           // repeatmasker out
        eGlimmer3,      // Glimmer3 predictions file
        ePhrapAce,
        eGtf,
        eAgp,
        eNewick,
        eAlignment
    };

    enum ESequenceType {
        eUndefined,
        eNucleotide,
        eProtein
    };

    // Guess sequence type. Function calculates sequence alphabet and 
    // identifies if the source belongs to nucleotide or protein sequence
    static ESequenceType SequenceType(const char* str, unsigned length);

    // Guess file format structure.
    EFormat Format(const string& path);
    EFormat Format(CNcbiIstream& input);

};

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2006/08/17 13:19:57  ludwigf
 * ADDED: Simple test that will identify *some* alignment files.
 *
 * Revision 1.5  2006/06/15 17:45:40  dicuccio
 * Added recognition of Glimmer3 predictions
 *
 * Revision 1.4  2006/06/05 15:06:36  ludwigf
 * ADDED: IDs for the PHRAP ACE, GTF, AGP, and Newick tree file types.
 *
 * Revision 1.3  2004/03/23 22:29:50  jcherry
 * Added Format(CNcbiIstream& input)
 *
 * Revision 1.2  2003/05/13 15:16:13  kuznets
 * added sequence type guessing function
 *
 * Revision 1.1  2003/05/08 19:45:55  kuznets
 * Initial revision
 *
 * ===========================================================================
 */

 #endif

