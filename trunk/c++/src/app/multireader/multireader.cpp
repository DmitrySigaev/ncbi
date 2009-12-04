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
* Author:  Frank Ludwig, NCBI
*
* File Description:
*   Reader for selected data file formats
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbistl.hpp>
#include <util/format_guess.hpp>

#include <serial/iterator.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objostrasn.hpp>
#include <serial/serial.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seq/Seq_annot.hpp>

#include <objects/seqset/Seq_entry.hpp>

#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/error_container.hpp>
#include <objtools/readers/idmapper.hpp>
#include <objtools/readers/multireader.hpp>
#include <objtools/readers/reader_base.hpp>
#include <objtools/readers/bed_reader.hpp> //  for flag definitions only
#include <objtools/readers/wiggle_reader.hpp> //  for flag definitions only
#include <objtools/readers/gff3_reader.hpp> //  for flag definitions only

USING_NCBI_SCOPE;
USING_SCOPE(objects);

//  ============================================================================
class CMultiReaderApp
//  ============================================================================
     : public CNcbiApplication
{
public:
    CMultiReaderApp(): m_pErrors( 0 ) {};
    
protected:
    void AppInitialize();
    
    void ReadObject(
        CRef<CSerialObject>& );
        
    void ReadAnnots(
        vector< CRef<CSeq_annot> >& );
        
    void MapObject(
        CSerialObject& );
        
    void MapAnnots(
        vector< CRef<CSeq_annot> >& );
        
    void DumpObject(
        CSerialObject& );
        
    void DumpAnnots(
        vector< CRef<CSeq_annot> >& );
        
    void DumpErrors(
        CNcbiOstream& );
    
    void SetFormat(
        const CArgs& );
        
    void SetFlags(
        const CArgs& );
            
    CIdMapper* GetMapper();
    
    void SetErrorContainer(
        const CArgs& );
            
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
    
    CFormatGuess::EFormat m_uFormat;
    CNcbiIstream* m_pInput;
    CNcbiOstream* m_pOutput;
    CErrorContainerBase* m_pErrors;
    bool m_bCheckOnly;
    int m_iFlags;
};

//  ============================================================================
class CErrorContainerCustom:
//  ============================================================================
    public CErrorContainerBase
{
public:
    CErrorContainerCustom(
        int iMaxCount,
        int iMaxLevel ): m_iMaxCount( iMaxCount ), m_iMaxLevel( iMaxLevel ) {};
    ~CErrorContainerCustom() {};
    
    bool
    PutError(
        const ILineError& err ) 
    {
        m_Errors.push_back( 
            CLineError( err.Severity(), err.Line(), err.Message() ) );
        return (err.Severity() <= m_iMaxLevel) && (m_Errors.size() < m_iMaxCount);
    };
    
protected:
    size_t m_iMaxCount;
    int m_iMaxLevel;    
};    
        
//  ============================================================================
void CMultiReaderApp::Init(void)
//  ============================================================================
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext
        (GetArguments().GetProgramBasename(),
         "C++ multi format file reader");

    //
    //  input / output:
    //        
    arg_desc->AddKey(
        "input", 
        "File_In",
        "Input filename",
        CArgDescriptions::eInputFile);

    arg_desc->AddDefaultKey(
        "output", 
        "OutputFile",
        "File_Out",
        CArgDescriptions::eOutputFile, "-"); 

    arg_desc->AddDefaultKey(
        "format", 
        "STRING",
        "Input file format",
        CArgDescriptions::eString, 
        "guess");
    arg_desc->SetConstraint(
        "format", 
        &(*new CArgAllow_Strings, 
            "bed", "microarray", "bed15", "wig", "wiggle", "gtf", "guess") );

    arg_desc->AddDefaultKey(
        "flags",
        "INTEGER",
        "Additional flags passed to the reader",
        CArgDescriptions::eString,
        "0" );

    //
    //  ID mapping:
    //
    arg_desc->AddDefaultKey(
        "mapfile",
        "File_In",
        "IdMapper config filename",
        CArgDescriptions::eString, "" );

    arg_desc->AddDefaultKey(
        "genome", 
        "STRING",
        "UCSC build number",
        CArgDescriptions::eString, 
        "" );
        
    //
    //  Error policy:
    //        
    arg_desc->AddFlag(
        "checkonly",
        "check for errors only",
        true );
        
    arg_desc->AddFlag(
        "noerrors",
        "suppress error display",
        true );
        
    arg_desc->AddFlag(
        "lenient",
        "accept all input format errors",
        true );
        
    arg_desc->AddFlag(
        "strict",
        "accept no input format errors",
        true );
        
    arg_desc->AddDefaultKey(
        "max-error-count", 
        "INTEGER",
        "Maximum permissible error count",
        CArgDescriptions::eInteger,
        "-1" );
        
    arg_desc->AddDefaultKey(
        "max-error-level", 
        "STRING",
        "Maximum permissible error level",
        CArgDescriptions::eString,
        "warning" );
        
    arg_desc->SetConstraint(
        "max-error-level", 
        &(*new CArgAllow_Strings, 
            "info", "warning", "error" ) );

    //
    //  bed reader specific arguments:
    //
    arg_desc->AddFlag(
        "all-ids-as-local",
        "turn all ids into local ids",
        true );
        
    arg_desc->AddFlag(
        "numeric-ids-as-local",
        "turn integer ids into local ids",
        true );
        
    //
    //  wiggle reader specific arguments:
    //
    arg_desc->AddFlag(
        "join-same",
        "join abutting intervals",
        true );
        
    arg_desc->AddFlag(
        "as-byte",
        "generate byte compressed data",
        true );
    
    arg_desc->AddFlag(
        "as-graph",
        "generate graph object",
        true );
    
    SetupArgDescriptions(arg_desc.release());
}

//  ============================================================================
int 
CMultiReaderApp::Run(void)
//  ============================================================================
{   
    AppInitialize();
    
    CRef< CSerialObject> object;
    vector< CRef< CSeq_annot > > annots;
    switch( m_uFormat ) {

    default:    
        try {
            ReadObject( object );
            MapObject( *object );
            DumpObject( *object );       
        }
        catch ( CObjReaderLineException& /*err*/ ) {
        }
        break;

    case CFormatGuess::eBed:
    case CFormatGuess::eGtf:
        try {
            ReadAnnots( annots );
            MapAnnots( annots );
            DumpAnnots( annots );       
        }
        catch ( CObjReaderLineException& /*err*/ ) {
        }
        break;
    }
    DumpErrors( cerr );
    
    return 0;
}

//  ============================================================================
void CMultiReaderApp::Exit(void)
//  ============================================================================
{
    delete m_pErrors;
    
    SetDiagStream(0);
}

//  ============================================================================
void CMultiReaderApp::AppInitialize()
//  ============================================================================
{
    const CArgs& args = GetArgs();
    m_pInput = &args["input"].AsInputFile();
    m_pOutput = &args["output"].AsOutputFile();
    
    SetFormat( args );    
    SetFlags( args );
    m_bCheckOnly = args["checkonly"];

    SetErrorContainer( args );
}

//  ============================================================================
void CMultiReaderApp::SetFormat(
    const CArgs& args )
//  ============================================================================
{
    m_uFormat = CFormatGuess::eUnknown;    
    string format = args["format"].AsString();
    const string& strProgramName = GetProgramDisplayName();
    
    if ( NStr::StartsWith( strProgramName, "wig" ) || format == "wig" ||
        format == "wiggle" ) {
        m_uFormat = CFormatGuess::eWiggle;
    }
    if ( NStr::StartsWith( strProgramName, "bed" ) || format == "bed" ) {
        m_uFormat = CFormatGuess::eBed;
    }
    if ( NStr::StartsWith( strProgramName, "b15" ) || format == "bed15" ||
        format == "microarray" ) {
        m_uFormat = CFormatGuess::eBed15;
    }
    if ( NStr::StartsWith( strProgramName, "gtf" ) || format == "gtf" ) {
        m_uFormat = CFormatGuess::eGtf;
    }
    if ( m_uFormat == CFormatGuess::eUnknown ) {
        m_uFormat = CFormatGuess::Format( *m_pInput );
    }
}

//  ============================================================================
void CMultiReaderApp::SetFlags(
    const CArgs& args )
//  ============================================================================
{
    if (m_uFormat == CFormatGuess::eUnknown) {
        SetFormat( args );
    }
    m_iFlags = NStr::StringToInt( 
        args["flags"].AsString(), NStr::fConvErr_NoThrow, 16 );
        
    switch( m_uFormat ) {
    
    case CFormatGuess::eWiggle:
        if ( args["join-same"] ) {
            m_iFlags |= CWiggleReader::fJoinSame;
        }
        if ( args["as-byte"] ) {
            m_iFlags |= CWiggleReader::fAsByte;
        }
        if ( args["as-graph"] ) {
            m_iFlags |= CWiggleReader::fAsGraph;
        }
        break;
    
    case CFormatGuess::eBed:
        if ( args["all-ids-as-local"] ) {
            m_iFlags |= CBedReader::fAllIdsAsLocal;
        }
        if ( args["numeric-ids-as-local"] ) {
            m_iFlags |= CBedReader::fNumericIdsAsLocal;
        }
        break;
                   
    default:
        break;
    }
}

//  ============================================================================
void CMultiReaderApp::ReadObject(
    CRef<CSerialObject>& object )
//  ============================================================================
{
    CMultiReader reader( m_uFormat, m_iFlags );
    object = reader.ReadObject( *m_pInput, m_pErrors );
}

//  ============================================================================
void CMultiReaderApp::ReadAnnots(
    vector< CRef<CSeq_annot> >& annots )
//  ============================================================================
{
    switch ( m_uFormat ) {
   
        default:
            break;
 
        case CFormatGuess::eBed: {
            CBedReader reader( m_iFlags );
            reader.ReadSeqAnnots( annots, *m_pInput, m_pErrors );
            break;
        }    
        case CFormatGuess::eGtf: {
            CGff3Reader reader( m_iFlags );
            reader.ReadSeqAnnots( annots, *m_pInput, m_pErrors );
            break;
        }
    }
}

//  ============================================================================
void CMultiReaderApp::MapAnnots(
    vector< CRef<CSeq_annot> >& annots )
//  ============================================================================
{
    auto_ptr< CIdMapper > pMapper( GetMapper() );
    if ( ! pMapper.get() ) {
        return;
    }
    for ( size_t u=0; u < annots.size(); ++u ) {
        CRef< CSeq_annot > pAnnot = annots[u];
        pMapper->MapObject( *pAnnot );
    }
}

//  ============================================================================
void CMultiReaderApp::DumpAnnots(
    vector< CRef<CSeq_annot> >& annots )
//  ============================================================================
{
//    *m_pOutput << "Here we go:" << endl;
    if ( m_bCheckOnly ) {
        return;
    }
    for ( size_t u=0; u < annots.size(); ++u ) {
        *m_pOutput << MSerial_AsnText << *(annots[u]);
    }
}

//  ============================================================================
void CMultiReaderApp::MapObject(
    CSerialObject& object )
//  ============================================================================
{
    auto_ptr<CIdMapper> pMapper(GetMapper());
    if ( !pMapper.get() ) {
        return;
    }
    pMapper->MapObject( object );
}
    
//  ============================================================================
void CMultiReaderApp::DumpObject(
    CSerialObject& object )
//  ============================================================================
{
    if ( m_bCheckOnly ) {
        return;
    }
    *m_pOutput << MSerial_AsnText << object << endl;
}

//  ============================================================================
CIdMapper*
CMultiReaderApp::GetMapper()
//  ============================================================================
{
    const CArgs& args = GetArgs();
    
    string strBuild = args["genome"].AsString();
    string strMapFile = args["mapfile"].AsString();
    
    bool bNoMap = strBuild.empty() && strMapFile.empty();
    if ( bNoMap ) {
        return 0;
    }
    if ( !strMapFile.empty() ) {
        CNcbiIfstream* pMapFile = new CNcbiIfstream( strMapFile.c_str() );
        CIdMapper* pMapper = new CIdMapperConfig( 
            *pMapFile, strBuild, false, m_pErrors );
        pMapFile->close();
        return pMapper;
    }
    return new CIdMapperBuiltin( strBuild, false, m_pErrors );
}        

//  ============================================================================
void
CMultiReaderApp::SetErrorContainer(
    const CArgs& args )
//  ============================================================================
{
    //
    //  By default, allow all errors up to the level of "warning" but nothing
    //  more serious. -strict trumps everything else, -lenient is the second
    //  strongest. In the absence of -strict and -lenient, -max-error-count and
    //  -max-error-level become additive, i.e. both are enforced.
    //
    if ( args["noerrors"] ) {   // not using error policy at all
        m_pErrors = 0;
        return;
    }
    if ( args["strict"] ) {
        m_pErrors = new CErrorContainerStrict;
        return;
    }
    if ( args["lenient"] ) {
        m_pErrors = new CErrorContainerLenient;
        return;
    }
    
    int iMaxErrorCount = args["max-error-count"].AsInteger();
    int iMaxErrorLevel = eDiag_Warning;
    string strMaxErrorLevel = args["max-error-level"].AsString();
    if ( strMaxErrorLevel == "info" ) {
        iMaxErrorLevel = eDiag_Info;
    }
    else if ( strMaxErrorLevel == "error" ) {
        iMaxErrorLevel = eDiag_Error;
    }
    
    if ( iMaxErrorCount == -1 ) {
        m_pErrors = new CErrorContainerLevel( iMaxErrorLevel );
        return;
    }
    m_pErrors = new CErrorContainerCustom( iMaxErrorCount, iMaxErrorLevel );
}
    
//  ============================================================================
void CMultiReaderApp::DumpErrors(
    CNcbiOstream& out )
//  ============================================================================
{
    if ( m_pErrors ) {
        m_pErrors->Dump( out );
    }
}

//  ============================================================================
int main(int argc, const char* argv[])
//  ============================================================================
{
    // Execute main application function
    return CMultiReaderApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
