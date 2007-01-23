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
 * Author:  Christiam Camacho
 *
 */

/// @file blast_aux_priv.cpp
/// Implements various auxiliary (private) functions for BLAST

#include <ncbi_pch.hpp>
#include "blast_aux_priv.hpp"
#include <algo/blast/core/blast_seqsrc.h>
#include <algo/blast/core/blast_query_info.h>
#include <algo/blast/api/seqinfosrc_seqdb.hpp>
#include <algo/blast/api/blast_exception.hpp>

#include <objects/seqloc/Seq_loc.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

IBlastSeqInfoSrc*
InitSeqInfoSrc(const BlastSeqSrc* seqsrc)
{
    string db_name;
    if (const char* seqsrc_name = BlastSeqSrcGetName(seqsrc)) {
        db_name.assign(seqsrc_name);
    }
    if (db_name.empty()) {
        NCBI_THROW(CBlastException, eNotSupported,
                   "BlastSeqSrc does not provide a name, probably it is not a"
                   " BLAST database");
    }
    bool is_prot = BlastSeqSrcGetIsProt(seqsrc) ? true : false;
    return new CSeqDbSeqInfoSrc(db_name, is_prot);
}

IBlastSeqInfoSrc*
InitSeqInfoSrc(CSeqDB* seqdb)
{
    return new CSeqDbSeqInfoSrc(seqdb);
}

CConstRef<CSeq_loc> CreateWholeSeqLocFromIds(const list< CRef<CSeq_id> > seqids)
{
    _ASSERT(!seqids.empty());
    CRef<CSeq_loc> retval(new CSeq_loc);
    retval->SetWhole().Assign(**seqids.begin());
    return retval;
}

void
Blast_Message2TSearchMessages(const Blast_Message* blmsg,
                              const BlastQueryInfo* query_info,
                              TSearchMessages& messages)
{
    if ( !blmsg || !query_info ) {
        return;
    }

    if (messages.size() != (size_t) query_info->num_queries) {
        messages.resize(query_info->num_queries);
    }

    const BlastContextInfo* kCtxInfo = query_info->contexts;

    // First copy the errors...
    for (; blmsg; blmsg = blmsg->next)
    {
        const int kContext = blmsg->context;
        _ASSERT(blmsg->message);
        string msg(blmsg->message);

        if (kContext != kBlastMessageNoContext) {
            // applies only to a single query
            const int kQueryIndex = kCtxInfo[kContext].query_index;
            CRef<CSearchMessage> sm(new CSearchMessage(blmsg->severity,
                                                       kQueryIndex, msg));
            messages[kCtxInfo[kContext].query_index].push_back(sm);
        } else {
            // applies to all queries
            CRef<CSearchMessage> sm(new CSearchMessage(blmsg->severity,
                                                       kBlastMessageNoContext, 
                                                       msg));
            NON_CONST_ITERATE(TSearchMessages, query_messages, messages) {
                query_messages->push_back(sm);
            }
        }


    }

    // ... then remove duplicate error messages
    messages.RemoveDuplicates();
}

string
BlastErrorCode2String(Int2 error_code)
{
    Blast_Message* blast_msg = NULL;
    Blast_PerrorEx(&blast_msg, error_code, __FILE__, __LINE__, -1);
    string retval(blast_msg->message);
    blast_msg = Blast_MessageFree(blast_msg);
    return retval;
}

CSearchResultSet
BlastBuildSearchResultSet(const vector< CConstRef<CSeq_id> >& query_ids,
                          const BlastScoreBlk* sbp,
                          const BlastQueryInfo* qinfo,
                          EBlastProgramType program,
                          const TSeqAlignVector& alignments,
                          TSearchMessages& messages,
                          const TSeqLocInfoVector* query_masks)
{
    const bool is_phi = Blast_ProgramIsPhiBlast(program);

    // Collect query Seq-locs
    
    vector< CConstRef<CSeq_id> > qlocs;
    
    if (is_phi) {
        qlocs.assign(alignments.size(), query_ids.front());
    } else {
        copy(query_ids.begin(), query_ids.end(), back_inserter(qlocs));
    }
    
    // Collect ancillary data
    
    vector< CRef<CBlastAncillaryData> > ancillary_data;
    
    if (is_phi) {
        CRef<CBlastAncillaryData> s(new CBlastAncillaryData(program, 0, sbp,
                                                            qinfo));
        
        for(unsigned i = 0; i < alignments.size(); i++) {
            ancillary_data.push_back(s);
        }
    } else {
        for(unsigned i = 0; i < alignments.size(); i++) {
            CRef<CBlastAncillaryData> s(new CBlastAncillaryData(program, i, sbp,
                                                                qinfo));
            ancillary_data.push_back(s);
        }
    }
    
    // The preliminary stage also produces errors and warnings; they
    // should be copied from that code to this class somehow, and
    // returned here if they have not been returned or reported yet.
    
    if (messages.size() < alignments.size()) {
        messages.resize(alignments.size());
    }
    
    return CSearchResultSet(qlocs, alignments, messages, ancillary_data,
                            query_masks);
}

TMaskedQueryRegions
PackedSeqLocToMaskedQueryRegions(CConstRef<objects::CSeq_loc> sloc_in,
                                 EBlastProgramType            prog)
{
    if (sloc_in.Empty() || 
        sloc_in->Which() == CSeq_loc::e_not_set ||
        sloc_in->IsEmpty() || 
        sloc_in->IsNull()) {
        return TMaskedQueryRegions();
    }
    
    CConstRef<CSeq_loc> sloc = sloc_in;
    
    if (sloc_in->IsInt()) {
        CRef<CSeq_interval>
            iv( const_cast<CSeq_interval *>(& sloc_in->GetInt()) );
        
        CRef<CSeq_loc> nsloc(new CSeq_loc);
        nsloc->SetPacked_int().Set().push_back(iv);
        
        sloc.Reset(&*nsloc);
    }
    
    if (! sloc->IsPacked_int()) {
        NCBI_THROW(CBlastException, eNotSupported, 
                   "Unsupported Seq-loc type used for mask");
    }
    
    const objects::CPacked_seqint & psi = sloc->GetPacked_int();
    
    TMaskedQueryRegions mqr;
    
    ITERATE(list< CRef< objects::CSeq_interval > >, iter, psi.Get()) {
        objects::CSeq_interval * iv =
            const_cast<objects::CSeq_interval*>(& (**iter));
        
        if (Blast_QueryIsProtein(prog)) {
            int fr = (int) CSeqLocInfo::eFrameNotSet;
            mqr.push_back(CRef<CSeqLocInfo>(new CSeqLocInfo(iv, fr)));
        } else {
            bool do_pos = false;
            bool do_neg = false;
        
            if (iv->CanGetStrand()) {
                switch(iv->GetStrand()) {
                case objects::eNa_strand_plus:
                    do_pos = true;
                    break;
                
                case objects::eNa_strand_minus:
                    do_neg = true;
                    break;
                
                case objects::eNa_strand_both:
                    do_pos = true;
                    do_neg = true;
                    break;
                
                default:
                    NCBI_THROW(CBlastException, eNotSupported, 
                               "Unsupported strand type used for query");
                }
            } else {
                // intervals with no strand assignment will use both.
                do_pos = true;
                do_neg = true;
            }
            
            if (do_pos) {
                int fr = (int) CSeqLocInfo::eFramePlus1;
                mqr.push_back(CRef<CSeqLocInfo>(new CSeqLocInfo(iv, fr)));
            }
            
            // No reversal is done here.  Tt seems that the code (in core)
            // that applies the mask reverses it.  Whether this is an
            // accidental or designed is not clear to me, but for now this
            // will remain as is.
            
            if (do_neg) {
                int fr = (int) CSeqLocInfo::eFrameMinus1;
                mqr.push_back(CRef<CSeqLocInfo>(new CSeqLocInfo(iv, fr)));
            }
        }
    }
    
    return mqr;
}

CRef<objects::CSeq_loc>
MaskedQueryRegionsToPackedSeqLoc( const TMaskedQueryRegions & /*sloc*/ )
{
    // This function could produce this error only in cases where a
    // clean (non-information-losing) conversion failed.
    
    NCBI_THROW(CBlastException, eNotSupported, 
               "Attempt to convert mask information in lossy direction.");
    
    return CRef<objects::CSeq_loc>();
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
