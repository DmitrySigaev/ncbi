#ifndef ALGO___TREE__ALGO_HPP
#define ALGO___TREE__ALGO_HPP
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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:
 *	 Tree algorithms
 *
 */

/// @file tree_algo.hpp
/// Tree algorithms

#include <corelib/ncbi_tree.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup Tree
 *
 * @{
 */


/////////////////////////////////////////////////////////////////////////////
//
//  General purpose tree algorithms
//


/// Visit every parent of the specified node
/// 
/// @param tree_node
///    Starting node
/// @param func
///    Visiting functor
/// @param skip_self
///    Flag to skip the first node (tree_node itself)
///    When TRUE visits only true ancestors
template<class TTreeNode, class TFunc>
TFunc TreeForEachParent(const TTreeNode& tree_node, 
                        TFunc            func,
                        bool             skip_self = false)
{
    const TTreeNode* node_ptr = &tree_node;
    if (skip_self) {
        node_ptr = node_ptr->GetParent();
    }
    for ( ;node_ptr; node_ptr = node_ptr->GetParent()) {
        func(*node_ptr);
    }
    return func;
}

/// Functor to trace node pointers to root (TreeForEachParent)
///
/// @internal
/// @sa TreeForEachParent
template<class TTreeNode, class TTraceContainer>
class CTreeParentTraceFunc
{
public:
    CTreeParentTraceFunc(TTraceContainer* trace) 
     : m_Trace(trace)
    {}

    void operator()(const TTreeNode& node)
    {
        m_Trace->push_back(&node);
    }
private:
    TTraceContainer* m_Trace;
};


/// Trace from the specified node to to the tree root
///
/// Trace path is a container of node const node pointers 
/// (The only requirement is push_back method)
/// The input node becomes first element, then comes its parent.
/// If the node is a tree top its pointer is the only element of the trace
/// vector
///
/// @param tree_node  
///    Starting node (added to the trace first)
/// @param trace 
///    Trace container (vector<const TTreeNode*> or similar)
///
template<class TTreeNode, class TTraceContainer>
void TreeTraceToRoot(const TTreeNode& tree_node, TTraceContainer& trace)
{
    CTreeParentTraceFunc<TTreeNode, TTraceContainer> func(&trace);
    TreeForEachParent(tree_node, func);
}


/// Change tree root (tree rotation)
///
/// @param new_root_node
///    new node which becomes new tree root after the call
template<class TTreeNode>
void TreeReRoot(TTreeNode& new_root_node)
{
    vector<const TTreeNode*> trace;
    TreeTraceToRoot(new_root_node, trace);

    TTreeNode* local_root = &new_root_node;
    ITERATE(typename vector<const TTreeNode*>, it, trace) {
        TTreeNode* node = const_cast<TTreeNode*>(*it);
        TTreeNode* parent = node->GetParent();
        if (parent)
            parent->DetachNode(node);
        if (local_root != node)
            local_root->AddNode(node);
        local_root = node;
    }
}

/// Check if two nodes have the same common root.
///
/// Algorithm finds first common ancestor (farthest from the root)
/// @param tree_node_a
///     Node A
/// @param tree_node_b
///     Node B
/// @return 
///     Nearest common root or NULL if there is no common parent
///
template<class TTreeNode>
const TTreeNode* TreeFindCommonParent(const TTreeNode& tree_node_a,
                                      const TTreeNode& tree_node_b)
{
    typedef vector<const TTreeNode*>  TTraceVector;

    TTraceVector trace_a;
    TTraceVector trace_b;

    TreeTraceToRoot(tree_node_a, trace_a);
    TreeTraceToRoot(tree_node_b, trace_b);

    // trace comparison: go to the 

    const TTreeNode* node_candidate = 0;


    // We need this next variables because of a bug(?) in MSVC 6 
    //    vector::rbegin() returns non const reverse iterator 
    //    for a non const vector (sort of)

    const TTraceVector& ctr_a = trace_a;
    const TTraceVector& ctr_b = trace_b;

    typename TTraceVector::const_reverse_iterator it_a = ctr_a.rbegin();
    typename TTraceVector::const_reverse_iterator it_b = ctr_b.rbegin();

    typename TTraceVector::const_reverse_iterator it_a_end = ctr_a.rend();
    typename TTraceVector::const_reverse_iterator it_b_end = ctr_b.rend();
    
    for (;it_a != it_a_end || it_b != it_b_end; ++it_a, ++it_b) {
        const TTreeNode* node_a = *it_a;
        const TTreeNode* node_b = *it_b;

        if (node_a != node_b) {
            break;
        }
        node_candidate = node_a;        
    }

    return node_candidate;
}


/// Tree print functor used as a tree traversal payload
/// @internal
template<class TTreeNode, class TConverter>
class CTreePrintFunc
{
public:
    CTreePrintFunc(CNcbiOstream& os, TConverter& conv, bool print_ptr)
    : m_OStream(os),
      m_Conv(conv),
      m_Level(0),
      m_PrintPtr(print_ptr)
    {}

    ETreeTraverseCode 
    operator()(const TTreeNode& tr, int delta) 
    {
        m_Level += delta;
        if (delta >= 0) { 
            PrintLevelMargin();
            const string& node_str = m_Conv(tr.GetValue());

            if (m_PrintPtr) {
                m_OStream  
                   << "[" << "0x" << hex << &tr << "]=> 0x" << tr.GetParent() 
                   << " ( " << node_str << " ) " << endl;
            } else {
                m_OStream << node_str << endl;
            }
        }

        return eTreeTraverse;
    }

    void PrintLevelMargin()
    {
        for (int i = 0; i < m_Level; ++i) {
            m_OStream << "  ";
        }
    }
    
private:
    CNcbiOstream&  m_OStream;
    TConverter&    m_Conv;
    int            m_Level;
    bool           m_PrintPtr;
};


/// Tree printing (use for debugging purposes)
///
/// @param os
///     Stream to print the tree
/// @param tree_node
///     Tree node to print (top or sub-tree)
/// @param conv
///     Converter of node value to string (string Conv(node_value) )
/// @param print_ptr
///     When TRUE function prints all internal pointers (debugging)
///
template<class TTreeNode, class TConverter>
void TreePrint(CNcbiOstream&     os, 
               const TTreeNode&  tree_node, 
               TConverter        conv,
               bool              print_ptr = false)
{
    // fake const_cast, nothing to worry 
    //   (should go away when we have const traverse)
    TTreeNode* non_c_tr = const_cast<TTreeNode*>(&tree_node);
    CTreePrintFunc<TTreeNode, TConverter> func(os, conv, print_ptr);
    TreeDepthFirstTraverse(*non_c_tr, func);
}



/////////////////////////////////////////////////////////////////////////////
//
//  Algorithms for Id trees
//


/// Algorithm to to search for a node with specified id
///
/// Tree is traversed back. When searching the upper(parent)
/// level the algorithm never goes down the hierarchy but tries only 
/// immediate children.
///
template<class TPairTree, class TValue>
const TPairTree* PairTreeBackTraceNode(const TPairTree& tr, 
                                       const TValue&    search_id)
{
    const TPairTree* ptr = &tr;

    do {
        const typename TPairTree::TValueType& node_value = ptr->GetValue();

        if (search_id == node_value) {
            return (TPairTree*) ptr;
        }

        typename TPairTree::TNodeList_CI it = ptr->SubNodeBegin();
        typename TPairTree::TNodeList_CI it_end = ptr->SubNodeEnd();

        for (;it != it_end; ++it) {
            const typename TPairTree::TParent* node = *it;
            const typename TPairTree::TTreePair& node_pair = node->GetValue();

            if (search_id == node_pair.GetId()) {
                return (TPairTree*) node;
            }

        } // for

        ptr = (TPairTree*)ptr->GetParent();

    } while (ptr);

    return 0;
}


/// Algorithm to trace the pair tree and find specified leaf along the node path
/// 
/// Algorithm always chooses the deepest leaf 
template<class TPairTree, class TPathList>
const TPairTree* PairTreeTraceNode(const TPairTree& tr, const TPathList& node_path)
{
    _ASSERT(!node_path.empty());

    const TPairTree* ptr = &tr;
    const TPairTree* pfnd = 0;

    typename TPathList::const_iterator last_it = node_path.end();
    --last_it;
    const typename TPathList::value_type& last_search_id = *last_it;

    ITERATE(typename TPathList, sit, node_path) {
        const typename TPairTree::TIdType& search_id = *sit;
        bool sub_level_found = false;
        
        typename TPairTree::TNodeList_CI it = ptr->SubNodeBegin();
        typename TPairTree::TNodeList_CI it_end = ptr->SubNodeEnd();

        for (;it != it_end; ++it) {
            const typename TPairTree::TParent* node = *it;
            const typename TPairTree::TTreePair& node_pair = node->GetValue();

            
            if (node_pair.GetId() == search_id) {  
                ptr = (TPairTree*) node;
                sub_level_found = true;
            }

            if (node_pair.id == last_search_id) {
                pfnd = (TPairTree*) node;
                sub_level_found = true;
            }

            if (sub_level_found)
                break;

        } // for it

        if (!sub_level_found) {
            break;
        }
        
    } // ITERATE

    return pfnd;
}


/// Convert list of node pointers to set of ids
/// Input set is represented by input forward iterators
/// Output set is a back insert iterator
template<class TNodeListIt, class TBackInsert>
void TreeListToSet(TNodeListIt node_list_first, 
                   TNodeListIt node_list_last, 
                   TBackInsert back_ins)
{
    for (;node_list_first != node_list_last; ++node_list_first)
    {
        unsigned uid = (unsigned)(*node_list_first)->GetValue().GetId();
        *back_ins = uid;
        ++back_ins;
    }
}

/// Convert list of node pointers to set of ids
/// @param node_list
///     node list container (must support const_iterator)
/// @param back_ins
///     back insert iterator for the set container
template<class TNodeList, class TBackInsert>
void TreeListToSet(const TNodeList& node_list, TBackInsert back_ins)
{
    typename TNodeList::const_iterator it = node_list.begin();
    typename TNodeList::const_iterator it_end = node_list.end();
    TreeListToSet(it, it_end, back_ins);
}


/// Functor to trace node pointers to root and create set of parents 
/// (TreeForEachParent)
///
/// @internal
/// @sa TreeForEachParent
template<class TTreeNode, class TBackInsert>
class CTreeIdToSetFunc
{
public:
    CTreeIdToSetFunc(TBackInsert back_ins) 
     : m_BackIns(back_ins)
    {}

    ETreeTraverseCode operator()(const TTreeNode& node, 
                                 int              delta_level=0)
    {
        if (delta_level >= 0) {
            *m_BackIns = node.GetValue().GetId();
            ++m_BackIns;
        }
        return eTreeTraverse;
    }

private:
    TBackInsert m_BackIns;
};


/// Traverses all ancestors and add their ids to a set
///
/// @param tree_node
///   Starting node
/// @param back_ins
///   Back insert iterator (represents set)
///
template<class TNode, class TBackInsert>
void TreeMakeParentsSet(const TNode& tree_node, TBackInsert back_ins)
{
    CTreeIdToSetFunc<TNode, TBackInsert> func(back_ins);
    TreeForEachParent(tree_node, func, true /* only true parents */);
}


/// Create set of all tree nodes (ids)
/// 
/// @param tree_node
///    root node
/// @param back_ins
///   Back insert iterator (represents set)
///
template<class TNode, class TBackInsert>
void TreeMakeSet(const TNode& tree_node, TBackInsert back_ins)
{
    TNode* node = const_cast<TNode*>(&tree_node);
    CTreeIdToSetFunc<TNode, TBackInsert> func(back_ins);
    TreeDepthFirstTraverse(node, func);
}


/// Functor to convert tree to a nodelist by the set pattern
///
/// @internal
template<class TTreeNode, class TSet, class TNodeList>
class CTreeSet2NodeListFunc
{
public:
    CTreeSet2NodeListFunc(const TSet& node_set, TNodeList* node_list)
    : m_NodeSet(node_set),
      m_NodeList(node_list)
    {}

    ETreeTraverseCode operator()(const TTreeNode& node, 
                                 int              delta_level=0)
    {
        if (delta_level >= 0) {
            unsigned id = node.GetValue().GetId();
            bool b = m_NodeSet[id];
            if (b) {
                m_NodeList->push_back(&node);
            }
        }
        return eTreeTraverse;
    }

private:
    const TSet&   m_NodeSet;
    TNodeList*    m_NodeList;
};




/// Convert set of node ids to list of nodes
///
/// Traverses the tree, if node is in the set adds node pointer to 
/// the nodelist.
///
template<class TNode, class TSet, class TNodeList>
void TreeSetToNodeList(const TNode&  tree_node, 
                       const TSet&   node_set, 
                       TNodeList&    nlist)
{
    TNode* node = const_cast<TNode*>(&tree_node);
    CTreeSet2NodeListFunc<TNode, TSet, TNodeList> func(node_set, &nlist);
    TreeDepthFirstTraverse(node, func);
}

/// Class-algorithm to compute Non Redundant Set
///
/// Takes a single nodelist as an arguiment and copies every node
/// which has no ancestors in the source set. 
/// @note Result set might be not minimal
///
template<class TNode, class TSet, class TNodeList>
class CTreeNonRedundantSet
{
public:
    void operator()(const TNodeList& src_nlist,
                    TNodeList&       dst_nlist)
    {
        TSet src_set;
        TreeListToSet(src_nlist, src_set.inserter());

        TSet ancestor_set;

        ITERATE(typename TNodeList, it, src_nlist) {
            TNode* snode = *it;

            ancestor_set.clear();

            TreeMakeParentsSet(*snode, ancestor_set.inserter());

            ancestor_set &= src_set;
            unsigned cnt = ancestor_set.count();

            if (cnt == 0) { // source set contains no ancestors of the node
                // node is part of the non redundant set
                dst_nlist.push_back(snode);
            }

        } // ITERATE
    }

};
                         

/* @} */

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2004/04/21 13:27:18  kuznets
 * Bug fix: typename in templates
 *
 * Revision 1.2  2004/04/21 12:56:34  kuznets
 * Added tree related algorithms and utilities based on sets algebra
 * (TreeListToSet, TreeMakeParentsSet, TreeMakeSet, TreeSetToNodeList
 * CTreeNonRedundantSet)
 *
 * Revision 1.1  2004/04/19 16:02:06  kuznets
 * Initial revision. Migrated from <corelib/ncbi_tree.hpp>
 *
 *
 * ==========================================================================
 */

#endif
