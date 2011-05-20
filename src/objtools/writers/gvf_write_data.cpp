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
 *   GVF file reader
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
#include <objects/seqfeat/Variation_ref.hpp>
#include <objects/seqfeat/Variation_inst.hpp>

#include <objtools/writers/gff3_write_data.hpp>
#include <objtools/writers/gvf_write_data.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

int CGvfWriteRecord::s_unique = 0;

//  ----------------------------------------------------------------------------
string CGvfWriteRecord::s_UniqueId()
//  ----------------------------------------------------------------------------
{
    return string( "id_" ) + NStr::IntToString( s_unique++ );
}

//  ----------------------------------------------------------------------------
CGvfWriteRecord::CGvfWriteRecord(
    feature::CFeatTree& feat_tree )
//  ----------------------------------------------------------------------------
    : CGff3WriteRecordFeature( feat_tree )
{
};

//  ----------------------------------------------------------------------------
CGvfWriteRecord::CGvfWriteRecord(
    const CGff3WriteRecordFeature& other )
//  ----------------------------------------------------------------------------
    : CGff3WriteRecordFeature( other )
{
};

//  ----------------------------------------------------------------------------
CGvfWriteRecord::~CGvfWriteRecord()
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------
bool CGvfWriteRecord::x_AssignSource(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    m_strSource = ".";
    if ( mapped_feat.IsSetExt() ) {
        const CSeq_feat::TExt& ext = mapped_feat.GetExt();
        if ( ext.IsSetType() && ext.GetType().IsStr() && 
            ext.GetType().GetStr() == "GvfAttributes" ) 
        {
            if ( ext.HasField( "source" ) ) {
                m_strSource = ext.GetField( "source" ).GetData().GetStr();
                return true;
            }
        }
    }

    if ( CSeqFeatData::eSubtype_variation_ref != mapped_feat.GetData().GetSubtype() ) {
        return true;
    }
    const CVariation_ref& variation = mapped_feat.GetData().GetVariation();
    if ( variation.IsSetId() ) {
        m_strSource = variation.GetId().GetDb();
        return true;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfWriteRecord::x_AssignType(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    m_strType = ".";
    if ( mapped_feat.IsSetExt() ) {
        const CSeq_feat::TExt& ext = mapped_feat.GetExt();
        if ( ext.IsSetType() && ext.GetType().IsStr() && 
            ext.GetType().GetStr() == "GvfAttributes" ) 
        {
            if ( ext.HasField( "custom-var_type" ) ) {
                m_strType = ext.GetField( "custom-var_type" ).GetData().GetStr();
                return true;
            }
        }
    }

    if ( CSeqFeatData::eSubtype_variation_ref != mapped_feat.GetData().GetSubtype() ) {
        return true;
    }
    const CVariation_ref& var_ref = mapped_feat.GetData().GetVariation();
    if ( var_ref.IsCNV() ) {
        m_strType = "CNV";
        return true;
    }

    if ( ! var_ref.GetData().IsInstance() ) {
        return true;
    }
    switch( var_ref.GetData().GetInstance().GetType() ) {
    
    default:
        return true;
    case CVariation_inst::eType_snv:
        m_strType = "SNV";
        return true;
    }

    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfWriteRecord::x_AssignAttributes(
    CMappedFeat mapped_feat )
//  ----------------------------------------------------------------------------
{
    if ( ! x_AssignAttributeID( mapped_feat ) ) {
        return false;
    }
    if ( ! x_AssignAttributeParent( mapped_feat ) ) {
        return false;
    }
    if ( ! x_AssignAttributeName( mapped_feat ) ) {
        return false;
    }
    if ( ! x_AssignAttributeVarType( mapped_feat ) ) {
        return false;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfWriteRecord::x_AssignAttributeID(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if ( mf.IsSetExt() ) {
        const CSeq_feat::TExt& ext = mf.GetExt();
        if ( ext.IsSetType() && ext.GetType().IsStr() && 
            ext.GetType().GetStr() == "GvfAttributes" ) 
        {
            if ( ext.HasField( "id" ) ) {
                m_Attributes["ID"] = 
                    ext.GetField( "id" ).GetData().GetStr();
                return true;
            }
        }
    }

    if ( CSeqFeatData::eSubtype_variation_ref != mf.GetData().GetSubtype() ) {
        m_Attributes["ID"] = s_UniqueId();
        return true;
    }
    const CVariation_ref& var_ref = mf.GetData().GetVariation();
    if ( ! var_ref.IsSetId() ) {
        m_Attributes["ID"] = s_UniqueId();
        return true;
    }
    const CVariation_ref::TId& id = var_ref.GetId();
    string strId;
    id.GetLabel( &strId );
    m_Attributes["ID"] = strId;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfWriteRecord::x_AssignAttributeParent(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if ( mf.IsSetExt() ) {
        const CSeq_feat::TExt& ext = mf.GetExt();
        if ( ext.IsSetType() && ext.GetType().IsStr() && 
            ext.GetType().GetStr() == "GvfAttributes" ) 
        {
            if ( ext.HasField( "parent" ) ) {
                m_Attributes["Parent"] = 
                    ext.GetField( "parent" ).GetData().GetStr();
                return true;
            }
        }
    }

    if ( CSeqFeatData::eSubtype_variation_ref != mf.GetData().GetSubtype() ) {
        return true;
    }
    const CVariation_ref& var_ref = mf.GetData().GetVariation();
    if ( ! var_ref.IsSetParent_id() ) {
        return true;
    }
    const CVariation_ref::TId& id = var_ref.GetParent_id();
    string strId;
    id.GetLabel( &strId );
    m_Attributes["Parent"] = strId;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfWriteRecord::x_AssignAttributeName(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if ( CSeqFeatData::eSubtype_variation_ref != mf.GetData().GetSubtype() ) {
        return true;
    }
    const CVariation_ref& var_ref = mf.GetData().GetVariation();
    if ( ! var_ref.IsSetName() ) {
        return true;
    }
    m_Attributes["Name"] = var_ref.GetName();
    return true;
}

//  ----------------------------------------------------------------------------
bool CGvfWriteRecord::x_AssignAttributeVarType(
    CMappedFeat mf )
//  ----------------------------------------------------------------------------
{
    if ( mf.IsSetExt() ) {
        const CSeq_feat::TExt& ext = mf.GetExt();
        if ( ext.IsSetType() && ext.GetType().IsStr() && 
            ext.GetType().GetStr() == "GvfAttributes" ) 
        {
            if ( ext.HasField( "custom-var_type" ) ) {
                m_Attributes["var_type"] = 
                    ext.GetField( "custom-var_type" ).GetData().GetStr();
                return true;
            }
        }
    }

    m_Attributes["var_type"] = StrType();
    return true;
}
END_objects_SCOPE
END_NCBI_SCOPE
