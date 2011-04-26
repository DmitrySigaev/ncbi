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
 * File Description:  Write gff3 file
 *
 */

#ifndef OBJTOOLS_READERS___GFF3_WRITER__HPP
#define OBJTOOLS_READERS___GFF3_WRITER__HPP

#include <corelib/ncbistd.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objtools/writers/gff_writer.hpp>
#include <objtools/writers/gff3_write_data.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

class CGffAlignmentRecord;

//  ============================================================================
class NCBI_XOBJWRITE_EXPORT CGff3Writer
//  ============================================================================
    : public CGff2Writer
{
public:
    CGff3Writer(
        CScope&,
        CNcbiOstream&,
        unsigned int = fNormal );
    CGff3Writer(
        CNcbiOstream&,
        unsigned int = fNormal );
    virtual ~CGff3Writer();

protected:
    bool x_WriteAlign( 
        const CSeq_align&,
        bool=false );                   // invert width 
    bool x_WriteAlignDenseg(
        const CSeq_align&,
        bool=false );                   // invert width 
    bool x_WriteAlignSpliced(
        const CSeq_align&,    
        bool=false );                   // invert width 
    bool x_WriteAlignDisc(
        const CSeq_align&,    
        bool=false );                   // invert width 

    virtual bool x_WriteHeader();
//    TFlags m_uFlags;

    virtual bool x_WriteFeature(
        feature::CFeatTree&,
        CMappedFeat );
    virtual bool x_WriteFeatureGene(
        feature::CFeatTree&,
        CMappedFeat );
    virtual bool x_WriteFeatureMrna(
        feature::CFeatTree&,
        CMappedFeat );
    virtual bool x_WriteFeatureCds(
        feature::CFeatTree&,
        CMappedFeat );
    virtual bool x_WriteFeatureGeneric(
        feature::CFeatTree&,
        CMappedFeat );

    void x_WriteAlignment( 
        const CGffAlignmentRecord& record );

    string x_GetParentId(
        CMappedFeat );

protected:
    unsigned int m_uRecordId;
 
    typedef map< CMappedFeat, CRef<CGff3WriteRecordFeature> > TGeneMap;
    TGeneMap m_GeneMap;
    unsigned int m_uPendingGeneId;

    typedef map< CMappedFeat, CRef<CGff3WriteRecordFeature> > TMrnaMap;
    TMrnaMap m_MrnaMap;
    unsigned int m_uPendingMrnaId;
    unsigned int m_uPendingExonId;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJTOOLS_WRITERS___GFF3_WRITER__HPP
