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

/// @file writedb_isam.cpp
/// Implementation for the CWriteDB_Isam class.
/// class for WriteDB.

#include <ncbi_pch.hpp>
#include <objtools/writers/writedb/writedb_error.hpp>
#include "writedb_isam.hpp"
#include "writedb_convert.hpp"
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/serial.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/general/general__.hpp>
#include <iostream>
#include <sstream>

BEGIN_NCBI_SCOPE

/// Import C++ std namespace.
USING_SCOPE(std);

/// Compute the file extension for an ISAM file.
///
/// @param itype The type of ID data stored in the file.
/// @param protein True if the database type is protein.
/// @param is_index True for the index file (i.e. pni).
/// @return The three letter extension as a string.
static string
s_IsamExtension(EWriteDBIsamType itype,
                bool             protein,
                bool             is_index)
{
    char type_ch = '?';
    
    switch(itype) {
    case ePig:
        type_ch = 'p';
        break;
        
    case eGi:
        type_ch = 'n';
        break;
        
    case eAcc:
        type_ch = 's';
        break;
        
    default:
        NCBI_THROW(CWriteDBException, eArgErr, "Not implemented.");
    }
    
    string extn("???");
    extn[0] = protein ? 'p' : 'n';
    extn[1] = type_ch;
    extn[2] = is_index ? 'i' : 'd';
    
    return extn;
}

CWriteDB_Isam::CWriteDB_Isam(EIsamType      itype,
                             const string & dbname,
                             bool           protein,
                             int            index,
                             Uint8          max_file_size,
                             bool           sparse)
{
    m_DFile.Reset(new CWriteDB_IsamData(itype,
                                        dbname,
                                        protein,
                                        index,
                                        max_file_size));
    
    m_IFile.Reset(new CWriteDB_IsamIndex(itype,
                                         dbname,
                                         protein,
                                         index,
                                         m_DFile,
                                         sparse));
}

CWriteDB_Isam::~CWriteDB_Isam()
{
}

bool CWriteDB_Isam::CanFit(int num)
{
    return m_IFile->CanFit(num);
}

void CWriteDB_Isam::AddIds(int oid, const TIdList & idlist)
{
    m_IFile->AddIds(oid, idlist);
}

void CWriteDB_Isam::AddPig(int oid, int pig)
{
    m_IFile->AddPig(oid, pig);
}

void CWriteDB_Isam::Close()
{
    // Index must be closed first.
    m_IFile->Close();
    m_DFile->Close();
}

void CWriteDB_Isam::RenameSingle()
{
    m_IFile->RenameSingle();
    m_DFile->RenameSingle();
}

CWriteDB_IsamIndex::CWriteDB_IsamIndex(EWriteDBIsamType        itype,
                                       const string          & dbname,
                                       bool                    protein,
                                       int                     index,
                                       CRef<CWriteDB_IsamData> datafile,
                                       bool                    sparse)
    : CWriteDB_File  (dbname,
                      s_IsamExtension(itype, protein, true),
                      index,
                      0),
      m_Type         (itype),
      m_Sparse       (sparse),
      m_PageSize     (0),
      m_BytesPerElem (0),
      m_DataFileSize (0),
      m_DataFile     (datafile)
{
    // This is the one case where I don't worry about file size; if
    // the data file can hold the relevant data, the index file can
    // too.  The index file can be larger than the data file, but only
    // if there are less than (about) 9 entries; at that size I'm not
    // concerned about the byte limits.
    
    if (itype == eAcc) {
        // If there is a maximum string size it's something large like
        // 4k per table line.  In practice, string table rows tend to
        // be less than 50 characters, with the median probably around
        // 15-20.  There is probably little harm in overestimating
        // this number so long as the overestimate is small relative
        // to the max file size.
        //
        // String indices normally have several versions of each key;
        // the number below represents 16 versions * 64 bytes per row.
        // This is far more than any current sequence currently uses.
        // If this is not enough, the max size limit may be violated
        // (but see the last sentence of paragraph 1).
        
        m_BytesPerElem = 1024;
        m_PageSize = 64;
    } else {
        m_BytesPerElem = 8;
        m_PageSize = 256;
    }
}

CWriteDB_IsamIndex::~CWriteDB_IsamIndex()
{
}

void CWriteDB_IsamIndex::x_WriteHeader()
{
    int isam_version  = 1;
    int isam_type     = 0;
    int num_terms     = 0;
    int max_line_size = 0;
    
    switch(m_Type) {
    case eGi:
    case ePig:
        isam_type = eIsamNumericType; // numeric w/ data
        num_terms = m_NumberTable.size();
        max_line_size = 0;
        break;
        
    case eAcc:
        isam_type = eIsamStringType; // string w/ data
        max_line_size = eMaxStringLine;
        num_terms = m_StringSort.size();
        break;
        
    default:
        NCBI_THROW(CWriteDBException,
                   eArgErr,
                   "Unknown id type specified.");
    }
    
    int samples = s_DivideRoundUp(num_terms, m_PageSize);
    
    // These should probably use be a WriteInt4 method which should be
    // added to the CWriteDB_File class.
    
    WriteInt4(isam_version);
    WriteInt4(isam_type);
    WriteInt4(m_DataFileSize);
    
    WriteInt4(num_terms);
    WriteInt4(samples);
    WriteInt4(m_PageSize);
    
    WriteInt4(max_line_size);
    WriteInt4(m_Sparse ? 1 : 0);
    WriteInt4(0);
}

void CWriteDB_IsamIndex::x_FlushStringIndex()
{
    // Note: This function can take a noticeable portion of the
    // database dumping time.  For some databases, the length of the
    // data file for the string index competes with the sequence file
    // to determine volumes seperation points.
    
    // String ISAM files have four parts.  First, the standard
    // meta-data header.  Then (at address 36), we have a table of
    // page offsets.  After this is a table of key offsets in the
    // index file, then finally the list of keys.
    
    int data_pos = 0;
    unsigned count = m_StringSort.size();
    
    unsigned nsamples = s_DivideRoundUp(count, m_PageSize);
    
    string key_buffer;
    vector<int> key_off;
    
    // Reserve enough room, throwing in some extra.  Since we excerpt
    // every 64th entry, dividing by 64 would be exactly balanced.  To
    // make reallocation rare, I divide by 63 instead, and throw in an
    // extra 16 bytes.
    
    key_buffer.reserve(m_DataFileSize/63 + 16);
    key_off.reserve(nsamples);
    
    unsigned i(0);
    
    string NUL("x");
    NUL[0] = (char) 0;
    
    const string * prevs = 0;
    
    int output_count = 0;
    int index = 0;
    
    ITERATE(set<string>, iter, m_StringSort) {
        const string & elemstr = * iter;
        
        if (prevs && ((*prevs) == elemstr)) {
            continue;
        } else {
            prevs = & elemstr;
        }
        
        // For each element whose index is a multiple of m_PageSize
        // (starting with element zero), we add the record to the
        // index file.
        
        // The page offset table can be written as it comes in, but
        // the key offsets and keys are not written until all the page
        // offsets have been written, so they are accumulated in a
        // vector (key_off) and string (key_buffer) respectively.
        
        if ((output_count & (m_PageSize-1)) == 0) {
            // Write the data file position to the index file.
            
            WriteInt4(data_pos);
            
            // Store the overall index file position where the key
            // will be written.
            
            key_off.push_back(key_buffer.size());
            
            // Store the string record for the index file (but this
            // string is NUL terminated, whereas the data file rows
            // are line feed (aka newline) terminated.
            
            key_buffer.append(elemstr.data(), elemstr.length()-1);
            key_buffer.append(NUL);
        }
        output_count ++;
        
        data_pos = m_DataFile->Write(elemstr);
        index ++;
    }
    
    // Write the final data position.
    
    WriteInt4(data_pos);
    
    // Push back the final buffer offset.
    
    key_off.push_back(key_buffer.size());
    
    int key_off_start = eKeyOffset + (nsamples + 1) * 8;
    
    // Write index file offsets of keys.
    
    for(i = 0; i < key_off.size(); i++) {
        WriteInt4(key_off[i] + key_off_start);
    }
    
    // Write buffer of keys.
    
    Write(key_buffer);
}

void CWriteDB_IsamIndex::x_FlushNumericIndex()
{
    int row_index = 0;
    
    sort(m_NumberTable.begin(), m_NumberTable.end());
    
    int count = m_NumberTable.size();
    
    const SIdOid * prevp = 0;
    
    for(int i = 0; i < count; i++) {
        const SIdOid & elem = m_NumberTable[i];
        
        if (prevp && (*prevp == elem)) {
            continue;
        } else {
            prevp = & elem;
        }
        
        if ((row_index & (m_PageSize-1)) == 0) {
            WriteInt4(elem.id());
            WriteInt4(elem.oid());
        }
        
        m_DataFile->WriteInt4(elem.id());
        m_DataFile->WriteInt4(elem.oid());
        row_index ++;
    }
    
    // Numeric files end in (max-uint, 0).
    
    WriteInt4(-1);
    WriteInt4(0);
}

void CWriteDB_IsamIndex::x_Flush()
{
    // Step 1: Write header data.
    x_WriteHeader();
    
    // Step 2: Flush all data to the data file.
    //  A. Sort all entries up front with sort.
    //  B. Pick out periodic samples for the index, every 256 elements
    //     for numeric and every 64 for string.
    
    if (m_Type == eAcc) {
        x_FlushStringIndex();
    } else {
        x_FlushNumericIndex();
    }
    
    x_Free();
}

void CWriteDB_IsamIndex::AddIds(int oid, const TIdList & idlist)
{
    if (m_Type == eAcc) {
        x_AddStringIds(oid, idlist);
    } else if (m_Type == eGi) {
        x_AddGis(oid, idlist);
    } else {
        NCBI_THROW(CWriteDBException,
                   eArgErr,
                   "Cannot call AddIds() for PIG index.");
    }
}

void CWriteDB_IsamIndex::AddPig(int oid, int pig)
{
    SIdOid row(pig, oid);
    m_NumberTable.push_back(row);
    m_DataFileSize += 8;
}

void CWriteDB_IsamIndex::x_AddGis(int oid, const TIdList & idlist)
{
    ITERATE(TIdList, iter, idlist) {
        const CSeq_id & seqid = **iter;
        
        if (seqid.IsGi()) {
            SIdOid row(seqid.GetGi(), oid);
            m_NumberTable.push_back(row);
            m_DataFileSize += 8;
        }
    }
}

void CWriteDB_IsamIndex::x_AddStringIds(int oid, const TIdList & idlist)
{
    // Build all sub-string objects and add those.
    
    ITERATE(TIdList, iter, idlist) {
        const char        * typestr = 0;
        const CTextseq_id * textid  = 0;
        bool add_gb = true;
        
        const CSeq_id & seqid = **iter;
        
        switch(seqid.Which()) {
        case CSeq_id::e_Genbank:
            typestr = "gb";
            textid = & seqid.GetGenbank();
            add_gb = false;
            break;
            
        case CSeq_id::e_Embl:
            typestr = "emb";
            textid = & seqid.GetEmbl();
            break;
            
        case CSeq_id::e_Pir:
            typestr = "pir";
            textid = & seqid.GetPir();
            break;
            
        case CSeq_id::e_Swissprot:
            typestr = "sp";
            textid = & seqid.GetSwissprot();
            break;
            
        case CSeq_id::e_Other:
            typestr = "ref";
            textid = & seqid.GetOther();
            add_gb = false;
            break;
            
        case CSeq_id::e_Gi:
            if (! m_Sparse) {
                x_AddGiString(oid, seqid.GetGi());
            }
            break;
            
        case CSeq_id::e_Ddbj:
            typestr = "dbj";
            textid = & seqid.GetDdbj();
            break;
            
        case CSeq_id::e_Prf:
            typestr = "prf";
            textid = & seqid.GetPrf();
            break;
            
        case CSeq_id::e_Pdb:
            x_AddPdb(oid, seqid);
            break;
            
        case CSeq_id::e_Tpg:
            typestr = "tpg";
            textid = & seqid.GetTpg();
            add_gb = false;
            break;
            
        case CSeq_id::e_Tpe:
            typestr = "tpe";
            textid = & seqid.GetTpe();
            add_gb = false;
            break;
            
        case CSeq_id::e_Tpd:
            typestr = "tpd";
            textid = & seqid.GetTpd();
            add_gb = false;
            break;
            
        case CSeq_id::e_Gpipe:
            typestr = "gpipe";
            textid = & seqid.GetGpipe();
            break;
            
        case CSeq_id::e_General:
            if (! m_Sparse) {
                x_AddString(oid, seqid.AsFastaString());
            }
            break;
            
        case CSeq_id::e_Local:
            x_AddLocal(oid, seqid);
            break;
            
        default:
            {
                string msg = "Seq-id type not implemented (";
                msg += seqid.AsFastaString();
                msg += ").";
                
                NCBI_THROW(CWriteDBException, eArgErr, msg);
            }
        }
        
        if (typestr && textid) {
            x_AddTextId(oid, typestr, *textid, add_gb);
        }
    }
}

void CWriteDB_IsamIndex::x_AddGiString(int oid, int gi)
{
    x_AddString(oid, string("gi|") + NStr::UIntToString(gi));
}

void CWriteDB_IsamIndex::x_AddLocal(int             oid,
                                    const CSeq_id & seqid)
{
    const CObject_id & objid = seqid.GetLocal();
    
    if (! m_Sparse) {
        x_AddString(oid, seqid.AsFastaString());
    }
    if (objid.IsStr()) {
        x_AddString(oid, objid.GetStr());
    }
}

void CWriteDB_IsamIndex::x_AddPdb(int             oid,
                                  const CSeq_id & seqid)
{
    const CPDB_seq_id & pdb = seqid.GetPdb();
    
    // Sparse mode:
    //
    // "102l"
    // "102l  "
    // "102l| "
    //
    // Non-sparse mode:
    // "102l"
    // "102l  "
    // "102l| "
    // "pdb|102l| "
    
    string mol;
    
    if (pdb.CanGetMol()) {
        mol = pdb.GetMol().Get();
    }
    
    if (! mol.size()) {
        NCBI_THROW(CWriteDBException,
                   eArgErr,
                   "Empty molecule string in pdb Seq-id.");
    }
    
    string f1, f2;
    bool chain2 = false;
    
    f1 = seqid.AsFastaString();
    
    _ASSERT(f1.size() > 4);
    
    string sf1(f1, 4, f1.size()-4);
    string sf2 = sf1;
    
    if (sf2[sf2.size()-2] == '|') {
        sf2[sf2.size()-2] = ' ';
    } else {
        _ASSERT(sf2[sf2.size()-3] == '|');
        chain2 = true;
        sf2[sf2.size()-3] = ' ';
    }
    
    bool pedantic = true;
    
    if (pedantic) {
        if (chain2) {
            const char * ep = sf1.data() + sf1.size();
            string ext(ep-2, ep);
            int offset = sf1.size()-2;
            
            sf1.resize(offset + 1);
            sf2.resize(offset + 1);
            
            if (ext == "VB") {
                sf1[offset] = sf2[offset] = '|';
            }
        } else if (pdb.CanGetChain() && (pdb.GetChain() == 0)) {
            if (sf1[sf1.size()] == '|') {
                sf1[sf1.size()-1] = ' ';
            }
        }
    }
    
    x_AddString(oid, mol);
    x_AddString(oid, sf1);
    x_AddString(oid, sf2);
    
    if (! m_Sparse) {
        x_AddString(oid, f1);
    }
}

void CWriteDB_IsamIndex::x_AddTextId(int                 oid,
                                     const char        * typestr,
                                     const CTextseq_id & id,
                                     bool                add_gb)
{
    string acc, nm;
    
    // Note: if there is no accession, the id will not be added to a
    // sparse databases (even if there is a name).
    
    if (id.CanGetAccession()) {
        acc = id.GetAccession();
    }
    
    if (id.CanGetName()) {
        nm = id.GetName();
    }
    
    x_ToLower(acc);
    x_ToLower(nm);
    
    if (! acc.empty()) {
        x_AddString(oid, acc);
    }
    
    if (! m_Sparse) {
        if (! (nm.empty() || acc == nm)) {
            x_AddString(oid, nm);
        }
        
        int ver = id.CanGetVersion() ? id.GetVersion() : 0;
        
        if (ver && acc.size()) {
            x_AddString(oid, acc, ver);
        }
        
        x_AddString(oid, typestr, acc, ver, nm);
        
        if (add_gb && acc.size() && ver) {
            x_AddString(oid, "gb", acc, ver, nm);
        }
    }
}

void CWriteDB_IsamIndex::x_AddString(int oid, const string & s)
{
    // NOTE: all of the string finagling in this code could probably
    // benefit from some kind of pool-of-strings swap-allocator.
    //
    // It would follow these rules:
    //
    // 1. User asks for string of a certain size and gets string
    //    reserve()d to that size.
    // 2. If pool is empty, it would just use reserve().
    // 3. If pool is not empty, it would take the first entry, use reserve(),
    //    and swap that back to the user.
    //
    // 4. Space management could be done via a large vector or a list
    //    of vectors.  If the first of these, the large vector could
    //    be reallocated by creating a new one and swapping in all
    //    the strings.  Alternately a fixed size pool of strings could
    //    be used.
    //
    // 5. User would need to return strings they were done with.
    
    _ASSERT(s.size());
    
    char buf[256];
    int sz = sprintf(buf, "%s%c%d%c",
                     s.c_str(), (char)eKeyDelim, oid, (char)eRecordDelim);
    
    // lowercase the 'key' portion
    
    for(unsigned i = 0; i < s.size(); i++) {
        buf[i] = tolower(buf[i]);
    }
    
    m_StringSort.insert(string(buf, sz));
    
    m_DataFileSize += sz;
}

void CWriteDB_IsamIndex::x_AddString(int oid, const string & acc, int ver)
{
    _ASSERT(! m_Sparse);
    
    if (acc.size() && ver) {
        char buf[256];
        sprintf(buf, "%s.%d", acc.c_str(), ver);
        x_AddString(oid, buf);
    }
}

void CWriteDB_IsamIndex::x_AddString(int            oid,
                                     const char   * typestr,
                                     const string & acc,
                                     int            ver,
                                     const string & nm)
{
    _ASSERT(! m_Sparse);
    
    // ab003779
    // ab003779.1
    // dbj|ab003779.1|
    // dbj|ab003779.1|ab003779
    // dbj|ab003779|
    // dbj|ab003779|ab003779
    // dbj||ab003779
    // gb|ab003779.1|
    // gb|ab003779.1|ab003779
    // gb|ab003779|
    // gb|ab003779|ab003779
    // gb||ab003779
    
    char buf[256];
    
    _ASSERT((acc.size() + nm.size()) < 240);
    
    if (acc.size()) {
        sprintf(buf, "%s|%s|", typestr, acc.c_str());
        x_AddString(oid, buf);
        
        if (nm.size()) {
            strcat(buf, nm.c_str());
            x_AddString(oid, buf);
        }
        
        if (ver) {
            sprintf(buf, "%s|%s.%d|", typestr, acc.c_str(), ver);
            x_AddString(oid, buf);
            
            if (nm.size()) {
                strcat(buf, nm.c_str());
                x_AddString(oid, buf);
            }
        }
    }
    
    if (nm.size()) {
        sprintf(buf, "%s||%s", typestr, nm.c_str());
        x_AddString(oid, buf);
    }
}

CWriteDB_IsamData::CWriteDB_IsamData(EWriteDBIsamType itype,
                                     const string   & dbname,
                                     bool             protein,
                                     int              index,
                                     Uint8            max_file_size)
    : CWriteDB_File (dbname,
                     s_IsamExtension(itype, protein, false),
                     index,
                     max_file_size)
{
}

CWriteDB_IsamData::~CWriteDB_IsamData()
{
}

void CWriteDB_IsamData::x_Flush()
{
}

bool CWriteDB_IsamIndex::CanFit(int num)
{
    return (m_DataFileSize + (num+1) * m_BytesPerElem) < m_MaxFileSize;
}

void CWriteDB_IsamIndex::x_Free()
{
    m_StringSort.clear();
    m_NumberTable.clear();
}

void CWriteDB_Isam::ListFiles(vector<string> & files) const
{
    files.push_back(m_IFile->GetFilename());
    files.push_back(m_DFile->GetFilename());
}

END_NCBI_SCOPE

