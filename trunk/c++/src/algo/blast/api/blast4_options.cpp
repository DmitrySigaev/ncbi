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
* Author:  Kevin Bealer
*
* File Description:
*   Queueing and Polling code for Blast4 API
*
* ===========================================================================
*/

#include <unistd.h>

#include <corelib/ncbi_system.hpp>
#include <algo/blast/api/blast4_options.hpp>

#include <objects/blast/blastclient.hpp>
#include <objects/blast/Blast4_request.hpp>
#include <objects/blast/Blast4_request_body.hpp>
#include <objects/blast/Blast4_error.hpp>
#include <objects/blast/Blas_get_searc_resul_reque.hpp>
#include <objects/blast/Blast4_error_code.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

// Static functions
template <class T>
void
s_Output(CNcbiOstream & os, CRef<T> t)
{
    auto_ptr<CObjectOStream> x(CObjectOStream::Open(eSerial_AsnText, os));
    *x << *t;
    os.flush();
}

static CRef<CBlast4_reply> s_SendRequest(CRef<CBlast4_request_body> body, bool echo = true)
{
    // Create the request; optionally echo it
    
    CRef<CBlast4_request> request(new CBlast4_request);
    request->SetBody(*body);
    
    if (echo) {
        s_Output(NcbiCout, request);
    }
    
    // submit to server, get reply; optionally echo it
    
    CRef<CBlast4_reply> reply(new CBlast4_reply);
    
    try {
        //throw some_kind_of_nothing();
        CBlast4Client().Ask(*request, *reply);
    }
    catch(CEofException & e) {
        ERR_POST(Error << "Unexpected EOF when contacting netblast server"
                 " - unable to complete request.");
#if defined(NCBI_OS_UNIX)
        // Use _exit() to avoid coredump.
        _exit(-1);
#else
        exit(-1);
#endif
    }
    
    if (echo) {
        s_Output(NcbiCout, reply);
    }
    
    return reply;
}

static CRef<CBlast4_reply>
s_GetSearchResults(const string & RID, bool echo = true)
{
    CRef<CBlast4_get_search_results_request>
        gsrr(new CBlast4_get_search_results_request);
    
    gsrr->SetRequest_id(RID);
    
    CRef<CBlast4_request_body> body(new CBlast4_request_body);
    body->SetGet_search_results(*gsrr);
    
    return s_SendRequest(body, echo);
}

typedef list< CRef<CBlast4_error> > TErrorList;

static bool
s_SearchPending(CRef<CBlast4_reply> reply)
{
    const list< CRef<CBlast4_error> > & errors = reply->GetErrors();
    
    TErrorList::const_iterator i;
    
    for(i = errors.begin(); i != errors.end(); i++) {
        if ((*i)->GetCode() == eBlast4_error_code_search_pending) {
            return true;
        }
    }
    return false;
}



// CBlast4Option methods


// Pre:  start, wait, or done
// Post: failed or done

// Returns: true if done

bool CBlast4Options::SubmitSync(void)
{
    // EFailed: no work to do, already an error.
    // EDone:   already done, just return.
    
    switch(x_GetState()) {
    case EStart:
        x_SubmitSearch();

    case EWait:
        x_PollUntilDone();
        break;
    }
    
    return (x_GetState() == EDone);
}



// Pre:  start
// Post: failed, wait or done

// Returns: true if no error so far

bool CBlast4Options::Submit(void)
{
    switch(x_GetState()) {
    case EStart:
        x_SubmitSearch();
    }
    
    return m_Err.empty();
}

// Pre:  start, wait or done
// Post: wait or done

// Returns: true if done

bool CBlast4Options::CheckDone(void)
{
    switch(x_GetState()) {
    case EFailed:
    case EDone:
        break;
        
    case EStart:
        Submit();
        break;
        
    case EWait:
        x_CheckResults();
    }
    
    return (x_GetState() == EDone);
}

CRef<objects::CSeq_align_set> CBlast4Options::GetAlignments(void)
{
    CRef<objects::CSeq_align_set> rv;
    
    if (! SubmitSync()) {
        return rv;
    }
    
    if (m_Reply.NotEmpty() &&
        m_Reply->CanGetBody() &&
        m_Reply->GetBody().IsGet_search_results()) {
            
        objects::CBlast4_get_search_results_reply & gsr =
            m_Reply->SetBody().SetGet_search_results();
            
        if (gsr.CanGetAlignments()) {
            rv = & gsr.SetAlignments();
        }
    }
        
    return rv;
}

CRef<objects::CBlast4_phi_alignments> CBlast4Options::GetPhiAlignments(void)
{
    CRef<objects::CBlast4_phi_alignments> rv;
        
    if (! SubmitSync()) {
        return rv;
    }
    
    if (m_Reply.NotEmpty() &&
        m_Reply->CanGetBody() &&
        m_Reply->GetBody().IsGet_search_results()) {
            
        objects::CBlast4_get_search_results_reply & gsr =
            m_Reply->SetBody().SetGet_search_results();
            
        if (gsr.CanGetPhi_alignments()) {
            rv = & gsr.SetPhi_alignments();
        }
    }
        
    return rv;
}
    
CRef<objects::CBlast4_mask> CBlast4Options::GetMask(void)
{
    CRef<objects::CBlast4_mask> rv;
        
    if (! SubmitSync()) {
        return rv;
    }
    
    if (m_Reply.NotEmpty() &&
        m_Reply->CanGetBody() &&
        m_Reply->GetBody().IsGet_search_results()) {
            
        objects::CBlast4_get_search_results_reply & gsr =
            m_Reply->SetBody().SetGet_search_results();
            
        if (gsr.CanGetMask()) {
            rv = & gsr.SetMask();
        }
    }
        
    return rv;
}
    
list< CRef<objects::CBlast4_ka_block > > CBlast4Options::GetKABlocks(void)
{ 
    list< CRef<objects::CBlast4_ka_block > > rv;
        
    if (! SubmitSync()) {
        return rv;
    }
    
    if (m_Reply.NotEmpty() &&
        m_Reply->CanGetBody() &&
        m_Reply->GetBody().IsGet_search_results()) {
        objects::CBlast4_get_search_results_reply & gsr =
            m_Reply->SetBody().SetGet_search_results();
            
        if (gsr.CanGetKa_blocks()) {
            rv = gsr.SetKa_blocks();
        }
    }
        
    return rv;
}
    
list< string > CBlast4Options::GetSearchStats(void)
{ 
    list< string > rv;
        
    if (! SubmitSync()) {
        return rv;
    }
    
    if (m_Reply.NotEmpty() &&
        m_Reply->CanGetBody() &&
        m_Reply->GetBody().IsGet_search_results()) {
        objects::CBlast4_get_search_results_reply & gsr =
            m_Reply->SetBody().SetGet_search_results();
            
        if (gsr.CanGetSearch_stats()) {
            rv = gsr.SetSearch_stats();
        }
    }
        
    return rv;
}


// Internal CBlast4Options methods

int CBlast4Options::x_GetState(void)
{
    // CBlast4Option states:
    
    // 0. start  (no rid, no errors)
    // 1. failed (errors)
    // 2. wait   (has rid, no errors, still pending)
    // 3. done   (has rid, no errors, not pending)
    
    int rv = 0;
    
    if (! m_Err.empty()) {
        rv = EFailed;
    } else if (m_RID.empty()) {
        rv = EStart;
    } else if (m_Pending) {
        rv = EWait;
    } else {
        rv = EDone;
    }
    
    return rv;
}

void CBlast4Options::x_SubmitSearch(void)
{
    CRef<CBlast4_request_body> body(new CBlast4_request_body);
    body->SetQueue_search(*m_Qsr);
    
    CRef<CBlast4_reply> reply = s_SendRequest(body, m_Verbose);
    
    if (reply->CanGetBody()  &&
        reply->GetBody().GetQueue_search().CanGetRequest_id()) {
        
        m_RID = reply->GetBody().GetQueue_search().GetRequest_id();
    }
    
    if (reply->CanGetErrors()) {
        const CBlast4_reply::TErrors & errs = reply->GetErrors();
        
        CBlast4_reply::TErrors::const_iterator i;
        
        for (i = errs.begin(); i != errs.end(); i++) {
            if ((*i)->GetMessage().size()) {
                if (m_Err.size()) {
                    m_Err += "\n";
                }
                m_Err += (*i)->GetMessage();
            }
        }
    }
    
    if (m_Err.empty()) {
        m_Pending = true;
    }
}

void CBlast4Options::x_CheckResults(void)
{
    if (!m_Err.empty()) {
        m_Pending = false;
    }
    
    if (! m_Pending)
        return;
    
    CRef<CBlast4_reply> r;
    
    try {
        r = s_GetSearchResults(m_RID, m_Verbose);
        m_Pending = s_SearchPending(r);
    }
    catch(CEofException &) {
    }
    
    if (! m_Pending) {
        if (r->CanGetBody() &&
            r->GetBody().IsGet_search_results()) {
            m_Reply = r;
        } else {
            m_Err = "Results were not a get-search-results reply";
        }
    }
}

void CBlast4Options::x_PollUntilDone(void)
{
    if (m_Verbose)
        cout << "polling " << 0 << endl;
    
    // Configuration - internal for now
    
    double start_sec = 10.0;
    double increment = 1.30;
    double max_sleep = 300.0;
    double max_time  = 3600.0 * 3.5; // 3.5 hours is approx 50 iterations
    
    if (m_Verbose)
        cout << "polling " << start_sec << "/" << increment << "/" << max_sleep << "/" << max_time << "/" << endl;
    
    // End config
    
    double sleep_next = start_sec;
    double sleep_totl = 0.0;
    
    if (m_Verbose)
        cout << "line " << __LINE__ << " sleep next " << sleep_next << " sleep totl " << sleep_totl << endl;
    
    while (m_Pending && (sleep_totl < max_time)) {
        if (m_Verbose)
            cout << " about to sleep " << sleep_next << endl;
        
        SleepSec(int(sleep_next));
        sleep_totl += sleep_next;
        
        if (m_Verbose)
            cout << " done, total = " << sleep_totl << endl;
        
        if (sleep_next < max_sleep) {
            sleep_next *= increment;
            if (sleep_next > max_sleep) {
                sleep_next = max_sleep;
            }
        }
        
        if (m_Verbose)
            cout << " next sleep time = " << sleep_next << endl;
        
        x_CheckResults();
    }
}

void CBlast4Options::x_Init(CBlastOptionsHandle * algo_opts,
                            const char          * program,
                            const char          * service)
{
    m_Verbose   = false;
    m_Pending   = false;
    
    m_Qsr.Reset(new objects::CBlast4_queue_search_request);
    m_Qsr->SetProgram(program);
    m_Qsr->SetService(service);
    m_Qsr->SetAlgorithm_options().Set()
        = *(algo_opts->SetOptions().GetBlast4AlgoOpts());
}

// the "int" version is not actually used (no program options need it.)
void CBlast4Options::x_SetOneParam(const char * name, const int * x)
{
    CRef<objects::CBlast4_value> v(new objects::CBlast4_value);
    v->SetInteger(*x);
        
    CRef<objects::CBlast4_parameter> p(new objects::CBlast4_parameter);
    p->SetName(name);
    p->SetValue(*v);
        
    m_Qsr->SetProgram_options().Set().push_back(p);
}

void CBlast4Options::x_SetOneParam(const char * name, const list<int> * x)
{
    CRef<objects::CBlast4_value> v(new objects::CBlast4_value);
    v->SetInteger_list() = *x;
        
    CRef<objects::CBlast4_parameter> p(new objects::CBlast4_parameter);
    p->SetName(name);
    p->SetValue(*v);
        
    m_Qsr->SetProgram_options().Set().push_back(p);
}

void CBlast4Options::x_SetOneParam(const char * name, const char ** x)
{
    CRef<objects::CBlast4_value> v(new objects::CBlast4_value);
    v->SetString().assign((x && (*x)) ? (*x) : "");
        
    CRef<objects::CBlast4_parameter> p(new objects::CBlast4_parameter);
    p->SetName(name);
    p->SetValue(*v);
        
    m_Qsr->SetProgram_options().Set().push_back(p);
}

void CBlast4Options::x_SetOneParam(const char * name, objects::CScore_matrix_parameters * matrix)
{
    CRef<objects::CBlast4_value> v(new objects::CBlast4_value);
    v->SetMatrix(*matrix);
        
    CRef<objects::CBlast4_parameter> p(new objects::CBlast4_parameter);
    p->SetName(name);
    p->SetValue(*v);
        
    m_Qsr->SetProgram_options().Set().push_back(p);
}

END_SCOPE(blast)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2004/02/04 22:31:14  bealer
* - Add async interface to Blast4 API.
* - Clean up, simplify code and interfaces.
* - Add state-based logic to promote robustness.
*
* Revision 1.1  2004/01/16 20:37:55  bealer
* - Add CBlast4Options class (Blast 4 API)
*
*
* ===========================================================================
*/
