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
 * File Description:  Implemented methods to identify file formats.
 *
 */

#include <util/format_guess.hpp>
#include <corelib/ncbifile.hpp>

BEGIN_NCBI_SCOPE


static bool isDNA_Alphabet(char ch)
{
    return ::strchr("ATGC", ch) != 0;
}

// Check if letter belongs to amino acid alphabet
static bool isProtein_Alphabet(char ch)
{
    return ::strchr("ACDEFGHIKLMNPQRSTVWYBZ", ch) != 0;
}

// Check if character belongs to the CR/LF group of symbols
static inline bool isLineEnd(char ch)
{
    return ch == 0x0D || ch == 0x0A || ch == '\n';
}

CFormatGuess::ESequenceType 
CFormatGuess::SequenceType(const char* str, unsigned length)
{
    if (length == 0)
        length = ::strlen(str);

    unsigned ATGC_content = 0;
    unsigned amino_acid_content = 0;

    for (unsigned i = 0; i < length; ++i) {
        unsigned char ch = str[i];
        char upch = toupper(ch);

        if (isDNA_Alphabet(upch)) {
            ++ATGC_content;
        }
        if (isProtein_Alphabet(upch)) {
            ++amino_acid_content;
        }
    }

    double dna_content = (double)ATGC_content / (double)length;
    double prot_content = (double)amino_acid_content / (double)length;

    if (dna_content > 0.7) {
        return eNucleotide;
    }
    if (prot_content > 0.7) {
        return eProtein;
    }
    return eUndefined;
}


CFormatGuess::EFormat CFormatGuess::Format(const string& path)
{
    EFormat format = eUnknown;

    CNcbiIfstream input(path.c_str(), IOS_BASE::in | IOS_BASE::binary);

    if (!input.is_open()) 
        return eUnknown;

    unsigned char buf[1024];

    input.read((char*)buf, sizeof(buf));
    size_t count = input.gcount();

    if (!count) 
        return eUnknown;

    // Buffer analysis (completely ad-hoc heuristics).

    // Check for XML signature...
    {{
        if (count > 5) {
            const char* xml_sig = "<?XML";
            bool xml_flag = true;
            for (unsigned i = 0; i < 5; ++i) {
                unsigned char ch = buf[i];
                char upch = toupper(ch);
                if (upch != xml_sig[i]) {
                    xml_flag = false;
                    break;
                }
            }
            if (xml_flag) {
                return eXml;
            }
        }

    }}

    
    unsigned ATGC_content = 0;
    unsigned amino_acid_content = 0;
    unsigned seq_length = count;

    unsigned alpha_content = 0;

    unsigned int i = 0;
    if (buf[0] == '>') { // FASTA ?
        for (; (!isLineEnd(buf[i])) && i < count; ++i) {
            // skip the first line (presumed this is free-text information)
            unsigned char ch = buf[i];
            if (isalnum(ch) || isspace(ch)) {
                ++alpha_content;
            }
        }
        seq_length = count - i;
        if (seq_length == 0) {
            return eUnknown;   // No way to tell what format is this...
        }
    }

    for (; i < count; ++i) {
        unsigned char ch = buf[i];
        char upch = toupper(ch);

        if (isalnum(ch) || isspace(ch)) {
            ++alpha_content;
        }
        if (isDNA_Alphabet(upch)) {
            ++ATGC_content;
        }
        if (isProtein_Alphabet(upch)) {
            ++amino_acid_content;
        }
        if (isLineEnd(ch)) {
            ++alpha_content;
            --seq_length;
        }
    }

    double dna_content = (double)ATGC_content / (double)seq_length;
    double prot_content = (double)amino_acid_content / (double)seq_length;
    double a_content = (double)alpha_content / (double)count;
    if (buf[0] == '>') {
        if (dna_content > 0.7 && a_content > 0.91) {
            return eFasta;  // DNA fasta file
        }
        if (prot_content > 0.7 && a_content > 0.91) {
            return eFasta;  // Protein fasta file
        }
    }

    if (a_content > 0.80) {  // Text ASN ?
        // extract first line
        char line[1024] = {0,};
        char* ptr = line;
        for (i = 0; i < count; ++i) {
            if (isLineEnd(buf[i])) {
                break;
            }
            *ptr = buf[i];
            ++ptr;
        }
        // roll it back to last non-space character...
        while (ptr > line) {
            --ptr;
            if (!isspace(*ptr)) break;
        }
        if (*ptr == '{') {  // "{" simbol says it's most likely ASN text
            return eTextASN;
        }
    }

    if (a_content < 0.80) {
        return eBinaryASN;
    }

    input.close();
    return format;

}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.7  2003/07/10 19:58:25  ivanov
 * Get rid of compilation warning: removed double variable declaration
 *
 * Revision 1.6  2003/07/08 20:30:50  kuznets
 * Fixed bug with different "\n" coding in DOS-Windows and Unix.
 *
 * Revision 1.5  2003/07/07 19:54:06  kuznets
 * Improved format recognition of short fasta files
 *
 * Revision 1.4  2003/06/20 20:58:04  kuznets
 * Cleaned up amino-acid alphabet recognition.
 *
 * Revision 1.3  2003/05/13 15:18:02  kuznets
 * added sequence type guessing function
 *
 * Revision 1.2  2003/05/09 14:08:28  ucko
 * ios_base:: -> IOS_BASE:: for gcc 2.9x compatibility
 *
 * Revision 1.1  2003/05/08 19:46:34  kuznets
 * Initial revision
 *
 * ===========================================================================
 */
