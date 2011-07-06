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
* Author:  Aaron Ucko, Mati Shomrat, NCBI
*
* File Description:
*   fasta-file generator application
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <dbapi/driver/drivers.hpp>

#include <objtools/format/flat_file_config.hpp>
#include <objtools/format/flat_file_generator.hpp>
#include <objtools/format/flat_expt.hpp>
#include <objects/seqset/gb_release_file.hpp>

#include <objects/entrez2/Entrez2_boolean_element.hpp>
#include <objects/entrez2/Entrez2_boolean_reply.hpp>
#include <objects/entrez2/Entrez2_boolean_exp.hpp>
#include <objects/entrez2/Entrez2_eval_boolean.hpp>
#include <objects/entrez2/Entrez2_id_list.hpp>
#include <objects/entrez2/entrez2_client.hpp>

#include <objtools/cleanup/cleanup.hpp>

#include <util/compress/zlib.hpp>
#include <util/compress/stream.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/writers/writer_exception.hpp>
#include <objtools/writers/writer.hpp>
#include <objtools/writers/gff_writer.hpp>
#include <objtools/writers/gtf_writer.hpp>
#include <objtools/writers/gff3_writer.hpp>
#include <objtools/writers/wiggle_writer.hpp>
#include <objtools/writers/bed_track_record.hpp>
#include <objtools/writers/bed_feature_record.hpp>
#include <objtools/writers/bed_writer.hpp>
#include <objtools/writers/vcf_writer.hpp>
#include <objtools/writers/gvf_writer.hpp>

#include <time.h>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
class CAnnotWriterApp : public CNcbiApplication
//  ----------------------------------------------------------------------------
{
public:
    void Init();
    int Run();

private:
    CWriterBase* x_CreateWriter(
        CScope&,
        CNcbiOstream&,
        const CArgs& );

    bool TrySeqAnnot(
        CScope&,
        CObjectIStream&,
        CNcbiOstream& );

    bool TrySeqEntry(
        CScope&,
        CObjectIStream&,
        CNcbiOstream& );

    bool TryBioseqSet(
        CScope&,
        CObjectIStream&,
        CNcbiOstream& );

    bool TryBioseq(
        CScope&,
        CObjectIStream&,
        CNcbiOstream& );

    bool TrySeqAlign(
        CScope&,
        CObjectIStream&,
        CNcbiOstream& );

    void WriteSeqId(CScope& scope,
                    CNcbiIstream& istr,
                    CNcbiOstream& ostr);

    bool WriteHandleGff3(
        CBioseq_Handle bsh,
        CNcbiOstream& );

    CGff2Writer::TFlags GffFlags( 
        const CArgs& );
};

//  ----------------------------------------------------------------------------
void CAnnotWriterApp::Init()
//  ----------------------------------------------------------------------------
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(
        GetArguments().GetProgramBasename(),
        "Convert an ASN.1 to alternative file formats",
        false);
    
    // input
    {{
         arg_desc->AddDefaultKey( "i", "InputFile", 
                                  "Input file name",
                                  CArgDescriptions::eInputFile,
                                  "-");
         arg_desc->AddDefaultKey("ifmt", "InputFormat",
                                 "Format of input file",
                                 CArgDescriptions::eString,
                                 "asn");
         arg_desc->SetConstraint("ifmt",
                                 &(*new CArgAllow_Strings,
                                   "id", 
                                   "asn-text"));
    }}

    // format
    {{
    arg_desc->AddDefaultKey(
        "format", 
        "STRING",
        "Output format",
        CArgDescriptions::eString, 
        "gff" );
    arg_desc->SetConstraint(
        "format", 
        &(*new CArgAllow_Strings, 
            "gvf",
            "gff", "gff2", 
            "gff3", 
            "gtf", 
            "wig", "wiggle",
            "bed",
            "vcf" ) );
    }}

    // output
    {{ 
        arg_desc->AddOptionalKey( "o", "OutputFile", 
            "Output file name", CArgDescriptions::eOutputFile );
    }}

    //  flags
    {{
    arg_desc->AddFlag(
        "structibutes",
        "limit attributes to inter feature relationships",
        true );
    arg_desc->AddFlag(
        "skip-gene-features",
        "GTF only: Do not emit gene features (for GTF 2.2 compliancy)",
        true );
    arg_desc->AddFlag(
        "skip-exon-numbers",
        "GTF only: For exon features, do not emit exon numbers",
        true );
    }}
    
    SetupArgDescriptions(arg_desc.release());
}


//  ----------------------------------------------------------------------------
int CAnnotWriterApp::Run()
//  ----------------------------------------------------------------------------
{
	CONNECT_Init(&GetConfig());

    const CArgs& args = GetArgs();

    CNcbiOstream* pOs = args["o"] ? &(args["o"].AsOutputFile()) : &cout;
    if ( pOs == 0 ) {
        NCBI_THROW(CFlatException, eInternal, "Could not open output stream");
    }

    CRef< CObjectManager > pObjMngr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager( *pObjMngr );
    CRef< CScope > pScope( new CScope( *pObjMngr ) );
    pScope->AddDefaults();

    CNcbiIstream& istr = args["i"].AsInputFile();
    auto_ptr<CObjectIStream> pIs
        (CObjectIStream::Open(eSerial_AsnText, istr));

    string ifmt = args["ifmt"].AsString();

    while ( true ) {
        if (ifmt == "id") {
            WriteSeqId(*pScope, istr, *pOs);
            break;
        }
        if ( TrySeqAnnot( *pScope, *pIs, *pOs ) ) {
            continue;
        }
        if ( TrySeqEntry( *pScope, *pIs, *pOs ) ) {
            continue;
        }
        if ( TryBioseqSet( *pScope, *pIs, *pOs ) ) {
            continue;
        }
        if ( TryBioseq( *pScope, *pIs, *pOs ) ) {
            continue;
        }
        if ( TrySeqAlign( *pScope, *pIs, *pOs ) ) {
            continue;
        }
        if ( ! pIs->EndOfData() ) {
            cerr << "Object type not supported!" << endl;
        }
        break;
    } 
    pOs->flush();

    pIs.reset();
    return 0;
}

//  -----------------------------------------------------------------------------
void CAnnotWriterApp::WriteSeqId(
    CScope& scope,
    CNcbiIstream& istr,
    CNcbiOstream& ostr )
//  -----------------------------------------------------------------------------
{
    string line;
    while (NcbiGetlineEOL(istr, line)) {
        NStr::TruncateSpacesInPlace(line);
        if (line.empty()  ||  line[0] == '#') {
            continue;
        }
        CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(line);
        CBioseq_Handle bsh = scope.GetBioseqHandle(idh);

        auto_ptr<CWriterBase> pWriter(x_CreateWriter(scope, ostr, GetArgs()));
        if ( ! pWriter.get() ) {
            cerr << "annotwriter: Cannot create suitable writer!" << endl;
        }
        pWriter->WriteHeader();
        pWriter->WriteBioseqHandle(bsh);
        pWriter->WriteFooter();
    }
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::TrySeqAnnot(
    CScope& scope,
    CObjectIStream& istr,
    CNcbiOstream& ostr )
//  -----------------------------------------------------------------------------
{
    CNcbiStreampos curr = istr.GetStreamPos();
    try {
        CRef<CSeq_annot> pAnnot(new CSeq_annot);
        istr >> *pAnnot;

        CWriterBase* pWriter = x_CreateWriter( scope, ostr, GetArgs() );
        if ( ! pWriter ) {
            cerr << "annotwriter: Cannot create suitable writer!" << endl;
            return false;
        }
        pWriter->WriteHeader();
        pWriter->WriteAnnot( *pAnnot );
        pWriter->WriteFooter();
        delete pWriter;
        return true;
    }
    catch ( ... ) {
        istr.SetStreamPos ( curr );
        return false;
    }
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::TryBioseqSet(
    CScope& scope,
    CObjectIStream& istr,
    CNcbiOstream& ostr )
//  -----------------------------------------------------------------------------
{
    CNcbiStreampos curr = istr.GetStreamPos();
    try {
        CRef<CBioseq_set> pBioset(new CBioseq_set);
        istr >> *pBioset;

        const CBioseq_set::TSeq_set& bss = pBioset->GetSeq_set();
        for ( CBioseq_set::TSeq_set::const_iterator it = bss.begin(); it != bss.end(); ++it ) {
            const CSeq_entry& se = **it;

            CRef< CObjectManager > pObjMngr = CObjectManager::GetInstance();
            CGBDataLoader::RegisterInObjectManager( *pObjMngr );
            CRef< CScope > pScope( new CScope( *pObjMngr ) );
            pScope->AddDefaults();
            const CBioseq& bs = se.GetSeq();
            pScope->AddBioseq( bs );

            WriteHandleGff3( pScope->GetBioseqHandle( bs ), ostr );
        }
        return true;
    }
    catch ( ... ) {
        istr.SetStreamPos ( curr );
        return false;
    }
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::TryBioseq(
    CScope& scope,
    CObjectIStream& istr,
    CNcbiOstream& ostr )
//  -----------------------------------------------------------------------------
{
    CNcbiStreampos curr = istr.GetStreamPos();
    try {
        CRef<CBioseq> pBioseq(new CBioseq);
        istr >> *pBioseq;

        CWriterBase* pWriter = x_CreateWriter( scope, ostr, GetArgs() );
        if ( ! pWriter ) {
            cerr << "annotwriter: Cannot create suitable writer!" << endl;
            return false;
        }
        pWriter->WriteHeader();
        CTypeIterator<CSeq_annot> annot_iter( *pBioseq );
        for ( ;  annot_iter;  ++annot_iter ) {
            CRef< CSeq_annot > annot( annot_iter.operator->() );
            if ( ! pWriter->WriteAnnot( *annot ) ) {
                return false;
            }
        }
        pWriter->WriteFooter();
        delete pWriter;
        return true;
    }
    catch ( ... ) {
        istr.SetStreamPos ( curr );
        return false;
    }
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::TrySeqEntry(
    CScope& scope,
    CObjectIStream& istr,
    CNcbiOstream& ostr )
//  -----------------------------------------------------------------------------
{
    CNcbiStreampos curr = istr.GetStreamPos();
    try {
        CRef<CSeq_entry> pEntry( new CSeq_entry );
        istr >> *pEntry;

        CSeq_entry_Handle seh = scope.AddTopLevelSeqEntry( *pEntry );
        CWriterBase* pWriter = x_CreateWriter( scope, ostr, GetArgs() );
        if ( ! pWriter ) {
            cerr << "annotwriter: Cannot create suitable writer!" << endl;
            return false;
        }
        pWriter->WriteHeader();
        for ( CBioseq_CI bci( seh ); bci; ++bci ) {
            if ( ! pWriter->WriteBioseqHandle( *bci ) ) {
                return false;
            }
        }
        pWriter->WriteFooter();
        delete pWriter;
        return true;

    }
    catch ( ... ) {
        istr.SetStreamPos ( curr );
        return false;
    }
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::TrySeqAlign(
    CScope& scope,
    CObjectIStream& istr,
    CNcbiOstream& ostr )
//  -----------------------------------------------------------------------------
{
    CNcbiStreampos curr = istr.GetStreamPos();
    try {
        CRef<CSeq_align> pAlign(new CSeq_align);
        istr >> *pAlign;

        CWriterBase* pWriter = x_CreateWriter( scope, ostr, GetArgs() );
        if ( ! pWriter ) {
            cerr << "annotwriter: Cannot create suitable writer!" << endl;
            return false;
        }
        pWriter->WriteHeader();
        pWriter->WriteAlign( *pAlign );
        pWriter->WriteFooter();
        delete pWriter;
        return true;
    }
    catch ( ... ) {
        istr.SetStreamPos ( curr );
        return false;
    }
}

//  -----------------------------------------------------------------------------
bool CAnnotWriterApp::WriteHandleGff3(
    CBioseq_Handle bsh,
    CNcbiOstream& ostr )
//  -----------------------------------------------------------------------------
{
    CGff3Writer writer( ostr, GffFlags( GetArgs() ) );
    writer.WriteHeader();
    writer.WriteBioseqHandle( bsh ) || true;
    writer.WriteFooter();
    return true;
}

//  -----------------------------------------------------------------------------
CGff2Writer::TFlags CAnnotWriterApp::GffFlags(
    const CArgs& args )
//  -----------------------------------------------------------------------------
{
    CGff2Writer::TFlags eFlags = CGff2Writer::fNormal;
    if ( args["structibutes"] ) {
        eFlags = CGtfWriter::TFlags( eFlags | CGtfWriter::fStructibutes );
    }
    if ( args["skip-gene-features"] ) {
        eFlags = CGtfWriter::TFlags( eFlags | CGtfWriter::fNoGeneFeatures );
    }
    if ( args["skip-exon-numbers"] ) {
        eFlags = CGtfWriter::TFlags( eFlags | CGtfWriter::fNoExonNumbers );
    }
    return eFlags;
}

//  ----------------------------------------------------------------------------
CWriterBase* CAnnotWriterApp::x_CreateWriter(
    CScope& scope,
    CNcbiOstream& ostr,
    const CArgs& args )
//  ----------------------------------------------------------------------------
{
    const string strFormat = args[ "format" ].AsString();
    if ( strFormat == "gff"  ||  strFormat == "gff2" ) { 
        return new CGff2Writer( scope, ostr, GffFlags( args ) );
    }
    if ( strFormat == "gff3" ) { 
        return new CGff3Writer( scope, ostr, GffFlags( args ) );
    }
    if ( strFormat == "gtf" ) {
        return new CGtfWriter( scope, ostr, GffFlags( args ) );
    }
    if ( strFormat == "gvf" ) { 
        return new CGvfWriter( scope, ostr, GffFlags( args ) );
    }
    if ( strFormat == "wiggle"  ||  strFormat == "wig" ) {
        return new CWiggleWriter( ostr, args["tracksize"].AsInteger() );
    }
    if ( strFormat == "bed" ) {
        return new CBedWriter( scope, ostr );
    }
    if ( strFormat == "vcf" ) {
        return new CVcfWriter( scope, ostr );
    }
    return 0;
}

END_NCBI_SCOPE

USING_NCBI_SCOPE;

/////////////////////////////////////////////////////////////////////////////
// Main

int main(int argc, const char** argv)
{
    return CAnnotWriterApp().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}
