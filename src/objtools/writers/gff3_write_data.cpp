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
#include <objects/general/User_object.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>

#include <objtools/writers/gff3_write_data.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>

#include <objects/seq/sofa_type.hpp>
#include <objects/seq/sofa_map.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
string s_GeneRefToGene(
    const CGene_ref& gene_ref )
//  ----------------------------------------------------------------------------
{
    if ( gene_ref.IsSetLocus() ) {
        return gene_ref.GetLocus();
    }
    if ( gene_ref.IsSetLocus_tag() ) {
        return gene_ref.GetLocus_tag();
    }
    if ( gene_ref.IsSetSyn()  && gene_ref.GetSyn().size() > 0 ) {
        return *( gene_ref.GetSyn().begin() );
    }
    if ( gene_ref.IsSetDesc() ) {
        return gene_ref.GetDesc();
    }
    return "";
}

//  ----------------------------------------------------------------------------
string s_MakeGffDbtag( 
    const CDbtag& dbtag )
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
string s_MakeGffDbtag( 
    const CSeq_id_Handle& idh,
    CScope& scope )
//  ----------------------------------------------------------------------------
{
    CSeq_id_Handle gi_idh = sequence::GetId( idh, scope, sequence::eGetId_ForceGi );
    if ( !gi_idh ) {
        return idh.AsString();
    }
    string strGffTag("GI:");
    gi_idh.GetSeqId()->GetLabel( &strGffTag, CSeq_id::eContent );
    return strGffTag;
}
        
//  ----------------------------------------------------------------------------
string s_BestIdString(
    CSeq_id_Handle idh,
    CScope& scope )
//  ----------------------------------------------------------------------------
{
    CSeq_id_Handle best_idh = sequence::GetId( idh, scope, sequence::eGetId_Best );
    if ( !best_idh ) {
        best_idh = idh;
    }
    string strId("");
    best_idh.GetSeqId()->GetLabel( &strId, CSeq_id::eContent );
    return strId;
}
    
//  ----------------------------------------------------------------------------
CConstRef<CUser_object> s_GetUserObjectByType(
    const CUser_object& uo,
    const string& strType )
//  ----------------------------------------------------------------------------
{
    if ( uo.IsSetType() && uo.GetType().IsStr() && 
            uo.GetType().GetStr() == strType ) {
        return CConstRef<CUser_object>( &uo );
    }
    const CUser_object::TData& fields = uo.GetData();
    for ( CUser_object::TData::const_iterator it = fields.begin(); 
            it != fields.end(); 
            ++it ) {
        const CUser_field& field = **it;
        if ( field.IsSetData() ) {
            const CUser_field::TData& data = field.GetData();
            if ( data.Which() == CUser_field::TData::e_Object ) {
                CConstRef<CUser_object> recur = s_GetUserObjectByType( 
                    data.GetObject(), strType );
                if ( recur ) {
                    return recur;
                }
            }
        }
    }
    return CConstRef<CUser_object>();
}
    
//  ----------------------------------------------------------------------------
CGff3WriteRecordFeature::CGff3WriteRecordFeature(
    CGffFeatureContext& fc,
    const string& id )
//  ----------------------------------------------------------------------------
    : CGffWriteRecordFeature(id), m_fc( fc )
{
};

//  ----------------------------------------------------------------------------
CGff3WriteRecordFeature::CGff3WriteRecordFeature(
    const CGff3WriteRecordFeature& other )
//  ----------------------------------------------------------------------------
    : CGffWriteRecordFeature( other ),
      m_fc( other.m_fc )
{
};

//  ----------------------------------------------------------------------------
CGff3WriteRecordFeature::~CGff3WriteRecordFeature()
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::AssignFromAsn(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    m_pLoc.Reset( new CSeq_loc( CSeq_loc::e_Mix ) );
    m_pLoc->Add( mf.GetLocation() );
    m_pLoc->ChangeToPackedInt();

    CBioseq_Handle bsh = m_fc.BioseqHandle();
    if (!bsh  ||  !bsh.IsSetInst_Topology()  
              ||  bsh.GetInst_Topology() != CSeq_inst::eTopology_circular) {
        return CGffWriteRecordFeature::AssignFromAsn(mf);
    }

    //  intervals wrapping around the origin extend beyond the sequence length
    //  instead of breaking and restarting at the origin.
    //
    unsigned int len = bsh.GetInst().GetLength();
    list< CRef< CSeq_interval > >& sublocs = m_pLoc->SetPacked_int().Set();
    if (sublocs.size() < 2) {
        return CGffWriteRecordFeature::AssignFromAsn(mf);
    }

    list< CRef< CSeq_interval > >::iterator it, it_ceil=sublocs.end(), 
        it_floor=sublocs.end();
    for ( it = sublocs.begin(); it != sublocs.end(); ++it ) {
        CSeq_interval& subint = **it;
        if (subint.IsSetFrom()  &&  subint.GetFrom() == 0) {
            it_floor = it;
        }
        if (subint.IsSetTo()  &&  subint.GetTo() == len-1) {
            it_ceil = it;
        }
        if (it_floor != sublocs.end()  &&  it_ceil != sublocs.end()) {
            break;
        } 
    }
    if ( it_ceil != sublocs.end()  &&  it_floor != sublocs.end() ) {
        (*it_ceil)->SetTo( (*it_ceil)->GetTo() + (*it_floor)->GetTo() );
        sublocs.erase(it_floor);
    }

    return CGffWriteRecordFeature::AssignFromAsn(mf);
};
    
//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignType(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    static CSofaMap SOFAMAP;

    if ( ! mf.IsSetData() ) {
        m_strType = SOFAMAP.DefaultName();
    }
    m_strType = SOFAMAP.MappedName( mf.GetFeatType(), mf.GetFeatSubtype() );
    return true;
};

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignStart(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    if ( m_pLoc ) {
        m_uSeqStart = m_pLoc->GetStart( eExtreme_Positional );;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignStop(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    if ( m_pLoc ) {
        m_uSeqStop = m_pLoc->GetStop( eExtreme_Positional );;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributesFromAsnCore(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    // If feature ids are present then they are likely used to show parent/child
    // relationships, via corresponding xrefs. Thus, any feature ids override
    // gb ID tags (feature ids and ID tags should agree in the first place, but
    // if not, feature ids must trump ID tags).
    //

    // Actually, I might be doing more harm that good. Disabling this logic 
    // pending further investigation...
    return true;

    bool bIdAssigned = false;

    if ( mapped_feat.IsSetId() ) {
        const CSeq_feat::TId& id = mapped_feat.GetId();
        string value = CGffWriteRecordFeature::x_FeatIdString( id );
        AddAttribute("ID", value);
        bIdAssigned = true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributesFromAsnExtended(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    // collection of feature attribute that we want to propagate to every feature
    //  type under the sun
    //
    if ( ! x_AssignAttributeGbKey( mapped_feat ) ) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
string CGff3WriteRecordFeature::StrAttributes() const
//  ----------------------------------------------------------------------------
{
    string strAttributes;
    strAttributes.reserve(256);
    CGffWriteRecord::TAttributes attrs;
    attrs.insert( Attributes().begin(), Attributes().end() );
    CGffWriteRecord::TAttrIt it;

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
    if ( strAttributes.empty() ) {
        strAttributes = ".";
    }
    return strAttributes;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributes(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if ( ! x_AssignAttributesFromAsnCore( mf ) ) {
        return false;
    }
    if ( ! x_AssignAttributesFromAsnExtended( mf ) ) {
        return false;
    }

    switch( mf.GetData().GetSubtype() ) {
        default:
            return x_AssignAttributesMiscFeature( mf );
        case CSeqFeatData::eSubtype_gene:
            return x_AssignAttributesGene( mf );
        case CSeqFeatData::eSubtype_mRNA:
            return x_AssignAttributesMrna( mf );
        case CSeqFeatData::eSubtype_tRNA:
            return x_AssignAttributesTrna( mf );
        case CSeqFeatData::eSubtype_cdregion:
            return x_AssignAttributesCds( mf );
    }
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributesGene(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    return (
        x_AssignAttributeGene( mapped_feat )  &&
        x_AssignAttributeGeneSynonym( mapped_feat )  &&
        x_AssignAttributeLocusTag( mapped_feat )  &&
        x_AssignAttributeDbXref( mapped_feat )  &&
        x_AssignAttributePseudo( mapped_feat )  &&
        x_AssignAttributePartial( mapped_feat )  &&
        x_AssignAttributeException( mapped_feat )  &&
        x_AssignAttributeGeneDesc( mapped_feat )  &&
        x_AssignAttributeMapLoc( mapped_feat )  &&
        x_AssignAttributeNote( mapped_feat ) );
}
//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributesMrna(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    return (
        x_AssignAttributeGene( mapped_feat )  &&

        x_AssignAttributeProduct( mapped_feat )  &&
        x_AssignAttributeTranscriptId( mapped_feat )  &&
        x_AssignAttributeException( mapped_feat )  &&
        x_AssignAttributeDbXref( mapped_feat )  &&
        x_AssignAttributeNote( mapped_feat ) );
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributesTrna(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    return (
        x_AssignAttributeException( mapped_feat )  &&
        x_AssignAttributeDbXref( mapped_feat )  &&
        x_AssignAttributeNote( mapped_feat ) );
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributesCds(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    return (
        x_AssignAttributeProteinId( mapped_feat )  &&
        x_AssignAttributeProduct( mapped_feat )  &&
        x_AssignAttributeException( mapped_feat )  &&
        x_AssignAttributeDbXref( mapped_feat )  &&
        x_AssignAttributeNote( mapped_feat ) );
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributesMiscFeature(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    return (
        x_AssignAttributeException( mapped_feat )  &&
        x_AssignAttributeExonNumber( mapped_feat )  &&
        x_AssignAttributePseudo( mapped_feat )  &&
        x_AssignAttributeNote( mapped_feat ) );
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeGene(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    string strGene;
    if ( mf.GetData().Which() == CSeq_feat::TData::e_Gene ) {
        const CGene_ref& gene_ref = mf.GetData().GetGene();
        strGene = s_GeneRefToGene( gene_ref );
    }

    if ( strGene.empty() && mf.IsSetXref() ) {
        const vector< CRef< CSeqFeatXref > > xrefs = mf.GetXref();
        for ( vector< CRef< CSeqFeatXref > >::const_iterator it = xrefs.begin();
            it != xrefs.end();
            ++it ) {
            const CSeqFeatXref& xref = **it;
            if ( xref.CanGetData() && xref.GetData().IsGene() ) {
                strGene = s_GeneRefToGene( xref.GetData().GetGene() );
                break;
            }
        }
    }

    if ( strGene.empty() ) {
        CMappedFeat gene = feature::GetBestGeneForFeat( mf, &m_fc.FeatTree() );
        if ( gene.IsSetData()  &&  gene.GetData().IsGene() ) {
            strGene = s_GeneRefToGene( gene.GetData().GetGene() );
        }
    }

    if ( ! strGene.empty() ) {
        AddAttribute("gene", strGene);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeNote(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    if ( ! mapped_feat.IsSetComment() ) {
        return true;
    }
    AddAttribute("Note", mapped_feat.GetComment());
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributePartial(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    if ( ! mapped_feat.IsSetPartial() ) {
        return true;
    }
    if ( mapped_feat.GetPartial() == true ) {
        AddAttribute("partial", "true");
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributePseudo(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    if ( ! mapped_feat.IsSetPseudo() ) {
        return true;
    }
    if ( mapped_feat.GetPseudo() == true ) {
        AddAttribute("pseudo", "true");
    }
    return true;
}

#include <algorithm>

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeDbXref(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    vector<string> values;
    CSeqFeatData::E_Choice choice = mf.GetData().Which();

    if ( mf.IsSetDbxref() ) {
        const CSeq_feat::TDbxref& dbxrefs = mf.GetDbxref();
        for ( size_t i=0; i < dbxrefs.size(); ++i ) {
            values.push_back( s_MakeGffDbtag( *dbxrefs[ i ] ) );
        }
    }
    if ( m_fc.FeatTree().GetParent( mf ) ) {
        const CSeq_feat::TDbxref& more_dbxrefs = 
            m_fc.FeatTree().GetParent( mf ).GetDbxref();
        for ( size_t i=0; i < more_dbxrefs.size(); ++i ) {
            string str = s_MakeGffDbtag( *more_dbxrefs[ i ] );
            if ( values.end() == find( values.begin(), values.end(), str ) ) {
                values.push_back( str );
            }
        }
    }

    if ( choice == CSeq_feat::TData::e_Rna || choice == CSeq_feat::TData::e_Cdregion ) {
        if ( mf.IsSetProduct() ) {
            string str = s_MakeGffDbtag( mf.GetProductId(), mf.GetScope() );
            if ( values.end() == find( values.begin(), values.end(), str ) ) {
                values.push_back( str );
            }
        }
        CMappedFeat gene_feat = m_fc.FeatTree().GetParent( mf, CSeqFeatData::e_Gene );
        if ( gene_feat  &&  gene_feat.IsSetDbxref() ) {
            const CSeq_feat::TDbxref& dbxrefs = gene_feat.GetDbxref();
            for ( size_t i=0; i < dbxrefs.size(); ++i ) {
                string str = s_MakeGffDbtag( *dbxrefs[ i ] );
                if ( values.end() == find( values.begin(), values.end(), str ) ) {
                    values.push_back( str );
                }
            }
        }
    }

    if ( ! values.empty() ) {
        // will see more processing before final output, so don't quote just yet.
        m_Attributes["Dbxref"] = NStr::Join( values, ";" );
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeGeneSynonym(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CGene_ref& gene_ref = mapped_feat.GetData().GetGene();
    if ( !gene_ref.IsSetSyn() ) {
        return true;
    }

    const list<string>& syns = gene_ref.GetSyn();
    list<string>::const_iterator it = syns.begin();
    if ( ! gene_ref.IsSetLocus() && ! gene_ref.IsSetLocus_tag() ) {
        ++it;
    }
    if ( it == syns.end() ) {
        return true;
    }
    string strGeneSyn = *( it++ );
    while ( it != syns.end() ) {
        strGeneSyn += ",";
        strGeneSyn += * (it++ );
    }

    if ( ! strGeneSyn.empty() ) {
        AddAttribute("gene_synonym", strGeneSyn);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeGeneDesc(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CGene_ref& gene_ref = mapped_feat.GetData().GetGene();
    if ( ! gene_ref.IsSetDesc() ) {
        return true;
    }
    AddAttribute("description", gene_ref.GetDesc());
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeMapLoc(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CGene_ref& gene_ref = mapped_feat.GetData().GetGene();
    if ( ! gene_ref.IsSetMaploc() ) {
        return true;
    }
    AddAttribute("map", gene_ref.GetMaploc());
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeLocusTag(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CGene_ref& gene_ref = mapped_feat.GetData().GetGene();
    if ( ! gene_ref.IsSetLocus() || ! gene_ref.IsSetLocus_tag() ) {
        return true;
    }
    AddAttribute("locus_tag", gene_ref.GetLocus_tag());
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeCodonStart(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CCdregion& cdr = mapped_feat.GetData().GetCdregion();
    if ( ! cdr.IsSetFrame() ) {
        return true;
    }
    string strFrame;
    switch( cdr.GetFrame() ) {
    default:
        break;

    case CCdregion::eFrame_one:
        strFrame = "1";
        break;

    case CCdregion::eFrame_two:
        strFrame = "2";
        break;

    case CCdregion::eFrame_three:
        strFrame = "3";
        break;
    }
    if ( ! strFrame.empty() ) {
        AddAttribute("codon_start", strFrame);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeProduct(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    const CProt_ref* pProtRef = mf.GetProtXref();
    if ( pProtRef && pProtRef->IsSetName() ) {
        const list<string>& names = pProtRef->GetName();
        AddAttribute("product", x_Encode(*names.begin()));
        return true;
    }
    if ( ! mf.IsSetProduct() ) {
        return true;
    }
    AddAttribute("product", s_BestIdString( mf.GetProductId(), mf.GetScope()));
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeEvidence(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const string strExperimentDefault(
        "experimental evidence, no additional details recorded" );
    const string strInferenceDefault(
        "non-experimental evidence, no additional details recorded" );

    bool bExperiment = false;
    bool bInference = false;
    const CSeq_feat::TQual& quals = mapped_feat.GetQual();
    for ( CSeq_feat::TQual::const_iterator it = quals.begin(); 
            ( it != quals.end() ) && ( !bExperiment && !bInference ); 
            ++it ) {
        if ( ! (*it)->CanGetQual() ) {
            continue;
        }
        string strKey = (*it)->GetQual();
        if ( strKey == "experiment" ) {
            AddAttribute("experiment", (*it)->GetVal());
            bExperiment = true;
        }
        if ( strKey == "inference" ) {
            AddAttribute("inference", (*it)->GetVal());
            bInference = true;
        }
    }

    // "exp_ev" only enters if neither "experiment" nor "inference" qualifiers
    //  present
    if ( !bExperiment && !bInference ) {
        if ( mapped_feat.IsSetExp_ev() ) {
            if ( mapped_feat.GetExp_ev() == CSeq_feat::eExp_ev_not_experimental ) {
                AddAttribute("inference", strInferenceDefault);
            }
            else if ( mapped_feat.GetExp_ev() == CSeq_feat::eExp_ev_experimental ) {
                AddAttribute("experiment", strExperimentDefault);
            }
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeModelEvidence(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    if ( mapped_feat.IsSetExt() ) {
        CConstRef<CUser_object> model_evidence = s_GetUserObjectByType( 
            mapped_feat.GetExt(), "ModelEvidence" );
        if ( model_evidence ) {
            string strNote;
            if ( model_evidence->HasField( "Method" ) ) {
                GetAttribute( "Note", strNote );
                if ( ! strNote.empty() ) {
                    strNote += "; ";
                }               
                strNote += "Derived by automated computational analysis";
                strNote += " using gene prediction method: ";
                strNote += model_evidence->GetField( "Method" ).GetData().GetStr();
                strNote += ".";
            }
            if ( model_evidence->HasField( "Counts" ) ) {
                const CUser_field::TData::TFields& fields =
                    model_evidence->GetField( "Counts" ).GetData().GetFields();
                unsigned int uCountMrna = 0;
                unsigned int uCountEst = 0;
                unsigned int uCountProtein = 0;
                for ( CUser_field::TData::TFields::const_iterator cit = fields.begin();
                    cit != fields.end();
                    ++cit ) {
                    string strLabel = (*cit)->GetLabel().GetStr();
                    if ( strLabel == "mRNA" ) {
                        uCountMrna = (*cit)->GetData().GetInt();
                        continue;
                    }
                    if ( strLabel == "EST" ) {
                        uCountEst = (*cit)->GetData().GetInt();
                        continue;
                    }
                    if ( strLabel == "Protein" ) {
                        uCountProtein = (*cit)->GetData().GetInt();
                        continue;
                    }
                }
                if ( uCountMrna || uCountEst || uCountProtein ) {
                    string strSupport = " Supporting evidence includes similarity to:";
                    string strPrefix = " ";
                    if ( uCountMrna ) {
                        strSupport += strPrefix;
                        strSupport += NStr::UIntToString( uCountMrna );
                        strSupport += ( uCountMrna > 1 ? " mRNAs" : " mRNA" );
                        strPrefix = "%2C ";
                    }
                    if ( uCountEst ) {
                        strSupport += strPrefix;
                        strSupport += NStr::UIntToString( uCountEst );
                        strSupport += ( uCountEst > 1 ? " ESTs" : " EST" );
                        strPrefix = "%2C ";
                    }
                    if ( uCountProtein ) {
                        strSupport += strPrefix;
                        strSupport += NStr::UIntToString( uCountProtein );
                        strSupport += ( uCountProtein > 1 ? " Proteins" : " Protein" );
                    }
                    strNote += strSupport;
                }
            }
            if ( ! strNote.empty() ) {
                AddAttribute("Note", strNote);
            }
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeGbKey(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    AddAttribute("gbkey", mapped_feat.GetData().GetKey());
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeTranscriptId(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    const CSeq_feat::TQual& quals = mapped_feat.GetQual();
    for ( CSeq_feat::TQual::const_iterator cit = quals.begin(); 
      cit != quals.end(); ++cit ) {
        if ( (*cit)->GetQual() == "transcript_id" ) {
            AddAttribute("transcript_id", (*cit)->GetVal());
            return true;
        }
    }

    if ( mapped_feat.IsSetProduct() ) {
        AddAttribute("transcript_id", 
            s_BestIdString(mapped_feat.GetProductId(), mapped_feat.GetScope()));
        return true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeProteinId(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    if ( ! mapped_feat.IsSetProduct() ) {
        return true;
    }
    AddAttribute("protein_id", 
        s_BestIdString(mapped_feat.GetProductId(), mapped_feat.GetScope()));
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeException(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    if ( mapped_feat.IsSetExcept_text() ) {
        AddAttribute("exception", mapped_feat.GetExcept_text());
        return true;
    }
    if ( mapped_feat.IsSetExcept() ) {
        // what should I do?
        return true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::x_AssignAttributeExonNumber(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if (mf.IsSetQual()) {
        const CSeq_feat::TQual& quals = mf.GetQual();
        for ( CSeq_feat::TQual::const_iterator cit = quals.begin(); 
            cit != quals.end(); 
            ++cit ) {
            const CGb_qual& qual = **cit;
            if (qual.IsSetQual()  &&  qual.GetQual() == "number") {
                AddAttribute("exon_number", qual.GetVal());
                return true;
            }
        }
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3WriteRecordFeature::AssignParent(
    const CGff3WriteRecordFeature& parent )
//  ----------------------------------------------------------------------------
{
    string strParentId;
    if ( ! parent.GetAttribute( "ID", strParentId ) ) {
        cerr << "Fix me: Parent record without GFF3 ID tag!" << endl;
        return false;
    }
    AddAttribute("Parent", strParentId);
    return true;
}

//  ----------------------------------------------------------------------------
void CGff3WriteRecordFeature::ForceAttributeID(
    const string& strId )
//  ----------------------------------------------------------------------------
{
    AddAttribute("ID", strId);
}  

//  ----------------------------------------------------------------------------
string CGff3WriteRecordFeature::x_Encode(
    const string& strRaw )
//  ----------------------------------------------------------------------------
{
    static const char s_Table[256][4] = {
        "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07",
        "%08", "%09", "%0A", "%0B", "%0C", "%0D", "%0E", "%0F",
        "%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17",
        "%18", "%19", "%1A", "%1B", "%1C", "%1D", "%1E", "%1F",
        " ",   "!",   "%22", "%23", "$",   "%25", "%26", "%27",
        "%28", "%29", "%2A", "%2B", "%2C", "-",   ".",   "%2F",
        "0",   "1",   "2",   "3",   "4",   "5",   "6",   "7",
        "8",   "9",   ":",   "%3B", "%3C", "%3D", "%3E", "%3F",
        "@",   "A",   "B",   "C",   "D",   "E",   "F",   "G",
        "H",   "I",   "J",   "K",   "L",   "M",   "N",   "O",
        "P",   "Q",   "R",   "S",   "T",   "U",   "V",   "W",
        "X",   "Y",   "Z",   "%5B", "%5C", "%5D", "^",   "_",
        "%60", "a",   "b",   "c",   "d",   "e",   "f",   "g",
        "h",   "i",   "j",   "k",   "l",   "m",   "n",   "o",
        "p",   "q",   "r",   "s",   "t",   "u",   "v",   "w",
        "x",   "y",   "z",   "%7B", "%7C", "%7D", "%7E", "%7F",
        "%80", "%81", "%82", "%83", "%84", "%85", "%86", "%87",
        "%88", "%89", "%8A", "%8B", "%8C", "%8D", "%8E", "%8F",
        "%90", "%91", "%92", "%93", "%94", "%95", "%96", "%97",
        "%98", "%99", "%9A", "%9B", "%9C", "%9D", "%9E", "%9F",
        "%A0", "%A1", "%A2", "%A3", "%A4", "%A5", "%A6", "%A7",
        "%A8", "%A9", "%AA", "%AB", "%AC", "%AD", "%AE", "%AF",
        "%B0", "%B1", "%B2", "%B3", "%B4", "%B5", "%B6", "%B7",
        "%B8", "%B9", "%BA", "%BB", "%BC", "%BD", "%BE", "%BF",
        "%C0", "%C1", "%C2", "%C3", "%C4", "%C5", "%C6", "%C7",
        "%C8", "%C9", "%CA", "%CB", "%CC", "%CD", "%CE", "%CF",
        "%D0", "%D1", "%D2", "%D3", "%D4", "%D5", "%D6", "%D7",
        "%D8", "%D9", "%DA", "%DB", "%DC", "%DD", "%DE", "%DF",
        "%E0", "%E1", "%E2", "%E3", "%E4", "%E5", "%E6", "%E7",
        "%E8", "%E9", "%EA", "%EB", "%EC", "%ED", "%EE", "%EF",
        "%F0", "%F1", "%F2", "%F3", "%F4", "%F5", "%F6", "%F7",
        "%F8", "%F9", "%FA", "%FB", "|", "%FD", "%FE", "%FF"
    };
    string strEncoded;
    for ( size_t i = 0;  i < strRaw.size();  ++i ) {
        strEncoded += s_Table[static_cast<unsigned char>( strRaw[i] )];
    }
    return strEncoded;
}
    
END_objects_SCOPE
END_NCBI_SCOPE
