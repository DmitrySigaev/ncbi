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
* Author: Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*   Sequence data container for object manager
*
*/


#include <ncbi_pch.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_vector_ci.hpp>
#include <corelib/ncbimtx.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objmgr/seq_map.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/impl/seq_vector_cvt.hpp>
#include <algorithm>
#include <map>
#include <vector>
#include <util/random_gen.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CNcbi2naRandomizer::
//

INcbi2naRandomizer::~INcbi2naRandomizer(void)
{
}


CNcbi2naRandomizer::CNcbi2naRandomizer(CRandom& gen)
{
    unsigned int bases[4]; // Count of each base in the random distribution
    for (int na4 = 0; na4 < 16; na4++) {
        int bit_count = 0;
        char set_bit = 0;
        for (int bit = 0; bit < 4; bit++) {
            // na4 == 0 is special case (gap) should be treated as 0xf
            if ( !na4  ||  (na4 & (1 << bit)) ) {
                bit_count++;
                bases[bit] = 1;
                set_bit = bit;
            }
            else {
                bases[bit] = 0;
            }
        }
        if (bit_count == 1) {
            // Single base
            m_FixedTable[na4] = set_bit;
            continue;
        }
        m_FixedTable[na4] = kRandomValue;
        // Ambiguity: create random distribution with possible bases
        for (int bit = 0; bit < 4; bit++) {
            bases[bit] *= kRandomDataSize/bit_count +
                kRandomDataSize % bit_count;
        }
        for (int i = kRandomDataSize - 1; i >= 0; i--) {
            CRandom::TValue rnd = gen.GetRand(0, i);
            for (int base = 0; base < 4; base++) {
                if (!bases[base]  ||  rnd > bases[base]) {
                    rnd -= bases[base];
                    continue;
                }
                m_RandomTable[na4][i] = base;
                bases[base]--;
                break;
            }
        }
    }
}


CNcbi2naRandomizer::~CNcbi2naRandomizer(void)
{
}


void CNcbi2naRandomizer::RandomizeData(char* data,
                                       size_t count,
                                       TSeqPos pos)
{
    for (char* stop = data + count; data < stop; ++data, ++pos) {
        int base4na = *data;
        char base2na = m_FixedTable[base4na];
        if ( base2na == kRandomValue ) {
            // Ambiguity, use random value
            base2na = m_RandomTable[base4na][(pos & kRandomizerPosMask)];
        }
        *data = base2na;
    }
}


////////////////////////////////////////////////////////////////////
//
//  CSeqVector::
//


CSeqVector::CSeqVector(void)
    : m_Size(0)
{
}


CSeqVector::CSeqVector(const CSeqVector& vec)
    : m_Scope(vec.m_Scope),
      m_SeqMap(vec.m_SeqMap),
      m_TSE(vec.m_TSE),
      m_Size(vec.m_Size),
      m_Mol(vec.m_Mol),
      m_Strand(vec.m_Strand),
      m_Coding(vec.m_Coding)
{
}


CSeqVector::CSeqVector(const CBioseq_Handle& bioseq,
                       EVectorCoding coding, ENa_strand strand)
    : m_Scope(bioseq.GetScope()),
      m_SeqMap(&bioseq.GetSeqMap()),
      m_TSE(bioseq.GetTSE_Handle()),
      m_Strand(strand),
      m_Coding(CSeq_data::e_not_set)
{
    m_Size = bioseq.GetBioseqLength();
    m_Mol = bioseq.GetSequenceType();
    SetCoding(coding);
}


CSeqVector::CSeqVector(const CSeqMap& seqMap, CScope& scope,
                       EVectorCoding coding, ENa_strand strand)
    : m_Scope(&scope),
      m_SeqMap(&seqMap),
      m_Strand(strand),
      m_Coding(CSeq_data::e_not_set)
{
    m_Size = m_SeqMap->GetLength(m_Scope);
    m_Mol = m_SeqMap->GetMol();
    SetCoding(coding);
}


CSeqVector::CSeqVector(const CSeqMap& seqMap, const CTSE_Handle& top_tse,
                       EVectorCoding coding, ENa_strand strand)
    : m_Scope(top_tse.GetScope()),
      m_SeqMap(&seqMap),
      m_TSE(top_tse),
      m_Strand(strand),
      m_Coding(CSeq_data::e_not_set)
{
    m_Size = m_SeqMap->GetLength(m_Scope);
    m_Mol = m_SeqMap->GetMol();
    SetCoding(coding);
}


CSeqVector::CSeqVector(const CSeq_loc& loc, CScope& scope,
                       EVectorCoding coding, ENa_strand strand)
    : m_Scope(&scope),
      m_SeqMap(CSeqMap::GetSeqMapForSeq_loc(loc, &scope)),
      m_Strand(strand),
      m_Coding(CSeq_data::e_not_set)
{
    m_Size = m_SeqMap->GetLength(m_Scope);
    m_Mol = m_SeqMap->GetMol();
    SetCoding(coding);
}


CSeqVector::CSeqVector(const CSeq_loc& loc, const CTSE_Handle& top_tse,
                       EVectorCoding coding, ENa_strand strand)
    : m_Scope(top_tse.GetScope()),
      m_SeqMap(CSeqMap::GetSeqMapForSeq_loc(loc, &top_tse.GetScope())),
      m_TSE(top_tse),
      m_Strand(strand),
      m_Coding(CSeq_data::e_not_set)
{
    m_Size = m_SeqMap->GetLength(m_Scope);
    m_Mol = m_SeqMap->GetMol();
    SetCoding(coding);
}


CSeqVector::CSeqVector(const CBioseq& bioseq,
                       CScope* scope,
                       EVectorCoding coding, ENa_strand strand)
    : m_Scope(scope),
      m_SeqMap(CSeqMap::CreateSeqMapForBioseq(bioseq)),
      m_Strand(strand),
      m_Coding(CSeq_data::e_not_set)
{
    m_Size = m_SeqMap->GetLength(scope);
    m_Mol = bioseq.GetInst().GetMol();
    SetCoding(coding);
}


CSeqVector::~CSeqVector(void)
{
}


CSeqVector& CSeqVector::operator= (const CSeqVector& vec)
{
    if ( &vec != this ) {
        m_Scope  = vec.m_Scope;
        m_SeqMap = vec.m_SeqMap;
        m_TSE    = vec.m_TSE;
        m_Size   = vec.m_Size;
        m_Mol    = vec.m_Mol;
        m_Strand = vec.m_Strand;
        m_Coding = vec.m_Coding;
        m_Iterator.reset();
    }
    return *this;
}


CSeqVector_CI* CSeqVector::x_CreateIterator(TSeqPos pos) const
{
    CSeqVector_CI* iter;
    m_Iterator.reset(iter = new CSeqVector_CI(*this, pos));
    return iter;
}


bool CSeqVector::CanGetRange(TSeqPos from, TSeqPos to) const
{
    SSeqMapSelector sel;
    sel.SetRange(from, to-from).SetStrand(m_Strand).SetLinkUsedTSE(m_TSE)
        .SetResolveCount(size_t(-1));
    return m_SeqMap->CanResolveRange(m_Scope.GetScopeOrNull(), sel);
}


void CSeqVector::GetPackedSeqData(string& dst_str,
                                  TSeqPos src_pos,
                                  TSeqPos src_end)
{
    dst_str.erase();
    src_end = min(src_end, size());
    if ( src_pos >= src_end ) {
        return;
    }

    TCoding dst_coding = GetCoding();
    switch ( dst_coding ) {
    case CSeq_data::e_Iupacna:
    case CSeq_data::e_Ncbi8na:
    case CSeq_data::e_Iupacaa:
    case CSeq_data::e_Ncbieaa:
    case CSeq_data::e_Ncbi8aa:
    case CSeq_data::e_Ncbistdaa:
        x_GetPacked8SeqData(dst_str, src_pos, src_end);
        break;
    case CSeq_data::e_Ncbi4na:
        x_GetPacked4naSeqData(dst_str, src_pos, src_end);
        break;
    case CSeq_data::e_Ncbi2na:
        x_GetPacked2naSeqData(dst_str, src_pos, src_end);
        break;
    default:
        NCBI_THROW_FMT(CSeqVectorException, eCodingError,
                       "Can not pack data using the selected coding: "<<
                       GetCoding());
    }
}

static const size_t kBufferSize = 1024; // must be multiple of 4

static inline
void x_Append8To8(string& dst_str, const string& src_str,
                  size_t src_pos, size_t count)
{
    if ( count ) {
        dst_str.append(src_str.data()+src_pos, count);
    }
}


static inline
void x_Append8To8(string& dst_str, const vector<char>& src_str,
                  size_t src_pos, size_t count)
{
    if ( count ) {
        dst_str.append(&src_str[src_pos], count);
    }
}


static inline
void x_AppendGapTo8(string& dst_str, size_t count, char gap)
{
    if ( count ) {
        dst_str.append(count, gap);
    }
}


static
void x_Append8To4(string& dst, char& dst_c, TSeqPos dst_pos,
                  const char* src, size_t count)
{
    if ( !count ) {
        return;
    }
    if ( dst_pos & 1 ) {
        dst += char((dst_c<<4)|*src);
        dst_c = 0;
        ++dst_pos;
        ++src;
        --count;
    }
    for ( ; count >= 2; dst_pos += 2, src += 2, count -= 2 ) {
        dst += char((src[0]<<4)|src[1]);
    }
    if ( count&1 ) {
        dst_c = *src;
    }
}


static
void x_Append4To4(string& dst, char& dst_c, TSeqPos dst_pos,
                  const vector<char>& src, TSeqPos src_pos,
                  TSeqPos count)
{
    if ( !count ) {
        return;
    }
    if ( (src_pos^dst_pos) & 1 ) {
        // misaligned data -> dst_str
        if ( dst_pos & 1 ) {
            // align dst_pos
            dst += char((dst_c<<4)|((src[src_pos>>1]>>4)&15));
            dst_c = 0;
            ++dst_pos;
            ++src_pos;
            --count;
        }
        _ASSERT((src_pos&1));
        size_t pos = src_pos>>1;
        for ( ; count >= 2; dst_pos += 2, pos += 1, count -= 2 ) {
            dst += char(((src[pos]<<4)&0xf0)|((src[pos+1]>>4)&0x0f));
        }
        if ( count&1 ) {
            _ASSERT((src_pos&1));
            dst_c = (src[pos])&15;
        }
    }
    else {
        // aligned data -> dst_str
        if ( dst_pos & 1 ) {
            // align dst_pos
            dst += char((dst_c<<4)|((src[src_pos>>1])&15));
            dst_c = 0;
            ++dst_pos;
            ++src_pos;
            --count;
        }
        _ASSERT(!(src_pos&1));
        _ASSERT(!(dst_pos&1));
        size_t octets = count>>1;
        size_t pos = src_pos>>1;
        if ( octets ) {
            dst.append(&src[pos], octets);
        }
        if ( count&1 ) {
            _ASSERT(!(src_pos&1));
            dst_c = (src[pos+octets]>>4)&15;
        }
    }
}


static
void x_AppendGapTo4(string& dst_str, char& dst_c, TSeqPos dst_pos,
                    TSeqPos count, char gap)
{
    if ( !count ) {
        return;
    }
    if ( dst_pos & 1 ) {
        // align dst_pos
        dst_str += char((dst_c << 4)|gap);
        dst_c = 0;
        ++dst_pos;
        --count;
    }
    _ASSERT(!(dst_pos&1));
    size_t octets = count>>1;
    if ( octets ) {
        dst_str.append(octets, char((gap<<4)|gap));
    }
    if ( count&1 ) {
        dst_c = gap;
    }
}


static
void x_Append8To2(string& dst_str, char& dst_c, TSeqPos dst_pos,
                  const char* buffer, TSeqPos count)
{
    if ( !count ) {
        return;
    }
    _ASSERT(dst_str.size() == dst_pos>>2);
    const char* unpacked = buffer;
    if ( dst_pos&3 ) {
        char c = dst_c;
        for ( ; count && (dst_pos&3); --count, ++dst_pos ) {
            c = (c<<2)|*unpacked++;
        }
        if ( (dst_pos&3) == 0 ) {
            dst_str += c;
            dst_c = 0;
        }
        else {
            dst_c = c;
        }
        if ( !count ) {
            return;
        }
    }
    _ASSERT((dst_pos&3) == 0);
    _ASSERT(dst_str.size() == dst_pos>>2);
    char packed_buffer[kBufferSize/4];
    char* packed_end = packed_buffer;
    for ( ; count >= 4; count -= 4, unpacked += 4 ) {
        *packed_end++ =
            (unpacked[0]<<6)|(unpacked[1]<<4)|(unpacked[2]<<2)|unpacked[3];
    }
    dst_str.append(packed_buffer, packed_end);
    switch ( count ) {
    case 1:
        dst_c = unpacked[0];
        break;
    case 2:
        dst_c = (unpacked[0]<<2)|unpacked[1];
        break;
    case 3:
        dst_c = (unpacked[0]<<4)|(unpacked[1]<<2)|unpacked[2];
        break;
    default:
        dst_c = 0;
        break;
    }
}


static
void x_Append2To2(string& dst, char& dst_c, TSeqPos dst_pos,
                  const vector<char>& src, TSeqPos src_pos,
                  TSeqPos count)
{
    if ( !count ) {
        return;
    }
    if ( (src_pos^dst_pos) & 3 ) {
        // misaligned src -> dst
        char buffer[kBufferSize];
        while ( count ) {
            // if count is larger than buffer size make sure
            // that the next dst_pos is aligned to 4.
            TSeqPos chunk = min(count, TSeqPos(kBufferSize-(dst_pos&3)));
            copy_2bit(buffer, chunk, src, src_pos);
            x_Append8To2(dst, dst_c, dst_pos, buffer, chunk);
            dst_pos += chunk;
            src_pos += chunk;
            count -= chunk;
        }
        return;
    }

    // aligned src -> dst
    if ( dst_pos&3 ) {
        // align dst_pos
        TSeqPos add = 4-(dst_pos&3);
        char c = (dst_c<<(add*2))|(src[src_pos>>2]&((1<<(add*2))-1));
        if ( count < add ) {
            dst_c = c >> (2*(add-count));
            return;
        }
        dst += c;
        dst_c = 0;
        src_pos += add;
        dst_pos += add;
        count -= add;
    }
    _ASSERT(!(src_pos&3));
    size_t octets = count>>2;
    size_t pos = src_pos>>2;
    if ( octets ) {
        dst.append(&src[pos], octets);
    }
    size_t rem = count&3;
    if ( rem ) {
        _ASSERT(!(src_pos&3));
        dst_c = (src[pos+octets]&255)>>(2*(4-rem));
    }
}


static
void x_AppendRandomTo2(string& dst_str, char& dst_c, TSeqPos dst_pos,
                       TSeqPos src_pos, TSeqPos count,
                       INcbi2naRandomizer& randomizer, char gap)
{
    char buffer[kBufferSize];
    while ( count ) {
        _ASSERT(dst_str.size() == dst_pos>>2);
        // if count is larger than buffer size make sure
        // that the next dst_pos is aligned to 4.
        TSeqPos chunk = min(count, TSeqPos(kBufferSize-(dst_pos&3)));
        fill_n(buffer, chunk, gap);
        randomizer.RandomizeData(buffer, chunk, src_pos);
        x_Append8To2(dst_str, dst_c, dst_pos, buffer, chunk);
        count -= chunk;
        src_pos += chunk;
        dst_pos += chunk;
        _ASSERT(dst_str.size() == dst_pos>>2);
    }
}


static
void x_AppendAnyTo8(string& dst_str,
                    const CSeq_data& data, TSeqPos dataPos,
                    TSeqPos total_count,
                    const char* table = 0, bool reverse = false)
{
    char buffer[kBufferSize];
    CSeq_data::E_Choice src_coding = data.Which();
    while ( total_count ) {
        TSeqPos count = min(total_count, TSeqPos(sizeof(buffer)));
        switch ( src_coding ) {
        case CSeq_data::e_Iupacna:
            copy_8bit_any(buffer, count, data.GetIupacna().Get(), dataPos,
                          table, reverse);
            break;
        case CSeq_data::e_Iupacaa:
            copy_8bit_any(buffer, count, data.GetIupacaa().Get(), dataPos,
                          table, reverse);
            break;
        case CSeq_data::e_Ncbi2na:
            copy_2bit_any(buffer, count, data.GetNcbi2na().Get(), dataPos,
                          table, reverse);
            break;
        case CSeq_data::e_Ncbi4na:
            copy_4bit_any(buffer, count, data.GetNcbi4na().Get(), dataPos,
                          table, reverse);
            break;
        case CSeq_data::e_Ncbi8na:
            copy_8bit_any(buffer, count, data.GetNcbi8na().Get(), dataPos,
                          table, reverse);
            break;
        case CSeq_data::e_Ncbi8aa:
            copy_8bit_any(buffer, count, data.GetNcbi8aa().Get(), dataPos,
                          table, reverse);
            break;
        case CSeq_data::e_Ncbieaa:
            copy_8bit_any(buffer, count, data.GetNcbieaa().Get(), dataPos,
                          table, reverse);
            break;
        case CSeq_data::e_Ncbistdaa:
            copy_8bit_any(buffer, count, data.GetNcbistdaa().Get(), dataPos,
                          table, reverse);
            break;
        default:
            NCBI_THROW_FMT(CSeqVectorException, eCodingError,
                           "Invalid data coding: "<<src_coding);
        }
        dst_str.append(buffer, count);
        if ( reverse ) {
            dataPos -= count;
        }
        else {
            dataPos += count;
        }
        total_count -= count;
    }
}


static
void x_AppendAnyTo4(string& dst_str, char& dst_c, TSeqPos dst_pos,
                    const CSeq_data& data, TSeqPos dataPos,
                    TSeqPos total_count,
                    const char* table, bool reverse)
{
    _ASSERT(table || reverse);
    char buffer[kBufferSize];
    CSeq_data::E_Choice src_coding = data.Which();
    while ( total_count ) {
        TSeqPos count = min(total_count, TSeqPos(sizeof(buffer)));
        switch ( src_coding ) {
        case CSeq_data::e_Iupacna:
            copy_8bit_any(buffer, count, data.GetIupacna().Get(), dataPos,
                          table, reverse);
            break;
        case CSeq_data::e_Iupacaa:
            copy_8bit_any(buffer, count, data.GetIupacaa().Get(), dataPos,
                          table, reverse);
            break;
        case CSeq_data::e_Ncbi2na:
            copy_2bit_any(buffer, count, data.GetNcbi2na().Get(), dataPos,
                          table, reverse);
            break;
        case CSeq_data::e_Ncbi4na:
            copy_4bit_any(buffer, count, data.GetNcbi4na().Get(), dataPos,
                          table, reverse);
            break;
        case CSeq_data::e_Ncbi8na:
            copy_8bit_any(buffer, count, data.GetNcbi8na().Get(), dataPos,
                          table, reverse);
            break;
        case CSeq_data::e_Ncbi8aa:
            copy_8bit_any(buffer, count, data.GetNcbi8aa().Get(), dataPos,
                          table, reverse);
            break;
        case CSeq_data::e_Ncbieaa:
            copy_8bit_any(buffer, count, data.GetNcbieaa().Get(), dataPos,
                          table, reverse);
            break;
        case CSeq_data::e_Ncbistdaa:
            copy_8bit_any(buffer, count, data.GetNcbistdaa().Get(), dataPos,
                          table, reverse);
            break;
        default:
            NCBI_THROW_FMT(CSeqVectorException, eCodingError,
                           "Invalid data coding: "<<src_coding);
        }
        x_Append8To4(dst_str, dst_c, dst_pos, buffer, count);
        if ( reverse ) {
            dataPos -= count;
        }
        else {
            dataPos += count;
        }
        dst_pos += count;
        total_count -= count;
    }
}


static
void x_AppendAnyTo2(string& dst_str, char& dst_c, TSeqPos dst_pos,
                    const CSeq_data& data, TSeqPos dataPos,
                    TSeqPos total_count,
                    const char* table, bool reverse,
                    INcbi2naRandomizer* randomizer, TSeqPos src_pos)
{
    _ASSERT(table || reverse || randomizer);
    char buffer[kBufferSize];
    CSeq_data::E_Choice src_coding = data.Which();
    while ( total_count ) {
        TSeqPos count = min(total_count, TSeqPos(sizeof(buffer)));
        switch ( src_coding ) {
        case CSeq_data::e_Iupacna:
            copy_8bit_any(buffer, count, data.GetIupacna().Get(), dataPos,
                          table, reverse);
            break;
        case CSeq_data::e_Iupacaa:
            copy_8bit_any(buffer, count, data.GetIupacaa().Get(), dataPos,
                          table, reverse);
            break;
        case CSeq_data::e_Ncbi2na:
            copy_2bit_any(buffer, count, data.GetNcbi2na().Get(), dataPos,
                          table, reverse);
            break;
        case CSeq_data::e_Ncbi4na:
            copy_4bit_any(buffer, count, data.GetNcbi4na().Get(), dataPos,
                          table, reverse);
            break;
        case CSeq_data::e_Ncbi8na:
            copy_8bit_any(buffer, count, data.GetNcbi8na().Get(), dataPos,
                          table, reverse);
            break;
        case CSeq_data::e_Ncbi8aa:
            copy_8bit_any(buffer, count, data.GetNcbi8aa().Get(), dataPos,
                          table, reverse);
            break;
        case CSeq_data::e_Ncbieaa:
            copy_8bit_any(buffer, count, data.GetNcbieaa().Get(), dataPos,
                          table, reverse);
            break;
        case CSeq_data::e_Ncbistdaa:
            copy_8bit_any(buffer, count, data.GetNcbistdaa().Get(), dataPos,
                          table, reverse);
            break;
        default:
            NCBI_THROW_FMT(CSeqVectorException, eCodingError,
                           "Invalid data coding: "<<src_coding);
        }
        if ( randomizer ) {
            randomizer->RandomizeData(buffer, count, src_pos);
        }
        x_Append8To2(dst_str, dst_c, dst_pos, buffer, count);
        if ( reverse ) {
            dataPos -= count;
        }
        else {
            dataPos += count;
        }
        dst_pos += count;
        src_pos += count;
        total_count -= count;
    }
}


void CSeqVector::x_GetPacked8SeqData(string& dst_str,
                                     TSeqPos src_pos,
                                     TSeqPos src_end)
{
    ECaseConversion case_conversion = eCaseConversion_none;
    SSeqMapSelector sel(CSeqMap::fDefaultFlags, kMax_UInt);
    sel.SetStrand(m_Strand);
    if ( m_TSE ) {
        sel.SetLinkUsedTSE(m_TSE);
    }
    CSeqMap_CI seg(m_SeqMap, m_Scope.GetScopeOrNull(), sel, src_pos);

    dst_str.reserve(src_end-src_pos);
    TCoding dst_coding = GetCoding();
    TSeqPos dst_pos = 0;
    while ( src_pos < src_end ) {
        _ASSERT(dst_str.size() == dst_pos);
        TSeqPos count = min(src_end-src_pos, seg.GetEndPosition()-src_pos);
        if ( seg.GetType() == CSeqMap::eSeqGap ) {
            x_AppendGapTo8(dst_str, count, GetGapChar());
        }
        else {
            const CSeq_data& data = seg.GetRefData();
            bool reverse = seg.GetRefMinusStrand();
            TCoding src_coding = data.Which();

            const char* table = 0;
            if ( dst_coding != src_coding || reverse ||
                 case_conversion != eCaseConversion_none ) {
                table = sx_GetConvertTable(src_coding, dst_coding,
                                           reverse, case_conversion);
                if ( !table && src_coding != dst_coding ) {
                    NCBI_THROW_FMT(CSeqVectorException, eCodingError,
                                   "Incompatible sequence codings: "<<
                                   src_coding<<" -> "<<dst_coding);
                }
            }

            TSeqPos dataPos;
            if ( reverse ) {
                // Revert segment offset
                dataPos = seg.GetRefEndPosition() -
                    (src_pos - seg.GetPosition()) - count;
            }
            else {
                dataPos = seg.GetRefPosition() +
                    (src_pos - seg.GetPosition());
            }

            if ( !table && !reverse ) {
                switch ( src_coding ) {
                case CSeq_data::e_Iupacna:
                    x_Append8To8(dst_str, data.GetIupacna().Get(),
                                 dataPos, count);
                    break;
                case CSeq_data::e_Iupacaa:
                    x_Append8To8(dst_str, data.GetIupacaa().Get(),
                                 dataPos, count);
                    break;
                case CSeq_data::e_Ncbi8na:
                    x_Append8To8(dst_str, data.GetNcbi8na().Get(),
                                 dataPos, count);
                    break;
                case CSeq_data::e_Ncbi8aa:
                    x_Append8To8(dst_str, data.GetNcbi8aa().Get(),
                                 dataPos, count);
                    break;
                case CSeq_data::e_Ncbieaa:
                    x_Append8To8(dst_str, data.GetNcbieaa().Get(),
                                 dataPos, count);
                    break;
                case CSeq_data::e_Ncbistdaa:
                    x_Append8To8(dst_str, data.GetNcbistdaa().Get(),
                                 dataPos, count);
                    break;
                default:
                    x_AppendAnyTo8(dst_str, data, dataPos, count);
                    break;
                }
            }
            else {
                x_AppendAnyTo8(dst_str, data, dataPos, count, table, reverse);
            }
        }
        ++seg;
        dst_pos += count;
        src_pos += count;
        _ASSERT(dst_str.size() == dst_pos);
    }
}


void CSeqVector::x_GetPacked4naSeqData(string& dst_str,
                                       TSeqPos src_pos,
                                       TSeqPos src_end)
{
    ECaseConversion case_conversion = eCaseConversion_none;
    SSeqMapSelector sel(CSeqMap::fDefaultFlags, kMax_UInt);
    sel.SetStrand(m_Strand);
    if ( m_TSE ) {
        sel.SetLinkUsedTSE(m_TSE);
    }
    CSeqMap_CI seg(m_SeqMap, m_Scope.GetScopeOrNull(), sel, src_pos);

    dst_str.reserve((src_end-src_pos+1)>>1);
    TCoding dst_coding = GetCoding();
    TSeqPos dst_pos = 0;
    char dst_c = 0;
    while ( src_pos < src_end ) {
        _ASSERT(dst_str.size() == dst_pos>>1);
        TSeqPos count = min(src_end-src_pos, seg.GetEndPosition()-src_pos);
        if ( seg.GetType() == CSeqMap::eSeqGap ) {
            x_AppendGapTo4(dst_str, dst_c, dst_pos, count, GetGapChar());
        }
        else {
            const CSeq_data& data = seg.GetRefData();
            bool reverse = seg.GetRefMinusStrand();
            TCoding src_coding = data.Which();
            
            const char* table = 0;
            if ( dst_coding != src_coding || reverse ||
                 case_conversion != eCaseConversion_none ) {
                table = sx_GetConvertTable(src_coding, dst_coding,
                                           reverse, case_conversion);
                if ( !table && src_coding != dst_coding ) {
                    NCBI_THROW_FMT(CSeqVectorException, eCodingError,
                                   "Incompatible sequence codings: "<<
                                   src_coding<<" -> "<<dst_coding);
                }
            }

            if ( table || reverse ) {
                TSeqPos dataPos;
                if ( reverse ) {
                    // Revert segment offset
                    dataPos = seg.GetRefEndPosition() -
                        (src_pos - seg.GetPosition()) - count;
                }
                else {
                    dataPos = seg.GetRefPosition() +
                        (src_pos - seg.GetPosition());
                }
                x_AppendAnyTo4(dst_str, dst_c, dst_pos,
                               data, dataPos, count, table, reverse);
            }
            else {
                TSeqPos dataPos = seg.GetRefPosition() +
                    (src_pos - seg.GetPosition());
                x_Append4To4(dst_str, dst_c, dst_pos,
                             data.GetNcbi4na().Get(), dataPos, count);
            }
        }
        ++seg;
        dst_pos += count;
        src_pos += count;
        _ASSERT(dst_str.size() == dst_pos>>1);
    }
    if ( dst_pos&1 ) {
        dst_str += char(dst_c<<4);
    }
}


void CSeqVector::x_GetPacked2naSeqData(string& dst_str,
                                       TSeqPos src_pos,
                                       TSeqPos src_end)
{
    ECaseConversion case_conversion = eCaseConversion_none;
    SSeqMapSelector sel(CSeqMap::fDefaultFlags, kMax_UInt);
    sel.SetStrand(m_Strand);
    if ( m_TSE ) {
        sel.SetLinkUsedTSE(m_TSE);
    }
    CSeqMap_CI seg(m_SeqMap, m_Scope.GetScopeOrNull(), sel, src_pos);

    dst_str.reserve((src_end-src_pos+3)>>2);
    _ASSERT(GetCoding() == CSeq_data::e_Ncbi2na);
    TSeqPos dst_pos = 0;
    char dst_c = 0;
    while ( src_pos < src_end ) {
        _ASSERT(dst_str.size() == dst_pos>>2);
        TSeqPos count = min(src_end-src_pos, seg.GetEndPosition()-src_pos);
        if ( seg.GetType() == CSeqMap::eSeqGap ) {
            if ( !m_Randomizer ) {
                NCBI_THROW(CSeqVectorException, eCodingError,
                           "Cannot fill NCBI2na gap without randomizer");
            }
            x_AppendRandomTo2(dst_str, dst_c, dst_pos, src_pos, count,
                              *m_Randomizer,
                              sx_GetGapChar(CSeq_data::e_Ncbi4na,
                                            eCaseConversion_none));
        }
        else {
            const CSeq_data& data = seg.GetRefData();
            bool reverse = seg.GetRefMinusStrand();
            TCoding src_coding = data.Which();
            TCoding dst_coding = CSeq_data::e_Ncbi2na;
            INcbi2naRandomizer* randomizer = 0;
            if ( src_coding != dst_coding && m_Randomizer) {
                randomizer = m_Randomizer.GetPointer();
                _ASSERT(randomizer);
                dst_coding = CSeq_data::e_Ncbi4na;
            }

            const char* table = 0;
            if ( dst_coding != src_coding || reverse ||
                 case_conversion != eCaseConversion_none ) {
                table = sx_GetConvertTable(src_coding, dst_coding,
                                           reverse, case_conversion);
                if ( !table && src_coding != dst_coding ) {
                    NCBI_THROW_FMT(CSeqVectorException, eCodingError,
                                   "Incompatible sequence codings: "<<
                                   src_coding<<" -> "<<dst_coding);
                }
            }

            if ( table || reverse || randomizer ) {
                TSeqPos dataPos;
                if ( reverse ) {
                    // Revert segment offset
                    dataPos = seg.GetRefEndPosition() -
                        (src_pos - seg.GetPosition()) - count;
                }
                else {
                    dataPos = seg.GetRefPosition() +
                        (src_pos - seg.GetPosition());
                }
                _ASSERT(!randomizer && dst_coding == CSeq_data::e_Ncbi2na ||
                        randomizer && dst_coding == CSeq_data::e_Ncbi4na);
                x_AppendAnyTo2(dst_str, dst_c, dst_pos,
                               data, dataPos, count, table, reverse,
                               randomizer, src_pos);
            }
            else {
                _ASSERT(dst_coding == CSeq_data::e_Ncbi2na);
                TSeqPos dataPos = seg.GetRefPosition() +
                    (src_pos - seg.GetPosition());
                x_Append2To2(dst_str, dst_c, dst_pos,
                             data.GetNcbi2na().Get(), dataPos, count);
            }
        }
        ++seg;
        dst_pos += count;
        src_pos += count;
        _ASSERT(dst_str.size() == dst_pos>>2);
    }
    if ( dst_pos&3 ) {
        dst_str += char(dst_c << 2*(-dst_pos&3));
    }
}


CSeqVectorTypes::TResidue
CSeqVectorTypes::sx_GetGapChar(TCoding coding, ECaseConversion case_cvt)
{
    switch (coding) {
    case CSeq_data::e_Iupacna: // DNA - N
        return case_cvt == eCaseConversion_lower? 'n': 'N';
    
    case CSeq_data::e_Ncbi8na: // DNA - bit representation
    case CSeq_data::e_Ncbi4na:
        return 0x0f;  // all bits set == any base

    case CSeq_data::e_Ncbieaa: // Proteins - X
    case CSeq_data::e_Iupacaa:
        return case_cvt == eCaseConversion_lower? 'x': 'X';
    
    case CSeq_data::e_Ncbi8aa: // Protein - numeric representation
    case CSeq_data::e_Ncbistdaa:
        return 21;

    case CSeq_data::e_not_set:
        return 0;     // It's not good to throw an exception here

    case CSeq_data::e_Ncbi2na: // Codings without gap symbols
        // Exception is not good here because it conflicts with CSeqVector_CI.
        return 0xff;

    case CSeq_data::e_Ncbipaa: //### Not sure about this
    case CSeq_data::e_Ncbipna: //### Not sure about this
    default:
        NCBI_THROW_FMT(CSeqVectorException, eCodingError,
                       "Can not indicate gap using the selected coding: "<<
                       coding);
    }
}


DEFINE_STATIC_FAST_MUTEX(s_ConvertTableMutex2);

const char*
CSeqVectorTypes::sx_GetConvertTable(TCoding src, TCoding dst,
                                    bool reverse, ECaseConversion case_cvt)
{
    CFastMutexGuard guard(s_ConvertTableMutex2);
    typedef pair<TCoding, TCoding> TMainConversion;
    typedef pair<bool, ECaseConversion> TConversionFlags;
    typedef pair<TMainConversion, TConversionFlags> TConversionKey;
    typedef vector<char> TConversionTable;
    typedef map<TConversionKey, TConversionTable> TTables;
    static TTables tables;

    TConversionKey key;
    key.first = TMainConversion(src, dst);
    key.second = TConversionFlags(reverse, case_cvt);
    TTables::iterator it = tables.find(key);
    if ( it != tables.end() ) {
        // already created
        return it->second.empty()? 0: &it->second[0];
    }
    TConversionTable& table = tables[key];
    if ( !CSeqportUtil::IsCodeAvailable(src) ||
         !CSeqportUtil::IsCodeAvailable(dst) ) {
        // invalid types
        return 0;
    }

    const size_t COUNT = kMax_UChar+1;
    const unsigned kInvalidCode = kMax_UChar;

    pair<unsigned, unsigned> srcIndex = CSeqportUtil::GetCodeIndexFromTo(src);
    if ( srcIndex.second >= COUNT ) {
        // too large range
        return 0;
    }

    if ( reverse ) {
        // check if src needs complement conversion
        try {
            CSeqportUtil::GetIndexComplement(src, srcIndex.first);
        }
        catch ( exception& /*noComplement*/ ) {
            reverse = false;
        }
    }
    if ( case_cvt != eCaseConversion_none ) {
        // check if dst is text format
        if ( dst != CSeq_data::e_Iupacaa &&
             dst != CSeq_data::e_Iupacna &&
             dst != CSeq_data::e_Ncbieaa ) {
            case_cvt = eCaseConversion_none;
        }
    }

    if ( dst != src ) {
        pair<unsigned, unsigned> dstIndex =
            CSeqportUtil::GetCodeIndexFromTo(dst);
        if ( dstIndex.second >= COUNT ) {
            // too large range
            return 0;
        }

        try {
            // check for types compatibility
            CSeqportUtil::GetMapToIndex(src, dst, srcIndex.first);
        }
        catch ( exception& /*badType*/ ) {
            // incompatible types
            return 0;
        }
    }
    else if ( !reverse && case_cvt == eCaseConversion_none ) {
        // no need to convert at all
        return 0;
    }

    table.resize(COUNT, char(kInvalidCode));
    bool different = false;
    for ( unsigned i = srcIndex.first; i <= srcIndex.second; ++i ) {
        try {
            unsigned code = i;
            if ( reverse ) {
                code = CSeqportUtil::GetIndexComplement(src, code);
            }
            if ( dst != src ) {
                code = CSeqportUtil::GetMapToIndex(src, dst, code);
            }
            code = min(kInvalidCode, code);
            if ( case_cvt == eCaseConversion_upper ) {
                code = toupper((unsigned char) code);
            }
            else if( case_cvt == eCaseConversion_lower ) {
                code = tolower((unsigned char) code);
            }
            if ( code != i ) {
                different = true;
            }
            table[i] = char(code);
        }
        catch ( exception& /*noConversion or noComplement*/ ) {
            different = true;
        }
    }
    if ( !different ) {
        table.clear();
        return 0;
    }
    return &table[0];
}


void CSeqVector::SetStrand(ENa_strand strand)
{
    if ( strand != m_Strand ) {
        m_Strand = strand;
        m_Iterator.reset();
    }
}


void CSeqVector::SetCoding(TCoding coding)
{
    if (m_Coding != coding) {
        m_Coding = coding;
        m_Iterator.reset();
    }
}


void CSeqVector::SetIupacCoding(void)
{
    SetCoding(IsProtein()? CSeq_data::e_Iupacaa: CSeq_data::e_Iupacna);
}


void CSeqVector::SetNcbiCoding(void)
{
    SetCoding(IsProtein()? CSeq_data::e_Ncbistdaa: CSeq_data::e_Ncbi4na);
}


void CSeqVector::SetCoding(EVectorCoding coding)
{
    switch ( coding ) {
    case CBioseq_Handle::eCoding_Iupac:
        SetIupacCoding();
        break;
    case CBioseq_Handle::eCoding_Ncbi:
        SetNcbiCoding();
        break;
    default:
        SetCoding(CSeq_data::e_not_set);
        break;
    }
}


void CSeqVector::SetRandomizeAmbiguities(void)
{
    CRandom random_gen;
    x_InitRandomizer(random_gen);
}


void CSeqVector::SetRandomizeAmbiguities(Uint4 seed)
{
    CRandom random_gen(seed);
    x_InitRandomizer(random_gen);
}


void CSeqVector::SetRandomizeAmbiguities(CRandom& random_gen)
{
    x_InitRandomizer(random_gen);
}


void CSeqVector::x_InitRandomizer(CRandom& random_gen)
{
    CRef<INcbi2naRandomizer> randomizer(new CNcbi2naRandomizer(random_gen));
    SetRandomizeAmbiguities(randomizer);
}


void CSeqVector::SetRandomizeAmbiguities(CRef<INcbi2naRandomizer> randomizer)
{
    if ( m_Randomizer != randomizer ) {
        m_Randomizer = randomizer;
        m_Iterator.reset();
    }
}


void CSeqVector::SetNoAmbiguities(void)
{
    SetRandomizeAmbiguities(null);
}


END_SCOPE(objects)
END_NCBI_SCOPE
