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

#include <ncbi_pch.hpp>
#include "seqdbimpl.hpp"
#include <iostream>

BEGIN_NCBI_SCOPE

CSeqDBImpl::CSeqDBImpl(const string & db_name_list,
                       char           prot_nucl,
                       Uint4          oid_begin,
                       Uint4          oid_end,
                       bool           use_mmap)
    : m_DBNames      (db_name_list),
      m_Aliases      (db_name_list, prot_nucl, use_mmap),
      m_VolSet       (m_MemPool,
                      m_Aliases.GetVolumeNames(),
                      prot_nucl,
                      use_mmap),
      m_RestrictBegin(oid_begin),
      m_RestrictEnd  (oid_end)
{
    m_Aliases.SetMasks(m_VolSet);
    
    if ( m_VolSet.HasMask() ) {
        m_OIDList.Reset( new CSeqDBOIDList(m_VolSet, use_mmap) );
    }
    
    if ((oid_begin == oid_end) || (m_RestrictEnd > GetNumSeqs())) {
        m_RestrictEnd = GetNumSeqs();
    }
}

CSeqDBImpl::~CSeqDBImpl(void)
{
}

bool CSeqDBImpl::CheckOrFindOID(Uint4 & next_oid) const
{
    bool success = true;
    
    if (next_oid < m_RestrictBegin) {
        next_oid = m_RestrictBegin;
    }
    
    if (next_oid >= m_RestrictEnd) {
        success = false;
    }
    
    if (success && m_OIDList.NotEmpty()) {
        success = m_OIDList->CheckOrFindOID(next_oid);
        
        if (next_oid > m_RestrictEnd) {
            success = false;
        }
    }
    
    return success;
}

Uint4 CSeqDBImpl::GetSeqLength(Uint4 oid) const
{
    Uint4 vol_oid = 0;
    
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetSeqLength(vol_oid, false);
    }
    
    NCBI_THROW(CSeqDBException,
               eArgErr,
               "OID not in valid range.");
}

Uint4 CSeqDBImpl::GetSeqLengthApprox(Uint4 oid) const
{
    Uint4 vol_oid = 0;
    
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetSeqLength(vol_oid, true);
    }
    
    NCBI_THROW(CSeqDBException,
               eArgErr,
               "OID not in valid range.");
}

CRef<CBioseq>
CSeqDBImpl::GetBioseq(Uint4 oid,
                      bool  use_objmgr,
                      bool  insert_ctrlA) const
{
    Uint4 vol_oid = 0;
    
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetBioseq(vol_oid, use_objmgr, insert_ctrlA);
    }
    
    NCBI_THROW(CSeqDBException,
               eArgErr,
               "OID not in valid range.");
}

void CSeqDBImpl::RetSequence(const char ** buffer) const
{
    m_MemPool.Free((void*) *buffer);
}

Uint4 CSeqDBImpl::GetSequence(Uint4 oid, const char ** buffer) const
{
    Uint4 vol_oid = 0;
    
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetSequence(vol_oid, buffer);
    }
    
    NCBI_THROW(CSeqDBException,
               eArgErr,
               "OID not in valid range.");
}

Uint4 CSeqDBImpl::GetAmbigSeq(Uint4           oid,
                              char         ** buffer,
                              Uint4           nucl_code,
                              ESeqDBAllocType alloc_type) const
{
    Uint4 vol_oid = 0;
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetAmbigSeq(vol_oid, buffer, nucl_code, alloc_type);
    }
    
    NCBI_THROW(CSeqDBException,
               eArgErr,
               "OID not in valid range.");
}

list< CRef<CSeq_id> > CSeqDBImpl::GetSeqIDs(Uint4 oid) const
{
    Uint4 vol_oid = 0;
    
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetSeqIDs(vol_oid);
    }
    
    NCBI_THROW(CSeqDBException,
               eArgErr,
               "OID not in valid range.");
}

Uint4 CSeqDBImpl::GetNumSeqs(void) const
{
    return m_Aliases.GetNumSeqs(m_VolSet);
}

Uint8 CSeqDBImpl::GetTotalLength(void) const
{
    return m_Aliases.GetTotalLength(m_VolSet);
}

string CSeqDBImpl::GetTitle(void) const
{
    return x_FixString( m_Aliases.GetTitle(m_VolSet) );
}

char CSeqDBImpl::GetSeqType(void) const
{
    if (const CSeqDBVol * vol = m_VolSet.GetVol(0)) {
        return vol->GetSeqType();
    }
    return kSeqTypeUnkn;
}

string CSeqDBImpl::GetDate(void) const
{
    if (const CSeqDBVol * vol = m_VolSet.GetVol(0)) {
        return x_FixString( vol->GetDate() );
    }
    return string();
}

CRef<CBlast_def_line_set> CSeqDBImpl::GetHdr(Uint4 oid) const
{
    Uint4 vol_oid = 0;
    
    if (const CSeqDBVol * vol = m_VolSet.FindVol(oid, vol_oid)) {
        return vol->GetHdr(vol_oid);
    }
    
    NCBI_THROW(CSeqDBException,
               eArgErr,
               "OID not in valid range.");
}

Uint4 CSeqDBImpl::GetMaxLength(void) const
{
    Uint4 max_len = 0;
    
    for(Uint4 i = 0; i < m_VolSet.GetNumVols(); i++) {
        Uint4 new_max = m_VolSet.GetVol(i)->GetMaxLength();
        
        if (new_max > max_len)
            max_len = new_max;
    }
    
    return max_len;
}

const string & CSeqDBImpl::GetDBNameList(void) const
{
    return m_DBNames;
}

// This is a work-around for bad data in the database; probably the
// fault of formatdb.  The problem is that the database date field has
// an incorrect length - possibly a general problem with string
// handling in formatdb?  In any case, this method trims a string to
// the minimum of its length and the position of the first NULL.  This
// technique will not work if the date field is null terminated, but
// apparently it usually is, in spite of the length bug.

string CSeqDBImpl::x_FixString(const string & s) const
{
    for(Uint4 i = 0; i < s.size(); i++) {
        if (s[i] == char(0)) {
            return string(s,0,i);
        }
    }
    return s;
}

END_NCBI_SCOPE

