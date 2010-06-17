#ifndef GUIDE_TREE__HPP
#define GUIDE_TREE__HPP
/*  $Id: guide_tree.hpp$
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
 * Author:  Greg Boratyn
 *
 * 
 */

#include <corelib/ncbiexpt.hpp>
#include <algo/phy_tree/bio_tree.hpp>

#include "guide_tree_calc.hpp"
#include "guide_tree_simplify.hpp"


USING_NCBI_SCOPE;

/// Class for adding tree features, maniplating and printing tree in standard
/// text formats
class CGuideTree
{
public:

    /// Type for BlastName to color map
    typedef pair<string, string> TBlastNameToColor;
    typedef vector<TBlastNameToColor> TBlastNameColorMap;

    /// Tree simplification modes
    enum ETreeSimplifyMode {
        eNone,             ///< No simplification mode
        eFullyExpanded,    ///< Tree fully expanded
        eByBlastName       ///< Subtrees that contain sequences with the
                           ///< the same Blast Name are collapsed
    };

    /// Output formats
    enum ETreeFormat {
        eASN, eNewick, eNexus
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

    
public:

    /// Constructor
    /// @param guide_tree_calc GuideTreeCalc object [in]
    /// @param lbl_type Type of labels to be used for tree leaves [in]
    /// @param mark_query_node If true, query node will be marked with
    /// different color [in]
    ///
    CGuideTree(CGuideTreeCalc& guide_tree_calc,
               ELabelType lbl_type = eSeqId,
               bool mark_query_node = true);

    /// Constructor
    /// @param btc BioTreeContainer object
    /// @param lbl_type ELabelType
    ///
    CGuideTree(CBioTreeContainer& btc, ELabelType lbl_type
               = eSeqId);

    /// Constructor with initialization of tree features
    /// @param btc BioTreeContainer object [in]
    /// @param seqids Seq-ids for sequences represented in the tree [in]
    /// @param scope Scope [in]
    /// @param query_node_id Id of query node [in]
    /// @param lbl_type Type of lables for tree leaves [in]
    /// @param mark_query_node Query node will be marked if true [in]
    ///
    CGuideTree(CBioTreeContainer& btc, const vector< CRef<CSeq_id> >& seqids,
               CScope& scope, int query_node_id,
               ELabelType lbl_type = eSeqId, 
               bool mark_query_node = true);


    /// Contructor
    /// @param tree Tree structure [in]
    ///
    CGuideTree(const CBioTreeDynamic& tree);


    /// Destructor
    ~CGuideTree() {}


    // --- Setters ---

    /// Set query node id
    /// @param id Query node id [in]
    ///
    /// Query node is marked by different label color
    void SetQueryNodeId(int id) {m_QueryNodeId = id;}

    /// Set Blast Name to color map
    /// @return Reference to Blast Name to color map
    ///
    TBlastNameColorMap& SetBlastNameColorMap(void) {return m_BlastNameColorMap;}


    // --- Getters ---

    /// Check if query node id is set
    /// @return True if query node id is set, false otherwise
    ///
    bool IsQueryNodeSet(void) {return m_QueryNodeId > -1;}

    /// Check whether tree is composed of sequences with the same Blast Name
    /// @return True if all sequences have the same Blast Name, false otherwise
    ///
    bool IsSingleBlastName(void);

    /// Get query node id
    /// @return Query node id
    ///
    int GetQueryNodeId(void) const {return m_QueryNodeId;}

    /// Get current tree simplification mode
    /// @return tree simplifcation mode
    ///
    ETreeSimplifyMode GetSimplifyMode(void) const {return m_SimplifyMode;}

    /// Get tree root node id
    /// @return root node id
    ///
    int GetRootNodeID(void);

    /// Get pointer to the node with given id
    /// @param id Node's numerical id [in]
    /// @return Pointer to the node or NULL if node not found
    ///
    CBioTreeDynamic::CBioNode* GetNode(TBioTreeNodeId id);

    /// Get pointer to the node with given id and throw exception if node not
    /// found
    /// @param id Node's numerical id [in]
    /// @return Pointer to the node
    CBioTreeDynamic::CBioNode* GetNonNullNode(TBioTreeNodeId id);

    /// Get tree structure
    /// @return Tree
    ///
    const CBioTreeDynamic& GetTree(void) const {return m_Dyntree;}

    /// Get serialized tree
    /// @return Biotree container
    ///
    CRef<CBioTreeContainer> GetSerialTree(void);


    // --- Generating output ---

    /// Save tree as: image, ASN object, or text format in a file.
    /// Wraper around methods below.
    /// @param format Output format [in]
    /// @param name Output file name, if empty string, output will be
    /// directed to stdout for Newic and Nexus [in]
    /// @return True on success, false on failure
    ///
    bool SaveTreeAs(CNcbiOstream& ostr, ETreeFormat format);


    /// Write tree structure to stream
    /// @param out Output stream [in|out]
    /// @return True on success, false on failure
    ///
    bool WriteTree(CNcbiOstream& out);


    /// Write tree structure to file
    /// @param filename Output file name [in]
    /// @return True on success, false on failure
    ///
    bool WriteTree(const string& filename = "");


    /// Write tree to netcache
    /// @param netcacheServiceName Netcache Service Name [in]
    /// @param netcacheClientName  Netcache Client Name  [in]
    /// @return string netcache key
    ///
    string WriteTreeInNetcache(string netcacheServiceName,string netcacheClientName);

    /// Write tree in Newick format to stream
    /// @param ostr Output stream [in|out]
    /// @return True on success, false on failure
    /// 
    bool PrintNewickTree(CNcbiOstream& ostr);

    /// Write tree in Newick format to file
    /// @param filename Ouput file name [in]
    /// @return True on success, false on failure
    ///
    bool PrintNewickTree(const string& filename);


    /// Write tree in Nexus format to stream
    /// @param ostr Output stream [in|out]
    /// @param tree_name Name of the tree field in Nexus output [in]
    /// @param force_win_eol If true end of lines will be always "\r\n" [in]
    /// @return True on success, false on failure
    ///
    bool PrintNexusTree(CNcbiOstream& ostr,
                        const string& tree_name = "Blast_guide_tree",
                        bool force_win_eol = false);

    /// Write tree in Nexus format to file
    /// @param filename Output file name [in]
    /// @param tree_name Name of the tree field in Nexus output [in]
    /// @param force_win_eol If true end of lines will be always "\r\n" [in]
    /// @return True on success, false on failure
    ///
    bool PrintNexusTree(const string& filename,
                        const string& tree_name = "Blast_guide_tree",
                        bool force_win_eol = false);



    // --- Tree manipulators ---
    
    /// Fully expand tree
    ///
    void FullyExpand(void);

    /// Group nodes according to user-selected scheme and collapse subtrees
    /// composed of nodes that belong to the same group
    /// @param method Name of the method for simplifying the tree [in]
    ///
    void SimplifyTree(ETreeSimplifyMode method);

    /// Expand or collapse subtree rooted in given node
    /// @param node_id Numerical id of the node to expand or collapse [in]
    ///
    bool ExpandCollapseSubtree(int node_id);


    /// Reroot tree
    /// @param new_root_id Node id of the new root [in]
    ///
    void RerootTree(int new_root_id);

    /// Show subtree
    /// @param root_id Node id of the subtree root [in]
    ///
    bool ShowSubtree(int root_id);


protected:    
    
    /// Forbiding copy constructor
    ///
    CGuideTree(CGuideTree& tree);

    /// Forbiding assignment operator
    ///
    CGuideTree& operator=(CGuideTree& tree);

    /// Init class attributes to default values
    ///
    void x_Init(void);

    /// Find pointer to a BioTreeDynamic node with given numerical id.
    /// Throws excepion if node not found.
    /// @param id Numerical node id [in]
    /// @param throw_if_null Throw exception if node not found [in]
    /// @return Pointer to the node with desired id
    ///
    CBioTreeDynamic::CBioNode* x_GetBioNode(TBioTreeNodeId id,
                                            bool throw_if_null = true);

    /// Check if node is expanded (subtree shown)
    /// @param node Node [in]
    /// @return True if node expanded, false otherwise
    ///
    static bool x_IsExpanded(const CBioTreeDynamic::CBioNode& node);

    /// Check if node is a leaf or collapsed
    /// @param node Node [in]
    /// @return True if node is a leaf or collapsed, false otherwise
    ///
    static bool x_IsLeafEx(const CBioTreeDynamic::CBioNode& node);

    /// Collapse node (do not show subtree)
    /// @param node Node [in|out]
    ///
    static void x_Collapse(CBioTreeDynamic::CBioNode& node);

    /// Expand node (show subtree)
    /// @param node Node [in|out]
    ///
    static void x_Expand(CBioTreeDynamic::CBioNode& node);

    /// Generates tree in Newick format, recursive
    /// @param ostr Output stream [in|out]
    /// @param node Tree root [in]
    /// @param is_outer_node Controls recursion should be true on first call [in]
    ///
    void x_PrintNewickTree(CNcbiOstream& ostr, 
                           const CBioTreeDynamic::CBioNode& node, 
                           bool is_outer_node = true);
        

    /// Mark query node after collapsing subtree. Checks if query node is in
    /// the collapsed subtree and marks the collapse node.
    /// @param node Root of the collapsed subtree [in]
    ///
    void x_MarkCollapsedQueryNode(CBioTreeDynamic::CBioNode* node);

    /// Collapse given subtrees
    /// @param groupper Object groupping nodes that contains a list of subtrees
    ///
    /// Sets node attributes: m_Label, m_ShowChilds, and node color for all
    /// nodes provided by the input paramater. Used by x_SimplifyTree.
    ///
    void x_CollapseSubtrees(CPhyloTreeNodeGroupper& groupper);


    /// Init tree leaf labels with selected labels type
    /// @param btc BioTreeContainer [in|out]
    /// @param lbl_type Labels type [in]
    ///
    void x_InitTreeLabels(CBioTreeContainer& btc, ELabelType lbl_type); 

private:
    

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
    static void x_InitTreeFeatures(CBioTreeContainer& btc,
                                 const vector< CRef<CSeq_id> >& seqids,
                                 CScope& scope,
                                 ELabelType label_type,
                                 bool mark_query_node,
                                 TBlastNameColorMap& bcolormap,
                                 int query_node_id);

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


    // Tree visitor classes used for manipulating the guide tree

    /// Tree visitor, finds BioTreeDynamic node by id
    class CBioNodeFinder
    {
    public:

        /// Constructor
        /// @param id Node id [in]
        CBioNodeFinder(TBioTreeNodeId id) : m_NodeId(id), m_Node(NULL) {}

        /// Get pointer to found node
        /// @return Pointer to node or NULL
        CBioTreeDynamic::CBioNode* GetNode(void) {return m_Node;}

        /// Check if node has desired id. Function invoked by tree traversal
        /// function.
        /// @param node Node [in]
        /// @param delta Traversal direction [in]
        ETreeTraverseCode operator()(CBioTreeDynamic::CBioNode& node, int delta)
        {
            if (delta == 0 || delta == 1) {
                if ((*node).GetId() == m_NodeId) {
                    m_Node = &node;
                    return eTreeTraverseStop;
                }
            }
            return eTreeTraverse;
        }

    protected:
        TBioTreeNodeId m_NodeId;
        CBioTreeDynamic::CBioNode* m_Node;
    };


    /// Tree visitor class, expands all nodes and corrects node colors
    class CExpander
    {
    public:
        /// Expand subtree. Function invoked on each node by traversal function.
        /// @param node Tree root [in]
        /// @param delta Direction of tree traversal [in]
        /// @return Traverse action
        ETreeTraverseCode operator()(CBioTreeDynamic::CBioNode& node,
                                     int delta);
    };


    // Tree visitor for examining whether a phylogenetic tree contains sequences
    // with only one Blast Name
    class CSingleBlastNameExaminer
    {
    public:

        /// Constructor
        CSingleBlastNameExaminer(CBioTreeDynamic& tree)
            : m_IsSingleBlastName(true) 
        {
            const CBioTreeFeatureDictionary& fdict
                = tree.GetFeatureDict();

            if (!fdict.HasFeature(kBlastNameTag)) {
                NCBI_THROW(CException, eInvalid, 
                           "No Blast Name feature CBioTreeFeatureDictionary");
            }
        }

        /// Check if all sequences in examined tree have the same Blast Name
        ///
        /// Meaningless if invoked before tree traversing
        /// @return True if all sequences have common blast name, false otherwise
        bool IsSingleBlastName(void) const {return m_IsSingleBlastName;}

        /// Expamine node. Function invoked on each node by traversal function.
        /// @param node Tree root [in]
        /// @param delta Direction of tree traversal [in]
        /// @return Traverse action
        ETreeTraverseCode operator()(CBioTreeDynamic::CBioNode& node,
                                     int delta) 
        {
            if (delta == 0 || delta == 1) {
                if (node.IsLeaf()) {


                    if (m_CurrentBlastName.empty()) {
                        m_CurrentBlastName = node.GetFeature(kBlastNameTag);
                    }
                    else {
                        if (m_CurrentBlastName
                            != node.GetFeature(kBlastNameTag)) {
                          
                            m_IsSingleBlastName = false;
                            return eTreeTraverseStop;
                        }
                    }
                }
            }
            return eTreeTraverse;
        }
    
    protected:
        /// Is one blast name in the tree
        bool m_IsSingleBlastName;              

        /// Last identified blast name
        string m_CurrentBlastName;
    };


protected:

    /// Stores tree data
    CBioTreeDynamic m_Dyntree;

    /// Id of query node
    int m_QueryNodeId;

    /// Current tree simplification mode
    ETreeSimplifyMode m_SimplifyMode;

    /// Blast Name to color map
    TBlastNameColorMap m_BlastNameColorMap;


public:
    // Tree feature tags

    /// Sequence label feature tag
    static const string kLabelTag;

    /// Distance feature tag
    static const string kDistTag;

    /// Sequence id feature tag
    static const string kSeqIDTag;

    /// Sequence title feature tag
    static const string kSeqTitleTag;

    /// Organizm name feature tag
    static const string kOrganismTag;

    /// Accession number feature tag
    static const string kAccessionNbrTag;

    /// Blast name feature tag
    static const string kBlastNameTag;

    /// Alignment index id feature tag
    static const string kAlignIndexIdTag;

    /// Node color feature tag (used by CPhyloTreeNode)
    static const string kNodeColorTag;

    /// Node label color feature tag (used by CPhyloTreeNode)
    static const string kLabelColorTag;

    /// Node label backrground color tag (used by CPhyloTreeNode)
    static const string kLabelBgColorTag;

    /// Node label tag color tag (used by CPhyloTreeNode)
    static const string kLabelTagColor;

    /// Node subtree collapse tag (used by CPhyloTreeNode)
    static const string kCollapseTag;
};


/// Guide tree exceptions
class CGuideTreeException : public CException
{
public:

    /// Error code
    enum EErrCode {
        eInvalid,
        eInvalidOptions,    ///< Invalid parameter values
        eNodeNotFound,      ///< Node with desired id not found
        eTraverseProblem,   ///< Problem in one of the tree visitor classes
        eTaxonomyError      ///< Problem initializing CTax object
    };

    NCBI_EXCEPTION_DEFAULT(CGuideTreeException, CException);
};


#endif // GUIDE_TREE__HPP
