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
        //< unknown format
        eUnknown = 0,

        //< binary ASN.1
        eBinaryASN,

        //< text ASN.1
        eTextASN,

        // FASTA format sequence record
        eFasta,

        //< XML
        eXml,

        //< RepeatMasker Output
        eRmo,

        //< Glimmer3 predictions
        eGlimmer3,

        //< Phrap ACE assembly file
        ePhrapAce,

        //< GFF/GTF style annotations
        eGtf,

        //< AGP format assembly
        eAgp,

        //< Newick file
        eNewick,

        //< Distance matrix file
        eDistanceMatrix,

        //< Five-column feature table
        eFiveColFeatureTable,

        //< Taxplot file
        eTaxplot,

        //< Generic table
        eTable,

        //< Text alignment
        eAlignment,

        //< GenBank/GenPept/DDBJ/EMBL flat-file sequence portion
        eFlatFileSequence,

        //< SNP Marker flat file
        eSnpMarkers,
        
        //< UCSC WIGGLE file format
        eWiggle,
        
        //< UCSC BED file format
        eBed,
        
        //< UCSC BED15 or microarray format
        eBed15
    };

    enum ESequenceType {
        eUndefined,
        eNucleotide,
        eProtein
    };

    enum EMode {
        eQuick,
        eThorough
    };

    enum EOnError { 
        eDefault = 0,      //< Return eUnknown 
        eThrowOnBadSource, //< Throw an exception if the data source (stream, file) can't be read 
    }; 
    
    // Guess sequence type. Function calculates sequence alphabet and 
    // identifies if the source belongs to nucleotide or protein sequence
    static ESequenceType SequenceType(const char* str, unsigned length);

    //  ------------------------------------------------------------------------
    //  "Stateless" interface:
    //  Useful for checking for all formats in one simple call.
    //  May go away; use object interface instead.
    //  ------------------------------------------------------------------------
public:
    /// Guess file format structure.
    static
    EFormat Format(const string& path, EOnError onerror = eDefault);

    /// Format prediction based on an input stream
    static
    EFormat Format(CNcbiIstream& input, EOnError onerror = eDefault);
    
    //  ------------------------------------------------------------------------
    //  "Object" interface:
    //  Use when interested only in a limited number of formats, in excluding 
    //  certain tests, a specific order in which formats are tested, ...
    //  ------------------------------------------------------------------------

    //  Construction, destruction
public:
    CFormatGuess();

    CFormatGuess(
        const string& /* file name */ );

    CFormatGuess(
        CNcbiIstream& );

    ~CFormatGuess();

    //  Interface:
public:
    
    NCBI_DEPRECATED EFormat GuessFormat(EMode);
    NCBI_DEPRECATED bool TestFormat(EFormat,EMode);

    EFormat GuessFormat(EOnError onerror = eDefault);
    bool TestFormat(EFormat,EOnError onerror = eDefault);

    // helpers:
protected:
    void Initialize();

    bool EnsureTestBuffer();
    bool EnsureStats();
    bool EnsureSplitLines();

    bool TestFormatRepeatMasker(
        EMode );
    bool TestFormatPhrapAce(
        EMode );
    bool TestFormatGtf(
        EMode );
    bool TestFormatGlimmer3(
        EMode );
    bool TestFormatAgp(
        EMode );
    bool TestFormatNewick(
        EMode );
    bool TestFormatXml(
        EMode );
    bool TestFormatAlignment(
        EMode );
    bool TestFormatBinaryAsn(
        EMode );
    bool TestFormatDistanceMatrix(
        EMode );
    bool TestFormatTaxplot(
        EMode );
    bool TestFormatFlatFileSequence(
        EMode );
    bool TestFormatFiveColFeatureTable(
        EMode );
    bool TestFormatTable(
        EMode );
    bool TestFormatFasta(
        EMode );
    bool TestFormatTextAsn(
        EMode );
    bool TestFormatSnpMarkers(
        EMode );
    bool TestFormatBed(
        EMode );
    bool TestFormatBed15(
        EMode );
    bool TestFormatWiggle(
        EMode );

    bool IsInputRepeatMaskerWithoutHeader();
    bool IsInputRepeatMaskerWithHeader();
    
    static bool IsLineFlatFileSequence(
        const std::string& );
    static bool IsLineNewick(
        const std::string& );
    static bool IsLabelNewick(
        const std::string& );
    static bool IsLineAgp(
        const std::string& );
    static bool IsLineGlimmer3(
        const std::string& );
    static bool IsLineGtf(
        const std::string& );
    static bool IsLinePhrapId(
        const std::string& );
    static bool IsLineRmo(
        const std::string& );
    static bool IsAsnComment(
        const vector<string>& );
        
private:
    static bool x_TestInput( CNcbiIstream& input, EOnError onerror );

    // data:
protected:
    static const streamsize s_iTestBufferSize = 1024;

    CNcbiIstream& m_Stream;
    bool m_bOwnsStream;
    char* m_pTestBuffer;
    streamsize m_iTestDataSize;

    bool m_bStatsAreValid;
    unsigned int m_iStatsCountData;
    unsigned int m_iStatsCountAlNumChars;
    unsigned int m_iStatsCountDnaChars;
    unsigned int m_iStatsCountAaChars;
    std::list<std::string> m_TestLines;
};

END_NCBI_SCOPE

#endif
