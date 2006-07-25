#ifndef CLEANUP___CLEANUPP__HPP
#define CLEANUP___CLEANUPP__HPP

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
* Author:  Robert Smith
*
* File Description:
*   Implementation of Basic Cleanup of CSeq_entries.
*
*/
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objmgr/scope.hpp>
#include <objtools/cleanup/cleanup_change.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CSeq_submit;
class CBioseq;
class CBioseq_set;
class CSeq_annot;
class CSeq_feat;
class CSeqFeatData;
class CSeq_descr;
class CSeqdesc;
class CGene_ref;
class CProt_ref;
class CRNA_ref;
class CImp_feat;
class CGb_qual;
class CDbtag;
class CUser_field;
class CUser_object;
class CObject_id;
class CGB_block;
class CPubdesc;
class CPub_equiv;
class CPub;
class CCit_gen;
class CCit_sub;
class CCit_art;
class CCit_book;
class CCit_pat;
class CCit_let;
class CCit_proc;
class CAuth_list;
class CAuthor;
class CAffil;
class CPerson_id;
class CName_std;
class CBioSource;
class COrg_ref;
class COrgName;
class COrgMod;
class CSubSource;


/// right now a slightly different cleanup is performed for EMBL/DDBJ and
/// SwissProt records. All other types are handled as GenBank records.
enum ECleanupMode
{
    eCleanup_NotSet,
    eCleanup_EMBL,
    eCleanup_DDBJ,
    eCleanup_SwissProt,
    eCleanup_GenBank
};


class CCleanup_imp
{
public:
    CCleanup_imp(CRef<CCleanupChange> changes, CRef<CScope> scope, Uint4 options = 0);
    virtual ~CCleanup_imp();
        
    /// Cleanup a Seq-entry. 
    void BasicCleanup(CSeq_entry& se);
    /// Cleanup a Seq-submit.
    void BasicCleanup(CSeq_submit& ss);
    /// Cleanup a Bioseq. 
    void BasicCleanup(CBioseq& bs);
    /// Cleanup a Bioseq_set.
    void BasicCleanup(CBioseq_set& bss);
    /// Cleanup a Seq-Annot. 
    void BasicCleanup(CSeq_annot& sa);
    /// Cleanup a Seq-feat.
    void BasicCleanup(CSeq_feat& sf);
    
    //Extended Cleanup
    void ExtendedCleanup(CSeq_entry_Handle seh);
    void ExtendedCleanup(CSeq_submit& ss);
    void ExtendedCleanup(CBioseq_Handle bsh);
    void ExtendedCleanup(CBioseq_set_Handle bss);
    void ExtendedCleanup(CSeq_annot_Handle sa);
    
    void RenormalizeNucProtSets (CBioseq_set_Handle bsh);

private:
    void Setup(const CSeq_entry& se);
    void Finish(CSeq_entry& se);
    
    void ChangeMade(CCleanupChange::EChanges e);

    // Cleanup other sub-objects.
    void BasicCleanup(CSeq_descr& sdr);
    void BasicCleanup(CSeqdesc& sd);
    void BasicCleanup(CSeqFeatData& data);
    void BasicCleanup(CGene_ref& gene_ref);
    void BasicCleanup(CProt_ref& prot_ref);
    void BasicCleanup(CRNA_ref& rna_ref);
    void BasicCleanup(CImp_feat& imf);
    void BasicCleanup(CGb_qual& gbq);
    void BasicCleanup(CDbtag& dbtag);
    void BasicCleanup(CGB_block& gbb);
    void BasicCleanup(CPubdesc& pd);
    void BasicCleanup(CPub_equiv& pe);
    void BasicCleanup(CPub& pub, bool fix_initials);
    void BasicCleanup(CCit_gen& cg, bool fix_initials);
    void BasicCleanup(CCit_sub& cs, bool fix_initials);
    void BasicCleanup(CCit_art& ca, bool fix_initials);
    void BasicCleanup(CCit_book& cb, bool fix_initials);
    void BasicCleanup(CCit_pat&  cp, bool fix_initials);
    void BasicCleanup(CCit_let&  cl, bool fix_initials);
    void BasicCleanup(CCit_proc& cp, bool fix_initials);
    void BasicCleanup(CAuth_list& al, bool fix_initials);
    void BasicCleanup(CAuthor& au, bool fix_initials);
    void BasicCleanup(CAffil& af);
    void BasicCleanup(CPerson_id& pid, bool fix_initials);
    void BasicCleanup(CName_std& name, bool fix_initials);
    void BasicCleanup(CBioSource& bs);
    void BasicCleanup(COrg_ref& oref);
    void BasicCleanup(COrgName& on);
    void BasicCleanup(COrgMod& om);
    void BasicCleanup(CSubSource& ss);
    void BasicCleanup(CObject_id& oid);
    void BasicCleanup(CUser_field& field);
    void BasicCleanup(CUser_object& uo);

    void BasicCleanup(CSeq_feat& feat, CSeqFeatData& data);
    bool BasicCleanup(CSeq_feat& feat, CGb_qual& qual);
    bool BasicCleanup(CGene_ref& gene, const CGb_qual& gb_qual);
    bool BasicCleanup(CSeq_feat& feat, CRNA_ref& rna, const CGb_qual& gb_qual);
    bool BasicCleanup(CProt_ref& rna, const CGb_qual& gb_qual);
    bool BasicCleanup(CSeq_feat& feat, CCdregion& cds, const CGb_qual& gb_qual);

    // cleaning up Seq_feat parts.
    void x_CleanupExcept_text(string& except_text);
    void x_CleanupRna(CSeq_feat& feat);
    void x_AddReplaceQual(CSeq_feat& feat, const string& str);
    void x_CombineSplitQual(string& val, string& new_val);

    // Gb_qual cleanup.
    void x_ExpandCombinedQuals(CSeq_feat::TQual& quals);
    void x_CleanupConsSplice(CGb_qual& gbq);
    bool x_CleanupRptUnit(CGb_qual& gbq);

    // Dbtag cleanup.
    void x_TagCleanup(CObject_id& tag);
    void x_DbCleanup(string& db);
    
    // pub axuiliary functions.
    void x_FlattenPubEquiv(CPub_equiv& pe);

    // CName_std cleanup 
    void x_FixEtAl(CName_std& name);
    void x_FixInitials(CName_std& name);
    void x_ExtractSuffixFromInitials(CName_std& name);
    void x_FixSuffix(CName_std& name);

    void x_OrgModToSubtype(CBioSource& bs);
    void x_SubtypeCleanup(CBioSource& bs);
    void x_ModToOrgMod(COrg_ref& oref);
    // cleanup strings in User objects and fields
    void x_CleanupUserString(string& str);

    bool x_ParseCodeBreak(CSeq_feat& feat, CCdregion& cds, string str);

    // Extended Cleanup
    typedef void (CCleanup_imp::*RecurseDescriptor)(CSeq_descr& sdr);
    void x_RecurseForDescriptors (CBioseq_Handle bs, RecurseDescriptor pmf);
    void x_RecurseForDescriptors (CBioseq_set_Handle bs, RecurseDescriptor pmf);
    
    typedef bool (CCleanup_imp::*IsMergeCandidate)(const CSeqdesc& sd);
    typedef bool (CCleanup_imp::*Merge)(CSeqdesc& sd1, CSeqdesc& sd2);
    void x_RecurseDescriptorsForMerge (CBioseq_Handle bs, IsMergeCandidate is_can, Merge do_merge);
    void x_RecurseDescriptorsForMerge (CBioseq_set_Handle bs, IsMergeCandidate is_can, Merge do_merge);

    typedef void (CCleanup_imp::*RecurseSeqAnnot)(CSeq_annot_Handle sah);
    void x_RecurseForSeqAnnots (CBioseq_Handle bs, RecurseSeqAnnot);
    void x_RecurseForSeqAnnots (CBioseq_set_Handle bs, RecurseSeqAnnot);
    
    void x_MolInfoUpdate(CSeq_descr& sdr);
    
    void x_RemoveEmptyGenbankDesc(CSeq_descr& sdr);

    void x_CleanGenbankBlockStrings (CGB_block& block);
    void x_CleanGenbankBlockStrings (CSeq_descr& sdr);
    
    void x_RemoveEmptyFeatures (CSeq_annot_Handle sa);
    
    void x_RemoveMultipleTitles (CSeq_descr& sdr);
    
    void x_MergeMultipleDates (CSeq_descr& sdr);

    void x_ExtendedCleanStrings (CSeqdesc& sd);
    void x_ExtendedCleanStrings (COrg_ref& org);
    void x_CleanOrgNameStrings (COrgName& on);
    void x_ExtendedCleanSubSourceList (CBioSource& bs);
    
    void x_MergeAdjacentAnnots (CBioseq_Handle bs);
    void x_MergeAdjacentAnnots (CBioseq_set_Handle bss);
    
    void x_ConvertFullLenFeatureToDescriptor (CSeq_annot_Handle sa, CSeqFeatData::E_Choice choice);
    void x_ConvertFullLenSourceFeatureToDescriptor (CSeq_annot_Handle sah);
    void x_ConvertFullLenPubFeatureToDescriptor (CSeq_annot_Handle sah);

    void x_CorrectExceptText (string& except_text);
    void x_CorrectExceptText( CSeq_feat& feat);
    void x_CorrectExceptText (CSeq_annot_Handle sa);

    void x_MoveDbxrefs( CSeq_feat& feat);
    void x_MoveDbxrefs (CSeq_annot_Handle sa);
    
    bool x_CheckCodingRegionEnds (CSeq_feat& feat);
    void x_CheckCodingRegionEnds (CSeq_annot_Handle sah);
    
    void x_ExtendSingleGeneOnmRNA (CBioseq_set_Handle bssh);
    void x_ExtendSingleGeneOnmRNA (CBioseq_Handle bsh);
    
    void x_MoveFeaturesOnPartsSets (CSeq_annot_Handle sa);
    void x_RemovePseudoProducts (CSeq_annot_Handle sa);

    void RemoveEmptyFeaturesDescriptorsAndAnnots (CBioseq_Handle bs);
    void RemoveEmptyFeaturesDescriptorsAndAnnots (CBioseq_set_Handle bs);
    
    void x_ExtendedCleanStrings (CSeq_descr& sdr);
    void x_ExtendedCleanStrings (CSeq_annot_Handle sah);
    void x_ExtendedCleanStrings (CSeq_feat& feat);
    void x_ExtendedCleanStrings (CGene_ref& gene_ref);
    void x_ExtendedCleanStrings (CProt_ref& prot_ref);
    void x_ExtendedCleanStrings (CRNA_ref& rna_ref);
    void x_ExtendedCleanStrings (CPubdesc& pd);
    void x_ExtendedCleanStrings (CImp_feat& imf);

    bool x_IsCitSubPub(const CSeqdesc& sd);
    bool x_CitSubsMatch(CSeqdesc& sd1, CSeqdesc& sd2);

    void x_MergeDuplicateBioSources (CBioSource& src, CBioSource& add_src);
    bool x_IsMergeableBioSource(const CSeqdesc& sd);    
    bool x_MergeDuplicateBioSources(CSeqdesc& sd1, CSeqdesc& sd2);

    // Prohibit copy constructor & assignment operator
    CCleanup_imp(const CCleanup_imp&);
    CCleanup_imp& operator= (const CCleanup_imp&);
    
    CRef<CCleanupChange>    m_Changes;
    Uint4                   m_Options;
    ECleanupMode            m_Mode;
    CRef<CScope>            m_Scope;
    
};


END_SCOPE(objects)
END_NCBI_SCOPE


/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.34  2006/07/25 14:36:47  bollin
 * added method to ExtendedCleanup to remove products on coding regions marked
 * as pseudo.
 *
 * Revision 1.33  2006/07/18 16:43:43  bollin
 * added x_RecurseDescriptorsForMerge and changed the ExtendedCleanup functions
 * for merging duplicate BioSources and equivalent CitSubs to use the new function
 *
 * Revision 1.32  2006/07/13 17:10:57  rsmith
 * report if changes made
 *
 * Revision 1.31  2006/07/11 14:38:28  bollin
 * aadded a step to ExtendedCleanup to extend the only gene found on the only
 * mRNA sequence in the set where there are zero or one coding region features
 * in the set so that the gene covers the entire mRNA sequence
 *
 * Revision 1.30  2006/07/10 19:01:57  bollin
 * added step to extend coding region to cover missing portion of a stop codon,
 * will also adjust the location of the overlapping gene if necessary.
 *
 * Revision 1.29  2006/07/05 17:26:11  bollin
 * cleared compiler error
 *
 * Revision 1.28  2006/07/05 16:43:34  bollin
 * added step to ExtendedCleanup to clean features and descriptors
 * and remove empty feature table seq-annots
 *
 * Revision 1.27  2006/07/03 18:45:00  bollin
 * changed methods in ExtendedCleanup for correcting exception text and moving
 * dbxrefs to use edit handles
 *
 * Revision 1.26  2006/07/03 17:48:04  bollin
 * changed RemoveEmptyFeatures method to use edit handles
 *
 * Revision 1.25  2006/07/03 17:02:32  bollin
 * converted steps for ExtendedCleanup that convert full length pub and source
 * features to descriptors to use edit handles
 *
 * Revision 1.24  2006/07/03 15:27:38  bollin
 * rewrote x_MergeAddjacentAnnots to use edit handles
 *
 * Revision 1.23  2006/07/03 14:51:11  bollin
 * corrected compiler errors
 *
 * Revision 1.22  2006/07/03 12:33:48  bollin
 * use edit handles instead of operating directly on the data
 * added step to renormalize nuc-prot sets to ExtendedCleanup
 *
 * Revision 1.21  2006/06/28 15:23:03  bollin
 * added step to move db_xref GenBank Qualifiers to real dbxrefs to Extended Cleanup
 *
 * Revision 1.20  2006/06/28 13:22:39  bollin
 * added step to merge duplicate biosources to ExtendedCleanup
 *
 * Revision 1.19  2006/06/27 18:43:02  bollin
 * added step for merging equivalent cit-sub publications to ExtendedCleanup
 *
 * Revision 1.18  2006/06/27 14:30:59  bollin
 * added step for correcting exception text to ExtendedCleanup
 *
 * Revision 1.17  2006/06/27 13:18:47  bollin
 * added steps for converting full length pubs and sources to descriptors to
 * Extended Cleanup
 *
 * Revision 1.16  2006/06/22 18:16:01  bollin
 * added step to merge adjacent Seq-Annots to ExtendedCleanup
 *
 * Revision 1.15  2006/06/22 17:29:15  bollin
 * added method to merge multiple create dates to Extended Cleanup
 *
 * Revision 1.14  2006/06/22 15:39:00  bollin
 * added step for removing multiple titles to Extended Cleanup
 *
 * Revision 1.13  2006/06/22 13:28:29  bollin
 * added step to remove empty features to ExtendedCleanup
 *
 * Revision 1.12  2006/06/21 17:21:28  bollin
 * added cleanup of GenbankBlock descriptor strings to ExtendedCleanup
 *
 * Revision 1.11  2006/06/21 14:12:59  bollin
 * added step for removing empty GenBank block descriptors to ExtendedCleanup
 *
 * Revision 1.10  2006/06/20 19:43:39  bollin
 * added MolInfoUpdate to ExtendedCleanup
 *
 * Revision 1.9  2006/05/17 17:39:36  bollin
 * added parsing and cleanup of anticodon qualifiers on tRNA features and
 * transl_except qualifiers on coding region features
 *
 * Revision 1.8  2006/04/18 14:32:36  rsmith
 * refactoring
 *
 * Revision 1.7  2006/04/17 17:03:12  rsmith
 * Refactoring. Get rid of cleanup-mode parameters.
 *
 * Revision 1.6  2006/04/05 14:01:22  dicuccio
 * Don't export internal classes
 *
 * Revision 1.5  2006/03/29 19:42:16  rsmith
 * Delete   x_CleanupExtName(CRNA_ref& rna_ref);
 * and x_CleanupExtTRNA(CRNA_ref& rna_ref);
 *
 * Revision 1.4  2006/03/29 16:38:14  rsmith
 * various implementation declaration changes.
 *
 * Revision 1.3  2006/03/23 18:31:40  rsmith
 * new BasicCleanup routines, particularly User Objects/Fields.
 *
 * Revision 1.2  2006/03/20 14:22:15  rsmith
 * add cleanup for CSeq_submit
 *
 * Revision 1.1  2006/03/14 20:21:50  rsmith
 * Move BasicCleanup functionality from objects to objtools/cleanup
 *
 *
 * ===========================================================================
 */

#endif  /* CLEANUP___CLEANUP__HPP */
