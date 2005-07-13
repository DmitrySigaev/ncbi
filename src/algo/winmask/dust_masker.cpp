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
 * Author:  Aleksandr Morgulis
 *
 * File Description:
 *   CDustMasker class implementation.
 *
 */

#include <ncbi_pch.hpp>
#include <vector>
#include <algorithm>

#include <algo/blast/core/blast_dust.h>

#include <algo/winmask/dust_masker.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//------------------------------------------------------------------------------
static inline char iupacna_to_blastna( char c )
{
    switch( c )
    {
    case 'a': case 'A': return 0; 
    case 'c': case 'C': return 1;
    case 'g': case 'G': return 2;
    case 't': case 'T': return 3;
    case 'r': case 'R': return 4;
    case 'y': case 'Y': return 5;
    case 'm': case 'M': return 6;
    case 'k': case 'K': return 7;
    case 'w': case 'W': return 8;
    case 's': case 'S': return 9;
    case 'b': case 'B': return 10;
    case 'd': case 'D': return 11;
    case 'h': case 'H': return 12;
    case 'v': case 'V': return 13;
    case 'n': case 'N': 
    default:            return 14;
    }
}

//------------------------------------------------------------------------------
CDustMasker::CDustMasker( Uint4 arg_window, Uint4 arg_level, Uint4 arg_linker )
: window( arg_window ), level( arg_level ), linker( arg_linker )
{}

//------------------------------------------------------------------------------
CDustMasker::~CDustMasker(){}

//------------------------------------------------------------------------------
CDustMasker::TMaskList * CDustMasker::operator()( 
    const CSeqVector & data, const TMaskList & exclude_ranges )
{
    // Transform to BLASTNA.
    vector< Uint1 > data_blastna;
    data_blastna.reserve( data.size() + 1 );
    TMaskList * result( new TMaskList );
    TMaskList::const_iterator e_it = exclude_ranges.begin();
    TMaskList::const_iterator e_end = exclude_ranges.end();
    CSeqVector::const_iterator start_it = data.begin();
    CSeqVector::const_iterator current_it = data.begin();
    CSeqVector::const_iterator end_it = data.end();

    if( e_it != e_end && e_it->first == 0 && e_it->second + 1 > window )
    {
        current_it = start_it + e_it->second - window + 2;
        ++e_it;
    }

    do
    {
        Uint4 offset = current_it - start_it;
        data_blastna.clear();

        while( e_it != e_end && e_it->second - e_it->first + 1 <= window )
            ++e_it;

        end_it = (e_it == e_end) ? data.end() 
                                   : start_it + e_it->first + window;

        for( ; current_it != end_it; ++current_it )
            data_blastna.push_back( iupacna_to_blastna( *current_it ) );

        data_blastna.push_back( 0 );

        // Now dust.
        BlastSeqLoc * blast_loc( 0 );
        SeqBufferDust( &data_blastna[0],
                       data_blastna.size(), 0, 
                       level, window, linker, &blast_loc );

        // Convert to the output type.
        while( blast_loc )
        {
            result->push_back(TMaskedInterval( 
                blast_loc->ssr->left + offset, 
                blast_loc->ssr->right + offset ) );
            blast_loc = blast_loc->next;
        }

        if( e_it != e_end )
        {
            current_it = start_it + e_it->second - window + 2;
            ++e_it;
        }

    } while( end_it != data.end() );

    // Results are not necessarily sorted, so sort and remove duplicates.
    sort( result->begin(), result->end() );
    result->erase( unique( result->begin(), result->end() ), 
                   result->end() );

    return result;
}


END_NCBI_SCOPE

/*
 * ========================================================================
 * $Log$
 * Revision 1.8  2005/07/13 15:59:56  morgulis
 * Dust only the parts of the sequences not masked by winmask module.
 *
 * Revision 1.7  2005/06/22 14:49:53  vasilche
 * CSeqVector_CI now supports its usage in std::transform().
 *
 * Revision 1.6  2005/03/22 01:56:04  ucko
 * Don't use transform<> when converting to blastna, as some
 * implementations require postincrement, which CSeqVector_CI lacks.
 *
 * Revision 1.5  2005/03/21 13:19:26  dicuccio
 * Updated API: use object manager functions to supply data, instead of passing
 * data as strings.
 *
 * Revision 1.4  2005/03/01 16:07:42  ucko
 * Fix 64-bit builds by using TMaskedInterval rather than a hard-coded
 * type that may not be correct.
 *
 * Revision 1.3  2005/02/12 20:24:39  dicuccio
 * Dropped use of std:: (not needed)
 *
 * Revision 1.2  2005/02/12 19:58:04  dicuccio
 * Corrected file type issues introduced by CVS (trailing return).  Updated
 * typedef names to match C++ coding standard.
 *
 * Revision 1.1  2005/02/12 19:15:11  dicuccio
 * Initial version - ported from Aleksandr Morgulis's tree in internal/winmask
 *
 * ========================================================================
 */

