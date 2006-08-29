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
 *   CDustMaskApplication class member and method definitions.
 *
 */

#include <ncbi_pch.hpp>

#include <memory>

#include <corelib/ncbidbg.hpp>
#include <util/line_reader.hpp>
#include <objtools/readers/fasta.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seq/IUPACna.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Packed_seqint.hpp>

#include <objmgr/bioseq_ci.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/readers/fasta.hpp>
#include <objtools/data_loaders/genbank/readers/id2/reader_id2.hpp>

#include "dust_mask_app.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//-------------------------------------------------------------------------
static CRef< CSeq_entry > GetNextSequence( CNcbiIstream * input_stream )
{
    if( input_stream == 0 )
        return CRef< CSeq_entry >( 0 );

    CStreamLineReader line_reader( *input_stream );

    CFastaReader::TFlags flags = 
        CFastaReader::fAssumeNuc |
        CFastaReader::fForceType |
        CFastaReader::fOneSeq    |
        CFastaReader::fAllSeqIds;

    CFastaReader fasta_reader( line_reader, flags );
    CFastaReader fasta_reader_2( 
            line_reader, flags|CFastaReader::fNoParseID );

    while( !input_stream->eof() )
    {
        CRef< CSeq_entry > aSeqEntry( null );
        CT_POS_TYPE pos = input_stream->tellg();

        try{ 
            aSeqEntry = fasta_reader.ReadSet( 1 );
        }catch( ... ) {
            input_stream->seekg( pos );
            aSeqEntry = fasta_reader_2.ReadSet( 1 );
        }

        if(    aSeqEntry != 0
            && aSeqEntry->IsSeq()
            && aSeqEntry->GetSeq().IsNa() )
            return aSeqEntry;
    }

    return CRef< CSeq_entry >( 0 );
}

//-------------------------------------------------------------------------
const char * const CDustMaskApplication::USAGE_LINE 
    = "Low complexity region masker based on Symmetric DUST algorithm";

//-------------------------------------------------------------------------
void CDustMaskApplication::Init(void)
{
    auto_ptr< CArgDescriptions > arg_desc( new CArgDescriptions );
    arg_desc->SetUsageContext( GetArguments().GetProgramBasename(),
                               USAGE_LINE );
    arg_desc->AddDefaultKey( "input", "input_file_name",
                             "input file name",
                             CArgDescriptions::eString, "" );
    arg_desc->AddDefaultKey( "output", "output_file_name",
                             "output file name",
                             CArgDescriptions::eString, "" );
    arg_desc->AddDefaultKey( "window", "window_size",
                             "DUST window length",
                             CArgDescriptions::eInteger, "64" );
    arg_desc->AddDefaultKey( "level", "level",
                             "DUST level (score threshold for subwindows)",
                             CArgDescriptions::eInteger, "20" );
    arg_desc->AddDefaultKey( "linker", "linker",
                             "DUST linker (how close masked intervals "
                             "should be to get merged together).",
                             CArgDescriptions::eInteger, "1" );
    arg_desc->AddDefaultKey( "oformat", "output_format",
                             "output format",
                             CArgDescriptions::eString, "interval" );
    arg_desc->SetConstraint( "oformat",
                             (new CArgAllow_Strings())->Allow( "interval" )
                             ->Allow( "fasta" )->Allow( "acclist" ) );
    SetupArgDescriptions( arg_desc.release() );
}

//-------------------------------------------------------------------------
void CDustMaskApplication::interval_out_handler( 
        CNcbiOstream * output_stream, 
        const CBioseq_Handle & bsh, 
        const duster_type::TMaskList & res )
{
    if( output_stream != 0 ) {
        *output_stream << ">"
                       << CSeq_id::GetStringDescr( 
                               *bsh.GetCompleteBioseq(),
                               CSeq_id::eFormat_FastA )
                       << " " << sequence::GetTitle( bsh ) << "\n";

        for( it_type it = res.begin(); it != res.end(); ++it ) {
            *output_stream << it->first  << " - " 
                           << it->second << "\n";
        }
    }
}

//-------------------------------------------------------------------------
void CDustMaskApplication::acclist_out_handler( 
        CNcbiOstream * output_stream, 
        const CBioseq_Handle & bsh, 
        const duster_type::TMaskList & res )
{
    if( output_stream != 0 ) {
        for( it_type it = res.begin(); it != res.end(); ++it ) {
            *output_stream << CSeq_id::GetStringDescr(
                    *bsh.GetCompleteBioseq(), CSeq_id::eFormat_FastA )
                           << "\t" << it->first 
                           << "\t" << it->second << "\n";
        }
    }
}

//-------------------------------------------------------------------------
static const Uint4 LINE_WIDTH = 60;

inline void CDustMaskApplication::write_normal( 
        CNcbiOstream * output_stream, const CSeqVector & data, 
        TSeqPos & start, TSeqPos & stop )
{
    for( Uint4 count = start; count < stop; ++count ) {
        *output_stream << data[count];

        if( (count + 1)%LINE_WIDTH == 0 ) {
            *output_stream << "\n";
        }
    }
}

inline void CDustMaskApplication::write_lowerc( 
        CNcbiOstream * output_stream, const CSeqVector & data,
        TSeqPos & start, TSeqPos & stop )
{
    for( Uint4 count = start; count < stop; ++count ) {
        *output_stream << (char)tolower( data[count] );

        if( (count + 1)%LINE_WIDTH == 0 ) {
            *output_stream << "\n";
        }
    }
}

//-------------------------------------------------------------------------
void CDustMaskApplication::fasta_out_handler( 
        CNcbiOstream * output_stream, 
        const CBioseq_Handle & bsh, 
        const duster_type::TMaskList & res )
{
    if( output_stream != 0 ) {
        *output_stream << ">"
                       << CSeq_id::GetStringDescr( 
                               *bsh.GetCompleteBioseq(),
                               CSeq_id::eFormat_FastA )
                       << " " << sequence::GetTitle( bsh ) << "\n";
        CSeqVector data 
            = bsh.GetSeqVector( CBioseq_Handle::eCoding_Iupac );
        typedef CSeqVector::const_iterator citer_type;
        TSeqPos start = 0, stop;

        for( it_type it = res.begin(); it != res.end(); ++it ) {
            stop = it->first;
            write_normal( output_stream, data, start, stop );
            start = stop; stop = it->second + 1;
            write_lowerc( output_stream, data, start, stop );
            start = stop;
        }

        stop = data.size();
        write_normal( output_stream, data, start, stop );

        if( stop%LINE_WIDTH != 0 ) {
            *output_stream << endl;
        }else {
            *output_stream << flush;
        }
    }
}

//-------------------------------------------------------------------------
int CDustMaskApplication::Run (void)
{
    // Set up the input and output streams.
    auto_ptr< CNcbiIstream > input_stream_ptr;
    auto_ptr< CNcbiOstream > output_stream_ptr;
    CNcbiIstream * input_stream = NULL;
    CNcbiOstream * output_stream = NULL;
    
    if( GetArgs()["input"].AsString().empty() )
        input_stream = &cin;
    else
    {
        input_stream_ptr.reset(
            new CNcbiIfstream( GetArgs()["input"].AsString().c_str() ) );
        input_stream = input_stream_ptr.get();
    }

    if( GetArgs()["output"].AsString().empty() )
        output_stream = &cout;
    else
    {
        output_stream_ptr.reset(
            new CNcbiOfstream( GetArgs()["output"].AsString().c_str() ) );
        output_stream = output_stream_ptr.get();
    }

    // Set up the object manager.
    CRef<CObjectManager> om(CObjectManager::GetInstance());
    CGBDataLoader::RegisterInObjectManager(
        *om, new CId2Reader, CObjectManager::eDefault);

    // Set up the duster object.
    Uint4 level = GetArgs()["level"].AsInteger();
    duster_type::size_type window = GetArgs()["window"].AsInteger();
    duster_type::size_type linker = GetArgs()["linker"].AsInteger();
    duster_type duster( level, window, linker );

    out_handler_type out_handler = 0;

    {
        std::string oformat = GetArgs()["oformat"].AsString();
        
        if( oformat == "interval" ) {
            out_handler = &interval_out_handler;
        }else if( oformat == "acclist" ) {
            out_handler = &acclist_out_handler;
        }else if( oformat == "fasta" ) {
            out_handler = &fasta_out_handler;
        }
    }

    // Now process each input sequence in a loop.
    CRef< CSeq_entry > aSeqEntry( 0 );

    while( (aSeqEntry = GetNextSequence( input_stream )).NotEmpty() )
    {
        CScope scope( *om );
        CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry( *aSeqEntry );

        CBioseq_CI bs_iter(seh, CSeq_inst::eMol_na);

        for ( ;  bs_iter;  ++bs_iter) 
        {
            CBioseq_Handle bsh = *bs_iter;

            if (bsh.GetBioseqLength() == 0) 
                continue;

            CSeqVector data 
                = bsh.GetSeqVector( CBioseq_Handle::eCoding_Iupac );
            std::auto_ptr< duster_type::TMaskList > res = duster( data );

            if( out_handler != 0 && res.get() != 0 ) {
                out_handler( output_stream, bsh, *res.get() );
            }

            /*
            if( output_stream != 0 )
            {
                *output_stream << ">"
                               << CSeq_id::GetStringDescr( 
                                    *bsh.GetCompleteBioseq(),
                                    CSeq_id::eFormat_FastA )
                               << " " << sequence::GetTitle( bsh ) << "\n";

                for( it_type it = res->begin(); it != res->end(); ++it )
                    *output_stream << it->first  << " - " 
                                   << it->second << "\n";
            }
            */

            /*
            CConstRef< objects::CSeq_id > id = bsh.GetSeqId();
            std::vector< CConstRef< objects::CSeq_loc > > locs;
            duster.GetMaskedLocs( 
                const_cast< objects::CSeq_id & >(*id.GetPointer()), 
                data, locs );

            if( output_stream != 0 )
            {
                *output_stream << ">"
                               << CSeq_id::GetStringDescr( 
                                    *bsh.GetCompleteBioseq(),
                                    CSeq_id::eFormat_FastA )
                               << " " << sequence::GetTitle( bsh ) << "\n";
                for( std::vector< CConstRef< objects::CSeq_loc > >::iterator 
                        it = locs.begin(); it != locs.end(); ++it )
                    *output_stream 
                      << it->GetPointer()->GetStart((objects::ESeqLocExtremes)1) 
                      << " - " 
                      << it->GetPointer()->GetStop((objects::ESeqLocExtremes)1) 
                      << "\n";
            }
            */

            /*
            CConstRef< objects::CSeq_id > id = bsh.GetSeqId();
            CRef< CPacked_seqint > masked( 
                    duster.GetMaskedInts( 
                        const_cast< objects::CSeq_id &>( *id.GetPointer() ),
                        data ) );
            typedef CPacked_seqint::Tdata Tdata;

            if( output_stream != 0 ) {
                const Tdata & mask_data = masked->Get();
                *output_stream << ">"
                               << CSeq_id::GetStringDescr( 
                                    *bsh.GetCompleteBioseq(),
                                    CSeq_id::eFormat_FastA )
                               << " " << sequence::GetTitle( bsh ) << "\n";
                for( Tdata::const_iterator i = mask_data.begin();
                        i != mask_data.end(); ++i )
                    *output_stream << (*i)->GetFrom()
                                   << " - "
                                   << (*i)->GetTo()
                                   << "\n";
            }
            */

            NcbiCerr << "." << flush;
        }
    }

    *output_stream << flush;
    NcbiCerr << endl;
    return 0;
}

END_NCBI_SCOPE


/*
 * ========================================================================
 * $Log$
 * Revision 1.11  2006/08/29 17:34:50  morgulis
 * Changed ReadFasta to CFastaReader
 *
 * Revision 1.10  2005/10/26 20:23:37  ucko
 * Use CT_POS_TYPE for portability to GCC 2.95.
 *
 * Revision 1.9  2005/10/26 18:44:23  morgulis
 * Added -oformat option to specify the output format.
 * Added support for acclist and fasta output formats.
 *
 * Revision 1.8  2005/10/24 20:54:15  morgulis
 * Fixed a problem with exception being thrown by ReadFasta if defline is not
 * properly formatted.
 *
 * Revision 1.7  2005/09/19 14:37:09  morgulis
 * Added API to return masked intervals as CRef< CPacked_seqint >.
 *
 * Revision 1.6  2005/07/12 14:16:19  morgulis
 * Changes to object manager related code to improve performance with large
 * number of short sequences.
 *
 * Revision 1.5  2005/06/06 20:33:53  morgulis
 * Small change in usage of linker parameter.
 * Added (commented out) code for testing SeqLoc based interface.
 *
 * Revision 1.4  2005/06/03 15:33:36  morgulis
 * Namespace bug fix.
 *
 * Revision 1.3  2005/06/02 18:33:05  morgulis
 * Fixed application to use non-templatized library.
 *
 * Revision 1.2  2005/06/01 19:13:54  morgulis
 * Verified to work identically to out of tree version.
 *
 * Revision 1.1  2005/05/31 14:41:32  morgulis
 * Initial checkin of the dustmasker project.
 *
 * ========================================================================
 */

