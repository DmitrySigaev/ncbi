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
*   Queueing and Polling code for Remote Blast API
*
* ===========================================================================
*/

#include <corelib/ncbi_system.hpp>
#include <algo/blast/api/remote_blast.hpp>

#include <objects/blast/blastclient.hpp>
#include <objects/blast/Blast4_request.hpp>
#include <objects/blast/Blast4_request_body.hpp>
#include <objects/blast/Blast4_error.hpp>
#include <objects/blast/Blas_get_searc_resul_reque.hpp>
#include <objects/blast/Blast4_error_code.hpp>

#if defined(NCBI_OS_UNIX)
#include <unistd.h>
#endif

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

static CRef<CBlast4_reply>
s_SendRequest(CRef<CBlast4_request_body> body,
              CRemoteBlast::EDebugMode   echo = CRemoteBlast::eSilent)
{
    // Create the request; optionally echo it
    
    CRef<CBlast4_request> request(new CBlast4_request);
    request->SetBody(*body);
    
    if (CRemoteBlast::eDebug == echo) {
        s_Output(NcbiCout, request);
    }
    
    // submit to server, get reply; optionally echo it
    
    CRef<CBlast4_reply> reply(new CBlast4_reply);
    
    try {
        //throw some_kind_of_nothing();
        CBlast4Client().Ask(*request, *reply);
    }
    catch(const CEofException&) {
        ERR_POST(Error << "Unexpected EOF when contacting netblast server"
                 " - unable to complete request.");
#if defined(NCBI_OS_UNIX)
        // Use _exit() to avoid coredump.
        _exit(-1);
#else
        exit(-1);
#endif
    }
    
    if (CRemoteBlast::eDebug == echo) {
        s_Output(NcbiCout, reply);
    }
    
    return reply;
}

static CRef<CBlast4_reply>
s_GetSearchResults(const string & RID, CRemoteBlast::EDebugMode echo = CRemoteBlast::eSilent)
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

bool CRemoteBlast::SubmitSync(int seconds)
{
    // eFailed: no work to do, already an error.
    // eDone:   already done, just return.
    
    EImmediacy immed = ePollAsync;
    
    switch(x_GetState()) {
    case eStart:
        x_SubmitSearch();
        if (! m_Err.empty()) {
            break;
        }
        immed = ePollImmed;
        // fall through
        
    case eWait:
        x_PollUntilDone(immed, seconds);
        break;
    }
    
    return (x_GetState() == eDone);
}



// Pre:  start
// Post: failed, wait or done

// Returns: true if no error so far

bool CRemoteBlast::Submit(void)
{
    switch(x_GetState()) {
    case eStart:
        x_SubmitSearch();
    }
    
    return m_Err.empty();
}

// Pre:  start, wait or done
// Post: wait or done

// Returns: true if done

bool CRemoteBlast::CheckDone(void)
{
    switch(x_GetState()) {
    case eFailed:
    case eDone:
        break;
        
    case eStart:
        Submit();
        break;
        
    case eWait:
        x_CheckResults();
    }
    
    return (x_GetState() == eDone);
}

CRef<objects::CSeq_align_set> CRemoteBlast::GetAlignments(void)
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

CRef<objects::CBlast4_phi_alignments> CRemoteBlast::GetPhiAlignments(void)
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
    
CRef<objects::CBlast4_mask> CRemoteBlast::GetMask(void)
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
    
list< CRef<objects::CBlast4_ka_block > > CRemoteBlast::GetKABlocks(void)
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
    
list< string > CRemoteBlast::GetSearchStats(void)
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


// Internal CRemoteBlast methods

int CRemoteBlast::x_GetState(void)
{
    // CBlast4Option states:
    
    // 0. start  (no rid, no errors)
    // 1. failed (errors)
    // 2. wait   (has rid, no errors, still pending)
    // 3. done   (has rid, no errors, not pending)
    
    int rv = 0;
    
    if (! m_Err.empty()) {
        rv = eFailed;
    } else if (m_RID.empty()) {
        rv = eStart;
    } else if (m_Pending) {
        rv = eWait;
    } else {
        rv = eDone;
    }
    
    return rv;
}

void CRemoteBlast::x_SubmitSearch(void)
{
    if (m_QSR.Empty()) {
        m_Err = "No request exists and no RID was specified.";
        return;
    }
    
    x_SetAlgoOpts();
    
    CRef<CBlast4_request_body> body(new CBlast4_request_body);
    body->SetQueue_search(*m_QSR);
    
    CRef<CBlast4_reply> reply;
    
    try {
        reply = s_SendRequest(body, m_Verbose);
    }
    catch(const CEofException&) {
        m_Err = "Unexpected EOF when contacting netblast server - unable to submit request.";
        return;
    }
    
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

void CRemoteBlast::x_CheckResults(void)
{
    if (!m_Err.empty()) {
        m_Pending = false;
    }
    
    if (! m_Pending)
        return;
    
    CRef<CBlast4_reply> r;
    
    bool try_again = true;
    
    while(try_again) {
        try {
            r = s_GetSearchResults(m_RID, m_Verbose);
            m_Pending = s_SearchPending(r);
            try_again = false;
        }
        catch(const CEofException&) {
            --m_ErrIgn;
            
            if (m_ErrIgn == 0) {
                m_Err = "Unexpected EOF when contacting netblast server - unable to submit request.";
                return;
            }
            
            SleepSec(10);
        }
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

// The input here is a hint as to whether the request might be ready.
// If the flag is true, then we are polling immediately after
// submission.  In this case, the results will not be ready, and so we
// skip the first results check to reduce net traffic.  If the flag is
// false, then the user is using the asynchronous interface, and we do
// not know how long it has been since the request was submitted.  In
// this case, we check the results before sleeping.
//
// If this was always set to 'true' then async mode would -always-
// sleep.  This is undesireable in the case where (for example) 100
// requests are batched together - the mandatory sleeps would add to a
// total of 1000 seconds, more than a quarter hour.
//
// If it were always specified as 'false', then synchronous mode would
// shoot off an immediate 'check results' as soon as the "submit"
// returned, which creates unnecessary traffic.
//
// Futher optimizations are no doubt possible.

void CRemoteBlast::x_PollUntilDone(EImmediacy immed, int timeout)
{
    if (eDebug == m_Verbose)
        cout << "polling " << 0 << endl;
    
    // Configuration - internal for now
    
    double start_sec = 10.0;
    double increment = 1.30;
    double max_sleep = 300.0;
    double max_time  = timeout;
    
    if (eDebug == m_Verbose)
        cout << "polling " << start_sec << "/" << increment << "/" << max_sleep << "/" << max_time << "/" << endl;
    
    // End config
    
    double sleep_next = start_sec;
    double sleep_totl = 0.0;
    
    if (eDebug == m_Verbose)
        cout << "line " << __LINE__ << " sleep next " << sleep_next << " sleep totl " << sleep_totl << endl;
    
    if (ePollAsync == immed) {
        x_CheckResults();
    }
    
    while (m_Pending && (sleep_totl < max_time)) {
        if (eDebug == m_Verbose)
            cout << " about to sleep " << sleep_next << endl;
        
        double max_left = max_time - sleep_totl;
        
        // Don't oversleep
        if (sleep_next > max_left) {
            sleep_next = max_left;
            
            // But never sleep less than 2
            if (sleep_next < 2.0)
                sleep_next = 2.0;
        }
        
        SleepSec(int(sleep_next));
        sleep_totl += sleep_next;
        
        if (eDebug == m_Verbose)
            cout << " done, total = " << sleep_totl << endl;
        
        if (sleep_next < max_sleep) {
            sleep_next *= increment;
            if (sleep_next > max_sleep) {
                sleep_next = max_sleep;
            }
        }
        
        if (eDebug == m_Verbose)
            cout << " next sleep time = " << sleep_next << endl;
        
        x_CheckResults();
    }
}

void CRemoteBlast::x_Init(CBlastOptionsHandle * opts_handle,
                            const char          * program,
                            const char          * service)
{
    m_CBOH.Reset( opts_handle );
    m_ErrIgn    = 5;
    m_Pending   = false;
    m_Verbose   = eSilent;
    
    m_QSR.Reset(new objects::CBlast4_queue_search_request);
    m_QSR->SetProgram(program);
    m_QSR->SetService(service);
    
    if (! (opts_handle && opts_handle->SetOptions().GetBlast4AlgoOpts())) {
        // This happens if you do not specify eRemote for the
        // CBlastOptions subclass constructor.
        
        string errmsg("CRemoteBlast: No remote API options.");
        
        ERR_POST(errmsg);
        throw runtime_error(errmsg);
    }
}

void CRemoteBlast::x_SetAlgoOpts(void)
{
    CBlast4_parameters * algo_opts =
        m_CBOH->SetOptions().GetBlast4AlgoOpts();
    
    m_QSR->SetAlgorithm_options().Set() = *algo_opts;
}

void CRemoteBlast::x_Init(const string & RID)
{
    m_ErrIgn    = 5;
    m_Pending   = true;
    m_Verbose   = eSilent;
    m_RID       = RID;
}

// the "int" version is not actually used (no program options need it.)
void CRemoteBlast::x_SetOneParam(const char * name, const int * x)
{
    CRef<objects::CBlast4_value> v(new objects::CBlast4_value);
    v->SetInteger(*x);
        
    CRef<objects::CBlast4_parameter> p(new objects::CBlast4_parameter);
    p->SetName(name);
    p->SetValue(*v);
        
    m_QSR->SetProgram_options().Set().push_back(p);
}

void CRemoteBlast::x_SetOneParam(const char * name, const list<int> * x)
{
    CRef<objects::CBlast4_value> v(new objects::CBlast4_value);
    v->SetInteger_list() = *x;
        
    CRef<objects::CBlast4_parameter> p(new objects::CBlast4_parameter);
    p->SetName(name);
    p->SetValue(*v);
        
    m_QSR->SetProgram_options().Set().push_back(p);
}

void CRemoteBlast::x_SetOneParam(const char * name, const char ** x)
{
    CRef<objects::CBlast4_value> v(new objects::CBlast4_value);
    v->SetString().assign((x && (*x)) ? (*x) : "");
        
    CRef<objects::CBlast4_parameter> p(new objects::CBlast4_parameter);
    p->SetName(name);
    p->SetValue(*v);
        
    m_QSR->SetProgram_options().Set().push_back(p);
}

void CRemoteBlast::x_SetOneParam(const char * name, objects::CScore_matrix_parameters * matrix)
{
    CRef<objects::CBlast4_value> v(new objects::CBlast4_value);
    v->SetMatrix(*matrix);
        
    CRef<objects::CBlast4_parameter> p(new objects::CBlast4_parameter);
    p->SetName(name);
    p->SetValue(*v);
        
    m_QSR->SetProgram_options().Set().push_back(p);
}

END_SCOPE(blast)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.5  2004/03/12 22:07:03  camacho
* Remove unused variables
*
* Revision 1.4  2004/02/26 22:24:46  gorelenk
* Include for <unistd.h> moved to be after
* #include <corelib/ncbi_system.hpp>.
*
* Revision 1.3  2004/02/26 17:07:40  gorelenk
* Added #if defined(NCBI_OS_UNIX) for #include <unistd.h>.
*
* Revision 1.2  2004/02/18 18:28:51  bealer
* - Fix verbosity tests.
*
* Revision 1.1  2004/02/18 17:30:57  bealer
* - Change blast4_options.* to remote_blast.*, plus changes.
*
* Revision 1.6  2004/02/09 22:35:37  bealer
* - Delay examination of CBlastOptionsHandle object until Submit() action.
*
* Revision 1.5  2004/02/06 00:16:39  bealer
* - Add RID capability.
* - Detect lack of eRemote flag.
*
* Revision 1.4  2004/02/05 19:20:39  bealer
* - Add retry capability to API code.
*
* Revision 1.3  2004/02/05 00:37:43  bealer
* - Polling optimization.
*
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
