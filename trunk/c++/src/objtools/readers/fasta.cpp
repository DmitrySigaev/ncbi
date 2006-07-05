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
* Authors:  Aaron Ucko, NCBI
*
* File Description:
*   Reader for FASTA-format sequences.  (The writer is CFastaOStream, in
*   src/objmgr/util/sequence.cpp.)
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objtools/readers/fasta.hpp>
#include "fasta_aln_builder.hpp"
#include <objtools/readers/reader_exception.hpp>

#include <corelib/ncbiutil.hpp>
#include <util/format_guess.hpp>
#include <util/sequtil/sequtil_convert.hpp>

#include <objects/general/Object_id.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/NCBIeaa.hpp>
#include <objects/seq/IUPACna.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/seqport_util.hpp>

#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Seq_align.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqloc/Seq_point.hpp>

#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <ctype.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

NCBI_PARAM_DEF(bool, READ_FASTA, USE_NEW_IMPLEMENTATION, false);

static
CRef<CSeq_entry> s_ReadFasta_OLD(CNcbiIstream& in, TReadFastaFlags flags,
                                 int* counter,
                                 vector<CConstRef<CSeq_loc> >* lcv);

template <typename TStack>
class CTempPusher
{
public:
    typedef typename TStack::value_type TValue;
    CTempPusher(TStack& s, const TValue& v) : m_Stack(s) { s.push(v); }
    ~CTempPusher() { _ASSERT( !m_Stack.empty() );  m_Stack.pop(); }    

private:
    TStack& m_Stack;
};

typedef CTempPusher<stack<CFastaReader::TFlags> > CFlagGuard;

// The FASTA reader uses these heavily, but the standard versions
// aren't inlined on as many configurations as one might hope, and we
// don't necessarily want locale-dependent behavior anyway.

inline bool s_ASCII_IsUpper(unsigned char c)
{
    return c >= 'A'  &&  c <= 'Z';
}

inline bool s_ASCII_IsLower(unsigned char c)
{
    return c >= 'a'  &&  c <= 'z';
}

inline bool s_ASCII_IsAlpha(unsigned char c)
{
    return s_ASCII_IsUpper(c)  ||  s_ASCII_IsLower(c);
}

inline unsigned char s_ASCII_ToUpper(unsigned char c)
{
    return s_ASCII_IsLower(c) ? c + 'A' - 'a' : c;
}

CFastaReader::CFastaReader(ILineReader& reader, TFlags flags)
    : m_LineReader(&reader), m_MaskVec(0), m_IDGenerator(new CSeqIdGenerator)
{
    m_Flags.push(flags);
}

CFastaReader::~CFastaReader(void)
{
    _ASSERT(m_Flags.size() == 1);
}

CRef<CSeq_entry> CFastaReader::ReadOneSeq(void)
{
    m_CurrentSeq.Reset(new CBioseq);
    // m_CurrentMask.Reset();
    m_SeqData.erase();
    m_Gaps.clear();
    m_CurrentPos = 0;
    m_MaskRangeStart = kInvalidSeqPos;
    if ( !TestFlag(fInSegSet) ) {
        if (m_MaskVec  &&  m_NextMask.IsNull()) {
            m_MaskVec->push_back(SaveMask());
        }
        m_CurrentMask.Reset(m_NextMask);
        m_NextMask.Reset();
        m_SegmentBase = 0;
        m_Offset = 0;
    }
    m_CurrentGapLength = m_TotalGapLength = 0;

    bool need_defline = true;
    while ( !GetLineReader().AtEOF() ) {
        char c = GetLineReader().PeekChar();
        if (GetLineReader().AtEOF()) {
            NCBI_THROW2(CObjReaderParseException, eEOF,
                        "CFastaReader: Input stream no longer valid",
                        StreamPosition());
        }
        if (c == '>') {
            if (need_defline) {
                ParseDefLine(*++GetLineReader());
                need_defline = false;
            } else {
                // start of the next sequence
                break;
            }
        } else if (strchr("#!\n\r", c)) {
            // no content, just a comment or blank line
            ++GetLineReader();
            continue;
        } else if (c == '[') {
            return x_ReadSegSet();
        } else if (c == ']') {
            if (need_defline) {
                NCBI_THROW2(CObjReaderParseException, eEOF,
                            "CFastaReader: Reached end of segmented set",
                            StreamPosition());
            } else {
                break;
            }
        } else if (need_defline) {
            if (TestFlag(fDLOptional)) {
                ParseDefLine(">");
            } else {
                NCBI_THROW2(CObjReaderParseException, eNoDefline,
                            "CFastaReader: Input doesn't start with"
                            " a defline or comment",
                            StreamPosition());
            }
        } else if (TestFlag(fNoSeqData)) {
            ++GetLineReader();
        } else {
            ParseDataLine(*++GetLineReader());
        }
    }

    AssembleSeq();
    CRef<CSeq_entry> entry(new CSeq_entry);
    entry->SetSeq(*m_CurrentSeq);
    return entry;
}

CRef<CSeq_entry> CFastaReader::x_ReadSegSet(void)
{
    CFlagGuard guard(m_Flags, GetFlags() | fInSegSet);
    CRef<CSeq_entry> entry(new CSeq_entry), master(new CSeq_entry), parts;

    _ASSERT(GetLineReader().PeekChar() == '[');
    try {
        ++GetLineReader();
        parts = ReadSet();
    } catch (CObjReaderParseException&) {
        if (GetLineReader().AtEOF()) {
            throw;
        } else if (GetLineReader().PeekChar() == ']') {
            ++GetLineReader();
        } else {
            throw;
        }
    }
    if (GetLineReader().AtEOF()) {
        NCBI_THROW2(CObjReaderParseException, eBadSegSet,
                    "CFastaReader: Segmented set not properly terminated",
                    StreamPosition());
    } else if (!parts->IsSet()  ||  parts->GetSet().GetSeq_set().empty()) {
        NCBI_THROW2(CObjReaderParseException, eBadSegSet,
                    "CFastaReader: Segmented set contains no sequences",
                    StreamPosition());
    }

    const CBioseq& first_seq = parts->GetSet().GetSeq_set().front()->GetSeq();
    CBioseq& master_seq = master->SetSeq();
    CSeq_inst& inst = master_seq.SetInst();
    // XXX - work out less generic ID?
    CRef<CSeq_id> id(SetIDGenerator().GenerateID(true));
    if (m_CurrentMask) {
        m_CurrentMask->SetId(*id);
    }
    master_seq.SetId().push_back(id);
    inst.SetRepr(CSeq_inst::eRepr_seg);
    inst.SetMol(first_seq.GetInst().GetMol());
    inst.SetLength(GetCurrentPos(ePosWithGapsAndSegs));
    CSeg_ext& ext = inst.SetExt().SetSeg();
    ITERATE (CBioseq_set::TSeq_set, it, parts->GetSet().GetSeq_set()) {
        CRef<CSeq_loc>      seg_loc(new CSeq_loc);
        const CBioseq::TId& seg_ids = (*it)->GetSeq().GetId();
        CRef<CSeq_id>       seg_id  = FindBestChoice(seg_ids, CSeq_id::Score);
        seg_loc->SetWhole(*seg_id);
        ext.Set().push_back(seg_loc);
    }

    parts->SetSet().SetClass(CBioseq_set::eClass_parts);
    entry->SetSet().SetClass(CBioseq_set::eClass_segset);
    entry->SetSet().SetSeq_set().push_back(master);
    entry->SetSet().SetSeq_set().push_back(parts);
    return entry;
}

CRef<CSeq_entry> CFastaReader::ReadSet(int max_seqs)
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    if (TestFlag(fOneSeq)) {
        max_seqs = 1;
    }
    for (int i = 0;  i < max_seqs  &&  !GetLineReader().AtEOF();  ++i) {
        try {
            CRef<CSeq_entry> entry2(ReadOneSeq());
            if (max_seqs == 1) {
                return entry2;
            }
            entry->SetSet().SetSeq_set().push_back(entry2);
        } catch (CObjReaderParseException& e) {
            if (e.GetErrCode() == CObjReaderParseException::eEOF) {
                break;
            } else {
                throw;
            }
        }
    }
    if (entry->IsSet()  &&  entry->GetSet().GetSeq_set().size() == 1) {
        return entry->SetSet().SetSeq_set().front();
    } else {
        return entry;
    }
}

CRef<CSeq_loc> CFastaReader::SaveMask(void)
{
    m_NextMask.Reset(new CSeq_loc);
    return m_NextMask;
}

void CFastaReader::SetIDGenerator(CSeqIdGenerator& gen)
{
    m_IDGenerator.Reset(&gen);
}

void CFastaReader::ParseDefLine(const TStr& s)
{
    size_t start = 1, pos, len = s.length(), title_start;
    do {
        bool has_id = true;
        if (TestFlag(fNoParseID)) {
            title_start = start;
        } else {
            for (pos = start;  pos < len;  ++pos) {
                if ((unsigned char) s[pos] <= ' ') { // assumes ASCII
                    break;
                }
            }
            size_t range_len = ParseRange(TStr(s.data(), start, pos - start));
            has_id = ParseIDs(TStr(s.data(), start, pos - start - range_len));
            title_start = pos + 1;
            // trim leading whitespace from title (is this appropriate?)
            while (isspace((unsigned char) s[title_start])) {
                ++title_start;
            }
        }
        for (pos = title_start + 1;  pos < len;  ++pos) {
            if ((unsigned char) s[pos] < ' ') {
                break;
            }
        }
        if ( !has_id ) {
            // no IDs after all, so take the whole line as a title
            // (done now rather than earlier to avoid rescanning)
            title_start = start;
        }
        if (pos <= len) {
            ParseTitle(TStr(s.data(), title_start, pos - title_start));
        }
        start = pos + 1;
    } while (TestFlag(fAllSeqIds)  &&  start < len  &&  s[start - 1] == '\1');

    if (GetIDs().empty()) {
        // No [usable] IDs
        if (TestFlag(fRequireID)) {
            NCBI_THROW2(CObjReaderParseException, eNoIDs,
                        "CFastaReader: Defline lacks a proper ID",
                        StreamPosition());
        }
        GenerateID();
    }

    m_BestID = FindBestChoice(GetIDs(), CSeq_id::Score);
}

bool CFastaReader::ParseIDs(const TStr& s)
{
    string str(s.data(), s.length());
    CBioseq::TId& ids = SetIDs();
    // CBioseq::TId  old_ids = ids;
    size_t count = 0;
    if (str.find('|') != NPOS) {
        try {
            count = CSeq_id::ParseFastaIds(ids, str, true); // be generous
        } catch (CSeqIdException&) {
            // swap(ids, old_ids);
        }
    }
    if ( !count ) {
        if (IsValidLocalID(str)) {
            ids.push_back(CRef<CSeq_id>(new CSeq_id(CSeq_id::e_Local, str)));
        } else {
            return false;
        }
    }
    return true;
}

size_t CFastaReader::ParseRange(const TStr& s)
{
    bool    on_start = false;
    TSeqPos start = 0, end = 0, mult = 1;
    size_t  pos;
    for (pos = s.length() - 1;  pos > 0;  --pos) {
        unsigned char c = s[pos];
        if (c >= '0'  &&  c <= '9') {
            if (on_start) {
                start += (c - '0') * mult;
            } else {
                end += (c - '0') * mult;
            }
            mult *= 10;
        } else if (c == '-'  &&  !on_start  &&  mult > 1) {
            on_start = true;
            mult = 1;
        } else if (c == ':'  &&  on_start  &&  mult > 1) {
            break;
        } else {
            return 0; // syntax error
        }
    }
    if (start > end  ||  s[pos] != ':') {
        return 0;
    }
    if (start > 0) {
        SGap gap = { 0, start };
        m_Gaps.push_back(gap);
    }
    m_ExpectedEnd = end;
    return s.length() - pos;
}

void CFastaReader::ParseTitle(const TStr& s)
{
    CRef<CSeqdesc> desc(new CSeqdesc);
    desc->SetTitle().assign(s.data(), s.length());
    m_CurrentSeq->SetDescr().Set().push_back(desc);
}

bool CFastaReader::IsValidLocalID(const string& s)
{
    static const char* const kLegal =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.";
    return (!s.empty()  &&  s.find_first_not_of(kLegal) == NPOS);
}

void CFastaReader::GenerateID(void)
{
    SetIDs().push_back(SetIDGenerator().GenerateID(true));
}

void CFastaReader::ParseDataLine(const TStr& s)
{
    size_t len = s.length();
    if (m_SeqData.capacity() < m_SeqData.size() + len) {
        // ensure exponential capacity growth to avoid quadratic runtime
        m_SeqData.reserve(2 * max(m_SeqData.capacity(), len));
    }
    m_SeqData.resize(m_CurrentPos + len);
    for (size_t pos = 0;  pos < len;  ++pos) {
        unsigned char c = s[pos];
        if (c == '-'  &&  TestFlag(fParseGaps)) {
            CloseMask();
            // OpenGap();
            size_t pos2 = pos + 1;
            while (pos2 < len  &&  s[pos2] == '-') {
                ++pos2;
            }
            m_CurrentGapLength += pos2 - pos;
            pos = pos2 - 1;
        } else if (s_ASCII_IsAlpha(c)  ||  c == '-'  ||  c == '*') {
            CloseGap();
            if (s_ASCII_IsLower(c)) {
                m_SeqData[m_CurrentPos] = s_ASCII_ToUpper(c);
                CloseMask();
            } else {
                m_SeqData[m_CurrentPos] = c;
                CloseMask();
            }
            ++m_CurrentPos;
        } else if (c == ';') {
            // comment -- ignore rest of line
            break;
        } else if ( !isspace(c) ) {
            ERR_POST(Warning << "CFastaReader: Ignoring invalid residue "
                     << c << " at position " << (StreamPosition() + pos - len));
        }
    }
    m_SeqData.resize(m_CurrentPos);
}

void CFastaReader::x_CloseGap(TSeqPos len)
{
    _ASSERT(len > 0  &&  TestFlag(fParseGaps));
    if (TestFlag(fAligning)) {
        TSeqPos pos = GetCurrentPos(ePosWithGapsAndSegs);
        m_Starts[pos + m_Offset][m_Row] = CFastaAlignmentBuilder::kNoPos;
        m_Offset += len;
        m_Starts[pos + m_Offset][m_Row] = pos;
    } else {
        TSeqPos pos = GetCurrentPos(eRawPos);
        // Special case -- treat a lone hyphen at the end of a line as
        // a gap of unknown length.
        if (len == 1) {
            TSeqPos l = m_SeqData.length();
            if (l == pos  ||  l == pos + (*GetLineReader()).length()) {
                len = 0;
            }
        }
        SGap gap = { pos, len };
        m_Gaps.push_back(gap);
        m_TotalGapLength += len;
        m_CurrentGapLength = 0;
    }
}

void CFastaReader::x_OpenMask(void)
{
    _ASSERT(m_MaskRangeStart == kInvalidSeqPos);
    m_MaskRangeStart = GetCurrentPos(ePosWithGapsAndSegs);
}

void CFastaReader::x_CloseMask(void)
{
    _ASSERT(m_MaskRangeStart != kInvalidSeqPos);
    m_CurrentMask->SetPacked_int().AddInterval
        (GetBestID(), m_MaskRangeStart, GetCurrentPos(ePosWithGapsAndSegs) - 1);
    m_MaskRangeStart = kInvalidSeqPos;
}

void CFastaReader::AssembleSeq(void)
{
    CSeq_inst& inst = m_CurrentSeq->SetInst();

    CloseGap();
    CloseMask();
    if (TestFlag(fInSegSet)) {
        m_SegmentBase += GetCurrentPos(ePosWithGaps);
    }
    AssignMolType();
    if (m_Gaps.empty()) {
        _ASSERT(m_TotalGapLength == 0);
        if (m_SeqData.empty()) {
            inst.SetRepr(CSeq_inst::eRepr_virtual);
        } else {
            inst.SetRepr(CSeq_inst::eRepr_raw);
            inst.SetLength(GetCurrentPos(eRawPos));
            SaveSeqData(inst.SetSeq_data(), m_SeqData);
        }
    } else {
        CDelta_ext& delta_ext = inst.SetExt().SetDelta();
        inst.SetRepr(CSeq_inst::eRepr_delta);
        inst.SetLength(GetCurrentPos(ePosWithGaps));
        SIZE_TYPE n = m_Gaps.size();
        for (SIZE_TYPE i = 0;  i < n;  ++i) {
            if (i == 0  &&  m_Gaps[i].pos > 0) {
                CRef<CDelta_seq> seq0_ds(new CDelta_seq);
                CSeq_literal&    seq0_lit = seq0_ds->SetLiteral();
                seq0_lit.SetLength(m_Gaps[i].pos);
                SaveSeqData(seq0_lit.SetSeq_data(),
                            TStr(m_SeqData, 0, m_Gaps[i].pos));
                delta_ext.Set().push_back(seq0_ds);
            }

            CRef<CDelta_seq> gap_ds(new CDelta_seq);
            if (m_Gaps[i].len == 0) { // unknown length
                gap_ds->SetLoc().SetNull();
            } else {
                gap_ds->SetLiteral().SetLength(m_Gaps[i].len);
            }
            delta_ext.Set().push_back(gap_ds);

            TSeqPos next_start = (i == n-1) ? m_CurrentPos : m_Gaps[i+1].pos;
            if (next_start == m_Gaps[i].pos) {
                continue;
            }

            CRef<CDelta_seq> seq_ds(new CDelta_seq);
            CSeq_literal&    seq_lit = seq_ds->SetLiteral();
            TSeqPos          seq_len = next_start - m_Gaps[i].pos;
            seq_lit.SetLength(seq_len);
            SaveSeqData(seq_lit.SetSeq_data(),
                        TStr(m_SeqData, m_Gaps[i].pos, seq_len));
            delta_ext.Set().push_back(seq_ds);
        }
    }
}

void CFastaReader::AssignMolType(void)
{
    CSeq_inst& inst = m_CurrentSeq->SetInst();
    // Did the user insist on a specific type?
    if (TestFlag(fForceType)) {
        switch (GetFlags() & (fAssumeNuc | fAssumeProt)) {
        case fAssumeNuc:   inst.SetMol(CSeq_inst::eMol_na);  break;
        case fAssumeProt:  inst.SetMol(CSeq_inst::eMol_aa);  break;
        default:           _TROUBLE;
        }
        return;
    }
    
    // Does any ID imply a specific type?
    ITERATE (CBioseq::TId, it, GetIDs()) {
        CSeq_id::EAccessionInfo acc_info = (*it)->IdentifyAccession();
        if (acc_info & CSeq_id::fAcc_nuc) {
            _ASSERT ( !(acc_info & CSeq_id::fAcc_prot) );
            inst.SetMol(CSeq_inst::eMol_na);
            return;
        } else if (acc_info & CSeq_id::fAcc_prot) {
            inst.SetMol(CSeq_inst::eMol_aa);
            return;
        }
        // XXX - verify that other IDs aren't contradictory?
    }

    // Did the user specify a default type?
    switch (GetFlags() & (fAssumeNuc | fAssumeProt)) {
    case fAssumeNuc:   inst.SetMol(CSeq_inst::eMol_na);  return;
    case fAssumeProt:  inst.SetMol(CSeq_inst::eMol_aa);  return;
    }

    if (m_SeqData.empty()) {
        // nothing else to go on, but that's OK
        return;
    }

    // Do the residue frequencies suggest a specific type?
    SIZE_TYPE length = min(m_SeqData.length(), SIZE_TYPE(4096));
    switch (CFormatGuess::SequenceType(m_SeqData.data(), length)) {
    case CFormatGuess::eNucleotide:  inst.SetMol(CSeq_inst::eMol_na);  return;
    case CFormatGuess::eProtein:     inst.SetMol(CSeq_inst::eMol_aa);  return;
    default:
        NCBI_THROW2(CObjReaderParseException, eAmbiguous,
                    "CFastaReader: unable to determine sequence type",
                    StreamPosition());
    }
}

void CFastaReader::SaveSeqData(CSeq_data& seq_data, const TStr& raw_string)
{
    SIZE_TYPE len = raw_string.length();
    if (m_CurrentSeq->IsAa()) {
        seq_data.SetNcbieaa().Set().assign(raw_string.data(),
                                           len);
    } else {
        // nucleotide -- pack to ncbi2na, or at least ncbi4na
        vector<char> v((len+1) / 2, '\0');
        CSeqUtil::ECoding coding;
        CSeqConvert::Pack(raw_string.data(), len, CSeqUtil::e_Iupacna, &v[0],
                          coding);
        if (coding == CSeqUtil::e_Ncbi2na) {
            seq_data.SetNcbi2na().Set().assign(v.begin(),
                                               v.begin() + (len + 3) / 4);
        } else {
            swap(seq_data.SetNcbi4na().Set(), v);
        }
    }
}

CRef<CSeq_entry> CFastaReader::ReadAlignedSet(int reference_row)
{
    TIds             ids;
    CRef<CSeq_entry> entry = x_ReadSeqsToAlign(ids);
    CRef<CSeq_annot> annot(new CSeq_annot);

    if (reference_row >= 0) {
        x_AddPairwiseAlignments(*annot, ids, reference_row);
    } else {
        x_AddMultiwayAlignment(*annot, ids);
    }
    entry->SetSet().SetAnnot().push_back(annot);
    return entry;
}

CRef<CSeq_entry> CFastaReader::x_ReadSeqsToAlign(TIds& ids)
{
    CRef<CSeq_entry> entry(new CSeq_entry);
    vector<TSeqPos>  lengths;

    CFlagGuard guard(m_Flags, GetFlags() | fAligning | fParseGaps);

    for (m_Row = 0, m_Starts.clear();  !GetLineReader().AtEOF();  ++m_Row) {
        try {
            // must mark m_Starts prior to reading in case of leading gaps
            m_Starts[0][m_Row] = 0;
            CRef<CSeq_entry> entry2(ReadOneSeq());
            entry->SetSet().SetSeq_set().push_back(entry2);
            CRef<CSeq_id> id(new CSeq_id);
            id->Assign(GetBestID());
            ids.push_back(id);
            lengths.push_back(GetCurrentPos(ePosWithGapsAndSegs) + m_Offset);
            _ASSERT(lengths.size() == size_t(m_Row) + 1);
            // redundant if there was a trailing gap, but that should be okay
            m_Starts[lengths[m_Row]][m_Row] = CFastaAlignmentBuilder::kNoPos;
        } catch (CObjReaderParseException&) {
            if (GetLineReader().AtEOF()) {
                break;
            } else {
                throw;
            }
        }
    }
    // check whether lengths are all equal, and warn if they differ?
    return entry;
}

void CFastaReader::x_AddPairwiseAlignments(CSeq_annot& annot, const TIds& ids,
                                           TRowNum reference_row)
{
    typedef CFastaAlignmentBuilder TBuilder;
    typedef CRef<TBuilder>         TBuilderRef;

    TRowNum             rows = m_Row;
    vector<TBuilderRef> builders(rows);
    
    for (TRowNum r = 0;  r < rows;  ++r) {
        if (r != reference_row) {
            builders[r].Reset(new TBuilder(ids[reference_row], ids[r]));
        }
    }
    ITERATE (TStartsMap, it, m_Starts) {
        const TSubMap& submap = it->second;
        TSubMap::const_iterator rr_it2 = submap.find(reference_row);
        if (rr_it2 == submap.end()) { // reference unchanged
            ITERATE (TSubMap, it2, submap) {
                int r = it2->first;
                _ASSERT(r != reference_row);
                builders[r]->AddData(it->first, TBuilder::kContinued,
                                     it2->second);
            }
        } else { // reference changed; all rows need updating
            TSubMap::const_iterator it2 = submap.begin();
            for (TRowNum r = 0;  r < rows;  ++r) {
                if (it2 != submap.end()  &&  r == it2->first) {
                    if (r != reference_row) {
                        builders[r]->AddData(it->first, rr_it2->second,
                                             it2->second);
                    }
                    ++it2;
                } else {
                    _ASSERT(r != reference_row);
                    builders[r]->AddData(it->first, rr_it2->second,
                                         TBuilder::kContinued);
                }
            }
        }
    }

    // finalize and store the alignments
    CSeq_annot::TData::TAlign& annot_align = annot.SetData().SetAlign();
    for (TRowNum r = 0;  r < rows;  ++r) {
        if (r != reference_row) {
            annot_align.push_back(builders[r]->GetCompletedAlignment());
        }
    }
}

void CFastaReader::x_AddMultiwayAlignment(CSeq_annot& annot, const TIds& ids)
{
    TRowNum              rows = m_Row;
    CRef<CSeq_align>     sa(new CSeq_align);
    CDense_seg&          ds   = sa->SetSegs().SetDenseg();
    CDense_seg::TStarts& dss  = ds.SetStarts();

    sa->SetType(CSeq_align::eType_not_set);
    sa->SetDim(rows);
    ds.SetDim(rows);
    ds.SetIds() = ids;
    dss.reserve((m_Starts.size() - 1) * rows);

    TSeqPos old_len = 0;
    for (TStartsMap::const_iterator next = m_Starts.begin(), it = next++;
         next != m_Starts.end();  it = next++) {
        TSeqPos len = next->first - it->first;
        _ASSERT(len > 0);
        ds.SetLens().push_back(len);

        const TSubMap&          submap = it->second;
        TSubMap::const_iterator it2 = submap.begin();
        for (TRowNum r = 0;  r < rows;  ++r) {
            if (it2 != submap.end()  &&  r == it2->first) {
                dss.push_back(it2->second);
                ++it2;
            } else {
                _ASSERT(dss.size() >= size_t(rows)  &&  old_len > 0);
                TSignedSeqPos last_pos = dss[dss.size() - rows];
                if (last_pos == CFastaAlignmentBuilder::kNoPos) {
                    dss.push_back(last_pos);
                } else {
                    dss.push_back(last_pos + old_len);
                }
            }
        }

        it = next;
        old_len = len;
    }
    ds.SetNumseg(ds.GetLens().size());
    annot.SetData().SetAlign().push_back(sa);
}


CRef<CSeq_id> CSeqIdGenerator::GenerateID(bool advance)
{
    CNcbiOstrstream oss;
    oss << m_Prefix
        << (advance ? m_Counter.Add(1) - 1 : m_Counter.Get())
        << m_Suffix;
    return CRef<CSeq_id>(new CSeq_id(CSeq_id::e_Local,
                                     CNcbiOstrstreamToString(oss)));
}

CRef<CSeq_id> CSeqIdGenerator::GenerateID(void) const
{
    return const_cast<CSeqIdGenerator*>(this)->GenerateID(false);
}


class CCounterManager
{
public:
    CCounterManager(CSeqIdGenerator& generator, int* counter)
        : m_Generator(generator), m_Counter(counter)
        { if (counter) { generator.SetCounter(*counter); } }
    ~CCounterManager()
        { if (m_Counter) { *m_Counter = m_Generator.GetCounter(); } }

private:
    CSeqIdGenerator& m_Generator;
    int*             m_Counter;
};

CRef<CSeq_entry> ReadFasta(CNcbiIstream& in, TReadFastaFlags flags,
                           int* counter, vector<CConstRef<CSeq_loc> >* lcv)
{
    typedef NCBI_PARAM_TYPE(READ_FASTA, USE_NEW_IMPLEMENTATION) TParam_NewImpl;

    TParam_NewImpl new_impl;

    if (new_impl.Get()) {
        CStreamLineReader lr(in);
        CFastaReader      reader(lr, flags);
        CCounterManager   counter_manager(reader.SetIDGenerator(), counter);
        if (lcv) {
            reader.SaveMasks(reinterpret_cast<CFastaReader::TMasks*>(lcv));
        }
        return reader.ReadSet();
    } else {
        return s_ReadFasta_OLD(in, flags, counter, lcv);
    }
}


IFastaEntryScan::~IFastaEntryScan()
{
}


class CFastaMapper : public CFastaReader
{
public:
    typedef CFastaReader TParent;

    CFastaMapper(ILineReader& reader, SFastaFileMap* fasta_map, TFlags flags);

protected:
    void ParseDefLine(const TStr& s);
    void ParseTitle(const TStr& s);
    void AssembleSeq(void);

private:
    SFastaFileMap*             m_Map;
    SFastaFileMap::SFastaEntry m_MapEntry;
};

CFastaMapper::CFastaMapper(ILineReader& reader, SFastaFileMap* fasta_map,
                           TFlags flags)
    : TParent(reader, flags), m_Map(fasta_map)
{
    _ASSERT(fasta_map);
    fasta_map->file_map.resize(0);
}

void CFastaMapper::ParseDefLine(const TStr& s)
{
    TParent::ParseDefLine(s); // We still want the default behavior.
    m_MapEntry.seq_id = GetIDs().front()->AsFastaString(); // XXX -- GetBestID?
    m_MapEntry.all_seq_ids.resize(0);
    ITERATE (CBioseq::TId, it, GetIDs()) {
        m_MapEntry.all_seq_ids.push_back((*it)->AsFastaString());
    }
    m_MapEntry.stream_offset = StreamPosition() - s.length();
}

void CFastaMapper::ParseTitle(const TStr& s)
{
    TParent::ParseTitle(s);
    m_MapEntry.description = s;
}

void CFastaMapper::AssembleSeq(void)
{
    TParent::AssembleSeq();
    m_Map->file_map.push_back(m_MapEntry);
}


void ReadFastaFileMap(SFastaFileMap* fasta_map, CNcbiIfstream& input)
{
    static const CFastaReader::TFlags kFlags
        = CFastaReader::fAssumeNuc | CFastaReader::fAllSeqIds
        | CFastaReader::fNoSeqData;

    if ( !input.is_open() ) {
        return;
    }

    CStreamLineReader lr(input);
    CFastaMapper      mapper(lr, fasta_map, kFlags);
    mapper.ReadSet();
}


void ScanFastaFile(IFastaEntryScan* scanner, 
                   CNcbiIfstream&   input,
                   TReadFastaFlags  fread_flags)
{
    if ( !input.is_open() ) {
        return;
    }

    CStreamLineReader lr(input);
    CFastaReader      reader(lr, fread_flags);

    while ( !lr.AtEOF() ) {
        try {
            CNcbiStreampos   pos = lr.GetPosition();
            CRef<CSeq_entry> se  = reader.ReadOneSeq();
            if (se->IsSeq()) {
                scanner->EntryFound(se, pos);
            }
        } catch (CObjReaderParseException& e) {
            if ( !lr.AtEOF() ) {
                throw;
            }
        }
    }
}


/// Everything below this point is specific to the old implementation.

static SIZE_TYPE s_EndOfFastaID(const string& str, SIZE_TYPE pos)
{
    SIZE_TYPE vbar = str.find('|', pos);
    if (vbar == NPOS) {
        return NPOS; // bad
    }

    CSeq_id::E_Choice choice =
        CSeq_id::WhichInverseSeqId(str.substr(pos, vbar - pos).c_str());

#if 1
    if (choice != CSeq_id::e_not_set) {
        SIZE_TYPE vbar_prev = vbar;
        int count;
        for (count=0; ; ++count, vbar_prev = vbar) {
            vbar = str.find('|', vbar_prev + 1);
            if (vbar == NPOS) {
                break;
            }
            choice = CSeq_id::WhichInverseSeqId(
                str.substr(vbar_prev + 1, vbar - vbar_prev - 1).c_str());
            if (choice != CSeq_id::e_not_set) {
                vbar = vbar_prev;
                break;
            }
        }
    } else {
        return NPOS; // bad
    }
#else
    switch (choice) {
    case CSeq_id::e_Patent: case CSeq_id::e_Other: // 3 args
        vbar = str.find('|', vbar + 1);
        // intentional fall-through - this allows us to correctly
        // calculate the number of '|' separations for FastA IDs

    case CSeq_id::e_Genbank:   case CSeq_id::e_Embl:    case CSeq_id::e_Pir:
    case CSeq_id::e_Swissprot: case CSeq_id::e_General: case CSeq_id::e_Ddbj:
    case CSeq_id::e_Prf:       case CSeq_id::e_Pdb:     case CSeq_id::e_Tpg:
    case CSeq_id::e_Tpe:       case CSeq_id::e_Tpd:
        // 2 args
        if (vbar == NPOS) {
            return NPOS; // bad
        }
        vbar = str.find('|', vbar + 1);
        // intentional fall-through - this allows us to correctly
        // calculate the number of '|' separations for FastA IDs

    case CSeq_id::e_Local: case CSeq_id::e_Gibbsq: case CSeq_id::e_Gibbmt:
    case CSeq_id::e_Giim:  case CSeq_id::e_Gi:
        // 1 arg
        if (vbar == NPOS) {
            if (choice == CSeq_id::e_Other) {
                // this is acceptable - member is optional
                break;
            }
            return NPOS; // bad
        }
        vbar = str.find('|', vbar + 1);
        break;

    default: // unrecognized or not set
        return NPOS; // bad
    }
#endif

    return (vbar == NPOS) ? str.size() : vbar;
}


static bool s_IsValidLocalID(const string& s)
{
    static const char* const kLegal =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.";
    return (!s.empty()  &&  s.find_first_not_of(kLegal) == NPOS);
}


static void s_FixSeqData(CBioseq* seq)
{
    _ASSERT(seq);
    CSeq_inst& inst = seq->SetInst();
    switch (inst.GetRepr()) {
    case CSeq_inst::eRepr_delta:
    {
        TSeqPos length = 0;
        NON_CONST_ITERATE (CDelta_ext::Tdata, it,
                           inst.SetExt().SetDelta().Set()) {
            if ((*it)->IsLiteral()  &&  (*it)->GetLiteral().IsSetSeq_data()) {
                CSeq_literal& lit  = (*it)->SetLiteral();
                CSeq_data&    data = lit.SetSeq_data();
                if (data.IsIupacna()) {
                    lit.SetLength(data.GetIupacna().Get().size());
                    CSeqportUtil::Pack(&data);
                } else {
                    string& s = data.SetNcbieaa().Set();
                    lit.SetLength(s.size());
                    s.reserve(s.size()); // free extra allocation
                }
                length += lit.GetLength();
            }
        }
        break;
    }
    case CSeq_inst::eRepr_raw:
    {
        CSeq_data& data = inst.SetSeq_data();
        if (data.IsIupacna()) {
            inst.SetLength(data.GetIupacna().Get().size());
            CSeqportUtil::Pack(&data);
        } else {
            string& s = data.SetNcbieaa().Set();
            inst.SetLength(s.size());
            s.reserve(s.size()); // free extra allocation
        }        
        break;
    }
    default: // especially not_set!
        break;
    }
}


static void s_AddData(CSeq_inst& inst, const string& residues)
{
    CRef<CSeq_data> data;
    if (inst.IsSetExt()  &&  inst.GetExt().IsDelta()) {
        CDelta_ext::Tdata& delta_data = inst.SetExt().SetDelta().Set();
        if (delta_data.empty()  ||  !delta_data.back()->IsLiteral()
            ||  !delta_data.back()->GetLiteral().IsSetSeq_data()) {
            CRef<CDelta_seq> delta_seq(new CDelta_seq);
            delta_data.push_back(delta_seq);
            data = &delta_seq->SetLiteral().SetSeq_data();
        } else {
            data = &delta_data.back()->SetLiteral().SetSeq_data();
        }
    } else {
        data = &inst.SetSeq_data();
    }

    string* s = 0;
    if (inst.GetMol() == CSeq_inst::eMol_aa) {
        if (data->IsNcbieaa()) {
            s = &data->SetNcbieaa().Set();
        } else {
            data->SetNcbieaa().Set(residues);
        }
    } else {
        if (data->IsIupacna()) {
            s = &data->SetIupacna().Set();
        } else {
            data->SetIupacna().Set(residues);
        }
    }

    if (s) {
        // grow exponentially to avoid O(n^2) behavior
        if (s->capacity() < s->size() + residues.size()) {
            s->reserve(s->capacity()
                       + max(residues.size(), s->capacity() / 2));
        }
        *s += residues;
    }
}


static CSeq_inst::EMol s_ParseFastaDefline(CBioseq::TId& ids, string& title,
                                           const string& line,
                                           TReadFastaFlags flags, int* counter)
{
    SIZE_TYPE       start = 0;
    CSeq_inst::EMol mol   = CSeq_inst::eMol_not_set;
    do {
        ++start;
        SIZE_TYPE space = line.find_first_of(" \t", start);
        string    name  = line.substr(start, space - start);
        string    local;

        if (flags & fReadFasta_NoParseID) {
            space = start - 1;
        } else {
            // try to parse out IDs
            SIZE_TYPE pos = 0;
            while (pos < name.size()) {
                SIZE_TYPE end = s_EndOfFastaID(name, pos);
                if (end == NPOS) {
                    if (pos > 0) {
                        NCBI_THROW2(CObjReaderParseException, eFormat,
                                    "s_ParseFastaDefline: Bad ID "
                                    + name.substr(pos),
                                    pos);
                    } else if (s_IsValidLocalID(name)) {
                        local = name;
                    } else {
                        space = start - 1;
                    }
                    break;
                }

                CRef<CSeq_id> id(new CSeq_id(name.substr(pos, end - pos)));
                ids.push_back(id);
                if (mol == CSeq_inst::eMol_not_set
                    &&  !(flags & fReadFasta_ForceType)) {
                    CSeq_id::EAccessionInfo ai = id->IdentifyAccession();
                    if (ai & CSeq_id::fAcc_nuc) {
                        mol = CSeq_inst::eMol_na;
                    } else if (ai & CSeq_id::fAcc_prot) {
                        mol = CSeq_inst::eMol_aa;
                    }
                }
                pos = end + 1;
            }
        }

        if ( !local.empty() ) {
            ids.push_back(CRef<CSeq_id>
                          (new CSeq_id(CSeq_id::e_Local, local, kEmptyStr)));
        }

        start = line.find('\1', start);
        if (space != NPOS  &&  title.empty()) {
            title.assign(line, space + 1,
                         (start == NPOS) ? NPOS : (start - space - 1));
        }
    } while (start != NPOS  &&  (flags & fReadFasta_AllSeqIds));

    if (ids.empty()) {
        if (flags & fReadFasta_RequireID) {
            NCBI_THROW2(CObjReaderParseException, eFormat,
                        "s_ParseFastaDefline: no ID present", 0);
        }
        CRef<CSeq_id> id(new CSeq_id);
        id->SetLocal().SetId((*counter)++);
        ids.push_back(id);
    }

    return mol;
}


static void s_GuessMol(CSeq_inst::EMol& mol, const string& data,
                       TReadFastaFlags flags, istream& in)
{
    if (mol != CSeq_inst::eMol_not_set) {
        return; // already known; no need to guess
    }

    if (mol == CSeq_inst::eMol_not_set  &&  !(flags & fReadFasta_ForceType)) {
        switch (CFormatGuess::SequenceType(data.data(), data.size())) {
        case CFormatGuess::eNucleotide:  mol = CSeq_inst::eMol_na;  return;
        case CFormatGuess::eProtein:     mol = CSeq_inst::eMol_aa;  return;
        default:                         break;
        }
    }

    // ForceType was set, or CFormatGuess failed, so we have to rely on
    // explicit assumptions
    if (flags & fReadFasta_AssumeNuc) {
        _ASSERT(!(flags & fReadFasta_AssumeProt));
        mol = CSeq_inst::eMol_na;
    } else if (flags & fReadFasta_AssumeProt) {
        mol = CSeq_inst::eMol_aa;
    } else { 
        NCBI_THROW2(CObjReaderParseException, eFormat,
                    "ReadFasta: unable to deduce molecule type"
                    " from IDs, flags, or sequence",
                    in.tellg() - CT_POS_TYPE(0));
    }
}


static
CRef<CSeq_entry> s_ReadFasta_OLD(CNcbiIstream& in, TReadFastaFlags flags,
                                 int* counter,
                                 vector<CConstRef<CSeq_loc> >* lcv)
{
    if ( !in ) {
        NCBI_THROW2(CObjReaderParseException, eFormat,
                    "ReadFasta: Input stream no longer valid",
                    in.tellg() - CT_POS_TYPE(0));
    } else {
        CT_INT_TYPE c = in.peek();
        if ( !strchr(">#!\n\r", CT_TO_CHAR_TYPE(c)) ) {
            NCBI_THROW2
                (CObjReaderParseException, eFormat,
                 "ReadFasta: Input doesn't start with a defline or comment",
                 in.tellg() - CT_POS_TYPE(0));
        }
    }

    CRef<CSeq_entry>       entry(new CSeq_entry);
    CBioseq_set::TSeq_set& sset  = entry->SetSet().SetSeq_set();
    CRef<CBioseq>          seq(0); // current Bioseq
    string                 line;
    TSeqPos                pos = 0, lc_start = 0;
    bool                   was_lc = false, in_gap = false;
    CRef<CSeq_id>          best_id;
    CRef<CSeq_loc>         lowercase(0);
    int                    defcounter = 1;

    if ( !counter ) {
        counter = &defcounter;
    }

    while ( !in.eof() ) {
        if ((flags & fReadFasta_OneSeq)  &&  seq.NotEmpty()
            &&  (in.peek() == '>')) {
            break;
        }
        NcbiGetlineEOL(in, line);
        if (NStr::EndsWith(line, "\r")) {
            line.resize(line.size() - 1);
        }
        if (in.eof()  &&  line.empty()) {
            break;
        } else if (line.empty()) {
            continue;
        }
        if (line[0] == '>') {
            // new sequence
            if (seq) {
                s_FixSeqData(seq);
                if (was_lc) {
                    lowercase->SetPacked_int().AddInterval
                        (*best_id, lc_start, pos - 1);
                }
            }
            seq = new CBioseq;
            if (flags & fReadFasta_NoSeqData) {
                seq->SetInst().SetRepr(CSeq_inst::eRepr_not_set);
            } else {
                seq->SetInst().SetRepr(CSeq_inst::eRepr_raw);
            }
            {{
                CRef<CSeq_entry> entry2(new CSeq_entry);
                entry2->SetSeq(*seq);
                sset.push_back(entry2);
            }}
            string          title;
            CSeq_inst::EMol mol = s_ParseFastaDefline(seq->SetId(), title,
                                                      line, flags, counter);
            if (mol == CSeq_inst::eMol_not_set
                &&  (flags & fReadFasta_NoSeqData)) {
                if (flags & fReadFasta_AssumeNuc) {
                    _ASSERT(!(flags & fReadFasta_AssumeProt));
                    mol = CSeq_inst::eMol_na;
                } else if (flags & fReadFasta_AssumeProt) {
                    mol = CSeq_inst::eMol_aa;
                }
            }
            seq->SetInst().SetMol(mol);

            if ( !title.empty() ) {
                CRef<CSeqdesc> desc(new CSeqdesc);
                desc->SetTitle(title);
                seq->SetDescr().Set().push_back(desc);
            }

            if (lcv) {
                pos       = 0;
                was_lc    = false;
                best_id   = FindBestChoice(seq->GetId(), CSeq_id::Score);
                lowercase = new CSeq_loc;
                lowercase->SetNull();
                lcv->push_back(lowercase);
            }
            in_gap = false;
        } else if (line[0] == '#'  ||  line[0] == '!') {
            continue; // comment
        } else if ( !seq ) {
            NCBI_THROW2
                (CObjReaderParseException, eFormat,
                 "ReadFasta: No defline preceding data",
                 in.tellg() - CT_POS_TYPE(0));
        } else if ( !(flags & fReadFasta_NoSeqData) ) {
            // These don't change, but the calls may be relatively expensive,
            // esp. with ref-counted implementations.
            SIZE_TYPE   line_size = line.size();
            const char* line_data = line.data();
            // actual data; may contain embedded junk
            CSeq_inst&  inst      = seq->SetInst();
            string      residues(line_size + 1, '\0');
            char*       res_data  = const_cast<char*>(residues.data());
            SIZE_TYPE   res_count = 0;
            for (SIZE_TYPE i = 0;  i < line_size;  ++i) {
                char c = line_data[i];
                if (isalpha((unsigned char) c)) {
                    in_gap = false;
                    if (lowercase) {
                        bool is_lc = islower((unsigned char) c) ? true : false;
                        if (is_lc && !was_lc) {
                            lc_start = pos;
                        } else if (was_lc && !is_lc) {
                            lowercase->SetPacked_int().AddInterval
                                (*best_id, lc_start, pos - 1);
                        }
                        was_lc = is_lc;
                        ++pos;
                    }
                    res_data[res_count++] = toupper((unsigned char) c);
                } else if (c == '-'  &&  (flags & fReadFasta_ParseGaps)) {
                    CDelta_ext::Tdata& d = inst.SetExt().SetDelta().Set();
                    if (in_gap) {
                        ++d.back()->SetLiteral().SetLength();
                        continue; // count long gaps
                    }
                    if (inst.GetRepr() == CSeq_inst::eRepr_raw) {
                        CRef<CDelta_seq> ds(new CDelta_seq);
                        inst.SetRepr(CSeq_inst::eRepr_delta);
                        if (inst.IsSetSeq_data()) {
                            ds->SetLiteral().SetSeq_data(inst.SetSeq_data());
                            d.push_back(ds);
                            inst.ResetSeq_data();
                        }
                    }
                    if ( res_count ) {
                        residues.resize(res_count);
                        if (inst.GetMol() == CSeq_inst::eMol_not_set) {
                            s_GuessMol(inst.SetMol(), residues, flags, in);
                        }
                        s_AddData(inst, residues);
                    }
                    in_gap    = true;
                    res_count = 0;
                    CRef<CDelta_seq> gap(new CDelta_seq);
                    if (line.find_first_not_of(" \t\r\n", i + 1) == NPOS) {
                        // consider a single - at the end of a line as
                        // a gap of unknown length, as we sometimes format
                        // them that way
                        gap->SetLoc().SetNull();
                    } else {
                        gap->SetLiteral().SetLength(1);
                    }
                    d.push_back(gap);
                } else if (c == '-'  ||  c == '*') {
                    in_gap = false;
                    // valid, at least for proteins
                    res_data[res_count++] = c;
                } else if (c == ';') {
                    i = line_size;
                    continue; // skip rest of line
                } else if ( !isspace((unsigned char) c) ) {
                    ERR_POST(Warning << "ReadFasta: Ignoring invalid residue "
                             << c << " at position "
                             << (in.tellg() - CT_POS_TYPE(0)));
                }
            }

            if (inst.GetMol() == CSeq_inst::eMol_not_set) {
                s_GuessMol(inst.SetMol(), residues, flags, in);
            }
            
            if (res_count) {
                // Add the accumulated data...
                residues.resize(res_count);
                s_AddData(inst, residues);
            }
        }
    }

    if (seq) {
        s_FixSeqData(seq);
        if (was_lc) {
            lowercase->SetPacked_int().AddInterval(*best_id, lc_start, pos - 1);
        }
    }
    // simplify if possible
    if (sset.size() == 1) {
        entry->SetSeq(*seq);
    }
    return entry;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.31  2006/07/05 19:37:14  ucko
* Avoid infinite loops when scanning input files.
*
* Revision 1.30  2006/06/27 19:04:17  ucko
* Move #include <corelib/ncbi_param.hpp> to fasta.hpp.
*
* Revision 1.29  2006/06/27 18:37:58  ucko
* Optionally implement ReadFasta as a wrapper around CFastaReader, as
* controlled by a NCBI_PARAM (READ_FASTA, USE_NEW_IMPLEMENTATION), still
* defaulting to false for now.
* Unconditionally switch to CFastaReader-based versions of
* ReadFastaFileMap and ScanFastaFile.
*
* Revision 1.28  2006/04/13 17:25:18  ucko
* Replace isblank with isspace, as Windows only has the latter.
*
* Revision 1.27  2006/04/13 14:44:18  ucko
* Add a new class-based FASTA reader, but leave the existing reader
* alone for now.
*
* Revision 1.26  2006/02/17 15:40:18  ucko
* ReadFasta: correct off-by-one error in lowercase-interval reporting
* caught by Josh Cherry.
*
* Revision 1.25  2005/11/08 19:37:47  ucko
* ReadFasta: strip off trailing \r characters, as CGIs may encounter
* them but they confuse some code (particularly s_ParseFastaDefline).
*
* Revision 1.24  2005/10/05 17:51:59  ucko
* Restore previous NoParseID behavior.
*
* Revision 1.23  2005/10/05 15:38:45  ucko
* Go back to allowing unidentified bare accessions to be treated as
* local IDs, but only on the condition that they pass a sanity check.
* Actually make s_AddData static (forgotten originally).
*
* Revision 1.22  2005/09/29 19:37:28  kuznets
* Added callback based fasta reader
*
* Revision 1.21  2005/09/29 18:38:07  ucko
* Disable remaining cases of coercing leading defline text into local
* IDs; in particular, RequireID should work better now.
*
* Revision 1.20  2005/09/26 15:18:07  ucko
* When generating IDs, use the counter *before* incrementing it.
*
* Revision 1.19  2005/09/26 15:14:59  kuznets
* Added list of ids to fasta map
*
* Revision 1.18  2005/09/23 12:43:35  kuznets
* Minor comment change
*
* Revision 1.17  2005/07/21 19:10:01  ucko
* s_ParseFastaDefline: make NoParseID act like the corresponding C
* Toolkit option, which uses the whole defline as the title and just
* assigns a numeric ID to the sequence.
*
* Revision 1.16  2005/06/03 17:01:25  lavr
* Explicit (unsigned char) casts in ctype routines
*
* Revision 1.15  2005/02/23 19:25:30  ucko
* ReadFasta: deal properly with runs of hyphens in ParseGaps mode;
* really support inline comments set off by semicolons.
*
* Revision 1.14  2005/02/07 19:03:05  ucko
* Improve handling of non-IUPAC residues.
*
* Revision 1.13  2004/11/24 19:28:05  ucko
* +fReadFasta_RequireID
*
* Revision 1.12  2004/05/21 21:42:55  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.11  2004/02/19 22:57:52  ucko
* Accommodate stricter implementations of CT_POS_TYPE.
*
* Revision 1.10  2003/12/05 03:00:36  ucko
* Validate input better.
*
* Revision 1.9  2003/10/03 15:09:18  ucko
* Tweak sequence string allocation to avoid O(n^2) performance from
* linear resizing.
*
* Revision 1.8  2003/08/25 21:30:09  ucko
* ReadFasta: tweak a bit to improve performance, and fix some bugs in
* parsing gaps.
*
* Revision 1.7  2003/08/11 14:39:54  ucko
* Populate "lowercase" with Packed_seqints rather than general Seq_loc_mixes.
*
* Revision 1.6  2003/08/08 21:29:12  dondosha
* Changed type of lcase_mask in ReadFasta to vector of CConstRefs
*
* Revision 1.5  2003/08/07 21:13:21  ucko
* Support a counter for assigning local IDs to sequences with no ID given.
* Fix some minor bugs in lowercase-character support.
*
* Revision 1.4  2003/08/06 19:08:33  ucko
* Slight interface tweak to ReadFasta: report lowercase locations in a
* vector with one entry per Bioseq rather than a consolidated Seq_loc_mix.
*
* Revision 1.3  2003/06/23 20:49:11  kuznets
* Changed to use Seq_id::AsFastaString() when reading fasta file map.
*
* Revision 1.2  2003/06/08 16:17:00  lavr
* Heed MSVC int->bool performance warning
*
* Revision 1.1  2003/06/04 17:26:22  ucko
* Split out from Seq_entry.cpp.
*
*
* ===========================================================================
*/
