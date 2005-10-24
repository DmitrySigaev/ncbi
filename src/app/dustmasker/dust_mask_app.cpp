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

#include <algo/dustmask/symdust.hpp>
#include "dust_mask_app.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//-------------------------------------------------------------------------
static CRef< CSeq_entry > GetNextSequence( CNcbiIstream * input_stream )
{
    if( input_stream == 0 )
        return CRef< CSeq_entry >( 0 );

    while( !input_stream->eof() )
    {
        TReadFastaFlags flags = fReadFasta_AssumeNuc |
                                fReadFasta_ForceType |
                                fReadFasta_OneSeq    |
                                fReadFasta_AllSeqIds;

        CRef< CSeq_entry > aSeqEntry;
        CNcbiIstream::pos_type pos = input_stream->tellg();

        try {
            aSeqEntry = ReadFasta( *input_stream, flags, 0, 0 );
        }catch( ... ) {
            input_stream->seekg( pos );
            aSeqEntry = ReadFasta( 
                    *input_stream, flags | fReadFasta_NoParseID, 0, 0 );
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
    = "DUST based low complexity region masker";

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
    SetupArgDescriptions( arg_desc.release() );
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
    typedef CSymDustMasker duster_type;
    Uint4 level = GetArgs()["level"].AsInteger();
    duster_type::size_type window = GetArgs()["window"].AsInteger();
    duster_type::size_type linker = GetArgs()["linker"].AsInteger();
    duster_type duster( level, window, linker );

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

            if( output_stream != 0 )
            {
                *output_stream << ">"
                               << CSeq_id::GetStringDescr( 
                                    *bsh.GetCompleteBioseq(),
                                    CSeq_id::eFormat_FastA )
                               << " " << sequence::GetTitle( bsh ) << "\n";
                typedef duster_type::TMaskList::const_iterator it_type;

                for( it_type it = res->begin(); it != res->end(); ++it )
                    *output_stream << it->first  << " - " 
                                   << it->second << "\n";
            }

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

            cerr << "." << flush;
        }
    }

    *output_stream << flush;
    return 0;
}

END_NCBI_SCOPE


/*
 * ========================================================================
 * $Log$
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

