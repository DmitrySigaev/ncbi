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
*     Code to build a database given various sources of sequence data.
*/

#include <ncbi_pch.hpp>

// Blast databases

#include <objtools/readers/seqdb/seqdbexpert.hpp>
#include <objtools/writers/writedb/writedb.hpp>
#include <objtools/readers/fasta.hpp>

// Object Manager

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/genbank/id2/reader_id2.hpp>
#include <objmgr/seq_vector.hpp>

// Other utilities

#include <util/sequtil/sequtil_convert.hpp>

// Local

#include "build_db.hpp"
#include "multisource_util.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);

int debug_mode = 0;

void CBuildDatabase::x_ResolveRemoteId(CRef<CSeq_id> & seqid, int & gi)
{
    CScope::TIds ids = x_GetScope().GetIds(*seqid);
    
    bool have_seqid = false;
    bool have_gi = false;
    
    gi = 0;
    
    ITERATE(CScope::TIds, iter, ids) {
        CConstRef<CSeq_id> id = iter->GetSeqId();
        if (debug_mode > 5)
            m_LogFile << "Seq-id " << seqid->AsFastaString()
                      << " contains id " << id->AsFastaString() << endl;
        
        if (id->IsGi()) {
            if (gi > 0) {
                if (debug_mode > 5)
                    m_LogFile << "WARNING: multiple GIs discovered; gi[0] = "
                              << gi << endl;
            } else {
                if (debug_mode > 5)
                    m_LogFile << "Seq-id " << seqid->AsFastaString()
                              << " resolved to "
                              << id->GetGi() << endl;
                gi = id->GetGi();
                have_gi = true;
            }
        } else if ((! have_seqid) && (id->Which() == seqid->Which())) {
            m_LogFile << "Remote: Resolving <" << seqid->AsFastaString()
                      << "> to <" << id->AsFastaString() << ">" << endl;
            
            if (id->GetTextseq_Id() == NULL ||
                id->GetTextseq_Id()->IsSetVersion() == false) {
                
                m_LogFile
                    << "Warning: Resolution still does not provide version."
                    << endl;
            } else {
                seqid.Reset(const_cast<CSeq_id*>(id.GetPointer()));
                have_seqid = true;
            }
        }
        
        if (have_gi)
            break;
    }
}

// Resolve all ids to GIs, storing them in a GI list.

CRef<CInputGiList> CBuildDatabase::x_ResolveGis(const vector<string> & ids)
{
    CRef<CInputGiList> gi_list(new CInputGiList);
    
    ITERATE(vector<string>, id, ids) {
        // There are three possibilities:
        //
        // 1. Numbers are added to the list as GIs.
        // 2. Remote services may be called to determine the most
        //    recent version.
        // 3. Non-numerical types are added to the list as Seq-ids.
        //
        // For #2, the remote service call is only made if:
        //
        // A. Remote services are enabled.
        // B. The Seq-id can have a version (only CTextseq_id types.)
        // C. The version is not present.
        
        int gi(0);
        bool specific = false;
        CRef<CSeq_id> seqid;
        
        bool worked = CheckAccession(*id, gi, seqid, specific);
        
        // If a source database is specified, try that as a backup
        // resolution mechanism.
        
        if (! worked) {
            if (m_SourceDb.NotEmpty()) {
                worked = x_ResolveFromSource(*id, seqid);
            }
        }
        
        if (! worked) {
            m_LogFile << "Did not recognize id: \"" << *id << "\"" << endl;
            continue;
        }
        
        // 1. Numeric GI
        
        if (gi != 0) {
            if (debug_mode > 5)
                m_LogFile << "Found numerical GI:" << gi << endl;
            
            gi_list->AppendGi(gi);
            continue;
        }
        
        // 2. Possible remote resolution.  We look for a GI and if
        // that is not found, try to find a Seq-id of the same type
        // (but with a version).
        
        if (m_UseRemote && (! specific)) {
            x_ResolveRemoteId(seqid, gi);
            
            if (gi != 0) {
                gi_list->AppendGi(gi);
                continue;
            }
        }
        
        // 3. Just add the Seq-id as a Seq-id.
        
        gi_list->AppendSeqId(seqid);
    }
    
    return gi_list;
}

bool CBuildDatabase::x_ResolveFromSource(const string  & acc,
                                         CRef<CSeq_id> & id)
{
    if (m_SourceDb.Empty()) {
        return false;
    }
    
    vector<int> oids;
    m_SourceDb->AccessionToOids(acc, oids);
    
    bool found(false), done(false);
    
    ITERATE(vector<int>, oid, oids) {
        list< CRef<CSeq_id> > ids = m_SourceDb->GetSeqIDs(*oid);
        
        ITERATE(list< CRef<CSeq_id> >, seqid, ids) {
            CRef<CSeq_id> s = *seqid;
            
            string S = s->AsFastaString();
            size_t pos = S.find(acc);
            
            if (pos != string::npos) {
                size_t endpos = pos + acc.size();
                
                bool start_okay = (pos == 0 || S[pos-1] == '|');
                bool end_okay = ((endpos == S.size()) ||
                                 (S[endpos] == '.' ||
                                  S[endpos] == '|'));
                
                if (start_okay && end_okay) {
                    done = true;
                }
                
                if (done || (! found)) {
                    found = true;
                    id = s;
                }
            }
            
            if (done)
                break;
        }
        
        if (done)
            break;
    }
    
    return found;
}

void CBuildDatabase::x_DupLocal()
{
    TIdToBits bitset;
    
    // Get sequence, deflines, ambiguities, and sometimes pigs.  The
    // simplest route (for WriteDB) is raw data + asn deflines, so we
    // use that when possible.
    
    CStopWatch sw(CStopWatch::eStart);
    int count = 0;
    
    for(int oid = 0; m_SourceDb->CheckOrFindOID(oid); oid++) {
        // Raw data.
        
        const char * buffer (0);
        int          slength(0);
        int          alength(0);
        
        m_SourceDb->GetRawSeqAndAmbig(oid, & buffer, & slength, & alength);
        
        CSequenceReturn seqret(*m_SourceDb, buffer);
        
        CTempString sequence(buffer, slength);
        CTempString ambig(buffer + slength, alength);
        
        // Deflines
        
        CRef<CBlast_def_line_set> headers = m_SourceDb->GetHdr(oid);
        m_DeflineCount += headers->Get().size();
        m_OIDCount ++;
        
        x_SetLinkAndMbit(headers);
        
        // Always include the taxid; although OPTIONAL, some programs
        // expect it, since the C ASN.1 loaders always emit integers.
        
        headers = m_Taxids->FixTaxId(headers, m_KeepTaxIds);
        
        // Now, add the sequence to the WriteDB database.
        
        m_OutputDb->AddSequence(sequence, ambig);
        m_OutputDb->SetDeflines(*headers);
        count ++;
    }
    
    if (count) {
        double t = sw.Elapsed();
        
        m_LogFile << "Duplication from source DB; duplicated "
                  << count << " sequences in " << t << " seconds." << endl;
    }
}

// This could be moved to writedb once it is tested and working.

static CConstRef<CBioseq> s_FixBioseqDeltas(CConstRef<CBioseq> bs)
{
    if ((! bs->CanGetInst()) || bs->GetInst().CanGetSeq_data()) {
        return bs;
    }
    
    if (bs->CanGetInst() &&
        bs->GetInst().CanGetExt() &&
        bs->GetInst().GetExt().IsDelta() &&
        bs->GetInst().CanGetMol() &&
        (bs->GetInst().GetMol() != CSeq_inst::eMol_na)) {
        
        NCBI_THROW(CMultisourceException, eArg,
                   "Protein delta sequences are not supported.");
    }
    
    try {
        const CDelta_ext & dext = bs->GetInst().GetExt().GetDelta();
        
        typedef list< CRef< CDelta_seq > > TItems;
        
        // Don't really want to use na4, because a half byte at the
        // end of a string would require that string to be manually
        // adjusted before appending.
        
        string seq8na;
        if (bs->GetInst().CanGetLength()) {
            seq8na.reserve(bs->GetInst().GetLength());
        }
        
        string na8;
        
        ITERATE(TItems, item, dext.Get()) {
            const CSeq_literal & L = (**item).GetLiteral();
            
            if (L.GetSeq_data().IsNcbi2na()) {
                CSeqConvert::Convert(L.GetSeq_data().GetNcbi2na(),
                                     CSeqUtil::e_Ncbi2na,
                                     0,
                                     L.GetLength(),
                                     na8,
                                     CSeqUtil::e_Ncbi8na);
            } else if (L.GetSeq_data().IsNcbi4na()) {
                CSeqConvert::Convert(L.GetSeq_data().GetNcbi4na(),
                                     CSeqUtil::e_Ncbi4na,
                                     0,
                                     L.GetLength(),
                                     na8,
                                     CSeqUtil::e_Ncbi8na);
            } else {
                NCBI_THROW(CMultisourceException, eArg,
                           "Unhandled type of sequence data encountered.");
            }
            
            seq8na += na8;
            na8.resize(0);
        }
        
        // Now convert back to 4na, since WriteDB does not yet handle
        // 8na sequences.
        
        int length = seq8na.size();
        vector<char> seq4na;
        CSeqConvert::Convert(seq8na,
                             CSeqUtil::e_Ncbi8na,
                             0,
                             length,
                             seq4na,
                             CSeqUtil::e_Ncbi4na);
        
        // Copy the needed fields of the CBioseq (but remove the delta
        // sequence) and add a Seq-data.
        
        CRef<CBioseq> bs2(new CBioseq);
        
        if (bs->IsSetId()) {
            bs2->SetId() = bs->GetId();
        }
        
        if (bs->IsSetDescr()) {
            bs2->SetDescr(const_cast<CSeq_descr&>(bs->GetDescr()));
        }
        
        CRef<CSeq_inst> inst(new CSeq_inst);
        
        inst->SetSeq_data().SetNcbi4na().Set().swap(seq4na);
        inst->SetMol(CSeq_inst::eMol_na);
        inst->SetLength(length);
        inst->SetRepr(CSeq_inst::eRepr_raw);
        
        bs2->SetInst(*inst);
        
        if (bs->IsSetAnnot()) {
            bs2->SetAnnot() = bs->GetAnnot();
        }
        
        bs = bs2;
    }
    catch(CInvalidChoiceSelection &) {
        NCBI_THROW(CMultisourceException, eArg,
                   "Bioseq must have Seq-data or "
                   "Delta containing only literals.");
    }
    
    return bs;
}

void CBuildDatabase::x_EditAndAddBioseq(CConstRef<CBioseq>   bs,
                                        CSeqVector         * sv)
{
    CRef<CBlast_def_line_set> headers =
        CWriteDB::ExtractBioseqDeflines(*bs);
    
    m_DeflineCount += headers->Get().size();
    m_OIDCount ++;
    
    // Always include the taxid; although OPTIONAL, some programs
    // expect it, since the C ASN.1 loaders always emit integers.
    
    headers = m_Taxids->FixTaxId(headers, m_KeepTaxIds);
    
    // Edit the linkouts
    
    x_SetLinkAndMbit(headers);
    
    // Add the sequence
    
    if (sv) {
        m_OutputDb->AddSequence(*bs, *sv);
    } else {
        bs = s_FixBioseqDeltas(bs);
        m_OutputDb->AddSequence(*bs);
    }
    
    m_OutputDb->SetDeflines(*headers);
}

void CBuildDatabase::x_AddOneRemoteSequence(const CSeq_id & seqid,
                                            bool          & found_all,
                                            bool          & error)
{
    // Get handle and bioseq
    
    CConstRef<CBioseq> bs;
    CBioseq_Handle bsh;
    
    try {
        bsh = x_GetScope().GetBioseqHandle(seqid);
        bs = bsh.GetCompleteBioseq();
        
        if (debug_mode > 5) m_LogFile << MSerial_AsnText << *bs << endl;
    }
    catch (const CException & e) {
        m_LogFile << "Caught exception for query: "
                  << seqid.AsFastaString() << endl
                  << e.what() << endl;
        found_all = false;
        error = true;
    }
    
    if (bsh.GetState() & CBioseq_Handle::fState_not_found) {
        error = true;
    }
    
    if (error) {
        if (debug_mode > 5)
            m_LogFile << "Could not find entry for: "
                      << seqid.AsFastaString() << endl;
        
        found_all = false;
        return;
    }
    
    CSeqVector sv(bsh);
    
    x_EditAndAddBioseq(bs, & sv);
    
    if (debug_mode > 5)
        m_LogFile << "-- REMOTE: Found sequence "
                  << seqid.AsFastaString() << endl;
}

bool CBuildDatabase::x_AddRemoteSequences(CInputGiList & gi_list)
{
    CStopWatch sw(CStopWatch::eStart);
    int count = 0;
    
    bool found_all = true;
    
    int num_gis = gi_list.GetNumGis();
    int i = 0;
    
    for(i = 0; i < num_gis; i++) {
        if (m_Verbose)
            m_LogFile << "GI " << gi_list[i].gi;
        
        // We only need to fetch here for those cases where the SeqDB
        // attempt could not translate the GI.
        
        if (gi_list.GetGiOid(i).oid == -1) {
            if (m_Verbose)
                m_LogFile << " not found locally; adding remotely." << endl;
            
            CRef<CSeq_id> id(new CSeq_id);
            id->SetGi(gi_list[i].gi);
            
            bool error = false;
            
            x_AddOneRemoteSequence(*id, found_all, error);
            count++;
        } else {
            if (m_Verbose)
                m_LogFile << " found locally; not adding remotely." << endl;
        }
    }
    
    int num_seqids = gi_list.GetNumSeqIds();
    
    for(i = 0; i < num_seqids; i++) {
        if (m_Verbose)
            m_LogFile << "Seq-id "
                      << gi_list.GetSeqIdOid(i).seqid->AsFastaString();
        
        // We only need to fetch here for those cases where the SeqDB
        // attempt could not translate the GI.
        
        if (gi_list.GetSeqIdOid(i).oid == -1) {
            if (m_Verbose)
                m_LogFile << " not found locally; adding remotely." << endl;
            
            bool error = false;
            
            x_AddOneRemoteSequence(*gi_list.GetSeqIdOid(i).seqid,
                                   found_all,
                                   error);
            count++;
        } else {
            if (m_Verbose)
                m_LogFile << " found locally; not adding remotely." << endl;
        }
    }
    
    if (count) {
        double t = sw.Elapsed();
        
        m_LogFile << "Adding sequences from remote source; added "
                  << count << " sequences in " << t << " seconds." << endl;
    }
    
    return found_all;
}

bool
CBuildDatabase::x_ReportUnresolvedIds(const CInputGiList & gi_list) const
{
    bool success = true;
    
    int num_gis = gi_list.GetNumGis();
    
    int unresolved = 0;
    
    int i;
    for(i = 0; i < num_gis; i++) {
        // We only need to fetch here for those cases where the SeqDB
        // attempt could not translate the GI.
        
        if (gi_list.GetGiOid(i).oid == -1) {
            if (m_Verbose)
                m_LogFile << "GI " << gi_list[i].gi
                          << " was not resolvable." << endl;
            
            success = false;
            unresolved ++;
        } else {
            if (m_Verbose)
                m_LogFile << "GI " << gi_list[i].gi
                          << " found locally." << endl;
        }
    }
    
    int num_seqids = gi_list.GetNumSeqIds();
    
    for(i = 0; i < num_seqids; i++) {
        // We only need to fetch here for those cases where the SeqDB
        // attempt could not translate the GI.
        
        if (gi_list.GetSeqIdOid(i).oid == -1) {
            if (m_Verbose)
                m_LogFile << "Seq-id "
                          << gi_list.GetSeqIdOid(i).seqid->AsFastaString()
                          << " was not resolvable." << endl;
            
            unresolved ++;
            success = false;
        } else {
            if (m_Verbose)
                m_LogFile << "Seq-id "
                          << gi_list.GetSeqIdOid(i).seqid->AsFastaString()
                          << " found locally." << endl;
        }
    }
    
    if (unresolved) {
        m_LogFile << "Could not resolve " << unresolved << " IDs." << endl;
    }
    
    success = false;
    unresolved ++;
    
    return success;
}

bool CBuildDatabase::x_AddFastaSequences(CNcbiIstream & fasta_file)
{
    bool found = false;
    
    CStopWatch sw(CStopWatch::eStart);
    int count = 0;
    
    CRef<ILineReader> lr(new CStreamLineReader(fasta_file));
    
    typedef CFastaReader::EFlags TFlags;
    
    TFlags flags = (TFlags) (CFastaReader::fAllSeqIds |
                             (m_IsProtein
                              ? CFastaReader::fAssumeProt
                              : CFastaReader::fAssumeNuc) |
                             CFastaReader::fForceType);
    
    CFastaReader fr(*lr, flags);
    
    while(! lr->AtEOF()) {
        CRef<CSeq_entry> entry = fr.ReadOneSeq();
        
        if (entry.Empty()) {
            break;
        }
        
        _ASSERT(entry->IsSeq());
        CConstRef<CBioseq> bs(& entry->GetSeq());
        
        if (m_Verbose)
            m_LogFile << "Adding bioseq from fasta; first id is: ";
        
        bool printed = false;
        
        if (bs->CanGetId()) {
            const list< CRef<CSeq_id> > & ids = bs->GetId();
            
            if (! ids.empty() && ids.front().NotEmpty()) {
                if (m_Verbose)
                    m_LogFile << ids.front()->AsFastaString() << endl;
                
                printed = true;
            }
        }
        
        if (! printed) {
            if (m_Verbose)
                m_LogFile << "< not found >" << endl;
        }
        
        x_EditAndAddBioseq(bs, 0); // FASTA has no linkouts or memberships.
        found = true;
        
        count++;
        
        if (debug_mode > 5) m_LogFile << "-- FASTA: Found sequence." << endl;
    }
    
    if (count) {
        double t = sw.Elapsed();
        
        m_LogFile << "Adding sequences from FASTA; added "
                  << count << " sequences in " << t << " seconds." << endl;
    }
    
    return found;
}

CBuildDatabase::CBuildDatabase(const string & dbname,
                               const string & title,
                               bool           is_protein,
                               ostream      * logfile)
    : m_IsProtein    (is_protein),
      m_KeepLinks    (false),
      m_KeepMbits    (false),
      m_LogFile      (*logfile),
      m_UseRemote    (true),
      m_DeflineCount (0),
      m_OIDCount     (0),
      m_Verbose      (false)
{
    m_LogFile << "\n\nBuilding a new DB, current time: "
              << CTime(CTime::eCurrent).AsString() << endl;
    
    m_LogFile << "New DB name:   " << dbname << endl;
    m_LogFile << "New DB title:  " << title << endl;
    m_LogFile << "Sequence type: "
              << (is_protein ? "Protein" : "Nucleotide") << endl;
    
    CWriteDB::ESeqType seqtype =
        (is_protein ? CWriteDB::eProtein : CWriteDB::eNucleotide);
    
    m_OutputDb.Reset(new CWriteDB(dbname,
                                  seqtype,
                                  title));
    
    // Standard 1 GB limit
    
    m_OutputDb->SetMaxFileSize(1000*1000*1000);
}

void CBuildDatabase::SetTaxids(CTaxIdSet & taxids)
{
    _ASSERT(m_Taxids.Empty());
    m_Taxids.Reset(& taxids);
}

void CBuildDatabase::SetMaskLetters(const string & letters)
{
    m_OutputDb->SetMaskedLetters(letters);
}

CScope & CBuildDatabase::x_GetScope()
{
    if (m_Scope.Empty()) {
        if (m_ObjMgr.Empty()) {
            m_ObjMgr.Reset(CObjectManager::GetInstance());
        }
        
        try {
            CRef<CReader> reader(new CId2Reader);
            reader->SetPreopenConnection(false);
            
            CGBDataLoader::RegisterInObjectManager(*m_ObjMgr,
                                                   reader,
                                                   CObjectManager::eDefault);
        } catch (const CException& e) {
            ERR_POST(Warning << e.GetMsg());
        }
        
        m_Scope.Reset(new CScope(*m_ObjMgr));
        
        // Add default loaders (GB loader in this demo) to the scope.
        m_Scope->AddDefaults();
    }
    
    return *m_Scope;
}

void CBuildDatabase::SetSourceDb(CRef<CSeqDBExpert> seqdb)
{
    m_LogFile << "Configured source DB: " << seqdb->GetDBNameList() << endl;
    m_LogFile << "Source DB has title:  " << seqdb->GetTitle() << endl;
    m_LogFile << "Source DB time stamp: " << seqdb->GetDate() << endl;
    m_SourceDb = seqdb;
}

void CBuildDatabase::SetSourceDb(const string & src_db_name)
{
    _ASSERT(src_db_name.size());
    CRef<CSeqDBExpert> src_db(new CSeqDBExpert(src_db_name,
                                               m_IsProtein
                                               ? CSeqDB::eProtein
                                               : CSeqDB::eNucleotide));
    
    SetSourceDb(src_db);
}

void CBuildDatabase::SetLinkouts(const TLinkoutMap & linkouts,
                                 bool                keep_links)
{
    m_LogFile << "Keep Linkouts: " << (keep_links ? "T" : "F") << endl;
    MapToLMBits(linkouts, m_Id2Links);
    m_KeepLinks = keep_links;
}

void CBuildDatabase::SetMembBits(const TLinkoutMap & membbits,
                                 bool                keep_mbits)
{
    m_LogFile << "Keep MBits: " << (keep_mbits ? "T" : "F") << endl;
    MapToLMBits(membbits, m_Id2Mbits);
    m_KeepMbits = keep_mbits;
}

void CBuildDatabase::SetKeepTaxIds(bool keep_taxids)
{
    m_LogFile << "Keep TaxIDs: " << (keep_taxids ? "T" : "F") << endl;
    m_KeepTaxIds = keep_taxids;
}

bool
CBuildDatabase::Build(const vector<string> & ids,
                      CNcbiIstream         * fasta_file)
{
    CStopWatch sw(CStopWatch::eStart);
    
    bool success = true;
    
    if (m_Taxids.Empty()) {
        m_Taxids.Reset(new CTaxIdSet);
    }
    
    // Resolve all ids to GIs, storing them in a GI list.
    
    CRef<CInputGiList> gi_list;
    
    if (! ids.empty()) {
        gi_list = x_ResolveGis(ids);
    }
    
    // Translate the GI list.
    
    if (gi_list.NotEmpty() && m_SourceDb.NotEmpty() &&
        (gi_list->GetNumGis() || gi_list->GetNumSeqIds())) {
        
        // The process of constructing a SeqDB object with a user GI
        // list causes translation of the User GI list, and is the
        // fastest way of performing such a translation in bulk.  It
        // is possible to iterate the list afterwards to determine
        // what subset of it that has been translated; non-translated
        // GIs will need to be fetched using a data loader.
        //
        // It is not necessary, however, to iterate the GI list to
        // find OIDs that correspond to the filtered DB; these can be
        // found using OID iteration over SeqDB, which produces a
        // better ordering inasmuch as the reads from the source
        // sequence data will be sequential on disk.
        
        CRef<CSeqDBExpert> filtered
            (new CSeqDBExpert(m_SourceDb->GetDBNameList(),
                              m_SourceDb->GetSequenceType(),
                              &* gi_list));
        
        m_SourceDb = filtered;
        
        // Add all local database sequences to the output DB.
        
        x_DupLocal();
        
        if (m_Verbose) {
            // Map oid to gi.
            map<int,int> seen_it;
            
            for(int i = 0; i < gi_list->GetNumGis(); i++) {
                int this_oid = gi_list->GetGiOid(i).oid;
                int this_gi = gi_list->GetGiOid(i).gi;
            
                if (this_oid != -1) {
                    if (seen_it.find(this_oid) == seen_it.end()) {
                        seen_it[this_oid] = this_gi;
                    } else {
                        m_LogFile << "GI " << this_gi
                                  << " is duplicate of GI "
                                  << seen_it[this_oid]
                                  << endl;
                    }
                }
            }
        }
    }
    
    if (gi_list.NotEmpty()) {
        if (m_UseRemote) {
            success = x_AddRemoteSequences(*gi_list);
        } else {
            success = x_ReportUnresolvedIds(*gi_list);
        }
    }
    
    // Add any fasta sequences as well.
    
    if (fasta_file) {
        bool rv = x_AddFastaSequences(*fasta_file);
        success = success && rv;
    }
    
    m_OutputDb->Close();
    
    vector<string> vols;
    vector<string> files;
    
    m_OutputDb->ListVolumes(vols);
    m_OutputDb->ListFiles(files);
    
    m_LogFile << endl;
    
    _ASSERT(vols.empty() == files.empty());
    
    if (vols.empty()) {
        m_LogFile << "No volumes were created because no sequences were found."
                  << endl;
        success = false;
    } else {
        ITERATE(vector<string>, iterv, vols) {
            m_LogFile << "volume: " << *iterv << endl;
        }
        
        m_LogFile << endl;
        ITERATE(vector<string>, iterf, files) {
            m_LogFile << "file: " << *iterf << endl;
        }
    }
    
    m_LogFile << endl;
    
    double t = sw.Elapsed();
    
    m_LogFile << "Total sequences stored: " << m_OIDCount << endl;
    m_LogFile << "Total deflines stored: " << m_DeflineCount << endl;
    
    m_LogFile << "Total time to build database: "
              << t << " seconds.\n" << endl;
    
    return success;
}

static void
s_SetDeflineBits(CBlast_def_line & defline,
                 TIdToBits       & bitmap,
                 bool              keep_old,
                 bool              is_memb,
                 vector<string>  & keys)
{
    bool found = false;
    int value = 0;
    
    ITERATE(vector<string>, key, keys) {
        if (! key->size())
            continue;
        
        TIdToBits::iterator item = bitmap.find(*key);
        
        if (item != bitmap.end()) {
            found = true;
            value |= item->second;
        }
    }
    
    if (found) {
        list<int> & linkv = (is_memb
                             ? defline.SetMemberships()
                             : defline.SetLinks());
            
        if (! keep_old) {
            linkv.clear();
        }
            
        if (linkv.empty()) {
            linkv.push_back(value);
        } else {
            linkv.front() |= value;
        }
    } else {
        if (! keep_old) {
            if (is_memb) {
                defline.ResetMemberships();
            } else {
                defline.ResetLinks();
            }
        }
    }
}

void CBuildDatabase::x_SetLinkAndMbit(CRef<CBlast_def_line_set> headers)
{
    vector<string> keys;
    
    NON_CONST_ITERATE(list< CRef<CBlast_def_line> >, iter, headers->Set()) {
        CBlast_def_line & defline = **iter;
        GetDeflineKeys(defline, keys);
        
        s_SetDeflineBits(defline, m_Id2Links, m_KeepLinks, false, keys);
        s_SetDeflineBits(defline, m_Id2Mbits, m_KeepMbits, true, keys);
    }
}

