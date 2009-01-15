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
#include <objtools/readers/reader_exception.hpp>

#include <objtools/readers/reader_base.hpp>
#include <objtools/readers/wiggle_reader.hpp>
#include <objtools/idmapper/ucscid.hpp>
#include <objtools/idmapper/idmapper.hpp>

#include "wiggle_data.hpp"

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ===========================================================================
CWiggleData::CWiggleData(
    unsigned int seq_start,
    double value ):
//  ===========================================================================
    m_uSeqStart( seq_start ),
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
    m_strChrom = "";
    m_uSeqStart = 0;
    m_uSeqStep = 0;
    m_uSeqSpan = 0;
    m_dValue = 0;
}

//  ----------------------------------------------------------------------------
bool CWiggleRecord::ParseTrackDefinition(
    const vector<string>& data,
    unsigned int uMode )
//  ----------------------------------------------------------------------------
{
    Reset();
    if ( data.size() < 2 || data[0] != "track" ) {
        return false;
    }
    vector<string>::const_iterator it = data.begin();
    for ( ++it; it != data.end(); ++it ) {
        string strKey;
        string strValue;
        if ( ! NStr::SplitInTwo( *it, "=", strKey, strValue ) ) {
            return false;
        }
        NStr::ReplaceInPlace( strValue, "\"", "" );
        if ( strKey == "name" ) {
            if ( uMode == IDMODE_CHROM ) {
                m_strId = "";
            }
            else {
                m_strId = strValue;
            }
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CWiggleRecord::ParseDataBed(
    const vector<string>& data,
    unsigned int )
//  ----------------------------------------------------------------------------
{
    if ( data.size() != 4 ) {
        return false;
    }
    m_strChrom = data[0];
    m_uSeqStart = NStr::StringToUInt( data[1] );
    m_uSeqSpan = NStr::StringToUInt( data[2] ) - m_uSeqStart;
    m_dValue = NStr::StringToDouble( data[3] );
//    m_type = TYPE_DATA_BED;
    return true;
}

//  ----------------------------------------------------------------------------
bool CWiggleRecord::ParseDataVarstep(
    const vector<string>& data,
    unsigned int )
//  ----------------------------------------------------------------------------
{
    m_uSeqStart = NStr::StringToUInt( data[0] ) - 1; // varStep is 1- based
    m_dValue = NStr::StringToDouble( data[1] );
    return true;
}

//  ----------------------------------------------------------------------------
bool CWiggleRecord::ParseDataFixedstep(
    const vector<string>& data,
    unsigned int )
//  ----------------------------------------------------------------------------
{
    m_uSeqStart += m_uSeqStep;
    m_dValue = NStr::StringToDouble( data[0] );
    return true;
}

//  ----------------------------------------------------------------------------
bool CWiggleRecord::ParseDeclarationVarstep(
    const vector<string>& data,
    unsigned int )
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
            return false;
        }
        if ( key_value_pair[0] == "chrom" ) {
            m_strChrom = key_value_pair[1];
            continue;
        }
        if ( key_value_pair[0] == "span" ) {
            m_uSeqSpan = NStr::StringToUInt( key_value_pair[1] );
            continue;
        }
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CWiggleRecord::ParseDeclarationFixedstep(
    const vector<string>& data,
    unsigned int )
//  ----------------------------------------------------------------------------
{
    Reset();

    vector<string>::const_iterator it = data.begin();
    for ( ++it; it != data.end(); ++it ) {
        vector<string> key_value_pair;
        CWiggleReader::Tokenize( *it, "=", key_value_pair );

        if ( key_value_pair.size() != 2 ) {
            return false;
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
            m_uSeqStart = NStr::StringToUInt( key_value_pair[1] ) - 1;
            continue;
        }
        if ( key_value_pair[0] == "step" ) {
            m_uSeqStep = NStr::StringToUInt( key_value_pair[1] );
            continue;
        }
        return false;
    }
    m_uSeqStart -= m_uSeqStep;
    return true;
}

//  ===========================================================================
CWiggleTrack::CWiggleTrack(
    const CWiggleRecord& record,
    CIdMapper* pMapper ):
//  ===========================================================================
    m_strChrom( record.Chrom() ),
    m_uGraphType( GRAPH_UNKNOWN )
{
    CWiggleData* pData = new CWiggleData( record.SeqStart(), record.Value() );
    m_Entries[ pData->SeqStart() ] = pData;
    m_uSeqSpan = record.SeqSpan();
    m_dMaxValue = (record.Value() > 0) ? record.Value() : 0;
    m_dMinValue = (record.Value() < 0) ? record.Value() : 0;

    m_MappedID = pMapper->MapID( UcscID::Label( record.Id(), record.Chrom() ), 
        m_uSeqLength );
    
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
    for ( DataIter it = m_Entries.begin(); it != m_Entries.end(); ++it ) {
        delete it->second;
    }
}

//  ===========================================================================
bool CWiggleTrack::AddRecord(
    const CWiggleRecord& record )
//  ===========================================================================
{
    if ( m_strChrom != record.Chrom() ) {
        return false;
    }
    if ( SeqSpan() != record.SeqSpan() ) {
//        NCBI_THROW2( CObjReaderParseException, eFormat,
//            string( "Data of multiple spans within a single track" ), 0 );
    }
    unsigned int iResidue = (record.SeqStart() - m_uSeqStart) % SeqSpan();
    if ( 0 != (record.SeqStart() - m_uSeqStart) % SeqSpan() ) {
//        NCBI_THROW2( CObjReaderParseException, eFormat,
//            string( "Data records within track not aligned at span multiples" ), 
//            0 );
    }
    CWiggleData* pData = new CWiggleData( record.SeqStart(), record.Value() );        
    m_Entries[ record.SeqStart() ] = pData;
    
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
    return true;
};

//  ===========================================================================
CWiggleSet::CWiggleSet(
    CIdMapper* pMapper ):
//  ===========================================================================
    m_pMapper( pMapper )
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
        m_Tracks[ record.Chrom() ] = new CWiggleTrack( record, m_pMapper );
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
bool CWiggleSet::MakeGraph(
    CSeq_annot::TData::TGraph& graphset )
//  ===========================================================================
{
    for ( TrackIter it = m_Tracks.begin(); it != m_Tracks.end(); ++it ) {
        it->second->MakeGraph( graphset );
    }       
    return true;
}

//  ===========================================================================
bool CWiggleTrack::MakeGraph(
    CSeq_annot::TData::TGraph& graphset )
//  ===========================================================================
{
    CRef<CSeq_graph> graph( new CSeq_graph );

    CSeq_interval& loc = graph->SetLoc().SetInt();
    loc.SetId().Assign( *m_MappedID.GetSeqId() );
    loc.SetFrom( SeqStart() );
    loc.SetTo( SeqStop() );
        
    graph->SetComp( SeqSpan() );
    graph->SetNumval( (SeqStop() - SeqStart() + 1) / SeqSpan() );
    graph->SetA( ScaleLinear() );
    graph->SetB( ScaleConst() );
    
    switch( GetGraphType() ) {
            
        default:
            FillGraphByte( graph->SetGraph().SetByte() );
            break;
    
        case GRAPH_REAL:
            FillGraphReal( graph->SetGraph().SetReal() );
            break;
            
        case GRAPH_INT:
            FillGraphInt( graph->SetGraph().SetInt() );
            break;
    }
            
    graphset.push_back( graph );
    return true;
}

//  ===========================================================================
bool CWiggleTrack::FillGraphReal(
    CReal_graph& graph )
//  ===========================================================================
{
    unsigned int uDataSize = (SeqStop() - SeqStart() + 1) / SeqSpan();
    vector<double> values( uDataSize, 0 );
    for ( unsigned int u = 0; u < uDataSize; ++u ) {
        values[ u ] = DataValue( SeqStart() + u * SeqSpan() ); 
    }
    graph.SetMin( m_dMinValue );
    graph.SetMax( m_dMaxValue );
    graph.SetAxis( 0 );
    graph.SetValues() = values;
    return true;
}

//  ===========================================================================
bool CWiggleTrack::FillGraphInt(
    CInt_graph& graph )
//  ===========================================================================
{
    return true;
}

//  ===========================================================================
bool CWiggleTrack::FillGraphByte(
    CByte_graph& graph )
//
//  Idea:   Scale the set of values found linearly to the intercal 1 (lowest)
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
    return true;
}

//  ===========================================================================
double CWiggleTrack::DataValue(
    unsigned int uStart )
//  ===========================================================================
{
    if ( GRAPH_UNKNOWN == m_uGraphType ) {
        m_uGraphType = GetGraphType();
    }
    DataIter it = m_Entries.find( uStart );
    if ( it == m_Entries.end() ) {
        return 0;
    }
    return it->second->Value();
}

//  ===========================================================================
unsigned char CWiggleTrack::ByteGraphValue(
    unsigned int uStart )
//  ===========================================================================
{
    double dRawValue = DataValue( uStart );
    if ( m_dMinValue == m_dMaxValue ) {
        return (dRawValue ? 255 : 0);
    }
    
    unsigned char dByteValue = (unsigned char)
        ( 255 * (dRawValue - m_dMinValue) / (m_dMaxValue - m_dMinValue) );
    return dByteValue;
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
    return (m_dMaxValue - m_dMinValue) / 256;
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
    char cBuffer[ 256 ];
    sprintf( cBuffer, 
        "track chrom=%s seqstart=%d seqstop=%d count=%d", 
        Chrom(), SeqStart(), SeqStop(), Count() );
    Out << cBuffer << endl;
    for (DataIter it = m_Entries.begin(); it != m_Entries.end(); ++it ) {
        it->second->Dump( Out );
    }
    Out << endl;
}

//  ===========================================================================
void CWiggleData::Dump(
    CNcbiOstream& Out )
//  ===========================================================================
{
    char cBuffer[ 80 ];
    sprintf( cBuffer, "  data start=%d value=%f", SeqStart(), Value() );
    Out << cBuffer << endl;
}

END_objects_SCOPE
END_NCBI_SCOPE

