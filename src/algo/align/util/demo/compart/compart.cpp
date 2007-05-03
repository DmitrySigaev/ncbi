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
 * Author:  Yuri Kapustin
 *
 * File Description:  Compartmentization demo
 *                   
*/

#include <ncbi_pch.hpp>

#include "compart.hpp"

#include <algo/align/util/compartment_finder.hpp>

#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/util/seq_loc_util.hpp>

#include <math.h>

namespace {
    const size_t g_MinIntronLength (25);
    const size_t g_MinExonLength   (10);
}

BEGIN_NCBI_SCOPE


void CCompartApp::Init()
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideVersion);

    auto_ptr<CArgDescriptions> argdescr(new CArgDescriptions);
    argdescr->SetUsageContext(GetArguments().GetProgramName(),
                              "Compart v.1.0.0. Input must be sorted "
                              "by query and subject (on Unix: sort -k 1,1 -k 2,2).");

    argdescr->AddDefaultKey("penalty", "penalty", 
                            "Per-compartment penalty",
                            CArgDescriptions::eDouble, "0.4");
    
    argdescr->AddDefaultKey("min_idty", "min_idty", 
                            "Minimal overall identity",
                            CArgDescriptions::eDouble, "0.1");
    
    argdescr->AddDefaultKey("min_singleton_idty", "min_singleton_idty", 
                            "Minimal identity for singleton compartments",
                            CArgDescriptions::eDouble, "0.1");
    
    argdescr->AddDefaultKey("N", "N", 
                            "Max number of compartments per query",
                            CArgDescriptions::eInteger, "5");
    
    argdescr->AddOptionalKey("seqlens", "seqlens", 
                             "Two-column file with sequence IDs and their lengths. "
                             "If no supplied, the program will attempt fetching "
                             "sequence lengths from GenBank.",
                             CArgDescriptions::eInputFile);

    CArgAllow* constrain01 = new CArgAllow_Doubles(0.0, 1.0);
    argdescr->SetConstraint("penalty", constrain01);
    argdescr->SetConstraint("min_idty", constrain01);
    argdescr->SetConstraint("min_singleton_idty", constrain01);

    {{
        USING_SCOPE(objects);
        CRef<CObjectManager> om = CObjectManager::GetInstance();
        CGBDataLoader::RegisterInObjectManager(*om);
        m_Scope.Reset(new CScope (*om));
        m_Scope->AddDefaults();
    }}
    SetupArgDescriptions(argdescr.release());
}


void CCompartApp::x_ReadSeqLens(CNcbiIstream& istr)
{
    m_id2len.clear();

    while(istr) {
        string id;
        size_t len = 0;
        istr >> id >> len;
        if(len != 0) {
            m_id2len[id] = len;
        }
    }
}


size_t CCompartApp::x_GetSeqLength(const string& id)
{
    TStrIdToLen::const_iterator ie = m_id2len.end(), im = m_id2len.find(id);
    if(im != ie) {
        return im->second;
    }
    else {
        USING_SCOPE(objects);

        CRef<CSeq_id> seqid;
        try {seqid.Reset(new CSeq_id(id));}
        catch(CSeqIdException& e) {
            return 0;
        }

        const size_t len = sequence::GetLength(*seqid, m_Scope.GetNonNullPointer());
        m_id2len[id] = len;
        return len;
    }
}


int CCompartApp::Run()
{   
    const CArgs& args = GetArgs();

    if(args["seqlens"]) {
        x_ReadSeqLens(args["seqlens"].AsInputFile());
    }

    m_penalty                  = args["penalty"].AsDouble();
    m_min_idty                 = args["min_idty"].AsDouble();
    m_min_singleton_idty       = args["min_singleton_idty"].AsDouble();

    m_MaxCompsPerQuery         = args["N"].AsInteger();

    m_CompartmentsPermanent.resize(0);

    THitRefs hitrefs;

    typedef map<string,string> TIdToId;
    TIdToId id2id;

    char line [1024];
    string query0, subj0;
    while(cin) {

        cin.getline(line, sizeof line, '\n');
        string s (NStr::TruncateSpaces(line));
        if(s.size()) {

            THitRef hit (new THit(s.c_str()));

            const string query (hit->GetQueryId()->GetSeqIdString(true));
            const string subj  (hit->GetSubjId()->GetSeqIdString(true));
            
            if(query0.size() == 0 || subj0.size() == 0) {
                query0 = query;
                subj0 = subj;
                id2id[query0] = subj0;
            }
            else {

                if(query != query0 || subj != subj0) {

                    int rv = x_ProcessPair(query0, hitrefs);
                    if(rv != 0) return rv;

                    if(query != query0) {
                        x_RankAndStore();
                    }

                    query0 = query;
                    subj0 = subj;
                    hitrefs.clear();

                    TIdToId::const_iterator im = id2id.find(query0);
                    if(im == id2id.end() || im->second != subj0) {
                        id2id[query0] = subj0;
                    }
                    else {
                        cerr << "Input hit stream not properly ordered" << endl;
                        return 2;
                    }
                }
            }

            hitrefs.push_back(hit);
        }
    }

    if(hitrefs.size()) {
        int rv = x_ProcessPair(query0, hitrefs);
        if(rv != 0) return rv;
        x_RankAndStore();
    }

    stable_sort(m_CompartmentsPermanent.begin(), m_CompartmentsPermanent.end());

    ITERATE(TCompartRefs, ii, m_CompartmentsPermanent) {
        cout << **ii << endl;
    }

    return 0;
}


int CCompartApp::x_ProcessPair(const string& query0, THitRefs& hitrefs)
{

    const size_t qlen (x_GetSeqLength(query0));

    if(qlen == 0) {
        cerr << "Cannot retrieve sequence lengths for: " 
             << query0 << endl;
        return 1;
    }

    const size_t penalty_bps (size_t(m_penalty * qlen + 0.5));
    const size_t min_matches (size_t(m_min_idty * qlen + 0.5));
    const size_t min_singleton_matches (size_t(m_min_singleton_idty * qlen + 0.5));

    CCompartmentAccessor<THit> ca (hitrefs.begin(), hitrefs.end(),
                                   penalty_bps,
                                   min_matches,
                                   min_singleton_matches,
                                   true);

    THitRefs comp;
    for(bool b0 (ca.GetFirst(comp)); b0 ; b0 = ca.GetNext(comp)) {

        TCompartRef cr (new CCompartment (comp, qlen));
        m_Compartments.push_back(cr);
    }

    return 0;
}


bool PCompartmentRanker(const CCompartApp::TCompartRef& lhs,
                        const CCompartApp::TCompartRef& rhs)
{
    //#define PCOMPARTMENT_RANKER_M1

#ifdef PCOMPARTMENT_RANKER_M1

    const size_t exons_lhs (lhs->GetExonCount());
    const size_t exons_rhs (rhs->GetExonCount());
    if(exons_lhs == exons_rhs) {
        return lhs->GetMatchCount() > rhs->GetMatchCount();
    }
    else {
        return exons_lhs > exons_rhs;
    }

#else

    const size_t idtybin_lhs (lhs->GetIdentityBin());
    const size_t idtybin_rhs (rhs->GetIdentityBin());
    if(idtybin_lhs == idtybin_rhs) {
        const size_t exons_lhs (lhs->GetExonCount());
        const size_t exons_rhs (rhs->GetExonCount());
        if(exons_lhs == exons_rhs) {
            return lhs->GetMatchCount() > rhs->GetMatchCount();
        }
        else {
            return exons_lhs > exons_rhs;
        }
    }
    else {
        return idtybin_lhs > idtybin_rhs;
    }
#endif

#undef PCOMPARTMENT_RANKER_M1
}


void CCompartApp::x_RankAndStore(void)
{
    const size_t cdim (m_Compartments.size());
    if(cdim == 0) {
        return;
    }

    if(cdim > m_MaxCompsPerQuery) {
        stable_sort(m_Compartments.begin(), m_Compartments.end(), PCompartmentRanker);
    }

    m_Compartments.resize(min(cdim, m_MaxCompsPerQuery));

    copy(m_Compartments.begin(), m_Compartments.end(),
         back_inserter(m_CompartmentsPermanent));
    
    m_Compartments.resize(0);
}


void CCompartApp::Exit()
{
    return;
}

 
CCompartApp::CCompartment::TRange CCompartApp::CCompartment::GetSpan(void) const
{
    if(m_HitRefs.size() == 0) {
        NCBI_THROW(CException, eUnknown, "Span requested for empty compartment");
    }
    THit::TCoord a (m_HitRefs.front()->GetSubjStart()),
        b (m_HitRefs.back()->GetSubjStop());
    if(a > b) {
        THit::TCoord c (a);
        a = b;
        b = c;
    }

    return CCompartApp::CCompartment::TRange(a, b);
}

CCompartApp::CCompartment::CCompartment(const THitRefs& hitrefs, size_t length):
    m_SeqLength(length), m_IdentityBin(0), m_ExonCount(0), m_MatchCount(0)
{
    if(hitrefs.size() == 0) {
        NCBI_THROW(CException, eUnknown,
                   "Cannot init compartment with empty hit list");
    }

    for(THitRefs::const_reverse_iterator ii(hitrefs.rbegin()), ie(hitrefs.rend());
        ii != ie; x_AddHit(*ii++));

    x_EvalExons();
}


void CCompartApp::CCompartment::x_AddHit(const THitRef& hitref)
{
    if(m_HitRefs.size() == 0) {
        m_HitRefs.push_back(hitref);
    }
    else {

        const THitRef& hb (m_HitRefs.back());
        const bool cs (hb->GetSubjStrand());
        if(cs != hitref->GetSubjStrand()) {
            NCBI_THROW(CException, eUnknown, "Hit being added has strand "
                       "different from that of the compartment.");
        }

        m_HitRefs.push_back(hitref);
    }
}


bool CCompartApp::CCompartment::GetStrand(void) const
{
    if(m_HitRefs.size()) {
        return m_HitRefs.front()->GetSubjStrand();
    }
    NCBI_THROW(CException, eUnknown, "Cannot determine compartment strand");
}


// compares by subject, query, strand, then order on the subject
bool CCompartApp::CCompartment::operator < (const CCompartApp::CCompartment& rhs)
const
{
    const THit::TId& subjid_lhs (m_HitRefs.front()->GetSubjId());
    const THit::TId& subjid_rhs (rhs.m_HitRefs.front()->GetSubjId());
    const int co (subjid_lhs->CompareOrdered(*subjid_rhs));
    if(co == 0) {

        const THit::TId& queryid_lhs (m_HitRefs.front()->GetQueryId());
        const THit::TId& queryid_rhs (rhs.m_HitRefs.front()->GetQueryId());
        const int co (queryid_lhs->CompareOrdered(*queryid_rhs));

        if(co == 0) {

            const bool strand_lhs (GetStrand());
            const bool strand_rhs (rhs.GetStrand());
            if(strand_lhs == strand_rhs) {
                if(strand_lhs) {
                    return GetSpan().second <= rhs.GetSpan().first;
                }
                else {
                    return GetSpan().first >= rhs.GetSpan().second;
                }
            }
            else {
                return strand_lhs < strand_rhs;
            }
        }
        else {
            return co < 0;
        }
    }
    else {
        return co < 0;
    }
}


bool operator < (const CCompartApp::TCompartRef& lhs,
                 const CCompartApp::TCompartRef& rhs)
{
    return *lhs < *rhs;
}


// Evaluate all variables used in comaprtment ranking. These are:
// - m_IdentityBin
// - m_ExonCount
// - m_MatchCount
void CCompartApp::CCompartment::x_EvalExons(void)
{
    size_t exons (1);
    THitRef& h (m_HitRefs.front());
    double matches ( h->GetLength() * h->GetIdentity() );

    if(m_HitRefs.size() > 1) {

        if(GetStrand()) {

            THitRef prev;
            ITERATE(THitRefs, ii, m_HitRefs) {

                const THitRef& h (*ii);
                if(prev.NotEmpty()) {

                    const THit::TCoord q0 (prev->GetQueryStop());
                    if(q0 + g_MinExonLength <= h->GetQueryStop()) {

                        const THit::TCoord s0 (h->GetSubjStart() 
                                               - (h->GetQueryStart() - q0));
                        if(prev->GetSubjStop() + g_MinIntronLength <= s0) {
                            ++exons;
                        }
                        const THit::TCoord q0max (max(q0,h->GetQueryStart()));
                        matches += (h->GetQueryStop() - q0max) * h->GetIdentity();
                    }
                }
                prev = h;
            }
        }
        else {

            THitRef prev;
            ITERATE(THitRefs, ii, m_HitRefs) {

                const THitRef& h (*ii);
                if(prev.NotEmpty()) {

                    const THit::TCoord q0 (prev->GetQueryStop());
                    if(q0 + g_MinExonLength <= h->GetQueryStop()) {

                        const THit::TCoord s0 (h->GetSubjStart() 
                                               + h->GetQueryStart() - q0);
                        if(s0 + g_MinIntronLength <= prev->GetSubjStop()) {
                            ++exons;
                        }
                        const THit::TCoord q0max (max(q0,h->GetQueryStart()));
                        matches += (h->GetQueryStop() - q0max) * h->GetIdentity();
                    }
                }
                prev = h;
            }
        }
    }
    
    m_ExonCount = exons;
    m_MatchCount = size_t(round(matches));
    m_IdentityBin = size_t(floor(double(m_MatchCount) / m_SeqLength / 0.1));
}


ostream& operator << (ostream& ostr, const CCompartApp::CCompartment& rhs)
{
    ITERATE(CCompartApp::THitRefs, ii, rhs.m_HitRefs) {
        ostr << **ii << endl;
    }
    return ostr;
}


END_NCBI_SCOPE


USING_NCBI_SCOPE;

int main(int argc, const char* argv[]) 
{
    return CCompartApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
