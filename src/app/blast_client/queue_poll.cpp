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
 *   Queueing and Polling code for blast_client.
 *
 */

// Local
#include "queue_poll.hpp"

// Corelib
#include <corelib/ncbi_system.hpp>

// Objects
#include <objects/blast/Blast4_subject.hpp>
#include <objects/blast/Blast4_queue_search_reques.hpp>
#include <objects/blast/Blast4_parameter.hpp>
#include <objects/blast/Blast4_parameters.hpp>
#include <objects/blast/Blast4_value.hpp>
#include <objects/blast/blastclient.hpp>
#include <objects/blast/Blast4_queue_search_reply.hpp>
#include <objects/blast/Blas_get_searc_resul_reque.hpp>
#include <objects/blast/Blas_get_searc_resul_reply.hpp>
#include <objects/blast/Blast4_error.hpp>
#include <objects/blast/Blast4_error_code.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

// Object Manager
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/gbloader.hpp>

// Objtools
#include <objtools/readers/fasta.hpp>

// Use _exit() if available.
#if defined(NCBI_OS_UNIX)
#include <unistd.h>
#endif

USING_NCBI_SCOPE;

using namespace ncbi::objects;

typedef list< CRef<CBlast4_error> > TErrorList;


/////////////////////////////////////////////////////////////////////////////
//
//  Helper Functions
//

#define BLAST4_POLL_DELAY_SEC 15

static inline bool
s_IsAmino(const string & program)
{
    // Should the FASTA be NUC or PROT data?
        
    return (program == "blastp")  ||  (program == "tblastn");
}

void
s_Setp(list<CRef<CBlast4_parameter> >& l, string n, CRef<CBlast4_cutoff> x)
{
    CRef<CBlast4_value> v(new CBlast4_value);
    v->SetCutoff(*x);

    CRef<CBlast4_parameter> p(new CBlast4_parameter);
    p->SetName(n);
    p->SetValue(*v);

    l.push_back(p);
}

void
s_Setp(list<CRef<CBlast4_parameter> >& l, string n, const string x)
{
    CRef<CBlast4_value> v(new CBlast4_value);
    v->SetString(x);

    CRef<CBlast4_parameter> p(new CBlast4_parameter);
    p->SetName(n);
    p->SetValue(*v);

    l.push_back(p);
}

void
s_Setp(list<CRef<CBlast4_parameter> >& l, string n, const int & x)
{
    CRef<CBlast4_value> v(new CBlast4_value);
    v->SetInteger(x);
    
    CRef<CBlast4_parameter> p(new CBlast4_parameter);
    p->SetName(n);
    p->SetValue(*v);
    
    l.push_back(p);
}

void
s_Setp(list<CRef<CBlast4_parameter> >& l, string n, const bool & x)
{
    CRef<CBlast4_value> v(new CBlast4_value);
    v->SetBoolean(x);
    
    CRef<CBlast4_parameter> p(new CBlast4_parameter);
    p->SetName(n);
    p->SetValue(*v);
    
    l.push_back(p);
}

void
s_Setp(list<CRef<CBlast4_parameter> >& l, string n, const double & x)
{
    CRef<CBlast4_value> v(new CBlast4_value);
    v->SetReal(x);
    
    CRef<CBlast4_parameter> p(new CBlast4_parameter);
    p->SetName(n);
    p->SetValue(*v);
    
    l.push_back(p);
}

template <class T1, class T2, class T3>
void
s_SetpOpt(T1 & params, T2 & name, T3 & object)
{
    if (object.Exists()) {
        s_Setp(params, name, object.GetValue());
    }
}

template <class T>
void
s_Output(CNcbiOstream & os, CRef<T> t)
{
    auto_ptr<CObjectOStream> x(CObjectOStream::Open(eSerial_AsnText, os));
    *x << *t;
    os.flush();
}


/////////////////////////////////////////////////////////////////////////////
//
//  Queueing and Polling
//


static CRef<CBioseq_set>
s_SetupQuery(CNcbiIstream    & query_in,
             CRef<CScope>      scope,
             TReadFastaFlags   fasta_flags)
{
    CRef<CSeq_entry> seqentry = ReadFasta(query_in, fasta_flags, 0, 0);
    
    scope->AddTopLevelSeqEntry(*seqentry);
    
    CRef<CBioseq_set> seqset(new CBioseq_set);
    seqset->SetSeq_set().push_back(seqentry);
    
    return seqset;
}

//#include <unistd.h>

class some_kind_of_nothing : public CEofException {
};

static CRef<CBlast4_reply>
s_Submit(CRef<CBlast4_request_body> body, bool echo = true)
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
        // Use _exit() avoid coredump.
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

class CSearchParamBuilder : public COptionWalker
{
public:
    CSearchParamBuilder(list< CRef<CBlast4_parameter> > & l)
        : m_List(l)
    {}
    
    template <class T>
    void Same(T          & valobj,
              CUserOpt,
              CNetName     nb_name,
              CArgKey,
              COptDesc)
    { s_SetpOpt(m_List, nb_name, valobj); }
    
    template <class T>
    void Local(T &,
               CUserOpt,
               CArgKey,
               COptDesc)
    { }
    
    template <class T> void Remote(T & valobj, CNetName net_name)
    { s_SetpOpt(m_List, net_name, valobj); }
    
    bool NeedRemote(void) { return true; }
    
private:
    list< CRef<CBlast4_parameter> > & m_List;
};

static void
s_SetSearchParams(CNetblastSearchOpts             & opts,
                  list< CRef<CBlast4_parameter> > & l)
{
    CSearchParamBuilder spb(l);
    
    opts.Apply(spb);
}

static string
s_QueueSearch(string              & program,
              string              & database,
              string              & service,
              CNetblastSearchOpts & opts,
              CRef<CBioseq_set>     query,
              bool                  verbose,
              string              & err)
{
    string returned_rid;
    
    CRef<CBlast4_subject> subject(new CBlast4_subject);
    subject->SetDatabase(database.data());
        
    CRef<CBlast4_queue_search_request> qsr(new CBlast4_queue_search_request);
    qsr->SetProgram(program.data());
    qsr->SetService(service.data());
        
    qsr->SetSubject(*subject);
        
    if (query->GetSeq_set().front()->IsSeq()) {
        qsr->SetQueries(*query);
    } else {
        const CBioseq_set * myset = & query->GetSeq_set().front()->GetSet();
        CBioseq_set * myset2 = (CBioseq_set *) myset;
                
        qsr->SetQueries(*myset2);
    }
                
    list< CRef<CBlast4_parameter> > & l = qsr->SetParams().Set();
    
    s_SetSearchParams(opts, l);
        
    CRef<CBlast4_request_body> body(new CBlast4_request_body);
    body->SetQueue_search(*qsr);
        
    CRef<CBlast4_reply> reply = s_Submit(body, verbose);
    
    if (reply->CanGetBody()  &&
        reply->GetBody().GetQueue_search().CanGetRequest_id()) {
        
        returned_rid = reply->GetBody().GetQueue_search().GetRequest_id();
    }
    
    if (reply->CanGetErrors()) {
        const CBlast4_reply::TErrors & errs = reply->GetErrors();
        
        CBlast4_reply::TErrors::const_iterator i;
        
        for (i = errs.begin(); i != errs.end(); i++) {
            if ((*i)->GetMessage().size()) {
                if (err.size()) {
                    err += "\n";
                }
                err += (*i)->GetMessage();
            }
        }
    }
    
    return returned_rid;
}

static CRef<CBlast4_reply>
s_GetSearchResults(const string & RID, bool echo = true)
{
    CRef<CBlast4_get_search_results_request>
        gsrr(new CBlast4_get_search_results_request);
    
    gsrr->SetRequest_id(RID);
        
    CRef<CBlast4_request_body> body(new CBlast4_request_body);
    body->SetGet_search_results(*gsrr);
        
    return s_Submit(body, echo);
}

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

static void
s_ShowAlign(CNcbiOstream       & os,
            CBlast4_reply_body & reply,
            CRef<CScope>         scope,
            CAlignParms        & alparms,
            bool                 gapped)
{
    CBlast4_get_search_results_reply & cgsrr(reply.SetGet_search_results());
    
    if (! cgsrr.CanGetAlignments()) {
        os << "ERROR: Could not get an alignment from reply.\n";
        return;
    }
    
    CSeq_align_set & alignment(cgsrr.SetAlignments());
    
    list <CDisplaySeqalign::SeqlocInfo*>  none1;
    list <CDisplaySeqalign::FeatureInfo*> none2;
    
    AutoPtr<CDisplaySeqalign> dsa_ptr;
    
    if (! gapped) {
        CRef<CSeq_align_set> newalign =
            CDisplaySeqalign::PrepareBlastUngappedSeqalign(alignment);
        
        dsa_ptr = new CDisplaySeqalign(*newalign, none1, none2, 0, * scope);
    } else {
        dsa_ptr = new CDisplaySeqalign(alignment, none1, none2, 0, * scope);
    }
    
    alparms.AdjustDisplay(*dsa_ptr);
    
    dsa_ptr->DisplaySeqalign(os);
}

static Int4
s_PollForResults(const string & RID,
                 bool           verbose,
                 bool           raw_asn,
                 CRef<CScope>   scope,
                 CAlignParms  & alparms,
                 bool           gapped)
{
    CRef<CBlast4_reply> r(s_GetSearchResults(RID, verbose));
    
    Int4 EOFtime    = 0;
    Int4 MaxEOFtime = 120;
    
    bool pending = s_SearchPending(r);
    
    while (pending  &&  (EOFtime < MaxEOFtime)) {
        SleepSec(BLAST4_POLL_DELAY_SEC);
        
        try {
            r = s_GetSearchResults(RID, verbose);
            pending = s_SearchPending(r);
        }
        catch(CEofException &) {
            EOFtime += BLAST4_POLL_DELAY_SEC;
        }
    }
    
    bool raw_output = false;
    
    if (raw_asn) {
        raw_output = true;
    }
    
    if (! (r->CanGetBody()  &&  r->GetBody().IsGet_search_results())) {
        raw_output = true;
    }
    
    if (raw_output) {
        s_Output(NcbiCout, r);
    } else {
        CBlast4_reply_body & repbody = r->SetBody();
        s_ShowAlign(NcbiCout, repbody, scope, alparms, gapped);
    }
    
    return 0;
}

Int4
QueueAndPoll(string                program,
             string                database,
             string                service,
             CNetblastSearchOpts & opts,
             CNcbiIstream        & query_in,
             bool                  verb,
             bool                  trust_defline,
             bool                  raw_asn,
             CAlignParms         & alparms)
{
    Int4 err_ret = 0;
        
    // Read the FASTA input data
    string fasta_line1;
    string fasta_block;
        
    // Queue and poll
    string RID;
        

    CRef<CObjectManager> objmgr(new CObjectManager);
    CRef<CScope>         scope (new CScope(*objmgr));
        
    objmgr->RegisterDataLoader(*new CGBDataLoader("ID", 0, 2),
                               CObjectManager::eDefault);
    
    scope->AddDefaults();
        
    CRef<CBioseq_set> cbss;
    
    bool amino         = s_IsAmino(program);
    
    int flags = fReadFasta_AllSeqIds; // | fReadFasta_OneSeq;
    
    if (amino) {
        flags |= fReadFasta_AssumeProt;
    } else {
        flags |= fReadFasta_AssumeNuc;
    }
    
    if (! trust_defline) {
        flags |= fReadFasta_NoParseID;
    }
    
    cbss = s_SetupQuery(query_in, scope, flags);
    
    string err;
    
    RID = s_QueueSearch(program,
                        database,
                        service,
                        opts,
                        cbss,
                        verb,
                        err);
    
    if (RID.size()) {
        alparms.SetRID(RID);
        
        if (! err_ret) {
            bool gapped = true;
            
            if (opts.Gapped().Exists()) {
                gapped = opts.Gapped().GetValue();
            }
            
            err_ret =
                s_PollForResults(RID, verb, raw_asn, scope, alparms, gapped);
        }
    } else {
        ERR_POST(Error << "Could not queue request.");
        
        if (err.size()) {
            ERR_POST(Error << err);
        }
        
        err_ret = -1;
    }
    
    return err_ret;
}

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.2  2003/09/26 20:01:43  bealer
 * - Fix Solaris compile errors.
 *
 * Revision 1.1  2003/09/26 16:53:49  bealer
 * - Add blast_client project for netblast protocol, initial code commit.
 *
 * ===========================================================================
 */
