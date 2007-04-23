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
 */

/// @file seqdb.cpp
/// Implementation for the CSeqDB class, the top level class for SeqDB.

#include <ncbi_pch.hpp>
#include <objtools/readers/seqdb/seqdb.hpp>
#include <util/sequtil/sequtil_convert.hpp>
#include "seqdbimpl.hpp"

BEGIN_NCBI_SCOPE

/// Helper function to translate enumerated type to character.
///
/// @param seqtype
///   The sequence type (eProtein, eNucleotide, or eUnknown).
/// @return
///   The sequence type as a char ('p', 'n', or '-').

static char s_GetSeqTypeChar(CSeqDB::ESeqType seqtype)
{
    switch(seqtype) {
    case CSeqDB::eProtein:
        return 'p';
    case CSeqDB::eNucleotide:
        return 'n';
    case CSeqDB::eUnknown:
        return '-';
    }
    
    NCBI_THROW(CSeqDBException,
               eArgErr,
               "Invalid sequence type specified.");
}

/// Helper function to build private implementation object.
///
/// This method builds and returns the object which implements the
/// functionality for the CSeqDB API.  If this method is called with
/// '-' for the sequence data type, protein will be tried first, then
/// nucleotide.  The created object will be returned.  Either
/// kSeqTypeProt for a protein database, kSeqTypeNucl for nucleotide,
/// or kSeqTypeUnkn to less this function try one then the other.
/// 
/// @param dbname
///   A list of database or alias names, seperated by spaces.
/// @param prot_nucl
///   Specify whether to use protein, nucleotide, or either.
/// @param oid_begin
///   Iterator will skip OIDs less than this value.  Only OIDs
///   found in the OID lists (if any) will be returned.
/// @param oid_end
///   Iterator will return up to (but not including) this OID.
/// @param use_mmap
///   If kSeqDBMMap is specified (the default), memory mapping is
///   attempted.  If kSeqDBNoMMap is specified, or memory mapping
///   fails, this platform does not support it, the less efficient
///   read and write calls are used instead.
/// @param gi_list
///   If non-null, this GI list will be used to filter the deflines
///   and OIDs.
/// @return
///   The CSeqDBImpl object that was created.

static CSeqDBImpl *
s_SeqDBInit(const string & dbname,
            char           prot_nucl,
            int            oid_begin,
            int            oid_end,
            bool           use_mmap,
            CSeqDBGiList * gi_list)
{
    CSeqDBImpl * impl = 0;
    
    if (prot_nucl == '-') {
        try {
            prot_nucl = 'p';
            impl = new CSeqDBImpl(dbname,
                                  prot_nucl,
                                  oid_begin,
                                  oid_end,
                                  use_mmap,
                                  gi_list);
        }
        catch(CSeqDBException &) {
            prot_nucl = 'n';
        }
    }
    
    if (! impl) {
        impl = new CSeqDBImpl(dbname,
                              prot_nucl,
                              oid_begin,
                              oid_end,
                              use_mmap,
                              gi_list);
    }
    
    _ASSERT(impl);
    
    return impl;
}

CSeqDB::CSeqDB(const string & dbname, ESeqType seqtype, CSeqDBGiList * gi_list)
{
    if (dbname.size() == 0) {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "Database name is required.");
    }
    
    m_Impl = s_SeqDBInit(dbname,
                         s_GetSeqTypeChar(seqtype),
                         0,
                         0,
                         true,
                         gi_list);
    
    m_Impl->Verify();
}

CSeqDB::CSeqDB(const vector<string> & dbs,
               ESeqType               seqtype,
               CSeqDBGiList         * gi_list)
{
    string dbname;
    SeqDB_CombineAndQuote(dbs, dbname);
    
    if (dbname.size() == 0) {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "Database name is required.");
    }
    
    m_Impl = s_SeqDBInit(dbname,
                         s_GetSeqTypeChar(seqtype),
                         0,
                         0,
                         true,
                         gi_list);
    
    m_Impl->Verify();
}

CSeqDB::CSeqDB(const string & dbname,
               ESeqType       seqtype,
               int            oid_begin,
               int            oid_end,
               bool           use_mmap,
               CSeqDBGiList * gi_list)
{
    if (dbname.size() == 0) {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "Database name is required.");
    }
    
    m_Impl = s_SeqDBInit(dbname,
                         s_GetSeqTypeChar(seqtype),
                         oid_begin,
                         oid_end,
                         use_mmap,
                         gi_list);
    
    m_Impl->Verify();
}

CSeqDB::CSeqDB(const vector<string> & dbs,
               ESeqType               seqtype,
               int                    oid_begin,
               int                    oid_end,
               bool                   use_mmap,
               CSeqDBGiList         * gi_list)
{
    string dbname;
    SeqDB_CombineAndQuote(dbs, dbname);
    
    if (dbname.size() == 0) {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "Database name is required.");
    }
    
    m_Impl = s_SeqDBInit(dbname,
                         s_GetSeqTypeChar(seqtype),
                         oid_begin,
                         oid_end,
                         use_mmap,
                         gi_list);
    
    m_Impl->Verify();
}

CSeqDB::CSeqDB()
{
    m_Impl = new CSeqDBImpl;
    m_Impl->Verify();
}

int CSeqDB::GetSeqLength(int oid) const
{
    m_Impl->Verify();
    int length = m_Impl->GetSeqLength(oid);
    m_Impl->Verify();
    
    return length;
}

int CSeqDB::GetSeqLengthApprox(int oid) const
{
    m_Impl->Verify();
    int length = m_Impl->GetSeqLengthApprox(oid);
    m_Impl->Verify();
    
    return length;
}

CRef<CBlast_def_line_set> CSeqDB::GetHdr(int oid) const
{
    m_Impl->Verify();
    CRef<CBlast_def_line_set> rv =m_Impl->GetHdr(oid);
    m_Impl->Verify();
    
    return rv;
}

CSeqDB::ESeqType CSeqDB::GetSequenceType() const
{
    switch(m_Impl->GetSeqType()) {
    case 'p':
        return eProtein;
    case 'n':
        return eNucleotide;
    }
    
    NCBI_THROW(CSeqDBException,
               eArgErr,
               "Internal sequence type is not valid.");
}

CRef<CBioseq>
CSeqDB::GetBioseq(int oid) const
{
    m_Impl->Verify();
    CRef<CBioseq> rv = m_Impl->GetBioseq(oid, 0, true);
    m_Impl->Verify();
    
    return rv;
}

CRef<CBioseq>
CSeqDB::GetBioseqNoData(int oid, int target_gi) const
{
    m_Impl->Verify();
    CRef<CBioseq> rv = m_Impl->GetBioseq(oid, target_gi, false);
    m_Impl->Verify();
    
    return rv;
}

void CSeqDB::GetTaxIDs(int             oid,
                       map<int, int> & gi_to_taxid,
                       bool            persist) const
{
    m_Impl->Verify();
    m_Impl->GetTaxIDs(oid, gi_to_taxid, persist);
    m_Impl->Verify();
}

void CSeqDB::GetTaxIDs(int           oid,
                       vector<int> & taxids,
                       bool          persist) const
{
    m_Impl->Verify();
    m_Impl->GetTaxIDs(oid, taxids, persist);
    m_Impl->Verify();
}

CRef<CBioseq>
CSeqDB::GetBioseq(int oid, int target_gi) const
{
    m_Impl->Verify();
    CRef<CBioseq> rv = m_Impl->GetBioseq(oid, target_gi, true);
    m_Impl->Verify();
    
    return rv;
}

void CSeqDB::RetSequence(const char ** buffer) const
{
    m_Impl->Verify();
    m_Impl->RetSequence(buffer);
    m_Impl->Verify();
}

int CSeqDB::GetSequence(int oid, const char ** buffer) const
{
    m_Impl->Verify();
    int rv = m_Impl->GetSequence(oid, buffer);
    m_Impl->Verify();
    
    return rv;
}

CRef<CSeq_data> CSeqDB::GetSeqData(int     oid,
                                   TSeqPos begin,
                                   TSeqPos end) const
{
    m_Impl->Verify();
    CRef<CSeq_data> rv = m_Impl->GetSeqData(oid, begin, end);
    m_Impl->Verify();
    
    return rv;
}

int CSeqDB::GetAmbigSeq(int oid, const char ** buffer, int nucl_code) const
{
    m_Impl->Verify();
    int rv = m_Impl->GetAmbigSeq(oid,
                                 (char **)buffer,
                                 nucl_code,
                                 0,
                                 (ESeqDBAllocType) 0);
    m_Impl->Verify();
    
    return rv;
}

int CSeqDB::GetAmbigSeq(int           oid,
                        const char ** buffer,
                        int           nucl_code,
                        int           begin_offset,
                        int           end_offset) const
{
    m_Impl->Verify();
    
    SSeqDBSlice region(begin_offset, end_offset);
    
    int rv = m_Impl->GetAmbigSeq(oid,
                                 (char **)buffer,
                                 nucl_code,
                                 & region,
                                 (ESeqDBAllocType) 0);
    m_Impl->Verify();
    
    return rv;
}

int CSeqDB::GetAmbigSeqAlloc(int             oid,
                             char         ** buffer,
                             int             nucl_code,
                             ESeqDBAllocType strategy) const
{
    m_Impl->Verify();
    
    if ((strategy != eMalloc) && (strategy != eNew)) {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "Invalid allocation strategy specified.");
    }
    
    int rv = m_Impl->GetAmbigSeq(oid, buffer, nucl_code, 0, strategy);
    
    m_Impl->Verify();
    
    return rv;
}

string CSeqDB::GetTitle() const
{
    return m_Impl->GetTitle();
}

string CSeqDB::GetDate() const
{
    return m_Impl->GetDate();
}

int CSeqDB::GetNumSeqs() const
{
    return m_Impl->GetNumSeqs();
}

int CSeqDB::GetNumOIDs() const
{
    return m_Impl->GetNumOIDs();
}

Uint8 CSeqDB::GetTotalLength() const
{
    return m_Impl->GetTotalLength();
}

Uint8 CSeqDB::GetVolumeLength() const
{
    return m_Impl->GetVolumeLength();
}

int CSeqDB::GetMaxLength() const
{
    return m_Impl->GetMaxLength();
}

CSeqDB::~CSeqDB()
{
    m_Impl->Verify();
    
    if (m_Impl)
        delete m_Impl;
}

CSeqDBIter CSeqDB::Begin() const
{
    return CSeqDBIter(this, 0);
}

bool CSeqDB::CheckOrFindOID(int & oid) const
{
    m_Impl->Verify();
    bool rv = m_Impl->CheckOrFindOID(oid);
    m_Impl->Verify();
    
    return rv;
}


CSeqDB::EOidListType
CSeqDB::GetNextOIDChunk(int         & begin,
                        int         & end,
                        vector<int> & lst,
                        int         * state)
{
    m_Impl->Verify();
    
    CSeqDB::EOidListType rv =
        m_Impl->GetNextOIDChunk(begin, end, lst, state);
    
    m_Impl->Verify();
    
    return rv;
}

void CSeqDB::ResetInternalChunkBookmark()
{
    m_Impl->ResetInternalChunkBookmark();
}

const string & CSeqDB::GetDBNameList() const
{
    return m_Impl->GetDBNameList();
}

list< CRef<CSeq_id> > CSeqDB::GetSeqIDs(int oid) const
{
    m_Impl->Verify();
    
    list< CRef<CSeq_id> > rv = m_Impl->GetSeqIDs(oid);
    
    m_Impl->Verify();
    
    return rv;
}

bool CSeqDB::PigToOid(int pig, int & oid) const
{
    m_Impl->Verify();
    bool rv = m_Impl->PigToOid(pig, oid);
    m_Impl->Verify();
    
    return rv;
}

bool CSeqDB::OidToPig(int oid, int & pig) const
{
    m_Impl->Verify();
    bool rv = m_Impl->OidToPig(oid, pig);
    m_Impl->Verify();
    
    return rv;
}

bool CSeqDB::GiToOid(int gi, int & oid) const
{
    m_Impl->Verify();
    bool rv = m_Impl->GiToOid(gi, oid);
    m_Impl->Verify();
    
    return rv;
}

bool CSeqDB::OidToGi(int oid, int & gi) const
{
    m_Impl->Verify();
    bool rv = m_Impl->OidToGi(oid, gi);
    m_Impl->Verify();
    
    return rv;
}

bool CSeqDB::PigToGi(int pig, int & gi) const
{
    m_Impl->Verify();
    bool rv = false;
    
    int oid(0);
    
    if (m_Impl->PigToOid(pig, oid)) {
        rv = m_Impl->OidToGi(oid, gi);
    }
    m_Impl->Verify();
    
    return rv;
}

bool CSeqDB::GiToPig(int gi, int & pig) const
{
    m_Impl->Verify();
    bool rv = false;
    
    int oid(0);
    
    if (m_Impl->GiToOid(gi, oid)) {
        rv = m_Impl->OidToPig(oid, pig);
    }
    
    m_Impl->Verify();
    
    return rv;
}

void CSeqDB::AccessionToOids(const string & acc, vector<int> & oids) const
{
    m_Impl->Verify();
    m_Impl->AccessionToOids(acc, oids);
    m_Impl->Verify();
}

void CSeqDB::SeqidToOids(const CSeq_id & seqid, vector<int> & oids) const
{
    m_Impl->Verify();
    m_Impl->SeqidToOids(seqid, oids, true);
    m_Impl->Verify();
}

bool CSeqDB::SeqidToOid(const CSeq_id & seqid, int & oid) const
{
    m_Impl->Verify();
    bool rv = false;
    
    oid = -1;
    
    vector<int> oids;
    m_Impl->SeqidToOids(seqid, oids, false);
    
    if (! oids.empty()) {
        rv = true;
        oid = oids[0];
    }
    
    m_Impl->Verify();
    
    return rv;
}

void CSeqDB::SetMemoryBound(Uint8 membound, Uint8 slice_size)
{
    m_Impl->SetMemoryBound(membound);
}

int CSeqDB::GetOidAtOffset(int first_seq, Uint8 residue) const
{
    m_Impl->Verify();
    int rv = m_Impl->GetOidAtOffset(first_seq, residue);
    m_Impl->Verify();
    
    return rv;
}

CSeqDBIter::CSeqDBIter(const CSeqDB * db, int oid)
    : m_DB    (db),
      m_OID   (oid),
      m_Data  (0),
      m_Length((int) -1)
{
    if (m_DB->CheckOrFindOID(m_OID)) {
        x_GetSeq();
    }
}

CSeqDBIter::CSeqDBIter(const CSeqDBIter & other)
    : m_DB    (other.m_DB),
      m_OID   (other.m_OID),
      m_Data  (0),
      m_Length((int) -1)
{
    if (m_DB->CheckOrFindOID(m_OID)) {
        x_GetSeq();
    }
}

/// Copy one iterator to another.
CSeqDBIter & CSeqDBIter::operator =(const CSeqDBIter & other)
{
    x_RetSeq();
    
    m_DB = other.m_DB;
    m_OID = other.m_OID;
    m_Data = 0;
    m_Length = -1;
    
    if (m_DB->CheckOrFindOID(m_OID)) {
        x_GetSeq();
    }
    
    return *this;
}

CSeqDBIter & CSeqDBIter::operator++()
{
    x_RetSeq();
    
    ++m_OID;
    
    if (m_DB->CheckOrFindOID(m_OID)) {
        x_GetSeq();
    } else {
        m_Length = -1;
    }
    
    return *this;
}

CRef<CBioseq>
CSeqDB::GiToBioseq(int gi) const
{
    m_Impl->Verify();
    
    CRef<CBioseq> bs;
    int oid(0);
    
    if (m_Impl->GiToOid(gi, oid)) {
        bs = m_Impl->GetBioseq(oid, gi, true);
    }
    
    m_Impl->Verify();
    
    return bs;
}

CRef<CBioseq>
CSeqDB::PigToBioseq(int pig) const
{
    m_Impl->Verify();
    
    int oid(0);
    CRef<CBioseq> bs;
    
    if (m_Impl->PigToOid(pig, oid)) {
        bs = m_Impl->GetBioseq(oid, 0, true);
    }
    
    m_Impl->Verify();
    
    return bs;
}

CRef<CBioseq>
CSeqDB::SeqidToBioseq(const CSeq_id & seqid) const
{
    m_Impl->Verify();
    
    vector<int> oids;
    CRef<CBioseq> bs;
    
    m_Impl->SeqidToOids(seqid, oids, false);
    
    if (! oids.empty()) {
        bs = m_Impl->GetBioseq(oids[0], 0, true);
    }
    
    m_Impl->Verify();
    
    return bs;
}

void
CSeqDB::FindVolumePaths(const string   & dbname,
                        ESeqType         seqtype,
                        vector<string> & paths)
{
    bool done = false;
    
    if (seqtype == CSeqDB::eProtein) {
        CSeqDBImpl::FindVolumePaths(dbname, 'p', paths);
    } else if (seqtype == CSeqDB::eNucleotide) {
        CSeqDBImpl::FindVolumePaths(dbname, 'n', paths);
    } else {
        try {
            CSeqDBImpl::FindVolumePaths(dbname, 'p', paths);
            done = true;
        }
        catch(...) {
            done = false;
        }
        
        if (! done) {
            CSeqDBImpl::FindVolumePaths(dbname, 'n', paths);
        }
    }
}

void
CSeqDB::FindVolumePaths(vector<string> & paths) const
{
    m_Impl->Verify();
    m_Impl->FindVolumePaths(paths);
    m_Impl->Verify();
}

void
CSeqDB::GetGis(int oid, vector<int> & gis, bool append) const
{
    m_Impl->Verify();
    
    // This could be done a little faster at a lower level, but not
    // necessarily by too much.  If this operation is important to
    // performance, that decision can be revisited.
    
    list< CRef<CSeq_id> > seqids = GetSeqIDs(oid);
    
    if (! append) {
        gis.clear();
    }
    
    ITERATE(list< CRef<CSeq_id> >, seqid, seqids) {
        if ((**seqid).IsGi()) {
            gis.push_back((**seqid).GetGi());
        }
    }
    
    m_Impl->Verify();
}

void CSeqDB::SetIterationRange(int oid_begin, int oid_end)
{
    m_Impl->SetIterationRange(oid_begin, oid_end);
}

void CSeqDB::GetAliasFileValues(TAliasFileValues & afv)
{
    m_Impl->Verify();
    m_Impl->GetAliasFileValues(afv);
    m_Impl->Verify();
}

void CSeqDB::GetTaxInfo(int taxid, SSeqDBTaxInfo & info) const
{
    m_Impl->Verify();
    m_Impl->GetTaxInfo(taxid, info);
    m_Impl->Verify();
}

void CSeqDB::GetTotals(ESummaryType   sumtype,
                       int          * oid_count,
                       Uint8        * total_length,
                       bool           use_approx) const
{
    m_Impl->Verify();
    m_Impl->GetTotals(sumtype, oid_count, total_length, use_approx);
    m_Impl->Verify();
}

const CSeqDBGiList * CSeqDB::GetGiList() const
{
    return m_Impl->GetGiList();
}

void CSeqDB::SetDefaultMemoryBound(Uint8 bytes)
{
    CSeqDBImpl::SetDefaultMemoryBound(bytes);
}

void CSeqDB::GetSequenceAsString(int      oid,
                                 string & output) const
{
    CSeqUtil::ECoding code_to = ((GetSequenceType() == CSeqDB::eProtein)
                                 ? CSeqUtil::e_Iupacaa
                                 : CSeqUtil::e_Iupacna);
    
    GetSequenceAsString(oid, code_to, output);
}

void CSeqDB::GetSequenceAsString(int                 oid,
                                 CSeqUtil::ECoding   coding,
                                 string            & output) const
{
    output.clear();

    string raw;
    const char * buffer = 0;

    // Protein dbs ignore encodings, always returning ncbistdaa.
    int length = GetAmbigSeq(oid, & buffer, kSeqDBNuclNcbiNA8);

    try {
        raw.assign(buffer, length);
    }
    catch(...) {
        RetSequence(& buffer);
        throw;
    }
    RetSequence(& buffer);
    
    CSeqUtil::ECoding code_from = ((GetSequenceType() == CSeqDB::eProtein)
                                   ? CSeqUtil::e_Ncbistdaa
                                   : CSeqUtil::e_Ncbi8na);
    
    string result;
    
    if (code_from == coding) {
        result.swap(raw);
    } else {
        CSeqConvert::Convert(raw,
                             code_from,
                             0,
                             length,
                             result,
                             coding);
    }
    
    output.swap(result);
}

END_NCBI_SCOPE

