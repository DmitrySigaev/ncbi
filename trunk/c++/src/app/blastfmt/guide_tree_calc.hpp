#ifndef GUIDE_TREE_CALC__HPP
#define GUIDE_TREE_CALC__HPP
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
 * Author:  Irena Zaretskaya, Greg Boratyn
 *
 */

/// @phylo_tree_calc.hpp
/// Phylogenetic tree calculation
///
/// This file provides various functions used for phylo tree calculation. 


#include <corelib/ncbiobj.hpp>

#include <algo/phy_tree/bio_tree.hpp>
#include <algo/phy_tree/bio_tree_conv.hpp>
#include <algo/phy_tree/dist_methods.hpp>

#include <objects/biotree/BioTreeContainer.hpp>
#include <objects/biotree/NodeSet.hpp>

#include <objtools/alnmgr/alnmix.hpp>

#include <gui/widgets/data/align_ds.hpp>

#include <corelib/hash_set.hpp>


USING_NCBI_SCOPE;
USING_SCOPE(objects);


/// Computaion of distance-based phylognetic/guide tree
class CGuideTreeCalc : public CObject {
    
public:

    /// Methods for computing evolutionary distance
    ///
    enum EDistMethod {
        eJukesCantor, ePoisson, eKimura, eGrishin, eGrishinGeneral
    };


    /// Algorithms for phylogenetic tree reconstruction
    ///
    enum ETreeMethod {
        eNJ,      ///< Neighbor Joining
        eFastME,  ///< Fast Minumum Evolution
        eCobalt   ///< Cobalt tree
    };


    /// Information shown as labels in the guide tree
    ///
    enum ELabelType {
        eTaxName, eSeqTitle, eBlastName, eSeqId, eSeqIdAndBlastName
    };



    /// Feature IDs used in guide tree
    ///
    enum EFeatureID {
        eLabelId = 0,       ///< Node label
        eDistId,            ///< Edge length from parent to this node
        eSeqIdId,           ///< Sequence id
        eOrganismId,        ///< Taxonomic organism id (for sequence)
        eTitleId,           ///< Sequence title
        eAccessionNbrId,    ///< Sequence accession
        eBlastNameId,       ///< Sequence Blast Name
        eAlignIndexId,      ///< Index of sequence in Seq_align
        eNodeColorId,       ///< Node color
        eLabelColorId,      ///< Node label color
        eLabelBgColorId,    ///< Color for backgroud of node label
        eLabelTagColorId,
        eTreeSimplificationTagId, ///< Is subtree collapsed
        eNumIds             ///< Number of known feature ids
    };


    
    typedef pair<string, string> TBlastNameToColor;
    typedef vector<TBlastNameToColor> TBlastNameColorMap;


public:

    //--- Constructors ---    

    /// Create CGuideTreeCalc from Seq-align
    /// @param seq_aln Seq-align [in]
    /// @param scope Scope [in]
    /// @param query_id Seqid of query sequence [in]
    ///
    CGuideTreeCalc(const CSeq_align& seq_aln, CRef<CScope> scope,
                   const string& query_id = "");

    /// Create CGuideTreeCalc from CSeq_align_set
    /// @param annot CSeq_align_set [in]
    /// @param scope Scope [in]    
    ///
    CGuideTreeCalc(CRef<CSeq_align_set> &seqAlignSet, CRef<CScope> scope);

    ~CGuideTreeCalc() {}


    //--- Setters ---

    /// Set maximum allowed divergence between sequences included in tree
    //  reconstruction
    /// @param div [in]
    ///
    void SetMaxDivergence(double div) {m_MaxDivergence = div;}


    /// Set evolutionary correction method for computing distance between
    /// sequences
    /// @param method Distance method [in]
    ///
    void SetDistMethod(EDistMethod method) {m_DistMethod = method;}


    /// Set algorithm for phylogenetic tree computation
    /// @param method Tree compuutation method [in]
    ///
    void SetTreeMethod(ETreeMethod method) {m_TreeMethod = method;}


    /// Set type of labels in guide tree
    /// @param label_type Label type
    ///
    void SetLabelType(ELabelType label_type) {m_LabelType = label_type;}


    /// Set marking of query node in the tree
    /// @param mark If true, query node will be marked
    ///
    void SetMarkQueryNode(bool mark) {m_MarkQueryNode = mark;}


    //--- Getters ---

    /// Get serial tree
    /// @return Tree
    ///
    const CBioTreeContainer& GetSerialTree(void) const;


    /// Get tree
    /// @return Tree
    ///
    auto_ptr<CBioTreeDynamic> GetTree(void);


    /// Get seq_align that corresponds to current tree
    /// @return Seq_align
    ///
    CRef<CSeq_align> GetSeqAlign(void) const;


    /// Get maximum allowed diveregence between sequences included in tree
    /// reconstruction
    /// @return Max divergence
    ///
    double GetMaxDivergence(void) const {return m_MaxDivergence;}


    /// Get evolutionary correction method for computing distance between
    /// sequences
    /// @return Distance method
    ///
    EDistMethod GetDistMethod(void) const {return m_DistMethod;}


    /// Get tree node id for the query sequence
    /// @return Query node id
    ///
    int GetQueryNodeId(void) const {return m_QueryNodeId;}


    /// Get type of tree labels
    /// @return Type of tree labels
    ///
    ELabelType GetLabelType(void) const {return m_LabelType;}


    /// Is query node going to be marked
    /// @return Query node marked if true, not marked otherwise
    ///
    bool GetMarkQueryNode(void) const {return m_MarkQueryNode;}


    /// Get ids of sequences excluded from tree computation
    /// @return Ids of excluded sequences
    ///
    const vector<string>& GetRemovedSeqIds(void) const {return m_RemovedSeqIds;}

    /// Get BlastName-to-Color map
    /// @return BlastName-to-color map
    ///
    const TBlastNameColorMap& GetBlastNameColorMap(void) const
    {return m_BlastNameColorMap;}

    /// Get error/warning messages
    /// @return List of messages
    ///
    const vector<string>& GetMessages(void) const {return m_Messages;}

    /// Check whether there are any messages
    /// @return True if there are messages, false otherwise
    ///
    bool IsMessage(void) const {return m_Messages.size() > 0;}


    //--- Tree computation ---

    /// Compute bio tree for the current alignment in a black box manner
    /// @return True is computation successful, false otherwise
    ///
    bool CalcBioTree(void);


    /// Compute distance matrix -- apply evolutionary correction to divergence
    /// @param pmat Divergence matrix [in]
    /// @param result Distance matrix [out]
    /// @param method Distance computation method [in]
    ///
    static void CalcDistMatrix(const CDistMethods::TMatrix& pmat,
                               CDistMethods::TMatrix& result,
                               EDistMethod method);


    /// Create and initialize tree features. Initializes node labels,
    /// descriptions, colors, etc.
    /// @param btc Tree for which features are to be initialized [in|out]
    /// @param seqids Sequence ids each corresponding to a tree leaf [in]
    /// @param scope Scope [in]
    /// @param label_type Type of labels to for tree leaves [in]
    /// @param mark_query_node Is query node to be marked [in]
    /// @param bcolormap Blast name to node color map [out]
    /// @param query_node_id Id of query node (set only if mark_query_node
    /// equal to true) [out]
    ///
    /// Tree leaves must have labels as numbers from zero to number of leaves
    /// minus 1. This function does not initialize distance feature.
    static void InitTreeFeatures(CBioTreeContainer& btc,
                                 const vector< CRef<CSeq_id> >& seqids,
                                 CScope& scope,
                                 CGuideTreeCalc::ELabelType label_type,
                                 bool mark_query_node,
                                 TBlastNameColorMap& bcolormap,
                                 int& query_node_id);

protected:

    /// Initialize class attributes
    void x_Init(void);

    /// Initialize alignment data source
    /// @param seq_aln Seq-align [in]
    /// @return True if success, false otherwise
    void x_InitAlignDS(const CSeq_align& seq_aln);

    /// Initialize alignment data source
    /// @param seqAlignSet CSeq_align_set [in]
    /// @return True if success, false otherwise
    bool x_InitAlignDS(CRef<CSeq_align_set> &seqAlignSet);

    /// Initialize tree freatures
    /// @param alnvec Alignment vector [in]
    void x_InitTreeFeatures(const CAlnVec& alnvec);


    /// Add feature descriptor to tree
    /// @param id Feature id [in]
    /// @param desc Feature description [in]
    /// @param btc Tree [in|out]
    static void x_AddFeatureDesc(int id, const string& desc,
                                 CBioTreeContainer& btc); 

    /// Add feature to tree node
    /// @param id Feature id [in]
    /// @param value Feature value [in]
    /// @param iter Tree node iterator [in|out]
    static void x_AddFeature(int id, const string& value,
                             CNodeSet::Tdata::iterator iter);    


    /// Compute divergence matrix and find sequences to exclude from tree
    /// reconstruction
    /// @param pmat Divergence matrix [out]
    /// @param max_divergence Maximum allowed divergence [in]
    /// @param removed Set of indices of excluded sequences [out]
    /// @return True if at least two sequences are left for tree computation,
    /// false otherwise
    ///
    bool x_CalcDivergenceMatrix(CDistMethods::TMatrix& pmat,
                              double max_divergence,
                              hash_set<int>& removed) const;

    /// Remove entries coresponding to excluded sequences from divergence
    /// matrix
    /// @param pmat Divergence matrix [in|out]
    /// @param used_inds Indices of sequences included in tree computation [in]
    ///
    static void x_TrimMatrix(CDistMethods::TMatrix& pmat,
                             const vector<int>& used_inds);


    /// Create alignment only for sequences included in tree computation
    /// @param removed_inds Indices of excluded sequences [in]
    /// @param new_align_index Indices of included sequences [out]
    /// @return New alignment vector
    ///
    CRef<CAlnVec> x_CreateValidAlign(const hash_set<int>& removed_inds,
                                     vector<int>& new_align_index);

    /// Create alignmnet composed of subset of sequences
    /// @param alnvec Initial alignment vector [in]
    /// @param align_index Indices of sequences included in the subset [in]
    /// @return New alignment vector
    CRef<CAlnVec> x_CreateSubsetAlign(const CRef<CAlnVec>& alnvec,
                                      const vector<int>& align_index);

    
    /// Compute phylogenetic tree
    /// @param alnvec Alignment vector [in]
    /// @param dmat Distance matrix [in]
    /// @param method Tree reconstruction method [in]
    /// @param correct Whether negative tree egde lengths should be set to zero
    /// [in]
    ///
    void x_ComputeTree(const CAlnVec& alnvec, const CDistMethods::TMatrix& dmat,
                       ETreeMethod method, bool correct = true);

protected:

    /// Scope
    CRef<CScope> m_Scope;

    /// Alignment data source
    CRef<CAlignDataSource> m_AlignDataSource;

    /// Seq-id of query sequence
    string m_QuerySeqId;

    /// Id of node that represents query sequence
    int m_QueryNodeId;

    /// Maximum allowed divergence between sequences
    double m_MaxDivergence;

    /// Method of calculating evolutionary distance correction
    EDistMethod m_DistMethod;

    /// Method of calculating tree
    ETreeMethod m_TreeMethod;

    /// Information shown in tree leaves labels
    ELabelType m_LabelType;

    /// Should query node be marked
    bool m_MarkQueryNode;

    /// Sequences that are not included in the tree
    vector<string> m_RemovedSeqIds;

    /// Computed tree
    CRef<CBioTreeContainer> m_TreeContainer;

    /// Blast name to color map
    TBlastNameColorMap m_BlastNameColorMap;

    /// Error/warning messages
    vector<string> m_Messages;    
};


/// Guide tree calc exceptions
class CGuideTreeCalcException : public CException
{
public:
    enum EErrCode {
        eInvalidOptions,
        eTreeComputationProblem,
        eNoTree,
        eTaxonomyError
    };

    NCBI_EXCEPTION_DEFAULT(CGuideTreeCalcException, CException);
};



#endif // GUIDE_TREE_CALC__HPP
