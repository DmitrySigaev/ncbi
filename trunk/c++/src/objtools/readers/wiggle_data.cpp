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
 * Author:  Frank Ludwig
 *
 * File Description:
 *   WIGGLE transient data structures
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>

// Objects includes
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <objects/seqres/Real_graph.hpp>
#include <objects/seqres/Int_graph.hpp>
#include <objects/seqres/Byte_graph.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqtable/seqtable__.hpp>
#include <objtools/readers/reader_exception.hpp>

#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/error_container.hpp>
#include <objtools/readers/reader_base.hpp>
#include <objtools/readers/wiggle_reader.hpp>
#include <cmath>

#include "wiggle_data.hpp"

#define ROUND(x)    floor((x)+0.5)

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ===========================================================================
CWiggleData::CWiggleData(
    unsigned int seq_start,
    unsigned int seq_span,
    double value ):
//  ===========================================================================
    m_uSeqStart( seq_start ),
    m_uSeqSpan( seq_span ),
    m_dValue( value )
{
};

//  ===========================================================================
CWiggleRecord::CWiggleRecord()
//  ===========================================================================
{
    Reset();
};

//  ----------------------------------------------------------------------------
void CWiggleRecord::Reset()
//  ----------------------------------------------------------------------------
{
    m_strName = "User Track";
    m_strBuild = "";
    m_strChrom = "";
    m_uSeqStart = 0;
    m_uSeqStep = 0;
    m_uSeqSpan = 1;
    m_dValue = 0;
}

//  ----------------------------------------------------------------------------
void CWiggleRecord::ParseTrackDefinition(
    const vector<string>& data )
//  ----------------------------------------------------------------------------
{
    Reset();
    if ( data.size() < 2 || data[0] != "track" ) {
        CObjReaderLineException err( 
            eDiag_Critical,
            0,
            "Missing track line --- Is this even WIGGLE?" );
        throw( err );
    }
    vector<string>::const_iterator it = data.begin();
    for ( ++it; it != data.end(); ++it ) {
        string strKey;
        string strValue;
        if ( ! NStr::SplitInTwo( *it, "=", strKey, strValue ) ) {
            CObjReaderLineException err( 
                eDiag_Error,
                0,
                "Invalid track line format --- only <key>=<value> pairs expected." );
            throw( err );
        }
        NStr::ReplaceInPlace( strValue, "\"", "" );
        if ( strKey == "name" ) {
            m_strName = strValue;
        }
    }
}

//  ----------------------------------------------------------------------------
void CWiggleRecord::ParseDataBed(
    const vector<string>& data )
//  ----------------------------------------------------------------------------
{
    if ( data.size() != 4 ) {
        CObjReaderLineException err( 
            eDiag_Error,
            0,
            "Invalid data line --- BED data with exactly four columns expected" );
        throw( err );
    }
    m_strChrom = data[0];
    try {
        m_uSeqStart = NStr::StringToUInt( data[1] );
    }
    catch ( ... ) {
        CObjReaderLineException err( 
            eDiag_Error,
            0,
            "Invalid data line --- Bad \"ChromStart\" value." );
        throw( err );
    }
    try {
        m_uSeqSpan = NStr::StringToUInt( data[2] ) - m_uSeqStart;
    }
    catch ( ... ) {
        CObjReaderLineException err( 
            eDiag_Error,
            0,
            "Invalid data line --- Bad \"ChromEnd\" value." );
        throw( err );
    }
    try {
        m_dValue = NStr::StringToDouble( data[3] );
    }
    catch ( ... ) {
        CObjReaderLineException err( 
            eDiag_Error,
            0,
            "Invalid data line --- Bad \"Score\" value." );
        throw( err );
    }
}

//  ----------------------------------------------------------------------------
void CWiggleRecord::ParseDataVarstep(
    const vector<string>& data )
//  ----------------------------------------------------------------------------
{
    try {
        m_uSeqStart = NStr::StringToUInt( data[0] ) - 1; // varStep is 1- based
    }
    catch ( ... ) {
        CObjReaderLineException err( 
            eDiag_Error,
            0,
            "Invalid data line --- Bad \"ChromStart\" value." );
        throw( err );
    }
    try {
        m_dValue = NStr::StringToDouble( data[1] );
    }
    catch ( ... ) {
        CObjReaderLineException err( 
            eDiag_Error,
            0,
            "Invalid data line --- Bad \"Score\" value." );
        throw( err );
    }
}

//  ----------------------------------------------------------------------------
void CWiggleRecord::ParseDataFixedstep(
    const vector<string>& data )
//  ----------------------------------------------------------------------------
{
    m_uSeqStart += m_uSeqStep;
    try {
        m_dValue = NStr::StringToDouble( data[0] );
    }
    catch ( ... ) {
        CObjReaderLineException err( 
            eDiag_Error,
            0,
            "Invalid data line --- Bad \"Score\" value." );
        throw( err );
    }
}

//  ----------------------------------------------------------------------------
void CWiggleRecord::ParseDeclarationVarstep(
    const vector<string>& data )
//
//  Note:   Once we make it in here, we know "data" starts with the obligatory
//          "variableStep" declaration.
//  ----------------------------------------------------------------------------
{
    Reset();

    vector<string>::const_iterator it = data.begin();
    for ( ++it; it != data.end(); ++it ) {
        vector<string> key_value_pair;
        CWiggleReader::Tokenize( *it, "=", key_value_pair );

        if ( key_value_pair.size() != 2 ) {
            CObjReaderLineException err( 
                eDiag_Error,
                0,
                "Invalid VarStep declaration --- only key value pairs allowed." );
            throw( err );
        }
        if ( key_value_pair[0] == "chrom" ) {
            m_strChrom = key_value_pair[1];
            continue;
        }
        if ( key_value_pair[0] == "span" ) {
            m_uSeqSpan = NStr::StringToUInt( key_value_pair[1] );
            continue;
        }
        CObjReaderLineException err( 
            eDiag_Error,
            0,
            "Invalid VarStep declaration --- only \"chrom\" and \"span\" are "
            "permissible as keys." );
        throw( err );
    }
}

//  ----------------------------------------------------------------------------
void CWiggleRecord::ParseDeclarationFixedstep(
    const vector<string>& data )
//  ----------------------------------------------------------------------------
{
    Reset();

    vector<string>::const_iterator it = data.begin();
    for ( ++it; it != data.end(); ++it ) {
        vector<string> key_value_pair;
        CWiggleReader::Tokenize( *it, "=", key_value_pair );

        if ( key_value_pair.size() != 2 ) {
            CObjReaderLineException err( 
                eDiag_Error,
                0,
                "Invalid FixedStep declaration --- only key value pairs allowed." );
            throw( err );
        }
        if ( key_value_pair[0] == "chrom" ) {
            m_strChrom = key_value_pair[1];
            continue;
        }
        if ( key_value_pair[0] == "span" ) {
            m_uSeqSpan = NStr::StringToUInt( key_value_pair[1] );
            continue;
        }
        if ( key_value_pair[0] == "start" ) {
            m_uSeqStart = NStr::StringToUInt( key_value_pair[1] );
            continue;
        }
        if ( key_value_pair[0] == "step" ) {
            m_uSeqStep = NStr::StringToUInt( key_value_pair[1] );
            continue;
        }
        CObjReaderLineException err( 
            eDiag_Error,
            0,
            "Invalid VarStep declaration --- only \"chrom\", \"span\", \"start\" "
            "and \"step\" are permissible as keys." );
        throw( err );
    }
    m_uSeqStart -= m_uSeqStep;
}

//  ===========================================================================
CWiggleTrack::CWiggleTrack(
    const CWiggleRecord& record ):
//  ===========================================================================
    m_strName( record.Name() ),
    m_strChrom( record.Chrom() ),
    m_uGraphType( GRAPH_UNKNOWN ),
    m_uSeqLength( 0 ),
    m_bEvenlySpaced( true )
{
    m_Data.push_back(CWiggleData(record.SeqStart(),
                                 record.SeqSpan(),
                                 record.Value()));
    m_uSeqSpan = record.SeqSpan();
    m_dMaxValue = (record.Value() > 0) ? record.Value() : 0;
    m_dMinValue = (record.Value() < 0) ? record.Value() : 0;

    if ( m_uSeqLength == 0 ) {
        m_uSeqStart = record.SeqStart();
        m_uSeqStop = record.SeqStart() + record.SeqSpan();
    }
    m_dMaxValue = (record.Value() > 0) ? record.Value() : 0;
    m_dMinValue = (record.Value() < 0) ? record.Value() : 0;
    
};

//  ===========================================================================
CWiggleTrack::~CWiggleTrack()
//  ===========================================================================
{
}

//  ===========================================================================
void CWiggleTrack::AddRecord(
    const CWiggleRecord& record )
//  ===========================================================================
{
    if ( m_strChrom != record.Chrom() ) {
        CObjReaderLineException err( 
            eDiag_Warning,
            0,
            "Data record with wrong chromosome: rejected." );
        throw( err );
    }
    if ( SeqSpan() != record.SeqSpan() ) {
        m_bEvenlySpaced = false;
    }
    if ( 0 != (record.SeqStart() - m_uSeqStart) % SeqSpan() ) {
        m_bEvenlySpaced = false;
    }
    m_Data.push_back(CWiggleData(record.SeqStart(),
                                 record.SeqSpan(),
                                 record.Value()));
    
    if ( m_uSeqLength == 0 ) {
        if ( m_uSeqStart > record.SeqStart() ) {
            m_uSeqStart = record.SeqStart();
        }
        if ( m_uSeqStop < record.SeqStart() + record.SeqSpan() ) {
            m_uSeqStop = record.SeqStart() + record.SeqSpan();
        }
    }
    if ( m_dMaxValue < record.Value() ) {
        m_dMaxValue = record.Value();
    }
    if ( record.Value() < m_dMinValue ) {
        m_dMinValue = record.Value();
    }
};

//  ===========================================================================
CWiggleSet::CWiggleSet()
//  ===========================================================================
{
};

//  ===========================================================================
unsigned int CWiggleTrack::SeqStart() const
//  ===========================================================================
{
    if ( m_uSeqLength == 0 ) { 
        return m_uSeqStart;
    }
    return 0;
};

//  ===========================================================================
unsigned int CWiggleTrack::SeqStop() const
//  ===========================================================================
{
    if ( m_uSeqLength == 0 ) { 
        return m_uSeqStop-1;
    }
    return m_uSeqLength-1;
};

//  ===========================================================================
bool CWiggleSet::AddRecord(
    const CWiggleRecord& record )
//  ===========================================================================
{
    CWiggleTrack* pTrack = 0;
    if ( FindTrack( record.Chrom(), pTrack ) ) {
        pTrack->AddRecord( record );
    }
    else {
        m_Tracks[ record.Chrom() ] = new CWiggleTrack( record );
    }
    return true;
};

//  ===========================================================================
bool CWiggleSet::FindTrack(
    const string& key,
    CWiggleTrack*& pTrack )
//  ===========================================================================
{
    for ( TrackIter it=m_Tracks.begin(); it!=m_Tracks.end(); ++it ) {
        if ( it->second->Chrom() == key ) {
            pTrack = it->second;
            return true;
        }
    }
    return false;
}

//  ===========================================================================
void CWiggleSet::Dump(
    CNcbiOstream& Out )
//  ===========================================================================
{
    for ( TrackIter it = m_Tracks.begin(); it != m_Tracks.end(); ++it ) {
        it->second->Dump( Out );
    }
}

//  ===========================================================================
void CWiggleSet::MakeGraph(
    CSeq_annot::TData::TGraph& graphset )
//  ===========================================================================
{
    for ( TrackIter it = m_Tracks.begin(); it != m_Tracks.end(); ++it ) {
        it->second->MakeGraph( graphset );
    }       
}

//  ===========================================================================
void CWiggleSet::MakeTable(
    CSeq_table& table,
    bool bJoinSame,
    bool bAsByte )
//  ===========================================================================
{
    table.SetFeat_type(0);
    for ( TrackIter it = m_Tracks.begin(); it != m_Tracks.end(); ++it ) {
        it->second->MakeTable( table, bJoinSame, bAsByte );
    }     
}

//  ===========================================================================
void CWiggleSet::DumpStats(
    CNcbiOstream& out )
//  ===========================================================================
{
    out << "---------------------------------------------------------" << endl;
    out << "Record Counts:" << endl;
    out << "---------------------------------------------------------" << endl;
    for ( TrackIter it = m_Tracks.begin(); it != m_Tracks.end(); ++it ) {
        out << it->first << " :    " << it->second->Count() << endl;
    } 
    out << endl;      
}

//  ===========================================================================
void CWiggleTrack::MakeTable(
    CSeq_table& table,
    bool bJoinSame,
    bool bAsByte )
//  ===========================================================================
{
    size_t uSize( Count() );
    
    { // Seq-id
        CRef<CSeqTable_column> col_id(new CSeqTable_column);
        table.SetColumns().push_back(col_id);
        col_id->SetHeader().SetField_id(CSeqTable_column_info::eField_id_location_id);
        col_id->SetDefault().SetId().Set(CSeq_id::e_Local, m_strChrom);
    }
    
    // position
    CRef<CSeqTable_column> col_pos(new CSeqTable_column);
    table.SetColumns().push_back(col_pos);
    col_pos->SetHeader().SetField_id(CSeqTable_column_info::eField_id_location_from);
    CSeqTable_multi_data::TInt& pos = col_pos->SetData().SetInt();
    
    TSeqPos span = 1;
    double min = 0, max = 0, step = 1;
    bool fixed_span = true, sorted = true;
    { // analyze
        if ( uSize ) {
            span = m_Data[0].SeqSpan();
            min = max = m_Data[0].Value();
        }
        for ( size_t i = 1; i < uSize; ++i ) {
            if ( m_Data[i].SeqSpan() != span ) {
                fixed_span = false;
            }
            if ( m_Data[i].SeqStart() < m_Data[i-1].SeqStart() ) {
                sorted = false;
            }
            double v = m_Data[i].Value();
            if ( v < min ) {
                min = v;
            }
            if ( v > max ) {
                max = v;
            }
        }
        if ( max > min ) {
            step = (max-min)/255;
        }
    }

    if ( !sorted ) {
        sort(m_Data.begin(), m_Data.end());
    }

    if ( bJoinSame && uSize ) {
        DataVector nv;
        nv.reserve(uSize);
        nv.push_back(m_Data[0]);
        for ( size_t i = 1; i < uSize; ++i ) {
            if ( m_Data[i].SeqStart() == nv.back().SeqStart()+nv.back().SeqSpan() &&
                 m_Data[i].Value() == nv.back().Value() ) {
                nv.back().m_uSeqSpan += m_Data[i].SeqSpan();
            }
            else {
                nv.push_back(m_Data[i]);
            }
        }
        if ( nv.size() != uSize ) {
            double s = EstimateSize(uSize, fixed_span, bAsByte);
            double ns = EstimateSize(nv.size(), false, bAsByte);
            if ( ns < s*.75 ) {
                m_Data.swap(nv);
                uSize = m_Data.size();
                LOG_POST("Joined size: "<<uSize);
                fixed_span = false;
            }
        }
    }
    table.SetNum_rows(uSize);
    pos.reserve(uSize);

    CSeqTable_multi_data::TInt* span_ptr = 0;
    { // span
        CRef<CSeqTable_column> col_span(new CSeqTable_column);
        table.SetColumns().push_back(col_span);
        col_span->SetHeader().SetField_name("span");
        if ( fixed_span ) {
            col_span->SetDefault().SetInt(span);
        }
        else {
            span_ptr = &col_span->SetData().SetInt();
            span_ptr->reserve(uSize);
        }
    }

    if ( bAsByte ) { // values

        CRef<CSeqTable_column> col_min(new CSeqTable_column);
        table.SetColumns().push_back(col_min);
        col_min->SetHeader().SetField_name("value_min");
        col_min->SetDefault().SetReal(min);

        CRef<CSeqTable_column> col_step(new CSeqTable_column);
        table.SetColumns().push_back(col_step);
        col_step->SetHeader().SetField_name("value_step");
        col_step->SetDefault().SetReal(step);

        CRef<CSeqTable_column> col_val(new CSeqTable_column);
        table.SetColumns().push_back(col_val);
        col_val->SetHeader().SetField_name("values");
        
        double mul = 1/step;
        AutoPtr< vector<char> > values(new vector<char>());
        values->reserve(uSize);
        ITERATE ( DataVector, it, m_Data ) {
            pos.push_back(it->SeqStart());
            if ( span_ptr ) {
                span_ptr->push_back(it->SeqSpan());
            }
            int val = int(ROUND((it->Value()-min)*mul));
            values->push_back(val);
        }
        col_val->SetData().SetBytes().push_back(values.release());
    }
    else {
        CRef<CSeqTable_column> col_val(new CSeqTable_column);
        table.SetColumns().push_back(col_val);
        col_val->SetHeader().SetField_name("values");
        CSeqTable_multi_data::TReal& values = col_val->SetData().SetReal();
        values.reserve(uSize);
        
        ITERATE ( DataVector, it, m_Data ) {
            pos.push_back(it->SeqStart());
            if ( span_ptr ) {
                span_ptr->push_back(it->SeqSpan());
            }
            values.push_back(it->Value());
        }
    }
}

//  ===========================================================================
double CWiggleTrack::EstimateSize(
    size_t rows, 
    bool fixed_span,
    bool bAsByte) const
//  ===========================================================================
{
    double ret = 0;
    ret += rows*4;
    if ( !fixed_span )
        ret += rows*4;
    if ( bAsByte )
        ret += rows;
    else
        ret += 8*rows;
    return ret;
}


//  ===========================================================================
void CWiggleTrack::MakeGraph(
    CSeq_annot::TData::TGraph& graphset )
//  ===========================================================================
{
    sort(m_Data.begin(), m_Data.end());
    if ( m_bEvenlySpaced ) {
        CRef<CSeq_graph> graph( new CSeq_graph );
        graph->SetTitle( m_strName );
        
        CSeq_interval& loc = graph->SetLoc().SetInt();
        loc.SetId().Set( CSeq_id::e_Local, m_strChrom );
        loc.SetFrom( SeqStart() );
        loc.SetTo( SeqStop() );
            
        graph->SetComp( SeqSpan() );
        graph->SetNumval( (SeqStop() - SeqStart() + 1) / SeqSpan() );
        graph->SetA( ScaleLinear() );
        graph->SetB( ScaleConst() );
        
        switch( GetGraphType() ) {
                
            default:
                FillGraphsByte( graph->SetGraph().SetByte() );
                break;
        
            case GRAPH_REAL:
                FillGraphsReal( graph->SetGraph().SetReal() );
                break;
                
            case GRAPH_INT:
                FillGraphsInt( graph->SetGraph().SetInt() );
                break;
        }
                
        graphset.push_back( graph );
    }
    else {
        for ( unsigned int u=0; u < m_Data.size(); ++u ) {
            CRef<CSeq_graph> graph( new CSeq_graph );
            graph->SetTitle( m_strName );
            
            switch( GetGraphType() ) {
                    
                default:
                    m_Data[u].FillGraphsByte( *graph, *this );
                    break;
            
                case GRAPH_REAL:
                    m_Data[u].FillGraphsReal( *graph );
                    break;
                    
                case GRAPH_INT:
                    m_Data[u].FillGraphsInt( *graph );
                    break;
            }                
            graphset.push_back( graph );
        }
    }
}

//  ===========================================================================
void CWiggleTrack::MakeGraphs(
    CSeq_annot::TData::TGraph& graphset )
//  ===========================================================================
{
    for ( unsigned int u=0; u < m_Data.size(); ++u ) {
        CRef<CSeq_graph> graph( new CSeq_graph );
        graph->SetTitle( m_strName );
        
        switch( GetGraphType() ) {
                
            default:
//                FillGraphsByte( graph->SetGraph().SetByte() );
                m_Data[u].FillGraphsByte( *graph, *this );
                break;
        
            case GRAPH_REAL:
//                m_Data[u]->FillGraphsReal( graph->SetGraph().SetReal() );
                m_Data[u].FillGraphsReal( *graph );
                break;
                
            case GRAPH_INT:
//                m_Data[u]->FillGraphsInt( graph->SetGraph().SetInt() );
                m_Data[u].FillGraphsInt( *graph );
                break;
        }                
        graphset.push_back( graph );
    }
}

//  ===========================================================================
void CWiggleData::FillGraphsReal(
    CSeq_graph& graph )
//  ===========================================================================
{
}


//  ===========================================================================
void CWiggleTrack::FillGraphsReal(
    CReal_graph& graph )
//  ===========================================================================
{
    unsigned int uDataSize = (SeqStop() - SeqStart() + 1) / SeqSpan();
    vector<double> values( uDataSize, 0 );
    for ( unsigned int u = 0; u < uDataSize; ++u ) {
    
        //  *******************************************************************
        //  Note:
        //  This code does not properly distiguish between missing values and 
        //  values being ==0. Need to come up with a convention if we ever
        //  commit to supporting float graph data.
        //  The byte graph convention does not quite carry over.
        //  ******************************************************************* 
        double dRaw( 0 );
        if ( DataValue( SeqStart() + u * SeqSpan(), dRaw ) ) {
            values[ u ] = dRaw;
        }
        else {
            values[ u ] = 0;
        } 
    }
    graph.SetMin( m_dMinValue );
    graph.SetMax( m_dMaxValue );
    graph.SetAxis( 0 );
    graph.SetValues() = values;
}

//  ===========================================================================
void CWiggleTrack::FillGraphsInt(
    CInt_graph& graph )
//  ===========================================================================
{
    /* to do --- if we ever have a need for this */
}

//  ===========================================================================
void CWiggleData::FillGraphsInt(
    CSeq_graph& graph )
//  ===========================================================================
{
}

//  ===========================================================================
void CWiggleTrack::FillGraphsByte(
    CByte_graph& graph )
//
//  Idea:   Scale the set of values found linearly to the interval 1 (lowest)
//          to 255 (highest). Gap "values" are set to 0.
//  ===========================================================================
{
    graph.SetMin( 0 );         // the interval we are scaling the y-values
    graph.SetMax( 255 );       //   into...
    graph.SetAxis( 0 );
    
    unsigned int uDataSize = (SeqStop() - SeqStart() + 1) / SeqSpan();
    vector<char> values( uDataSize, 0 );
    for ( unsigned int u = 0; u < uDataSize; ++u ) {
        values[ u ] = ByteGraphValue( SeqStart() + u * SeqSpan() );
    }
    graph.SetValues() = values;
}

//  ===========================================================================
void CWiggleData::FillGraphsByte(
    CSeq_graph& graph,
    const CWiggleTrack& track)
//
//  Idea:   Scale the set of values found linearly to the interval 1 (lowest)
//          to 255 (highest). Gap "values" are set to 0.
//  ===========================================================================
{
    CSeq_interval& loc = graph.SetLoc().SetInt();
    loc.SetId().Set( CSeq_id::e_Local, track.Chrom() );
    loc.SetFrom( SeqStart() );
    loc.SetTo( SeqStart() + SeqSpan() );

    graph.SetComp( SeqSpan() );
    graph.SetNumval( 1 );
    graph.SetA( 0 );
    graph.SetB( Value() );
        
    CByte_graph& bytes = graph.SetGraph().SetByte();
    bytes.SetMin( 0 );
    bytes.SetMax( 1 );
    bytes.SetAxis( 0 );
    vector<char> values( 1, 1 );
    bytes.SetValues() = values;
}

//  ===========================================================================
bool CWiggleTrack::DataValue(
    unsigned int uStart,
    double& dValue )
//  ===========================================================================
{
    if ( GRAPH_UNKNOWN == m_uGraphType ) {
        m_uGraphType = GetGraphType();
    }
    CWiggleData key(uStart, 0, 0);
    DataIter it = lower_bound(m_Data.begin(), m_Data.end(), key);
    if ( it == m_Data.end() || it->SeqStart() != uStart ) {
        return false;
    }
    dValue = it->Value();
    return true;
}

//  ===========================================================================
unsigned char CWiggleTrack::ByteGraphValue(
    unsigned int uStart )
//  ===========================================================================
{
    double dRaw( 0 );
    if ( ! DataValue( uStart, dRaw ) ) {
        // return 0 as the default value
        return static_cast<unsigned char>( 0 );
    }
    else {
        // scale into interval [0,255]
        if ( m_dMinValue == m_dMaxValue ) {
            return static_cast<unsigned char>( (dRaw ? 255 : 0) );
        }
        double dScaled =
            ( 255 * (dRaw - m_dMinValue) / (m_dMaxValue - m_dMinValue) );
        return static_cast<unsigned char>( dScaled + 0.5 );
    }
    
    
}

//  ===========================================================================
double CWiggleTrack::ScaleConst() const
//  ===========================================================================
{
    return m_dMinValue;
}

//  ===========================================================================
double CWiggleTrack::ScaleLinear() const
//  ===========================================================================
{
    return (m_dMaxValue - m_dMinValue) / 255;
}
    
//  ===========================================================================
unsigned int CWiggleTrack::GetGraphType()
//  ===========================================================================
{
    if ( m_uGraphType != GRAPH_UNKNOWN ) {
        return m_uGraphType;
    }
    m_uGraphType = GRAPH_BYTE;
    return m_uGraphType;
} 

//  ===========================================================================
void CWiggleTrack::Dump(
    CNcbiOstream& Out )
//  ===========================================================================
{
    Out << "track chrom=" << Chrom() << " seqstart=" << SeqStart()
        << " seqstop=" << SeqStop() << " count=" << Count() << endl;
    for (DataIter it = m_Data.begin(); it != m_Data.end(); ++it ) {
        it->Dump( Out );
    }
    Out << endl;
}

//  ===========================================================================
void CWiggleData::Dump(
    CNcbiOstream& Out )
//  ===========================================================================
{
    Out << "  data start=" << SeqStart() << " value=" << Value() << endl;
}

END_objects_SCOPE
END_NCBI_SCOPE

