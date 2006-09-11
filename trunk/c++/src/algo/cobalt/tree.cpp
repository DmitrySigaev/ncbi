static char const rcsid[] = "$Id$";

/*
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: tree.cpp

Author: Jason Papadopoulos

Contents: Implementation of CTree class

******************************************************************************/

/// @file tree.cpp
/// Implementation of CTree class

#include <ncbi_pch.hpp>
#include <algo/cobalt/tree.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

void 
CTree::PrintTree(const TPhyTreeNode *tree, int level)
{
    int i, j;

    for (i = 0; i < level; i++)
        printf("    ");

    printf("node: ");
    if (tree->IsLeaf() && tree->GetValue().GetId() >= 0)
        printf("query %d ", tree->GetValue().GetId());
    if (tree->GetValue().IsSetDist())
        printf("distance %lf", tree->GetValue().GetDist());
    printf("\n");

    if (tree->IsLeaf())
        return;

    TPhyTreeNode::TNodeList_CI child(tree->SubNodeBegin());

    j = 0;
    while (child != tree->SubNodeEnd()) {
        for (i = 0; i < level; i++)
            printf("    ");

        printf("%d:\n", j);
        PrintTree(*child, level + 1);

        j++;
        child++;
    }
}


TPhyTreeNode *
CTree::x_FindLargestEdge(TPhyTreeNode *node, 
                         TPhyTreeNode *best_node)
{
    if (node->GetValue().GetDist() >
        best_node->GetValue().GetDist()) {
        best_node = node;
    }
    if (node->IsLeaf())
        return best_node;

    TPhyTreeNode::TNodeList_I child(node->SubNodeBegin());
    while (child != node->SubNodeEnd()) {
        best_node = x_FindLargestEdge(*child, best_node);
        child++;
    }

    return best_node;
}


void
CTree::ListTreeEdges(const TPhyTreeNode *node, 
                     vector<STreeEdge>& edge_list)
{
    if (node->GetParent()) {
        edge_list.push_back(STreeEdge(node, node->GetValue().GetDist()));
    }

    TPhyTreeNode::TNodeList_CI child(node->SubNodeBegin());
    while (child != node->SubNodeEnd()) {
        ListTreeEdges(*child, edge_list);
        child++;
    }
}

void
CTree::ListTreeLeaves(const TPhyTreeNode *node, 
            vector<STreeLeaf>& node_list,
            double curr_dist)
{
    if (node->IsLeaf()) {

        // negative edge lengths are not valid, and we assume
        // that distances closer to the root are weighted more
        // heavily, hence the reciprocal

        if (curr_dist <= 0)
            node_list.push_back(STreeLeaf(node->GetValue().GetId(), 0.0));
        else
            node_list.push_back(STreeLeaf(node->GetValue().GetId(),
                                          1.0 / curr_dist));
        return;
    }

    TPhyTreeNode::TNodeList_CI child(node->SubNodeBegin());
    while (child != node->SubNodeEnd()) {
        double new_dist = curr_dist;

        // the fastme algorithm does not allow edges to have
        // negative length unless they are leaf edges. We allow
        // lengths that are slightly negative to account for
        // roundoff effects

        _ASSERT((*child)->GetValue().GetDist() >= -1e-6 || (*child)->IsLeaf());
        if ((*child)->GetValue().GetDist() > 0) {
            new_dist += (*child)->GetValue().GetDist();
        }

        ListTreeLeaves(*child, node_list, new_dist);
        child++;
    }
}

void
CTree::x_RerootTree(TPhyTreeNode *node)
{
    // For trees generated by FASTme this will automatically make
    // the tree strictly binary; by default FASTme produces a
    // tree whose root has three subtrees. In this case all of
    // the original tree nodes are reused, so nothing in the
    // original tree needs to be freed.

    double distance = 0.0;
    TPhyTreeNode *new_root = new TPhyTreeNode;

    if (node == m_Tree) {

        // the root has distance zero, and if no other 
        // tree edge has distance larger than this then
        // the tree is degenerate. Just make it strictly
        // binary

        TPhyTreeNode *leaf = 0;
        TPhyTreeNode::TNodeList_I child(m_Tree->SubNodeBegin());
        while (child != m_Tree->SubNodeEnd()) {
            if ((*child)->IsLeaf()) {
                leaf = m_Tree->DetachNode(*child);
                break;
            }
            child++;
        }

        _ASSERT(leaf);
        new_root->GetValue().SetDist(0.0);
        new_root->AddNode(m_Tree);
        new_root->AddNode(leaf);
        m_Tree = new_root;
        return;
    }

    TPhyTreeNode *parent = node->GetParent();
    
    // The node (and the entire subtree underneath it) now 
    // are detached from the input tree and then attached to 
    // the new root

    node = parent->DetachNode(node);
    new_root->AddNode(node);
    node = new_root;

    // Now proceed up the original tree until the root is
    // reached. Every child node met along the way becomes
    // a child node in the new tree (i.e. is unchanged). 
    // Every parent node in the original tree also becomes 
    // a child node in the new tree. This requires detaching
    // the node, and attaching it to the new tree
    //
    // Because the original tree root can never be selected
    // as the new tree root, at least one parent node must
    // be attached to the new tree

    do { 
        // Turning the parent node into a child node is
        // equivalent to reversing the direction of the
        // tree for that node. That means the distance that
        // appears in the parent node must be replaced with
        // the distance of its child (previously determined)

        double new_distance = parent->GetValue().GetDist();
        parent->GetValue().SetDist(distance);
        distance = new_distance;

        // Detach the parent from the original tree (if
        // necessary; the tree root doesn't need to be
        // detached from anything)

        TPhyTreeNode *next_parent = parent->GetParent();
        if (next_parent)
            parent = next_parent->DetachNode(parent);

        // Attach the parent to the new tree, and point to it
        // so that future nodes get attached as children of
        // this node

        node->AddNode(parent);
        node = parent;
        parent = next_parent;
    } while (parent);

    m_Tree = new_root;
}

void
CTree::ComputeTree(const CDistMethods::TMatrix& distances,
                   bool use_fastme)
{
    if (m_Tree) {
        delete m_Tree;
    }

    if (use_fastme)
        m_Tree = CDistMethods::FastMeTree(distances);
    else
        m_Tree = CDistMethods::NjTree(distances);

    m_Tree->GetValue().SetDist(0.0);

    x_RerootTree(x_FindLargestEdge(m_Tree, m_Tree));
}

END_SCOPE(cobalt)
END_NCBI_SCOPE

/* ====================================================================
 * $Log$
 * Revision 1.7  2006/09/11 16:28:41  papadopo
 * 1. Use neighbor-joining by default, with FastME as an option
 * 2. When printing trees, assume query sequences only occur at tree leaves
 *
 * Revision 1.6  2006/03/22 19:23:17  dicuccio
 * Cosmetic changes: adjusted include guards; formatted CVS logs; added export
 * specifiers
 *
 * Revision 1.5  2006/01/11 16:44:44  papadopo
 * handle rerooting a tree whose distance matrix is degenerate
 *
 * Revision 1.4  2005/11/21 21:03:00  papadopo
 * fix documentation, add doxygen
 *
 * Revision 1.3  2005/11/08 18:42:16  papadopo
 * assert -> _ASSERT
 *
 * Revision 1.2  2005/11/08 17:56:56  papadopo
 * ASSERT -> assert
 *
 * Revision 1.1  2005/11/07 18:14:01  papadopo
 * Initial revision
 *
 * ====================================================================
 */
