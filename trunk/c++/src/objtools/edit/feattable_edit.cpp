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
* Author: Frank Ludwig, NCBI
*
* File Description:
*   Convenience wrapper around some of the other feature processing finctions
*   in the xobjedit library.
*/

#include <ncbi_pch.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Gb_qual.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/edit/cds_fix.hpp>
#include <objtools/edit/loc_edit.hpp>
#include <objtools/edit/feattable_edit.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

//  -------------------------------------------------------------------------
CFeatTableEdit::CFeatTableEdit(
    CSeq_annot& annot,
    IMessageListener* pMessageListener):
//  -------------------------------------------------------------------------
    mAnnot(annot),
    mpMessageListener(pMessageListener),
    mNextFeatId(1),
	mLocusTagNumber(1)
{
    mpScope.Reset(new CScope(*CObjectManager::GetInstance()));
    mpScope->AddDefaults();
    mHandle = mpScope->AddSeq_annot(mAnnot);
    mEditHandle = mpScope->GetEditHandle(mHandle);
};

//  -------------------------------------------------------------------------
CFeatTableEdit::~CFeatTableEdit()
//  -------------------------------------------------------------------------
{
};

//  -------------------------------------------------------------------------
void CFeatTableEdit::InferParentMrnas()
//  -------------------------------------------------------------------------
{
    SAnnotSelector sel;
    sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_cdregion);
    CFeat_CI it(mHandle, sel);
    for ( ; it; ++it) {
        const CSeq_feat& cds = it->GetOriginalFeature();
        CRef<CSeq_feat> pRna = edit::MakemRNAforCDS(cds, *mpScope);
        if (!pRna) {
            continue;
        }
        //find proper name for rna
        string rnaId(xNextFeatId());
        pRna->SetId().SetLocal().SetStr(rnaId);
        //add rna xref to cds
        CSeq_feat_EditHandle feh(mpScope->GetObjectHandle(cds));
        feh.AddFeatXref(rnaId);
        //add new rna to feature table
        mEditHandle.AddFeat(*pRna);
    }
}

//  ---------------------------------------------------------------------------
void CFeatTableEdit::InferParentGenes()
//  ---------------------------------------------------------------------------
{
    SAnnotSelector sel;
    sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_mRNA);
    CFeat_CI it(mHandle, sel);
    for ( ; it; ++it) {
        const CSeq_feat& rna = it->GetOriginalFeature();
        CRef<CSeq_feat> pGene = xMakeGeneForMrna(rna, *mpScope);
        const CSeq_loc& rnaLoc = rna.GetLocation();
        if (!pGene) {
            continue;
        }
        //find proper name for gene
        string geneId(xNextFeatId());
        pGene->SetId().SetLocal().SetStr(geneId);
        //add gene xref to rna
        CSeq_feat_EditHandle feh(mpScope->GetObjectHandle(rna));
        feh.AddFeatXref(geneId);
        //add new gene to feature table
        mEditHandle.AddFeat(*pGene);
    }
}

//  ----------------------------------------------------------------------------
void CFeatTableEdit::InferPartials()
//  ----------------------------------------------------------------------------
{
    edit::CLocationEditPolicy editPolicy(
        edit::CLocationEditPolicy::ePartialPolicy_eSetForBadEnd,
        edit::CLocationEditPolicy::ePartialPolicy_eSetForBadEnd, 
        false, //extend 5'
        false, //extend 3'
        edit::CLocationEditPolicy::eMergePolicy_NoChange);

    SAnnotSelector sel;
    sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_cdregion);
    CFeat_CI it(mHandle, sel);
    for ( ; it; ++it) {
        const CSeq_feat& cds = it->GetOriginalFeature();
        CRef<CSeq_feat> pEditedCds(new CSeq_feat);
        pEditedCds->Assign(cds);
        editPolicy.ApplyPolicyToFeature(*pEditedCds, *mpScope);    
        CSeq_feat_EditHandle feh(mpScope->GetObjectHandle(cds));
        feh.Replace(*pEditedCds);
    }
}

//  ----------------------------------------------------------------------------
void CFeatTableEdit::EliminateBadQualifiers()
//  ----------------------------------------------------------------------------
{
    typedef CSeq_feat::TQual QUALS;

    CFeat_CI it(mHandle);
    for ( ; it; ++it) {
        CSeq_feat_EditHandle feh(mpScope->GetObjectHandle(
            (it)->GetOriginalFeature()));
        const QUALS& quals = (*it).GetQual();
        vector<string> badQuals;
        for (QUALS::const_iterator qual = quals.begin(); qual != quals.end(); 
                ++qual) {
            string qualVal = (*qual)->GetQual();
            CSeqFeatData::EQualifier qualType = CSeqFeatData::GetQualifierType(qualVal);
            if (qualType == CSeqFeatData::eQual_bad) {
                badQuals.push_back(qualVal);
            }
        }
        for (vector<string>::const_iterator badIt = badQuals.begin();
                badIt != badQuals.end(); ++badIt) {
            feh.RemoveQualifier(*badIt);
        }
    }
}

//  ----------------------------------------------------------------------------
void CFeatTableEdit::GenerateProteinIds()
//  ----------------------------------------------------------------------------
{
    SAnnotSelector sel;
    sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_cdregion);
    CFeat_CI it(mHandle, sel);
    for ( ; it; ++it) {
        const CSeq_feat& cds = it->GetOriginalFeature();
		string proteinId = this->xGetProteinId(cds, *mpScope);
		if (proteinId.empty()) {
			continue;
		}
        CRef<CSeq_feat> pEditedCds(new CSeq_feat);
        pEditedCds->Assign(cds);
		CRef<CGb_qual> pProteinId(new CGb_qual);
		pProteinId->SetQual("protein_id");
		pProteinId->SetVal(proteinId);
		pEditedCds->SetQual().push_back(pProteinId);
        CSeq_feat_EditHandle feh(mpScope->GetObjectHandle(cds));
        feh.Replace(*pEditedCds);
	}
}

//  ----------------------------------------------------------------------------
void CFeatTableEdit::GenerateTranscriptIds()
//  ----------------------------------------------------------------------------
{
    SAnnotSelector sel;
    sel.IncludeFeatSubtype(CSeqFeatData::eSubtype_mRNA);
    CFeat_CI it(mHandle, sel);
    for ( ; it; ++it) {
        const CSeq_feat& rna = it->GetOriginalFeature();
	}
}

//  ----------------------------------------------------------------------------
void CFeatTableEdit::GenerateLocusTags(
	const string& prefix)
//  ----------------------------------------------------------------------------
{
	CRef<CGb_qual> pLocusTag;

    SAnnotSelector selGenes;
    selGenes.IncludeFeatSubtype(CSeqFeatData::eSubtype_gene);
    CFeat_CI itGenes(mHandle, selGenes);
    for ( ; itGenes; ++itGenes) {
		string locusTagVal = itGenes->GetNamedQual("locus_tag");
		if (!locusTagVal.empty()) {
			continue;
		}
        CSeq_feat_EditHandle feh(mpScope->GetObjectHandle(
            (itGenes)->GetOriginalFeature()));
		feh.AddQualifier("locus_tag", xNextLocusTag(prefix));
	}
	SAnnotSelector selOther;
	selOther.ExcludeFeatSubtype(CSeqFeatData::eSubtype_gene);
    CFeat_CI itOther(mHandle, selOther);
	for ( ; itOther; ++itOther) {
        const CSeq_feat& feat = itOther->GetOriginalFeature();		
        CSeq_feat_EditHandle feh(mpScope->GetObjectHandle(
            (itOther)->GetOriginalFeature()));
		feh.RemoveQualifier("locus_tag");
		CConstRef<CSeq_feat> pGeneParent = xGetGeneParent(feat);
		if (!pGeneParent) {
			continue;
		}
		string locusTag = pGeneParent->GetNamedQual("locus_tag");
		feh.AddQualifier("locus_tag", locusTag);
	}
}

//  ----------------------------------------------------------------------------
string CFeatTableEdit::xNextFeatId()
//  ----------------------------------------------------------------------------
{
    const int WIDTH = 6;
    const string padding = string(WIDTH, '0');
    string suffix = NStr::NumericToString(mNextFeatId++);
    if (suffix.size() < WIDTH) {
        suffix = padding.substr(0, WIDTH-suffix.size()) + suffix;
    }
    string nextTag("auto");
    return nextTag + suffix;
}

//  ----------------------------------------------------------------------------
string CFeatTableEdit::xNextLocusTag(
	const string& prefix)
//  ----------------------------------------------------------------------------
{
    const int WIDTH = 6;
    const string padding = string(WIDTH, '0');
    string suffix = NStr::NumericToString(mLocusTagNumber++);
    if (suffix.size() < WIDTH) {
        suffix = padding.substr(0, WIDTH-suffix.size()) + suffix;
    }
    string nextTag = prefix + "_" + suffix;
    return nextTag;
}

//	----------------------------------------------------------------------------
CConstRef<CSeq_feat> CFeatTableEdit::xGetGeneParent(
	const CSeq_feat& feat)
//	----------------------------------------------------------------------------
{
	CConstRef<CSeq_feat> pGene;
	CSeq_feat_Handle sfh = mpScope->GetSeq_featHandle(feat);
	CSeq_annot_Handle sah = sfh.GetAnnot();
	if (!sah) {
		return pGene;
	}

    size_t bestLength(0);
    CFeat_CI findGene(sah, CSeqFeatData::eSubtype_gene);
    for ( ; findGene; ++findGene) {
        Int8 compare = sequence::TestForOverlap64(
            findGene->GetLocation(), feat.GetLocation(),
            sequence::eOverlap_Contained);
        if (compare == -1) {
            continue;
        }
        size_t currentLength = sequence::GetLength(findGene->GetLocation(), mpScope);
        if (!bestLength  ||  currentLength > bestLength) {
            pGene.Reset(&(findGene->GetOriginalFeature()));
            bestLength = currentLength;
        }
    }
	return pGene;
}


//  ----------------------------------------------------------------------------
string CFeatTableEdit::xGetProteinId(
    const CSeq_feat& cds,
    CScope& scope)
//  ----------------------------------------------------------------------------
{
	CConstRef<CSeq_feat> pGene = xGetGeneParent(cds);
    if (!pGene) {
		return "";
	}
	string locusTag = pGene->GetNamedQual("locus_tag");
	return locusTag;
}

//  ----------------------------------------------------------------------------
CRef<CSeq_feat> CFeatTableEdit::xMakeGeneForMrna(
    const CSeq_feat& rna,
    CScope& scope)
//  ----------------------------------------------------------------------------
{
    CRef<CSeq_feat> pGene;
    CSeq_feat_Handle sfh = scope.GetSeq_featHandle(rna);
    CSeq_annot_Handle sah = sfh.GetAnnot();
    if (!sah) {
        return pGene;
    }
    CConstRef<CSeq_feat> pExistingGene = xGetGeneParent(rna);
    if (pExistingGene) {
        return pGene;
    }
    pGene.Reset(new CSeq_feat);
    pGene->SetLocation().SetInt();
    pGene->SetLocation().SetId(*rna.GetLocation().GetId());
    pGene->SetLocation().SetInt().SetFrom(rna.GetLocation().GetStart(
        eExtreme_Positional));
    pGene->SetLocation().SetInt().SetTo(rna.GetLocation().GetStop(
        eExtreme_Positional));
    pGene->SetData().SetGene();
    return pGene;
}

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

