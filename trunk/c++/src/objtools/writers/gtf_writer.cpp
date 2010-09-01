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
 * Authors:  Frank Ludwig
 *
 * File Description:  Write gff file
 *
 */

#include <ncbi_pch.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/SeqFeatXref.hpp>

#include <objmgr/feat_ci.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/util/feature.hpp>

#include <objtools/writers/gff2_write_data.hpp>
#include <objtools/writers/gtf_write_data.hpp>
#include <objtools/writers/gff_writer.hpp>
#include <objtools/writers/gtf_writer.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
CGtfWriter::CGtfWriter(
    CScope& scope,
    CNcbiOstream& ostr,
    unsigned int uFlags ) :
//  ----------------------------------------------------------------------------
    CGff2Writer( scope, ostr ),
    m_uFlags( uFlags )
{
};

//  ----------------------------------------------------------------------------
CGtfWriter::~CGtfWriter()
//  ----------------------------------------------------------------------------
{
};

//  ----------------------------------------------------------------------------
bool CGtfWriter::x_WriteHeader()
//  ----------------------------------------------------------------------------
{
    m_Os << "#gtf-version 2.2" << endl;
    return true;
};

//  ----------------------------------------------------------------------------
bool CGtfWriter::x_WriteRecord( 
    const CGff2WriteRecord* pRecord )
//  ----------------------------------------------------------------------------
{
    const CGff2WriteRecord& record = *pRecord;

    m_Os << pRecord->StrId() << '\t';
    m_Os << pRecord->StrSource() << '\t';
    m_Os << pRecord->StrType() << '\t';
    m_Os << pRecord->StrSeqStart() << '\t';
    m_Os << pRecord->StrSeqStop() << '\t';
    m_Os << pRecord->StrScore() << '\t';
    m_Os << pRecord->StrStrand() << '\t';
    m_Os << pRecord->StrPhase() << '\t';

    if ( m_uFlags & fStructibutes ) {
        m_Os << pRecord->StrStructibutes() << endl;
    }
    else {
        m_Os << pRecord->StrAttributes() << endl;
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::x_AssignObject(
    feature::CFeatTree& feat_tree,
    CMappedFeat mapped_feature,        
    CGff2WriteRecordSet& set )
//  ----------------------------------------------------------------------------
{
    const CSeq_feat& feature = mapped_feature.GetOriginalFeature();
       
    if ( ! feature.CanGetData() ) {
        return false;
    }
    switch ( feature.GetData().GetSubtype() ) {
    default:
        // GTF is not interested --- ignore
        return true;

    case CSeq_feat::TData::eSubtype_mRNA: 
        return x_AssignObjectMrna( feat_tree, mapped_feature, set );       

    case CSeq_feat::TData::eSubtype_cdregion:
        return x_AssignObjectCds( feat_tree, mapped_feature, set );

    case CSeq_feat::TData::eSubtype_gene:
            return x_AssignObjectGene( feat_tree, mapped_feature, set );
    }
}
    
//  ----------------------------------------------------------------------------
bool CGtfWriter::x_AssignObjectGene(
    feature::CFeatTree& feat_tree,
    CMappedFeat mapped_feature,
    CGff2WriteRecordSet& set )
//  ----------------------------------------------------------------------------
{
    const CSeq_feat& feature = mapped_feature.GetOriginalFeature();
       
    CGff2WriteRecord* pRecord = x_CreateRecord( feat_tree );
    if ( ! pRecord->AssignFromAsn( mapped_feature ) ) {
        return false;
    }
    set.AddRecord( pRecord );
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::x_AssignObjectMrna(
    feature::CFeatTree& feat_tree,
    CMappedFeat mapped_feature,
    CGff2WriteRecordSet& set )
//  ----------------------------------------------------------------------------
{
    const CSeq_feat& feature = mapped_feature.GetOriginalFeature();
       
    m_exonMap.clear();

    CGtfRecord* pParent = dynamic_cast< CGtfRecord* >(
        x_CreateRecord( feat_tree ) );
    if ( ! pParent->AssignFromAsn( mapped_feature ) ) {
        delete pParent;
        return false;
    }

    const CSeq_loc& loc = feature.GetLocation();
    unsigned int uExonNumber = 1;

    CRef< CSeq_loc > pLocMrna( new CSeq_loc( CSeq_loc::e_Mix ) );
    pLocMrna->Add( loc );
    pLocMrna->ChangeToPackedInt();

    if ( pLocMrna->IsPacked_int() && pLocMrna->GetPacked_int().CanGet() ) {
        list< CRef< CSeq_interval > >& sublocs = pLocMrna->SetPacked_int().Set();
        list< CRef< CSeq_interval > >::iterator it;
        for ( it = sublocs.begin(); it != sublocs.end(); ++it ) {
            CSeq_interval& subint = **it;
            CGtfRecord* pExon = new CGtfRecord( feat_tree );
            pParent->ForceType( "exon" );
            pExon->MakeChildRecord( *pParent, subint, uExonNumber );
            set.AddOrMergeRecord( pExon );
            m_exonMap[ uExonNumber++ ] = CRef< CSeq_interval >( 
                new CSeq_interval( subint.SetId(), subint.SetFrom(), subint.SetTo() ) );
        }
    }
    return true;
}
    
//  ----------------------------------------------------------------------------
bool CGtfWriter::x_AssignObjectCds(
    feature::CFeatTree& feat_tree,
    CMappedFeat mapped_feature,        
    CGff2WriteRecordSet& set )
//  ----------------------------------------------------------------------------
{
    const CSeq_feat& feature = mapped_feature.GetOriginalFeature();
       
    CGtfRecord* pParent = dynamic_cast< CGtfRecord* >( x_CreateRecord( feat_tree ) );
    if ( ! pParent->AssignFromAsn( mapped_feature ) ) {
        delete pParent;
        return false;
    }
    CRef< CSeq_loc > pLocStartCodon;
    CRef< CSeq_loc > pLocCode;
    CRef< CSeq_loc > pLocStopCodon;
    if ( ! x_SplitCdsLocation( feature, pLocStartCodon, pLocCode, pLocStopCodon ) ) {
        return false;
    }

    if ( pLocCode ) {
        pParent->ForceType( "CDS" );
        x_AddMultipleRecords( *pParent, pLocCode, set );
    }
    if ( pLocStartCodon ) {
        pParent->ForceType( "start_codon" );
        x_AddMultipleRecords( *pParent, pLocStartCodon, set );
    }
    if ( pLocStopCodon ) {
        pParent->ForceType( "stop_codon" );
        x_AddMultipleRecords( *pParent, pLocStopCodon, set );
    }

    delete pParent;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGtfWriter::x_SplitCdsLocation(
    const CSeq_feat& cds,
    CRef< CSeq_loc >& pLocStartCodon,
    CRef< CSeq_loc >& pLocCode,
    CRef< CSeq_loc >& pLocStopCodon ) const
//  ----------------------------------------------------------------------------
{
    //  Note: pLocCode will contain the location of the start codon but not the
    //  stop codon.

    const CSeq_loc& cdsLocation = cds.GetLocation();
    if ( cdsLocation.GetTotalRange().GetLength() < 6 ) {
        return false;
    }
    CSeq_id cdsLocId( cdsLocation.GetId()->AsFastaString() );

    pLocStartCodon.Reset( new CSeq_loc( CSeq_loc::e_Mix ) ); 
    pLocCode.Reset( new CSeq_loc( CSeq_loc::e_Mix ) ); 
    pLocStopCodon.Reset( new CSeq_loc( CSeq_loc::e_Mix ) ); 

    pLocCode->Add( cdsLocation );
    CRef< CSeq_loc > pLocCode2( new CSeq_loc( CSeq_loc::e_Mix ) );
    pLocCode2->Add( cdsLocation );

    CSeq_loc interval;
    interval.SetInt().SetId( cdsLocId );
    interval.SetStrand( cdsLocation.GetStrand() );

    for ( size_t u = 0; u < 3; ++u ) {

        size_t uLowest = pLocCode2->GetTotalRange().GetFrom();
        interval.SetInt().SetFrom( uLowest );
        interval.SetInt().SetTo( uLowest );
        pLocStartCodon = pLocStartCodon->Add( 
            interval, CSeq_loc::fSortAndMerge_All, 0 );
        pLocCode2 = pLocCode2->Subtract( 
            interval, CSeq_loc::fSortAndMerge_All, 0, 0 );    

        size_t uHighest = pLocCode->GetTotalRange().GetTo();
        interval.SetInt().SetFrom( uHighest );
        interval.SetInt().SetTo( uHighest );
        pLocStopCodon = pLocStopCodon->Add( 
            interval, CSeq_loc::fSortAndMerge_All, 0 );
        pLocCode = pLocCode->Subtract( 
            interval, CSeq_loc::fSortAndMerge_All, 0, 0 );    
    }

    if ( cdsLocation.GetStrand() == eNa_strand_minus ) {
        pLocCode = pLocCode2;
        pLocCode2 = pLocStartCodon;
        pLocStartCodon = pLocStopCodon;
        pLocStopCodon = pLocCode2;
    }

    pLocStartCodon->ChangeToPackedInt();
    pLocCode->ChangeToPackedInt();
    pLocStopCodon->ChangeToPackedInt();

    return true;
}

//  ----------------------------------------------------------------------------
void CGtfWriter::x_AddMultipleRecords(
    CGtfRecord& parent,
    CRef< CSeq_loc > pLocation,
    CGff2WriteRecordSet& set )
//  ----------------------------------------------------------------------------
{
    const list< CRef< CSeq_interval > >& sublocs =
        pLocation->GetPacked_int().Get();
    list< CRef< CSeq_interval > >::const_iterator it;
    for ( it = sublocs.begin(); it != sublocs.end(); ++it ) {
        const CSeq_interval& subint = **it;

        unsigned int uExonNumber = 0;
        for ( TExonCit xit = m_exonMap.begin(); xit != m_exonMap.end(); ++xit ) {
            const CSeq_interval& compint = *(xit->second);
            if ( compint.GetFrom() <= subint.GetFrom()  &&  compint.GetTo() >= subint.GetTo() ) {
                uExonNumber = xit->first;
                break;
            }
        }
            
        CGtfRecord* pRecord = new CGtfRecord( parent.FeatTree() );
        pRecord->MakeChildRecord( parent, subint, uExonNumber );

        if ( pRecord->Type() == "CDS" ||
            pRecord->Type() == "start_codon" || pRecord->Type() == "stop_codon" ) 
        {
            pRecord->SetCdsPhase( sublocs, pLocation->GetStrand() );
        }
        set.AddOrMergeRecord( pRecord );
    }
}

//  ----------------------------------------------------------------------------
SAnnotSelector CGtfWriter::x_GetAnnotSelector()
//  ----------------------------------------------------------------------------
{
    SAnnotSelector sel;
    sel.IncludeFeatType(CSeqFeatData::e_Gene);
    sel.IncludeFeatType(CSeqFeatData::e_Rna);
    sel.IncludeFeatType(CSeqFeatData::e_Cdregion);
    return sel;
}

//  ============================================================================
CGff2WriteRecord* CGtfWriter::x_CreateRecord(
    feature::CFeatTree& feat_tree )
//  ============================================================================
{
    return new CGtfRecord( feat_tree );
}

END_NCBI_SCOPE
