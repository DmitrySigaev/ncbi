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
#include "seqdbfile.hpp"

BEGIN_NCBI_SCOPE

/// Index file.
///
/// Index files (extension nin or pin) contain information on where to
/// find information in other files.  The OID is the (implied) key.


// A Word About Mutexes and Mutability in the File Classes
//
// The stream object in CSeqDBRawFile is mutable: this is because the
// stream access methods modify the file.  Specifically, they modify
// the file offset.  This means that two users of a stream object will
// step on each other if they try to read from different offsets
// concurrently.  Memory mapping does not have this problem of course.
//
// To fix this, the file object is mutable, but to access it, the user
// needs to hold the m_FileLock mutex.
//
// One goal I have for these classes is to eliminate all locking for
// the mmap case.  Locking is not needed to call a const method, so
// methods are marked const whenever possible.  After construction of
// CSeqDB, ONLY const methods are called.
//
// Some of the const methods need to modify fields; to do this, I mark
// the fields 'mutable' and hold a mutex whenever accessing them.
//
// Each method falls into one of these categories:
//
// 1. Non-const: called only during CSeqDB construction.
// 2. Const: no changes to any fields.
// 3. Const: modifies mutable fields while holding m_FileLock.

Uint8 BytesToUint8(char * bytes_sc)
{
    unsigned char * bytes = (unsigned char *) bytes_sc;
    Uint8 value;
    Int4 i;

    value = 0;
    for(i = 7; i >= 0; i--) {
        value += bytes[i];
        if(i) value <<= 8;
    }
    
    return value;
}


Uint8 CSeqDBRawFile::ReadSwapped(CSeqDBMemLease & lease, Uint8 offset, Uint4 * buf, CSeqDBLockHold & locked) const
{
    m_Atlas.Lock(locked);
    
    if (! lease.Contains(offset, offset + sizeof(*buf))) {
        m_Atlas.GetRegion(lease, m_FileName, offset, offset + sizeof(*buf));
    }
    
    *buf = SeqDB_GetStdOrd((Uint4 *) lease.GetPtr(offset));
    
    return offset + sizeof(*buf);
}

Uint8 CSeqDBRawFile::ReadSwapped(CSeqDBMemLease & lease,
                                 Uint8            offset,
                                 Uint8          * buf,
                                 CSeqDBLockHold & locked) const
{
    m_Atlas.Lock(locked);
    
    if (! lease.Contains(offset, offset + sizeof(*buf))) {
        m_Atlas.GetRegion(lease, m_FileName, offset, offset + sizeof(*buf));
    }
    
    *buf = SeqDB_GetBroken((Int8 *) lease.GetPtr(offset));
    
    return offset + sizeof(*buf);
}

Uint8 CSeqDBRawFile::ReadSwapped(CSeqDBMemLease & lease,
                                 Uint8            offset,
                                 string         * v,
                                 CSeqDBLockHold & locked) const
{
    Uint4 len = 0;
    
    m_Atlas.Lock(locked);
    
    if (! lease.Contains(offset, offset + sizeof(len))) {
        m_Atlas.GetRegion(lease, m_FileName, offset, offset + sizeof(len));
    }
    
    len = SeqDB_GetStdOrd((Int4 *) lease.GetPtr(offset));
    
    offset += sizeof(len);
    
    if (! lease.Contains(offset, offset + len)) {
        m_Atlas.GetRegion(lease, m_FileName, offset, offset + sizeof(len));
    }
    
    v->assign(lease.GetPtr(offset), (int) len);
    
    return offset + len;
}


// Does not modify (or use) internal file offset

bool CSeqDBRawFile::ReadBytes(CSeqDBMemLease & lease,
                              char           * buf,
                              Uint8            start,
                              Uint8            end,
                              CSeqDBLockHold & locked) const
{
    m_Atlas.Lock(locked);
    
    if (! lease.Contains(start, end)) {
        m_Atlas.GetRegion(lease, m_FileName, start, end);
    }
    
    memcpy(buf, lease.GetPtr(start), end-start);
    
    return true;
}

CSeqDBExtFile::CSeqDBExtFile(CSeqDBAtlas   & atlas,
                             const string  & dbfilename,
                             char            prot_nucl)
    : m_Atlas   (atlas),
      m_Lease   (atlas),
      m_FileName(dbfilename),
      m_File    (atlas)
{
    if ((prot_nucl != kSeqTypeProt) && (prot_nucl != kSeqTypeNucl)) {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "Error: Invalid sequence type requested.");
    }
    
    x_SetFileType(prot_nucl);
    
    if (! m_File.Open(m_FileName)) {
        string msg = string("Error: File (") + m_FileName + ") not found.";
        
        NCBI_THROW(CSeqDBException, eFileErr, msg);
    }
}

CSeqDBIdxFile::CSeqDBIdxFile(CSeqDBAtlas    & atlas,
                             const string   & dbname,
                             char             prot_nucl)
    : CSeqDBExtFile(atlas, dbname + ".-in", prot_nucl),
      m_NumSeqs       (0),
      m_TotLen        (0),
      m_MaxLen        (0),
      m_HdrRegion     (0),
      m_SeqRegion     (0),
      m_AmbRegion     (0)
{
    // Input validation
    
    _ASSERT(! dbname.empty());
    
    if ((prot_nucl != kSeqTypeProt) && (prot_nucl != kSeqTypeNucl)) {
        NCBI_THROW(CSeqDBException,
                   eArgErr,
                   "Error: Invalid sequence type requested.");
    }
    
    Uint8 offset = 0;
    
    Uint4 f_format_version = 0;
    Uint4 f_db_seqtype = 0;
    
    CSeqDBMemLease lease(m_Atlas);
    CSeqDBLockHold locked(m_Atlas);
    
    offset = x_ReadSwapped(lease, offset, & f_format_version, locked);
    
    if (f_format_version != 4) {
        NCBI_THROW(CSeqDBException,
                   eFileErr,
                   "Error: Not a valid version 4 database.");
    }
    
    offset = x_ReadSwapped(lease, offset, & f_db_seqtype, locked);
    offset = x_ReadSwapped(lease, offset, & m_Title,      locked);
    offset = x_ReadSwapped(lease, offset, & m_Date,       locked);
    offset = x_ReadSwapped(lease, offset, & m_NumSeqs,    locked);
    offset = x_ReadSwapped(lease, offset, & m_TotLen,     locked);
    offset = x_ReadSwapped(lease, offset, & m_MaxLen,     locked);
    
    Uint8 region_bytes = 4 * (m_NumSeqs + 1);
    
    Uint8 off1, off2, off3, offend;
    
    off1   = offset;
    off2   = off1 + region_bytes;
    off3   = off2 + region_bytes;
    offend = off3 + region_bytes;
    
    // This could still be done this way, for example, by keeping
    // leases on the parts of the index file.  If this was done a
    // little effort may be needed to insure proper destruction order.
    // For now, I will keep offsets and get the lease each time it is
    // needed.  The atlas will still contain all the pieces.
    
    char db_seqtype = ((f_db_seqtype == 1)
                       ? kSeqTypeProt
                       : kSeqTypeNucl);
    
    m_Atlas.RetRegion(lease);
    
    if (db_seqtype != x_GetSeqType()) {
        NCBI_THROW(CSeqDBException,
                   eFileErr,
                   "Error: requested sequence type does not match DB.");
    }
    
    m_HdrRegion = (Uint4*) m_Atlas.GetRegion(m_FileName, off1, off2, locked);
    m_SeqRegion = (Uint4*) m_Atlas.GetRegion(m_FileName, off2, off3, locked);
    m_AmbRegion = (Uint4*) m_Atlas.GetRegion(m_FileName, off3, offend, locked);
}

END_NCBI_SCOPE


