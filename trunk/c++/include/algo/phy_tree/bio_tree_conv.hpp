#ifndef ALGO_PHY_TREE___BIO_TREE_CONV__HPP
#define ALGO_PHY_TREE___BIO_TREE_CONV__HPP

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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description:  bio trees convertions
 *
 */

/// @file bio_tree.hpp
/// Things for bio tree convertions

BEGIN_NCBI_SCOPE

/** @addtogroup Tree
 *
 * @{
 */


// --------------------------------------------------------------------------


/// Visitor functor to convert one tree to another
///
/// @internal

template<class TDstTreeNode, class TSrcTreeNode, class TNodeConvFunc>
class CTree2TreeFunc
{
public:
    typedef TDstTreeNode                         TDstTreeNodeType;
    typedef TSrcTreeNode                         TSrcTreeNodeType;

public:
    CTree2TreeFunc(TNodeConvFunc& func) : m_DstTree(0), m_ConvFunc(func) {}


    ETreeTraverseCode 
    operator()(const TSrcTreeNodeType& node, 
               int                     delta_level)
    {
        if (m_TreeStack.size() == 0) {
            auto_ptr<TDstTreeNodeType> pnode(MakeNewTreeNode(node));

            m_TreeStack.push_back(pnode.get());
            m_DstTree = pnode.release();
            return eTreeTraverse;
        }

        if (delta_level == 0) {
            if (m_TreeStack.size() > 0) {
                m_TreeStack.pop_back();
            }
            TDstTreeNodeType* parent_node = m_TreeStack.back();
            TDstTreeNodeType* pnode= MakeNewTreeNode(node);

            parent_node->AddNode(pnode);
            m_TreeStack.push_back(pnode);
            return eTreeTraverse;
        }

        if (delta_level == 1) {
            TDstTreeNodeType* parent_node = m_TreeStack.back();
            TDstTreeNodeType* pnode= MakeNewTreeNode(node);

            parent_node->AddNode(pnode);
            m_TreeStack.push_back(pnode);
            return eTreeTraverse;                        
        }
        if (delta_level == -1) {
            m_TreeStack.pop_back();
        }

        return eTreeTraverse;
    }

    TDstTreeNodeType* GetTreeNode() { return m_DstTree; }

protected:

    TDstTreeNodeType* MakeNewTreeNode(const TSrcTreeNodeType& src_node)
    {
        auto_ptr<TDstTreeNodeType> pnode(new TDstTreeNodeType());
        unsigned int uid = src_node.GetValue().GetId();
        pnode->GetValue().SetId(uid);

        m_ConvFunc(*pnode.get(), src_node);
        return pnode.release();
    }

private:
    TDstTreeNodeType*            m_DstTree;
    TNodeConvFunc&               m_ConvFunc;
    vector<TDstTreeNodeType*>    m_TreeStack;
};



/// Convert biotree to dynamic tree using a node converter
///
template<class TDynamicTree, class TSrcBioTree, class TNodeConvFunc>
void BioTreeConvert2Dynamic(TDynamicTree&         dyn_tree, 
                            const TSrcBioTree&    bio_tree,
                            TNodeConvFunc         node_conv)
{
    dyn_tree.Clear();
    typedef typename TSrcBioTree::TBioTreeNode    TSrcTreeNodeType;
    typedef typename TDynamicTree::TBioTreeNode   TDstTreeNodeType;


    CTree2TreeFunc<TDstTreeNodeType, TSrcTreeNodeType, TNodeConvFunc> 
       func(node_conv);

    const TSrcTreeNodeType *n = bio_tree.GetTreeNode();

    CTree2TreeFunc<TDstTreeNodeType, TSrcTreeNodeType, TNodeConvFunc> 
        rfunc(
           TreeDepthFirstTraverse(*(const_cast<TSrcTreeNodeType*>(n)), func)
        );

    dyn_tree.SetTreeNode(rfunc.GetTreeNode());
}




/// Convert CTreeNode<> to dynamic tree using a node converter
///
template<class TDynamicTree, class TTreeNode, class TNodeConvFunc>
void TreeConvert2Dynamic(TDynamicTree&      dyn_tree, 
                         const TTreeNode*   src_tree,
                         TNodeConvFunc      node_conv)
{
    dyn_tree.Clear();

    typedef TTreeNode    TSrcTreeNodeType;
    typedef typename TDynamicTree::TBioTreeNode   TDstTreeNodeType;

    CTree2TreeFunc<TDstTreeNodeType, TSrcTreeNodeType, TNodeConvFunc> 
       func(node_conv);
    CTree2TreeFunc<TDstTreeNodeType, TSrcTreeNodeType, TNodeConvFunc> 
        rfunc(
          TreeDepthFirstTraverse(*(const_cast<TTreeNode*>(src_tree)), func)
        );
    dyn_tree.SetTreeNode(rfunc.GetTreeNode());
}



/// Convert dynamic tree to CTreeNode<>, 
/// returned CTReeNode<> to be deleted by caller.
///
template<class TDynamicTree, class TTreeNode, class TNodeConvFunc>
TTreeNode* DynamicConvert2Tree(TDynamicTree&      dyn_tree, 
                               TNodeConvFunc      node_conv,
                               TTreeNode*&        dst_node)
{
    typedef TTreeNode    TDstTreeNodeType;
    typedef typename TDynamicTree::TBioTreeNode   TSrcTreeNodeType;

    CTree2TreeFunc<TDstTreeNodeType, TSrcTreeNodeType, TNodeConvFunc> 
       func(node_conv);

    const TSrcTreeNodeType *n = dyn_tree.GetTreeNode();

    CTree2TreeFunc<TDstTreeNodeType, TSrcTreeNodeType, TNodeConvFunc> 
        rfunc(
        TreeDepthFirstTraverse(*(const_cast<TSrcTreeNodeType*>(n)), func)
        );
    return (dst_node = rfunc.GetTreeNode());
}





// --------------------------------------------------------------------------

/// Visitor functor to convert dynamic tree nodes to ASN.1 BioTree container
///
/// @internal
template<class TBioTreeContainer, class TDynamicTree>
class CBioTreeConvert2ContainerFunc
{
protected:
    typedef typename TDynamicTree::TBioTreeNode         TDynamicNodeType;
    typedef typename TDynamicNodeType::TValueType       TDynamicNodeValueType;

    typedef typename TBioTreeContainer::TNodes           TCNodeSet;
    typedef typename TCNodeSet::Tdata                    TNodeList;
    typedef typename TNodeList::value_type::element_type TCNode;
    typedef typename TCNode::TFeatures                   TCNodeFeatureSet;
    typedef typename TCNodeFeatureSet::Tdata             TNodeFeatureList;
    typedef typename 
       TNodeFeatureList::value_type::element_type        TCNodeFeature;
public:
    CBioTreeConvert2ContainerFunc(TBioTreeContainer* tree_container)
    : m_Container(tree_container)
    {
        m_NodeList = &(tree_container->SetNodes().Set());
    }

    ETreeTraverseCode 
    operator()(const TDynamicNodeType& node, 
               int                     delta_level)
    {
        if (delta_level < 0) {
            return eTreeTraverse;
        }

        const TDynamicNodeValueType& v = node.GetValue();
        TBioTreeNodeId uid = v.GetId();

        CRef<TCNode> cnode(new TCNode);
        cnode->SetId(uid);

        const TDynamicNodeType* node_parent = 
                        (TDynamicNodeType*) node.GetParent();
        if (node_parent) {
            cnode->SetParent(node_parent->GetValue().GetId());
        }
        
        typedef typename 
           TDynamicNodeValueType::TNodeFeaturesType::TFeatureList TFList;
        const TFList& flist = v.features.GetFeatureList();

        if (!flist.empty()) {
            
            TCNodeFeatureSet& fset = cnode->SetFeatures();

            ITERATE(typename TFList, it, flist) {
                TBioTreeFeatureId fid = it->id;
                const string fvalue = it->value;

                CRef<TCNodeFeature>  cfeat(new TCNodeFeature);
                cfeat->SetFeatureid(fid);
                cfeat->SetValue(fvalue);
                
                fset.Set().push_back(cfeat);

            
            } // ITERATE
        }

        m_NodeList->push_back(cnode);

        return eTreeTraverse;
    }

private:
    TBioTreeContainer*   m_Container;
    TNodeList*           m_NodeList;
};


/// Convert Dynamic tree to ASN.1 BioTree container
///
template<class TBioTreeContainer, class TDynamicTree>
void BioTreeConvert2Container(TBioTreeContainer&      tree_container,
                              const TDynamicTree&     dyn_tree)
{
    // Convert feature dictionary

    typedef typename TBioTreeContainer::TFdict  TContainerDict;

    const CBioTreeFeatureDictionary& dict = dyn_tree.GetFeatureDict();
    const CBioTreeFeatureDictionary::TFeatureDict& dict_map = 
                                                dict.GetFeatureDict();

    TContainerDict& fd = tree_container.SetFdict();
    typename TContainerDict::Tdata& feat_list = fd.Set();
    typedef 
    typename TContainerDict::Tdata::value_type::element_type TCFeatureDescr;
    
    ITERATE(CBioTreeFeatureDictionary::TFeatureDict, it, dict_map) {
        TBioTreeFeatureId fid = it->first;
        const string& fvalue = it->second;

        {{
        CRef<TCFeatureDescr> d(new TCFeatureDescr);
        d->SetId(fid);
        d->SetName(fvalue);

        feat_list.push_back(d);
        }}
    } // ITERATE


    // convert tree data (nodes)

    typedef typename TDynamicTree::TBioTreeNode TTreeNode;
    const TTreeNode *n = dyn_tree.GetTreeNode();

    CBioTreeConvert2ContainerFunc<TBioTreeContainer, TDynamicTree>
        func(&tree_container);
    TreeDepthFirstTraverse(*(const_cast<TTreeNode*>(n)), func);
    
}

/// Convert ASN.1 BioTree container to dynamic tree
///
template<class TBioTreeContainer, class TDynamicTree>
void BioTreeConvertContainer2Dynamic(TDynamicTree&             dyn_tree,
		                             const TBioTreeContainer&  tree_container)
{
	dyn_tree.Clear();
	
    // Convert feature dictionary

    typedef typename TBioTreeContainer::TFdict  TContainerDict;

    CBioTreeFeatureDictionary& dict = dyn_tree.GetFeatureDict();
    const TContainerDict& fd = tree_container.GetFdict();
    const typename TContainerDict::Tdata& feat_list = fd.Get();

		ITERATE(typename TContainerDict::Tdata, it, feat_list) {
        TBioTreeFeatureId fid = (*it)->GetId();
        const string& fvalue = (*it)->GetName();
		
		dict.Register(fid, fvalue);
    }

	// convert tree data (nodes)
    typedef typename TBioTreeContainer::TNodes            TCNodeSet;
    typedef typename TCNodeSet::Tdata                     TNodeList;
    typedef typename TNodeList::value_type::element_type  TCNode;

    const TNodeList node_list = tree_container.GetNodes().Get();

	ITERATE(typename TNodeList, it, node_list) {

		const CRef<TCNode>& cnode = *it;

        TBioTreeNodeId uid = cnode->GetId();

	    typedef typename TDynamicTree::TBioTreeNode      TDynamicNodeType;
		typedef typename TDynamicNodeType::TValueType    TDynamicNodeValueType;

		TDynamicNodeValueType v;
		v.SetId(uid);
    
		typedef typename TCNode::TFeatures               TCNodeFeatureSet;

		if (cnode->CanGetFeatures()) {
			const TCNodeFeatureSet& fset = cnode->GetFeatures();

			const typename TCNodeFeatureSet::Tdata& flist = fset.Get();

			ITERATE(typename TCNodeFeatureSet::Tdata, fit, flist) {
				unsigned int fid = (*fit)->GetFeatureid();
				const string& fvalue = (*fit)->GetValue();

				v.features.SetFeature(fid, fvalue);

			} // ITERATE 

		}

		if (cnode->CanGetParent()) {
	        TBioTreeNodeId parent_id = cnode->GetParent();
			/*TDynamicNodeType* dnode = */dyn_tree.AddNode(v, parent_id);
		} else {
			TDynamicNodeType* dnode = new TDynamicNodeType(v);
			dyn_tree.SetTreeNode(dnode);
		}


	} // ITERATE TNodeList
}

// --------------------------------------------------------------------------


/// Taxon1 tree visitor functor. Converts taxonomy into dynamic tree.
///
/// @internal
///
template<class TITaxon4Each,    class TITaxon1Node, 
         class TITreeIterator,  class TBioTreeContainer>
class CTaxon1NodeConvertVisitor : public TITaxon4Each   
{
	typedef typename TITreeIterator::EAction  EAction;

    typedef typename TBioTreeContainer::TNodes           TCNodeSet;
    typedef typename TCNodeSet::Tdata                    TNodeList;
    typedef typename TNodeList::value_type::element_type TCNode;
    typedef typename TCNode::TFeatures                   TCNodeFeatureSet;
    typedef typename TCNodeFeatureSet::Tdata             TNodeFeatureList;
    typedef typename 
       TNodeFeatureList::value_type::element_type        TCNodeFeature;

public:
	CTaxon1NodeConvertVisitor(TBioTreeContainer*  tree_container)
	: m_TreeContainer(tree_container)
	{
        m_NodeList = &(tree_container->SetNodes().Set());
	}
	
	virtual ~CTaxon1NodeConvertVisitor() {}

	virtual 
	EAction Execute(const TITaxon1Node* pNode)
	{

        TBioTreeNodeId uid = pNode->GetTaxId();

        CRef<TCNode> cnode(new TCNode);
        cnode->SetId(uid);

		vector<int>::size_type psize = m_Parents.size();
		if (psize != 0){
			int parent_tax_id = m_Parents[psize - 1];
			cnode->SetParent(parent_tax_id);
		}


        TCNodeFeatureSet& fset = cnode->SetFeatures();

		// Convert features

		// Name
		{{
			CRef<TCNodeFeature>  cfeat(new TCNodeFeature);
			cfeat->SetFeatureid(1);
			cfeat->SetValue(pNode->GetName());

            fset.Set().push_back(cfeat);
		}}

		// Blast_name
		{{
			CRef<TCNodeFeature>  cfeat(new TCNodeFeature);
			cfeat->SetFeatureid(2);
			cfeat->SetValue(pNode->GetBlastName());

            fset.Set().push_back(cfeat);
		}}

		// Rank
		{{
			CRef<TCNodeFeature>  cfeat(new TCNodeFeature);
			cfeat->SetFeatureid(3);
			int v = pNode->GetRank();

			cfeat->SetValue(NStr::IntToString(v));

            fset.Set().push_back(cfeat);
		}}

		// Division
		{{
			CRef<TCNodeFeature>  cfeat(new TCNodeFeature);
			cfeat->SetFeatureid(4);
			int v = pNode->GetDivision();

			cfeat->SetValue(NStr::IntToString(v));

            fset.Set().push_back(cfeat);
		}}

		// Genetic code
		{{
			CRef<TCNodeFeature>  cfeat(new TCNodeFeature);
			cfeat->SetFeatureid(5);
			int v = pNode->GetGC();

			cfeat->SetValue(NStr::IntToString(v));

            fset.Set().push_back(cfeat);
		}}

		// Mitocondrial genetic code
		{{
			CRef<TCNodeFeature>  cfeat(new TCNodeFeature);
			cfeat->SetFeatureid(6);
			int v = pNode->GetMGC();

			cfeat->SetValue(NStr::IntToString(v));

            fset.Set().push_back(cfeat);
		}}

		// Uncultured
		{{
			CRef<TCNodeFeature>  cfeat(new TCNodeFeature);
			cfeat->SetFeatureid(6);
			int v = pNode->IsUncultured();

			cfeat->SetValue(NStr::IntToString(v));

            fset.Set().push_back(cfeat);
		}}

        m_NodeList->push_back(cnode);


		return TITreeIterator::eOk;
	}
	
	virtual 
	EAction LevelBegin(const TITaxon1Node* pParent)
	{
		m_Parents.push_back(pParent->GetTaxId());
		return TITreeIterator::eOk; 
	}
	
	virtual 
	EAction LevelEnd(const TITaxon1Node* /*pParent*/)
	{ 
		m_Parents.pop_back();
		return TITreeIterator::eOk; 
	}
private:
	TBioTreeContainer*  m_TreeContainer;
    TNodeList*          m_NodeList;
	vector<int>         m_Parents; //<! Stack of parent tax ids
};


template<class TBioTreeContainer>
void BioTreeAddFeatureToDictionary(TBioTreeContainer&  tree_container,
								  unsigned int        feature_id,
								  const string&       feature_name)
{
    typedef typename TBioTreeContainer::TFdict  TContainerDict;
	typedef typename TContainerDict::Tdata::value_type::element_type TFeatureDescr;

    TContainerDict& fd = tree_container.SetFdict();
    typename TContainerDict::Tdata& feat_list = fd.Set();


    CRef<TFeatureDescr> d(new TFeatureDescr);
    d->SetId(feature_id);
    d->SetName(feature_name);

    feat_list.push_back(d);
}



template<class TBioTreeContainer, 
	 class TTaxon1, class TITaxon1Node, class TITreeIterator>
class CTaxon1ConvertToBioTreeContainer
{
public:
  void operator()(TBioTreeContainer&  tree_container,
		  TTaxon1&            tax, 
		  int                 tax_id)
  {
    // Setup features dictionary
    
    BioTreeAddFeatureToDictionary(tree_container, 1, "name");
    BioTreeAddFeatureToDictionary(tree_container, 2, "blast_name");
    BioTreeAddFeatureToDictionary(tree_container, 3, "rank");
    BioTreeAddFeatureToDictionary(tree_container, 4, "division");
    BioTreeAddFeatureToDictionary(tree_container, 5, "GC");
    BioTreeAddFeatureToDictionary(tree_container, 6, "MGC");
    BioTreeAddFeatureToDictionary(tree_container, 7, "IsUncultured");
    
    // Convert nodes
    
    const TITaxon1Node* tax_node=0;
    bool res = tax.LoadSubtree(tax_id, &tax_node);
    if (res) {
      CRef<TITreeIterator> tax_tree_iter(tax.GetTreeIterator());
      tax_tree_iter->GoNode(tax_node);
      
      typedef typename TITreeIterator::I4Each T4Each;
      
      CTaxon1NodeConvertVisitor<T4Each, TITaxon1Node,
	TITreeIterator, TBioTreeContainer>
	tax_vis(&tree_container);
      tax_tree_iter->TraverseDownward(tax_vis);
    }
    
  }
  
  void operator()(TBioTreeContainer&  tree_container,
		  CRef<TITreeIterator> tax_tree_iter)
  {
    // Setup features dictionary
    
    BioTreeAddFeatureToDictionary(tree_container, 1, "name");
    BioTreeAddFeatureToDictionary(tree_container, 2, "blast_name");
    BioTreeAddFeatureToDictionary(tree_container, 3, "rank");
    BioTreeAddFeatureToDictionary(tree_container, 4, "division");
    BioTreeAddFeatureToDictionary(tree_container, 5, "GC");
    BioTreeAddFeatureToDictionary(tree_container, 6, "MGC");
    BioTreeAddFeatureToDictionary(tree_container, 7, "IsUncultured");
    
    // Convert nodes
    
    tax_tree_iter->GoRoot();
    typedef typename TITreeIterator::I4Each T4Each;
    CTaxon1NodeConvertVisitor<T4Each, TITaxon1Node,
      TITreeIterator, TBioTreeContainer>
      tax_vis(&tree_container);
    tax_tree_iter->TraverseDownward(tax_vis);
  }

};


/* @} */


END_NCBI_SCOPE 


/*
 * ===========================================================================
 * $Log$
 * Revision 1.9  2004/11/17 17:53:40  kuznets
 * Reimplemented Tree to Dynamic tree converters. More universal templates
 *
 * Revision 1.8  2004/11/10 19:26:24  kuznets
 * Added coverter from CTreeNode<> to dynamic
 *
 * Revision 1.7  2004/10/08 11:15:44  kuznets
 * Doxygen formatting (group Tree)
 *
 * Revision 1.6  2004/08/18 12:14:35  kuznets
 * Type castings for compatibility with new CBioTree node
 *
 * Revision 1.5  2004/06/28 17:01:20  ckenny
 * + conv from part tax tree to CBioTreeContainer
 *
 * Revision 1.4  2004/06/09 13:38:40  kuznets
 * Fixed compilation errors (GCC)
 *
 * Revision 1.3  2004/06/07 11:54:12  kuznets
 * + converters for taxon1 protocol
 *
 * Revision 1.2  2004/06/01 15:20:48  kuznets
 * + coversion function ASN.1 -> dynamic tree
 *
 * Revision 1.1  2004/05/26 15:15:19  kuznets
 * Initial revision. Tree conversion algorithms moved from bio_tree.hpp
 *
 *
 * ===========================================================================
 */


#endif

