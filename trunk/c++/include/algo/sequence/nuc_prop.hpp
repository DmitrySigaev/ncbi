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
 * Authors:  Josh Cherry
 *
 * File Description:
 *
 */


#ifndef ALGO_SEQUENCE___NUC_PROP__HPP
#define ALGO_SEQUENCE___NUC_PROP__HPP

#include <objmgr/seq_vector.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/// this class defines functions for calculating various
/// properties of nucleotide sequences

class NCBI_XALGOSEQ_EXPORT CNucProp
{
public:
    /// Tally the occurrences of each non-ambiguous n-mer.
    static void CountNmers(CSeqVector& seqvec, int n, vector<int>& table);

    /// Encode an n-mer as an integer.
    static int Nmer2Int(const char *seq, int n);

    /// Decode an integer representation of an n-mer.
    static void Int2Nmer(int nmer_int, int nmer_size, string& out);

    /// The number of distinct n-mers (4^n)
    static int NumberOfNmers(int n);

    /// Calculate percent G+C+S
    static int GetPercentGC(const CSeqVector& seqvec);

private:
    static int Nuc2Nybble(char nuc);
    static char Nybble2Nuc(int n);
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // ALGO_SEQUENCE___NUC_PROP__HPP


/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2003/08/18 17:23:34  jcherry
 * Changed string parameter to be passed by reference.
 *
 * Revision 1.5  2003/08/04 15:43:20  dicuccio
 * Modified export specifiers to be more flexible
 *
 * Revision 1.4  2003/07/28 11:54:02  dicuccio
 * Changed calling semantics of Int2Nmer to use std::string
 *
 * Revision 1.3  2003/07/01 19:00:44  ucko
 * Drop gratuitous GUI header inclusion; minor formal cleanups
 *
 * Revision 1.2  2003/07/01 17:31:25  dicuccio
 * Added Win32 export specifiers
 *
 * Revision 1.1  2003/07/01 15:11:24  jcherry
 * Initial versions of nuc_prop and prot_prop
 *
 * ===========================================================================
 */
