#ifndef OBJMGR_UTIL_AUTODEF_FEATURE_CLAUSE_BASE__HPP
#define OBJMGR_UTIL_AUTODEF_FEATURE_CLAUSE_BASE__HPP

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
* Author:  Colleen Bollin
*
* File Description:
*   Creates unique definition lines for sequences in a set using organism
*   descriptions and feature clauses.
*/

#include <corelib/ncbistd.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include <objmgr/util/seq_loc_util.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
    
    
class NCBI_XOBJUTIL_EXPORT CAutoDefFeatureClause_Base
{
public:
    typedef vector<CAutoDefFeatureClause_Base *> TClauseList;

    CAutoDefFeatureClause_Base(bool suppress_locus_tags);
    ~CAutoDefFeatureClause_Base();
    
    void AddSubclause (CAutoDefFeatureClause_Base *subclause);

    string PrintClause(bool print_typeword, bool typeword_is_plural);
    
    virtual CSeqFeatData::ESubtype  GetMainFeatureSubtype() { return CSeqFeatData::eSubtype_bad; }
    virtual void Label();
    virtual bool AddmRNA (CAutoDefFeatureClause_Base *mRNAClause);
    virtual bool AddGene (CAutoDefFeatureClause_Base *gene_clause);
    
    virtual sequence::ECompare CompareLocation(const CSeq_loc& loc);
    virtual void AddToOtherLocation(CRef<CSeq_loc> loc);
    virtual void AddToLocation(CRef<CSeq_loc> loc);
    virtual bool SameStrand(const CSeq_loc& loc);
    virtual bool IsPartial() { return false; }
    virtual bool IsTransposon() { return false; }
    virtual bool IsInsertionSequence() { return false; }
    virtual bool IsEndogenousVirusSourceFeature() { return false; }
    virtual bool IsGeneCluster() { return false; }
    virtual bool IsNoncodingProductFeat() { return false; }
    virtual bool IsIntergenicSpacer() { return false; }
    virtual bool IsSatelliteClause() { return false; }    
    virtual CAutoDefFeatureClause_Base *FindBestParentClause(CAutoDefFeatureClause_Base * subclause, bool gene_cluster_opp_strand);
    
    void GroupClauses(bool gene_cluster_opp_strand);

    virtual CRef<CSeq_loc> GetLocation();
    
    string ListClauses(bool allow_semicolons, bool suppress_final_and);

    bool IsGeneMentioned(CAutoDefFeatureClause_Base *gene_clause);
    bool IsTypewordFirst() { return m_ShowTypewordFirst; }
    bool DisplayAlleleName ();

    string GetInterval() { return m_Interval; }
    string GetTypeword() { return m_Typeword; }
    string GetDescription() { return m_Description; }
    string GetProductName() { return m_ProductName; }
    string GetGeneName() { return m_GeneName; }
    string GetAlleleName() { return m_AlleleName; }
    bool   GetGeneIsPseudo() { return m_GeneIsPseudo; }
    bool   NeedPlural() { return m_MakePlural; }
    bool   IsAltSpliced() { return m_IsAltSpliced; }
    void   SetAltSpliced(string splice_name);
    bool IsMarkedForDeletion() { return m_DeleteMe; }
    void MarkForDeletion() { m_DeleteMe = true; }
    void SetMakePlural() { m_MakePlural = true; }
    void SetSuppressLocusTag(bool do_suppress) { m_SuppressLocusTag = do_suppress; }
    void PluralizeInterval();
    void PluralizeDescription();
    
    // Grouping functions
    void RemoveDeletedSubclauses();
   
    void GroupmRNAs();
    void GroupGenes();
    void RemoveGenesMentionedElsewhere();
    void ConsolidateRepeatedClauses();
    void FindAltSplices();
    void TransferSubclauses(TClauseList &other_clause_list);
    void CountUnknownGenes();
   
    virtual bool OkToGroupUnderByType(CAutoDefFeatureClause_Base *parent_clause) { return false; }
    virtual bool OkToGroupUnderByLocation(CAutoDefFeatureClause_Base *parent_clause, bool gene_cluster_opp_strand) { return false; }
   
protected:
    CAutoDefFeatureClause_Base();
    TClauseList  m_ClauseList;

    string       m_GeneName;
    string       m_AlleleName;
    bool         m_GeneIsPseudo;
    string       m_Interval;
    bool         m_IsAltSpliced;
    bool         m_HasmRNA;
    bool         m_HasGene;
    bool         m_MakePlural;
    bool         m_IsUnknown;
    bool         m_ClauseInfoOnly;
    bool m_Pluralizable;
    bool m_ShowTypewordFirst;
    string m_Typeword;
    bool   m_TypewordChosen;
    string m_Description;
    bool   m_DescriptionChosen;
    string m_ProductName;
    bool   m_ProductNameChosen;
    bool m_SuppressLocusTag;
    
    bool   m_DeleteMe;

    unsigned int x_LastIntervalChangeBeforeEnd ();
    bool x_OkToConsolidate (unsigned int clause1, unsigned int clause2);
    bool x_MeetAltSpliceRules (unsigned int clause1, unsigned int clause2, string &splice_name);

};


class NCBI_XOBJUTIL_EXPORT CAutoDefUnknownGeneList : public CAutoDefFeatureClause_Base
{
public:    
    CAutoDefUnknownGeneList(bool suppress_locus_tags);
    ~CAutoDefUnknownGeneList();
  
    virtual void Label();
    virtual bool IsRecognizedFeature() { return true; }
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.2  2006/04/17 17:39:36  ucko
* Fix capitalization of header filenames.
*
* Revision 1.1  2006/04/17 16:25:01  bollin
* files for automatically generating definition lines, using a combination
* of modifiers to make definition lines unique within a set and listing the
* relevant features on the sequence.
*
*
* ===========================================================================
*/

#endif //OBJMGR_UTIL_AUTODEF_FEATURE_CLAUSE_BASE__HPP
