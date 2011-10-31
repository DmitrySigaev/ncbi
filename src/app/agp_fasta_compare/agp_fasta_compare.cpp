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
 * Authors:  Mike DiCuccio, Michael Kornbluh
 *
 * File Description:
 *     Makes sure an AGP file builds the same sequence found in a FASTA
 *     file.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <util/checksum.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/scope.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/lds2/lds2_dataloader.hpp>
#include <objtools/lds2/lds2.hpp>
#include <objtools/readers/agp_util.hpp>
#include <objtools/readers/fasta.hpp>
#include <objtools/readers/reader_exception.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

namespace {
    // command-line argument names
    const string kArgnameAgp = "agp";
    const string kArgnameObjFile = "objfile";
    const string kArgnameLoadLog = "loadlog";
}

/////////////////////////////////////////////////////////////////////////////
//  CAgpToSeqEntry::

// This class is used to turn an AGP file into a Seq-entry
class CAgpToSeqEntry : public CAgpReader {
public:

    // When reading, loads results into "entries"
    CAgpToSeqEntry( vector< CRef<CSeq_entry> > & entries ) : 
      CAgpReader( eAgpVersion_2_0 ),
          m_entries(entries)
      { }

protected:
    virtual void OnGapOrComponent() {
        if( ! m_bioseq || 
            m_prev_row->GetObject() != m_this_row->GetObject() ) 
        {
            x_FinishedBioseq();

            // initialize new bioseq
            CRef<CSeq_inst> seq_inst( new CSeq_inst );
            seq_inst->SetRepr(CSeq_inst::eRepr_delta);
            seq_inst->SetMol(CSeq_inst::eMol_dna);
            seq_inst->SetLength(0);

            m_bioseq.Reset( new CBioseq );
            m_bioseq->SetInst(*seq_inst);

            CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Local,
                m_this_row->GetObject(), m_this_row->GetObject() ));
            m_bioseq->SetId().push_back(id);
        }

        CRef<CSeq_inst> seq_inst( & m_bioseq->SetInst() );

        CRef<CDelta_seq> delta_seq( new CDelta_seq );
        seq_inst->SetExt().SetDelta().Set().push_back(delta_seq);

        if( m_this_row->is_gap ) {
            delta_seq->SetLiteral().SetLength(m_this_row->gap_length);
            if( m_this_row->component_type == 'U' ) {
                delta_seq->SetLiteral().SetFuzz().SetLim();
            }
            seq_inst->SetLength() += m_this_row->gap_length;
        } else {
            CSeq_loc& loc = delta_seq->SetLoc();

            CRef<CSeq_id> comp_id;
            try { 
                comp_id.Reset( new CSeq_id( m_this_row->GetComponentId() ) );
            } catch(...) {
                // couldn't create real seq-id.  fall back on local seq-id
                comp_id.Reset( new CSeq_id );
                comp_id->SetLocal().SetStr( m_this_row->GetComponentId() );
            }
            loc.SetInt().SetId(*comp_id);

            loc.SetInt().SetFrom( m_this_row->component_beg - 1 );
            loc.SetInt().SetTo(   m_this_row->component_end - 1 );
            seq_inst->SetLength() += ( m_this_row->component_end - m_this_row->component_beg + 1 );
            
            switch( m_this_row->orientation ) {
            case CAgpRow::eOrientationPlus:
                loc.SetInt().SetStrand( eNa_strand_plus );
                break;
            case CAgpRow::eOrientationMinus:
                loc.SetInt().SetStrand( eNa_strand_minus );
                break;
            case CAgpRow::eOrientationUnknown:
                loc.SetInt().SetStrand( eNa_strand_unknown );
                break;
            case CAgpRow::eOrientationIrrelevant:
                loc.SetInt().SetStrand( eNa_strand_other );
                break;
            default:
                throw runtime_error("unknown orientation " + NStr::IntToString(m_this_row->orientation));
            }
        }
    }

    virtual int Finalize(void)
    {
        // First, do real finalize
        const int return_val = CAgpReader::Finalize();
        // Then, our own finalization
        x_FinishedBioseq();

        return return_val;
    }

    void x_FinishedBioseq(void)
    {
        if( m_bioseq ) {
            CRef<CSeq_entry> entry( new CSeq_entry );
            entry->SetSeq(*m_bioseq);
            m_entries.push_back( entry );

            m_bioseq.Reset();
        }
    }

    CRef<CBioseq> m_bioseq;
    vector< CRef<CSeq_entry> > & m_entries;
};


/////////////////////////////////////////////////////////////////////////////
//  CAgpFastaCompareApplication::


class CAgpFastaCompareApplication : public CNcbiApplication
{
public:
    CAgpFastaCompareApplication(void);

private:
    // after constructor, only set to false
    bool m_bSuccess; 

    // where we write detailed loading logs
    // This is unset if we're not writing anywhere
    auto_ptr<CNcbiOfstream> m_pLoadLogFile;

    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);

    // TKey pair is: MD5 checksum and sequence length
    typedef pair<string, TSeqPos> TKey;
    typedef map<TKey, CSeq_id_Handle> TUniqueSeqs;

    typedef set<CSeq_id_Handle> TSeqIdSet;

    void x_ProcessObject( const string & filename,
                          TUniqueSeqs& seqs );
    void x_ProcessAgp(CNcbiIstream& istr,
                      TUniqueSeqs& seqs);

    void x_Process(const CSeq_entry_Handle seh,
                   TUniqueSeqs& seqs,
                   int * in_out_pUniqueBioseqsLoaded,
                   int * in_out_pBioseqsSkipped );

    void x_OutputDifferences(
        const TSeqIdSet & vSeqIdFASTAOnly,
        const TSeqIdSet & vSeqIdAGPOnly );

    void x_SetBinaryVsText( CNcbiIstream & file_istrm, 
        CFormatGuess::EFormat guess_format );
};

CAgpFastaCompareApplication::CAgpFastaCompareApplication(void)
    : m_bSuccess(true)
{
}

/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments

void CAgpFastaCompareApplication::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "CArgDescriptions demo program");

    arg_desc->AddKey(kArgnameAgp, "Agp",
                     "AGP data to process",
                     CArgDescriptions::eInputFile);

    arg_desc->AddKey(kArgnameObjFile, "ObjFile",
                     "Fasta or ASN.1 sequences to process, which contains the "
                     "objects we're comparing against the agp file.",
                     CArgDescriptions::eInputFile );

    arg_desc->AddOptionalKey( kArgnameLoadLog, "LoadLog",
        "This file will receive the detailed list of all loaded sequences",
        CArgDescriptions::eOutputFile );

    arg_desc->AddExtra( 0, kMax_UInt, 
        "Optional Fasta sequences to process, which contains the "
        "components.  This is useful if the components are not yet in genbank",
        CArgDescriptions::eInputFile );

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



/////////////////////////////////////////////////////////////////////////////
//  Run test (printout arguments obtained from command-line)


int CAgpFastaCompareApplication::Run(void)
{
    // Get arguments
    const CArgs& args = GetArgs();

    if( args[kArgnameLoadLog] ) {
        m_pLoadLogFile.reset( 
            new CNcbiOfstream(args[kArgnameLoadLog].AsString().c_str() ) );
    }

    CRef<CObjectManager> om(CObjectManager::GetInstance());

    // load local component FASTA sequences and Genbank into 
    // local scope for lookups using local data storage
    auto_ptr<CTmpFile> ldsdb_file;
    CRef<CLDS2_Manager> lds_mgr;
    if( args.GetNExtra() > 0 ) {
        ldsdb_file.reset( new CTmpFile ); // file deleted on object destruction
        LOG_POST(Error << "Loading temporary component FASTA database at " 
            << ldsdb_file->GetFileName() );
        lds_mgr.Reset(new CLDS2_Manager( ldsdb_file->GetFileName() ));

        // Note that argument numbering is 1-based, not 0-based
        for( unsigned int arg_idx = 1; arg_idx <= args.GetNExtra(); ++arg_idx ) {
            lds_mgr->AddDataFile( args[arg_idx].AsString() );
        }

        lds_mgr->UpdateData();
        CLDS2_DataLoader::RegisterInObjectManager(*om, ldsdb_file->GetFileName(), -1,
            CObjectManager::eDefault, 1 );
    }
    CGBDataLoader::RegisterInObjectManager(*om, 0, 
        CObjectManager::eDefault, 2 );

    // calculate checksum of the AGP sequences and the FASTA sequences
    CNcbiIstream& istr_agp = args[kArgnameAgp].AsInputFile();

    // Guess the format of the file
    TUniqueSeqs fasta_ids;
    x_ProcessObject( args[kArgnameObjFile].AsString(), fasta_ids );

    TUniqueSeqs agp_ids;
    x_ProcessAgp(istr_agp, agp_ids);

    // will hold ones that are only in FASTA or only in AGP.
    // Of course, if one appears in both, we should print it in a more
    // user-friendly way
    TSeqIdSet vSeqIdFASTAOnly;
    TSeqIdSet vSeqIdAGPOnly;

    TUniqueSeqs::const_iterator iter1 = fasta_ids.begin();
    TUniqueSeqs::const_iterator iter1_end = fasta_ids.end();

    TUniqueSeqs::const_iterator iter2 = agp_ids.begin();
    TUniqueSeqs::const_iterator iter2_end = agp_ids.end();

    // make sure set of sequences in obj FASTA match AGP's objects.
    // Print discrepancies.
    LOG_POST(Error << "Reporting differences...");
    for ( ;  iter1 != iter1_end  &&  iter2 != iter2_end;  ) {
        if (iter1->first < iter2->first) {
            vSeqIdFASTAOnly.insert( iter1->second );
            ++iter1;
        }
        else if (iter2->first < iter1->first) {
            vSeqIdAGPOnly.insert( iter2->second );
            ++iter2;
        }
        else {
            ++iter1;
            ++iter2;
        }
    }

    for ( ;  iter1 != iter1_end;  ++iter1) {
        vSeqIdFASTAOnly.insert( iter1->second );
    }

    for ( ;  iter2 != iter2_end;  ++iter2) {
        vSeqIdAGPOnly.insert( iter2->second );
    }

    // look at vSeqIdFASTAOnly and vSeqIdAGPOnly and 
    // print in user-friendly way
    x_OutputDifferences( vSeqIdFASTAOnly, vSeqIdAGPOnly );

    const bool bThereWereDifferences = ( 
        ! vSeqIdFASTAOnly.empty() ||
        ! vSeqIdAGPOnly.empty() );
    if( ! bThereWereDifferences ) {
        LOG_POST(Error << "Success: No differences found");
    }
    if( bThereWereDifferences ) {
        m_bSuccess = false;
    }

    return ( m_bSuccess ? 0 : 1 );
}


void CAgpFastaCompareApplication::x_Process(const CSeq_entry_Handle seh,
                                            TUniqueSeqs& seqs,
                                            int * in_out_pUniqueBioseqsLoaded,
                                            int * in_out_pBioseqsSkipped )
{
    _ASSERT( 
        in_out_pUniqueBioseqsLoaded != NULL && 
        in_out_pBioseqsSkipped != NULL );

    // skipped is total minus loaded.
    int total = 0;

    for (CBioseq_CI bioseq_it(seh, CSeq_inst::eMol_na);  bioseq_it;  ++bioseq_it) {
        ++total;
        CSeqVector vec(*bioseq_it, CBioseq_Handle::eCoding_Iupac);
        CSeq_id_Handle idh = sequence::GetId(*bioseq_it,
                                             sequence::eGetId_Best);
        string data;
        if( ! vec.CanGetRange(0, bioseq_it->GetBioseqLength() - 1) ) {
            LOG_POST(Error << "  Skipping one: could not load due to error in AGP file "
                "(length issue or doesn't exist) for " << idh 
                << " (though issue could be due to failure to resolve one of the contigs.  "
                "Are all necessary components in GenBank or in files specified on the command-line?)");
            m_bSuccess = false;
            continue;
        }
        try {
            vec.GetSeqData(0, bioseq_it->GetBioseqLength() - 1, data);
        } catch(CSeqVectorException ex) {
            LOG_POST(Error << "  Skipping one: could not load due to error, probably in AGP file, possibly a length issue, for " << idh);
            m_bSuccess = false;
            continue;
        }

        CChecksum cks(CChecksum::eMD5);
        cks.AddLine(data);

        string md5;
        cks.GetMD5Digest(md5);

        TKey key(md5, bioseq_it->GetBioseqLength());
        pair<TUniqueSeqs::iterator, bool> insert_result =
            seqs.insert(TUniqueSeqs::value_type(key, idh));
        if( ! insert_result.second ) {
            LOG_POST(Error << "  Error: duplicate sequence " << idh );
            m_bSuccess = false;
            continue;
        }

        if( m_pLoadLogFile.get() != NULL ) {
            CNcbiOstrstream os;
            ITERATE (string, i, key.first) {
                os << setw(2) << setfill('0') << hex << (int)((unsigned char)*i);
            }

            *m_pLoadLogFile << "  " << idh << ": "
                << string(CNcbiOstrstreamToString(os))
                << " / " << key.second << endl;
        }

        ++*in_out_pUniqueBioseqsLoaded;
    }

    *in_out_pBioseqsSkipped = ( total -  *in_out_pUniqueBioseqsLoaded);
}


void CAgpFastaCompareApplication::x_ProcessObject( const string & filename,
                                                   TUniqueSeqs& fasta_ids )
{
    int iNumLoaded = 0;
    int iNumSkipped = 0;

    LOG_POST(Error << "Processing object file...");
    if( m_pLoadLogFile.get() != NULL ) {
        *m_pLoadLogFile << "Processing object file..." << endl;
    }
    try {
        CFormatGuess guesser( filename );
        const CFormatGuess::EFormat format = 
            guesser.GuessFormat();

        if( format == CFormatGuess::eFasta ) {
            CNcbiIfstream file_istrm( filename.c_str(), ios::binary );
            CFastaReader reader(file_istrm);
            while (file_istrm) {
                CRef<CSeq_entry> entry = reader.ReadOneSeq();

                CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
                CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
                x_Process(seh, fasta_ids, &iNumLoaded, &iNumSkipped );
            }
        } else if( format == CFormatGuess::eBinaryASN || 
                   format == CFormatGuess::eTextASN )
        {
            // see if it's a submit
            CRef<CSeq_submit> submit( new CSeq_submit );
            {
                CNcbiIfstream file_istrm( filename.c_str(), ios::binary );
                x_SetBinaryVsText( file_istrm, format );
                try {
                    file_istrm >> *submit;
                } catch(...) {
                    // didn't work
                }
            }

            if( submit ) {

                if( ! submit->IsEntrys() ) {
                    LOG_POST(Error << "Seq-submits must have 'entrys'.");
                    return;
                }

                ITERATE( CSeq_submit::C_Data::TEntrys, entry_iter, 
                    submit->GetData().GetEntrys() ) 
                {
                    const CSeq_entry &entry = **entry_iter;

                    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
                    CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(entry);
                    x_Process(seh, fasta_ids, &iNumLoaded, &iNumSkipped );
                }
            } 
            else
            {
                CRef<CSeq_entry> entry( new CSeq_entry );

                CNcbiIfstream file_istrm( filename.c_str(), ios::binary );
                x_SetBinaryVsText( file_istrm, format );
                file_istrm >> *entry;

                CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
                CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
                x_Process(seh, fasta_ids, &iNumLoaded, &iNumSkipped );
            }
        } else {
            LOG_POST(Error << "Could not determine format of " << filename 
                << ", best guess is: " << CFormatGuess::GetFormatName(format) );
            return;
        }
    }
    catch(CObjReaderParseException & ex ) {
        if( ex.GetErrCode() == CObjReaderParseException::eEOF ) {
            // end of file; no problem
        } else {
            LOG_POST(Error << "Error reading object file: " << ex.what() );
        }
    }
    catch (CException& ex ) {
        LOG_POST(Error << "Error reading object file: " << ex.what() );
    }

    LOG_POST(Error << "Loaded " << iNumLoaded << " object file sequence(s).");
    if( iNumSkipped > 0 ) {
        LOG_POST(Error << "  Skipped " << iNumSkipped << " FASTA sequence(s).");
    }
}


void CAgpFastaCompareApplication::x_ProcessAgp(CNcbiIstream& istr,
                                               TUniqueSeqs& agp_ids)
{
    int iNumLoaded = 0;
    int iNumSkipped = 0;

    LOG_POST(Error << "Processing AGP...");
    if( m_pLoadLogFile.get() != NULL ) {
        *m_pLoadLogFile << "Processing AGP..." << endl;
    }
    while (istr) {
        vector< CRef<CSeq_entry> > entries;
        CAgpToSeqEntry agp_reader( entries );
        int err_code = agp_reader.ReadStream( istr ); // loads var entries
        if( err_code != 0 ) {
            LOG_POST(Error << "Error occurred reading AGP file: " << err_code );
            m_bSuccess = false;
            return;
        }
        ITERATE (vector< CRef<CSeq_entry> >, it, entries) {
            CRef<CSeq_entry> entry = *it;

            CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
            CSeq_entry_Handle seh = scope->AddTopLevelSeqEntry(*entry);
            scope->AddDefaults();

            x_Process(seh, agp_ids, &iNumLoaded, &iNumSkipped );
        }
    }

    LOG_POST(Error << "Loaded " << iNumLoaded << " AGP sequence(s).");
    if( iNumSkipped > 0 ) {
        LOG_POST(Error << "  Skipped " << iNumSkipped << " AGP sequence(s).");
    }
}

void CAgpFastaCompareApplication::x_OutputDifferences(
    const TSeqIdSet & vSeqIdFASTAOnly,
    const TSeqIdSet & vSeqIdAGPOnly )
{
    // find the ones in both
    TSeqIdSet vSeqIdTempSet;
    set_intersection( 
        vSeqIdFASTAOnly.begin(), vSeqIdFASTAOnly.end(),
        vSeqIdAGPOnly.begin(), vSeqIdAGPOnly.end(),
        inserter(vSeqIdTempSet, vSeqIdTempSet.begin()) );
    if( ! vSeqIdTempSet.empty() ) {
        LOG_POST(Error << "  These differ between object file and AGP:");
        ITERATE( TSeqIdSet, id_iter, vSeqIdTempSet ) {
            LOG_POST(Error << "    " << *id_iter);
        }
    }

    // find the ones in FASTA only
    vSeqIdTempSet.clear();
    set_difference( 
        vSeqIdFASTAOnly.begin(), vSeqIdFASTAOnly.end(),
        vSeqIdAGPOnly.begin(), vSeqIdAGPOnly.end(),
        inserter(vSeqIdTempSet, vSeqIdTempSet.begin()) );
    if( ! vSeqIdTempSet.empty() ) {
        LOG_POST(Error << "  These are in Object file only: " << "\n"
            << "  (Check above: were some AGP sequences skipped due "
            << "to errors?)");
        ITERATE( TSeqIdSet, id_iter, vSeqIdTempSet ) {
            LOG_POST(Error << "    " << *id_iter);
        }
    }

    // find the ones in AGP only
    vSeqIdTempSet.clear();
    set_difference( 
        vSeqIdAGPOnly.begin(), vSeqIdAGPOnly.end(),
        vSeqIdFASTAOnly.begin(), vSeqIdFASTAOnly.end(),
        inserter(vSeqIdTempSet, vSeqIdTempSet.begin()) );
    if( ! vSeqIdTempSet.empty() ) {
        LOG_POST(Error << "  These are in AGP only: " << "\n"
            << "  (Check above: were some FASTA sequences skipped due "
            << "to errors?)");
        ITERATE( TSeqIdSet, id_iter, vSeqIdTempSet ) {
            LOG_POST(Error << "    " << *id_iter);
        }
    }
}

void CAgpFastaCompareApplication::x_SetBinaryVsText( CNcbiIstream & file_istrm, 
                                                    CFormatGuess::EFormat guess_format )
{
    // set binary vs. text
    switch( guess_format ) {
        case CFormatGuess::eBinaryASN:
            file_istrm >> MSerial_AsnBinary;
            break;
        case CFormatGuess::eTextASN:
            file_istrm >> MSerial_AsnText;
            break;
        default:
            break;
            // a format where binary vs. text is irrelevant
    }
}

/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CAgpFastaCompareApplication::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CAgpFastaCompareApplication().AppMain(argc, argv);

}
