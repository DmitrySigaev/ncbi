/*  $Id: guide_tree.cpp$
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
 * Author:  Greg Boratyn, Irena Zaretskaya
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbidbg.hpp>
#include <corelib/hash_set.hpp>

#include <util/image/image.hpp>
#include <util/image/image_util.hpp>
#include <util/image/image_io.hpp>

#include <objects/taxon1/taxon1.hpp>

#include <gui/widgets/phylo_tree/phylo_tree_rect_cladogram.hpp>
#include <gui/widgets/phylo_tree/phylo_tree_slanted_cladogram.hpp>
#include <gui/widgets/phylo_tree/phylo_tree_radial.hpp>
#include <gui/widgets/phylo_tree/phylo_tree_force.hpp>

#include <connect/ncbi_conn_stream.hpp>

#include <serial/objostrasnb.hpp>
#include <algo/phy_tree/bio_tree_conv.hpp>

#include <connect/services/netcache_api.hpp>
//This include is here - to fix compile error related to #include <serial/objostrasnb.hpp> - not sure why
#include <objmgr/util/sequence.hpp>

#include "guide_tree_calc.hpp"
#include "guide_tree.hpp"


USING_NCBI_SCOPE;
USING_SCOPE(objects);


const string CGuideTree::kLabelTag = "label";
const string CGuideTree::kDistTag = "dist";
const string CGuideTree::kSeqIDTag = "seq-id";
const string CGuideTree::kSeqTitleTag = "seq-title";
const string CGuideTree::kOrganismTag = "organism";
const string CGuideTree::kAccessionNbrTag = "accession-nbr";        
const string CGuideTree::kBlastNameTag = "blast-name";    
const string CGuideTree::kAlignIndexIdTag = "align-index";     

const string CGuideTree::kNodeColorTag = "$NODE_COLOR";
const string CGuideTree::kLabelColorTag = "$LABEL_COLOR";
const string CGuideTree::kLabelBgColorTag = "$LABEL_BG_COLOR";
const string CGuideTree::kLabelTagColor = "$LABEL_TAG_COLOR";
const string CGuideTree::kCollapseTag = "$NODE_COLLAPSED";

// query node colors
static const string s_kQueryNodeColor = "255 0 0";
static const string s_kQueryNodeBgColor = "255 255 0";

// tree leaf label for unknown taxonomy
static const string s_kUnknown = "unknown";

// initial value for collapsed subtree feature
static const string s_kSubtreeDisplayed = "0";


CGuideTree::CGuideTree(CGuideTreeCalc& guide_tree_calc, ELabelType label_type,
                       bool mark_query_node) 
{
    x_Init();

    CRef<CBioTreeContainer> btc = guide_tree_calc.GetSerialTree();
    x_InitTreeFeatures(*btc, guide_tree_calc.GetSeqIds(),
                       *guide_tree_calc.GetScope(), 
                       label_type, mark_query_node,
                       m_BlastNameColorMap,
                       guide_tree_calc.GetQueryNodeId());

    BioTreeConvertContainer2Dynamic(m_Dyntree, *btc);
    m_QueryNodeId = guide_tree_calc.GetQueryNodeId();
}

CGuideTree::CGuideTree(CBioTreeContainer& btc,
                       CGuideTree::ELabelType lblType)
{
    x_Init();

    x_InitTreeLabels(btc,lblType);
    BioTreeConvertContainer2Dynamic(m_Dyntree, btc);
}


CGuideTree::CGuideTree(CBioTreeContainer& btc,
                       const vector< CRef<CSeq_id> >& seqids,
                       CScope& scope,
                       int query_node_id,
                       CGuideTree::ELabelType lbl_type,
                       bool mark_query_node)
{
    x_Init();

    m_QueryNodeId = query_node_id;
    x_InitTreeFeatures(btc, seqids, scope, lbl_type, mark_query_node,
                       m_BlastNameColorMap, m_QueryNodeId);

    BioTreeConvertContainer2Dynamic(m_Dyntree, btc);
}


CGuideTree::CGuideTree(const CBioTreeDynamic& tree)
    : m_Dyntree(tree)
{
    x_Init();
}


bool CGuideTree::WriteImage(CNcbiOstream& out)
{
    bool success = x_RenderImage();            
    if(success) {
        CImageIO::WriteImage(m_Context->GetBuffer(), out, m_ImageFormat);
    }
    return success;
}


bool CGuideTree::WriteImage(const string& filename)
{
    bool success = x_RenderImage();
    if(success) {
        CImageIO::WriteImage(m_Context->GetBuffer(), filename, m_ImageFormat);
    }
    return success;
}

string CGuideTree::WriteImageToNetcache(string netcacheServiceName,string netcacheClientName)
{
    string imageKey;
    bool success = x_RenderImage();            
    if(success) {
        CNetCacheAPI nc_client(netcacheServiceName,netcacheClientName);
        imageKey = nc_client.PutData(m_Context->GetBuffer().GetData(),  m_Width*m_Height*3);
    }
    return imageKey;
}


bool CGuideTree::WriteTree(CNcbiOstream& out)
{
    out << MSerial_AsnText << *GetSerialTree();

    return true;
}


bool CGuideTree::WriteTree(const string& filename)
{
    CBioTreeDynamic dyntree;
    CBioTreeContainer btc;

    BioTreeConvert2Container(btc, m_Dyntree);
    
    // Writing to file
    CNcbiOfstream ostr(filename.c_str());
    if (!ostr) {
        return false;
    }
    ostr << MSerial_AsnText << btc;

    return true;
}

string CGuideTree::WriteTreeInNetcache(string netcacheServiceName,string netcacheClientName)
{
    CBioTreeDynamic dyntree;
    CBioTreeContainer btc;

    BioTreeConvert2Container(btc, m_Dyntree);
        
    //Put contetnts of the tree in memory
    CConn_MemoryStream iostr;
    CObjectOStreamAsnBinary outBin(iostr);        
    outBin << btc;
    outBin.Flush();
    
    
    CNetCacheAPI nc_client(netcacheServiceName,netcacheClientName);    
    size_t data_length = iostr.tellp();
    string data;                
    data.resize(data_length);        
    iostr.read(const_cast<char*>(data.data()), data_length);
    string treeKey = nc_client.PutData(&data[0], data.size());
    return treeKey;
}    



bool CGuideTree::PrintNewickTree(const string& filename)
{
    CNcbiOfstream out(filename.c_str());
    if (!out) {
        return false;
    }
    return PrintNewickTree(out);
}

bool CGuideTree::PrintNewickTree(CNcbiOstream& ostr)
{
    x_PrintNewickTree(ostr, *m_Dyntree.GetTreeNode());
    ostr << NcbiEndl;
    return true;
}

bool CGuideTree::PrintNexusTree(const string& filename, 
                                const string& tree_name,
                                bool force_win_eol)
{
    CNcbiOfstream out(filename.c_str());
    if (!out) {
        return false;
    }
    return PrintNexusTree(out, tree_name, force_win_eol);
}

bool CGuideTree::PrintNexusTree(CNcbiOstream& ostr, const string& tree_name,
                                bool force_win_eol)
{
    ostr << "#NEXUS";
    // Nedded when used for downloading from web
    if (force_win_eol) {
        ostr << "\r\n";
    }
    else {
        ostr << NcbiEndl;
    }
    ostr << "BEGIN TREES;";
    if (force_win_eol) {
        ostr << "\r\n";
    }
    else {
        ostr << NcbiEndl;
    }
    ostr <<  "TREE " << tree_name << " = ";
    x_PrintNewickTree(ostr, *m_Dyntree.GetTreeNode());
    if (force_win_eol) {
        ostr << "\r\n";
    }
    else {
        ostr << NcbiEndl;
    }
    ostr << "END";
    if (force_win_eol) {
        ostr << "\r\n";
    }
    else {
        ostr << NcbiEndl;
    }

    return true;
}

bool CGuideTree::SaveTreeAs(CNcbiOstream& ostr, ETreeFormat format)
{
    switch (format) {
    case eImage :
        return WriteImage(ostr);

    case eASN :
        return WriteTree(ostr);

    case eNewick :
        return PrintNewickTree(ostr);

    case eNexus :
        return PrintNexusTree(ostr);
    }

    return false;
}


void CGuideTree::FullyExpand(void)
{
    TreeDepthFirstTraverse(*m_Dyntree.GetTreeNodeNonConst(), CExpander());
}


void CGuideTree::SimplifyTree(ETreeSimplifyMode method)
{

    switch (method) {

    // Do nothing
    case eNone:
        break;
        
    // Collapse all subtrees with common blast name       
    case eBlastName :
    {
        FullyExpand();
        CPhyloTreeNodeGroupper groupper 
            = TreeDepthFirstTraverse(*m_Dyntree.GetTreeNodeNonConst(), 
                   CPhyloTreeNodeGroupper(kBlastNameTag, kNodeColorTag,
                                          m_Dyntree));

        if (!groupper.GetError().empty()) {
            NCBI_THROW(CGuideTreeException, eTraverseProblem,
                       groupper.GetError());
        }

        x_CollapseSubtrees(groupper);
        break;
    }

    //Fully expand the tree        
    case eFullyExpanded :
        FullyExpand();
        break;

    default:
      NCBI_THROW(CGuideTreeException, eInvalidOptions,
                 "Invalid tree simplify mode");
    }

    m_SimplifyMode = method;
}

bool CGuideTree::ExpandCollapseSubtree(int node_id)
{
    CBioTreeDynamic::CBioNode* node = x_GetBioNode(node_id);
    
    if (x_IsExpanded(*node)) {

        // Collapsing
        x_Collapse(*node);

        // Track labels in order to select proper color for collapsed node
        CPhyloTreeLabelTracker 
            tracker = TreeDepthFirstTraverse(*node, CPhyloTreeLabelTracker(
                                                                kBlastNameTag, 
                                                                kNodeColorTag,
                                                                kLabelBgColorTag,
                                                                m_QueryNodeId,
                                                                m_Dyntree));

        if (!tracker.GetError().empty()) {
            NCBI_THROW(CGuideTreeException, eTraverseProblem,
                       tracker.GetError());
        }

        CPhyloTreeLabelTracker::TLabelColorMap_I it = tracker.Begin();
        string label = it->first;
        ++it;
        for (; it != tracker.End(); ++it) {
            label += ", " + it->first;
        }
        node->SetFeature(kLabelTag, label);
        if (tracker.GetNumLabels() == 1) {
            node->SetFeature(kNodeColorTag, tracker.Begin()->second);
        }
        // Mark collapsed subtree that contains query node
        if (IsQueryNodeSet()) {
            x_MarkCollapsedQueryNode(node);
        }
    }
    else {

        // Expanding
        x_Expand(*node);
        node->SetFeature(kNodeColorTag, "");
    }

    m_SimplifyMode = eNone;
    return x_IsExpanded(*node);
}

// Traverse tree from new root up to old root and change parents to children
// @param node Parent of new root
// @param fid Feature id for node's distance (child's edge length)
void s_RerootUpstream(CBioTreeDynamic::CBioNode* node, TBioTreeFeatureId fid)
{
    _ASSERT(node);

    CBioTreeDynamic::CBioNode* parent
        = (CBioTreeDynamic::CBioNode*)node->GetParent();

    if (parent->GetParent()) {
        s_RerootUpstream((CBioTreeDynamic::CBioNode*)node->GetParent(), fid);
    }

    parent->GetValue().features.SetFeature(fid,
                              node->GetValue().features.GetFeatureValue(fid));

    node = parent->DetachNode(node);
    node->AddNode(parent);    
}

void CGuideTree::RerootTree(int new_root_id)
{
    CBioTreeDynamic::CBioNode* node = x_GetBioNode(new_root_id);

    // Collapsed node cannot be a root, so a parent node will be a new
    // root if such node is selected
    if (node && x_IsLeafEx(*node) && node->GetParent()) {
        node = (CBioTreeDynamic::CBioNode*)node->GetParent();
    }

    // if new root is already a root, do nothing
    if (!node->GetParent()) {
        return;
    }

    // tree is deleted when new dyntree root is set, hence the old
    // root node must be copied and its children moved
    CBioTreeDynamic::CBioNode* old_root = m_Dyntree.GetTreeNodeNonConst();

    // get old root children
    vector<CBioTreeDynamic::CBioNode*> children;
    CBioTreeDynamic::CBioNode::TParent::TNodeList_I it
        = old_root->SubNodeBegin();

    for(; it != old_root->SubNodeEnd();it++) {
        children.push_back((CBioTreeDynamic::CBioNode*)*it);
    }
    NON_CONST_ITERATE (vector<CBioTreeDynamic::CBioNode*>, ch, children) {
        old_root->DetachNode(*ch);
    }
    
    // copy old root node and attach old root's children
    CBioTreeDynamic::CBioNode* new_old_root
        = new CBioTreeDynamic::CBioNode(*old_root);    
    ITERATE (vector<CBioTreeDynamic::CBioNode*>, ch, children) {
        new_old_root->AddNode(*ch);
    }
  
    // detach new root from the tree
    CBioTreeDynamic::CBioNode* parent
        = (CBioTreeDynamic::CBioNode*)node->GetParent();
    node = parent->DetachNode(node);
   
    // replace child - parent relationship in all parents of the new root
    s_RerootUpstream(parent, (TBioTreeFeatureId)eDistId);

    // make new root's parent its child and set new tree root
    node->AddNode(parent);
    m_Dyntree.SetTreeNode(node);
    parent->SetFeature(kDistTag, node->GetFeature(kDistTag));
    node->SetFeature(kDistTag, "0");
}

bool CGuideTree::ShowSubtree(int root_id)
{
    CBioTreeDynamic::CBioNode* node = x_GetBioNode(root_id);
    _ASSERT(node);

    // If a collapsed node is clicked, then it needs to be expanded
    // in order to show the subtree
    bool collapsed = false;
    if (!x_IsExpanded(*node)) {
        collapsed = true;
        x_Expand(*node);
        m_SimplifyMode = eNone;        
    }

    // replace root, unused part of the tree will be deleted
    CBioTreeDynamic::CBioNode::TParent* parent = node->GetParent();
    if (parent) {
        parent->DetachNode(node);
        m_Dyntree.SetTreeNode(node);
    }

    return collapsed;
}


void CGuideTree::x_ExtendRoot(void)
{
    _ASSERT(!m_DataSource.Empty());

    // Often the edge starting in root has zero length which
    // does not render well. By setting very small distance this is
    // avoided.
    CPhyloTreeNode::TNodeList_I it = m_DataSource->GetTree()->SubNodeBegin();
    while (it != m_DataSource->GetTree()->SubNodeEnd()) {
        if ((*it)->GetValue().GetDistance() == 0.0) {
            (*it)->GetValue().SetDistance(
                 m_DataSource->GetNormDistance() * 0.005);
            (*it)->GetValue().Sync();
        }
        ++it;
    }

    m_DataSource->Refresh();
}

bool CGuideTree::IsSingleBlastName(void)
{
    CSingleBlastNameExaminer examiner
        = TreeDepthFirstTraverse(*m_Dyntree.GetTreeNodeNonConst(), 
                                 CSingleBlastNameExaminer(m_Dyntree));
    return examiner.IsSingleBlastName();
}


auto_ptr<CGuideTree::STreeInfo> CGuideTree::GetTreeInfo(int node_id)
{
    if (m_DataSource.Empty()) {
        NCBI_THROW(CGuideTreeException, eInvalid, "CGuideTree::"
                   "GetTreeInfo(int) function must be called after tree is "
                   "rendered");
    }
    CPhyloTreeNode* node = x_GetNode(node_id);

    // find all subtree leaves
    CLeaveFinder leave_finder = TreeDepthFirstTraverse(*node, CLeaveFinder());
    vector<CPhyloTreeNode*>& leaves = leave_finder.GetLeaves();

    TBioTreeFeatureId fid_blast_name = x_GetFeatureId(kBlastNameTag);

    TBioTreeFeatureId fid_node_color = x_GetFeatureId(kNodeColorTag);

    TBioTreeFeatureId fid_seqid = x_GetFeatureId(kSeqIDTag);
    TBioTreeFeatureId fid_accession = x_GetFeatureId(kAccessionNbrTag);
    

    auto_ptr<STreeInfo> info(new STreeInfo);
    hash_set<string> found_blast_names;

    // for each leave get feature values
    ITERATE (vector<CPhyloTreeNode*>, it, leaves) {
        string blast_name = x_GetNodeFeature(*it, fid_blast_name);
        string color = x_GetNodeFeature(*it, fid_node_color);
        string seqid = x_GetNodeFeature(*it, fid_seqid);
        string accession = x_GetNodeFeature(*it, fid_accession);
        
        // only one entry per blast name for color map
        if (found_blast_names.find(blast_name) == found_blast_names.end()) {
            info->blastname_color_map.push_back(make_pair(blast_name, color));
            found_blast_names.insert(blast_name);
        }

        info->seq_ids.push_back(seqid);
        info->accessions.push_back(accession);
    }

    return info;
}


void CGuideTree::x_Init(void)
{
    m_Width = 800;
    m_Height = 600;
    m_ImageFormat = CImageIO::ePng;
    m_RenderFormat = eRect;
    m_DistanceMode = true;
    m_NodeSize = 3;
    m_LineWidth = 1;
    m_QueryNodeId = -1;
    m_SimplifyMode = eNone;
}


CPhyloTreeNode* CGuideTree::x_GetNode(int id, bool throw_if_null,
                                      CPhyloTreeNode* root)
{
    if (m_DataSource.Empty()) {
        NCBI_THROW(CGuideTreeException, eInvalid, "Empty data source");
    }

    CPhyloTreeNode* tree = root ? root : m_DataSource->GetTree();
    CNodeFinder node_finder = TreeDepthFirstTraverse(*tree, CNodeFinder(id));

    if (!node_finder.GetNode() && throw_if_null) {
        NCBI_THROW(CGuideTreeException, eNodeNotFound, (string)"Node "
                   + NStr::IntToString(id) + (string)" not found");
    }

    return node_finder.GetNode();
}

CBioTreeDynamic::CBioNode* CGuideTree::x_GetBioNode(TBioTreeNodeId id)
{
    CBioNodeFinder finder = TreeDepthFirstTraverse(
                                            *m_Dyntree.GetTreeNodeNonConst(),
                                            CBioNodeFinder(id));

    if (!finder.GetNode()) {
        NCBI_THROW(CGuideTreeException, eNodeNotFound, (string)"Node "
                   + NStr::IntToString(id) + (string)" not found");
    }

    return finder.GetNode();
}

bool CGuideTree::x_IsExpanded(const CBioTreeDynamic::CBioNode& node)
{
    return node.GetFeature(kCollapseTag) == s_kSubtreeDisplayed;
}

bool CGuideTree::x_IsLeafEx(const CBioTreeDynamic::CBioNode& node)
{
    return node.IsLeaf() || !x_IsExpanded(node);
}

void CGuideTree::x_Collapse(CBioTreeDynamic::CBioNode& node)
{
    node.SetFeature(kCollapseTag, "1");
}

void CGuideTree::x_Expand(CBioTreeDynamic::CBioNode& node)
{
    node.SetFeature(kCollapseTag, s_kSubtreeDisplayed);
}

inline
TBioTreeFeatureId CGuideTree::x_GetFeatureId(const string& tag)
{
    const CBioTreeFeatureDictionary& fdict = m_DataSource->GetDictionary();
    if (!fdict.HasFeature(tag)) {
        NCBI_THROW(CGuideTreeException, eInvalid, "Feature " + tag
                   + " not present");
    }

    return fdict.GetId(tag);
}

inline
string CGuideTree::x_GetNodeFeature(const CPhyloTreeNode* node,
                                    TBioTreeFeatureId fid)
{
    _ASSERT(node);

    const CBioTreeFeatureList& flist = node->GetValue().GetBioTreeFeatureList();
    return flist.GetFeatureValue(fid);
}


void CGuideTree::PreComputeImageDimensions()
{
    if (m_DataSource.Empty()) {
        m_DataSource.Reset(new CPhyloTreeDataSource(m_Dyntree));
        m_DataSource->Relabel("$(label)");
        x_ExtendRoot();
    }
    m_PhyloTreeScheme.Reset(new CPhyloTreeScheme());

    m_PhyloTreeScheme->SetSize(CPhyloTreeScheme::eNodeSize) = m_NodeSize;
    m_PhyloTreeScheme->SetSize(CPhyloTreeScheme::eLineWidth) = m_LineWidth;
    m_PhyloTreeScheme->SetLabelStyle(CPhyloTreeScheme::eSimpleLabels);

    GLdouble mleft = 10;
    GLdouble mtop = 10;
    GLdouble mright = 140;
    GLdouble mbottom = 10;

    //For now until done automatically  
    if(m_RenderFormat == eRadial || m_RenderFormat == eForce) {
        mleft = 200;
    }
    m_PhyloTreeScheme->SetMargins(mleft, mtop, mright, mbottom);

    // Min dimensions check
    // The minimum width and height which should be acceptable to output 
    // image to display all data without information loss.
    auto_ptr<IPhyloTreeRenderer> calcRender;       
    //use recatngle for calulations of all min sizes
    calcRender.reset(new CPhyloRectCladogram());      
    m_MinDimRect = IPhyloTreeRenderer::GetMinDimensions(*m_DataSource,
                                                        *calcRender,
                                                        *m_PhyloTreeScheme);
}

void CGuideTree::x_CreateLayout(void)
{
    if (m_PhyloTreeScheme.Empty() || m_DataSource.Empty()) {
        PreComputeImageDimensions();
    }

    // Creating pane
    m_Pane.reset(new CGlPane());

    switch (m_RenderFormat) {
    case eSlanted : 
        m_Renderer.reset(new CPhyloSlantedCladogram());
        break;

    case eRadial : 
        m_Renderer.reset(new CPhyloRadial());
        break;

    case eForce : 
        m_Renderer.reset(new CPhyloForce());
        break;

    default:
    case eRect : 
        m_Renderer.reset(new CPhyloRectCladogram());
    }

    m_Renderer->SetDistRendering(m_DistanceMode);
 
    // Creating layout
    m_Renderer->SetFont(new CGlBitmapFont(CGlBitmapFont::eHelvetica10));
    
    
    m_Renderer->SetModelDimensions(m_Width, m_Height);

    // Setting variable sized collapsed nodes
    m_PhyloTreeScheme->SetBoaNodes(true);

    m_Renderer->SetScheme(*m_PhyloTreeScheme);    

    m_Renderer->SetZoomablePrimitives(false);

    m_Renderer->Layout(m_DataSource.GetObject());


    // modifying pane to reflect layout changes
    m_Pane->SetAdjustmentPolicy(CGlPane::fAdjustAll, CGlPane::fAdjustAll);
    m_Pane->SetMinScaleX(1 / 30.0);  m_Pane->SetMinScaleY(1 / 30.0);
    m_Pane->SetOriginType(CGlPane::eOriginLeft, CGlPane::eOriginBottom);
    m_Pane->EnableZoom(true, true);
    m_Pane->SetViewport(TVPRect(0, 0, m_Width, m_Height));
    m_Pane->SetModelLimitsRect(m_Renderer->GetRasterRect());
    m_Pane->SetVisibleRect(m_Renderer->GetRasterRect());

    m_Pane->ZoomAll();
}

void CGuideTree::x_Render(void)
{
   glClearColor(0.95f, 1.0f, 0.95f, 1.0f);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   m_Renderer->Render(*m_Pane.get(), m_DataSource.GetObject()); 
}


bool CGuideTree::x_RenderImage(void)
{
    if (m_DataSource.Empty()) {
        m_DataSource.Reset(new CPhyloTreeDataSource(m_Dyntree));
        m_DataSource->Relabel("$(label)");
        x_ExtendRoot();
    }

    bool success = false;
   
    try {


        m_Context.Reset(new CGlOsContext(m_Width, m_Height));
        m_Context->MakeCurrent();         

        x_CreateLayout();
        x_Render();

        glFinish();
        m_Context->SetBuffer().SetDepth(3);
        CImageUtil::FlipY(m_Context->SetBuffer());

        success = true;
    }
    catch (CException& e) {
        m_ErrorMessage = "Error rendering image: " + e.GetMsg();

    }
    catch (exception& e) {
        m_ErrorMessage = "Error rendering image: " + (string)e.what();
    }
    catch (...) {
        m_ErrorMessage = "Error rendering image: unknown error";
    }
    
    return success;
}    


//TO DO: Input parameter should be an interface for future groupping options
void CGuideTree::x_CollapseSubtrees(CPhyloTreeNodeGroupper& groupper)
{
    for (CPhyloTreeNodeGroupper::CLabeledNodes_I it=groupper.Begin();
         it != groupper.End(); ++it) {
        x_Collapse(*it->GetNode());
        it->GetNode()->SetFeature(kLabelTag, it->GetLabel());
        it->GetNode()->SetFeature(kNodeColorTag, it->GetColor());
        if (m_QueryNodeId > -1) {
            x_MarkCollapsedQueryNode(it->GetNode());
        }
    }
}


void CGuideTree::x_MarkCollapsedQueryNode(CBioTreeDynamic::CBioNode* node)
{
    //Check if query node is in the collapsed subtree
    // and marking query node
    if (m_QueryNodeId >= 0) {
        CBioNodeFinder finder = TreeDepthFirstTraverse(*node,
                                               CBioNodeFinder(m_QueryNodeId));

        CBioTreeDynamic::CBioNode* query_node = finder.GetNode();
        if (query_node) {
            const string& color = query_node->GetFeature(kLabelBgColorTag);
            node->SetFeature(kLabelBgColorTag, color);
        }
    }

} 

//Recusrive
void CGuideTree::x_PrintNewickTree(CNcbiOstream& ostr,
                                   const CBioTreeDynamic::CBioNode& node,
                                   bool is_outer_node)
{

    string label;

    if (!node.IsLeaf()) {
        ostr << '(';
        for (CBioTreeDynamic::CBioNode::TNodeList_CI it = node.SubNodeBegin(); it != node.SubNodeEnd(); ++it) {
            if (it != node.SubNodeBegin())
                ostr << ", ";
            x_PrintNewickTree(ostr, (CBioTreeDynamic::CBioNode&)**it, false);
        }
        ostr << ')';
    }

    if (!is_outer_node) {
        label = node.GetFeature(kLabelTag);
        if (node.IsLeaf() || !label.empty()) {
            for (size_t i=0;i < label.length();i++)
                if (!isalpha(label.at(i)) && !isdigit(label.at(i)))
                    label.at(i) = '_';
            ostr << label;
        }
        ostr << ':' << node.GetFeature(kDistTag);
    }
    else
        ostr << ';';
}

void CGuideTree::x_InitTreeLabels(CBioTreeContainer &btc,
                                  CGuideTree::ELabelType lblType) 
{
   
    NON_CONST_ITERATE (CNodeSet::Tdata, node, btc.SetNodes().Set()) {
        if ((*node)->CanGetFeatures()) {
            string  blastName = "";
            //int id = (*node)->GetId();
            CRef< CNodeFeature > label_feature_node;
            CRef< CNodeFeature > selected_feature_node;
            int featureSelectedID = eLabelId;
            if (lblType == eSeqId || lblType == eSeqIdAndBlastName) {
//              featureSelectedID = s_kSeqIdId;
                featureSelectedID = eAccessionNbrId;                
            }
            else if (lblType == eTaxName) {
                featureSelectedID = eOrganismId;
            }                
            else if (lblType == eSeqTitle) {
                featureSelectedID = eTitleId;
            }
            else if (lblType == eBlastName) {
                featureSelectedID = eBlastNameId;
            }
            
            NON_CONST_ITERATE (CNodeFeatureSet::Tdata, node_feature,
                               (*node)->SetFeatures().Set()) {

                
                //typedef list< CRef< CNodeFeature > > Tdata

                if ((*node_feature)->GetFeatureid() == eLabelId) { 
                   label_feature_node = *node_feature;                                       
                }
                //If label typ = GI and blast name - get blast name here
                if ((*node_feature)->GetFeatureid() == eBlastNameId && 
                                    lblType == eSeqIdAndBlastName) { 
                   blastName = (*node_feature)->GetValue();
                }
            
                if ((*node_feature)->GetFeatureid() == featureSelectedID) {
                    // a terminal node
                    // figure out which sequence this corresponds to
                    // from the numerical id we stuck in as a label
                    selected_feature_node = *node_feature;                                        
                }                
            }
            if(label_feature_node.NotEmpty() && selected_feature_node.NotEmpty())
            {
                string  label = selected_feature_node->GetValue();
                if(lblType == eSeqIdAndBlastName) {
                    //concatinate with blastName
                    label = label + "(" + blastName + ")";
                }
                //label_feature_node->SetValue(label);
                label_feature_node->ResetValue();
                label_feature_node->SetValue() = label;                
            }
        }
    }            
}



int CGuideTree::GetRootNodeID()
{
    return(m_DataSource->GetTree()->GetValue().GetId());
}


CRef<CBioTreeContainer> CGuideTree::GetSerialTree(void)
{
    CRef<CBioTreeContainer> btc(new CBioTreeContainer());
    BioTreeConvert2Container(*btc, m_Dyntree);

    return btc;
}


//Sets data for display purposes marking the selected node on the tree
void CGuideTree::SetSelection(int hit)
{
    if (m_DataSource.Empty()) {
        NCBI_THROW(CGuideTreeException, eInvalid, "CGuideTree::"
                   "SetSelection(int) function must be called after tree is "
                   "rendered");
    }
    
    if ((hit+1) >=0){
        CPhyloTreeNode * node = x_GetNode(hit, false);
        if (node) {
            m_DataSource->SetSelection(node, true, true, true);
            m_DataSource->Save(m_Dyntree);
        }
    }
}


string CGuideTree::GetMap(string jsClickNode,string jsClickLeaf,string jsMouseover,string jsMouseout,string jsClickQuery,bool showQuery)
                                        
{
    if (m_DataSource.Empty()) {
        NCBI_THROW(CGuideTreeException, eInvalid, "CGuideTree::"
                   "GetMap(...) function must be called after tree is "
                   "rendered");
    }

    CGuideTreeCGIMap map_visitor(m_Pane.get(),jsClickNode,jsClickLeaf,jsMouseover,jsMouseout,jsClickQuery,showQuery);    
        map_visitor = TreeDepthFirstTraverse(*m_DataSource->GetTree(), map_visitor);
        return map_visitor.GetMap();
}


// Get SeqID string from CBioseq_Handle such as gi|36537373
// If getGIFirst tries to get gi. If gi does not exist tries to get be 'Best ID'
static string s_GetSeqIDString(CBioseq_Handle& handle, bool get_gi_first)
{
    CSeq_id_Handle seq_id_handle;
    bool get_best_id = true;

    if(get_gi_first) {        
        try {
            seq_id_handle = sequence::GetId(handle, sequence::eGetId_ForceGi);
            if(seq_id_handle.IsGi()) get_best_id = false;
        }
        catch(CException& e) {
            //x_TraceLog("sequence::GetId-ForceGi error"  + (string)e.what());
            //seq_id_handle = sequence::GetId(handle,sequence::eGetId_Best);                 
        }
    }

    if(get_best_id) {
        seq_id_handle = sequence::GetId(handle, sequence::eGetId_Best);                 
    }
    CConstRef<CSeq_id> seq_id = seq_id_handle.GetSeqId();

    string id_string;
    (*seq_id).GetLabel(&id_string);

    return id_string;
}

// Generate Blast Name-based colors for tree leaves
// TO DO: This needs to be redesigned
#define MAX_NODES_TO_COLOR 24
static string s_GetBlastNameColor(
                   CGuideTree::TBlastNameColorMap& blast_name_color_map,
                   string blast_tax_name)
{
    string color = "";

    //This should be rewritten in more elegant way
    string colors[MAX_NODES_TO_COLOR] 
                   = {"0 0 255", "0 255 0", "191 159 0", "30 144 255",
                      "255 0 255", "223 11 95", "95 79 95", "143 143 47",
                      "0 100 0", "128 0 0", "175 127 255", "119 136 153",
                      "255 69 0", "205 102 0", "0 250 154", "173 255 47",
                      "139 0 0", "255 131 250", "155 48 255", "205 133 0",
                      "127 255 212", "255 222 173", "221 160 221", "200 100 0"};


    unsigned int i = 0;
    for(;i < blast_name_color_map.size();i++) {
        pair<string, string>& map_item = blast_name_color_map[i];

        if(map_item.first == blast_tax_name) {
            color = map_item.second;
            break;
        }
    }
    
    if(color == "") { //blast name not in the map
        if(blast_name_color_map.size() >= MAX_NODES_TO_COLOR) {
            i = MAX_NODES_TO_COLOR - 1;
        }
        color = colors[i];
        blast_name_color_map.push_back(make_pair(blast_tax_name, color));
    }
    return color;
}    


void CGuideTree::x_InitTreeFeatures(CBioTreeContainer& btc,
                                    const vector< CRef<CSeq_id> >& seqids,
                                    CScope& scope,
                                    CGuideTree::ELabelType label_type,
                                    bool mark_query_node,
                                    TBlastNameColorMap& bcolormap,
                                    int query_node_id)
{
    CTaxon1 tax;

    bool success = tax.Init();
    if (!success) {
        NCBI_THROW(CGuideTreeException, eTaxonomyError,
                   "Problem initializing taxonomy information.");
    }
    
    // Come up with some labels for the terminal nodes
    int num_rows = (int)seqids.size();
    vector<string> labels(num_rows);
    vector<string> organisms(num_rows);
    vector<string> accession_nbrs(num_rows);    
    vector<string> titles(num_rows);
    vector<string> blast_names(num_rows);    
    vector<string> tax_node_colors(num_rows);
    vector<CBioseq_Handle> bio_seq_handles(num_rows);

    for (int i=0;i < num_rows;i++) {
        bio_seq_handles[i] = scope.GetBioseqHandle(*seqids[i]);

        int tax_id = 0;
        try{
            const COrg_ref& org_ref = sequence::GetOrg_ref(bio_seq_handles[i]);                                
            organisms[i] = org_ref.GetTaxname();
            tax_id = org_ref.GetTaxId();
            if (success) {
                tax.GetBlastName(tax_id, blast_names[i]);
            }
            else {
                blast_names[i] = s_kUnknown;
            }
        }
        catch(CException& e) {            
            organisms[i] = s_kUnknown;
            blast_names[i]= s_kUnknown;
        }

        try{
            titles[i] = sequence::GetTitle(bio_seq_handles[i]);
        }
        catch(CException& e) {
            titles[i] = s_kUnknown;
        }
                   
        //May be we need here eForceAccession here ???
        CSeq_id_Handle accession_handle = sequence::GetId(bio_seq_handles[i],
                                                        sequence::eGetId_Best);

        CConstRef<CSeq_id> accession = accession_handle.GetSeqId();
        (*accession).GetLabel(&accession_nbrs[i]);

        tax_node_colors[i] = s_GetBlastNameColor(bcolormap,
                                                 blast_names[i]);

        switch (label_type) {
        case eTaxName:
            labels[i] = organisms[i];
            break;

        case eSeqTitle:
            labels[i] = titles[i];
            break;

        case eBlastName:
            labels[i] = blast_names[i];
            break;

        case eSeqId:
            labels[i] = accession_nbrs[i];
            break;

        case eSeqIdAndBlastName:
            labels[i] = accession_nbrs[i] + "(" + blast_names[i] + ")";
            break;
        }


        if (labels[i].empty()) {
            CSeq_id_Handle best_id_handle = sequence::GetId(bio_seq_handles[i],
                                                       sequence::eGetId_Best);

            CConstRef<CSeq_id> best_id = best_id_handle.GetSeqId();
            (*best_id).GetLabel(&labels[i]);            
        }

    }
    
    // Add attributes to terminal nodes
    x_AddFeatureDesc(eSeqIdId, kSeqIDTag, btc);
    x_AddFeatureDesc(eOrganismId, kOrganismTag, btc);
    x_AddFeatureDesc(eTitleId, kSeqTitleTag, btc);
    x_AddFeatureDesc(eAccessionNbrId, kAccessionNbrTag, btc);
    x_AddFeatureDesc(eBlastNameId, kBlastNameTag, btc);
    x_AddFeatureDesc(eAlignIndexId, kAlignIndexIdTag, btc);
    x_AddFeatureDesc(eNodeColorId, kNodeColorTag, btc);
    x_AddFeatureDesc(eLabelColorId, kLabelColorTag, btc);
    x_AddFeatureDesc(eLabelBgColorId, kLabelBgColorTag, btc);
    x_AddFeatureDesc(eLabelTagColorId, kLabelTagColor, btc);
    x_AddFeatureDesc(eTreeSimplificationTagId, kCollapseTag, btc);

    
    NON_CONST_ITERATE (CNodeSet::Tdata, node, btc.SetNodes().Set()) {
        if ((*node)->CanGetFeatures()) {
            NON_CONST_ITERATE (CNodeFeatureSet::Tdata, node_feature,
                               (*node)->SetFeatures().Set()) {
                if ((*node_feature)->GetFeatureid() == eLabelId) {
                    // a terminal node
                    // figure out which sequence this corresponds to
                    // from the numerical id we stuck in as a label
                    
                    string label_id = (*node_feature)->GetValue();
                    unsigned int seq_number;
                    if(!isdigit((unsigned char) label_id[0])) {
                        const char* ptr = label_id.c_str();
                        // For some reason there is "N<number>" now, 
                        // not numerical any more. Need to skip "N" 
                        seq_number = NStr::StringToInt((string)++ptr);
                    }
                    else {
                        seq_number = NStr::StringToInt(
                                                 (*node_feature)->GetValue());                    
                    }
                    // Replace numeric label with real label
                    (*node_feature)->SetValue(labels[seq_number]);

                    //Gets gi, if cnnot gets best id
                    string id_string 
                        = s_GetSeqIDString(bio_seq_handles[seq_number], true);

                    x_AddFeature(eSeqIdId, id_string, node); 

                    // add organism attribute if possible
                    if (!organisms[seq_number].empty()) {
                        x_AddFeature(eOrganismId, organisms[seq_number], node);
                    }

                    // add seq-title attribute if possible
                    if (!titles[seq_number].empty()) {
                        x_AddFeature(eTitleId, titles[seq_number], node); 
                    }
                    // add blast-name attribute if possible
                    if (!accession_nbrs[seq_number].empty()) {
                        x_AddFeature(eAccessionNbrId, accession_nbrs[seq_number],
                                     node);
                    }

                    // add blast-name attribute if possible
                    if (!blast_names[seq_number].empty()) {
                        x_AddFeature(eBlastNameId, blast_names[seq_number],
                                     node);
                    }                   

                    x_AddFeature(eAlignIndexId, NStr::IntToString(seq_number),
                                 node); 

                    x_AddFeature(eNodeColorId,
                                 tax_node_colors[seq_number], node);                         

                    if(seq_number == 0 && mark_query_node) { 
                        // color for query node
                        x_AddFeature(eLabelBgColorId,
                                     s_kQueryNodeBgColor, node); 

                        x_AddFeature(eLabelTagColorId,
                                     s_kQueryNodeColor, node); 

                        //Not sure if needed
                        //m_QueryAccessionNbr = accession_nbrs[seq_number];

                        query_node_id = (*node).GetObject().GetId();
                    }
                    
                    // done with this node
                    break;
                }
            }
            x_AddFeature(eTreeSimplificationTagId, s_kSubtreeDisplayed, node);
        }
    }      
}


// Add feature descriptor in bio tree
void CGuideTree::x_AddFeatureDesc(int id, const string& desc, 
                                  CBioTreeContainer& btc) 
{
    CRef<CFeatureDescr> feat_descr(new CFeatureDescr);
    feat_descr->SetId(id);
    feat_descr->SetName(desc);
    btc.SetFdict().Set().push_back(feat_descr);
}   


// Add feature to a node in bio tree
void CGuideTree::x_AddFeature(int id, const string& value,
                              CNodeSet::Tdata::iterator iter) 
{
    CRef<CNodeFeature> node_feature(new CNodeFeature);
    node_feature->SetFeatureid(id);
    node_feature->SetValue(value);
    (*iter)->SetFeatures().Set().push_back(node_feature);
}

ETreeTraverseCode
CGuideTree::CExpander::operator()(CBioTreeDynamic::CBioNode& node, int delta)
{
    if (delta == 0 || delta == 1) {
        if (node.GetFeature(kCollapseTag) != s_kSubtreeDisplayed
            && !node.IsLeaf()) {
            
            node.SetFeature(kCollapseTag, s_kSubtreeDisplayed);
            node.SetFeature(kNodeColorTag, "");
        }
    }
    return eTreeTraverse;
}



void CGuideTreeCGIMap::x_FillNodeMapData(CPhyloTreeNode &  tree_node) 
{           
       if (!(*tree_node).GetDictionaryPtr()) {
           NCBI_THROW(CException, eInvalid,
                      "Tree node has no feature dicionary");
       }

       int x  = m_Pane->ProjectX((*tree_node).XY().first);
       int y  = m_Pane->GetViewport().Height() - m_Pane->ProjectY((*tree_node).XY().second);           
       int ps = 5;

       
       string strTooltip = (*tree_node).GetLabel().size()?(*tree_node).GetLabel():"No Label";
           

       int nodeID = (*tree_node).GetId();
       bool isQuery = false;
       
       //IsLeaf() instead of IsLeafEx() allows popup menu for collapsed nodes
       if(tree_node.IsLeaf()) {
            const CBioTreeFeatureList& featureList = (*tree_node).GetBioTreeFeatureList();
       
            if (m_ShowQuery && (*tree_node).GetDictionaryPtr()->HasFeature(CGuideTree::kAlignIndexIdTag)){ //"align-index"
                int align_index = NStr::StringToInt(
                                   featureList.GetFeatureValue(        
                                       (*tree_node).GetDictionaryPtr()->GetId(
                                           CGuideTree::kAlignIndexIdTag)));

                isQuery = align_index == 0; //s_kPhyloTreeQuerySeqIndex  
            }       

            if(isQuery) {
                strTooltip += "(query)";
            }
            m_Map += "<area alt=\""+strTooltip+"\" title=\""+strTooltip+"\""; 

            string accessionNbr;
            if ((*tree_node).GetDictionaryPtr()->HasFeature(
                               CGuideTree::kAccessionNbrTag)){ //accession-nbr

                accessionNbr = featureList.GetFeatureValue(
                                  (*tree_node).GetDictionaryPtr()->GetId(
                                        CGuideTree::kAccessionNbrTag));            
            }
            string arg = NStr::IntToString(nodeID);
            if(!accessionNbr.empty()) {
                arg += ",'" + accessionNbr + "'";
            }            
            if (isQuery && !m_JSClickQueryFunction.empty()) {
                m_Map += " href=\"" + m_JSClickQueryFunction + arg+");\""; 
            }
            else if(!isQuery && !m_JSClickLeafFunction.empty()) {
                    //m_Map += " href=\"" + string(JS_SELECT_NODE_FUNC) + NStr::IntToString(nodeID)+");\"";
                m_Map += " href=\"" + m_JSClickLeafFunction + arg+");\"";                
            }
       }
       else {           
            m_Map += "<area";
            if(!m_JSMouseoverFunction.empty())
                //"javascript:setupPopupMenu(" 
                m_Map += " onmouseover=\"" + m_JSMouseoverFunction + NStr::IntToString(nodeID)+ ");\"";
                    
            if(!m_JSMouseoutFunction.empty())
                //"PopUpMenu2_Hide(0);"
                m_Map += " onmouseout=\"" + m_JSMouseoutFunction + "\"";
            if(!m_JSClickNodeFunction.empty()) 
                //"//javascript:MouseHit(" //string(JS_SELECT_NODE_FUNC)
                m_Map += " href=\"" + m_JSClickNodeFunction + NStr::IntToString(nodeID)+");\"";       
       } 
       m_Map += " coords=\"";
       m_Map += NStr::IntToString(x-ps); m_Map+=",";
       m_Map += NStr::IntToString(y-ps); m_Map+=",";
       m_Map += NStr::IntToString(x+ps); m_Map+=",";
       m_Map += NStr::IntToString(y+ps); 
       m_Map+="\">";      
}

