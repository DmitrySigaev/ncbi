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
*   flat-file generator application
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>

#include <serial/objistr.hpp>
#include <serial/serial.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <objtools/format/flat_file_config.hpp>
#include <objtools/format/flat_file_generator.hpp>
#include <objtools/format/flat_expt.hpp>
#include <objects/seqset/gb_release_file.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


class CAsn2FlatApp : public CNcbiApplication, public CGBReleaseFile::ISeqEntryHandler
{
public:
    void Init(void);
    int  Run (void);

    bool HandleSeqEntry(CRef<CSeq_entry>& se);

private:
    // types
    typedef CFlatFileConfig::TFormat    TFormat;
    typedef CFlatFileConfig::TMode      TMode;
    typedef CFlatFileConfig::TStyle     TStyle;
    typedef CFlatFileConfig::TFlags     TFlags;
    typedef CFlatFileConfig::TView      TView;

    CObjectIStream* x_OpenIStream(const CArgs& args);

    CFlatFileGenerator* x_CreateFlatFileGenerator(const CArgs& args);
    TFormat         x_GetFormat(const CArgs& args);
    TMode           x_GetMode(const CArgs& args);
    TStyle          x_GetStyle(const CArgs& args);
    TFlags          x_GetFlags(const CArgs& args);
    TView           x_GetView(const CArgs& args);
    TSeqPos x_GetFrom(const CArgs& args);
    TSeqPos x_GetTo  (const CArgs& args);
    void x_GetLocation(const CSeq_entry_Handle& entry,
        const CArgs& args, CSeq_loc& loc);
    CBioseq_Handle x_DeduceTarget(const CSeq_entry_Handle& entry);

    // data
    CRef<CObjectManager>        m_Objmgr;       // Object Manager
    CNcbiOstream*               m_Os;           // Output stream
    CRef<CFlatFileGenerator>    m_FFGenerator;  // Flat-file generator
};


void CAsn2FlatApp::Init(void)
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(
        GetArguments().GetProgramBasename(),
        "Convert an ASN.1 Seq-entry into a flat report",
        false);
    
    // input
    {{
        // name
        arg_desc->AddOptionalKey("i", "InputFile", 
            "Input file name", CArgDescriptions::eInputFile);
        
        // input file serial format (AsnText\AsnBinary\XML, default: AsnText)
        arg_desc->AddDefaultKey("serial", "SerialFormat", "Input file format",
            CArgDescriptions::eString, "text");
        arg_desc->SetConstraint("serial", &(*new CArgAllow_Strings,
            "text", "binary", "XML"));
    }}

    // batch processing
    {{
        arg_desc->AddOptionalKey("batch", "BatchMode",
            "Process NCBI release file", CArgDescriptions::eString);
        // compression
        arg_desc->AddFlag("c", "Compressed file");
        // propogate top descriptors
        arg_desc->AddFlag("p", "Propogate top descriptors");
    }}
    
    // output
    {{ 
        // name
        arg_desc->AddOptionalKey("o", "OutputFile", 
            "Output file name", CArgDescriptions::eOutputFile);
    }}
    
    // loaders
    {{
        arg_desc->AddOptionalKey("load", "Loader", "Add data loader",
            CArgDescriptions::eString);
        // currently we support only the Genbank loader.
        arg_desc->SetConstraint("load", &(*new CArgAllow_Strings, "gb"));
    }}

    // report
    {{
        // format (default: genbank)
        arg_desc->AddDefaultKey("format", "Format", "Output format",
            CArgDescriptions::eString, "genbank");
        arg_desc->SetConstraint("format",
            &(*new CArgAllow_Strings,
              "genbank", "embl", "ddbj", "gbseq", "ftable", "gff", "gff3"));

        // mode (default: dump)
        arg_desc->AddDefaultKey("mode", "Mode", "Restriction level",
            CArgDescriptions::eString, "gbench");
        arg_desc->SetConstraint("mode", 
            &(*new CArgAllow_Strings, "release", "entrez", "gbench", "dump"));

        // style (default: normal)
        arg_desc->AddDefaultKey("style", "Style", "Formatting style",
            CArgDescriptions::eString, "normal");
        arg_desc->SetConstraint("style", 
            &(*new CArgAllow_Strings, "normal", "segment", "master", "contig"));

        // flags (default: 0)
        arg_desc->AddDefaultKey("flags", "Flags", "Custom flags",
            CArgDescriptions::eInteger, "0");

        // view (default: nucleotide)
        arg_desc->AddDefaultKey("view", "View", "View",
            CArgDescriptions::eString, "nuc");
        arg_desc->SetConstraint("view", 
            &(*new CArgAllow_Strings, "all", "prot", "nuc"));
        
        // id
        arg_desc->AddOptionalKey("id", "ID", 
            "Specific ID to display", CArgDescriptions::eString);

        // from
        arg_desc->AddOptionalKey("from", "From",
            "Begining of shown range", CArgDescriptions::eInteger);
        
        // to
        arg_desc->AddOptionalKey("to", "To",
            "End of shown range", CArgDescriptions::eInteger);
        
        // strand
        
        // accession to extract
    }}
    
    SetupArgDescriptions(arg_desc.release());
}


int CAsn2FlatApp::Run(void)
{
    const CArgs&   args = GetArgs();

    // create object manager
    m_Objmgr.Reset(new CObjectManager);
    if ( !m_Objmgr ) {
        NCBI_THROW(CFlatException, eInternal, "Could not create object manager");
    }
    if ( args["load"]  &&  args["load"].AsString() == "gb" ) {
        m_Objmgr->RegisterDataLoader(*new CGBDataLoader("ID"),
                                 CObjectManager::eDefault);
    }

    // open the output stream
    m_Os = args["o"] ? &(args["o"].AsOutputFile()) : &cout;
    if ( m_Os == 0 ) {
        NCBI_THROW(CFlatException, eInternal, "Could not open output stream");
    }

    // create the flat-file generator
    m_FFGenerator.Reset(x_CreateFlatFileGenerator(args));

    if ( args["batch"] ) {
        CGBReleaseFile in(args["i"].AsString());
        in.RegisterHandler(this);
        in.Read();  // HandleSeqEntry will be called from this function
    } else {
        // open the input file (default: stdin)
        auto_ptr<CObjectIStream> in(x_OpenIStream(args));

        // read in the seq-entry
        CRef<CSeq_entry> se(new CSeq_entry);
        if ( !se ) {
            NCBI_THROW(CFlatException, eInternal, 
                "Could not allocate Seq-entry object");
        }
        in->Read(ObjectInfo(*se));
        if ( se->Which() == CSeq_entry::e_not_set ) {
            NCBI_THROW(CFlatException, eInternal, "Invalid Seq-entry");
        }
        HandleSeqEntry(se);
    }

    m_Os->flush();

    return 0;
}


bool CAsn2FlatApp::HandleSeqEntry(CRef<CSeq_entry>& se)
{
    const CArgs&   args = GetArgs();

    // create new scope
    CRef<CScope> scope(new CScope(*m_Objmgr));
    if ( !scope ) {
        NCBI_THROW(CFlatException, eInternal, "Could not create scope");
    }
    scope->AddDefaults();

    // add entry to scope    
    CSeq_entry_Handle entry = scope->AddTopLevelSeqEntry(*se);
    if ( !entry ) {
        NCBI_THROW(CFlatException, eInternal, "Failed to insert entry to scope.");
    }

    // generate flat file
    if ( args["from"]  ||  args["to"] ) {
        CSeq_loc loc;
        x_GetLocation(entry, args, loc);
        m_FFGenerator->Generate(loc, *scope, *m_Os);
    } else {
        m_FFGenerator->Generate(entry, *m_Os);
    }

    m_FFGenerator->Reset();

    return true;
}


CObjectIStream* CAsn2FlatApp::x_OpenIStream(const CArgs& args)
{
    // determine the file serialization format.
    // use user specifications (if available) otherwise the defualt
    // is AsnText.
    ESerialDataFormat serial = eSerial_AsnText;
    if ( args["serial"] ) {
        const string& val = args["serial"].AsString();
        if ( val == "text" ) {
            serial = eSerial_AsnText;
        } else if ( val == "binary" ) {
            serial = eSerial_AsnBinary;
        } else if ( val == "XML" ) {
            serial = eSerial_Xml;
        }
    }

    // open the input file, or standard input if no file specified.
    return args["i"] ?
        CObjectIStream::Open(args["i"].AsString(), serial) :
        CObjectIStream::Open(serial, cin);
}


CFlatFileGenerator* CAsn2FlatApp::x_CreateFlatFileGenerator(const CArgs& args)
{
    TFormat    format = x_GetFormat(args);
    TMode      mode   = x_GetMode(args);
    TStyle     style  = x_GetStyle(args);
    TFlags     flags  = x_GetFlags(args);
    TView      view   = x_GetView(args);

    CFlatFileConfig cfg(format, mode, style, flags, view);
    return new CFlatFileGenerator(cfg);
}


CAsn2FlatApp::TFormat CAsn2FlatApp::x_GetFormat(const CArgs& args)
{
    const string& format = args["format"].AsString();
    if ( format == "genbank" ) {
        return CFlatFileConfig::eFormat_GenBank;
    } else if ( format == "embl" ) {
        return CFlatFileConfig::eFormat_EMBL;
    } else if ( format == "ddbj" ) {
        return CFlatFileConfig::eFormat_DDBJ;
    } else if ( format == "gbseq" ) {
        return CFlatFileConfig::eFormat_GBSeq;
    } else if ( format == "ftable" ) {
        return CFlatFileConfig::eFormat_FTable;
    } else if ( format == "gff" ) {
        return CFlatFileConfig::eFormat_GFF;
    } else if ( format == "gff3" ) {
        return CFlatFileConfig::eFormat_GFF3;
    }

    // default
    return CFlatFileConfig::eFormat_GenBank;
}


CAsn2FlatApp::TMode CAsn2FlatApp::x_GetMode(const CArgs& args)
{
    const string& mode = args["mode"].AsString();
    if ( mode == "release" ) {
        return CFlatFileConfig::eMode_Release;
    } else if ( mode == "entrez" ) {
        return CFlatFileConfig::eMode_Entrez;
    } else if ( mode == "gbench" ) {
        return CFlatFileConfig::eMode_GBench;
    } else if ( mode == "dump" ) {
        return CFlatFileConfig::eMode_Dump;
    } 

    // default
    return CFlatFileConfig::eMode_GBench;
}


CAsn2FlatApp::TStyle CAsn2FlatApp::x_GetStyle(const CArgs& args)
{
    const string& style = args["style"].AsString();
    if ( style == "normal" ) {
        return CFlatFileConfig::eStyle_Normal;
    } else if ( style == "segment" ) {
        return CFlatFileConfig::eStyle_Segment;
    } else if ( style == "master" ) {
        return CFlatFileConfig::eStyle_Master;
    } else if ( style == "contig" ) {
        return CFlatFileConfig::eStyle_Contig;
    } 

    // default
    return CFlatFileConfig::eStyle_Normal;
}


CAsn2FlatApp::TFlags CAsn2FlatApp::x_GetFlags(const CArgs& args)
{
    return args["flags"].AsInteger();
}


CAsn2FlatApp::TView CAsn2FlatApp::x_GetView(const CArgs& args)
{
    const string& view = args["view"].AsString();
    if ( view == "all" ) {
        return CFlatFileConfig::fViewAll;
    } else if ( view == "prot" ) {
        return CFlatFileConfig::fViewProteins;
    } else if ( view == "nuc" ) {
        return CFlatFileConfig::fViewNucleotides;
    } 

    // default
    return CFlatFileConfig::fViewNucleotides;
}


TSeqPos CAsn2FlatApp::x_GetFrom(const CArgs& args)
{
    return args["from"] ? 
        static_cast<TSeqPos>(args["from"].AsInteger() - 1) :
        CRange<TSeqPos>::GetWholeFrom(); 
}


TSeqPos CAsn2FlatApp::x_GetTo(const CArgs& args)
{
    return args["to"] ? 
        static_cast<TSeqPos>(args["to"].AsInteger() - 1) :
        CRange<TSeqPos>::GetWholeTo();
}


void CAsn2FlatApp::x_GetLocation
(const CSeq_entry_Handle& entry,
 const CArgs& args,
 CSeq_loc& loc)
{
    _ASSERT(entry);
        
    CBioseq_Handle h = x_DeduceTarget(entry);
    if ( !h ) {
        NCBI_THROW(CFlatException, eInvalidParam,
            "Cannot deduce target bioseq.");
    }
    TSeqPos length = h.GetInst_Length();
    TSeqPos from   = x_GetFrom(args);
    TSeqPos to     = min(x_GetTo(args), length);
    
    if ( from == CRange<TSeqPos>::GetWholeFrom()  &&  to == length ) {
        // whole
        loc.SetWhole().Assign(*h.GetSeqId());
    } else {
        // interval
        loc.SetInt().SetId().Assign(*h.GetSeqId());
        loc.SetInt().SetFrom(from);
        loc.SetInt().SetTo(to);
    }
}


// if the 'from' or 'to' flags are specified try to guess the bioseq.
CBioseq_Handle CAsn2FlatApp::x_DeduceTarget(const CSeq_entry_Handle& entry)
{
    if ( entry.IsSeq() ) {
        return entry.GetSeq();
    }

    _ASSERT(entry.IsSet());
    CBioseq_set_Handle bsst = entry.GetSet();
    if ( !bsst.IsSetClass() ) {
        NCBI_THROW(CFlatException, eInvalidParam,
            "Cannot deduce target bioseq.");
    }
    _ASSERT(bsst.IsSetClass());
    switch ( bsst.GetClass() ) {
    case CBioseq_set::eClass_nuc_prot:
        // return the nucleotide
        for ( CSeq_entry_CI it(entry); it; ++it ) {
            if ( it->IsSeq() ) {
                CBioseq_Handle h = it->GetSeq();
                if ( h  &&  CSeq_inst::IsNa(h.GetInst_Mol()) ) {
                    return h;
                }
            }
        }
        break;
    case CBioseq_set::eClass_gen_prod_set:
        // return the genomic
        for ( CSeq_entry_CI it(bsst); it; ++it ) {
            if ( it->IsSeq()  &&
                 it->GetSeq().GetInst_Mol() == CSeq_inst::eMol_dna ) {
                 return it->GetSeq();
            }
        }
        break;
    case CBioseq_set::eClass_segset:
        // return the segmented bioseq
        for ( CSeq_entry_CI it(bsst); it; ++it ) {
            if ( it->IsSeq() ) {
                return it->GetSeq();
            }
        }
        break;
    default:
        break;
    }
    NCBI_THROW(CFlatException, eInvalidParam,
            "Cannot deduce target bioseq.");
}


END_NCBI_SCOPE

USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//
// Main

int main(int argc, const char** argv)
{
    return CAsn2FlatApp().AppMain(argc, argv, 0, eDS_Default, 0);
}

/*
* ===========================================================================
*
* $Log$
* Revision 1.10  2004/06/21 18:55:57  ucko
* Add a GFF3 format.
*
* Revision 1.9  2004/05/21 21:41:38  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.8  2004/05/19 14:48:36  shomrat
* Implemented batch processing
*
* Revision 1.7  2004/04/22 16:04:37  shomrat
* Support partial region
*
* Revision 1.6  2004/03/31 19:23:47  shomrat
* name changes
*
* Revision 1.5  2004/03/25 21:07:54  shomrat
* Use handles
*
* Revision 1.4  2004/03/05 18:51:45  shomrat
* use view instead of filter
*
* Revision 1.3  2004/02/11 22:58:26  shomrat
* using flags from flag file
*
* Revision 1.2  2004/01/20 20:41:53  ucko
* Don't try to return auto_ptrs, or construct them with "=".
*
* Revision 1.1  2004/01/14 15:03:29  shomrat
* Initial Revision
*
*
* ===========================================================================
*/
