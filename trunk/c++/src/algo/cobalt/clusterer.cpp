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

File name: clusterer.cpp

Author: Greg Boratyn

Contents: Implementation of CClusterer class

******************************************************************************/


#include <ncbi_pch.hpp>
#include <algo/cobalt/clusterer.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(cobalt);

static void s_CheckDistMatrix(const CClusterer::TDistMatrix& dmat)
{
    if (dmat.GetRows() != dmat.GetCols()) {
        NCBI_THROW(CClustererException, eInvalidOptions,
                   "Distance matrix is not square");
    }
}


CClusterer::CClusterer(const CClusterer::TDistMatrix& dmat)
    : m_DistMatrix(new TDistMatrix(dmat))
{
    s_CheckDistMatrix(*m_DistMatrix);
}

CClusterer::CClusterer(auto_ptr<CClusterer::TDistMatrix>& dmat)
    : m_DistMatrix(dmat)
{
    s_CheckDistMatrix(*m_DistMatrix);
}

static void s_PurgeTrees(vector<TPhyTreeNode*>& trees)
{
    NON_CONST_ITERATE(vector<TPhyTreeNode*>, it, trees) {
        delete *it;
        *it = NULL;
    }
    trees.clear();
}

CClusterer::~CClusterer()
{
    s_PurgeTrees(m_Trees);
}

void CClusterer::SetDistMatrix(const TDistMatrix& dmat)
{
    s_CheckDistMatrix(dmat);

    m_DistMatrix.reset(new TDistMatrix());
    m_DistMatrix->Resize(dmat.GetRows(), dmat.GetCols());
    copy(dmat.begin(), dmat.end(), m_DistMatrix->begin());
}

void CClusterer::SetDistMatrix(auto_ptr<TDistMatrix>& dmat)
{
    s_CheckDistMatrix(*dmat);

    m_DistMatrix = dmat;
}

const CClusterer::TDistMatrix& CClusterer::GetDistMatrix(void) const
{
    if (!m_DistMatrix.get()) {
        NCBI_THROW(CClustererException, eNoDistMatrix,
                   "Distance matrix not assigned");
    }

    return *m_DistMatrix;
}


const CClusterer::TSingleCluster&
CClusterer::GetSingleCluster(size_t index) const
{
    if (index >= m_Clusters.size()) {
        NCBI_THROW(CClustererException, eClusterIndexOutOfRange,
                   "Cluster index out of range");
    }

    return m_Clusters[index];
}


// Finds maximum distance between given element and element in given cluster
static double s_FindMaxDistFromElem(int elem, 
                                    const CClusterer::TSingleCluster& cluster, 
                                    const CClusterer::TDistMatrix& dmat) {

    _ASSERT(cluster.size() > 0);
    _ASSERT(elem < (int)dmat.GetRows());

    double result = 0.0;
    ITERATE(CClusterer::TSingleCluster, elem_it, cluster) {
        if (dmat(elem, *elem_it) > result) {

            _ASSERT(*elem_it < (int)dmat.GetRows());

            result = dmat(elem, *elem_it);
        }
    }

    return result;
}

// Finds maximum distance between any pair of elements from given clusters
static double s_FindClusterDist(const CClusterer::TSingleCluster& cluster1,
                                const CClusterer::TSingleCluster& cluster2,
                                const CClusterer::TDistMatrix& dmat) {

    _ASSERT(cluster1.size() > 0 && cluster2.size() > 0);

    double result = 0.0;
    ITERATE(CClusterer::TSingleCluster, elem, cluster1) {

        _ASSERT(*elem < (int)dmat.GetRows());

        double dist = s_FindMaxDistFromElem(*elem, cluster2, dmat);
        if (dist > result) {
            result = dist;
        }
    }

    return result;
}

/// Find sum of distances between given element and cluster elements
static double s_FindSumDistFromElem(int elem, 
                                    const CClusterer::TSingleCluster& cluster, 
                                    const CClusterer::TDistMatrix& dmat) {

    _ASSERT(cluster.size() > 0);
    _ASSERT(elem < (int)dmat.GetRows());

    double result = 0.0;
    ITERATE(CClusterer::TSingleCluster, elem_it, cluster) {
            _ASSERT(*elem_it < (int)dmat.GetRows());

            result += dmat(elem, *elem_it);
    }

    return result;
}

/// Find mean distance between cluster elements
static double s_FindMeanDist(const CClusterer::TSingleCluster& cluster1,
                             const CClusterer::TSingleCluster& cluster2,
                             const CClusterer::TDistMatrix& dmat)
{
    _ASSERT(cluster1.size() > 0 && cluster2.size() > 0);

    double result = 0.0;
    ITERATE(CClusterer::TSingleCluster, elem, cluster1) {

        _ASSERT(*elem < (int)dmat.GetRows());

        result += s_FindSumDistFromElem(*elem, cluster2, dmat);
    }
    result /= (double)(cluster1.size() * cluster2.size());

    return result;    
}


/// Find mean
static double s_FindMean(const vector<double>& vals)
{
    double result = 0.0;
    if (!vals.empty()) {
        ITERATE(vector<double>, it, vals) {
            result += *it;
        }
        result /= (double)vals.size();
    }
    return result;
}

// Complete Linkage clustering with dendrograms
void CClusterer::ComputeClusters(double max_diam, bool do_trees)
{
    m_Clusters.clear();

    size_t num_elements = m_DistMatrix->GetRows();

    // If there is exactly one element to cluster
    if (num_elements == 1) {
        m_Clusters.resize(1);
        m_Clusters[0].AddElement(0);

        return;
    }

    // If there are exactly two elements
    if (num_elements == 2) {
        if ((*m_DistMatrix)(0, 1) < max_diam) {
            m_Clusters.resize(1);
            m_Clusters[0].AddElement(0);
            m_Clusters[0].AddElement(1);
        }
        else {
            m_Clusters.resize(2);
            m_Clusters[0].AddElement(0);
            m_Clusters[1].AddElement(1);
        }

        return;
    }

    // If there are at least 3 elements to cluster

    // Checking whether the data is in one cluster
    // skip if tree is requested
    if (!do_trees) {
        double max_dist = 0.0;
        for (size_t i=0;i < num_elements - 1;i++) {
            for (size_t j=i+1;j < num_elements;j++) {
                if ((*m_DistMatrix)(i, j) > max_dist) {
                    max_dist = (*m_DistMatrix)(i, j);
                }
            }
        }
        // If yes, create the cluster and return
        if (max_dist <= max_diam) {
            m_Clusters.resize(1);
            for (int i=0;i < (int)num_elements;i++) {
                m_Clusters[0].AddElement(i);
            }
            return;
        }
    }


    // Getting to this point means that there are at least two clusters
    // and max distance is larger than max_diam
    // or tree was requested

    // Creating working copy of dist matrix
    TDistMatrix dmat(*m_DistMatrix);
    // Denotes whether an entry in dist matrix and clusters is used
    vector<bool> used_entry(num_elements, true);

    // Creating initial one-element clusters 
    TClusters clusters(num_elements);
    for (size_t i=0;i < num_elements;i++) {
        clusters[i].AddElement((int)i);
    }

    // Create leaf nodes for dendrogram
    vector<TPhyTreeNode*> nodes(num_elements);
    if (do_trees) {
        for (size_t i=0;i < nodes.size();i++) {
            nodes[i] = new TPhyTreeNode();
            nodes[i]->GetValue().SetId(i);

            // This is needed so that serialized tree can be interpreted
            // by external applications
            nodes[i]->GetValue().SetLabel(NStr::IntToString(i));
        }
    }
    vector< vector<double> > dists_to_root(num_elements);

    // Computing clusters
    // Exits the loop once minimum distance is larger than max_diam
    // It was checked above that such distance exists
    int num_clusters = num_elements;
    while (num_clusters > 1) {

        // Find first used dist matrix entries
        size_t min_i = 0;
        size_t min_j;
        do {
            while (!used_entry[min_i] && min_i < num_elements) {
                min_i++;
            }
        
            min_j = min_i + 1;
            while (!used_entry[min_j] && min_j < num_elements) {
                min_j++;
            }

            if (min_j >= num_elements) {
                min_i++;
            }
        } while (min_j >= num_elements && min_i < num_elements);

        // A distance larger than max_diam exists in the dist matrix,
        // then there always should be at least two used entires in dist matrix
        _ASSERT(do_trees || (min_i < num_elements && min_j < num_elements));

        // Find smallest distance entry
        for (size_t i=0;i < num_elements - 1;i++) {
            for (size_t j=i+1;j < num_elements;j++) {
                
                if (!used_entry[i] || !used_entry[j]) {
                    continue;
                }

                if (dmat(i, j) < dmat(min_i, min_j)) {
                    min_i = i;
                    min_j = j;
                }            
            }
        }

        _ASSERT(used_entry[min_i] && used_entry[min_j]);

        // Check whether the smallest distance is larger than max_diam
        if (dmat(min_i, min_j) > max_diam) {
            break;
        }

        _ASSERT(clusters[min_i].size() > 0 && clusters[min_j].size() > 0);
        _ASSERT(min_i < num_elements && min_j < num_elements);

        // Joining tree nodes
        // must be done before joining clusters
        if (do_trees) {
            TPhyTreeNode* new_root = new TPhyTreeNode();
            
            _ASSERT(nodes[min_i]);
            _ASSERT(nodes[min_j]);

            // left and right are meaningless, only to make code readble
            const int left = min_i;
            const int right = min_j;

            new_root->AddNode(nodes[left]);
            new_root->AddNode(nodes[right]);

            // set sub node distances
            double dist = s_FindMeanDist(clusters[left], clusters[right],
                                         *m_DistMatrix);

            // find average distances too root in left and right subtrees 
            double mean_dist_to_root_left = s_FindMean(dists_to_root[left]);
            double mean_dist_to_root_right = s_FindMean(dists_to_root[right]);
            
            // set edge length between new root and subtrees
            double left_edge_length = dist - mean_dist_to_root_left;
            double right_edge_length = dist - mean_dist_to_root_right;
            left_edge_length = left_edge_length > 0.0 ? left_edge_length : 0.0;
            right_edge_length 
                = right_edge_length > 0.0 ? right_edge_length : 0.0;

            nodes[left]->GetValue().SetDist(left_edge_length);
            nodes[right]->GetValue().SetDist(right_edge_length);
            
            // compute distances from leaves to new root
            if (dists_to_root[left].empty()) {
                dists_to_root[left].push_back(dist);
            }
            else {
                NON_CONST_ITERATE(vector<double>, it, dists_to_root[left]) {
                    *it += left_edge_length;
                }
            }

            if (dists_to_root[right].empty()) {
                dists_to_root[right].push_back(dist);
            }
            else {
                NON_CONST_ITERATE(vector<double>, it, dists_to_root[right]) {
                    *it += right_edge_length;
                }
            }

            // merge cluster related information
            // Note: the extended and included cluster must correspond to
            // Joining clusters procedure below
            nodes[min_i] = new_root;
            nodes[min_j] = NULL;
            ITERATE(vector<double>, it, dists_to_root[min_j]) {
                dists_to_root[min_i].push_back(*it);
            }
            dists_to_root[min_j].clear();
        }

        // Joining clusters
        TSingleCluster& extended_cluster = clusters[min_i];
        TSingleCluster& included_cluster = clusters[min_j];
        used_entry[min_j] = false;
        num_clusters--;
        _ASSERT(!do_trees || !nodes[min_j]);

        ITERATE(TSingleCluster, elem, included_cluster) {
            extended_cluster.AddElement(*elem);
        }
        
        // Updating distance matrix
        // Distance between clusters is the largest pairwise distance
        for (size_t i=0;i < min_i && min_i > 0;i++) {
            if (!used_entry[i]) {
                continue;
            }

            double dist = s_FindClusterDist(clusters[i], included_cluster, 
                                            *m_DistMatrix);
            if (dist > dmat(i, min_i)) {
                dmat(i, min_i) = dist;
            }
        }
            for (size_t j=min_i+1;j < num_elements;j++) {
            if (!used_entry[j]) {
                continue;
            }

            double dist = s_FindClusterDist(clusters[j], included_cluster,
                                            *m_DistMatrix);
            if (dist > dmat(min_i, j)) {
                dmat(min_i, j) = dist;
            }

        }
        clusters[min_j].clear();
    }

    // Putting result in class attribute
    for (size_t i=0;i < num_elements;i++) {
        if (used_entry[i]) {

            _ASSERT(clusters[i].size() > 0);

            m_Clusters.push_back(TSingleCluster());
            TClusters::iterator it = m_Clusters.end();
            --it;
            ITERATE(TSingleCluster, elem, clusters[i]) {
                it->AddElement(*elem);
            }

            if (do_trees) {
                _ASSERT(nodes[i]);

                m_Trees.push_back((TPhyTreeNode*)nodes[i]);
            }
        }
        else {

            _ASSERT(clusters[i].size() == 0);

            if (do_trees) {
                _ASSERT(!nodes[i]);
                _ASSERT(dists_to_root[i].empty());
            }
        }
    }
}


void CClusterer::GetTrees(vector<TPhyTreeNode*>& trees) const
{
    trees.clear();
    ITERATE(vector<TPhyTreeNode*>, it, m_Trees) {
        trees.push_back(*it);
    }
}


void CClusterer::ReleaseTrees(vector<TPhyTreeNode*>& trees)
{
    trees.clear();
    ITERATE(vector<TPhyTreeNode*>, it, m_Trees) {
        trees.push_back(*it);
    }
    m_Trees.clear();
}

#ifdef NCBI_COMPILER_WORKSHOP
// In some configurations, cobalt.o winds up with an otherwise
// undefined reference to a slightly mismangled name!  The compiler's
// own README recommends this workaround for such incidents.
#pragma weak "__1cEncbiGcobaltKCClustererMReleaseTrees6MrnDstdGvector4Cpn0AJCTreeNode4n0AMCPhyNodeData_n0AVCDefaultNodeKeyGetter4n0E_____n0DJallocator4Cp4_____v_" = "__1cEncbiGcobaltKCClustererMReleaseTrees6MrnDstdGvector4Cpn0AJCTreeNode4n0AMCPhyNodeData_n0AVCDefaultNodeKeyGetter4n0E_____n0DJallocator4C5_____v_"
#endif


const TPhyTreeNode* CClusterer::GetTree(int index) const
{
    if (index < 0 || index >= (int)m_Trees.size()) {
        NCBI_THROW(CClustererException, eClusterIndexOutOfRange,
                   "Tree index out of range");
    }

    return m_Trees[index];
}


TPhyTreeNode* CClusterer::ReleaseTree(int index)
{
    if (index < 0 || index >= (int)m_Trees.size()) {
        NCBI_THROW(CClustererException, eClusterIndexOutOfRange,
                   "Tree index out of range");
    }

    TPhyTreeNode* result = m_Trees[index];
    m_Trees[index] = NULL;

    return result;
}


void CClusterer::SetPrototypes(void) {

    NON_CONST_ITERATE(TClusters, cluster, m_Clusters) {
        cluster->SetPrototype(cluster->FindCenterElement(*m_DistMatrix));
    }
}


void CClusterer::GetClusterDistMatrix(int index, TDistMatrix& mat) const
{
    if (index >= (int)m_Clusters.size()) {
        NCBI_THROW(CClustererException, eClusterIndexOutOfRange,
                   "Cluster index out of range");
    }

    const CSingleCluster& cluster = m_Clusters[index];

    mat.Resize(cluster.size(), cluster.size(), 0.0);
    for (size_t i=0;i < cluster.size() - 1;i++) {
        for (size_t j=i+1;j < cluster.size();j++) {

            if (cluster[i] >= (int)m_DistMatrix->GetRows()
                || cluster[j] >= (int)m_DistMatrix->GetRows()) {
                NCBI_THROW(CClustererException, eElementOutOfRange,
                           "Distance matrix size is smaller than number of"
                           " elements");
            }

            mat(i, j) = mat(j, i) = (*m_DistMatrix)(cluster[i], cluster[j]);
        }
    }
}

void CClusterer::Reset(void)
{
    s_PurgeTrees(m_Trees);
    m_Clusters.clear();
    PurgeDistMatrix();
}


//------------------------------------------------------------

int CClusterer::CSingleCluster::operator[](size_t index) const
{
    if (index >= m_Elements.size()) {
        NCBI_THROW(CClustererException, eElemIndexOutOfRange,
                   "Cluster element index out of range");
    }

    return m_Elements[index];
}


int CClusterer::CSingleCluster::FindCenterElement(const TDistMatrix& 
                                                  dmatrix) const
{
    if (m_Elements.empty()) {
        NCBI_THROW(CClustererException, eInvalidOptions, "Cluster is empty");
    }


    if (m_Elements.size() == 1) {
        return m_Elements[0];
    }

    vector<double> sum_distance;
    sum_distance.resize(m_Elements.size());
    for (size_t i=0;i < m_Elements.size(); i++) {
        double dist = 0.0;
        for (size_t j=0;j < m_Elements.size();j++) {
            if (i == j) {
                continue;
            }

            dist += dmatrix(m_Elements[i], m_Elements[j]);
        }
        sum_distance[i] = dist;
    }

    size_t min_index = 0;
    for (size_t i=1;i < sum_distance.size();i++) {
        if (sum_distance[i] < sum_distance[min_index]) {
            min_index = i;
        }
    }

    return m_Elements[min_index];
}


