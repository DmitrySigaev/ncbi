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
 *   Implementation of CWinMaskCountsGenerator class.
 *
 */

#include <ncbi_pch.hpp>
#include <stdlib.h>

#include <vector>
#include <sstream>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seq/IUPACna.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seq_vector.hpp>

#include <algo/winmask/seq_masker_util.hpp>

#include "win_mask_fasta_reader.hpp"
#include "win_mask_gen_counts.hpp"
#include "win_mask_dup_table.hpp"
#include "win_mask_util.hpp"
#include "algo/winmask/seq_masker_ostat_factory.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


//------------------------------------------------------------------------------
static Uint4 letter( char c )
{
    switch( c )
    {
    case 'a': case 'A': return 0;
    case 'c': case 'C': return 1;
    case 'g': case 'G': return 2;
    case 't': case 'T': return 3;
    default: return 0;
    }
}

//------------------------------------------------------------------------------
static inline bool ambig( char c )
{
    return    c != 'a' && c != 'A' && c != 'c' && c != 'C'
        && c != 'g' && c != 'G' && c != 't' && c != 'T';
}

#if 0
//------------------------------------------------------------------------------
string mkdata( const CSeq_entry & entry )
{
    const CBioseq & bioseq( entry.GetSeq() );

    if(    bioseq.CanGetInst() 
           && bioseq.GetInst().CanGetLength()
           && bioseq.GetInst().CanGetSeq_data() )
    {
        TSeqPos len( bioseq.GetInst().GetLength() );
        const CSeq_data & seqdata( bioseq.GetInst().GetSeq_data() );
        auto_ptr< CSeq_data > dest( new CSeq_data );
        CSeqportUtil::Convert( seqdata, dest.get(), CSeq_data::e_Iupacna, 
                               0, len );
        return dest->GetIupacna().Get();
    }

    return string( "" );
}
#endif

//------------------------------------------------------------------------------
Uint8 CWinMaskCountsGenerator::fastalen( const string & fname ) const
{
    CNcbiIfstream input_stream( fname.c_str() );
    CWinMaskFastaReader reader( input_stream );
    CRef< CSeq_entry > entry( 0 );
    Uint8 result = 0;

    CRef<CObjectManager> om(CObjectManager::GetInstance());

    while( (entry = reader.GetNextSequence()).NotEmpty() )
    {
        CScope scope(*om);
        CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

        CBioseq_CI bs_iter(seh, CSeq_inst::eMol_na);
        for ( ;  bs_iter;  ++bs_iter) {
            CBioseq_Handle bsh = *bs_iter;

            if( CWinMaskUtil::consider( bsh, ids, exclude_ids ) )
                result += bs_iter->GetBioseqLength();
        }
    }

    return result;
}

//------------------------------------------------------------------------------
static Uint4 reverse_complement( Uint4 seq, Uint1 size )
{ return CSeqMaskerUtil::reverse_complement( seq, size ); }

//------------------------------------------------------------------------------
CWinMaskCountsGenerator::CWinMaskCountsGenerator( 
    const string & arg_input,
    const string & output,
    const string & sformat,
    const string & arg_th,
    Uint4 mem_avail,
    Uint1 arg_unit_size,
    Uint8 arg_genome_size,
    Uint4 arg_min_count,
    Uint4 arg_max_count,
    bool arg_check_duplicates,
    bool arg_use_list,
    const set< CSeq_id_Handle > & arg_ids,
    const set< CSeq_id_Handle > & arg_exclude_ids,
    bool use_ba )
:   input( arg_input ),
    ustat( CSeqMaskerOstatFactory::create( sformat, output, use_ba ) ),
    max_mem( mem_avail*1024*1024 ), unit_size( arg_unit_size ),
    genome_size( arg_genome_size ),
    min_count( arg_min_count == 0 ? 1 : arg_min_count ), 
    max_count( 500 ),
    t_high( arg_max_count ),
    has_min_count( arg_min_count != 0 ),
    no_extra_pass( arg_min_count != 0 && arg_max_count != 0 ),
    check_duplicates( arg_check_duplicates ),use_list( arg_use_list ), 
    total_ecodes( 0 ), 
    score_counts( max_count, 0 ),
    ids( arg_ids ), exclude_ids( arg_exclude_ids )
{
    // Parse arg_th to set up th[].
    string::size_type pos( 0 );
    Uint1 count( 0 );

    while( pos != string::npos && count < 4 )
    {
        string::size_type newpos = arg_th.find_first_of( ",", pos );
        th[count++] = atof( arg_th.substr( pos, newpos - pos ).c_str() );
        pos = (newpos == string::npos ) ? newpos : newpos + 1;
    }
}

//------------------------------------------------------------------------------
CWinMaskCountsGenerator::~CWinMaskCountsGenerator() {}

//------------------------------------------------------------------------------
void CWinMaskCountsGenerator::operator()()
{
    // Generate a list of files to process.
    vector< string > file_list;

    if( !use_list ) {
        file_list.push_back( input );
    } else {
        string line;
        CNcbiIfstream fl_stream( input.c_str() );

        while( getline( fl_stream, line ) ) {
            if( !line.empty() ) {
                file_list.push_back( line );
            }
        }
    }

    // Check for duplicates, if necessary.
    if( check_duplicates )
    {
        CheckDuplicates( file_list, ids, exclude_ids );
        cerr << "." << flush;
    }

    if( unit_size == 0 )
    {
        if( genome_size == 0 )
        {
            cerr << "Computing the genome length" << flush;
            Uint8 total = 0;

            for(    vector< string >::const_iterator i = file_list.begin();
                    i != file_list.end(); ++i )
            {
                total += fastalen( *i );
                cerr << "." << flush;
            }

            cerr << "done." << endl;
            // cerr << total << endl;
            genome_size = total;
        }

        for( unit_size = 15; unit_size >= 0; --unit_size ) {
            if(   (genome_size>>(2*unit_size)) >= 5 ) {
                break;
            }
        }

        ++unit_size;
        cerr << "Unit size is: " << unit_size << endl;
    }

    // Estimate the length of the prefix. 
    // Prefix length is unit_size - suffix length, where suffix length
    // is max N: (4**N) < max_mem.
    Uint1 prefix_size( 0 ), suffix_size( 0 );

    for( Uint4 suffix_exp( 1 ); suffix_size <= unit_size; 
         ++suffix_size, suffix_exp *= 4 ) {
        if( suffix_exp >= max_mem/sizeof( Uint4 ) ) {
            prefix_size = unit_size - (--suffix_size);
        }
    }

    if( prefix_size == 0 ) {
        suffix_size = unit_size;
    }

    ustat->setUnitSize( unit_size );

    // Now process for each prefix.
    Uint4 prefix_exp( 1<<(2*prefix_size) );
    Uint4 passno = 1;
    cerr << "Pass " << passno << flush;

    for( Uint4 prefix( 0 ); prefix < prefix_exp; ++prefix ) {
        process( prefix, prefix_size, file_list, no_extra_pass );
    }

    ++passno;
    cerr << endl;

    // Now put the final statistics as comments at the end of the output.
    for( Uint4 i( 1 ); i < max_count; ++i )
        score_counts[i] += score_counts[i-1];

    Uint4 offset( total_ecodes - score_counts[max_count - 1] );
    Uint4 index[4] = {0, 0, 0, 0};
    double previous( 0.0 );
    double current;

    if( no_extra_pass )
    {
        ustat->setBlank();
        ostringstream s;
        s << " " << total_ecodes << " ecodes";
        ustat->setComment( s.str() );
    }

    for( Uint4 i( 1 ); i <= max_count; ++i )
    {
        current = 100.0*(((double)(score_counts[i - 1] + offset))
                  /((double)total_ecodes));

        if( no_extra_pass )
        {
            ostringstream s;
            s << " " << dec << i << "\t" << score_counts[i - 1] + offset << "\t"
              << current;
            ustat->setComment( s.str() );
        }

        for( Uint1 j( 0 ); j < 4; ++j )
            if( previous < th[j] && current >= th[j] )
                index[j] = i;

        previous = current;
    }

    // If min_count or t_high must be deduced do it and reprocess.
    if( !no_extra_pass )
    {
        total_ecodes = 0;

        if( !has_min_count )
            min_count = index[0];

        if( t_high == 0 )
            t_high = index[3];

        if( min_count == 0 )
          min_count = 1;

        for( Uint4 i( 0 ); i < max_count; ++i )
            score_counts[i] = 0;

        cerr << "Pass " << passno << flush;

        for( Uint4 prefix( 0 ); prefix < prefix_exp; ++prefix )
            process( prefix, prefix_size, file_list, true );

        cerr << endl;

        for( Uint4 i( 1 ); i < max_count; ++i )
            score_counts[i] += score_counts[i-1];

        offset = total_ecodes - score_counts[max_count - 1];

        {
            ustat->setBlank();
            ostringstream s;
            s << " " << total_ecodes << " ecodes";
            ustat->setComment( s.str() );
        }

        for( Uint4 i( 1 ); i <= max_count; ++i )
        {
            current 
                = 100.0*(((double)(score_counts[i - 1] + offset))
                  /((double)total_ecodes));
            ostringstream s;
            s << " " << dec << i << "\t" << score_counts[i - 1] + offset << "\t"
              << current;
            ustat->setComment( s.str() );
        }
    }

    ustat->setComment( "" );

    for( Uint1 i( 0 ); i < 4; ++i )
    {
        ostringstream s;
        s << " " << th[i] << "%% threshold at " << index[i];
        ustat->setComment( s.str() );
    }

    ustat->setBlank();
    ustat->setParam( "t_low      ", index[0] );
    ustat->setParam( "t_extend   ", index[1] );
    ustat->setParam( "t_threshold", index[2] );
    ustat->setParam( "t_high     ", index[3] );
    ustat->setBlank();
    ustat->finalize();
    cerr << endl;
}

//------------------------------------------------------------------------------
void CWinMaskCountsGenerator::process( Uint4 prefix, 
                                       Uint1 prefix_size, 
                                       const vector< string > & input_list,
                                       bool do_output )
{
    Uint1 suffix_size( unit_size - prefix_size );
    Uint4 vector_size( 1<<(2*suffix_size) );
    vector< Uint4 > counts( vector_size, 0 );
    Uint4 unit_mask( (1<<(2*unit_size)) - 1 );
    Uint4 prefix_mask( ((1<<(2*prefix_size)) - 1)<<(2*suffix_size) );
    Uint4 suffix_mask( (1<<2*suffix_size) - 1 );
    prefix <<= (2*suffix_size);
    CRef<CObjectManager> om(CObjectManager::GetInstance());

    for( vector< string >::const_iterator it( input_list.begin() );
         it != input_list.end(); ++it )
    {
        CNcbiIfstream input_stream( it->c_str() );
        CWinMaskFastaReader reader( input_stream );
        CRef< CSeq_entry > entry( 0 );

        while( (entry = reader.GetNextSequence()).NotEmpty() )
        {
            CScope scope(*om);
            CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry(*entry);

            CBioseq_CI bs_iter(seh, CSeq_inst::eMol_na);
            for ( ;  bs_iter;  ++bs_iter) {
                CBioseq_Handle bsh = *bs_iter;

                if( CWinMaskUtil::consider( bsh, ids, exclude_ids ) )
                {
                    CSeqVector data =
                        bs_iter->GetSeqVector(CBioseq_Handle::eCoding_Iupac);

                    if( data.empty() )
                        continue;

                    TSeqPos length( data.size() );
                    Uint4 count( 0 );
                    Uint4 unit( 0 );

                    for( Uint4 i( 0 ); i < length; ++i ) {
                        if( ambig( data[i] ) )
                        {
                            count = 0;
                            unit = 0;
                            continue;
                        }
                        else
                        {
                            unit = ((unit<<2)&unit_mask) + letter( data[i] );

                            if( count >= unit_size - 1 )
                            {
                                Uint4 runit( reverse_complement( unit, unit_size ) );
    
                                if( unit <= runit && (unit&prefix_mask) == prefix )
                                    ++counts[unit&suffix_mask];

                                if( runit <= unit && (runit&prefix_mask) == prefix )
                                    ++counts[runit&suffix_mask];
                            }

                            ++count;
                        }
                    }
                }
            }
        }

        cerr << "." << flush;
    }

    for( Uint4 i( 0 ); i < vector_size; ++i )
    {
        Uint4 ri = 0; 

        if( counts[i] > 0 )
        {
            ri = reverse_complement( i, unit_size );

            if( i == ri )
                ++total_ecodes; 
            else total_ecodes += 2;
        }

        if( counts[i] >= min_count )
        {
            if( counts[i] >= max_count )
                if( i == ri )
                    ++score_counts[max_count - 1];
                else score_counts[max_count - 1] += 2;
            else if( i == ri )
                ++score_counts[counts[i] - 1];
            else score_counts[counts[i] - 1] += 2;

            if( do_output )
                ustat->setUnitCount( prefix + i,
                                     (counts[i] > t_high) ? t_high
                                                          : counts[i] );
        }
    }
}


END_NCBI_SCOPE

/*
 * ========================================================================
 * $Log$
 * Revision 1.15  2005/08/30 14:35:20  morgulis
 * NMer counts optimization using bit arrays. Performance is improved
 * by about 20%.
 *
 * Revision 1.14  2005/07/11 14:36:17  morgulis
 * Fixes for performance problems with large number of short sequences.
 * Windowmasker is now statically linked against object manager libs.
 *
 * Revision 1.13  2005/05/02 17:58:01  morgulis
 * Fixed a few warnings for solaris.
 *
 * Revision 1.12  2005/05/02 14:27:46  morgulis
 * Implemented hash table based unit counts formats.
 *
 * Revision 1.11  2005/04/18 20:11:36  morgulis
 * Stage 1 can now take -t_high parameter.
 * Unit counts generated do not contain counts above T_high.
 *
 * Revision 1.10  2005/03/28 22:41:06  morgulis
 * Moved win_mask_ustat* files to library and renamed them.
 *
 * Revision 1.9  2005/03/28 21:33:26  morgulis
 * Added -sformat option to specify the output format for unit counts file.
 * Implemented framework allowing usage of different output formats for
 * unit counts. Rewrote the code generating the unit counts file using
 * that framework.
 *
 * Revision 1.8  2005/03/24 16:50:21  morgulis
 * -ids and -exclude-ids options can be applied in Stage 1 and Stage 2.
 *
 * Revision 1.7  2005/03/21 21:02:44  morgulis
 * Fixed one possible bug that might occur there are no valid units in the
 * whole input.
 * Fixed merging conflicts with changes by Michael DiCuccio.
 *
 * Revision 1.6  2005/03/21 13:19:26  dicuccio
 * Updated API: use object manager functions to supply data, instead of passing
 * data as strings.
 *
 * Revision 1.5  2005/03/17 20:35:06  morgulis
 * *** empty log message ***
 *
 * Revision 1.4  2005/03/17 20:30:21  morgulis
 * *** empty log message ***
 *
 * Revision 1.3  2005/03/17 20:21:22  morgulis
 * Only store half of the units in unit counts file.
 *
 * Revision 1.2  2005/03/08 17:02:30  morgulis
 * Changed unit counts file to include precomputed threshold values.
 * Changed masking code to pick up threshold values from the units counts file.
 * Unit size is computed automatically from the genome length.
 * Added extra option for specifying genome length.
 * Removed all experimental command line options.
 * Fixed id strings in duplicate sequence checking code.
 *
 * Revision 1.1  2005/02/25 21:32:54  dicuccio
 * Rearranged winmasker files:
 * - move demo/winmasker to a separate app directory (src/app/winmasker)
 * - move win_mask_* to app directory
 *
 * Revision 1.4  2005/02/25 21:09:18  morgulis
 * 1. Reduced the number of binary searches by the factor of 2 by locally
 *    caching some search results.
 * 2. Automatically compute -lowscore value if it is not specified on the
 *    command line during the counts generation pass.
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
