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

#include "seqdbvol.hpp"

BEGIN_NCBI_SCOPE

char CSeqDBVol::GetSeqType(void)
{
    CFastMutexGuard guard(m_Lock);
    return x_GetSeqType();
}

char CSeqDBVol::x_GetSeqType(void)
{
    return m_Idx.GetSeqType();
}

Int4 CSeqDBVol::GetSeqLength(Uint4 oid, bool approx)
{
    CFastMutexGuard guard(m_Lock);
    
    Uint4 start_offset = 0;
    Uint4 end_offset   = 0;
        
    Int4 length = -1;
        
    if (! m_Idx.GetSeqStartEnd(oid, start_offset, end_offset))
        return -1;
    
    char seqtype = m_Idx.GetSeqType();
    
    if (kSeqTypeProt == seqtype) {
        // Subtract one, for the inter-sequence null.
        length = end_offset - start_offset - 1;
    } else if (kSeqTypeNucl == seqtype) {
        Int4 whole_bytes = end_offset - start_offset - 1;
        
        if (approx) {
            // Same principle as below - but use lower bits of oid
            // instead of fetching the actual last byte.  this should
            // correct for bias, unless sequence length modulo 4 has a
            // significant statistical bias, which seems unlikely to
            // me.
            
            length = (whole_bytes * 4) + (oid & 0x03);
        } else {
            // The last byte is partially full; the last two bits of
            // the last byte store the number of nucleotides in the
            // last byte (0 to 3).
            
            char amb_char = 0;
            
            m_Seq.ReadBytes(& amb_char, end_offset - 1, end_offset);
            
            Int4 remainder = amb_char & 3;
            length = (whole_bytes * 4) + remainder;
        }
    }
        
    return length;
}

static CFastMutex s_MapNaMutex;

vector<Uint1> CSeqDBMapNa2ToNa4Setup(void)
{
    vector<Uint1> translated;
    translated.resize(512);
    
    // var construction - not done
    
    Uint1 convert[16] = {17,  18,  20,  24,  33,  34,  36,  40,
                         65,  66,  68,  72, 129, 130, 132, 136};
    Int2 pair1 = 0;
    Int2 pair2 = 0;
    
    for(pair1 = 0; pair1 < 16; pair1++) {
        for(pair2 = 0; pair2 < 16; pair2++) {
            Int2 index = (pair1 * 16 + pair2) * 2;
            
            translated[index]   = convert[pair1];
            translated[index+1] = convert[pair2];
        }
    }
    
    return translated;
}

void CSeqDBMapNa2ToNa4(const char * buf2bit, vector<char> & buf4bit, int base_length)
{
    static vector<Uint1> expanded = CSeqDBMapNa2ToNa4Setup();
    
    Uint4 estimated_length = (base_length + 1)/2;
    buf4bit.reserve(estimated_length);
    
    int inp_chars = base_length/4;
    
    for(int i=0; i<inp_chars; i++) {
        char inp_char = buf2bit[i];
        
        buf4bit.push_back(expanded[ (inp_char*2)     ]);
        buf4bit.push_back(expanded[ (inp_char*2) + 1 ]);
    }
    
    int bases_remain = base_length - (inp_chars*4);
    
    if (bases_remain) {
        Uint1 remainder_bits = 2 * bases_remain;
        Uint1 remainder_mask = 0xFF << (8 - remainder_bits);
        char last_masked = buf2bit[inp_chars] & remainder_mask;
        
        buf4bit.push_back(expanded[ (last_masked*2)   ]);
        if (bases_remain > 2) {
            buf4bit.push_back(expanded[ (last_masked*2)+1 ]);
        }
    }
    
    assert(estimated_length == buf4bit.size());
}

//--------------------
// NEW (long) version
//--------------------

inline Uint4 s_ResLenNew(const vector<Int4> & ambchars, Uint4 i)
{
    return (ambchars[i] >> 16) & 0xFFF;
}

inline Uint4 s_ResPosNew(const vector<Int4> & ambchars, Uint4 i)
{
    return ambchars[i+1];
}

//-----------------------
// OLD (compact) version
//-----------------------

inline Uint4 s_ResVal(const vector<Int4> & ambchars, Uint4 i)
{
    return (ambchars[i] >> 28);
}

inline Uint4 s_ResLenOld(const vector<Int4> & ambchars, Uint4 i)
{
    return (ambchars[i] >> 24) & 0xF;
}

inline Uint4 s_ResPosOld(const vector<Int4> & ambchars, Uint4 i)
{
    return ambchars[i];
}


bool CSeqDBRebuildDNA_4na(vector<char> & buf4bit, const vector<Int4> & amb_chars)
{
    if (buf4bit.empty())
        return false;
    
    if (amb_chars.empty()) 
        return true;
    
    Uint4 amb_num = amb_chars[0];
    
    /* Check if highest order bit set. */
    bool new_format = (amb_num & 0x80000000) != 0;
    
    if (new_format) {
	amb_num &= 0x7FFFFFFF;
    }
    
    for(Uint4 i=1; i < amb_num+1; i++) {
        Int4  row_len  = 0;
        Int4  position = 0;
        Uint1 char_r   = 0;
        
	if (new_format) {
            char_r    = s_ResVal   (amb_chars, i);
            row_len   = s_ResLenNew(amb_chars, i); 
            position  = s_ResPosNew(amb_chars, i);
	} else {
            char_r    = s_ResVal   (amb_chars, i);
            row_len   = s_ResLenOld(amb_chars, i); 
            position  = s_ResPosOld(amb_chars, i);
	}
        
        Int4  pos = position / 2;
        Int4  rem = position & 1;  /* 0 or 1 */
        Uint1 char_l = char_r << 4;
        
        Int4 j;
        Int4 index = pos;
        
        // This could be made slightly faster for long runs.
        
        for(j = 0; j <= row_len; j++) {
            if (!rem) {
           	buf4bit[index] = (buf4bit[index] & 0x0F) + char_l;
            	rem = 1;
            } else {
           	buf4bit[index] = (buf4bit[index] & 0xF0) + char_r;
            	rem = 0;
                index++;
            }
    	}
        
	if (new_format) /* for new format we have 8 bytes for each element. */
            i++;
    }
    
    return true;
}

static void
s_CSeqDBWriteSeqDataProt(CSeq_inst  & seqinst,
                         const char * seq_buffer,
                         Int4         length)
{
    // stuff - ncbistdaa
    // mol = aa
        
    // This possibly/probably copies several times.
    // 1. One copy into stdaa_data.
    // 2. Second copy into NCBIstdaa.
    // 3. Third copy into seqdata.
    
    vector<char> aa_data;
    aa_data.resize(length);
    
    for(int i = 0; i < length; i++) {
        aa_data[i] = seq_buffer[i];
    }
    
    seqinst.SetSeq_data().SetNcbistdaa().Set().swap(aa_data);
    seqinst.SetMol(CSeq_inst::eMol_aa);
}

static void
s_CSeqDBWriteSeqDataNucl(CSeq_inst    & seqinst,
                         const char   * seq_buffer,
                         Int4           length)
{
    Int4 whole_bytes  = length / 4;
    Int4 partial_byte = ((length & 0x3) != 0) ? 1 : 0;
    
    vector<char> na_data;
    na_data.resize(whole_bytes + partial_byte);
    
    for(int i = 0; i<whole_bytes; i++) {
        na_data[i] = seq_buffer[i];
    }
    
    if (partial_byte) {
        na_data[whole_bytes] = seq_buffer[whole_bytes] & (0xFF - 0x03);
    }
    
    seqinst.SetSeq_data().SetNcbi2na().Set().swap(na_data);
    seqinst.SetMol(CSeq_inst::eMol_na);
}

static void
s_CSeqDBWriteSeqDataNucl(CSeq_inst    & seqinst,
                         const char   * seq_buffer,
                         Int4           length,
                         vector<Int4> & amb_chars)
{
    vector<char> buffer_4na;
    CSeqDBMapNa2ToNa4(seq_buffer, buffer_4na, length); // length is not /4 here
    CSeqDBRebuildDNA_4na(buffer_4na, amb_chars);
    
    seqinst.SetSeq_data().SetNcbi4na().Set().swap(buffer_4na);
    seqinst.SetMol(CSeq_inst::eMol_na);
}

void s_GetDescrFromDefline(CRef<CBlast_def_line_set> deflines, string & descr)
{
    descr.erase();
    
    string seqid_str;
    
    typedef list< CRef<CBlast_def_line> >::const_iterator TDefIt; 
    typedef list< CRef<CSeq_id        > >::const_iterator TSeqIt;
   
    const list< CRef<CBlast_def_line> > & dl = deflines->Get();
    
    for(TDefIt iter = dl.begin(); iter != dl.end(); iter++) {
        ostringstream oss;
        
        const CBlast_def_line & defline = **iter;
        
        if (! descr.empty()) {
            //oss << "\1";
            oss << " ";
        }
        
        if (defline.CanGetSeqid()) {
            const list< CRef<CSeq_id > > & sl = defline.GetSeqid();
            
            for (TSeqIt seqit = sl.begin(); seqit != sl.end(); seqit++) {
                (*seqit)->WriteAsFasta(oss);
            }
        }
        
        if (defline.CanGetTitle()) {
            oss << defline.GetTitle();
        }
        
        descr += oss.str();
    }
}

CRef<CBioseq>
CSeqDBVol::GetBioseq(Int4 oid,
                      bool use_objmgr,
                      bool insert_ctrlA)
{
    CFastMutexGuard guard(m_Lock);
    
    // 1. Declare variables.
    
    CRef<CBioseq> null_result;
    
    // 2. if we can't find the seq_num (get_link), we are done.
    
    //xx  Not relevant - the layer above this would do that before
    //xx  This specific 'CSeqDB' object was involved.
    
    // 3. get the descriptor - i.e. sip, defline.  Probably this
    // is like the function above that gets asn, but split it up
    // more when returning it.
    
    // QUESTIONS: What do we actually want?  The defline & seqid, or
    // the defline set and seqid set?  From readdb.c it looks like the
    // single items are the prized artifacts.
    
    CRef<CBlast_def_line_set> defline_set(x_GetHdr(oid));
    CRef<CBlast_def_line>     defline;
    list< CRef< CSeq_id > >   seqids;
    
    {
        if (defline_set.Empty() ||
            (! defline_set->CanGet())) {
            return null_result;
        }
        
    
        defline = defline_set->Get().front();
        
        if (! defline->CanGetSeqid()) {
            // Not sure if this is necessary - if it turns out we can
            // handle a null, this should be taken back out.
            return null_result;
        }
        
        // Do we want the list, or just the first CRef<>??  For now, get
        // the whole list.
        
        seqids = defline->GetSeqid();
    }
    
    // 4. If insert_ctrlA is FALSE, we convert each "^A" to " >";
    // Otherwise, we just copy the defline across. (inline func?)
    
    //xx Ignoring ctrl-A issue for now (as per Tom's insns.)
    
    // 5. Get length & sequence (_get_sequence).  That should be
    // mimicked before this.
    
    const char * seq_buffer = 0;
    
    Int4 length = x_GetSequence(oid, & seq_buffer);
    
    if (length < 1) {
        return null_result;
    }
    
    // 6. If using obj mgr, BioseqNew() is called; otherwise
    // MemNew().  --> This is the BioseqPtr, bsp.
    
    //byte_store = BSNew(0);
    
    // 7. A byte store is created (BSNew()).
    
    //xx No allocation is done; instead we just call *Set() etc.
    
    // 8. If protein, we set bsp->mol = Seq_mol_aa, seq_data_type
    // = Seq_code_ncbistdaa; then we write the buffer into the
    // byte store.
    
    // 9. Nucleotide sequences require more pain, suffering.
    // a. Try to get ambchars
    // b. If there are any, convert sequence to 4 byte rep.
    // c. Otherwise write to a byte store.
    // d. Set mol = Seq_mol_na;
    
    // 10. Stick the byte store into the bsp->seq_data
    
    CRef<CBioseq> bioseq(new CBioseq);
    
    CSeq_inst & seqinst = bioseq->SetInst();
    //CSeq_data & seqdata = seqinst->SetSeq_data();
    
    bool is_prot = (x_GetSeqType() == kSeqTypeProt);
    
    if (is_prot) {
        s_CSeqDBWriteSeqDataProt(seqinst, seq_buffer, length);
    } else {
        // nucl
        vector<Int4> ambchars;
        
        if (! x_GetAmbChar(oid, ambchars)) {
            return null_result;
        }
        
        if (ambchars.empty()) {
            // keep as 2 bit
            s_CSeqDBWriteSeqDataNucl(seqinst, seq_buffer, length);
        } else {
            // translate to 4 bit
            s_CSeqDBWriteSeqDataNucl(seqinst, seq_buffer, length, ambchars);
        }
        
        // mol = na
        seqinst.SetMol(CSeq_inst::eMol_na);
    }
    
    if (seq_buffer) {
        x_RetSequence(& seq_buffer);
        
        seq_buffer = 0;
    }
    
    // 11. Set the length, id (Seq_id), and repr (== raw).
    
    seqinst.SetLength(length);
    seqinst.SetRepr(CSeq_inst::eRepr_raw);
    bioseq->SetId().swap(seqids);
    
    // 12. Add the new_defline to the list at bsp->descr if
    // non-null, with ValNodeAddString().
    
    // 13. If the format is not text (si), we get the defline and
    // chain it onto the bsp->desc list; then we read and append
    // taxonomy names to the list (readdb_get_taxonomy_names()).
    
    if (defline_set.NotEmpty()) {
        // Convert defline to ... string.
        
        //1. Have a defline.. maybe.  Want to add this as the title.(?)
        // A. How to add a description item:
        
        string description;
        
        s_GetDescrFromDefline(defline_set, description);
        
        CRef<CSeqdesc> desc1(new CSeqdesc);
        desc1->SetTitle().swap(description);
        
        CSeq_descr & seqdesc = bioseq->SetDescr();
        seqdesc.Set().push_back(desc1);
    }
    
    // I'm going to let the taxonomy slide for now....
    
    //cerr << "Taxonomy, etc." << endl;

    // 14. Return the bsp.
    
    // Everything seems eerily quiet so far, so...
    
    return bioseq;
}

bool CSeqDBVol::RetSequence(const char ** buffer)
{
    CFastMutexGuard guard(m_Lock);
    return x_RetSequence(buffer);
}

bool CSeqDBVol::x_RetSequence(const char ** buffer)
{
    bool found = m_Seq.RetRegion(*buffer);
    
    if (found) {
        *buffer = 0;
    }
    
    return found;
}

Int4 CSeqDBVol::GetSequence(Int4 oid, const char ** buffer)
{
    CFastMutexGuard guard(m_Lock);
    return x_GetSequence(oid, buffer);
}

Int4 CSeqDBVol::x_GetSequence(Int4 oid, const char ** buffer)
{
    Uint4 start_offset = 0;
    Uint4 end_offset   = 0;
        
    Int4 length = -1;
        
    if (! m_Idx.GetSeqStartEnd(oid, start_offset, end_offset))
        return -1;
        
    char seqtype = m_Idx.GetSeqType();
        
    if (kSeqTypeProt == seqtype) {
        // Subtract one, for the inter-sequence null.
                
        end_offset --;
                
        length = end_offset - start_offset;
            
        *buffer = m_Seq.GetRegion(start_offset, end_offset);
    } else if (kSeqTypeNucl == seqtype) {
        // The last byte is partially full; the last two bits of
        // the last byte store the number of nucleotides in the
        // last byte (0 to 3).
            
        *buffer = m_Seq.GetRegion(start_offset, end_offset);
            
        Int4 whole_bytes = end_offset - start_offset - 1;
            
        char last_char = (*buffer)[whole_bytes];
        Int4 remainder = last_char & 3;
        length = (whole_bytes * 4) + remainder;
    }
        
    return length;
}

Uint8 CSeqDBVol::GetTotalLength(void)
{
    CFastMutexGuard guard(m_Lock);
    return m_Idx.GetTotalLength();
}

CRef<CBlast_def_line_set> CSeqDBVol::GetHdr(Uint4 oid)
{
    CFastMutexGuard guard(m_Lock);
    return x_GetHdr(oid);
}

CRef<CBlast_def_line_set> CSeqDBVol::x_GetHdr(Uint4 oid)
{
    CRef<CBlast_def_line_set> nullret;
        
    Uint4 hdr_start = 0;
    Uint4 hdr_end   = 0;
        
    if (! m_Idx.GetHdrStartEnd(oid, hdr_start, hdr_end)) {
        return nullret;
    }
        
    const char * asn_region = m_Hdr.GetRegion(hdr_start, hdr_end);
        
    // Now; create an ASN.1 object from the memory chunk provided
    // here.
        
    if (! asn_region) {
        return nullret;
    }
    
    // Get the data as a string, then as a stringstream, then build
    // the actual asn.1 object.  How much 'extra' work is done here?
    // Perhaps a dedicated ASN.1 memory-stream object would be of some
    // benefit, particularly in the "called 100000 times" cases.
    
    istringstream asndata( string(asn_region, asn_region + (hdr_end - hdr_start)) );
    
    m_Hdr.RetRegion(asn_region);
    
    auto_ptr<CObjectIStream> inpstr(CObjectIStream::Open(eSerial_AsnBinary, asndata));
    
    CRef<objects::CBlast_def_line_set> phil(new objects::CBlast_def_line_set);
    
    *inpstr >> *phil;
    
    
    return phil;
}

bool CSeqDBVol::x_GetAmbChar(Uint4 oid, vector<Int4> ambchars)
{
    Uint4 start_offset = 0;
    Uint4 end_offset   = 0;
    
    bool ok = m_Idx.GetAmbStartEnd(oid, start_offset, end_offset);
    
    if (! ok) {
        return false;
    }
    
    Int4 length = end_offset - start_offset;
    
    if (0 == length)
        return true;
    
    Int4 total = length / 4;
    
    Int4 * buffer = (Int4*) m_Seq.GetRegion(start_offset, start_offset + (total * 4));
    
    // This makes no sense...
    total &= 0x7FFFFFFF;
    
    ambchars.resize(total);
    
    for(int i = 0; i<total; i++) {
	//ambchars[i] = CByteSwap::GetInt4((const unsigned char *)(& buffer[i]));
	ambchars[i] = SeqDB_GetStdOrd((const unsigned char *)(& buffer[i]));
    }
    
    return true;
}

Uint4 CSeqDBVol::GetNumSeqs(void)
{
    CFastMutexGuard guard(m_Lock);
    return m_Idx.GetNumSeqs();
}

string CSeqDBVol::GetTitle(void)
{
    CFastMutexGuard guard(m_Lock);
    return m_Idx.GetTitle();
}

string CSeqDBVol::GetDate(void)
{
    CFastMutexGuard guard(m_Lock);
    return m_Idx.GetDate();
}

Uint4 CSeqDBVol::GetMaxLength(void)
{
    CFastMutexGuard guard(m_Lock);
    return m_Idx.GetMaxLength();
}

END_NCBI_SCOPE

