#ifndef OBJTOOLS_READERS_SEQDB__SEQDBOIDLIST_HPP
#define OBJTOOLS_READERS_SEQDB__SEQDBOIDLIST_HPP

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

/// CSeqDBOIDList class
/// 
/// This object defines access to one database aliasume.

#include <objtools/readers/seqdb/seqdb.hpp>
#include "seqdbfile.hpp"
#include "seqdbvolset.hpp"

BEGIN_NCBI_SCOPE

using namespace ncbi::objects;

class CSeqDBOIDList : public CObject {
public:
    typedef Uint4 TOID;
    
    CSeqDBOIDList(CSeqDBVolSet & volumes, bool use_mmap);
    
    ~CSeqDBOIDList();
    
    bool CheckOrFindOID(TOID & next_oid) const
    {
        if (x_IsSet(next_oid)) {
            return true;
        }
        return x_FindNext(next_oid);
    }
    
private:
    typedef const unsigned char TCUC;
    typedef unsigned char TUC;
    
    bool x_IsSet   (TOID   oid) const;
    void x_SetBit  (TOID   oid);
    bool x_FindNext(TOID & oid) const;
    
    void x_Setup(const string & filename, bool use_mmap);
    void x_Setup(CSeqDBVolSet & volset, bool use_mmap);
    void x_OrFileBits(const string & mask_fname, Uint4 oid_start, Uint4 oid_end, bool use_mmap);
    void x_SetBitRange(Uint4 oid_start, Uint4 oid_end);
    
    // Data
    
    CSeqDBMemPool         m_MemPool;
    CRef<CSeqDBRawFile>   m_RawFile;
    Uint4                 m_NumOIDs;
    
    TUC                 * m_Bits;
    TUC                 * m_BitEnd;
};


END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBOIDLIST_HPP

