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
#include <algo/blast/api/igblast.hpp>
#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/api/remote_blast.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <objtools/alnmgr/alnmap.hpp>


/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CIgAnnotationInfo::CIgAnnotationInfo(CConstRef<CIgBlastOptions> &ig_opt)
{
    // read domain info from pdm or ndm file
    const string suffix = (ig_opt->m_IsProtein) ? "_gl_V.p.dm." : "_gl_V.n.dm.";
    string fn = ig_opt->m_Origin + suffix + ig_opt->m_DomainSystem;
    CNcbiIfstream fs(fn.c_str(), IOS_BASE::in);

    if (!(! CFile(fn).Exists()) || fs.fail()) {
        char line[256];
        fs.getline(line, 256);
        int index = 0;
        while(!fs.eof()) {
            string l(line);
            vector<string> tokens;
            NStr::Tokenize(l, " \t\n\r", tokens, NStr::eMergeDelims);

            if (!tokens.empty() && tokens[0][0] != '#') {
                m_DomainIndex[tokens[0]] = index;
                for (int i=1; i<11; ++i) {
                    m_DomainData.push_back(NStr::StringToInt(tokens[i]));
                }
                index += 10;
            } 
            fs.getline(line, 256);
        }
    }
    fs.close();

    // read chain type info from ct files
    fn = ig_opt->m_Origin + "_gl.ct";
    fs.open(fn.c_str(), IOS_BASE::in);

    if (!(! CFile(fn).Exists()) || fs.fail()) {
        char line[256];
        fs.getline(line, 256);
        while(!fs.eof()) {
            string l(line);
            vector<string> tokens;
            NStr::Tokenize(l, " \t\n\r", tokens, NStr::eMergeDelims);

            if (!tokens.empty() && tokens[0][0] != '#') {
                m_ChainType[tokens[0]] = tokens[1];
            }
            fs.getline(line, 256);
        }
    }
    fs.close();

    // read frame info from nfm file
    if (ig_opt->m_IsProtein) return; 
    fn = ig_opt->m_Origin + "_gl_J.nfm";
    fs.open(fn.c_str(), IOS_BASE::in);

    if (!(! CFile(fn).Exists()) || fs.fail()) {
        char line[256];
        fs.getline(line, 256);
        while(!fs.eof()) {
            string l(line);
            vector<string> tokens;
            NStr::Tokenize(l, " \t\n\r", tokens, NStr::eMergeDelims);

            if (!tokens.empty() && tokens[0][0] != '#') {
                m_FrameOffset[tokens[0]] = NStr::StringToInt(tokens[1]);
            }
            fs.getline(line, 256);
        }
    }
    fs.close();
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
    x_SetupVSearch(qf, opts_hndl);
    CLocalBlast blast(qf, opts_hndl, m_IgOptions->m_Db[0]);
    results[0] = blast.Run();
    x_AnnotateV(results[0], annots);

    /*** search V for domain annotation */
    if (m_IgOptions->m_Db[3]->GetDatabaseName() 
        !=  m_IgOptions->m_Db[0]->GetDatabaseName()) {
        //x_SetupVSearch(qf, opts_hndl);
        CLocalBlast blast(qf, opts_hndl, m_IgOptions->m_Db[3]);
        results[3] = blast.Run();
        x_AnnotateDomain(results[0], results[3], annots);
    } else {
        x_AnnotateDomain(results[0], results[0], annots);
    }

    /*** search DJ germline */
    int num_genes =  (m_IgOptions->m_IsProtein) ? 1 : 3;
    if (num_genes > 1) {
        x_SetupDJSearch(annots, qf, opts_hndl);
        for (int gene = 1; gene < num_genes; ++gene) {
            CLocalBlast blast(qf, opts_hndl, m_IgOptions->m_Db[gene]);
            results[gene] = blast.Run();
        }
        x_AnnotateJ(results[2], annots);
        x_AnnotateD(results[1], annots);
    }

    /*** collect germline search results */
    for (int gene = 0; gene  < num_genes; ++gene) {
        s_AppendResults(results[gene], m_IgOptions->m_NumAlign[gene], final_results);
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
    s_AppendResults(result, -1, final_results);

    /*** set the chain type infor */
    x_SetChainType(final_results, annots);

    /*** attach annotation info back to the results */
    s_SetAnnotation(annots, final_results);

    return final_results;
};

void CIgBlast::x_SetupVSearch(CRef<IQueryFactory>       &qf,
                              CRef<CBlastOptionsHandle> &opts_hndl)
{
    if (m_IgOptions->m_IsProtein) {
        // TODO: no composition based.
    } else {
        CBlastOptions & opts = opts_hndl->SetOptions();
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
    opts.SetMismatchPenalty(-3);
    opts.SetWordSize(5);
    opts.SetGapOpeningCost(5);
    opts.SetGapExtensionCost(2);
    opts_hndl->SetEvalueThreshold(1000.0);
    opts_hndl->SetFilterString("F");
    opts_hndl->SetHitlistSize(10);

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
static bool s_CompareSeqAlign(const CRef<CSeq_align> &x, const CRef<CSeq_align> &y)
{
    int sx, sy;
    x->GetNamedScore(CSeq_align::eScore_Score, sx);
    y->GetNamedScore(CSeq_align::eScore_Score, sy);
    if (sx != sy) return (sx > sy);
    x->GetNamedScore(CSeq_align::eScore_IdentityCount, sx);
    y->GetNamedScore(CSeq_align::eScore_IdentityCount, sy);
    return (sx <= sy);
};

void CIgBlast::x_AnnotateJ(CRef<CSearchResultSet>        &results,
                           vector<CRef <CIgAnnotation> > &annots)
{
    int iq = 0;
    ITERATE(CSearchResultSet, result, *results) {

        CIgAnnotation *annot = &*(annots[iq++]);
        string qct = annot->m_ChainType[0];

        if ((*result)->HasAlignments()) {

            CRef<CSeq_align_set> align(const_cast<CSeq_align_set *>
                                           (&*((*result)->GetSeqAlign())));
            CSeq_align_set::Tdata & align_list = align->Set();
            align_list.sort(s_CompareSeqAlign);

            CSeq_align_set::Tdata::iterator it = align_list.begin();

            int V_end = (annot->m_MinusStrand) ? 
                         annot->m_GeneInfo[0] : annot->m_GeneInfo[1] - 1;
            ENa_strand strand = (annot->m_MinusStrand) ? eNa_strand_minus : eNa_strand_plus;

            bool annotated = false;
            while (it != align_list.end()) {
                
                bool keep = true;

                if (qct != "N/A") {
                    string sid = (*it)->GetSeq_id(1).AsFastaString();
                    if (sid.substr(0, 4) == "lcl|") sid = sid.substr(4, sid.length());
                    string sct = m_AnnotationInfo.GetChainType(sid);
                    if (sct != "N/A") {
                        if (qct == "VH" && sct != "JH" ||
                            qct != "VH" && sct == "JH") keep = false;
                    }
                }

                if (keep) {
                    if (annot->m_MinusStrand) {
                        if ((*it)->GetSeqStrand(0) != strand                ||
                            (int)(*it)->GetSeqStop(0)   <  V_end - 60       ||
                            (int)(*it)->GetSeqStart(0)  >  V_end            ||
                            (int)(*it)->GetSeqStart(1)  >  32) keep = false;
                    } else {
                        if ((*it)->GetSeqStrand(0) != strand                ||
                            (int)(*it)->GetSeqStart(0)  >  V_end + 60       ||
                            (int)(*it)->GetSeqStop(0)   <  V_end            ||
                            (int)(*it)->GetSeqStart(1)  >  32) keep = false;
                    }
                }
    
                if (!keep) {
                     it = align_list.erase(it);
                } else {
                     if (!annotated) {
                         annot->m_GeneInfo[4] = (*it)->GetSeqStart(0);
                         annot->m_GeneInfo[5] = (*it)->GetSeqStop(0)+1;

                         string sid = (*it)->GetSeq_id(1).AsFastaString();
                         if (sid.substr(0, 4) == "lcl|") sid = sid.substr(4, sid.length());
                         int frame_offset = m_AnnotationInfo.GetFrameOffset(sid);
                         if (frame_offset >= 0) {
                             int frame_adj = ((*it)->GetSeqStart(1) - frame_offset) % 3 + 3;
                             annot->m_FrameInfo[1] = (annot->m_MinusStrand) ?
                                                     (*it)->GetSeqStop(0)  + frame_adj 
                                                   : (*it)->GetSeqStart(0) - frame_adj;
                         } 
                         annotated = true;
                     }
                     ++it;
                }
            }
        }
    }
};

void CIgBlast::x_AnnotateD(CRef<CSearchResultSet>        &results,
                           vector<CRef <CIgAnnotation> > &annots)
{
    int iq = 0;
    ITERATE(CSearchResultSet, result, *results) {

        CIgAnnotation *annot = &*(annots[iq++]);
        string qct = annot->m_ChainType[0];
 
        if ((*result)->HasAlignments()) {

            CRef<CSeq_align_set> align(const_cast<CSeq_align_set *>
                                           (&*((*result)->GetSeqAlign())));
            CSeq_align_set::Tdata & align_list = align->Set();
            align_list.sort(s_CompareSeqAlign);

            ENa_strand strand = (annot->m_MinusStrand) ? eNa_strand_minus : eNa_strand_plus;
            bool strand_found = false;
            ITERATE(CSeq_align_set::Tdata, itt, align_list) {
                if ((*itt)->GetSeqStrand(0) == strand) {
                    strand_found = true;
                    break;
                }
            }
            CSeq_align_set::Tdata::iterator it = align_list.begin();
            if (strand_found) {
                while (it != align_list.end()) {
                    if ((*it)->GetSeqStrand(0) != strand) {
                        it = align_list.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        
            int V_end = (annot->m_MinusStrand) ? 
                         annot->m_GeneInfo[0] : annot->m_GeneInfo[1] - 1;
            int J_begin = annot->m_GeneInfo[4];
            int J_end = annot->m_GeneInfo[5] - 1;

            bool annotated = false;
            it = align_list.begin();
            while (it != align_list.end()) {
                
                bool keep = (qct=="VH" || qct=="N/A");

                if (keep) {
                    if (annot->m_MinusStrand) {
                        if ((int)(*it)->GetSeqStop(0)   <  V_end - 30 ||
                            (int)(*it)->GetSeqStart(0)  >  V_end) keep = false;
                        if (J_begin>=0 &&
                            ((int)(*it)->GetSeqStart(0) <  J_begin + 3 ||
                             (int)(*it)->GetSeqStop(0)  <  J_end + 3)) keep = false;
                    } else {
                        if ((int)(*it)->GetSeqStart(0)  >  V_end + 30 ||
                            (int)(*it)->GetSeqStop(0)   <  V_end) keep = false;
                        if (J_begin>=0 &&
                            ((int)(*it)->GetSeqStart(0) >  J_begin - 3 ||
                             (int)(*it)->GetSeqStop(0)  >  J_end - 3)) keep = false; 
                    }
                }

                if (!keep) {
                     it = align_list.erase(it);
                } else {
                     if (!annotated) {
                         annot->m_GeneInfo[2] = (*it)->GetSeqStart(0);
                         annot->m_GeneInfo[3] = (*it)->GetSeqStop(0)+1;
                         annotated = true;
                     }
                     ++it;
                }
            }
        }
    }
};

// query chain type and domain is annotated by germline alignment
void CIgBlast::x_AnnotateDomain(CRef<CSearchResultSet>        &gl_results,
                                CRef<CSearchResultSet>        &dm_results, 
                                vector<CRef <CIgAnnotation> > &annots)
{
    int iq = 0;
    ITERATE(CSearchResultSet, result, *dm_results) {

        if ((*result)->HasAlignments() && (*gl_results)[iq].HasAlignments()) {

            CIgAnnotation *annot = &*(annots[iq]);

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

            annot->m_FrameInfo[0] = q_ends[1] - q_dir * (master_align->GetSeqStop(1) % 3);

            const CSeq_align_set::Tdata & align_list = (*result)->GetSeqAlign()->Get();

            ITERATE(CSeq_align_set::Tdata, it, align_list) {

                string sid = (*it)->GetSeq_id(1).AsFastaString();
                if (sid.substr(0, 4) == "lcl|") sid = sid.substr(4, sid.length());
                annot->m_ChainType.push_back(m_AnnotationInfo.GetChainType(sid));

                int domain_info[10];

                if (m_AnnotationInfo.GetDomainInfo(sid, domain_info)) {

                    CAlnMap s_map((*it)->GetSegs().GetDenseg());
                    int s_start = (*it)->GetSeqStart(1);
                    int s_stop = (*it)->GetSeqStop(1);
                    int start, stop;

                    for (int i =0; i<10; i+=2) {

                        start = domain_info[i] - 1;
                        stop = domain_info[i+1] - 1;
                    
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

                    // the last annotated domain must be extended if necessary
                    int i = 9;
                    while (i>0 && annot->m_DomainInfo[i] < 0) i-=2;
                    if (i>0) {
                        int stop = s_map.GetSeqPosFromSeqPos(1, 0, annot->m_DomainInfo[i],
                                                             IAlnExplorer::eForward);
                        annot->m_DomainInfo[12] = domain_info[i] -1 - stop;
                    }

                    break;
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

void CIgBlast::s_AppendResults(CRef<CSearchResultSet> &results,
                               int                     num_aligns,
                               CRef<CSearchResultSet> &final_results)
{
    bool  new_result = (final_results.Empty());
    if (new_result) {
        final_results.Reset(new CSearchResultSet());
    }

    int iq = 0;
    ITERATE(CSearchResultSet, result, *results) {

        CRef<CSeq_align_set> align;

        if ((*result)->HasAlignments()) {

            // keep only the first num_alignments
            align.Reset(const_cast<CSeq_align_set *>
                                   (&*((*result)->GetSeqAlign())));
            if (num_aligns >= 0) {
                CSeq_align_set::Tdata & align_list = align->Set();
                if (align_list.size() > (CSeq_align_set::Tdata::size_type)num_aligns) {
                    CSeq_align_set::Tdata::iterator it = align_list.begin();
                    for (int i=0; i<num_aligns; ++i) ++it;
                    align_list.erase(it, align_list.end());
                }
            }
        }

        TQueryMessages errmsg = (*result)->GetErrors();

        if (new_result) {
            CConstRef<CSeq_id> query = (*result)->GetSeqId();
            // TODO maybe we need the db ancillary instead?
            CRef<CBlastAncillaryData> ancillary = (*result)->GetAncillaryData();
            CRef<CSearchResults> ig_result(
                  new CIgBlastResults(query, align, errmsg, ancillary));
            final_results->push_back(ig_result);
        } else if (!align.Empty()) {
            CIgBlastResults *ig_result = dynamic_cast<CIgBlastResults *>
                                         (&(*final_results)[iq]);
            CSeq_align_set::Tdata & ig_list = ig_result->SetSeqAlign()->Set();
            // Remove duplicates first
            CSeq_align_set::Tdata & align_list = align->Set();
            CSeq_align_set::Tdata::iterator it = align_list.begin();
            while (it != align_list.end() && s_SeqAlignInSet(ig_list, *it)) ++it;
            if (it != align_list.begin())  align_list.erase(align_list.begin(), it);

            if (!align_list.empty()) {
                ig_list.insert(ig_list.end(), align_list.begin(), align_list.end());
                ig_result->GetErrors().Combine(errmsg);
            }
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
