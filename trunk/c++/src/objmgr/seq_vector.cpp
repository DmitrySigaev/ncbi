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
* Author: Aleksey Grichenko
*
* File Description:
*   Sequence data container for object manager
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.6  2002/02/21 19:27:06  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.5  2002/02/15 20:35:38  gouriano
* changed implementation of HandleRangeMap
*
* Revision 1.4  2002/01/30 22:09:28  gouriano
* changed CSeqMap interface
*
* Revision 1.3  2002/01/23 21:59:32  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.2  2002/01/16 16:25:58  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:24  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/


#include <objects/objmgr1/seq_vector.hpp>
#include "data_source.hpp"
#include <objects/seq/NCBI8aa.hpp>
#include <objects/seq/NCBIpaa.hpp>
#include <objects/seq/NCBIstdaa.hpp>
#include <objects/seq/NCBIeaa.hpp>
#include <objects/seq/NCBIpna.hpp>
#include <objects/seq/NCBI8na.hpp>
#include <objects/seq/NCBI4na.hpp>
#include <objects/seq/NCBI2na.hpp>
#include <objects/seq/IUPACaa.hpp>
#include <objects/seq/IUPACna.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/seqport_util.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CSeqVector::
//


CSeqVector::CSeqVector(const CBioseq_Handle& handle,
                         bool plus_strand,
                         CScope& scope)
    : m_Scope(&scope),
      m_Handle(handle),
      m_PlusStrand(plus_strand),
      m_Size(-1),
      m_Coding(CSeq_data::e_not_set)
{
    m_CurData.dest_start = -1;
    m_CurData.length = 0;
    m_SeqMap.Reset(&m_Handle.x_GetDataSource().GetSeqMap(m_Handle));
}


CSeqVector::~CSeqVector(void)
{
    return;
}


CSeqVector::CSeqVector(const CSeqVector& vec)
{
    *this = vec;
}


CSeqVector& CSeqVector::operator= (const CSeqVector& vec)
{
    if (&vec == this)
        return *this;
    m_Scope = vec.m_Scope;
    m_Handle = vec.m_Handle;
    m_PlusStrand = vec.m_PlusStrand;
    m_CurData.dest_start = vec.m_CurData.dest_start;
    m_CurData.length = vec.m_CurData.length;
    m_CurData.src_start = vec.m_CurData.src_start;
    m_CurData.src_data = vec.m_CurData.src_data;
    m_SeqMap = vec.m_SeqMap;
    m_Size = vec.m_Size;
    m_Coding = vec.m_Coding;
    return *this;
}


size_t CSeqVector::size(void)
{
    if ( m_Size < 0 )
    {
        // Calculate sequence size only once
        m_Size = 0;
        for (size_t i = 0; i < m_SeqMap->size(); i++) {
            if ((*m_SeqMap)[i].m_RefLen > 0) {
                // Use explicit segment size
                m_Size += (*m_SeqMap)[i].m_RefLen;
            }
            else {
                switch ((*m_SeqMap)[i].m_SegType) {
                case CSeqMap::eSeqData:
                    {
                        break;
                    }
                case CSeqMap::eSeqRef:
                    {
                        // Zero length stands for "whole" reference
                        const CSeq_id* id =
                            &CSeq_id_Mapper::GetSeq_id(
                            (*m_SeqMap)[i].m_RefSeq);
                        CBioseq_Handle::TBioseqCore ref_seq =
                            (m_Scope->GetBioseqHandle(*id)).GetBioseqCore();
                        if (ref_seq.GetPointer()  &&
                            ref_seq->GetInst().IsSetLength()) {
                            m_Size += ref_seq->GetInst().GetLength();
                        }
                        break;
                    }
                case CSeqMap::eSeqGap:
                    {
                        break;
                    }
                case CSeqMap::eSeqEnd:
                    {
                        break;
                    }
                }
            }
        }
    }
    return m_Size;
}


CSeqVector::TResidue CSeqVector::operator[] (int pos)
{
    if ( !m_PlusStrand ) {
        //### Is this enough?
        pos = size() - pos - 1;
    }
    if (m_CurData.dest_start > pos  ||
        m_CurData.dest_start+m_CurData.length <= pos) {
        m_CurData.src_data = 0; // Reset data
        m_Scope->x_GetSequence(m_Handle, pos, &m_CurData);
        m_CachedPos = -1; // Reset cached data
        m_CachedData = "";
    }
    return x_GetResidue(pos);
}


const CSeqVector::TResidue kGap = 'N';

CSeqVector::TResidue CSeqVector::x_GetResidue(int pos)
{
    if (m_CachedPos < 0  ||  m_CachedLen <= 0) {
        if (!m_CurData.src_data)
            return kGap; // No data - return gap symbol
        m_CachedPos = m_CurData.dest_start;
        m_CachedLen = m_CurData.length;
        // Need CRef<> to delete temporary object on return
        CConstRef<CSeq_data> out;

        if (m_CurData.src_data->Which() == m_Coding  ||
            m_Coding == CSeq_data::e_not_set) {
            out = m_CurData.src_data;
        }
        else {
            CSeq_data* tmp = new CSeq_data;
            out.Reset(tmp);
            CSeqportUtil::Convert(*m_CurData.src_data, tmp,
                m_Coding, m_CurData.src_start, m_CurData.length);
        }

        if ( !m_PlusStrand ) {
            CSeq_data* tmp = new CSeq_data;
            CSeqportUtil::Complement(*out, tmp);
            out.Reset(tmp);
        }
        switch ( out->Which() ) {
        case CSeq_data::e_Iupacna:
            {
                m_CachedData = out->GetIupacna().Get().
                    substr(m_CurData.src_start, m_CachedLen);
                break;
            }
        case CSeq_data::e_Iupacaa:
            {
                m_CachedData = out->GetIupacaa().Get().
                    substr(m_CurData.src_start, m_CachedLen);
                break;
            }
        case CSeq_data::e_Ncbi2na:
            {
                m_CachedData = "";
                const vector<char>& buf = out->GetNcbi2na().Get();
                for (int i = m_CurData.src_start; i <
                    m_CurData.src_start + m_CachedLen; i++) {
                    m_CachedData +=
                        (buf[i/4] >> (6-(i%4)*2)) & 0x03;
                }
                break;
            }
        case CSeq_data::e_Ncbi4na:
            {
                m_CachedData = "";
                const vector<char>& buf = out->GetNcbi4na().Get();
                for (int i = m_CurData.src_start; i <
                    m_CurData.src_start + m_CachedLen; i++) {
                    m_CachedData += (buf[i/2] >> (4-(i % 2)*4)) & 0x0f;
                }
                break;
            }
        case CSeq_data::e_Ncbi8na:
            {
                m_CachedData = "";
                const vector<char>& buf = out->GetNcbi8na().Get();
                for (int i = m_CurData.src_start; i <
                    m_CurData.src_start + m_CachedLen; i++) {
                    m_CachedData += buf[i];
                }
                break;
            }
        case CSeq_data::e_Ncbieaa:
            {
                m_CachedData = out->GetNcbieaa().Get().
                    substr(m_CurData.src_start, m_CachedLen);
                break;
            }
        case CSeq_data::e_Ncbipna:
            {
                m_CachedData = "";
                const vector<char>& buf = out->GetNcbipna().Get();
                for (int i = m_CurData.src_start; i <
                    m_CurData.src_start + m_CachedLen; i++) {
                    m_CachedData += buf[i];
                }
                break;
            }
        case CSeq_data::e_Ncbi8aa:
            {
                m_CachedData = "";
                const vector<char>& buf = out->GetNcbi8aa().Get();
                for (int i = m_CurData.src_start; i <
                    m_CurData.src_start + m_CachedLen; i++) {
                    m_CachedData += buf[i];
                }
                break;
            }
        case CSeq_data::e_Ncbipaa:
            {
                m_CachedData = "";
                const vector<char>& buf = out->GetNcbipaa().Get();
                for (int i = m_CurData.src_start; i <
                    m_CurData.src_start + m_CachedLen; i++) {
                    m_CachedData += buf[i];
                }
                break;
            }
        case CSeq_data::e_Ncbistdaa:
            {
                m_CachedData = "";
                const vector<char>& buf = out->GetNcbistdaa().Get();
                for (int i = m_CurData.src_start; i <
                    m_CurData.src_start + m_CachedLen; i++) {
                    m_CachedData += buf[i];
                }
                break;
            }
        default:
            {
                return kGap;
            }
        }
    }
    return m_CachedData[pos - m_CurData.dest_start];
}


void CSeqVector::SetIupacCoding(void)
{
    (void) ((*this)[0]); // force instantiantion
    if ( !m_CurData.src_data ) {
        return;
    }
    switch (m_CurData.src_data->Which()) {
    case CSeq_data::e_Ncbi2na:
    case CSeq_data::e_Ncbi4na:
    case CSeq_data::e_Ncbi8na:
    case CSeq_data::e_Ncbipna:
        SetCoding(CSeq_data::e_Iupacna);
        break;
    case CSeq_data::e_Ncbi8aa:
    case CSeq_data::e_Ncbieaa:
    case CSeq_data::e_Ncbipaa:
    case CSeq_data::e_Ncbistdaa:
        SetCoding(CSeq_data::e_Iupacaa);
        break;
    default:
        break;
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE
