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

/// @guide_tree_calc.cpp
/// Phylogenetic tree calulation

#include <ncbi_pch.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbifloat.h>    

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/taxon1/taxon1.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/align_ci.hpp>

#include <algo/cobalt/tree.hpp>
#include <util/bitset/ncbi_bitset.hpp>

#include <math.h>

#include "guide_tree.hpp"
#include "guide_tree_calc.hpp"


USING_NCBI_SCOPE;
USING_SCOPE(objects);



//static const int    s_kLabelId = 0;
//static const int    s_kSeqIdId = 2;
//static const int    s_kOrganismId = 3;
//static const int    s_kTitleId = 4;
//static const int    s_kAccessionNbr = 5;
//static const int    s_kBlastName = 6;
//static const int    s_kAlignIndexId = 7;
//static const int    s_kQueryNodeColorId = 8;
//static const int    s_kQueryLabelColorId = 9;
//static const int    s_kQueryLabelBgColorId = 10;
//static const int    s_kQueryLabelTagColorId = 11;
static const int    s_kTreeSimplificationTagId = 12;
static const string s_kQueryNodeColor = "255 0 0";
static const string s_kQueryLabelColor = "176 0 0";
static const string s_kQueryNodeBgColor = "255 255 0";

static const string s_kUnknown = "unknown";
static const string s_kSubtreeDisplayed = "0";


CGuideTreeCalc::CDistMatrix::CDistMatrix(int num_elements)
    : m_NumElements(num_elements), m_Diagnol(0.0)
{
    if (num_elements > 0) {
        m_Distances.resize(num_elements * num_elements - num_elements, -1.0);
    }
}

void CGuideTreeCalc::CDistMatrix::Resize(int num_elements)
{
    m_NumElements = num_elements;
    if (num_elements > 0) {
        m_Distances.resize(num_elements * num_elements - num_elements);
    }
}

const double& CGuideTreeCalc::CDistMatrix::operator()(int i, int j) const
{
    if (i >= m_NumElements || j >= m_NumElements || i < 0 || j < 0) {
        NCBI_THROW(CGuideTreeCalcException, eDistMatrixError,
                   "Distance matrix index out of bounds");
    }

    if (i == j) {
        return m_Diagnol;
    }

    if (i < j) {
        swap(i, j);
    }

    int index = (i*i - i) / 2 + j;
    _ASSERT(index < (int)m_Distances.size());

    return m_Distances[index];
}

double& CGuideTreeCalc::CDistMatrix::operator()(int i, int j)
{
    if (i >= m_NumElements || j >= m_NumElements || i < 0 || j < 0) {
        NCBI_THROW(CGuideTreeCalcException, eDistMatrixError,
                   "Distance matrix index out of bounds");
    }

    if (i == j) {
        NCBI_THROW(CGuideTreeCalcException, eDistMatrixError,
                   "Distance matrix diagnol elements cannot be set");
    }

    if (i < j) {
        swap(i, j);
    }

    int index = (i*i - i) / 2 + j;
    _ASSERT(index < (int)m_Distances.size());

    return m_Distances[index];
}


CGuideTreeCalc::CGuideTreeCalc(const CSeq_align& seq_aln,
                               CRef<CScope> scope,
                               const string& query_id)
    : m_Scope(scope),
      m_QuerySeqId(query_id)
{
    x_Init();
    x_InitAlignDS(seq_aln);
}


CGuideTreeCalc::CGuideTreeCalc(CRef<CSeq_align_set> &seqAlignSet,
                               CRef<CScope> scope)
    : m_Scope(scope)      
{
    x_Init();
    x_InitAlignDS(seqAlignSet);
}

const CBioTreeContainer& CGuideTreeCalc::GetSerialTree(void) const
{
    if (m_TreeContainer.Empty()) {
        NCBI_THROW(CGuideTreeCalcException, eNoTree,
                   "Tree was not constructed");
    }
    return *m_TreeContainer;
}

auto_ptr<CBioTreeDynamic> CGuideTreeCalc::GetTree(void)
{
    auto_ptr<CBioTreeDynamic> dyntree(new CBioTreeDynamic());

    BioTreeConvertContainer2Dynamic(*dyntree, *m_TreeContainer);

    return dyntree;
}


CRef<CSeq_align> CGuideTreeCalc::GetSeqAlign(void) const
{
    CRef<CDense_seg> denseg(new CDense_seg());
    denseg->Assign(m_AlignDataSource->GetDenseg());

    CRef<CSeq_align> seqalign(new CSeq_align());
    seqalign->SetType(CSeq_align::eType_global);
    seqalign->SetSegs().SetDenseg(*denseg);
    seqalign->SetDim(denseg->GetDim());

    return seqalign;
}


static const string& s_GetSequence(const CAlnVec& avec,
                                   vector<string> &seq_strings,
                                   unsigned int index)
{
    // The seq_strings array has to be pre-allocated
    _ASSERT(index < seq_strings.size());

    if (!seq_strings[index].empty()) {
        return seq_strings[index];
    }
    else {
        avec.GetWholeAlnSeqString(index, seq_strings[index]);
        return seq_strings[index];
    }
}

// Make sure that matrix has finite non-negative values
bool s_ValidateMatrix(const CGuideTreeCalc::CDistMatrix& mat)
{
    for (int i=0;i < mat.GetNumElements() - 1;i++) {
        for (int j=i+1;j < mat.GetNumElements();j++) {
            double val = mat(i, j);
            if (!finite(val) || val < 0.0) {
                return false;
            }
        }
    }
    return true;
}

// Calculate divergence matrix, considering max divergence constraint
bool CGuideTreeCalc::x_CalcDivergenceMatrix(vector<int>& included)
{
    int num_seqs = m_AlignDataSource->GetNumRows();

    bm::bvector<> bitvector; // which seqeunces will be included in phylo tree
    list<SLink> links;       // for storing dissimilariues between sequences
    vector<string> sequences(num_seqs);  // buffer for AA sequences

    // query has always zero index and is always included
    const int query_idx = 0;
    const string& query_seq = s_GetSequence(*m_AlignDataSource, sequences,
                                            query_idx);
    bitvector.set(query_idx);
    
    // Compute distances between query and each sequence
    // and include only sequences that are similar enough to the query

    // for each sequence except for query
    for (int i=1;i < num_seqs;i++) {

        // find divergence
        const string& seqi = s_GetSequence(*m_AlignDataSource, sequences, i);
        double dist = CDistMethods::Divergence(query_seq, seqi);
        _ASSERT(!finite(dist) || dist >= 0.0);

        // if divergence is finite and smaller than cutoff save divergence
        // and mark sequence as included
        if (finite(dist) && dist <= m_MaxDivergence) {
            links.push_back(SLink(query_idx, i, dist));
            bitvector.set(i);
        }
    }

    // Compute distances between incuded sequences
    list<SLink>::const_iterator it1 = links.begin();

    // for each included sequence 1
    for (; it1 != links.end() && it1->index1 == query_idx; ++it1) {

        // check if still included
        if (!bitvector.test(it1->index2)) {
            continue;
        }
        const string& seqi = s_GetSequence(*m_AlignDataSource, sequences,
                                           it1->index2);

        list<SLink>::const_iterator it2(it1);
        it2++;

        // for each sequence 2
        for (; it2 != links.end() && it2->index1 == query_idx; ++it2) {

            // check if still included
            if (!bitvector.test(it2->index2)) {
                continue;
            }
            const string& seqj = s_GetSequence(*m_AlignDataSource, sequences,
                                               it2->index2);

            // compute divergence
            double dist = CDistMethods::Divergence(seqi, seqj);
            _ASSERT(!finite(dist) || dist >= 0.0);

            // if divergence finite and smaller than cutoff save divergence
            if (finite(dist) && dist <= m_MaxDivergence) {
                links.push_back(SLink(it2->index2, it1->index2, dist));
            }
            else {
                 
                //TO DO: This should be changed to which divergence
                // otherwise exclude sequence less similar to the query
                int score_1 = m_AlignDataSource->CalculateScore(query_idx,
                                                                it1->index2);
                int score_2 = m_AlignDataSource->CalculateScore(query_idx,
                                                                it2->index2);
            
                bitvector[score_1 > score_2 ? it2->index2 : it1->index2]
                    = false;
            }
        }
    }

    unsigned int skip_array[bm::set_total_blocks] = {0,};
    bitvector.count_blocks(skip_array);

    // Get indices of included sequences
    bm::bvector<>::counted_enumerator en = bitvector.first();
    for (; en.valid(); ++en) {
        included.push_back((int)*en);
    }

    // Create divergence matrix and set values
    m_DivergenceMatrix.Resize(bitvector.count());
    // for each saved divergence
    ITERATE (list<SLink>, it, links) {
        // if both sequences included
        if (bitvector.test(it->index1) && bitvector.test(it->index2)) {

            // set divergence value using bit vector storage encoding

            int index1 = (it->index1 == 0 ? 0 : 
                    bitvector.count_range(0, it->index1 - 1, skip_array));

            int index2 = (it->index2 == 0 ? 0 : 
                    bitvector.count_range(0, it->index2 - 1, skip_array));

            m_DivergenceMatrix(index1, index2) = it->distance;
        }
    }
    _ASSERT(s_ValidateMatrix(m_DivergenceMatrix));

    return included.size() > 1;
}

static void s_RecordSeqId(int index,
                          const CAlnVec& align_data_source,
                          vector<string>& seqids)
{
    CSeq_id_Handle seq_id_handle 
        = sequence::GetId(align_data_source.GetBioseqHandle(index),
                          sequence::eGetId_Best);
                
    CConstRef<CSeq_id> seq_id = seq_id_handle.GetSeqId();
    string seq_id_str = "";
    (*seq_id).GetLabel(&seq_id_str);
            
    seqids.push_back(seq_id_str);
}

void CGuideTreeCalc::x_CreateValidAlign(const vector<int>& used_indices)
{
    if ((int)used_indices.size() < m_AlignDataSource->GetNumRows()) {

        // collect ids of removed sequences
        int index = 0;
        ITERATE (vector<int>, it, used_indices) {
            for (; index < *it; index++) {
                s_RecordSeqId(index, *m_AlignDataSource, m_RemovedSeqIds);
            }
            index++;
        }
        for (; index < m_AlignDataSource->GetNumRows();index++) {
            s_RecordSeqId(index, *m_AlignDataSource, m_RemovedSeqIds);
        }
        _ASSERT(used_indices.size() + m_RemovedSeqIds.size()
                == (size_t)m_AlignDataSource->GetNumRows());


        // Old implementation
//        m_AlignDataSource = x_CreateSubsetAlign(m_AlignDataSource, used_indices);

        CRef<CDense_seg> new_denseg
            = m_AlignDataSource->GetDenseg().ExtractRows(used_indices);

        m_AlignDataSource.Reset(new CAlnVec(*new_denseg, *m_Scope));
    }
}

// Calculate pairwise distances for sequeneces in m_AlignDataSource
void CGuideTreeCalc::x_CalcDistMatrix(void)
{
    _ASSERT(!m_DivergenceMatrix.Empty());

    // create dist matrix data structure required by CDistMethos
    CDistMethods::TMatrix pmat(m_DivergenceMatrix.GetNumElements(),
                               m_DivergenceMatrix.GetNumElements(),
                               0.0);

    for (int i=0;i < m_DivergenceMatrix.GetNumElements() - 1;i++) {
        for (int j=i+1; j < m_DivergenceMatrix.GetNumElements();j++) {
            pmat(i, j) = pmat(j, i) = m_DivergenceMatrix(i, j);
        }
    }

    // prepare structure for string results of distance computation
    m_FullDistMatrix.Resize(m_DivergenceMatrix.GetNumElements(),
                            m_DivergenceMatrix.GetNumElements(),
                            0.0);

    // compute distances
    switch (m_DistMethod) {
    case eJukesCantor : 
        CDistMethods::JukesCantorDist(pmat, m_FullDistMatrix);
        break;

    case ePoisson :
        CDistMethods::PoissonDist(pmat, m_FullDistMatrix);
        break;

    case eKimura :
        CDistMethods::KimuraDist(pmat, m_FullDistMatrix);
        break;

    case eGrishin :
        CDistMethods::GrishinDist(pmat, m_FullDistMatrix);
        break;

    case eGrishinGeneral :
        CDistMethods::GrishinGeneralDist(pmat, m_FullDistMatrix);
        break;

    default:
        NCBI_THROW(CGuideTreeCalcException, eInvalidOptions,
                   "Invalid distance calculation method");
    }

    // store distances in memory efficient data structure
    m_DistMatrix.Resize(m_FullDistMatrix.GetRows());
    for (int i=0;i < m_DistMatrix.GetNumElements() - 1;i++) {
        for (int j=i+1;j < m_DistMatrix.GetNumElements();j++) {
            m_DistMatrix(i, j) = m_FullDistMatrix(i, j);
        }
    }
    _ASSERT(s_ValidateMatrix(m_DistMatrix));
}

// Compute phylogenetic tree
void CGuideTreeCalc::x_ComputeTree(bool correct)
{
    _ASSERT((size_t)m_AlignDataSource->GetNumRows()
            == m_FullDistMatrix.GetRows());

    // Build a tree based on distances
    // Use number as labels, and sort it out later
    // Number corresponds to the position in  alnVec
    vector<string> labels(m_AlignDataSource->GetNumRows());
    for (unsigned int i = 0;i < labels.size();i++) {
        labels[i] = NStr::IntToString(i);
    }
    
    // TODO: Re-rooting now handled through CTree class needs to be implemented
    // in CGuideTreeCalc
    cobalt::CTree tree;

    switch (m_TreeMethod) {
    case eNJ :
        tree.ComputeTree(m_FullDistMatrix, false);
        break;

    case eFastME :
        tree.ComputeTree(m_FullDistMatrix, true);
        break;

    default:
        NCBI_THROW(CGuideTreeCalcException, eInvalidOptions,
                   "Invalid tree reconstruction method");
    };

    // release memory used by full distance matrix
    m_FullDistMatrix.Resize(1, 1);

    if (!tree.GetTree()) {
        NCBI_THROW(CGuideTreeCalcException, eTreeComputationProblem,
                   "Tree was not created");
    }

    TPhyTreeNode* ptree = (TPhyTreeNode*)tree.GetTree();

    if (correct) {
        CDistMethods::ZeroNegativeBranches(ptree);
    }

    m_TreeContainer = MakeBioTreeContainer(ptree);

    x_InitTreeFeatures(*m_AlignDataSource);
}


bool CGuideTreeCalc::CalcBioTree(void)
{

    CDistMethods::TMatrix pmat;
    CDistMethods::TMatrix dmat;
    hash_set<int> removed_inds;
    vector<int> used_inds;

    bool valid;
    valid = x_CalcDivergenceMatrix(used_inds);

    if (valid) {

        if ((int)used_inds.size() < m_AlignDataSource->GetNumRows()) {
            int initial_num_seqs = m_AlignDataSource->GetNumRows();
            x_CreateValidAlign(used_inds);
            m_Messages.push_back(NStr::IntToString(initial_num_seqs
                                                   - used_inds.size())
                                 + " sequences were discarded due to"
                                 " divergence that exceeds maximum allowed.");
        }
        x_CalcDistMatrix();
        x_ComputeTree();

    }
    else {
        m_Messages.push_back("Sequence dissimilarity exceeds maximum"
                             " divergence.");
    }

    return valid;
}


/*
// Needed for displaying alignment
bool CGuideTreeCalc::CalcSelectedAligns(int selected_node_id)
{
    CRef<CAlnVec> alignvec;
    bool success = true;

    if(m_AlignDataSource.Empty()) {   
        // Create m_SeqAlign and initialize m_AlignDataSource
        success = x_InitAlignDS();
        if(success) {
            // Validate distances, if invalid seqs are found
            // initialize m_AlignDataSource with m_SeqAlign 
//            x_CreatePhyloValidAlign();
            CreateValidAlign();
        }
    }
    if(success) {        
        const CAlnVec& alnvec = m_AlignDataSource->GetAlnMgr();
        alignvec.Reset(const_cast<CAlnVec*>(&alnVec));        
    }
    return success;
}
*/


// Init parameters to default values
void CGuideTreeCalc::x_Init(void)
{
    m_DistMethod = eGrishin;
    m_TreeMethod = eFastME;
    m_LabelType = eSeqTitle;
    m_MarkQueryNode = true;
    m_QueryNodeId = -1;
    m_MaxDivergence = 0.85;
}



bool CGuideTreeCalc::x_InitAlignDS(CRef<CSeq_align_set> &seqAlignSet)
{
    bool success = true;

    if(seqAlignSet->Get().size() == 1) {   
        x_InitAlignDS(**(seqAlignSet->Get().begin()));        
    }
    else if(seqAlignSet->Get().size() > 1) {   
        CAlnMix mix;
        ITERATE (CSeq_annot::TData::TAlign, iter, seqAlignSet->Get()) {

            CRef<CSeq_align> seq_align = *iter;            
            mix.Add(**iter);        
        }

        CAlnMix::TMergeFlags merge_flags;
        merge_flags = CAlnMix::fGapJoin | CAlnMix::fTruncateOverlaps;
        mix.Merge(merge_flags);

        x_InitAlignDS(mix.GetSeqAlign());        
    }
    else {        
        success = false;
        NCBI_THROW(CGuideTreeCalcException, eInvalidOptions,
                   "Empty seqalign provided");
    }
    return success;
}



void CGuideTreeCalc::x_InitAlignDS(const CSeq_align& seq_aln)
{
    m_AlignDataSource.Reset(new CAlnVec(seq_aln.GetSegs().GetDenseg(),
                                        *m_Scope));
    m_AlignDataSource->SetGapChar('-');
    m_AlignDataSource->SetEndChar('-');
}


// Find the first segment that does not have all gaps
static void s_GetNonGapSegmentRange(const CAlnVec& avec,
                                    const vector<int>& align_index,
                                    int* seg_start, int* seg_stop)
{
    const size_t& num_segs = avec.GetNumSegs(); 
    const size_t& num_rows = avec.GetNumRows(); 
    const CDense_seg& denseSeq = avec.GetDenseg();
    const CDense_seg::TStarts&  starts  = denseSeq.GetStarts();
    
    bool segment_start = false;
    bool segment_stop = false;
    size_t seg;
    for (seg=0;seg < num_segs;seg++) {
        
        for(size_t i=1;i < align_index.size();i++)
        {
            int sel_aln_row = align_index[i];
            int curr_start = starts[seg*num_rows + sel_aln_row];
            if(curr_start != -1) {
                segment_start = true;
                break;
            }            
        }
        if(segment_start) {
            break;
        }
    }
    *seg_start = (seg < num_segs) ? seg : 0;
    for (seg=num_segs - 1;seg >= 0;seg--) {
        
        for(size_t i=1;i < align_index.size();i++)
        {
            int sel_aln_row = align_index[i];
            
            if(starts[seg * num_rows + sel_aln_row] != -1) {
                segment_stop = true;                
                break;
             }            
        }
        if(segment_stop) {
            break;
        }
    }
    *seg_stop = (seg > 0) ? seg + 1 : num_segs;
}


// Create seq_align from a subset of given alnvec
CRef<CAlnVec> CGuideTreeCalc::x_CreateSubsetAlign(const CRef<CAlnVec>& alnvec,
                                        const vector<int>& align_index) 
{
    CAlnVec avec(alnvec->GetDenseg(), alnvec->GetScope());
    avec.SetGapChar('-');  
    avec.SetEndChar('-');
   
    // Create dense-seg from curent avec
    const CDense_seg& dense_seq = avec.GetDenseg();
    const size_t& num_segs = avec.GetNumSegs(); 
    const size_t& num_rows = avec.GetNumRows(); 
    const CDense_seg::TStarts& starts  = dense_seq.GetStarts();
    const CDense_seg::TStrands& strands = dense_seq.GetStrands();
    const CDense_seg::TLens& lens = dense_seq.GetLens();

    // Create new dense-seg
    CRef<CDense_seg> new_dseg(new CDense_seg);
    CDense_seg::TStarts& new_starts = new_dseg->SetStarts();
    CDense_seg::TStrands& new_strands = new_dseg->SetStrands();
    CDense_seg::TLens& new_lens = new_dseg->SetLens();
    CDense_seg::TIds&  new_ids = new_dseg->SetIds();

    // Compute dimensions
    int new_num_rows = align_index.size();
    new_dseg->SetDim(new_num_rows);
    CDense_seg::TNumseg& new_num_segs = new_dseg->SetNumseg();
    new_num_segs = num_segs; // initialize

    // Assign ids
    new_ids.resize(new_num_rows);
    for (int i=0;i < new_num_rows;i++) {
        int sel_aln_row = align_index[i];
        const CSeq_id& seq_id = avec.GetSeqId(sel_aln_row);
        CRef<CSeq_id> selected_seq_id(new CSeq_id);
        SerialAssign(*selected_seq_id, seq_id);
        new_ids[i] = selected_seq_id;
    }
    new_lens.resize(new_num_segs);
    new_starts.resize(new_num_rows * new_num_segs, -1);
    if (!strands.empty()) {
        new_strands.resize(new_num_rows * new_num_segs, eNa_strand_minus);
    }
    
    // main loop through segments
    int new_seg = 0;

    // Delete the whole segment if all starts = -1 
    int seg_start = 0;
    int seg_stop = new_num_segs;
    if(align_index.size() > 1) {
        s_GetNonGapSegmentRange(avec, align_index, &seg_start, &seg_stop);
    }

    for (int seg=seg_start;seg < seg_stop;seg++) {

        new_lens[new_seg] = lens[seg];
        bool delete_gap_seg = true;
        bool seg_cont_query_ins = false;
        for (int row=0;row < new_num_rows;row++) {            
            int sel_aln_row = align_index[row];

            delete_gap_seg = (starts[seg*num_rows + sel_aln_row] == -1);


            new_starts[new_seg*new_num_rows + row] = starts[seg*num_rows
                                                            + sel_aln_row];
            if (!strands.empty()) {
                new_strands[new_seg*new_num_rows + row] = strands[seg*num_rows
                                                                  + sel_aln_row];
            }
            
        }        
        if(!delete_gap_seg) {
            new_seg++;
        }
        // If extra segment was inserted to keep query continious
        if(seg_cont_query_ins) {
           seg--;            // don't increment current seg
        }
    }
    new_num_segs = new_seg;
    
    new_lens.resize(new_num_segs);
    new_starts.resize(new_num_rows * new_num_segs, -1);
    if (!strands.empty()) {
        new_strands.resize(new_num_rows * new_num_segs);
    }
           
    new_dseg->Validate(true);

    CRef<CAlnVec> new_alnvec(new CAlnVec(*new_dseg, *m_Scope));
    return new_alnvec;
}


#define MAX_NODES_TO_COLOR 24
static string s_GetBlastNameColor(
                     CGuideTreeCalc::TBlastNameColorMap& blast_name_color_map,
                     string blast_tax_name)
{
    string color = "";

    //number of blast taxgroups ???
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

void CGuideTreeCalc::x_InitTreeFeatures(const CAlnVec& alnvec)
{
    _ASSERT(m_TreeContainer.NotEmpty());
    _ASSERT(m_Scope.NotEmpty());

    InitTreeFeatures(*m_TreeContainer, alnvec.GetDenseg().GetIds(),
                     *m_Scope, m_LabelType, m_MarkQueryNode,
                     m_BlastNameColorMap, m_QueryNodeId);
}


void CGuideTreeCalc::InitTreeFeatures(CBioTreeContainer& btc,
                                      const vector< CRef<CSeq_id> >& seqids,
                                      CScope& scope,
                                      CGuideTreeCalc::ELabelType label_type,
                                      bool mark_query_node,
                                      TBlastNameColorMap& bcolormap,
                                      int& query_node_id)
{
    CTaxon1 tax;

    bool success = tax.Init();
    if (!success) {
        NCBI_THROW(CGuideTreeCalcException, eTaxonomyError,
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
    x_AddFeatureDesc(eSeqIdId, CGuideTree::kSeqIDTag, btc);
    x_AddFeatureDesc(eOrganismId, CGuideTree::kOrganismTag, btc);
    x_AddFeatureDesc(eTitleId, CGuideTree::kSeqTitleTag, btc);
    x_AddFeatureDesc(eAccessionNbrId, CGuideTree::kAccessionNbrTag, btc);

    x_AddFeatureDesc(eBlastNameId, CGuideTree::kBlastNameTag, btc);
    x_AddFeatureDesc(eAlignIndexId, CGuideTree::kAlignIndexIdTag, btc);

    x_AddFeatureDesc(eNodeColorId, CGuideTree::kNodeColorTag, btc);

    x_AddFeatureDesc(eLabelColorId, CGuideTree::kLabelColorTag, btc);

    x_AddFeatureDesc(eLabelBgColorId, CGuideTree::kLabelBgColorTag, btc);

    x_AddFeatureDesc(eLabelTagColorId, CGuideTree::kLabelTagColor, btc);

    x_AddFeatureDesc(s_kTreeSimplificationTagId, CGuideTree::kCollapseTag, btc);

    
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
            x_AddFeature(s_kTreeSimplificationTagId, s_kSubtreeDisplayed, node);
        }
    }      
}

// Add feature descriptor in bio tree
void CGuideTreeCalc::x_AddFeatureDesc(int id, const string& desc, 
                                      CBioTreeContainer& btc) 
{
    CRef<CFeatureDescr> feat_descr(new CFeatureDescr);
    feat_descr->SetId(id);
    feat_descr->SetName(desc);
    btc.SetFdict().Set().push_back(feat_descr);
}   


// Add feature to a node in bio tree
void CGuideTreeCalc::x_AddFeature(int id, const string& value,
                                  CNodeSet::Tdata::iterator iter) 
{
    CRef<CNodeFeature> node_feature(new CNodeFeature);
    node_feature->SetFeatureid(id);
    node_feature->SetValue(value);
    (*iter)->SetFeatures().Set().push_back(node_feature);
}

