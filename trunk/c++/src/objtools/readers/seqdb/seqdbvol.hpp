#ifndef OBJTOOLS_READERS_SEQDB__SEQDBVOL_HPP
#define OBJTOOLS_READERS_SEQDB__SEQDBVOL_HPP

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

/// CSeqDBVol class
/// 
/// This object defines access to one database volume.

#include <assert.h>
#include <iostream>

#include <objtools/readers/seqdb/seqdb.hpp>
#include "seqdbfile.hpp"

#include <sstream>

#include <objects/seq/NCBI2na.hpp>
#include <objects/seq/NCBI4na.hpp>
#include <objects/seq/NCBIstdaa.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>
#include <corelib/ncbimtx.hpp>

BEGIN_NCBI_SCOPE

using namespace ncbi;
using namespace ncbi::objects;

class CSeqDBVol : public CObject {
public:
    CSeqDBVol(const string & name, char prot_nucl, bool use_mmap)
        : m_VolName(name),
          m_Idx(name, prot_nucl, use_mmap),
          m_Seq(name, prot_nucl, use_mmap),
          m_Hdr(name, prot_nucl, use_mmap)
    {
    }
    
    Int4 GetSeqLength(Uint4 oid, bool approx);
    
    Int4 GetSeqLengthApprox(Uint4 oid);
    
    CRef<CBlast_def_line_set> GetHdr(Uint4 oid);
    
    bool RetSequence(const char ** buffer);
    
    char GetSeqType(void);
    
    CRef<CBioseq> GetBioseq(Int4 oid,
                            bool use_objmgr,
                            bool insert_ctrlA);
    
    Int4 GetSequence(Int4 oid, const char ** buffer);
    
    string GetTitle(void);
    
    string GetDate(void);
    
    Uint4  GetNumSeqs(void);
    
    Uint8  GetTotalLength(void);
    
    Uint4  GetMaxLength(void);
    
    string GetVolName(void) const
    {
        return m_VolName;
    }
    
private:
    CRef<CBlast_def_line_set> x_GetHdr(Uint4 oid);
    bool x_RetSequence(const char ** buffer);
    char x_GetSeqType(void);
    bool x_GetAmbChar(Uint4 oid, vector<Int4> ambchars);
    Int4 x_GetSequence(Int4 oid, const char ** buffer);
    
    string        m_VolName;
    CFastMutex    m_Lock;
    CSeqDBIdxFile m_Idx;
    CSeqDBSeqFile m_Seq;
    CSeqDBHdrFile m_Hdr;
};

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS_SEQDB__SEQDBVOL_HPP


