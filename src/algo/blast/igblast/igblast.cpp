#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */
/* ===========================================================================
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
 * Author:  Ning Ma
 *
 */

/** @file igblast.cpp
 * Implementation of CIgBlast.
 */

#include <ncbi_pch.hpp>
#include <algo/blast/igblast/igblast.hpp>
#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/api/bl2seq.hpp>
#include <algo/blast/api/remote_blast.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <objtools/alnmgr/alnmap.hpp>
#include <algo/blast/composition_adjustment/composition_constants.h>
#include <objmgr/object_manager.hpp>


/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

static void s_ReadLinesFromFile(const string& fn, vector<string>& lines)
{
    CNcbiIfstream fs(fn.c_str(), IOS_BASE::in);
    lines.clear();

    if (CFile(fn).Exists() && ! fs.fail()) {
        char line[256];
        while(true) {
            fs.getline(line, 256);
            if (fs.eof()) break;
            if (line[0] == '#') continue;
            string l(line);
            lines.push_back(l);
        }
    }
    fs.close();
};

CIgAnnotationInfo::CIgAnnotationInfo(CConstRef<CIgBlastOptions> &ig_opt)
{
    vector<string> lines;

    // read domain info from pdm or ndm file
    const string suffix = (ig_opt->m_IsProtein) ? "_gl.p.dm." : "_gl.n.dm.";
    string fn(SeqDB_ResolveDbPath("internal_data/" + ig_opt->m_Origin + suffix + ig_opt->m_DomainSystem));
    if (fn == "") {
        NCBI_THROW(CBlastException,  eInvalidArgument, 
              "Domain annotation data file could not be found in [internal_data] directory");
    }
    s_ReadLinesFromFile(fn, lines);
    int index = 0;
    ITERATE(vector<string>, l, lines) {
        vector<string> tokens;
        NStr::Tokenize(*l, " \t\n\r", tokens, NStr::eMergeDelims);
        if (!tokens.empty()) {
            m_DomainIndex[tokens[0]] = index;
            for (int i=1; i<11; ++i) {
                m_DomainData.push_back(NStr::StringToInt(tokens[i]));
            }
            index += 10;
            m_DomainChainType[tokens[0]] = tokens[11];
        } 
    }

    // read chain type info and frame info from aux files
    fn = ig_opt->m_ChainType;
    s_ReadLinesFromFile(fn, lines);
    if (lines.size() == 0) {
        ERR_POST(Warning << "Auxilary data file could not be found");
    }
    ITERATE(vector<string>, l, lines) {
        vector<string> tokens;
        NStr::Tokenize(*l, " \t\n\r", tokens, NStr::eMergeDelims);
        if (!tokens.empty()) {
            if (tokens[1] != "N/A") {
                m_ChainType[tokens[0]] = tokens[1];
            }
            int frame = NStr::StringToInt(tokens[2]);
            if (frame != -1) {
                m_FrameOffset[tokens[0]] = frame;
            }
        }
    }
};

CRef<CSearchResultSet>
CIgBlast::Run()
{
    vector<CRef <CIgAnnotation> > annots;
    CRef<CSearchResultSet> final_results;
    CRef<IQueryFactory> qf;
    CRef<CBlastOptionsHandle> opts_hndl(CBlastOptionsFactory
           ::Create((m_IgOptions->m_IsProtein)? eBlastp: eBlastn));
    CRef<CSearchResultSet> results[4], result;

    /*** search V germline */
    {
        x_SetupVSearch(qf, opts_hndl);
        CLocalBlast blast(qf, opts_hndl, m_IgOptions->m_Db[0]);
        results[0] = blast.Run();
        s_SortResultsByEvalue(results[0]);
        x_AnnotateV(results[0], annots);
    }

    /*** search V for domain annotation */
    {
        CLocalBlast blast(qf, opts_hndl, m_IgOptions->m_Db[3]);
        results[3] = blast.Run();
        x_AnnotateDomain(results[0], results[3], annots);
    }

    /*** search DJ germline */
    int num_genes =  (m_IgOptions->m_IsProtein) ? 1 : 3;
    if (num_genes > 1) {
        x_SetupDJSearch(annots, qf, opts_hndl);
        for (int gene = 1; gene < num_genes; ++gene) {
            CLocalBlast blast(qf, opts_hndl, m_IgOptions->m_Db[gene]);
            results[gene] = blast.Run();
        }
        x_AnnotateDJ(results[1], results[2], annots);
    }

    /*** collect germline search results */
    for (int gene = 0; gene  < num_genes; ++gene) {
        s_AppendResults(results[gene], m_IgOptions->m_NumAlign[gene], gene, final_results);
    }

    /*** search user specified db */
    x_SetupDbSearch(annots, qf);
    if (m_IsLocal) {
        CLocalBlast blast(qf, m_Options, m_LocalDb);
        blast.SetNumberOfThreads(m_NumThreads);
        result = blast.Run();
    } else {
        CRef<CRemoteBlast> blast;
        if (m_RemoteDb.NotEmpty()) {
            _ASSERT(m_Subject.Empty());
            blast.Reset(new CRemoteBlast(qf, m_Options, *m_RemoteDb));
        } else {
            blast.Reset(new CRemoteBlast(qf, m_Options, m_Subject));
        }
        result = blast->GetResultSet();
    }
    s_SortResultsByEvalue(result);
    s_AppendResults(result, -1, -1, final_results);

    /*** set the chain type infor */
    x_SetChainType(final_results, annots);

    /*** attach annotation info back to the results */
    s_SetAnnotation(annots, final_results);

    return final_results;
};

void CIgBlast::x_SetupVSearch(CRef<IQueryFactory>       &qf,
                              CRef<CBlastOptionsHandle> &opts_hndl)
{
    CBlastOptions & opts = opts_hndl->SetOptions();
    if (m_IgOptions->m_IsProtein) {
        opts.SetCompositionBasedStats(eNoCompositionBasedStats);
    } else {
        int penalty = m_Options->GetOptions().GetMismatchPenalty();
        opts.SetMatchReward(1);
        opts.SetMismatchPenalty(penalty);
        opts.SetWordSize(7);
        if (penalty == -1) {
            opts.SetGapOpeningCost(4);
            opts.SetGapExtensionCost(1);
        }
    }
    opts_hndl->SetEvalueThreshold(10.0);
    opts_hndl->SetFilterString("F");
    opts_hndl->SetHitlistSize(5);
    qf.Reset(new CObjMgr_QueryFactory(*m_Query));

};

void CIgBlast::x_SetupDJSearch(vector<CRef <CIgAnnotation> > &annots,
                               CRef<IQueryFactory>           &qf,
                               CRef<CBlastOptionsHandle>     &opts_hndl)
{
    // Only igblastn will search DJ
    CBlastOptions & opts = opts_hndl->SetOptions();
    opts.SetMatchReward(1);
    opts.SetMismatchPenalty(-4);
    opts.SetWordSize(5);
    opts.SetGapOpeningCost(5);
    opts.SetGapExtensionCost(2);
    opts_hndl->SetEvalueThreshold(1000.0);
    opts_hndl->SetFilterString("F");
    opts_hndl->SetHitlistSize(50);

    // Mask query for D, J search
    int iq = 0;
    ITERATE(vector<CRef <CIgAnnotation> >, annot, annots) {
        CRef<CBlastSearchQuery> query = m_Query->GetBlastSearchQuery(iq);
        CSeq_id *q_id = const_cast<CSeq_id *>(&*query->GetQueryId());
        int len = query->GetLength();
        if ((*annot)->m_GeneInfo[0] == -1) {
            // This is not a germline sequence.  Mask it out
            TMaskedQueryRegions mask_list;
            CRef<CSeqLocInfo> mask(
                  new CSeqLocInfo(new CSeq_interval(*q_id, 0, len-1), 0));
            mask_list.push_back(mask);
            m_Query->SetMaskedRegions(iq, mask_list);
        } else {
            // Excluding the V gene except the last 7 bp for D and J gene search;
            // also limit the J match to 150bp beyond V gene.
            bool ms = (*annot)->m_MinusStrand;
            int begin = (ms)? 
              (*annot)->m_GeneInfo[0] - 150: (*annot)->m_GeneInfo[1] - 7;
            int end = (ms)? 
              (*annot)->m_GeneInfo[0] + 7: (*annot)->m_GeneInfo[1] + 150;
            if (begin > 0) {
                CRef<CSeqLocInfo> mask(
                  new CSeqLocInfo(new CSeq_interval(*q_id, 0, begin-1), 0));
                m_Query->AddMask(iq, mask);
            }
            if (end < len) {
                CRef<CSeqLocInfo> mask(
                  new CSeqLocInfo(new CSeq_interval(*q_id, end, len-1), 0));
                m_Query->AddMask(iq, mask);
            }
        }
        ++iq;
    }

    // Generate query factory
    qf.Reset(new CObjMgr_QueryFactory(*m_Query));
};

void CIgBlast::x_SetupDbSearch(vector<CRef <CIgAnnotation> > &annots,
                               CRef<IQueryFactory>           &qf)
{
    // Options already passed in as m_Options.  Only set up (mask) the query
    int iq = 0;
    ITERATE(vector<CRef <CIgAnnotation> >, annot, annots) {
        CRef<CBlastSearchQuery> query = m_Query->GetBlastSearchQuery(iq);
        CSeq_id *q_id = const_cast<CSeq_id *>(&*query->GetQueryId());
        int len = query->GetLength();
        TMaskedQueryRegions mask_list;
        if ((*annot)->m_GeneInfo[0] ==-1) {
            // This is not a germline sequence.  Mask it out
            CRef<CSeqLocInfo> mask(
                      new CSeqLocInfo(new CSeq_interval(*q_id, 0, len-1), 0));
            mask_list.push_back(mask);
        } else if (m_IgOptions->m_FocusV) {
            // Restrict to V gene 
            int begin = (*annot)->m_GeneInfo[0];
            int end = (*annot)->m_GeneInfo[1];
            if (begin > 0) {
                CRef<CSeqLocInfo> mask(
                      new CSeqLocInfo(new CSeq_interval(*q_id, 0, begin-1), 0));
                mask_list.push_back(mask);
            }
            if (end < len) {
                CRef<CSeqLocInfo> mask(
                      new CSeqLocInfo(new CSeq_interval(*q_id, end, len-1), 0));
                mask_list.push_back(mask);
            }
        }
        m_Query->SetMaskedRegions(iq, mask_list);
        ++iq;
    }
    qf.Reset(new CObjMgr_QueryFactory(*m_Query));
};

void CIgBlast::x_AnnotateV(CRef<CSearchResultSet>        &results, 
                           vector<CRef <CIgAnnotation> > &annots)
{
    ITERATE(CSearchResultSet, result, *results) {

        CIgAnnotation *annot = new CIgAnnotation();
        annots.push_back(CRef<CIgAnnotation>(annot));
 
        if ((*result)->HasAlignments()) {
            CRef<CSeq_align> align = (*result)->GetSeqAlign()->Get().front();
            annot->m_GeneInfo[0] = align->GetSeqStart(0);
            annot->m_GeneInfo[1] = align->GetSeqStop(0)+1;
            annot->m_TopGeneIds[0] = align->GetSeq_id(1).AsFastaString();
        } 
    }
};

// Test if the alignment is already in the align_list
static bool s_SeqAlignInSet(CSeq_align_set::Tdata & align_list, CRef<CSeq_align> &align)
{
    ITERATE(CSeq_align_set::Tdata, it, align_list) {
        if ((*it)->GetSeq_id(1).Match(align->GetSeq_id(1)) &&
            (*it)->GetSeqStart(1) == align->GetSeqStart(1) &&
            (*it)->GetSeqStop(1) == align->GetSeqStop(1)) return true;
    }
    return false;
};

// Compare two seqaligns according to their evalue and coverage
static bool s_CompareSeqAlignByEvalue(const CRef<CSeq_align> &x, 
                                      const CRef<CSeq_align> &y)
{
    double sx, sy;
    x->GetNamedScore(CSeq_align::eScore_EValue, sx);
    y->GetNamedScore(CSeq_align::eScore_EValue, sy);
    if (sx != sy) return (sx < sy);
    int ix, iy;
    x->GetNamedScore(CSeq_align::eScore_Score, ix);
    y->GetNamedScore(CSeq_align::eScore_Score, iy);
    return (ix >= iy);
};

// Compare two seqaligns according to their evalue and coverage
static bool s_CompareSeqAlignByScore(const CRef<CSeq_align> &x, const CRef<CSeq_align> &y)
{
    int sx, sy;
    x->GetNamedScore(CSeq_align::eScore_Score, sx);
    y->GetNamedScore(CSeq_align::eScore_Score, sy);
    if (sx != sy) return (sx > sy);
    x->GetNamedScore(CSeq_align::eScore_IdentityCount, sx);
    y->GetNamedScore(CSeq_align::eScore_IdentityCount, sy);
    return (sx <= sy);
};

// Test if D and J annotation not compatible
static bool s_DJNotCompatible(const CSeq_align &d, const CSeq_align &j, bool ms)
{
    int ds = d.GetSeqStart(0);
    int de = d.GetSeqStop(0);
    int js = j.GetSeqStart(0);
    int je = j.GetSeqStop(0);
    if (ms) {
        if (ds < js + 3 || de < je + 3) return true;
    } else { 
        if (ds > js - 3 || de > je - 3) return true;
    }
    return false;
};

void CIgBlast::x_AnnotateDJ(CRef<CSearchResultSet>        &results_D,
                            CRef<CSearchResultSet>        &results_J,
                            vector<CRef <CIgAnnotation> > &annots)
{
    int iq = 0;
    NON_CONST_ITERATE(vector<CRef <CIgAnnotation> >, annot, annots) {

        string q_ct = (*annot)->m_ChainType[0];
        bool q_ms = (*annot)->m_MinusStrand;
        ENa_strand q_st = (q_ms) ? eNa_strand_minus : eNa_strand_plus;
        int q_ve = (q_ms) ? (*annot)->m_GeneInfo[0] : (*annot)->m_GeneInfo[1] - 1;

        CRef<CSeq_align_set> align_D, align_J;

        /* preprocess D */
        CSearchResults& res_D = (*results_D)[iq];
        if (res_D.HasAlignments()) {
            align_D.Reset(const_cast<CSeq_align_set *>
                                           (&*(res_D.GetSeqAlign())));
            CSeq_align_set::Tdata & align_list = align_D->Set();
            CSeq_align_set::Tdata::iterator it = align_list.begin();
            /* chain type test */
            if (q_ct!="VH" && q_ct!="N/A") {
                while (it != align_list.end()) {
                    it = align_list.erase(it);
                }
            }
            /* strand test */
            bool strand_found = false;
            ITERATE(CSeq_align_set::Tdata, it, align_list) {
                if ((*it)->GetSeqStrand(0) == q_st) {
                    strand_found = true;
                    break;
                }
            }
            if (strand_found) {
                it = align_list.begin();
                while (it != align_list.end()) {
                    if ((*it)->GetSeqStrand(0) != q_st) {
                        it = align_list.erase(it);
                    } else ++it;
                }
            }
            /* v end test */
            it = align_list.begin();
            while (it != align_list.end()) {
                bool keep = false;
                int q_ds = (*it)->GetSeqStart(0);
                int q_de = (*it)->GetSeqStop(0);
                if (q_ms) keep = (q_de >= q_ve - 30 && q_ds <= q_ve);
                else      keep = (q_ds <= q_ve + 30 && q_de >= q_ve);
                if (!keep) it = align_list.erase(it);
                else ++it;
            }
            /* sort according to score */
            align_list.sort(s_CompareSeqAlignByScore);
        }

        /* preprocess J */
        CSearchResults& res_J = (*results_J)[iq];
        if (res_J.HasAlignments()) {
            align_J.Reset(const_cast<CSeq_align_set *>
                                           (&*(res_J.GetSeqAlign())));
            CSeq_align_set::Tdata & align_list = align_J->Set();
            CSeq_align_set::Tdata::iterator it = align_list.begin();
            while (it != align_list.end()) {
                bool keep = true;
                /* chain type test */
                if (q_ct != "N/A") {
                    string sid = (*it)->GetSeq_id(1).AsFastaString();
                    if (sid.substr(0, 4) == "lcl|") sid = sid.substr(4, sid.length());
                    string sct = m_AnnotationInfo.GetChainType(sid);
                    if (sct != "N/A") {
                        if (q_ct == "VH" && sct != "JH" ||
                            q_ct != "VH" && sct == "JH") keep = false;
                    }
                }
                /* strand test */
                if ((*it)->GetSeqStrand(0) != q_st) keep = false;
                /* subject start test */
                if ((*it)->GetSeqStart(1) > 32) keep = false;
                /* v end test */
                int q_js = (*it)->GetSeqStart(0);
                int q_je = (*it)->GetSeqStop(0);
                if (q_ms) { 
                    if (q_je < q_ve - 60 || q_js > q_ve) keep = false;
                } else {
                    if (q_js > q_ve + 60 || q_je < q_ve) keep = false;
                }
                /* remove failed seq_align */
                if (!keep) it = align_list.erase(it);
                else ++it;
            }
            /* sort according to score */
            align_list.sort(s_CompareSeqAlignByScore);
        }

        /* which one to keep, D or J? */
        if (align_D.NotEmpty() && !align_D->IsEmpty() &&
            align_J.NotEmpty() && !align_J->IsEmpty()) {
            CSeq_align_set::Tdata & al_D = align_D->Set();
            CSeq_align_set::Tdata & al_J = align_J->Set();
            CSeq_align_set::Tdata::iterator it;
            bool keep_J = s_CompareSeqAlignByScore(*(al_J.begin()), *(al_D.begin()));
            if (keep_J) {
                it = al_D.begin();
                while (it != al_D.end()) {
                    if (s_DJNotCompatible(**it, **(al_J.begin()), q_ms)) {
                        it = al_D.erase(it);
                    } else ++it;
                }
            } else {
                it = al_J.begin();
                while (it != al_J.end()) {
                    if (s_DJNotCompatible(**(al_D.begin()), **it, q_ms)) {
                        it = al_J.erase(it);
                    } else ++it;
                }
            }
        }
                
        /* annotate D */    
        if (align_D.NotEmpty() && !align_D->IsEmpty()) {
            const CSeq_align & it = **(align_D->Get().begin());
            (*annot)->m_GeneInfo[2] = it.GetSeqStart(0);
            (*annot)->m_GeneInfo[3] = it.GetSeqStop(0)+1;
            (*annot)->m_TopGeneIds[1] = it.GetSeq_id(1).AsFastaString();
        }
            
        /* annotate J */    
        if (align_J.NotEmpty() && !align_J->IsEmpty()) {
            const CSeq_align & it = **(align_J->Get().begin());
            (*annot)->m_GeneInfo[4] = it.GetSeqStart(0);
            (*annot)->m_GeneInfo[5] = it.GetSeqStop(0)+1;
            (*annot)->m_TopGeneIds[2] = it.GetSeq_id(1).AsFastaString();
            string sid = it.GetSeq_id(1).AsFastaString();
            if (sid.substr(0, 4) == "lcl|") sid = sid.substr(4, sid.length());
            int frame_offset = m_AnnotationInfo.GetFrameOffset(sid);
            if (frame_offset >= 0) {
                int frame_adj = (it.GetSeqStart(1) + 3 - frame_offset) % 3;
                (*annot)->m_FrameInfo[1] = (q_ms) ?
                                           it.GetSeqStop(0)  + frame_adj 
                                         : it.GetSeqStart(0) - frame_adj;
            } 
        }
     
        /* next set of results */
        ++iq;
    }
};

// query chain type and domain is annotated by germline alignment
void CIgBlast::x_AnnotateDomain(CRef<CSearchResultSet>        &gl_results,
                                CRef<CSearchResultSet>        &dm_results, 
                                vector<CRef <CIgAnnotation> > &annots)
{
    CRef<CObjectManager> mgr = CObjectManager::GetInstance();
    CScope scope_q(*mgr), scope_s(*mgr);
    CRef<CSeqDB> db_V, db_domain;
    bool annotate_subject = false;
    if (m_IgOptions->m_Db[0]->IsBlastDb()) {
        string db_name_V = m_IgOptions->m_Db[0]->GetDatabaseName(); 
        string db_name_domain = m_IgOptions->m_Db[3]->GetDatabaseName(); 
        CSeqDB::ESeqType db_type = (m_IgOptions->m_IsProtein)? 
                                   CSeqDB::eProtein : CSeqDB::eNucleotide;
        db_V.Reset(new CSeqDB(db_name_V, db_type));
        if (db_name_V == db_name_domain) {
            db_domain.Reset(&(*db_V));
        } else {
            db_domain.Reset(new CSeqDB(db_name_domain, db_type));
        }
        annotate_subject = true;
    }

    int iq = 0;
    ITERATE(CSearchResultSet, result, *dm_results) {

        CIgAnnotation *annot = &*(annots[iq]);
        annot->m_ChainType.push_back("NON");  // Assuming non-ig sequence first

        if ((*result)->HasAlignments() && (*gl_results)[iq].HasAlignments()) {


            CConstRef<CSeq_align> master_align = 
                            (*gl_results)[iq].GetSeqAlign()->Get().front();
            CAlnMap q_map(master_align->GetSegs().GetDenseg());

            if (master_align->GetSeqStrand(0) == eNa_strand_minus) {
                annot->m_MinusStrand = true;
            }

            int q_ends[2], q_dir;

            if (annot->m_MinusStrand) {
                q_ends[1] = master_align->GetSeqStart(0);
                q_ends[0] = master_align->GetSeqStop(0);
                q_dir = -1;

            } else {
                q_ends[0] = master_align->GetSeqStart(0);
                q_ends[1] = master_align->GetSeqStop(0);
                q_dir = 1;
            }

            string sid = master_align->GetSeq_id(1).AsFastaString();
            if (sid.substr(0, 4) == "lcl|") sid = sid.substr(4, sid.length());
            int frame_offset = m_AnnotationInfo.GetFrameOffset(sid);
            if (frame_offset >= 0) {
                annot->m_FrameInfo[2] = frame_offset;
                int frame_adj = (master_align->GetSeqStop(1) + 3 - frame_offset) % 3;
                annot->m_FrameInfo[0] = q_ends[1] - q_dir * frame_adj;
            }

            const CSeq_align_set::Tdata & align_list = (*result)->GetSeqAlign()->Get();

            ITERATE(CSeq_align_set::Tdata, it, align_list) {

                string sid = (*it)->GetSeq_id(1).AsFastaString();
                if (sid.substr(0, 4) == "lcl|") sid = sid.substr(4, sid.length());
                annot->m_ChainType[0] = m_AnnotationInfo.GetDomainChainType(sid);

                int domain_info[10];

                if (m_AnnotationInfo.GetDomainInfo(sid, domain_info)) {


                    CAlnMap s_map((*it)->GetSegs().GetDenseg());
                    int s_start = (*it)->GetSeqStart(1);
                    int s_stop = (*it)->GetSeqStop(1);

                    CRef<CAlnMap> d_map;
                    int d_start = -1;
                    int d_stop = -1;

                    int start, stop;

                    if (annotate_subject) {
                        CRef<CBioseq> seq_q = db_domain->SeqidToBioseq((*it)->GetSeq_id(1));
                        CBioseq_Handle hdl_q = scope_q.AddBioseq(*seq_q);
                        CRef<CBioseq> seq_s = db_V->SeqidToBioseq(master_align->GetSeq_id(1));
                        CBioseq_Handle hdl_s = scope_s.AddBioseq(*seq_s);
                        CSeq_loc query, subject;
                        query.SetWhole();
                        query.SetId((*it)->GetSeq_id(1));
                        subject.SetWhole();
                        subject.SetId(master_align->GetSeq_id(1));
                        SSeqLoc q_loc(&query, &scope_q);
                        SSeqLoc s_loc(&subject, &scope_s);
                        CBl2Seq bl2seq(q_loc, s_loc, (m_IgOptions->m_IsProtein)? eBlastp: eBlastn);
                        const CSearchResults& result = (*(bl2seq.RunEx()))[0];
                        if (result.HasAlignments()) {
                            CConstRef<CSeq_align> subject_align = result.GetSeqAlign()->Get().front();
                            d_map.Reset(new CAlnMap(subject_align->GetSegs().GetDenseg()));
                            d_start = subject_align->GetSeqStart(0);
                            d_stop = subject_align->GetSeqStop(0);
                        }
                        scope_q.RemoveBioseq(hdl_q);
                        scope_s.RemoveBioseq(hdl_s);
                    }

                    for (int i =0; i<10; i+=2) {

                        start = domain_info[i] - 1;
                        stop = domain_info[i+1] - 1;

                        if (start <= d_stop && stop >= d_start) {
                            int start_copy = start;
                            int stop_copy = stop;
                            if (start_copy < d_start) start_copy = d_start;
                            if (stop_copy > d_stop) stop_copy = d_stop;
                            if (start_copy <= stop_copy) {
                                annot->m_DomainInfo_S[i] = 
                                   d_map->GetSeqPosFromSeqPos(1, 0, start_copy, IAlnExplorer::eForward);
                                annot->m_DomainInfo_S[i+1] = 
                                   d_map->GetSeqPosFromSeqPos(1, 0, stop_copy, IAlnExplorer::eBackwards);
                            }
                        }
                    
                        if (start > s_stop || stop < s_start) continue;

                        if (start < s_start) start = s_start;

                        if (stop > s_stop) stop = s_stop;

                        if (start > stop) continue;

                        start = s_map.GetSeqPosFromSeqPos(0, 1, start, IAlnExplorer::eForward);
                        stop = s_map.GetSeqPosFromSeqPos(0, 1, stop, IAlnExplorer::eBackwards);

                        if ((start - q_ends[1])*q_dir > 0 || (stop - q_ends[0])*q_dir < 0) continue;

                        if ((start - q_ends[0])*q_dir < 0) start = q_ends[0];

                        if ((stop - q_ends[1])*q_dir > 0) stop = q_ends[1];

                        if ((start - stop)*q_dir > 0) continue;

                        start = q_map.GetSeqPosFromSeqPos(1, 0, start, IAlnExplorer::eForward);
                        stop = q_map.GetSeqPosFromSeqPos(1, 0, stop, IAlnExplorer::eBackwards);

                        start = q_map.GetSeqPosFromSeqPos(0, 1, start);
                        stop = q_map.GetSeqPosFromSeqPos(0, 1, stop);
 
                        if ((start - stop)*q_dir > 0) continue;

                        annot->m_DomainInfo[i] = start;
                        annot->m_DomainInfo[i+1] = stop;
                    }

                    // any extra alignments after FWR3 are attributed to CDR3
                    start = annot->m_DomainInfo[9];

                    if (start >= 0 && (start - q_ends[1])*q_dir < 0) {
                        start = q_map.GetSeqPosFromSeqPos(1, 0, start+q_dir, IAlnExplorer::eForward);
                        start = q_map.GetSeqPosFromSeqPos(0, 1, start);
 
                        if ((start - q_ends[1])*q_dir > 0) continue;
                        annot->m_DomainInfo[10] = start;
                        annot->m_DomainInfo[11] = q_ends[1];
                    }

                    // extension of the first and last annotated domain (if any)
                    int i = 0;
                    while (i<10 && annot->m_DomainInfo[i] < 0) i+=2;
                    annot->m_DomainInfo[i] += (domain_info[i] - 1 -
                                       s_map.GetSeqPosFromSeqPos(1, 0, annot->m_DomainInfo[i],
                                                                 IAlnExplorer::eBackwards))*q_dir;
                    if (annot->m_DomainInfo[i] < 0) annot->m_DomainInfo[i] = 0;
                    i = 9;
                    while (i>0 && annot->m_DomainInfo[i] < 0) i-=2;
                    if (i >= 0) {
                        annot->m_DomainInfo[i] += (domain_info[i] - 1 -
                                                   s_map.GetSeqPosFromSeqPos(1, 0, annot->m_DomainInfo[i],
                                                                             IAlnExplorer::eForward))*q_dir;
                        if (annot->m_DomainInfo[i] < 0) annot->m_DomainInfo[i] = 0;
                        break;
                    }
                }
            }
        }
        ++iq;
    }
};

void CIgBlast::x_SetChainType(CRef<CSearchResultSet>  &results,
                              vector<CRef <CIgAnnotation> > &annots) 
{
    int iq = 0;
    ITERATE(CSearchResultSet, result, *results) {

        CIgAnnotation *annot = &*(annots[iq++]);

        if ((*result)->HasAlignments()) {

            const CSeq_align_set::Tdata & align_list = (*result)->GetSeqAlign()->Get();
            ITERATE(CSeq_align_set::Tdata, it, align_list) {
                string sid = (*it)->GetSeq_id(1).AsFastaString();
                if (sid.substr(0, 4) == "lcl|") sid = sid.substr(4, sid.length());
                annot->m_ChainType.push_back(m_AnnotationInfo.GetChainType(sid));
            }
        }
    }
};

void CIgBlast::s_SortResultsByEvalue(CRef<CSearchResultSet>& results)
{
    if (results->GetResultType() == eSequenceComparison) {
        //TODO convert bl2seq align_set to db align_set
    }
    ITERATE(CSearchResultSet, result, *results) {
        if ((*result)->HasAlignments()) {
            CRef<CSeq_align_set> align(const_cast<CSeq_align_set *>
                                   (&*((*result)->GetSeqAlign())));
            CSeq_align_set::Tdata & align_list = align->Set();
            align_list.sort(s_CompareSeqAlignByEvalue);
        }
    }
};

            // keep only the first num_alignments
void CIgBlast::s_AppendResults(CRef<CSearchResultSet> &results,
                               int                     num_aligns,
                               int                     gene,
                               CRef<CSearchResultSet> &final_results)
{
    bool  new_result = (final_results.Empty());
    if (new_result) {
        final_results.Reset(new CSearchResultSet());
    }

    int iq = 0;
    ITERATE(CSearchResultSet, result, *results) {

        CRef<CSeq_align_set> align;
        int actual_align = 0;

        if ((*result)->HasAlignments()) {
            align.Reset(const_cast<CSeq_align_set *>
                                   (&*((*result)->GetSeqAlign())));

            // keep only the first num_alignments
            if (num_aligns >= 0) {
                CSeq_align_set::Tdata & align_list = align->Set();
                if (align_list.size() > (CSeq_align_set::Tdata::size_type)num_aligns) {
                    CSeq_align_set::Tdata::iterator it = align_list.begin();
                    for (int i=0; i<num_aligns; ++i) ++it;
                    align_list.erase(it, align_list.end());
                    actual_align = num_aligns;
                } else {
                    actual_align = align_list.size();
                }
            }
        }

        TQueryMessages errmsg = (*result)->GetErrors();

        CIgBlastResults *ig_result;
        if (new_result) {
            CConstRef<CSeq_id> query = (*result)->GetSeqId();
            // TODO maybe we need the db ancillary instead?
            CRef<CBlastAncillaryData> ancillary = (*result)->GetAncillaryData();
            ig_result = new CIgBlastResults(query, align, errmsg, ancillary);
            CRef<CSearchResults> r(ig_result);
            final_results->push_back(r);
        } else {
            ig_result = dynamic_cast<CIgBlastResults *> (&(*final_results)[iq]);
            if (!align.Empty()) {
                CSeq_align_set::Tdata & ig_list = ig_result->SetSeqAlign()->Set();
                CSeq_align_set::Tdata & align_list = align->Set();

                if (gene < 0) {
                    // Remove duplicate seq_aligns
                    CSeq_align_set::Tdata::iterator it = align_list.begin();
                    while (it != align_list.end()) {
                        if (s_SeqAlignInSet(ig_list, *it)) it = align_list.erase(it);
                        else ++it;
                    }
                }

                if (!align_list.empty()) {
                    ig_list.insert(ig_list.end(), align_list.begin(), align_list.end());
                    ig_result->GetErrors().Combine(errmsg);
                }
            }
        }

        switch(gene) {
        case 0: ig_result->m_NumActualV = actual_align; break;
        case 1: ig_result->m_NumActualD = actual_align; break;
        case 2: ig_result->m_NumActualJ = actual_align; break;
        default: break;
        }
        ++iq;
    }
};

void CIgBlast::s_SetAnnotation(vector<CRef <CIgAnnotation> > &annots,
                               CRef<CSearchResultSet> &final_results)
{
    int iq = 0;
    ITERATE(CSearchResultSet, result, *final_results) {
        CIgBlastResults *ig_result = dynamic_cast<CIgBlastResults *>
                                     (const_cast<CSearchResults *>(&**result));
        CIgAnnotation *annot = &*(annots[iq++]);
        ig_result->SetIgAnnotation().Reset(annot);
    }
};

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
