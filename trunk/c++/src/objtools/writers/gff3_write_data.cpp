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

#include <objtools/writers/gff3_write_data.hpp>
#include <objmgr/util/seq_loc_util.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
string s_MakeGffDbtag( const CDbtag& dbtag )
//
//  Currently, simply produce "DB:TAG" (which is different from 
//    dbtag.GetLabel() ).
//  In the future, may have to convert between Genbank DB abbreviations and
//    GFF DB abbreviations.
//  ----------------------------------------------------------------------------
{
    string strGffTag;
    if ( dbtag.IsSetDb() ) {
        strGffTag += dbtag.GetDb();
        strGffTag += ":";
    }
    if ( dbtag.IsSetTag() ) {
        if ( dbtag.GetTag().IsId() ) {
            strGffTag += NStr::UIntToString( dbtag.GetTag().GetId() );
        }
        if ( dbtag.GetTag().IsStr() ) {
            strGffTag += dbtag.GetTag().GetStr();
        }
    }
    return strGffTag;
}
        
//  ----------------------------------------------------------------------------
CGff3WriteRecord::CGff3WriteRecord(
    CSeq_annot_Handle sah ):
    m_uSeqStart( 0 ),
    m_uSeqStop( 0 ),
    m_pdScore( 0 ),
    m_peStrand( 0 ),
    m_puPhase( 0 )
//  ----------------------------------------------------------------------------
{
    m_Sah = sah;
};

//  ----------------------------------------------------------------------------
CGff3WriteRecord::~CGff3WriteRecord()
//  ----------------------------------------------------------------------------
{
    delete m_pdScore;
    delete m_peStrand;
    delete m_puPhase; 
};

//  ----------------------------------------------------------------------------
bool CGff3WriteRecord::GetAttribute(
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
bool CGff3WriteRecord::MergeRecord(
    const CGff3WriteRecord& other )
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
bool CGff3WriteRecord::AssignFromAsn(
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
    if ( ! x_AssignAttributesFromAsnCore( feature ) ) {
        return false;
    }
    if ( ! x_AssignAttributesFromAsnExtended( feature ) ) {
        return false;
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecord::MakeExon(
    const CGff3WriteRecord& parent,
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
string CGff3WriteRecord::StrId() const
//  ----------------------------------------------------------------------------
{
    return Id();
}

//  ----------------------------------------------------------------------------
string CGff3WriteRecord::StrSource() const
//  ----------------------------------------------------------------------------
{
    return Source();
}

//  ----------------------------------------------------------------------------
string CGff3WriteRecord::StrType() const
//  ----------------------------------------------------------------------------
{
    string strGffType;
    if ( GetAttribute( "gff_type", strGffType ) ) {
        return strGffType;
    }
    return Type();
}

//  ----------------------------------------------------------------------------
string CGff3WriteRecord::StrSeqStart() const
//  ----------------------------------------------------------------------------
{
    return NStr::UIntToString( SeqStart() + 1 );;
}

//  ----------------------------------------------------------------------------
string CGff3WriteRecord::StrSeqStop() const
//  ----------------------------------------------------------------------------
{
    return NStr::UIntToString( SeqStop() + 1 );
}

//  ----------------------------------------------------------------------------
string CGff3WriteRecord::StrScore() const
//  ----------------------------------------------------------------------------
{
    if ( ! IsSetScore() ) {
        return ".";
    }

    //  can NStr format floating point numbers? Didn't see ...
    char pcBuffer[ 16 ];
    ::sprintf( pcBuffer, "%6.6f", Score() );
    return string( pcBuffer );
}

//  ----------------------------------------------------------------------------
string CGff3WriteRecord::StrStrand() const
//  ----------------------------------------------------------------------------
{
    if ( ! IsSetStrand() ) {
        return ".";
    }
    switch ( Strand() ) {
    default:
        return ".";
    case eNa_strand_plus:
        return "+";
    case eNa_strand_minus:
        return "-";
    }
}

//  ----------------------------------------------------------------------------
string CGff3WriteRecord::StrPhase() const
//  ----------------------------------------------------------------------------
{
    if ( ! IsSetPhase() ) {
        return ".";
    }
    return NStr::UIntToString( Phase() );
}

//  ----------------------------------------------------------------------------
string CGff3WriteRecord::StrAttributes() const
//  ----------------------------------------------------------------------------
{
    string strAttributes;
	strAttributes.reserve(256);
    CGff3WriteRecord::TAttributes attrs;
    attrs.insert( Attributes().begin(), Attributes().end() );
    CGff3WriteRecord::TAttrIt it;

    x_PriorityProcess( "ID", attrs, strAttributes );
    x_PriorityProcess( "Name", attrs, strAttributes );
    x_PriorityProcess( "Alias", attrs, strAttributes );
    x_PriorityProcess( "Parent", attrs, strAttributes );
    x_PriorityProcess( "Target", attrs, strAttributes );
    x_PriorityProcess( "Gap", attrs, strAttributes );
    x_PriorityProcess( "Derives_from", attrs, strAttributes );
    x_PriorityProcess( "Note", attrs, strAttributes );
    x_PriorityProcess( "Dbxref", attrs, strAttributes );
    x_PriorityProcess( "Ontology_term", attrs, strAttributes );

    for ( it = attrs.begin(); it != attrs.end(); ++it ) {
        string strKey = it->first;
        if ( NStr::StartsWith( strKey, "gff_" ) ) {
            continue;
        }

        if ( ! strAttributes.empty() ) {
            strAttributes += "; ";
        }
        strAttributes += strKey;
        strAttributes += "=";
		
		bool quote = x_NeedsQuoting(it->second);
		if ( quote )
			strAttributes += '\"';		
		strAttributes += it->second;
		if ( quote )
			strAttributes += '\"';
    }
    return strAttributes;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecord::x_NeedsQuoting(
    const string& str )
//  ----------------------------------------------------------------------------
{
    if( str.empty() )
		return true;

	for ( size_t u=0; u < str.length(); ++u ) {
        if ( str[u] == '\"' )
			return false;
		if ( str[u] == ' ' || str[u] == ';' ) {
            return true;
        }
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecord::x_AssignSeqIdFromAsn(
    const CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    m_strId = "<unknown>";

    if ( feature.CanGetLocation() ) {
    	const CSeq_loc& loc = feature.GetLocation();
	    CConstRef<CSeq_id> id(loc.GetId());
		if (id) {
			m_strId.clear();
			id->GetLabel(&m_strId, CSeq_id::eContent);
		}
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecord::x_AssignTypeFromAsn(
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
bool CGff3WriteRecord::x_AssignStartFromAsn(
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
bool CGff3WriteRecord::x_AssignStopFromAsn(
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
bool CGff3WriteRecord::x_AssignSourceFromAsn(
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
bool CGff3WriteRecord::x_AssignScoreFromAsn(
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
bool CGff3WriteRecord::x_AssignStrandFromAsn(
    const CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    if ( feature.CanGetLocation() ) {
        m_peStrand = new ENa_strand( feature.GetLocation().GetStrand() );
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecord::x_AssignPhaseFromAsn(
    const CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    if ( ! feature.CanGetData() ) {
        return true;
    }
    const CSeq_feat::TData& data = feature.GetData();
    if ( data.GetSubtype() == CSeq_feat::TData::eSubtype_cdregion ) {
        const CCdregion& cdr = data.GetCdregion();
        m_puPhase = new unsigned int( 0 );
        return true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecord::x_AssignAttributesFromAsnCore(
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
        string value = CGff3WriteRecord::x_FeatIdString( id );
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
                    CGff3WriteRecord::x_GetSubtypeOf( id );
                if ( ! x_IsParentOf( other_type, feature.GetData().GetSubtype() ) ) {
                    continue;
                }
                if ( ! value.empty() ) {
                    value += ",";
                }
                value += CGff3WriteRecord::x_FeatIdString( id );
            }
        }
        if ( ! value.empty() ) {
            m_Attributes[ "Parent" ] = value;
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecord::x_AssignAttributesFromAsnExtended(
    const CSeq_feat& feature )
//  ----------------------------------------------------------------------------
{
    if ( feature.CanGetDbxref() ) {
        const CSeq_feat::TDbxref& dbxrefs = feature.GetDbxref();
        if ( dbxrefs.size() > 0 ) {
            string value = s_MakeGffDbtag( *dbxrefs[ 0 ] );
            for ( size_t i=1; i < dbxrefs.size(); ++i ) {
                value += ";";
                value += s_MakeGffDbtag( *dbxrefs[ i ] );
            }
            m_Attributes[ "Dbxref" ] = value;
        }
    }
    if ( feature.CanGetComment() ) {
        m_Attributes[ "Note" ] = feature.GetComment();
    }
    if ( feature.CanGetPseudo()  &&  feature.GetPseudo() ) {
        m_Attributes[ "pseudo" ] = "";
    }
    if ( feature.CanGetPartial()  &&  feature.GetPartial() ) {
        m_Attributes[ "partial" ] = "";
    }
    return true;
}

//  ----------------------------------------------------------------------------
CSeq_feat::TData::ESubtype CGff3WriteRecord::x_GetSubtypeOf(
    const CFeat_id& id )
//  ----------------------------------------------------------------------------
{
    CSeq_feat::TData::ESubtype subtype( CSeq_feat::TData::eSubtype_bad );
	if ( id.Which() == CFeat_id::e_Local ) { 
       	CSeq_entry_Handle seh = m_Sah.GetTopLevelEntry();
       	CSeq_feat_Handle sfh = seh.GetTSE_Handle().GetFeatureWithId( 
           	CSeqFeatData::e_not_set, id.GetLocal() );
       	if(sfh) {
            subtype = sfh.GetFeatSubtype();
        } else {
//            ERR_POST(Warning << "CGff3WriteRecord::x_GetSubtypeOf: Feature could not be found.");
        }
	}
    return subtype;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecord::x_IsParentOf(
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
string CGff3WriteRecord::x_FeatIdString(
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

//  ----------------------------------------------------------------------------
void CGff3WriteRecord::x_PriorityProcess(
    const string& strKey,
    map<string, string >& attrs,
    string& strAttributes ) const
//  ----------------------------------------------------------------------------
{
    string strKeyMod( strKey );

    map< string, string >::iterator it = attrs.find( strKeyMod );
    if ( it == attrs.end() ) {
        return;
    }

    // Some of the attributes are multivalue translating into multiple gff attributes
    //  all carrying the same key. These are special cased here:
    //
    if ( strKey == "Dbxref" ) {
        vector<string> tags;
        NStr::Tokenize( it->second, ";", tags );
        for ( vector<string>::iterator pTag = tags.begin(); 
            pTag != tags.end(); pTag++ ) {
            if ( ! strAttributes.empty() ) {
                strAttributes += "; ";
            }
            strAttributes += strKeyMod;
            strAttributes += "=\""; // quoted in all samples I have seen
            strAttributes += *pTag;
            strAttributes += "\"";
        }
		attrs.erase(it);
        return;
    }

    // General case: Single value, make straight forward gff attribute:
    //
    if ( ! strAttributes.empty() ) {
        strAttributes += "; ";
    }

    strAttributes += strKeyMod;
    strAttributes += "=";
   	bool quote = x_NeedsQuoting(it->second);
	if ( quote )
		strAttributes += '\"';		
	strAttributes += it->second;
	attrs.erase(it);
	if ( quote )
		strAttributes += '\"';
}

END_objects_SCOPE
END_NCBI_SCOPE
