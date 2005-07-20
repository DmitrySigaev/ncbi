/* $Id$
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
 * Author:  Adapted from CDTree-1 code by Chris Lanczycki
 *
 * File Description:
 *
 *       High-level algorithmic operations on one or more CCd objects.
 *       (methods that only traverse the cdd ASN.1 data structure are in 
 *        placed in the CCd class itself)
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuSeqTreeAPI.hpp>
#include <algo/structure/cd_utils/cuSeqTreeFactory.hpp>
#include <algo/structure/cd_utils/cuSeqTreeRootedLayout.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

SeqTreeAPI::SeqTreeAPI(vector<CCdCore*>& cds)
	: m_ma(), m_seqTree(0), m_useMembership(true),
	m_taxLevel(BySuperkingdom), m_taxTree(0), m_treeOptions()
{
	vector<CDFamily> families;
	CDFamily::createFamilies(cds, families);
	if(families.size() != 1 )
		return;
	m_ma.setAlignment(families[0]);
}

SeqTreeAPI::~SeqTreeAPI()
{
	if (!m_seqTree) delete m_seqTree;
	if (!m_taxTree) delete m_taxTree;
}

void SeqTreeAPI::annotateTreeByMembership()
{
	m_useMembership = true;
}

void SeqTreeAPI::annotateTreeByTaxonomy(TaxonomyLevel tax)
{
	m_taxLevel = tax;	
}

int SeqTreeAPI::getNumOfLeaves()
{
	return m_ma.GetNumRows();
}
	//return a string of tree method names
	//lay out the tree to the specified area, .i.e. fit to screen
string SeqTreeAPI::layoutSeqTree(int maxX, int maxY, vector<SeqTreeEdge>& edges)
{
	if (m_seqTree == 0)
		makeOrLoadTree();
	return layoutSeqTree(maxX, maxY, -1, edges);
}

	//lay out the tree with a fixed spacing between sequences
string SeqTreeAPI::layoutSeqTree(int maxX, vector<SeqTreeEdge>& edges, int yInt)
{
	if (m_seqTree == 0)
		makeOrLoadTree();
	int maxY = 0; //not used
	if (yInt < 2)
		yInt = 2;
	return layoutSeqTree(maxX, maxY, yInt, edges);
}

void SeqTreeAPI::makeOrLoadTree()
{
	m_seqTree = TreeFactory::makeTree(&m_ma, m_treeOptions);
	m_seqTree->fixRowName(m_ma, SeqTree::eGI);
}

string SeqTreeAPI::layoutSeqTree(int maxX, int maxY, int yInt, vector<SeqTreeEdge>& edges)
{
	SeqTreeRootedLayout treeLayout(yInt);
	treeLayout.calculateNodePositions(*m_seqTree, maxX, maxY);
	getAllEdges(edges);
	string param = GetTreeAlgorithmName(m_treeOptions.clusteringMethod);
	param.append(" / " + DistanceMatrix::GetDistMethodName(m_treeOptions.distMethod));
	if (DistanceMatrix::DistMethodUsesScoringMatrix(m_treeOptions.distMethod) ) {
		param.append(" / " + GetScoringMatrixName(m_treeOptions.matrix));
	}
	return param;
}

int SeqTreeAPI::getAllEdges(vector<SeqTreeEdge>& edges)
{
	getEgesFromSubTree(m_seqTree->begin(), edges);
	return edges.size();
}

void SeqTreeAPI::getEgesFromSubTree(const SeqTree::iterator& cursor, vector<SeqTreeEdge>& edges)
{
    //if cursor is a leaf node, draw its name and return
    if (cursor.number_of_children() == 0)
    {
        return;
    }
    //always draw from parent to children
	SeqTreeNode nodeParent;
	nodeParent.isLeaf = false;
	nodeParent.x = cursor->x;
	nodeParent.y = cursor->y;
    SeqTree::sibling_iterator sib = cursor.begin();
    while (sib != cursor.end())
    {
		SeqTreeNode nodeChild, nodeTurn;
		nodeChild.x = sib->x;
		nodeChild.y = sib->y;
		nodeTurn.isLeaf = false;
		nodeTurn.x = nodeParent.x;
		nodeTurn.y= nodeChild.y;
		if (sib.number_of_children() == 0)
		{
			nodeChild.isLeaf =true;
			nodeChild.name = sib->name;
			//nodeChild.childAcc = sib->membership;
			annotateLeafNode(*sib,nodeChild);
		}
		else
			nodeChild.isLeaf = false;
		SeqTreeEdge e1, e2;
		e1.first = nodeParent;
		e1.second = nodeTurn;
		e2.first = nodeTurn;
		e2.second = nodeChild;
		edges.push_back(e1);
		edges.push_back(e2);
		getEgesFromSubTree(sib, edges);
        ++sib;
    }
}

void SeqTreeAPI::annotateLeafNode(const SeqItem& nodeData, SeqTreeNode& node)
{
	if (m_useMembership)
		node.annotation = nodeData.membership;
	else
	{
		if (!m_taxTree)
			m_taxTree = new TaxTreeData(m_ma);
		string rankName = "Superkingdom";
		if(m_taxLevel == ByKingdom)
			rankName = "Kingdom";
		TaxTreeIterator taxNode = m_taxTree->getParentAtRank(nodeData.rowID, rankName);
		node.annotation = taxNode->orgName;
	}
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.1  2005/07/20 20:05:08  cliu
 * redesign SeqTreeAPI
 *
 * Revision 1.8  2005/07/19 15:37:32  cliu
 * make a tree for family
 *
 * Revision 1.7  2005/07/06 20:58:06  cliu
 * return tree parameter string
 *
 * Revision 1.6  2005/06/21 13:09:43  cliu
 * add tree layout.
 *
 * Revision 1.5  2005/05/10 20:11:31  cliu
 * make and save trees
 *
 * Revision 1.4  2005/04/21 18:34:36  lanczyck
 * revert to GetOrgRef: it was fixed to allow tax_id = 1
 *
 * Revision 1.3  2005/04/19 22:03:35  ucko
 * Empty strings with erase() rather than clear() for GCC 2.95 compatibility.
 *
 * Revision 1.2  2005/04/19 20:13:50  lanczyck
 * CTaxon1::GetOrgRef returns null CRef for root tax_id = 1; use CTaxon1::GetById instead
 *
 * Revision 1.1  2005/04/19 14:27:18  lanczyck
 * initial version under algo/structure
 *
 * ---------------------------------------------------------------------------
 */
