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
 *   GFF file reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>

#include <objtools/readers/gff3_data.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
CGff3Record::CGff3Record():
    m_uSeqStart( 0 ),
    m_uSeqStop( 0 ),
    m_pdScore( 0 ),
    m_peStrand( 0 ),
    m_pePhase( 0 )
//  ----------------------------------------------------------------------------
{};

//  ----------------------------------------------------------------------------
CGff3Record::~CGff3Record()
//  ----------------------------------------------------------------------------
{
    delete m_pdScore;
    delete m_peStrand;
    delete m_pePhase; 
};

//  ----------------------------------------------------------------------------
bool CGff3Record::AssignFromGff(
    const string& strRawInput )
//  ----------------------------------------------------------------------------
{
    vector< string > columns;
    NStr::Tokenize( strRawInput, " \t", columns, NStr::eMergeDelims );

    if ( columns.size() > 9 ) {
        // did these bozos sprinkled blanks between the attributes?
        NStr::TruncateSpacesInPlace( columns[8] );
        for ( unsigned int u = 9; u < columns.size(); ++u ) {
            columns[8] += NStr::TruncateSpaces( columns[u] );
        }
    }

    //  to do: sanity checks

    m_strId = columns[0];
    m_strSource = columns[1];
    m_strType = columns[2];
    m_uSeqStart = NStr::StringToUInt( columns[3] ) - 1;
    m_uSeqStop = NStr::StringToUInt( columns[4] ) - 1;

    if ( columns[5] != "." ) {
        m_pdScore = new double( NStr::StringToDouble( columns[5] ) );
    }

    if ( columns[6] == "+" ) {
        m_peStrand = new ENa_strand( eNa_strand_plus );
    }
    if ( columns[6] == "-" ) {
        m_peStrand = new ENa_strand( eNa_strand_minus );
    }
    if ( columns[6] == "?" ) {
        m_peStrand = new ENa_strand( eNa_strand_unknown );
    }

    if ( columns[7] == "0" ) {
        m_pePhase = new TFrame( CCdregion::eFrame_one );
    }
    if ( columns[7] == "1" ) {
        m_pePhase = new TFrame( CCdregion::eFrame_two );
    }
    if ( columns[7] == "2" ) {
        m_pePhase = new TFrame( CCdregion::eFrame_three );
    }

    m_strAttributes = columns[8];
    
    return x_AssignAttributesFromGff( m_strAttributes );
}

//  ----------------------------------------------------------------------------
bool CGff3Record::GetAttribute(
    const string& strKey,
    string& strValue ) const
//  ----------------------------------------------------------------------------
{
    TAttrCit it = m_Attributes.find( strKey );
    if ( it == m_Attributes.end() ) {
        return false;
    }
    strValue = it->second;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Record::MergeRecord(
    const CGff3Record& other )
//  ----------------------------------------------------------------------------
{
    const TAttributes& newAttrs = other.Attributes(); 
    for ( TAttrCit cit  = newAttrs.begin(); cit != newAttrs.end(); ++cit ) {
        if ( cit->first == "gff_score" ) {
            delete m_pdScore;
            m_pdScore = new double( NStr::StringToDouble( cit->second ) );
            continue;
        }
        m_Attributes[ cit->first ] = cit->second;
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Record::x_AssignAttributesFromGff(
    const string& strRawAttributes )
//  ----------------------------------------------------------------------------
{
    vector< string > attributes;
    NStr::Tokenize( strRawAttributes, ";", attributes, NStr::eMergeDelims );
    for ( size_t u=0; u < attributes.size(); ++u ) {
        string strKey;
        string strValue;
        if ( ! NStr::SplitInTwo( attributes[u], "=", strKey, strValue ) ) {
            return false;
        }
        if ( NStr::StartsWith( strValue, "\"" ) ) {
            strValue = strValue.substr( 1, string::npos );
        }
        if ( NStr::EndsWith( strValue, "\"" ) ) {
            strValue = strValue.substr( 0, strValue.length() - 1 );
        }

        //
        //  Fix the worst of capitalization attrocities:
        //
        if ( 0 == NStr::CompareNocase( strKey, "ID" ) ) {
            strKey = "ID";
        }
        if ( 0 == NStr::CompareNocase( strKey, "Name" ) ) {
            strKey = "Name";
        }
        if ( 0 == NStr::CompareNocase( strKey, "Alias" ) ) {
            strKey = "Alias";
        }
        if ( 0 == NStr::CompareNocase( strKey, "Parent" ) ) {
            strKey = "Parent";
        }
        if ( 0 == NStr::CompareNocase( strKey, "Target" ) ) {
            strKey = "Target";
        }
        if ( 0 == NStr::CompareNocase( strKey, "Gap" ) ) {
            strKey = "Gap";
        }
        if ( 0 == NStr::CompareNocase( strKey, "Derives_from" ) ) {
            strKey = "Derives_from";
        }
        if ( 0 == NStr::CompareNocase( strKey, "Note" ) ) {
            strKey = "Note";
        }
        if ( 0 == NStr::CompareNocase( strKey, "Dbxref" ) ) {
            strKey = "Dbxref";
        }
        if ( 0 == NStr::CompareNocase( strKey, "Ontology_term" ) ) {
            strKey = "Ontology_term";
        }

        m_Attributes[ strKey ] = strValue;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Record::AssignFromAsn(
    const CSeq_annot& annot,
    const CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    if ( ! x_AssignTypeFromAsn( feature ) ) {
        return false;
    }
    if ( ! x_AssignSeqIdFromAsn( feature ) ) {
        return false;
    }
    if ( ! x_AssignSourceFromAsn( feature ) ) {
        return false;
    }
    if ( ! x_AssignStartFromAsn( feature ) ) {
        return false;
    }
    if ( ! x_AssignStopFromAsn( feature ) ) {
        return false;
    }
    if ( ! x_AssignScoreFromAsn( feature ) ) {
        return false;
    }
    if ( ! x_AssignStrandFromAsn( feature ) ) {
        return false;
    }
    if ( ! x_AssignPhaseFromAsn( feature ) ) {
        return false;
    }
    if ( ! x_AssignAttributesFromAsnCore( annot, feature ) ) {
        return false;
    }
    if ( ! x_AssignAttributesFromAsnExtended( feature ) ) {
        return false;
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Record::MakeExon(
    const CGff3Record& parent,
    const CSeq_interval& loc )
//  ----------------------------------------------------------------------------
{
    if ( ! loc.CanGetFrom() || ! loc.CanGetTo() ) {
        return false;
    }
    m_strId = parent.Id();
    m_strSource = parent.Source();
    m_strType = "exon";
    m_uSeqStart = loc.GetFrom();
    m_uSeqStop = loc.GetTo();
    if ( parent.IsSetScore() ) {
        m_pdScore = new double( parent.Score() );
    }
    if ( parent.IsSetStrand() ) {
        m_peStrand = new ENa_strand( parent.Strand() );
    }
    string strParentId;
    if ( parent.GetAttribute( "ID", strParentId ) ) {
        m_Attributes[ "Parent" ] = strParentId;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Record::x_AssignSeqIdFromAsn(
    const CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    m_strId = "<unknown>";

    if ( feature.CanGetLocation() ) {
        const CSeq_loc& location = feature.GetLocation();
        const CSeq_id* pId = location.GetId();
        switch ( pId->Which() ) {
            
            case CSeq_id::e_Local:
                if ( pId->GetLocal().IsId() ) {
                    m_strId = NStr::UIntToString( pId->GetLocal().GetId() );
                }
                else {
                    m_strId = pId->GetLocal().GetStr();
                }
                break;

            case CSeq_id::e_Gi:
                m_strId = NStr::IntToString( pId->GetGi() );
                break;

            case CSeq_id::e_Other:
                if ( pId->GetOther().CanGetAccession() ) {
                    m_strId = pId->GetOther().GetAccession();
                    if ( pId->GetOther().CanGetVersion() ) {
                        m_strId += ".";
                        m_strId += NStr::UIntToString( 
                            pId->GetOther().GetVersion() ); 
                    }
                }
                break;

            default:
                break;
        }
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Record::x_AssignTypeFromAsn(
    const CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    m_strType = "region";

    if ( feature.CanGetQual() ) {
        const vector< CRef< CGb_qual > >& quals = feature.GetQual();
        vector< CRef< CGb_qual > >::const_iterator it = quals.begin();
        while ( it != quals.end() ) {
            if ( (*it)->CanGetQual() && (*it)->CanGetVal() ) {
                if ( (*it)->GetQual() == "standard_name" ) {
                    m_strType = (*it)->GetVal();
                    return true;
                }
            }
            ++it;
        }
    }

    if ( ! feature.CanGetData() ) {
        return true;
    }

    switch ( feature.GetData().GetSubtype() ) {
    default:
        m_strType = feature.GetData().GetKey();
        break;

    case CSeq_feat::TData::eSubtype_gene:
        m_strType = "gene";
        break;

    case CSeq_feat::TData::eSubtype_cdregion:
        m_strType = "CDS";
        break;

    case CSeq_feat::TData::eSubtype_mRNA:
        m_strType = "mRNA";
        break;

    case CSeq_feat::TData::eSubtype_scRNA:
        m_strType = "scRNA";
        break;

    case CSeq_feat::TData::eSubtype_exon:
        m_strType = "exon";
        break;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Record::x_AssignStartFromAsn(
    const CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    if ( feature.CanGetLocation() ) {
        const CSeq_loc& location = feature.GetLocation();
        unsigned int uStart = location.GetStart( eExtreme_Positional );
        m_uSeqStart = uStart;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Record::x_AssignStopFromAsn(
    const CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    if ( feature.CanGetLocation() ) {
        const CSeq_loc& location = feature.GetLocation();
        unsigned int uEnd = location.GetStop( eExtreme_Positional );
        m_uSeqStop = uEnd;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Record::x_AssignSourceFromAsn(
    const CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    m_strSource = ".";

    if ( feature.CanGetQual() ) {
        const vector< CRef< CGb_qual > >& quals = feature.GetQual();
        vector< CRef< CGb_qual > >::const_iterator it = quals.begin();
        while ( it != quals.end() ) {
            if ( (*it)->CanGetQual() && (*it)->CanGetVal() ) {
                if ( (*it)->GetQual() == "gff_source" ) {
                    m_strSource = (*it)->GetVal();
                    return true;
                }
            }
            ++it;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Record::x_AssignScoreFromAsn(
    const CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    if ( feature.CanGetQual() ) {
        const vector< CRef< CGb_qual > >& quals = feature.GetQual();
        vector< CRef< CGb_qual > >::const_iterator it = quals.begin();
        while ( it != quals.end() ) {
            if ( (*it)->CanGetQual() && (*it)->CanGetVal() ) {
                if ( (*it)->GetQual() == "gff_score" ) {
                    m_pdScore = new double( NStr::StringToDouble( 
                        (*it)->GetVal() ) );
                    return true;
                }
            }
            ++it;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Record::x_AssignStrandFromAsn(
    const CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    if ( feature.CanGetLocation() ) {
        m_peStrand = new ENa_strand( feature.GetLocation().GetStrand() );
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Record::x_AssignPhaseFromAsn(
    const CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    if ( ! feature.CanGetData() ) {
        return true;
    }
    const CSeq_feat::TData& data = feature.GetData();
    if ( data.GetSubtype() != CSeq_feat::TData::eSubtype_cdregion ) {
        return true;
    }

    const CCdregion& cdr = data.GetCdregion();
    m_pePhase = new TFrame( cdr.GetFrame() );
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Record::x_AssignAttributesFromAsnCore(
    const CSeq_annot& annot,
    const CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    // If feature ids are present then they are likely used to show parent/child
    // relationships, via corresponding xrefs. Thus, any feature ids override
    // gb ID tags (feature ids and ID tags should agree in the first place, but
    // if not, feature ids must trump ID tags).
    //
    bool bIdAssigned = false;

    if ( feature.CanGetId() ) {
        const CSeq_feat::TId& id = feature.GetId();
        string value = CGff3Record::x_FeatIdString( id );
        m_Attributes[ "ID" ] = value;
        bIdAssigned = true;
    }

    if ( feature.CanGetXref() ) {
        const CSeq_feat::TXref& xref = feature.GetXref();
        string value;
        for ( size_t i=0; i < xref.size(); ++i ) {
            if ( xref[i]->CanGetId() /* && xref[i]->CanGetData() */ ) {
                const CSeqFeatXref::TId& id = xref[i]->GetId();
                CSeq_feat::TData::ESubtype other_type = 
                    CGff3Record::x_GetSubtypeOf( annot, id );
                if ( ! x_IsParentOf( other_type, feature.GetData().GetSubtype() ) ) {
                    continue;
                }
                if ( ! value.empty() ) {
                    value += ",";
                }
                value += CGff3Record::x_FeatIdString( id );
            }
        }
        if ( ! value.empty() ) {
            m_Attributes[ "Parent" ] = value;
        }
    }

    if ( feature.CanGetQual() ) {
        const vector< CRef< CGb_qual > >& quals = feature.GetQual();
        vector< CRef< CGb_qual > >::const_iterator it = quals.begin();
        for ( /*NOOP*/; it != quals.end(); ++it ) {
            if ( (*it)->CanGetQual() && (*it)->CanGetVal() ) {
                if ( (*it)->GetQual() == "ID" ) {
                    if ( ! bIdAssigned ) {
                        m_Attributes[ "ID" ] = (*it)->GetVal();
                    }
                    continue;
                }
                m_Attributes[ (*it)->GetQual() ] = (*it)->GetVal();
            }
        }
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Record::x_AssignAttributesFromAsnExtended(
    const CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    if ( feature.CanGetDbxref() ) {
        const CSeq_feat::TDbxref& dbxrefs = feature.GetDbxref();
        if ( dbxrefs.size() > 0 ) {
            string value;
            dbxrefs[0]->GetLabel( &value );
            for ( size_t i=1; i < dbxrefs.size(); ++i ) {
                string label;
                dbxrefs[i]->GetLabel( &label );
                value += ",";
                value += label;
            }
            m_Attributes[ "Dbxref" ] = value;
        }
    }
    if ( feature.CanGetComment() ) {
        m_Attributes[ "comment" ] = feature.GetComment();
    }
    return true;
}

//  ----------------------------------------------------------------------------
CSeq_feat::TData::ESubtype CGff3Record::x_GetSubtypeOf(
    const CSeq_annot& annot,
    const CFeat_id& id )
//  ----------------------------------------------------------------------------
{
    const list< CRef< CSeq_feat > >& table = annot.GetData().GetFtable();
    list< CRef< CSeq_feat > >::const_iterator it = table.begin();
    while ( it != table.end() ) {
        if ( (*it)->CanGetId() && (*it)->CanGetData() ) {
            if ( id.Equals( (*it)->GetId() ) ) {
                return (*it)->GetData().GetSubtype();
            }
        }
        ++it;
    }
    return CSeq_feat::TData::eSubtype_bad;
}

//  ----------------------------------------------------------------------------
bool CGff3Record::x_IsParentOf(
    CSeq_feat::TData::ESubtype maybe_parent,
    CSeq_feat::TData::ESubtype maybe_child )
//  ----------------------------------------------------------------------------
{
    switch ( maybe_parent ) {
    default:
        return false;

    case CSeq_feat::TData::eSubtype_10_signal:
    case CSeq_feat::TData::eSubtype_35_signal:
    case CSeq_feat::TData::eSubtype_3UTR:
    case CSeq_feat::TData::eSubtype_5UTR:
        return false;

    case CSeq_feat::TData::eSubtype_mRNA:
        switch ( maybe_child ) {
        default:
            return false;
        case CSeq_feat::TData::eSubtype_cdregion:
            return true;
        }

    case CSeq_feat::TData::eSubtype_operon:
        switch ( maybe_child ) {

        case CSeq_feat::TData::eSubtype_gene:
        case CSeq_feat::TData::eSubtype_promoter:
            return true;

        default:
            return x_IsParentOf( CSeq_feat::TData::eSubtype_gene, maybe_child ) ||
                x_IsParentOf( CSeq_feat::TData::eSubtype_promoter, maybe_child );
        }

    case CSeq_feat::TData::eSubtype_gene:
        switch ( maybe_child ) {

        case CSeq_feat::TData::eSubtype_intron:
        case CSeq_feat::TData::eSubtype_mRNA:
            return true;

        default:
            return x_IsParentOf( CSeq_feat::TData::eSubtype_intron, maybe_child ) ||
                x_IsParentOf( CSeq_feat::TData::eSubtype_mRNA, maybe_child );
        }

    case CSeq_feat::TData::eSubtype_cdregion:
        switch ( maybe_child ) {

        case CSeq_feat::TData::eSubtype_exon:
            return true;

        default:
            return x_IsParentOf( CSeq_feat::TData::eSubtype_exon, maybe_child );
        }
    }

    return false;
}

//  ----------------------------------------------------------------------------
string CGff3Record::x_FeatIdString(
    const CFeat_id& id )
//  ----------------------------------------------------------------------------
{
    switch ( id.Which() ) {
    default:
        break;

    case CFeat_id::e_Local: {
        const CFeat_id::TLocal& local = id.GetLocal();
        if ( local.IsId() ) {
            return NStr::IntToString( local.GetId() );
        }
        if ( local.IsStr() ) {
            return local.GetStr();
        }
        break;
        }
    }
    return "FEATID";
}


END_objects_SCOPE
END_NCBI_SCOPE
